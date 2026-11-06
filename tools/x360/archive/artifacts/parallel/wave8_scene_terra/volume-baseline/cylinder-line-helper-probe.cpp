#include "vendor/renderware/collision/LineSegIntersect.hpp"
#include <cmath>
namespace rw { namespace collision {
s32 rwcCylinderLineSegIntersect(Fraction* lpDist, f32 afAxisLengthSq, f32 afRadius,
                                s32 abInvert, s32 abIgnoreInside,
                                rw::math::vpu::Vector3 aLineStart,
                                rw::math::vpu::Vector3 aLineDelta,
                                rw::math::vpu::Vector3 aBase,
                                rw::math::vpu::Vector3 aAxis)
{
    const __vector4 relative = __vsubfp(aBase.mV, aLineStart.mV);
    const f32 radiusLimit = (afAxisLengthSq * afRadius) * afRadius;
    const __vector4 axisPerm = __vpermwi(aAxis.mV, 0x63);
    const __vector4 cross0 = __vpermwi(__vnmsubfp(__vpermwi(relative, 0x63), aAxis.mV,
        __vmulfp(relative, axisPerm)), 0x63);
    const f32 distanceSquare = __vmsum3fp(cross0, cross0).x;
    if (distanceSquare < radiusLimit && !abIgnoreInside)
    {
        lpDist->den = 1.0f;
        lpDist->num = 0.0f;
        return 1;
    }
    const __vector4 crossDelta = __vpermwi(__vnmsubfp(__vpermwi(aLineDelta.mV, 0x63), aAxis.mV,
        __vmulfp(aLineDelta.mV, axisPerm)), 0x63);
    const f32 projection = __vmsum3fp(cross0, crossDelta).x;
    if (!abInvert && !(projection > 0.0f)) return -1;
    const f32 deltaSquare = __vmsum3fp(crossDelta, crossDelta).x;
    volatile f32 product = deltaSquare * distanceSquare;
    volatile f32 first = projection * projection - product;
    const f32 discriminant = deltaSquare * radiusLimit + first;
    if (discriminant < 0.0f) return 0;
    const f32 remaining = projection - deltaSquare;
    if (!abInvert)
    {
        if (!(remaining < 0.0f) && !(remaining * remaining < discriminant)) return 0;
    }
    else
    {
        if (!(remaining < 0.0f) || !(remaining * remaining > discriminant)) return 0;
    }
    const f32 sign = abInvert ? -1.0f : 1.0f;
    const f32 root = static_cast<f32>(std::sqrt(discriminant));
    lpDist->den = deltaSquare;
    lpDist->num = projection - root * sign;
    return 1;
}
} }
