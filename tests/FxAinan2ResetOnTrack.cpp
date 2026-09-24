// FX-AINAN2 regression (crash parity 2026-09-24): the reset-on-track placement's NaN polarity,
// extracted verbatim from BrnResetOnTrackManager.cpp / _Strategies.cpp by
// run_fxainan2_reset_on_track.py.
//
//   ComputeNearestPositionInSegment @0x82768908: `fcmpu distance, 0.0 ; ble -> return lStart`
//     (0x82768A08/0x82768A0C) -- ble is taken on an unordered compare, so a NaN distance answers
//     the segment START. The old `distance <= 0` sent it on to `lStart + direction * NaN`. This is
//     the crash exit's STANDARD reset (ComputeInitialCoordinatesStandard projects the car's last
//     good position with it).
//   GetRoadSideForStartingLine @0x82784378: the [-0.25, 0.25] clamp is two `fsubs ; fsel` pairs
//     (0x82784498..0x827844B0); fsel takes its third operand on NaN, so a NaN offset is +0.25 and
//     the grid slot is 0.5 -+ 0.25. The old if/if kept the NaN.
//   ScanBackwardsAlongExtrapolatedRoute @0x827847D0: `fcmpu aheadness, reset ; bge -> keep
//     walking` (0x82784B14/0x82784B48); ScanForwardsAlongExtrapolatedRoute @0x82784C40:
//     `fcmpu aheadness, reset ; ble -> keep walking` (0x82784F14/0x82784F18). Both are taken on
//     NaN, so a NaN walks off the end (false); the old `>=` / `<=` accepted the first pair.
// The ordered controls pass on both spellings.
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/World/AI/Route/BrnRouteMapModule.h"
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "rw/math/vpu/vector3_operation.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Message { u64 gxMessageFilterFlags = 0; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }
namespace CgsResource {
BaseResourcePtr::BaseResourcePtr() {}
BaseResourcePtr::~BaseResourcePtr() {}
}

// ---- fixtures ---------------------------------------------------------------------------------
namespace
{
    BrnAI::BoundaryLine gBoundary;
    f32 gfBoundaryLength = 0.0f;

    struct Walk { s32 miCount; u32 mauSections[16]; };
    Walk gForwardWalk;         // ExtrapolateRouteForwards when asked for more than one section
    Walk gForwardOneWalk;      // ExtrapolateRouteForwards(1, ...) -- ScanBackwards' look-ahead
    Walk gBackwardWalk;

    s32 Fill(const Walk& lrWalk, BrnAI::ExtrapolatedIndexArray& lrOut)
    {
        for (s32 li = 0; li < lrWalk.miCount; ++li)
        {
            lrOut[static_cast<u32>(li)].muSection = lrWalk.mauSections[li];
            lrOut[static_cast<u32>(li)].muPortal  = 0;
        }
        return lrWalk.miCount;
    }
}

namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetDirection() const { return mDirection; }
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
const Portal* AISection::GetPortal(u8 luPortalIndex) const { return &mpaPortals[luPortalIndex]; }
f32 Portal::GetPositionX() const { return mPositionX; }
f32 Portal::GetPositionY() const { return mPositionY; }
f32 Portal::GetPositionZ() const { return mPositionZ; }
const BoundaryLine* Portal::GetBoundaryLine(u8) const { return &gBoundary; }
f32 BoundaryLine::GetLength() const { return gfBoundaryLength; }
s32 RacingLineGenerator::ExtrapolateRouteForwards(s32 liNumSectionsToGenerate, s32, Vector2, Vector2,
                                                  const AISectionsData*, ExtrapolatedIndexArray& lrOut)
{
    return Fill(liNumSectionsToGenerate == 1 ? gForwardOneWalk : gForwardWalk, lrOut);
}
s32 RacingLineGenerator::ExtrapolateRouteBackwards(s32, u16, Vector2, Vector2, const AISectionsData*,
                                                   ExtrapolatedIndexArray& lrOut)
{
    return Fill(gBackwardWalk, lrOut);
}
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    bool Same(Vector3 lA, f32 lfX, f32 lfY, f32 lfZ)
    {
        return std::fabs(lA.x - lfX) < 1e-4f && std::fabs(lA.y - lfY) < 1e-4f && std::fabs(lA.z - lfZ) < 1e-4f;
    }

    AISection saSections[8];
    Portal saPortals[8][2];
    AISectionsData sData;
    AICar saCars[1];
    ResetOnTrackManager sManager;

    // Eight sections in a straight line along +Z; section i's portal 0 sits at z = lafZ[i].
    void World(const f32* lafZ, s32 liCount)
    {
        std::memset(saSections, 0, sizeof(saSections));
        std::memset(saPortals, 0, sizeof(saPortals));
        for (s32 li = 0; li < 8; ++li)
        {
            saSections[li].mId = static_cast<u32>(li);
            saSections[li].mpaPortals = saPortals[li];
            saSections[li].mu8NumPortals = 1;
            saPortals[li][0].mPositionX = 0.0f;
            saPortals[li][0].mPositionY = 0.0f;
            saPortals[li][0].mPositionZ = (li < liCount) ? lafZ[li] : 1000.0f;
        }
        sData.mpaSections = saSections;
        sData.muNumSections = 8;
        sManager.mpAISectionData.mpResourceMemory = &sData;
        sManager.mpaAICars = saCars;
        sManager.mePlayerGlobalRaceCarIndex = E_GLOBAL_RACE_CAR_INDEX_0;
        AICar& lrCar = saCars[0];
        lrCar.mPosition  = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        lrCar.mDirection = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        lrCar.muBestSectionIndex = 0;
        lrCar.muDefaultSectionIndex = 0;
        std::memset(&gForwardWalk, 0, sizeof(gForwardWalk));
        std::memset(&gForwardOneWalk, 0, sizeof(gForwardOneWalk));
        std::memset(&gBackwardWalk, 0, sizeof(gBackwardWalk));
    }
    void SetWalk(Walk& lrWalk, s32 liCount)
    {
        lrWalk.miCount = liCount;
        for (s32 li = 0; li < liCount; ++li) lrWalk.mauSections[li] = static_cast<u32>(li);
    }

    bool Forwards(f32 lfResetDistance)
    {
        const RouteNode* lpPrev = 0;
        const RouteNode* lpNext = 0;
        return sManager.ScanForwardsAlongExtrapolatedRoute(lpPrev, lpNext, lfResetDistance);
    }
    bool Backwards(f32 lfResetDistance, EExtrapolateType leType)
    {
        const RouteNode* lpPrev = 0;
        const RouteNode* lpNext = 0;
        return sManager.ScanBackwardsAlongExtrapolatedRoute(lpPrev, lpNext, lfResetDistance, leType);
    }
}

