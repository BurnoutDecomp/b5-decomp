// FX-SCENEMGR (crash parity 2026-09-24, item 3b): BaseCollisionGenerator::CollideLineAgainstPolySoupList
// (ARTIST 0x82812AE0, absent before), extracted VERBATIM by run_fxscenemgr_collide_line_all_hits.py with the
// file's own KF_SHORT_LINE_LENGTH_SQ / LeafOverlapsBoxXYZ. Its two Geometric callees are not in the tree
// (IntersectLinePolygonSoupSingleSided @0x8283C598, PolygonSoupListSpatialMap::RunQuery(const Line&)
// @0x82843E98), so the driver must: claim the list with (max, tagA, tagB); split at 400 > |d|^2 (flt_8200889C);
// on the short arm gather leaves through RunQuery(box of min/max lanes), skip non-overlapping leaves and TRAP
// once per overlapping leaf at the kernel call; TRAP once on the long arm; write the count (0) and return the
// list index. The real Line::IsValid (CgsLine.cpp) is compiled alongside.
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

static std::vector<std::string> gaAsserts;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
}
}
static int CountAsserts(const char* lpcNeedle)
{
    int n = 0;
    for (const std::string& s : gaAsserts) if (s.find(lpcNeedle) != std::string::npos) ++n;
    return n;
}

namespace CgsGeometric {
struct PolygonSoupListSpatialMap
{
    int miRunQueries = 0;
    AxisAlignedBox mLastBox = {};
    std::vector<u16> maOutput;
    std::vector<PolygonSoupLeafNode> maLeaves;
    s32 RunQuery(const AxisAlignedBox& lrBox) { ++miRunQueries; mLastBox = lrBox; return static_cast<s32>(maOutput.size()); }
    const u16* GetOutputQueryBuffer() const { return maOutput.data(); }
    const PolygonSoupLeafNode* GetLeafNodes() const { return maLeaves.data(); }
};
}

namespace CgsSceneManager {
namespace CgsCollision {
namespace {
#include "fxsm_cla_helpers.inc"   // KF_SHORT_LINE_LENGTH_SQ + LeafOverlapsBoxXYZ, from the source
}
struct CollisionResult { u8 maRaw[80]; };
struct CollisionResultList { CollisionResult* mpResults; u32 mu32UserTagA; u16 mu16UserTagB; u16 mu16MaxNumResults; u16 mu16NumResults; u8 meResultType; };
struct BaseCollisionGenerator
{
    CollisionResultList maLists[8];
    CollisionResultList* mapCollisionResultLists[8];
    int miPrepares = 0; u16 mu16PrepMax = 0; u32 muPrepTagA = 0; u16 mu16PrepTagB = 0;
    BaseCollisionGenerator()
    {
        for (int i = 0; i < 8; ++i) { std::memset(&maLists[i], 0, sizeof(maLists[i])); maLists[i].mu16NumResults = 0xBEEF; mapCollisionResultLists[i] = &maLists[i]; }
    }
    s32 PrepareNewPrimitiveTestResultsList(u16 lu16Max, u32 luTagA, u16 lu16TagB)
    {
        ++miPrepares; mu16PrepMax = lu16Max; muPrepTagA = luTagA; mu16PrepTagB = lu16TagB;
        return 5;
    }
    u16 CollideLineAgainstPolySoupList(const CgsGeometric::Line& lrLine, CgsGeometric::PolygonSoupListSpatialMap* lpMap,
                                       u16 lu16MaxNumResults, u32 lu32UserTagA, u16 lu16UserTagB);
};
#include "fxsm_cla_body.inc"
}
}

using CgsSceneManager::CgsCollision::BaseCollisionGenerator;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static CgsGeometric::Line MakeLine(f32 x0, f32 y0, f32 z0, f32 w0, f32 x1, f32 y1, f32 z1, f32 w1)
{
    CgsGeometric::Line l;
    l.mStart.x = x0; l.mStart.y = y0; l.mStart.z = z0; l.mStart.w = w0;
    l.mEnd.x = x1; l.mEnd.y = y1; l.mEnd.z = z1; l.mEnd.w = w1;
    return l;
}
static CgsGeometric::PolygonSoupLeafNode Leaf(f32 lfMinX, f32 lfMaxX)
{
    CgsGeometric::PolygonSoupLeafNode n;
    std::memset(&n, 0, sizeof(n));
    n.mBox.mMin = Vector4{ lfMinX, -100.0f, -100.0f, 0.0f };
    n.mBox.mMax = Vector4{ lfMaxX, 100.0f, 100.0f, 0.0f };
    return n;
}

