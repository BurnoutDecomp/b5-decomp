// FX-AINAN2 regression (crash parity 2026-09-24): NaN polarity of the race rubber-band clamps. The
// runner extracts the PRODUCTION RaceBalancingRoute / RaceBalancingGraph / RaceBalancingManager
// bodies; AICar / AISectionsData accessors are fixtures over the real headers.
//
// The console inlines rw::math::fpu::Clamp<float> as a two-fsel ladder, and fsel d,a,b,c
// (= a >= 0 ? b : c) takes its THIRD operand on an unordered test, so a NaN comes back as the
// upper bound; the old `if (x < lo) ..; if (x > hi) ..` spelling kept the NaN:
//   RaceBalancingRoute::ComputeRaceCompletionRatio @0x8277AD98  ratio fsel 0x8277ADD0/0x8277AE04,
//       result fsel 0x8277AE10/0x8277AE18 (and the same pair inlined in Prepare @0x82789368 /
//       Recalculate @0x82789708)
//   RaceBalancingGraph::ComputeSpeedRatio @0x8277B748            segment fsel 0x8277B8A8/0x8277B8B4
//   RaceBalancingManager::ComputeTargetSpeed @0x827916E0         multiplier fsel 0x827917C0 (in
//       range) / 0x827917D8 (out of range) then 0x827917E4
// Every NaN check fails on the pre-fix bodies (--rev <fix>~1); the ordered controls pass on both.
#define _ALLOW_KEYWORD_MACROS 1
#define private public
#define protected public
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingGraph.h"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#undef protected
#undef private
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
s8 AICar::GetOpponentIndex() const { return miOpponentIndex; }
bool AICar::IsPlayerCar() const { return mbIsPlayer; }
bool AICar::HasValidRoute() const { return GetRoute()->GetStatus() != Route::E_STATUS_UNINITIALISED && GetRoute()->GetNodeCount() > 0; }
s32 AICar::GetNextRouteNodeIndex() const { return miNextRouteNodeIndex; }
const RouteNode* AICar::GetNextRouteNode() const { return GetRoute()->GetNode(miNextRouteNodeIndex); }
EAICarState AICar::GetState() const { return meCarState; }
bool AICar::IsAheadOfPlayer() const { return mbIsAheadOfPlayer; }
f32 AICar::GetMaxPlayerSpeed() const { return mfMaxPlayerSpeed; }
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
}

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::printf("FAIL %s\n", lpcName); }
}
static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) < 1e-4f; }
static bool IsNaN(f32 lfValue) { return lfValue != lfValue; }
static const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

static AICar gCar;
static AISection gaSections[2];
static AISectionsData gSections;
static RaceBalancingManager gManager;

// Three nodes along world Z at x == 0, 100 m apart, section 1; node[0] carries the whole-route
// distance (Route::GetDistance == node[0].mfDistanceToCheckpoint).
static void BuildRoute(Route* lpRoute)
{
    std::memset(lpRoute, 0, sizeof(Route));
    const f32 lafZ[3] = { 100.0f, 200.0f, 300.0f };
    const f32 lafDist[3] = { 200.0f, 100.0f, 0.0f };
    for (s32 liNode = 0; liNode < 3; ++liNode)
    {
        RouteNode* lpNode = reinterpret_cast<RouteNode*>(&lpRoute->maNodes[liNode]);
        lpNode->mfX = 0.0f;
        lpNode->mfY = lafZ[liNode];
        lpNode->mfDistanceToCheckpoint = lafDist[liNode];
        lpNode->muSectionIndex = 1;
    }
    lpRoute->miNodeCount = 3;
    lpRoute->meStatus = Route::E_STATUS_COMPLETE;
}

static RaceBalancingGraph MakeGraph()
{
    // AHEAD ratios 0.1, 0.2, ..., 0.8 (distinct per point); BEHIND 1.0 everywhere.
    RaceBalancingGraph lGraph;
    lGraph.Construct();
    for (s32 liPoint = 0; liPoint < RaceBalancingGraph::KI_GRAPH_POINT_COUNT; ++liPoint)
    {
        lGraph.SetPoint(E_GRAPH_TYPE_AHEAD, liPoint, 0.1f * static_cast<f32>(liPoint + 1));
        lGraph.SetPoint(E_GRAPH_TYPE_BEHIND, liPoint, 1.0f);
    }
    return lGraph;
}

static void GroupCompletionRatio()
{
    RaceBalancingRoute lRoute{};
    lRoute.mfDistance = 200.0f;
    lRoute.miCurrentCheckpointIndex = 0;
    Check(Near(lRoute.ComputeRaceCompletionRatio(KF_NAN, 2), 0.5f),
          "ComputeRaceCompletionRatio: a NaN distance clamps to 1.0 (fsel 0x8277AE04) -> the checkpoint's end, 0.5");
    Check(Near(lRoute.ComputeRaceCompletionRatio(50.0f, 0), 1.0f),
          "ComputeRaceCompletionRatio: a checkpoint count of 0 (0/0) comes back 1.0 (fsel 0x8277AE18)");
    // Ordered controls.
    Check(Near(lRoute.ComputeRaceCompletionRatio(150.0f, 2), 0.125f), "control: a quarter of checkpoint 0 of 2 -> 0.125");
    Check(Near(lRoute.ComputeRaceCompletionRatio(250.0f, 2), 0.0f), "control: behind the segment start floors at 0");
    Check(Near(lRoute.ComputeRaceCompletionRatio(-100.0f, 2), 0.5f), "control: past the segment end caps at the checkpoint's end");
    lRoute.miCurrentCheckpointIndex = 3;
    Check(Near(lRoute.ComputeRaceCompletionRatio(0.0f, 2), 1.0f), "control: a past-the-end checkpoint index caps at 1.0");
    lRoute.mfDistance = 0.0f;
    Check(lRoute.ComputeRaceCompletionRatio(KF_NAN, 2) == 0.0f, "control: a zero-length route answers 0 (beq 0x8277ADA8)");
}

