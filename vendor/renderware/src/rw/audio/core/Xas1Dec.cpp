// =====================================================================================
// rw::audio::core::Xas1Dec bodies.
//
// EARenderWare "rwaudio" XAS (block-ADPCM) stream decoder plug-in. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every offset, width and
// side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this TU. See Xas1Dec.h
// for the appended layout and Decoder.h for the shared streaming base.
//   GetDecoderDesc @0x82B91E90
//   DecodeEvent    @0x82B94308
//   DecodeChannel  @0x82B91EB8  -- BLOCKED (needs un-recovered XAS coefficient rodata),
//                                  defined in its own TU; only declared in Xas1Dec.h.
//
// The `_savegprlr_26` / `_restgprlr_26` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so they
// are dropped.
// =====================================================================================

#include "rw/audio/core/Xas1Dec.h"

#include <cstring> // memmove

namespace rw
{
namespace audio
{
namespace core
{

// This codec's static registration descriptor (off_82F8A544 in the X360 rodata). Its
// bytes (name / GUID / factory callbacks) are not recovered here; GetDecoderDesc only
// returns its address, and DecoderDesc is an incomplete type, so no contents are needed
// (nor fabricated). Same pattern as the foreign statics reached from Decoder.cpp.
extern "C" DecoderDesc off_82F8A544;

// -------------------------------------------------------------------------------------
// GetDecoderDesc @0x82B91E90
// Hand back the address of this codec's static registration descriptor. Callers
// (DecoderRegistry::RegisterStandardRunTimeDecoders, the AEMS sample factories) use it to
// register / instantiate the XAS decoder.
// -------------------------------------------------------------------------------------
DecoderDesc *Xas1Dec::GetDecoderDesc()
{
    return &off_82F8A544;
}

// -------------------------------------------------------------------------------------
// DecodeEvent @0x82B94308
// Codec decode callback (installed as Decoder::mpDecodeCallback). Produces one 128-sample
// block across all interleaved channels into `pOutput` and returns the sample count.
//
// When the current request is drained (miRemainingSamples <= 0), pull the next request
// descriptor and re-seed the XAS frame cursor: the request's start sample selects the
// 128-sample block (frame index = startSample / 128), and the encoded cursor lands at
// the request's data base plus that block's byte span (76 bytes per channel per block).
// `iSkip` is how many samples at the front of the decoded block precede the request's
// start; those are shifted out per channel. The first block of a request thus yields
// (128 - iSkip) samples, every later block yields a full 128.
// -------------------------------------------------------------------------------------
s32 Xas1Dec::DecodeEvent(DecoderBuffer *pOutput)
{
    s32 iSkip = 0;

    if (miRemainingSamples <= 0)
    {
        DecoderRequest *pDesc = GetCurrentRequestDesc();

        // Flag byte at Request+0x10: when clear, reset the streaming state for a fresh
        // request; when set, keep decoding into the same run.
        if (!pDesc->muReserved10)
        {
            miRemainingSamples = 0;
            mpEncodedCursor = 0;
        }

        // Request+0x00 holds the base pointer of this request's encoded XAS data.
        u8 *pDataBase = reinterpret_cast<u8 *>(
            static_cast<usize>(pDesc->muReserved00));

        mpEncodedCursor = pDataBase;

        s32 iBlockIndex = pDesc->miStartSample / 128;
        mpEncodedCursor = pDataBase + 76 * mucChannelCount * iBlockIndex;

        iSkip = pDesc->miStartSample - (iBlockIndex << 7);
        miRemainingSamples = pDesc->miEndSample - pDesc->miStartSample;
    }

    if (mucChannelCount)
    {
        u32 uChannel = 0;
        do
        {
            f32 *pChannelOut = pOutput->mpData + pOutput->muStride * uChannel;

            DecodeChannel(mpEncodedCursor, pChannelOut);
            mpEncodedCursor += 76;

            // Drop the iSkip leading samples that precede the request's start.
            if (iSkip > 0)
                memmove(pChannelOut, pChannelOut + iSkip,
                        (128 - iSkip) * sizeof(f32));

            ++uChannel;
        } while (uChannel < mucChannelCount);
    }

    s32 iProduced = 128;
    if (iSkip > 0)
        iProduced = 128 - iSkip;

    miRemainingSamples -= iProduced;
    return iProduced;
}

} // namespace core
} // namespace audio
} // namespace rw
