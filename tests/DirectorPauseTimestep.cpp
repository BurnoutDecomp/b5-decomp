// Issue #58: execute the production camera-clock staging with real timer snapshots.
#include "GameShared/GameClasses/System/Timer/CgsTimer.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"
#include "rw/math/fpu/scalar_operation.h"
#include <cmath>
#include <cstdio>

static int giChecks, giFailures, giAsserts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++giAsserts; return 0; }
void* EndAssert() { return 0; }
} }

namespace ICE { struct CameraSpaceHandler {}; }
namespace BrnDirector {
namespace DirectorIO {
struct InputBuffer {
    CgsSystem::TimerStatusInterface mTimers;
    bool mbSimPaused = false;
    const CgsSystem::TimerStatusInterface* GetTimerStatusInterface() const { return &mTimers; }
    bool IsSimPaused() const { return mbSimPaused; }
};
}
struct DirectorInputOutput { const DirectorIO::InputBuffer* mpInputBuffer; };
namespace Camera { struct BehaviourSharedInfo { Timestep mTimestep; }; }
struct MainDirector {
    enum { E_FLAG_TAIL_DEBUG_ZERO_TIMESTEP };
    bool maStateFlagTail[1] = {};
#include "director_pause_decl.inc"
    void PreClock(const DirectorInputOutput*, s32, Camera::BehaviourSharedInfo&, ICE::CameraSpaceHandler&);
    void PostClock(const DirectorInputOutput*, s32, Camera::BehaviourSharedInfo&, ICE::CameraSpaceHandler&);
};
}

struct ZoomWitness {
    f32 mfLastIdealFOV = 45.0f;
    f32 mfSmoothedIdealFOVVelocity = 0.0f;
    void Update(f32 lfTimeStep) {
        const f32 lfIdealFOV = 60.0f;
        struct { f32 mfIdealFOVVelocityLerpAmount = 0.5f; } lrParams;
#include "director_pause_zoom.inc"
    }
};
#include "director_pause_clock.inc"

static void Check(bool lbPass, const char* lpcText) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL %s\n", lpcText); }
}

static void Run(bool lbSnapshotRunning, bool lbPaused, bool lbZero, f32 lfScale) {
    CgsSystem::Timer lGame, lSim;
    lGame.Prepare(1.0f / 60.0f); lSim.Prepare(1.0f / 60.0f);
    lGame.SetRunning(true); lSim.SetRunning(true);
    lSim.SetScaleTarget(lfScale); lGame.Update(); lSim.Update();
    lSim.SetRunning(lbSnapshotRunning);
    BrnDirector::DirectorIO::InputBuffer lInput;
    lInput.mTimers.StoreTimers(&lGame, &lSim);
    // Pause actions change the live timer after BridgeTimers captured the step.
    // The current director pause byte and the stored running byte can disagree.
    lInput.mbSimPaused = lbPaused;
    lSim.SetRunning(!lbPaused);
    BrnDirector::DirectorInputOutput lIO = { &lInput };
    BrnDirector::MainDirector lDirector;
    lDirector.maStateFlagTail[0] = lbZero;
    ICE::CameraSpaceHandler lSpaces;
    BrnDirector::Camera::BehaviourSharedInfo lPre, lPost;
    lDirector.PreClock(&lIO, 0, lPre, lSpaces);
    lDirector.PostClock(&lIO, 0, lPost, lSpaces);
    const f32 lfBase = lSim.GetRate();
    const f32 lfStep = lfBase * lSim.GetScaleCurrent();
    Check(lSim.GetScaleCurrent() == lfScale, "timer snapshot retains the configured wreck slow-motion scale");
    const bool lbAdvance[2] = { !lbPaused && !lbZero, lbSnapshotRunning && !lbZero };
    const BrnDirector::Timestep* lapSteps[2] = { &lPre.mTimestep, &lPost.mTimestep };
    for (int liPhase = 0; liPhase < 2; ++liPhase) {
        const BrnDirector::Timestep& lrSteps = *lapSteps[liPhase];
        const f32 lfExpectedWorld = lbAdvance[liPhase] ? lfStep : 0.0f;
        const f32 lfExpectedNoSlomo = lbAdvance[liPhase] ? lfBase : 0.0f;
        Check(lrSteps.Get(BrnDirector::Timestep::E_WORLD) == lfExpectedWorld,
              liPhase == 0 ? "pre-scene uses the current pause byte" : "post-scene uses the captured running byte");
        Check(lrSteps.Get(BrnDirector::Timestep::E_WORLD_NO_SLOMO) == lfExpectedNoSlomo,
              "no-slomo clock follows its phase gate");
        Check(lrSteps.Get(BrnDirector::Timestep::E_GAME) == (lbZero ? 0.0f : lGame.GetRate()),
              "game clock remains independent of world pause");
        for (int liLane = 0; liLane < 4; ++liLane) {
            Check(lrSteps.GetVecFloat(BrnDirector::Timestep::E_WORLD).maLanes[liLane] == lfExpectedWorld,
                  "all world SIMD lanes match the scalar clock");
            Check(lrSteps.GetVecFloat(BrnDirector::Timestep::E_WORLD_NO_SLOMO).maLanes[liLane] == lfExpectedNoSlomo,
                  "all no-slomo SIMD lanes match the scalar clock");
        }
    }
    Check(lInput.mTimers.GetSimTimerStatus()->IsRunning() == lbSnapshotRunning,
          "clock selection does not mutate the captured timer");
    if (!lbPaused && !lbZero) {
        const int liBefore = giAsserts;
        static ZoomWitness slZoom;
        slZoom.Update(lPre.mTimestep.Get(BrnDirector::Timestep::E_WORLD));
        Check(giAsserts == liBefore, "resuming a wreck reaches the production zoom predicate with a nonzero step");
        Check(std::isfinite(slZoom.mfSmoothedIdealFOVVelocity), "resumed zoom velocity stays finite");
    }
}

int main() {
    const f32 lafScales[] = { 1.0f, 0.25f, 0.000125f };
    for (f32 lfScale : lafScales)
        for (int liRunning = 0; liRunning < 2; ++liRunning)
            for (int liPaused = 0; liPaused < 2; ++liPaused)
                for (int liZero = 0; liZero < 2; ++liZero)
                    Run(liRunning != 0, liPaused != 0, liZero != 0, lfScale);
    // Repeated rapid menu exits during a wreck must not accumulate bad zoom state.
    for (int liTransition = 0; liTransition < 100; ++liTransition) {
        Run(true, true, false, 0.000125f);
        Run(false, false, false, 0.000125f);
    }
    std::printf("DirectorPauseTimestep: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
