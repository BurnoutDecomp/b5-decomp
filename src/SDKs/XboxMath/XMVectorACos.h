#pragma once

// XMVectorACos -- the Xbox 360 XDK math library's inverse cosine, X360 0x821F0980 (one out-of-line
// copy, 30 `bl` sites in the ARTIST image; no inlined copy loads its tables anywhere else). The game's
// callers read ONE lane of the result, so this is the per-lane arithmetic, operation for operation.
//
// The console body (vmaddfp / vnmsubfp are FUSED: one rounding; IDA prints classic vmaddfp in the raw
// field order D, A, B, C, which computes D = A * C + B):
//   0x821F098C x2   = x * x                      0x821F09CC ax = |x| (vandc of the sign bit)
//   0x821F09E4 ax3  = x2 * ax
//   0x821F09E8 p2   = B2 * ax + A2               0x821F09F8 q0 = B0 * ax + A0
//   0x821F0A10 p3   = B3 * ax + A3               0x821F0A14 q1 = B1 * ax + A1
//   0x821F0A0C oma  = 1.00000012 - ax            (splat 0x82001C40 = 0x3F800001: never rsqrt(0) at |x| = 1)
//   0x821F0A18 p2   = p2 * ax + C2               0x821F0A1C q0 = q0 * ax + C0
//   0x821F0A24 p3   = p3 * ax + C3               0x821F0A2C q1 = q1 * ax + C1
//   0x821F0A20 y0   = vrsqrtefp(oma)             0x821F0A28 h  = oma * 0.5   (vcfsx 1, 1 at 0x821F0994)
//   0x821F0A30 p    = p2 * ax3 + p3              0x821F0A38 px = x * p
//   0x821F0A34 y0sq = y0 * y0                    0x821F0A3C e  = 0.5 - h * y0sq         (vnmsubfp)
//   0x821F0A50 y1   = y0 * e + y0                (one Newton step: y1 ~ 1 / sqrt(oma))
//   0x821F0A54 q    = q0 * ax3 + q1              0x821F0A58 u  = x - ax * x            (vnmsubfp)
//   0x821F0A5C q    = u * q                      0x821F0A60 r  = q * y1 + px
//   0x821F0A48 / 0x821F0A4C halfpi = D0 * 0.5    0x821F0A64 result = halfpi - r
// i.e. acos(x) = pi/2 - x * (sqrt(1 - |x|) * Q(|x|) + P(|x|)), with u * y1 standing for x * sqrt(1 - |x|).
// The coefficient splats, read from the image (x360rd):
//   B = 0x82000C30: BD6DD42D BED65553 3E663246 400B1889
//   A = 0x82000C40: 3F1DD7B6 408980BD BF983F2F C0D1360E
//   C = 0x82000C50: BFAF4418 C08F6AD9 3FB58485 40AF6AD8
//   D = 0x82000C60: 40490FDB (pi) 40C90FDB 3EA2F983 3E22F983   -- only D0 is read
// So it is NOT std::acos: XMVectorACos(1.0) = 0x35000000 (4.8e-7), XMVectorACos(-1.0) = 0x40490FD9
// (pi - 2 ulp), and any |x| > 1 (even 1.00000012, where oma = 0 and e = 0 * inf) is NaN.
//
// FLAG (estimate model): vrsqrtefp is modelled as the exactly rounded 1 / sqrt(oma) -- the tree's standing
// convention for the VMX estimates (the Xenon's 12-bit table is not known) -- so a result may sit an ulp
// or so from the console's. vrsqrtefp's specials are kept: +0 -> +inf, -0 -> -inf, < 0 -> NaN,
// +inf -> +0. Not modelled: the VMX non-Java mode's flush of denormal operands and results to zero (it
// can move the result only for a denormal x, where the console answers pi/2 and this pi/2 - ~0).
// A NaN input returns a NaN (its payload is not modelled).
#include <cmath>

namespace XboxMath
{
    namespace XMVectorACosDetail
    {
        inline float Bits(unsigned int luBits)
        {
            union { unsigned int mu; float mf; } lValue;
            lValue.mu = luBits;
            return lValue.mf;
        }

        inline float RsqrtEstimate(float lfValue)   // vrsqrtefp, modelled (see FLAG above)
        {
            return static_cast<float>(1.0 / std::sqrt(static_cast<double>(lfValue)));
        }
    }

    inline float XMVectorACos(float lfX)
    {
        using XMVectorACosDetail::Bits;
        const float kfB0 = Bits(0xBD6DD42Du), kfB1 = Bits(0xBED65553u), kfB2 = Bits(0x3E663246u), kfB3 = Bits(0x400B1889u);
        const float kfA0 = Bits(0x3F1DD7B6u), kfA1 = Bits(0x408980BDu), kfA2 = Bits(0xBF983F2Fu), kfA3 = Bits(0xC0D1360Eu);
        const float kfC0 = Bits(0xBFAF4418u), kfC1 = Bits(0xC08F6AD9u), kfC2 = Bits(0x3FB58485u), kfC3 = Bits(0x40AF6AD8u);
        const float kfPi              = Bits(0x40490FDBu);   // D0
        const float kfOnePlusEpsilon  = Bits(0x3F800001u);   // 0x82001C40
        const float kfHalf            = 0.5f;                // vcfsx(splat 1, 1)

        const float lfX2   = lfX * lfX;
        const float lfAbs  = std::fabs(lfX);
        const float lfAbs3 = lfX2 * lfAbs;

        float lfP2 = std::fmaf(kfB2, lfAbs, kfA2);
        float lfQ0 = std::fmaf(kfB0, lfAbs, kfA0);
        const float lfOneMinusAbs = kfOnePlusEpsilon - lfAbs;
        float lfP3 = std::fmaf(kfB3, lfAbs, kfA3);
        float lfQ1 = std::fmaf(kfB1, lfAbs, kfA1);
        lfP2 = std::fmaf(lfP2, lfAbs, kfC2);
        lfQ0 = std::fmaf(lfQ0, lfAbs, kfC0);
        const float lfY0 = XMVectorACosDetail::RsqrtEstimate(lfOneMinusAbs);
        lfP3 = std::fmaf(lfP3, lfAbs, kfC3);
        const float lfHalfOneMinusAbs = lfOneMinusAbs * kfHalf;
        lfQ1 = std::fmaf(lfQ1, lfAbs, kfC1);
        const float lfP = std::fmaf(lfP2, lfAbs3, lfP3);
        const float lfY0Squared = lfY0 * lfY0;
        const float lfPX = lfX * lfP;
        const float lfE = std::fmaf(-lfHalfOneMinusAbs, lfY0Squared, kfHalf);   // vnmsubfp: 0.5 - h * y0^2
        const float lfHalfPi = kfPi * kfHalf;
        const float lfY1 = std::fmaf(lfY0, lfE, lfY0);
        float lfQ = std::fmaf(lfQ0, lfAbs3, lfQ1);
        const float lfU = std::fmaf(-lfAbs, lfX, lfX);                          // vnmsubfp: x - |x| * x
        lfQ = lfU * lfQ;
        const float lfR = std::fmaf(lfQ, lfY1, lfPX);
        return lfHalfPi - lfR;
    }
}
