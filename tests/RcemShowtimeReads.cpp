// FX-RCEM (crash-parity 2026-09-22): the two showtime reads of RaceCarEntityModule that the
// console takes off module +0x1823D == mCrashPlayManager (+0x180F0) . mbIsInShowtime (+0x14D),
// extracted VERBATIM from BrnRaceCarEntityModule_ResetPump.cpp by run_rcem_showtime_reads.py:
//   WriteUpdatedAIData @0x822D1FC8, 0x822D2130..0x822D2154:
//       showtime = (i == mePlayerActiveRaceCarIndex) && lbzx(this + 0x1823D)
//   CheckForResetOnTrackConditions @0x822CE9E0, 0x822CEDF0..0x822CEE40:
//       if (mfTimeInAir > 10.0 (flt_820149B0, fcmpu/ble) && !mbReset (this+0x183DF))
//           verdict = !lbzx(this + 0x1823D) && dword_82FB7518 == 0      (an ASSIGNMENT)
// The fixture carries the retired phantom `mbIsInShowtimeMode` as the pre-fix build had it --
// declared, never written -- while the showtime writer (HandlePrepareForModeAction's
// KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR arm) sets the MANAGER's byte, exactly as on PC.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include <cstdio>

namespace Fixture {
struct CrashPlayManager {
    bool mbIsInShowtime = false;                       // console +0x14D
    bool IsInShowtime() const { return mbIsInShowtime; }
};
struct PlayerVehicleControls { bool mbReset = false; }; // console module +0x183DF
struct RaceCarState { f32 mfTimeInAir = 0.0f; };        // console phys +0x404
#include "rcem_showtime_constants.inc"
struct RaceCarEntityModule {
    CrashPlayManager      mCrashPlayManager;
    bool                  mbIsInShowtimeMode = false;   // the pre-fix phantom: never written
    PlayerVehicleControls mPlayerVehicleControls;
    EActiveRaceCarIndex   mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_2;
    bool PublishedShowtime(EActiveRaceCarIndex leCar) const;
    bool InAirVerdict(const RaceCarState* lpState, bool lbNeedsReset) const;
};
#include "rcem_showtime_reads.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    Fixture::RaceCarEntityModule lModule;
    Fixture::RaceCarState lState;

    // ---- WriteUpdatedAIData's showtime bool (UpdateActiveRaceCarData r10) ----------------------
    lModule.mCrashPlayManager.mbIsInShowtime = true;     // HandlePrepareForModeAction @0x823099C8
    Check(lModule.PublishedShowtime(E_ACTIVE_RACE_CAR_INDEX_2),
          "WriteUpdatedAIData: player slot publishes the manager's showtime byte (1)");
    Check(!lModule.PublishedShowtime(E_ACTIVE_RACE_CAR_INDEX_3),
          "WriteUpdatedAIData: a non-player slot publishes 0 (cmpw r22, player ; bne)");
    lModule.mCrashPlayManager.mbIsInShowtime = false;    // HandleStopModeAction @0x82307BE4
    Check(!lModule.PublishedShowtime(E_ACTIVE_RACE_CAR_INDEX_2),
          "WriteUpdatedAIData: player slot publishes 0 once showtime is cleared");

    // ---- CheckForResetOnTrackConditions' in-air arm (5) -----------------------------------------
    lState.mfTimeInAir = 10.5f;
    lModule.mCrashPlayManager.mbIsInShowtime = true;
    Check(!lModule.InAirVerdict(&lState, true),
          "in-air arm in showtime: verdict ASSIGNED false, earlier reason cancelled (bne @0x822CEE28 -> r28 = 0)");
    Check(!lModule.InAirVerdict(&lState, false),
          "in-air arm in showtime: no reset requested");
    lModule.mCrashPlayManager.mbIsInShowtime = false;
    Check(lModule.InAirVerdict(&lState, false),
          "in-air arm outside showtime: > 10 s requests a reset (r24 = 1)");
    lState.mfTimeInAir = 10.0f;                           // fcmpu f13, f0 ; ble -> arm skipped
    Check(lModule.InAirVerdict(&lState, true) && !lModule.InAirVerdict(&lState, false),
          "in-air arm: exactly 10.0 s does not enter the arm (earlier verdict kept)");
    lState.mfTimeInAir = 10.5f;
    lModule.mPlayerVehicleControls.mbReset = true;        // lbzx this+0x183DF ; bne -> skip
    lModule.mCrashPlayManager.mbIsInShowtime = true;
    Check(lModule.InAirVerdict(&lState, true),
          "in-air arm: a pending player reset (+0x183DF) skips the arm (earlier verdict kept)");

    std::printf("RcemShowtimeReads: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
