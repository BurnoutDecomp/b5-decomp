#include "GameSource/Graphics/BrnRendererModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace BrnGraphics
{
    void DebugComponent::Construct(::BrnRendererModule* lpRendererModule)
    {
        CGS_ASSERT(lpRendererModule != nullptr, "lpRendererModule != NULL");
        mpRendererModule = lpRendererModule;
        mfSunFlareOverrideBrightness = 0.3f;
        mbSunFlareOverrideBrightness = false;
    }

    const bool DebugComponent::GetOverrideSunBrightness()
    {
        return mbSunFlareOverrideBrightness;
    }

    const f32 DebugComponent::GetOverriddenSunBrightness()
    {
        return mfSunFlareOverrideBrightness;
    }

    const char* DebugComponent::GetName() const
    {
        return "RendererModule";
    }

    const char* DebugComponent::GetPath() const
    {
        return "Renderer";
    }

    void DebugComponent::OnActivate()
    {
        RegisterVariable(&mpRendererModule->mRenderSwitches.mbRenderShadows, "Render shadow map");
        RegisterVariable(&mpRendererModule->mRenderSwitches.mbRenderEnvmap, "Render environment map");
        RegisterVariable(&mpRendererModule->mRenderSwitches.mbRenderWorld, "Render world");
        RegisterVariable(&mpRendererModule->mRenderSwitches.mbRenderProps, "Render props");
        RegisterVariable(&mpRendererModule->mRenderSwitches.mbRenderRaceCars, "Render race cars");
        RegisterVariable(&mpRendererModule->mRenderSwitches.mbRenderTraffic, "Render traffic cars");
        RegisterVariable(&mpRendererModule->mbRenderWorldOpaque, "Render world opaque");
        RegisterVariable(&mpRendererModule->mbRenderWorldTransparent, "Render world transparent");
        RegisterVariable(&mpRendererModule->mbRenderCarsOpaque, "Render vehicles opaque");
        RegisterVariable(&mpRendererModule->mbRenderCarsTransparent, "Render vehicles transparent");
        RegisterVariable(&mpRendererModule->mbRenderSky, "Render sky");
        RegisterVariable(&mpRendererModule->mbRenderBlobbyShadows, "Render blobby shadows");
        RegisterVariable(&mpRendererModule->mbRenderParticles, "Render particles");
        RegisterVariable(&mpRendererModule->mbRenderCoronas, "Render coronas");
        RegisterVariable(&mpRendererModule->mbRenderWorldImmediateMode, "Render 3D immediate mode");
        RegisterVariable(&mpRendererModule->mbRenderHudImmediateMode, "Render 2D immediate mode");
        RegisterVariable(&mpRendererModule->mbRenderPostFX, "Render PostFX");

        RegisterVariable(&mpRendererModule->mbShowShadowMap, "ShadowMap", "Show shadow map");
        RegisterVariable(&mpRendererModule->miShowShadowMapIndex, "ShadowMap", "Show shadow map index");
        RegisterVariable(&mpRendererModule->mShadowMapRenderManager.mbForceFrontFaceCull,
                         "ShadowMap", "Force front face cull");
        RegisterVariable(&mpRendererModule->mfBlobbyShadowAlpha, "Blobby Shadow Alpha");
        SetRange(&mpRendererModule->mfBlobbyShadowAlpha, 0.0f, 1.0f);
        SetStep(&mpRendererModule->mfBlobbyShadowAlpha, 0.05f);
        RegisterVariable(&mpRendererModule->mbShowEnvironmentMap, "Show environment map textures");
        RegisterVariable(&mpRendererModule->mbGreyBackgroundColour, "Grey background colour");

        RegisterVariable(&mpRendererModule->mu32NumWorldOpaqueObjects,
                         "DisplayLists", "Num World Opaque Objects per 60 Dispatch Frames");
        RegisterVariable(&mpRendererModule->mu32NumCarOpaqueObjects,
                         "DisplayLists", "Num Car Opaque Objects per 60 Dispatch Frames");
        RegisterVariable(&mpRendererModule->mu32NumWorldTransparentObjects,
                         "DisplayLists", "Num World Transparent Objects per 60 Dispatch Frames");
        RegisterVariable(&mpRendererModule->mu32NumCarTransparentObjects,
                         "DisplayLists", "Num Car Transparent Objects per 60 Dispatch Frames");
        RegisterVariable(&mpRendererModule->mu32NumShadowObjects,
                         "DisplayLists", "Num Shadow Objects per 60 Dispatch Frames");
        RegisterVariable(&mpRendererModule->mbUpdateThreadTakeScreenshot, "Take screenshot");
        RegisterVariable(&mpRendererModule->mbCaptureOverlaysInScreenshot, "Capture overlays in screenshot");

        RegisterVariable(&mpRendererModule->mbRenderPreZ, "OcclusionCull", "PreZ");
        RegisterVariable(&mpRendererModule->mbPreZNearOnly, "OcclusionCull", "PreZ Near Only");
        RegisterVariable(&mpRendererModule->mbRenderPreZAlpha, "OcclusionCull", "PreZ Alpha Tested Geo");
        RegisterVariable(&mpRendererModule->mfPreZDistanceThreshold,
                         "OcclusionCull", "PreZ Distance Thresh");
        SetRange(&mpRendererModule->mfPreZDistanceThreshold, 0.0f, 20000.0f);
        SetStep(&mpRendererModule->mfPreZDistanceThreshold, 50.0f);
        RegisterVariable(&mpRendererModule->mfOccludeeNearClipOffset,
                         "OcclusionCull", "Occludee Near Clip Offset");
        SetRange(&mpRendererModule->mfOccludeeNearClipOffset, 0.0f, 1000.0f);
        SetStep(&mpRendererModule->mfOccludeeNearClipOffset, 10.0f);
        RegisterVariable(&mpRendererModule->mbOcclusionCullWorldOpaque,
                         "OcclusionCull", "Cull world opaque");
        RegisterVariable(&mpRendererModule->mbOcclusionCullWorldTransparent,
                         "OcclusionCull", "Cull world transparent");
        RegisterVariable(&mpRendererModule->mbOcclusionCullCarOpaque,
                         "OcclusionCull", "Cull car opaque");
        RegisterVariable(&mpRendererModule->mbOcclusionCullCarTransparent,
                         "OcclusionCull", "Cull car transparent");
        RegisterVariable(&mpRendererModule->mbOcclusionCullShadowMap,
                         "OcclusionCull", "Cull shadowmap");
        RegisterVariable(&mpRendererModule->muOcclusionCullIndexCountThreshold,
                         "OcclusionCull", "Index count threshold");

        RegisterVariable(&mpRendererModule->mOcclusionCullManager.mbOcclusionEnabled,
                         "OcclusionCull", "Occlusion enabled");
        RegisterVariable(&mpRendererModule->mOcclusionCullManager.mbShowOccludeeBBs,
                         "OcclusionCull", "Show Occludee BBs");
        RegisterVariable(&mpRendererModule->mOcclusionCullManager.mbOcclusionCheapShader,
                         "OcclusionCull", "Cheap shader");
        RegisterVariable(&mpRendererModule->mOcclusionCullManager.mbOccludeesDoubleSided,
                         "OcclusionCull", "Double sided");
        RegisterVariable(&mpRendererModule->mOcclusionCullManager.mbDisplayStats,
                         "OcclusionCull", "Display Stats");
        RegisterVariable(&mpRendererModule->mOcclusionCullManager.mbDetectNearClipIntersections,
                         "OcclusionCull", "Detect near clip intersections");
        RegisterVariable(&mpRendererModule->mOcclusionCullManager.mbImmediateMode,
                         "OcclusionCull", "Immediate mode");
        RegisterVariable(&mpRendererModule->mOcclusionCullManager.mbConditionalRendering,
                         "OcclusionCull", "Conditional rendering");

        RegisterVariable(&mbSunFlareOverrideBrightness, "Override sun corona intensity");
        RegisterVariable(&mfSunFlareOverrideBrightness,
                         "Overridden sun corona intensity");
        SetRange(&mfSunFlareOverrideBrightness, 0.0f, 2.0f);
        SetStep(&mfSunFlareOverrideBrightness, 0.01f);
    }
}
