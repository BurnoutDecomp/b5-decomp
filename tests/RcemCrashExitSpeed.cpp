// FX-RCEM (crash-parity 2026-09-22): RaceCarEntityModule::ProcessRaceCarCrashCompleteEvents
// (ARTIST 0x822F3FE0), extracted VERBATIM from BrnRaceCarEntityModule_CrashExit.cpp with its
// TU-local constants by run_rcem_crash_exit_speed.py, run against fixture cars and a fixture
// crash-complete queue. The expectations are the console's instructions:
//   0x822F43C8  !mbCrashing (+0x52A)                     -> ResetAfterCrash(false)
//   0x822F4428  network car && !mbRemoveRaceCar          -> ResetAfterCrash(false)
//   0x822F4440  speed = f29 (flt_82001CC0, 0.0); if meEngineState (+0x768) == 2:
//                 lbzx this+0x18345 (mbIsInOnlineGameMode) ? flt_82FAD8C0 : flt_82FAD720
//               flt_82FAD720 = 0.44704f * 50.0f (CRT 0x82C4BB30), flt_82FAD8C0 = 0.44704f * 75.0f
//               (CRT 0x82C4BB50); type 1, distance 0.0 -- or type 3 / -50.0 (flt_820148B4) for an
//               AI car when game-mode flag 0x80000000 is set
//   0x822F452C  RaceCar::RequestResetOnTrack(f1 = speed, r5 = type, f2 = distance)
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/World/AI/BrnAISharedConstants.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <vector>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; std::fprintf(stderr, "ASSERT: %s\n", lpcMessage); return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace Fixture {
using namespace BrnWorld;
// The persistent-damage arm (G67-D1, b6efda77) names its flag through GameModeParams; a Fixture-local
// stand-in is found before ::BrnGameState, which the output-interface header only forward-declares.
namespace BrnGameState { struct GameModeParams { static const u64 KU_FLAG_AI_PERSISTENT_DAMAGE = 0x40000000ull; }; }
struct VolumeIdFixture {
    u32 muEntityIndex = 0;
    u32 GetEntityIDEntityIndex() const { return muEntityIndex; }
};
namespace CrashIO {
struct RaceCarCrashCompleteEvent {
    VolumeIdFixture mRaceCarVolumeInstanceId;
    bool            mbRemoveRaceCar = false;
};
struct RaceCarOutputInterface {
    struct RaceCarCrashCompleteEventQueue {
        std::vector<RaceCarCrashCompleteEvent> maEvents;
        s32 GetLength() const { return static_cast<s32>(maEvents.size()); }
        const RaceCarCrashCompleteEvent& GetEvent(s32 liIndex) const { return maEvents[liIndex]; }
    };
    RaceCarCrashCompleteEventQueue mQueue;
    const RaceCarCrashCompleteEventQueue* GetRaceCarCrashCompleteEventQueue() const { return &mQueue; }
};
}
namespace RaceCarEntityModuleIO {
using namespace BrnWorld::RaceCarEntityModuleIO;   // the real EActiveRaceCarEngineState
struct InputBuffer_PostScene {
    CrashIO::RaceCarOutputInterface mCrash;
    const CrashIO::RaceCarOutputInterface* GetCrashInterface() const { return &mCrash; }
};
}
struct RaceCar {
    ERaceCarType      meType = E_RACE_CAR_TYPE_AI;
    int               miResets = 0;
    f32               mfResetSpeed = -1.0f, mfResetDistance = -1.0f;
    BrnAI::EResetType meResetType = BrnAI::E_RESET_TYPE_INVALID;
    f32               mfPersistentDamage = 0.0f;
    ERaceCarType GetType() const { return meType; }
    f32 GetPersistentDamage() const { return mfPersistentDamage; }
    // Same arithmetic as RaceCar::IncreasePersistentDamage (its own test: run_persistent_damage.py).
    bool IncreasePersistentDamage() {
        mfPersistentDamage += 0.3f;
        if (mfPersistentDamage < 1.0f) return false;
        mfPersistentDamage = 0.0f;
        return true;
    }
    void RequestResetOnTrack(f32 lfSpeed, BrnAI::EResetType leType, f32 lfDistance) {
        ++miResets; mfResetSpeed = lfSpeed; meResetType = leType; mfResetDistance = lfDistance;
    }
};
struct ActiveRaceCar {
    RaceCar* mpRaceCar = nullptr;
    bool     mbCrashing = true, mbTakenDown = false;
    int      miResetAfterCrash = 0;
    RaceCarEntityModuleIO::EActiveRaceCarEngineState meEngineState =
        RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
    bool IsAttached() const { return mpRaceCar != nullptr; }
    bool IsCrashing() const { return mbCrashing; }
    bool IsTakenDown() const { return mbTakenDown; }
    void SetTakenDown(bool lbTakenDown) { mbTakenDown = lbTakenDown; }
    void ResetAfterCrash(bool) { ++miResetAfterCrash; }
    RaceCar* GetGlobalRaceCar() const { return mpRaceCar; }
    RaceCarEntityModuleIO::EActiveRaceCarEngineState GetEngineState() const { return meEngineState; }
};
struct RaceCarEntityModule {
    RaceCar       maRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    u64           mxGameModeFlags = 0;
    bool          mbIsInOnlineGameMode = false;
    RaceCarEntityModule() {
        for (int liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
            maActiveRaceCars[liSlot].mpRaceCar = &maRaceCars[liSlot];
        maRaceCars[0].meType = E_RACE_CAR_TYPE_PLAYER;
    }
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) { return &maActiveRaceCars[leIndex]; }
    bool GetGameModeFlag(u64 lxFlag) const { return (mxGameModeFlags & lxFlag) != 0; }
    s32 GetPersistentDamageCarCount() const {
        s32 liCount = 0;
        for (const RaceCar& lrCar : maRaceCars)
            if (lrCar.meType != E_RACE_CAR_TYPE_INACTIVE && lrCar.mfPersistentDamage > 0.0f) ++liCount;
        return liCount;
    }
    void ProcessRaceCarCrashCompleteEvents(const RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput);
};
#include "rcem_crash_exit_speed.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static void Complete(Fixture::RaceCarEntityModule& lrModule, u32 luSlot, bool lbRemove = false) {
    Fixture::RaceCarEntityModuleIO::InputBuffer_PostScene lInput;
    Fixture::CrashIO::RaceCarCrashCompleteEvent lEvent;
    lEvent.mRaceCarVolumeInstanceId.muEntityIndex = luSlot;
    lEvent.mbRemoveRaceCar = lbRemove;
    lInput.mCrash.mQueue.maEvents.push_back(lEvent);
    lrModule.ProcessRaceCarCrashCompleteEvents(&lInput);
}

