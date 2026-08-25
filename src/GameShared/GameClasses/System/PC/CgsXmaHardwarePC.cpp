// ============================================================================
// CgsXmaHardwarePC.cpp -- software XMA decoder behind the Xbox 360 XMA hardware
// HAL (SDKs/XAudio/XmaHardware.h). 2026-08-25, faithful-audio-engine phase A2.
//
// FLAG [PC platform leaf -- the sanctioned seam]: the X360 decodes XMA on a
// dedicated hardware unit driven through per-stream DMA contexts; the XDK's
// XMA* C API (imported by BURNOUT_X360_ARTIST.XEX, not part of the game image)
// is its driver surface. XmaHardware.h documents that "a PC implementation
// (e.g. a software XMA decoder behind the same API) supplies the bodies" --
// this TU is that implementation, built on the SAME proven FFmpeg
// AV_CODEC_ID_XMAFRAMES path as CgsMovieAudioPC.cpp / tools/audio/
// sns_xma_decode.cpp (verified against CRITERION.SNS). The bit-exact packet /
// frame extraction helpers are carried over from CgsMovieAudioPC.cpp verbatim
// where possible (they live in an anonymous namespace there, so they are
// duplicated here with this provenance note).
//
// Contract semantics implemented EXACTLY as the only in-tree consumer
// (rw::audio::core::EaXmaDec) drives them:
//  * two 2048-byte input packet buffers per context, fed alternately; the
//    "hardware" consumes a whole packet then clears that buffer's valid flag
//    (EaXmaDec::Service refills it and re-marks it valid);
//  * XMASetInputBufferReadOffset(bits) positions the first-frame bit inside
//    the NEXT parsed packet (the packet-header value: bits 11..25 + 32);
//  * the decoded-PCM output ring (6144 bytes, 24 x 256-byte hardware blocks)
//    holds NATIVE interleaved s16; XMAGetOutputBufferWriteOffset reports the
//    ring write position in 256-byte units (the caller scales << 8);
//  * XMASetOutputBufferReadOffset takes the subframe-quantised cursor the
//    caller computes as 4 * (bytes / (channels * 1024)) * channels -- inverted
//    here back to a ring byte position;
//  * the output-valid flag disambiguates the equal-offset case: valid == space
//    available (empty at equal offsets); cleared by the decoder when a write
//    fills the ring up to the read cursor (full at equal offsets) -- exactly
//    the (iAvailableBytes != 0 || !XMAIsOutputBufferValid) test in
//    EaXmaDec_wG_03.cpp;
//  * the two XmaContext status bytes (+0x08/+0x0C, bit 0x80) mirror the input
//    buffers' valid bits -- the EaXmaDec hardware-wedge probe reads them;
//  * decode progress runs synchronously inside the polling entries (the
//    enable/valid/write-offset calls) -- the software stand-in for the
//    asynchronous hardware unit.
// ============================================================================

#include "SDKs/XAudio/XmaHardware.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
}

#include <cstring>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>
#include <new>

namespace
{
    // --- XMA stream geometry (identical constants to CgsMovieAudioPC.cpp) ---
    const int kPacketBytes      = 2048;
    const int kPacketBits       = kPacketBytes * 8;
    const int kPacketHeaderBits = 32;
    const int kFrameSamples     = 512;   // one XMA frame decodes 512 samples/channel

