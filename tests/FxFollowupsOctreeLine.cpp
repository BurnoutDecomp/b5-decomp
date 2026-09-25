// FX-FOLLOWUPS (crash parity 2026-09-25): the loose octree's LINE walk --
//   LooseOctree::LineTestOptimized @0x828CA5F8        (a CGS_ASSERT(false) trap on PC before)
//   LooseOctree::LineTestRecursive @0x828BCF50         (no PC body before)
//   CgsGeometric::TestLineSphere4 (CgsLineTests.cpp:172 / :305) and
//   CgsGeometric::TestLineBoundingBoxAgainstAxisAlignedBox4 (:766), inlined into LineTestRecursive
//   SpatialPartition::LineTestRecursiveFuncParams (DWARF CgsSpatialPartition.h:131-139; an opaque
//     0x40-byte block before)
//   LooseOctree::TestLineAgainstNodeBoundingBox @0x828B0FC8 (reads the block by name now)
// run_fxfollowups_octree_line.py pastes the PRODUCTION text:
//   fxfu_oct.inc       the file-local constants / helpers, the LooseOctree constructor,
//                      TestLineAgainstNodeBoundingBox and LineTestRecursive (CgsLooseOctree.cpp)
//   fxfu_oct_opt.inc   LineTestOptimized (CgsLooseOctree.cpp), included behind a capture shim on its
//                      LineTestRecursive call so the parameter block it builds can be read back
//   fxfu_oct_old.inc   the PRE-CHANGE TestLineAgainstNodeBoundingBox (raw float offsets +0x00 / +0x30),
//                      renamed to a free function -- the "no math change" witness
// and compiles CgsLineTests.cpp and CgsSpatialPartition.cpp alongside.
//
// Every expectation comes from the ARTIST decode (the banners in production carry the addresses):
//   * the block: start +0x00, end +0x10, unit direction +0x20, reciprocal +0x30 (1/d, but 1/eps with
//     eps = 1e-7 (flt_820F5E68) for any lane with -eps < d < eps -- POSITIVE 1/eps even for a tiny
//     negative d), length splatted +0x40, flags +0x50, buffer after;
//   * a segment of length <= 1e-4 (flt_82002540) is answered by the vtable SphereTest(flags, start,
//     1e-4, buffer) and its answer returned; otherwise the root box gate, then the walk from node 0,
//     then GetNumResultsAttempted() > 0;
//   * the walk: a node's own entities when the node mask meets the query; the chain walked
//     muNumElements times; matching links batched four at a time, a full batch pushed in lane order at
//     once, the remainder after the chain; then the four children, each gated by its SUB-TREE mask and
//     by the PER-AXIS slab test (the segment's axis ranges overlap the box's -- not the exact
//     interval intersection), recursed in child order;
//   * line vs sphere, lane k: ((t >= 0) & !(t > L_k) & !(|perp|^2 > R^2)) | start inside | end
//     inside, with L_k lane k of the length vector (vperm / vsldoi rebuild it lane for lane).
#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.h"
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsCoarseQueryResultBuffer.h"
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsAxisAlignedBox.h"
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"

// LineTestOptimized's [octree-line] DIAG logs through gpDebugPrint; this harness has no log (the
// witness returns before formatting when the printer is null).
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
namespace CgsDev { namespace Log { DebugPrint* gpDebugPrint = nullptr; } }

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <string>
#include <vector>

// ---- the assert seam: count and record every message ---------------------------------------------
static std::vector<std::string> gaAsserts;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int) { gaAsserts.push_back(lpcMessage ? lpcMessage : ""); return 0; }
void* EndAssert() { return nullptr; }
} }

// ---- the fixture's view of LineTestOptimized's walk call --------------------------------------------
static CgsSceneManager::SpatialPartition::LineTestRecursiveFuncParams gCapturedParams;
static int giCapturedWalks = 0;
static u16 gu16CapturedNode = 0xFFFF;

// ---- the vtable SphereTest the short-segment arm dispatches to ---------------------------------------
struct SphereTestCall { u32 mx32Flags; Vector3 mCentre; f32 mfRadius; const void* mpBuffer; };
static std::vector<SphereTestCall> gaSphereCalls;
static bool gbSphereAnswer = false;

namespace CgsSceneManager
{
#include "fxfu_oct.inc"

#define LineTestRecursive(liNode, lpParams)                                                     \
    (gCapturedParams = *(lpParams), ++giCapturedWalks, gu16CapturedNode = (liNode),          \
     this->LineTestRecursive((liNode), (lpParams)))
#include "fxfu_oct_opt.inc"
#undef LineTestRecursive

    namespace OldBody
    {
#include "fxfu_oct_old.inc"
    }

