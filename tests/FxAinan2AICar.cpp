// FX-AINAN2 regression (crash parity 2026-09-24): NaN branch polarity in the AICar bodies. The
// production bodies are extracted verbatim by run_fxainan2_aicar.py; the callees that are not
// under test are fixtures (accessors return their member, setters store it, the section / route
// helpers answer configured values).
//
// Console polarity (PowerPC: after fcmpu, ble/bge are TAKEN on unordered, blt/bgt are not;
// vcmpgtfp. + CR6 "all false" counts a NaN lane as "not above"):
//   UpdateRelativePositionToPlayer @0x8276FA88  ble 0x8276FB18 (NaN forward -> in front),
//       bgt 0x8276FB94 (in front, NaN facing -> 2), ble 0x8276FB4C (behind, NaN facing -> 1)
//   UpdateRaceDistance @0x8278AEB8               bgt 0x8278B05C: a NaN distance keeps the wrong-way
//       clock running
//   UpdateRouteFindingRace @0x82765E08           bltlr 0x82765E8C: a NaN timer requests the route
//   CheckForSectionChange @0x8277BFD0            blt 0x8277C088: a NaN dwell time drops the route
//   Update @0x82798F68                           bge 0x82799118 skips "mfDesiredSpeed >= 0.0f"
//   CheckForFreeRoamSwapToPursuit @0x8276EE68    bgelr 0x8276EE84: a NaN distance never swaps
//   UpdatePositionOutOfRange @0x8276F060         vcmpgtfp. 0x8276F194, all false: a NaN delta is
//       "on the node" -> advance, no move
//   IsFreeRoamingCarsRouteOld @0x8276FBD0        bge 0x8276FD10: a NaN distance is "not old"
//   ComputeDistanceToCheckpoint @0x8277C118      vcmpeqfp/vsel 0x8277C44C/0x8277C478 (only an exact
//       zero gap is 0, a NaN gap stays NaN) and bge 0x8277C494 skips "lfDistance >= 0.0f"
//   UpdateOutOfRangeData @0x8276F398             vcmpgtfp. 0x8276F550, all false: a NaN cross is
//       zero -> the Z axis
//   OnModeStart @0x8277BD20                      blt 0x8277BDF0 / ble 0x8277BDF8 skip the rank
//       ratio assert for a NaN
// Every NaN check fails on the pre-fix bodies (--rev <fix>~1); the ordered controls pass on both.
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

static f32  gfDesiredSpeed = 0.0f;
static bool gbSectionOK = false;
static Vector3 gMiddle{};

namespace BrnAI {
Vector3 AICar::GetPosition() const                 { return mPosition; }
Vector3 AICar::GetDirection() const                { return mDirection; }
Vector3 AICar::GetRight() const                    { return mRight; }
f32     AICar::GetSpeed() const                    { return mfSpeedInRange; }
void    AICar::SetDirection(Vector3 lDirection)    { mDirection = lDirection; }
void    AICar::SetRight(Vector3 lRight)            { mRight = lRight; }
f32     AICar::GetCurrentNodeY(AISectionsData*)    { return 0.0f; }
void    AICar::SetNextRouteNodeIndex(s32 liIndex)  { miNextRouteNodeIndex = liIndex; }
bool    AICar::CurrentSectionIsOK()                { return gbSectionOK; }
bool    AICar::MoveToSectionOnRoute(u16, const Route*) { return false; }
void    AICar::UpdateShortcut(AISectionsData*)     {}
void    AICar::UpdateRouteFinding(f32, const AICar*) {}
void    AICar::SetRoadRageMadness(f32)             {}
f32     AICar::CalcDesiredSpeed(const RaceBalancingManager*, AISectionsData*, const AICar*) { return gfDesiredSpeed; }
void    RaceBalancingManager::CalculateScheduleOffset(const AICar*, f32*) const {}
void Aggressiveness::SetAggression(f32 lfValue)                   { mfAggressionLevel = lfValue; mbAggressionLevelSet = true; }
void Aggressiveness::SetProximityToSpeedMatch(f32 lfValue)        { mfProximitySpeedMatch = lfValue; }
void Aggressiveness::SetTimeForSpeedMatch(f32 lfValue)            { mfTimeForSpeedMatch = lfValue; }
void Aggressiveness::SetRelativeSpeedForMatch(f32 lfValue)        { mfRelativeSpeedForSpeedMatch = lfValue; }
void Aggressiveness::SetAcclerationRateForSpeedMatch(f32 lfValue) { mfAcclerationRateForSpeedMatch = lfValue; }
static AISection gSection{};
const AISection* AISectionsData::GetAISection(u32) const { return &gSection; }
Vector3 AISection::GetMiddle() const { return gMiddle; }
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
    Vector3 V(f32 lfX, f32 lfY, f32 lfZ) { return Vector3{ lfX, lfY, lfZ, 0.0f }; }

