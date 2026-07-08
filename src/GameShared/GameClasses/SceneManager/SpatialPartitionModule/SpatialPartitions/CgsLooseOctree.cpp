// ===========================================================================
// CgsSceneManager::LooseOctree -- broad-phase loose-octree body-home TU.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (7 ledger functions):
//   LooseOctree::CalcNodeCorners              @ 0x828B0D40
//   LooseOctree::EntityInsideNodeBounds       @ 0x828C9768
//   LooseOctree::FlagBranchForUpdate          @ 0x828AA800
//   LooseOctree::LooseOctree (ctor)           @ 0x828C9718
//   LooseOctree::TestLineAgainstNodeBoundingBox @ 0x828B0FC8
//   LooseOctree::UpdateNodeYBounds            @ 0x828B0EC8
//   LooseOctree::operator new                 @ 0x828BADD8
//
// Behaviour-faithful (semantic parity): the X360 hand-vectorises the geometry over
// VMX; these bodies reproduce the same math on the named Vector3/Vector4 lanes. Node
// access uses the DWARF-attested byte offsets (mpNodes @ this+0x446A4, jobs @ +0x8D960).
// ===========================================================================
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.h"
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator / rw::Resource / rw::ResourceDescriptor (operator new)

// The EA-jobs SDK chain (pulled by CgsLooseOctree.h -> job.h) drags in a platform header that
// #defines a function-like GetFreeSpace macro; neutralise it before CgsCoarseQueryResultBuffer.h,
// whose CoarseQueryResultBuffer<N>::GetFreeSpace() member declaration would otherwise fail to parse.
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsCoarseQueryResultBuffer.h" // CoarseQueryResultBuffer<16384> (WaitForFrustumTestJobResults)
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.h"    // JobCoarseResultBuffer

#include <cmath>   // std::fabs

namespace CgsSceneManager
{
    // 0x828C9718 -- LooseOctree constructor. The base SpatialPartition sub-object is
    // constructed first (compiler-emitted), then this body default-constructs the 4
    // embedded frustum-test jobs (EA::Jobs::Job, 848 bytes each, DWARF maFrustumTestJobs[4])
    // at this+0x8D960 with a null name. The derived vtable stamp (X360 off_820F5C78) is
    // carried by the C++ object's vptr, not modelled by hand.
    LooseOctree::LooseOctree()
    {
        EA::Jobs::Job* lpJobs = reinterpret_cast<EA::Jobs::Job*>(
            reinterpret_cast<unsigned char*>(this) + KU_FRUSTUM_TEST_JOBS_BYTE_OFFSET);

        for (u32 luJob = 0; luJob < KU_NUM_FRUSTUM_TEST_JOBS; ++luJob)
        {
            new (&lpJobs[luJob]) EA::Jobs::Job(nullptr);
        }
    }

