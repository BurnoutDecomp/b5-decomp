// FX-AIMOD (crash parity 2026-09-22), G08-D1: ResetOnTrackManager::AvoidObstacles @0x827941E0.
// The runner extracts the PRODUCTION AvoidObstacles / TestRecentResets / GetAICar bodies from the real
// source files (restored_methods.inc); TestSectionHNG and TestCarHNG are observed fixtures here, so
// the sweep's side order and the fallback shove are measured in isolation.
//
// The console's lateral is v124 == Cross(worldUp, dir) == (dir.z, 0, -dir.x) (0x8279428C..0x827942C0),
// the sweep tries -v124 first (var_E0, 0x827943C8 / r29 from 0x827943E4) then +v124 (var_D0,
// 0x827943C0), and the non-STANDARD fallback is pos + v124 * ((miResetCount % 8) - 4) * 3.0 * 2.0
// (0x82794518..0x827945C4). Every geometry below is exactly representable, so the 6 m boundary
// (`36 > d^2`, flt_82093AB4) resolves the same way on every compiler.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#undef protected
#undef private
#include "GameSource/Math/BrnMathUtils.h"
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

// ---- observed fixtures ------------------------------------------------------------------------
static int gCarHNGCalls = 0;
static int gCarHNGHitOnCall = 0;     // 1-based call that reports "a car is in the way"; 0 == never
namespace BrnAI {
bool ResetOnTrackManager::TestSectionHNG(const AISection*, Vector2, Vector2) { return false; }
bool ResetOnTrackManager::TestCarHNG(const AISection*, const NearbyVehicles*, Vector2, Vector2, f32)
{
    ++gCarHNGCalls;
    return gCarHNGCalls == gCarHNGHitOnCall;
}
}

// ---- the production bodies, unchanged -----------------------------------------------------------
#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(Vector3 lA, f32 lfX, f32 lfY, f32 lfZ)
{
    return std::fabs(lA.x - lfX) < 1e-4f && std::fabs(lA.y - lfY) < 1e-4f && std::fabs(lA.z - lfZ) < 1e-4f;
}

struct Scene
{
    ResetOnTrackManager mManager;
    AICar               mCar;
    AISection           mSection;
    ResetOnTrackManager::ResetOnTrackCoords mCoords;
    AIModuleIO::ResetOnTrackRequest         mRequest;

    Scene(EResetType leType, Vector3 lPosition, Vector3 lDirection, s32 liResetCount)
        : mManager(), mCar(), mSection(), mCoords(), mRequest()
    {
        mManager.mRecentResets.Construct();
        mManager.mpaAICars = &mCar;
        mManager.miResetCount = liResetCount;
        mCar.meCarState = E_AI_CAR_STATE_IN_RANGE;
        mRequest.meGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
        mRequest.mfResetSpeed = 20.0f;
        mRequest.mfResetDistance = 0.0f;
        mRequest.meResetType = leType;
        mCoords.mpAISection = &mSection;
        mCoords.mPosition = lPosition;
        mCoords.mDirection = lDirection;
        gCarHNGCalls = 0;
        gCarHNGHitOnCall = 0;
    }
    void RecentResetAt(Vector3 lPosition)
    {
        ResetOnTrackManager::RecentResetEntry lEntry{};
        lEntry.mPosition = lPosition;
        lEntry.mfTime = 0.0f;
        mManager.mRecentResets.Push(&lEntry);
    }
    bool Run() { return mManager.AvoidObstacles(&mRequest, &mCoords); }
};

