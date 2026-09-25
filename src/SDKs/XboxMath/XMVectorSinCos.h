#pragma once

// XMVectorSinCos -- the Xbox 360 XDK math library's sine / cosine pair. The console has no out-of-line copy: every
// caller inlines it and loads its tables from the same rodata rows. Read at CameraRig::Construct @0x8220B0E8, which
// inlines it three times, instruction for instruction the same each time (yaw 0x8220B280..0x8220B3F0, pitch
// 0x8220B4F4..0x8220B610, roll 0x8220B710..0x8220B830). The game's callers read ONE lane of the result (the angle is
// a splat), so this is the per-lane arithmetic, operation for operation. [FX-DIRECTOR2 2026-09-25]
//
// The console body (the campaign rounding rule, scratch/CRASHPARITY_0922/ROUNDING_RULE.md: vmaddfp / vnmsubfp are
// FUSED, one rounding -- rule 3; vmulfp is rounded on its own -- rule 4; IDA prints the classic vmaddfp operands in
// field order D, A, B, C, which computes D = A * C + B):
//   0x8220B280 q  = V * (1 / 2pi)                  vmulfp128
//   0x8220B284 n  = vrfin(q)                       round to the nearest integer, ties to even
//   0x8220B288 V1 = -(2pi * n - V)                 vnmsubfp: rounded once, then negated
//   0x8220B290.. the powers, each ONE vmulfp128 of two earlier powers:
//       V2 = V1 V1   V3 = V2 V1   V4 = V2 V2   V5 = V3 V2   V6 = V3 V3   V7 = V4 V3   V8 = V4 V4
//       V9 = V5 V4   V10 = V5 V5  V11 = V6 V5  V12 = V6 V6  V13 = V7 V6  V14 = V7 V7  V15 = V8 V7
//       V16 = V8 V8  V17 = V9 V8  V18 = V9 V9  V19 = V10 V9 V20 = V10 V10 V21 = V11 V10 V22 = V11 V11
//       V23 = V12 V11
//   sin = V1, then + S1 V3, + S2 V5, ..., + S11 V23      eleven vmaddfp, in that order (0x8220B2A8 .. 0x8220B390)
//   cos = 1.0, then + C1 V2, + C2 V4, ..., + C11 V22     eleven vmaddfp / vmaddfp128 (0x8220B354 .. 0x8220B3F0);
//                                                         the 1.0 is the w lane of a vupkd3d128 of zero
// The tables, read from the image (x360rd):
//   0x82000BD0: 3F800000 BE2AAAAB 3C088889 B9500D01     1, S1, S2, S3   (the leading 1 is not read)
//   0x82000BE0: 3638EF1D B2D7322B 2F309231 AB573F9F     S4 .. S7
//   0x82000BF0: 274A963C A317A4DA 1EB8DC78 9A3B0DA1     S8 .. S11
//   0x82000C00: 3F800000 BF000000 3D2AAAAB BAB60B61     1, C1, C2, C3   (the leading 1 is not read)
//   0x82000C10: 37D00D01 B493F27E 310F76C8 AD49CBA5     C4 .. C7
//   0x82000C20: 29573F9F A53413C3 20F2A15D 9C8671CB     C8 .. C11
//   0x82000C60: 40490FDB 40C90FDB 3EA2F983 3E22F983     pi, 2pi, 1/pi, 1/2pi  (2pi and 1/2pi are read)
// They are the textbook alternating 1/n! series, so on [-pi, pi] the pair is the degree-23 / degree-22 Taylor
// polynomial -- NOT std::sin / std::cos, which it misses by an ulp or so over much of the range.
//
// FLAG (PC-platform): the VMX's flush of denormal operands and results to zero is not modelled (rule 6); it can
// only matter for |V| below 1.2e-38. A NaN, an infinity or a |V| large enough to overflow the powers gives a NaN
// (its payload is not modelled).
#include <cmath>

