// FX-GS2 (crash parity 2026-09-23, G11-D5): the PRODUCTION GameStateToGuiInterface::Construct,
// AppendRaceCarCrashes and SetPlayerRaceCarIndex (extracted from
// src/GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.cpp by run_fxgs2_crash_append.py)
// against the real interface header and the real BrnPhysics::Vehicle::RaceCarCrashEvent.
//
// Checked against the ARTIST asm:
//   Construct @0x82379908      ninth leg `addi r3, r31, 0x1E0 ; bl RaceCarCrashEvent_8_::Construct`
//                              (0x82379964): the crash queue gets its inline storage, length 0, max 8
//   AppendRaceCarCrashes @0x82379980
//                              assert "lpRaceCarCrashEventQueue" (line 247), then
//                              `addi r3, r30, 0x1E0 ; bl RaceCarCrashEvent_::Append` (0x823799CC):
//                              the source records are appended in order behind what is there
//   SetPlayerRaceCarIndex      inlined at 0x823A5594 as `stw r28, 0(r11)`: interface +0 = index
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"   // the real interface
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

// The production bodies under test (or the runner's labelled empty stand-ins).
#include "crash_append_methods.inc"

using BrnGameState::GameStateModuleIO::GameStateToGuiInterface;
using BrnPhysics::Vehicle::RaceCarCrashEvent;
typedef CgsModule::EventQueue<RaceCarCrashEvent, 8> CrashQueue;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("  FAIL %s\n", lpcName);
    }
    else
    {
        std::printf("  ok   %s\n", lpcName);
    }
}

static RaceCarCrashEvent MakeCrash(f32 lfSpeed, s32 liTakedownType, bool lbPrimary, bool lbAI, f32 lfNormalX)
{
    RaceCarCrashEvent lEvent;
    std::memset(&lEvent, 0, sizeof(lEvent));
    lEvent.mfSpeedMPH            = lfSpeed;
    lEvent.meInstantTakedownType = static_cast<BrnGameState::ETakedownType>(liTakedownType);
    lEvent.mbIsPrimaryCrash      = lbPrimary;
    lEvent.mbCarIsAI             = lbAI;
    lEvent.mCollisionNormal.x    = lfNormalX;
    return lEvent;
}

static bool Same(const RaceCarCrashEvent& a, const RaceCarCrashEvent& b)
{
    return std::memcmp(&a, &b, sizeof(RaceCarCrashEvent)) == 0;
}

static GameStateToGuiInterface gInterface;   // static storage: zero before Construct, like `new T()`
static CrashQueue              gSource;

int main()
{
    // (1) Construct builds the ninth queue: inline storage, empty, capacity 8
    gInterface.Construct();
    Check(gInterface.mRaceCarCrashEventQueue.mpEvents != nullptr, "Construct: the crash queue points at its inline storage");
    Check(gInterface.mRaceCarCrashEventQueue.GetLength() == 0 && gInterface.mRaceCarCrashEventQueue.GetMaxLength() == 8,
          "Construct: the crash queue is empty with capacity 8");
    Check(gInterface.miPlayerRaceCarIndex == -1, "Construct: the player index is the invalid slot (-1)");

    // (2) AppendRaceCarCrashes copies the frame's records in order
    gSource.Construct();
    const RaceCarCrashEvent kaCrash[5] = {
        MakeCrash(61.5f, 3, true, false, 0.25f),
        MakeCrash(12.0f, 0, false, true, -1.0f),
        MakeCrash(88.25f, 7, true, true, 0.5f),
        MakeCrash(33.0f, 1, false, false, 0.75f),
        MakeCrash(140.0f, 2, true, false, -0.5f),
    };
    for (s32 i = 0; i < 3; ++i)
    {
        gSource.AddEvent(kaCrash[i]);
    }
    gInterface.AppendRaceCarCrashes(&gSource);
    Check(gInterface.mRaceCarCrashEventQueue.GetLength() == 3, "Append: three source records -> three queued");
    Check(gInterface.mRaceCarCrashEventQueue.GetLength() == 3
              && Same(gInterface.mRaceCarCrashEventQueue.GetEvent(0), kaCrash[0])
              && Same(gInterface.mRaceCarCrashEventQueue.GetEvent(1), kaCrash[1])
              && Same(gInterface.mRaceCarCrashEventQueue.GetEvent(2), kaCrash[2]),
          "Append: records copied whole, in source order");
    Check(gSource.GetLength() == 3, "Append: the source queue is left as it was");

    // (3) a second append lands behind the first
    gSource.Construct();
    gSource.AddEvent(kaCrash[3]);
    gSource.AddEvent(kaCrash[4]);
    gInterface.AppendRaceCarCrashes(&gSource);
    Check(gInterface.mRaceCarCrashEventQueue.GetLength() == 5
              && Same(gInterface.mRaceCarCrashEventQueue.GetEvent(3), kaCrash[3])
              && Same(gInterface.mRaceCarCrashEventQueue.GetEvent(4), kaCrash[4])
              && Same(gInterface.mRaceCarCrashEventQueue.GetEvent(0), kaCrash[0]),
          "Append again: the new records follow the old ones");

    // (4) the append touches only the crash queue
    Check(gInterface.mOnTailEventQueue.GetLength() == 0 && gInterface.mFinishedRaceEventQueue.GetLength() == 0
              && gInterface.mNewDirtyTrickQueue.GetLength() == 0,
          "Append: the notification queues are untouched");

    // (5) the inlined player-index store
    gInterface.SetPlayerRaceCarIndex(5);
    Check(gInterface.miPlayerRaceCarIndex == 5, "SetPlayerRaceCarIndex: interface +0 = the player's slot");

    // (6) the per-sub-step retire (Construct) empties the crash queue again
    gInterface.Construct();
    Check(gInterface.mRaceCarCrashEventQueue.GetLength() == 0 && gInterface.miPlayerRaceCarIndex == -1,
          "Construct again: crash queue empty, player index -1");

    // (7) an empty source appends nothing
    gSource.Construct();
    gInterface.AppendRaceCarCrashes(&gSource);
    Check(gInterface.mRaceCarCrashEventQueue.GetLength() == 0, "Append of an empty queue: nothing queued");

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxGs2CrashAppend: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
