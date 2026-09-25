#pragma once

// XMScalarSinCos -- the Xbox 360 XDK math library's SCALAR sine / cosine pair, X360 0x821F0C08..0x821F0D74, a
// 92-instruction leaf: XMScalarSinCos(float* pSin (r3), float* pCos (r4), float Value (f1)). XMMatrixRotationY
// @0x82203560 calls it (0x8220357C), so every console rotation built by that SDK function takes its sine and cosine
// from here. It is NOT XMVectorSinCos (SDKs/XboxMath/XMVectorSinCos.h): the range reduction and the evaluation order
// differ. [FX-GATE, crash parity 2026-09-25]
//
// The console body, word for word (rules from scratch/CRASHPARITY_0922/ROUNDING_RULE.md):
//   0x821F0C08..0x821F0C80  s   = copysign(pi, V): `rlwimi` of 0x01243F6D rotated by 30 (0x40490FDB) into V's bits,
//                                 keeping V's sign bit
//   0x821F0C84  fadds       a   = s + V
//   0x821F0C90  fmuls       a   = a * (1/2pi)                          [0x82001C90] = 0x3E22F983
//   0x821F0C94  fctiwz      q   = a toward zero (saturating; a NaN gives 0x80000000), then fcfid + frsp
//   0x821F0CB4  fnmsubs     y   = -(q * 2pi - V), ONE rounding (rule 3) [0x82001C94] = 0x40C90FDB
//   0x821F0CBC / 0x821F0CC4 fmuls  y2 = y * y, y3 = y2 * y (rule 4); v13 = (1, y, y2, y3)
//   0x821F0CD8  vmulfp128   p0  = v13 * v13            = (1, y^2, y^4, y^6)
//   0x821F0CE4  vmsum4fp128 cA  = p0 . (1, C1, C2, C3)                 0x82000C00
//   0x821F0CEC  vmulfp128   p1  = p0 * y               = (y, y^3, y^5, y^7)
//   0x821F0CF0  vmulfp128   y8  = y^6 * y2
//   0x821F0CF8  vmsum4fp128 sA  = p1 . (1, S1, S2, S3)                 0x82000BD0
//   0x821F0D00 / 0x821F0D04 / 0x821F0D0C / 0x821F0D14  vmulfp128  p0b = p0 * y8, p1b = p1 * y8, p0c = p0b * y8,
//                                                                  p1c = p1b * y8
//   0x821F0D10  vmsum4fp128 cB  = p0b . (C4..C7)                       0x82000C10
//   0x821F0D18  vmsum4fp128 sB  = p1b . (S4..S7)                       0x82000BE0
//   0x821F0D20  vmsum4fp128 cC  = p0c . (C8..C11)                      0x82000C20
//   0x821F0D24  vmsum4fp128 sC  = p1c . (S8..S11)                      0x82000BF0
//   0x821F0D4C / 0x821F0D60  fadds  cos = (cC + cB) + cA    -> [r4]
//   0x821F0D58 / 0x821F0D6C  fadds  sin = (sC + sB) + sA    -> [r3]
// The coefficient rows are the same image rows XMVectorSinCos reads (0x82000BD0..0x82000C2F), here with the leading
// 1.0 of the first sine and cosine rows USED (the y and 1 terms).
//
// PROOF: this body against the REAL words run on emu64 (scratch/CRASHPARITY_0922/fixes/FX-GATE.sincos/
// scalarsincos.py): 2000 inputs (+-0, +-pi and neighbours, 2pi, huge, +-inf, NaN, 1500 in +-10, 500 in +-1000), 0
// mismatches. std::sin / std::cos differ from it on 1844 of those 2000.
//
// FLAG (model): vmsum4fp128 is ROUNDING_RULE 1 -- each product exact in double, summed left to right in double, one
// rounding to float, and a finite double sum that overflows float gives a QNaN (xenia DOT_PRODUCT_4). The emulator's
// exact-sum model and this one agree on every proof input. The VMX denormal flush is not modelled (rule 6).
#include <climits>
#include <cmath>
#include <limits>

namespace XboxMath
{
    namespace XMScalarSinCosDetail
    {
        inline float Bits(unsigned int luBits)
        {
            union { unsigned int mu; float mf; } lValue;
            lValue.mu = luBits;
            return lValue.mf;
        }

        // fctiwz: toward zero, saturating at the int range; a NaN gives INT_MIN (0x80000000).
        inline int ConvertTowardZero(float lfValue)
        {
            if (lfValue != lfValue || lfValue <= -2147483648.0f)
                return INT_MIN;
            if (lfValue >= 2147483647.0f)
                return INT_MAX;
            return static_cast<int>(lfValue);
        }

