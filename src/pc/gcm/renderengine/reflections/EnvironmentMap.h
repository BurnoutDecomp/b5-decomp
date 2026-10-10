#pragma once
// FLAG PC-platform leaf: match engine cube-face projection to D3D9 cube lookup.
#include "GameShared/GameClasses/Graphics/CgsCamera.h"

namespace renderengine
{
    inline void SetEnvironmentMapProjectionPC(CgsGraphics::Camera& lrCamera, u32 luFaceSize = 128u)
    {
        lrCamera.UpdatePerspectiveProjectionMatrix();
        // ARTIST Camera::LookAt @0x827F95B4..EC retains cross(dir,up) as
        // horizontal basis. D3D9 cube lookup needs cross(up,dir): e.g. +Z
        // lies LEFT in the +X face. Mirror clip X, leaving engine camera
        // scalars, view, query volume and world/reflection directions intact.
        lrCamera.mProjection.xAxis.x = -lrCamera.maProjectionScalars[1];
        // D3D9 raster pixel centres are integers; cube sampling addresses texel
        // centres at (n + 0.5) / size. Shift clip x/y by half a viewport pixel
        // so every face captures the directions the cube sampler will request.
        // The shipped cube is 128 square (BrnRendererMemory::CreateEnvmapBuffer).
        const f32 lfInvFaceSize = 1.0f / static_cast<f32>(luFaceSize);
        lrCamera.mProjection.zAxis.x -= lrCamera.mProjection.zAxis.w * lfInvFaceSize;
        lrCamera.mProjection.zAxis.y += lrCamera.mProjection.zAxis.w * lfInvFaceSize;
        lrCamera.mProjection.wAxis.x -= lrCamera.mProjection.wAxis.w * lfInvFaceSize;
        lrCamera.mProjection.wAxis.y += lrCamera.mProjection.wAxis.w * lfInvFaceSize;
        lrCamera.UpdateViewProjectionMatrix();
    }
}
