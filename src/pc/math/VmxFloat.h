#pragma once

#include "BrnCommonTypes.h"
#include <cmath>
#include <limits>

// FLAG PC-platform leaf: scalar lowering of the X360 VMX floating instructions.
// These operations flush subnormal operands/results without changing the host
// floating-point mode. Scalar PPC arithmetic must remain outside this interface.
// Dot3 follows the pinned Xenia f64 (X+Z)+Y and finite-overflow-to-QNaN model.
// Reciprocal-square-root seeds use correctly rounded host arithmetic. Exact
// hardware estimate/dot bits, NaN payloads and floating status remain unverified.
namespace CgsPC { namespace VmxFloat
{
    inline f32 Flush(f32 lfValue)
    {
        return std::fpclassify(lfValue) == FP_SUBNORMAL
            ? std::copysign(0.0f, lfValue) : lfValue;
    }

    inline f32 Add(f32 lfA, f32 lfB)
    {
        return Flush(Flush(lfA) + Flush(lfB));
    }

    inline f32 Subtract(f32 lfA, f32 lfB)
    {
        return Flush(Flush(lfA) - Flush(lfB));
    }

    inline f32 Multiply(f32 lfA, f32 lfB)
    {
        return Flush(Flush(lfA) * Flush(lfB));
    }

    inline f32 MultiplyAdd(f32 lfA, f32 lfB, f32 lfC)
    {
        return Flush(std::fma(Flush(lfA), Flush(lfB), Flush(lfC)));
    }

    // Classic IDA vnmsubfp D,A,B,C prints A*C-B, rounded then negated.
    // A NaN retains the fused result's sign; exact cancellation becomes -0.
    inline f32 NegativeMultiplySubtract(f32 lfA, f32 lfC, f32 lfB)
    {
        const f32 lfDifference = Flush(std::fma(Flush(lfA), Flush(lfC), -Flush(lfB)));
        return std::isnan(lfDifference) ? lfDifference : -lfDifference;
    }

    inline f32 Maximum(f32 lfA, f32 lfB)
    {
        lfA = Flush(lfA);
        lfB = Flush(lfB);
        if (std::isnan(lfA)) return lfA;
        if (std::isnan(lfB)) return lfB;
        if (lfA == 0.0f && lfB == 0.0f)
            return std::signbit(lfA) && std::signbit(lfB) ? -0.0f : 0.0f;
        return Flush(lfA > lfB ? lfA : lfB);
    }

    inline f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        // Model boundary: Xenia Canary 72c2afa3 DOT_PRODUCT_3_V128 accumulates
        // exact f32 products in f64, (X+Z)+Y, then narrows once. Its
        // EmitDotProductResult maps finite narrowing overflow to quiet NaN;
        // infinity/NaN from input operands remains nonfinite. Hardware status,
        // payloads and last-bit behavior are not certified by this lowering.
        const f64 ldX = static_cast<f64>(Flush(lrA.x)) * Flush(lrB.x);
        const f64 ldY = static_cast<f64>(Flush(lrA.y)) * Flush(lrB.y);
        const f64 ldZ = static_cast<f64>(Flush(lrA.z)) * Flush(lrB.z);
        const f64 ldXZ = ldX + ldZ;
        const f64 ldSum = ldXZ + ldY;
        const f32 lfSum = static_cast<f32>(ldSum);
        if (std::isfinite(ldSum) && std::isinf(lfSum))
        {
            return std::numeric_limits<f32>::quiet_NaN();
        }
        return Flush(lfSum);
    }

    inline Vector3 Subtract(const Vector3& lrA, const Vector3& lrB)
    {
        return { Subtract(lrA.x, lrB.x), Subtract(lrA.y, lrB.y),
                 Subtract(lrA.z, lrB.z), Subtract(lrA.w, lrB.w) };
    }

    inline Vector3 Add(const Vector3& lrA, const Vector3& lrB)
    {
        return { Add(lrA.x, lrB.x), Add(lrA.y, lrB.y),
                 Add(lrA.z, lrB.z), Add(lrA.w, lrB.w) };
    }

    inline Vector3 Multiply(const Vector3& lrVector, f32 lfScale)
    {
        return { Multiply(lrVector.x, lfScale), Multiply(lrVector.y, lfScale),
                 Multiply(lrVector.z, lfScale), Multiply(lrVector.w, lfScale) };
    }

    inline Vector3 MultiplyAdd(const Vector3& lrA, f32 lfScale, const Vector3& lrB)
    {
        return { MultiplyAdd(lrA.x, lfScale, lrB.x), MultiplyAdd(lrA.y, lfScale, lrB.y),
                 MultiplyAdd(lrA.z, lfScale, lrB.z), MultiplyAdd(lrA.w, lfScale, lrB.w) };
    }

    inline Vector3 MultiplyAdd(const Vector3& lrA, const Vector3& lrB, const Vector3& lrC)
    {
        return { MultiplyAdd(lrA.x, lrB.x, lrC.x), MultiplyAdd(lrA.y, lrB.y, lrC.y),
                 MultiplyAdd(lrA.z, lrB.z, lrC.z), MultiplyAdd(lrA.w, lrB.w, lrC.w) };
    }

    inline f32 RefinedReciprocalSqrt(f32 lfValue, u32 luSteps)
    {
        lfValue = Flush(lfValue);
        f32 lfEstimate = Flush(static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfValue))));
        for (u32 luStep = 0; luStep < luSteps; ++luStep)
        {
            const f32 lfSquare = Multiply(lfEstimate, lfEstimate);
            const f32 lfHalf = Multiply(lfEstimate, 0.5f);
            const f32 lfError = NegativeMultiplySubtract(lfValue, lfSquare, 1.0f);
            lfEstimate = MultiplyAdd(lfHalf, lfError, lfEstimate);
        }
        return lfEstimate;
    }

    inline f32 Magnitude3(const Vector3& lrVector)
    {
        const f32 lfSquared = Dot3(lrVector, lrVector);
        const f32 lfInverse = RefinedReciprocalSqrt(lfSquared, 2);
        const f32 lfLength = Multiply(lfSquared, lfInverse);
        // ARTIST vsel occurs after both unconditional refinements and multiply.
        return lfSquared == 0.0f ? 0.0f : lfLength;
    }

    inline Vector3 Normalize3Unconditional(const Vector3& lrVector, u32 luSteps)
    {
        return Multiply(lrVector, RefinedReciprocalSqrt(Dot3(lrVector, lrVector), luSteps));
    }
}}
