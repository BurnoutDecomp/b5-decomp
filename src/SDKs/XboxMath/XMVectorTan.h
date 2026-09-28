#pragma once

// XMVectorTan -- the Xbox 360 XDK math library's tangent, X360 0x821F0788 (one out-of-line copy, a 38-instruction
// leaf). Callers here: the car-attached camera's frustum resolver (FrustrumCollisionResolver::
// CalculateFrustumLineTests @0x8220DE70 and ::ProcessSceneQueryResults @0x82224404, both on the half field of view).
// Every lane is computed alone, so this is the per-lane arithmetic, operation for operation (owner's list 2026-09-28,
// lane L1, piece 6). The algorithm is Cody and Waite's: reduce x by the nearest multiple of pi/2 (in two parts), then
// the rational tan(VC) ~ N / D, and -D / N (the cotangent) in the odd quadrants.
//
// The console body (vmaddfp / vnmsubfp are FUSED: one rounding; IDA prints classic vmaddfp in the raw field order
// D, A, B, C, which computes D = A * C + B, and vnmsubfp D = -(A * C - B)):
//   0x821F07BC VA   = x * (2 / pi)                        vmulfp128
//   0x821F07E8 VA   = round-to-nearest-even(VA)            vrfin
//   0x821F07F8 VC   = -(VA * C0 - x)                       vnmsubfp  (C0 = pi/2, the high part)
//   0x821F07FC VC   = -(VA * C1 - VC)                      vnmsubfp  (C1 = the low part of pi/2)
//   0x821F0800 VC2  = VC * VC
//   0x821F0808 M    = vcmpbfp(VC, Epsilon)                 the RAW bounds word: bit 31 set when VC > Epsilon, bit 30
//                                                          when VC < -Epsilon, both for a NaN, 0 when in bounds
//   0x821F080C..0x821F0828  N = VC2 * T7 + T6 ; D = VC2 * T4 + T3 ; N = VC2 * N + T5 ; D = VC2 * D + T2 ;
//                           N = VC2 * N (vmulfp128) ; D = VC2 * D + T1 ; N = VC * N + VC ; D = VC2 * D + T0
//   0x821F082C N    = vsel(N, VC, M) ; 0x821F0830 D = vsel(D, 1.0, M)   -- selected BIT BY BIT through the raw
//                           vcmpbfp word (the XDK's XMVectorInBounds without its vcmpequw), so only the top two bits
//                           of an out-of-bounds lane come from VC / 1.0; an in-bounds lane keeps N / D whole
//   0x821F0834..0x821F0860  odd = (vctsxs(|VA|) & 1) != 0  (vandc, vctsxs saturating, vand, vcmpequw)
//   0x821F083C -N  = vxor(N, sign)
//   0x821F0850..0x821F0894  1 / D and 1 / -N: vrefp e, e1 = e * (1 - x e) + e, e2 = e1 * (1 - x e1) + e1, and e2 only
//                           where e1 is not NaN (vcmpeqfp e1, e1 ; vsel) -- the SDK's XMVectorReciprocal
//   0x821F0898 R1   = N * (1 / D) ; 0x821F089C R0 = D * (1 / -N)
//   0x821F08A0 R    = odd ? R0 : R1 ; 0x821F08A4 R = (x == 0) ? 0 : R      (vcmpeqfp x, 0 -- +-0 both)
// The constant splats, read from the image (x360rd):
//   0x82001C00: 3F800000 (T0 1.0) BEEEF582 (T1) 3CD23CF5 (T2) B9A37B25 (T3)
//   0x82001C10: 3505BBA8 (T4) BE0895AF (T5) 3B607415 (T6) B795D5B9 (T7)
//   0x82001C20: 3FC90FDB (C0 pi/2) 2E85A309 (C1) 39800000 (Epsilon 2^-12) 3F22F983 (2 / pi)
//
// FLAG (estimate model): vrefp is modelled as the exactly rounded 1 / x -- the tree's standing convention for the
// VMX estimates (XMVectorATan.h) -- with its specials (+-0 -> +-inf, +-inf -> +-0). vctsxs of a NaN is 0 and of a
// value beyond 2^31 saturates (only the parity bit is read).
// ROUNDING_RULE 6: the VMX non-Java mode reads a denormal operand as a zero of its sign, so a denormal x is x == 0
// here (the result is +0, as the console's final vcmpeqfp / vsel gives); the flush of denormal INTERMEDIATES is not
// modelled -- its callers pass a half field of view in radians.
#include <cmath>
#include <cstring>

namespace XboxMath
{
    namespace XMVectorTanDetail
    {
        inline float Bits(unsigned int luBits)
        {
            float lf;
            std::memcpy(&lf, &luBits, sizeof(lf));
            return lf;
        }

