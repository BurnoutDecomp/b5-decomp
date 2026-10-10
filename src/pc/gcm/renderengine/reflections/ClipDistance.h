#pragma once

#include "pc/gcm/renderengine/reflections/RenderContext.h"
#include "GameShared/GameClasses/Graphics/CgsCamera.h"

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: preserve the geometry's projection/depth buffer
    // while applying independent far cutoffs to immediate effects and decals.
    template<class Draw>
    u32 RenderWithinDistance(IDirect3DDevice9* lpDevice, const CgsGraphics::Camera& lrCamera,
        f32 lfDistance, const Draw& lrDraw)
    {
        if (!(lfDistance > lrCamera.maProjectionScalars[7])) return 0;
        DWORD luClipMask = 0;
        f32 lafOldPlane[4] = {};
        lpDevice->GetRenderState(D3DRS_CLIPPLANEENABLE, &luClipMask);
        lpDevice->GetClipPlane(0, lafOldPlane);
        const auto& lrProjection = lrCamera.mProjection;
        const f32 lfClipZ = (lfDistance * lrProjection.zAxis.z + lrProjection.wAxis.z)
            / (lfDistance * lrProjection.zAxis.w + lrProjection.wAxis.w);
        const f32 lafCutoff[4] = {0, 0, -1, lfClipZ};
        const DWORD luPreviousExtrasMask = suExtrasClipMask;
        lpDevice->SetClipPlane(0, lafCutoff);
        suExtrasClipMask |= luClipMask | 1u;
        lpDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, luClipMask | suExtrasClipMask);
        const u32 luResult = lrDraw();
        suExtrasClipMask = luPreviousExtrasMask;
        lpDevice->SetClipPlane(0, lafOldPlane);
        lpDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, luClipMask);
        return luResult;
    }
}
