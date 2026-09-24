// FX-RCEM4 (crash-parity 2026-09-24): CHAIN-STOMPEES part (c) -- RaceCarEntityModule::
// ProcessLeapedAndStompedCars @0x822BD5B8, extracted VERBATIM from BrnRaceCarEntityModule_CrashExit.cpp
// by run_fxrcem4_stompees.py and replayed against fixtures. Pre-fix the function had no body (the
// runner replays it as an empty stub), so miStoredStompeeCount never left 0 and
// ProcessPlayerVehicleInput's AddTargetAssist loop never ran.
//   lbzx +0x1823D (IsInShowtime) ; player car mPhysicsState.mfTimeInAir (+0x4E4) > 0.0f
//   -> count = interface +0x208 -> miStoredStompeeCount ; records {pos +0, EntityId +0x10} x count
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

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
namespace BrnTraffic { namespace BrnTrafficIO {
struct alignas(16) VehicleStompingData { Vector3 mStompeePosition; EntityId mStompeeEntityId; f32 mfDistanceSquared; };
struct TrafficToRaceCarInterface_PreScene {
    VehicleStompingData maStompees[8];
    s32 miCount = 0;
    int miCalls = 0;
    // The DWARF :122 accessor's shape (the real one is FX-TRAFFIC2's, BrnTrafficToRaceCarInterface.h).
    const VehicleStompingData* GetPotentialStompees(s32* lpiNumStompees) const {
        ++const_cast<TrafficToRaceCarInterface_PreScene*>(this)->miCalls;
        *lpiNumStompees = miCount; return maStompees;
    }
};
} }
namespace RaceCarEntityModuleIO {
struct InputBuffer_PostScene {
    BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene mTraffic;
    const BrnTraffic::BrnTrafficIO::TrafficToRaceCarInterface_PreScene* GetTrafficToRaceCarInterface_PreScene() const { return &mTraffic; }
};
struct OutputBuffer_PostScene {};
}
struct RaceCarState { f32 mfTimeInAir = 0.0f; };
struct ActiveRaceCar {
    RaceCarState mPhysicsState;
    RaceCarState* GetPhysicsState() { return &mPhysicsState; }
};
struct CrashPlayManager { bool mbIsInShowtime = false; bool IsInShowtime() const { return mbIsInShowtime; } };
struct StoredStompeeData { Vector3 mPosition; EntityId mEntityId; };

struct RaceCarEntityModule {
    ActiveRaceCar       maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_2;
    CrashPlayManager    mCrashPlayManager;
    StoredStompeeData   mStoredStompees[E_ACTIVE_RACE_CAR_INDEX_COUNT] = {};
    s32                 miStoredStompeeCount = 0;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) { return &maActiveRaceCars[leIndex]; }
    void ProcessLeapedAndStompedCars(const RaceCarEntityModuleIO::InputBuffer_PostScene* lpInput,
                                     RaceCarEntityModuleIO::OutputBuffer_PostScene* lpOutput);
};
#include "fxrcem4_stompees.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    using namespace Fixture;
    RaceCarEntityModuleIO::InputBuffer_PostScene lIn; RaceCarEntityModuleIO::OutputBuffer_PostScene lOut;
    for (int i = 0; i < 8; ++i) {
        lIn.mTraffic.maStompees[i].mStompeePosition = { 10.0f * i, 1.5f, -3.0f * i, 0.0f };
        lIn.mTraffic.maStompees[i].mStompeeEntityId.muValue = 0x40000u + static_cast<u32>(i);
        lIn.mTraffic.maStompees[i].mfDistanceSquared = 99.0f;
    }
    lIn.mTraffic.miCount = 3;

    // Showtime + airborne: the three candidates are stored.
    {
        RaceCarEntityModule lModule;
        lModule.mCrashPlayManager.mbIsInShowtime = true;
        lModule.maActiveRaceCars[2].mPhysicsState.mfTimeInAir = 0.25f;
        lModule.ProcessLeapedAndStompedCars(&lIn, &lOut);
        Check(lModule.miStoredStompeeCount == 3, "Showtime + airborne: miStoredStompeeCount = interface count (lwz 0x208 -> stw +0x185F0)");
        bool lbCopied = true;
        for (int i = 0; i < 3; ++i)
            lbCopied = lbCopied && lModule.mStoredStompees[i].mPosition.x == 10.0f * i
                       && lModule.mStoredStompees[i].mPosition.y == 1.5f && lModule.mStoredStompees[i].mPosition.z == -3.0f * i
                       && lModule.mStoredStompees[i].mEntityId.muValue == 0x40000u + static_cast<u32>(i);
        Check(lbCopied, "Showtime + airborne: {position +0, entity id +0x10} copied per record (stvx +0x184F0 / stw +0x18500, stride 32)");
        Check(lModule.mStoredStompees[3].mEntityId.muValue == 0u, "only count records are written");
    }
    // Not in Showtime: nothing is written (the old list and count stand).
    {
        RaceCarEntityModule lModule; lModule.miStoredStompeeCount = 5;
        lModule.maActiveRaceCars[2].mPhysicsState.mfTimeInAir = 1.0f;
        const int liCallsBefore = lIn.mTraffic.miCalls;
        lModule.ProcessLeapedAndStompedCars(&lIn, &lOut);
        Check(lModule.miStoredStompeeCount == 5 && lIn.mTraffic.miCalls == liCallsBefore,
              "not in Showtime (lbzx +0x1823D == 0): no read, no write (beq @0x822BD5E4)");
    }
    // Showtime but on the ground / NaN: nothing is written.
    {
        RaceCarEntityModule lGround; lGround.mCrashPlayManager.mbIsInShowtime = true; lGround.miStoredStompeeCount = 5;
        lGround.ProcessLeapedAndStompedCars(&lIn, &lOut);
        RaceCarEntityModule lNan; lNan.mCrashPlayManager.mbIsInShowtime = true; lNan.miStoredStompeeCount = 5;
        lNan.maActiveRaceCars[2].mPhysicsState.mfTimeInAir = std::numeric_limits<f32>::quiet_NaN();
        lNan.ProcessLeapedAndStompedCars(&lIn, &lOut);
        RaceCarEntityModule lOther; lOther.mCrashPlayManager.mbIsInShowtime = true; lOther.miStoredStompeeCount = 5;
        lOther.maActiveRaceCars[0].mPhysicsState.mfTimeInAir = 1.0f;   // a rival airborne, not the player
        lOther.ProcessLeapedAndStompedCars(&lIn, &lOut);
        Check(lGround.miStoredStompeeCount == 5 && lNan.miStoredStompeeCount == 5 && lOther.miStoredStompeeCount == 5,
              "time in air 0 / NaN / only a non-player car airborne: nothing written (fcmpu ; bgt @0x822BD60C)");
    }
    // No candidates this frame: the count is still stored (0).
    {
        RaceCarEntityModuleIO::InputBuffer_PostScene lEmpty;
        RaceCarEntityModule lModule; lModule.mCrashPlayManager.mbIsInShowtime = true; lModule.miStoredStompeeCount = 4;
        lModule.maActiveRaceCars[2].mPhysicsState.mfTimeInAir = 0.5f;
        lModule.ProcessLeapedAndStompedCars(&lEmpty, &lOut);
        Check(lModule.miStoredStompeeCount == 0, "Showtime + airborne with no candidates: count 0 is stored (stw before ble)");
    }
    Check(guAssertions == 0, "no assertions");
    std::printf("FxRcem4Stompees: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
