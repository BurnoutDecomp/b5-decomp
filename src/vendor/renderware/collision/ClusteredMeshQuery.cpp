#include "vendor/renderware/collision/ClusteredMeshQuery.hpp"

#include <cmath>     // sqrt
#include <cstddef>   // offsetof

// ===========================================================================
// rw::collision clustered-mesh query helpers -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (dedicated VMX pass; hand-vectorised bodies lowered
// to portable scalar lane maths per the committed Feature / FeatureEdge /
// AALineClipper precedent; store order, branch polarity and every
// caller-visible store preserved).
//
//   rw::collision::AA              @ 0x82BB17E0
//   rw::collision::ComputeEdgeCos  @ 0x82BB13A0
//   rw::collision::AddQueryResult  @ 0x82BB1588
// ===========================================================================

namespace rw
{
namespace collision
{

// ===========================================================================
// rw::collision::AA @ 0x82BB17E0
//
// Compute the axis-aligned bounding box of ONE clustered-mesh unit (a
// triangle or a quad), decompressing its 3 / 4 vertices from whichever of the
// three cluster vertex encodings is in use, and min/max-folding them per
// lane.
//
// Register contract (X360 PPC):
//     r3 = hidden return slot (32-byte AABBox: min row @+0x00, max row @+0x10)
//     r4 = the ClusteredMesh   (+0x34 cluster-offset table, +0x38 granularity)
//     r5 = cluster index       (word-indexes the offset table)
//     r6 = unit byte offset    (into the cluster's unit-data stream)
//
// Cluster addressing (matches the committed ClusteredMeshCluster view):
//     cluster  = (u8*)mesh + meshClusterOffsetTable[clusterIndex]
//     unit     = cluster + 16*(muUnitCount + 1) + unitOffset
//     vertices = cluster + 0x10 (one 16-byte header row precedes them)
// Unit record: [0] flags byte, low nibble == 2 selects the QUAD path (four
// vertex-index bytes at [1..4]); any other nibble takes the TRIANGLE path
// (three index bytes at [1..3]).
//
// Vertex encodings (cluster byte +0x0C):
//     1 = 16-bit compressed: one 16-byte row of 32-bit lane offsets at +0x10,
//         then 6-byte u16 xyz triplets from +0x1C;
//         lane = float(offset[lane] + u16word[lane]) * granularity
//     2 = 32-bit compressed: 12-byte s32 xyz triplets from +0x10;
//         lane = float(s32word[lane]) * granularity
//     else = uncompressed: aligned 16-byte float rows, vertex i at
//         cluster + 16*(i + 1)
// The cluster-relative byte offsets are raw serialised rw::collision data
// (external blob layout), accessed by documented offset per the project's
// serialised-data exception.
//
// All four lanes are computed and stored exactly as the asm does: lane 3 (w)
// of a compressed vertex is a decode artifact (the neighbouring vertex's
// leading bytes run through the same pipeline) and its min/max lands in the
// stored rows; it is reproduced, not sanitised.
//
// VMX lowering: lvx128 aligned loads; lvlx/lvrx+vor unaligned loads (the 6-
// and 12-byte strides are not quadword aligned); vspltisb 0 + vmrghh
// zero-extend of the u16 words; vaddsws lane += 32-bit offset (saturating in
// the asm; the operands -- a u16 plus a de-quantise bias -- cannot approach
// the s32 rails, so a plain add is exact); vcfsx int->float; lvlx+vspltw
// granularity splat; vmulfp128; vminfp/vmaxfp folds in the asm's exact
// pairing order; two stvx128 result-row stores.
// ===========================================================================

namespace
{
    // Cluster-relative byte offsets the asm addresses with (all X360-attested).
    const u32 KU_CLUSTER_UNIT_DATA_START   = 0x04; // lhz 4(r10): vertex-block row count
    const u32 KU_CLUSTER_COMPRESSION_MODE  = 0x0C; // lbz 0xC(r10)
    const u32 KU_CLUSTER_VERTEX_DATA       = 0x10; // first row after the header
    const u32 KU_CLUSTER_VERTEX_DATA_16BIT = 0x1C; // u16 triplets (after the offset row)

