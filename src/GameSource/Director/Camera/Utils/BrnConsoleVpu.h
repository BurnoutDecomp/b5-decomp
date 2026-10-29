#pragma once

// Native VMX operations expanded by ARTIST's director camera code. These helpers
// retain the named-lane facade used by its reconstructed callers; all arithmetic
// below uses the original vector instructions, including their fused rounding,
// denormal behavior and hardware reciprocal estimates. No vector class ABI is
// inferred from this inline facade.

#include <vectorintrinsics.h>
#include "types.hpp"
#include "rw/math/vpu/types.h"
#include "SDKs/XboxMath/XMVectorSinCos.h"

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
namespace ConsoleVpu
{
    using rw::math::vpu::Matrix44Affine;
    using rw::math::vpu::Vector3;

    __forceinline f32 MultiplyAdd(f32 lfA, f32 lfB, f32 lfC)
    {
        return __vmaddfp(XMVectorReplicate(lfA), XMVectorReplicate(lfB),
                         XMVectorReplicate(lfC)).x;
    }

    __forceinline f32 NegativeMultiplySubtract(f32 lfA, f32 lfB, f32 lfC)
    {
        return __vnmsubfp(XMVectorReplicate(lfA), XMVectorReplicate(lfB),
                          XMVectorReplicate(lfC)).x;
    }

    // MomentTakedownLookback::Update 82266554/82266558: three-lane dot.
    __forceinline f32 Dot3(const Vector3& lrA, const Vector3& lrB)
    {
        return __vmsum3fp(__lvx(&lrA, 0), __lvx(&lrB, 0)).x;
    }

    // 82266578..822665A4: one Newton step after the hardware rsqrt estimate.
    __forceinline Vector3 NormalizeFast(const Vector3& lrVector)
    {
        const __vector4 lvVector = __lvx(&lrVector, 0);
        const __vector4 lvLengthSquared = __vmsum3fp(lvVector, lvVector);
        const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lvHalf = __vcfsx(__vspltisw(1), 1);
        const __vector4 lvEstimate = __vrsqrtefp(lvLengthSquared);
        const __vector4 lvSquared = __vmulfp(lvEstimate, lvEstimate);
        const __vector4 lvHalfEstimate = __vmulfp(lvEstimate, lvHalf);
        const __vector4 lvResidual = __vnmsubfp(lvLengthSquared, lvSquared, lvOne);
        const __vector4 lvRefined = __vmaddfp(lvHalfEstimate, lvResidual, lvEstimate);
        Vector3 lResult;
        __stvx(__vmulfp(lvVector, lvRefined), &lResult, 0);
        return lResult;
    }

    // 8226657C/82266588/82266590/8226659C/822665A8/822665B8: two reciprocal steps.
    __forceinline f32 Divide(f32 lfNumerator, f32 lfDenominator)
    {
        const __vector4 lvDenominator = XMVectorReplicate(lfDenominator);
        const __vector4 lvOne = __vcfsx(__vspltisw(1), 0);
        __vector4 lvReciprocal = __vrefp(lvDenominator);
        __vector4 lvResidual = __vnmsubfp(lvReciprocal, lvDenominator, lvOne);
        lvReciprocal = __vmaddfp(lvReciprocal, lvResidual, lvReciprocal);
        lvResidual = __vnmsubfp(lvReciprocal, lvDenominator, lvOne);
        lvReciprocal = __vmaddfp(lvReciprocal, lvResidual, lvReciprocal);
        return __vmulfp(lvReciprocal, XMVectorReplicate(lfNumerator)).x;
    }

    __forceinline Vector3 MultiplyAdd(const Vector3& lrA, f32 lfScalar, const Vector3& lrC)
    {
        Vector3 lResult;
        __stvx(__vmaddfp(__lvx(&lrA, 0), XMVectorReplicate(lfScalar), __lvx(&lrC, 0)), &lResult, 0);
        return lResult;
    }

    // CameraRig::Construct 8220B3F4..8220B468. Positive terms are fused;
    // negative terms have a separate multiply and subtract. The native permute at
    // 82CDA350 duplicates X in the unused W lane; keep those bits as well.
    __forceinline __vector4 PackRotationRow(__vector4 lvX, __vector4 lvY, __vector4 lvZ)
    {
        __vector4 lvRow = __vmrghw(lvX, lvY);
        lvRow = __vrlimi(lvRow, lvZ, 2, 0);
        return __vrlimi(lvRow, lvX, 1, 0);
    }

