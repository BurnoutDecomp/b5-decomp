#pragma once

// XMVectorCos -- the Xbox 360 XDK math library's cosine, X360 0x821F06B0 (one out-of-line copy, a 36-instruction
// leaf, 23 `bl` sites in the ARTIST image). Caller here: the car-attached camera's traffic push-out,
// FrustrumCollisionResolver::GetHeightAboveTraffic @0x821F90F0 (owner's list 2026-09-28, lane L1, piece 6b). Every
// lane is computed alone, so this is the per-lane arithmetic, operation for operation.
//
// It is NOT XMVectorSinCos's cosine (SDKs/XboxMath/XMVectorSinCos.h): the same tables and reduction, but its even
// powers are built as V6 = V4 V2, V10 = V6 V4, V14 = V8 V6, V18 = V10 V8, V22 = V12 V10 (the SinCos inline squares the
// odd powers instead: V6 = V3 V3, ...), so the two round differently.
//
// The console body (vmaddfp / vnmsubfp are FUSED: one rounding; IDA prints classic vmaddfp in the raw field order
// D, A, B, C, which computes D = A * C + B, and vnmsubfp D = -(A * C - B)):
//   0x821F06D8 q  = V * (1 / 2pi)                  vmulfp128
//   0x821F0710 n  = vrfin(q)                       round to the nearest integer, ties to even
//   0x821F0720 V1 = -(2pi * n - V)                 vnmsubfp: rounded once, then negated
//   0x821F0724.. V2 = V1 V1, V4 = V2 V2, V6 = V4 V2, V8 = V4 V4, V10 = V6 V4, V12 = V6 V6, V14 = V8 V6,
//                V16 = V8 V8, V18 = V10 V8, V20 = V10 V10, V22 = V12 V10     each one vmulfp128
//   0x821F0728.. cos = 1.0 + C1 V2, then + C2 V4, ..., + C11 V22             eleven vmaddfp, in that order; the 1.0
//                                                                            is the w lane of a vupkd3d128 of zero
// The tables, read from the image (x360rd) -- the rows XMVectorSinCos.h cites:
//   0x82000C00: 3F800000 BF000000 3D2AAAAB BAB60B61     1, C1, C2, C3   (the leading 1 is not read)
//   0x82000C10: 37D00D01 B493F27E 310F76C8 AD49CBA5     C4 .. C7
//   0x82000C20: 29573F9F A53413C3 20F2A15D 9C8671CB     C8 .. C11
//   0x82000C60: 40490FDB 40C90FDB 3EA2F983 3E22F983     pi, 2pi, 1/pi, 1/2pi  (2pi and 1/2pi are read)
//
// FLAG (PC-platform): the VMX's flush of denormal operands and results to zero is not modelled (rule 6); it can only
// matter for |V| below 1.2e-38, whose cosine is 1 either way. A NaN, an infinity or a |V| large enough to overflow
// the powers gives a NaN (its payload is not modelled).
#include <cmath>

namespace XboxMath
{
    namespace XMVectorCosDetail
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

    inline float XMVectorCos(float lfV)
    {
        using XMVectorCosDetail::Bits;
        const float kfTwoPi           = Bits(0x40C90FDBu);   // 0x82000C64
        const float kfReciprocalTwoPi = Bits(0x3E22F983u);   // 0x82000C6C
        const float kfOne             = 1.0f;                // vupkd3d128 lane w (0x821F06BC)
        const float kfC1  = Bits(0xBF000000u), kfC2  = Bits(0x3D2AAAABu), kfC3  = Bits(0xBAB60B61u);   // 0x82000C04..
        const float kfC4  = Bits(0x37D00D01u), kfC5  = Bits(0xB493F27Eu), kfC6  = Bits(0x310F76C8u);   // 0x82000C10..
        const float kfC7  = Bits(0xAD49CBA5u);
        const float kfC8  = Bits(0x29573F9Fu), kfC9  = Bits(0xA53413C3u), kfC10 = Bits(0x20F2A15Du);   // 0x82000C20..
        const float kfC11 = Bits(0x9C8671CBu);

        // XMVectorModAngles: reduce to [-pi, pi].
        const float lfQuotient = std::nearbyint(lfV * kfReciprocalTwoPi);
        const float lfV1  = XMVectorCosDetail::NegativeMultiplySubtract(kfTwoPi, lfQuotient, lfV);
        const float lfV2  = lfV1 * lfV1;
        const float lfV4  = lfV2 * lfV2;
        const float lfV6  = lfV4 * lfV2;
        const float lfV8  = lfV4 * lfV4;
        const float lfV10 = lfV6 * lfV4;
        const float lfV12 = lfV6 * lfV6;
        const float lfV14 = lfV8 * lfV6;
        const float lfV16 = lfV8 * lfV8;
        const float lfV18 = lfV10 * lfV8;
        const float lfV20 = lfV10 * lfV10;
        const float lfV22 = lfV12 * lfV10;

        float lfResult = std::fmaf(kfC1, lfV2, kfOne);
        lfResult = std::fmaf(kfC2, lfV4, lfResult);
        lfResult = std::fmaf(kfC3, lfV6, lfResult);
        lfResult = std::fmaf(kfC4, lfV8, lfResult);
        lfResult = std::fmaf(kfC5, lfV10, lfResult);
        lfResult = std::fmaf(kfC6, lfV12, lfResult);
        lfResult = std::fmaf(kfC7, lfV14, lfResult);
        lfResult = std::fmaf(kfC8, lfV16, lfResult);
        lfResult = std::fmaf(kfC9, lfV18, lfResult);
        lfResult = std::fmaf(kfC10, lfV20, lfResult);
        lfResult = std::fmaf(kfC11, lfV22, lfResult);
        return lfResult;
    }
}
