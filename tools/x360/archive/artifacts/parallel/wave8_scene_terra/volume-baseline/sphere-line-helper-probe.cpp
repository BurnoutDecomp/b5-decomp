#include "vendor/renderware/collision/LineSegIntersect.hpp"
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
} }
