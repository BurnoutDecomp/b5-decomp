// ============================================================================
// GameSource/Effects/BrnEffectsDebugComponent.cpp
//
// BrnEffects::EffectsDebugComponent - the effects (VFX) module's in-game debug
// menu. Reconstructed from the X360 ARTIST build (semantic parity):
//   Construct                    @ 0x82278C98
//   GetName                      @ 0x82278EB8
//   RenderWorld                  @ 0x82278DB8
//   OnActivate                   @ 0x8228F3E8
//   SetupLionMenu                @ 0x82278EC8
//   SetupBoostMenu               @ 0x82279010
//   SetupJumpMenu                @ 0x82279108
//   OnJunkyardEffectEditChange   @ 0x82287B68
//   OnJunkyardEffectValueChange  @ 0x82287C10
//   OnJunkyardEffectIndexChange  @ 0x8227F120
//
// The debug values live as named members of EffectsDebugComponent (see the header);
// playing "LION" effects are reached through the owning EffectsModule's particle
// module (EffectsModule::ParticleModule().GetLionEffect(handle)).
// ============================================================================

#include "GameSource/Effects/BrnEffectsDebugComponent.h"

#include <cmath>   // atan2f / sinf / cosf (the de-optimised form of the inlined VMX heading math)

#include "GameSource/Effects/EffectsModule.h"
#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "rw/math/vpu/types.h"
#include "rw/rwcore_structs.h"

namespace BrnEffects
{
    // --- File-scope debug-menu group names (X360 BrnEffectsDebugComponent.cpp:30-33). ----
    // The Setup* builders register their variables under these sub-menu paths.
    static const char* const KP_SUBMENU_LION  = "Lion";
    static const char* const KP_SUBMENU_BOOST = "Boost";
    static const char* const KP_SUBMENU_JUMP  = "Jump";
    static const char* const KP_SUBMENU_PROPS = "Props";

    // --- Player-RaceCar corona tuning globals (registered by OnActivate). -----------------
    // These f32s live in the coronas/lighting module (X360 .data @ 0x82F246xx-0x82F247xx);
    // OnActivate exposes them in the debug menu so artists can tune the player car's
    // head/spot/blues/PS3 light coronas. Owned elsewhere (their defining TU is not this one);
    // referenced here by extern so OnActivate can take their addresses.
    extern "C++" {
        // SpotLights
        extern f32 gSpotLightsSmoothStepMin;
        extern f32 gSpotLightsSmoothStepMid;
        extern f32 gSpotLightsSmoothStepMax;
        extern f32 gSpotLightsScaleFactorMin;
        extern f32 gSpotLightsScaleFactorMax;
        extern f32 gSpotLightsSizeX;
        extern f32 gSpotLightsSizeY;
        // HeadLights
        extern f32 gHeadLightsSmoothStepMin;
        extern f32 gHeadLightsSmoothStepMid;
        extern f32 gHeadLightsSmoothStepMax;
        extern f32 gHeadLightsScaleFactorMin;
        extern f32 gHeadLightsScaleFactorMax;
        extern f32 gHeadLightsSizeX;
        extern f32 gHeadLightsSizeY;
        // Blues2s Red
        extern f32 gBlues2sRedSmoothStepMin;
        extern f32 gBlues2sRedSmoothStepMid;
        extern f32 gBlues2sRedSmoothStepMax;
        extern f32 gBlues2sRedScaleFactorMin;
        extern f32 gBlues2sRedScaleFactorMax;
        extern f32 gBlues2sRedSizeX;
        extern f32 gBlues2sRedSizeY;
        // Blues2s Blue
        extern f32 gBlues2sBlueSmoothStepMin;
        extern f32 gBlues2sBlueSmoothStepMid;
        extern f32 gBlues2sBlueSmoothStepMax;
        extern f32 gBlues2sBlueScaleFactorMin;
        extern f32 gBlues2sBlueScaleFactorMax;
        extern f32 gBlues2sBlueSizeX;
        extern f32 gBlues2sBlueSizeY;
        // PS3Lights
        extern f32 gPS3LightsSmoothStepMin;
        extern f32 gPS3LightsSmoothStepMid;
        extern f32 gPS3LightsSmoothStepMax;
        extern f32 gPS3LightsScaleFactorMin;
        extern f32 gPS3LightsScaleFactorMax;
        extern f32 gPS3LightsSizeX;
        extern f32 gPS3LightsSizeY;
    }