namespace XboxMath
{
    namespace XMVectorSinCosDetail
    {
        inline float Bits(unsigned int luBits)
        {
            union { unsigned int mu; float mf; } lValue;
            lValue.mu = luBits;
            return lValue.mf;
        }

        // vnmsubfp: -(a * c - b), rounded ONCE and then negated -- so an exact cancellation is -0; a QNaN keeps its
        // sign (the EffectsModule.cpp Vnmsub convention).
        inline float NegativeMultiplySubtract(float lfA, float lfC, float lfB)
        {
            const float lfDifference = std::fmaf(lfA, lfC, -lfB);
            return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
        }
    }

    inline void XMVectorSinCos(float* lpfSin, float* lpfCos, float lfV)
    {
        using XMVectorSinCosDetail::Bits;
        const float kfTwoPi           = Bits(0x40C90FDBu);   // 0x82000C64
        const float kfReciprocalTwoPi = Bits(0x3E22F983u);   // 0x82000C6C
        const float kfOne             = 1.0f;                // vupkd3d128 lane w

        const float kafSin[11] = {
            Bits(0xBE2AAAABu), Bits(0x3C088889u), Bits(0xB9500D01u),                       // 0x82000BD4..
            Bits(0x3638EF1Du), Bits(0xB2D7322Bu), Bits(0x2F309231u), Bits(0xAB573F9Fu),    // 0x82000BE0..
            Bits(0x274A963Cu), Bits(0xA317A4DAu), Bits(0x1EB8DC78u), Bits(0x9A3B0DA1u),    // 0x82000BF0..
        };
        const float kafCos[11] = {
            Bits(0xBF000000u), Bits(0x3D2AAAABu), Bits(0xBAB60B61u),                       // 0x82000C04..
            Bits(0x37D00D01u), Bits(0xB493F27Eu), Bits(0x310F76C8u), Bits(0xAD49CBA5u),    // 0x82000C10..
            Bits(0x29573F9Fu), Bits(0xA53413C3u), Bits(0x20F2A15Du), Bits(0x9C8671CBu),    // 0x82000C20..
        };

        // XMVectorModAngles: reduce to [-pi, pi].
        const float lfQuotient = std::nearbyint(lfV * kfReciprocalTwoPi);
        float lafV[24];
        lafV[1]  = XMVectorSinCosDetail::NegativeMultiplySubtract(kfTwoPi, lfQuotient, lfV);
        lafV[2]  = lafV[1] * lafV[1];
        lafV[3]  = lafV[2] * lafV[1];
        lafV[4]  = lafV[2] * lafV[2];
        lafV[5]  = lafV[3] * lafV[2];
        lafV[6]  = lafV[3] * lafV[3];
        lafV[7]  = lafV[4] * lafV[3];
        lafV[8]  = lafV[4] * lafV[4];
        lafV[9]  = lafV[5] * lafV[4];
        lafV[10] = lafV[5] * lafV[5];
        lafV[11] = lafV[6] * lafV[5];
        lafV[12] = lafV[6] * lafV[6];
        lafV[13] = lafV[7] * lafV[6];
        lafV[14] = lafV[7] * lafV[7];
        lafV[15] = lafV[8] * lafV[7];
        lafV[16] = lafV[8] * lafV[8];
        lafV[17] = lafV[9] * lafV[8];
        lafV[18] = lafV[9] * lafV[9];
        lafV[19] = lafV[10] * lafV[9];
        lafV[20] = lafV[10] * lafV[10];
        lafV[21] = lafV[11] * lafV[10];
        lafV[22] = lafV[11] * lafV[11];
        lafV[23] = lafV[12] * lafV[11];

        float lfSin = lafV[1];
        float lfCos = kfOne;
        for (int liTerm = 0; liTerm < 11; ++liTerm)
        {
            lfSin = std::fmaf(kafSin[liTerm], lafV[3 + 2 * liTerm], lfSin);
            lfCos = std::fmaf(kafCos[liTerm], lafV[2 + 2 * liTerm], lfCos);
        }
        *lpfSin = lfSin;
        *lpfCos = lfCos;
    }
}
