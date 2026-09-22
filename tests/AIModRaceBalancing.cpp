// FX-AIMOD (crash parity 2026-09-22), G04-D4 + G05-D4: the race rubber-band chain.
//   RaceBalancingManager::UpdateOpponentRoute @0x82789C48 (export hole, ppcdis)  -- G04-D4
//   RaceBalancingManager::Update (inlined in AIModule::Update 0x8279B678..0x8279B6C0) -- G05-D4
//   RaceBalancingRoute::Prepare @0x82789368 first segment: Flatten(position) (vperm unk_82CDA450)
// The runner extracts the PRODUCTION bodies of the manager, route and graph from the real source
// files (restored_methods.inc). When a body is absent in the source under test (the pre-fix tree),
// the runner substitutes an empty stub so the numeric checks below report the missing behaviour
// instead of a link error. AICar / AISectionsData accessors are fixtures over the real headers.
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

static unsigned gAssertions = 0, gChecks = 0, gFailures = 0;
namespace CgsDev {
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcText, const char*, int) { ++gAssertions; std::fprintf(stderr, "assert: %s\n", lpcText); return 0; }
void* EndAssert() { return nullptr; }
} }

// ---- fixtures over the real AICar / AISectionsData declarations ------------------------------
namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
s8 AICar::GetOpponentIndex() const { return miOpponentIndex; }
bool AICar::IsPlayerCar() const { return mbIsPlayer; }
bool AICar::HasValidRoute() const { return GetRoute()->GetStatus() != Route::E_STATUS_UNINITIALISED && GetRoute()->GetNodeCount() > 0; }
s32 AICar::GetNextRouteNodeIndex() const { return miNextRouteNodeIndex; }
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
}

#include "restored_methods.inc"

using namespace BrnAI;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass) { ++gFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) < 1e-4f; }

static AICar gCar;
static AISection gaSections[2];
static AISectionsData gSections;
static RaceBalancingManager gManager;