    AICar gCar;
    AICar gPlayer;
    AISectionsData gSections{};

    // A rival IN_RANGE in a RACE, at the origin facing world +Z; the player 50 m ahead of it,
    // facing +Z. Neither has a route until a check builds one.
    void Reset()
    {
        gCar = AICar{};
        gPlayer = AICar{};
        gCar.meCarState = E_AI_CAR_STATE_IN_RANGE;
        gCar.meRouteFindingStyle = E_ROUTE_FINDING_RACE;
        gCar.mPosition = V(0.0f, 0.0f, 0.0f);
        gCar.mDirection = V(0.0f, 0.0f, 1.0f);
        gCar.mRight = V(1.0f, 0.0f, 0.0f);
        gCar.mfSpeedInRange = 30.0f;
        gCar.miOpponentIndex = 0;
        gCar.muDestinationSectionIndex = AICar::KI_INVALID_SECTION_INDEX;
        gCar.muBestSectionIndex = 1;
        gCar.muDefaultSectionIndex = 1;
        gPlayer.meCarState = E_AI_CAR_STATE_IN_RANGE;
        gPlayer.mPosition = V(0.0f, 0.0f, 50.0f);
        gPlayer.mDirection = V(0.0f, 0.0f, 1.0f);
        gPlayer.mRight = V(1.0f, 0.0f, 0.0f);
        gPlayer.mbIsPlayer = true;
        gfDesiredSpeed = 20.0f;
        gbSectionOK = false;
        gMiddle = V(0.0f, 0.0f, 0.0f);
    }

    // A route of liCount nodes along world +Z, 10 m apart, section 0, distance-to-checkpoint
    // counting down from the far end.
    void BuildRoute(AICar& lrCar, s32 liCount, Route::Status leStatus)
    {
        Route* lpRoute = lrCar.GetRoute();
        for (s32 li = 0; li < liCount; ++li)
        {
            Vector4 lNode{};
            lNode.x = 0.0f;
            lNode.y = 10.0f * static_cast<f32>(li);
            lNode.z = 10.0f * static_cast<f32>(liCount - 1 - li);
            lNode.w = 0.0f;
            lpRoute->maNodes[li] = lNode;
        }
        lpRoute->miNodeCount = liCount;
        lpRoute->meStatus = leStatus;
    }
}