    // Number of junkyard-VFX slots the editor cycles through (mirrors
    // EffectsModule::KU_MAX_JUNKYARD_VFX; named locally so the asserts read naturally).
    static const u32 KU_MAX_JUNKYARD_VFX = EffectsModule::KU_MAX_JUNKYARD_VFX;

    // ------------------------------------------------------------------------------------
    // EffectsDebugJumping / EffectsDebugProps trivial constructors (declared in the header;
    // the X360 leaves their fields uninitialised here - they are set up by SetupJumpMenu /
    // SetupPropMenu and edited through the debug UI).
    // ------------------------------------------------------------------------------------
    EffectsDebugJumping::EffectsDebugJumping() {}
    EffectsDebugProps::EffectsDebugProps() {}

    // ====================================================================================
    // Construct - X360 0x82278C98
    // ====================================================================================
    void EffectsDebugComponent::Construct(EffectsModule* lpEffectsModule)
    {
        // Base debug-component construction (the X360 inlines DebugComponent's trivial body,
        // which the linker folded to a shared empty function - semantically the base ctor).
        DebugComponent::Construct();

        CGS_ASSERT(lpEffectsModule != NULL, "lpEffectsModule != NULL");
        mpEffectsModule = lpEffectsModule;

        // Defaults (verified from the Construct store sequence in the asm).
        mfSimulationRate            = 1.0f;
        mfBoostExhaustCutOffSpeed   = 20.0f;

        mbBypassAllVFXProcessing    = false;
        mbSparksEnabled             = true;
        mbTrailsEnabled             = true;
        mbDebrisEnabled             = true;
        mbGlassEnabled              = true;
        mbPopcornEnabled            = true;
        mbSimpleEnabled             = true;
        mbLionEnabled               = true;
        mbLionStatisticsEnabled     = false;
        mbUseZFade                  = false;
        mbEngineCrashEffectsEnabled = false;
        mbSparksDebugEnabled        = false;
        mbQAEnabled                 = false;
        mbEnabled                   = false;
        mbForceStateBlend           = false;
        mbBoostTransition           = false;
        mbForceFinishLineRendering  = false;
        mbDumpRunningLionEffects    = false;
        mbForceNonPlayerEffects     = false;

        mTagPointIndex              = 0;

        mbBloom                     = true;
        mbVignette                  = true;
        mbDepthOfField              = true;
        mbTint                      = true;
        mbTint2d                    = true;
        mbMotionBlur                = true;
        mbMotionBlurEnableUserSettings = false;
        mbMotionBlurUserHighQuality = true;
        mfMotionBlurUserAmountCars  = 0.0f;
        mfMotionBlurUserAmountWorld = 1.0f;

        mbJunkyardVfxEdit           = false;
        mbJunkyardVfxShowAxes       = false;
        mnJunkyardVfxId             = 0;
        mfJunkyardVfxPosX           = 0.0f;
        mfJunkyardVfxPosY           = 0.0f;
        mfJunkyardVfxPosZ           = 0.0f;
        mfJunkyardVfxAngle          = 0.0f;
    }

    // ====================================================================================
    // Destruct - trivial (no asm body of its own beyond the base; DWARF declares it).
    // ====================================================================================
    void EffectsDebugComponent::Destruct()
    {
        DebugComponent::Destruct();
    }

    // ====================================================================================
    // GetName - X360 0x82278EB8
    // ====================================================================================
    const char* EffectsDebugComponent::GetName() const
    {
        return "Effects";
    }

