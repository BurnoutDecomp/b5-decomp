// FX-DIRECTOR2 (crash parity 2026-09-25): THE ORDER OF THE TIMER LEGS IN ONE UPDATE STEP.
//
// The console's DoUpdate @0x823F0AF8 runs, per step:
//   S  BridgeTimers @0x823BD150 (called at 0x823F0DE4, FIRST): reset the request accumulator and
//      snapshot both timers (TimerStatusInterface::StoreTimers @0x828D7518) into gm+0x9A0B0C;
//   G  the game-state pre-world leg (DoUpdate_GameStatePreWorld, 0x823F10CC) -- a game-state
//      time-scale request (e.g. DriveThruManager::SetPlayerCarDriver's 0.52631581f) is posted here;
//   W  the world (DoUpdate_World 0x823F14B4), which integrates with that snapshot (0x823E8C78..
//      0x823E8C84 hand gm+0x9A0B0C to the world input);
//   D  the director (DoUpdate_Director 0x823F1640): it reads the same snapshot and publishes its
//      camera's time scale (MainDirector::Update, SetTimestepMultiplier @0x82275148);
//   U  UpdateTimers @0x823BCFD0 (called at 0x823F1CFC, LAST): fold the game-state requests and --
//      unless the game state asked for a multiplier (0x823BD02C) -- the director's, ApplyToTimers
//      @0x828D7468, Timer::Update @0x828D7320 (the scale target becomes current at 0x828D7340).
// So a director request made in step N is integrated by the world in step N+1.
//
// The runner (run_fxdirector2_timer_order.py) reads the PRODUCTION BrnGameModule::GameMain sub-step
// loop and DoUpdate_Director of the revision under test and writes the order of those legs into
// fxdirector2_timer_order.inc (a snapshot taken inside DoUpdate_Director counts as an S just
// before that D). This fixture then runs the scenario through the PRODUCTION timer primitives
// (CgsTimer.cpp, CgsTimerRequestInterface.cpp, CgsTimerStatusInterface.cpp, compiled beside it)
// in that order, one leg at a time. UpdateTimers itself is a member of the (unbuildable-here) game
// module; its body is transcribed from the console words above, as BrnGameModule.cpp has it.
//
// The scenario is the h225 hard stop: the director publishes 1.0 every step until the step the
// HardStop moment goes VALID (k), then the moment's 0.0075 (the pinned BRN_ULTRA_SLOMO_SCALE; the
// console draws RandomFloat(0.005, 0.01)) for 400 steps, then 1.0 again. On the console's order the
// world integrates step k at full dt and step k+1 at 0.0075. The PC's old order (UpdateTimers
// mid-step, before the director; the snapshot inside DoUpdate_Director) puts the world one step late.

#include <cmath>
#include <cstdio>
#include <cstring>
#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTimer.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h"
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"

static int giAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcText, const char*, int) { ++giAsserts; std::printf("ASSERT %s\n", lpcText); return 0; }
    void* EndAssert() { return nullptr; }
}
}

// static const char kacOrder[] = "...";  -- the leg order of the revision under test.
#include "fxdirector2_timer_order.inc"

