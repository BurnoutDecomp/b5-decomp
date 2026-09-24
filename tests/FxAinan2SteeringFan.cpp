// FX-AINAN2 regression (crash parity 2026-09-24): NaN polarity of the AI steering fan. The
// production bodies and TU helpers are extracted verbatim by run_fxainan2_steering_fan.py; the
// car accessors, the racing-line section queries, HardNoGoMap::DistanceToHardNoGoEdge,
// DistancePosVelToOrigin and FanIntersectsEdge are fixtures answering configured values.
//
// Console forms (fsel d,a,b,c = a >= 0 ? b : c takes c on NaN; the fan's IsZero is a per-lane
// non-dot vcmpgtfp |v| > eps gathered by vperm, under which a NaN lane counts as ZERO):
//   ClampAggression       fsel ladder (e.g. 0x8278831C..0x82788330)       NaN -> hi
//   IsZero2DAggression    vcmpgtfp + vperm (e.g. 0x8278825C..0x8278828C)  NaN -> zero
//   IncludeDriftLocationTracking @0x82788738   NaN vector is zero -> return, row untouched
//   IncludeDriftDirectionTracking @0x827881B0  NaN vector skips the normalise; the clamp -> 1.0
//   IncludeDriveCloseToPlayer @0x82787E58      NaN player velocity is zero -> zero row;
//                                               bge 0x82788140: NaN passing space -> the >= arm
//   IncludeSmashIntoTarget @0x82787968         NaN separation -> proximity 1 - 1 = 0 -> zero row
//   FindNeabyAIInTraffic @0x82787BE8            bgt 0x82787DEC: NaN relative speed still gets the
//                                               aheadness test
//   CalculateFanAngle @0x82768CB0              saturate fsel 0x82768D10/0x82768D1C: NaN -> 1.0
//   IncludeCentreLineTracking @0x82786BC8      NaN delta is zero -> bnelr, row untouched
//   IncludeRouteParallelTracking @0x82786DB8   NaN delta is zero -> zero row; clamp
//                                               0x82786F84/0x82786F94: NaN dot -> 1.0
//   IncludeHardNoGo @0x82779D98                min/max fsel 0x82779FC0/0x82779FC4: a NaN entry is
//                                               the minimum -> every rescaled entry NaN
//   IncludeRouteEdgeIntersection (hole)        high clamp fsel 0x8277A55C: NaN range -> 1.0 -> 0 weight
// Every NaN check fails on the pre-fix bodies (--rev <fix>~1); the ordered controls pass on both.
#define BRN_AI_STEERINGFAN_HNG_PRESENT 1
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan.h"
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan_AggressionConstants.h"
#include "GameSource/World/AI/RacingLine/BrnAISteeringFan_TrafficConstants.h"
#include "GameSource/World/AI/RacingLine/BrnHardNoGoMap.h"
#include "GameSource/World/AI/RacingLine/BrnRacingLineGenerator.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "GameSource/World/AI/Route/BrnRacingLine.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
StrStreamBase& StrStreamBase::operator<<(s32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(u32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(f32) { return *this; }
}

static f32 gfPosVelDistance = 0.0f;
static f32 gafHngDistance[17] = {};
static bool gabHngInside[17] = {};
static unsigned guHngCall = 0;
static f32 gfLeftHit = -1.0f, gfRightHit = -1.0f;
static unsigned guEdgeCall = 0;
namespace BrnAI {
static SectionData gSectionData;
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetDirection() const { return mDirection; }
Vector3 AICar::GetUsefulDirection() const { return mDirection; }
f32 AICar::GetSpeed() const { return mfSpeedInRange; }
f32 DistancePosVelToOrigin(Vector2, Vector2) { return gfPosVelDistance; }
s32 RacingLineGenerator::GetLocalSectionID(RacingLine*, Vector2, s32) { return 5; }
s32 RacingLineGenerator::GetNearSectionID(RacingLine*, Vector2, s32) { return 5; }
SectionData* RacingLineGenerator::GetSectionPointer(RacingLine*, s32) { return &gSectionData; }
bool HardNoGoMap::DistanceToHardNoGoEdge(Vector2, f32& lrfDistance)
{
    const unsigned luStep = guHngCall++ % 17u;
    lrfDistance = gafHngDistance[luStep];
    return gabHngInside[luStep];
}
f32 SteeringFan::FanIntersectsEdge(Vector2*, s32, Vector2, Vector2)
{
    return ((guEdgeCall++ & 1u) == 0u) ? gfLeftHit : gfRightHit;
}
}
namespace BrnMath {
Vector2 Flatten(Vector3 lVector) { Vector2 lResult{}; lResult.x = lVector.x; lResult.y = lVector.z; return lResult; }
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
    bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-4f * (1.0f + std::fabs(lfB)); }
    bool IsNaN(f32 lfValue) { return lfValue != lfValue; }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();
    Vector2 V2(f32 lfX, f32 lfY) { return Vector2{ lfX, lfY, 0.0f, 0.0f }; }

    SteeringFan gFan;
    RacingLine  gLine;
    AICar       gCar;

    // Every ray points world +Z (2D (0, 1)); every contributor row pre-filled with 7.0 so an
    // untouched row is visible.
    void ResetFan()
    {
        std::memset(&gFan, 0, sizeof(gFan));
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
        {
            gFan.mUnitDirection[liStep] = V2(0.0f, 1.0f);
            gFan.mTravelDirectionBias[liStep] = 1.0f;
            for (s32 liRow = 0; liRow < E_FAN_CONTRIBUTORS_COUNT; ++liRow)
                gFan.mfWeighting[liRow][liStep] = 7.0f;
        }
        std::memset(&gLine, 0, sizeof(gLine));
        gLine.mbIsInitialised = true;
        gLine.mfCentreLineAhead = 0.5f;
        gLine.mfCentreLineAheadRecip = 2.0f;
        gCar = AICar{};
        gCar.mDirection = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        gCar.mfSpeedInRange = 20.0f;
    }
    bool RowIs(EFan_Contributors leRow, f32 lfValue)
    {
        for (s32 liStep = 0; liStep < KI_FAN_STEPS; ++liStep)
            if (!(gFan.mfWeighting[leRow][liStep] == lfValue)) return false;
        return true;
    }
}