    // ====================================================================================
    // OnActivate - X360 0x8228F3E8
    // Registers the whole effects debug-variable tree with the debug menu.
    // ====================================================================================
    void EffectsDebugComponent::OnActivate()
    {
        // Post-process toggles.
        RegisterVariable(&mbBloom,                        "Enable Bloom");
        RegisterVariable(&mbVignette,                     "Enable Vignette");
        RegisterVariable(&mbDepthOfField,                 "Enable DOF");
        RegisterVariable(&mbTint,                         "Enable Tint");
        RegisterVariable(&mbTint2d,                       "Enable 2d Tint");
        RegisterVariable(&mbMotionBlur,                   "Enable MotionBlur");
        RegisterVariable(&mbMotionBlurEnableUserSettings, "Enable MotionBlur User Settings");

        RegisterVariable(&mfMotionBlurUserAmountCars,     "MotionBlur User: Cars");
        SetRange(&mfMotionBlurUserAmountCars, 0.0f, 1.0f);
        SetStep(&mfMotionBlurUserAmountCars, 0.1f);

        RegisterVariable(&mfMotionBlurUserAmountWorld,    "MotionBlur User: World");
        SetRange(&mfMotionBlurUserAmountWorld, 0.0f, 1.0f);
        SetStep(&mfMotionBlurUserAmountWorld, 0.1f);

        RegisterVariable(&mbMotionBlurUserHighQuality,    "MotionBlur User: High quality");

        // Per-feature render enables.
        RegisterVariable(&mbBypassAllVFXProcessing, "Enable/Disable all VFX Update() processing");
        RegisterVariable(&mbSparksEnabled,          "Enable/Disable spark rendering");
        RegisterVariable(&mbTrailsEnabled,          "Enable/Disable trail rendering");
        RegisterVariable(&mbDebrisEnabled,          "Enable/Disable debris rendering");
        RegisterVariable(&mbGlassEnabled,           "Enable/Disable glass rendering");
        RegisterVariable(&mbPopcornEnabled,         "Enable/Disable popcorn rendering");
        RegisterVariable(&mbSimpleEnabled,          "Enable/Disable simple (particle) rendering");
        RegisterVariable(&mbUseZFade,               "Use ZFade on simple particles?");
        RegisterVariable(&mbEngineCrashEffectsEnabled, "Enable/Disable engine crash rendering");
        RegisterVariable(&mbSparksDebugEnabled,     "Enable/Disable sparks DEBUG rendering");
        RegisterVariable(&mbQAEnabled,              "Enable/Disable QA mode");
        RegisterVariable(&mbForceFinishLineRendering, "Force finish line rendering");
        RegisterVariable(&mbForceNonPlayerEffects,  "Force non player effects");

        // Player-RaceCar corona tuning (SpotLights).
        RegisterVariable(&gSpotLightsSmoothStepMin,  "Coronas\\Player RaceCar", "SpotLightsSmooth Step Min");
        SetStep(&gSpotLightsSmoothStepMin, 100.0f);
        RegisterVariable(&gSpotLightsSmoothStepMid,  "Coronas\\Player RaceCar", "SpotLightsSmooth Step Mid");
        SetStep(&gSpotLightsSmoothStepMid, 100.0f);
        RegisterVariable(&gSpotLightsSmoothStepMax,  "Coronas\\Player RaceCar", "SpotLightsSmooth Step Max");
        SetStep(&gSpotLightsSmoothStepMax, 100.0f);
        RegisterVariable(&gSpotLightsScaleFactorMin, "Coronas\\Player RaceCar", "SpotLightsScale Factor Min");
        SetStep(&gSpotLightsScaleFactorMin, 0.025f);
        RegisterVariable(&gSpotLightsScaleFactorMax, "Coronas\\Player RaceCar", "SpotLightsScale Factor Max");
        SetStep(&gSpotLightsScaleFactorMax, 0.025f);
        RegisterVariable(&gSpotLightsSizeX,          "Coronas\\Player RaceCar", "SpotLightsSize X");
        SetStep(&gSpotLightsSizeX, 0.025f);
        RegisterVariable(&gSpotLightsSizeY,          "Coronas\\Player RaceCar", "SpotLightsSize Y");
        SetStep(&gSpotLightsSizeY, 0.025f);

        // HeadLights.
        RegisterVariable(&gHeadLightsSmoothStepMin,  "Coronas\\Player RaceCar", "HeadLightsSmooth Step Min");
        SetStep(&gHeadLightsSmoothStepMin, 100.0f);
        RegisterVariable(&gHeadLightsSmoothStepMid,  "Coronas\\Player RaceCar", "HeadLightsSmooth Step Mid");
        SetStep(&gHeadLightsSmoothStepMid, 100.0f);
        RegisterVariable(&gHeadLightsSmoothStepMax,  "Coronas\\Player RaceCar", "HeadLightsSmooth Step Max");
        SetStep(&gHeadLightsSmoothStepMax, 100.0f);
        RegisterVariable(&gHeadLightsScaleFactorMin, "Coronas\\Player RaceCar", "HeadLightsScale Factor Min");
        SetStep(&gHeadLightsScaleFactorMin, 0.025f);
        RegisterVariable(&gHeadLightsScaleFactorMax, "Coronas\\Player RaceCar", "HeadLightsScale Factor Max");
        SetStep(&gHeadLightsScaleFactorMax, 0.025f);
        RegisterVariable(&gHeadLightsSizeX,          "Coronas\\Player RaceCar", "HeadLightsSize X");
        SetStep(&gHeadLightsSizeX, 0.025f);
        RegisterVariable(&gHeadLightsSizeY,          "Coronas\\Player RaceCar", "HeadLightsSize Y");
        SetStep(&gHeadLightsSizeY, 0.025f);

        // Blues2s Red.
        RegisterVariable(&gBlues2sRedSmoothStepMin,  "Coronas\\Player RaceCar", "Blues2s RedSmooth Step Min");
        SetStep(&gBlues2sRedSmoothStepMin, 100.0f);
        RegisterVariable(&gBlues2sRedSmoothStepMid,  "Coronas\\Player RaceCar", "Blues2s RedSmooth Step Mid");
        SetStep(&gBlues2sRedSmoothStepMid, 100.0f);
        RegisterVariable(&gBlues2sRedSmoothStepMax,  "Coronas\\Player RaceCar", "Blues2s RedSmooth Step Max");
        SetStep(&gBlues2sRedSmoothStepMax, 100.0f);
        RegisterVariable(&gBlues2sRedScaleFactorMin, "Coronas\\Player RaceCar", "Blues2s RedScale Factor Min");
        SetStep(&gBlues2sRedScaleFactorMin, 0.025f);
        RegisterVariable(&gBlues2sRedScaleFactorMax, "Coronas\\Player RaceCar", "Blues2s RedScale Factor Max");
        SetStep(&gBlues2sRedScaleFactorMax, 0.025f);
        RegisterVariable(&gBlues2sRedSizeX,          "Coronas\\Player RaceCar", "Blues2s RedSize X");
        SetStep(&gBlues2sRedSizeX, 0.025f);
        RegisterVariable(&gBlues2sRedSizeY,          "Coronas\\Player RaceCar", "Blues2s RedSize Y");
        SetStep(&gBlues2sRedSizeY, 0.025f);

        // Blues2s Blue.
        RegisterVariable(&gBlues2sBlueSmoothStepMin,  "Coronas\\Player RaceCar", "Blues2s BlueSmooth Step Min");
        SetStep(&gBlues2sBlueSmoothStepMin, 100.0f);
        RegisterVariable(&gBlues2sBlueSmoothStepMid,  "Coronas\\Player RaceCar", "Blues2s BlueSmooth Step Mid");
        SetStep(&gBlues2sBlueSmoothStepMid, 100.0f);
        RegisterVariable(&gBlues2sBlueSmoothStepMax,  "Coronas\\Player RaceCar", "Blues2s BlueSmooth Step Max");
        SetStep(&gBlues2sBlueSmoothStepMax, 100.0f);
        RegisterVariable(&gBlues2sBlueScaleFactorMin, "Coronas\\Player RaceCar", "Blues2s BlueScale Factor Min");
        SetStep(&gBlues2sBlueScaleFactorMin, 0.025f);
        RegisterVariable(&gBlues2sBlueScaleFactorMax, "Coronas\\Player RaceCar", "Blues2s BlueScale Factor Max");
        SetStep(&gBlues2sBlueScaleFactorMax, 0.025f);
        RegisterVariable(&gBlues2sBlueSizeX,          "Coronas\\Player RaceCar", "Blues2s BlueSize X");
        SetStep(&gBlues2sBlueSizeX, 0.025f);
        RegisterVariable(&gBlues2sBlueSizeY,          "Coronas\\Player RaceCar", "Blues2s BlueSize Y");
        SetStep(&gBlues2sBlueSizeY, 0.025f);

        // PS3Lights.
        RegisterVariable(&gPS3LightsSmoothStepMin,  "Coronas\\Player RaceCar", "PS3LightsSmooth Step Min");
        SetStep(&gPS3LightsSmoothStepMin, 100.0f);
        RegisterVariable(&gPS3LightsSmoothStepMid,  "Coronas\\Player RaceCar", "PS3LightsSmooth Step Mid");
        SetStep(&gPS3LightsSmoothStepMid, 100.0f);
        RegisterVariable(&gPS3LightsSmoothStepMax,  "Coronas\\Player RaceCar", "PS3LightsSmooth Step Max");
        SetStep(&gPS3LightsSmoothStepMax, 100.0f);
        RegisterVariable(&gPS3LightsScaleFactorMin, "Coronas\\Player RaceCar", "PS3LightsScale Factor Min");
        SetStep(&gPS3LightsScaleFactorMin, 0.025f);
        RegisterVariable(&gPS3LightsScaleFactorMax, "Coronas\\Player RaceCar", "PS3LightsScale Factor Max");
        SetStep(&gPS3LightsScaleFactorMax, 0.025f);
        RegisterVariable(&gPS3LightsSizeX,          "Coronas\\Player RaceCar", "PS3LightsSize X");
        SetStep(&gPS3LightsSizeX, 0.025f);
        RegisterVariable(&gPS3LightsSizeY,          "Coronas\\Player RaceCar", "PS3LightsSize Y");
        SetStep(&gPS3LightsSizeY, 0.025f);

        // Feature sub-menus.
        SetupLionMenu();
        SetupBoostMenu();
        SetupJumpMenu();

        // Props sub-menu.
        RegisterVariable(&mPropParams.mbEnable,       KP_SUBMENU_PROPS, "Force Props");
        RegisterVariable(&mPropParams.mMaterialIndex, KP_SUBMENU_PROPS, "Material Index");
        SetRange(&mPropParams.mMaterialIndex, 0u, 14u);

        // Junkyard-VFX editor.
        RegisterVariable(&mbJunkyardVfxEdit, "Junkyard", "Edit junkyard VFX");
        SetChangeCallback(&mbJunkyardVfxEdit, &EffectsDebugComponent::OnJunkyardEffectEditChange, this);

        RegisterVariable(&mnJunkyardVfxId, "Junkyard", "Effect index");
        SetChangeCallback(&mnJunkyardVfxId, &EffectsDebugComponent::OnJunkyardEffectIndexChange, this);

        RegisterVariable(&mfJunkyardVfxPosX, "Junkyard", "Effect position X");
        SetChangeCallback(&mfJunkyardVfxPosX, &EffectsDebugComponent::OnJunkyardEffectValueChange, this);
        SetStep(&mfJunkyardVfxPosX, 0.05f);

        RegisterVariable(&mfJunkyardVfxPosY, "Junkyard", "Effect position Y");
        SetChangeCallback(&mfJunkyardVfxPosY, &EffectsDebugComponent::OnJunkyardEffectValueChange, this);
        SetStep(&mfJunkyardVfxPosY, 0.05f);

        RegisterVariable(&mfJunkyardVfxPosZ, "Junkyard", "Effect position Z");
        SetChangeCallback(&mfJunkyardVfxPosZ, &EffectsDebugComponent::OnJunkyardEffectValueChange, this);
        SetStep(&mfJunkyardVfxPosZ, 0.05f);

        RegisterVariable(&mfJunkyardVfxAngle, "Junkyard", "Effect angle");
        SetChangeCallback(&mfJunkyardVfxAngle, &EffectsDebugComponent::OnJunkyardEffectValueChange, this);
        SetStep(&mfJunkyardVfxAngle, 2.0f);

        RegisterVariable(&mbJunkyardVfxShowAxes, "Junkyard", "Show axes");
    }

