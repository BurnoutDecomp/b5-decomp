// ===========================================================================
// CgsSceneManager::LooseOctree -- broad-phase loose-quadtree body-home TU.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   LooseOctree::LooseOctree (ctor)             @ 0x828C9718
//   LooseOctree::operator new                   @ 0x828BADD8
//   LooseOctree::Construct                      @ 0x828C99D8
//   LooseOctree::AllocRecursive                 @ 0x828BB4A0
//   LooseOctree::PrepareRecursive               @ 0x828BB1E8
//   LooseOctree::Prepare                        @ 0x828CA2D0
//   LooseOctree::CalcNextSubNode                @ 0x828B11A0
//   LooseOctree::CalcNodeCorners                @ 0x828B0D40
//   LooseOctree::EntityInsideNodeBounds         @ 0x828C9768
//   LooseOctree::FlagBranchForUpdate            @ 0x828AA800
//   LooseOctree::TestLineAgainstNodeBoundingBox @ 0x828B0FC8
//   LooseOctree::UpdateNodeYBounds              @ 0x828B0EC8
//   LooseOctree::UpdateRecursive                @ 0x828B12A0
//   LooseOctree::AddEntityToGraph/AddEntityInternal   @ 0x828BB648
//   LooseOctree::RemoveEntityFromGraph/RemoveEntityInternal @ 0x828C9818 / 0x828BB948
//   LooseOctree::SetEntityPosition              @ 0x828C9820
//   LooseOctree::SetEntityRadius                @ 0x828BC740
//   LooseOctree::Update                         @ 0x828D0180
//   LooseOctree::AddJobFrustumTest              @ 0x828AA958
//   LooseOctree::StartFrustumTestJobs           @ 0x828B23E0
//   LooseOctree::WaitForFrustumTestJobResults   @ 0x828B2558
//   LooseOctree::FrustumTestVp                  @ 0x828CA7F8
//   LooseOctree::FrustumTestVpRecursive         @ 0x828BDC38
//   LooseOctree::TrivialAcceptRecursive         @ 0x828B1B50
//   LooseOctree::FrustumTestEntities            @ 0x828B1CA0
//   LooseOctree::NodeInsideFrustum              @ 0x828BDAC0
//
// Behaviour-faithful (semantic parity): the X360 hand-vectorises the geometry over
// VMX; these bodies reproduce the same math on the named Vector3/Vector4 lanes.
//
// ---------------------------------------------------------------------------
// FLAG PC-platform leaf: THE FRUSTUM TEST RUNS SYNCHRONOUSLY.
// The console posts the per-frame frustum tests to the EA::Jobs scheduler
// (StartFrustumTestJobs -> FrustumTestEntry -> FrustumTestJob::Execute on an idle
// hardware thread) and blocks on them in WaitForFrustumTestJobResults. This host has
// no job scheduler wired, so StartFrustumTestJobs runs each pending job's queries
// inline instead of calling JobScheduler::AddJobs -- and only that call is replaced.
// Everything the results depend on is the real thing and is preserved exactly:
//   * the 0x800-byte FrustumTestJobData block and its three parallel query arrays,
//   * the per-job JobCoarseResultBuffer (query offsets / per-query counts / the
//     shared u16 run pool) and the mabFrustumJobRequested -> mabFrustumJobActive
//     handshake,
//   * WaitForFrustumTestJobResults' drain into the shared CoarseQueryResultBuffer,
//     one BeginResultsBatch/EndResultsBatch pair per query, in query order,
//   * the traversal itself and the per-entity accept test.
// The RESULTS and their ORDER are therefore identical to the console's.
//
// FLAG (deferred, not a divergence in the results): the ADAPTIVE-DEPTH refinement
// (SplitAndPropogateRecursive @0x828BBBD0 / MergeSubTreeRecursive @0x828BC340, driven
// from Update's AdaptiveDepthUpdate*Recursive passes) is not reconstructed here. It
// subdivides a static leaf that has accumulated more than muAdaptiveNodeSplitThreshold
// entities, down to muAdaptiveMaxDepth, by handing out four-node groups from
// mFreeNodeGroupPool. It changes only how DEEP the walk can prune -- a query's result
// set is the same either way, because a node is only pruned when its loose bounds miss
// the frustum entirely and only trivially accepted when they are fully inside, and
// both properties are inherited by a node's children. The static tree Construct builds
// (muDepth levels) is the tree used; the pool is allocated with the console's sizing so
// the refinement can be dropped in without touching the memory profile.
// ===========================================================================
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree.h"
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator / rw::Resource / rw::ResourceDescriptor (operator new)
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsSpatialPartitionManager.h" // SpatialPartitionConstructParams

// The EA-jobs SDK chain drags in a platform header that #defines a function-like
// GetFreeSpace macro; neutralise it before CgsCoarseQueryResultBuffer.h, whose
// CoarseQueryResultBuffer<N>::GetFreeSpace() declaration would otherwise fail to parse.
#ifdef GetFreeSpace
#undef GetFreeSpace
#endif
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsCoarseQueryResultBuffer.h" // CoarseQueryResultBuffer<16384>

#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [DIAG culling wave]

#include <cmath>     // std::fabs
#include <cstdlib>   // std::getenv ([DIAG] BRN_CULL_OFF)
#include <cstring>   // std::memcpy

namespace CgsSceneManager
{
    namespace
    {
        // X360 unk_83085A70 (CgsLooseOctree.cpp:62, .data) --
        // Vector3 KA_LOOSE_OCTREE_CHILD_OFFSETS[4], scaled by the parent's FULL size
        // when PrepareRecursive places the four children, so each component is a
        // quarter of the parent extent in X/Z and zero in Y (the tree does not split
        // in Y -- it tracks a loose [minY, maxY] band per node). The four sign pairs
        // are the four XZ quadrants; the intra-group ORDER is unobservable (every
        // reader either visits all four children or resolves a child through
        // CalcNextSubNode, which is defined below against this same order).
        const f32 KAF_CHILD_OFFSET_X[4] = { -0.25f,  0.25f, -0.25f,  0.25f };
        const f32 KAF_CHILD_OFFSET_Z[4] = { -0.25f, -0.25f,  0.25f,  0.25f };

    }

    // 0x828C9718 -- LooseOctree constructor. The base SpatialPartition sub-object is
    // constructed first (compiler-emitted), then this body default-constructs the four
    // embedded frustum-test jobs. On this host the EA::Jobs objects are not held (see
    // the TU banner's FLAG PC-platform leaf note), so the ctor just parks the pointers.
    LooseOctree::LooseOctree()
        : muDepth(0)
        , miNumStaticNodes(0)
        , mfBaseSize(0.0f)
        , mfLooseness(0.0f)
        , mpRootNode(0)
        , mpNodes(0)
        , mpNodesEntityInfo(0)
        , muNumNodes(0)
        , muNumNodeGroups(0)
        , muAdaptiveNodeSplitThreshold(0)
        , muAdaptiveMaxDepth(0)
    {
        mCentrePos.x = mCentrePos.y = mCentrePos.z = mCentrePos.w = 0.0f;

        for (u32 luJob = 0; luJob < KU_NUM_FRUSTUM_TEST_JOBS; ++luJob)
        {
            mabFrustumJobActive[luJob]    = false;
            mabFrustumJobRequested[luJob] = false;
        }
    }