    // Vertex-compression modes (cmplwi cr6, r9, 1 / 2 dispatch).
    const u32 KU_VERTICES_16BIT_COMPRESSED = 1;
    const u32 KU_VERTICES_32BIT_COMPRESSED = 2;

    // Unit-flags low nibble that selects the four-vertex path.
    const u32 KU_UNIT_TYPE_QUAD = 2;

    // vminfp / vmaxfp -- per-lane float min/max. (The VMX ops quiet-NaN-
    // propagate either operand; collision vertex data is finite, so the plain
    // compare-select is exact here.)
    inline void LaneMin(f32* lpDst, const f32* lpA, const f32* lpB)
    {
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            lpDst[liLane] = (lpA[liLane] < lpB[liLane]) ? lpA[liLane] : lpB[liLane];
        }
    }

    inline void LaneMax(f32* lpDst, const f32* lpA, const f32* lpB)
    {
        for (int liLane = 0; liLane < 4; ++liLane)
        {
            lpDst[liLane] = (lpA[liLane] > lpB[liLane]) ? lpA[liLane] : lpB[liLane];
        }
    }

    // Mode 1 -- 16-bit compressed vertex auIndex.
    //   v13 = lvx128(cluster+0x10)                  the four 32-bit lane offsets
    //         (lane 3 is the row's tail word: the first two u16s of the stream)
    //   data = lvlx/lvrx+vor @ cluster+0x1C+6*index unaligned 16-byte load
    //   v    = vmrghh(0, data)                      zero-extend u16 words to lanes
    //   v    = vaddsws(v, v13); vcfsx(v);           bias, int->float
    //   v    = vmulfp128(v, splat(granularity))
    void LoadVertex16Bit(f32* lpLanes, const u8* lpCluster, u32 auIndex,
                         f32 afGranularity)
    {
        const s32* lpOffsets =
            reinterpret_cast<const s32*>(lpCluster + KU_CLUSTER_VERTEX_DATA);
        const u8* lpVertex = lpCluster + KU_CLUSTER_VERTEX_DATA_16BIT + 6u * auIndex;

        for (int liLane = 0; liLane < 4; ++liLane)
        {
            const u16 luWord = *reinterpret_cast<const u16*>(lpVertex + 2 * liLane);
            // vaddsws (plain add is exact for this data), vcfsx, vmulfp128.
            lpLanes[liLane] =
                static_cast<f32>(lpOffsets[liLane] + static_cast<s32>(luWord))
                * afGranularity;
        }
    }

    // Mode 2 -- 32-bit compressed vertex auIndex.
    //   data = lvlx/lvrx+vor @ cluster+0x10+12*index (lane 3 = next vertex's x)
    //   v    = vcfsx(data); v = vmulfp128(v, splat(granularity))
    void LoadVertex32Bit(f32* lpLanes, const u8* lpCluster, u32 auIndex,
                         f32 afGranularity)
    {
        const u8* lpVertex = lpCluster + KU_CLUSTER_VERTEX_DATA + 12u * auIndex;

        for (int liLane = 0; liLane < 4; ++liLane)
        {
            const s32 liWord = *reinterpret_cast<const s32*>(lpVertex + 4 * liLane);
            lpLanes[liLane] = static_cast<f32>(liWord) * afGranularity;
        }
    }

