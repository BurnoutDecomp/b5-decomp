// FX-CRASHVFX (crash parity 2026-09-25): THE PARTICLE TIME STEP IS A PER-UPDATE-FRAME SUM.
//
// ParticleModule::Update @0x822817D8 ADDS each simulation sub-step's scaled step to mRenderData.mfCurrentTimeStep
// (0x8228185C..0x82281870), and BrnGameModule::OnStartOfUpdateFrame @0x823A8BB0 CLEARS it at the start of every
// update frame -- EffectsModule::StartOfFrame -> ParticleModule::StartOfFrame, both inline, one store
// (`stfsx f0(0.0), r11, r9` with r9 = 0x88194C = the particle module's +0x8E0C), before its tail call to
// BrnRendererModule::StartOfFrame. So every reader of the published record -- the spark ring, the trails, the debris
// jobs, the motion blur -- gets the frame's sum of sub-steps, and 0.0 on a frame with none. The PC dropped the store
// and published the running total since boot.
//
// run_fxcrashvfx_timestep_reset.py compiles the PRODUCTION bodies onto this fixture: BrnGameModule::
// OnStartOfUpdateFrame (a game module holding the effects module and a recording renderer), EffectsModule::
// StartOfFrame and ParticleModule::StartOfFrame (the headers' inline definitions), and ParticleModule::Update with
// its constants. Each update frame is OnStartOfUpdateFrame then k Updates, and the expected values are the CONSOLE'S
// OWN (FxCrashVfxTimeStepResetData.h, gen_tstep_data.py: 0x823A8BB0 and 0x822817D8 interpreted on emu64 frame by
// frame). Six checks per case, each over all of the case's frames: the clear, the renderer's StartOfFrame after it,
// and the frame's step, time, multiplier and Lion clock. A console NaN is matched by any NaN (the x86 default NaN
// carries the other sign).
#include "types.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "FxCrashVfxTimeStepResetData.h"

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPassed, const char* lpcLabel)
{
    ++gChecks;
    if (!lbPassed)
        ++gFailures;
    std::printf("%s  %s\n", lbPassed ? "pass" : "FAIL", lpcLabel);
}

static u32 Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
static f32 Float(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }

static bool SameF32(u32 luConsole, f32 lfHost)
{
    const f32 lfConsole = Float(luConsole);
    if (lfConsole != lfConsole)
        return lfHost != lfHost;
    return luConsole == Bits(lfHost);
}

// ---- the types the production bodies name ----------------------------------------------------------------
namespace CgsGraphics { struct Camera { u32 muUnused; }; }
struct FixtureMatrix44Affine { f32 maf[16]; };

namespace BrnDirector
{
namespace Camera
{
    struct FixtureCameraState
    {
        u64  muFlags;                                   // camera +0x140, the state's current 64-bit flag set
        bool IsFlagSet(u32 luFlag) const { return ((muFlags >> luFlag) & 1u) != 0u; }
    };
    struct FixtureCameraEffects
    {
        f32 mfSimTimeScale;                             // camera +0x104
        f32 GetSimTimeScale() const { return mfSimTimeScale; }
    };
    class Camera
    {
    public:
        FixtureMatrix44Affine mTransform;
        FixtureCameraState    mState;
        FixtureCameraEffects  mEffects;
        void CopyToCgsCamera(CgsGraphics::Camera*) const {}
        const FixtureMatrix44Affine& GetTransform() const { return mTransform; }
        const FixtureCameraState&    GetState() const { return mState; }
        const FixtureCameraEffects&  GetEffects() const { return mEffects; }
    };
}
}

struct cTime;

namespace BrnParticle
{
    class ParticleModule
    {
    public:
        struct ParticleRenderData
        {
            static const u16 eRenderDataFlagCameraSwitched   = 1;
            static const u16 eRenderDataFlagRenderSparks     = 2;
            static const u16 eRenderDataFlagRenderDebris     = 4;
            static const u16 eRenderDataFlagRenderSimple     = 8;
            static const u16 eRenderDataFlagRenderLion       = 16;
            static const u16 eRenderDataFlagRenderTrails     = 32;
            static const u16 eRenderDataFlagReducedFrameRate = 64;
            static const u16 eRenderDataFlagInSlowMotion     = 128;

            f32                   mfCurrentTime;
            f32                   mfCurrentTimeStep;
            f32                   mfTimeStepMultiplier;
            FixtureMatrix44Affine mCameraTransform;
            CgsGraphics::Camera   mCgsCamera;
            u16                   muFlags;
        };

        f32                mfSimulationRate;
        cTime*             mpLionCurrentTime;
        bool               mbSparksEnabled, mbTrailsEnabled, mbDebrisEnabled, mbSimpleEnabled, mbLionEnabled;
        bool               mbHasCameraSwitched;
        ParticleRenderData mRenderData;

#include "fxcrashvfx_tstep_particle_sof.inc"

        void Update(f32 lfTimeStep, f32 lfTime, f32 lfTimeStepMultiplier, const BrnDirector::Camera::Camera* lpCamera);
    };

#include "fxcrashvfx_tstep_update.inc"
}

namespace BrnEffects
{
    class EffectsModule
    {
    public:
        BrnParticle::ParticleModule mParticleModule;

#include "fxcrashvfx_tstep_effects_sof.inc"
    };
}

// The renderer's StartOfFrame: counts its calls and records the step it finds when it runs (the console's clear
// comes first, then the tail call).
struct FixtureRenderModule
{
    const BrnParticle::ParticleModule* mpParticleModule;
    u32  muCalls;
    u32  muStepSeen;
    void StartOfFrame()
    {
        ++muCalls;
        muStepSeen = Bits(mpParticleModule->mRenderData.mfCurrentTimeStep);
    }
};