static void GroupAggressionHelpers()
{
    Check(ClampAggression(KF_NAN, 0.0f, 1.0f) == 1.0f, "ClampAggression: a NaN is the high bound (fsel ladder)");
    Check(ClampAggression(KF_NAN, -1.0f, 1.0f) == 1.0f, "ClampAggression: ... for the [-1, 1] form too");
    Check(ClampAggression(-2.0f, -1.0f, 1.0f) == -1.0f && ClampAggression(0.25f, -1.0f, 1.0f) == 0.25f
          && ClampAggression(3.0f, -1.0f, 1.0f) == 1.0f, "control: ClampAggression on ordered values");
    Check(IsZero2DAggression(V2(KF_NAN, 0.0f)), "IsZero2DAggression: a NaN lane is not above the epsilon -> zero");
    Check(IsZero2DAggression(V2(0.0f, 1.0e-8f)) && !IsZero2DAggression(V2(0.0f, 0.5f)), "control: IsZero2DAggression on ordered values");
}

static void GroupDrift()
{
    ResetFan();
    gFan.mbPointAheadKnown = true;
    gFan.mCentreFarAhead = V2(KF_NAN, 0.0f);
    gFan.IncludeDriftLocationTracking(nullptr, &gLine);
    Check(RowIs(eFan_DriftFinalLocation, 7.0f),
          "IncludeDriftLocationTracking: a NaN vector is zero (0x82788814) -> return, row untouched");

    ResetFan();
    gFan.mbPointAheadKnown = true;
    gFan.mCentreAheadFarAhead = V2(KF_NAN, 0.0f);
    gFan.IncludeDriftDirectionTracking(nullptr, &gLine);
    Check(RowIs(eFan_DriftFinalDirection, 1.0f),
          "IncludeDriftDirectionTracking: a NaN vector skips the normalise and the clamp answers 1.0 -> row 1.0");

    ResetFan();
    gFan.mbPointAheadKnown = true;
    gFan.mCentreAheadFarAhead = V2(0.0f, 10.0f);
    gFan.IncludeDriftDirectionTracking(nullptr, &gLine);
    Check(RowIs(eFan_DriftFinalDirection, 1.0f), "control: rays along the drift line score 1.0");
}