static void GroupSpeedRatio()
{
    const RaceBalancingGraph lGraph = MakeGraph();
    const f32 lfRatio = lGraph.ComputeSpeedRatio(E_GRAPH_TYPE_AHEAD, KF_NAN);
    Check(!IsNaN(lfRatio) && Near(lfRatio, 0.1f),
          "ComputeSpeedRatio: a NaN fraction clamps the segment to 1.0 (fsel 0x8277B8B4) -> point 0's ratio, not NaN");
    Check(Near(lGraph.ComputeSpeedRatio(E_GRAPH_TYPE_AHEAD, 0.5f), 0.4f + 0.5f * 0.1f), "control: fraction 0.5 lerps points 3..4");
    Check(Near(lGraph.ComputeSpeedRatio(E_GRAPH_TYPE_AHEAD, 1.0f), 0.8f), "control: fraction 1.0 is the last point");
}

static void GroupTargetSpeedAndPrepare()
{
    gaSections[1].muSpeed = 1;
    gSections.mpaSections = gaSections;
    gSections.muNumSections = 2;
    for (s32 liClass = 0; liClass < 5; ++liClass)
    {
        gSections.mafSectionMinSpeeds[liClass] = 10.0f;
        gSections.mafSectionMaxSpeeds[liClass] = 30.0f;
    }
    Array<RaceBalancingGraph, 7u> laGraphs;
    laGraphs.Construct();
    laGraphs.Append(MakeGraph());
    gManager.maRaceBalancingGraphs.MarkUnconstructed();
    gManager.maRaceBalancingRoutes.MarkUnconstructed();
    gManager.OnRaceStart(&laGraphs, 2, false);

    BuildRoute(gCar.GetRoute());
    gCar.miOpponentIndex = 0;
    gCar.mbIsPlayer = false;
    gCar.meCarState = E_AI_CAR_STATE_IN_RANGE;
    gCar.mfMaxPlayerSpeed = 1000.0f;
    gCar.mPosition = Vector3{ 0.0f, 0.0f, 90.0f, 0.0f };
    gCar.miNextRouteNodeIndex = 1;

    RaceBalancingRoute& lrRoute = gManager.maRaceBalancingRoutes[0u];
    lrRoute.Prepare(gCar.GetPosition(), &gSections, &gManager.maRaceBalancingGraphs[0u], gCar.GetRoute(), 2);
    Check(lrRoute.mbValid && !IsNaN(lrRoute.mafTimes[E_GRAPH_TYPE_AHEAD][2]), "setup: the route is prepared");

    const f32 lfPar = gManager.ComputeParSpeed(E_GRAPH_TYPE_AHEAD, &gCar, &gSections);
    const f32 lfParTime = lrRoute.GetTime(E_GRAPH_TYPE_AHEAD, 1);
    gManager.mfRaceTime = KF_NAN;
    Check(Near(gManager.ComputeTargetSpeed(E_GRAPH_TYPE_AHEAD, &gCar, &gSections), lfPar * 1.1f),
          "ComputeTargetSpeed: a NaN multiplier is the in-range max 1.1 (fsel 0x827917E4)");
    gCar.meCarState = E_AI_CAR_STATE_OUT_OF_RANGE;
    gCar.mbIsAheadOfPlayer = false;
    Check(Near(gManager.ComputeTargetSpeed(E_GRAPH_TYPE_AHEAD, &gCar, &gSections), lfPar * 1.2f),
          "ComputeTargetSpeed: ... and the out-of-range max 1.2 (fsel 0x827917D8 / 0x827917E4)");
    // Ordered controls.
    gCar.meCarState = E_AI_CAR_STATE_IN_RANGE;
    gManager.mfRaceTime = lfParTime;
    Check(Near(gManager.ComputeTargetSpeed(E_GRAPH_TYPE_AHEAD, &gCar, &gSections), lfPar), "control: on schedule -> par");
    gManager.mfRaceTime = lfParTime + 100.0f;
    Check(Near(gManager.ComputeTargetSpeed(E_GRAPH_TYPE_AHEAD, &gCar, &gSections), lfPar * 1.1f), "control: far behind schedule -> 1.1");
    gManager.mfRaceTime = lfParTime - 100.0f;
    Check(Near(gManager.ComputeTargetSpeed(E_GRAPH_TYPE_AHEAD, &gCar, &gSections), lfPar * 0.9f), "control: far ahead of schedule -> 0.9");

    // Prepare over a node whose distance-to-checkpoint is NaN: the inlined completion ratio clamps
    // it to 1.0, so the par times stay finite on the console.
    BuildRoute(gCar.GetRoute());
    reinterpret_cast<RouteNode*>(&gCar.GetRoute()->maNodes[1])->mfDistanceToCheckpoint = KF_NAN;
    RaceBalancingRoute lFresh{};
    lFresh.Prepare(gCar.GetPosition(), &gSections, &gManager.maRaceBalancingGraphs[0u], gCar.GetRoute(), 2);
    Check(!IsNaN(lFresh.mafTimes[E_GRAPH_TYPE_AHEAD][1]) && !IsNaN(lFresh.mafTimes[E_GRAPH_TYPE_AHEAD][2]),
          "Prepare: a NaN node distance keeps the par times finite (the ratio ladder answers 1.0)");
}

int main()
{
    GroupCompletionRatio();
    GroupSpeedRatio();
    GroupTargetSpeedAndPrepare();
    std::printf("FxAinan2RaceBalancing: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