    // The virtuals this test does not exercise (the vtable needs them).
    void LooseOctree::Construct(SpatialPartitionConstructParams*, rw::IResourceAllocator*) {}
    void LooseOctree::Destruct() {}
    bool LooseOctree::Prepare() { return true; }
    bool LooseOctree::Release() { return true; }
    bool LooseOctree::SphereTest(u32 lx32EntityTypeFlags, Vector3 lCentre, f32 lfRadius,
                                 CoarseQueryResultBuffer<16384>* lpResultBufferOut)
    {
        gaSphereCalls.push_back(SphereTestCall{ lx32EntityTypeFlags, lCentre, lfRadius, lpResultBufferOut });
        return gbSphereAnswer;
    }
    bool LooseOctree::LineTest(u32, Vector3, Vector3, CoarseQueryResultBuffer<16384>*) { return false; }
    bool LooseOctree::FrustumTestVp(u32, const CgsGeometric::Frustum&, const Matrix44&,
                                    CoarseQueryResultBuffer<16384>*) { return false; }
    void LooseOctree::Update() {}
    void LooseOctree::SetEntityPosition(u16, Vector3) {}
    void LooseOctree::SetEntityRadius(u16, f32) {}
    void LooseOctree::FrustumTestEntities(const CgsGeometric::Frustum&, u32, const u16*, s32,
                                          CoarseQueryResultBuffer<16384>*) {}
    void LooseOctree::AddEntityToGraph(u16) {}
    void LooseOctree::RemoveEntityFromGraph(u16) {}
    // Virtuals a later revision's header adds (the runner writes a stub for each one that revision declares,
    // e.g. VolumeTest from item 2 step C; empty for the revisions before it).
#include "fxfu_oct_stubs.inc"
}

using namespace CgsSceneManager;
typedef SpatialPartition::LineTestRecursiveFuncParams Params;

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPassed, const char* lpcName)
{
    ++giChecks;
    if (!lbPassed) { ++giFailures; std::printf("FAIL  %s\n", lpcName); }
    else           { std::printf("ok    %s\n", lpcName); }
}

static Vector3 V3(f32 x, f32 y, f32 z, f32 w = 0.0f) { Vector3 v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }
static Vector4 V4(f32 x, f32 y, f32 z, f32 w) { Vector4 v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }
static bool Near(f32 a, f32 b, f32 tol) { return std::fabs(a - b) <= tol; }
static u32 Bits(f32 f) { u32 u; std::memcpy(&u, &f, 4); return u; }
static u32 LaneBits(const Vector4& v, int k) { return Bits((&v.x)[k]); }

static CgsGeometric::Sphere MakeSphere(f32 x, f32 y, f32 z, f32 r)
{
    CgsGeometric::Sphere s; s.mPositionRadius = V4(x, y, z, r); return s;
}
static CgsGeometric::AxisAlignedBox MakeBox(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1)
{
    CgsGeometric::AxisAlignedBox b; b.mMin = V4(x0, y0, z0, 0.0f); b.mMax = V4(x1, y1, z1, 0.0f); return b;
}

// ---- the fixture octree -----------------------------------------------------------------------------
alignas(64) static unsigned char gaOctreeStorage[sizeof(LooseOctree)];
static LooseOctreeNode gaNodes[8];
static LooseOctreeNodeEntityInfo gaNodeInfo[8];

static LooseOctree* MakeTree()
{
    std::memset(gaOctreeStorage, 0, sizeof(gaOctreeStorage));
    LooseOctree* lpTree = ::new (static_cast<void*>(gaOctreeStorage)) LooseOctree();
    std::memset(gaNodes, 0, sizeof(gaNodes));
    std::memset(gaNodeInfo, 0, sizeof(gaNodeInfo));
    for (int i = 0; i < 8; ++i) { gaNodes[i].muFirstChildIndex = KU_INVALID_NODE; gaNodes[i].muHeadIndex = 0xFFFF; gaNodes[i].muTailIndex = 0xFFFF; gaNodes[i].muParentIndex = KU_INVALID_NODE; }
    lpTree->mpNodes = gaNodes;
    lpTree->mpRootNode = &gaNodes[0];
    lpTree->mpNodesEntityInfo = gaNodeInfo;
    for (int i = 0; i < SpatialPartition::KI_MAX_NUM_ENTITIES; ++i)
    {
        lpTree->maEntityLinks[i].mx32TypeFlags = 0;
        lpTree->maEntityLinks[i].mu16NextEntity = 0xFFFF;
        lpTree->maEntityLinks[i].mu16PrevEntity = 0xFFFF;
    }
    return lpTree;
}

