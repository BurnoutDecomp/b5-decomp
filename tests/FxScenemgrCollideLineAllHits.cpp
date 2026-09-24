// FX-SCENEMGR (crash parity 2026-09-24, item 3b) + FX-GEOMETRIC (same day): BaseCollisionGenerator::
// CollideLineAgainstPolySoupList (ARTIST 0x82812AE0), extracted VERBATIM by run_fxscenemgr_collide_line_all_hits.py
// with the file's own KF_SHORT_LINE_LENGTH_SQ / LeafOverlapsBoxXYZ.
//
// FX-SCENEMGR landed the driver with TRAPS where it calls its two absent Geometric callees. FX-GEOMETRIC landed
// both (IntersectLinePolygonSoupSingleSided @0x8283C598, PolygonSoupListSpatialMap::RunQuery(const Line&)
// @0x82843E98) and replaced the traps with the calls, so the checks below are the console's driver end to end:
//   claim the list with (max, tagA, tagB); split at 400 > |d|^2 (flt_8200889C);
//   short arm: RunQuery(box of min/max lanes), skip non-overlapping leaves, per overlapping leaf
//              found += kernel(*leaf.mpPolygonSoup, start, end, results + found, max - found)   (0x82812CE8)
//   long arm:  RunQuery(const Line&), per leaf the inlined TestLineStartEndAxisAlignedBox (the REAL
//              CgsLineTests.cpp is compiled alongside), the same kernel call                   (0x8281317C)
//   no guard on found < max before a call (the console passes max - found as it is); write the count; return idx.
// The kernel is a recording fake here (its own body is FxGeometricSoupSingleSided's subject).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupSpacialNode.h"
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoup.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsPolygonSoupTests.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
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

// ---- the recording kernel fake --------------------------------------------------------------------
struct KernelCall { const CgsGeometric::PolygonSoup* mpSoup; const CgsGeometric::PolySoupLineNearestResult* mpOut; s32 miMax; f32 mfStartY; };
static std::vector<KernelCall> gaKernelCalls;
static std::map<const CgsGeometric::PolygonSoup*, s32> gaKernelReturns;
namespace CgsGeometric {
s32 IntersectLinePolygonSoupSingleSided(const PolygonSoup& lPolygonSoup, const Vector3& lLineStart, const Vector3&,
                                        PolySoupLineNearestResult* lpResultBuffer, s32 liMaxResults)
{
    gaKernelCalls.push_back(KernelCall{ &lPolygonSoup, lpResultBuffer, liMaxResults, lLineStart.y });
    return gaKernelReturns.count(&lPolygonSoup) ? gaKernelReturns[&lPolygonSoup] : 0;
}
}

namespace CgsGeometric {
struct PolygonSoupListSpatialMap
{
    int miRunQueries = 0;
    int miRunLineQueries = 0;
    AxisAlignedBox mLastBox = {};
    Line mLastLine = {};
    std::vector<u16> maOutput;
    std::vector<PolygonSoupLeafNode> maLeaves;
    s32 RunQuery(const AxisAlignedBox& lrBox) { ++miRunQueries; mLastBox = lrBox; return static_cast<s32>(maOutput.size()); }
    s32 RunQuery(const Line& lrLine) { ++miRunLineQueries; mLastLine = lrLine; return static_cast<s32>(maOutput.size()); }
    const u16* GetOutputQueryBuffer() const { return maOutput.data(); }
    const PolygonSoupLeafNode* GetLeafNodes() const { return maLeaves.data(); }
};
}

