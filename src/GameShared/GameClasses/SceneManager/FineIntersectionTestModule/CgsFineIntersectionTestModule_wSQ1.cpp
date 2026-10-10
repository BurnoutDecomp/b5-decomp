// FineIntersectionTestModule scene-query implementations, split during wave SQ1.
// ARTIST: LineTestFine828C7D70, LineTestNearest828C8CC8,
// VolumeTestDeepest828C90D0, VolumeTestFine828C93C8.
// Construct/Prepare and the vendor query walkers are mounted. LineTestFine has both of
// its arms: the primitive-volume arm used by triggers, and the clustered-mesh fast path
// for aggregate volumes.

#include "GameShared/GameClasses/SceneManager/FineIntersectionTestModule/CgsFineIntersectionTestModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"        // EntityId, K_INVALID_ENTITY_ID
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"   // GetEntityIdByIndex / GetFirstEntityVolumeInstance / GetVolumeInstance
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"  // VolumeInstance
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"   // GetVolumeTypeFlags / GetRwVolume
#include "vendor/renderware/collision/CollisionVolume.hpp"          // rw::collision::Volume
#include "vendor/renderware/collision/GPInstance.hpp"               // PrimitivePairIntersectResult
#include "vendor/renderware/collision/LineSegIntersect.hpp"         // VolumeLineSegIntersectResult (the line walk's results)
#include "vendor/renderware/collision/VolumeQuery.hpp"              // rw::collision::VolumeVolumeQuery / VolumeLineQuery
#include "vendor/renderware/collision/ClusteredMesh.hpp"            // rw::collision::ClusteredMesh (the aggregate fast path)
#include "vendor/renderware/collision/ClusteredMeshCluster.hpp"     // rw::collision::ClusteredMeshCluster
#include "vendor/renderware/collision/KdTreeLineQuery.hpp"          // rw::collision::KdTreeLineQuery / KdTree

#include <cmath>     // std::sqrt (the reciprocal-square-root estimate)
#include <cstdint>   // uintptr_t (the serialised mesh's pointer words)
#include <cstring>   // std::memcpy (the vertex words, the cluster parameters)

namespace CgsSceneManager
{
namespace
{
    // =========================================================================================
    // THE CLUSTERED-MESH FAST PATH of ComputeLineTestFine.
    //
    // An aggregate volume (per-type descriptor typeID 6) is taken to wrap an
    // rw::collision::ClusteredMesh, and the console tests the query line against the mesh's
    // triangles itself instead of going through VolumeLineQuery: a kd-tree line walk over the
    // mesh's leaves (KdTreeLineQuery's ctor, then its GetNext / ProcessBranchNode inlined),
    // and for every unit of every leaf the unit's vertices decoded out of their cluster and run
    // through a single-sided line-triangle test, twice for a unit holding more than one
    // triangle. The mesh is tested where it is: the volume instance's transform is not applied.
    //
    // Every hit becomes one LineTestIntersection: the hit point, the REVERSED line direction as
    // the normal (normalize(start - end), not the triangle's), the distance in metres along the
    // line from its start as mfLineParam (dot(hit - start, normalize(end - start))), the unit's
    // surface id / group id as the two tags, and the same instance / entity indices the
    // primitive arm records.
    //
    // PC LOWERING. The console runs the triangle test four lanes wide on VMX; it is written here
    // one lane at a time. A reciprocal or reciprocal-square-root estimate is the exact quotient,
    // and the console's Newton-Raphson steps then run as written (the CgsLineTests.cpp
    // precedent); the fused multiply-adds round once on the console and twice here.
    // =========================================================================================

    // The aggregate volume's per-type descriptor typeID (`lwz 0x40 ; lwz 0 ; cmpwi 6`).
    const u32 KU_VOLUME_TYPE_AGGREGATE = 6;

    // The fatness the kd-tree line walk is built with (the image's 0.0f constant).
    const f32 KF_CLUSTERED_MESH_LINE_FATNESS = 0.0f;

    // The triangle test's two tolerance vectors. Both are .bss statics that a dynamic
    // initialiser splats at load, from the image's 1e-8 and 1e-5 float constants -- the
    // same pair ContactGeneratorJob's line-triangle kernel uses.
    const f32 KF_TRIANGLE_MIN_DETERMINANT       = 1.0e-8f;
    const f32 KF_TRIANGLE_BARYCENTRIC_TOLERANCE = 1.0e-5f;

    // A kd-tree child reference whose m_content is all ones is a BRANCH child.
    const u32 KU_KDTREE_BRANCH_CONTENT = 0xFFFFFFFFu;

    // A leaf entry packs (cluster index << bits) | unit byte offset, the offset field being 16
    // bits wide, or 20 when the mesh's cluster flags carry 0x4 (`rlwinm 0,29,29 ; addi 0x10`).
    const u32 KU_NARROW_UNIT_OFFSET_BITS        = 16;
    const u16 KU16_CLUSTER_FLAG_WIDE_UNIT_OFFSET = 0x4;

    // A cluster's vertex compression modes (ClusteredMeshCluster::muCompressionMode).
    const u8 KU8_VERTICES_16BIT_COMPRESSED = 1;
    const u8 KU8_VERTICES_32BIT_COMPRESSED = 2;
    // Where the 16-bit vertices start: past the header row and the row of four 32-bit lane
    // offsets they are biased by.
    const u32 KU_CLUSTER_16BIT_VERTEX_DATA = 0x1C;
    // Where the offset row and the 32-bit vertices start: the header row's end.
    const u32 KU_CLUSTER_VERTEX_DATA       = 0x10;

    // A unit record's flags byte: the low nibble is its type, the top three bits announce the
    // optional trailing data.
    const u8 KU8_UNIT_TYPE_MASK       = 0x0F;
    const u8 KU8_UNIT_TYPE_TRIANGLE   = 1;   // three vertex indices
    const u8 KU8_UNIT_TYPE_QUAD       = 2;   // four vertex indices, two triangles
    const u8 KU8_UNIT_TYPE_COUNTED    = 3;   // a triangle-count byte, then the indices
    const u8 KU8_UNIT_FLAG_EDGE_DATA  = 0x20;
    const u8 KU8_UNIT_FLAG_GROUP_ID   = 0x40;
    const u8 KU8_UNIT_FLAG_SURFACE_ID = 0x80;

    typedef rw::collision::Vec4 Lane4;

    inline Lane4 MakeLane4(f32 lfX, f32 lfY, f32 lfZ, f32 lfW)
    {
        Lane4 lOut;
        lOut.x = lfX; lOut.y = lfY; lOut.z = lfZ; lOut.w = lfW;
        return lOut;
    }

    inline Lane4 ToLane4(const Vector3& lrV) { return MakeLane4(lrV.x, lrV.y, lrV.z, lrV.w); }

    // vsubfp / vaddfp / vmulfp128 by a splat -- all four lanes, w riding along.
    inline Lane4 SubLane4(const Lane4& lrA, const Lane4& lrB)
    {
        return MakeLane4(lrA.x - lrB.x, lrA.y - lrB.y, lrA.z - lrB.z, lrA.w - lrB.w);
    }
    inline Lane4 AddLane4(const Lane4& lrA, const Lane4& lrB)
    {
        return MakeLane4(lrA.x + lrB.x, lrA.y + lrB.y, lrA.z + lrB.z, lrA.w + lrB.w);
    }
    inline Lane4 ScaleLane4(const Lane4& lrV, f32 lfScale)
    {
        return MakeLane4(lrV.x * lfScale, lrV.y * lfScale, lrV.z * lfScale, lrV.w * lfScale);
    }