// ---- UpdateRelativePositionToPlayer @0x8276FA88 ------------------------------------------------
static void GroupRelativePosition()
{
    Reset();
    gCar.mPosition = V(KF_NAN, 0.0f, 0.0f);
    gCar.UpdateRelativePositionToPlayer(&gPlayer);
    Check(gCar.meRelativeLocation == E_RELATIVE_INFRONT_SEPARATING,
          "UpdateRelativePositionToPlayer: a NaN forward separation takes ble 0x8276FB18 (in front), same facing -> 3");

    Reset();
    gCar.mPosition = V(0.0f, 0.0f, 60.0f);          // 10 m in front of the player
    gCar.mDirection = V(KF_NAN, 0.0f, 0.0f);
    gCar.UpdateRelativePositionToPlayer(&gPlayer);
    Check(gCar.meRelativeLocation == E_RELATIVE_INFRONT_APPROACHING,
          "UpdateRelativePositionToPlayer: in front with a NaN facing fails bgt 0x8276FB94 -> 2");

    Reset();
    gCar.mDirection = V(KF_NAN, 0.0f, 0.0f);        // 50 m behind the player
    gCar.UpdateRelativePositionToPlayer(&gPlayer);
    Check(gCar.meRelativeLocation == E_RELATIVE_BEHIND_SEPARATING,
          "UpdateRelativePositionToPlayer: behind with a NaN facing takes ble 0x8276FB4C -> 1");

    // Ordered controls.
    Reset();
    gCar.UpdateRelativePositionToPlayer(&gPlayer);
    Check(gCar.meRelativeLocation == E_RELATIVE_BEHIND_APPROACHING, "control: behind, same facing -> 0");
    gCar.mDirection = V(0.0f, 0.0f, -1.0f);
    gCar.UpdateRelativePositionToPlayer(&gPlayer);
    Check(gCar.meRelativeLocation == E_RELATIVE_BEHIND_SEPARATING, "control: behind, opposed facing -> 1");
    gCar.mPosition = V(0.0f, 0.0f, 60.0f);
    gCar.UpdateRelativePositionToPlayer(&gPlayer);
    Check(gCar.meRelativeLocation == E_RELATIVE_INFRONT_APPROACHING, "control: in front, opposed facing -> 2");
    gCar.mDirection = V(0.0f, 0.0f, 1.0f);
    gCar.UpdateRelativePositionToPlayer(&gPlayer);
    Check(gCar.meRelativeLocation == E_RELATIVE_INFRONT_SEPARATING, "control: in front, same facing -> 3");
}

// ---- UpdateRaceDistance @0x8278AEB8 + ComputeDistanceToCheckpoint @0x8277C118 -------------------
static void GroupRaceDistance()
{
    // A 3-node route, next node 1 at (0, 10) carrying a NaN distance-to-checkpoint: the distance
    // comes out NaN on both spellings.
    Reset();
    BuildRoute(gCar, 3, Route::E_STATUS_COMPLETE);
    gCar.GetRoute()->maNodes[1].z = KF_NAN;
    gCar.miNextRouteNodeIndex = 1;
    gCar.mbIsDrivenByPlayer = true;
    gCar.mbIsInGameMode = true;
    gCar.mfSpeedInRange = 1000.0f;
    gCar.mfDistanceToCheckpoint = 100.0f;
    gCar.mfWrongWayTime = 1.0f;
    unsigned luBefore = gAssertions;
    gCar.UpdateRaceDistance(&gSections, &gPlayer, 0.25f);
    Check(IsNaN(gCar.mfDistanceToCheckpoint), "UpdateRaceDistance: the NaN node distance comes through");
    Check(gAssertions == luBefore,
          "ComputeDistanceToCheckpoint: bge 0x8277C494 skips \"lfDistance >= 0.0f\" for the NaN");
    Check(Near(gCar.mfWrongWayTime, 1.25f),
          "UpdateRaceDistance: bgt 0x8278B05C does not reset on a NaN -- the wrong-way clock runs on");

    // Ordered controls: getting further from the checkpoint runs the clock, closing resets it.
    Reset();
    BuildRoute(gCar, 3, Route::E_STATUS_COMPLETE);
    gCar.miNextRouteNodeIndex = 1;
    gCar.mbIsDrivenByPlayer = true;
    gCar.mbIsInGameMode = true;
    gCar.mfSpeedInRange = 1000.0f;
    gCar.mPosition = V(0.0f, 0.0f, 5.0f);           // 5 m before node 1 -> distance 10 + 5
    gCar.mfDistanceToCheckpoint = 12.0f;
    gCar.mfWrongWayTime = 1.0f;
    luBefore = gAssertions;
    gCar.UpdateRaceDistance(&gSections, &gPlayer, 0.25f);
    Check(Near(gCar.mfDistanceToCheckpoint, 15.0f) && Near(gCar.mfWrongWayTime, 1.25f) && gAssertions == luBefore,
          "control: 12 -> 15 m (further away) runs the wrong-way clock");
    gCar.mfDistanceToCheckpoint = 20.0f;
    gCar.UpdateRaceDistance(&gSections, &gPlayer, 0.25f);
    Check(gCar.mfWrongWayTime == 0.0f, "control: 20 -> 15 m (closing) resets the wrong-way clock");

    // The PARTIAL route's gap to the destination: vcmpeqfp/vsel keeps a NaN gap NaN.
    Reset();
    BuildRoute(gCar, 3, Route::E_STATUS_PARTIAL);
    gCar.miNextRouteNodeIndex = 1;
    gCar.mPosition = V(0.0f, 0.0f, 5.0f);
    gCar.muDestinationSectionIndex = 7;
    gMiddle = V(KF_NAN, 0.0f, 0.0f);
    f32 lfDistance = 0.0f;
    luBefore = gAssertions;
    Check(gCar.ComputeDistanceToCheckpoint(&gSections, &gPlayer, &lfDistance) && IsNaN(lfDistance),
          "ComputeDistanceToCheckpoint: a NaN gap stays NaN (vsel 0x8277C478 selects 0 only on an exact zero)");
    Check(gAssertions == luBefore, "ComputeDistanceToCheckpoint: ... and the NaN result does not assert");
    gMiddle = V(3.0f, 0.0f, 24.0f);                 // last node (0, 20): gap 3-4-5
    Check(gCar.ComputeDistanceToCheckpoint(&gSections, &gPlayer, &lfDistance) && Near(lfDistance, 20.0f),
          "control: a 5 m gap adds 5 to 15");
}