    // ====================================================================================
    // SetupLionMenu - X360 0x82278EC8
    // ====================================================================================
    void EffectsDebugComponent::SetupLionMenu()
    {
        RegisterVariable(&mbLionEnabled,            KP_SUBMENU_LION, "Enable/Disable LION rendering");
        RegisterVariable(&mbLionStatisticsEnabled,  KP_SUBMENU_LION, "Enable/Disable LION statistics display");
        RegisterVariable(&mbEnabled,                KP_SUBMENU_LION, "Enable/Disable Effects debugging");
        RegisterVariable(&mTagPointIndex,           KP_SUBMENU_LION, "Tag point index: 0 - 10");

        RegisterVariable(&mfSimulationRate,         KP_SUBMENU_LION, "Lion Simulation Rate:");
        SetRange(&mfSimulationRate, 0.25f, 4.0f);
        SetStep(&mfSimulationRate, 0.25f);

        RegisterVariable(&mbDumpRunningLionEffects, KP_SUBMENU_LION, "Dump running Lion effects");
        RegisterVariable(&mbForceStateBlend,        KP_SUBMENU_LION, "Force State Blend");
        RegisterVariable(&mfForceStateBlendValue,   KP_SUBMENU_LION, "Force State Value: ");
        SetRange(&mfForceStateBlendValue, 0.0f, 1.0f);
        SetStep(&mfForceStateBlendValue, 0.1f);
    }

