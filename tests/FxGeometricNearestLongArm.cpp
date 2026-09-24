// FX-GEOMETRIC (crash parity 2026-09-24): BaseCollisionGenerator::CollideLineAgainstPolySoupListNearest (ARTIST
// 0x828131C0), extracted VERBATIM by run_fxgeometric_nearest_long_arm.py with the file's own KF_SHORT_LINE_LENGTH_SQ /
// KF_LINE_PARAM_NO_HIT / LeafOverlapsBoxXYZ. Its LONG arm (a line of 20 m or more, 0x82813440..0x82813918) was a named
// trap (CGS_ASSERT "... is not reconstructed") until the two callees it shares with the all-hits twin landed. The asm:
//   0x82813444  n = sub_82843E98(map, &line) == PolygonSoupListSpatialMap::RunQuery(const Line&) ; n <= 0 -> count
//   per leaf    0x8281348C..0x82813870 the inlined TestLineStartEndAxisAlignedBox (the REAL CgsLineTests.cpp is
//               compiled alongside) ; 0x82813874 no hit -> next leaf
//               0x8281389C IntersectLinePolygonSoupNearestSingleSided(*leaf.mpPolygonSoup, &tmp, start, end) ;
//               0x828138A0 no hit -> next leaf ; 0x828138C4 `vcmpgtfp. best+0x50, tmp+0x50` -> 14-qword copy
//   0x82813918  mu16NumResults = (1.0 >= best t) ; return idx
// The nearest kernel is a recording fake here (its own body is reconstructed in CgsPolygonSoupTests_LineNearest.cpp).
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

// ---- the recording nearest-kernel fake ------------------------------------------------------------------
// Per soup: the t it answers (no entry == no hit). It fills the whole record the way the real kernel does
// (t splatted over +0x50, the soup's id in the tag lanes) and returns 1.0 >= t.
static std::vector<const CgsGeometric::PolygonSoup*> gaKernelCalls;
static std::map<const CgsGeometric::PolygonSoup*, f32> gaKernelT;
static std::map<const CgsGeometric::PolygonSoup*, u32> gaKernelTag;
namespace CgsGeometric {
bool IntersectLinePolygonSoupNearestSingleSided(const PolygonSoup& lPolygonSoup, PolySoupLineNearestResult* lpOutResult,
                                                const Vector3& lStart, const Vector3& lEnd)
{
    gaKernelCalls.push_back(&lPolygonSoup);
    std::memset(lpOutResult, 0, sizeof(*lpOutResult));
    const f32 lfT = gaKernelT.count(&lPolygonSoup) ? gaKernelT[&lPolygonSoup] : 2.0f;
    lpOutResult->mLineParam = Vector4{ lfT, lfT, lfT, lfT };
    lpOutResult->mPosition  = Vector3{ lStart.x + (lEnd.x - lStart.x) * lfT, lStart.y + (lEnd.y - lStart.y) * lfT,
                                       lStart.z + (lEnd.z - lStart.z) * lfT, 0.0f };
    const u32 luTag = gaKernelTag.count(&lPolygonSoup) ? gaKernelTag[&lPolygonSoup] : 0u;
    for (int i = 0; i < 4; ++i) lpOutResult->mau32Tag[i] = luTag;
    return 1.0f >= lfT;
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
#include "fxg_nla_helpers.inc"   // KF_SHORT_LINE_LENGTH_SQ + KF_LINE_PARAM_NO_HIT + LeafOverlapsBoxXYZ, from the source
}
struct CollisionResult { u8 maRaw[112]; };
struct CollisionResultList { CollisionResult* mpResults; u32 mu32UserTagA; u16 mu16UserTagB; u16 mu16MaxNumResults; u16 mu16NumResults; u8 meResultType; };
struct BaseCollisionGenerator
{
    CollisionResultList maLists[8];
    CollisionResultList* mapCollisionResultLists[8];
    CgsGeometric::PolySoupLineNearestResult maRecords[2];
    int miPrepares = 0; u16 mu16PrepMax = 0; u32 muPrepTagA = 0; u16 mu16PrepTagB = 0;
    BaseCollisionGenerator()
    {
        std::memset(maRecords, 0xCD, sizeof(maRecords));
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
        return 6;
    }
    u16 CollideLineAgainstPolySoupListNearest(const CgsGeometric::Line& lrLine, CgsGeometric::PolygonSoupListSpatialMap* lpMap,
                                              u32 lu32UserTagA, u16 lu16UserTagB);
};
#include "fxg_nla_body.inc"
}
}