// ---- UpdateRouteFindingRace @0x82765E08 / CheckForSectionChange @0x8277BFD0 ---------------------
static void GroupTimers()
{
    Reset();
    gCar.mbWantsAlternativeRoute = 1;
    gCar.mfAlternativeRouteTimer = 5.0f;
    gCar.UpdateRouteFindingRace(KF_NAN, &gPlayer);
    Check(gCar.mbRouteRequested, "UpdateRouteFindingRace: a NaN timer falls through bltlr 0x82765E8C and requests the route");

    Reset();
    gCar.mbWantsAlternativeRoute = 1;
    gCar.mfAlternativeRouteTimer = 5.0f;
    gCar.UpdateRouteFindingRace(1.0f, &gPlayer);
    Check(!gCar.mbRouteRequested, "control: 6 s of a 10 s hold does not request");
    gCar.mfAlternativeRouteTimer = 9.5f;
    gCar.UpdateRouteFindingRace(1.0f, &gPlayer);
    Check(gCar.mbRouteRequested, "control: 10.5 s of a 10 s hold requests");

    Reset();
    BuildRoute(gCar, 3, Route::E_STATUS_COMPLETE);
    gCar.mfTimeInInvalidSection = KF_NAN;
    gCar.CheckForSectionChange(0.1f, &gSections, gCar.GetRoute());
    Check(gCar.GetRoute()->GetNodeCount() == 0,
          "CheckForSectionChange: a NaN dwell time falls through blt 0x8277C088 and drops the route");

    Reset();
    BuildRoute(gCar, 3, Route::E_STATUS_COMPLETE);
    gCar.mfTimeInInvalidSection = 0.2f;
    gCar.CheckForSectionChange(0.1f, &gSections, gCar.GetRoute());
    Check(gCar.GetRoute()->GetNodeCount() == 3, "control: 0.3 s off-route keeps the route");
    gCar.mfTimeInInvalidSection = 0.95f;
    gCar.CheckForSectionChange(0.1f, &gSections, gCar.GetRoute());
    Check(gCar.GetRoute()->GetNodeCount() == 0, "control: 1.05 s off-route drops it");
}

