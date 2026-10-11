#pragma once

#include <cmath>
#include <cstring>

// ARTIST 821F05E0..821F06AC. Each VMX lane reduces by the nearest multiple
// of 2*pi, then accumulates the eleven odd-power terms in order. Unlike the
// paired SinCos routine, successive powers here multiply the preceding odd
// power by x*x. VMX operands and results flush subnormals to signed zero.
namespace XboxMath
{
    namespace XMVectorSinDetail
    {
        inline float Bits(unsigned int luBits)
        {
            float lfValue;
            std::memcpy(&lfValue, &luBits, sizeof(lfValue));
            return lfValue;
        }

        inline float Flush(float lfValue)
        {
            return std::fpclassify(lfValue) == FP_SUBNORMAL
                ? std::copysign(0.0f, lfValue) : lfValue;
        }

        inline float Multiply(float lfA, float lfB)
        {
            return Flush(Flush(lfA) * Flush(lfB));
        }

        inline float MultiplyAdd(float lfA, float lfB, float lfC)
        {
            return Flush(std::fmaf(Flush(lfA), Flush(lfB), Flush(lfC)));
        }

        inline float RoundNearestEven(float lfValue)
        {
            // Scalar lowering of vrfin, independent of the host rounding mode.
            if (!std::isfinite(lfValue) || std::fabs(lfValue) >= 8388608.0f)
                return lfValue;
            const float lfFloor = std::floor(lfValue);
            const float lfFraction = lfValue - lfFloor;
            const bool lbRoundUp = lfFraction > 0.5f ||
                (lfFraction == 0.5f && std::fmod(lfFloor, 2.0f) != 0.0f);
            const float lfRounded = lfFloor + (lbRoundUp ? 1.0f : 0.0f);
            return lfRounded == 0.0f ? std::copysign(0.0f, lfValue) : lfRounded;
        }
    }

    inline float XMVectorSin(float lfValue)
    {
        using namespace XMVectorSinDetail;
        // Image tables 82000BD0..82000BFF and 82000C60..82000C6F.
        const float laCoefficients[11] = {
            Bits(0xBE2AAAABu), Bits(0x3C088889u), Bits(0xB9500D01u),
            Bits(0x3638EF1Du), Bits(0xB2D7322Bu), Bits(0x2F309231u),
            Bits(0xAB573F9Fu), Bits(0x274A963Cu), Bits(0xA317A4DAu),
            Bits(0x1EB8DC78u), Bits(0x9A3B0DA1u),
        };
        lfValue = Flush(lfValue);
        const float lfMultiple = RoundNearestEven(Multiply(lfValue, Bits(0x3E22F983u)));
        const float lfDifference = MultiplyAdd(Bits(0x40C90FDBu), lfMultiple, -lfValue);
        const float lfReduced = lfDifference != lfDifference ? lfDifference : -lfDifference;
        const float lfSquared = Multiply(lfReduced, lfReduced);
        float lfPower = Multiply(lfSquared, lfReduced);
        float lfResult = lfReduced;
        for (int liTerm = 0; liTerm < 11; ++liTerm)
        {
            lfResult = MultiplyAdd(laCoefficients[liTerm], lfPower, lfResult);
            if (liTerm != 10)
                lfPower = Multiply(lfPower, lfSquared);
        }
        return lfResult;
    }
}
