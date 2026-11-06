#include "vendor/renderware/collision/CylinderVolume.hpp"
#include "rw/rwcore_structs.h"
namespace rw { namespace collision {
CylinderVolume* CylinderVolume::Initialize(const ::rw::Resource& arResource,
                                           f32 afRadius, f32 afHalfHeight, f32 afFatness)
{
    CylinderVolume* lpVolume = static_cast<CylinderVolume*>(arResource.m_baseResources[0]);
    if (!lpVolume) return 0;
    lpVolume->mfRadius = 0.0f;
    lpVolume->muGroupID = 0;
    lpVolume->muSurfaceID = 0;
    lpVolume->muFlags = 1;
    lpVolume->mpVTable = gVolumeVTable[E_VOLUMETYPE_CYLINDER];
    lpVolume->mfRadius = afFatness;
    lpVolume->mCylinderData.mfInnerRadius = afRadius;
    lpVolume->mCylinderData.mfHalfHeight = afHalfHeight;
    lpVolume->mTransform.xAxis = Vector3(1.0f, 0.0f, 0.0f);
    lpVolume->mTransform.wAxis = Vector3(0.0f, 0.0f, 0.0f);
    lpVolume->mTransform.yAxis = Vector3(0.0f, 1.0f, 0.0f);
    lpVolume->mTransform.zAxis = Vector3(0.0f, 0.0f, 1.0f);
    return lpVolume;
}
math::vpu::Vector3 CylinderVolume::GetBBoxDiag() const
{
    const Vector3 dimensions(mCylinderData.mfInnerRadius, mCylinderData.mfInnerRadius,
                             mCylinderData.mfHalfHeight);
    return Vector3(__vmulfp(dimensions.mV, __vcfsx(__vspltisw(2), 0)));
}
} }