    // ====================================================================================
    // SetupBoostMenu - X360 0x82279010
    // ====================================================================================
    void EffectsDebugComponent::SetupBoostMenu()
    {
        RegisterVariable(&mbBoostTransition,        KP_SUBMENU_BOOST, "Transition");

        RegisterVariable(&mfTransitionMin,          KP_SUBMENU_BOOST, "Transition Min: ");
        SetRange(&mfTransitionMin, 0.0f, 20.0f);
        SetStep(&mfTransitionMin, 1.0f);

        RegisterVariable(&mfTransitionMax,          KP_SUBMENU_BOOST, "Transition Max: ");
        SetRange(&mfTransitionMax, 0.0f, 20.0f);
        SetStep(&mfTransitionMax, 1.0f);

        RegisterVariable(&mfBoostExhaustCutOffSpeed, KP_SUBMENU_BOOST, "Exhaust cut off speed");
    }

    // ====================================================================================
    // SetupJumpMenu - X360 0x82279108
    // ====================================================================================
    void EffectsDebugComponent::SetupJumpMenu()
    {
        RegisterVariable(&mJumpParams.mbForceJumping,         KP_SUBMENU_JUMP, "Jumping");

        RegisterVariable(&mJumpParams.mfJumpLandingSparkSpeed, KP_SUBMENU_JUMP, "Landing Spark Speed");

        RegisterVariable(&mJumpParams.mfLandingSparksTime,    KP_SUBMENU_JUMP, "Landing Spark Time");
        SetStep(&mJumpParams.mfLandingSparksTime, 0.01f);

        RegisterVariable(&mJumpParams.mfVapourDelayTime,      KP_SUBMENU_JUMP, "Vapour Start Delay");
        SetStep(&mJumpParams.mfVapourDelayTime, 0.01f);

        RegisterVariable(&mJumpParams.mfVapourRampTime,       KP_SUBMENU_JUMP, "Vapour Ramp Time");
        SetStep(&mJumpParams.mfVapourRampTime, 0.01f);
    }