        inline unsigned int ToBits(float lf)
        {
            unsigned int lu;
            std::memcpy(&lu, &lf, sizeof(lu));
            return lu;
        }

        // vnmsubfp lane: -(a * c - b), rounded ONCE; a NaN comes back un-negated.
        inline float NegativeMultiplySubtract(float lfA, float lfC, float lfB)
        {
            const float lfDifference = std::fmaf(lfA, lfC, -lfB);
            return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
        }

        // The SDK's XMVectorReciprocal as the console runs it (see XMVectorATan.h's twin).
        inline float Reciprocal(float lfValue)
        {
            const float lfEstimate = static_cast<float>(1.0 / static_cast<double>(lfValue));   // vrefp, modelled
            const float lfStep1 = std::fmaf(lfEstimate, NegativeMultiplySubtract(lfValue, lfEstimate, 1.0f), lfEstimate);
            const float lfStep2 = std::fmaf(lfStep1, NegativeMultiplySubtract(lfValue, lfStep1, 1.0f), lfStep1);
            return (lfStep1 == lfStep1) ? lfStep2 : lfEstimate;
        }

        // vcmpbfp lane: bit 31 = !(v <= bound), bit 30 = !(v >= -bound) (both set for a NaN).
        inline unsigned int BoundsWord(float lfValue, float lfBound)
        {
            unsigned int luWord = 0u;
            if (!(lfValue <= lfBound))
                luWord |= 0x80000000u;
            if (!(lfValue >= -lfBound))
                luWord |= 0x40000000u;
            return luWord;
        }

        // vsel lane: (a & ~mask) | (b & mask).
        inline float SelectBits(float lfA, float lfB, unsigned int luMask)
        {
            return Bits((ToBits(lfA) & ~luMask) | (ToBits(lfB) & luMask));
        }

        // vctsxs lane with a zero scale: truncate toward zero, saturate, NaN -> 0.
        inline int ConvertToIntSaturate(float lfValue)
        {
            if (lfValue != lfValue)
                return 0;
            if (lfValue >= 2147483648.0f)
                return 0x7FFFFFFF;
            if (lfValue < -2147483648.0f)
                return static_cast<int>(0x80000000u);
            return static_cast<int>(lfValue);
        }
    }

    inline float XMVectorTan(float lfX)
    {
        using namespace XMVectorTanDetail;
        const float kfT0 = Bits(0x3F800000u), kfT1 = Bits(0xBEEEF582u), kfT2 = Bits(0x3CD23CF5u), kfT3 = Bits(0xB9A37B25u);
        const float kfT4 = Bits(0x3505BBA8u), kfT5 = Bits(0xBE0895AFu), kfT6 = Bits(0x3B607415u), kfT7 = Bits(0xB795D5B9u);
        const float kfC0 = Bits(0x3FC90FDBu), kfC1 = Bits(0x2E85A309u);
        const float kfEpsilon  = Bits(0x39800000u);
        const float kfTwoDivPi = Bits(0x3F22F983u);

        if (std::fpclassify(lfX) == FP_SUBNORMAL)
            lfX = std::copysign(0.0f, lfX);                                                // the VMX operand flush
        const float lfVA  = std::nearbyint(lfX * kfTwoDivPi);                              // vmulfp128, vrfin
        float       lfVC  = NegativeMultiplySubtract(lfVA, kfC0, lfX);
        lfVC              = NegativeMultiplySubtract(lfVA, kfC1, lfVC);
        const float lfVC2 = lfVC * lfVC;
        const unsigned int luInBounds = BoundsWord(lfVC, kfEpsilon);

        float lfN = std::fmaf(lfVC2, kfT7, kfT6);
        float lfD = std::fmaf(lfVC2, kfT4, kfT3);
        lfN = std::fmaf(lfVC2, lfN, kfT5);
        lfD = std::fmaf(lfVC2, lfD, kfT2);
        lfN = lfVC2 * lfN;
        lfD = std::fmaf(lfVC2, lfD, kfT1);
        lfN = std::fmaf(lfVC, lfN, lfVC);
        lfD = std::fmaf(lfVC2, lfD, kfT0);

        lfN = SelectBits(lfN, lfVC, luInBounds);
        lfD = SelectBits(lfD, 1.0f, luInBounds);

        const bool  lbOdd = (ConvertToIntSaturate(std::fabs(lfVA)) & 1) != 0;
        const float lfNegN = Bits(ToBits(lfN) ^ 0x80000000u);                             // vxor
        const float lfR1 = lfN * Reciprocal(lfD);
        const float lfR0 = lfD * Reciprocal(lfNegN);
        const float lfResult = lbOdd ? lfR0 : lfR1;
        return (lfX == 0.0f) ? 0.0f : lfResult;
    }
}
