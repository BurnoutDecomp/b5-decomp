#pragma once

#include "BrnCommonTypes.h"

namespace renderengine
{
    // FLAG PC-platform leaf: native SM3 receivers use the camera that fitted the
    // shadow atlas for cascade selection and fading, including cube captures.
    // c255 is reserved by the matching vertex-shader compilation option; it is
    // uploaded only for programs whose CTAB declares ShadowMap_ViewDepthPC.
    const u32 KU_SHADOW_RECEIVER_DEPTH_REGISTER_PC = 255u;
    inline Vector4& ShadowReceiverViewDepthPC()
    {
        static Vector4 sViewDepth = {0.0f, 0.0f, 0.0f, 0.0f};
        return sViewDepth;
    }

    inline void SetShadowReceiverCameraPC(const Matrix44& lrViewProjection)
    {
        ShadowReceiverViewDepthPC() = {lrViewProjection.xAxis.w, lrViewProjection.yAxis.w,
                                      lrViewProjection.zAxis.w, lrViewProjection.wAxis.w};
    }
}
