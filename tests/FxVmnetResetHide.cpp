// FX-VMNET (crash parity 2026-09-23, G44-D1): the per-event tail of
// VehicleManager::ProcessResetEvents @0x82617820 that follows the RaceCarResetEvent post
// (0x82617E2C..0x8261800C), EXTRACTED from the production body in
// BrnVehicleManager_WriteOutVehicleStats.cpp by run_fxvmnet_reset_hide.py (everything between the
// AddRaceCarResetEvent block and the PC-only [teleport] diagnostic) and compiled into
// VmFixture::ResetTail. The fixture carries the members the tail reads with their real types and a
// recording SetAllNetworkRaceCarsHidden. The number formatting is the production
// StrStreamBase (CgsStrStream.cpp, compiled in); only the DebugPrint sink is a capture.
//
// Console facts checked (ARTIST asm):
//   0x82617E2C lwz r11,0(r19) [this+0x2A0AC mePlayerActiveRaceCarIndex] ; cmpw r28,r11 ; bne 0x82617EF0
//   0x82617E38 ld 0x1908(lis 0x82F3) = gxMessageFilterFlags ; clrldi 63 ; beq 0x82617EE4 -- LOG only:
//              "HIDE_ONLINE: " (0x82091358) "Making all network race cars hidden for at least 1 frame
//              because player car " (0x82099D38) <player idx> " was just reset\n" (0x82099E30)
//   0x82617EE4 li r4,1 ; mr r3,r30 ; bl SetAllNetworkRaceCarsHidden -- reached from BOTH arms
//   0x82617EF0 same gate: "HIDE_ONLINE: " "Resetting race car " (0x82099E44) <idx> ", type "
//              (0x820941FC) <maeRaceCarTypes[idx]> "\n" (0x82001CC4) -- every event, player or not
#define _ALLOW_KEYWORD_MACROS 1
#include "GameSource/BurnoutConstants.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <string>

static std::string gLog;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int) { std::printf("ASSERT: %s\n", lpcMessage); return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Message { u64 gxMessageFilterFlags = 1; }
namespace Log
{
    // Harness-only sink: the game's DebugPrint writes to the log file; this one appends to gLog.
    StrStreamBase& DebugPrint::operator<<(const char* lpcText) { gLog += lpcText; return *this; }
    void WriteToLog(const char*) {}
    static DebugPrint sCapture;
    DebugPrint* gpDebugPrint = &sCapture;
}
}

// The production scalar formatting (StrStreamBase::operator<<(s32) etc.).
#include "GameShared/GameClasses/Development/CgsStrStream.cpp"

namespace BrnPhysics
{
namespace Vehicle
{
    struct VmFixture
    {
        EActiveRaceCarIndex    mePlayerActiveRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_0;
        BrnWorld::ERaceCarType maeRaceCarTypes[8] = {};
        s32                    miHideCalls      = 0;
        s32                    miLastHideFrames = -99;

        void SetAllNetworkRaceCarsHidden(s32 liFrames) { ++miHideCalls; miLastHideFrames = liFrames; }
        void ResetTail(s32 liRaceCar);
    };

    void VmFixture::ResetTail(s32 liRaceCar)
    {
#include "reset_tail.inc"
    }
}
}

using BrnPhysics::Vehicle::VmFixture;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::printf("FAIL: %s\n", lpcName); }
}

int main()
{
    // ---- the player's own car is reset: every network car is hidden for >= 1 frame -----------------
    {
        VmFixture lVm;
        lVm.mePlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(2);
        lVm.maeRaceCarTypes[2] = BrnWorld::E_RACE_CAR_TYPE_PLAYER;
        gLog.clear();
        lVm.ResetTail(2);
        Check(lVm.miHideCalls == 1 && lVm.miLastHideFrames == 1,
              "player-car reset calls SetAllNetworkRaceCarsHidden exactly once with 1 (0x82617EE4 li r4,1)");
        Check(gLog ==
              "HIDE_ONLINE: Making all network race cars hidden for at least 1 frame because player car 2 was just reset\n"
              "HIDE_ONLINE: Resetting race car 2, type 0\n",
              "player-car reset logs the two console streams in order (0x82099D38/0x82099E30, then 0x82099E44/0x820941FC)");
    }

    // ---- another car is reset: no hide, only the per-event stream ----------------------------------
    {
        VmFixture lVm;
        lVm.mePlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
        lVm.maeRaceCarTypes[5] = BrnWorld::E_RACE_CAR_TYPE_AI;
        gLog.clear();
        lVm.ResetTail(5);
        Check(lVm.miHideCalls == 0, "an AI car's reset does NOT hide the network cars (bne 0x82617EF0)");
        Check(gLog == "HIDE_ONLINE: Resetting race car 5, type 1\n",
              "an AI car's reset logs only the per-event stream");
    }

    // ---- the filter bit gates the LOG only ----------------------------------------------------------
    {
        CgsDev::Message::gxMessageFilterFlags = 0;
        VmFixture lVm;
        gLog.clear();
        lVm.ResetTail(0);
        Check(lVm.miHideCalls == 1 && lVm.miLastHideFrames == 1,
              "with gxMessageFilterFlags bit 0 clear the hide call STILL happens (beq 0x82617EE4 lands on it)");
        Check(gLog.empty(), "with the bit clear nothing is logged");
        CgsDev::Message::gxMessageFilterFlags = 1;
    }

    std::printf("FxVmnetResetHide: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
