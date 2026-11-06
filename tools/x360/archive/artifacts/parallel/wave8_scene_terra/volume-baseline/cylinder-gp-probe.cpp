#include "vendor/renderware/collision/CylinderVolume.hpp"
#include "vendor/renderware/collision/GPInstance.hpp"
namespace rw { namespace collision { extern const GPInstance::VolumeMethods g_aGPVolumeMethods[GPInstance::NUMINTERNALTYPES];
RwBool CylinderVolume::CreateGPInstance(GPInstance& arInst, const Matrix44Affine* lpTransform) const
{
    __vector4 lx = mTransform.xAxis.mV;
    __vector4 ly = mTransform.yAxis.mV;
    __vector4 lz = mTransform.zAxis.mV;
    __vector4 lw = mTransform.wAxis.mV;
    if (lpTransform)
    {
        const __vector4 tx = lpTransform->xAxis.mV;
        const __vector4 ty = lpTransform->yAxis.mV;
        const __vector4 tz = lpTransform->zAxis.mV;
        const __vector4 tw = lpTransform->wAxis.mV;
        __vector4 x = __vmulfp(__vspltw(lx, 0), tx);
        __vector4 y = __vmulfp(__vspltw(ly, 0), tx);
        __vector4 z = __vmulfp(__vspltw(lz, 0), tx);
        __vector4 w = __vmaddfp(__vspltw(lw, 0), tx, tw);
        x = __vmaddfp(__vspltw(lx, 1), ty, x);
        y = __vmaddfp(__vspltw(ly, 1), ty, y);
        z = __vmaddfp(__vspltw(lz, 1), ty, z);
        w = __vmaddfp(__vspltw(lw, 1), ty, w);
        lx = __vmaddfp(__vspltw(lx, 2), tz, x);
        ly = __vmaddfp(__vspltw(ly, 2), tz, y);
        lz = __vmaddfp(__vspltw(lz, 2), tz, z);
        lw = __vmaddfp(__vspltw(lw, 2), tz, w);
    }
    arInst.mVolumeTag = static_cast<u32>(reinterpret_cast<uintptr_t>(this));
    arInst.mPos.mV = lw;
    arInst.mNumFaceNormals = 1;
    arInst.mVolumeType = GPInstance::CYLINDER;
    arInst.mNumEdgeDirections = 1;
    arInst.mUserTag = 0;
    arInst.mFaceNormals[1].mV = ly;
    arInst.mFaceNormals[2].mV = lx;
    arInst.mEdgeDirections[0].mV = lz;
    arInst.mFaceNormals[0].mV = lz;
    arInst.mDimensions.mV.x = mCylinderData.mfHalfHeight;
    arInst.mDimensions.mV.y = mCylinderData.mfInnerRadius;
    arInst.mFlags = muFlags;
    arInst.mFatness = mfRadius;
    arInst.mMethods = g_aGPVolumeMethods[GPInstance::CYLINDER];
    return 1;
}
} }