namespace CgsSceneManager {
namespace CgsCollision {
namespace {
#include "fxsm_cla_helpers.inc"   // KF_SHORT_LINE_LENGTH_SQ + LeafOverlapsBoxXYZ, from the source
}
struct CollisionResult { u8 maRaw[112]; };
struct CollisionResultList { CollisionResult* mpResults; u32 mu32UserTagA; u16 mu16UserTagB; u16 mu16MaxNumResults; u16 mu16NumResults; u8 meResultType; };
struct BaseCollisionGenerator
{
    CollisionResultList maLists[8];
    CollisionResultList* mapCollisionResultLists[8];
    CgsGeometric::PolySoupLineNearestResult maRecords[64];
    int miPrepares = 0; u16 mu16PrepMax = 0; u32 muPrepTagA = 0; u16 mu16PrepTagB = 0;
    BaseCollisionGenerator()
    {
        for (int i = 0; i < 8; ++i)
        {
            std::memset(&maLists[i], 0, sizeof(maLists[i]));
            maLists[i].mu16NumResults = 0xBEEF;
            maLists[i].mpResults = reinterpret_cast<CollisionResult*>(maRecords);
            mapCollisionResultLists[i] = &maLists[i];
        }
    }
    s32 PrepareNewPrimitiveTestResultsList(u16 lu16Max, u32 luTagA, u16 lu16TagB)
    {
        ++miPrepares; mu16PrepMax = lu16Max; muPrepTagA = luTagA; mu16PrepTagB = lu16TagB;
        return 5;
    }
    u16 CollideLineAgainstPolySoupList(const CgsGeometric::Line& lrLine, CgsGeometric::PolygonSoupListSpatialMap* lpMap,
                                       u16 lu16MaxNumResults, u32 lu32UserTagA, u16 lu16UserTagB);
};
static void NoteLineSoupListOverrun(s32, u16) {}   // FX-TAILS-A item 7: the body's [DIAG] tripwire, a no-op here
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
static CgsGeometric::PolygonSoup gaSoups[8];
static CgsGeometric::PolygonSoupLeafNode Leaf(f32 lfMinX, f32 lfMaxX, int liSoup, f32 lfMinZ = -100.0f, f32 lfMaxZ = 100.0f)
{
    CgsGeometric::PolygonSoupLeafNode n;
    std::memset(&n, 0, sizeof(n));
    n.mBox.mMin = Vector4{ lfMinX, -100.0f, lfMinZ, 0.0f };
    n.mBox.mMax = Vector4{ lfMaxX, 100.0f, lfMaxZ, 0.0f };
    n.mpPolygonSoup = &gaSoups[liSoup];
    return n;
}
static void Reset() { gaAsserts.clear(); gaKernelCalls.clear(); gaKernelReturns.clear(); }

int main()
{
    // C1: a 10 m line over three leaves, two of which overlap its box.
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        map.maLeaves = { Leaf(-1.0f, 1.0f, 0), Leaf(50.0f, 60.0f, 1), Leaf(-0.5f, 0.0f, 2) };
        map.maOutput = { 2, 1, 0 };
        gaKernelReturns[&gaSoups[2]] = 2;
        gaKernelReturns[&gaSoups[0]] = 1;
        const CgsGeometric::Line l = MakeLine(0.0f, 5.0f, 0.0f, 1.0f, 0.0f, -5.0f, 0.0f, 0.25f);
        const u16 idx = gen.CollideLineAgainstPolySoupList(l, &map, 0x20, 11u, 3);
        Check(gen.miPrepares == 1 && gen.mu16PrepMax == 0x20 && gen.muPrepTagA == 11u && gen.mu16PrepTagB == 3,
              "C1 the list is claimed with (lu16MaxNumResults, tagA, tagB) (0x82812B9C..BAC)");
        Check(map.miRunQueries == 1 && map.miRunLineQueries == 0 && map.mLastBox.mMin.y == -5.0f && map.mLastBox.mMax.y == 5.0f
              && map.mLastBox.mMin.w == 0.25f && map.mLastBox.mMax.w == 1.0f,
              "C1 under 20 m: RunQuery(box) with the vminfp/vmaxfp of start and end, all four lanes");
        Check(gaKernelCalls.size() == 2 && gaKernelCalls[0].mpSoup == &gaSoups[2] && gaKernelCalls[1].mpSoup == &gaSoups[0],
              "C1 the soup kernel runs once per OVERLAPPING leaf, in the query's order (the far leaf is skipped)");
        Check(gaKernelCalls.size() == 2 && gaKernelCalls[0].mpOut == gen.maRecords && gaKernelCalls[0].miMax == 0x20
              && gaKernelCalls[1].mpOut == gen.maRecords + 2 && gaKernelCalls[1].miMax == 0x20 - 2
              && gaKernelCalls[0].mfStartY == 5.0f,
              "C1 each call gets v1 = start, results + 0x70 * found and max - found (0x82812CD4..0x82812CE4)");
        Check(gen.maLists[5].mu16NumResults == 3 && idx == 5 && CountAsserts("not reconstructed") == 0,
              "C1 the count (2 + 1) is written (sth -> list+0x0C) and the list index returned; no trap");
    }
    // C2: a 100 m line (the place-on-track drop test) -> the long arm.
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        // Leaf 0's box does not contain x = 3 (the real slab test rejects it); leaf 1's does.
        map.maLeaves = { Leaf(-1.0f, 1.0f, 3), Leaf(2.0f, 4.0f, 4, 3.0f, 5.0f) };
        map.maOutput = { 0, 1 };
        gaKernelReturns[&gaSoups[4]] = 4;
        const u16 idx = gen.CollideLineAgainstPolySoupList(MakeLine(3.0f, 50.0f, 4.0f, 0.0f, 3.0f, -50.0f, 4.0f, 0.0f), &map, 0x20, 0u, 0);
        Check(map.miRunLineQueries == 1 && map.miRunQueries == 0 && map.mLastLine.mStart.y == 50.0f && map.mLastLine.mEnd.y == -50.0f,
              "C2 20 m and over: RunQuery(const Line&) (sub_82843E98) gathers the leaves, no box query");
        Check(gaKernelCalls.size() == 1 && gaKernelCalls[0].mpSoup == &gaSoups[4] && gaKernelCalls[0].mpOut == gen.maRecords
              && gaKernelCalls[0].miMax == 0x20,
              "C2 the inlined TestLineStartEndAxisAlignedBox drops the leaf the vertical line misses; the other gets the kernel");
        Check(gen.maLists[5].mu16NumResults == 4 && idx == 5 && gen.miPrepares == 1 && gaAsserts.empty(),
              "C2 ... the count is written, no trap and no reciprocal tripwire (zero lanes refine to NaN)");
    }
    // C3: exactly 20 m -> `vcmpgtfp. 400 > |d|^2` is false -> the long arm.
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        gen.CollideLineAgainstPolySoupList(MakeLine(0.0f, 10.0f, 0.0f, 0.0f, 0.0f, -10.0f, 0.0f, 0.0f), &map, 0x20, 0u, 0);
        Check(map.miRunLineQueries == 1 && map.miRunQueries == 0, "C3 |d|^2 == 400 takes the long arm (strict 400 > |d|^2, flt_8200889C)");
    }
    // C4: an invalid line fires "Invalid line" (:880) and does not gate.
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
        gen.CollideLineAgainstPolySoupList(MakeLine(lfNaN, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f), &map, 0x20, 0u, 0);
        Check(CountAsserts("Invalid line") == 1 && gen.miPrepares == 1, "C4 Line::IsValid false -> the :880 tripwire, then the body runs on");
    }
    // C5: no guard on found < max -- the console hands the kernel max - found even when it is 0.
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        map.maLeaves = { Leaf(-1.0f, 1.0f, 5), Leaf(-2.0f, 2.0f, 6) };
        map.maOutput = { 0, 1 };
        gaKernelReturns[&gaSoups[5]] = 3;
        gaKernelReturns[&gaSoups[6]] = 1;
        gen.CollideLineAgainstPolySoupList(MakeLine(0.0f, 5.0f, 0.0f, 0.0f, 0.0f, -5.0f, 0.0f, 0.0f), &map, 3, 0u, 0);
        Check(gaKernelCalls.size() == 2 && gaKernelCalls[1].miMax == 0 && gaKernelCalls[1].mpOut == gen.maRecords + 3
              && gen.maLists[5].mu16NumResults == 4,
              "C5 max 3 filled by the first leaf -> the second still runs with max 0 (found 3 + 1 = 4), as shipped");
    }

    std::printf("FxScenemgrCollideLineAllHits: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
