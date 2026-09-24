// FX-AIMOD (crash parity 2026-09-22), G07-D1: ResetOnTrackManager::TestCarHNG @0x82790BD8.
// The runner extracts the PRODUCTION TestCarHNG from BrnResetOnTrackManager_AvoidObstacles.cpp;
// TestSectionHNG is an observed fixture that records the two segments it is asked about.
//
// Console geometry (0x82790C94..0x82790D80): front = pos + dir*(speed + 6.0), middle = pos + dir*4.5,
// half = (dir.y, -dir.x) * 2.5 * 0.5; TestSectionHNG(pos, front) then TestSectionHNG(middle + half,
// middle - half); a section hit answers 1 before traffic is consulted.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/World/AI/BrnHNGTest.h"
#undef protected
#undef private
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

static int gSectionCalls = 0;
static int gSectionHitOnCall = 0;
static Vector2 gaStart[4], gaEnd[4];
namespace BrnAI {
bool ResetOnTrackManager::TestSectionHNG(const AISection*, Vector2 lStartPos, Vector2 lEndPos)
{
    if (gSectionCalls < 4) { gaStart[gSectionCalls] = lStartPos; gaEnd[gSectionCalls] = lEndPos; }
    ++gSectionCalls;
    return gSectionCalls == gSectionHitOnCall;
}
// Every call below passes no nearby set, so TestCarHNG returns before its traffic legs; these
// two only satisfy the compile/link (the traffic legs are covered by run_fxairumble_traffic_hng.py).
bool LineTestTrafficHNG(const NearbyVehicles*, Vector2, Vector2) { return false; }
namespace { void NoteTrafficLegs(const NearbyVehicles*, f32, bool, bool) {} }
}

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(Vector2 lA, f32 lfX, f32 lfY) { return std::fabs(lA.x - lfX) < 1e-4f && std::fabs(lA.y - lfY) < 1e-4f; }

int main()
{
    static ResetOnTrackManager sManager;
    AISection lSection{};
    const Vector2 lPos = { 10.0f, 20.0f, 0.0f, 0.0f };
    const Vector2 lDir = { 0.0f, 1.0f, 0.0f, 0.0f };

    // No hit anywhere, no nearby set: both section legs run, answer false.
    gSectionCalls = 0; gSectionHitOnCall = 0;
    const bool lbClear = sManager.TestCarHNG(&lSection, 0, lPos, lDir, 14.0f);
    Check(!lbClear && gSectionCalls == 2, "clear road: two section legs, answer false");
    Check(Near(gaStart[0], 10.0f, 20.0f) && Near(gaEnd[0], 10.0f, 40.0f), "leg 1 runs pos -> pos + dir*(speed + 6.0) (0x82790D60)");
    Check(Near(gaStart[1], 11.25f, 24.5f) && Near(gaEnd[1], 8.75f, 24.5f), "leg 2 runs across the car at pos + dir*4.5, half-width 1.25 (0x82790D80)");

    // A hard-no-go line ahead: answered on the first leg.
    gSectionCalls = 0; gSectionHitOnCall = 1;
    Check(sManager.TestCarHNG(&lSection, 0, lPos, lDir, 14.0f) && gSectionCalls == 1, "hit on the look-ahead leg answers true at once");

    // A hard-no-go line across the car's front.
    gSectionCalls = 0; gSectionHitOnCall = 2;
    Check(sManager.TestCarHNG(&lSection, 0, lPos, lDir, 14.0f) && gSectionCalls == 2, "hit on the cross leg answers true");

    // A diagonal heading: the perpendicular is (dir.y, -dir.x).
    const Vector2 lDiag = { 0.6f, 0.8f, 0.0f, 0.0f };
    gSectionCalls = 0; gSectionHitOnCall = 0;
    sManager.TestCarHNG(&lSection, 0, lPos, lDiag, 4.0f);
    Check(Near(gaEnd[0], 10.0f + 6.0f, 20.0f + 8.0f), "diagonal look-ahead (speed 4 + 6 = 10 m)");
    // middle = (12.7, 23.6); half = (0.8, -0.6) * 1.25 = (1.0, -0.75)
    Check(Near(gaStart[1], 13.7f, 22.85f) && Near(gaEnd[1], 11.7f, 24.35f),
          "diagonal cross leg uses (dir.y, -dir.x) * 1.25");

    Check(gAssertions == 0, "valid inputs raise no assertion");
    std::printf("AIModCarHNG: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
