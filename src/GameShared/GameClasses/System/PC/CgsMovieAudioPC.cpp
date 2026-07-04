#include "GameShared/GameClasses/System/PC/CgsMovieAudioPC.h"
#include "GameShared/GameClasses/System/PC/CgsAudioOutputPC.h"
#include "GameShared/GameClasses/System/PC/CgsStreamHeadersPC.h"   // the resident SNR table (console chain)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
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
    // Set per-stream from the SNR header. The video logos are 48 kHz; the rest are
    // 44.1 kHz. Defaults to 48 kHz for the SNR-less fallback path.
    int       g_sampleRate  = 48000;

    std::uint32_t ReadBe24(const std::uint8_t* p) {
        return (std::uint32_t(p[0]) << 16) | (std::uint32_t(p[1]) << 8) | std::uint32_t(p[2]);
    }
    std::uint32_t ReadBe32(const std::uint8_t* p) {
        return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) |
               (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
    }

    struct RestoredXma {
        std::vector<std::uint8_t> packets;
        std::size_t samples;
    };

    // EA blocked stream -> contiguous, 2048-aligned XMA2 packets.
    // [u8 flags][u24 block_size][u32 samples][u32 pseudo_size][XMA bytes @ +12]
    RestoredXma RestorePackets(const std::vector<std::uint8_t>& sns) {
        RestoredXma restored;
        restored.samples = 0;
        std::size_t offset = 0;
        while (offset < sns.size()) {
            if (sns.size() - offset < 12) throw std::runtime_error("truncated EA block header");
            const std::uint8_t  flags      = sns[offset];
            const std::uint32_t block_size = ReadBe24(&sns[offset + 1]);
            if (block_size < 12 || block_size > sns.size() - offset) throw std::runtime_error("bad EA block size");
            const std::uint32_t samples      = ReadBe32(&sns[offset + 4]);
            const std::uint32_t pseudo_size  = ReadBe32(&sns[offset + 8]);
            const std::size_t   subblock_size = pseudo_size / 4;
            if (subblock_size < 4 || subblock_size > block_size - 8) throw std::runtime_error("bad EA XMA subblock");
            const std::size_t   data_size = subblock_size - 4;
            const std::uint8_t* source    = &sns[offset + 12];
            restored.packets.insert(restored.packets.end(), source, source + data_size);
            const std::size_t padded = (data_size + kPacketBytes - 1) / kPacketBytes * kPacketBytes;
            restored.packets.insert(restored.packets.end(), padded - data_size, 0xFF);
            restored.samples += samples;
            offset += block_size;
            if ((flags & 0x80) != 0) break;
        }
        if (restored.packets.empty() || restored.packets.size() % kPacketBytes != 0)
            throw std::runtime_error("restored XMA not packet aligned");
        return restored;
    }

    // The SNR (GenericRwacWaveContent) resource that pairs with the streamed .SNS.
    // On X360 the game looks this up by hashed name; here it is staged next to the
    // .SNS (same base name, ".SNR" extension). It carries the real channel count,
    // sample rate and total sample count, plus a *prefetched* inline chunk of XMA
    // holding the first ~0.3 s of audio (the attack) -- which is NOT in the .SNS.
    // Decoding the .SNS alone therefore drops the attack; the prefetch must be
    // prepended. Layout: [0x00] 16B wrapper, [0x10] EAAC header (codec/ch/rate,
    // type/num_samples), [0x18] prefetch_samples + prefetch_size, [0x28] XMA data.
    struct SnrHeader {
        int channels = 0;
        int sampleRate = 0;
        std::uint32_t numSamples = 0;
        std::vector<std::uint8_t> prefetch;   // raw inline XMA bytes
        bool valid = false;
    };

    SnrHeader ParseSnr(const std::vector<std::uint8_t>& d) {
        SnrHeader h;
        const std::size_t kBase = 0x10;        // skip 16-byte resource wrapper
        const std::size_t kDataOffset = 0x28;  // wrapper + EAAC(8) + extended(16)
        // A header-only SNR (no prefetch -- e.g. the 32-byte music-stream headers in
        // STREAMHEADERS) still carries the authoritative codec/channels/rate/samples;
        // only the prefetch fields/data may be absent.
        if (d.size() < kBase + 8) return h;
        const std::uint32_t h1 = ReadBe32(&d[kBase]);
        const std::uint32_t h2 = ReadBe32(&d[kBase + 4]);
        const int codec = int((h1 >> 24) & 0xF);
        if (codec != 3) return h;              // 3 == EA-XMA
        h.channels   = int((h1 >> 18) & 0x3F) + 1;
        h.sampleRate = int(h1 & 0x3FFFF);
        h.numSamples = h2 & 0x1FFFFFFF;
        if (d.size() > kDataOffset) {
            const std::uint32_t prefetchSize = ReadBe32(&d[kBase + 12]);  // 0x1c
            const std::size_t avail = d.size() - kDataOffset;
            const std::size_t bytes = std::min<std::size_t>(prefetchSize, avail);
            h.prefetch.assign(d.begin() + kDataOffset, d.begin() + kDataOffset + bytes);
        }
        h.valid = true;
        return h;
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

    std::size_t ReadFrameLength(const std::vector<std::uint8_t>& bits) {
        std::size_t frame_bits = 0;
        for (unsigned i = 0; i < 15; ++i)
            frame_bits = (frame_bits << 1) | bits[i];
        return frame_bits;
    }

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
                const std::size_t continuation_payload =
                    std::min<std::size_t>(continuation_bits, available);
                std::size_t copied = 0;
                if (expected_partial_bits == 0 && partial.size() < 15) {
                    const std::size_t needed = 15 - partial.size();
                    const std::size_t chunk = std::min(needed, continuation_payload);
                    AppendBits(partial, packet, bit_offset, chunk);
                    bit_offset += chunk;
                    copied += chunk;
                    if (partial.size() == 15)
                        expected_partial_bits = ReadFrameLength(partial);
                }
                if (expected_partial_bits != 0) {
                    if (partial.size() > expected_partial_bits)
                        throw std::runtime_error("split XMA frame length mismatch");
                    const std::size_t needed = expected_partial_bits - partial.size();
                    const std::size_t chunk = std::min(needed, continuation_payload - copied);
                    AppendBits(partial, packet, bit_offset, chunk);
                    bit_offset += chunk;
                    copied += chunk;
                }
                if (expected_partial_bits == 0 || partial.size() > expected_partial_bits)
                    throw std::runtime_error("split XMA frame length mismatch");
                if (partial.size() < expected_partial_bits) continue;
                if (!continuation_only && copied != continuation_payload)
                    throw std::runtime_error("XMA continuation overran completed frame");
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
                if (bit_offset + 15 > (std::size_t)kPacketBits) {
                    const std::size_t remaining = kPacketBits - bit_offset;
                    if (remaining != 0) {
                        AppendBits(partial, packet, bit_offset, remaining);
                        expected_partial_bits = 0;
                    }
                    break;
                }
            }
        }
        return frames;   // a truncated trailing partial is tolerated (HW priming tail)
    }

    // Decode all extracted frames through the FFmpeg xmaframes leaf into stereo s16.
    std::vector<std::int16_t> DecodeAll(const std::vector<FrameBits>& frames, int source_channels,
                                        std::size_t exact_samples) {
        std::vector<std::int16_t> pcm;
        const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_XMAFRAMES);
        if (codec == nullptr) { AUDIO_LOG << "[MovieAudio] no xmaframes decoder in this FFmpeg build\n"; return pcm; }

        AVCodecContext* ctx = avcodec_alloc_context3(codec);
        AVFrame* frame  = av_frame_alloc();
        AVPacket* packet = av_packet_alloc();
        if (!ctx || !frame || !packet) { AUDIO_LOG << "[MovieAudio] FFmpeg alloc failed\n"; return pcm; }
        ctx->sample_rate = g_sampleRate;
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
        const std::size_t wanted_values = exact_samples * 2;
        if (pcm.size() < wanted_values)
            pcm.resize(wanted_values, 0);
        else if (pcm.size() > wanted_values)
            pcm.resize(wanted_values);
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

