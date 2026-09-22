// FX-RCEM (crash-parity 2026-09-22): the takedown flow's four world-side consumers in
// RaceCarEntityModule::HandleGameActions (ARTIST 0x8230BE08), extracted VERBATIM from
// BrnRaceCarEntityModule.cpp by run_rcem_takedown_actions.py and dispatched against fixture
// race cars. Every expectation below is an ARTIST instruction:
//   case 3   0x8230C720..0x8230C744  player global car ->RequestResetOnTrack(f1 = record+0,
//                                    r5 = 1, f2 = f30 = flt_82001CC0 == 0.0)
//   case 111 0x8230D230..0x8230D244  player slot +0x724 (mfInvulnerablityTime) = record+0
//   case 120 0x8230D09C..0x8230D0E8  slot(record+0x10): IsAIDriven ? mbDamaged (+0x1BE4) = 1
//                                    -- no five-damaged-car budget in this arm
//   case 121 0x8230D26C..0x8230D2D4  slot(record+0): IsAttached ? { RemoveRaceCar(its GLOBAL
//                                    index, lpOutput) ; player global car ->RequestResetOnTrack(
//                                    flt_82FAD720 == 0.44704f * 50.0f, 1, 0.0) }
// A missing arm is replayed as the console's `default: break;` (the pre-fix source), so the
// old body reports per-check failures rather than failing to build.
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/World/AI/BrnAISharedConstants.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
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

static std::vector<std::string> gaCalls;   // ordered side effects, for the 121 ordering check

struct RaceCar {
    ERaceCarType        meType   = E_RACE_CAR_TYPE_INACTIVE;
    EGlobalRaceCarIndex meGlobal = E_GLOBAL_RACE_CAR_INDEX_0;
    int                 miResets = 0;
    f32                 mfResetSpeed = -1.0f, mfResetDistance = -1.0f;
    BrnAI::EResetType   meResetType = BrnAI::E_RESET_TYPE_INVALID;
    bool IsAIDriven() const { return meType == E_RACE_CAR_TYPE_AI; }
    EGlobalRaceCarIndex GetGlobalRaceCarIndex() const { return meGlobal; }
    void RequestResetOnTrack(f32 lfSpeed, BrnAI::EResetType leType, f32 lfDistance) {
        ++miResets; mfResetSpeed = lfSpeed; meResetType = leType; mfResetDistance = lfDistance;
        gaCalls.push_back("reset:" + std::to_string(static_cast<int>(meGlobal)));
    }
};
struct RenderParams {
    bool mbDamaged = false;
    bool IsDamaged() const { return mbDamaged; }
    void SetDamaged(bool lbDamaged) { mbDamaged = lbDamaged; }
};
struct ActiveRaceCar {
    RaceCar*     mpRaceCar = nullptr;
    RenderParams mRenderParams;
    f32          mfInvulnerablityTime = -1.0f;   // Attach seeds -1.0f
    bool IsAttached() const { return mpRaceCar != nullptr; }
    RaceCar* GetGlobalRaceCar() const { CGS_ASSERT(IsAttached(), "IsAttached()"); return mpRaceCar; }
    RenderParams* GetRenderParams() { return &mRenderParams; }
    void SetInvulnerabilityTime(f32 lfTime) { mfInvulnerablityTime = lfTime; }
};
struct OutputFixture { int miTag = 0x5EED; };
struct RaceCarEntityModule {
    RaceCar             maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];
    ActiveRaceCar       maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    std::vector<EGlobalRaceCarIndex> maRemoved;
    std::vector<const OutputFixture*> maRemovedOutputs;
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex leIndex) {
        CGS_ASSERT(leIndex >= 0 && leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "active index");
        return &maActiveRaceCars[leIndex];
    }
    // Observed double of the real RemoveRaceCar: records the call and detaches the slot the
    // way DetachActiveRaceCar does, so a later read of the victim sees it gone.
    void RemoveRaceCar(EGlobalRaceCarIndex leGlobal, OutputFixture* lpOutput) {
        maRemoved.push_back(leGlobal); maRemovedOutputs.push_back(lpOutput);
        gaCalls.push_back("remove:" + std::to_string(static_cast<int>(leGlobal)));
        for (ActiveRaceCar& lrCar : maActiveRaceCars)
            if (lrCar.mpRaceCar == &maRaceCars[leGlobal]) lrCar.mpRaceCar = nullptr;
        maRaceCars[leGlobal].meType = E_RACE_CAR_TYPE_INACTIVE;
    }
    void Dispatch(s32 liType, const CgsModule::Event* lpEvent, OutputFixture* lpOutput);
};
#include "rcem_takedown_actions.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

