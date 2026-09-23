// FX-GS (crash parity 2026-09-23): the PRODUCTION BrnGameState::CrashModeScoring bodies, extracted
// from src/GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoring.cpp by
// run_fxgs_crash_scoring.py and driven through the REAL CrashModeScoring layout
// (BrnCrashModeScoringRecentCrash.h), checked store-for-store against the ARTIST asm:
//
//   CrashModeScoring::ClearData @0x82320D10 (G10-D10)
//     0x82320D38..0x82320D84  the scalar stores: 0.0 (flt_82001CC0) to +0x40/+0x44/+0x48/+0x4C,
//                             +0x308/+0x310/+0x314/+0x318/+0x31C/+0x320; 1.0 (flt_82001C98) to
//                             +0x50 and +0x30C; 4 -> +0x2D8; 1 -> +0x2E4; 0 -> +0x2DC/+0x2E0/
//                             +0x2E8..+0x2F4/+0x2F8/+0x2FC/+0x324/+0x328; stb 0 +0x300, stb 1 +0x54
//     0x82320D88/0x82320D8C   stvx128 v0(=0) -> +0x20, +0x30   (the two Vector3 anchors)
//     0x82320D90..0x82320D98  stw 0 -> +0x288/+0x28C/+0x290    mRecentStuntSet read/write/length
//     0x82320D9C..0x82320DA4  stw 0 -> +0x60/+0x64/+0x68       mRecentlyHitPropSet read/write/length
//     0x82320DAC              stw 0 -> +0x27C                  maRecentCrashes count
//     and NOTHING to +0x304 (mfResetComboGracePeriod) or +0x55 (mbInfiniteCrashMode).
//
// The object starts as 0xCD garbage with every container holding entries and the two
// never-stored members holding sentinels, so a missed store and an invented store both fail.
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static unsigned gAsserts = 0, gChecks = 0, gFailures = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char*, const char*, int) { ++gAsserts; return 0; }
    void* EndAssert() { return nullptr; }
}
}

#include "crash_scoring_methods.inc"

using namespace BrnGameState;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
}

static bool Zero3(const Vector3& lr) { return lr.x == 0.0f && lr.y == 0.0f && lr.z == 0.0f; }

