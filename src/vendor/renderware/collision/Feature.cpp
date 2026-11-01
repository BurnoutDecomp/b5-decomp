#include "vendor/renderware/collision/Feature.hpp"
#include <float.h>

// ARTIST 0x82BB9D68: full-lane cross, reciprocal-square-root refinement,
// mask and stores. The epsilon is the value installed at 0x8327EFD0 by
// the original initializer 0x82C73E10.

namespace rw
{
namespace collision
{

const rw::math::vpu::VecFloat VEC_EPSILON(FLT_EPSILON);

void Feature::BuildEdgePlanes(RwBool abCcw, const rw::math::vpu::Vector3& arExtrusionDir)
{
    const __vector4 zero = __vspltisw(0);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const __vector4 all = __vspltisw(-1);
    const __vector4 sign = __vslw(all, all);
    for (s32 i = 0; i < numedges; ++i)
    {
        __vector4 direction = arExtrusionDir.mV;
        if (!abCcw) direction = __vxor(direction, sign);
        const __vector4 edge = edges[i].dir.mV;
        const __vector4 cross = __vpermwi(__vnmsubfp(__vpermwi(edge, 0x63), direction,
            __vmulfp(edge, __vpermwi(direction, 0x63))), 0x63);
        edges[i].pn.mV = cross;
        const __vector4 square = __vmsum3fp(cross, cross);
        const __vector4 estimate = __vrsqrtefp(square);
        const __vector4 valid = __vcmpgtfp(square, VEC_EPSILON.mV);
        const __vector4 estimateSquare = __vmulfp(estimate, estimate);
        const __vector4 halfEstimate = __vmulfp(estimate, half);
        const __vector4 correction = __vnmsubfp(square, estimateSquare, one);
        const __vector4 refined = __vmaddfp(halfEstimate, correction, estimate);
        edges[i].pn.mV = __vmulfp(cross, __vsel(zero, refined, valid));
    }
}

} // namespace collision
} // namespace rw