    // Bit helpers (verbatim logic from CgsMovieAudioPC.cpp).
    std::uint32_t ReadBits(const std::uint8_t* data, std::size_t bit, unsigned n)
    {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < n; ++i)
            v = (v << 1) | ((data[(bit + i) / 8] >> (7 - ((bit + i) & 7))) & 1);
        return v;
    }
    void AppendBits(std::vector<std::uint8_t>& dst, const std::uint8_t* src,
                    std::size_t bit, std::size_t n)
    {
        for (std::size_t i = 0; i < n; ++i)
            dst.push_back(std::uint8_t((src[(bit + i) / 8] >> (7 - ((bit + i) & 7))) & 1));
    }

    // ------------------------------------------------------------------------
    // The software decoder context. The leading bytes ARE the public XmaContext
    // (the EaXmaDec wedge probe reads the two status bytes through that view);
    // the software state follows.
    // ------------------------------------------------------------------------
    struct XmaSoftContext
    {
        XmaContext mPublic;              // +0x00 -- the HAL view (status bytes @+0x08/+0x0C)

        // Program state (from XMAInitializeContext).
        const std::uint8_t* mpInput[2];  // the two 2048-byte packet buffers
        std::uint8_t*       mpOutput;    // the 6144-byte s16 ring
        std::uint32_t       muOutputBytes;
        std::uint32_t       muChannels;  // 1 or 2

        // Ring cursors (bytes). Valid flag disambiguates equal offsets.
        std::uint32_t muWriteBytes;
        std::uint32_t muReadBytes;
        bool          mbOutputValid;     // true == space available

        // Input state.
        bool          mabInputValid[2];  // packet pending in buffer N
        int           miCurrentInput;    // which buffer the decoder consumes next
        std::int32_t  miPendingBitOffset; // from XMASetInputBufferReadOffset, -1 = none
        bool          mbEnabled;

        // Cross-packet partial-frame carry (the packet continuation mechanism).
        std::vector<std::uint8_t> mPartialBits;
        std::size_t               muExpectedPartialBits;

        // Decoded-but-not-yet-ringed samples (frame granularity vs ring space).
        std::vector<std::int16_t> mStaged;   // interleaved s16
        std::size_t               muStagedCursor;

        // The FFmpeg xmaframes decoder instance.
        AVCodecContext* mpCodec;
        AVFrame*        mpFrame;
        AVPacket*       mpPacket;
    };

    void SyncStatusBytes(XmaSoftContext* c)
    {
        // Bit 0x80 of each status byte mirrors that input buffer's valid (pending)
        // state -- what the EaXmaDec recovery probe tests.
        c->mPublic.mucStatus08 = c->mabInputValid[0] ? 0x80 : 0x00;
        c->mPublic.mucStatus0C = c->mabInputValid[1] ? 0x80 : 0x00;
    }

    bool OpenCodec(XmaSoftContext* c)
    {
        if (c->mpCodec)
            return true;
        const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_XMAFRAMES);
        if (!codec)
            return false;
        c->mpCodec  = avcodec_alloc_context3(codec);
        c->mpFrame  = av_frame_alloc();
        c->mpPacket = av_packet_alloc();
        if (!c->mpCodec || !c->mpFrame || !c->mpPacket)
            return false;
        c->mpCodec->sample_rate = 48000;   // rate-neutral for the frame decode; a valid value is required
        av_channel_layout_default(&c->mpCodec->ch_layout, static_cast<int>(c->muChannels));
        return avcodec_open2(c->mpCodec, codec, 0) >= 0;
    }

    // Decode one extracted frame (bit vector) and append interleaved s16 to mStaged.
    void DecodeFrameBits(XmaSoftContext* c, const std::vector<std::uint8_t>& bits)
    {
        if (!OpenCodec(c))
            return;
        const std::size_t frameBytes = (bits.size() + 7) / 8;
        const std::uint8_t paddingEnd = std::uint8_t(frameBytes * 8 - bits.size());
        std::vector<std::uint8_t> enc(1 + frameBytes, 0);
        enc[0] = std::uint8_t(paddingEnd << 2);
        for (std::size_t i = 0; i < bits.size(); ++i)
            enc[1 + i / 8] |= std::uint8_t(bits[i] << (7 - (i & 7)));

        av_packet_unref(c->mpPacket);
        if (av_new_packet(c->mpPacket, int(enc.size())) < 0)
            return;
        std::memcpy(c->mpPacket->data, enc.data(), enc.size());
        if (avcodec_send_packet(c->mpCodec, c->mpPacket) < 0)
            return;
        if (avcodec_receive_frame(c->mpCodec, c->mpFrame) < 0)
            return;
        if (c->mpCodec->sample_fmt != AV_SAMPLE_FMT_FLTP || !c->mpFrame->data[0])
            return;

        const float* ch0 = reinterpret_cast<const float*>(c->mpFrame->data[0]);
        const float* ch1 = (c->muChannels == 2 && c->mpFrame->data[1])
                               ? reinterpret_cast<const float*>(c->mpFrame->data[1]) : 0;
        for (int i = 0; i < c->mpFrame->nb_samples; ++i)
        {
            long l = std::lrintf(ch0[i] * 32767.0f);
            c->mStaged.push_back(std::int16_t(std::clamp<long>(l, -32767, 32767)));
            if (c->muChannels == 2)
            {
                long r = std::lrintf((ch1 ? ch1[i] : ch0[i]) * 32767.0f);
                c->mStaged.push_back(std::int16_t(std::clamp<long>(r, -32767, 32767)));
            }
        }
        av_frame_unref(c->mpFrame);
    }

    // Parse ONE 2048-byte packet, decoding every completed frame; partial frames
    // carry across packets in mPartialBits (the continuation mechanism, logic
    // carried from CgsMovieAudioPC::ExtractFrames adapted to per-packet state).
    void ConsumePacket(XmaSoftContext* c, const std::uint8_t* packet)
    {
        const std::size_t continuationBits = ReadBits(packet, 6, 15);
        std::size_t bitOffset = kPacketHeaderBits;

        if (c->miPendingBitOffset >= 0)
        {
            // A seek positioned the first frame explicitly; discard carry state.
            bitOffset = std::size_t(c->miPendingBitOffset);
            c->miPendingBitOffset = -1;
            c->mPartialBits.clear();
            c->muExpectedPartialBits = 0;
        }
        else if (!c->mPartialBits.empty())
        {
            const std::size_t available = kPacketBits - bitOffset;
            const bool continuationOnly = continuationBits >= available;
            const std::size_t payload = std::min<std::size_t>(continuationBits, available);
            std::size_t copied = 0;
            if (c->muExpectedPartialBits == 0 && c->mPartialBits.size() < 15)
            {
                const std::size_t need = 15 - c->mPartialBits.size();
                const std::size_t chunk = std::min(need, payload);
                AppendBits(c->mPartialBits, packet, bitOffset, chunk);
                bitOffset += chunk; copied += chunk;
                if (c->mPartialBits.size() == 15)
                {
                    std::size_t fb = 0;
                    for (unsigned i = 0; i < 15; ++i) fb = (fb << 1) | c->mPartialBits[i];
                    c->muExpectedPartialBits = fb;
                }
            }
            if (c->muExpectedPartialBits != 0 && c->mPartialBits.size() < c->muExpectedPartialBits)
            {
                const std::size_t need = c->muExpectedPartialBits - c->mPartialBits.size();
                const std::size_t chunk = std::min(need, payload - copied);
                AppendBits(c->mPartialBits, packet, bitOffset, chunk);
                bitOffset += chunk;
            }
            if (c->muExpectedPartialBits != 0 &&
                c->mPartialBits.size() >= c->muExpectedPartialBits)
            {
                DecodeFrameBits(c, c->mPartialBits);
                c->mPartialBits.clear();
                c->muExpectedPartialBits = 0;
            }
            if (continuationOnly)
                return;
        }

        while (bitOffset + 15 <= std::size_t(kPacketBits))
        {
            const std::uint32_t frameBits = ReadBits(packet, bitOffset, 15);
            if (frameBits == 0 || frameBits == 0x7FFF)
                break;
            const std::size_t remaining = kPacketBits - bitOffset;
            if (frameBits > remaining)
            {
                c->mPartialBits.clear();
                AppendBits(c->mPartialBits, packet, bitOffset, remaining);
                c->muExpectedPartialBits = frameBits;
                break;
            }
            std::vector<std::uint8_t> frame;
            AppendBits(frame, packet, bitOffset, frameBits);
            const bool moreFrames = frame.back() != 0;
            DecodeFrameBits(c, frame);
            bitOffset += frameBits;
            if (!moreFrames)
                break;
        }
    }

    // The synchronous "hardware": move staged PCM into the ring while space
    // allows, consuming input packets as needed.
    void RunDecoder(XmaSoftContext* c)
    {
        if (!c->mbEnabled || !c->mpOutput || c->muOutputBytes == 0)
            return;

        for (;;)
        {
            // 1) Drain staged samples into the ring.
            while (c->muStagedCursor < c->mStaged.size())
            {
                if (!c->mbOutputValid && c->muWriteBytes == c->muReadBytes)
                    return;   // ring full
                const std::uint32_t freeBytes =
                    (c->muReadBytes + c->muOutputBytes - c->muWriteBytes - 1) % c->muOutputBytes + 1;
                std::uint32_t chunk = std::uint32_t(
                    std::min<std::size_t>((c->mStaged.size() - c->muStagedCursor) * 2, freeBytes));
                chunk &= ~1u;
                if (chunk == 0)
                    return;
                const std::uint32_t untilWrap = c->muOutputBytes - c->muWriteBytes;
                const std::uint32_t now = std::min(chunk, untilWrap);
                std::memcpy(c->mpOutput + c->muWriteBytes,
                            c->mStaged.data() + c->muStagedCursor, now);
                c->muWriteBytes = (c->muWriteBytes + now) % c->muOutputBytes;
                c->muStagedCursor += now / 2;
                if (c->muWriteBytes == c->muReadBytes)
                    c->mbOutputValid = false;   // just became full
            }
            if (c->muStagedCursor >= c->mStaged.size())
            {
                c->mStaged.clear();
                c->muStagedCursor = 0;
            }

            // 2) Need more decoded data: consume the next pending input packet.
            const int in = c->miCurrentInput;
            if (!c->mabInputValid[in] || !c->mpInput[in])
                return;   // starved until Service refills
            ConsumePacket(c, c->mpInput[in]);
            c->mabInputValid[in] = false;    // packet consumed -> buffer needs refill
            c->miCurrentInput = in ^ 1;
            SyncStatusBytes(c);
            if (c->mStaged.empty())
                return;   // packet produced nothing rendable (pure continuation)
        }
    }
} // namespace