using CgsSceneManager::CgsCollision::BaseCollisionGenerator;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static CgsGeometric::Line MakeLine(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1)
{
    CgsGeometric::Line l;
    l.mStart.x = x0; l.mStart.y = y0; l.mStart.z = z0; l.mStart.w = 0.0f;
    l.mEnd.x = x1; l.mEnd.y = y1; l.mEnd.z = z1; l.mEnd.w = 0.0f;
    return l;
}
static CgsGeometric::PolygonSoup gaSoups[8];
static CgsGeometric::PolygonSoupLeafNode Leaf(f32 lfMinX, f32 lfMaxX, f32 lfMinZ, f32 lfMaxZ, int liSoup)
{
    CgsGeometric::PolygonSoupLeafNode n;
    std::memset(&n, 0, sizeof(n));
    n.mBox.mMin = Vector4{ lfMinX, -100.0f, lfMinZ, 0.0f };
    n.mBox.mMax = Vector4{ lfMaxX, 100.0f, lfMaxZ, 0.0f };
    n.mpPolygonSoup = &gaSoups[liSoup];
    return n;
}
static void Reset() { gaAsserts.clear(); gaKernelCalls.clear(); gaKernelT.clear(); gaKernelTag.clear(); }

int main()
{
    // N1: a 60 m horizontal camera probe along +x at y 2, z 0 over four leaves:
    //   leaf 0 [ 5,15]x[-1,1]  crossed, soup 0 answers t 0.40
    //   leaf 1 [ 0,60]x[ 7,9]  NOT crossed (the segment runs at z 0) -- the slab test drops it
    //   leaf 2 [20,30]x[-1,1]  crossed, soup 2 answers t 0.25  (nearer -> replaces)
    //   leaf 3 [40,50]x[-1,1]  crossed, soup 3 answers t 0.25  (a TIE -> strictly nearer only: kept 2)
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        map.maLeaves = { Leaf(5.0f, 15.0f, -1.0f, 1.0f, 0), Leaf(0.0f, 60.0f, 7.0f, 9.0f, 1),
                         Leaf(20.0f, 30.0f, -1.0f, 1.0f, 2), Leaf(40.0f, 50.0f, -1.0f, 1.0f, 3) };
        map.maOutput = { 0, 1, 2, 3 };
        gaKernelT[&gaSoups[0]] = 0.40f; gaKernelTag[&gaSoups[0]] = 100u;
        gaKernelT[&gaSoups[1]] = 0.10f; gaKernelTag[&gaSoups[1]] = 101u;   // would win if it were ever asked
        gaKernelT[&gaSoups[2]] = 0.25f; gaKernelTag[&gaSoups[2]] = 102u;
        gaKernelT[&gaSoups[3]] = 0.25f; gaKernelTag[&gaSoups[3]] = 103u;
        const u16 idx = gen.CollideLineAgainstPolySoupListNearest(MakeLine(0.0f, 2.0f, 0.0f, 60.0f, 2.0f, 0.0f), &map, 7u, 2);
        Check(gen.miPrepares == 1 && gen.mu16PrepMax == 1 && gen.muPrepTagA == 7u && gen.mu16PrepTagB == 2,
              "N1 the list is claimed with (1, tagA, tagB) (0x8281327C..0x8281328C)");
        Check(map.miRunLineQueries == 1 && map.miRunQueries == 0 && map.mLastLine.mEnd.x == 60.0f,
              "N1 20 m and over: RunQuery(const Line&) (sub_82843E98 @0x82813444) gathers the leaves, no box query");
        Check(gaKernelCalls.size() == 3 && gaKernelCalls[0] == &gaSoups[0] && gaKernelCalls[1] == &gaSoups[2]
              && gaKernelCalls[2] == &gaSoups[3],
              "N1 the inlined TestLineStartEndAxisAlignedBox drops the leaf the segment misses; the others get the nearest kernel");
        Check(gen.maRecords[0].mLineParam.x == 0.25f && gen.maRecords[0].mau32Tag[0] == 102u,
              "N1 strictly nearer replaces (0.40 -> 0.25), a tie does not (`vcmpgtfp. best, tmp`)");
        Check(gen.maLists[6].mu16NumResults == 1 && idx == 6 && gaAsserts.empty(),
              "N1 1.0 >= best t -> mu16NumResults 1, the list index returned; no trap, no reciprocal tripwire");
    }
    // N2: a 30 m line whose only crossed leaf answers no hit -> the 2.0 seed survives, count 0.
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        map.maLeaves = { Leaf(-5.0f, 5.0f, -5.0f, 5.0f, 4) };
        map.maOutput = { 0 };
        const u16 idx = gen.CollideLineAgainstPolySoupListNearest(MakeLine(0.0f, 15.0f, 0.0f, 0.0f, -15.0f, 0.0f), &map, 0u, 0);
        Check(gaKernelCalls.size() == 1 && gen.maRecords[0].mLineParam.x == 2.0f && gen.maRecords[0].mLineParam.w == 2.0f,
              "N2 a miss does not copy: the record keeps the 2.0 seed in all four lanes (0x82813290)");
        Check(gen.maLists[6].mu16NumResults == 0 && idx == 6 && gaAsserts.empty(),
              "N2 ... so 1.0 >= 2.0 is false and mu16NumResults is 0 (0x82813960); no trap");
    }
    // N3: exactly 20 m -> `vcmpgtfp. 400 > |d|^2` is false -> the long arm; no leaves -> the count store.
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        gen.CollideLineAgainstPolySoupListNearest(MakeLine(0.0f, 10.0f, 0.0f, 0.0f, -10.0f, 0.0f), &map, 0u, 0);
        Check(map.miRunLineQueries == 1 && map.miRunQueries == 0 && gaKernelCalls.empty()
              && gen.maLists[6].mu16NumResults == 0 && gaAsserts.empty(),
              "N3 |d|^2 == 400 takes the long arm; n == 0 goes straight to the count (ble @0x8281344C)");
    }
    // N4: the short arm is unchanged -- a 10 m ray, RunQuery(box), nearest of two overlapping leaves.
    {
        Reset();
        BaseCollisionGenerator gen;
        CgsGeometric::PolygonSoupListSpatialMap map;
        map.maLeaves = { Leaf(-1.0f, 1.0f, -1.0f, 1.0f, 5), Leaf(-2.0f, 2.0f, -2.0f, 2.0f, 6) };
        map.maOutput = { 0, 1 };
        gaKernelT[&gaSoups[5]] = 0.7f; gaKernelTag[&gaSoups[5]] = 105u;
        gaKernelT[&gaSoups[6]] = 0.3f; gaKernelTag[&gaSoups[6]] = 106u;
        gen.CollideLineAgainstPolySoupListNearest(MakeLine(0.0f, 5.0f, 0.0f, 0.0f, -5.0f, 0.0f), &map, 0u, 0);
        Check(map.miRunQueries == 1 && map.miRunLineQueries == 0 && gaKernelCalls.size() == 2
              && gen.maRecords[0].mau32Tag[0] == 106u && gen.maLists[6].mu16NumResults == 1 && gaAsserts.empty(),
              "N4 under 20 m: RunQuery(box), both overlapping leaves tested, the nearer (0.3) kept");
    }

    std::printf("FxGeometricNearestLongArm: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
