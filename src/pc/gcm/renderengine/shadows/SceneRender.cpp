#define NOMINMAX
#include <Windows.h>
#include <d3d9.h>
#include "pc/gcm/renderengine/shadows/SceneRender.h"
#include "pc/gcm/renderengine/shadows/SceneSettings.h"
#include "pc/gcm/renderengine/shadows/RenderContext.h"
#include "pc/gcm/renderengine/DepthRange.h"
#include "pc/gcm/renderengine/device.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
namespace CgsPC::Shadows
{
    bool HasDebris(const BrnParticle::ParticleModule::ParticleRenderData* lpData)
    {
        if (!Debris().mbEnabled || !lpData || !lpData->mpParticleModule || lpData->mbPlayingEffectsSuspendedPC) return false;
        if (!(lpData->muFlags & BrnParticle::ParticleModule::ParticleRenderData::eRenderDataFlagRenderDebris)) return false;
        if (!Reflections::GetShadowCamera(0) && !Reflections::GetShadowCamera(1) && !Reflections::GetShadowCamera(2)) return false;
        return Reflections::ParticleCapture::HasDebrisShadow(*lpData, Debris().GetDrawDistance(NormalDistance()));
    }

    // FLAG PC-platform leaf: emit the original physical debris meshes into the
    // existing three cascade targets. No billboard/opacity shadow is invented.
    void RenderDebris(u32 luCascade, const BrnParticle::ParticleModule::ParticleRenderData* lpData)
    {
        if (!Debris().mbEnabled || !lpData || !lpData->mpParticleModule || lpData->mbPlayingEffectsSuspendedPC) return;
        if (!(lpData->muFlags & BrnParticle::ParticleModule::ParticleRenderData::eRenderDataFlagRenderDebris)) return;
        const auto* lpCamera = Reflections::GetShadowCamera(luCascade);
        auto* lpDevice = renderengine::gDevice;
        if (!lpCamera || !lpDevice) return;
        IDirect3DStateBlock9* lpSaved = nullptr;
        if (FAILED(lpDevice->CreateStateBlock(D3DSBT_ALL, &lpSaved))) return;
        const auto leLogicalDepth = renderengine::DepthRangePC::GetState(lpDevice).meLogicalCompare;
        lpDevice->GetRenderState(D3DRS_ZFUNC, &suDepthFunction);
        lpDevice->GetRenderState(D3DRS_DEPTHBIAS, &suDepthBias);
        lpDevice->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, &suSlopeBias);
        sbDrawingDebris = true;
        lpDevice->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        const u32 luCount = Reflections::ParticleCapture::RenderDebrisShadow(*lpData, *lpCamera,
            Debris().GetDrawDistance(NormalDistance()));
        sbDrawingDebris = false;
        lpSaved->Apply(); lpSaved->Release();
        renderengine::DepthRangePC::GetState(lpDevice).meLogicalCompare = leLogicalDepth;
        shadow::Device::ResetShadowing();
        static const bool sbTrace = std::getenv("BRN_SMALL_SHADOW_TRACE") != nullptr;
        static bool sabSeen[3][2] = {};
        if (sbTrace && !sabSeen[luCascade][luCount != 0])
        {
            sabSeen[luCascade][luCount != 0] = true;
            char lacMessage[128];
            std::snprintf(lacMessage, sizeof(lacMessage), "[small-shadow] cascade=%u solidDebris=%u cutoff=%.1f\n",
                luCascade, luCount, Debris().GetDrawDistance(NormalDistance()));
            CgsDev::Log::WriteToLog(lacMessage);
        }
    }
}