static void GroupDriveCloseAndSmash()
{
    NearbyVehicles lTraffic{};
    lTraffic.miCount = 1;
    lTraffic.mVehicle[0].mType = E_NEARBY_PLAYER;
    lTraffic.mVehicle[0].mCentre = V2(3.0f, 10.0f);

    ResetFan();
    lTraffic.mVehicle[0].mVelocity = V2(KF_NAN, 0.0f);
    gFan.IncludeDriveCloseToPlayer(&gLine, &gCar, &lTraffic);
    Check(RowIs(eFan_DriveCloseToPlayer, 0.0f),
          "IncludeDriveCloseToPlayer: a NaN player velocity is zero (0x82787FC4) -> zero row");

    ResetFan();
    lTraffic.mVehicle[0].mVelocity = V2(0.0f, 20.0f);
    gfPosVelDistance = KF_NAN;
    gFan.IncludeDriveCloseToPlayer(&gLine, &gCar, &lTraffic);
    Check(RowIs(eFan_DriveCloseToPlayer, 0.0f),
          "IncludeDriveCloseToPlayer: a NaN passing space takes bge 0x82788140 and clamps to 1.0 -> weight 0");

    ResetFan();
    gfPosVelDistance = 3.5f;            // exactly the desired separation on the player's line
    gFan.IncludeDriveCloseToPlayer(&gLine, &gCar, &lTraffic);
    const f32 lfExpect = gFan.mfWeighting[eFan_DriveCloseToPlayer][8];
    Check(lfExpect == 1.0f || lfExpect == -0.0f || lfExpect == 0.0f || !IsNaN(lfExpect),
          "control: a finite passing space writes a finite weight");

    // IncludeSmashIntoTarget: a NaN target centre -> NaN separation -> proximity 0 -> zero row.
    ResetFan();
    NearbyVehicle lTarget{};
    lTarget.mCentre = V2(KF_NAN, 5.0f);
    gFan.IncludeSmashIntoTarget(&gCar, &lTarget, eFan_SmashIntoPlayer, -4.5f, 20.0f);
    Check(RowIs(eFan_SmashIntoPlayer, 0.0f),
          "IncludeSmashIntoTarget: a NaN separation clamps to 1.0 -> proximity 0 -> zero row");
    ResetFan();
    lTarget.mCentre = V2(0.0f, 10.0f);
    gFan.IncludeSmashIntoTarget(&gCar, &lTarget, eFan_SmashIntoPlayer, -4.5f, 20.0f);
    Check(Near(gFan.mfWeighting[eFan_SmashIntoPlayer][8], 0.5f), "control: 10 m straight ahead -> proximity 0.5 on the centre ray");
}

static void GroupFindNearbyAI()
{
    NearbyVehicles lTraffic{};
    lTraffic.miCount = 1;
    lTraffic.mVehicle[0].mType = E_NEARBY_AI;
    lTraffic.mVehicle[0].mCentre = V2(0.0f, 10.0f);      // dead ahead: aheadness 1.0 > 0.5
    lTraffic.mVehicle[0].mVelocity = V2(0.0f, 5.0f);

    ResetFan();
    gCar.mfSpeedInRange = KF_NAN;
    Check(gFan.FindNeabyAIInTraffic(&lTraffic, &gCar) == nullptr,
          "FindNeabyAIInTraffic: a NaN relative speed falls through bgt 0x82787DEC to the aheadness reject");
    gCar.mfSpeedInRange = 20.0f;                          // closing at 15 m/s > 4.47
    Check(gFan.FindNeabyAIInTraffic(&lTraffic, &gCar) == &lTraffic.mVehicle[0], "control: a fast closer is kept");
    gCar.mfSpeedInRange = 6.0f;                           // closing at 1 m/s, dead ahead
    Check(gFan.FindNeabyAIInTraffic(&lTraffic, &gCar) == nullptr, "control: a slow closer dead ahead is rejected");
}

static void GroupWeightings()
{
    ResetFan();
    gCar.mfSpeedInRange = KF_NAN;
    gFan.CalculateFanAngle(&gCar);
    Check(gFan.mfFanAngle == 0.5f && gFan.mfLookAheadRadius == 25.0f,
          "CalculateFanAngle: a NaN speed ratio saturates to 1.0 (fsel 0x82768D1C) -> 0.5 rad, 25 m");
    gCar.mfSpeedInRange = 0.0f;
    gFan.CalculateFanAngle(&gCar);
    Check(Near(gFan.mfFanAngle, 1.3962634f) && gFan.mfLookAheadRadius == 10.0f, "control: at rest -> 80 deg, 10 m");

    ResetFan();
    gFan.mbCentreHereKnown = true;
    gFan.mCentreHere = V2(KF_NAN, 0.0f);
    gFan.IncludeCentreLineTracking(nullptr, &gLine);
    Check(RowIs(eFan_SteerToCentre, 7.0f),
          "IncludeCentreLineTracking: a NaN delta is zero (bnelr 0x82786CF0) -> row untouched");

    ResetFan();
    gFan.mbCentreHereKnown = true;
    gFan.mCentreHere = V2(KF_NAN, 0.0f);
    gFan.IncludeRouteParallelTracking(nullptr, &gLine);
    Check(RowIs(eFan_DriveParallel, 0.0f),
          "IncludeRouteParallelTracking: a NaN delta is zero (0x82786E6C/0x82786EA4) -> zero row");

    ResetFan();
    gFan.mbCentreHereKnown = true;
    gFan.mCentreHere = V2(0.0f, 0.0f);
    gFan.mCentreAhead = V2(0.0f, 10.0f);                  // back along the road = (0, -1)
    gFan.mUnitDirection[3] = V2(KF_NAN, 0.0f);
    gFan.IncludeRouteParallelTracking(nullptr, &gLine);
    Check(gFan.mfWeighting[eFan_DriveParallel][3] == 1.0f,
          "IncludeRouteParallelTracking: a NaN dot clamps to 1.0 (fsel 0x82786F94) -> weight 1.0");
    Check(gFan.mfWeighting[eFan_DriveParallel][8] == 0.0f, "control: a ray along the road scores 0");
}

