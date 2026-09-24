// FX-AIBUZZ item 2 (crash parity 2026-09-24): RCEntityActiveRaceCarOutputInterface::IsRaceCarPlayer
// @0x82681DF0, run on the PRODUCTION body (extracted from BrnRCEntityActiveRaceCarOutputInterface.cpp by
// run_fxaibuzz_race_car_player.py) against the REAL interface struct (BrnRaceCarEntityModuleOutputInterface.h),
// on raw storage so no member constructor hides a field.
// The asm (an export hole, ppcdis): the two index asserts, then
//   addi r11, idx, 0x13C0 ; slwi r11, r11, 1 ; lhzx r11, r11, this      maxRaceCarFlags[idx] (+0x2780)
//   srwi r11, r11, 1 ; clrlwi r3, r11, 31                                 (flags >> 1) & 1
// -- bit 1, E_RACE_CAR_OUTPUT_FLAG_PLAYER. Both callers used a stand-in, `GetPlayerActiveRaceCarIndex() ==
// idx`, which reads a different member (mePlayerActiveRaceCarIndex): the cases below make the two disagree,
// and the flag must win.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::printf("  [assert] %s\n", lpcMessage ? lpcMessage : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
// The production body, extracted verbatim.
#include "fxaibuzz_race_car_player.inc"
}
}

using BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface;

namespace
{
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++gChecks;
        if (!lbPass) { ++gFailures; std::printf("FAIL %s\n", lpcLabel); }
    }

    alignas(16) unsigned char gaStorage[sizeof(RCEntityActiveRaceCarOutputInterface)];
    RCEntityActiveRaceCarOutputInterface& Iface() { return *reinterpret_cast<RCEntityActiveRaceCarOutputInterface*>(gaStorage); }

    void Reset(EActiveRaceCarIndex lePlayer)
    {
        std::memset(gaStorage, 0xA5, sizeof(gaStorage));
        for (s32 li = 0; li < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++li)
            Iface().maxRaceCarFlags[li] = 0;
        Iface().mePlayerActiveRaceCarIndex = lePlayer;
    }

    bool IsPlayer(s32 liIndex) { return Iface().IsRaceCarPlayer(static_cast<EActiveRaceCarIndex>(liIndex)); }
}

int main()
{
    // ---- the bit ---------------------------------------------------------------------------------------------
    Reset(E_ACTIVE_RACE_CAR_INDEX_0);
    Iface().maxRaceCarFlags[3] = 0x0002;
    Check(IsPlayer(3), "flags 0x0002 (PLAYER alone) -> true");
    Iface().maxRaceCarFlags[3] = 0x0003;
    Check(IsPlayer(3), "flags 0x0003 (IN_USE | PLAYER) -> true");
    Iface().maxRaceCarFlags[3] = 0x0102;
    Check(IsPlayer(3), "flags 0x0102 (PLAYER | IN_SHOWTIME) -> true");
    Iface().maxRaceCarFlags[3] = 0xFFFD;
    Check(!IsPlayer(3), "flags 0xFFFD (every bit but 1) -> false");
    Iface().maxRaceCarFlags[3] = 0x0001;
    Check(!IsPlayer(3), "flags 0x0001 (IN_USE alone) -> false");
    Iface().maxRaceCarFlags[3] = 0x0004;
    Check(!IsPlayer(3), "flags 0x0004 (RIVAL) -> false");

    // ---- every slot reads its own element (lhzx 2*(idx+0x13C0)) --------------------------------------------------
    Reset(E_ACTIVE_RACE_CAR_INDEX_0);
    Iface().maxRaceCarFlags[6] = 0x0003;
    for (s32 li = 0; li < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++li)
    {
        char lacLabel[96];
        std::snprintf(lacLabel, sizeof(lacLabel), "slot %d reads maxRaceCarFlags[%d] (only 6 has the bit)", li, li);
        Check(IsPlayer(li) == (li == 6), lacLabel);
    }

    // ---- the flag, not the player index (the stand-in's member) ---------------------------------------------------
    Reset(E_ACTIVE_RACE_CAR_INDEX_0);
    Iface().maxRaceCarFlags[0] = 0x0001;          // the player index names slot 0, its flag does not
    Check(!IsPlayer(0), "player index 0 but slot 0 lacks bit 1 -> false (the stand-in said true)");
    Iface().maxRaceCarFlags[5] = 0x0003;          // slot 5 carries the bit, the index says 0
    Check(IsPlayer(5), "slot 5 carries bit 1 while the player index is 0 -> true (the stand-in said false)");

    Reset(E_ACTIVE_RACE_CAR_INDEX_INVALID);       // the index is unset (the stand-in's accessor logs / asserts)
    Iface().maxRaceCarFlags[2] = 0x0003;
    Check(IsPlayer(2), "player index unset, slot 2 flagged -> true");
    Check(!IsPlayer(1), "player index unset, slot 1 unflagged -> false");

    Check(gAsserts == 0, "no assertion for in-range indices (the two asserts are the index bounds only)");

    std::printf("FxAiBuzzRaceCarPlayer: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
