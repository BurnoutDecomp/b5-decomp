#include <Windows.h>
#include <d3d9.h>
#include "pc/gcm/renderengine/reflections/SceneRender.h"
#include "pc/gcm/renderengine/reflections/SceneSettings.h"
#include "pc/gcm/renderengine/reflections/LightSubmission.h"
#include "pc/gcm/renderengine/reflections/RenderContext.h"
#include "pc/gcm/renderengine/reflections/ClipDistance.h"
#include "pc/gcm/renderengine/DepthRange.h"
#include "pc/gcm/renderengine/StateBlockRestore.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h"
#include "SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcoronarenderer.h"
#include "pc/gcm/renderengine/device.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>

namespace CgsPC::Reflections
{
    namespace
    {
        struct FaceData
        {
            CgsGraphics::Camera mCamera;
            std::array<renderengine::Corona, 512> maCoronas;
            renderengine::CoronaBuffer mBuffer = {};
            BrnCoronaManager::BrnSubmissionInterface mInterface;
            bool mbValid = false;
        };
        struct SceneFrame
        {
            std::array<FaceData, 6> maFaces;
            std::array<CgsGraphics::Camera, 3> maShadowCameras;
            std::array<bool, 3> mabShadowValid = {};
        };
        SceneFrame saFrames[2];
        u32 suReadFrame = 0;
        FaceData& WriteFace(u32 luFace) { return saFrames[1u - suReadFrame].maFaces[luFace]; }
        FaceData& ReadFace(u32 luFace) { return saFrames[suReadFrame].maFaces[luFace]; }
    }

    void BeginSceneFrame()
    {
        for (FaceData& lrFace : saFrames[1u - suReadFrame].maFaces) lrFace.mbValid = false;
        saFrames[1u - suReadFrame].mabShadowValid.fill(false);
    }
    void PublishSceneFrame() { suReadFrame = 1u - suReadFrame; }
    void SetShadowCamera(u32 luCascade, const CgsGraphics::Camera& lrCamera)
    {
        auto& lrFrame = saFrames[1u - suReadFrame];
        lrFrame.maShadowCameras[luCascade] = lrCamera;
        lrFrame.mabShadowValid[luCascade] = true;
    }
    const CgsGraphics::Camera* GetShadowCamera(u32 luCascade)
    { return saFrames[suReadFrame].mabShadowValid[luCascade] ? &saFrames[suReadFrame].maShadowCameras[luCascade] : nullptr; }

    BrnCoronaManager::BrnSubmissionInterface* LightCapture::BeginFace(u32 luFace, const CgsGraphics::Camera& lrCamera)
    {
        FaceData& lrFace = WriteFace(luFace);
        lrFace.mCamera = lrCamera;
        lrFace.mbValid = true;
        lrFace.mBuffer.mpData = lrFace.maCoronas.data();
        lrFace.mBuffer.muNumCoronas = static_cast<u32>(lrFace.maCoronas.size());
        auto& lrInterface = lrFace.mInterface;
        lrInterface.mpBuffer = &lrFace.mBuffer;
        lrFace.mBuffer.Lock(lrInterface.mBufferIterator);
        const f32 lfHorizontal = lrCamera.maProjectionScalars[1];
        const f32 lfScale = (std::max)(lfHorizontal, 1.0f);
        const Vector4 lvScale = {lfScale, lrCamera.maProjectionScalars[4] / lfHorizontal * lfScale, 0, 0};
        lrInterface.SetCameraInfo(lrCamera.GetViewProjectionMatrix(), lrCamera.GetPosition(), lvScale);
        return Lights().mbEnabled ? &lrInterface : nullptr;
    }

    void LightCapture::SubmitRaceCars(u32 luFace, BrnWorld::RaceCarEntityModule& lrCars)
    {
        if (!Lights().mbEnabled) return;
        auto& lrFace = WriteFace(luFace);
        LightSubmissionScope lScope(&lrFace.mInterface, lrFace.mCamera.GetPosition());
        for (s32 liCar = 0; liCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liCar)
        {
            auto& lrCar = lrCars.maActiveRaceCars[liCar];
            if (!lrCar.IsActive() || lrCar.GetRenderParams()->IsRaceCarHidden()) continue;
            const auto& lrPhysics = lrCars.mRaceCarStreamer.GetPhysicsResourceBringUp(liCar);
            if (!lrPhysics.HasMemoryResource()) continue;
            lrCars.SubmitCoronasForRaceCar(&lrFace.mInterface, lrPhysics, lrCar.GetRenderParams(),
                lrCars.mePlayerActiveRaceCarIndex == static_cast<EActiveRaceCarIndex>(liCar));
        }
    }
    void LightCapture::SubmitTrafficSignals(u32 luFace, BrnTraffic::TrafficEntityModule& lrTraffic)
    {
        if (!Lights().mbEnabled) return;
        auto& lrFace = WriteFace(luFace);
        LightSubmissionScope lScope(&lrFace.mInterface, lrFace.mCamera.GetPosition());
        lrTraffic.RenderTrafficLightCoronas(&lrFace.mInterface, lrFace.mCamera.GetPosition(), lrFace.mCamera.GetDirection());
    }
    u32 LightCapture::GetCoronaCount(u32 luFace) { return ReadFace(luFace).mInterface.mBufferIterator.GetNumCoronasWritten(); }
    bool LightCapture::IsRendererReady(const BrnCoronaManager& lrManager)
    { return lrManager.mbActive && lrManager.m_textureStateAtlas != nullptr; }

