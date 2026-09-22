// FX-RCEM (crash-parity 2026-09-22): the Showtime arm of
// RaceCarEntityModule::HandlePrepareForModeAction (ARTIST 0x823092F0), extracted VERBATIM from
// BrnRaceCarEntityModule_ModeArming.cpp by run_rcem_showtime_wreck_latch.py.
//   0x82309980  ld 0x860(params) ; rlwinm 0,22,22     -- KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR
//   0x823099A4  strategy vtable +0x34 (slot 13)      -- OnStartCrashPlay
//   0x823099C8  stbx r18(=1), this, 0x1823D          -- mCrashPlayManager.mbIsInShowtime
//   0x823099D0  stb  r18(=1), 0x782(player car)      -- ActiveRaceCar::mbIsWrecked (NOT +0x788)
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"
#include <cstdio>
#include <string>
#include <vector>

namespace Fixture {
static std::vector<std::string> gaWitness;
void WreckLatchWitness(const char* lpcSite, s32, bool lbNewValue) {
    gaWitness.push_back(std::string(lpcSite) + (lbNewValue ? "=1" : "=0"));
}
struct GameModeParamsFixture {
    u64 mxFlags = 0;
    bool GetFlag(u64 lxFlag) const { return (mxFlags & lxFlag) != 0; }
};
struct BoostStrategy {
    int miCrashPlayStarts = 0;
    void OnStartCrashPlay() { ++miCrashPlayStarts; }
};
struct BoostManager {
    BoostStrategy mStrategy;
    BoostStrategy* GetBoostStrategy() { return &mStrategy; }
};
struct CrashPlayManager { bool mbIsInShowtime = false; };
struct ActiveRaceCar {
    bool mbIsWrecked = false;      // console +0x782
    bool mbIsInShowtime = false;   // console +0x788
    void SetInShowtime(bool lbInShowtime) { mbIsInShowtime = lbInShowtime; }
};
struct RaceCarEntityModule {
    BoostManager        mBoostManager;
    CrashPlayManager    mCrashPlayManager;
    ActiveRaceCar       maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_1;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) { return &maActiveRaceCars[leIndex]; }
    void ShowtimeArm(const GameModeParamsFixture* lpGameModeParams);
};
#include "rcem_showtime_wreck_latch.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    {
        Fixture::RaceCarEntityModule lModule;
        Fixture::GameModeParamsFixture lParams;
        lParams.mxFlags = BrnGameState::GameModeParams::KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR;
        lModule.ShowtimeArm(&lParams);
        const Fixture::ActiveRaceCar& lrPlayer = lModule.maActiveRaceCars[1];
        Check(lModule.mBoostManager.mStrategy.miCrashPlayStarts == 1, "showtime: OnStartCrashPlay once (vtable +0x34)");
        Check(lModule.mCrashPlayManager.mbIsInShowtime, "showtime: module+0x1823D (CrashPlayManager::mbIsInShowtime) = 1");
        Check(lrPlayer.mbIsWrecked, "showtime: player car +0x782 (mbIsWrecked) latched = 1 (stb r18, 0x782 @0x823099D0)");
        Check(!lrPlayer.mbIsInShowtime, "showtime: player car +0x788 (mbIsInShowtime) untouched here (its setter is action 143)");
        bool lbOthersClean = true;
        for (int liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
            if (liSlot != 1 && (lModule.maActiveRaceCars[liSlot].mbIsWrecked || lModule.maActiveRaceCars[liSlot].mbIsInShowtime))
                lbOthersClean = false;
        Check(lbOthersClean, "showtime: only the player's slot is written");
    }
    {
        Fixture::RaceCarEntityModule lModule;
        Fixture::GameModeParamsFixture lParams;   // flag clear
        lModule.ShowtimeArm(&lParams);
        Check(!lModule.mCrashPlayManager.mbIsInShowtime && !lModule.maActiveRaceCars[1].mbIsWrecked &&
              lModule.mBoostManager.mStrategy.miCrashPlayStarts == 0, "no showtime flag: the arm does nothing");
    }
    std::printf("RcemShowtimeWreckLatch: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
