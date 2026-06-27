#include "GameShared/GameClasses/System/PC/CgsMovieAudioPC.h"
#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

// The EA-XMA deblock + XMA-frame extraction here is the proven logic from
// tools/audio/sns_xma_decode.cpp (which produced a verified WAV from
// CRITERION.SNS through the same xmaframes leaf). It is kept verbatim so the
// in-game path matches the validated offline decode bit-for-bit.

#define AUDIO_LOG if (CgsDev::Log::gpDebugPrint) (*CgsDev::Log::gpDebugPrint)

namespace CgsSystem
{
namespace
{
    // ---- decoded-stream playback state (single movie at a time) -----------------
    s16*            g_pcm      = nullptr;   // interleaved stereo, 48 kHz
    long            g_frames   = 0;         // total stereo frames in g_pcm
    long            g_cursor   = 0;         // next frame to output
    volatile bool   g_finished = true;
    bool            g_loaded   = false;

    const int kPacketBytes = 2048;
    const int kPacketBits  = kPacketBytes * 8;
    const int kPacketHeaderBits = 32;
    const int kSampleRate  = 48000;

    std::uint32_t ReadBe24(const std::uint8_t* p) {
        return (std::uint32_t(p[0]) << 16) | (std::uint32_t(p[1]) << 8) | std::uint32_t(p[2]);
    }
    std::uint32_t ReadBe32(const std::uint8_t* p) {
        return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
               (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
    }

    // EA blocked stream -> contiguous, 2048-aligned XMA2 packets.
    // [u8 flags][u24 block_size][u32 samples][u32 pseudo_size][XMA bytes @ +12]
    std::vector<std::uint8_t> RestorePackets(const std::vector<std::uint8_t>& sns) {
        std::vector<std::uint8_t> packets;
        std::size_t offset = 0;
        while (offset < sns.size()) {
            if (sns.size() - offset < 12) throw std::runtime_error("truncated EA block header");
            const std::uint8_t  flags      = sns[offset];
            const std::uint32_t block_size = ReadBe24(&sns[offset + 1]);
            if (block_size < 12 || block_size > sns.size() - offset) throw std::runtime_error("bad EA block size");
            const std::uint32_t pseudo_size  = ReadBe32(&sns[offset + 8]);
            const std::size_t   subblock_size = pseudo_size / 4;
            if (subblock_size < 4 || subblock_size > block_size - 8) throw std::runtime_error("bad EA XMA subblock");
            const std::size_t   data_size = subblock_size - 4;
            const std::uint8_t* source    = &sns[offset + 12];
            packets.insert(packets.end(), source, source + data_size);
            const std::size_t padded = (data_size + kPacketBytes - 1) / kPacketBytes * kPacketBytes;
            packets.insert(packets.end(), padded - data_size, 0xFF);
            offset += block_size;
            if ((flags & 0x80) != 0) break;
        }
        if (packets.empty() || packets.size() % kPacketBytes != 0)
            throw std::runtime_error("restored XMA not packet aligned");
        return packets;
    }

    std::uint32_t ReadBits(const std::uint8_t* data, std::size_t bit, unsigned n) {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < n; ++i)
            v = (v << 1) | ((data[(bit + i) / 8] >> (7 - ((bit + i) & 7))) & 1);
        return v;
    }
    void AppendBits(std::vector<std::uint8_t>& dst, const std::uint8_t* src, std::size_t bit, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i)
            dst.push_back(std::uint8_t((src[(bit + i) / 8] >> (7 - ((bit + i) & 7))) & 1));
    }

    struct FrameBits { std::vector<std::uint8_t> bits; };

    std::vector<FrameBits> ExtractFrames(const std::vector<std::uint8_t>& packets) {
        std::vector<FrameBits> frames;
        std::vector<std::uint8_t> partial;
        std::size_t expected_partial_bits = 0;
        for (std::size_t po = 0; po < packets.size(); po += kPacketBytes) {
            const std::uint8_t* packet = &packets[po];
            const std::size_t   continuation_bits = ReadBits(packet, 6, 15);
            const std::uint32_t packet_skip       = ReadBits(packet, 21, 11);
            if (packet_skip != 0) throw std::runtime_error("unsupported XMA packet header");
            std::size_t bit_offset = kPacketHeaderBits;
            if (!partial.empty()) {
                const std::size_t available = kPacketBits - bit_offset;
                const bool continuation_only = continuation_bits >= available;
                std::size_t copied = std::min<std::size_t>(continuation_bits, available);
                if (expected_partial_bits != 0)
                    copied = std::min(copied, expected_partial_bits - partial.size());
                AppendBits(partial, packet, bit_offset, copied);
                bit_offset += copied;
                if (expected_partial_bits == 0 && partial.size() >= 15) {
                    expected_partial_bits = 0;
                    for (unsigned i = 0; i < 15; ++i)
                        expected_partial_bits = (expected_partial_bits << 1) | partial[i];
                }
                if (expected_partial_bits == 0 || partial.size() > expected_partial_bits)
                    throw std::runtime_error("split XMA frame length mismatch");
                if (partial.size() < expected_partial_bits) continue;
                frames.push_back({std::move(partial)});
                partial.clear();
                expected_partial_bits = 0;
                if (continuation_only) continue;
            } else if (continuation_bits != 0) {
                throw std::runtime_error("orphan XMA frame continuation");
            }
            while (bit_offset + 15 <= (std::size_t)kPacketBits) {
                const std::uint32_t frame_bits = ReadBits(packet, bit_offset, 15);
                if (frame_bits == 0 || frame_bits == 0x7FFF) break;
                const std::size_t remaining = kPacketBits - bit_offset;
                if (frame_bits > remaining) {
                    AppendBits(partial, packet, bit_offset, remaining);
                    expected_partial_bits = frame_bits;
                    break;
                }
                FrameBits frame;
                AppendBits(frame.bits, packet, bit_offset, frame_bits);
                const bool more_frames = frame.bits.back() != 0;
                frames.push_back(std::move(frame));
                bit_offset += frame_bits;
                if (!more_frames) break;
            }
        }
        return frames;   // a truncated trailing partial is tolerated (HW priming tail)
    }