    // 0x828BADD8 -- placement-new that carves a LooseOctree out of a RenderWare resource
    // allocator. Descriptor entry 0 = {m_size = luSize, m_alignment = 16}, entries 1..4 =
    // {m_size = 0, m_alignment = 1}; dispatch through the allocator's resource-allocation
    // virtual (X360 vtable +0x10, modelled as DoAllocate) and return m_baseResources[0].
    // Mirror of CgsResource::Type::operator new @ 0x82666180.
    void* LooseOctree::operator new(size_t luSize, rw::IResourceAllocator* lpAllocator)
    {
        rw::ResourceDescriptor lDescriptor;
        for (u32 li = 0; li < 4; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(luSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;

        rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
        return lResource.m_baseResources[0];
    }

    // 0x828B0D40 -- CalcNodeCorners: emit the 8 world-space corners of the node's
    // (loose) box. The X360 loads the node half size (mParams0.w, lane 3) and half
    // height (mParams1.z, lane 2), then builds each corner as centre + (sx*halfSize,
    // sy*halfHeight, sz*halfSize) over the 8 sign combinations, using a rodata
    // sign-permute table (unk_82CDA350) and vector splat/xor to flip the sign bits.
    // Reconstructed at source level (the eight corners, centre-relative then
    // translated); the sole caller NodeInsideFrustum tests all 8 order-independently.
    void LooseOctree::CalcNodeCorners(const LooseOctreeNode* lpNode, Vector3* lpCornersOut) const
    {
        const float lfHS = lpNode->GetHalfSize().x;    // mParams0.w
        const float lfHH = lpNode->GetHalfHeight().x;  // mParams1.z
        const Vector3 lNodePosition = lpNode->GetPosition();

        // The 8 sign combinations for (x,y,z) == (+/-halfSize, +/-halfHeight, +/-halfSize).
        static const float KA_SIGN_X[8] = { -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f };
        static const float KA_SIGN_Y[8] = { -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f };
        static const float KA_SIGN_Z[8] = { -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f };

        for (u32 luCorner = 0; luCorner < 8; ++luCorner)
        {
            lpCornersOut[luCorner].x = lNodePosition.x + KA_SIGN_X[luCorner] * lfHS;
            lpCornersOut[luCorner].y = lNodePosition.y + KA_SIGN_Y[luCorner] * lfHH;
            lpCornersOut[luCorner].z = lNodePosition.z + KA_SIGN_Z[luCorner] * lfHS;
            lpCornersOut[luCorner].w = 0.0f;
        }
    }

    // 0x828C9768 -- EntityInsideNodeBounds: true iff the entity's bounding-sphere centre
    // lies within the node's horizontal (X and Z) half-base extent. Loads the node's
    // half base size from mParams0 (lane 1 splat), takes the X/Z offset of the sphere
    // centre from the node centre, and rejects if either exceeds the half base size.
    bool LooseOctree::EntityInsideNodeBounds(u16 lu16EntityIndex, LooseOctreeNode* lpNode)
    {
        CGS_ASSERT(lu16EntityIndex < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");

        const CgsGeometric::Sphere& lEntityBoundingSphere =
            GetEntityBoundingSphere(lu16EntityIndex);

        const float lfHalfBaseSize = lpNode->mParams0.y;

        Vector3 lDiff;
        lDiff.x = lEntityBoundingSphere.mPositionRadius.x - lpNode->mPosition.x;
        lDiff.y = lEntityBoundingSphere.mPositionRadius.y - lpNode->mPosition.y;
        lDiff.z = lEntityBoundingSphere.mPositionRadius.z - lpNode->mPosition.z;
        lDiff.w = lEntityBoundingSphere.mPositionRadius.w - lpNode->mPosition.w;

        if (std::fabs(lDiff.x) > lfHalfBaseSize || std::fabs(lDiff.z) > lfHalfBaseSize)
            return false;

        return true;
    }

    // 0x828AA800 -- FlagBranchForUpdate: set KU_OCTREE_NODE_FLAG_NEEDS_UPDATE on the given
    // node and every ancestor up to the root. The parent link is muParentIndex (0xFFFF at
    // the root); ancestors are addressed as mpNodes + 96 * parentIndex. The X360 forms the
    // node stride via 32 * (idx + rotl(idx,1)) == 96 * idx.
    void LooseOctree::FlagBranchForUpdate(LooseOctreeNode* lpNode)
    {
        lpNode->muFlags |= KU_OCTREE_NODE_FLAG_NEEDS_UPDATE;

        if (lpNode->muParentIndex == KU_INVALID_NODE)
            return;

        LooseOctreeNode* lpNodes = GetNodes();
        u16 lu16ParentIndex = lpNode->muParentIndex;

        do
        {
            LooseOctreeNode* lpParent = &lpNodes[lu16ParentIndex];
            lpParent->muFlags |= KU_OCTREE_NODE_FLAG_NEEDS_UPDATE;
            lu16ParentIndex = lpParent->muParentIndex;
        }
        while (lu16ParentIndex != KU_INVALID_NODE);
    }

    // 0x828B0FC8 -- TestLineAgainstNodeBoundingBox: slab test of the recursive line-test's
    // segment against the node's AABB. The node box (centre +/- (halfSize,halfHeight,halfSize))
    // is expressed relative to the line origin (params[0]); the per-axis entry/exit params are
    // invDir*(bbMin) and invDir*(bbMax) (invDir at params+0x30). The segment [0,1] overlaps the
    // box iff, on every axis, max(t0,t1) >= 0 and min(t0,t1) <= 1. Returns true on overlap.
    //
    // FLAG (confidence=low): the packing of the node extent vector via the rodata permute
    // table (unk_82CDA350) and the LineTestRecursiveFuncParams field offsets (line origin at
    // +0x00, inverse direction at +0x30) are inferred from the lane math; the param-block
    // field layout is not committed, so this is modelled at the slab-test source level.
    bool LooseOctree::TestLineAgainstNodeBoundingBox(
        const LooseOctreeNode* lpNode,
        const SpatialPartition::LineTestRecursiveFuncParams* lpParams) const
    {
        const float* lpParamsF = reinterpret_cast<const float*>(lpParams);
        const Vector3 lLineOrigin = { lpParamsF[0], lpParamsF[1], lpParamsF[2], 0.0f };
        // Inverse line direction lives one vector further into the param block (+0x30).
        const Vector3 lInvDir     = { lpParamsF[12], lpParamsF[13], lpParamsF[14], 0.0f };

        const Vector3 lNodePos = lpNode->GetPosition();
        const float   lfHS = lpNode->GetHalfSize().x;
        const float   lfHH = lpNode->GetHalfHeight().x;
        const Vector3 lExtent = { lfHS, lfHH, lfHS, 0.0f };

        const Vector3 lBbMin = {
            (lNodePos.x - lExtent.x) - lLineOrigin.x,
            (lNodePos.y - lExtent.y) - lLineOrigin.y,
            (lNodePos.z - lExtent.z) - lLineOrigin.z, 0.0f };
        const Vector3 lBbMax = {
            (lNodePos.x + lExtent.x) - lLineOrigin.x,
            (lNodePos.y + lExtent.y) - lLineOrigin.y,
            (lNodePos.z + lExtent.z) - lLineOrigin.z, 0.0f };

        const float lt0x = lInvDir.x * lBbMin.x, lt1x = lInvDir.x * lBbMax.x;
        const float lt0y = lInvDir.y * lBbMin.y, lt1y = lInvDir.y * lBbMax.y;
        const float lt0z = lInvDir.z * lBbMin.z, lt1z = lInvDir.z * lBbMax.z;

        const float ltMaxX = lt0x > lt1x ? lt0x : lt1x, ltMinX = lt0x < lt1x ? lt0x : lt1x;
        const float ltMaxY = lt0y > lt1y ? lt0y : lt1y, ltMinY = lt0y < lt1y ? lt0y : lt1y;
        const float ltMaxZ = lt0z > lt1z ? lt0z : lt1z, ltMinZ = lt0z < lt1z ? lt0z : lt1z;

        const bool lbX = !(0.0f > ltMaxX) && (1.0f >= ltMinX);
        const bool lbY = !(0.0f > ltMaxY) && (1.0f >= ltMinY);
        const bool lbZ = !(0.0f > ltMaxZ) && (1.0f >= ltMinZ);

        return lbX && lbY && lbZ;
    }

    // 0x828B0EC8 -- UpdateNodeYBounds: if the entity's vertical extent (sphere centre.y +/-
    // radius) pushes past the node's current [minY, maxY], flag the branch for update. The
    // X360 compares the entity top (y + radius) against node maxY, and (only if that did not
    // trip) node minY against the entity bottom (y - radius); either overflow calls
    // FlagBranchForUpdate. The actual bound widening happens in the caller's update pass.
    void LooseOctree::UpdateNodeYBounds(LooseOctreeNode* lpNode, u16 lu16EntityIndex)
    {
        CGS_ASSERT(lu16EntityIndex < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");

        const CgsGeometric::Sphere& lEntityBoundingSphere =
            GetEntityBoundingSphere(lu16EntityIndex);

        const float lfEntityY = lEntityBoundingSphere.mPositionRadius.y;
        const float lfRadius   = lEntityBoundingSphere.mPositionRadius.w;

        if (lfEntityY + lfRadius > lpNode->mParams1.y)          // entity top > node maxY
        {
            FlagBranchForUpdate(lpNode);
            return;
        }

        if (lpNode->mParams1.x > lfEntityY - lfRadius)          // node minY > entity bottom
        {
            FlagBranchForUpdate(lpNode);
        }
    }

    // 0x828B2558 -- WaitForFrustumTestJobResults: block on each of the 4 frustum-test jobs
    // that is active, then drain every per-query result run out of that job's result buffer
    // into the shared output CoarseQueryResultBuffer, one query == one batch. For query q the
    // run starts at maJobResultBuffers[i].mpu16Buffer + maQueryOffsets[q] and is
    // maQueryNumResults[q] long; the number of queries is the job's mQueryInfo.muNumQueries.
    // After draining, the job's active flag and query count are cleared.
    //
    // The huge LooseOctree layout is reached via the batch's attested byte offsets:
    //   mabFrustumJobActive[i]                          @ this+0x90B00 (bool, stride 0x001)
    //   maFrustumTestJobs[i]                            @ this+0x8D960 (Job,  stride 0x350)
    //   maFrustumTestJobData[i].mQueryInfo.muNumQueries @ this+0x8EEC8 (u32,  stride 0x800)
    //   maJobResultBuffers[i]                           @ this+0x90700 (JobCoarseResultBuffer, stride 0x100)
    //
    // The X360 WaitOn takes (job,0,0,-1); the committed vendor Job exposes only the nullary
    // WaitOn() (block-until-done, job.h:110), used here at semantic parity. Per-array byte
    // offsets are asm-attested; the intervening layout is not field-modelled, matching the
    // rest of this TU's reach-by-offset style.
    void LooseOctree::WaitForFrustumTestJobResults(CoarseQueryResultBuffer<16384>* lpResultBufferOut)
    {
        unsigned char* lpcThis = reinterpret_cast<unsigned char*>(this);

        for (u32 luJob = 0; luJob < KU_NUM_FRUSTUM_TEST_JOBS; ++luJob)
        {
            bool* lpbActive =
                reinterpret_cast<bool*>(lpcThis + 0x90B00 + luJob);
            if (!*lpbActive)
                continue;

            EA::Jobs::Job* lpJob = reinterpret_cast<EA::Jobs::Job*>(
                lpcThis + KU_FRUSTUM_TEST_JOBS_BYTE_OFFSET + 0x350 * luJob);
            lpJob->WaitOn();

            u32* lpuNumQueries =
                reinterpret_cast<u32*>(lpcThis + 0x8EEC8 + 0x800 * luJob);
            const u32 luNumQueries = *lpuNumQueries;

            if (luNumQueries != 0)
            {
                JobCoarseResultBuffer* lpJobResultBuffer =
                    reinterpret_cast<JobCoarseResultBuffer*>(lpcThis + 0x90700 + 0x100 * luJob);

                for (u32 luQuery = 0; luQuery < luNumQueries; ++luQuery)
                {
                    lpResultBufferOut->BeginResultsBatch();
                    lpResultBufferOut->PushResults(
                        lpJobResultBuffer->mpu16Buffer + lpJobResultBuffer->maQueryOffsets[luQuery],
                        lpJobResultBuffer->maQueryNumResults[luQuery]);

                    CGS_ASSERT(lpResultBufferOut->GetNumResultsAttempted() ==
                               lpResultBufferOut->GetNumResultsWritten(),
                               "lpResultBufferOut->GetNumResultsAttempted() == lpResultBufferOut->GetNumResultsWritten()");

                    lpResultBufferOut->EndResultsBatch();
                }
            }

            *lpbActive = false;
            *lpuNumQueries = 0;
        }
    }

    // 0x828CA360 -- AdaptiveDepthUpdateAddNodesRecursive: walk the branch rooted at
    // lu16NodeIndex, subdividing nodes that have accumulated more entities than the
    // adaptive split threshold. Proceeds only while the free-node-group pool has spare
    // capacity and we are shallower than muAdaptiveMaxDepth-1. A leaf (no first child)
    // that reaches here is split via SplitAndPropogateRecursive; an interior node
    // recurses into each of its 4 children whose sub-tree count still exceeds the
    // threshold and which is flagged NEEDS_UPDATE.
    void LooseOctree::AdaptiveDepthUpdateAddNodesRecursive(u16 lu16NodeIndex, u32 luDepth)
    {
        LooseOctreeNode* lpNodes = GetNodes();
        LooseOctreeNode* lpNode  = &lpNodes[lu16NodeIndex];

        if (GetFreeNodeGroupNumFree() == 0)
            return;
        if (luDepth >= GetAdaptiveMaxDepth() - 1)
            return;

        if (lpNode->muFirstChildIndex == KU_INVALID_NODE)
        {
            CGS_ASSERT(lpNode->mEntityList.CountElements() > GetAdaptiveNodeSplitThreshold(),
                       "(uint32_t)lpNode->mEntityList.CountElements() > muAdaptiveNodeSplitThreshold");
            CGS_ASSERT(GetFreeNodeGroupNumFree() > 0, "mFreeNodeGroupPool.GetNumFree() > 0");
            CGS_ASSERT(luDepth >= GetDepth() - 1, "luDepth >= muDepth-1");

            SplitAndPropogateRecursive(lu16NodeIndex, luDepth);
        }
        else
        {
            const u16 lu16FirstChild = lpNode->muFirstChildIndex;
            for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
            {
                const u16 lu16ChildIndex = static_cast<u16>(lu16FirstChild + luChild);
                LooseOctreeNode* lpChild = &lpNodes[lu16ChildIndex];
                if (lpChild->muSubTreeEntityCount > GetAdaptiveNodeSplitThreshold() &&
                    (lpChild->muFlags & KU_OCTREE_NODE_FLAG_NEEDS_UPDATE) != 0)
                {
                    AdaptiveDepthUpdateAddNodesRecursive(lu16ChildIndex, luDepth + 1);
                }
            }
        }
    }

    // 0x828CA4F8 -- AdaptiveDepthUpdateRemoveNodesRecursive: the inverse pass. Walk the
    // branch and, once no dynamic node groups are outstanding beyond the static set,
    // either recurse into the flagged children (while still deep enough or still busy) or
    // merge the sub-tree back into this node (MergeSubTreeRecursive) when the adaptive
    // depth/threshold criteria no longer justify the subdivision.
    void LooseOctree::AdaptiveDepthUpdateRemoveNodesRecursive(u16 lu16NodeIndex, u32 luDepth)
    {
        LooseOctreeNode* lpNodes = GetNodes();
        LooseOctreeNode* lpNode  = &lpNodes[lu16NodeIndex];

        if (GetFreeNodeGroupNumUsed() == GetNumStaticNodes())
            return;
        if (lpNode->muFirstChildIndex == KU_INVALID_NODE)
            return;

        const u32 luChildDepth = luDepth + 1;
        if (luChildDepth < GetDepth() ||
            lpNode->muSubTreeEntityCount > GetAdaptiveNodeSplitThreshold())
        {
            const u16 lu16FirstChild = lpNode->muFirstChildIndex;
            for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
            {
                const u16 lu16ChildIndex = static_cast<u16>(lu16FirstChild + luChild);
                LooseOctreeNode* lpChild = &lpNodes[lu16ChildIndex];
                if ((lpChild->muFlags & KU_OCTREE_NODE_FLAG_NEEDS_UPDATE) != 0)
                    AdaptiveDepthUpdateRemoveNodesRecursive(lu16ChildIndex, luChildDepth);
            }
        }
        else
        {
            MergeSubTreeRecursive(lu16NodeIndex, lpNode, luChildDepth);
        }
    }

    // 0x828D0180 -- Update: per-frame maintenance. Only does work when the root node is
    // flagged NEEDS_UPDATE; then it runs the adaptive-depth remove pass, the add pass, and
    // finally the recursive loose-bounds update, all from the root (node index 0, depth 0).
    void LooseOctree::Update()
    {
        LooseOctreeNode* lpRootNode = GetRootNode();
        if ((lpRootNode->muFlags & KU_OCTREE_NODE_FLAG_NEEDS_UPDATE) != 0)
        {
            AdaptiveDepthUpdateRemoveNodesRecursive(0, 0);
            AdaptiveDepthUpdateAddNodesRecursive(0, 0);
            UpdateRecursive(0);
        }
    }
}
