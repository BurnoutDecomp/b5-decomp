#include "vendor/renderware/collision/CapsuleVolume.hpp"
namespace rw { namespace collision {
math::vpu::Vector3 CapsuleVolume::GetBBoxDiag() const
{
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const __vector4 axis = __vandc(mTransform.zAxis.mV, sign);
    const __vector4 height = __vspltw(__lvlx(&mCapsuleData.mfHalfHeight, 0), 0);
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    return math::vpu::Vector3(__vmulfp(__vmaddfp(axis, height, radius), __vcfsx(__vspltisw(2), 0)));
}
RwBool CapsuleVolume::GetBBox(const Matrix44Affine* lpTransform, RwBool /*abTight*/, AABBox& arResult) const
{
    __vector4 axis = mTransform.zAxis.mV;
    __vector4 centre = mTransform.wAxis.mV;
    if (lpTransform)
    {
        const __vector4 tx = lpTransform->xAxis.mV;
        const __vector4 ty = lpTransform->yAxis.mV;
        const __vector4 tz = lpTransform->zAxis.mV;
        const __vector4 tw = lpTransform->wAxis.mV;
        __vector4 a = __vmulfp(__vspltw(axis, 0), tx);
        __vector4 c = __vmaddfp(__vspltw(centre, 0), tx, tw);
        a = __vmaddfp(__vspltw(axis, 1), ty, a);
        c = __vmaddfp(__vspltw(centre, 1), ty, c);
        axis = __vmaddfp(__vspltw(axis, 2), tz, a);
        centre = __vmaddfp(__vspltw(centre, 2), tz, c);
    }
    const __vector4 ones = __vspltisw(-1);
    const __vector4 sign = __vslw(ones, ones);
    const __vector4 height = __vspltw(__lvlx(&mCapsuleData.mfHalfHeight, 0), 0);
    const __vector4 radius = __vspltw(__lvlx(&mfRadius, 0), 0);
    const __vector4 extent = __vmaddfp(__vandc(axis, sign), height, radius);
    arResult.mMin.mV = __vsubfp(centre, extent);
    arResult.mMax.mV = __vaddfp(centre, extent);
    return 1;
}
} }
