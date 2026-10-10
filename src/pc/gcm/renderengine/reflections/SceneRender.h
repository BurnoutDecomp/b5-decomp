#pragma once

#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameSource/Graphics/BrnCoronaManager.h"
#include "GameSource/Effects/Particles/ParticleModule.h"

namespace BrnWorld { class RaceCarEntityModule; }
namespace BrnTraffic { class TrafficEntityModule; }
namespace CgsGraphics { class DispatchList; struct DispatchPacketInterpreter; struct DispatchObjectContext; }

namespace CgsPC::Reflections
{
    // FLAG PC-platform leaf: face cameras and light inputs follow the joined
    // command-frame publication, independently from the normal corona buffer.
    struct LightCapture
    {
        static BrnCoronaManager::BrnSubmissionInterface* BeginFace(u32 luFace, const CgsGraphics::Camera& lrCamera);
        static void SubmitRaceCars(u32 luFace, BrnWorld::RaceCarEntityModule& lrCars);
        static void SubmitTrafficSignals(u32 luFace, BrnTraffic::TrafficEntityModule& lrTraffic);
        static u32 GetCoronaCount(u32 luFace);
        static bool IsRendererReady(const BrnCoronaManager& lrManager);
    };
    void BeginSceneFrame();
    void PublishSceneFrame();
    void SetShadowCamera(u32 luCascade, const CgsGraphics::Camera& lrCamera);
    const CgsGraphics::Camera* GetShadowCamera(u32 luCascade);
    void RenderSceneExtras(u32 luFace, BrnCoronaManager& lrMainLights, bool lbRenderCoronas, bool lbRenderGlass,
        const BrnParticle::ParticleModule::ParticleRenderData* lpParticleData, f32 lfWhiteLevel,
        CgsGraphics::DispatchList* lpGlassList, CgsGraphics::DispatchPacketInterpreter* lpInterpreter,
        CgsGraphics::DispatchObjectContext* lpContext);

    struct ParticleCapture
    {
        static void Publish(BrnParticle::ParticleModule& lrParticles);
        static bool Prepare(const BrnParticle::ParticleModule::ParticleRenderData* lpData);
        static u32 Render(u32 luFace, BrnParticle::ParticleModule& lrParticles,
            const BrnParticle::ParticleModule::ParticleRenderData& lrData, const CgsGraphics::Camera& lrCamera);
        static bool HasDebrisShadow(const BrnParticle::ParticleModule::ParticleRenderData& lrData, f32 lfDistance);
        static u32 RenderDebrisShadow(const BrnParticle::ParticleModule::ParticleRenderData& lrData,
            const CgsGraphics::Camera& lrCamera, f32 lfDistance);
    };
}