// ---------------------------------------------------------------------------
// Shared SNS(+staged SNR) -> interleaved stereo s16 PCM decode. Used by the
// movie stream (Load) and the menu-music stream (MenuMusicPC::Play); lpacTag
// prefixes the log lines so each consumer keeps its probe identity.
// ---------------------------------------------------------------------------
static bool DecodeSnsFileToPcm(const char* lpSnsPath, int liChannels, const char* lpacTag,
                               std::vector<std::int16_t>& lrPcm, int& lrSampleRate)
{
    if (lpSnsPath == nullptr || lpSnsPath[0] == 0) return false;
    std::FILE* lpFile = std::fopen(lpSnsPath, "rb");
    if (lpFile == nullptr) {
        AUDIO_LOG << lpacTag << " open failed: " << lpSnsPath << "\n";
        return false;
    }
    std::fseek(lpFile, 0, SEEK_END);
    long lSize = std::ftell(lpFile);
    std::fseek(lpFile, 0, SEEK_SET);
    if (lSize <= 0) { std::fclose(lpFile); return false; }
    std::vector<std::uint8_t> sns(static_cast<std::size_t>(lSize));
    const std::size_t lRead = std::fread(sns.data(), 1, sns.size(), lpFile);
    std::fclose(lpFile);
    if (lRead != sns.size()) { AUDIO_LOG << lpacTag << " short read: " << lpSnsPath << "\n"; return false; }

    // Resolve the stream's SNR (GenericRwacWaveContent) from the RESIDENT
    // StreamHeaders bundle by the .SNS file name (path zone 1) -- the console
    // chain (StreamsRegistry ContentSpec -> HashString(gamedb url) ->
    // StreamHeaders resource; see CgsStreamHeadersPC.h). It supplies the
    // authoritative channels / sample rate / total length plus the PREFETCHED
    // attack: without it the first ~0.3 s of audio is missing and everything
    // after plays EARLY (the reported intro desync), and an unknown rate
    // falls back to 48 kHz (the reported pitch shift on 44.1 kHz streams).
    lrSampleRate     = 48000;            // default for the header-less fallback
    int       liDecodeChannels = liChannels;
    SnrHeader snr;
    {
        const char* lpacBase = std::strrchr(lpSnsPath, '\\');
        lpacBase = lpacBase ? (lpacBase + 1) : lpSnsPath;
        const u8* lpSnrData = nullptr;
        u32       luSnrLen  = 0;
        if (StreamHeadersPC::ResolveBySnsName(lpacBase, &lpSnrData, &luSnrLen))
        {
            std::vector<std::uint8_t> lSnrData(lpSnrData, lpSnrData + luSnrLen);
            snr = ParseSnr(lSnrData);
        }
        if (snr.valid) {
            AUDIO_LOG << lpacTag << " SNR (StreamHeaders) " << lpacBase << " ch=" << snr.channels
                      << " rate=" << snr.sampleRate << " samples=" << (int)snr.numSamples
                      << " prefetch=" << (int)snr.prefetch.size() << "B\n";
            lrSampleRate     = snr.sampleRate;
            liDecodeChannels = snr.channels;
        } else {
            AUDIO_LOG << lpacTag << " no StreamHeaders entry for " << lpSnsPath
                      << " -- decoding .SNS only (rate 48k fallback; attack may be clipped)\n";
        }
    }

    try {
        RestoredXma restored = RestorePackets(sns);
        std::size_t lTargetSamples = restored.samples;
        // Prepend the SNR's inline prefetch (the attack) so the decode covers the
        // whole sound; prefetch + body form one continuous XMA packet stream.
        if (snr.valid && !snr.prefetch.empty()) {
            std::vector<std::uint8_t> lCombined = snr.prefetch;
            const std::size_t lPadded =
                (lCombined.size() + kPacketBytes - 1) / kPacketBytes * kPacketBytes;
            lCombined.resize(lPadded, 0xFF);
            lCombined.insert(lCombined.end(), restored.packets.begin(), restored.packets.end());
            restored.packets.swap(lCombined);
        }
        if (snr.valid) lTargetSamples = snr.numSamples;
        const std::vector<FrameBits> frames = ExtractFrames(restored.packets);
        lrPcm = DecodeAll(frames, liDecodeChannels, lTargetSamples);
    } catch (const std::exception& lEx) {
        AUDIO_LOG << lpacTag << " decode error (" << lpSnsPath << "): " << lEx.what() << "\n";
        return false;
    }
    return !lrPcm.empty();
}

