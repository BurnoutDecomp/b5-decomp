// FX-GEOMETRIC (crash parity 2026-09-24): CgsGeometric::PolygonSoupListSpatialMap::RunQuery(const Line&) @0x82843E98
// (X360 `sub_82843E98`, PS3 mangle @0xB64574) -- the segment leaf gather of the 20 m-and-over arm of
// BaseCollisionGenerator::CollideLineAgainstPolySoupList. run_fxgeometric_run_query_line.py extracts the production
// body from CgsPolygonSoupListSpatialMap_Query.cpp; the real TestLineStartEndAxisAlignedBox (CgsLineTests.cpp, the
// per-node test the console inlines) is compiled alongside.
//
// Expectations from the asm:
//   miNumLevels == 0 -> return 0 and write NOTHING (0x82843EC4)
//   level sweep from the root index 0 through mapQueryBuffers[0]/[1] (+0x50 / +0x54), per node the segment test,
//   `lhz 0x24` == 0 skipped, then every index appended; capacity (u16)count < (u16)miQueryBufferSize, else the
//   non-gating :523 "Too many results in level " assert (the append still happens)
//   tail: mpOutputQueryBuffer = the buffer the LAST level wrote, miLastQueryResultCount = count, return count
#include "types.hpp"
#include "BrnCommonTypes.h"
#define private public
#include "GameShared/GameClasses/Geometric/Primitives/PolygonSoup/CgsPolygonSoupListSpatialMap.h"
#undef private
#include "GameShared/GameClasses/Geometric/Primitives/CgsLine.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>
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
#include "fxg_rql_body.inc"   // PolygonSoupListSpatialMap::RunQuery(const Line&), verbatim
}

