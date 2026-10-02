#pragma once
// FLAG PC-platform leaf: match engine cube-face projection to D3D9 cube lookup.
#include "GameShared/GameClasses/Graphics/CgsCamera.h"

namespace renderengine
{
    inline void SetEnvironmentMapProjectionPC(CgsGraphics::Camera& lrCamera)
    {
        lrCamera.UpdatePerspectiveProjectionMatrix();
        // ARTIST Camera::LookAt @0x827F95B4..EC retains cross(dir,up) as
        // horizontal basis. D3D9 cube lookup needs cross(up,dir): e.g. +Z
        // lies LEFT in the +X face. Mirror clip X, leaving engine camera
        // scalars, view, query volume and world/reflection directions intact.
        lrCamera.mProjection.xAxis.x = -lrCamera.maProjectionScalars[1];
        lrCamera.UpdateViewProjectionMatrix();
    }
}