int main()
{
    const char* KAC_KERNEL = "IntersectLinePolygonSoupSingleSided @0x8283C598";
    const char* KAC_LONG   = "RunQuery(const Line&) @0x82843E98";

    // C1: a 10 m line over three leaves, two of which overlap its box.
    {
        gaAsserts.clear();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        map.maLeaves = { Leaf(-1.0f, 1.0f), Leaf(50.0f, 60.0f), Leaf(-0.5f, 0.0f) };
        map.maOutput = { 2, 1, 0 };
        const CgsGeometric::Line l = MakeLine(0.0f, 5.0f, 0.0f, 1.0f, 0.0f, -5.0f, 0.0f, 0.25f);
        const u16 idx = gen.CollideLineAgainstPolySoupList(l, &map, 0x20, 11u, 3);
        Check(gen.miPrepares == 1 && gen.mu16PrepMax == 0x20 && gen.muPrepTagA == 11u && gen.mu16PrepTagB == 3,
              "C1 the list is claimed with (lu16MaxNumResults, tagA, tagB) (0x82812B9C..BAC)");
        Check(map.miRunQueries == 1 && map.mLastBox.mMin.y == -5.0f && map.mLastBox.mMax.y == 5.0f
              && map.mLastBox.mMin.w == 0.25f && map.mLastBox.mMax.w == 1.0f,
              "C1 under 20 m: RunQuery(box) with the vminfp/vmaxfp of start and end, all four lanes");
        Check(CountAsserts(KAC_KERNEL) == 2 && CountAsserts(KAC_LONG) == 0,
              "C1 the absent soup kernel traps once per OVERLAPPING leaf (the far leaf is skipped by the xyz overlap)");
        Check(gen.maLists[5].mu16NumResults == 0 && idx == 5,
              "C1 the count is written (sth -> list+0x0C: nothing found) and the list index returned");
    }
    // C2: a 100 m line (the place-on-track drop test) -> the long arm, one trap.
    {
        gaAsserts.clear();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        map.maLeaves = { Leaf(-1.0f, 1.0f) }; map.maOutput = { 0 };
        const u16 idx = gen.CollideLineAgainstPolySoupList(MakeLine(3.0f, 50.0f, 4.0f, 0.0f, 3.0f, -50.0f, 4.0f, 0.0f), &map, 0x20, 0u, 0);
        Check(CountAsserts(KAC_LONG) == 1 && gaAsserts.size() == 1 && map.miRunQueries == 0,
              "C2 20 m and over: exactly one long-arm trap (RunQuery(const Line&) @0x82843E98 absent), no box query");
        Check(gen.maLists[5].mu16NumResults == 0 && idx == 5 && gen.miPrepares == 1, "C2 ... the empty count is still written");
    }
    // C3: exactly 20 m -> `vcmpgtfp. 400 > |d|^2` is false -> the long arm.
    {
        gaAsserts.clear();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        gen.CollideLineAgainstPolySoupList(MakeLine(0.0f, 10.0f, 0.0f, 0.0f, 0.0f, -10.0f, 0.0f, 0.0f), &map, 0x20, 0u, 0);
        Check(CountAsserts(KAC_LONG) == 1 && map.miRunQueries == 0, "C3 |d|^2 == 400 takes the long arm (strict 400 > |d|^2, flt_8200889C)");
    }
    // C4: an invalid line fires "Invalid line" (:880) and does not gate.
    {
        gaAsserts.clear();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
        gen.CollideLineAgainstPolySoupList(MakeLine(lfNaN, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f), &map, 0x20, 0u, 0);
        Check(CountAsserts("Invalid line") == 1 && gen.miPrepares == 1, "C4 Line::IsValid false -> the :880 tripwire, then the body runs on");
    }

    std::printf("FxScenemgrCollideLineAllHits: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