    // vmsum3fp128: the xyz dot product.
    inline f32 Dot3(const Lane4& lrA, const Lane4& lrB)
    {
        return (lrA.x * lrB.x) + (lrA.y * lrB.y) + (lrA.z * lrB.z);
    }

    // The `vpermwi128 0x63` (yzxw) / vmulfp128 / vnmsubfp idiom: (a * b.yzx - a.yzx * b).yzx,
    // i.e. a x b. Its w lane is never read (every consumer is a vmsum3fp).
    inline Lane4 Cross(const Lane4& lrA, const Lane4& lrB)
    {
        return MakeLane4(lrA.y * lrB.z - lrA.z * lrB.y,
                         lrA.z * lrB.x - lrA.x * lrB.z,
                         lrA.x * lrB.y - lrA.y * lrB.x,
                         0.0f);
    }

    // vrefp + two Newton-Raphson steps (`vnmsubfp128 e = 1 - x*d ; vmaddfp x = x*e + x`).
    inline f32 RefinedReciprocal(f32 lfValue)
    {
        f32 lfReciprocal = 1.0f / lfValue;                                   // vrefp (the estimate)
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const f32 lfError = 1.0f - lfReciprocal * lfValue;
            lfReciprocal      = lfReciprocal * lfError + lfReciprocal;
        }
        return lfReciprocal;
    }

    // vrsqrtefp + two Newton-Raphson steps (`e = 1 - d*(x*x) ; x = (x*0.5)*e + x`).
    inline f32 RefinedReciprocalSqrt(f32 lfValue)
    {
        f32 lfInvSqrt = 1.0f / std::sqrt(lfValue);                          // vrsqrtefp (the estimate)
        for (s32 liStep = 0; liStep < 2; ++liStep)
        {
            const f32 lfError = 1.0f - lfValue * (lfInvSqrt * lfInvSqrt);
            lfInvSqrt         = (lfInvSqrt * 0.5f) * lfError + lfInvSqrt;
        }
        return lfInvSqrt;
    }