// ---- Update @0x82798F68 / CheckForFreeRoamSwapToPursuit @0x8276EE68 -----------------------------
static void GroupUpdateAndSwap()
{
    Reset();
    gCar.meRouteFindingStyle = E_ROUTE_FINDING_FREE_ROAM;
    gCar.mPosition = V(0.0f, 0.0f, 0.0f);
    gPlayer.mPosition = V(0.0f, 0.0f, 1000.0f);
    gfDesiredSpeed = KF_NAN;
    unsigned luBefore = gAssertions;
    gCar.Update(nullptr, 0.033f, &gPlayer, &gSections, gCar.GetRoute(), false);
    Check(gAssertions == luBefore, "Update: a NaN desired speed skips \"mfDesiredSpeed >= 0.0f\" (bge 0x82799118)");

    Reset();
    gCar.meRouteFindingStyle = E_ROUTE_FINDING_FREE_ROAM;
    gPlayer.mPosition = V(0.0f, 0.0f, 1000.0f);
    gfDesiredSpeed = -1.0f;
    luBefore = gAssertions;
    gCar.Update(nullptr, 0.033f, &gPlayer, &gSections, gCar.GetRoute(), false);
    Check(gAssertions == luBefore + 1, "control: a negative desired speed fires it once");

    Reset();
    gCar.meRouteFindingStyle = E_ROUTE_FINDING_FREE_ROAM;
    gCar.mfBuzzDistanceToPlayer = KF_NAN;
    gCar.miProximityIndex = 0;
    gCar.CheckForFreeRoamSwapToPursuit();
    Check(gCar.meRouteFindingStyle == E_ROUTE_FINDING_FREE_ROAM,
          "CheckForFreeRoamSwapToPursuit: a NaN distance returns at bgelr 0x8276EE84 -- no swap");
    Reset();
    gCar.meRouteFindingStyle = E_ROUTE_FINDING_FREE_ROAM;
    gCar.miProximityIndex = 0;
    gCar.mfBuzzDistanceToPlayer = 150.0f;
    gCar.CheckForFreeRoamSwapToPursuit();
    Check(gCar.meRouteFindingStyle == E_ROUTE_FINDING_FREE_ROAM, "control: 150 m away stays free-roaming");
    gCar.mfBuzzDistanceToPlayer = 50.0f;
    gCar.CheckForFreeRoamSwapToPursuit();
    Check(gCar.meRouteFindingStyle == E_ROUTE_FINDING_PURSUIT, "control: 50 m away swaps to pursuit");
}