    // ====================================================================================
    // OnJunkyardEffectEditChange - X360 0x82287B68
    // "Edit junkyard VFX" toggled. When turning editing ON, re-sync the playing effects
    // (forward to the index-change handler). When turning OFF, flag every junkyard slot's
    // playing effect as Enabled|Changed so the module restores normal behaviour.
    // ====================================================================================
    void EffectsDebugComponent::OnJunkyardEffectEditChange(void* /*lpValue*/, void* lpUserData)
    {
        EffectsDebugComponent* lpThis = static_cast<EffectsDebugComponent*>(lpUserData);
        EffectsModule* lpModule = lpThis->mpEffectsModule;

        if (lpThis->mbJunkyardVfxEdit)
        {
            OnJunkyardEffectIndexChange(NULL, lpUserData);
            return;
        }

        BrnParticle::ParticleModule& lrParticleModule = lpModule->ParticleModule();
        for (u32 luIndex = 0; luIndex < KU_MAX_JUNKYARD_VFX; ++luIndex)
        {
            u32 luHandle = lpModule->GetJunkyardEffectHandle(luIndex);
            BrnParticle::LionEffect* lpLionEffect = lrParticleModule.GetLionEffect(luHandle);
            if (lpLionEffect)
            {
                lpLionEffect->muFlags |= (BrnParticle::LionEffect::EPPE_FLAG_ENABLED |
                                          BrnParticle::LionEffect::EPPE_FLAG_CHANGED);
            }
        }
    }

