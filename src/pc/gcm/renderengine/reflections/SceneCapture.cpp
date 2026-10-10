#include "pc/gcm/renderengine/reflections/SceneCapture.h"
#include "pc/gcm/renderengine/reflections/SceneSettings.h"
#include "pc/gcm/renderengine/reflections/ReflectionDistance.h"
#include "GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.h"
#include "GameSource/World/BrnShaderLodInfo.h"
#include "GameShared/GameClasses/Graphics/Instances/CgsInstance.h"
#include "GameShared/GameClasses/Graphics/CgsModel.h"
#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"
#include "GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModule.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/BrnTrafficEntityModuleIO.h"
#include <array>

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: use face-query ids instead of the main camera's
    // traffic pre-dispatch array. This keeps visibility and cleanup history intact.
    void TrafficCapture::Submit(BrnTraffic::TrafficEntityModule& lrTraffic,
        const Array<CgsSceneManager::EntityId, 650u>& lrVisible,
        const BrnTraffic::BrnTrafficIO::InputBuffer_Dispatch* lpInput,
        const CgsGraphics::Camera& lrCamera, Vector4 lvFogScattering, Vector4 lvFogColour, s32 liFaceList)
    {
        if (!Traffic().mbEnabled || !lpInput || lrTraffic.meState != BrnTraffic::TrafficEntityModule::E_STATE_RUNNING
            || lrTraffic.meEmptyTrafficPoolState != BrnTraffic::TrafficEntityModule::E_EMPTYTRAFFICPOOLSTATE_IDLE) return;
        struct Candidate { f32 mfDistanceSquared; u32 muIndex; };
        std::array<Candidate, BrnTraffic::KU_MAX_TOTAL_TRAFFIC> laCandidates;
        std::array<bool, BrnTraffic::KU_MAX_TOTAL_TRAFFIC> labSeen = {};
        u32 luCount = 0;
        const f32 lfNormalDistance = std::sqrt((std::max)(lrTraffic.mfRenderCullDistanceSq, 0.0f));
        for (u32 luVisible = 0; luVisible < lrVisible.GetLength(); ++luVisible)
        {
            const u32 luIndex = lrVisible.GetItem(luVisible).GetEntityIndex();
            if (luIndex >= labSeen.size() || labSeen[luIndex]) continue;
            labSeen[luIndex] = true;
            const auto* lpVehicle = lrTraffic.GetVehicle(luIndex);
            if (!lpVehicle || !lpVehicle->IsAlive()) continue;
            const f32 lfDistanceSquared = rw::math::vpu::MagnitudeSquared(lrCamera.GetPosition() - lrTraffic.GetVehicleTransform(luIndex).Pos());
            if (Traffic().IsVisible(lfDistanceSquared, lfNormalDistance))
                laCandidates[luCount++] = {lfDistanceSquared, luIndex};
        }
        std::sort(laCandidates.begin(), laCandidates.begin() + luCount,
            [](const Candidate& lrA, const Candidate& lrB) { return lrA.mfDistanceSquared < lrB.mfDistanceSquared; });
        luCount = (std::min)(luCount, (std::min)(lrTraffic.muMaxVehiclesToRender, 64u));
        sfTrafficNormalDistance = lfNormalDistance;
        VehicleScope lScope(E_CAPTURE_TRAFFIC, liFaceList, lfNormalDistance);
        lpInput->LockForRead();
        s32 liDamagedVehicles = 0;
        for (u32 luCandidate = 0; luCandidate < luCount; ++luCandidate)
        {
            const Vector4 lvLights = {};
            lrTraffic.RenderTrafficCar(lpInput->GetDispatchFrame(), laCandidates[luCandidate].muIndex,
                lrCamera.GetPosition(), lvFogScattering, lvFogColour, nullptr,
                liFaceList, liFaceList, liFaceList, lpInput->GetShadowMap(),
                CgsGraphics::Model::E_STATE_LOD_2, lvLights, lvLights, &liDamagedVehicles);
        }
        lpInput->UnlockForRead();
    }

    // FLAG PC-platform leaf: fill the distant shell with the existing resident
    // backdrop meshes, including stand-ins suppressed by main-view streaming.
    // Do not add/remove scene entities or change zone/streaming state for a cube.
    void WorldCapture::SubmitBackdrops(BrnWorld::WorldEntityModule& lrWorld, CgsGraphics::DispatchFrame* lpFrame,
        const CgsGraphics::Camera& lrCamera, const BrnWorld::ShaderLodInfo& lrShaderLod, s32 liFaceList)
    {
        const ObjectSettings& lrSettings = Backdrops();
        if (!lrSettings.mbEnabled || !lpFrame || lrWorld.miCurrentBackdropZoneId == -1) return;
        const s32 liIndex = lrWorld.mWorldGraphicsStreamer.GetIndexFromId(lrWorld.miCurrentBackdropZoneId);
        if (liIndex == -1) return;
        const CgsGraphics::InstanceList* lpInstances = lrWorld.mWorldGraphicsStreamer.GetInstanceList(liIndex);
        if (!lpInstances) return;
        const f32 lfWorldRadius = renderengine::ExtendEnvironmentMapDrawDistancePC(75.0f);
        CgsGraphics::DispatchList* lpList = lpFrame->GetList(liFaceList);
        for (u32 luInstance = lpInstances->muNumInstances; luInstance < lpInstances->muArraySize; ++luInstance)
        {
            const CgsGraphics::Instance* lpInstance = lpInstances->GetInstance(luInstance);
            const CgsGraphics::Model* lpModel = lpInstance->mpModel;
            if (!lpModel) continue;
            const f32 lfDistanceSquared = rw::math::vpu::MagnitudeSquared(lrCamera.GetPosition() - lpInstance->mTransform.Pos());
            if (lfDistanceSquared <= lfWorldRadius * lfWorldRadius) continue;
            const f32 lfAuthored = std::sqrt((std::max)(lpInstance->mfMaxDrawDistanceSq, 0.0f));
            if (!lrSettings.IsVisible(lfDistanceSquared, lfAuthored)) continue;
            const s32 liLod = lrSettings.SelectLod(lpModel, lfDistanceSquared);
            if (liLod < 0) continue;
            const Renderable* lpRenderable = lpModel->GetRenderable(static_cast<CgsGraphics::Model::State>(liLod));
            if (!lpRenderable) continue;
            CgsGraphics::mShaderConstantTable.SetShaderConstantData(0, lpInstance->mTransform);
            lpFrame->GetBin().BeginPacket();
            CgsGraphics::DrawRenderable::AddToBin(lpRenderable, lpFrame, (lpList->GetCount() & 0x7Fu) == 0,
                static_cast<s8>(liFaceList), static_cast<s8>(liFaceList), 1,
                static_cast<u8>(lrShaderLod.GetEnvMapTechnique()), false, 0xFFu, 0, 0, 0);
            lpList->Submit(0, lpFrame->GetBin().EndPacket());
        }
    }
}