// ---------------------------------------------------------------------------
// The extern "C" HAL surface (signatures verbatim from SDKs/XAudio/XmaHardware.h).
// ---------------------------------------------------------------------------
extern "C"
{

s32 XMACreateContext(XmaContext **ppContext)
{
    XmaSoftContext* c = new (std::nothrow) XmaSoftContext();
    if (!c)
        return -1;
    c->mpInput[0] = 0; c->mpInput[1] = 0;
    c->mpOutput = 0; c->muOutputBytes = 0; c->muChannels = 1;
    c->muWriteBytes = 0; c->muReadBytes = 0; c->mbOutputValid = true;
    c->mabInputValid[0] = false; c->mabInputValid[1] = false;
    c->miCurrentInput = 0; c->miPendingBitOffset = -1; c->mbEnabled = false;
    c->muExpectedPartialBits = 0; c->muStagedCursor = 0;
    c->mpCodec = 0; c->mpFrame = 0; c->mpPacket = 0;
    std::memset(&c->mPublic, 0, sizeof(c->mPublic));
    *ppContext = &c->mPublic;
    return 0;
}

void XMAReleaseContext(XmaContext *pContext)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    if (!c)
        return;
    if (c->mpPacket) av_packet_free(&c->mpPacket);
    if (c->mpFrame)  av_frame_free(&c->mpFrame);
    if (c->mpCodec)  avcodec_free_context(&c->mpCodec);
    delete c;
}