int main()
{
    const Vector3 lPos       = { 16.0f, 0.0f, 32.0f, 0.0f };
    const Vector3 lDirZ      = { 0.0f, 0.0f, 1.0f, 0.0f };   // v124 == (1, 0, 0)
    const Vector3 lDirX      = { 1.0f, 0.0f, 0.0f, 0.0f };   // v124 == (0, 0, -1)

    // (1) STANDARD crash reset on top of a recent reset: the sweep reaches attempt 3 (6 m, first
    //     distance that is not `36 > d^2`) and takes SIDE 0 == -v124.
    {
        Scene s(E_RESET_TYPE_STANDARD, lPos, lDirZ, 0);
        s.RecentResetAt(lPos);
        Check(s.Run(), "type 1 sweep accepts a pose");
        Check(Near(s.mCoords.mPosition, 10.0f, 0.0f, 32.0f), "type 1: attempt 3 side 0 is pos - v124*6 (v124 = (1,0,0))");
    }
    {
        Scene s(E_RESET_TYPE_STANDARD, lPos, lDirX, 0);
        s.RecentResetAt(lPos);
        Check(s.Run(), "type 1 sweep accepts a pose (dir +x)");
        Check(Near(s.mCoords.mPosition, 16.0f, 0.0f, 38.0f), "type 1: dir +x -> side 0 is pos - (0,0,-1)*6");
    }
    // (2) Every candidate blocked by recent resets: a non-STANDARD type takes the deterministic
    //     fallback pos + v124 * ((count % 8) - 4) * 6.
    {
        Scene s(E_RESET_TYPE_BEHIND_PLAYER, lPos, lDirZ, 0);
        s.RecentResetAt(lPos);
        s.RecentResetAt(Vector3{ 22.0f, 0.0f, 32.0f, 0.0f });
        s.RecentResetAt(Vector3{ 10.0f, 0.0f, 32.0f, 0.0f });
        Check(s.Run(), "type 2 fallback accepts");
        Check(Near(s.mCoords.mPosition, -8.0f, 0.0f, 32.0f), "type 2 fallback, count 0: pos + v124*(-24)");
    }
    {
        Scene s(E_RESET_TYPE_BEHIND_PLAYER_ROAD_RAGE, lPos, lDirZ, 13);   // 13 % 8 == 5 -> +6 m
        s.RecentResetAt(lPos);
        s.RecentResetAt(Vector3{ 22.0f, 0.0f, 32.0f, 0.0f });
        s.RecentResetAt(Vector3{ 10.0f, 0.0f, 32.0f, 0.0f });
        Check(s.Run(), "type 3 fallback accepts");
        Check(Near(s.mCoords.mPosition, 22.0f, 0.0f, 32.0f), "type 3 fallback, count 13: pos + v124*(+6)");
    }
    {
        Scene s(E_RESET_TYPE_STANDARD, lPos, lDirZ, 0);
        s.RecentResetAt(lPos);
        s.RecentResetAt(Vector3{ 22.0f, 0.0f, 32.0f, 0.0f });
        s.RecentResetAt(Vector3{ 10.0f, 0.0f, 32.0f, 0.0f });
        Check(s.Run() && Near(s.mCoords.mPosition, 16.0f, 0.0f, 32.0f), "type 1 swept out keeps its pose (0x82794514)");
    }
    // (3) A car in the way on a race-start pose: the recent-reset test is skipped for type 6 and
    //     the first candidate (attempt 1, side 0 == -v124*2) is taken.
    {
        Scene s(E_RESET_TYPE_BEHIND_PLAYER_RACE_START, lPos, lDirZ, 0);
        gCarHNGHitOnCall = 1;
        Check(s.Run(), "type 6 car-in-the-way sweep accepts");
        Check(Near(s.mCoords.mPosition, 14.0f, 0.0f, 32.0f), "type 6: attempt 1 side 0 is pos - v124*2");
        Check(gCarHNGCalls == 2, "TestCarHNG ran on the pose and on the accepted candidate");
    }
    // (4) Controls that do not depend on the lateral sign.
    {
        Scene s(E_RESET_TYPE_BEHIND_PLAYER_RACE_START, lPos, lDirZ, 0);
        s.RecentResetAt(lPos);
        Check(s.Run() && Near(s.mCoords.mPosition, 16.0f, 0.0f, 32.0f), "type 6 ignores recent resets");
    }
    {
        Scene s(E_RESET_TYPE_STANDARD, lPos, lDirZ, 0);
        s.mCar.meCarState = E_AI_CAR_STATE_INACTIVE;
        s.RecentResetAt(lPos);
        Check(!s.Run() && Near(s.mCoords.mPosition, 16.0f, 0.0f, 32.0f), "inactive AICar is refused (0x82794318)");
    }
    Check(gAssertions == 0, "valid fixtures raise no assertion");
    std::printf("AIModAvoidObstacles: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
