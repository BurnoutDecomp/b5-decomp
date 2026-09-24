// FX-SHOWTIME2 (crash parity 2026-09-24): the PRODUCTION CrashModeScoring::GetVehicleScoreData
// @0x82312AB0, extracted from src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp by
// run_fxshowtime2_score_data_print.py, run against a capturing CgsDev::Log::DebugPrint and a controllable
// CgsDev::Message::gxMessageFilterFlags.
//
// ARTIST: every fallback arm (a type id the 24-row table @0x82020FA8 lacks) ends `b 0x82312C4C`:
//   0x82312C54  CgsIDUnCompress(id, sp+0x50)
//   0x82312C5C  ld 0x82F31908 (gxMessageFilterFlags) ; clrldi 63 ; beq -> skip   (bit 0 only)
//   0x82312C8C / 0x82312CA4 / 0x82312CC0  gpDebugPrint slot 1 with
//               "Unknown traffic vehicle in Showtime scoring: " (@0x8202304C), the id, "\n" (@0x82001CC4)
// A table hit returns before any of it (`blt cr6, 0x82312CC4` @0x82312BA0).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>
#include <cstring>
#include <string>

static unsigned gChecks = 0, gFailures = 0;
static unsigned gAsserts = 0;
static std::string gPrinted;
static unsigned gUnCompressCalls = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
namespace Log
{
    // The capture: DebugPrint's one sink, every string it is handed appended in order.
    StrStreamBase& DebugPrint::operator<<(const char* lpcText)
    {
        gPrinted += (lpcText != nullptr) ? lpcText : "<null>";
        return *this;
    }
    static DebugPrint gCapture;
    DebugPrint* gpDebugPrint = &gCapture;
}
namespace Message
{
    u64 gxMessageFilterFlags = 0;
}
}

// A recognisable stand-in for the id un-compress (the real one is covered elsewhere): the low
// 40 bits in hex, 12 characters + NUL == KI_CGSID_STRING_LEN.
void CgsIDUnCompress(CgsID lId, char* lpcString)
{
    ++gUnCompressCalls;
    std::snprintf(lpcString, KI_CGSID_STRING_LEN, "ID%010llX", static_cast<unsigned long long>(lId & 0xFFFFFFFFFFull));
}

namespace BrnGameState
{
#include "score_data_methods.inc"
}

using namespace BrnGameState;

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

int main()
{
    const CgsID KU_ID_TARGET  = 0xBF2E42A8A7700000ULL;   // a table row (TARGETVEHICLE, 6000, x1)
    const CgsID KU_ID_UNKNOWN = 0x0000001234ABCDEFULL;   // no row -> the class fallback
    s32 liScore = -1, liMultiplier = -1;
    BrnTraffic::VehicleScoreCategory leCategory = BrnTraffic::E_VEHICLESCORE_CAR;

    std::printf("-- a table hit, filter bit 0 set\n");
    CgsDev::Message::gxMessageFilterFlags = 1;
    gPrinted.clear();
    CrashModeScoring::GetVehicleScoreData(BrnTraffic::E_VEHICLECLASS_CAR, KU_ID_TARGET, &liScore, &liMultiplier, &leCategory);
    Check(liScore == 6000 && liMultiplier == 1 && leCategory == BrnTraffic::E_VEHICLESCORE_TARGETVEHICLE,
          "the table row is returned (6000, x1, TARGETVEHICLE)");
    Check(gPrinted.empty() && gUnCompressCalls == 0, "a table hit prints nothing and un-compresses nothing");

    std::printf("-- an unknown type, filter bit 0 set\n");
    gPrinted.clear();
    CrashModeScoring::GetVehicleScoreData(BrnTraffic::E_VEHICLECLASS_BUS, KU_ID_UNKNOWN, &liScore, &liMultiplier, &leCategory);
    Check(liScore == 5000 && liMultiplier == 0 && leCategory == BrnTraffic::E_VEHICLESCORE_BUS,
          "the class fallback is returned (bus: 5000, x0, category 3)");
    Check(gUnCompressCalls == 1, "the id is un-compressed once (0x82312C54)");
    Check(gPrinted == "Unknown traffic vehicle in Showtime scoring: ID1234ABCDEF\n",
          "the console's line is printed: the literal @0x8202304C, the un-compressed id, \"\\n\" @0x82001CC4");

    std::printf("-- an unknown type, filter bit 0 clear (every other bit set)\n");
    CgsDev::Message::gxMessageFilterFlags = ~static_cast<u64>(1);
    gPrinted.clear();
    CrashModeScoring::GetVehicleScoreData(BrnTraffic::E_VEHICLECLASS_VAN, KU_ID_UNKNOWN, &liScore, &liMultiplier, &leCategory);
    Check(liScore == 2000 && leCategory == BrnTraffic::E_VEHICLESCORE_VAN, "the van fallback is returned");
    Check(gUnCompressCalls == 2, "the id is still un-compressed (the call precedes the filter test)");
    Check(gPrinted.empty(), "nothing is printed: only bit 0 gates it (clrldi r11, r11, 63)");

    Check(gAsserts == 0, "no assert fired");

    std::printf("FxShowtime2ScoreDataPrint: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
