#pragma once

#include "BrnCommonTypes.h"

namespace renderengine
{
    // FLAG PC-platform leaf: native SM3 receivers use the camera that fitted the
    // shadow atlas for cascade selection and fading, including cube captures.
    // c255 is reserved by the matching vertex-shader compilation option; it is
    // uploaded only for programs whose CTAB declares ShadowMap_ViewDepthPC.
    const u32 KU_SHADOW_RECEIVER_DEPTH_REGISTER_PC = 255u;
    const u32 KU_SHADOW_RECEIVER_BOUNDS_REGISTER_PC = 223u;
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

    // FLAG PC-platform leaf: the shared main-view atlas does not cover every
    // reflected receiver. Only cube draws opt into tile/depth coverage checks.
    // The matching pixel programs reserve c223; older programs retain that slot.
    inline Vector4& ShadowReceiverReflectionPC()
    {
        static Vector4 sReflection = {0.0f, 0.0f, 0.0f, 0.0f};
        return sReflection;
    }

    inline void SetShadowReceiverReflectionPC(bool lbReflection)
    {
        ShadowReceiverReflectionPC().x = lbReflection ? 1.0f : 0.0f;
    }

    inline void SetShadowReceiverAtlasSizePC(u32 luWidth, u32 luHeight)
    {
        ShadowReceiverReflectionPC().y = luWidth ? 0.5f / static_cast<f32>(luWidth) : 0.0f;
        ShadowReceiverReflectionPC().z = luHeight ? 0.5f / static_cast<f32>(luHeight) : 0.0f;
    }
}