bool MovieAudioPC::Load(const char* lpSnsPath, int liChannels)
{
    // drop any previous stream
    Stop();
    if (g_pcm) { std::free(g_pcm); g_pcm = nullptr; }
    g_frames = 0; g_cursor = 0; g_finished = true; g_loaded = false;

    std::vector<std::int16_t> pcm;
    if (!DecodeSnsFileToPcm(lpSnsPath, liChannels, "[MovieAudio]", pcm, g_sampleRate))
        return false;

    g_frames = long(pcm.size() / 2);
    g_pcm = static_cast<s16*>(std::malloc(std::size_t(g_frames) * 2 * sizeof(s16)));
    if (g_pcm == nullptr) { g_frames = 0; return false; }
    std::memcpy(g_pcm, pcm.data(), std::size_t(g_frames) * 2 * sizeof(s16));
    g_cursor = 0; g_finished = false; g_loaded = true;

    // Open the XAudio2 device NOW, while still decoding/loading -- before MoviePlayer::Play()
    // stamps the wall-clock. Opening the audio endpoint (CreateMasteringVoice) costs a few
    // hundred ms the first time; doing it here (with a silent fill) keeps the later Start()
    // instant, so audio and video begin together instead of the audio lagging by the open cost.
    // If the MENU-MUSIC stream has the device open at a different rate, close it first (the
    // single source voice is fixed-rate; MenuMusicPC::Update reclaims it after this stream stops).
    if (AudioOutputPC::IsOpen() && AudioOutputPC::GetOpenSampleRate() != g_sampleRate)
        AudioOutputPC::Close();
    if (!AudioOutputPC::IsOpen())
        AudioOutputPC::Open(g_sampleRate, 2, nullptr, nullptr);   // null fill -> silence until Start()

    AUDIO_LOG << "[MovieAudio] loaded " << lpSnsPath << " (" << (int)g_frames << " frames, "
              << (int)(g_frames / g_sampleRate) << " s)\n";
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
        AudioOutputPC::Open(g_sampleRate, 2, &MovieAudioPC::FillStatic, nullptr);   // fallback
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

// ===========================================================================
//  MenuMusicPC -- the looping menu-music stream (see the header note). Shares
//  this TU's decode; the MOVIE stream keeps fill priority (console parity: the
//  menu stream is hash-0-stopped before any attract/boot video plays).
// ===========================================================================
namespace
{
    s16* m_musicPcm    = nullptr;   // interleaved stereo PCM (cached across Stop)
    long m_musicFrames = 0;
    long m_musicCursor = 0;
    int  m_musicRate   = 44100;
    bool m_musicActive = false;
    char m_macMusicPath[300] = { 0 };

    inline bool MovieStreamBusy() { return g_loaded && !g_finished; }
}

void MenuMusicPC::FillStatic(s16* lpOut, int liFrames, void* /*lpUser*/)
{
    // Realtime XAudio2 callback: copy from the decoded buffer, LOOPING at the end.
    long li = 0;
    if (m_musicPcm != nullptr && m_musicActive && m_musicFrames > 0) {
        for (; li < liFrames; ++li) {
            if (m_musicCursor >= m_musicFrames) m_musicCursor = 0;   // loop
            *lpOut++ = m_musicPcm[m_musicCursor * 2 + 0];
            *lpOut++ = m_musicPcm[m_musicCursor * 2 + 1];
            ++m_musicCursor;
        }
    }
    for (; li < liFrames; ++li) { *lpOut++ = 0; *lpOut++ = 0; }
}

bool MenuMusicPC::PlaySpec(const char* lpacSpecName)
{
    // ContentSpec name -> the registry's zone-1 .SNS file (the same resident
    // chain the SNR lookup uses); then stream it from SOUND\STREAMS\.
    char lacSns[96] = { 0 };
    if (!StreamHeadersPC::ResolveBySpecName(lpacSpecName, nullptr, nullptr,
                                            lacSns, sizeof(lacSns)))
    {
        AUDIO_LOG << "[MenuMusic] spec '" << lpacSpecName << "' not in StreamsRegistry\n";
        return false;
    }
    char lacPath[160];
    std::snprintf(lacPath, sizeof(lacPath), "SOUND\\STREAMS\\%s", lacSns);
    return Play(lacPath);
}

bool MenuMusicPC::Play(const char* lpSnsPath)
{
    if (lpSnsPath == nullptr || lpSnsPath[0] == 0) return false;

    if (!(m_musicPcm != nullptr && std::strcmp(m_macMusicPath, lpSnsPath) == 0))
    {
        // A different track: decode it (replacing the cached one). Close the device
        // first when the fill could be ours -- DestroyVoice synchronises with the
        // callback, making the free safe (same discipline as the movie stream).
        if (AudioOutputPC::IsOpen() && !MovieStreamBusy())
            AudioOutputPC::Close();
        m_musicActive = false;
        if (m_musicPcm) { std::free(m_musicPcm); m_musicPcm = nullptr; }
        m_musicFrames = 0; m_macMusicPath[0] = 0;

        std::vector<std::int16_t> pcm;
        int liRate = 48000;
        if (!DecodeSnsFileToPcm(lpSnsPath, 2, "[MenuMusic]", pcm, liRate))
            return false;
        m_musicFrames = long(pcm.size() / 2);
        m_musicPcm = static_cast<s16*>(std::malloc(std::size_t(m_musicFrames) * 2 * sizeof(s16)));
        if (m_musicPcm == nullptr) { m_musicFrames = 0; return false; }
        std::memcpy(m_musicPcm, pcm.data(), std::size_t(m_musicFrames) * 2 * sizeof(s16));
        m_musicRate = liRate;
        std::strncpy(m_macMusicPath, lpSnsPath, sizeof(m_macMusicPath) - 1);
        m_macMusicPath[sizeof(m_macMusicPath) - 1] = 0;
        AUDIO_LOG << "[MenuMusic] decoded " << lpSnsPath << " (" << (int)m_musicFrames
                  << " frames, " << (int)(m_musicRate > 0 ? m_musicFrames / m_musicRate : 0)
                  << " s, rate " << m_musicRate << ", looping)\n";
    }

    m_musicCursor = 0;
    m_musicActive = true;
    Update();   // claim the output now if it is free
    return true;
}

void MenuMusicPC::Stop()
{
    if (!m_musicActive) return;
    m_musicActive = false;
    // Release the device only when the fill is (at most) ours -- never yank a
    // playing movie stream. The decoded PCM stays cached for the next Play.
    if (AudioOutputPC::IsOpen() && !MovieStreamBusy())
        AudioOutputPC::Close();
    AUDIO_LOG << "[MenuMusic] stopped\n";
}

void MenuMusicPC::Update()
{
    if (!m_musicActive || m_musicPcm == nullptr) return;
    if (MovieStreamBusy()) return;               // the movie stream owns the output
    if (AudioOutputPC::IsOpen() && AudioOutputPC::GetOpenSampleRate() != m_musicRate)
        AudioOutputPC::Close();                  // a finished movie left it at another rate
    if (!AudioOutputPC::IsOpen())
        AudioOutputPC::Open(m_musicRate, 2, &MenuMusicPC::FillStatic, nullptr);
    else
        AudioOutputPC::SetFill(&MenuMusicPC::FillStatic, nullptr);   // reclaim (idempotent)
}

bool MenuMusicPC::IsActive() { return m_musicActive; }

} // namespace CgsSystem
