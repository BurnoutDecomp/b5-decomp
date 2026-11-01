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

#include <cmath>
#include "SDKs/XboxMath/XMVectorATan.h"

#include "GameSource/Effects/EffectsModule.h"
#include "GameSource/Effects/Particles/ParticleModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "rw/math/vpu/types.h"
#include "rw/rwcore_structs.h"
#include "GameSource/Graphics/BrnCoronaManager.h"   // BrnCoronaTypeParams::smParams (the corona archetype table OnActivate edits)

namespace BrnEffects
{
    // --- File-scope debug-menu group names (X360 BrnEffectsDebugComponent.cpp:30-33). ----
    // The Setup* builders register their variables under these sub-menu paths.
    static const char* const KP_SUBMENU_LION  = "Lion";
    static const char* const KP_SUBMENU_BOOST = "Boost";
    static const char* const KP_SUBMENU_JUMP  = "Jump";
    static const char* const KP_SUBMENU_PROPS = "Props";

    // --- Player-RaceCar corona tuning fields (registered by OnActivate). ------------------
    // ⚠ CORRECTED 2026-09-02 (tyre-mark wave). This file used to declare THIRTY-FIVE
    // `extern f32 gXxx;` globals for these and said "owned elsewhere (their defining TU is not
    // this one)". No such TU exists, and none ever could: THE CONSOLE ADDRESSES ARE FIELDS OF
    // THE CORONA ARCHETYPE TABLE, which is already reconstructed in BrnCoronaTypeParams.cpp.
    // OnActivate @0x8228F3E8 registers, in order,
    //     unk_82F24610/14        unk_82F24628..38        (HeadLights)
    //     unk_82F24700/04        unk_82F24718..28        (Blues2s Red)
    //     unk_82F24730/34        unk_82F24748..58        (Blues2s Blue)
    //     unk_82F24760/64        unk_82F24778..88        (SpotLights)
    //     unk_82F24790/94        unk_82F247A8..B8        (PS3Lights)
    // and the table is `unk_82F24310`, 25 records of 48 bytes. (addr - 0x82F24310) / 48 gives
    // 16, 21, 22, 23, 24 -- exactly eCoronaTypePlayerCarHeadLight, ...BluesTwosRed,
    // ...BluesTwosBlue, ...Spotlights, ...PS3BluRayLights; the intra-record offsets are +0x00/
    // +0x04 (mvSize.mV.x/y) and +0x18/+0x1C/+0x20/+0x24/+0x28 (mScaleCurve's mCurveParams.xyz and
    // mScaleFactors.xy). So the debug menu edits THE LIVE ARCHETYPE, which is the whole point of
    // it -- 35 separate globals would have edited copies nothing renders from, and would have
    // left the link short by 35 symbols (measured: it did).
    namespace
    {
        // ARTIST82287D38..DCC uses the degree-five folded-cycle approximation,
        // not the SDK Taylor sin/cos. Its BSS vectors are populated by CRT
        // routines82C6EB00/EBC8/F100/F128/F150 from the cited rodata words.
        f32 JunkyardSinCosLane(f32 lfRadians, f32 lfPhase)
        {
            // ARTIST 0x82287D38..0x82287DEC, in instruction order. Replicate the
            // scalar inputs so each VMX intrinsic performs the original four-lane op.
            const rw::math::vpu::VectorIntrinsic lRadians = { lfRadians, lfRadians, lfRadians, lfRadians };
            const rw::math::vpu::VectorIntrinsic lScale = { 0.15915493667125702f, 0.15915493667125702f,
                                                            0.15915493667125702f, 0.15915493667125702f };
            const rw::math::vpu::VectorIntrinsic lPhaseVector = { lfPhase, lfPhase, lfPhase, lfPhase };
            const rw::math::vpu::VectorIntrinsic lCycle = __vmaddfp(lRadians, lScale, lPhaseVector);
            const rw::math::vpu::VectorIntrinsic lFraction = __vsubfp(lCycle, __vrfim(lCycle));
            const rw::math::vpu::VectorIntrinsic lOne = { 1.0f, 1.0f, 1.0f, 1.0f };
            const rw::math::vpu::VectorIntrinsic lMirror = __vsubfp(lOne, lFraction);
            const rw::math::vpu::VectorIntrinsic lQuarter = { -0.25f, -0.25f, -0.25f, -0.25f };
            const rw::math::vpu::VectorIntrinsic lFolded = __vaddfp(__vminfp(lFraction, lMirror), lQuarter);
            const rw::math::vpu::VectorIntrinsic lSquare = __vmulfp(lFolded, lFolded);
            const rw::math::vpu::VectorIntrinsic lPolynomialHigh = { 40.897369384765625f, 40.897369384765625f,
                                                                     40.897369384765625f, 40.897369384765625f };
            const rw::math::vpu::VectorIntrinsic lPolynomialMiddle = { -71.17897033691406f, -71.17897033691406f,
                                                                       -71.17897033691406f, -71.17897033691406f };
            const rw::math::vpu::VectorIntrinsic lPolynomialLow = { -6.278042793273926f, -6.278042793273926f,
                                                                    -6.278042793273926f, -6.278042793273926f };
            const rw::math::vpu::VectorIntrinsic lPolynomial = __vmaddfp(lSquare, lPolynomialMiddle, lPolynomialHigh);
            const rw::math::vpu::VectorIntrinsic lPolynomial2 = __vmaddfp(lSquare, lPolynomial, lPolynomialLow);
            return __vmulfp(lPolynomial2, lFolded).x;
        }

