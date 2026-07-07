// ============================================================================
// SDKs/EATech/include/snd/EALayer3Reorder.cpp
//
// Out-of-line definitions for the Snd EALayer3 spectral-buffer helpers declared
// in EALayer3Reorder.h. Reconstructed from the X360 assembly; the buffers are
// signal-data float arrays, indexed directly.
// ============================================================================

#include "SDKs/EATech/include/snd/EALayer3Reorder.h"

namespace Snd
{
    // 0x82B79468. The X360 guards the block walk with an unsigned-wrap check
    // (base < base + 0x900) that is always taken for a valid buffer; the strength-
    // reduced count 0x900 / 0x120 collapses to a fixed 8 blocks of 72 floats. In
    // each block it negates the coefficient pairs (5,7), (13,15), ... (69,71) --
    // indices (5 + 8*pair) and (7 + 8*pair) for pair 0..8 -- by multiplying each
    // by -1.0f (the X360 loads flt_820037C8 = -1.0 and uses fmuls).
    f32* FrequencyInversionX4(f32* apData)
    {
        for (s32 liBlock = 0; liBlock < 8; ++liBlock)
        {
            f32* lpBlock = apData + liBlock * 72;
            for (s32 liPair = 0; liPair < 9; ++liPair)
            {
                lpBlock[5 + 8 * liPair] = -lpBlock[5 + 8 * liPair];
                lpBlock[7 + 8 * liPair] = -lpBlock[7 + 8 * liPair];
            }
        }
        return apData;
    }

    // 0x82B8A9B0. Unsigned-wrap guard on the destination span (a2 < a2 + 0x900),
    // always taken; the strength-reduced count collapses to 18 rows. For each row
    // and each of 9 sub-band groups, copy a run of 4 coefficients from the source
    // (row stride 4 floats, group stride 72 floats) to the destination (row stride
    // 32 floats, group stride 4 floats).
    f32* ReorderForFPolySynth(f32* apSrc, f32* apDst)
    {
        for (s32 liRow = 0; liRow < 18; ++liRow)
        {
            for (s32 liGroup = 0; liGroup < 9; ++liGroup)
            {
                for (s32 liCol = 0; liCol < 4; ++liCol)
                {
                    apDst[liRow * 32 + liGroup * 4 + liCol] =
                        apSrc[liRow * 4 + liGroup * 72 + liCol];
                }
            }
        }
        return apSrc;
    }

    // 0x82B89A28. Unsigned-wrap guard on the source span (result < result + 0x48),
    // always taken; 18 rows. For each row and each of 9 groups, gather a run of 4
    // coefficients from the source (row stride 1 float, group stride 72 floats,
    // in-run stride 18 floats) into the contiguous destination (row stride 4
    // floats, group stride 72 floats).
    f32* ReorderForVectoring(f32* apSrc, f32* apDst)
    {
        for (s32 liRow = 0; liRow < 18; ++liRow)
        {
            for (s32 liGroup = 0; liGroup < 9; ++liGroup)
            {
                for (s32 liCol = 0; liCol < 4; ++liCol)
                {
                    apDst[liRow * 4 + liGroup * 72 + liCol] =
                        apSrc[liRow + liGroup * 72 + 18 * liCol];
                }
            }
        }
        return apSrc;
    }
} // namespace Snd
