#include "vendor/renderware/collision/CylinderVolume.hpp"
#include "vendor/renderware/collision/AABBox.hpp"
namespace rw { namespace collision {
static __vector4 CylinderSqrt(__vector4 value)
{
    const __vector4 zero = __vspltisw(0);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const __vector4 isZero = __vcmpeqfp(zero, value);
    __vector4 estimate = __vrsqrtefp(value);
    __vector4 square = __vmulfp(estimate, estimate);
    __vector4 scale = __vmulfp(estimate, half);
    __vector4 correction = __vnmsubfp(value, square, one);
    estimate = __vmaddfp(scale, correction, estimate);
    square = __vmulfp(estimate, estimate);
    scale = __vmulfp(estimate, half);
    correction = __vnmsubfp(value, square, one);
    estimate = __vmaddfp(scale, correction, estimate);
    return __vsel(__vmulfp(value, estimate), zero, isZero);
}

static __vector4 CylinderAxisExtent(__vector4 axis, __vector4 height, __vector4 radius)
{
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const __vector4 sq = __vmulfp(axis, axis);
    const f32 square = sq.x;
    const f32 difference = 1.0f - square;
    const __vector4 absDifference = __vandc(__vspltw(__lvlx(&difference, 0), 0), sign);
    const __vector4 axial = __vmulfp(__vandc(axis, sign), height);
    return __vmaddfp(CylinderSqrt(absDifference), radius, axial);
}

RwBool CylinderVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool abTight, AABBox& arResult) const
{
    const f32 radial = mfRadius + mCylinderData.mfInnerRadius;
    const f32 axial = mfRadius + mCylinderData.mfHalfHeight;
    if (!lpTransform)
    {
        const Vector3 extent(radial, radial, axial);
        const __vector4 ones = __vspltisw(-1);
        const __vector4 sign = __vslw(ones, ones);
        arResult.mMax = extent;
        arResult.mMin.mV = __vxor(extent.mV, sign);
        return 1;
    }
    __vector4 extent;
    if (!abTight)
    {
        volatile f32 radiusSquare = radial * radial;
        const f32 distanceSquare = axial * axial + radiusSquare;
        const __vector4 distance = CylinderSqrt(__vspltw(__lvlx(&distanceSquare, 0), 0));
        const f32 d = distance.x;
        extent = Vector3(d, d, d).mV;
    }
    else
    {
        const __vector4 axis = lpTransform->zAxis.mV;
        const __vector4 height = __vspltw(__lvlx(&axial, 0), 0);
        const __vector4 radius = __vspltw(__lvlx(&radial, 0), 0);
        const __vector4 x = CylinderAxisExtent(__vspltw(axis, 0), height, radius);
        const __vector4 y = CylinderAxisExtent(__vspltw(axis, 1), height, radius);
        const __vector4 z = CylinderAxisExtent(__vspltw(axis, 2), height, radius);
        // The original builds this row from the spilled axis-x square; its W
        // lane survives the three component inserts.
        const __vector4 axisX = __vspltw(axis, 0);
        extent = __vrlimi(__vmulfp(axisX, axisX), x, 8, 0);
        extent = __vrlimi(extent, y, 4, 0);
        extent = __vrlimi(extent, z, 2, 0);
    }
    const __vector4 centre = lpTransform->wAxis.mV;
    arResult.mMin.mV = __vsubfp(centre, extent);
    arResult.mMax.mV = __vaddfp(centre, extent);
    return 1;
}
} }