namespace BrnGame
{
    class BrnGameModule
    {
    public:
        BrnEffects::EffectsModule mEffectsModule;
        FixtureRenderModule       mRenderModule;
        void OnStartOfUpdateFrame();
    };

#include "fxcrashvfx_tstep_body.inc"
}

static s32 gLionClock = 0;

int main()
{
    static BrnGame::BrnGameModule sGame;
    BrnParticle::ParticleModule& lrModule = sGame.mEffectsModule.mParticleModule;
    sGame.mRenderModule.mpParticleModule = &lrModule;

    const u32 luNumCases = static_cast<u32>(sizeof(kaTStepCases) / sizeof(kaTStepCases[0]));
    for (u32 luCase = 0; luCase < luNumCases; ++luCase)
    {
        const TStepCase& lrCase = kaTStepCases[luCase];

        std::memset(&lrModule, 0, sizeof(lrModule));
        lrModule.mfSimulationRate            = Float(lrCase.muRate);
        lrModule.mpLionCurrentTime           = reinterpret_cast<cTime*>(&gLionClock);
        lrModule.mbSparksEnabled             = lrCase.mau8Enables[0] != 0;
        lrModule.mbTrailsEnabled             = lrCase.mau8Enables[1] != 0;
        lrModule.mbDebrisEnabled             = lrCase.mau8Enables[2] != 0;
        lrModule.mbSimpleEnabled             = lrCase.mau8Enables[3] != 0;
        lrModule.mbLionEnabled               = lrCase.mau8Enables[4] != 0;
        lrModule.mbHasCameraSwitched         = lrCase.mu8Switched != 0;
        lrModule.mRenderData.mfCurrentTimeStep = Float(lrCase.muSeedStep);
        gLionClock = 0;

        BrnDirector::Camera::Camera lCamera;
        for (u32 lu = 0; lu < 16u; ++lu)
            lCamera.mTransform.maf[lu] = Float(lrCase.mauTransform[lu]);
        lCamera.mState.muFlags          = lrCase.muCameraFlags;
        lCamera.mEffects.mfSimTimeScale = Float(lrCase.muTimeScale);

        bool lbReset = true, lbOrder = true, lbStep = true, lbTime = true, lbMult = true, lbLion = true;
        s32 liFirstBad = -1;
        u32 luSub = 0;
        char lacDetail[160] = "";
        for (u32 luFrame = 0; luFrame < lrCase.muNumFrames; ++luFrame)
        {
            const TStepFrameOut& lrOut = lrCase.mpOut[luFrame];
            const u32 luCallsBefore = sGame.mRenderModule.muCalls;

            sGame.OnStartOfUpdateFrame();

            const u32 luAfterReset = Bits(lrModule.mRenderData.mfCurrentTimeStep);
            const bool lbFrameReset = (luAfterReset == lrOut.muReset);
            const bool lbFrameOrder = (sGame.mRenderModule.muCalls == luCallsBefore + 1u)
                                   && (sGame.mRenderModule.muStepSeen == lrOut.muReset);

            for (u32 luStep = 0; luStep < lrCase.mpNumSubSteps[luFrame]; ++luStep, ++luSub)
            {
                const TStepSubStep& lrSub = lrCase.mpSubSteps[luSub];
                lrModule.Update(Float(lrSub.muStep), Float(lrSub.muTime), Float(lrSub.muMult), &lCamera);
            }

            const bool lbFrameStep = SameF32(lrOut.muStep, lrModule.mRenderData.mfCurrentTimeStep);
            const bool lbFrameTime = SameF32(lrOut.muTime, lrModule.mRenderData.mfCurrentTime);
            const bool lbFrameMult = SameF32(lrOut.muMult, lrModule.mRenderData.mfTimeStepMultiplier);
            const bool lbFrameLion = (static_cast<u32>(gLionClock) == lrOut.muLion);
            if (!(lbFrameReset && lbFrameOrder && lbFrameStep) && liFirstBad < 0)
            {
                liFirstBad = static_cast<s32>(luFrame);
                std::snprintf(lacDetail, sizeof(lacDetail),
                              " -- first bad frame %u: after the clear %08X (console %08X), published %08X (console %08X)",
                              luFrame, luAfterReset, lrOut.muReset, Bits(lrModule.mRenderData.mfCurrentTimeStep),
                              lrOut.muStep);
            }
            lbReset = lbReset && lbFrameReset;
            lbOrder = lbOrder && lbFrameOrder;
            lbStep  = lbStep && lbFrameStep;
            lbTime  = lbTime && lbFrameTime;
            lbMult  = lbMult && lbFrameMult;
            lbLion  = lbLion && lbFrameLion;
        }

        char lacLabel[400];
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "[%s] OnStartOfUpdateFrame clears the particle step to 0.0 every update frame (%u frames)%s",
                      lrCase.mpcName, lrCase.muNumFrames, lbReset ? "" : lacDetail);
        Check(lbReset, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "[%s] ...and only then calls the renderer's StartOfFrame, once a frame", lrCase.mpcName);
        Check(lbOrder, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "[%s] the published step is the frame's sum of scaled sub-steps, as on the console%s",
                      lrCase.mpcName, lbStep ? "" : lacDetail);
        Check(lbStep, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] mfCurrentTime is the console's", lrCase.mpcName);
        Check(lbTime, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] mfTimeStepMultiplier is the console's", lrCase.mpcName);
        Check(lbMult, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel), "[%s] the Lion clock word is the console's", lrCase.mpcName);
        Check(lbLion, lacLabel);
    }

    std::printf("FxCrashVfxTimeStepReset: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