// ---- UpdatePositionOutOfRange @0x8276F060 / IsFreeRoamingCarsRouteOld @0x8276FBD0 ---------------
static void GroupOutOfRange()
{
    Reset();
    BuildRoute(gCar, 3, Route::E_STATUS_COMPLETE);
    gCar.meCarState = E_AI_CAR_STATE_OUT_OF_RANGE;
    gCar.miNextRouteNodeIndex = 0;
    gCar.mPosition = V(KF_NAN, 0.0f, 0.0f);
    gCar.mfSpeedOutOfRange = 10.0f;
    gCar.UpdatePositionOutOfRange(0.1f, &gSections);
    Check(gCar.miNextRouteNodeIndex == 1 && gCar.mDirection.z == 1.0f,
          "UpdatePositionOutOfRange: a NaN delta is zero to vcmpgtfp. 0x8276F194 -> advance, no move");

    Reset();
    BuildRoute(gCar, 3, Route::E_STATUS_COMPLETE);
    gCar.meCarState = E_AI_CAR_STATE_OUT_OF_RANGE;
    gCar.miNextRouteNodeIndex = 1;                   // node (0, 10); the car at the origin
    gCar.mfSpeedOutOfRange = 10.0f;
    gCar.UpdatePositionOutOfRange(0.1f, &gSections);
    Check(gCar.miNextRouteNodeIndex == 1 && Near(gCar.mPosition.z, 1.0f), "control: 10 m short moves 1 m, no advance");
    gCar.mPosition = V(0.0f, 0.0f, 10.0f);
    gCar.UpdatePositionOutOfRange(0.1f, &gSections);
    Check(gCar.miNextRouteNodeIndex == 2, "control: on the node advances");

    Reset();
    BuildRoute(gCar, 3, Route::E_STATUS_COMPLETE);
    gCar.muDestinationSectionIndex = 9;
    gCar.mPosition = V(KF_NAN, 0.0f, 0.0f);
    Check(!gCar.IsFreeRoamingCarsRouteOld() && gCar.muDestinationSectionIndex == 9,
          "IsFreeRoamingCarsRouteOld: a NaN distance takes bge 0x8276FD10 -- not old, destination kept");
    gCar.muDestinationSectionIndex = 9;
    gCar.mPosition = V(0.0f, 0.0f, -500.0f);
    Check(!gCar.IsFreeRoamingCarsRouteOld() && gCar.muDestinationSectionIndex == 9, "control: 520 m from the end is not old");
    gCar.mPosition = V(0.0f, 0.0f, -100.0f);
    Check(gCar.IsFreeRoamingCarsRouteOld() && gCar.muDestinationSectionIndex == AICar::KI_INVALID_SECTION_INDEX,
          "control: 120 m from the end is old and drops the destination");
}

// ---- UpdateOutOfRangeData @0x8276F398 / OnModeStart @0x8277BD20 ---------------------------------
static void GroupDataAndModeStart()
{
    Reset();
    gCar.UpdateOutOfRangeData(V(0.0f, 0.0f, 0.0f), V(KF_NAN, 0.0f, 0.0f), 3, 65);
    Check(gCar.mRight.x == 0.0f && gCar.mRight.y == 0.0f && gCar.mRight.z == 1.0f,
          "UpdateOutOfRangeData: a NaN cross is zero to vcmpgtfp. 0x8276F550 -> the Z axis");
    gCar.UpdateOutOfRangeData(V(0.0f, 0.0f, 0.0f), V(1.0f, 0.0f, 0.0f), 3, 65);
    Check(Near(gCar.mRight.z, -1.0f) && Near(gCar.mRight.x, 0.0f), "control: at +X -> right = Y x X = -Z");
    gCar.UpdateOutOfRangeData(V(0.0f, 0.0f, 0.0f), V(0.0f, 1.0f, 0.0f), 3, 65);
    Check(gCar.mRight.z == 1.0f, "control: at +Y has a zero cross -> the Z axis");

    Reset();
    unsigned luBefore = gAssertions;
    gCar.OnModeStart(static_cast<EAISpeedSelectionMethod>(1), 0, E_ROUTE_FINDING_RACE, false, false,
                     AICar::KI_INVALID_SECTION_INDEX, KF_NAN);
    Check(gAssertions == luBefore, "OnModeStart: a NaN rank ratio skips the assert (blt 0x8277BDF0 / ble 0x8277BDF8)");
    luBefore = gAssertions;
    gCar.OnModeStart(static_cast<EAISpeedSelectionMethod>(1), 0, E_ROUTE_FINDING_RACE, false, false,
                     AICar::KI_INVALID_SECTION_INDEX, 0.5f);
    Check(gAssertions == luBefore, "control: rank 0.5 raises no assertion");
    gCar.OnModeStart(static_cast<EAISpeedSelectionMethod>(1), 0, E_ROUTE_FINDING_RACE, false, false,
                     AICar::KI_INVALID_SECTION_INDEX, 1.5f);
    Check(gAssertions == luBefore + 1, "control: rank 1.5 fires the assert once");
}

int main()
{
    GroupRelativePosition();
    GroupRaceDistance();
    GroupTimers();
    GroupUpdateAndSwap();
    GroupOutOfRange();
    GroupDataAndModeStart();
    std::printf("FxAinan2AICar: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