static void SetNode(int n, f32 x, f32 y, f32 z, f32 halfSize, f32 halfHeight, u32 nodeFlags, u32 subTreeFlags)
{
    LooseOctreeNode& lrNode = gaNodes[n];
    lrNode.mPosition = V3(x, y, z, 0.0f);
    lrNode.mHalfDimensions = V3(halfSize, halfHeight, halfSize, 0.0f);
    lrNode.mParams0 = V4(0.0f, halfSize, 0.0f, halfSize);
    lrNode.mParams1 = V4(y - halfHeight, y + halfHeight, halfHeight, 0.0f);
    lrNode.mxNodeEntityFlags = nodeFlags;
    gaNodeInfo[n].mxSubTreeEntityFlags = subTreeFlags;
}

// File a chain of entities on node n (in chain order), each with its type mask and sphere.
struct EntitySpec { u16 mu16Index; u32 mx32Type; CgsGeometric::Sphere mSphere; };
static void FileChain(LooseOctree* lpTree, int n, const std::vector<EntitySpec>& laSpecs, u32 luCountOverride = 0xFFFFFFFFu)
{
    LooseOctreeNode& lrNode = gaNodes[n];
    lrNode.muNumElements = (luCountOverride != 0xFFFFFFFFu) ? luCountOverride : static_cast<u32>(laSpecs.size());
    lrNode.muHeadIndex = laSpecs.empty() ? 0xFFFF : laSpecs.front().mu16Index;
    for (size_t i = 0; i < laSpecs.size(); ++i)
    {
        SpatialPartitionEntityLink& lrLink = lpTree->maEntityLinks[laSpecs[i].mu16Index];
        lrLink.mx32TypeFlags = laSpecs[i].mx32Type;
        lrLink.mu16NextEntity = (i + 1 < laSpecs.size()) ? laSpecs[i + 1].mu16Index : 0xFFFF;
        lrLink.mu16PrevEntity = (i > 0) ? laSpecs[i - 1].mu16Index : 0xFFFF;
        lpTree->maEntityBoundingSpheres[laSpecs[i].mu16Index] = laSpecs[i].mSphere;
    }
}

static CoarseQueryResultBuffer<16384>* NewBuffer()
{
    CoarseQueryResultBuffer<16384>* lpBuffer = new CoarseQueryResultBuffer<16384>();
    lpBuffer->Construct();
    lpBuffer->BeginResultsBatch();
    return lpBuffer;
}
static std::vector<u16> Pushed(CoarseQueryResultBuffer<16384>* lpBuffer)
{
    std::vector<u16> la;
    for (s32 i = lpBuffer->miCurrentResultsStartPosition; i < lpBuffer->miTotalBufferSize; ++i) la.push_back(lpBuffer->mau16Buffer[i]);
    return la;
}
static bool SameList(const std::vector<u16>& a, std::initializer_list<u16> b)
{
    return a == std::vector<u16>(b);
}
static void PrintList(const char* lpcLabel, const std::vector<u16>& la)
{
    std::printf("      %s:", lpcLabel);
    for (u16 u : la) std::printf(" %u", static_cast<unsigned>(u));
    std::printf("\n");
}