static void BuildRoute(Route* lpRoute)
{
    // Three nodes along world Z at x == 0 (RouteNode mfX/mfY == world X/Z), 100 m apart; node[0]
    // carries the whole-route distance (Route::GetDistance == node[0].mfDistanceToCheckpoint).
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

int main()
{
    // Sections: speed class 1 with a 10..30 m/s band.
    gaSections[1].muSpeed = 1;
    gSections.mpaSections = gaSections;
    gSections.muNumSections = 2;
    for (s32 liClass = 0; liClass < 5; ++liClass)
    {
        gSections.mafSectionMinSpeeds[liClass] = 10.0f;
        gSections.mafSectionMaxSpeeds[liClass] = 30.0f;
    }

    // One opponent graph: AHEAD ratio 0.5 everywhere (20 m/s), BEHIND 1.0 everywhere (30 m/s).
    RaceBalancingGraph lGraph;
    lGraph.Construct();
    for (s32 liPoint = 0; liPoint < RaceBalancingGraph::KI_GRAPH_POINT_COUNT; ++liPoint)
    {
        lGraph.SetPoint(E_GRAPH_TYPE_AHEAD, liPoint, 0.5f);
        lGraph.SetPoint(E_GRAPH_TYPE_BEHIND, liPoint, 1.0f);
    }
    Array<RaceBalancingGraph, 7u> laGraphs;
    laGraphs.Construct();
    laGraphs.Append(lGraph);

    gManager.maRaceBalancingGraphs.MarkUnconstructed();
    gManager.maRaceBalancingRoutes.MarkUnconstructed();
    gManager.OnRaceStart(&laGraphs, 2, false);

    // The rival: opponent 0, in the mode, not the player; standing 10 m before node 0 in XZ and
    // 50 m UP (world Y) -- a height that must not enter the planar segment length.
    BuildRoute(gCar.GetRoute());
    gCar.miOpponentIndex = 0;
    gCar.mbIsPlayer = false;
    gCar.mbIsInGameMode = true;
    gCar.mPosition = Vector3{ 0.0f, 50.0f, 90.0f, 0.0f };
    gCar.miNextRouteNodeIndex = 1;

    // ---- G04-D4: UpdateOpponentRoute prepares the route (the only caller of Prepare) -----------
    gManager.UpdateOpponentRoute(&gCar, &gSections);
    const RaceBalancingRoute& lrRoute = gManager.maRaceBalancingRoutes[0u];
    Check(lrRoute.mbValid, "UpdateOpponentRoute: an invalid route is Prepared (mbValid set)");
    Check(lrRoute.miTimeCount == 3, "Prepare: one par time per route node");
    // First segment: |(0,90) - (0,100)| = 10 m at 20 m/s (AHEAD) / 30 m/s (BEHIND); then 100 m legs.
    Check(lrRoute.mbValid && Near(lrRoute.mafTimes[E_GRAPH_TYPE_AHEAD][0], 0.5f),
          "Prepare: first AHEAD segment starts at Flatten(position) == (x, z)");
    Check(lrRoute.mbValid && Near(lrRoute.mafTimes[E_GRAPH_TYPE_AHEAD][2], 10.5f), "Prepare: AHEAD par time at the last node");
    Check(lrRoute.mbValid && Near(lrRoute.mafTimes[E_GRAPH_TYPE_BEHIND][2], 7.0f), "Prepare: BEHIND par time at the last node");

    // A second response for the same rival re-derives the table backwards (Recalculate).
    gManager.UpdateOpponentRoute(&gCar, &gSections);
    Check(lrRoute.mbValid && Near(lrRoute.mafTimes[E_GRAPH_TYPE_AHEAD][1], 5.5f)
          && Near(lrRoute.mafTimes[E_GRAPH_TYPE_AHEAD][0], 0.5f),
          "UpdateOpponentRoute: a valid route is Recalculated from its last par time");

    // Gates: a one-node route and a manager out of race leave the table alone.
    {
        RaceBalancingRoute& lrMutable = gManager.maRaceBalancingRoutes[0u];
        lrMutable.mbValid = false;
        lrMutable.miTimeCount = 0;
        gCar.GetRoute()->miNodeCount = 1;
        gManager.UpdateOpponentRoute(&gCar, &gSections);
        Check(!lrMutable.mbValid && lrMutable.miTimeCount == 0, "UpdateOpponentRoute: node count <= 1 is skipped (0x82789CF8)");
        gCar.GetRoute()->miNodeCount = 3;
        gManager.mbInRace = false;
        gManager.UpdateOpponentRoute(&gCar, &gSections);
        Check(!lrMutable.mbValid, "UpdateOpponentRoute: nothing outside a race (0x82789C68)");
        gManager.mbInRace = true;
        gManager.UpdateOpponentRoute(&gCar, &gSections);
        Check(lrMutable.mbValid, "UpdateOpponentRoute: re-prepared once back in the race");
    }

    // ---- G05-D4: the race clock (RaceBalancingManager::Update) ---------------------------------
    AICar lPlayer{};
    lPlayer.mbIsCrashing = false;
    f32 lafOffsets[2] = { -1.0f, -1.0f };

    gManager.Update(&lPlayer, 0.25f);   // still on the start line: no time accrues
    gManager.CalculateScheduleOffset(&gCar, lafOffsets);
    Check(Near(lafOffsets[E_GRAPH_TYPE_AHEAD], 5.5f), "Update: the clock does not run on the start line (0x8279B68C)");

    gManager.OnRaceStartPlaying();
    gManager.Update(&lPlayer, 0.25f);
    gManager.CalculateScheduleOffset(&gCar, lafOffsets);
    Check(Near(gManager.mfRaceTime, 0.25f), "Update: the clock advances by dt once racing (0x8279B6BC)");
    Check(Near(lafOffsets[E_GRAPH_TYPE_AHEAD], 5.25f) && Near(lafOffsets[E_GRAPH_TYPE_BEHIND], 3.4166667f),
          "CalculateScheduleOffset reads the running clock");

    lPlayer.mbIsCrashing = true;
    gManager.Update(&lPlayer, 0.25f);
    Check(Near(gManager.mfRaceTime, 0.375f), "Update: a crashing player runs the clock at half speed (flt_820C4168 == 0.5)");

    gManager.OnRaceEnd();
    gManager.Update(&lPlayer, 1.0f);
    Check(Near(gManager.mfRaceTime, 0.375f), "Update: no time accrues outside a race (0x8279B680)");

    Check(gAssertions == 0, "valid fixtures raise no assertion");
    std::printf("AIModRaceBalancing: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