using CgsGeometric::PolygonSoupListSpatialMap;
using CgsGeometric::PolygonSoupSpacialNode;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName)
{
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

static PolygonSoupSpacialNode Node(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1, u16* lpaIndices, u16 lu16Count)
{
    PolygonSoupSpacialNode n;
    std::memset(&n, 0, sizeof(n));
    n.mBox.mMin = Vector4{ x0, y0, z0, 0.0f };
    n.mBox.mMax = Vector4{ x1, y1, z1, 0.0f };
    n.mpaIndices = lpaIndices;
    n.mu16NumIndices = lu16Count;
    return n;
}
static CgsGeometric::Line MakeLine(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1)
{
    CgsGeometric::Line l;
    l.mStart.x = x0; l.mStart.y = y0; l.mStart.z = z0; l.mStart.w = 0.0f;
    l.mEnd.x = x1; l.mEnd.y = y1; l.mEnd.z = z1; l.mEnd.w = 0.0f;
    return l;
}

// Two levels: one root over [0,100]^3 pointing at four quadrant nodes; the quadrants point at LEAF indices.
struct Partition
{
    u16 mau16RootChildren[4] = { 0, 1, 2, 3 };
    u16 mau16Q0Leaves[2] = { 5, 7 };      // quadrant x[0,50] z[0,50], full height
    u16 mau16Q1Leaves[1] = { 9 };         // quadrant x[50,100] z[0,50], only y[60,80]
    u16 mau16Q3Leaves[3] = { 11, 12, 13 };// quadrant x[50,100] z[50,100], full height
    PolygonSoupSpacialNode maLevel0[1];
    PolygonSoupSpacialNode maLevel1[4];
    u16 mau16BufferA[64];
    u16 mau16BufferB[64];
    PolygonSoupListSpatialMap mMap;

    Partition()
    {
        maLevel0[0] = Node(0, 0, 0, 100, 100, 100, mau16RootChildren, 4);
        maLevel1[0] = Node(0, 0, 0, 50, 100, 50, mau16Q0Leaves, 2);
        maLevel1[1] = Node(50, 60, 0, 100, 80, 50, mau16Q1Leaves, 1);
        maLevel1[2] = Node(0, 0, 50, 50, 100, 100, nullptr, 0);          // empty node
        maLevel1[3] = Node(50, 0, 50, 100, 100, 100, mau16Q3Leaves, 3);
        std::memset(mau16BufferA, 0xEE, sizeof(mau16BufferA));
        std::memset(mau16BufferB, 0xEE, sizeof(mau16BufferB));
        std::memset(&mMap, 0, sizeof(mMap));
        mMap.mapParentNodes[0] = maLevel0;  mMap.maiParentNodeCounts[0] = 1;
        mMap.mapParentNodes[1] = maLevel1;  mMap.maiParentNodeCounts[1] = 4;
        mMap.mapQueryBuffers[0] = mau16BufferA;
        mMap.mapQueryBuffers[1] = mau16BufferB;
        mMap.miNumLevels = 2;
        mMap.miQueryBufferSize = 64;
        mMap.mpOutputQueryBuffer = nullptr;
        mMap.miLastQueryResultCount = -7;
    }
};

int main()
{
    // Q1: the place-on-track shape -- a vertical 100 m line through quadrant 0.
    {
        gaAsserts.clear();
        Partition p;
        const s32 n = p.mMap.RunQuery(MakeLine(10, 90, 10, 10, -10, 10));
        Check(n == 2 && p.mMap.miLastQueryResultCount == 2, "Q1 a vertical line through quadrant 0 gathers its two leaves");
        Check(p.mMap.mpOutputQueryBuffer == p.mau16BufferA && p.mau16BufferA[0] == 5 && p.mau16BufferA[1] == 7,
              "Q1 ... published in the buffer the LAST level wrote (level 0 -> B, level 1 -> A), in node order");
        Check(p.mau16BufferB[0] == 0 && p.mau16BufferB[1] == 1 && p.mau16BufferB[2] == 2 && p.mau16BufferB[3] == 3,
              "Q1 ... level 0 appended the root's four children to the other buffer");
        Check(gaAsserts.empty(), "Q1 ... no tripwire: zero direction lanes refine to NaN, never to 0");
    }
    // Q2: xz inside quadrant 1 but the segment never reaches its y range [60,80].
    {
        Partition p;
        Check(p.mMap.RunQuery(MakeLine(75, 50, 25, 75, -10, 25)) == 0 && p.mMap.miLastQueryResultCount == 0
              && p.mMap.mpOutputQueryBuffer == p.mau16BufferA,
              "Q2 a segment below quadrant 1's box gathers nothing (count 0 still published)");
        Check(p.mMap.RunQuery(MakeLine(75, 90, 25, 75, -10, 25)) == 1 && p.mau16BufferA[0] == 9,
              "Q2 ... and the same column through its y range gathers leaf 9");
    }
    // Q3: a diagonal through quadrants 0 and 3 (the empty quadrant 2 contributes nothing).
    {
        Partition p;
        const s32 n = p.mMap.RunQuery(MakeLine(10, 50, 10, 90, 50, 90));
        Check(n == 5 && p.mau16BufferA[0] == 5 && p.mau16BufferA[1] == 7 && p.mau16BufferA[2] == 11
              && p.mau16BufferA[3] == 12 && p.mau16BufferA[4] == 13,
              "Q3 a diagonal meets quadrants 0 and 3: leaves 5,7 then 11,12,13");
    }
    // Q4: a short segment wholly inside quadrant 3 -- the endpoint terms carry it.
    {
        Partition p;
        Check(p.mMap.RunQuery(MakeLine(70, 40, 70, 71, 41, 72)) == 3, "Q4 a segment inside a node's box is a hit");
    }
    // Q5: no levels -> return 0, nothing written.
    {
        Partition p;
        p.mMap.miNumLevels = 0;
        Check(p.mMap.RunQuery(MakeLine(10, 90, 10, 10, -10, 10)) == 0 && p.mMap.mpOutputQueryBuffer == nullptr
              && p.mMap.miLastQueryResultCount == -7 && p.mau16BufferA[0] == 0xEEEE,
              "Q5 miNumLevels == 0 -> `li r3, 0`, the output fields and the seed untouched");
    }
    // Q6: the capacity compare is 16-bit on BOTH sides (`clrlwi 16` x2, 0x828443F8..0x82844404).
    {
        gaAsserts.clear();
        Partition p;
        p.mMap.miQueryBufferSize = 0x10001;   // (u16) 1
        const s32 n = p.mMap.RunQuery(MakeLine(10, 90, 10, 10, -10, 10));
        Check(CountAsserts("Too many results in level") >= 1,
              "Q6 miQueryBufferSize 0x10001 truncates to 1 -> the :523 tripwire fires");
        Check(n == 2 && p.mau16BufferA[1] == 7, "Q6 ... and is non-gating: the append still happens");
    }

    std::printf("FxGeometricRunQueryLine: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