    // 0x828BADD8 -- placement-new that carves a LooseOctree out of a RenderWare resource
    // allocator. Descriptor entry 0 = {m_size = luSize, m_alignment = 16}, entries 1..4 =
    // {m_size = 0, m_alignment = 1}; dispatch through the allocator's resource-allocation
    // virtual and return m_baseResources[0].
    void* LooseOctree::operator new(size_t luSize, rw::IResourceAllocator* lpAllocator)
    {
        rw::ResourceDescriptor lDescriptor;
        for (u32 li = 0; li < rw::KU_RESOURCE_LANE_COUNT; ++li)
        {
            lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(luSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;

        rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
        return lResource.m_baseResources[0];
    }

    namespace
    {
        // The same descriptor-driven carve the octree's own operator new uses, for the
        // two side arrays Construct allocates (X360 @0x828C9B08 / @0x828C9B74).
        void* AllocFromResourceAllocator(rw::IResourceAllocator* lpAllocator,
                                         u32 luSize, u32 luAlignment)
        {
            rw::ResourceDescriptor lDescriptor;
            for (u32 li = 0; li < rw::KU_RESOURCE_LANE_COUNT; ++li)
            {
                lDescriptor.m_baseResourceDescriptors[li].m_size      = 0;
                lDescriptor.m_baseResourceDescriptors[li].m_alignment = 1;
            }
            lDescriptor.m_baseResourceDescriptors[0].m_size      = luSize;
            lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;

            rw::Resource lResource = lpAllocator->DoAllocate(lDescriptor, 0);
            return lResource.m_baseResources[0];
        }
    }

    // ===========================================================================
    // Construct @ 0x828C99D8
    //
    // Copy the construct params out, size the node array from the level count, carve
    // the node array + the per-node sub-tree-mask array + the free-node-group pool out
    // of the scene resource allocator, build the STATIC tree topology (AllocRecursive)
    // and reset the job state.
    //
    // The node count (asm @0x828C9A40..0x828C9AB0):
    //   uiNumSubNodes = sum(k = 1 .. muDepth-1) 4^k
    //   uiNumNodes    = ((uiNumSubNodes + 0x2003) & ~3) + 1
    // i.e. the static sub-node total rounded up to a whole four-node group, plus
    // KU_LOOSE_OCTREE_ADAPTIVE_NODEARRAY_POOL_SIZE spare groups for the adaptive-depth
    // refinement, plus node 0 (the root). Reproduced exactly so the memory profile
    // matches the console's.
    // ===========================================================================
    void LooseOctree::Construct(SpatialPartitionConstructParams* lpParams,
                                rw::IResourceAllocator* lpAllocator)
    {
        CGS_ASSERT(lpParams != 0, "lpParams != NULL");
        CGS_ASSERT(lpAllocator != 0, "lpAllocator != NULL");

        muDepth                      = static_cast<u32>(lpParams->muDepth);
        mCentrePos                   = lpParams->mCentrePos;
        mfBaseSize                   = lpParams->mfBaseSize;
        mfLooseness                  = lpParams->mfLooseness;
        muAdaptiveNodeSplitThreshold = static_cast<u32>(lpParams->muAdaptiveNodeSplitThreshold);
        muAdaptiveMaxDepth           = static_cast<u32>(lpParams->muAdaptiveMaxDepth);

        u32 luNumSubNodes = 0;
        if (muDepth > 1)
        {
            u32 luShift = 2;
            for (u32 luLevel = muDepth - 1; luLevel != 0; --luLevel)
            {
                luNumSubNodes += (1u << luShift);
                luShift += 2;
            }
        }

        const u32 luRounded  = (luNumSubNodes + (KU_LOOSE_OCTREE_ADAPTIVE_NODEARRAY_POOL_SIZE * 4) + 3) & ~3u;
        muNumNodeGroups      = luRounded >> 2;
        muNumNodes           = luRounded + 1;

        mpNodes = static_cast<LooseOctreeNode*>(
            AllocFromResourceAllocator(lpAllocator, muNumNodes * sizeof(LooseOctreeNode), 16));
        CGS_ASSERT(mpNodes != 0, "Failed to allocate mpNodes\n");

        mpNodesEntityInfo = static_cast<LooseOctreeNodeEntityInfo*>(
            AllocFromResourceAllocator(lpAllocator, muNumNodes * sizeof(LooseOctreeNodeEntityInfo), 128));

        if (mpNodes == 0 || mpNodesEntityInfo == 0)
        {
            mpRootNode = 0;
            muNumNodes = 0;
            return;
        }

        // Root: node index 0, no children until AllocRecursive links them.
        mpRootNode = mpNodes;
        mpRootNode->muFirstChildIndex = KU_INVALID_NODE;

        AllocRecursive(0, 0, KU_INVALID_NODE);

        for (u32 luJob = 0; luJob < KU_NUM_FRUSTUM_TEST_JOBS; ++luJob)
        {
            // JobCoarseResultBuffer::Construct @0x828BB128 -- clear the counters and
            // publish the run pool (the console carves 0x4000 bytes out of the same
            // allocator; the block is held inline here, see the header).
            JobCoarseResultBuffer& lrBuffer = maJobResultBuffers[luJob];
            lrBuffer.muNumQueries         = 0;
            lrBuffer.muCurrentWriteOffset = 0;
            for (u32 luQuery = 0; luQuery < KU_JOB_BUFFER_MAX_NUM_QUERIES; ++luQuery)
            {
                lrBuffer.maQueryOffsets[luQuery]    = 0;
                lrBuffer.maQueryNumResults[luQuery] = 0;
            }
            lrBuffer.mpu16Buffer = maJobResultStorage[luJob];

            mabFrustumJobActive[luJob]    = false;
            mabFrustumJobRequested[luJob] = false;
            maFrustumTestJobData[luJob].mQueryInfo.muNumQueries = 0;
        }
    }

    // ===========================================================================
    // AllocRecursive @ 0x828BB4A0
    //
    // Build the STATIC tree topology: link node lu16NodeIndex to its parent, and while
    // the level budget lasts hand it a four-node group out of the free pool and recurse
    // into the four children. The console's pool hands out a group's precomputed first
    // child index (Construct seeds mpElements[i].muFirstChildIndex = 1 + 4*i), so group
    // g owns nodes 1+4g .. 4+4g -- reproduced here by taking the groups in order.
    // miNumStaticNodes is the number of GROUPS the static tree consumed (Construct
    // latches the pool's used count after this returns).
    // ===========================================================================
    void LooseOctree::AllocRecursive(u32 luDepth, u16 lu16NodeIndex, u16 lu16ParentIndex)
    {
        LooseOctreeNode& lrNode = mpNodes[lu16NodeIndex];
        lrNode.muParentIndex     = lu16ParentIndex;
        lrNode.muFirstChildIndex = KU_INVALID_NODE;

        if (luDepth + 1 >= muDepth)
        {
            return;   // deepest static level -- a leaf until the adaptive pass splits it
        }

        // Take the next four-node group (index 1 + 4 * groupIndex).
        const u32 luGroup = static_cast<u32>(miNumStaticNodes);
        if (luGroup >= muNumNodeGroups)
        {
            CGS_ASSERT(false, "Failed to allocate child nodes\n");
            return;
        }
        ++miNumStaticNodes;

        const u16 lu16FirstChild = static_cast<u16>(1 + 4 * luGroup);
        lrNode.muFirstChildIndex = lu16FirstChild;

        for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
        {
            AllocRecursive(luDepth + 1,
                           static_cast<u16>(lu16FirstChild + luChild),
                           lu16NodeIndex);
        }
    }

    // ===========================================================================
    // PrepareRecursive @ 0x828BB1E8
    //
    // Seed one node's geometry + empty state, then recurse into its four children.
    // lfSize is the node's FULL size; the half size is lfSize * 0.5 (proven by
    // SplitAndPropogateRecursive @0x828BBD30, which recovers a node's full size as
    // mParams0.y * 2.0 and passes mParams0.y down as the child's lfSize).
    //   mParams0.y HalfBaseSize        = lfSize * 0.5
    //   mParams0.w HalfSize            = HalfBaseSize
    //   mParams0.z MaxRadiusThreshold  = mfLooseness * lfSize
    //   mParams0.x MaxEntityRadius     = 0
    //   mParams1.x MinY = +BIG, .y MaxY = -BIG   (empty band; vmin/vmax'd by entities)
    //   mParams1.z HalfHeight          = 0
    //   mHalfDimensions = { HalfSize, HalfHeight, HalfSize }
    // The topology (parent / first child) is NOT touched here -- it belongs to
    // AllocRecursive and the adaptive split/merge.
    // ===========================================================================
    void LooseOctree::PrepareRecursive(u16 lu16NodeIndex, Vector3 lPosition, f32 lfSize)
    {
        CGS_ASSERT(lu16NodeIndex != KU_INVALID_NODE, "Can't prepare invalid node\n");
        if (lu16NodeIndex == KU_INVALID_NODE)
        {
            return;
        }

        LooseOctreeNode& lrNode = mpNodes[lu16NodeIndex];
        const f32 lfHalfSize = lfSize * 0.5f;

        mpNodesEntityInfo[lu16NodeIndex].mxSubTreeEntityFlags = 0;

        lrNode.muSubTreeEntityCount = 0;
        lrNode.mxNodeEntityFlags    = 0;
        lrNode.muNumElements        = 0;
        lrNode.muListElementsPad    = 0;
        lrNode.muHeadIndex          = KU_INVALID_NODE;
        lrNode.muTailIndex          = KU_INVALID_NODE;
        lrNode.muFlags              = 0;

        lrNode.mParams0.y = lfHalfSize;                 // HalfBaseSize
        lrNode.mParams0.w = lrNode.mParams0.y;          // HalfSize
        lrNode.mParams0.z = mfLooseness * lfSize;       // MaxRadiusThreshold
        lrNode.mParams0.x = 0.0f;                       // MaxEntityRadius

        lrNode.mPosition  = lPosition;

        lrNode.mParams1.x = KF_LOOSE_OCTREE_MAX_Y;      // MinY  (empty band)
        lrNode.mParams1.y = KF_LOOSE_OCTREE_MIN_Y;      // MaxY
        lrNode.mParams1.z = 0.0f;                       // HalfHeight

        lrNode.mHalfDimensions.x = lrNode.mParams0.w;
        lrNode.mHalfDimensions.y = lrNode.mParams1.z;
        lrNode.mHalfDimensions.z = lrNode.mParams0.w;
        lrNode.mHalfDimensions.w = 0.0f;

        if (lrNode.muFirstChildIndex == KU_INVALID_NODE)
        {
            return;
        }

        for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
        {
            Vector3 lChildPosition;
            lChildPosition.x = lPosition.x + KAF_CHILD_OFFSET_X[luChild] * lfSize;
            lChildPosition.y = lPosition.y;
            lChildPosition.z = lPosition.z + KAF_CHILD_OFFSET_Z[luChild] * lfSize;
            lChildPosition.w = 0.0f;

            PrepareRecursive(static_cast<u16>(lrNode.muFirstChildIndex + luChild),
                             lChildPosition, lfHalfSize);
        }
    }

    // ===========================================================================
    // Prepare @ 0x828CA2D0
    //
    //   PrepareRecursive(0, mCentrePos, mfBaseSize);
    //   mpRootNode->SetHalfBaseSize(mfBaseSize * 0.5);   // a redundant restatement of
    //                                                    // what PrepareRecursive stored
    //   return true;
    // ===========================================================================
    bool LooseOctree::Prepare()
    {
        if (mpNodes == 0)
        {
            return false;
        }

        PrepareRecursive(0, mCentrePos, mfBaseSize);

        // The root's HalfBaseSize is then OVERRIDDEN to mfBaseSize * 4 (X360 @0x828CA328,
        // flt_82004EF4 == 4.0f) -- four times the world extent, so EntityInsideNodeBounds
        // can never reject at the root and an entity outside the authored world still
        // files somewhere. (The pseudocode drops this fmuls entirely; only the asm has it.)
        mpRootNode->mParams0.y = mfBaseSize * 4.0f;

        for (u32 luEntity = 0; luEntity < static_cast<u32>(KI_MAX_NUM_ENTITIES); ++luEntity)
        {
            maEntityNodeIndex[luEntity] = KU_INVALID_NODE;
        }

        return true;
    }

    bool LooseOctree::Release()
    {
        return true;
    }

    void LooseOctree::Destruct()
    {
    }

    // ===========================================================================
    // CalcNextSubNode @ 0x828B11A0 -- which of the four sub-nodes a position falls in.
    // The split is in the world XZ plane about the node centre; the child order is the
    // one KAF_CHILD_OFFSET_* above lays the children out in ((x > cx) | (z > cz) << 1).
    // ===========================================================================
    u32 LooseOctree::CalcNextSubNode(const LooseOctreeNode* lpNode, const Vector4& lrPosition) const
    {
        u32 luChild = 0;
        if (lrPosition.x > lpNode->mPosition.x) { luChild |= 1u; }
        if (lrPosition.z > lpNode->mPosition.z) { luChild |= 2u; }
        return luChild;
    }

    // 0x828B0D40 -- CalcNodeCorners: the 8 world-space corners of the node's (loose)
    // box, mPosition +/- { HalfSize, HalfHeight, HalfSize }.
    void LooseOctree::CalcNodeCorners(const LooseOctreeNode* lpNode, Vector3* lpCornersOut) const
    {
        const f32 lfHS = lpNode->GetHalfSize();
        const f32 lfHH = lpNode->GetHalfHeight();
        const Vector3 lNodePosition = lpNode->GetPosition();

        static const f32 KA_SIGN_X[8] = { -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f,  1.0f,  1.0f };
        static const f32 KA_SIGN_Y[8] = { -1.0f, -1.0f,  1.0f,  1.0f, -1.0f, -1.0f,  1.0f,  1.0f };
        static const f32 KA_SIGN_Z[8] = { -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f, -1.0f,  1.0f };

        for (u32 luCorner = 0; luCorner < 8; ++luCorner)
        {
            lpCornersOut[luCorner].x = lNodePosition.x + KA_SIGN_X[luCorner] * lfHS;
            lpCornersOut[luCorner].y = lNodePosition.y + KA_SIGN_Y[luCorner] * lfHH;
            lpCornersOut[luCorner].z = lNodePosition.z + KA_SIGN_Z[luCorner] * lfHS;
            lpCornersOut[luCorner].w = 0.0f;
        }
    }

    // 0x828C9768 -- EntityInsideNodeBounds: true iff the entity's bounding-sphere centre
    // lies within the node's horizontal (X and Z) half-base extent.
    bool LooseOctree::EntityInsideNodeBounds(u16 lu16EntityIndex, LooseOctreeNode* lpNode)
    {
        CGS_ASSERT(lu16EntityIndex < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");

        const CgsGeometric::Sphere& lrSphere = GetEntityBoundingSphere(lu16EntityIndex);
        const f32 lfHalfBaseSize = lpNode->GetHalfBaseSize();

        const f32 lfDx = lrSphere.mPositionRadius.x - lpNode->mPosition.x;
        const f32 lfDz = lrSphere.mPositionRadius.z - lpNode->mPosition.z;

        if (std::fabs(lfDx) > lfHalfBaseSize || std::fabs(lfDz) > lfHalfBaseSize)
        {
            return false;
        }
        return true;
    }

    // 0x828AA800 -- FlagBranchForUpdate: set NEEDS_UPDATE on the node and every ancestor.
    void LooseOctree::FlagBranchForUpdate(LooseOctreeNode* lpNode)
    {
        lpNode->muFlags |= KU_OCTREE_NODE_FLAG_NEEDS_UPDATE;

        u16 lu16ParentIndex = lpNode->muParentIndex;
        while (lu16ParentIndex != KU_INVALID_NODE)
        {
            LooseOctreeNode* lpParent = &mpNodes[lu16ParentIndex];
            lpParent->muFlags |= KU_OCTREE_NODE_FLAG_NEEDS_UPDATE;
            lu16ParentIndex = lpParent->muParentIndex;
        }
    }

    // 0x828B0FC8 -- TestLineAgainstNodeBoundingBox: slab test of the recursive line
    // test's segment against the node's AABB. The node box (centre +/- (halfSize,
    // halfHeight, halfSize)) is expressed relative to the line origin (params +0x00);
    // the per-axis entry/exit params are invDir * bbMin and invDir * bbMax (invDir at
    // params +0x30). The segment [0,1] overlaps iff, on every axis, max(t0,t1) >= 0 and
    // min(t0,t1) <= 1.
    bool LooseOctree::TestLineAgainstNodeBoundingBox(
        const LooseOctreeNode* lpNode,
        const SpatialPartition::LineTestRecursiveFuncParams* lpParams) const
    {
        const f32* lpParamsF = reinterpret_cast<const f32*>(lpParams);
        const Vector3 lLineOrigin = { lpParamsF[0], lpParamsF[1], lpParamsF[2], 0.0f };
        const Vector3 lInvDir     = { lpParamsF[12], lpParamsF[13], lpParamsF[14], 0.0f };

        const Vector3 lNodePos = lpNode->GetPosition();
        const f32     lfHS = lpNode->GetHalfSize();
        const f32     lfHH = lpNode->GetHalfHeight();
        const Vector3 lExtent = { lfHS, lfHH, lfHS, 0.0f };

        const Vector3 lBbMin = {
            (lNodePos.x - lExtent.x) - lLineOrigin.x,
            (lNodePos.y - lExtent.y) - lLineOrigin.y,
            (lNodePos.z - lExtent.z) - lLineOrigin.z, 0.0f };
        const Vector3 lBbMax = {
            (lNodePos.x + lExtent.x) - lLineOrigin.x,
            (lNodePos.y + lExtent.y) - lLineOrigin.y,
            (lNodePos.z + lExtent.z) - lLineOrigin.z, 0.0f };

        const f32 lt0x = lInvDir.x * lBbMin.x, lt1x = lInvDir.x * lBbMax.x;
        const f32 lt0y = lInvDir.y * lBbMin.y, lt1y = lInvDir.y * lBbMax.y;
        const f32 lt0z = lInvDir.z * lBbMin.z, lt1z = lInvDir.z * lBbMax.z;

        const f32 ltMaxX = lt0x > lt1x ? lt0x : lt1x, ltMinX = lt0x < lt1x ? lt0x : lt1x;
        const f32 ltMaxY = lt0y > lt1y ? lt0y : lt1y, ltMinY = lt0y < lt1y ? lt0y : lt1y;
        const f32 ltMaxZ = lt0z > lt1z ? lt0z : lt1z, ltMinZ = lt0z < lt1z ? lt0z : lt1z;

        const bool lbX = !(0.0f > ltMaxX) && (1.0f >= ltMinX);
        const bool lbY = !(0.0f > ltMaxY) && (1.0f >= ltMinY);
        const bool lbZ = !(0.0f > ltMaxZ) && (1.0f >= ltMinZ);

        return lbX && lbY && lbZ;
    }

    // 0x828B0EC8 -- UpdateNodeYBounds: if the entity's vertical extent pushes past the
    // node's current [minY, maxY], flag the branch so UpdateRecursive re-derives it.
    void LooseOctree::UpdateNodeYBounds(LooseOctreeNode* lpNode, u16 lu16EntityIndex)
    {
        CGS_ASSERT(lu16EntityIndex < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");

        const CgsGeometric::Sphere& lrSphere = GetEntityBoundingSphere(lu16EntityIndex);
        const f32 lfEntityY = lrSphere.mPositionRadius.y;
        const f32 lfRadius  = lrSphere.mPositionRadius.w;

        if (lfEntityY + lfRadius > lpNode->GetMaxY())
        {
            FlagBranchForUpdate(lpNode);
            return;
        }

        if (lpNode->GetMinY() > lfEntityY - lfRadius)
        {
            FlagBranchForUpdate(lpNode);
        }
    }

    // ===========================================================================
    // UpdateRecursive @ 0x828B12A0
    //
    // Re-derive one branch's loose bounds bottom-up: a node's [minY, maxY] band is the
    // union of its own entities' vertical extents and its children's bands (each padded
    // by KF_NODE_HEIGHT_OFFSET), its MaxEntityRadius is the largest radius it holds, and
    // its loose HalfSize / HalfHeight follow from those. Clears NEEDS_UPDATE on the way
    // back out.
    // ===========================================================================
    void LooseOctree::UpdateRecursive(u16 lu16NodeIndex)
    {
        LooseOctreeNode& lrNode = mpNodes[lu16NodeIndex];

        lrNode.SetMaxEntityRadius(0.0f);
        lrNode.SetMinY(KF_LOOSE_OCTREE_MAX_Y);
        lrNode.SetMaxY(KF_LOOSE_OCTREE_MIN_Y);
        mpNodesEntityInfo[lu16NodeIndex].mxSubTreeEntityFlags = 0;
        lrNode.mxNodeEntityFlags = 0;

        // This node's own entities. NOTE the +/- KF_NODE_HEIGHT_OFFSET is applied INSIDE
        // the loop, against the already-padded field, so the band grows by the pad once
        // per entity -- that is literally what the X360 emits (@0x828B149C / 0x828B14D8)
        // and it only ever widens the (conservative) loose box.
        u16 lu16Entity = lrNode.muHeadIndex;
        while (lu16Entity != KU_INVALID_NODE)
        {
            const SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16Entity);
            const CgsGeometric::Sphere& lrSphere = GetEntityBoundingSphereConst(lu16Entity);
            const f32 lfY = lrSphere.mPositionRadius.y;
            const f32 lfR = lrSphere.mPositionRadius.w;

            if (lfR > lrNode.GetMaxEntityRadius()) { lrNode.SetMaxEntityRadius(lfR); }

            const f32 lfTop = (lfY + lfR > lrNode.GetMaxY()) ? (lfY + lfR) : lrNode.GetMaxY();
            lrNode.SetMaxY(lfTop + KF_NODE_HEIGHT_OFFSET);
            const f32 lfBot = (lfY - lfR < lrNode.GetMinY()) ? (lfY - lfR) : lrNode.GetMinY();
            lrNode.SetMinY(lfBot - KF_NODE_HEIGHT_OFFSET);

            lrNode.mxNodeEntityFlags |= lrLink.mx32TypeFlags;
            lu16Entity = lrLink.mu16NextEntity;
        }

        mpNodesEntityInfo[lu16NodeIndex].mxSubTreeEntityFlags = lrNode.mxNodeEntityFlags;

        if (lrNode.muFirstChildIndex != KU_INVALID_NODE)
        {
            for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
            {
                const u16 lu16ChildIndex = static_cast<u16>(lrNode.muFirstChildIndex + luChild);
                LooseOctreeNode& lrChild = mpNodes[lu16ChildIndex];

                // Only a FLAGGED child is re-derived; its bounds are folded in either way.
                if ((lrChild.muFlags & KU_OCTREE_NODE_FLAG_NEEDS_UPDATE) != 0)
                {
                    UpdateRecursive(lu16ChildIndex);
                }

                if (lrChild.GetMaxEntityRadius() > lrNode.GetMaxEntityRadius())
                {
                    lrNode.SetMaxEntityRadius(lrChild.GetMaxEntityRadius());
                }
                if (lrChild.GetMinY() < lrNode.GetMinY()) { lrNode.SetMinY(lrChild.GetMinY()); }
                if (lrChild.GetMaxY() > lrNode.GetMaxY()) { lrNode.SetMaxY(lrChild.GetMaxY()); }

                mpNodesEntityInfo[lu16NodeIndex].mxSubTreeEntityFlags |=
                    mpNodesEntityInfo[lu16ChildIndex].mxSubTreeEntityFlags;
            }
        }

        // MaxY still at the seed == the whole sub-tree is empty (the X360's vcmpeqfp.
        // against flt_820F27C0 @0x828B1630).
        if (lrNode.GetMaxY() == KF_LOOSE_OCTREE_MIN_Y)
        {
            lrNode.SetHalfHeight(0.0f);
        }
        else
        {
            lrNode.SetHalfHeight((lrNode.GetMaxY() - lrNode.GetMinY()) * 0.5f);
            lrNode.mPosition.y = (lrNode.GetMaxY() + lrNode.GetMinY()) * 0.5f;
        }

        // The LOOSE horizontal half-extent: an entity is filed by its CENTRE (the XZ
        // containment test uses the strict half base size), so the box has to grow by the
        // largest radius the sub-tree holds for it to contain every sphere.
        lrNode.mParams0.w = lrNode.GetHalfBaseSize() + lrNode.GetMaxEntityRadius();

        lrNode.mHalfDimensions.x = lrNode.mParams0.w;
        lrNode.mHalfDimensions.y = lrNode.mParams1.z;
        lrNode.mHalfDimensions.z = lrNode.mParams0.w;
        lrNode.mHalfDimensions.w = 0.0f;

        lrNode.muFlags &= ~KU_OCTREE_NODE_FLAG_NEEDS_UPDATE;
    }

    // ---------------------------------------------------------------------------
    // IndexedLinkList<SpatialPartitionEntity,u16> AddTail / Remove, hoisted out of the
    // three X360 sites that inline them (AddEntityInternal @0x828BB828 / @0x828BBA00,
    // SetEntityRadius @0x828BCD3C / @0x828BCBF8, and the split/merge redistribution).
    // ---------------------------------------------------------------------------
    void LooseOctree::LinkEntityToNode(LooseOctreeNode* lpNode, u16 lu16EntityIndex)
    {
        SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16EntityIndex);
        lrLink.mu16NextEntity = KU_INVALID_NODE;

        if (lpNode->muNumElements == 0)
        {
            lrLink.mu16PrevEntity = KU_INVALID_NODE;
            lpNode->muHeadIndex   = lu16EntityIndex;
            lpNode->muTailIndex   = lu16EntityIndex;
        }
        else
        {
            CGS_ASSERT(lpNode->muNumElements > 0,
                       "To add node to a zero length list call InternalAddFirstNode\n");
            lrLink.mu16PrevEntity = lpNode->muTailIndex;
            GetEntityLink(lpNode->muTailIndex).mu16NextEntity = lu16EntityIndex;
            lpNode->muTailIndex = lu16EntityIndex;
        }
        ++lpNode->muNumElements;
    }

    void LooseOctree::UnlinkEntityFromNode(LooseOctreeNode* lpNode, u16 lu16EntityIndex)
    {
        SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16EntityIndex);

        if (lpNode->muNumElements > 1)
        {
            if (lu16EntityIndex == lpNode->muHeadIndex)
            {
                const u16 lu16Next = lrLink.mu16NextEntity;
                lpNode->muHeadIndex = lu16Next;
                GetEntityLink(lu16Next).mu16PrevEntity = KU_INVALID_NODE;
            }
            else if (lu16EntityIndex == lpNode->muTailIndex)
            {
                const u16 lu16Prev = lrLink.mu16PrevEntity;
                lpNode->muTailIndex = lu16Prev;
                GetEntityLink(lu16Prev).mu16NextEntity = KU_INVALID_NODE;
            }
            else
            {
                GetEntityLink(lrLink.mu16PrevEntity).mu16NextEntity = lrLink.mu16NextEntity;
                GetEntityLink(lrLink.mu16NextEntity).mu16PrevEntity = lrLink.mu16PrevEntity;
            }
            --lpNode->muNumElements;
        }
        else
        {
            CGS_ASSERT(lpNode->muNumElements == 1,
                       "This should only be called to add a node to list when it is zero length\n");
            lpNode->muNumElements = 0;
            lpNode->muHeadIndex   = KU_INVALID_NODE;
            lpNode->muTailIndex   = KU_INVALID_NODE;
        }
        // (The removed element's own next/prev are deliberately left alone, as on the console.)
    }

    // ===========================================================================
    // AddEntityToGraph / AddEntityInternal @ 0x828BB648
    //
    // Descend from the root while the entity's sphere still fits the child it falls in
    // (CalcNextSubNode picks the child; EntityInsideNodeBounds gates the step, and the
    // descent also stops once the entity's DIAMETER exceeds the child's
    // MaxRadiusThreshold -- a large object is filed higher up so the loose bounds of the
    // small cells stay tight), then link it onto that node's intrusive entity list and
    // flag the branch for the bounds update.
    // ===========================================================================
    void LooseOctree::AddEntityToGraph(u16 lu16Id)
    {
        AddEntityInternal(lu16Id);
    }

    void LooseOctree::AddEntityInternal(u16 lu16EntityIndex)
    {
        CGS_ASSERT(lu16EntityIndex < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
        if (mpNodes == 0 || lu16EntityIndex >= KI_MAX_NUM_ENTITIES)
        {
            return;
        }

        const CgsGeometric::Sphere& lrSphere = GetEntityBoundingSphere(lu16EntityIndex);
        const f32 lfDiameter = lrSphere.mPositionRadius.w * 2.0f;

        // Descend from the root. Every node on the path takes the NEEDS_UPDATE flag and a
        // sub-tree count bump ON THE WAY DOWN (which is why this never needs
        // FlagBranchForUpdate); the descent stops as soon as the entity's DIAMETER
        // outgrows the node's admissible-size threshold (mParams0.z), or at a leaf.
        u16 lu16NodeIndex = 0;
        mpNodes[0].muFlags |= KU_OCTREE_NODE_FLAG_NEEDS_UPDATE;
        ++mpNodes[0].muSubTreeEntityCount;

        while (!(lfDiameter > mpNodes[lu16NodeIndex].GetMaxRadiusThreshold()) &&
               mpNodes[lu16NodeIndex].muFirstChildIndex != KU_INVALID_NODE)
        {
            const LooseOctreeNode& lrNode = mpNodes[lu16NodeIndex];
            lu16NodeIndex = static_cast<u16>(
                lrNode.muFirstChildIndex + CalcNextSubNode(&lrNode, lrSphere.mPositionRadius));

            mpNodes[lu16NodeIndex].muFlags |= KU_OCTREE_NODE_FLAG_NEEDS_UPDATE;
            ++mpNodes[lu16NodeIndex].muSubTreeEntityCount;
        }

        LinkEntityToNode(&mpNodes[lu16NodeIndex], lu16EntityIndex);
        maEntityNodeIndex[lu16EntityIndex] = lu16NodeIndex;
    }

    // ===========================================================================
    // RemoveEntityFromGraph / RemoveEntityInternal @ 0x828C9818 / 0x828BB948
    // ===========================================================================
    void LooseOctree::RemoveEntityFromGraph(u16 lu16Id)
    {
        RemoveEntityInternal(lu16Id);
    }

    void LooseOctree::RemoveEntityInternal(u16 lu16EntityIndex)
    {
        CGS_ASSERT(lu16EntityIndex < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
        if (mpNodes == 0 || lu16EntityIndex >= KI_MAX_NUM_ENTITIES)
        {
            return;
        }

        const u16 lu16NodeIndex = maEntityNodeIndex[lu16EntityIndex];
        if (lu16NodeIndex == KU_INVALID_NODE)
        {
            return;   // not in the graph (already removed, or never added)
        }

        UnlinkEntityFromNode(&mpNodes[lu16NodeIndex], lu16EntityIndex);
        maEntityNodeIndex[lu16EntityIndex] = KU_INVALID_NODE;

        // Walk UP flagging + decrementing (the mirror of the add descent; this is
        // FlagBranchForUpdate with the extra sub-tree count step).
        u16 lu16Walk = lu16NodeIndex;
        for (;;)
        {
            LooseOctreeNode& lrWalk = mpNodes[lu16Walk];
            lrWalk.muFlags |= KU_OCTREE_NODE_FLAG_NEEDS_UPDATE;
            if (lrWalk.muSubTreeEntityCount > 0)
            {
                --lrWalk.muSubTreeEntityCount;
            }
            if (lrWalk.muParentIndex == KU_INVALID_NODE)
            {
                break;
            }
            lu16Walk = lrWalk.muParentIndex;
        }
    }

    // ===========================================================================
    // SetEntityPosition @ 0x828C9820 / SetEntityRadius @ 0x828BC740
    //
    // Update the bounding sphere, then re-file the entity when it no longer fits the
    // node it is in (the X360 re-runs EntityInsideNodeBounds and, when it fails, pulls
    // the entity out and re-descends); otherwise just widen the branch's Y band.
    // ===========================================================================
    void LooseOctree::SetEntityPosition(u16 lu16Id, Vector3 lPosition)
    {
        CGS_ASSERT(lu16Id < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
        if (mpNodes == 0 || lu16Id >= KI_MAX_NUM_ENTITIES)
        {
            return;
        }

        const u16 lu16NodeIndex = maEntityNodeIndex[lu16Id];

        // Keep the radius (the X360 vrlimi128 copies lane w of the old sphere).
        CgsGeometric::Sphere& lrSphere = GetEntityBoundingSphere(lu16Id);
        lrSphere.mPositionRadius.x = lPosition.x;
        lrSphere.mPositionRadius.y = lPosition.y;
        lrSphere.mPositionRadius.z = lPosition.z;

        if (lu16NodeIndex == KU_INVALID_NODE)
        {
            return;   // not in the graph
        }

        LooseOctreeNode& lrNode = mpNodes[lu16NodeIndex];
        if (!EntityInsideNodeBounds(lu16Id, &lrNode))
        {
            RemoveEntityInternal(lu16Id);
            AddEntityInternal(lu16Id);
            return;
        }

        UpdateNodeYBounds(&lrNode, lu16Id);
    }

    // ===========================================================================
    // SetEntityRadius @ 0x828BC740
    //
    // The resting invariant AddEntityInternal establishes is
    // `node.MaxRadiusThreshold * 0.5 < radius <= node.MaxRadiusThreshold` (a node admits
    // entities whose DIAMETER fits its threshold, and its children admit half that). The
    // body tests exactly that band: inside it the entity stays put and only the bounds
    // need refreshing; outside it the entity walks UP (grown) or DOWN (shrunk) to the
    // node whose band it now belongs in, and is re-filed there.
    // ===========================================================================
    void LooseOctree::SetEntityRadius(u16 lu16Id, f32 lfRadius)
    {
        CGS_ASSERT(lu16Id < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");
        if (mpNodes == 0 || lu16Id >= KI_MAX_NUM_ENTITIES)
        {
            return;
        }

        const u16 lu16OldNodeIndex = maEntityNodeIndex[lu16Id];
        if (lu16OldNodeIndex == KU_INVALID_NODE)
        {
            GetEntityBoundingSphere(lu16Id).mPositionRadius.w = lfRadius;
            return;   // not in the graph
        }

        CgsGeometric::Sphere& lrSphere = GetEntityBoundingSphere(lu16Id);
        const f32 lfOldRadius = lrSphere.mPositionRadius.w;
        LooseOctreeNode* lpOldNode = &mpNodes[lu16OldNodeIndex];

        // Still inside this node's admissible band -> stay put.
        if (!(lfRadius > lpOldNode->GetMaxRadiusThreshold()) &&
            !(lpOldNode->GetMaxRadiusThreshold() * 0.5f >= lfRadius))
        {
            lrSphere.mPositionRadius.w = lfRadius;
            if (lfRadius > lpOldNode->GetMaxEntityRadius())
            {
                FlagBranchForUpdate(lpOldNode);
            }
            else
            {
                UpdateNodeYBounds(lpOldNode, lu16Id);
            }
            return;
        }

        LooseOctreeNode* lpNewNode = lpOldNode;
        if (lfRadius > lfOldRadius)
        {
            // GROW: climb until the node admits it, dropping the sub-tree count of every
            // node LEFT behind.
            while (lfRadius > lpNewNode->GetMaxRadiusThreshold())
            {
                if (lpNewNode == mpRootNode)
                {
                    break;
                }
                if (lpNewNode->muSubTreeEntityCount > 0) { --lpNewNode->muSubTreeEntityCount; }
                CGS_ASSERT(lpNewNode->muParentIndex != KU_INVALID_NODE, "Node has no parent\n");
                if (lpNewNode->muParentIndex == KU_INVALID_NODE) { break; }
                lpNewNode = &mpNodes[lpNewNode->muParentIndex];
            }
            FlagBranchForUpdate(lpOldNode);
        }
        else
        {
            // SHRINK: descend while the child band still fits, bumping the count of every
            // node ENTERED.
            while (lpNewNode->GetMaxRadiusThreshold() * 0.5f >= lfRadius)
            {
                if (lpNewNode->muFirstChildIndex == KU_INVALID_NODE)
                {
                    break;
                }
                lpNewNode = &mpNodes[lpNewNode->muFirstChildIndex +
                                     CalcNextSubNode(lpNewNode, lrSphere.mPositionRadius)];
                ++lpNewNode->muSubTreeEntityCount;
            }
            FlagBranchForUpdate(lpNewNode);
        }

        if (lpNewNode != lpOldNode)
        {
            UnlinkEntityFromNode(lpOldNode, lu16Id);
            LinkEntityToNode(lpNewNode, lu16Id);
            CGS_ASSERT(lpNewNode != 0, "lpNode");
            maEntityNodeIndex[lu16Id] = GetNodeIndex(lpNewNode);
        }

        lrSphere.mPositionRadius.w = lfRadius;
    }

    // ===========================================================================
    // Update @ 0x828D0180 -- per-frame maintenance: only when the root wants an update,
    // re-derive the loose bounds from the root down. (The X360 runs the two
    // adaptive-depth passes first; see the TU banner for why their absence cannot change
    // a query's result set.)
    // ===========================================================================
    void LooseOctree::Update()
    {
        if (mpRootNode == 0)
        {
            return;
        }
        if ((mpRootNode->muFlags & KU_OCTREE_NODE_FLAG_NEEDS_UPDATE) != 0)
        {
            UpdateRecursive(0);
        }

        // [DIAG culling wave]
        {
            static s32 siDiag = 0;
            if ((siDiag++ % 240) == 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[culling-diag] octree root subtree=" << static_cast<s32>(mpRootNode->muSubTreeEntityCount)
                    << " own=" << static_cast<s32>(mpRootNode->muNumElements)
                    << " mask=" << static_cast<s32>(mpNodesEntityInfo[0].mxSubTreeEntityFlags)
                    << " halfBase=" << mpRootNode->GetHalfBaseSize()
                    << " halfSize=" << mpRootNode->GetHalfSize()
                    << " minY=" << mpRootNode->GetMinY() << " maxY=" << mpRootNode->GetMaxY()
                    << " child0mask=" << static_cast<s32>(
                           mpRootNode->muFirstChildIndex == KU_INVALID_NODE
                               ? 0u : mpNodesEntityInfo[mpRootNode->muFirstChildIndex].mxSubTreeEntityFlags)
                    << " nodes=" << static_cast<s32>(muNumNodes)
                    << " staticGroups=" << miNumStaticNodes << "\n";
            }
        }
    }

    // ===========================================================================
    // AddJobFrustumTest @ 0x828AA958
    //
    // Stage one query into job luJobIndex's data block: the 128-byte swizzled frustum
    // into maFrustums[q], the 64-byte view-projection into maViewProjections[q], the
    // entity-type mask into max32EntityTypeMasks[q]; bump muNumQueries and raise the
    // job's REQUESTED flag.
    // ===========================================================================
    void LooseOctree::AddJobFrustumTest(u32 lx32EntityTypeMask,
                                        const CgsGeometric::Frustum* lpFrustum,
                                        const Matrix44* lpViewProjection,
                                        u32 luJobIndex)
    {
        CGS_ASSERT(luJobIndex < KU_NUM_FRUSTUM_TEST_JOBS, "luJobIndex < KU_NUM_FRUSTUM_TEST_JOBS");
        if (luJobIndex >= KU_NUM_FRUSTUM_TEST_JOBS)
        {
            return;
        }

        FrustumJobQueryInfo& lrQueryInfo = maFrustumTestJobData[luJobIndex].mQueryInfo;
        const u32 luQueryIndex = lrQueryInfo.muNumQueries;

        CGS_ASSERT(luQueryIndex < KU_MAX_QUERIES_PER_JOB,
                   "luQueryIndex < KU_JOB_BUFFER_MAX_NUM_QUERIES");
        if (luQueryIndex >= KU_MAX_QUERIES_PER_JOB)
        {
            return;
        }

        lrQueryInfo.maFrustums[luQueryIndex]            = *lpFrustum;
        lrQueryInfo.maViewProjections[luQueryIndex]     = *lpViewProjection;
        lrQueryInfo.max32EntityTypeMasks[luQueryIndex]  = lx32EntityTypeMask;
        ++lrQueryInfo.muNumQueries;

        mabFrustumJobRequested[luJobIndex] = true;
    }

    // ===========================================================================
    // StartFrustumTestJobs @ 0x828B23E0
    //
    // For every job with queries pending: clear its result buffer, fill the job data
    // header (the pointers/counters the job walks the tree with), drop the REQUESTED
    // flag, raise ACTIVE and dispatch.
    //
    // FLAG PC-platform leaf: the JobScheduler::AddJobs call is replaced by running the
    // job's queries inline (see the TU banner). Everything else -- the buffer reset,
    // the header, the flag handshake and the per-query result runs -- is the console's.
    // ===========================================================================
    void LooseOctree::StartFrustumTestJobs()
    {
        for (u32 luJob = 0; luJob < KU_NUM_FRUSTUM_TEST_JOBS; ++luJob)
        {
            if (!mabFrustumJobRequested[luJob])
            {
                continue;
            }

            // JobCoarseResultBuffer::Clear (inlined @0x828B2464..0x828B2488) -- the
            // mpu16Buffer pointer is deliberately NOT touched.
            JobCoarseResultBuffer& lrBuffer = maJobResultBuffers[luJob];
            lrBuffer.muNumQueries = 0;
            for (u32 luQuery = 0; luQuery < KU_JOB_BUFFER_MAX_NUM_QUERIES; ++luQuery)
            {
                lrBuffer.maQueryOffsets[luQuery]    = 0;
                lrBuffer.maQueryNumResults[luQuery] = 0;
            }
            lrBuffer.muCurrentWriteOffset = 0;

            FrustumTestJobData& lrJobData = maFrustumTestJobData[luJob];
            lrJobData.mpNodes         = mpRootNode;
            lrJobData.mpNodeTypeMasks = mpNodesEntityInfo;
            lrJobData.mpEntityLinks   = &GetEntityLink(0);
            lrJobData.mpEntitySpheres = &GetEntityBoundingSphere(0);
            lrJobData.muNumNodes      = muNumNodeGroups << 2;
            lrJobData.muMaxEntities   = static_cast<u32>(KI_MAX_NUM_ENTITIES);
            lrJobData.mpResultBuffer  = &lrBuffer;
            lrJobData.muMaxResults    = KU_JOB_RESULT_BUFFER_SIZE;

            mabFrustumJobRequested[luJob] = false;
            mabFrustumJobActive[luJob]    = true;

            // ---- FLAG PC-platform leaf: run the job body here ----
            // FrustumTestJob::Execute @0x82BE0158: per query, record the run's start
            // offset, walk the tree, then publish the run length.
            const FrustumJobQueryInfo& lrQueryInfo = lrJobData.mQueryInfo;
            for (u32 luQuery = 0; luQuery < lrQueryInfo.muNumQueries; ++luQuery)
            {
                lrBuffer.maQueryOffsets[luQuery]    = lrBuffer.muCurrentWriteOffset;
                lrBuffer.maQueryNumResults[luQuery] = 0;
                lrBuffer.muNumQueries               = luQuery;

                FrustumTestParams lParams;
                lParams.mpFrustum          = &lrQueryInfo.maFrustums[luQuery];
                lParams.mx32EntityTypeMask = lrQueryInfo.max32EntityTypeMasks[luQuery];
                lParams.mpResultBuffer     = 0;          // the job writes its OWN run pool
                lParams.mpJobResultBuffer  = &lrBuffer;
                lParams.muNumNodesVisited  = 0;
                lParams.CachePlanes();

                // [DIAG culling wave] BRN_CULL_OFF=1 accepts every type-matching entity
                // without the frustum test, so the SAME build / SAME resident set can be
                // measured with culling on and off. Read once; delete with the wave.
                static s32 siCullOff = -1;
                if (siCullOff < 0)
                {
                    const char* lpcEnv = std::getenv("BRN_CULL_OFF");
                    siCullOff = (lpcEnv != 0 && lpcEnv[0] == '1') ? 1 : 0;
                }

                if (siCullOff != 0)
                {
                    TrivialAcceptRecursive(0, &lParams);
                }
                else
                {
                    FrustumTestVpRecursive(0, &lParams);
                }

                ++lrBuffer.muNumQueries;
            }
        }
    }

    // ===========================================================================
    // WaitForFrustumTestJobResults @ 0x828B2558
    //
    // Block on each ACTIVE job, then drain every per-query result run out of that job's
    // buffer into the shared output CoarseQueryResultBuffer -- one query == one batch,
    // in query order. Clears the active flag and the job's query count afterwards.
    // ===========================================================================
    void LooseOctree::WaitForFrustumTestJobResults(CoarseQueryResultBuffer<16384>* lpResultBufferOut)
    {
        for (u32 luJob = 0; luJob < KU_NUM_FRUSTUM_TEST_JOBS; ++luJob)
        {
            if (!mabFrustumJobActive[luJob])
            {
                continue;
            }

            // (FLAG PC-platform leaf: the console blocks on maFrustumTestJobs[luJob]
            //  here; the work already ran inline in StartFrustumTestJobs.)

            FrustumTestJobData&    lrJobData = maFrustumTestJobData[luJob];
            JobCoarseResultBuffer& lrBuffer  = maJobResultBuffers[luJob];

            const u32 luNumQueries = lrJobData.mQueryInfo.muNumQueries;
            for (u32 luQuery = 0; luQuery < luNumQueries; ++luQuery)
            {
                lpResultBufferOut->BeginResultsBatch();
                lpResultBufferOut->PushResults(
                    lrBuffer.mpu16Buffer + lrBuffer.maQueryOffsets[luQuery],
                    lrBuffer.maQueryNumResults[luQuery]);

                CGS_ASSERT(lpResultBufferOut->GetNumResultsAttempted() ==
                           lpResultBufferOut->GetNumResultsWritten(),
                           "lpResultBufferOut->GetNumResultsAttempted() == lpResultBufferOut->GetNumResultsWritten()");

                lpResultBufferOut->EndResultsBatch();
            }

            mabFrustumJobActive[luJob]                = false;
            lrJobData.mQueryInfo.muNumQueries         = 0;
        }
    }

    // ===========================================================================
    // FrustumTestVp @ 0x828CA7F8 -- the synchronous view-projection frustum query
    // (the non-job entry point; same traversal, results straight into the caller's
    // CoarseQueryResultBuffer).
    // ===========================================================================
    bool LooseOctree::FrustumTestVp(u32 lx32EntityTypeMask,
                                    const CgsGeometric::Frustum& lrFrustum,
                                    const Matrix44& /*lrViewProjection*/,
                                    CoarseQueryResultBuffer<16384>* lpResultBuffer)
    {
        FrustumTestParams lParams;
        lParams.mpFrustum          = &lrFrustum;
        lParams.mx32EntityTypeMask = lx32EntityTypeMask;
        lParams.mpResultBuffer     = lpResultBuffer;
        lParams.mpJobResultBuffer  = 0;
        lParams.muNumNodesVisited  = 0;
        lParams.CachePlanes();

        FrustumTestVpRecursive(0, &lParams);

        return lpResultBuffer->GetNumResultsAttempted() > 0;
    }

    // ===========================================================================
    // FrustumTestVpRecursive @ 0x828BDC38 (== FrustumTestRecursive @0x828CA9D0's shape)
    //
    //   ++muNumNodesVisited
    //   if (node->muSubTreeEntityCount >= 4 || node has children)
    //       e = NodeInsideFrustum(node)
    //       if (e == 1) { TrivialAcceptRecursive(node); return; }   // fully inside
    //       if (e != 2) return;                                      // fully outside
    //   test this node's own entities
    //   for each of the FOUR children whose sub-tree type mask intersects the query
    //       recurse
    // (The node test is skipped for a tiny leaf: testing the box costs more than
    // testing its three-or-fewer spheres.)
    // ===========================================================================
    void LooseOctree::FrustumTestVpRecursive(u16 lu16NodeIndex, FrustumTestParams* lpParams)
    {
        ++lpParams->muNumNodesVisited;

        LooseOctreeNode& lrNode = mpNodes[lu16NodeIndex];

        if (lrNode.muSubTreeEntityCount >= 4 || lrNode.muFirstChildIndex != KU_INVALID_NODE)
        {
            const u32 luInside = NodeInsideFrustum(&lrNode, *lpParams->mpFrustum);
            if (luInside == 1)
            {
                TrivialAcceptRecursive(lu16NodeIndex, lpParams);
                return;
            }
            if (luInside != 2)
            {
                return;
            }
        }

        if (lrNode.muNumElements > 0)
        {
            FrustumTestEntities(lrNode.muHeadIndex, lpParams);
        }

        if (lrNode.muFirstChildIndex != KU_INVALID_NODE)
        {
            for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
            {
                const u16 lu16ChildIndex = static_cast<u16>(lrNode.muFirstChildIndex + luChild);
                if ((lpParams->mx32EntityTypeMask &
                     mpNodesEntityInfo[lu16ChildIndex].mxSubTreeEntityFlags) != 0)
                {
                    FrustumTestVpRecursive(lu16ChildIndex, lpParams);
                }
            }
        }
    }

    // ===========================================================================
    // TrivialAcceptRecursive @ 0x828B1B50 -- the same walk with no frustum test: the
    // node's loose bounds are entirely inside the frustum, so every entity it holds is
    // too. Gated per child on the sub-tree type mask exactly as the tested walk is.
    // ===========================================================================
    void LooseOctree::TrivialAcceptRecursive(u16 lu16NodeIndex, FrustumTestParams* lpParams)
    {
        LooseOctreeNode& lrNode = mpNodes[lu16NodeIndex];

        u16 lu16Entity = lrNode.muHeadIndex;
        while (lu16Entity != KU_INVALID_NODE)
        {
            const SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16Entity);
            if ((lrLink.mx32TypeFlags & lpParams->mx32EntityTypeMask) != 0)
            {
                PushCoarseResult(lpParams, lu16Entity);
            }
            lu16Entity = lrLink.mu16NextEntity;
        }

        if (lrNode.muFirstChildIndex != KU_INVALID_NODE)
        {
            for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
            {
                const u16 lu16ChildIndex = static_cast<u16>(lrNode.muFirstChildIndex + luChild);
                if ((lpParams->mx32EntityTypeMask &
                     mpNodesEntityInfo[lu16ChildIndex].mxSubTreeEntityFlags) != 0)
                {
                    TrivialAcceptRecursive(lu16ChildIndex, lpParams);
                }
            }
        }
    }

    // ===========================================================================
    // FrustumTestEntities @ 0x828B1CA0
    //
    // The leaf test, reproduced from the asm exactly. Per entity in the node's chain:
    //   * skip unless (maEntityLinks[i].mx32TypeFlags & queryMask) -- the FIRST word of
    //     the 8-byte link, read at `this + 8*(i+16)`;
    //   * load the bounding sphere at `this + 16*(i+5008)` and run the SoA plane batch
    //         dA = P0*cx + P1*cy + P2*cz - P3      (lanes = frustum planes 0..3)
    //         dB = P4*cx + P5*cy + P6*cz - P7      (lanes = frustum planes 4..7)
    //     rejecting when ANY lane of either exceeds the radius (vcmpgtfp / vor / vsel /
    //     vcmpeqfp. against zero -- accept only when every lane is inside); that is
    //     exactly CgsGeometric::Frustum::IsSphereInFrustum @0x828AF020, instruction for
    //     instruction, so the committed method is used;
    //   * PushResult(index) on accept.
    // ===========================================================================
    void LooseOctree::FrustumTestEntities(u16 lu16FirstEntity, FrustumTestParams* lpParams)
    {
        u16 lu16Entity = lu16FirstEntity;
        while (lu16Entity != KU_INVALID_NODE)
        {
            CGS_ASSERT(lu16Entity < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");

            const SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16Entity);
            if ((lrLink.mx32TypeFlags & lpParams->mx32EntityTypeMask) != 0)
            {
                const Vector4& lrSphere =
                    GetEntityBoundingSphereConst(lu16Entity).mPositionRadius;
                const f32 lfCx = lrSphere.x, lfCy = lrSphere.y;
                const f32 lfCz = lrSphere.z, lfR  = lrSphere.w;

                bool lbInside = true;
                for (u32 luPlane = 0; luPlane < 8; ++luPlane)
                {
                    if (lpParams->maNx[luPlane] * lfCx + lpParams->maNy[luPlane] * lfCy
                        + lpParams->maNz[luPlane] * lfCz - lpParams->maD[luPlane] > lfR)
                    {
                        lbInside = false;
                        break;
                    }
                }

                if (lbInside)
                {
                    PushCoarseResult(lpParams, lu16Entity);
                }
            }

            lu16Entity = lrLink.mu16NextEntity;
        }
    }

    // ===========================================================================
    // PushCoarseResult -- publish one accepted entity index.
    //
    // The JOB path writes its own run pool exactly as FrustumTestJob::TestEntitiesBulk
    // @0x82BDFF70 does: `mpu16Buffer[muCurrentWriteOffset] = index;
    // ++maQueryNumResults[muNumQueries]; ++muCurrentWriteOffset` (both counters in u16
    // ELEMENTS), capped at KU_JOB_RESULT_BUFFER_SIZE. The SYNCHRONOUS path goes straight
    // into the shared CoarseQueryResultBuffer through PushResult.
    // ===========================================================================
    void LooseOctree::PushCoarseResult(FrustumTestParams* lpParams, u16 lu16EntityIndex)
    {
        if (lpParams->mpJobResultBuffer != 0)
        {
            JobCoarseResultBuffer& lrBuffer = *lpParams->mpJobResultBuffer;
            if (lrBuffer.muCurrentWriteOffset < KU_JOB_RESULT_BUFFER_SIZE)
            {
                lrBuffer.mpu16Buffer[lrBuffer.muCurrentWriteOffset] = lu16EntityIndex;
                ++lrBuffer.maQueryNumResults[lrBuffer.muNumQueries];
                ++lrBuffer.muCurrentWriteOffset;
            }
            return;
        }

        if (lpParams->mpResultBuffer != 0)
        {
            lpParams->mpResultBuffer->PushResult(lu16EntityIndex);
        }
    }

    // ===========================================================================
    // NodeInsideFrustum @ 0x828BDAC0
    //
    // Classify the node's loose box against the frustum: 1 = fully inside, 2 =
    // intersecting, 0 = fully outside. The box is mPosition +/- { HalfSize, HalfHeight,
    // HalfSize } (CalcNodeCorners); a plane rejects the whole box when its farthest
    // corner is still outside, and the box is fully inside when every plane's NEAREST
    // corner is inside.
    // ===========================================================================
    u32 LooseOctree::NodeInsideFrustum(const LooseOctreeNode* lpNode,
                                       const CgsGeometric::Frustum& lrFrustum) const
    {
        const f32 lfCx = lpNode->mPosition.x;
        const f32 lfCy = lpNode->mPosition.y;
        const f32 lfCz = lpNode->mPosition.z;
        const f32 lfEx = lpNode->GetHalfSize();
        const f32 lfEy = lpNode->GetHalfHeight();
        const f32 lfEz = lpNode->GetHalfSize();

        bool lbFullyInside = true;

        for (u32 luBatch = 0; luBatch < 2; ++luBatch)
        {
            const Vector4& lrNx = lrFrustum.maSwizzledPlanes[luBatch * 4 + 0];
            const Vector4& lrNy = lrFrustum.maSwizzledPlanes[luBatch * 4 + 1];
            const Vector4& lrNz = lrFrustum.maSwizzledPlanes[luBatch * 4 + 2];
            const Vector4& lrD  = lrFrustum.maSwizzledPlanes[luBatch * 4 + 3];

            const f32 laNx[4] = { lrNx.x, lrNx.y, lrNx.z, lrNx.w };
            const f32 laNy[4] = { lrNy.x, lrNy.y, lrNy.z, lrNy.w };
            const f32 laNz[4] = { lrNz.x, lrNz.y, lrNz.z, lrNz.w };
            const f32 laD [4] = { lrD.x,  lrD.y,  lrD.z,  lrD.w  };

            for (u32 luLane = 0; luLane < 4; ++luLane)
            {
                const f32 lfCentre = laNx[luLane] * lfCx + laNy[luLane] * lfCy
                                   + laNz[luLane] * lfCz - laD[luLane];
                const f32 lfExtent = std::fabs(laNx[luLane]) * lfEx
                                   + std::fabs(laNy[luLane]) * lfEy
                                   + std::fabs(laNz[luLane]) * lfEz;

                if (lfCentre - lfExtent > 0.0f)
                {
                    return 0;   // the whole box is on the outside half-space of this plane
                }
                if (lfCentre + lfExtent > 0.0f)
                {
                    lbFullyInside = false;
                }
            }
        }

        return lbFullyInside ? 1u : 2u;
    }
}