int main()
{
    // ================================================================================================
    // A. the parameter block keeps the console offsets (only the buffer pointer widens)
    // ================================================================================================
    Check(offsetof(Params, mLineStart) == 0x00 && offsetof(Params, mLineEnd) == 0x10
          && offsetof(Params, mLineDirection) == 0x20 && offsetof(Params, mLineReciprocal) == 0x30
          && offsetof(Params, mfLineLength) == 0x40 && offsetof(Params, mx32EntityTypeFlags) == 0x50,
          "A1 LineTestRecursiveFuncParams: start/end/direction/reciprocal/length/flags at +0x00/+0x10/+0x20/+0x30/+0x40/+0x50");

    // ================================================================================================
    // B. TestLineAgainstNodeBoundingBox: the name switch changed no answer (old raw-offset body vs new)
    // ================================================================================================
    {
        LooseOctree* lpTree = MakeTree();
        u32 luSeed = 12345u;
        auto Rand = [&luSeed]() { luSeed = luSeed * 1664525u + 1013904223u; return static_cast<f32>((luSeed >> 8) & 0xFFFF) / 65535.0f; };
        int liMismatch = 0, liTrue = 0, liCases = 0;
        for (int i = 0; i < 20000; ++i)
        {
            LooseOctreeNode lNode; std::memset(&lNode, 0, sizeof(lNode));
            lNode.mPosition = V3(Rand() * 200 - 100, Rand() * 40 - 20, Rand() * 200 - 100);
            lNode.mParams0 = V4(0, 0, 0, Rand() * 60 + 1);
            lNode.mParams1 = V4(0, 0, Rand() * 30 + 1, 0);
            Params lParams; std::memset(&lParams, 0, sizeof(lParams));
            lParams.mLineStart = V3(Rand() * 300 - 150, Rand() * 60 - 30, Rand() * 300 - 150, Rand());
            const f32 lfDx = Rand() * 300 - 150, lfDy = (i % 7 == 0) ? 0.0f : Rand() * 60 - 30, lfDz = Rand() * 300 - 150;
            lParams.mLineReciprocal = V3(1.0f / lfDx, lfDy == 0.0f ? 1.0e7f : 1.0f / lfDy, 1.0f / lfDz, Rand());
            const bool lbNew = lpTree->TestLineAgainstNodeBoundingBox(&lNode, &lParams);
            const bool lbOld = OldBody::OldTestLineAgainstNodeBoundingBox(&lNode, &lParams);
            ++liCases; if (lbNew != lbOld) ++liMismatch; if (lbNew) ++liTrue;
        }
        std::printf("      B: %d cases, %d crossed, %d mismatches\n", liCases, liTrue, liMismatch);
        Check(liMismatch == 0 && liTrue > 1000 && liTrue < liCases - 1000,
              "B1 TestLineAgainstNodeBoundingBox: 20000 random node/segment pairs answer exactly as the pre-change raw-offset body");
    }

    // ================================================================================================
    // C. TestLineSphere4 -- one lane per predicate arm
    // ================================================================================================
    {
        // Segment A -> B along +x, length 10.
        const Vector3 lStart = V3(0, 0, 0, 0), lDir = V3(1, 0, 0, 0);
        const VecFloat lLength = V4(10, 10, 10, 10);
        const Vector4 lHits = CgsGeometric::TestLineSphere4(
            MakeSphere(5, 2, 0, 3),      // lane 0: foot inside the segment, perp 2 <= 3        -> hit
            MakeSphere(5, 3, 0, 3),      // lane 1: perp exactly R (touching counts)             -> hit
            MakeSphere(5, 3.01f, 0, 3),  // lane 2: perp just beyond R                            -> miss
            MakeSphere(-2, 0, 0, 3),     // lane 3: behind the start, the start inside           -> hit
            lStart, lDir, lLength);
        Check(LaneBits(lHits, 0) == 0xFFFFFFFFu && LaneBits(lHits, 1) == 0xFFFFFFFFu
              && LaneBits(lHits, 2) == 0u && LaneBits(lHits, 3) == 0xFFFFFFFFu,
              "C1 TestLineSphere4 (:172): on-segment perp <= R hits, perp == R hits, perp > R misses, start-inside hits; lanes are all-ones / zero");

        const Vector4 lHits2 = CgsGeometric::TestLineSphere4(
            MakeSphere(-5, 0, 0, 3),     // lane 0: behind the start (t < 0), start outside, perp 0 -> miss
            MakeSphere(15, 0, 0, 3),     // lane 1: beyond the end (t > L), end outside, perp 0    -> miss
            MakeSphere(12, 0, 0, 3),     // lane 2: beyond the end, the END inside                 -> hit
            MakeSphere(5, 0, 0, 0),      // lane 3: radius 0 on the segment, perp 0 == R            -> hit
            lStart, lDir, lLength);
        Check(LaneBits(lHits2, 0) == 0u && LaneBits(lHits2, 1) == 0u
              && LaneBits(lHits2, 2) == 0xFFFFFFFFu && LaneBits(lHits2, 3) == 0xFFFFFFFFu,
              "C2 TestLineSphere4: t < 0 with the start outside misses, t > L with the end outside misses, end-inside hits, R == perp == 0 hits");

        // Lane k tests with lane k of the length (vperm unk_82CDA3C0 / unk_82CDA400 + vsldoi 8 rebuild
        // {L.x, L.y, L.z, L.w}): the same sphere at t = 5 hits where L_k = 10 and misses where L_k = 1
        // (the end at x = 1 is outside it).
        const CgsGeometric::Sphere lSame = MakeSphere(5, 1, 0, 2);
        const Vector4 lHits3 = CgsGeometric::TestLineSphere4(lSame, lSame, lSame, lSame,
                                                             lStart, lDir, V4(10, 10, 1, 10));
        Check(LaneBits(lHits3, 0) == 0xFFFFFFFFu && LaneBits(lHits3, 1) == 0xFFFFFFFFu
              && LaneBits(lHits3, 2) == 0u && LaneBits(lHits3, 3) == 0xFFFFFFFFu,
              "C3 TestLineSphere4: lane k uses lane k of the length vector (L = {10,10,1,10} -> hit,hit,miss,hit)");

        // The :305 twin stores the same masks and asserts on a misaligned results pointer.
        alignas(16) s32 laiResults[8] = { 7, 7, 7, 7, 7, 7, 7, 7 };
        gaAsserts.clear();
        CgsGeometric::TestLineSphere4(MakeSphere(5, 2, 0, 3), MakeSphere(5, 3.01f, 0, 3), MakeSphere(-2, 0, 0, 3),
                                      MakeSphere(15, 0, 0, 3), lStart, lDir, lLength, laiResults);
        const bool lbAlignedQuiet = gaAsserts.empty();
        Check(laiResults[0] == -1 && laiResults[1] == 0 && laiResults[2] == -1 && laiResults[3] == 0 && laiResults[4] == 7
              && lbAlignedQuiet,
              "C4 TestLineSphere4 (:305): four s32 masks {-1,0,-1,0} stored, nothing past them, no assert on a 16-byte-aligned array");
        CgsGeometric::TestLineSphere4(MakeSphere(5, 2, 0, 3), MakeSphere(5, 2, 0, 3), MakeSphere(5, 2, 0, 3),
                                      MakeSphere(5, 2, 0, 3), lStart, lDir, lLength, laiResults + 1);
        Check(gaAsserts.size() == 1 && gaAsserts[0] == "Results must be 16 byte aligned\n"
              && laiResults[1] == -1 && laiResults[4] == -1,
              "C5 TestLineSphere4 (:305): a misaligned results pointer fires \"Results must be 16 byte aligned\\n\" (:308) and still stores");
    }

    // ================================================================================================
    // D. TestLineBoundingBoxAgainstAxisAlignedBox4 -- the per-axis slab test
    // ================================================================================================
    {
        // Segment (0,0,0) -> (10,0,0): reciprocal {0.1, 1e7, 1e7} (the y/z lanes are the 1/eps clamp).
        const Vector3 lStart = V3(0, 0, 0, 0), lEnd = V3(10, 0, 0, 0), lRecip = V3(0.1f, 1.0e7f, 1.0e7f, 0);
        gaAsserts.clear();
        const Vector4 lHits = CgsGeometric::TestLineBoundingBoxAgainstAxisAlignedBox4(
            MakeBox(4, -1, -1, 6, 1, 1),        // lane 0: straddles the segment                        -> crossed
            MakeBox(-5, -1, -1, 0, 1, 1),       // lane 1: ends exactly at the start (far == 0)         -> crossed
            MakeBox(10, -1, -1, 12, 1, 1),      // lane 2: starts exactly at the end (near == 1)        -> crossed
            MakeBox(10.5f, -1, -1, 12, 1, 1),   // lane 3: beyond the end                               -> missed
            lStart, lEnd, lRecip);
        Check(LaneBits(lHits, 0) == 0xFFFFFFFFu && LaneBits(lHits, 1) == 0xFFFFFFFFu
              && LaneBits(lHits, 2) == 0xFFFFFFFFu && LaneBits(lHits, 3) == 0u && gaAsserts.empty(),
              "D1 TestLineBoundingBoxAgainstAxisAlignedBox4: touching at t == 0 and t == 1 counts, a box past the end misses, no assert");

        const Vector4 lHits2 = CgsGeometric::TestLineBoundingBoxAgainstAxisAlignedBox4(
            MakeBox(4, 2, -1, 6, 3, 1),         // lane 0: above the segment (y slab never entered)       -> missed
            MakeBox(4, -1, -3, 6, 1, -2),       // lane 1: beside it in z                                 -> missed
            MakeBox(-3, -1, -1, -1, 1, 1),      // lane 2: behind the start                               -> missed
            MakeBox(0, 0, 0, 0, 0, 0),          // lane 3: a point box at the start                        -> crossed
            lStart, lEnd, lRecip);
        Check(LaneBits(lHits2, 0) == 0u && LaneBits(lHits2, 1) == 0u && LaneBits(lHits2, 2) == 0u
              && LaneBits(lHits2, 3) == 0xFFFFFFFFu,
              "D2 TestLineBoundingBoxAgainstAxisAlignedBox4: a box off the y / z slab or behind the start misses; a point box at the start is crossed");

        gaAsserts.clear();
        const CgsGeometric::AxisAlignedBox lBox = MakeBox(4, -1, -1, 6, 1, 1);
        CgsGeometric::TestLineBoundingBoxAgainstAxisAlignedBox4(lBox, lBox, lBox, lBox, lStart, lEnd, V3(0, 0, 0, 0));
        Check(gaAsserts.size() == 3 && gaAsserts[0] == "Line reciprocal X is 0\n"
              && gaAsserts[1] == "Line reciprocal Y is 0\n" && gaAsserts[2] == "Line reciprocal Z is 0\n",
              "D3 TestLineBoundingBoxAgainstAxisAlignedBox4: a zero reciprocal lane fires :769 / :770 / :771 in x, y, z order");

        // vmaxfp / vminfp, not C selects (0x828BD99C..0x828BD9BC): a NaN slab parameter makes far AND near NaN, so
        // the axis's `1 >= near` lane (vcmpgefp128 0x828BD9B0) fails. Lane 0: x max NaN -> t_max NaN (the vA
        // operand) beside t_min = 0.5 -- the selects answered far = near = 0.5 and crossed it. Lane 1: x min NaN
        // (the vB operand). Lanes 2 / 3: finite controls (crossed / past the end).
        const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
        const Vector4 lHits3 = CgsGeometric::TestLineBoundingBoxAgainstAxisAlignedBox4(
            MakeBox(5, -1, -1, lfNaN, 1, 1), MakeBox(lfNaN, -1, -1, 6, 1, 1),
            MakeBox(5, -1, -1, 6, 1, 1), MakeBox(11, -1, -1, 12, 1, 1), lStart, lEnd, lRecip);
        Check(LaneBits(lHits3, 0) == 0u && LaneBits(lHits3, 1) == 0u
              && LaneBits(lHits3, 2) == 0xFFFFFFFFu && LaneBits(lHits3, 3) == 0u,
              "D4 TestLineBoundingBoxAgainstAxisAlignedBox4: a NaN slab parameter in either operand fails its axis (vmaxfp / vminfp answer NaN)");
    }

    // ================================================================================================
    // E. LineTestOptimized -- the parameter block, the short-segment arm, the root gate, the answer
    // ================================================================================================
    const u32 KX_QUERY = 0x1E;   // the camera VisibilityTest's flags (FX-DIRECTOR2's closure run)
    {
        LooseOctree* lpTree = MakeTree();
        SetNode(0, 0, 0, 0, 100, 50, 0, 0xFFFFFFFFu);
        CoarseQueryResultBuffer<16384>* lpBuffer = NewBuffer();

        // A segment whose y delta is 0, whose x delta is a tiny NEGATIVE value and whose z delta is
        // ordinary: the length comes from z.
        giCapturedWalks = 0; gaSphereCalls.clear(); gaAsserts.clear();
        const Vector3 lA = V3(0.0f, 1.0f, -80.0f, 0.0f), lB = V3(-5.0e-8f, 1.0f, 40.0f, 0.0f);
        const bool lbAnswer = lpTree->LineTestOptimized(KX_QUERY, lA, lB, lpBuffer);
        const Params& p = gCapturedParams;
        std::printf("      E1: len %.9g dir (%.9g %.9g %.9g) recip (%.9g %.9g %.9g %.9g)\n", p.mfLineLength.x,
                    p.mLineDirection.x, p.mLineDirection.y, p.mLineDirection.z,
                    p.mLineReciprocal.x, p.mLineReciprocal.y, p.mLineReciprocal.z, p.mLineReciprocal.w);
        Check(giCapturedWalks == 1 && gu16CapturedNode == 0 && gaSphereCalls.empty(),
              "E1 LineTestOptimized: a long segment inside the root box walks from node 0 once (no SphereTest)");
        Check(p.mLineStart.x == lA.x && p.mLineStart.y == lA.y && p.mLineStart.z == lA.z
              && p.mLineEnd.x == lB.x && p.mLineEnd.y == lB.y && p.mLineEnd.z == lB.z
              && p.mx32EntityTypeFlags == KX_QUERY && p.mpResultBufferOut == lpBuffer,
              "E2 LineTestOptimized: +0x00 start, +0x10 end, +0x50 flags, the result buffer");
        const f32 lfLen = std::sqrt((lB.x - lA.x) * (lB.x - lA.x) + (lB.z - lA.z) * (lB.z - lA.z));
        Check(Near(p.mfLineLength.x, lfLen, lfLen * 1e-6f) && p.mfLineLength.y == p.mfLineLength.x
              && p.mfLineLength.z == p.mfLineLength.x && p.mfLineLength.w == p.mfLineLength.x,
              "E3 LineTestOptimized: +0x40 the length, splatted into all four lanes");
        Check(Near(p.mLineDirection.z, 1.0f, 1e-6f) && p.mLineDirection.y == 0.0f && std::fabs(p.mLineDirection.x) < 1e-9f,
              "E4 LineTestOptimized: +0x20 the unit direction (end - start) / length");
        Check(Near(p.mLineReciprocal.z, 1.0f / 120.0f, 1e-9f) && Near(p.mLineReciprocal.y, 1.0e7f, 20.0f)
              && Near(p.mLineReciprocal.x, 1.0e7f, 20.0f) && p.mLineReciprocal.x > 0.0f && Near(p.mLineReciprocal.w, 1.0e7f, 20.0f),
              "E5 LineTestOptimized: +0x30 1/d, but +1/eps (eps = 1e-7) for |d| < eps -- positive even for the tiny NEGATIVE x delta");
        Check(!lbAnswer && lpBuffer->GetNumResultsAttempted() == 0,
              "E6 LineTestOptimized: an empty tree answers false (GetNumResultsAttempted() == 0)");

        // |d| exactly eps is NOT tiny (vnot(vcmpgefp d >= eps)): 1/d; -eps is not tiny either (d > -eps fails).
        giCapturedWalks = 0;
        lpTree->LineTestOptimized(KX_QUERY, V3(0, 0, 0), V3(1.0e-7f, -1.0e-7f, 50.0f), lpBuffer);
        Check(giCapturedWalks == 1 && Near(gCapturedParams.mLineReciprocal.x, 1.0e7f, 20.0f)
              && Near(gCapturedParams.mLineReciprocal.y, -1.0e7f, 20.0f),
              "E7 LineTestOptimized: d == +eps and d == -eps take 1/d (+1e7 / -1e7) -- the eps window is open at both ends");

        // The short-segment arm: length <= 1e-4 is answered by SphereTest(flags, start, 1e-4, buffer).
        giCapturedWalks = 0; gaSphereCalls.clear(); gbSphereAnswer = true;
        const Vector3 lC = V3(3.0f, 4.0f, 5.0f, 0.0f);
        const bool lbShort = lpTree->LineTestOptimized(KX_QUERY, lC, V3(3.0f + 5.0e-5f, 4.0f, 5.0f, 0.0f), lpBuffer);
        Check(lbShort && giCapturedWalks == 0 && gaSphereCalls.size() == 1 && gaSphereCalls[0].mx32Flags == KX_QUERY
              && gaSphereCalls[0].mCentre.x == 3.0f && gaSphereCalls[0].mCentre.y == 4.0f && gaSphereCalls[0].mCentre.z == 5.0f
              && Bits(gaSphereCalls[0].mfRadius) == 0x38D1B717u && gaSphereCalls[0].mpBuffer == lpBuffer,
              "E8 LineTestOptimized: a 5e-5 segment goes to SphereTest(flags, start, 1e-4 == 0x38D1B717, buffer) and returns its answer");
        gaSphereCalls.clear(); gbSphereAnswer = false;
        const bool lbZero = lpTree->LineTestOptimized(KX_QUERY, lC, lC, lpBuffer);
        Check(!lbZero && giCapturedWalks == 0 && gaSphereCalls.size() == 1,
              "E9 LineTestOptimized: a zero-length segment (len2 == 0 -> length 0) also goes to SphereTest");

        // The root gate: a segment far above the root's loose box never walks.
        giCapturedWalks = 0; gaSphereCalls.clear();
        const bool lbAbove = lpTree->LineTestOptimized(KX_QUERY, V3(-50, 500, 0), V3(50, 500, 0), lpBuffer);
        Check(!lbAbove && giCapturedWalks == 0 && gaSphereCalls.empty(),
              "E10 LineTestOptimized: a segment that misses the root box never walks and answers false");
        delete lpBuffer;
    }

    // ================================================================================================
    // F. LineTestRecursive -- batches, masks, children (through LineTestOptimized)
    // ================================================================================================
    {
        LooseOctree* lpTree = MakeTree();
        // Root (node 0) and its four quadrant children (nodes 1..4). Node 2's sub-tree mask misses the
        // query; node 4's own mask misses it (its sub-tree mask does not).
        SetNode(0,   0, 0,   0, 100, 50, 0x06, 0xFFFFFFFFu);
        SetNode(1, -50, 0, -50,  50, 50, 0x02, 0x02);
        SetNode(2,  50, 0, -50,  50, 50, 0x02, 0x40);
        SetNode(3, -50, 0,  50,  50, 50, 0x04, 0x04);
        SetNode(4,  50, 0,  50,  50, 50, 0x00, 0x02);
        gaNodes[0].muFirstChildIndex = 1;

        // Segment A -> B, direction (1, 0, 1) / sqrt2, length 100 sqrt2.
        const Vector3 lA = V3(-10, 0, -90), lB = V3(90, 0, 10);
        const f32 u = 0.70710677f;
        auto At = [&](f32 t, f32 dy) { return MakeSphere(-10 + u * t, dy, -90 + u * t, 0); };
        auto WithR = [](CgsGeometric::Sphere s, f32 r) { s.mPositionRadius.w = r; return s; };
        const f32 L = 141.421356f;
        FileChain(lpTree, 0, {
            { 100, 0x02, WithR(At(L * 0.5f, 5.0f), 6.0f) },     // e0: on-segment, perp 5 <= 6          -> hit
            { 101, 0x40, WithR(At(L * 0.5f, 0.0f), 6.0f) },     // e1: link mask misses the query        -> skipped (no slot)
            { 102, 0x04, WithR(At(L + 20.0f, 0.0f), 5.0f) },    // e2: 20 past the end, end outside       -> miss
            { 103, 0x02, WithR(At(-3.0f, 0.0f), 5.0f) },        // e3: 3 behind the start, start inside   -> hit
            { 104, 0x02, WithR(At(L + 3.0f, 0.0f), 5.0f) },     // e4: 3 past the end, end inside         -> hit
            { 105, 0x04, WithR(At(-20.0f, 0.0f), 5.0f) },       // e5: 20 behind the start, start outside -> miss (remainder)
            { 106, 0x02, WithR(At(L * 0.25f, 4.0f), 4.5f) },    // e6: on-segment, perp 4 <= 4.5         -> hit  (remainder)
        });
        FileChain(lpTree, 1, { { 107, 0x02, MakeSphere(-30, 0, -40, 60) } });   // node 1: hit if walked
        FileChain(lpTree, 2, { { 108, 0x02, MakeSphere( 50, 0, -50, 20) } });   // node 2: hit if walked (disabled)
        FileChain(lpTree, 3, { { 109, 0x04, MakeSphere(-50, 0,  50, 130) } });  // node 3: hit if walked
        FileChain(lpTree, 4, { { 110, 0x02, MakeSphere( 50, 0,  50, 60) } });   // node 4: hit if tested (node mask off)

        CoarseQueryResultBuffer<16384>* lpBuffer = NewBuffer();
        gaAsserts.clear(); giCapturedWalks = 0;
        const bool lbAnswer = lpTree->LineTestOptimized(KX_QUERY, lA, lB, lpBuffer);
        const std::vector<u16> laPushed = Pushed(lpBuffer);
        PrintList("F pushed", laPushed);
        Check(SameList(laPushed, { 100, 103, 104, 106, 107, 109 }),
              "F1 LineTestRecursive: root batch {e0,e2,e3,e4} pushes 100,103,104 in lane order, the remainder {e5,e6} pushes 106, "
              "then child 0 (107) and child 2 (109) -- child 1 (sub-tree mask) and child 3's entities (node mask) never pushed");
        Check(lbAnswer && lpBuffer->GetNumResultsAttempted() == 6 && gaAsserts.empty(),
              "F2 LineTestOptimized: answers true (6 attempted), no assert on the walk");

        // Node 3 is walked although the SEGMENT never enters it (x <= 0 needs t <= 0.1, z >= 0 needs
        // t >= 0.9): the console's per-axis test only asks that each axis range overlaps.
        Check(std::find(laPushed.begin(), laPushed.end(), static_cast<u16>(109)) != laPushed.end(),
              "F3 LineTestRecursive: the child slab test is PER AXIS (a child the segment's axis ranges overlap is walked)");

        // The chain is walked muNumElements times: a count of 2 on a chain of three hits pushes two.
        lpTree->mpNodes[0].muFirstChildIndex = KU_INVALID_NODE;
        FileChain(lpTree, 0, {
            { 200, 0x02, WithR(At(L * 0.5f, 0.0f), 1.0f) },
            { 201, 0x02, WithR(At(L * 0.6f, 0.0f), 1.0f) },
            { 202, 0x02, WithR(At(L * 0.7f, 0.0f), 1.0f) },
        }, 2u);
        CoarseQueryResultBuffer<16384>* lpBuffer2 = NewBuffer();
        gaAsserts.clear();
        lpTree->LineTestOptimized(KX_QUERY, lA, lB, lpBuffer2);
        PrintList("F4 pushed", Pushed(lpBuffer2));
        Check(SameList(Pushed(lpBuffer2), { 200, 201 }) && gaAsserts.empty(),
              "F4 LineTestRecursive: the node's chain is walked muNumElements (2) times, not to its end");

        // Two full batches plus a remainder of two: 4 + 4 + 2 matching entities, all hits, pushed in chain order.
        std::vector<EntitySpec> laMany;
        for (u16 i = 0; i < 10; ++i) laMany.push_back({ static_cast<u16>(300 + i), 0x02, WithR(At(L * (0.05f + 0.09f * i), 0.0f), 1.0f) });
        FileChain(lpTree, 0, laMany);
        CoarseQueryResultBuffer<16384>* lpBuffer3 = NewBuffer();
        lpTree->LineTestOptimized(KX_QUERY, lA, lB, lpBuffer3);
        Check(SameList(Pushed(lpBuffer3), { 300, 301, 302, 303, 304, 305, 306, 307, 308, 309 }),
              "F5 LineTestRecursive: ten hits (two full batches + a remainder of two) are pushed in chain order");

        // The node mask gates the node's own chain even when every link matches.
        gaNodes[0].mxNodeEntityFlags = 0x40;
        CoarseQueryResultBuffer<16384>* lpBuffer4 = NewBuffer();
        const bool lbGated = lpTree->LineTestOptimized(KX_QUERY, lA, lB, lpBuffer4);
        Check(!lbGated && Pushed(lpBuffer4).empty(),
              "F6 LineTestRecursive: a node whose own mask misses the query tests none of its entities");
        delete lpBuffer; delete lpBuffer2; delete lpBuffer3; delete lpBuffer4;
    }

    std::printf("FxFollowupsOctreeLine: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures == 0 ? 0 : 1;
}