        // ARTIST 0x8227F37C..0x8227F3D0: vrefp, one reciprocal refinement,
        // XMVectorATan, sign-bit OR and the negative/zero Z selects.
        f32 JunkyardHeadingRadians(f32 lfX, f32 lfZ)
        {
            const rw::math::vpu::VectorIntrinsic lX = { lfX, lfX, lfX, lfX };
            const rw::math::vpu::VectorIntrinsic lZ = { lfZ, lfZ, lfZ, lfZ };
            const rw::math::vpu::VectorIntrinsic lZero = { 0.0f, 0.0f, 0.0f, 0.0f };
            const rw::math::vpu::VectorIntrinsic lOne = { 1.0f, 1.0f, 1.0f, 1.0f };
            const rw::math::vpu::VectorIntrinsic lReciprocal = __vrefp(lZ);
            const rw::math::vpu::VectorIntrinsic lError = __vnmsubfp(lReciprocal, lZ, lOne);
            const rw::math::vpu::VectorIntrinsic lRefinedReciprocal = __vmaddfp(lReciprocal, lError, lReciprocal);
            const rw::math::vpu::VectorIntrinsic lRatio = __vmaddfp(lX, lRefinedReciprocal, lZero);
            f32 lfAngle = XboxMath::XMVectorATan(lRatio.x);

            const rw::math::vpu::VectorIntrinsic lAllBits = __vspltisw(-1);
            const rw::math::vpu::VectorIntrinsic lSignMask = __vslw(lAllBits, lAllBits);
            const rw::math::vpu::VectorIntrinsic lSignBits = __vand(lX, lSignMask);
            const rw::math::vpu::VectorIntrinsic lPi = { 3.1415927410125732f, 3.1415927410125732f,
                                                         3.1415927410125732f, 3.1415927410125732f };
            const rw::math::vpu::VectorIntrinsic lHalfPi = { 1.5707963705062866f, 1.5707963705062866f,
                                                             1.5707963705062866f, 1.5707963705062866f };
            const f32 lfSignedPi = __vor(lPi, lSignBits).x;
            const f32 lfSignedHalfPi = __vor(lHalfPi, lSignBits).x;
            if (lfZ < 0.0f)
            {
                const rw::math::vpu::VectorIntrinsic lAngle = { lfAngle, lfAngle, lfAngle, lfAngle };
                const rw::math::vpu::VectorIntrinsic lSignedPiVector = { lfSignedPi, lfSignedPi, lfSignedPi, lfSignedPi };
                lfAngle = __vaddfp(lSignedPiVector, lAngle).x;
            }
            if (lfZ == 0.0f)
                lfAngle = lfSignedHalfPi;
            return lfAngle;
        }
        BrnCoronaTypeParams& CoronaParams(BrnCoronaType leType)
        {
            // GetCoronaTypeParams is the table's own accessor (smParams is private); the debug
            // menu edits the archetype in place, exactly as the console's &unk_82F24xxx does.
            return const_cast<BrnCoronaTypeParams&>(
                BrnCoronaTypeParams::GetCoronaTypeParams(leType));
        }
    }

    // Number of junkyard-VFX slots the editor cycles through (mirrors
    // EffectsModule::KU_MAX_JUNKYARD_VFX; named locally so the asserts read naturally).
    static const u32 KU_MAX_JUNKYARD_VFX = EffectsModule::KU_MAX_JUNKYARD_VFX;