// Player in slot 2 (global 5); AI rivals in slots 0,1,3..7 on globals 10+slot; slot 6 is a
// network car. Every slot attached.
static void Build(Fixture::RaceCarEntityModule& lrModule) {
    using namespace BrnWorld;
    Fixture::gaCalls.clear();
    for (int liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot) {
        const int liGlobal = (liSlot == 2) ? 5 : 10 + liSlot;
        Fixture::RaceCar& lrCar = lrModule.maRaceCars[liGlobal];
        lrCar.meGlobal = static_cast<EGlobalRaceCarIndex>(liGlobal);
        lrCar.meType = (liSlot == 2) ? E_RACE_CAR_TYPE_PLAYER
                     : (liSlot == 6) ? E_RACE_CAR_TYPE_NETWORK : E_RACE_CAR_TYPE_AI;
        lrModule.maActiveRaceCars[liSlot].mpRaceCar = &lrCar;
    }
    lrModule.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_2;
}

static int ResetCount(Fixture::RaceCarEntityModule& lrModule) {
    int liCount = 0;
    for (const Fixture::RaceCar& lrCar : lrModule.maRaceCars) liCount += lrCar.miResets;
    return liCount;
}

int main() {
    using namespace BrnGameState::GameStateModuleIO;
    const auto AsEvent = [](const void* lp) { return reinterpret_cast<const CgsModule::Event*>(lp); };

    // ---- action 120 -- E_ACTION_SHUTDOWN ------------------------------------------------------
    {
        Fixture::RaceCarEntityModule lModule; Build(lModule); Fixture::OutputFixture lOut;
        ShutdownAction lAction; std::memset(&lAction, 0, sizeof(lAction));
        lAction.mVictimCarID = 0x0000000100000001ull;   // +0: a car id whose words read as slots 0/1
        lAction.mRivalID     = 0x0000000400000004ull;   // +8: unwritten on console; decoy slot 4
        lAction.meVictimIndex = E_ACTIVE_RACE_CAR_INDEX_3;
        lModule.Dispatch(E_ACTION_SHUTDOWN, AsEvent(&lAction), &lOut);
        Check(lModule.maActiveRaceCars[3].mRenderParams.mbDamaged, "120: AI victim (record+0x10) drawn damaged (stb r23=1, 0x1BE4)");
        bool lbOthersClean = true;
        for (int liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
            if (liSlot != 3 && lModule.maActiveRaceCars[liSlot].mRenderParams.mbDamaged) lbOthersClean = false;
        Check(lbOthersClean, "120: only the record+0x10 slot changes (not +0 / +8)");

        lAction.meVictimIndex = E_ACTIVE_RACE_CAR_INDEX_2;   // the player: IsAIDriven false -> beq
        lModule.Dispatch(E_ACTION_SHUTDOWN, AsEvent(&lAction), &lOut);
        Check(!lModule.maActiveRaceCars[2].mRenderParams.mbDamaged, "120: player-driven victim untouched (IsAIDriven gate)");
        lAction.meVictimIndex = E_ACTIVE_RACE_CAR_INDEX_6;   // network car
        lModule.Dispatch(E_ACTION_SHUTDOWN, AsEvent(&lAction), &lOut);
        Check(!lModule.maActiveRaceCars[6].mRenderParams.mbDamaged, "120: network-driven victim untouched (IsAIDriven gate)");

        // No budget: five cars already damaged, the arm still switches the AI victim on.
        for (int liSlot : {0, 1, 3, 4, 5}) lModule.maActiveRaceCars[liSlot].mRenderParams.mbDamaged = true;
        lAction.meVictimIndex = E_ACTIVE_RACE_CAR_INDEX_7;
        lModule.Dispatch(E_ACTION_SHUTDOWN, AsEvent(&lAction), &lOut);
        Check(lModule.maActiveRaceCars[7].mRenderParams.mbDamaged, "120: no five-damaged-car budget in this arm");
        Check(ResetCount(lModule) == 0 && lModule.maRemoved.empty(), "120: no reset / removal side effects");
    }

    // ---- action 111 -- E_ACTION_PLAYER_INVULNERABLE --------------------------------------------
    {
        Fixture::RaceCarEntityModule lModule; Build(lModule); Fixture::OutputFixture lOut;
        PlayerInvulnerableAction lAction; lAction.mfInvulnerableTime = 3.925f;   // 1.425 + 2.5
        lModule.Dispatch(E_ACTION_PLAYER_INVULNERABLE, AsEvent(&lAction), &lOut);
        Check(lModule.maActiveRaceCars[2].mfInvulnerablityTime == 3.925f, "111: player slot +0x724 = record float (stfs 0x724)");
        bool lbOthersUntouched = true;
        for (int liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
            if (liSlot != 2 && lModule.maActiveRaceCars[liSlot].mfInvulnerablityTime != -1.0f) lbOthersUntouched = false;
        Check(lbOthersUntouched, "111: only the player's slot (this+0x182F8) is written");
    }

    // ---- action 3 -- E_ACTION_RESET_PLAYER_CAR_ON_TRACK ----------------------------------------
    {
        Fixture::RaceCarEntityModule lModule; Build(lModule); Fixture::OutputFixture lOut;
        ResetPlayerCarOnTrackAction lAction; lAction.mfSpeed = 26.8224f;   // 60 mph
        lModule.Dispatch(E_ACTION_RESET_PLAYER_CAR_ON_TRACK, AsEvent(&lAction), &lOut);
        const Fixture::RaceCar& lrPlayer = lModule.maRaceCars[5];
        Check(lrPlayer.miResets == 1, "3: exactly one RequestResetOnTrack on the player's global car");
        Check(lrPlayer.mfResetSpeed == 26.8224f, "3: speed argument is the record's float (f1 = lfs 0(record))");
        Check(lrPlayer.meResetType == BrnAI::E_RESET_TYPE_STANDARD, "3: reset type 1 (li r5, 1)");
        Check(lrPlayer.mfResetDistance == 0.0f, "3: distance 0.0 (f2 = f30 = flt_82001CC0)");
        Check(ResetCount(lModule) == 1, "3: no other car is reset");
    }

    // ---- action 121 -- E_ACTION_SHUTDOWN_FINISHED ----------------------------------------------
    {
        Fixture::RaceCarEntityModule lModule; Build(lModule); Fixture::OutputFixture lOut;
        ShutdownFinishedAction lAction; lAction.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_4;
        lModule.Dispatch(E_ACTION_SHUTDOWN_FINISHED, AsEvent(&lAction), &lOut);
        Check(lModule.maRemoved.size() == 1 && lModule.maRemoved[0] == static_cast<EGlobalRaceCarIndex>(14),
              "121: RemoveRaceCar(victim's GLOBAL index 14, not active index 4)");
        Check(lModule.maRemovedOutputs.size() == 1 && lModule.maRemovedOutputs[0] == &lOut,
              "121: RemoveRaceCar receives lpOutput (r5 = r20)");
        const Fixture::RaceCar& lrPlayer = lModule.maRaceCars[5];
        const f32 kfConsoleSpeed = 0.44704f * 50.0f;   // CRT 0x82C4BB30: fmuls flt_82F31928, flt_820138DC
        Check(lrPlayer.miResets == 1, "121: the player's global car gets one RequestResetOnTrack");
        Check(lrPlayer.mfResetSpeed == kfConsoleSpeed, "121: speed flt_82FAD720 == 0.44704f*50.0f (22.352 m/s, not 0.0)");
        Check(lrPlayer.meResetType == BrnAI::E_RESET_TYPE_STANDARD && lrPlayer.mfResetDistance == 0.0f,
              "121: reset type 1 and distance 0.0");
        Check(Fixture::gaCalls.size() == 2 && Fixture::gaCalls[0] == "remove:14" && Fixture::gaCalls[1] == "reset:5",
              "121: removal first, then the player reset (0x8230D2A8 before 0x8230D2D0)");

        // The IsAttached gate: an unattached victim does nothing at all.
        Fixture::RaceCarEntityModule lDetached; Build(lDetached);
        lDetached.maActiveRaceCars[4].mpRaceCar = nullptr;
        lDetached.Dispatch(E_ACTION_SHUTDOWN_FINISHED, AsEvent(&lAction), &lOut);
        Check(lDetached.maRemoved.empty() && ResetCount(lDetached) == 0,
              "121: unattached victim -> no removal and no player reset (beq @0x8230D284)");
    }

    Check(guAssertions == 0, "valid fixtures fire no assertions");
    std::printf("RcemTakedownActions: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