int main()
{
    // ---- ComputeNearestPositionInSegment -----------------------------------------------------
    const Vector3 lStart = { 0.0f, 0.0f, 0.0f, 0.0f };
    const Vector3 lEnd   = { 0.0f, 0.0f, 10.0f, 0.0f };
    Check(Same(sManager.ComputeNearestPositionInSegment(Vector3{ 3.0f, 9.0f, 5.0f, 0.0f }, lStart, lEnd), 0.0f, 0.0f, 5.0f),
          "control: a point beside the segment projects onto it");
    Check(Same(sManager.ComputeNearestPositionInSegment(Vector3{ 0.0f, 0.0f, -4.0f, 0.0f }, lStart, lEnd), 0.0f, 0.0f, 0.0f),
          "control: a point behind the start answers the start");
    Check(Same(sManager.ComputeNearestPositionInSegment(Vector3{ 0.0f, 0.0f, 15.0f, 0.0f }, lStart, lEnd), 0.0f, 0.0f, 10.0f),
          "control: a point past the end answers the end");
    Check(Same(sManager.ComputeNearestPositionInSegment(Vector3{ KF_NAN, KF_NAN, KF_NAN, 0.0f }, lStart, lEnd), 0.0f, 0.0f, 0.0f),
          "a NaN position gives a NaN distance -> ble 0x82768A0C taken -> the segment START");
    Check(Same(sManager.ComputeNearestPositionInSegment(Vector3{ 1.0f, KF_NAN, 4.0f, 0.0f }, lStart, lEnd), 0.0f, 0.0f, 0.0f),
          "one NaN lane is enough: the dot is NaN -> the start, not start + dir * NaN");

    // ---- GetRoadSideForStartingLine ------------------------------------------------------------
    {
        const f32 lafZ[1] = { 10.0f };
        World(lafZ, 1);
        sManager.mRandom.Construct();
        RouteNode lNode;
        std::memset(&lNode, 0, sizeof(lNode));
        lNode.muSectionIndex = 0;
        gfBoundaryLength = 0.0f;
        Check(sManager.GetRoadSideForStartingLine(&lNode, 1) == 0.5f,
              "control: a zero-width road answers the centre (bne 0x827843F0)");
        gfBoundaryLength = 16.0f;
        const f32 lfOrdered = sManager.GetRoadSideForStartingLine(&lNode, 1);
        Check(lfOrdered >= 0.25f && lfOrdered <= 0.75f, "control: an ordered width stays within 0.5 -+ 0.25");
        gfBoundaryLength = KF_NAN;
        Check(sManager.GetRoadSideForStartingLine(&lNode, 1) == 0.25f,
              "a NaN width -> NaN offset -> fsel 0x827844B0 picks +0.25 -> odd car 0.5 - 0.25");
        Check(sManager.GetRoadSideForStartingLine(&lNode, 2) == 0.75f,
              "... and an even car 0.25 + 0.5");
    }

    // ---- ScanForwardsAlongExtrapolatedRoute: portals ahead at z = 10, 20, 30, 40, 50 ----------
    {
        const f32 lafZ[5] = { 10.0f, 20.0f, 30.0f, 40.0f, 50.0f };
        World(lafZ, 5);
        SetWalk(gForwardWalk, 5);
        Check(Forwards(15.0f), "control: forwards accepts the first portal past 15 m");
        Check(sManager.mHelperNodePrev.mfY == 20.0f && sManager.mHelperNodeNext.mfY == 10.0f,
              "control: ... straddling it with the previous portal");
        Check(!Forwards(100.0f), "control: forwards finds nothing past 100 m");
        Check(!Forwards(KF_NAN), "a NaN reset distance -> ble 0x82784F18 taken on every portal -> false");
        saCars[0].mPosition = Vector3{ KF_NAN, 0.0f, KF_NAN, 0.0f };
        Check(!Forwards(15.0f), "a NaN car position -> NaN aheadness -> walks off the end -> false");
    }

    // ---- ScanBackwardsAlongExtrapolatedRoute: portals behind at z = -5, -10, -20, -30 ---------
    {
        const f32 lafZ[4] = { -5.0f, -10.0f, -20.0f, -30.0f };
        World(lafZ, 4);
        SetWalk(gBackwardWalk, 4);
        Check(Backwards(-15.0f, eExtrapolateType_RaceStart),
              "control: backwards accepts the first portal behind -15 m");
        Check(sManager.mHelperNodePrev.mfY == -20.0f && sManager.mHelperNodeNext.mfY == -10.0f,
              "control: ... straddling it with the portal before");
        Check(!Backwards(-100.0f, eExtrapolateType_RaceStart),
              "control: nothing behind -100 m and not road rage -> false");
        Check(Backwards(-100.0f, eExtrapolateType_RoadRage),
              "control: road rage takes the best pair once it is past -10 m (blt 0x82784C14)");
        Check(!Backwards(KF_NAN, eExtrapolateType_RaceStart),
              "a NaN reset distance -> bge 0x82784B48 taken on every portal -> false");
        Check(Backwards(KF_NAN, eExtrapolateType_RoadRage),
              "control: ... while road rage still takes the ordered best (-20) past -10");
        saCars[0].mPosition = Vector3{ KF_NAN, 0.0f, KF_NAN, 0.0f };
        Check(!Backwards(-15.0f, eExtrapolateType_RaceStart),
              "a NaN car position -> NaN aheadness -> walks off the end -> false");
        Check(!Backwards(-15.0f, eExtrapolateType_RoadRage),
              "... and road rage refuses a NaN best separation too (blt not taken)");
    }

    std::printf("FxAinan2ResetOnTrack: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