s32 XMAInitializeContext(XmaContext *pContext, const XmaContextInit *pInit)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    c->mpInput[0]    = static_cast<const std::uint8_t*>(pInit->mpInputBuffer0);
    c->mpInput[1]    = static_cast<const std::uint8_t*>(pInit->mpInputBuffer1);
    c->mpOutput      = static_cast<std::uint8_t*>(pInit->mpOutputBuffer);
    c->muOutputBytes = pInit->muOutputBufferBlocks * 256u;
    c->muChannels    = pInit->muChannelsMinusOne + 1u;
    c->muWriteBytes  = 0;
    c->muReadBytes   = 0;
    c->mbOutputValid = true;
    c->mabInputValid[0] = false;
    c->mabInputValid[1] = false;
    c->miCurrentInput = 0;
    c->miPendingBitOffset = -1;
    c->mPartialBits.clear();
    c->muExpectedPartialBits = 0;
    c->mStaged.clear();
    c->muStagedCursor = 0;
    if (c->mpCodec)
    {
        // Re-init may change the channel pairing; rebuild the codec lazily.
        avcodec_free_context(&c->mpCodec);
        c->mpCodec = 0;
    }
    SyncStatusBytes(c);
    return 0;
}

s32 XMAEnableContext(XmaContext *pContext)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    c->mbEnabled = true;
    RunDecoder(c);
    return 0;
}

