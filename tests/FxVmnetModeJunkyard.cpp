// FX-VMNET (crash parity 2026-09-23, G40-D2 / G43-D2): the PRODUCTION bodies of
// VehicleManager::OnPrepareGameMode @0x825B5770, OnStartGameMode @0x825B5838 and
// OnJunkYardDriveThru @0x825EB050 (BrnVehicleManagerPlayerStats.cpp), extracted by
// run_fxvmnet_mode_junkyard.py and re-homed onto VmFixture (the two HIDE_ONLINE latches plus a
// recording SetAllNetworkRaceCarsHidden). The action payloads are the REAL BrnGameActions.h records
// and PrepareForModeAction::GetGameModeParams is the production body from BrnGameActions.cpp.
//
// Console facts checked (ARTIST asm):
//   OnPrepareGameMode  lwz 0x178(payload) = GetGameModeParams()->GetGameModeType(); != 15 && != 16
//                      -> log (gxMessageFilterFlags & 1) + stbx 1 -> +0x2A11E
//   OnStartGameMode    lwz 0(payload) = meGameMode; != 15 && != 16 -> log + stbx 0 -> +0x2A11E
//   OnJunkYardDriveThru lbz 0(payload): != 0 -> log, SetAllNetworkRaceCarsHidden(1), stbx 1 -> +0x2A11F
//                                       == 0 -> log, stbx 0 -> +0x2A11F (no hide call)
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/GameState/BrnGameActions.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <string>

static unsigned giAsserts = 0;
static std::string gLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { ++giAsserts; std::printf("ASSERT: %s\n", lpcMessage); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message { u64 gxMessageFilterFlags = 1; }
// Harness-only log capture (not under test): the stream base as CgsStrStream.cpp constructs it,
// and a DebugPrint sink that appends to gLog instead of the game log file.
StrStreamBase::StrStreamBase() : mePrintMode(E_PRINTMODE_DECIMAL) {}
namespace Log
{
    StrStreamBase& DebugPrint::operator<<(const char* lpcText) { gLog += lpcText; return *this; }
    void WriteToLog(const char*) {}
    static DebugPrint sCapture;
    DebugPrint* gpDebugPrint = &sCapture;
}
}

// The production GetGameModeParams (BrnGameActions.cpp), pasted by the runner.
namespace BrnGameState { namespace GameStateModuleIO {
#include "get_game_mode_params.inc"
} }

namespace BrnPhysics
{
namespace Vehicle
{
    struct VmFixture
    {
        bool mbInOnlineGameModeStartLine = false;
        bool mbPlayerCarInJunkYard       = false;
        s32  miHideCalls                 = 0;
        s32  miLastHideFrames            = -99;

        void SetAllNetworkRaceCarsHidden(s32 liFrames) { ++miHideCalls; miLastHideFrames = liFrames; }

        void OnPrepareGameMode(const BrnGameState::GameStateModuleIO::PrepareForModeAction* lpPrepareModeAction);
        void OnStartGameMode(const BrnGameState::GameStateModuleIO::StartPlayingModeAction* lpStartModeAction);
        void OnJunkYardDriveThru(const BrnGameState::GameStateModuleIO::DriveThruJunkYardAction* lpJunkYardAction);
    };

#include "mode_junkyard_methods.inc"
}
}

using namespace BrnGameState::GameStateModuleIO;
using BrnPhysics::Vehicle::VmFixture;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

static PrepareForModeAction gPrepare;   // static storage: zeroed, like the queue record it models

static void Prepare(VmFixture& lrVm, EGameModeType leMode)
{
    gPrepare.mGameModeParams.meGameModeType = leMode;
    lrVm.OnPrepareGameMode(&gPrepare);
}

static void Start(VmFixture& lrVm, EGameModeType leMode)
{
    StartPlayingModeAction lStart{};
    lStart.meGameMode = leMode;
    lrVm.OnStartGameMode(&lStart);
}

static void Junkyard(VmFixture& lrVm, bool lbIn)
{
    DriveThruJunkYardAction lAction{};
    lAction.mbIsInJunkYard = lbIn;
    lrVm.OnJunkYardDriveThru(&lAction);
}