    // ====================================================================================
    // OnJunkyardEffectIndexChange - X360 0x8227F120
    // The active junkyard slot changed. Count the live junkyard effects, clamp the editor's
    // index, snapshot the active effect's position/angle into the editor fields, and (when
    // "Edit junkyard VFX" is on) mark the active slot Enabled|Changed and the rest Changed.
    // ====================================================================================
    void EffectsDebugComponent::OnJunkyardEffectIndexChange(void* /*lpValue*/, void* lpUserData)
    {
        EffectsDebugComponent* lpThis = static_cast<EffectsDebugComponent*>(lpUserData);
        EffectsModule* lpModule = lpThis->mpEffectsModule;
        BrnParticle::ParticleModule& lrParticleModule = lpModule->ParticleModule();

        // Count the contiguous run of live junkyard effects from the front of the slot list.
        u32 luLiveCount = 0;
        while (luLiveCount < KU_MAX_JUNKYARD_VFX)
        {
            u32 luHandle = lpModule->GetJunkyardEffectHandle(luLiveCount);
            if (lrParticleModule.GetLionEffect(luHandle) == NULL)
                break;
            ++luLiveCount;
        }

        // Past the live run, every slot must be empty.
        for (u32 luIndex = luLiveCount; luIndex < KU_MAX_JUNKYARD_VFX; ++luIndex)
        {
            u32 luHandle = lpModule->GetJunkyardEffectHandle(luIndex);
            CGS_ASSERT(lrParticleModule.GetLionEffect(luHandle) == NULL, "lpLionEffect == NULL");
        }

        if (luLiveCount == 0)
        {
            // No live junkyard effects: clear the editor.
            lpThis->mbJunkyardVfxEdit = false;
            lpThis->mnJunkyardVfxId   = 0;
            lpThis->mfJunkyardVfxPosX = 0.0f;
            lpThis->mfJunkyardVfxPosY = 0.0f;
            lpThis->mfJunkyardVfxPosZ = 0.0f;
            lpThis->mfJunkyardVfxAngle = 0.0f;
            return;
        }

        // Clamp the edited index into [0, luLiveCount).
        if (lpThis->mnJunkyardVfxId < 0)
            lpThis->mnJunkyardVfxId = static_cast<s32>(luLiveCount) - 1;
        else if (static_cast<u32>(lpThis->mnJunkyardVfxId) >= luLiveCount)
            lpThis->mnJunkyardVfxId = 0;

        // The active effect.
        u32 luActiveHandle = lpModule->GetJunkyardEffectHandle(static_cast<u32>(lpThis->mnJunkyardVfxId));
        BrnParticle::LionEffect* lpActive = lrParticleModule.GetLionEffect(luActiveHandle);
        CGS_ASSERT(lpActive != NULL, "lpActiveLionEffect");

        // Snapshot its translation (the affine transform's position row) into the editor.
        const rw::math::vpu::Matrix44Affine& lrTransform = lpActive->GetTransform();
        lpThis->mfJunkyardVfxPosX = lrTransform.Pos().x;
        lpThis->mfJunkyardVfxPosY = lrTransform.Pos().y;
        lpThis->mfJunkyardVfxPosZ = lrTransform.Pos().z;

        // Recover the effect's heading (degrees) from atan2(forward.x, forward.z), normalised
        // into [0, 360). The forward direction is the transform's first row (Right()/xAxis):
        // the X360 reads LionEffect+0x10 (the matrix's first row) and runs XMVectorATan over
        // its .x/.z lanes. This is the equivalent scalar form.
        f32 lfHeadingRadians = atan2f(lrTransform.Right().x, lrTransform.Right().z);
        f32 lfHeadingDegrees = lfHeadingRadians * 57.295776f; // rad -> deg
        if (lfHeadingDegrees < 0.0f)
            lfHeadingDegrees += 360.0f;
        lpThis->mfJunkyardVfxAngle = lfHeadingDegrees;

        // While editing, mark the active slot Enabled|Changed and the others (Changed, not Enabled).
        if (lpThis->mbJunkyardVfxEdit)
        {
            for (u32 luIndex = 0; luIndex < luLiveCount; ++luIndex)
            {
                u32 luHandle = lpModule->GetJunkyardEffectHandle(luIndex);
                BrnParticle::LionEffect* lpLionEffect = lrParticleModule.GetLionEffect(luHandle);
                bool lbActive = (static_cast<u32>(lpThis->mnJunkyardVfxId) == luIndex);
                CGS_ASSERT(lpLionEffect != NULL, "lpLionEffect");

                if (lbActive)
                {
                    lpLionEffect->muFlags |= (BrnParticle::LionEffect::EPPE_FLAG_ENABLED |
                                              BrnParticle::LionEffect::EPPE_FLAG_CHANGED);
                }
                else
                {
                    lpLionEffect->muFlags = static_cast<u16>(
                        (lpLionEffect->muFlags & ~BrnParticle::LionEffect::EPPE_FLAG_ENABLED) |
                        BrnParticle::LionEffect::EPPE_FLAG_CHANGED);
                }
            }
        }
    }

