// =====================================================================================
// rw::audio::core::Xas1Dec bodies.
//
// EARenderWare "rwaudio" XAS (block-ADPCM) stream decoder plug-in. Reconstructed from
// BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative for every offset, width and
// side-effect. No Feb-2007 source and no DecFIGS DWARF exist for this TU. See Xas1Dec.h
// for the appended layout and Decoder.h for the shared streaming base.
//   GetDecoderDesc @0x82B91E90
//   DecodeEvent    @0x82B94308
//   DecodeChannel  @0x82B91EB8  -- reconstructed 2026-08-25 (the coefficient rodata
//                                  was recovered by the XasDec.cpp table measurement).
//
// The `_savegprlr_26` / `_restgprlr_26` calls in the pseudocode are the compiler's
// register save/restore prologue/epilogue helpers -- not source-level calls -- so they
// are dropped.
// =====================================================================================

#include "rw/audio/core/Xas1Dec.h"
#include "rw/audio/core/DecoderRegistry.h" // complete DecoderDesc (the descriptor is DEFINED in this TU)

#include <cstring> // memmove

namespace rw
{
namespace audio
{
namespace core
{

// This codec's static registration descriptor (off_82F8A544, .data). DEFINED here
// (2026-08-25, faithful-audio-engine phase A2). XEX recovery (big-endian):
//   +0x00 0x82B91E48 GetSize (ICF-shared with Pcm16BigDec::GetSize -- identical body)
//   +0x04 0x82B91EA0 CreateInstanceEvent (shared with XasDec -- identical body)
//   +0x08 0 (no ReleaseEvent)            +0x0C 0x82B94308 Xas1Dec::DecodeEvent
//   +0x10 mpNext = 0                     +0x14 muId = 0x58617331 'Xas1'
// FLAG (host callback slots deferred): see the EaXmaDec.cpp descriptor note --
// the header stays the opaque zeroed span until the dispatch consumer lands.
extern "C" DecoderDesc off_82F8A544 = { {0}, 0, 0x58617331u /* 'Xas1' */ };

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
        if (!pDesc->mucContinue)
        {
            miRemainingSamples = 0;
            mpEncodedCursor = 0;
        }

