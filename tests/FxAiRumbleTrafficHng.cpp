// FX-AI-RUMBLE (crash parity 2026-09-23), G07-D1: the two TRAFFIC legs of
// ResetOnTrackManager::TestCarHNG @0x82790BD8, i.e. BrnAI::LineTestTrafficHNG @0x8277A878 and the
// 4-argument BrnAI::DistancePointToLine @0x827653C0 it calls.
//
// The runner extracts the PRODUCTION bodies of all three from the real source files
// (BrnResetOnTrackManager_AvoidObstacles.cpp, BrnHNGTest.cpp, BrnAIUtils.cpp); a body missing from
// the revision under test is replaced by a stub that answers "not in the way", which is what the
// tree did before the fix. ResetOnTrackManager::TestSectionHNG is a fixture here (it reports no
// hard-no-go line), so every TestCarHNG check below reaches the traffic legs.
//
// Expected values are worked from the ARTIST asm (see BrnHNGTest.cpp / BrnAIUtils.cpp banners):
//   DistancePointToLine: foot = A + (B-A) * Dot(B-A, P-A)/|B-A|^2; returns Cross(B-A, P-A)/|B-A|,
//                        SIGNED; |B-A| == 0 -> foot = A, returns |P-A|.
//   LineTestTrafficHNG:  per vehicle, dist < 5.0 (flt_82F30698, signed) and the foot strictly
//                        inside A..B, then any of its 4 HNG lines with r in [0,1] and s in [0,1];
//                        parallel lines (denominator == 0) never count.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnHNGTest.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#undef protected
#undef private
#include "GameSource/Math/BrnMathUtils.h"   // BrnMath::IsNormal(Vector2): TestCarHNG's :2216 assert (FX-NANPOL)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
static const char* gpcLastAssert = "";
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; gpcLastAssert = lpcText; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

// Fixture: no hard-no-go line anywhere, so TestCarHNG always reaches its traffic legs.
static int gSectionCalls = 0;
namespace BrnAI {
bool ResetOnTrackManager::TestSectionHNG(const AISection*, Vector2, Vector2)
{
    ++gSectionCalls;
    return false;
}
}

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(f32 lfA, f32 lfB, f32 lfTol = 1e-5f) { return std::fabs(lfA - lfB) <= lfTol; }
static Vector2 V2(f32 lfX, f32 lfY) { Vector2 lV; lV.x = lfX; lV.y = lfY; lV.z = 0.0f; lV.w = 0.0f; return lV; }

static void SetLine(BoundaryLine& lrLine, f32 lfSX, f32 lfSY, f32 lfEX, f32 lfEY)
{
    lrLine.mfStartX = lfSX; lrLine.mfStartY = lfSY; lrLine.mfEndX = lfEX; lrLine.mfEndY = lfEY;
}

// A vehicle boxed by its four HNG lines: centre (cx, cy), half extents (hx, hy).
static void SetBoxVehicle(NearbyVehicle& lrVehicle, f32 lfCX, f32 lfCY, f32 lfHX, f32 lfHY)
{
    std::memset(&lrVehicle, 0, sizeof(lrVehicle));
    lrVehicle.mCentre = V2(lfCX, lfCY);
    SetLine(lrVehicle.maHNGLines[0], lfCX - lfHX, lfCY + lfHY, lfCX + lfHX, lfCY + lfHY);
    SetLine(lrVehicle.maHNGLines[1], lfCX + lfHX, lfCY + lfHY, lfCX + lfHX, lfCY - lfHY);
    SetLine(lrVehicle.maHNGLines[2], lfCX + lfHX, lfCY - lfHY, lfCX - lfHX, lfCY - lfHY);
    SetLine(lrVehicle.maHNGLines[3], lfCX - lfHX, lfCY - lfHY, lfCX - lfHX, lfCY + lfHY);
}

static NearbyVehicles gNearby;

static bool Traffic(f32 lfAX, f32 lfAY, f32 lfBX, f32 lfBY)
{
    return LineTestTrafficHNG(&gNearby, V2(lfAX, lfAY), V2(lfBX, lfBY));
}