    // Default -- uncompressed vertex auIndex: lvx128 @ cluster + 16*(index+1)
    // (an aligned four-float row; no granularity multiply).
    void LoadVertexUncompressed(f32* lpLanes, const u8* lpCluster, u32 auIndex)
    {
        const f32* lpVertex = reinterpret_cast<const f32*>(
            lpCluster + 16u * (auIndex + 1u));

        for (int liLane = 0; liLane < 4; ++liLane)
        {
            lpLanes[liLane] = lpVertex[liLane];
        }
    }
}

AABBox AA(const ClusteredMesh* lpMesh, u32 auClusterIndex, u32 auUnitOffset)
{
    const u8* lpMeshBytes = reinterpret_cast<const u8*>(lpMesh);

    // r10 = mesh + (*(mesh+0x34))[clusterIndex]
    const u8* lpCluster = lpMeshBytes + lpMesh->mpuClusterOffsets[auClusterIndex];

    // r11 = cluster + 16*(lhz(cluster+4) + 1) + unitOffset -- the unit record
    // sits past the header row and the muUnitCount 16-byte vertex-block rows.
    const u16 luUnitDataStart = *reinterpret_cast<const u16*>(
        lpCluster + KU_CLUSTER_UNIT_DATA_START);
    const u8* lpUnit = lpCluster
                     + 16u * (static_cast<u32>(luUnitDataStart) + 1u)
                     + auUnitOffset;

    // r9 = lbz(cluster+0xC): the vertex-compression mode.
    const u32 luMode = lpCluster[KU_CLUSTER_COMPRESSION_MODE];

    // lfGranularity = splat(*(f32*)(mesh+0x38))  (lvlx + vspltw v,v,0).
    const f32 lfGranularity = lpMesh->mfVertexCompressionGranularity;

    AABBox lResult;
    f32* lafResultMin = lResult.mMin.mV.mafLane;
    f32* lafResultMax = lResult.mMax.mV.mafLane;

    if ((lpUnit[0] & 0xFu) == KU_UNIT_TYPE_QUAD)
    {
        // QUAD: four vertex-index bytes (lbz 1..4(r11), each clrlwi'd to u8).
        const u32 luIndex0 = lpUnit[1];
        const u32 luIndex1 = lpUnit[2];
        const u32 luIndex2 = lpUnit[3];
        const u32 luIndex3 = lpUnit[4];

        f32 laafVertex[4][4];
        if (luMode == KU_VERTICES_16BIT_COMPRESSED)
        {
            LoadVertex16Bit(laafVertex[0], lpCluster, luIndex0, lfGranularity);
            LoadVertex16Bit(laafVertex[1], lpCluster, luIndex1, lfGranularity);
            LoadVertex16Bit(laafVertex[2], lpCluster, luIndex2, lfGranularity);
            LoadVertex16Bit(laafVertex[3], lpCluster, luIndex3, lfGranularity);
        }
        else if (luMode == KU_VERTICES_32BIT_COMPRESSED)
        {
            LoadVertex32Bit(laafVertex[0], lpCluster, luIndex0, lfGranularity);
            LoadVertex32Bit(laafVertex[1], lpCluster, luIndex1, lfGranularity);
            LoadVertex32Bit(laafVertex[2], lpCluster, luIndex2, lfGranularity);
            LoadVertex32Bit(laafVertex[3], lpCluster, luIndex3, lfGranularity);
        }
        else
        {
            LoadVertexUncompressed(laafVertex[0], lpCluster, luIndex0);
            LoadVertexUncompressed(laafVertex[1], lpCluster, luIndex1);
            LoadVertexUncompressed(laafVertex[2], lpCluster, luIndex2);
            LoadVertexUncompressed(laafVertex[3], lpCluster, luIndex3);
        }

        // Exact asm fold order:
        //   v9  = vminfp(v0, v1); v12 = vminfp(v2, v3); min = vminfp(v9, v12)
        //   v13 = vmaxfp(v0, v1); v0  = vmaxfp(v2, v3); max = vmaxfp(v13, v0)
        f32 lafMin01[4], lafMin23[4], lafMax01[4], lafMax23[4];
        LaneMin(lafMin01, laafVertex[0], laafVertex[1]);
        LaneMin(lafMin23, laafVertex[2], laafVertex[3]);
        LaneMax(lafMax01, laafVertex[0], laafVertex[1]);
        LaneMax(lafMax23, laafVertex[2], laafVertex[3]);
        LaneMin(lafResultMin, lafMin01, lafMin23);   // stvx128 v12, 0, r3
        LaneMax(lafResultMax, lafMax01, lafMax23);   // stvx128 v0, r3, 0x10
    }
    else
    {
        // TRIANGLE: three vertex-index bytes (lbz 1..3(r11)).
        const u32 luIndex0 = lpUnit[1];
        const u32 luIndex1 = lpUnit[2];
        const u32 luIndex2 = lpUnit[3];

        f32 laafVertex[3][4];
        if (luMode == KU_VERTICES_16BIT_COMPRESSED)
        {
            LoadVertex16Bit(laafVertex[0], lpCluster, luIndex0, lfGranularity);
            LoadVertex16Bit(laafVertex[1], lpCluster, luIndex1, lfGranularity);
            LoadVertex16Bit(laafVertex[2], lpCluster, luIndex2, lfGranularity);
        }
        else if (luMode == KU_VERTICES_32BIT_COMPRESSED)
        {
            LoadVertex32Bit(laafVertex[0], lpCluster, luIndex0, lfGranularity);
            LoadVertex32Bit(laafVertex[1], lpCluster, luIndex1, lfGranularity);
            LoadVertex32Bit(laafVertex[2], lpCluster, luIndex2, lfGranularity);
        }
        else
        {
            LoadVertexUncompressed(laafVertex[0], lpCluster, luIndex0);
            LoadVertexUncompressed(laafVertex[1], lpCluster, luIndex1);
            LoadVertexUncompressed(laafVertex[2], lpCluster, luIndex2);
        }

        // Exact asm fold order:
        //   v11 = vminfp(v0, v1); min = vminfp(v11, v2)
        //   v0  = vmaxfp(v0, v1); max = vmaxfp(v0, v2)
        f32 lafMin01[4], lafMax01[4];
        LaneMin(lafMin01, laafVertex[0], laafVertex[1]);
        LaneMax(lafMax01, laafVertex[0], laafVertex[1]);
        LaneMin(lafResultMin, lafMin01, laafVertex[2]);   // stvx128 v13, 0, r3
        LaneMax(lafResultMax, lafMax01, laafVertex[2]);   // stvx128 v0, r3, 0x10
    }

    return lResult;
}

// ===========================================================================
// rw::collision::ComputeEdgeCos @ 0x82BB13A0
//
// Given two triangles sharing the edge (EdgeStart, EdgeEnd):
//     triangle A = (ApexA, EdgeStart, EdgeEnd)     normal nA
//     triangle B = (ApexB, EdgeEnd,   EdgeStart)   normal nB (opposed winding)
// returns cos(angle between nA and nB) and stores a one-byte convexity flag
// through the first argument:
//     0x20  when dot(EdgeEnd - EdgeStart, cross(nA, nB)) > 0  (convex edge)
//     0x00  otherwise
// The flag byte is written unconditionally BEFORE the degeneracy early-outs.
// When either triangle normal is degenerate (squared length <= FLT_MIN) the
// function returns 1.0f (a flat edge) without normalising.
//
// .rdata constants (all valued): flt_82001CC0 = 0.0f (convexity compare),
// flt_82001C98 = 1.0f (default return), flt_821805BC = 1.1754944e-38
// (FLT_MIN degeneracy threshold; value folded by the decompiler).
//
// Asm shape (in order): lvx128 x4; vsubfp x5 edge-vector fan-out; two
// vpermwi128-YZXW cross blocks (triangle normals); cross(nA, nB);
// vmsum3fp128 + vcmpgtfp. + mfocrf + rlwinm + stb convexity byte;
// vmsum3fp128 x2 + fcmpu/blelr x2 degeneracy early-outs (ret 1.0);
// vmsum3fp128 dot(nA, nB); vrsqrtefp + 2x NR refine x2 (1/|nA|, 1/|nB|);
// vmulfp128 x2 + lfs f1 final combine.
// ===========================================================================

// Degenerate-normal threshold (X360 flt_821805BC): the smallest positive
// normalised float. A squared normal length must EXCEED it to be normalised.
static const f32 KF_MIN_NORMAL_LENSQ = 1.1754944e-38f;

namespace
{
    // vsubfp: per-lane subtract (w carried, unused downstream).
    inline Vec4 SubRow(const Vec4& lA, const Vec4& lB)
    {
        Vec4 lResult;
        lResult.x = lA.x - lB.x;
        lResult.y = lA.y - lB.y;
        lResult.z = lA.z - lB.z;
        lResult.w = lA.w - lB.w;
        return lResult;
    }