        // vmsum4fp128, ROUNDING_RULE 1 (see the FLAG above).
        inline float Dot4(const float* lpfA, const float* lpfB)
        {
            double ldSum = static_cast<double>(lpfA[0]) * static_cast<double>(lpfB[0]);
            ldSum = ldSum + static_cast<double>(lpfA[1]) * static_cast<double>(lpfB[1]);
            ldSum = ldSum + static_cast<double>(lpfA[2]) * static_cast<double>(lpfB[2]);
            ldSum = ldSum + static_cast<double>(lpfA[3]) * static_cast<double>(lpfB[3]);
            const float lfResult = static_cast<float>(ldSum);
            if (std::isinf(lfResult) && std::isfinite(ldSum))
                return std::numeric_limits<float>::quiet_NaN();
            return lfResult;
        }
    }

    inline void XMScalarSinCos(float* lpfSin, float* lpfCos, float lfValue)
    {
        using XMScalarSinCosDetail::Bits;
        using XMScalarSinCosDetail::Dot4;

        const float kaCosA[4] = { Bits(0x3F800000u), Bits(0xBF000000u), Bits(0x3D2AAAABu), Bits(0xBAB60B61u) };  // 0x82000C00
        const float kaSinA[4] = { Bits(0x3F800000u), Bits(0xBE2AAAABu), Bits(0x3C088889u), Bits(0xB9500D01u) };  // 0x82000BD0
        const float kaCosB[4] = { Bits(0x37D00D01u), Bits(0xB493F27Eu), Bits(0x310F76C8u), Bits(0xAD49CBA5u) };  // 0x82000C10
        const float kaSinB[4] = { Bits(0x3638EF1Du), Bits(0xB2D7322Bu), Bits(0x2F309231u), Bits(0xAB573F9Fu) };  // 0x82000BE0
        const float kaCosC[4] = { Bits(0x29573F9Fu), Bits(0xA53413C3u), Bits(0x20F2A15Du), Bits(0x9C8671CBu) };  // 0x82000C20
        const float kaSinC[4] = { Bits(0x274A963Cu), Bits(0xA317A4DAu), Bits(0x1EB8DC78u), Bits(0x9A3B0DA1u) };  // 0x82000BF0

        // Range reduction: q = trunc((V + copysign(pi, V)) / 2pi), y = -(q * 2pi - V).
        const float lfSignedPi = std::copysign(Bits(0x40490FDBu), lfValue);
        const float lfShifted  = lfSignedPi + lfValue;
        const float lfScaled   = lfShifted * Bits(0x3E22F983u);
        const float lfQuotient =
            static_cast<float>(static_cast<double>(XMScalarSinCosDetail::ConvertTowardZero(lfScaled)));
        const float lfDifference = std::fmaf(lfQuotient, Bits(0x40C90FDBu), -lfValue);
        const float lfY = (lfDifference != lfDifference) ? lfDifference : -lfDifference;

        const float lfY2 = lfY * lfY;
        const float lfY3 = lfY2 * lfY;
        const float kaV13[4] = { 1.0f, lfY, lfY2, lfY3 };

        float lafP0[4];
        float lafP1[4];
        for (int liLane = 0; liLane < 4; ++liLane)
            lafP0[liLane] = kaV13[liLane] * kaV13[liLane];
        const float lfCosA = Dot4(lafP0, kaCosA);
        for (int liLane = 0; liLane < 4; ++liLane)
            lafP1[liLane] = lafP0[liLane] * lfY;
        const float lfY8 = lafP0[3] * lfY2;
        const float lfSinA = Dot4(lafP1, kaSinA);

        float lafP0b[4];
        float lafP1b[4];
        float lafP0c[4];
        float lafP1c[4];
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            lafP0b[liLane] = lafP0[liLane] * lfY8;
            lafP1b[liLane] = lafP1[liLane] * lfY8;
        }
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            lafP0c[liLane] = lafP0b[liLane] * lfY8;
            lafP1c[liLane] = lafP1b[liLane] * lfY8;
        }
        const float lfCosB = Dot4(lafP0b, kaCosB);
        const float lfSinB = Dot4(lafP1b, kaSinB);
        const float lfCosC = Dot4(lafP0c, kaCosC);
        const float lfSinC = Dot4(lafP1c, kaSinC);

        *lpfCos = (lfCosC + lfCosB) + lfCosA;
        *lpfSin = (lfSinC + lfSinB) + lfSinA;
    }
}
