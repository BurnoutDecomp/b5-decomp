#pragma once

#include "pc/gcm/renderengine/GraphicsSettings.h"
#include <cstring>
#include <atomic>
struct IDirect3DDevice9;

namespace renderengine
{
    // FLAG PC-platform leaf: the native atlas keeps the original 2:1 cascade
    // aspect and three vertical bands. Clamp requests to actual device limits.
    struct ShadowAtlasSizePC { u32 muWidth, muHeight, muScale; };
    inline std::atomic<u32>& ShadowAtlasScalePC() { static std::atomic<u32> suScale{1u}; return suScale; }
    ShadowAtlasSizePC ChooseShadowAtlasSizePC(IDirect3DDevice9* lpDevice, u32 luRequested);

    // FLAG PC-platform leaf: optional native comparison-filter compensation.
    // The original material bias is retained; only shadow caster state gains
    // the requested amount. The pass bracket restores its unmodified value.
    inline u32 ShadowSlopeBiasForPassPC(u32 luOriginalBits, bool lbShadowPass)
    {
        const f32 lfExtra = GetGraphicsSettingsPC().mfShadowSlopeBias;
        if (!lbShadowPass || lfExtra == 0.0f) return luOriginalBits;
        f32 lfBias;
        std::memcpy(&lfBias, &luOriginalBits, sizeof(lfBias));
        lfBias += lfExtra;
        u32 luResult;
        std::memcpy(&luResult, &lfBias, sizeof(luResult));
        return luResult;
    }
}
