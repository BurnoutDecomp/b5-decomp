#include "vendor/renderware/collision/LineSegIntersect.hpp"
namespace rw { namespace collision {
s32 ThinTriangleLineSegIntersect(VolumeLineSegIntersectResult* lpResult,
                                 rw::math::vpu::Vector3 aLineStart,
                                 rw::math::vpu::Vector3 aLineDelta,
                                 rw::math::vpu::Vector3 aV0,
                                 rw::math::vpu::Vector3 aV1,
                                 rw::math::vpu::Vector3 aV2)
{
    const __vector4 e2 = __vsubfp(aV2.mV, aV0.mV);
    const __vector4 e1 = __vsubfp(aV1.mV, aV0.mV);
    const __vector4 q = __vpermwi(__vnmsubfp(__vpermwi(aLineDelta.mV, 0x63), e2,
        __vmulfp(aLineDelta.mV, __vpermwi(e2, 0x63))), 0x63);
    const f32 det = __vmsum3fp(e1, q).x;
    if (!(det > 1.0e-8f)) return 0;
    const __vector4 tvec = __vsubfp(aLineStart.mV, aV0.mV);
    const f32 lower = det * -1.0e-5f;
    const f32 upper = det - lower;
    const f32 u = __vmsum3fp(tvec, q).x;
    if (u < lower || u > upper) return 0;
    const __vector4 q2 = __vpermwi(__vnmsubfp(__vpermwi(tvec, 0x63), e1,
        __vmulfp(tvec, __vpermwi(e1, 0x63))), 0x63);
    const f32 v = __vmsum3fp(aLineDelta.mV, q2).x;
    if (v < lower || v + u > upper) return 0;
    f32 t = __vmsum3fp(e2, q2).x;
    lpResult->lineParam = t;
    if (t < lower || t > upper) return 0;
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 determinant = __vspltw(__lvlx(&det, 0), 0);
    __vector4 reciprocal = __vrefp(determinant);
    __vector4 correction = __vnmsubfp(reciprocal, determinant, one);
    reciprocal = __vmaddfp(reciprocal, correction, reciprocal);
    correction = __vnmsubfp(reciprocal, determinant, one);
    reciprocal = __vmaddfp(reciprocal, correction, reciprocal);
    const f32 inv = reciprocal.x;
    t = inv * t;
    lpResult->lineParam = t;
    lpResult->volParam = rw::math::vpu::Vector3(inv * u, inv * v, 0.0f);
    const __vector4 time = __vspltw(__lvlx(&lpResult->lineParam, 0), 0);
    lpResult->position.mV = __vmaddfp(aLineDelta.mV, time, aLineStart.mV);
    return 1;
}
    inline __vector4 TriangleUnitNormal(const rw::math::vpu::Vector3& p0,
                                        const rw::math::vpu::Vector3& p1,
                                        const rw::math::vpu::Vector3& p2)
    {
        const __vector4 e2 = __vsubfp(p0.mV, p2.mV);
        const __vector4 e1 = __vsubfp(p0.mV, p1.mV);
        const __vector4 cross = __vpermwi(__vnmsubfp(__vpermwi(e1, 0x63), e2,
            __vmulfp(e1, __vpermwi(e2, 0x63))), 0x63);
        const __vector4 square = __vmsum3fp(cross, cross);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        __vector4 estimate = __vrsqrtefp(square);
        __vector4 correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
        estimate = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        return __vmulfp(cross, estimate);
    }
s32 TriangleLineSegIntersect(VolumeLineSegIntersectResult* lpResult,
                             rw::math::vpu::Vector3 aLineStart,
                             rw::math::vpu::Vector3 aLineDelta,
                             rw::math::vpu::Vector3 aV0,
                             rw::math::vpu::Vector3 aV1,
                             rw::math::vpu::Vector3 aV2,
                             f32 afFatness)
{
    s32 hit;
    if (afFatness == 0.0f)
    {
        hit = ThinTriangleLineSegIntersect(lpResult, aLineStart, aLineDelta, aV0, aV1, aV2);
        if (hit) lpResult->normal.mV = TriangleUnitNormal(aV0, aV1, aV2);
    }
    else
    {
        lpResult->normal.mV = TriangleUnitNormal(aV0, aV1, aV2);
        hit = FatTriangleLineSegIntersect(lpResult, aLineStart, aLineDelta, aV0, aV1, aV2, afFatness);
        if (hit)
        {
            const __vector4 fatness = __vspltw(__lvlx(&afFatness, 0), 0);
            lpResult->position.mV = __vsubfp(lpResult->position.mV, __vmulfp(lpResult->normal.mV, fatness));
        }
    }
    return hit;
}
} }