s32 XMADisableContext(XmaContext *pContext, s32 /*iBlockIfBusy*/)
{
    // Synchronous software decode: nothing is ever mid-flight, so "block if
    // busy" is trivially satisfied.
    reinterpret_cast<XmaSoftContext*>(pContext)->mbEnabled = false;
    return 0;
}

u32 XMAGetOutputBufferWriteOffset(XmaContext *pContext)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    RunDecoder(c);   // the poll is the software unit's forward progress
    return c->muWriteBytes >> 8;   // 256-byte hardware blocks
}

u32 XMAGetOutputBufferReadOffset(XmaContext *pContext)
{
    // Report in the caller's subframe units: 4 * (bytes / (channels*1024)) * channels.
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    const u32 ch = c->muChannels ? c->muChannels : 1;
    return 4u * (c->muReadBytes / (ch * 1024u)) * ch;
}

s32 XMASetOutputBufferReadOffset(XmaContext *pContext, u32 uReadOffset)
{
    // Invert the caller's subframe quantisation back to ring bytes.
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    const u32 ch = c->muChannels ? c->muChannels : 1;
    c->muReadBytes = (uReadOffset / (4u * ch)) * (ch * 1024u);
    if (c->muOutputBytes)
        c->muReadBytes %= c->muOutputBytes;
    return 0;
}

s32 XMAIsOutputBufferValid(XmaContext *pContext)
{
    return reinterpret_cast<XmaSoftContext*>(pContext)->mbOutputValid ? 1 : 0;
}

s32 XMASetOutputBufferValid(XmaContext *pContext)
{
    reinterpret_cast<XmaSoftContext*>(pContext)->mbOutputValid = true;
    return 0;
}

s32 XMASetInputBuffer0(XmaContext *pContext, const void *pBuffer, u32 /*uNumBlocks*/)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    c->mpInput[0] = static_cast<const std::uint8_t*>(pBuffer);
    return 0;
}

s32 XMASetInputBuffer1(XmaContext *pContext, const void *pBuffer, u32 /*uNumBlocks*/)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    c->mpInput[1] = static_cast<const std::uint8_t*>(pBuffer);
    return 0;
}

s32 XMAIsInputBuffer0Valid(XmaContext *pContext)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    RunDecoder(c);
    return c->mabInputValid[0] ? 1 : 0;
}

s32 XMAIsInputBuffer1Valid(XmaContext *pContext)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    RunDecoder(c);
    return c->mabInputValid[1] ? 1 : 0;
}

s32 XMASetInputBuffer0Valid(XmaContext *pContext)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    c->mabInputValid[0] = true;
    SyncStatusBytes(c);
    return 0;
}

s32 XMASetInputBuffer1Valid(XmaContext *pContext)
{
    XmaSoftContext* c = reinterpret_cast<XmaSoftContext*>(pContext);
    c->mabInputValid[1] = true;
    SyncStatusBytes(c);
    return 0;
}

s32 XMASetInputBufferReadOffset(XmaContext *pContext, u32 uOffsetInBits)
{
    reinterpret_cast<XmaSoftContext*>(pContext)->miPendingBitOffset =
        std::int32_t(uOffsetInBits);
    return 0;
}

} // extern "C"
