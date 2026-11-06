#include "vendor/renderware/collision/TriangleVolume.hpp"
#include "rw/rwcore_structs.h"
#include <cstring>
#include <cfloat>
namespace rw { namespace collision { const rw::math::vpu::VecFloat VEC_EPSILON(FLT_EPSILON);
TriangleVolume* TriangleVolume::Initialize(const ::rw::Resource& arResource,
                                           Vector3 arP0, Vector3 arP1, Vector3 arP2)
{
    TriangleVolume* lpVolume = static_cast<TriangleVolume*>(arResource.m_baseResources[0]);
    if (!lpVolume) return 0;
    lpVolume->mfRadius = 0.0f;
    lpVolume->muGroupID = 0;
    lpVolume->mTriangleData.mfEdgeCos0 = -1.0f;
    lpVolume->muSurfaceID = 0;
    lpVolume->mTriangleData.mfEdgeCos1 = -1.0f;
    lpVolume->mTriangleData.mfEdgeCos2 = -1.0f;
    lpVolume->mpVTable = gVolumeVTable[E_VOLUMETYPE_TRIANGLE];
    lpVolume->mTransform.xAxis = arP0;
    lpVolume->muFlags = 0x1e3;
    lpVolume->mTransform.yAxis = arP1;
    lpVolume->mTransform.zAxis = arP2;
    lpVolume->mTransform.wAxis.mV = __vspltisw(0);
    return lpVolume;
}
void TriangleVolume::GetNormal(Vector3& arNormal, const Matrix44Affine* lpTransform) const
{
    __vector4 normal = mTransform.wAxis.mV;
    if (muFlags & 2u)
    {
        const __vector4 p0 = mTransform.xAxis.mV;
        const __vector4 e2 = __vsubfp(mTransform.zAxis.mV, p0);
        const __vector4 e1 = __vsubfp(mTransform.yAxis.mV, p0);
        muFlags &= ~2u;
        const __vector4 perm2 = __vpermwi(e2, 0x63);
        const __vector4 perm1 = __vpermwi(e1, 0x63);
        const __vector4 product = __vmulfp(e1, perm2);
        const __vector4 cross = __vpermwi(__vnmsubfp(perm1, e2, product), 0x63);
        const __vector4 square = __vmsum3fp(cross, cross);
        const __vector4 valid = __vcmpgtfp(square, VEC_EPSILON.mV);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 half = __vcfsx(__vspltisw(1), 1);
        const __vector4 estimate = __vrsqrtefp(square);
        const __vector4 correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
        const __vector4 inverse = __vmaddfp(__vmulfp(estimate, half), correction, estimate);
        normal = __vsel(cross, __vmulfp(cross, inverse), valid);
        mTransform.wAxis.mV = normal;
    }
    if (lpTransform)
    {
        __vector4 rotated = __vmulfp(lpTransform->xAxis.mV, __vspltw(normal, 0));
        rotated = __vmaddfp(lpTransform->yAxis.mV, __vspltw(normal, 1), rotated);
        normal = __vmaddfp(lpTransform->zAxis.mV, __vspltw(normal, 2), rotated);
    }
    arNormal.mV = normal;
}
static __vector4 TriangleTransformPoint(__vector4 point, const Matrix44Affine& transform)
{
    __vector4 result = __vmaddfp(transform.xAxis.mV, __vspltw(point, 0), transform.wAxis.mV);
    result = __vmaddfp(transform.yAxis.mV, __vspltw(point, 1), result);
    return __vmaddfp(transform.zAxis.mV, __vspltw(point, 2), result);
}
void TriangleVolume::GetPoints(Vector3& arP0, Vector3& arP1, Vector3& arP2, const Matrix44Affine* lpTransform) const
{
    if (lpTransform)
    {
        arP0.mV = TriangleTransformPoint(mTransform.xAxis.mV, *lpTransform);
        arP1.mV = TriangleTransformPoint(mTransform.yAxis.mV, *lpTransform);
        arP2.mV = TriangleTransformPoint(mTransform.zAxis.mV, *lpTransform);
    }
    else
    {
        arP0 = mTransform.xAxis;
        arP1 = mTransform.yAxis;
        arP2 = mTransform.zAxis;
    }
}
RwBool TriangleVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    __vector4 p0 = mTransform.xAxis.mV;
    __vector4 p1 = mTransform.yAxis.mV;
    __vector4 p2 = mTransform.zAxis.mV;
    if (lpTransform)
    {
        p0 = TriangleTransformPoint(p0, *lpTransform);
        p1 = TriangleTransformPoint(p1, *lpTransform);
        p2 = TriangleTransformPoint(p2, *lpTransform);
    }
    const __vector4 low = __vminfp(p0, __vminfp(p1, p2));
    const __vector4 high = __vmaxfp(p0, __vmaxfp(p1, p2));
    arResult.mMin.mV = low;
    arResult.mMax.mV = high;
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    arResult.mMin.mV = __vsubfp(low, radius);
    arResult.mMax.mV = __vaddfp(arResult.mMax.mV, radius);
    return 1;
}
math::vpu::Vector3 TriangleVolume::GetBBoxDiag() const
{
    const __vector4 p0 = mTransform.xAxis.mV;
    const __vector4 p1 = mTransform.yAxis.mV;
    const __vector4 p2 = mTransform.zAxis.mV;
    const __vector4 low = __vminfp(p0, __vminfp(p1, p2));
    const __vector4 high = __vmaxfp(p0, __vmaxfp(p1, p2));
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    return Vector3(__vsubfp(__vaddfp(high, radius), __vsubfp(low, radius)));
}
RwBool TriangleVolume::GetMaximumFeature(RwBool /*abCcw*/, Vector3 /*arDir*/, Feature& arResult) const
{
    const Vector3 p0 = mTransform.xAxis;
    const Vector3 p1 = mTransform.yAxis;
    const Vector3 p2 = mTransform.zAxis;
    const FeatureEdge edge0(p0, p1);
    memcpy(&arResult.edges[0], &edge0, sizeof(edge0));
    const FeatureEdge edge1(p1, p2);
    memcpy(&arResult.edges[1], &edge1, sizeof(edge1));
    const FeatureEdge edge2(p2, p0);
    memcpy(&arResult.edges[2], &edge2, sizeof(edge2));
    arResult.numedges = 3;
    return 1;
}
} }