int main()
{
    // ---- DistancePointToLine(point, start, end, &foot) @0x827653C0 ----------------------------
    {
        Vector2 lFoot = V2(99.0f, 99.0f);
        f32 lfD = DistancePointToLine(V2(1.0f, 1.0f), V2(0.0f, 0.0f), V2(2.0f, 0.0f), lFoot);
        Check(Near(lfD, 1.0f) && Near(lFoot.x, 1.0f) && Near(lFoot.y, 0.0f),
              "D1 point left of the line: foot (1,0), distance +1");

        lFoot = V2(99.0f, 99.0f);
        lfD = DistancePointToLine(V2(1.0f, -3.0f), V2(0.0f, 0.0f), V2(2.0f, 0.0f), lFoot);
        Check(Near(lfD, -3.0f) && Near(lFoot.x, 1.0f) && Near(lFoot.y, 0.0f),
              "D2 point right of the line: the distance is SIGNED (-3), no fabs");

        lFoot = V2(99.0f, 99.0f);
        lfD = DistancePointToLine(V2(5.0f, 1.0f), V2(0.0f, 0.0f), V2(2.0f, 0.0f), lFoot);
        Check(Near(lfD, 1.0f) && Near(lFoot.x, 5.0f) && Near(lFoot.y, 0.0f),
              "D3 foot beyond the end is not clamped (param 2.5)");

        lFoot = V2(99.0f, 99.0f);
        lfD = DistancePointToLine(V2(5.0f, 2.0f), V2(1.0f, 2.0f), V2(4.0f, 6.0f), lFoot);
        Check(Near(lfD, -3.2f) && Near(lFoot.x, 2.44f) && Near(lFoot.y, 3.92f),
              "D4 diagonal line: foot (2.44,3.92), distance Cross/|B-A| = -16/5");

        lFoot = V2(99.0f, 99.0f);
        lfD = DistancePointToLine(V2(6.0f, 8.0f), V2(3.0f, 4.0f), V2(3.0f, 4.0f), lFoot);
        Check(Near(lfD, 5.0f) && Near(lFoot.x, 3.0f) && Near(lFoot.y, 4.0f),
              "D5 zero-length line: foot = start, distance |P-A| = 5");

        lFoot = V2(99.0f, 99.0f);
        lfD = DistancePointToLine(V2(3.0f, 4.0f), V2(3.0f, 4.0f), V2(3.0f, 4.0f), lFoot);
        Check(lfD == 0.0f && Near(lFoot.x, 3.0f) && Near(lFoot.y, 4.0f),
              "D6 zero-length line through the point: distance 0 (vsel guard)");
    }

    // ---- LineTestTrafficHNG @0x8277A878: segment A (0,0) -> B (10,0) -------------------------
    {
        const unsigned luAssertsBefore = gAssertions;

        std::memset(&gNearby, 0, sizeof(gNearby));
        Check(!Traffic(0, 0, 10, 0), "T1 empty avoidance list: nothing in the way");

        gNearby.miCount = 1;
        SetBoxVehicle(gNearby.mVehicle[0], 5.0f, 1.0f, 2.0f, 2.0f);
        Check(Traffic(0, 0, 10, 0), "T2 a car boxed across the segment is in the way");

        SetBoxVehicle(gNearby.mVehicle[0], 5.0f, 6.0f, 1.0f, 7.0f);
        Check(!Traffic(0, 0, 10, 0),
              "T3 centre 6 m LEFT (distance +6 >= 5.0): skipped even though its long box crosses");

        SetBoxVehicle(gNearby.mVehicle[0], 5.0f, -6.0f, 1.0f, 7.0f);
        Check(Traffic(0, 0, 10, 0),
              "T4 centre 6 m RIGHT (distance -6 < 5.0, signed): tested, and its long box crosses");

        SetBoxVehicle(gNearby.mVehicle[0], -1.0f, 0.5f, 2.0f, 2.0f);
        Check(!Traffic(0, 0, 10, 0), "T5 foot behind the start: skipped (Dot(foot-A, B-A) > 0 fails)");

        SetBoxVehicle(gNearby.mVehicle[0], 11.0f, 0.5f, 2.0f, 2.0f);
        Check(!Traffic(0, 0, 10, 0), "T6 foot beyond the end: skipped (Dot(foot-B, B-A) < 0 fails)");

        SetBoxVehicle(gNearby.mVehicle[0], 0.0f, 1.0f, 2.0f, 2.0f);
        Check(!Traffic(0, 0, 10, 0), "T7 foot exactly on the start: skipped (strict > 0)");

        std::memset(&gNearby.mVehicle[0], 0, sizeof(gNearby.mVehicle[0]));
        gNearby.mVehicle[0].mCentre = V2(5.0f, 0.5f);
        for (s32 i = 0; i < NearbyVehicle::KI_TRAFFIC_NUM_HNG_LINES; ++i)
            SetLine(gNearby.mVehicle[0].maHNGLines[i], 4.0f, 0.0f, 6.0f, 0.0f);
        Check(!Traffic(0, 0, 10, 0), "T8 collinear HNG lines: zero denominator never counts");

        std::memset(&gNearby.mVehicle[0], 0, sizeof(gNearby.mVehicle[0]));
        gNearby.mVehicle[0].mCentre = V2(9.0f, 1.0f);
        for (s32 i = 0; i < NearbyVehicle::KI_TRAFFIC_NUM_HNG_LINES; ++i)
            SetLine(gNearby.mVehicle[0].maHNGLines[i], 30.0f, -1.0f, 30.0f, 3.0f);
        SetLine(gNearby.mVehicle[0].maHNGLines[3], 10.0f, -1.0f, 10.0f, 3.0f);
        Check(Traffic(0, 0, 10, 0), "T9 HNG line through B: r == 1 is inside (inclusive), line index 3 reached");

        SetLine(gNearby.mVehicle[0].maHNGLines[3], 10.0f + 1.0f / 64.0f, -1.0f, 10.0f + 1.0f / 64.0f, 3.0f);
        Check(!Traffic(0, 0, 10, 0), "T10 HNG line just past B: r > 1 misses");

        std::memset(&gNearby.mVehicle[0], 0, sizeof(gNearby.mVehicle[0]));
        gNearby.mVehicle[0].mCentre = V2(5.0f, 2.0f);
        for (s32 i = 0; i < NearbyVehicle::KI_TRAFFIC_NUM_HNG_LINES; ++i)
            SetLine(gNearby.mVehicle[0].maHNGLines[i], 30.0f, -1.0f, 30.0f, 3.0f);
        SetLine(gNearby.mVehicle[0].maHNGLines[1], 5.0f, 0.0f, 5.0f, 4.0f);
        Check(Traffic(0, 0, 10, 0), "T11 HNG line starting ON the segment: s == 0 is inside (inclusive)");

        SetLine(gNearby.mVehicle[0].maHNGLines[1], 5.0f, 1.0f / 64.0f, 5.0f, 4.0f);
        Check(!Traffic(0, 0, 10, 0), "T12 HNG line starting just above the segment: s < 0 misses");

        gNearby.miCount = 3;
        SetBoxVehicle(gNearby.mVehicle[0], 50.0f, 50.0f, 2.0f, 2.0f);
        SetBoxVehicle(gNearby.mVehicle[1], -30.0f, 0.0f, 2.0f, 2.0f);
        SetBoxVehicle(gNearby.mVehicle[2], 8.0f, -0.5f, 1.0f, 1.0f);
        Check(Traffic(0, 0, 10, 0), "T13 the loop walks past two clear cars to the third, which is in the way");
        gNearby.miCount = 2;
        Check(!Traffic(0, 0, 10, 0), "T14 miCount bounds the walk (the third car is not read)");

        Check(gAssertions == luAssertsBefore, "T15 valid lists raise no assertion");

        gNearby.miCount = -1;
        const bool lbNegative = Traffic(0, 0, 10, 0);
        Check(!lbNegative && gAssertions == luAssertsBefore + 1 && std::strcmp(gpcLastAssert, "miCount >= 0") == 0,
              "T16 the walk reads the count through GetCount and its 'miCount >= 0' assert (BrnAIDriver.cpp:2912)");
        gNearby.miCount = 0;
    }

    // ---- TestCarHNG @0x82790BD8: the traffic legs --------------------------------------------
    {
        static ResetOnTrackManager sManager;
        AISection lSection{};
        const Vector2 lPos = V2(0.0f, 0.0f);
        const Vector2 lDir = V2(0.0f, 1.0f);   // front (0, 20) at speed 14; across (1.25,4.5)->(-1.25,4.5)

        std::memset(&gNearby, 0, sizeof(gNearby));
        gSectionCalls = 0;
        Check(!sManager.TestCarHNG(&lSection, 0, lPos, lDir, 14.0f) && gSectionCalls == 2,
              "C1 no nearby set: both section legs, answer false (0x82790DA4)");

        gNearby.miCount = 1;
        SetBoxVehicle(gNearby.mVehicle[0], 0.5f, 12.0f, 2.0f, 2.0f);
        Check(sManager.TestCarHNG(&lSection, &gNearby, lPos, lDir, 14.0f),
              "C2 a car on the look-ahead leg (pos -> pos + dir*(speed+6)) is in the way (0x82790DB8)");

        SetBoxVehicle(gNearby.mVehicle[0], 0.5f, 6.5f, 0.3f, 2.2f);
        Check(!Traffic(0, 0, 0, 20), "C3a the narrow car beside the look-ahead leg is not on it");
        Check(sManager.TestCarHNG(&lSection, &gNearby, lPos, lDir, 14.0f),
              "C3 ...but it straddles the cross leg at pos + dir*4.5 (0x82790DD4)");

        SetBoxVehicle(gNearby.mVehicle[0], 10.0f, 10.0f, 2.0f, 2.0f);
        Check(!sManager.TestCarHNG(&lSection, &gNearby, lPos, lDir, 14.0f),
              "C4 a car off to the side is not in the way");

        SetBoxVehicle(gNearby.mVehicle[0], 0.5f, 12.0f, 2.0f, 2.0f);
        Check(!sManager.TestCarHNG(&lSection, &gNearby, lPos, lDir, 0.0f),
              "C5 at speed 0 the look-ahead is 6 m and the car at 12 m is beyond it");
    }

    std::printf("FxAiRumbleTrafficHng: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
