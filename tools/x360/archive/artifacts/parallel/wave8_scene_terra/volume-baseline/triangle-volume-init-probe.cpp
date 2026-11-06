#include "vendor/renderware/collision/TriangleVolume.hpp"
#include "rw/rwcore_structs.h"
namespace rw { namespace collision {
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
} }