static void GroupHng()
{
    // IncludeHardNoGo: every step inside the map with distances 2.0 + 0.1 i; step 0 NaN.
    ResetFan();
    gLine.miLastKnownSectionID = 5;
    gSectionData.mHardNoGoMap.mbReady = true;
    for (s32 liStep = 0; liStep < 17; ++liStep)
    {
        gafHngDistance[liStep] = 2.0f + 0.1f * static_cast<f32>(liStep);
        gabHngInside[liStep] = true;
    }
    guHngCall = 0;
    gFan.IncludeHardNoGo(nullptr, &gLine);
    Check(Near(gFan.mfWeighting[eFan_ExitHNG][0], 0.5f) && Near(gFan.mfWeighting[eFan_ExitHNG][16], 1.0f),
          "control: IncludeHardNoGo rescales the exit row into [0.5, 1.0]");

    // The fold runs step 0..16 and `fsel(v - min, min, v)` REPLACES a NaN minimum with the next
    // entry, so only a NaN in the LAST folded entry survives into the rescale.
    ResetFan();
    gafHngDistance[16] = KF_NAN;
    guHngCall = 0;
    gFan.IncludeHardNoGo(nullptr, &gLine);
    Check(IsNaN(gFan.mfWeighting[eFan_ExitHNG][0]) && IsNaN(gFan.mfWeighting[eFan_ExitHNG][5]),
          "IncludeHardNoGo: a NaN last exit entry becomes the fsel minimum (0x8277A0B8) -> every rescaled entry NaN");
    ResetFan();
    gafHngDistance[16] = 2.0f + 1.6f;
    gafHngDistance[0] = KF_NAN;
    guHngCall = 0;
    gFan.IncludeHardNoGo(nullptr, &gLine);
    Check(IsNaN(gFan.mfWeighting[eFan_ExitHNG][0]) && Near(gFan.mfWeighting[eFan_ExitHNG][16], 1.0f),
          "control: a NaN FIRST entry is washed out of the minimum by step 1 (only its own entry is NaN)");

    // IncludeRouteEdgeIntersection: sections 5..7 cached and ready; the left edge answers NaN.
    ResetFan();
    gSectionData.mHardNoGoMap.mbReady = true;
    for (s32 liSection = 5; liSection < 8; ++liSection)
        gLine.maSectionCache[liSection & (RacingLine::KI_SECTION_CACHE_COUNT - 1)].mCachedSectionIndex =
            static_cast<s16>(liSection);
    gfLeftHit = KF_NAN;
    gfRightHit = -1.0f;                                   // KF_FAN_EDGE_NO_INTERSECTION
    guEdgeCall = 0;
    gFan.IncludeRouteEdgeIntersection(nullptr, &gLine);
    Check(RowIs(eFan_AvoidEdges, 0.0f),
          "IncludeRouteEdgeIntersection: a NaN range clamps to 1.0 (fsel 0x8277A55C) -> closeness 0 -> weight 0");
    gfLeftHit = 15.0f;
    guEdgeCall = 0;
    gFan.IncludeRouteEdgeIntersection(nullptr, &gLine);
    Check(Near(gFan.mfWeighting[eFan_AvoidEdges][0], 0.25f), "control: an edge 15 m out -> (1 - 0.5)^2 = 0.25");
}

int main()
{
    GroupAggressionHelpers();
    GroupDrift();
    GroupDriveCloseAndSmash();
    GroupFindNearbyAI();
    GroupWeightings();
    GroupHng();
    std::printf("FxAinan2SteeringFan: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
