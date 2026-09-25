#pragma once

// XMVectorATan -- the Xbox 360 XDK math library's inverse tangent, X360 0x821F0A70 (one out-of-line copy, a
// 101-instruction leaf; its caller here is the SDK's axis / angle query inside rw::math::vpu::SLerp, bl @0x822166DC).
// Every lane is computed alone, so this is the per-lane arithmetic, operation for operation (FX-GATE, crash parity
// 2026-09-25). The algorithm is Cody and Waite's: reduce |x| > 1 to its reciprocal (+ pi/2), then F > 2 - sqrt(3) to
// ((sqrt(3) - 1) F - 1) / (F + sqrt(3)) (+ pi/6 or pi/3), then the rational P(G) G / Q(G) of G = F * F.
//
// The console body (vmaddfp / vnmsubfp are FUSED: one rounding; IDA prints classic vmaddfp in the raw field order
// D, A, B, C, which computes D = A * C + B, and vnmsubfp D = -(A * C - B)):
//   0x821F0AE4 F    = |x|                       (vandc of the sign bit)
//   0x821F0AF0 gt1  = F > 1                     (vcmpgtfp; the 1.0 is vupkd3d128's w lane, 0x821F0AA4)
//   0x821F0AF8..0x821F0B20  1 / F: vrefp e, e1 = e * (1 - F e) + e, e2 = e1 * (1 - F e1) + e1, and e2 only
//                           where e1 is not NaN (vcmpeqfp e1, e1 ; vsel) -- the SDK's XMVectorReciprocal
//   0x821F0B04 / 0x821F0B0C angle2 = gt1 ? pi/3 : pi/6 ; angle1 = gt1 ? pi/2 : 0
//   0x821F0B24 F    = gt1 ? 1 / F : F
//   0x821F0B30 gt   = F > 2 - sqrt(3)
//   0x821F0B34 FA   = F * (sqrt(3) - 1) + F, then + -1.0   (0x821F0B3C; -1.0 = vcfsx of vspltisw -1)
//   0x821F0B28 FB   = F + sqrt(3), 0x821F0B40..0x821F0B5C its reciprocal (the same guarded two steps)
//   0x821F0B60 / 0x821F0B64 F = gt ? FA * (1 / FB) : F ; 0x821F0B68 angle = gt ? angle2 : angle1
//   0x821F0B6C G    = F * F
//   0x821F0B70..0x821F0BA8  D = G + Q3 ; N = G * P3 + P2 ; D = G * D + Q2 ; N = G * N + P1 ; D = G * D + Q1 ;
//                           N = G * N + P0 ; D = G * D + Q0 ; N = N * G
//   0x821F0BAC..0x821F0BCC  R = N * (1 / D) (the same guarded two-step reciprocal)
//   0x821F0BD0 R    = F * R + F ; 0x821F0BD4 R = (|F| < 2^-12) ? F : R   (vcmpgtfp eps, |F|)
//   0x821F0BE0 R    = gt1 ? -R : R ; 0x821F0BE8 R = R + angle ; 0x821F0BF4 R = (x < 0) ? -R : R
//   0x821F0BF8 / 0x821F0BFC  R = (x > MaxV) ? pi/2 : R ; R = (x < -MaxV) ? -pi/2 : R
// The constant splats, read from the image (x360rd):
//   P = 0x82001C50: C15B0533 C1A40BFE C107E9FB BF566BD7
//   Q = 0x82001C60: 422443E6 42AC5090 426E5052 4170624F
//   0x82001C70: 3FDDB3D7 (sqrt 3) 3F3B67AF (sqrt 3 - 1) 3E8930A3 (2 - sqrt 3) 39800000 (epsilon 2^-12)
//   0x82001C80: 3FC90FDB (pi/2) 3F860A92 (pi/3) 3F060A92 (pi/6) 7E800000 (MaxV 8.5e37)
// Negations are vxor of the sign bit (a NaN's sign flips too); every compare is false on a NaN.
//
// FLAG (estimate model): vrefp is modelled as the exactly rounded 1 / x -- the tree's standing convention for the
// VMX estimates -- with its specials (+-0 -> +-inf, +-inf -> +-0). Not modelled: the VMX non-Java mode's flush of
// denormal operands and results to zero (ROUNDING_RULE 6).
// So it is NOT std::atan: the two differ on 137 of the 600 inputs of run_fxgate_slerp.py (--as-std). The real words
// run on emu64 match this function on all 600 (edges: +-0, +-1, the 2^-12 / 2 - sqrt(3) / MaxV boundaries, +-inf, NaN).
#include <cmath>

namespace XboxMath
{
    namespace XMVectorATanDetail
    {
        inline float Bits(unsigned int luBits)
        {
            union { unsigned int mu; float mf; } lValue;
            lValue.mu = luBits;
            return lValue.mf;
        }