        // Request+0x00 holds the base pointer of this request's encoded XAS data.
        u8 *pDataBase = const_cast<u8 *>(static_cast<const u8 *>(pDesc->mpFedData));

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

// -------------------------------------------------------------------------------------
// XAS1 coefficient rodata -- the binary's per-TU DUPLICATE copies of the XAS tables
// (flt_8215A844 pair table / flt_8215A864 scale ladder), byte-identical to XasDec.cpp's
// KF_XAS0_FILTER_COEF @0x8215A7F0 / KF_XAS0_RESIDUAL_SCALE @0x8215A810 (see that TU's
// measurement notes; the duplication itself is the shipped `static const` per-TU copy,
// reproduced here 2026-08-25, faithful-audio-engine phase A).
// -------------------------------------------------------------------------------------
static const f32 KF_XAS1_FILTER_COEF[4][2] =
{
    { 0.0f,      0.0f      }, // 0x00000000, 0x00000000
    { 0.9375f,   0.0f      }, // 0x3F700000, 0x00000000   ( 240/256, 0)
    { 1.796875f, -0.8125f  }, // 0x3FE60000, 0xBF500000   ( 460/256, -208/256)
    { 1.53125f,  -0.859375f}, // 0x3FC40000, 0xBF5C0000   ( 392/256, -220/256)
};

static const f32 KF_XAS1_RESIDUAL_SCALE[13] =
{
    4.65661287e-10f, 2.32830644e-10f, 1.16415322e-10f, 5.82076609e-11f,
    2.91038305e-11f, 1.45519152e-11f, 7.27595761e-12f, 3.63797881e-12f,
    1.81898940e-12f, 9.09494702e-13f, 4.54747351e-13f, 2.27373675e-13f,
    1.13686838e-13f,
};

// Seed-sample dequantisation constant (flt_820ADBE8 = 2^-15 = 1/32768, shared image-wide).
static const f32 KF_XAS1_SEED_SCALE = 3.0517578125e-05f;

// -------------------------------------------------------------------------------------
// DecodeChannel @0x82B91EB8  (reconstructed 2026-08-25, faithful-audio-engine phase A --
// was declared-only pending the coefficient rodata, which the XasDec.cpp measurement
// recovered).
//
// Decode one channel's 128-sample XAS1 block (76 encoded bytes) into pOutput. The block
// is FOUR parallel sub-streams of 32 consecutive samples each (sub-stream j occupies
// pOutput[32j .. 32j+31]):
//   * 16 header bytes: per sub-stream {seed0 hdr, seed1 hdr} pairs -- each seed is the
//     upper 12 bits of a little-endian 16-bit word ((s8)byte1<<8 | byte0&0xF0, scaled by
//     1/32768); byte0's low nibble carries the filter index (seed0 header) and the
//     residual scale exponent (seed1 header);
//   * 60 residual bytes: 15 rows x 4 sub-stream bytes, two 4-bit residual nibbles per
//     byte (hi nibble = the earlier sample). Each nibble is pre-shifted to nibble<<28
//     (signed) and scaled by the 2^-(31+e) ladder entry -- full scale at e=0.
// Filter: out[n] = coef0*out[n-1] + coef1*out[n-2] + residual, history read back from
// the output buffer itself (the asm's stack seed copies are register staging only).
//
// The X360 epilogue returns without setting r3 (the _BYTE* return is dead -- the caller
// DecodeEvent advances its own cursor by 76); the natural next-block cursor is returned
// here for the same dead slot.
// -------------------------------------------------------------------------------------
u8 *Xas1Dec::DecodeChannel(u8 *pEncoded, f32 *pOutput)
{
    f32 lafCoef0[4];
    f32 lafCoef1[4];
    f32 lafScale[4];

    // --- the 16 header bytes: seeds + filter/scale selects, 4 bytes per sub-stream ---
    for (s32 iSub = 0; iSub < 4; ++iSub)
    {
        const u8 *pHdr = pEncoded + 4 * iSub;
        f32 *pSub = pOutput + 32 * iSub;

        const s32 iSeed0 = (static_cast<s32>(static_cast<s8>(pHdr[1])) << 8) +
                           (pHdr[0] & 0xF0);
        const u32 uFilter = pHdr[0] & 0xFu;              // clrlslwi ..,28,3 = idx*8 into the pair table
        lafCoef0[iSub] = KF_XAS1_FILTER_COEF[uFilter][0];
        lafCoef1[iSub] = KF_XAS1_FILTER_COEF[uFilter][1];
        pSub[0] = static_cast<f32>(iSeed0) * KF_XAS1_SEED_SCALE;

        const s32 iSeed1 = (static_cast<s32>(static_cast<s8>(pHdr[3])) << 8) +
                           (pHdr[2] & 0xF0);
        lafScale[iSub] = KF_XAS1_RESIDUAL_SCALE[pHdr[2] & 0xF];  // clrlslwi ..,28,2 = e*4 into the ladder
        pSub[1] = static_cast<f32>(iSeed1) * KF_XAS1_SEED_SCALE;
    }

    // --- the 60 residual bytes: 15 rows x 4 sub-streams, 2 samples per byte ---
    const u8 *pResidual = pEncoded + 16;
    for (s32 iRow = 0; iRow < 15; ++iRow)
    {
        for (s32 iSub = 0; iSub < 4; ++iSub)
        {
            const u8 uByte = *pResidual++;
            // Nibbles pre-shifted to nibble<<28, SIGNED (extlwi + extsw in the asm).
            const s32 iHi = static_cast<s32>(static_cast<u32>(uByte >> 4)  << 28);
            const s32 iLo = static_cast<s32>(static_cast<u32>(uByte & 0xF) << 28);

            f32 *p = pOutput + 32 * iSub + 1 + 2 * iRow;   // p[0] = out[n-1], p[-1] = out[n-2]
            const f32 fPrev2 = p[-1];
            const f32 fPrev1 = p[0];

            const f32 fOut1 = lafCoef0[iSub] * fPrev1 + lafCoef1[iSub] * fPrev2 +
                              static_cast<f32>(iHi) * lafScale[iSub];
            p[1] = fOut1;
            const f32 fOut2 = lafCoef0[iSub] * fOut1 + lafCoef1[iSub] * fPrev1 +
                              static_cast<f32>(iLo) * lafScale[iSub];
            p[2] = fOut2;
        }
    }

    return pEncoded + 76;
}

} // namespace core
} // namespace audio
} // namespace rw