    // Decode all extracted frames through the FFmpeg xmaframes leaf into stereo s16.
    std::vector<std::int16_t> DecodeAll(const std::vector<FrameBits>& frames, int source_channels) {
        std::vector<std::int16_t> pcm;
        const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_XMAFRAMES);
        if (codec == nullptr) { AUDIO_LOG << "[MovieAudio] no xmaframes decoder in this FFmpeg build\n"; return pcm; }

        AVCodecContext* ctx = avcodec_alloc_context3(codec);
        AVFrame* frame  = av_frame_alloc();
        AVPacket* packet = av_packet_alloc();
        if (!ctx || !frame || !packet) { AUDIO_LOG << "[MovieAudio] FFmpeg alloc failed\n"; return pcm; }
        ctx->sample_rate = kSampleRate;
        av_channel_layout_default(&ctx->ch_layout, source_channels);
        if (avcodec_open2(ctx, codec, nullptr) < 0) {
            AUDIO_LOG << "[MovieAudio] avcodec_open2(xmaframes) failed\n";
            av_packet_free(&packet); av_frame_free(&frame); avcodec_free_context(&ctx);
            return pcm;
        }
        pcm.reserve(frames.size() * 512 * 2);
        std::size_t failed = 0;
        for (const FrameBits& src : frames) {
            const std::size_t frame_bytes = (src.bits.size() + 7) / 8;
            const std::uint8_t padding_end = std::uint8_t(frame_bytes * 8 - src.bits.size());
            std::vector<std::uint8_t> enc(1 + frame_bytes, 0);
            enc[0] = std::uint8_t(padding_end << 2);            // padding_start=0, padding_end in bits 2..4
            for (std::size_t i = 0; i < src.bits.size(); ++i)
                enc[1 + i / 8] |= std::uint8_t(src.bits[i] << (7 - (i & 7)));

            av_packet_unref(packet);
            if (av_new_packet(packet, int(enc.size())) < 0) { ++failed; continue; }
            std::memcpy(packet->data, enc.data(), enc.size());
            if (avcodec_send_packet(ctx, packet) < 0) { ++failed; continue; }
            if (avcodec_receive_frame(ctx, frame) < 0)  { ++failed; continue; }
            if (ctx->sample_fmt != AV_SAMPLE_FMT_FLTP || frame->data[0] == nullptr) { ++failed; continue; }

            const float* L = reinterpret_cast<const float*>(frame->data[0]);
            const float* R = (source_channels == 2 && frame->data[1]) ? reinterpret_cast<const float*>(frame->data[1]) : L;
            for (int i = 0; i < frame->nb_samples; ++i) {
                const long l = std::lrintf(L[i] * 32767.0f);
                const long r = std::lrintf(R[i] * 32767.0f);
                pcm.push_back(std::int16_t(std::clamp<long>(l, -32767, 32767)));
                pcm.push_back(std::int16_t(std::clamp<long>(r, -32767, 32767)));
            }
            av_frame_unref(frame);
        }
        av_packet_free(&packet); av_frame_free(&frame); avcodec_free_context(&ctx);
        AUDIO_LOG << "[MovieAudio] decoded " << (int)(frames.size() - failed) << "/" << (int)frames.size()
                  << " XMA frames -> " << (int)(pcm.size() / 2) << " samples\n";
        return pcm;
    }
} // namespace

void MovieAudioPC::Construct()
{
    g_pcm = nullptr; g_frames = 0; g_cursor = 0; g_finished = true; g_loaded = false;
}

void MovieAudioPC::Release()
{
    Stop();
    if (g_pcm) { std::free(g_pcm); g_pcm = nullptr; }
    g_frames = 0; g_cursor = 0; g_finished = true; g_loaded = false;
}