int main()
{
    static CrashModeScoring lScoring;
    std::memset(&lScoring, 0xCD, sizeof(lScoring));

    // Attach the three containers and give each some content.
    lScoring.mRecentStuntSet.Construct();
    lScoring.mRecentlyHitPropSet.Construct();
    lScoring.maRecentCrashes.Clear();
    for (CgsID lId = 11; lId <= 13; ++lId)
        lScoring.mRecentStuntSet.Push(&lId);
    CgsID lPopped = 0;
    lScoring.mRecentStuntSet.Pop(&lPopped);                    // read 1, write 3, length 2
    for (u16 luProp = 5; luProp <= 7; ++luProp)
        lScoring.mRecentlyHitPropSet.Push(&luProp);
    u16 luPoppedProp = 0;
    lScoring.mRecentlyHitPropSet.Pop(&luPoppedProp);           // read 1, write 3, length 2
    CrashModeScoring::RecentCrash lCrash = { 42, 1, 3.0f };
    lScoring.maRecentCrashes.Append(lCrash);

    Check(lScoring.mRecentStuntSet.miLength == 2 && lScoring.mRecentStuntSet.miReadPos == 1
              && lScoring.mRecentStuntSet.miWritePos == 3, "fixture: the stunt ring is populated");

    // The two members ClearData must NOT store, and the debug component it must not touch.
    lScoring.mfResetComboGracePeriod = 0.5f;   // Update's re-arm value (flt_82001DA0)
    lScoring.mbInfiniteCrashMode     = true;
    u8 laDebug[sizeof(lScoring.maCrashScoreDebugComponent)];
    std::memcpy(laDebug, lScoring.maCrashScoreDebugComponent, sizeof(laDebug));

    CgsContainers::RingBuffer<CgsID>& lrStunt = lScoring.mRecentStuntSet;
    CgsID* lpStuntData = lrStunt.mpData;
    CgsContainers::RingBuffer<u16>& lrProps = lScoring.mRecentlyHitPropSet;
    u16* lpPropData = lrProps.mpData;

    lScoring.ClearData();

    // --- G10-D10: the stunt ring IS cleared (0x82320D90..0x82320D98), its attach is kept ---
    Check(lrStunt.miReadPos == 0,  "D10 mRecentStuntSet read pos  (+0x288) = 0  @0x82320D90");
    Check(lrStunt.miWritePos == 0, "D10 mRecentStuntSet write pos (+0x28C) = 0  @0x82320D94");
    Check(lrStunt.miLength == 0,   "D10 mRecentStuntSet length    (+0x290) = 0  @0x82320D98");
    Check(lrStunt.mpData == lpStuntData && lrStunt.miMaxLength == 8,
          "D10 mRecentStuntSet keeps its buffer and capacity (no store to +0x280/+0x284)");
    // --- G10-D10: NO store to +0x304 ---
    Check(lScoring.mfResetComboGracePeriod == 0.5f,
          "D10 mfResetComboGracePeriod (+0x304) is NOT stored (no +0x304 in 0x82320D10..0x82320DC4)");

    // --- the rest of the console's store list ---
    Check(lrProps.miReadPos == 0 && lrProps.miWritePos == 0 && lrProps.miLength == 0,
          "mRecentlyHitPropSet read/write/length (+0x60/+0x64/+0x68) = 0  @0x82320D9C..0x82320DA4");
    Check(lrProps.mpData == lpPropData && lrProps.miMaxLength == 8, "mRecentlyHitPropSet keeps its buffer");
    Check(lScoring.maRecentCrashes.GetCount() == 0, "maRecentCrashes count (+0x27C) = 0  @0x82320DAC");
    Check(Zero3(lScoring.mPlayerPosLastFrame) && Zero3(lScoring.mPlayerPosLastStored),
          "Vector3 anchors (+0x20/+0x30) zeroed  @0x82320D88/0x82320D8C");
    Check(lScoring.mfTimeSincePlayerCarMoved == 0.0f && lScoring.mfTimeSinceLastEvent == 0.0f
              && lScoring.mfTimeSinceModeStart == 0.0f && lScoring.mfDistanceUntilStorePosition == 0.0f,
          "+0x40/+0x44/+0x48/+0x4C = 0.0");
    Check(lScoring.mfPlayerBoostPercentage == 1.0f, "+0x50 = 1.0 (flt_82001C98)  @0x82320DA8");
    Check(lScoring.mbPlayerIsCrashing == true, "+0x54 = 1  @0x82320DB0");
    Check(lScoring.mbInfiniteCrashMode == true, "+0x55 is NOT stored");
    Check(lScoring.miNumWheelsLastFrame == 4, "+0x2D8 = 4  @0x82320D44");
    Check(lScoring.miBaseScore == 0 && lScoring.miCurrentComboCount == 0, "+0x2DC/+0x2E0 = 0");
    Check(lScoring.miScoreMultiplier == 1, "+0x2E4 = 1  @0x82320D3C");
    Check(lScoring.maiNumCarsCrashed[0] == 0 && lScoring.maiNumCarsCrashed[1] == 0
              && lScoring.maiNumCarsCrashed[2] == 0 && lScoring.maiNumCarsCrashed[3] == 0,
          "+0x2E8..+0x2F4 = 0  @0x82320DB4..0x82320DC0");
    Check(lScoring.miNumCarsLeaped == 0 && lScoring.miNumPropsDestroyed == 0, "+0x2F8/+0x2FC = 0");
    Check(lScoring.mbAboutToResetCombo == false, "+0x300 = 0  @0x82320D80");
    Check(lScoring.mfDistanceTravelled == 0.0f, "+0x308 = 0.0  @0x82320D38");
    Check(lScoring.mfTimeSinceLastHitOverheadSign == 1.0f, "+0x30C = 1.0  @0x82320D84");
    Check(lScoring.mfTimeContactingWall == 0.0f && lScoring.mfTotalAirTime == 0.0f
              && lScoring.mfCurrentJumpAirTime == 0.0f && lScoring.mfLongestJumpAirTime == 0.0f
              && lScoring.mfHighestJump == 0.0f,
          "+0x310/+0x314/+0x318/+0x31C/+0x320 = 0.0");
    Check(lScoring.miStuntsPerformed == 0 && lScoring.miCarDestructionBonus == 0, "+0x324/+0x328 = 0");
    Check(std::memcmp(laDebug, lScoring.maCrashScoreDebugComponent, sizeof(laDebug)) == 0,
          "the embedded debug component (+0x00..+0x1F) is untouched");
    Check(gAsserts == 0, "no assert fires on a constructed scorer");

    std::printf("FxGsCrashScoring: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