int main()
{
    static const char* const KAC_PREPARED =
        "HIDE_ONLINE: Just prepared an online game mode (start forcing race cars to be visible when created)\n";
    static const char* const KAC_STARTED =
        "HIDE_ONLINE: Just started an online game mode (stop forcing race cars to be visible when created)\n";
    static const char* const KAC_ENTERED =
        "HIDE_ONLINE: Player entered a junkyard, so making all network cars hidden\n";
    static const char* const KAC_EXITED =
        "HIDE_ONLINE: Player exitted a junkyard, so can start making network cars visible again\n";

    // ---- G40-D2 OnPrepareGameMode: every mode but 15/16 sets the start-line latch ------------------
    {
        VmFixture lVm; gLog.clear();
        Prepare(lVm, E_MODE_OFFLINE_RACE);
        Check(lVm.mbInOnlineGameModeStartLine, "prepare(OFFLINE_RACE 0) sets mbInOnlineGameModeStartLine (stbx 1 @0x825B582C)");
        Check(gLog == KAC_PREPARED, "prepare logs the two-part HIDE_ONLINE line (0x82091358 + 0x82091300)");
    }
    {
        VmFixture lVm; gLog.clear();
        Prepare(lVm, E_MODE_ONLINE_FREE_BURN_LOBBY);
        Check(!lVm.mbInOnlineGameModeStartLine && gLog.empty(), "prepare(15 lobby) leaves the latch and logs nothing (cmpwi 0xF)");
        Prepare(lVm, E_MODE_ONLINE_SHOWTIME);
        Check(!lVm.mbInOnlineGameModeStartLine && gLog.empty(), "prepare(16 online Showtime) leaves the latch (cmpwi 0x10)");
        Prepare(lVm, E_MODE_ONLINE_FREE_BURN);
        Check(lVm.mbInOnlineGameModeStartLine, "prepare(14 online free burn) sets the latch -- only 15/16 are excluded");
    }
    {
        VmFixture lVm; lVm.mbInOnlineGameModeStartLine = true;
        Prepare(lVm, E_MODE_ONLINE_SHOWTIME);
        Check(lVm.mbInOnlineGameModeStartLine, "prepare(16) does not CLEAR an already-set latch (no store on that arm)");
        Prepare(lVm, E_MODE_ROAD_RAGE);
        Check(lVm.mbInOnlineGameModeStartLine, "prepare(3 road rage) keeps it set");
    }

    // ---- G40-D2 OnStartGameMode: every mode but 15/16 clears it --------------------------------------
    {
        VmFixture lVm; lVm.mbInOnlineGameModeStartLine = true; gLog.clear();
        Start(lVm, E_MODE_ROAD_RAGE);
        Check(!lVm.mbInOnlineGameModeStartLine, "start(3 road rage) clears the latch (stbx 0 @0x825B58D4)");
        Check(gLog == KAC_STARTED, "start logs the two-part HIDE_ONLINE line (0x82091358 + 0x820913B8)");
    }
    {
        VmFixture lVm; lVm.mbInOnlineGameModeStartLine = true; gLog.clear();
        Start(lVm, E_MODE_ONLINE_FREE_BURN_LOBBY);
        Check(lVm.mbInOnlineGameModeStartLine && gLog.empty(), "start(15) leaves the latch set");
        Start(lVm, E_MODE_ONLINE_SHOWTIME);
        Check(lVm.mbInOnlineGameModeStartLine && gLog.empty(), "start(16) leaves the latch set");
        Start(lVm, E_MODE_ONLINE_RACE);
        Check(!lVm.mbInOnlineGameModeStartLine, "start(10 online race) clears it");
    }
    {
        // The full PREPARE -> START window of an ordinary mode: set between the two, clear after.
        VmFixture lVm;
        Prepare(lVm, E_MODE_ONLINE_ROAD_RAGE);
        const bool lbDuring = lVm.mbInOnlineGameModeStartLine;
        Start(lVm, E_MODE_ONLINE_ROAD_RAGE);
        Check(lbDuring && !lVm.mbInOnlineGameModeStartLine, "the latch is set exactly between PREPARE and START");
    }

    // ---- G43-D2 OnJunkYardDriveThru ------------------------------------------------------------------
    {
        VmFixture lVm; gLog.clear();
        Junkyard(lVm, true);
        Check(lVm.miHideCalls == 1 && lVm.miLastHideFrames == 1,
              "entering calls SetAllNetworkRaceCarsHidden exactly once with 1 (li r4,1 @0x825EB0D8)");
        Check(lVm.mbPlayerCarInJunkYard, "entering sets mbPlayerCarInJunkYard (stbx 1 @0x825EB124)");
        Check(gLog == KAC_ENTERED, "entering logs 0x82091358 + 0x82097264");
    }
    {
        VmFixture lVm; lVm.mbPlayerCarInJunkYard = true; gLog.clear();
        Junkyard(lVm, false);
        Check(lVm.miHideCalls == 0, "leaving does NOT call SetAllNetworkRaceCarsHidden");
        Check(!lVm.mbPlayerCarInJunkYard, "leaving clears mbPlayerCarInJunkYard (stbx 0)");
        Check(gLog == KAC_EXITED, "leaving logs 0x82091358 + 0x82097218");
    }

    // ---- the log gate is the log ONLY (gxMessageFilterFlags bit 0) -----------------------------------
    {
        CgsDev::Message::gxMessageFilterFlags = 0;
        VmFixture lVm; gLog.clear();
        Prepare(lVm, E_MODE_OFFLINE_RACE);
        const bool lbPrepared = lVm.mbInOnlineGameModeStartLine;
        Junkyard(lVm, true);
        Check(lbPrepared && lVm.mbPlayerCarInJunkYard && lVm.miHideCalls == 1 && gLog.empty(),
              "with the filter bit clear the stores and the hide call still happen, silently");
        CgsDev::Message::gxMessageFilterFlags = 1;
    }

    Check(giAsserts == 0, "no console assert fires on a valid payload");

    std::printf("FxVmnetModeJunkyard: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
