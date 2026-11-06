#include "vendor/renderware/collision/TriangleVolume.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"
#include <cfloat>
namespace rw { namespace collision { const rw::math::vpu::VecFloat VEC_EPSILON(FLT_EPSILON); extern const GPInstance::VolumeMethods g_aGPVolumeMethods[GPInstance::NUMINTERNALTYPES];
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
static __vector4 TriangleInverseLength(__vector4 delta)
{
    const __vector4 square = __vmsum3fp(delta, delta);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const __vector4 estimate = __vrsqrtefp(square);
    const __vector4 correction = __vnmsubfp(square, __vmulfp(estimate, estimate), one);
    return __vmaddfp(__vmulfp(estimate, half), correction, estimate);
}
static __vector4 TriangleLengthFromInverse(__vector4 inverse)
{
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    __vector4 estimate = __vrefp(inverse);
    __vector4 correction = __vnmsubfp(estimate, inverse, one);
    estimate = __vmaddfp(estimate, correction, estimate);
    correction = __vnmsubfp(estimate, inverse, one);
    return __vmaddfp(estimate, correction, estimate);
}
RwBool TriangleVolume::CreateGPInstance(GPInstance& arInst, const Matrix44Affine* lpTransform) const
{
    __vector4 p0 = mTransform.xAxis.mV;
    __vector4 p1 = mTransform.yAxis.mV;
    __vector4 p2 = mTransform.zAxis.mV;
    Vector3 localNormal;
    GetNormal(localNormal, 0);
    __vector4 normal = localNormal.mV;
    if (lpTransform)
    {
        const __vector4 zero = __vspltisw(0);
        const __vector4 one = __vcfsx(__vspltisw(1), 0);
        const __vector4 tx = __vrlimi(lpTransform->xAxis.mV, zero, 1, 0);
        const __vector4 ty = __vrlimi(lpTransform->yAxis.mV, zero, 1, 0);
        const __vector4 tz = __vrlimi(lpTransform->zAxis.mV, zero, 1, 0);
        const __vector4 tw = __vrlimi(lpTransform->wAxis.mV, one, 1, 0);
        __vector4 a = __vmaddfp(tx, __vspltw(p0, 0), tw);
        __vector4 b = __vmaddfp(tx, __vspltw(p1, 0), tw);
        __vector4 c = __vmaddfp(tx, __vspltw(p2, 0), tw);
        __vector4 n = __vmulfp(tx, __vspltw(normal, 0));
        a = __vmaddfp(ty, __vspltw(p0, 1), a);
        b = __vmaddfp(ty, __vspltw(p1, 1), b);
        c = __vmaddfp(ty, __vspltw(p2, 1), c);
        n = __vmaddfp(ty, __vspltw(normal, 1), n);
        p0 = __vmaddfp(tz, __vspltw(p0, 2), a);
        p1 = __vmaddfp(tz, __vspltw(p1, 2), b);
        p2 = __vmaddfp(tz, __vspltw(p2, 2), c);
        normal = __vmaddfp(tz, __vspltw(normal, 2), n);
    }
    const __vector4 e0 = __vsubfp(p2, p0);
    const __vector4 e1 = __vsubfp(p1, p2);
    const __vector4 e2 = __vsubfp(p0, p1);
    const __vector4 i0 = TriangleInverseLength(e0);
    const __vector4 i1 = TriangleInverseLength(e1);
    const __vector4 i2 = TriangleInverseLength(e2);
    const __vector4 l0 = TriangleLengthFromInverse(i0);
    const __vector4 l1 = TriangleLengthFromInverse(i1);
    const __vector4 l2 = TriangleLengthFromInverse(i2);
    __vector4 dimensions = __vrlimi(l0, l1, 4, 0);
    dimensions = __vrlimi(dimensions, l2, 2, 0);
    arInst.mVolumeType = GPInstance::TRIANGLE;
    arInst.mNumEdgeDirections = 3;
    arInst.mPos.mV = p0;
    arInst.mNumFaceNormals = 1;
    arInst.mUserTag = 0;
    arInst.mVolumeTag = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInst.mFaceNormals[1].mV = p1;
    arInst.mFaceNormals[2].mV = p2;
    arInst.mFatness = mfRadius;
    arInst.mFaceNormals[0].mV = normal;
    arInst.mEdgeData[2] = mTriangleData.mfEdgeCos2;
    arInst.mEdgeData[1] = mTriangleData.mfEdgeCos1;
    arInst.mEdgeData[0] = mTriangleData.mfEdgeCos0;
    arInst.mFlags = muFlags;
    arInst.mDimensions.mV = dimensions;
    arInst.mEdgeDirections[0].mV = __vmulfp(e0, i0);
    arInst.mEdgeDirections[1].mV = __vmulfp(e1, i1);
    arInst.mEdgeDirections[2].mV = __vmulfp(e2, i2);
    arInst.mMethods = g_aGPVolumeMethods[GPInstance::TRIANGLE];
    return 1;
}
} }