int main() {
    using namespace BrnWorld::RaceCarEntityModuleIO;
    const f32 kfOffline = 0.44704f * 50.0f;   // flt_82FAD720 (CRT 0x82C4BB30)
    const f32 kfOnline  = 0.44704f * 75.0f;   // flt_82FAD8C0 (CRT 0x82C4BB50)

    {   // the player, engine running, offline free roam
        Fixture::RaceCarEntityModule lModule;
        Complete(lModule, 0);
        const Fixture::RaceCar& lrCar = lModule.maRaceCars[0];
        Check(lrCar.miResets == 1, "running player wreck -> one RequestResetOnTrack");
        Check(lrCar.mfResetSpeed == kfOffline, "engine running, offline: speed flt_82FAD720 = 22.352 m/s (50 mph)");
        Check(lrCar.meResetType == BrnAI::E_RESET_TYPE_STANDARD && lrCar.mfResetDistance == 0.0f,
              "type 1, distance 0.0 without the 0x80000000 mode flag");
    }
    {   // engine running, online
        Fixture::RaceCarEntityModule lModule; lModule.mbIsInOnlineGameMode = true;
        Complete(lModule, 0);
        Check(lModule.maRaceCars[0].mfResetSpeed == kfOnline,
              "engine running, online (+0x18345): speed flt_82FAD8C0 = 33.528 m/s (75 mph)");
    }
    {   // an AI rival in Road Rage (flag 0x80000000), engine running
        Fixture::RaceCarEntityModule lModule; lModule.mxGameModeFlags = 0x80000000ull;
        Complete(lModule, 3);
        const Fixture::RaceCar& lrCar = lModule.maRaceCars[3];
        Check(lrCar.mfResetSpeed == kfOffline && lrCar.meResetType == BrnAI::E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE &&
              lrCar.mfResetDistance == -50.0f, "Road Rage AI wreck: 50 mph, type 3, -50 m (flt_820148B4)");
    }
    {   // engine not running -> the f29 zero
        for (EActiveRaceCarEngineState leState : { E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF,
                                                   E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING,
                                                   E_ACTIVE_RACE_CAR_ENGINE_STATE_STOPPING }) {
            Fixture::RaceCarEntityModule lModule; lModule.mbIsInOnlineGameMode = true;
            lModule.maActiveRaceCars[2].meEngineState = leState;
            Complete(lModule, 2);
            Check(lModule.maRaceCars[2].miResets == 1 && lModule.maRaceCars[2].mfResetSpeed == 0.0f,
                  "engine state != 2 -> speed 0.0 (f29 = flt_82001CC0)");
        }
    }
    {   // not crashing any more -> ResetAfterCrash, no request
        Fixture::RaceCarEntityModule lModule; lModule.maActiveRaceCars[1].mbCrashing = false;
        Complete(lModule, 1);
        Check(lModule.maActiveRaceCars[1].miResetAfterCrash == 1 && lModule.maRaceCars[1].miResets == 0,
              "!mbCrashing -> ResetAfterCrash only (0x822F43C8)");
    }
    {   // a network car that is not being removed -> ResetAfterCrash, no request
        Fixture::RaceCarEntityModule lModule; lModule.maRaceCars[4].meType = BrnWorld::E_RACE_CAR_TYPE_NETWORK;
        Complete(lModule, 4, false);
        Check(lModule.maActiveRaceCars[4].miResetAfterCrash == 1 && lModule.maRaceCars[4].miResets == 0,
              "network car, !mbRemoveRaceCar -> ResetAfterCrash only (0x822F4428)");
    }
    {   // a taken-down Road Rage rival (0x40000000 | 0x80000000): +0.3 persistent damage, then the reset
        Fixture::RaceCarEntityModule lModule; lModule.mxGameModeFlags = 0xC0000000ull;
        lModule.maActiveRaceCars[3].mbTakenDown = true;
        Complete(lModule, 3);
        Check(lModule.maRaceCars[3].mfPersistentDamage == 0.3f && !lModule.maActiveRaceCars[3].mbTakenDown &&
              lModule.maRaceCars[3].miResets == 1,
              "taken-down AI in a persistent-damage mode carries 0.3 and is still reset on track (G67-D1)");
    }
    Check(guAssertions == 0, "no assertions");
    std::printf("RcemCrashExitSpeed: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
