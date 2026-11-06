#include "vendor/renderware/collision/CapsuleVolume.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"
namespace rw { namespace collision { extern const GPInstance::VolumeMethods g_aGPVolumeMethods[GPInstance::NUMINTERNALTYPES];
RwBool CapsuleVolume::CreateGPInstance(GPInstance& arInst, const Matrix44Affine* lpTransform) const
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
    arInst.mVolumeTag = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInst.mPos.mV = centre;
    arInst.mNumFaceNormals = 0;
    arInst.mUserTag = 0;
    arInst.mVolumeType = GPInstance::CAPSULE;
    arInst.mNumEdgeDirections = 1;
    arInst.mFatness = mfRadius;
    arInst.mEdgeDirections[0].mV = axis;
    arInst.mDimensions.mV.x = mCapsuleData.mfHalfHeight;
    arInst.mFlags = muFlags;
    arInst.mMethods = g_aGPVolumeMethods[GPInstance::CAPSULE];
    return 1;
}
} }