    // ====================================================================================
    // OnJunkyardEffectValueChange - X360 0x82287C10
    // A junkyard position/angle field changed. When not editing, forward to the index
    // handler. Otherwise wrap the angle into [0, 360), resolve the active effect, build its
    // world transform from the editor's position + heading, and flag it Changed.
    // ====================================================================================
    void EffectsDebugComponent::OnJunkyardEffectValueChange(void* /*lpValue*/, void* lpUserData)
    {
        EffectsDebugComponent* lpThis = static_cast<EffectsDebugComponent*>(lpUserData);
        EffectsModule* lpModule = lpThis->mpEffectsModule;

        if (!lpThis->mbJunkyardVfxEdit)
        {
            OnJunkyardEffectIndexChange(NULL, lpUserData);
            return;
        }

        // Wrap the edited heading into [0, 360).
        if (lpThis->mfJunkyardVfxAngle < 0.0f)
            lpThis->mfJunkyardVfxAngle += 360.0f;
        else if (lpThis->mfJunkyardVfxAngle >= 360.0f)
            lpThis->mfJunkyardVfxAngle -= 360.0f;

        u32 luEffectIndex = static_cast<u32>(lpThis->mnJunkyardVfxId);
        CGS_ASSERT((lpThis->mnJunkyardVfxId >= 0) &&
                   (lpThis->mnJunkyardVfxId < static_cast<s32>(KU_MAX_JUNKYARD_VFX)),
                   "( lnEffectIndex >= 0 ) && ( lnEffectIndex < static_cast<int32_t>( EffectsModule::KU_MAX_JUNKYARD_VFX ) )");

        u32 luHandle = lpModule->GetJunkyardEffectHandle(luEffectIndex);
        BrnParticle::LionEffect* lpLionEffect = lpModule->ParticleModule().GetLionEffect(luHandle);
        if (lpLionEffect)
        {
            // Build the effect's world transform from the editor's position + heading and
            // write it back, then flag the effect Changed. The X360 build inlines the matrix
            // construction with VMX intrinsics; this is the equivalent scalar form: a heading
            // rotation about the up (Y) axis, placed at the edited position. The forward
            // direction goes in the FIRST row (Right()/xAxis) so that the index-change read-back
            // atan2(Right().x, Right().z) recovers the same heading; Up() is the constant
            // {0,1,0} the X360 writes; At() is the perpendicular completing a right-handed basis.
            // The Pos() row's w-lane is the affine/unused lane: the X360 stores 0.0 there
            // (stw r8=0 -> v26[3]=0.0, stvx128 into mTransform+0x30), matching
            // Matrix44Affine::SetIdentity (wAxis.w = 0.0), not a homogeneous 1.0.
            f32 lfHeadingRadians = lpThis->mfJunkyardVfxAngle * 0.017453292f; // deg -> rad
            f32 lfSin = sinf(lfHeadingRadians);
            f32 lfCos = cosf(lfHeadingRadians);

            rw::math::vpu::Matrix44Affine lTransform;
            lTransform.Right() = rw::math::vpu::Vector3 {  lfSin, 0.0f,  lfCos, 0.0f };
            lTransform.Up()    = rw::math::vpu::Vector3 {  0.0f,  1.0f,  0.0f,  0.0f };
            lTransform.At()    = rw::math::vpu::Vector3 {  lfCos, 0.0f, -lfSin, 0.0f };
            lTransform.Pos()   = rw::math::vpu::Vector3 {  lpThis->mfJunkyardVfxPosX,
                                                           lpThis->mfJunkyardVfxPosY,
                                                           lpThis->mfJunkyardVfxPosZ, 0.0f };
            lpLionEffect->mTransform = lTransform;

            lpLionEffect->muFlags |= BrnParticle::LionEffect::EPPE_FLAG_CHANGED;
        }
    }

    // ====================================================================================
    // RenderWorld - X360 0x82278DB8
    // When junkyard editing + show-axes are on, draw the active junkyard effect's axes.
    // ====================================================================================
    void EffectsDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpRender)
    {
        CGS_ASSERT(mpEffectsModule != NULL, "mpEffectsModule != NULL");

        // If the module has left junkyard-edit mode, reset the editor.
        if (!mpEffectsModule->IsJunkyardVfxActive())
        {
            mfJunkyardVfxPosX  = 0.0f;
            mbJunkyardVfxEdit  = false;
            mfJunkyardVfxPosY  = 0.0f;
            mfJunkyardVfxPosZ  = 0.0f;
            mfJunkyardVfxAngle = 0.0f;
        }

        if (mbJunkyardVfxEdit && mbJunkyardVfxShowAxes)
        {
            u32 luEffectIndex = static_cast<u32>(mnJunkyardVfxId);
            CGS_ASSERT((mnJunkyardVfxId >= 0) &&
                       (mnJunkyardVfxId < static_cast<s32>(KU_MAX_JUNKYARD_VFX)),
                       "( lnEffectIndex >= 0 ) && ( lnEffectIndex < static_cast<int32_t>( EffectsModule::KU_MAX_JUNKYARD_VFX ) )");

            u32 luHandle = mpEffectsModule->GetJunkyardEffectHandle(luEffectIndex);
            BrnParticle::LionEffect* lpLionEffect = mpEffectsModule->ParticleModule().GetLionEffect(luHandle);
            if (lpLionEffect)
                lpRender->DrawAxis(lpLionEffect->GetTransform());  // X360 passes only the transform
        }
    }
}