    void RenderSceneExtras(u32 luFace, BrnCoronaManager& lrMainLights, bool lbRenderCoronas, bool lbRenderGlass,
        const BrnParticle::ParticleModule::ParticleRenderData* lpParticleData, f32 lfWhiteLevel,
        CgsGraphics::DispatchList* lpGlassList, CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
        CgsGraphics::DispatchObjectContext* lpContext)
    {
        FaceData& lrFace = ReadFace(luFace);
        if (!lrFace.mbValid || ((!lbRenderCoronas || !Lights().mbEnabled)
            && (!lpParticleData || (!Particles().mbEnabled && !Decals().mbEnabled))
            && (!lbRenderGlass || !Glass().mbEnabled))) return;
        IDirect3DDevice9* lpDevice = renderengine::gDevice;
        if (!lpDevice) return;
        // FLAG PC-platform leaf: glass and immediate effects can change shader,
        // stream, sampler, blend and stencil state as well as depth/cull. Restore
        // the complete face state before subsequent geometry or the main view.
        IDirect3DStateBlock9* lpSaved = nullptr;
        if (FAILED(lpDevice->CreateStateBlock(D3DSBT_ALL, &lpSaved))) return;
        const auto lDepthState = renderengine::DepthRangePC::GetState(lpDevice);
        u32 luGlass = 0, luLights = 0;
        {
            ExtrasScope lScope;
            renderengine::DepthRangePC::SetDepthFunction(lpDevice, D3DCMP_LESSEQUAL);
            lpDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
            if (lbRenderGlass && Glass().mbEnabled && lpGlassList && lpGlassList->GetCount())
            {
                luGlass = lpGlassList->GetCount();
                lpGlassList->DispatchAllMeshes(lpInterpreter, lpContext, 0, -1);
            }
            if (lbRenderCoronas && Lights().mbEnabled && LightCapture::IsRendererReady(lrMainLights))
            {
                luLights = LightCapture::GetCoronaCount(luFace);
                renderengine::CoronaBuffer lVisible = {luLights, lrFace.maCoronas.data()};
                if (lVisible.muNumCoronas)
                {
                    renderengine::CoronaRenderer::RenderParameters lParams = {};
                    lParams.mvCameraPositionPlusWhiteLevel = {lrFace.mCamera.GetPosition().x,
                        lrFace.mCamera.GetPosition().y, lrFace.mCamera.GetPosition().z, lfWhiteLevel};
                    lParams.mViewProjectionMatrix = lrFace.mCamera.GetViewProjectionMatrix();
                    const f32 lfHorizontal = lrFace.mCamera.maProjectionScalars[1];
                    const f32 lfScale = (std::max)(lfHorizontal, 1.0f);
                    lParams.mvViewXyScale = {lfScale, lrFace.mCamera.maProjectionScalars[4] / lfHorizontal * lfScale, 0, 0};
                    renderengine::CoronaRenderer::Begin(lParams);
                    renderengine::CoronaRenderer::BatchParameters lBatch = {};
                    lBatch.muNumCoronas = lVisible.muNumCoronas;
                    lBatch.muFlags = renderengine::CoronaRenderer::BatchParameters::FLAG_OCCLUSION_ZTEST;
                    renderengine::CoronaRenderer::Dispatch(lBatch, &lVisible);
                    renderengine::CoronaRenderer::End();
                }
            }
            u32 luParticleBytes = 0;
            u32 luDecalDraws = 0;
            if (Decals().mbEnabled && lpParticleData && lpParticleData->mpParticleModule)
            {
                const f32 lfDistance = Decals().GetDrawDistance(lpParticleData->mCgsCamera.maProjectionScalars[8]);
                luDecalDraws = RenderWithinDistance(lpDevice, lrFace.mCamera, lfDistance, [&] {
                    return DecalCapture::Render(*lpParticleData, lrFace.mCamera);
                });
            }
            if (Particles().mbEnabled && lpParticleData && lpParticleData->mpParticleModule)
            {
                const f32 lfDistance = Particles().GetDrawDistance(lpParticleData->mCgsCamera.maProjectionScalars[8]);
                luParticleBytes = RenderWithinDistance(lpDevice, lrFace.mCamera, lfDistance, [&] {
                    return ParticleCapture::Render(luFace, *lpParticleData->mpParticleModule, *lpParticleData, lrFace.mCamera);
                });
            }
            static const bool sbTrace = std::getenv("BRN_REFLECTION_SCENE_TRACE") != nullptr;
            static u32 sauWitnesses[6] = {};
            const u32 luWitness = (luGlass ? 1u : 0u) | (luLights ? 2u : 0u) | (luParticleBytes ? 4u : 0u) | (luDecalDraws ? 8u : 0u);
            if (sbTrace && !(sauWitnesses[luFace] & (1u << luWitness)))
            {
                sauWitnesses[luFace] |= 1u << luWitness;
                char lacTrace[192];
                std::snprintf(lacTrace, sizeof(lacTrace), "[reflection-scene] face=%u glassMeshes=%u coronas=%u particleBytes=%u decalDraws=%u\n",
                    luFace, luGlass, luLights, luParticleBytes, luDecalDraws);
                CgsDev::Log::WriteToLog(lacTrace);
            }
        }
        renderengine::RestoreStateBlockPC(lpSaved);
        lpSaved->Release();
        renderengine::DepthRangePC::GetState(lpDevice) = lDepthState;
        shadow::Device::ResetShadowing();
    }
}