    __forceinline Matrix44Affine Matrix44AffineFromAxisRotationAngle(const Vector3& lrAxis, f32 lfAngle)
    {
        __vector4 lvSin, lvCos;
        ::XMVectorSinCos(&lvSin, &lvCos, XMVectorReplicate(lfAngle));
        const __vector4 lvAxis = __lvx(&lrAxis, 0);
        const __vector4 lvX = __vspltw(lvAxis, 0);
        const __vector4 lvY = __vspltw(lvAxis, 1);
        const __vector4 lvZ = __vspltw(lvAxis, 2);
        const __vector4 lvT = __vsubfp(__vcfsx(__vspltisw(1), 0), lvCos);
        const __vector4 lvTX = __vmulfp(lvT, lvX);
        const __vector4 lvTY = __vmulfp(lvT, lvY);
        const __vector4 lvTZ = __vmulfp(lvT, lvZ);
        const __vector4 lvSX = __vmulfp(lvSin, lvX);
        const __vector4 lvSY = __vmulfp(lvSin, lvY);
        const __vector4 lvSZ = __vmulfp(lvSin, lvZ);
        Matrix44Affine lResult;
        __stvx(PackRotationRow(__vmaddfp(lvTX, lvX, lvCos), __vmaddfp(lvTX, lvY, lvSZ),
                              __vsubfp(__vmulfp(lvTX, lvZ), lvSY)), &lResult.xAxis, 0);
        __stvx(PackRotationRow(__vsubfp(__vmulfp(lvTY, lvX), lvSZ), __vmaddfp(lvTY, lvY, lvCos),
                              __vmaddfp(lvTY, lvZ, lvSX)), &lResult.yAxis, 0);
        __stvx(PackRotationRow(__vmaddfp(lvTZ, lvX, lvSY), __vsubfp(__vmulfp(lvTZ, lvY), lvSX),
                              __vmaddfp(lvTZ, lvZ, lvCos)), &lResult.zAxis, 0);
        __stvx(__vspltisw(0), &lResult.wAxis, 0);
        return lResult;
    }

    // BehaviourRig::Update 82242E58..82242E60: X multiply, Y and Z fused adds.
    __forceinline Vector3 TransformVector(const Matrix44Affine& lrMatrix, const Vector3& lrVector)
    {
        const __vector4 lvVector = __lvx(&lrVector, 0);
        __vector4 lvResult = __vmulfp(__vspltw(lvVector, 0), __lvx(&lrMatrix.xAxis, 0));
        lvResult = __vmaddfp(__vspltw(lvVector, 1), __lvx(&lrMatrix.yAxis, 0), lvResult);
        lvResult = __vmaddfp(__vspltw(lvVector, 2), __lvx(&lrMatrix.zAxis, 0), lvResult);
        Vector3 lResult;
        __stvx(lvResult, &lResult, 0);
        return lResult;
    }

    // 82242FA8/82242FDC/82242FFC: translation, then X/Y/Z fused adds.
    __forceinline Vector3 TransformPoint(const Matrix44Affine& lrMatrix, const Vector3& lrPoint)
    {
        const __vector4 lvPoint = __lvx(&lrPoint, 0);
        __vector4 lvResult = __vmaddfp(__vspltw(lvPoint, 0), __lvx(&lrMatrix.xAxis, 0), __lvx(&lrMatrix.wAxis, 0));
        lvResult = __vmaddfp(__vspltw(lvPoint, 1), __lvx(&lrMatrix.yAxis, 0), lvResult);
        lvResult = __vmaddfp(__vspltw(lvPoint, 2), __lvx(&lrMatrix.zAxis, 0), lvResult);
        Vector3 lResult;
        __stvx(lvResult, &lResult, 0);
        return lResult;
    }

    __forceinline Matrix44Affine Mult(const Matrix44Affine& lrLhs, const Matrix44Affine& lrRhs)
    {
        Matrix44Affine lResult;
        lResult.xAxis = ConsoleVpu::TransformVector(lrRhs, lrLhs.xAxis);
        lResult.yAxis = ConsoleVpu::TransformVector(lrRhs, lrLhs.yAxis);
        lResult.zAxis = ConsoleVpu::TransformVector(lrRhs, lrLhs.zAxis);
        lResult.wAxis = ConsoleVpu::TransformPoint(lrRhs, lrLhs.wAxis);
        return lResult;
    }

    // 82242D20..82242D64: transpose with zero fourth lanes, negate by subtraction,
    // then accumulate inverse translation Z first, followed by Y and X.
    __forceinline Matrix44Affine InverseOfMatrixWithOrthonormal3x3(const Matrix44Affine& lrMatrix)
    {
        const __vector4 lvZero = __vspltisw(0);
        const __vector4 lvX = __lvx(&lrMatrix.xAxis, 0);
        const __vector4 lvY = __lvx(&lrMatrix.yAxis, 0);
        const __vector4 lvZ = __lvx(&lrMatrix.zAxis, 0);
        const __vector4 lvXZHigh = __vmrghw(lvX, lvZ);
        const __vector4 lvXZLow = __vmrglw(lvX, lvZ);
        const __vector4 lvYHigh = __vmrghw(lvY, lvZero);
        const __vector4 lvYLow = __vmrglw(lvY, lvZero);
        const __vector4 lvTX = __vmrghw(lvXZHigh, lvYHigh);
        const __vector4 lvTY = __vmrglw(lvXZHigh, lvYHigh);
        const __vector4 lvTZ = __vmrghw(lvXZLow, lvYLow);
        const __vector4 lvNegPos = __vsubfp(lvZero, __lvx(&lrMatrix.wAxis, 0));
        __vector4 lvTranslation = __vmulfp(__vspltw(lvNegPos, 2), lvTZ);
        lvTranslation = __vmaddfp(__vspltw(lvNegPos, 1), lvTY, lvTranslation);
        lvTranslation = __vmaddfp(lvTX, __vspltw(lvNegPos, 0), lvTranslation);
        Matrix44Affine lResult;
        __stvx(lvTX, &lResult.xAxis, 0);
        __stvx(lvTY, &lResult.yAxis, 0);
        __stvx(lvTZ, &lResult.zAxis, 0);
        __stvx(lvTranslation, &lResult.wAxis, 0);
        return lResult;
    }
}
}
}
}