    // dot3 of the xyz lanes (the asm's vmsum3fp128).
    inline f32 Dot3(const Vec4& lA, const Vec4& lB)
    {
        return lA.x * lB.x + lA.y * lB.y + lA.z * lB.z;
    }

    // 3D cross product (the vpermwi128 0x63 / vmulfp128 / vnmsubfp idiom).
    // The w lane of the VMX result is garbage and never read (the dot3 folds
    // ignore it); it is zeroed here.
    inline Vec4 CrossRow(const Vec4& lA, const Vec4& lB)
    {
        Vec4 lResult;
        lResult.x = lA.y * lB.z - lA.z * lB.y;
        lResult.y = lA.z * lB.x - lA.x * lB.z;
        lResult.z = lA.x * lB.y - lA.y * lB.x;
        lResult.w = 0.0f;
        return lResult;
    }
}

f32 ComputeEdgeCos(u8* lpConvexFlag,
                   const Vec4* lpApexA,
                   const Vec4* lpEdgeStart,
                   const Vec4* lpEdgeEnd,
                   const Vec4* lpApexB)
{
    // vsubfp fan-out (v10/v12/v9/v11/v7 in the asm).
    const Vec4 lvStartFromB = SubRow(*lpEdgeStart, *lpApexB);     // v10
    const Vec4 lvEndFromB   = SubRow(*lpEdgeEnd,   *lpApexB);     // v12
    const Vec4 lvEndFromA   = SubRow(*lpEdgeEnd,   *lpApexA);     // v9
    const Vec4 lvStartFromA = SubRow(*lpEdgeStart, *lpApexA);     // v11
    const Vec4 lvEdgeDir    = SubRow(*lpEdgeEnd,   *lpEdgeStart); // v7

    // Triangle normals (the two vpermwi128-YZXW cross blocks):
    //   nA = (start - apexA) x (end - apexA)     triangle (apexA, start, end)
    //   nB = (end - apexB) x (start - apexB)     triangle (apexB, end, start)
    const Vec4 lvNormalA = CrossRow(lvStartFromA, lvEndFromA);    // v13
    const Vec4 lvNormalB = CrossRow(lvEndFromB,   lvStartFromB);  // v0

    // Convexity flag -- written unconditionally, BEFORE the early-outs.
    // vmsum3fp128(edgeDir, cross(nA, nB)) ; vcmpgtfp. against 0.0f
    // (flt_82001CC0) ; mfocrf/rlwinm distil CR6 "all lanes true" into 0x20.
    // The dot is lane-broadcast so all-true == the scalar compare (a NaN dot
    // compares false in both renderings).
    const Vec4 lvNormalCross = CrossRow(lvNormalA, lvNormalB);    // v12
    *lpConvexFlag = (Dot3(lvEdgeDir, lvNormalCross) > 0.0f) ? KU_EDGE_CONVEX_FLAG
                                                            : 0;

    // Squared normal lengths (vmsum3fp128 v12,v13,v13 / v11,v0,v0), spilled
    // and re-read as scalars for the fcmpu/blelr early-outs. blelr returns
    // the preloaded f1 = 1.0f (flt_82001C98); an unordered (NaN) compare
    // falls through in both renderings. A is tested first, then B.
    const f32 lfLenSqA = Dot3(lvNormalA, lvNormalA);
    const f32 lfLenSqB = Dot3(lvNormalB, lvNormalB);
    if (lfLenSqA <= KF_MIN_NORMAL_LENSQ)
    {
        return 1.0f;
    }
    if (lfLenSqB <= KF_MIN_NORMAL_LENSQ)
    {
        return 1.0f;
    }

    // dot(nA, nB) (vmsum3fp128 v10,v13,v0), computed before the reciprocal
    // square roots.
    const f32 lfDot = Dot3(lvNormalA, lvNormalB);

    // Reciprocal lengths: vrsqrtefp estimate refined by TWO Newton-Raphson
    // iterations, each  est' = est + (0.5*est) * (1.0 - lenSq*est*est).
    // Two refinements reach full f32 precision, so this is rendered as the
    // equivalent 1/sqrt.
    const f32 lfInvLenB = 1.0f / std::sqrt(lfLenSqB);             // v0
    const f32 lfInvLenA = 1.0f / std::sqrt(lfLenSqA);             // v13

    // Final combine preserves the asm's multiply order:
    //   vmulfp128 v13, v13, v10   (1/|nA|) * dot
    //   vmulfp128 v0,  v13, v0    ... * (1/|nB|)
    return (lfInvLenA * lfDot) * lfInvLenB;
}

// ===========================================================================
// rw::collision::AddQueryResult @ 0x82BB1588
//
// Appends one triangle hit to a VolumeLineQuery's result list:
//   * fills a 208-byte VolumeLineSegIntersectResult record (the exact stride
//     the committed VolumeLineQuery::Initialize @ 0x82BB3888 lays the result
//     list out with), whose +0x50 sub-block is the committed 0x80-stride
//     VolRef record;
//   * transforms the local-space hit position into world space with a
//     vspltw + vmaddfp VMX chain over the instance transform rows;
//   * recomputes the world normal via TriangleVolume::GetNormal;
//   * composes the clustered-mesh unit tag with the query's parent aggregate
//     tag;
//   * returns 1 only while the query still has room for MORE results, 0 once
//     either the result buffer or the instanced-volume pool is full.
// ===========================================================================

namespace
{
    // TU-local view of the VolumeLineQuery fields AddQueryResult touches
    // (offsets attested by the this-relative loads in the 0x82BB1588 asm).
    // It matches the committed layout in src/SDKs/EATech/rwcollision/
    // volumelinequery.cpp where the two overlap (result-list pointer @ +0x10;
    // +0x18 is the active-intersection budget GetAllIntersections seeds,
    // +0xEC the result capacity Initialize seeds) and extends past its end
    // with the two aggregate-tag fields @ +0x104/+0x108. A view struct is used
    // instead of a second class definition to avoid an ODR clash with that TU.
    struct LineQueryView
    {
        u32 muInputVolsBase;             // +0x00  input volume-pointer stack base
        u32 muPad04[2];                  // +0x04..+0x0B
        u32 muCurrInput;                 // +0x0C  1-based cursor into the stack
        u32 muResListPtr;                // +0x10  result-list base (not read here)
        u32 muResCursor;                 // +0x14  results consumed so far
        u32 muResBudget;                 // +0x18  result budget for this pass
        u8  maPad1C[0xE8 - 0x1C];        // +0x1C..+0xE7
        u32 muPoolCursor;                // +0xE8  instanced-volume pool cursor
        u32 muResCapacity;               // +0xEC  instanced-volume pool capacity
        u8  maPadF0[0x104 - 0xF0];       // +0xF0..+0x103
        u32 muTag;                       // +0x104 parent aggregate tag (low bits)
        u8  muNumTagBits;                // +0x108 parent tag bit count
    };