    // ---- the cluster vertex decode (the three compression modes) ---------------------------
    // The cluster is a serialised rw::collision blob: its vertex data is addressed by the
    // documented byte offsets above (the project's serialised-data exception). Each decode
    // fills all four lanes, as the console's 16-byte load does: lane 3 of a compressed vertex
    // is whatever the next bytes decode to, and it rides along unread.
    Lane4 GetClusterVertex(const rw::collision::ClusteredMeshCluster* lpCluster, u32 luIndex, f32 lfGranularity)
    {
        const u8* lpClusterBytes = reinterpret_cast<const u8*>(lpCluster);
        f32 lafLanes[4];
        if (lpCluster->muCompressionMode == KU8_VERTICES_16BIT_COMPRESSED)
        {
            // vmrghh(0, data) zero-extends four u16 words; vaddsws adds the cluster's lane
            // offsets (a u16 plus a dequantise bias cannot reach the s32 rails, so the add is
            // exact); vcfsx; times the granularity.
            const s32* lpaOffsets = reinterpret_cast<const s32*>(lpClusterBytes + KU_CLUSTER_VERTEX_DATA);
            const u8*  lpVertex   = lpClusterBytes + KU_CLUSTER_16BIT_VERTEX_DATA + 6u * luIndex;
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                u16 lu16Word;
                std::memcpy(&lu16Word, lpVertex + 2 * liLane, sizeof(lu16Word));
                lafLanes[liLane] = static_cast<f32>(lpaOffsets[liLane] + static_cast<s32>(lu16Word)) * lfGranularity;
            }
        }
        else if (lpCluster->muCompressionMode == KU8_VERTICES_32BIT_COMPRESSED)
        {
            // Three s32 words per vertex (12-byte stride); vcfsx; times the granularity.
            const u8* lpVertex = lpClusterBytes + KU_CLUSTER_VERTEX_DATA + 12u * luIndex;
            for (s32 liLane = 0; liLane < 4; ++liLane)
            {
                s32 liWord;
                std::memcpy(&liWord, lpVertex + 4 * liLane, sizeof(liWord));
                lafLanes[liLane] = static_cast<f32>(liWord) * lfGranularity;
            }
        }
        else
        {
            // Uncompressed: aligned float rows, vertex i at cluster + 16 * (i + 1).
            std::memcpy(lafLanes, lpClusterBytes + 16u * (luIndex + 1u), sizeof(lafLanes));
        }
        return MakeLane4(lafLanes[0], lafLanes[1], lafLanes[2], lafLanes[3]);
    }

    // ---- the single-sided line-triangle test -------------------------------------------------
    // The triangle is (lrP0, lrP0 + lrEdge1, lrP0 + lrEdge2); the line runs from lrStart along
    // lrDelta. All seven conditions are taken on the determinant-scaled values, with a relative
    // tolerance of 1e-5 of the determinant on every bound and a strictly positive determinant
    // (> 1e-8: back faces and edge-on triangles miss). The `vcmpgefp` lower bounds and the
    // `vnot(vcmpgtfp)` upper bounds are kept apart because they disagree on a NaN.
    // On a hit, lrHit = lrStart + lrDelta * t.
    bool IntersectLineTriangle(const Lane4& lrStart, const Lane4& lrDelta,
                               const Lane4& lrP0, const Lane4& lrEdge1, const Lane4& lrEdge2,
                               Lane4& lrHit)
    {
        const Lane4 lToStart = SubLane4(lrStart, lrP0);
        const Lane4 lP       = Cross(lrDelta, lrEdge2);
        const Lane4 lQ       = Cross(lToStart, lrEdge1);

        const f32 lfDet  = Dot3(lrEdge1, lP);
        const f32 lfUDet = Dot3(lToStart, lP);
        const f32 lfTDet = Dot3(lrEdge2, lQ);
        const f32 lfVDet = Dot3(lrDelta, lQ);

        const f32 lfLow  = (-lfDet) * KF_TRIANGLE_BARYCENTRIC_TOLERANCE;   // vxor sign ; vmulfp128
        const f32 lfHigh = lfDet - lfLow;
        const f32 lfUVDet = lfUDet + lfVDet;

        const bool lbU   = (lfUDet >= lfLow) && !(lfUDet > lfHigh);
        const bool lbT   = (lfTDet >= lfLow) && !(lfTDet > lfHigh);
        const bool lbV   = (lfVDet >= lfLow) && !(lfUVDet > lfHigh);
        const bool lbDet = (lfDet > KF_TRIANGLE_MIN_DETERMINANT);
        if (!(lbU && lbV && lbDet && lbT))
        {
            return false;
        }

        const f32 lfT = lfTDet * RefinedReciprocal(lfDet);
        lrHit = AddLane4(lrStart, ScaleLane4(lrDelta, lfT));               // vmaddcfp128
        return true;
    }

    // Append one hit to the per-pass intersection array, remembering where this query's run
    // starts (the same pair of calls in both arms).
    void AppendIntersection(FineIntersectionTestIO::OutputBuffer::LineTestIntersectionArray* lpIntersections,
                            const LineTestIntersection& lrHit, const LineTestIntersection*& lrpFirst)
    {
        lpIntersections->Append(lrHit);
        if (!lrpFirst)
        {
            lrpFirst = &(*lpIntersections)[lpIntersections->GetLength() - 1];
        }
    }

    // ---- the kd-tree line walk (KdTreeLineQuery's GetNext, inlined by the console) ----------
    // ProcessBranchNode (the declaration's KdTreeLineQueryBase::ProcessBranchNode): split the popped branch
    // entry's [key, far] line interval at the node's two extents (each widened by the clipper's
    // pad on that axis) and push the children the interval reaches -- the one on the far side
    // of the line's direction first, so the near one is popped next.
    void ProcessBranchNode(rw::collision::KdTreeLineQuery& lrQuery,
                           const rw::collision::KdTreeLineQuery::NodeStackEntry& lrEntry)
    {
        const rw::collision::KdTree::BranchNode* lpaBranchNodes =
            reinterpret_cast<const rw::collision::KdTree::BranchNode*>(
                static_cast<uintptr_t>(lrQuery.mpKdTree->muBranchNodes));
        const rw::collision::KdTree::BranchNode& lrNode = lpaBranchNodes[lrEntry.muNode1];

        const u32 luAxis    = lrNode.muAxis;
        const f32 lfPad     = (&lrQuery.mClipper.mPadExtent.x)[luAxis];
        const f32 lfClipMin = (&lrQuery.mClipper.mClipMin.x)[luAxis];
        const f32 lfRecip   = (&lrQuery.mClipper.mRecipSpan.x)[luAxis];

        f32 lafSplit[2];
        lafSplit[0] = ((lrNode.mafExtents[0] + lfPad) - lfClipMin) * lfRecip;
        lafSplit[1] = ((lrNode.mafExtents[1] - lfPad) - lfClipMin) * lfRecip;

        const u32 luFarChild  = (lfRecip > 0.0f) ? 1u : 0u;   // `fcmpu ; bgt`
        const u32 luNearChild = 1u - luFarChild;

        if (lrEntry.mfFar > lafSplit[luFarChild])
        {
            rw::collision::KdTreeLineQuery::NodeStackEntry& lrPush = lrQuery.maNodeStack[lrQuery.muStackCount];
            lrPush.muNode0 = lrNode.maChildRefs[luFarChild].muContent;
            lrPush.muNode1 = lrNode.maChildRefs[luFarChild].muIndex;
            const f32 lfSplit = lafSplit[luFarChild];
            lrPush.mfKey = (lrEntry.mfKey - lfSplit >= 0.0f) ? lrEntry.mfKey : lfSplit;   // fsel: max(key, split)
            lrPush.mfFar = lrEntry.mfFar;
            ++lrQuery.muStackCount;
        }

        if (lrEntry.mfKey < lafSplit[luNearChild])
        {
            rw::collision::KdTreeLineQuery::NodeStackEntry& lrPush = lrQuery.maNodeStack[lrQuery.muStackCount];
            lrPush.muNode0 = lrNode.maChildRefs[luNearChild].muContent;
            lrPush.muNode1 = lrNode.maChildRefs[luNearChild].muIndex;
            const f32 lfSplit = lafSplit[luNearChild];
            lrPush.mfKey = lrEntry.mfKey;
            lrPush.mfFar = (lrEntry.mfFar - lfSplit >= 0.0f) ? lfSplit : lrEntry.mfFar;   // fsel: min(far, split)
            ++lrQuery.muStackCount;
        }
    }

    // Pop stack entries until a non-empty leaf is latched in muTopNode (its entry count) and
    // muTopNodeHi (its first entry). False once the stack runs dry.
    bool LatchNextLeaf(rw::collision::KdTreeLineQuery& lrQuery)
    {
        while (lrQuery.muTopNode == 0)
        {
            if (lrQuery.muStackCount == 0)
            {
                return false;
            }
            --lrQuery.muStackCount;
            const rw::collision::KdTreeLineQuery::NodeStackEntry lEntry = lrQuery.maNodeStack[lrQuery.muStackCount];
            if (lEntry.muNode0 == KU_KDTREE_BRANCH_CONTENT)
            {
                ProcessBranchNode(lrQuery, lEntry);
            }
            else
            {
                lrQuery.muTopNode   = lEntry.muNode0;
                lrQuery.muTopNodeHi = lEntry.muNode1;
            }
        }
        return true;
    }

    // One clustered-mesh hit, recorded.
    void RecordClusteredMeshHit(const rw::collision::ClusteredMesh*        lpMesh,
                                const rw::collision::ClusteredMeshCluster* lpCluster,
                                u32 luUnitOffset, const Lane4& lrHit,
                                const Vector3& lrLineStart, const Vector3& lrLineEnd,
                                s32 liVolumeInstance, u16 lu16EntityIndex,
                                FineIntersectionTestIO::OutputBuffer::LineTestIntersectionArray* lpIntersections,
                                const LineTestIntersection*& lrpFirst, s32& lriCount)
    {
        // GetGroupAndSurfaceId reads the id widths out of an 8-byte copy of the mesh's cluster
        // parameters (`ld 0x38(mesh) ; std`).
        rw::collision::ClusteredMeshCluster::CompressionInfo lInfo;
        static_assert(sizeof(lInfo) == 8, "the cluster parameters are eight bytes");
        std::memcpy(&lInfo, &lpMesh->mfVertexCompressionGranularity, sizeof(lInfo));
        u32 luGroupId   = 0;
        u32 luSurfaceId = 0;
        lpCluster->GetGroupAndSurfaceId(static_cast<int>(luUnitOffset), &lInfo, &luGroupId, &luSurfaceId);

        const Lane4 lStart = ToLane4(lrLineStart);
        const Lane4 lEnd   = ToLane4(lrLineEnd);

        const Lane4 lForward = SubLane4(lEnd, lStart);
        const Lane4 lUnitForward = ScaleLane4(lForward, RefinedReciprocalSqrt(Dot3(lForward, lForward)));
        const Lane4 lBackward = SubLane4(lStart, lEnd);
        const Lane4 lUnitBackward = ScaleLane4(lBackward, RefinedReciprocalSqrt(Dot3(lBackward, lBackward)));

        LineTestIntersection lIntersection;
        lIntersection.mPosition.x = lrHit.x;
        lIntersection.mPosition.y = lrHit.y;
        lIntersection.mPosition.z = lrHit.z;
        lIntersection.mPosition.w = lrHit.w;
        lIntersection.mNormal.x = lUnitBackward.x;
        lIntersection.mNormal.y = lUnitBackward.y;
        lIntersection.mNormal.z = lUnitBackward.z;
        lIntersection.mNormal.w = lUnitBackward.w;
        lIntersection.mVolumeInstanceId.muId = static_cast<u64>(static_cast<s64>(liVolumeInstance));
        lIntersection.mEntityId = EntityId(lu16EntityIndex);
        lIntersection.mfLineParam = Dot3(SubLane4(lrHit, lStart), lUnitForward);
        lIntersection.mu16MaterialTag = static_cast<u16>(luSurfaceId);
        lIntersection.mu16GroupTag    = static_cast<u16>(luGroupId);

        AppendIntersection(lpIntersections, lIntersection, lrpFirst);
        ++lriCount;
    }

    // The fast path itself: every unit of every leaf the line reaches.
    void ComputeLineTestClusteredMesh(const rw::collision::ClusteredMesh* lpMesh,
                                      const Vector3& lrLineStart, const Vector3& lrLineEnd,
                                      s32 liVolumeInstance, u16 lu16EntityIndex,
                                      FineIntersectionTestIO::OutputBuffer::LineTestIntersectionArray* lpIntersections,
                                      const LineTestIntersection*& lrpFirst, s32& lriCount)
    {
        const Lane4 lStart = ToLane4(lrLineStart);
        const Lane4 lDelta = SubLane4(ToLane4(lrLineEnd), lStart);
        const Lane4 lEnd   = AddLane4(lStart, lDelta);   // the walk is handed start + (end - start)

        const rw::collision::AALineClipper::Vec4 lWalkStart = { lStart.x, lStart.y, lStart.z, lStart.w };
        const rw::collision::AALineClipper::Vec4 lWalkEnd   = { lEnd.x, lEnd.y, lEnd.z, lEnd.w };
        rw::collision::KdTreeLineQuery lLineQuery(lpMesh->GetKDTree(), lWalkStart, lWalkEnd,
                                                  KF_CLUSTERED_MESH_LINE_FATNESS);

        const u32 luOffsetBits = KU_NARROW_UNIT_OFFSET_BITS
                               + (lpMesh->muClusterFlags & KU16_CLUSTER_FLAG_WIDE_UNIT_OFFSET);
        const u32 luOffsetMask = (1u << luOffsetBits) - 1u;
        const f32 lfGranularity = lpMesh->mfVertexCompressionGranularity;

        while (LatchNextLeaf(lLineQuery))
        {
            const u32 luNumUnits = lLineQuery.muTopNode;
            const u32 luEntry    = lLineQuery.muTopNodeHi;
            lLineQuery.muTopNodeHi = luEntry + 1u;
            lLineQuery.muTopNode   = 0;

            u32 luClusterIndex = luEntry >> luOffsetBits;
            u32 luUnitOffset   = luEntry & luOffsetMask;
            for (u32 luUnit = 0; luUnit < luNumUnits; ++luUnit)
            {
                const rw::collision::ClusteredMeshCluster* lpCluster =
                    reinterpret_cast<const rw::collision::ClusteredMeshCluster*>(
                        reinterpret_cast<const u8*>(lpMesh) + lpMesh->GetClusterOffsets()[luClusterIndex]);
                const u8* lpUnit = reinterpret_cast<const u8*>(lpCluster)
                                 + 16u * (static_cast<u32>(lpCluster->muUnitDataStart) + 1u) + luUnitOffset;

                // The unit's triangle count, and its four vertex-index bytes (a counted unit
                // has its count byte in front of them). The fourth index is read whatever the
                // type; only a unit of two or more triangles uses it.
                const u8  lu8Flags = lpUnit[0];
                const u32 luType   = lu8Flags & KU8_UNIT_TYPE_MASK;
                const u32 luNumTriangles = ((luType == KU8_UNIT_TYPE_QUAD) ? 2u : 0u)
                                         + ((luType == KU8_UNIT_TYPE_TRIANGLE) ? 1u : 0u)
                                         + ((luType == KU8_UNIT_TYPE_COUNTED) ? static_cast<u32>(lpUnit[1]) : 0u);
                const u8* lpIndices = lpUnit + ((luType == KU8_UNIT_TYPE_COUNTED) ? 1 : 0);

                const Lane4 lV0 = GetClusterVertex(lpCluster, lpIndices[1], lfGranularity);
                const Lane4 lV1 = GetClusterVertex(lpCluster, lpIndices[2], lfGranularity);
                const Lane4 lV2 = GetClusterVertex(lpCluster, lpIndices[3], lfGranularity);
                const Lane4 lV3 = GetClusterVertex(lpCluster, lpIndices[4], lfGranularity);

                Lane4 lHit;
                if (IntersectLineTriangle(lStart, lDelta, lV0, SubLane4(lV1, lV0), SubLane4(lV2, lV0), lHit))
                {
                    RecordClusteredMeshHit(lpMesh, lpCluster, luUnitOffset, lHit, lrLineStart, lrLineEnd,
                                           liVolumeInstance, lu16EntityIndex, lpIntersections, lrpFirst, lriCount);
                }
                if (luNumTriangles > 1u &&
                    IntersectLineTriangle(lStart, lDelta, lV3, SubLane4(lV2, lV3), SubLane4(lV1, lV3), lHit))
                {
                    RecordClusteredMeshHit(lpMesh, lpCluster, luUnitOffset, lHit, lrLineStart, lrLineEnd,
                                           liVolumeInstance, lu16EntityIndex, lpIntersections, lrpFirst, lriCount);
                }

                // Step past the unit (ClusteredMeshCluster::GetUnitSize, inlined): flags byte,
                // the counted type's count byte, triangles + 2 vertex indices, then the optional
                // per-vertex edge bytes, group id and surface id. A cluster's unit data ends at
                // muUnitDataSize; the next unit is then the first of the next cluster.
                u32 luTriangles   = 1;
                u32 luHeaderBytes = 1;
                if (luType == KU8_UNIT_TYPE_QUAD)
                {
                    luTriangles = 2;
                }
                else if (luType == KU8_UNIT_TYPE_COUNTED)
                {
                    luTriangles   = lpUnit[1];
                    luHeaderBytes = 2;
                }
                u32 luUnitSize = luTriangles + luHeaderBytes + 2u;
                if ((lu8Flags & KU8_UNIT_FLAG_EDGE_DATA) != 0)
                {
                    luUnitSize += luTriangles + 2u;
                }
                if ((lu8Flags & KU8_UNIT_FLAG_GROUP_ID) != 0)
                {
                    luUnitSize += lpMesh->muGroupIdSize;
                }
                if ((lu8Flags & KU8_UNIT_FLAG_SURFACE_ID) != 0)
                {
                    luUnitSize += lpMesh->muSurfaceIdSize;
                }
                luUnitOffset += luUnitSize;
                if (luUnitOffset >= lpCluster->muUnitDataSize)
                {
                    ++luClusterIndex;
                    luUnitOffset = 0;
                }
            }
        }
    }
}

    // ARTIST828C7D70: primitive-volume arm828C8AA0..828C8C3C. Results retain
    // internal entity/instance indices; ProcessLineTestFine resolves their IDs.
    void FineIntersectionTestModule::ComputeLineTestFine(const InEventLineTestFine* lpQuery,
                                                         OutEventLineTestFineResult* lpOutResult,
                                                         void* lpResultsOut)
    {
        auto* lpIntersections = static_cast<FineIntersectionTestIO::OutputBuffer::LineTestIntersectionArray*>(lpResultsOut);
        u32 luMask = ~0u;
        u32 luExclude = static_cast<u32>(K_INVALID_ENTITY_ID);
        if (lpQuery->mu16ExcludeEntityIndex != 0xffff)
        {
            const EntityId lExcludeId = mpEntityManager->GetEntityIdByIndex(lpQuery->mu16ExcludeEntityIndex);
            luMask = lpQuery->mbExcludeParts ? lExcludeId.GetPartComparisonMask() : ~0u;
            luExclude = luMask & static_cast<u32>(lExcludeId);
        }
        CGS_ASSERT(lpOutResult != 0, "lpOutResult != NULL");
        lpOutResult->mQueryId = lpQuery->mQueryId;
        s32 liCount = 0;
        const LineTestIntersection* lpFirst = 0;
        for (u16 luCandidate = 0; luCandidate < lpQuery->mu16NumEntities; ++luCandidate)
        {
            const u16 luEntityIndex = lpQuery->mpau16EntityIndices[luCandidate];
            if ((static_cast<u32>(mpEntityManager->GetEntityIdByIndex(luEntityIndex)) & luMask) == luExclude)
                continue;
            s32 liInstance = 0;
            const VolumeInstance* lpInstance = mpEntityManager->GetFirstEntityVolumeInstance(luEntityIndex, &liInstance);
            while (lpInstance)
            {
                const s32 liVolume = lpInstance->miVolumeIndex;
                if ((mpVolumeManager->GetVolumeTypeFlags(liVolume) & lpQuery->mxVolumeTypeFlags) != 0)
                {
                    const rw::collision::Volume* lapVolumes[1] = {
                        reinterpret_cast<const rw::collision::Volume*>(mpVolumeManager->GetRwVolume(liVolume)) };
                    const Matrix44Affine* lapTransforms[1] = { &lpInstance->mWorldSpaceTransform };
                    if (rw::collision::gVolumeVTable[lapVolumes[0]->muVTableSlot]->muTypeID == KU_VOLUME_TYPE_AGGREGATE)
                    {
                        // The aggregate is a clustered mesh, walked directly (see the fast path's
                        // banner above). Trigger boxes take the other arm.
                        const rw::collision::ClusteredMesh* lpMesh =
                            reinterpret_cast<const rw::collision::ClusteredMesh*>(
                                static_cast<uintptr_t>(lapVolumes[0]->mAggregateData.muAggregate));
                        ComputeLineTestClusteredMesh(lpMesh, lpQuery->mLineStart, lpQuery->mLineEnd,
                                                     liInstance, luEntityIndex, lpIntersections, lpFirst, liCount);
                    }
                    else
                    {
                        const rw::collision::VolRef::Vec4 lStart = {
                            lpQuery->mLineStart.x, lpQuery->mLineStart.y, lpQuery->mLineStart.z, lpQuery->mLineStart.w };
                        const rw::collision::VolRef::Vec4 lEnd = {
                            lpQuery->mLineEnd.x, lpQuery->mLineEnd.y, lpQuery->mLineEnd.z, lpQuery->mLineEnd.w };
                        mpVolumeLineQuery->InitQuery(lapVolumes, lapTransforms, 1, lStart, lEnd, 0.0f);
                        while (!mpVolumeLineQuery->Finished())
                        {
                            const u32 luCount = mpVolumeLineQuery->GetAllIntersections();
                            const rw::collision::VolumeLineSegIntersectResult* lpHits = mpVolumeLineQuery->m_resBuffer;
                            liCount += luCount;
                            for (u32 luHit = 0; luHit < luCount; ++luHit)
                            {
                                const auto& lrHit = lpHits[luHit];
                                LineTestIntersection lHit;
                                lHit.mPosition.x = lrHit.position.x;
                                lHit.mPosition.y = lrHit.position.y;
                                lHit.mPosition.z = lrHit.position.z;
                                lHit.mPosition.w = lrHit.position.w;
                                lHit.mNormal.x = lrHit.normal.x;
                                lHit.mNormal.y = lrHit.normal.y;
                                lHit.mNormal.z = lrHit.normal.z;
                                lHit.mNormal.w = lrHit.normal.w;
                                lHit.mVolumeInstanceId.muId = static_cast<u64>(static_cast<s64>(liInstance));
                                lHit.mEntityId = EntityId(luEntityIndex);
                                lHit.mfLineParam = lrHit.lineParam;
                                const auto* lpHitVolume = reinterpret_cast<const rw::collision::Volume*>(lrHit.vRef.muVolumePtr);
                                lHit.mu16MaterialTag = lpHitVolume ? static_cast<u16>(lpHitVolume->muSurfaceID) : 0;
                                lHit.mu16GroupTag = lpHitVolume ? static_cast<u16>(lpHitVolume->muGroupID) : 0;
                                lpIntersections->Append(lHit);
                                if (!lpFirst)
                                    lpFirst = &(*lpIntersections)[lpIntersections->GetLength() - 1];
                            }
                        }
                    }
                }
                liInstance = lpInstance->miNextEntityVolumeInstance;
                lpInstance = static_cast<const EntityManager*>(mpEntityManager)->GetVolumeInstance(liInstance);
            }
        }
        lpOutResult->miNumResults = liCount;
        lpOutResult->mpaResults = lpFirst;
    }


    // =========================================================================================
    // ComputeLineTestNearest @ 0x828C8CC8 -- RECONSTRUCTED 2026-09-25 (crash parity FX-FOLLOWUPS stage a); it was a
    // LOUD trap here. Where does the segment first meet the candidate entities' collision volumes? One
    // VolumeLineQuery walk per volume instance whose volume-type flags meet the query's; the smallest line
    // parameter over every result of every batch, instance and entity wins.
    //   r3 = this (r17), r4 = lpQuery (r21), r5 = lpOutResult (r30).
    //   0x828C8CF4  f31 = flt_820F259C (0x7F7FFFFF, FLT_MAX) -- the nearest so far; it is NEVER reset.
    //   0x828C8D10  query+0x2A (exclude index) != 0xFFFF: mask = query+0x2D (mbExcludeParts) ? 0xFFFFFC00 :
    //               0xFFFFFFFF (`subfic 0 ; subfe ; rlwinm 0,31,21 ; addi -1`), exclude = mask & the excluded
    //               entity's id (the inlined GetEntityIdByIndex, the :301 assert, `lwz 0(r3)`);
    //               == 0xFFFF: exclude = dword_82F33F64 (0xFFFFFFFF, K_INVALID_ENTITY_ID), mask = -1.
    //   0x828C8D9C  out+0x3A (mbIntersection) = 0, out+0x36 / +0x38 (the two tags) = 0, out+0x00 = query+0x20
    //               (mQueryId). Nothing else is written unless a result wins.
    //   0x828C8E10  per candidate i < query+0x28 (u16), entity index query+0x24[i]: (mask & id) == exclude -> next
    //               candidate (0x828C8E60 `cmplw ; beq` -- an EQUALITY, unlike ComputeVolumeTestDeepest's superset
    //               test at 0x828C9220).
    //   0x828C8E74    GetFirstEntityVolumeInstance(index, &instance) (0x828C5DC0; the instance index is var_5C,
    //                 r22), then per instance GetVolumeInstance(+0x60) (0x828B9F28, the CONST overload; r22 = that
    //                 index) until null:
    //   0x828C8E90      GetVolumeTypeFlags(+0x5C volume index) & query+0x2C == 0 -> next instance (the inlined
    //                   h:203 / h:204 body)
    //   0x828C8F14      the VolumeLineQuery (this+0x5981C) primed as InitQuery does -- the one input volume
    //                   {GetRwVolume} (0x828C5E68, var_6C), the one input matrix {the instance's transform, +0x00}
    //                   (var_70), numInputs 1, the segment query+0x00 / query+0x10 (four lanes each), fatness
    //                   f29 = flt_82001CC0 (0.0f); m_endClipVal = f30 = flt_82001C98 (1.0f)
    //   0x828C8F90      until Finished(): r29 = -1; n = GetAllIntersections (0x82BB3820); over its n results
    //                   (m_resBuffer re-read, stride 0xD0, compared unsigned) lineParam (+0x40) < nearest (`fcmpu ;
    //                   bge` -- a NaN never wins) -> nearest = lineParam, best = k; best >= 0 ->
    //   0x828C9014        out+0x3A = 1, +0x34 = the entity index, +0x30 = lineParam, +0x10 = position (+0x10),
    //                     +0x04 = the instance index, +0x20 = normal (+0x20); the hit volume (vRef +0x50) non-null:
    //                     +0x36 = its surfaceID (+0x58), +0x38 = its groupID (+0x54) (sth, the low halves); else
    //                     both 0.
    // =========================================================================================
    void FineIntersectionTestModule::ComputeLineTestNearest(const InEventLineTestNearest* lpQuery,
                                                            OutEventLineTestNearestResult* lpOutResult)
    {
        // flt_82001CC0 (0.0f): the fatness InitQuery is handed (f29).
        static const f32 KF_LINE_TEST_NEAREST_FATNESS = 0.0f;
        // The exclude-index sentinel query+0x2A is compared with (`cmplwi cr6, r11, 0xFFFF` @0x828C8D10).
        static const u16 KU16_NO_EXCLUDE_ENTITY_INDEX = 0xFFFF;
        // flt_820F259C: 0x7F7FFFFF, the largest finite f32 -- the starting "nearest" (f31).
        static const f32 KF_LINE_TEST_NEAREST_START = 3.40282346638528859812e+38f;

        f32 lfNearest = KF_LINE_TEST_NEAREST_START;   // f31

        u32 lx32Mask;      // var_54
        u32 lx32Exclude;   // var_58
        if (lpQuery->mu16ExcludeEntityIndex != KU16_NO_EXCLUDE_ENTITY_INDEX)
        {
            const EntityId lExcludeEntityId = mpEntityManager->GetEntityIdByIndex(lpQuery->mu16ExcludeEntityIndex);
            lx32Mask    = lpQuery->mbExcludeParts ? lExcludeEntityId.GetPartComparisonMask() : ~0u;
            lx32Exclude = lx32Mask & static_cast<u32>(lExcludeEntityId);
        }
        else
        {
            lx32Exclude = static_cast<u32>(K_INVALID_ENTITY_ID);   // dword_82F33F64
            lx32Mask    = ~0u;                                     // li r11, -1
        }

        lpOutResult->mbIntersection  = false;              // stb 0, 0x3A(r30)
        lpOutResult->mu16MaterialTag = 0;                  // sth 0, 0x36(r30)
        lpOutResult->mu16GroupTag    = 0;                  // sth 0, 0x38(r30)
        lpOutResult->mQueryId        = lpQuery->mQueryId;  // lwz 0x20(r21) ; stw 0(r30)

        for (u16 lu16Candidate = 0; lu16Candidate < lpQuery->mu16NumEntities; ++lu16Candidate)
        {
            const u16 lu16EntityIndex = lpQuery->mpau16EntityIndices[lu16Candidate];
            const u32 lx32EntityId    = static_cast<u32>(mpEntityManager->GetEntityIdByIndex(lu16EntityIndex));
            if ((lx32EntityId & lx32Mask) == lx32Exclude)
            {
                continue;
            }

            s32 liVolumeInstance = 0;   // var_5C, then r22
            const VolumeInstance* lpVolumeInstance =
                mpEntityManager->GetFirstEntityVolumeInstance(lu16EntityIndex, &liVolumeInstance);
            while (lpVolumeInstance != 0)
            {
                const s32 liVolumeIndex = lpVolumeInstance->miVolumeIndex;
                if ((mpVolumeManager->GetVolumeTypeFlags(liVolumeIndex) & lpQuery->mxVolumeTypeFlags) != 0)
                {
                    const rw::collision::Volume* lapInputVolumes[1] =                          // var_6C
                        { reinterpret_cast<const rw::collision::Volume*>(mpVolumeManager->GetRwVolume(liVolumeIndex)) };
                    const Matrix44Affine* lapInputMatrices[1] =                                // var_70
                        { &lpVolumeInstance->mWorldSpaceTransform };
                    const rw::collision::VolRef::Vec4 lLineStart =
                        { lpQuery->mLineStart.x, lpQuery->mLineStart.y, lpQuery->mLineStart.z, lpQuery->mLineStart.w };
                    const rw::collision::VolRef::Vec4 lLineEnd =
                        { lpQuery->mLineEnd.x, lpQuery->mLineEnd.y, lpQuery->mLineEnd.z, lpQuery->mLineEnd.w };

                    mpVolumeLineQuery->InitQuery(lapInputVolumes, lapInputMatrices, 1, lLineStart, lLineEnd,
                                                 KF_LINE_TEST_NEAREST_FATNESS);
                    while (!mpVolumeLineQuery->Finished())
                    {
                        s32 liBest = -1;                                                         // r29
                        const u32 luNumResults = mpVolumeLineQuery->GetAllIntersections();
                        const rw::collision::VolumeLineSegIntersectResult* lpaResults =
                            mpVolumeLineQuery->m_resBuffer;                                      // lwz 0x10 once
                        for (u32 luResult = 0; luResult < luNumResults; ++luResult)
                        {
                            if (lpaResults[luResult].lineParam < lfNearest)
                            {
                                liBest    = static_cast<s32>(luResult);
                                lfNearest = lpaResults[luResult].lineParam;
                            }
                        }
                        if (liBest < 0)
                        {
                            continue;
                        }

                        const rw::collision::VolumeLineSegIntersectResult& lrBest = lpaResults[liBest];
                        lpOutResult->mbIntersection        = true;               // stb 1, 0x3A
                        lpOutResult->mu16EntityIndex       = lu16EntityIndex;    // sth r20, 0x34
                        lpOutResult->mfLineParam           = lrBest.lineParam;   // lfs 0x40 ; stfs 0x30
                        lpOutResult->mPosition.x           = lrBest.position.x;  // lvx128 +0x10 ; stvx128 +0x10
                        lpOutResult->mPosition.y           = lrBest.position.y;
                        lpOutResult->mPosition.z           = lrBest.position.z;
                        lpOutResult->mPosition.w           = lrBest.position.w;
                        lpOutResult->muVolumeInstanceIndex = static_cast<u32>(liVolumeInstance);   // stw r22, 4
                        lpOutResult->mNormal.x             = lrBest.normal.x;    // lvx128 +0x20 ; stvx128 +0x20
                        lpOutResult->mNormal.y             = lrBest.normal.y;
                        lpOutResult->mNormal.z             = lrBest.normal.z;
                        lpOutResult->mNormal.w             = lrBest.normal.w;
                        const rw::collision::Volume* lpHitVolume =
                            reinterpret_cast<const rw::collision::Volume*>(lrBest.vRef.muVolumePtr);
                        if (lpHitVolume != 0)
                        {
                            lpOutResult->mu16MaterialTag = static_cast<u16>(lpHitVolume->muSurfaceID);   // +0x58
                            lpOutResult->mu16GroupTag    = static_cast<u16>(lpHitVolume->muGroupID);     // +0x54
                        }
                        else
                        {
                            lpOutResult->mu16MaterialTag = 0;
                            lpOutResult->mu16GroupTag    = 0;
                        }
                    }
                }
                // 0x828C9074: the next instance through the CONST overload (0x828B9F28), its index kept (r22).
                liVolumeInstance = lpVolumeInstance->miNextEntityVolumeInstance;
                lpVolumeInstance = static_cast<const EntityManager*>(mpEntityManager)->GetVolumeInstance(liVolumeInstance);
            }
        }
    }

    // =========================================================================================
    // ComputeVolumeTestDeepest @ 0x828C90D0 -- RECONSTRUCTED 2026-09-25 (crash parity FX-FOLLOWUPS, item 2); it was a
    // LOUD trap here. How deep does the query volume sink into the candidate entities' collision volumes? One
    // VolumeVolumeQuery per volume instance whose volume-type flags meet the query's; the deepest penetration over
    // every contact result of every instance wins.
    //   r3 = this, r4 = lpQuery (r27), r5 = lpOutResult (r24, spilled to sp+0x134).
    //   0x828C90F8  f30 = f31 = flt_82001CC0 (0.0f) -- the deepest so far; it is NOT reset per instance or entity.
    //   0x828C9100  query+0xCA (exclude index) != 0xFFFF:
    //                 mask = query+0xCD (mbExcludeParts) ? 0xFFFFFC00 : 0xFFFFFFFF (`subfic 0 ; subfe ;
    //                 rlwinm 0,31,21 ; addi -1` -- the part-comparison mask or all ones), and
    //                 r14 = mask & the excluded entity's id (the inlined GetEntityIdByIndex, :301 assert)
    //               == 0xFFFF: r14 = dword_82F33F64 (0xFFFFFFFF, K_INVALID_ENTITY_ID)
    //   0x828C9194  out+8 (mbIntersection) = 0, out+0 (mQueryId) = query+0xC0. out+4 (mfDepth) is NOT written here.
    //   0x828C91E0  per candidate i < query+0xC8, entity index query+0xC4[i]:
    //                 id = GetEntityIdByIndex; (r14 & id) == r14 -> next candidate (0x828C9220: a bit-SUPERSET test
    //                 against the masked exclude id, as compiled -- not an equality)
    //   0x828C9238    GetFirstEntityVolumeInstance(index, &first) (0x828C5DC0), then GetVolumeInstance(+0x60)
    //                 (0x828B9F28) until null; per instance:
    //   0x828C924C      GetVolumeTypeFlags(+0x5C volume index) & query+0xCC == 0 -> next instance (the inlined
    //                   h:203 / h:204 body)
    //   0x828C92D8      prime the VolumeVolumeQuery (this+0x59818): +0x14 m_padding = f30 (0.0f), +0x00 m_inputVols
    //                   = {GetRwVolume}, +0x04 m_inputMats = {the instance's transform (+0x00)}, +0x08 m_numInputs =
    //                   1, +0x0C m_currInput = 0, +0x1C m_volRefPairCount = 0, +0x38 m_queryVol = query+0x40,
    //                   +0x3C m_queryMtx = query+0x00, +0x10 m_cullTable = 0; GetPrimitiveIntersections (0x82BB3FF0)
    //   0x828C9340      over the results (m_intersectionBuffer, stride 0x750, count compared unsigned):
    //                   -distance (+0x4F0) > deepest (`fneg ; fcmpu ; ble` -- a NaN never wins) and numPoints
    //                   (+0x740) != 0 (`cmplwi ; ble`) -> deepest = -distance, best = i
    //   0x828C9374      best >= 0 -> out+4 = deepest, out+8 = 1
    // =========================================================================================
    void FineIntersectionTestModule::ComputeVolumeTestDeepest(const InEventVolumeTestDeepest* lpQuery,
                                                              OutEventVolumeTestDeepestResult* lpOutResult)
    {
        // flt_82001CC0 (0.0f): the starting "deepest" (only a penetration, -distance > 0, can beat it) and the
        // query padding -- one register, f30, feeds both.
        static const f32 KF_VOLUME_TEST_DEEPEST_ZERO = 0.0f;
        // The exclude-index sentinel query+0xCA is compared with (`cmplwi r11, 0xFFFF` @0x828C9120).
        static const u16 KU16_NO_EXCLUDE_ENTITY_INDEX = 0xFFFF;

        f32 lfDeepest = KF_VOLUME_TEST_DEEPEST_ZERO;   // f31

        u32 lx32Exclude;                               // r14
        if (lpQuery->mu16ExcludeEntityIndex != KU16_NO_EXCLUDE_ENTITY_INDEX)
        {
            const EntityId lExcludeEntityId = mpEntityManager->GetEntityIdByIndex(lpQuery->mu16ExcludeEntityIndex);
            const u32      lx32Mask         = lpQuery->mbExcludeParts ? lExcludeEntityId.GetPartComparisonMask()
                                                                      : ~0u;
            lx32Exclude = lx32Mask & static_cast<u32>(lExcludeEntityId);
        }
        else
        {
            lx32Exclude = static_cast<u32>(K_INVALID_ENTITY_ID);   // dword_82F33F64
        }

        lpOutResult->mbIntersection = false;              // stb 0, 8(r24)
        lpOutResult->mQueryId       = lpQuery->mQueryId;  // lwz 0xC0(r27) ; stw 0(r24)

        for (u16 lu16Candidate = 0; lu16Candidate < lpQuery->mu16NumEntities; ++lu16Candidate)
        {
            const u16 lu16EntityIndex = lpQuery->mpau16EntityIndices[lu16Candidate];
            const u32 lx32EntityId    = static_cast<u32>(mpEntityManager->GetEntityIdByIndex(lu16EntityIndex));
            if ((lx32Exclude & lx32EntityId) == lx32Exclude)
            {
                continue;
            }

            s32 liFirstVolumeInstance = 0;   // var_B0: written by the call, never read
            const VolumeInstance* lpVolumeInstance =
                mpEntityManager->GetFirstEntityVolumeInstance(lu16EntityIndex, &liFirstVolumeInstance);
            while (lpVolumeInstance != 0)
            {
                const s32 liVolumeIndex = lpVolumeInstance->miVolumeIndex;
                if ((mpVolumeManager->GetVolumeTypeFlags(liVolumeIndex) & lpQuery->mxVolumeTypeFlags) != 0)
                {
                    const rw::collision::Volume* lapInputVolumes[1] =                          // var_BC
                        { reinterpret_cast<const rw::collision::Volume*>(mpVolumeManager->GetRwVolume(liVolumeIndex)) };
                    const Matrix44Affine* lapInputMatrices[1] =                                // var_B8
                        { &lpVolumeInstance->mWorldSpaceTransform };

                    rw::collision::VolumeVolumeQuery* lpQueryObject = mpVolumeVolumeQuery;
                    lpQueryObject->m_padding         = KF_VOLUME_TEST_DEEPEST_ZERO;          // +0x14
                    lpQueryObject->m_inputVols       = lapInputVolumes;                      // +0x00
                    lpQueryObject->m_inputMats       = lapInputMatrices;                     // +0x04
                    lpQueryObject->m_numInputs       = 1;                                    // +0x08
                    lpQueryObject->m_currInput       = 0;                                    // +0x0C
                    lpQueryObject->m_volRefPairCount = 0;                                    // +0x1C
                    lpQueryObject->m_queryVol        =                                       // +0x38
                        reinterpret_cast<const rw::collision::Volume*>(&lpQuery->mVolumeBuffer);
                    lpQueryObject->m_queryMtx        = &lpQuery->mTransform;                 // +0x3C
                    lpQueryObject->m_cullTable       = 0;                                    // +0x10

                    const u32 luNumResults = static_cast<u32>(mpVolumeVolumeQuery->GetPrimitiveIntersections());
                    const rw::collision::PrimitivePairIntersectResult* lpaResults =
                        mpVolumeVolumeQuery->m_intersectionBuffer;                           // lwz 0x30 once
                    s32 liBest = -1;                                                         // r30
                    for (u32 luResult = 0; luResult < luNumResults; ++luResult)
                    {
                        const f32 lfDepth = -lpaResults[luResult].distance;                  // fneg
                        if (lfDepth > lfDeepest && lpaResults[luResult].numPoints != 0)
                        {
                            liBest    = static_cast<s32>(luResult);
                            lfDeepest = lfDepth;
                        }
                    }
                    if (liBest >= 0)
                    {
                        lpOutResult->mfDepth        = lfDeepest;   // stfs f31, 4
                        lpOutResult->mbIntersection = true;        // stb 1, 8
                    }
                }
                // 0x828C9390: the CONST overload (0x828B9F28), as GetFirstEntityVolumeInstance itself uses.
                lpVolumeInstance = static_cast<const EntityManager*>(mpEntityManager)
                                       ->GetVolumeInstance(lpVolumeInstance->miNextEntityVolumeInstance);
            }
        }
    }

    // =========================================================================================
    // ComputeVolumeTestFine
    //
    // Which candidate entities does the query volume touch? One VolumeVolumeQuery per volume
    // instance that passes the filters below, primed exactly as ComputeVolumeTestDeepest primes
    // it (zero padding, the instance's volume and transform as the single input, the query's
    // volume image and transform, no cull table); every instance with at least one primitive
    // intersection appends its ENTITY index to the per-pass entity buffer. The answer is the
    // run of indices this query appended: its count, and a pointer to its first element.
    //   * the exclude test is the EQUALITY form LineTestNearest uses: (id & mask) == exclude,
    //     mask the part-comparison mask when mbExcludeParts, else all ones;
    //   * an instance is tested only when its volume's type flags meet the query's AND the
    //     candidate's entity index ANDed with the query's volume-type flags is non-zero
    //     (an AND of the two, its low 16 bits tested) -- the second test is
    //     the console's own;
    //   * an entity is appended once per instance that hits, not once per entity.
    // =========================================================================================
    void FineIntersectionTestModule::ComputeVolumeTestFine(const InEventVolumeTestFine* lpQuery,
                                                           OutEventVolumeTestFineResult* lpOutResult,
                                                           void* lpEntityBuffer)
    {
        // The query padding (the image's 0.0f constant).
        static const f32 KF_VOLUME_TEST_FINE_PADDING = 0.0f;
        // The exclude-index sentinel query+0xCA is compared with.
        static const u16 KU16_NO_EXCLUDE_ENTITY_INDEX = 0xFFFF;

        FineIntersectionTestIO::OutputBuffer::EntityBuffer* lpEntities =
            static_cast<FineIntersectionTestIO::OutputBuffer::EntityBuffer*>(lpEntityBuffer);

        u32 lx32Mask;
        u32 lx32Exclude;
        if (lpQuery->mu16ExcludeEntityIndex != KU16_NO_EXCLUDE_ENTITY_INDEX)
        {
            const EntityId lExcludeEntityId = mpEntityManager->GetEntityIdByIndex(lpQuery->mu16ExcludeEntityIndex);
            lx32Mask    = lpQuery->mbExcludeParts ? lExcludeEntityId.GetPartComparisonMask() : ~0u;
            lx32Exclude = lx32Mask & static_cast<u32>(lExcludeEntityId);
        }
        else
        {
            lx32Exclude = static_cast<u32>(K_INVALID_ENTITY_ID);
            lx32Mask    = ~0u;
        }

        lpOutResult->mQueryId = lpQuery->mQueryId;

        s32 liNumEntities = 0;
        for (u16 lu16Candidate = 0; lu16Candidate < lpQuery->mu16NumEntities; ++lu16Candidate)
        {
            const u16 lu16EntityIndex = lpQuery->mpau16EntityIndices[lu16Candidate];
            const u32 lx32EntityId    = static_cast<u32>(mpEntityManager->GetEntityIdByIndex(lu16EntityIndex));
            if ((lx32EntityId & lx32Mask) == lx32Exclude)
            {
                continue;
            }

            s32 liFirstVolumeInstance = 0;   // written by the call, never read
            const VolumeInstance* lpVolumeInstance =
                mpEntityManager->GetFirstEntityVolumeInstance(lu16EntityIndex, &liFirstVolumeInstance);
            while (lpVolumeInstance != 0)
            {
                const s32 liVolumeIndex = lpVolumeInstance->miVolumeIndex;
                if ((mpVolumeManager->GetVolumeTypeFlags(liVolumeIndex) & lpQuery->mxVolumeTypeFlags) != 0 &&
                    static_cast<u16>(lu16EntityIndex & lpQuery->mxVolumeTypeFlags) != 0)
                {
                    const rw::collision::Volume* lapInputVolumes[1] =
                        { reinterpret_cast<const rw::collision::Volume*>(mpVolumeManager->GetRwVolume(liVolumeIndex)) };
                    const Matrix44Affine* lapInputMatrices[1] =
                        { &lpVolumeInstance->mWorldSpaceTransform };

                    rw::collision::VolumeVolumeQuery* lpQueryObject = mpVolumeVolumeQuery;
                    lpQueryObject->m_padding         = KF_VOLUME_TEST_FINE_PADDING;
                    lpQueryObject->m_inputVols       = lapInputVolumes;
                    lpQueryObject->m_inputMats       = lapInputMatrices;
                    lpQueryObject->m_numInputs       = 1;
                    lpQueryObject->m_currInput       = 0;
                    lpQueryObject->m_volRefPairCount = 0;
                    lpQueryObject->m_queryVol        =
                        reinterpret_cast<const rw::collision::Volume*>(&lpQuery->mVolumeBuffer);
                    lpQueryObject->m_queryMtx        = &lpQuery->mTransform;
                    lpQueryObject->m_cullTable       = 0;

                    if (mpVolumeVolumeQuery->GetPrimitiveIntersections() != 0)
                    {
                        lpEntities->Append(lu16EntityIndex);
                        ++liNumEntities;
                    }
                }
                lpVolumeInstance = static_cast<const EntityManager*>(mpEntityManager)
                                       ->GetVolumeInstance(lpVolumeInstance->miNextEntityVolumeInstance);
            }
        }

        lpOutResult->miNumEntities = liNumEntities;
        if (liNumEntities > 0)
        {
            lpOutResult->mpuResults = &(*lpEntities)[lpEntities->GetLength() - static_cast<u32>(liNumEntities)];
        }
        else
        {
            lpOutResult->mpuResults = 0;
        }
    }
}
