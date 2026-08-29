// =====================================================================================
// rw::audio::core::Pcm16BigDec bodies.
//
// EARenderWare "rwaudio" 16-bit big-endian PCM stream decoder plug-in -- the simplest
// codec of the family (the stream is already raw interleaved signed 16-bit PCM).
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every
// offset, width and side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this
// TU. See Pcm16BigDec.h for the appended layout and Decoder.h for the shared base.
//   GetDecoderDesc @0x82B91E38
//   GetSize        @0x82B91E48
//   DecodeEvent    @0x82B951B8
//
// The `_savegprlr_28` / `_restgprlr_28` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so they
// are dropped.
// =====================================================================================

#include "rw/audio/core/Pcm16BigDec.h"
#include "rw/audio/core/DecoderRegistry.h" // complete DecoderDesc (the descriptor is DEFINED in this TU)

namespace rw
{
namespace audio
{
namespace core
{

// s16 -> f32 normalisation scale (flt_820ADBE8 in the X360 rodata): the PCM full-scale
// reciprocal 1/32768, exactly representable in f32 (the asm loads it once per call and
// multiplies each fcfid/frsp-converted sample by it).
static const f32 KF_PCM16_SCALE = 1.0f / 32768.0f;

// This codec's static registration descriptor (off_82F893CC, .data). DEFINED here
// (2026-08-25, faithful-audio-engine phase A2). XEX recovery (big-endian):
//   +0x00 0x82B91E48 Pcm16BigDec::GetSize   +0x04 0x82B91E58 CreateInstanceEvent
//         (0x82B91E58 hand-decoded from the XEX: mr r11,r3; li r10,0; li r3,1;
//          stw r10,0x34(r11); stw r10,0x38(r11); blr -- zero self+0x34/+0x38, return 1)
//   +0x08 0 (no ReleaseEvent)               +0x0C 0x82B951B8 DecodeEvent
//   +0x10 mpNext = 0                        +0x14 muId = 0x50364230 'P6B0'
// FLAG (host callback slots deferred): see the EaXmaDec.cpp descriptor note --
// the header stays the opaque zeroed span until the dispatch consumer lands.
extern "C" DecoderDesc off_82F893CC = { {0}, 0, 0x50364230u /* 'P6B0' */ };

// -------------------------------------------------------------------------------------
// GetDecoderDesc @0x82B91E38
// Hand back the address of this codec's static registration descriptor. Callers
// (CgsSound::Playback::GenericRwacFactory, DecoderRegistry) use it to register /
// instantiate the 16-bit big-endian PCM decoder.
// -------------------------------------------------------------------------------------
DecoderDesc *Pcm16BigDec::GetDecoderDesc()
{
    return &off_82F893CC;
}

// -------------------------------------------------------------------------------------
// GetSize @0x82B91E48
// Report the codec's per-instance allocation footprint: the required alignment (16) is
// written through the out-param and the fixed X360 instance size (60 bytes / 0x3C) is
// returned. The descriptor framework passes the instance pointer in r3, but the asm
// overwrites r3 with the return value without reading it, so it is unused here.
// -------------------------------------------------------------------------------------
s32 Pcm16BigDec::GetSize(Pcm16BigDec *pDecoder, u32 *puAlignment)
{
    (void)pDecoder;
    *puAlignment = 16;
    return 60;
}

// -------------------------------------------------------------------------------------
// DecodeEvent @0x82B951B8
// Codec decode callback (installed as Decoder::mpDecodeCallback). De-interleaves and
// normalises `iNumSamples` samples of every channel out of the raw interleaved s16 PCM
// stream into `pOutput`, and returns the sample count.
//
// When the current run is drained (miRemainingSamples <= 0), pull the next request
// descriptor and re-seed the PCM cursor: Request+0x00 is the base pointer of the request's
// interleaved PCM data, Request+0x08 its start sample, Request+0x0C its end sample. The
// flag byte at Request+0x10 (when clear) resets the streaming state first. A non-zero
// start advances the cursor by `start` interleaved frames (channelCount samples each) and
// shortens the run to end-start.
//
// Output is planar: channel `c` is written contiguously at pOutput->mpData +
// muStride*c, one f32 per sample; the source steps channelCount samples between the frames
// of a given channel (interleaved layout). Each s16 is normalised by 1/32768.
// -------------------------------------------------------------------------------------
s32 Pcm16BigDec::DecodeEvent(DecoderBuffer *pOutput, s32 iNumSamples)
{
    if (miRemainingSamples <= 0)
    {
        DecoderRequest *pDesc = GetCurrentRequestDesc();

        // Flag byte at Request+0x10: when clear, reset the streaming state for a fresh
        // request (immediately re-seeded below, matching the console code).
        if (!pDesc->mucContinue)
        {
            mpSampleCursor = nullptr;
            miRemainingSamples = 0;
        }

        // Request+0x00 holds the base pointer of this request's interleaved PCM data.
        s16 *pBase = const_cast<s16 *>(static_cast<const s16 *>(pDesc->mpFedData));
        mpSampleCursor = pBase;
        miRemainingSamples = pDesc->miEndSample;

        s32 iStart = pDesc->miStartSample;
        if (iStart != 0)
        {
            miRemainingSamples = pDesc->miEndSample - iStart;
            mpSampleCursor = pBase + mucChannelCount * iStart;
        }
    }

    // De-interleave + normalise raw s16 PCM into planar f32, one channel at a time.
    for (u32 uChannel = 0; uChannel < mucChannelCount; ++uChannel)
    {
        s16 *pSample = mpSampleCursor + uChannel;
        f32 *pChannelOut = pOutput->mpData + pOutput->muStride * uChannel;

        for (s32 i = 0; i < iNumSamples; ++i)
        {
            *pChannelOut++ = static_cast<f32>(*pSample) * KF_PCM16_SCALE;
            pSample += mucChannelCount;
        }
    }

    miRemainingSamples -= iNumSamples;
    mpSampleCursor += mucChannelCount * iNumSamples;
    return iNumSamples;
}

} // namespace core
} // namespace audio
} // namespace rw