    static_assert(offsetof(LineQueryView, muResCursor)  == 0x14,  "view layout");
    static_assert(offsetof(LineQueryView, muPoolCursor) == 0xE8,  "view layout");
    static_assert(offsetof(LineQueryView, muTag)        == 0x104, "view layout");
    static_assert(offsetof(LineQueryView, muNumTagBits) == 0x108, "view layout");

    // Copy one 16-byte transform row into the VolRef's cached row (the
    // lvx128/stvx128 pair; VolRef carries its own nested row type).
    inline void CopyRow(VolRef::Vec4& rDst, const Vec4& rSrc)
    {
        rDst.x = rSrc.x;
        rDst.y = rSrc.y;
        rDst.z = rSrc.z;
        rDst.w = rSrc.w;
    }
}

int AddQueryResult(VolumeLineQuery*              lpQuery,
                   VolumeLineSegIntersectResult* lpResult,
                   const TriangleVolume*         lpVolume,
                   const Vec4*                   lpTransform,
                   const Vec4&                   rLocalPosition,
                   const Vec4&                   rLineParam,
                   const Vec4&                   rVolParam,
                   const Vec4&                   rNormal,
                   const u32*                    lpClusterId,
                   const u32*                    lpUnitOffset,
                   const u8*                     lpNumTagBits,
                   const u32*                    lpNumMeshTagBits,
                   const u32*                    lpNumClusterTagBits)
{
    const LineQueryView* lpView = reinterpret_cast<const LineQueryView*>(lpQuery);

    // lwz r23, -4(...): the input volume the query is currently walking --
    // inputVols[currInput - 1] (word @+0x00 = stack base, word @+0x0C = cursor).
    const u32* lpInputVols =
        reinterpret_cast<const u32*>(static_cast<uintptr_t>(lpView->muInputVolsBase));
    const u32 luInputVolume = lpInputVols[lpView->muCurrInput - 1];

    // stw r3 -> +0x50, stw r23 -> +0x00 (store order as on X360). The console
    // pointer words are kept as the u32 image (committed VolRef convention).
    lpResult->vRef.muVolumePtr =
        static_cast<u32>(reinterpret_cast<uintptr_t>(lpVolume));
    lpResult->v = luInputVolume;

    // 4x lvx128/stvx128 (+0x00/+0x10/+0x20/+0x30 -> +0x60/+0x70/+0x80/+0x90):
    // copy the instance transform rows inline, then aim the record's transform
    // pointer at that copy (stw r11 -> +0x54).
    CopyRow(lpResult->vRef.mRow0, lpTransform[0]);
    CopyRow(lpResult->vRef.mRow1, lpTransform[1]);
    CopyRow(lpResult->vRef.mRow2, lpTransform[2]);
    CopyRow(lpResult->vRef.mRow3, lpTransform[3]);
    lpResult->vRef.muTransformPtr =
        static_cast<u32>(reinterpret_cast<uintptr_t>(&lpResult->vRef.mRow0));

    // stvx128 v4 -> +0x20, stvx128 v3 -> +0x30. The staged normal is then
    // recomputed by TriangleVolume::GetNormal below, exactly as the X360 does.
    lpResult->normal   = rNormal;
    lpResult->volParam = rVolParam;

    // stvx128 v2 to its stack home slot + lfs/stfs of its first word -> +0x40:
    // the line parameter is lane X of the v2 vector argument.
    lpResult->lineParam = rLineParam.x;

    // vspltw v0/v13/v12 = splat rLocalPosition lanes X/Y/Z, then the vmaddfp
    // chain folds the transform rows into the world-space position:
    //   v0 = xAxis*pos.x + wAxis;  v0 = yAxis*pos.y + v0;  v0 = zAxis*pos.z + v0
    // stvx128 v0 -> +0x10 (all four lanes stored; per-lane association kept).
    lpResult->position.x =
        lpTransform[2].x * rLocalPosition.z +
        (lpTransform[1].x * rLocalPosition.y +
         (lpTransform[0].x * rLocalPosition.x + lpTransform[3].x));
    lpResult->position.y =
        lpTransform[2].y * rLocalPosition.z +
        (lpTransform[1].y * rLocalPosition.y +
         (lpTransform[0].y * rLocalPosition.x + lpTransform[3].y));
    lpResult->position.z =
        lpTransform[2].z * rLocalPosition.z +
        (lpTransform[1].z * rLocalPosition.y +
         (lpTransform[0].z * rLocalPosition.x + lpTransform[3].z));
    lpResult->position.w =
        lpTransform[2].w * rLocalPosition.z +
        (lpTransform[1].w * rLocalPosition.y +
         (lpTransform[0].w * rLocalPosition.x + lpTransform[3].w));

    // bl rw::collision::TriangleVolume::GetNormal (r3 = the triangle volume,
    // r4 = &lpResult->normal, r5 = the transform rows).
    lpVolume->GetNormal(lpResult->normal, lpTransform);

    // Compose the clustered-mesh unit tag (loads in asm order). Only the
    // subtraction is attested for the shift pair: the cluster id is shifted up
    // by (*lpNumMeshTagBits - *lpNumClusterTagBits) -- the unit-id bit count --
    // added to the word-scaled unit offset, then shifted past the parent
    // aggregate tag bits and OR'd with the parent tag (subf/slw/srwi/add/slw/or).
    const u32 luClusterShift = *lpNumMeshTagBits - *lpNumClusterTagBits;
    u32 luTag = (*lpClusterId << luClusterShift) + (*lpUnitOffset >> 2);
    luTag = (luTag << lpView->muNumTagBits) | lpView->muTag;
    lpResult->vRef.muTag        = luTag;          // stw  -> +0xC0
    lpResult->vRef.muNumTagBits = *lpNumTagBits;  // stb  -> +0xC4

    // Room-for-more check: return 1 only while BOTH the result buffer
    // (+0x14 == +0x18 -> full) and the instanced-volume pool (+0xE8 == +0xEC
    // -> full) still have space; the caller uses 0 as its stop condition.
    if (lpView->muResCursor == lpView->muResBudget)
    {
        return 0;
    }
    if (lpView->muPoolCursor == lpView->muResCapacity)
    {
        return 0;
    }
    return 1;
}

} // namespace collision
} // namespace rw