bool MovieAudioPC::Load(const char* lpSnsPath, int liChannels)
{
    // drop any previous stream
    Stop();
    if (g_pcm) { std::free(g_pcm); g_pcm = nullptr; }
    g_frames = 0; g_cursor = 0; g_finished = true; g_loaded = false;

    if (lpSnsPath == nullptr || lpSnsPath[0] == 0) return false;
    std::FILE* lpFile = std::fopen(lpSnsPath, "rb");
    if (lpFile == nullptr) {
        AUDIO_LOG << "[MovieAudio] open failed: " << lpSnsPath << "\n";
        return false;
    }
    std::fseek(lpFile, 0, SEEK_END);
    long lSize = std::ftell(lpFile);
    std::fseek(lpFile, 0, SEEK_SET);
    if (lSize <= 0) { std::fclose(lpFile); return false; }
    std::vector<std::uint8_t> sns(static_cast<std::size_t>(lSize));
    const std::size_t lRead = std::fread(sns.data(), 1, sns.size(), lpFile);
    std::fclose(lpFile);
    if (lRead != sns.size()) { AUDIO_LOG << "[MovieAudio] short read: " << lpSnsPath << "\n"; return false; }

    std::vector<std::int16_t> pcm;
    try {
        const std::vector<std::uint8_t> packets = RestorePackets(sns);
        const std::vector<FrameBits>    frames  = ExtractFrames(packets);
        pcm = DecodeAll(frames, liChannels);
    } catch (const std::exception& lEx) {
        AUDIO_LOG << "[MovieAudio] decode error (" << lpSnsPath << "): " << lEx.what() << "\n";
        return false;
    }
    if (pcm.empty()) return false;

    g_frames = long(pcm.size() / 2);
    g_pcm = static_cast<s16*>(std::malloc(std::size_t(g_frames) * 2 * sizeof(s16)));
    if (g_pcm == nullptr) { g_frames = 0; return false; }
    std::memcpy(g_pcm, pcm.data(), std::size_t(g_frames) * 2 * sizeof(s16));
    g_cursor = 0; g_finished = false; g_loaded = true;

    // Open the XAudio2 device NOW, while still decoding/loading -- before MoviePlayer::Play()
    // stamps the wall-clock. Opening the audio endpoint (CreateMasteringVoice) costs a few
    // hundred ms the first time; doing it here (with a silent fill) keeps the later Start()
    // instant, so audio and video begin together instead of the audio lagging by the open cost.
    if (!AudioOutputPC::IsOpen())
        AudioOutputPC::Open(kSampleRate, 2, nullptr, nullptr);   // null fill -> silence until Start()

    AUDIO_LOG << "[MovieAudio] loaded " << lpSnsPath << " (" << (int)g_frames << " frames, "
              << (int)(g_frames / kSampleRate) << " s)\n";
    return true;
}

void MovieAudioPC::FillStatic(s16* lpOut, int liFrames, void* /*lpUser*/)
{
    // Realtime XAudio2 callback: copy from the decoded buffer; pad with silence
    // once exhausted. Touches only g_pcm/g_cursor (set up before Start()).
    long li = 0;
    if (g_pcm != nullptr) {
        for (; li < liFrames && g_cursor < g_frames; ++li, ++g_cursor) {
            *lpOut++ = g_pcm[g_cursor * 2 + 0];
            *lpOut++ = g_pcm[g_cursor * 2 + 1];
        }
        if (g_cursor >= g_frames) g_finished = true;
    }
    for (; li < liFrames; ++li) { *lpOut++ = 0; *lpOut++ = 0; }
}

void MovieAudioPC::Fill(s16* lpOut, int liFrames) { FillStatic(lpOut, liFrames, this); }

void MovieAudioPC::Start()
{
    if (!g_loaded) return;
    g_cursor = 0; g_finished = false;
    // The device was already opened (silent) in Load(), before Play() -- so starting playback is
    // just swapping the fill to this stream, which is instant and keeps audio aligned with the
    // wall-clock the video started on. (Opening here instead would cost the audio-endpoint open
    // time AFTER Play(), leaving the audio lagging the video by that much -- the "first half
    // second cut off".) Only the ~16 ms of already-queued silence precedes frame 0.
    if (AudioOutputPC::IsOpen())
        AudioOutputPC::SetFill(&MovieAudioPC::FillStatic, nullptr);
    else
        AudioOutputPC::Open(kSampleRate, 2, &MovieAudioPC::FillStatic, nullptr);   // fallback
}

void MovieAudioPC::Stop()
{
    // Close the device (DestroyVoice synchronises with the XAudio2 callback, so no fill runs
    // afterwards -- making it safe for a following Load() to free the PCM buffer, and leaving the
    // device off between movies so the next Start() opens fresh from frame 0).
    AudioOutputPC::Close();
    g_finished = true;
}

bool MovieAudioPC::IsLoaded() const   { return g_loaded; }
bool MovieAudioPC::IsFinished() const { return g_finished; }

} // namespace CgsSystem