static int giChecks = 0;
static int giFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass)
    {
        ++giFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

namespace
{
    const f32 KF_RATE              = 1.0f / 60.0f;   // the timers' base step (the world's 0.016667)
    const f32 KF_HARDSTOP_SCALE    = 0.0075f;        // the pinned ultra slo-mo scale
    const f32 KF_DRIVE_THRU_SCALE  = 0.52631581f;    // DriveThruManager::SetPlayerCarDriver's request
    const s32 KI_HOLD_STEPS        = 400;            // MomentHardStop's 400 updates
    const s32 KI_VALID_STEP        = 5;              // k: the step the moment goes VALID
    const s32 KI_GAME_STATE_STEP   = 440;            // g: a game-state request, well after the hold
    const s32 KI_STEPS             = 460;

    struct Leg
    {
        CgsSystem::Timer                 mGameTimer;
        CgsSystem::Timer                 mSimTimer;
        CgsSystem::TimerRequestInterface mAccumulator;          // BrnGameModule::mTimerRequestInterface
        CgsSystem::TimerStatusInterface  mStatus;               // BrnGameModule::mTimerStatusInterface
        CgsSystem::TimerRequestInterface mDirectorRequests;     // DirectorIO::OutputBuffer's
        CgsSystem::TimerRequestInterface mGameStateRequests;    // GameStateModuleIO::OutputBuffer's

        f32 mafWorldDt[KI_STEPS];
        f32 mafDirectorDt[KI_STEPS];

        void Construct()
        {
            mGameTimer.Prepare(KF_RATE);
            mSimTimer.Prepare(KF_RATE);
            mGameTimer.SetRunning(true);
            mSimTimer.SetRunning(true);
            mAccumulator.Clear();
            mDirectorRequests.Clear();
            mGameStateRequests.Clear();
            mStatus.StoreTimers(&mGameTimer, &mSimTimer);
            std::memset(mafWorldDt, 0, sizeof(mafWorldDt));
            std::memset(mafDirectorDt, 0, sizeof(mafDirectorDt));
        }

        static f32 DirectorScale(s32 liStep)
        {
            return (liStep >= KI_VALID_STEP && liStep < KI_VALID_STEP + KI_HOLD_STEPS) ? KF_HARDSTOP_SCALE : 1.0f;
        }

        // UpdateTimers @0x823BCFD0, as BrnGameModule.cpp transcribes it.
        void UpdateTimers()
        {
            mAccumulator.GetGameTimerRequests()->Append(*mGameStateRequests.GetGameTimerRequests());
            mAccumulator.GetSimTimerRequests()->Append(*mGameStateRequests.GetSimTimerRequests());
            if (!mGameStateRequests.GetSimTimerRequests()->IsMultiplierRequested())
            {
                mAccumulator.GetGameTimerRequests()->Append(*mDirectorRequests.GetGameTimerRequests());
                mAccumulator.GetSimTimerRequests()->Append(*mDirectorRequests.GetSimTimerRequests());
            }
            mAccumulator.ApplyToTimers(&mGameTimer, &mSimTimer);
            mGameTimer.Update();
            mSimTimer.Update();
        }

        void Step(s32 liStep)
        {
            for (const char* lpc = kacOrder; *lpc != '\0'; ++lpc)
            {
                switch (*lpc)
                {
                case 'S':   // BridgeTimers: the reset half and the snapshot
                    mAccumulator.Clear();
                    mStatus.StoreTimers(&mGameTimer, &mSimTimer);
                    break;
                case 'G':   // the game-state pre-world leg
                    if (liStep == KI_GAME_STATE_STEP)
                        mGameStateRequests.GetSimTimerRequests()->SetTimestepMultiplier(KF_DRIVE_THRU_SCALE);
                    break;
                case 'W':   // the world integrates with the snapshot
                    mafWorldDt[liStep] = mStatus.GetSimTimerStatus()->GetCurrentTimeStep();
                    break;
                case 'D':   // the director reads the snapshot, then publishes (its buffer starts cleared)
                    mafDirectorDt[liStep] = mStatus.GetSimTimerStatus()->GetCurrentTimeStep();
                    mDirectorRequests.Clear();
                    mDirectorRequests.GetSimTimerRequests()->SetTimestepMultiplier(DirectorScale(liStep));
                    break;
                case 'U':
                    UpdateTimers();
                    break;
                default:
                    break;
                }
            }
            mGameStateRequests.Clear();   // the step's game-action retire (after every leg)
        }
    };

    Leg gLeg;
}

int main()
{
    std::printf("leg order under test: %s\n", kacOrder);
    gLeg.Construct();
    for (s32 liStep = 0; liStep < KI_STEPS; ++liStep)
        gLeg.Step(liStep);

    const f32 lfScaled = KF_RATE * KF_HARDSTOP_SCALE;
    const s32 k = KI_VALID_STEP;
    s32 liFirstScaled = -1;
    s32 liScaledSteps = 0;
    for (s32 liStep = 0; liStep < KI_GAME_STATE_STEP; ++liStep)
    {
        if (gLeg.mafWorldDt[liStep] == lfScaled)
        {
            ++liScaledSteps;
            if (liFirstScaled < 0)
                liFirstScaled = liStep;
        }
    }
    std::printf("world dt: step k-1 %.6f  k %.6f  k+1 %.6f  k+2 %.6f | first scaled step %d (k = %d) | scaled steps %d\n",
                gLeg.mafWorldDt[k - 1], gLeg.mafWorldDt[k], gLeg.mafWorldDt[k + 1], gLeg.mafWorldDt[k + 2],
                liFirstScaled, k, liScaledSteps);
    std::printf("world dt after the hold: k+400 %.6f  k+401 %.6f  k+402 %.6f\n",
                gLeg.mafWorldDt[k + 400], gLeg.mafWorldDt[k + 401], gLeg.mafWorldDt[k + 402]);

    Check(gLeg.mafWorldDt[k] == KF_RATE,
          "the step the moment goes VALID integrates at full dt (the director publishes after the world)");
    Check(gLeg.mafWorldDt[k + 1] == lfScaled,
          "the NEXT step integrates at the hard stop's 0.0075 -- the console's impact frame (N+2) runs in slo-mo");
    Check(liFirstScaled == k + 1, "the first slowed world step is k+1 (one step after the request), not k+2");
    Check(liScaledSteps == KI_HOLD_STEPS, "the world integrates exactly 400 steps at the hold's scale");
    Check(gLeg.mafWorldDt[k + KI_HOLD_STEPS] == lfScaled && gLeg.mafWorldDt[k + KI_HOLD_STEPS + 1] == KF_RATE,
          "real time returns one step after the director publishes 1.0 again (k+401), not two");
    Check(gLeg.mafDirectorDt[k] == KF_RATE && gLeg.mafDirectorDt[k + 1] == lfScaled,
          "the director's own view: full dt on the VALID step, the hold's scale the step after (both orders agree)");
    Check(gLeg.mafWorldDt[KI_GAME_STATE_STEP] == KF_RATE
              && gLeg.mafWorldDt[KI_GAME_STATE_STEP + 1] == KF_RATE * KF_DRIVE_THRU_SCALE,
          "a game-state request (the drive-thru's 0.52631581) reaches the world the step after it is posted");
    Check(gLeg.mafWorldDt[KI_GAME_STATE_STEP + 2] == KF_RATE,
          "the game-state request is one-shot: real time the step after, with the director's 1.0 folded back in");
    Check(giAsserts == 0, "no TimerRequests assert over the run (accumulate-and-reset stays one cycle)");
    Check(gLeg.mSimTimer.GetScaleCurrent() == 1.0f, "the sim timer ends back at scale 1.0");

    std::printf("fxdirector2_timer_order: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
