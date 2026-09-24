// FX-TAILS-A item 6 (crash parity 2026-09-24): AStar::BuildRoute @0x8277F930 normalises the route's last segment
// the console's way -- NO zero guard. run_fxtailsa_astar_normalise.py extracts the production AStar::BuildRoute,
// AStarNodePool::GetNode, AStar::KF_ZERO_EPSILON and Route::AddNode (BrnRoute.cpp @0x827642A0) verbatim.
//
// Console, the extrapolated exit node (step 3 of BuildRoute):
//   0x8277FB68..0x8277FBB4  travel v124 = (last - prev) * y2, y2 = vrsqrtefp(lenSq) + two Newton-Raphson steps
//                           (vnmsubfp e = 1 - lenSq*y*y, vmaddfp y' = (y*0.5)*e + y, fused), NO vcmpeqfp/vsel
//   0x8277FBDC / 0x8277FBE4 best dot seeded -2.0 (flt_820C4358 = 0xC0000000), best portal r27 = r21 = 0 (li @0x8277F998)
//   0x8277FC6C..0x8277FCF4  per portal: skip an offset that is zero in both lanes (|d| > flt_820C3B70 per lane)
//   0x8277FCF8..0x8277FD50  normalise the offset (same unguarded chain), dot with v124 (lane 0 + lane 1)
//   0x8277FD5C / 0x8277FD60 `fcmpu dot, best ; ble -> skip`: an unordered dot never replaces the best
// For a zero-length last segment vrsqrtefp(+0) = +inf, 0 * inf = NaN, v124 is NaN, every dot is NaN and the exit
// stays portal 0. The old body's invented `lfDirLen != 0` made travel (0, 0), every dot 0.0 > -2.0, and the exit
// became the first portal the zero-offset gate let through.
// Route::AddNode de-dups a node equal to the last one, so a built route never has two EQUAL last nodes; a route
// that already holds them (case 1) and a last segment whose squared length underflows to 0 (case 2, reachable
// through AddNode) are the two ways in. Neither occurs with the shipped AI data (FX-AINAN2 EQUIV-UNREACHABLE).
#include "GameSource/World/AI/Route/BrnAStar.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace PerfMonCpu {
void StartMonitor(s32) {}
void StopMonitor(s32) {}
}
}

namespace BrnAI {
const AISection* AISectionsData::GetAISection(u32 luIndex) const { return &mpaSections[luIndex]; }
const Portal* AISection::GetPortal(u8 luPortalIndex) const { return &mpaPortals[luPortalIndex]; }
f32 Portal::GetPositionX() const { return mPositionX; }
f32 Portal::GetPositionY() const { return mPositionY; }
f32 Portal::GetPositionZ() const { return mPositionZ; }
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
        if (!lbPass) { ++guFailures; }
    }

    alignas(16) unsigned char gaAStar[sizeof(AStar)];
    alignas(16) unsigned char gaRoute[sizeof(Route)];
    AStar& TheAStar() { return *reinterpret_cast<AStar*>(gaAStar); }
    Route& TheRoute() { return *reinterpret_cast<Route*>(gaRoute); }

    AISection      gSection;
    Portal         gaPortals[2];
    AISectionsData gData;

    Vector4 Node(f32 lfX, f32 lfY)
    {
        Vector4 lNode;
        lNode.x = lfX; lNode.y = lfY; lNode.z = 0.0f; lNode.w = 0.0f;
        return lNode;
    }

    // One section with two portals (x, z on the ground plane); the A* best node is the search's start node (no
    // parent), so the trace emits nothing and step 3 extrapolates from the route's own last two nodes.
    AStarNode* World(f32 lfPortal0X, f32 lfPortal0Z, f32 lfPortal1X, f32 lfPortal1Z)
    {
        std::memset(gaAStar, 0, sizeof(gaAStar));
        std::memset(gaRoute, 0, sizeof(gaRoute));
        std::memset(&gSection, 0, sizeof(gSection));
        std::memset(gaPortals, 0, sizeof(gaPortals));
        gaPortals[0].mPositionX = lfPortal0X; gaPortals[0].mPositionZ = lfPortal0Z;
        gaPortals[1].mPositionX = lfPortal1X; gaPortals[1].mPositionZ = lfPortal1Z;
        gSection.mpaPortals = gaPortals;
        gSection.mu8NumPortals = 2;
        gData.mpaSections = &gSection;
        gData.muNumSections = 1;
        TheAStar().mpAISectionsData = &gData;
        TheAStar().mAStarNodePool.mauNodeCount[0] = 1;
        AStarNode* lpBest = TheAStar().mAStarNodePool.GetNode(0);
        lpBest->Construct(AStarVector2(0.0f, 0.0f), 0, 0, 0);   // section 0, no parent
        gAssertions = 0;
        return lpBest;
    }
    u8 PortalTag(const Vector4& lrNode) { return reinterpret_cast<const u8*>(&lrNode.w)[2]; }
}

int main()
{
    // ---- case 1: the last two nodes coincide (a route that already holds them) -----------------------------------
    {
        AStarNode* lpBest = World(10.0f, 20.0f, 15.0f, 20.0f);   // portal 0 ON the last node, portal 1 5 m along +x
        TheRoute().maNodes[0] = Node(10.0f, 20.0f);
        TheRoute().maNodes[1] = Node(10.0f, 20.0f);
        TheRoute().miNodeCount = 2;
        TheAStar().BuildRoute(lpBest, &TheRoute());
        Check(TheRoute().miNodeCount == 2,
              "zero last segment: NaN travel, no dot beats -2.0, exit = portal 0 = the last node -> AddNode de-dups it "
              "(the old (0, 0) travel appended portal 1)");
        Check(gAssertions == 0, "zero last segment: no assertion");
    }

    // ---- case 2: a last segment whose squared length underflows to 0 (reachable through Route::AddNode) ----------
    {
        AStarNode* lpBest = World(1.0e-30f, 0.0f, 5.0f, 0.0f);
        TheRoute().AddNode(Node(0.0f, 0.0f));
        TheRoute().AddNode(Node(1.0e-30f, 0.0f));               // not equal -> kept
        Check(TheRoute().miNodeCount == 2, "setup: AddNode keeps a 1e-30 m step");
        TheAStar().BuildRoute(lpBest, &TheRoute());
        Check(TheRoute().miNodeCount == 2,
              "1e-30 m last segment: lenSq 1e-60 is +0, the travel is unordered, exit = portal 0 (de-duped) -- "
              "the old guard's (0, 0) travel appended portal 1");
    }

    // ---- control: an ordinary last segment picks the best-aligned portal on both spellings ------------------------
    {
        AStarNode* lpBest = World(10.0f, 30.0f, 20.0f, 0.0f);     // travel +x: portal 0 at 90 deg, portal 1 dead ahead
        TheRoute().AddNode(Node(0.0f, 0.0f));
        TheRoute().AddNode(Node(10.0f, 0.0f));
        TheAStar().BuildRoute(lpBest, &TheRoute());
        const Vector4& lrLast = TheRoute().maNodes[TheRoute().miNodeCount - 1];
        Check(TheRoute().miNodeCount == 3 && lrLast.x == 20.0f && lrLast.y == 0.0f && PortalTag(lrLast) == 1,
              "control: travel +x -> exit node at portal 1 (20, 0), tagged portal 1");
    }

    std::printf("FxTailsAAStarNormalise: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
