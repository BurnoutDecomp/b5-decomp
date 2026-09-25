// FX-RCEM4 (crash parity 2026-09-24, G68-D10): RaceCarEntityModule::HandleGameActions' online arms
// (ARTIST 0x8230BE08) and RemoveAllNetworkCarsFromWorld (0x82306028), extracted VERBATIM from
// BrnRaceCarEntityModule.cpp / BrnRaceCarEntityModule_Rivals.cpp by run_fxrcem4_network_arms.py and
// dispatched against fixture race cars. A missing arm is replayed as the console's `default: break;`.
//   case 11   0x8230CCA8..0x8230CD40  asserts :7150/:7156/:7160 ; maActiveRaceCars[idx]+0x799 = 1
//   case 27   0x8230CD44              RemoveAllNetworkCarsFromWorld(lpOutput)  (41 shares it)
//   case 220  0x8230D50C..0x8230D614  asserts :7240/:7241/:7242 ; idx != player: ClearActiveRaceCar-
//                                     ToPlayerScoringMapping, SetInCurrentGameMode(0,0), +0x777 = 0,
//                                     RemoveRaceCar(GetGlobalRaceCarIndex())
//   0x82306028 for 0..34: IsInWorld (type != 3) && IsNetworkDriven (type == 2) ->
//                                     SetInCurrentGameMode(0,0) ; RemoveRaceCar
#include "types.hpp"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/PC/BrnNetHarnessPC.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static unsigned guAssertions = 0;
static std::string gsLastAssertion;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { ++guAssertions; gsLastAssertion = lpcMessage; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

// The [net] "network car disconnected / removed" witnesses in arms 11 / 220 and in
// RemoveAllNetworkCarsFromWorld (b5 ca4ac341, PC harness lines, not console code). The real
// declaration is included above, so this silent definition must keep its signature.
namespace BrnNetHarnessPC
{
    void Witness(const char*, const char*, ...) {}
}

namespace Fixture {
static std::vector<std::string> gaCalls;
static void Call(const std::string& lrCall) { gaCalls.push_back(lrCall); }

struct OutputFixture {};
namespace RaceCarEntityModuleIO { typedef OutputFixture OutputBuffer_PreScene; }

enum ERaceCarType : u8 { E_RACE_CAR_TYPE_PLAYER = 0, E_RACE_CAR_TYPE_AI = 1, E_RACE_CAR_TYPE_NETWORK = 2,
                         E_RACE_CAR_TYPE_INACTIVE = 3 };

struct RaceCar {
    ERaceCarType muType = E_RACE_CAR_TYPE_INACTIVE;
    EGlobalRaceCarIndex meIndex = E_GLOBAL_RACE_CAR_INDEX_0;
    s32 miActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    bool mbInGameMode = true, mbCarSelectAllowed = true;
    bool IsInWorld() const { return muType != E_RACE_CAR_TYPE_INACTIVE; }
    bool IsNetworkDriven() const { return muType == E_RACE_CAR_TYPE_NETWORK; }
    EGlobalRaceCarIndex GetGlobalRaceCarIndex() const { return meIndex; }
    // BrnRaceCar.h's inline getter (no side effect); only ca4ac341's witness reads it here.
    EActiveRaceCarIndex GetActiveRaceCarIndex() const { return static_cast<EActiveRaceCarIndex>(miActiveRaceCarIndex); }
    void SetInCurrentGameMode(bool lbInGameMode, bool lbCarSelectAllowed) {
        mbInGameMode = lbInGameMode; mbCarSelectAllowed = lbCarSelectAllowed;
        Call("mode " + std::to_string(static_cast<s32>(meIndex)) + " " + (lbInGameMode ? "1" : "0") + (lbCarSelectAllowed ? "1" : "0"));
    }
};

struct ActiveRaceCar {
    RaceCar* mpRaceCar = nullptr;
    bool mbIsDisconnectedFromNetwork = false;
    bool mbIsInGameMode = true;
    RaceCar* GetGlobalRaceCar() const { return mpRaceCar; }
    void SetDisconnectedFromNetwork() { mbIsDisconnectedFromNetwork = true; }
    void SetInGameMode(bool lbInGameMode) { mbIsInGameMode = lbInGameMode; }
};

struct RaceCarEntityModule {
    ActiveRaceCar maActiveRaceCars[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    RaceCar maRaceCars[E_GLOBAL_RACE_CAR_INDEX_COUNT];
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
    bool mbIsInOnlineGameMode = true;

    RaceCarEntityModule() {
        for (s32 i = 0; i < E_GLOBAL_RACE_CAR_INDEX_COUNT; ++i) maRaceCars[i].meIndex = static_cast<EGlobalRaceCarIndex>(i);
    }
    ActiveRaceCar* GetActiveRaceCar(EActiveRaceCarIndex le) { return &maActiveRaceCars[le]; }
    RaceCar* GetGlobalRaceCar(EGlobalRaceCarIndex le) { return &maRaceCars[le]; }
    void ClearActiveRaceCarToPlayerScoringMapping(EActiveRaceCarIndex le) { Call("unmap " + std::to_string(static_cast<s32>(le))); }
    void RemoveRaceCar(EGlobalRaceCarIndex le, RaceCarEntityModuleIO::OutputBuffer_PreScene*) {
        Call("remove " + std::to_string(static_cast<s32>(le)));
        maRaceCars[le].muType = E_RACE_CAR_TYPE_INACTIVE;
    }
    void RemoveAllNetworkCarsFromWorld(RaceCarEntityModuleIO::OutputBuffer_PreScene* lpOutput);
    void Dispatch(s32 liType, const CgsModule::Event* lpEvent, OutputFixture* lpOutput);
};
#include "fxrcem4_na_pieces.inc"
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Called(const char* lpc) {
    for (const std::string& s : Fixture::gaCalls) if (s == lpc) return true;
    return false;
}

int main() {
    using namespace Fixture;
    namespace GsmIO = BrnGameState::GameStateModuleIO;
    Check(GsmIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED == 11 && GsmIO::E_ACTION_FINISH_MODE_FINAL_ONLINE == 27
          && GsmIO::E_ACTION_QUIT_MODE_ONLINE == 41 && GsmIO::E_ACTION_ONLINE_PLAYER_REMOVED == 220,
          "the four ids are the ARTIST jump-table slots (low 11/27/41, high 113 == 220)");
    OutputFixture lOut;

    // ---- case 11 ----------------------------------------------------------------------------------
    {
        RaceCarEntityModule m; m.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        GsmIO::RemotePlayerDisconnectedAction a; std::memset(&a, 0, sizeof(a));
        a.meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(3);
        guAssertions = 0;
        m.Dispatch(GsmIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED, reinterpret_cast<const CgsModule::Event*>(&a), &lOut);
        Check(m.maActiveRaceCars[3].mbIsDisconnectedFromNetwork && !m.maActiveRaceCars[0].mbIsDisconnectedFromNetwork
              && guAssertions == 0, "11: slot 3 is marked disconnected from the network (stb 1, +0x799)");

        a.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        m.Dispatch(GsmIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED, reinterpret_cast<const CgsModule::Event*>(&a), &lOut);
        Check(guAssertions == 1 && gsLastAssertion.find("!mbIsInOnlineGameMode") == 0,
              "11: the local player's own slot online fires :7156");
        m.mbIsInOnlineGameMode = false; guAssertions = 0;
        m.Dispatch(GsmIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED, reinterpret_cast<const CgsModule::Event*>(&a), &lOut);
        Check(guAssertions == 0, "11: ...but not offline (lbzx +0x18345 == 0 skips the compare)");
    }

    // ---- cases 27 / 41 -> RemoveAllNetworkCarsFromWorld ------------------------------------------------
    for (s32 liType : { static_cast<s32>(GsmIO::E_ACTION_FINISH_MODE_FINAL_ONLINE), static_cast<s32>(GsmIO::E_ACTION_QUIT_MODE_ONLINE) })
    {
        RaceCarEntityModule m;
        m.maRaceCars[0].muType = E_RACE_CAR_TYPE_PLAYER;
        m.maRaceCars[4].muType = E_RACE_CAR_TYPE_NETWORK;
        m.maRaceCars[9].muType = E_RACE_CAR_TYPE_AI;
        m.maRaceCars[34].muType = E_RACE_CAR_TYPE_NETWORK;
        gaCalls.clear(); guAssertions = 0;
        m.Dispatch(liType, nullptr, &lOut);
        Check(Called("mode 4 00") && Called("remove 4") && Called("mode 34 00") && Called("remove 34"),
              "27/41: every NETWORK car in the world leaves the mode and is removed (incl. the last slot, 34)");
        Check(!Called("remove 0") && !Called("remove 9") && m.maRaceCars[9].mbInGameMode,
              "27/41: the player's and the AI cars stay");
        Check(gaCalls.size() == 4u && gaCalls[0] == "mode 4 00" && gaCalls[1] == "remove 4",
              "27/41: SetInCurrentGameMode(false, false) before RemoveRaceCar (0x823060D8 / 0x823060E8)");
    }

    // ---- case 220 ---------------------------------------------------------------------------------
    {
        RaceCarEntityModule m; m.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        m.maRaceCars[6].muType = E_RACE_CAR_TYPE_NETWORK;
        m.maActiveRaceCars[2].mpRaceCar = &m.maRaceCars[6];
        GsmIO::OnlinePlayerRemovedAction a; std::memset(&a, 0, sizeof(a));
        a.meActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(2);
        gaCalls.clear(); guAssertions = 0;
        m.Dispatch(GsmIO::E_ACTION_ONLINE_PLAYER_REMOVED, reinterpret_cast<const CgsModule::Event*>(&a), &lOut);
        Check(gaCalls.size() == 3u && gaCalls[0] == "unmap 2" && gaCalls[1] == "mode 6 00" && gaCalls[2] == "remove 6",
              "220: unmap the scoring slot, leave the mode, remove the global car -- in that order");
        Check(!m.maActiveRaceCars[2].mbIsInGameMode && guAssertions == 0, "220: the slot's mbIsInGameMode (+0x777) clears");

        a.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0; gaCalls.clear();
        m.Dispatch(GsmIO::E_ACTION_ONLINE_PLAYER_REMOVED, reinterpret_cast<const CgsModule::Event*>(&a), &lOut);
        Check(gaCalls.empty() && m.maActiveRaceCars[0].mbIsInGameMode,
              "220: the local player's own slot is left alone (cmpw ; beq @0x8230D580)");

        RaceCarEntityModule m2;
        a.meActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        m2.mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;   // so the body is skipped
        guAssertions = 0; gsLastAssertion.clear();
        m2.Dispatch(GsmIO::E_ACTION_ONLINE_PLAYER_REMOVED, reinterpret_cast<const CgsModule::Event*>(&a), &lOut);
        Check(guAssertions == 1 && gsLastAssertion.find(">= E_ACTIVE_RACE_CAR_INDEX_0") != std::string::npos,
              "220: a -1 slot fires :7241 (bge skips only >= 0)");
    }

    std::printf("FxRcem4NetworkArms: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