    // ------------------------------------------------------------------------------------
    // EffectsDebugJumping / EffectsDebugProps constructors (declared in the header).
    // ARTIST folds these member defaults into EffectsModule's C++ constructor
    // at827E366C..3684. SetupJumpMenu/SetupPropMenu only register the live fields.
    // ------------------------------------------------------------------------------------
    EffectsDebugJumping::EffectsDebugJumping()
    {
        mbForceJumping = false;
        mfJumpLandingSparkSpeed = 50.0f;
        mfLandingSparksTime = 0.1f;
        mfVapourDelayTime = 0.2f;
        mfVapourRampTime = 0.2f;
    }
    EffectsDebugProps::EffectsDebugProps() : mbEnable(false), mMaterialIndex(0) {}

    // ====================================================================================
    // Construct - X360 0x82278C98
    // ====================================================================================
    void EffectsDebugComponent::Construct(EffectsModule* lpEffectsModule)
    {
        // Base debug-component construction (the X360 inlines DebugComponent's trivial body,
        // which the linker folded to a shared empty function - semantically the base ctor).

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
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mCurveParams.X(),  "Coronas\\Player RaceCar", "SpotLightsSmooth Step Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mCurveParams.X(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mCurveParams.Y(),  "Coronas\\Player RaceCar", "SpotLightsSmooth Step Mid");
        SetStep(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mCurveParams.Y(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mCurveParams.Z(),  "Coronas\\Player RaceCar", "SpotLightsSmooth Step Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mCurveParams.Z(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mScaleFactors.X(), "Coronas\\Player RaceCar", "SpotLightsScale Factor Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mScaleFactors.X(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mScaleFactors.Y(), "Coronas\\Player RaceCar", "SpotLightsScale Factor Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarSpotlights).mScaleCurve.mScaleFactors.Y(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarSpotlights).mvSize.mV.x,          "Coronas\\Player RaceCar", "SpotLightsSize X");
        SetStep(&CoronaParams(eCoronaTypePlayerCarSpotlights).mvSize.mV.x, 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarSpotlights).mvSize.mV.y,          "Coronas\\Player RaceCar", "SpotLightsSize Y");
        SetStep(&CoronaParams(eCoronaTypePlayerCarSpotlights).mvSize.mV.y, 0.025f);

        // HeadLights.
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mCurveParams.X(),  "Coronas\\Player RaceCar", "HeadLightsSmooth Step Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mCurveParams.X(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mCurveParams.Y(),  "Coronas\\Player RaceCar", "HeadLightsSmooth Step Mid");
        SetStep(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mCurveParams.Y(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mCurveParams.Z(),  "Coronas\\Player RaceCar", "HeadLightsSmooth Step Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mCurveParams.Z(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mScaleFactors.X(), "Coronas\\Player RaceCar", "HeadLightsScale Factor Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mScaleFactors.X(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mScaleFactors.Y(), "Coronas\\Player RaceCar", "HeadLightsScale Factor Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarHeadLight).mScaleCurve.mScaleFactors.Y(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarHeadLight).mvSize.mV.x,          "Coronas\\Player RaceCar", "HeadLightsSize X");
        SetStep(&CoronaParams(eCoronaTypePlayerCarHeadLight).mvSize.mV.x, 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarHeadLight).mvSize.mV.y,          "Coronas\\Player RaceCar", "HeadLightsSize Y");
        SetStep(&CoronaParams(eCoronaTypePlayerCarHeadLight).mvSize.mV.y, 0.025f);

        // Blues2s Red.
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mCurveParams.X(),  "Coronas\\Player RaceCar", "Blues2s RedSmooth Step Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mCurveParams.X(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mCurveParams.Y(),  "Coronas\\Player RaceCar", "Blues2s RedSmooth Step Mid");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mCurveParams.Y(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mCurveParams.Z(),  "Coronas\\Player RaceCar", "Blues2s RedSmooth Step Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mCurveParams.Z(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mScaleFactors.X(), "Coronas\\Player RaceCar", "Blues2s RedScale Factor Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mScaleFactors.X(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mScaleFactors.Y(), "Coronas\\Player RaceCar", "Blues2s RedScale Factor Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mScaleCurve.mScaleFactors.Y(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mvSize.mV.x,          "Coronas\\Player RaceCar", "Blues2s RedSize X");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mvSize.mV.x, 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mvSize.mV.y,          "Coronas\\Player RaceCar", "Blues2s RedSize Y");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosRed).mvSize.mV.y, 0.025f);

        // Blues2s Blue.
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mCurveParams.X(),  "Coronas\\Player RaceCar", "Blues2s BlueSmooth Step Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mCurveParams.X(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mCurveParams.Y(),  "Coronas\\Player RaceCar", "Blues2s BlueSmooth Step Mid");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mCurveParams.Y(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mCurveParams.Z(),  "Coronas\\Player RaceCar", "Blues2s BlueSmooth Step Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mCurveParams.Z(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mScaleFactors.X(), "Coronas\\Player RaceCar", "Blues2s BlueScale Factor Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mScaleFactors.X(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mScaleFactors.Y(), "Coronas\\Player RaceCar", "Blues2s BlueScale Factor Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mScaleCurve.mScaleFactors.Y(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mvSize.mV.x,          "Coronas\\Player RaceCar", "Blues2s BlueSize X");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mvSize.mV.x, 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mvSize.mV.y,          "Coronas\\Player RaceCar", "Blues2s BlueSize Y");
        SetStep(&CoronaParams(eCoronaTypePlayerCarBluesTwosBlue).mvSize.mV.y, 0.025f);

        // PS3Lights.
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mCurveParams.X(),  "Coronas\\Player RaceCar", "PS3LightsSmooth Step Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mCurveParams.X(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mCurveParams.Y(),  "Coronas\\Player RaceCar", "PS3LightsSmooth Step Mid");
        SetStep(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mCurveParams.Y(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mCurveParams.Z(),  "Coronas\\Player RaceCar", "PS3LightsSmooth Step Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mCurveParams.Z(), 100.0f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mScaleFactors.X(), "Coronas\\Player RaceCar", "PS3LightsScale Factor Min");
        SetStep(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mScaleFactors.X(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mScaleFactors.Y(), "Coronas\\Player RaceCar", "PS3LightsScale Factor Max");
        SetStep(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mScaleCurve.mScaleFactors.Y(), 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mvSize.mV.x,          "Coronas\\Player RaceCar", "PS3LightsSize X");
        SetStep(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mvSize.mV.x, 0.025f);
        RegisterVariable(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mvSize.mV.y,          "Coronas\\Player RaceCar", "PS3LightsSize Y");
        SetStep(&CoronaParams(eCoronaTypePlayerCarPS3BluRayLights).mvSize.mV.y, 0.025f);

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
        lpThis->mfJunkyardVfxPosX = lrTransform.Pos().mV.x;
        lpThis->mfJunkyardVfxPosY = lrTransform.Pos().mV.y;
        lpThis->mfJunkyardVfxPosZ = lrTransform.Pos().mV.z;

        // Recover the effect's heading from the first row (Right()/xAxis).
        // ARTIST reads LionEffect+0x10, applies the reciprocal/ATan selections
        // above, then converts to degrees and corrects a negative result.
        f32 lfHeadingRadians = JunkyardHeadingRadians(lrTransform.Right().mV.x, lrTransform.Right().mV.z);
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
            // ARTIST forms x=I*sin+K*cos and z=K*sin-I*cos, with each
            // original VMX multiply/add/subtract boundary retained.
            // The Pos() row's w-lane is the affine/unused lane: the X360 stores 0.0 there
            // (stw r8=0 -> v26[3]=0.0, stvx128 into mTransform+0x30), matching
            // Matrix44Affine::SetIdentity (wAxis.w = 0.0), not a homogeneous 1.0.
            f32 lfHeadingRadians = lpThis->mfJunkyardVfxAngle * 0.017453292f; // deg -> rad
            f32 lfSin = JunkyardSinCosLane(lfHeadingRadians, -0.25f);
            f32 lfCos = JunkyardSinCosLane(lfHeadingRadians, 0.0f);

            rw::math::vpu::Matrix44Affine lTransform;
            const Vector3 lI(1.0f, 0.0f, 0.0f);
            const Vector3 lK(0.0f, 0.0f, 1.0f);
            lTransform.Up()    = rw::math::vpu::Vector3(0.0f, 1.0f, 0.0f);
            lTransform.Pos()   = rw::math::vpu::Vector3(lpThis->mfJunkyardVfxPosX,
                                                           lpThis->mfJunkyardVfxPosY,
                                                           lpThis->mfJunkyardVfxPosZ);
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
