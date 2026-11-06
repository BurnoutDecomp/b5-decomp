#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include "vendor/renderware/collision/CollisionVolume.hpp"
#include <cfloat>
#include <cmath>
namespace rw { namespace collision {
s32 rwcSphereLineSegIntersect(Fraction* lpDist,
                              const rw::math::vpu::Vector3* lpLineStart,
                              const rw::math::vpu::Vector3* lpLineDelta,
                              const rw::math::vpu::Vector3* lpCentre,
                              f32 afRadius)
{
    const __vector4 toCentre = __vsubfp(lpCentre->mV, lpLineStart->mV);
    const f32 radiusSquare = afRadius * afRadius;
    const f32 distanceSquare = __vmsum3fp(toCentre, toCentre).x;
    if (distanceSquare < radiusSquare)
    {
        lpDist->den = 1.0f;
        lpDist->num = 0.0f;
        return 1;
    }
    const __vector4 delta = lpLineDelta->mV;
    const f32 projection = __vmsum3fp(toCentre, delta).x;
    if (!(projection > 0.0f)) return -1;
    const f32 deltaSquare = __vmsum3fp(delta, delta).x;
    const __vector4 cross = __vpermwi(__vnmsubfp(__vpermwi(toCentre, 0x63), delta,
        __vmulfp(toCentre, __vpermwi(delta, 0x63))), 0x63);
    const f32 crossSquare = __vmsum3fp(cross, cross).x;
    const f32 discriminant = deltaSquare * radiusSquare - crossSquare;
    if (discriminant < 0.0f) return 0;
    const f32 remaining = projection - deltaSquare;
    if (remaining > 0.0f && remaining * remaining > discriminant) return 0;
    lpDist->den = deltaSquare;
    lpDist->num = projection - static_cast<f32>(std::sqrt(discriminant));
    return 1;
}
RwBool SphereVolume::LineSegIntersect(const Vector3& arPt1, const Vector3& arPt2,
                                      const Matrix44Affine* lpTransform,
                                      VolumeLineSegIntersectResult& arResult, f32 afFatness) const
{
    __vector4 centre = mTransform.wAxis.mV;
    if (lpTransform)
    {
        __vector4 result = __vmaddfp(lpTransform->xAxis.mV, __vspltw(centre, 0), lpTransform->wAxis.mV);
        result = __vmaddfp(lpTransform->yAxis.mV, __vspltw(centre, 1), result);
        centre = __vmaddfp(lpTransform->zAxis.mV, __vspltw(centre, 2), result);
    }
    arResult.v = this;
    const Vector3 delta(__vsubfp(arPt2.mV, arPt1.mV));
    const Vector3 centreValue(centre);
    const f32 radius = mfRadius + afFatness;
    Fraction fraction;
    if (rwcSphereLineSegIntersect(&fraction, &arPt1, &delta, &centreValue, radius) <= 0) return 0;
    arResult.lineParam = fraction.num / fraction.den;
    const __vector4 time = __vspltw(__lvlx(&arResult.lineParam, 0), 0);
    const __vector4 position = __vmaddfp(delta.mV, time, arPt1.mV);
    __vector4 normal = __vsubfp(position, centre);
    arResult.position.mV = position;
    arResult.normal.mV = normal;
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    if (fraction.num > 0.0f)
    {
        const __vector4 rad = __vspltw(__lvlx(&radius, 0), 0);
        __vector4 estimate = __vrefp(rad);
        __vector4 correction = __vnmsubfp(estimate, rad, one);
        estimate = __vmaddfp(estimate, correction, estimate);
        correction = __vnmsubfp(estimate, rad, one);
        estimate = __vmaddfp(estimate, correction, estimate);
        normal = __vmulfp(normal, estimate);
        arResult.normal.mV = normal;
    }
    else
    {
        const __vector4 zero = __vspltisw(0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        const __vector4 square = __vmsum3fp(normal, normal);
        const __vector4 equalZero = __vcmpeqfp(zero, square);
        __vector4 estimate = __vrsqrtefp(square);
        __vector4 correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        const __vector4 magnitude = __vsel(__vmulfp(square, estimate), zero, equalZero);
        const f32 threshold = FLT_MIN;
        unsigned int comparison = 0;
        __vcmpgtfpR(magnitude, __vspltw(__lvlx(&threshold, 0), 0), &comparison);
        if ((comparison & 0x80u) != 0)
        {
            const __vector4 square2 = __vmsum3fp(normal, normal);
            __vector4 inverse = __vrsqrtefp(square2);
            correction = __vnmsubfp(square2, __vmulfp(inverse, inverse), one);
            inverse = __vmaddfp(__vmulfp(inverse, half), correction, inverse);
            correction = __vnmsubfp(square2, __vmulfp(inverse, inverse), one);
            inverse = __vmaddfp(__vmulfp(inverse, half), correction, inverse);
            normal = __vmulfp(normal, inverse);
            arResult.normal.mV = normal;
        }
    }
    const __vector4 fatness = __vspltw(__lvlx(&afFatness, 0), 0);
    arResult.position.mV = __vsubfp(arResult.position.mV, __vmulfp(arResult.normal.mV, fatness));
    return 1;
}
} }