        inline float Negate(float lfValue)   // vxor with the sign mask: flips the sign bit, NaN included
        {
            union { float mf; unsigned int mu; } lValue;
            lValue.mf = lfValue;
            lValue.mu ^= 0x80000000u;
            return lValue.mf;
        }

        // vnmsubfp lane: -(a * c - b), rounded ONCE; a NaN comes back un-negated.
        inline float NegativeMultiplySubtract(float lfA, float lfC, float lfB)
        {
            const float lfDifference = std::fmaf(lfA, lfC, -lfB);
            return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
        }

        // The SDK's XMVectorReciprocal as the console runs it: vrefp e ; e1 = e * (1 - x e) + e ;
        // e2 = e1 * (1 - x e1) + e1 ; e2 where e1 is not NaN, else e (so 1 / +-0 stays +-inf and 1 / +-inf stays +-0).
        inline float Reciprocal(float lfValue)
        {
            const float lfEstimate = static_cast<float>(1.0 / static_cast<double>(lfValue));   // vrefp, modelled
            const float lfStep1 = std::fmaf(lfEstimate, NegativeMultiplySubtract(lfValue, lfEstimate, 1.0f), lfEstimate);
            const float lfStep2 = std::fmaf(lfStep1, NegativeMultiplySubtract(lfValue, lfStep1, 1.0f), lfStep1);
            return (lfStep1 == lfStep1) ? lfStep2 : lfEstimate;
        }
    }

    inline float XMVectorATan(float lfX)
    {
        using XMVectorATanDetail::Bits;
        using XMVectorATanDetail::Negate;
        using XMVectorATanDetail::Reciprocal;
        const float kfP0 = Bits(0xC15B0533u), kfP1 = Bits(0xC1A40BFEu), kfP2 = Bits(0xC107E9FBu), kfP3 = Bits(0xBF566BD7u);
        const float kfQ0 = Bits(0x422443E6u), kfQ1 = Bits(0x42AC5090u), kfQ2 = Bits(0x426E5052u), kfQ3 = Bits(0x4170624Fu);
        const float kfSqrt3         = Bits(0x3FDDB3D7u);
        const float kfSqrt3MinusOne = Bits(0x3F3B67AFu);
        const float kfTwoMinusSqrt3 = Bits(0x3E8930A3u);
        const float kfEpsilon       = Bits(0x39800000u);
        const float kfHalfPi        = Bits(0x3FC90FDBu);
        const float kfThirdPi       = Bits(0x3F860A92u);
        const float kfSixthPi       = Bits(0x3F060A92u);
        const float kfMaxV          = Bits(0x7E800000u);

        const float lfAbs = std::fabs(lfX);                                   // vandc
        const bool  lbGreaterThanOne = lfAbs > 1.0f;
        const float lfAngle2 = lbGreaterThanOne ? kfThirdPi : kfSixthPi;
        const float lfAngle1 = lbGreaterThanOne ? kfHalfPi : 0.0f;
        const float lfF = lbGreaterThanOne ? Reciprocal(lfAbs) : lfAbs;

        const bool  lbReduce = lfF > kfTwoMinusSqrt3;
        const float lfFA = std::fmaf(lfF, kfSqrt3MinusOne, lfF) + -1.0f;
        const float lfFB = lfF + kfSqrt3;
        const float lfVF = lbReduce ? lfFA * Reciprocal(lfFB) : lfF;
        const float lfAngle = lbReduce ? lfAngle2 : lfAngle1;

        const float lfG = lfVF * lfVF;
        float lfD = lfG + kfQ3;
        float lfN = std::fmaf(lfG, kfP3, kfP2);
        lfD = std::fmaf(lfG, lfD, kfQ2);
        lfN = std::fmaf(lfG, lfN, kfP1);
        lfD = std::fmaf(lfG, lfD, kfQ1);
        lfN = std::fmaf(lfG, lfN, kfP0);
        lfD = std::fmaf(lfG, lfD, kfQ0);
        lfN = lfN * lfG;

        float lfResult = lfN * Reciprocal(lfD);
        lfResult = std::fmaf(lfVF, lfResult, lfVF);
        if (kfEpsilon > std::fabs(lfVF))
        {
            lfResult = lfVF;
        }
        if (lbGreaterThanOne)
        {
            lfResult = Negate(lfResult);
        }
        lfResult = lfResult + lfAngle;
        if (0.0f > lfX)
        {
            lfResult = Negate(lfResult);
        }
        if (lfX > kfMaxV)
        {
            lfResult = kfHalfPi;
        }
        if (Negate(kfMaxV) > lfX)
        {
            lfResult = Negate(kfHalfPi);
        }
        return lfResult;
    }
}
