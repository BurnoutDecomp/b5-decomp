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
//   LooseOctree::SphereTest / SphereTestRecursive   (the coarse sphere query, 2026-09-11)
//   LooseOctree::LineTestOptimized              @ 0x828CA5F8  (2026-09-25, FX-FOLLOWUPS)
//   LooseOctree::LineTestRecursive              @ 0x828BCF50  (2026-09-25, FX-FOLLOWUPS)
//   LooseOctree::VolumeTest                     @ 0x828CA910  (2026-09-25, FX-FOLLOWUPS)
//   LooseOctree::VolumeTestRecursive            @ 0x828BDE28  (2026-09-25, FX-FOLLOWUPS)
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
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Geometric/Intersection/CgsLineTests.h"      // TestLineSphere4 / TestLineBoundingBoxAgainstAxisAlignedBox4
#include "vendor/renderware/collision/CollisionVolume.hpp"                    // rw::collision::Volume / BoxVolume / SphereVolume

#include <cmath>     // std::fabs
#include <cstdio>    // std::snprintf ([DIAG] BRN_OCTREE_LINE_DIAG)
#include <cstdlib>   // std::getenv ([DIAG] BRN_CULL_OFF / BRN_OCTREE_LINE_DIAG)
#include <cstring>   // std::memcpy

// includes folded in from the CgsLooseOctree_w*.cpp partfiles (2026-09-15)
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT

namespace CgsSceneManager
{
    // The "Octree SphereTest" CPU monitor SphereTest brackets its traversal with. The
    // tree's Construct registers none of the octree's monitors yet, so this stays -1 and
    // PerfMonCpu::Start/StopMonitor no-op on the invalid handle.
    s32 LooseOctree::_miSphereTestPerfMon = -1;

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
        , mpEntityVolume(0)
        , mpNodeVolume(0)
        , mpVolumeVolumeQuery(0)
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

        // 0x828C9E34..0x828C9F7C -- the volume walk's two frames start as identity rotations with an all-zero
        // translation row, w included (1.0 is flt_82001C98, 0.0 flt_82001CC0; the rows are staged on the stack
        // and stored with stvx128 at +0x00 / +0x10 / +0x20 / +0x30 of mNodeTransform (+0x8D8D0) and
        // mEntityTransform (+0x8D910)) -- exactly Matrix44Affine::SetIdentity's four rows.
        mNodeTransform.SetIdentity();
        mEntityTransform.SetIdentity();

        // 0x828C9F50..0x828C9FE4 -- the entity arm's VolumeVolumeQuery, built in place in
        // macVolumeVolumeQueryBuffer (+0x446C0) for 100 volumes / 100 results: the descriptor, its size
        // assert (:208, `li r5, 0xD0`), then Initialize with the buffer table {buffer, 0, 0, 0, 0}
        // (var_1E0); the returned handle is stored at +0x8D8C8 (`stwx r8, r30, r7`).
        // NOT X360: host GPInstance / VolRef widths -- the console compares against 0x49000
        // (0x828C9F8C); the host buffer is KU_OCTREE_VOLUME_VOLUME_QUERY_BUFFER_SIZE (0x49600).
        {
            u32 lauDescriptor[10];
            rw::collision::VolumeVolumeQuery::GetResourceDescriptor(lauDescriptor, KI_OCTREE_VOLUME_QUERY_NUM_VOLUMES,
                                                                    KI_OCTREE_VOLUME_QUERY_NUM_RESULTS);
            CGS_ASSERT(lauDescriptor[0] <= KU_OCTREE_VOLUME_VOLUME_QUERY_BUFFER_SIZE,
                       "VolumeVolumeQueryMem is too small");

            void* lapBuffer[5] = { macVolumeVolumeQueryBuffer, 0, 0, 0, 0 };
            mpVolumeVolumeQuery = static_cast<rw::collision::VolumeVolumeQuery*>(
                rw::collision::VolumeVolumeQuery::Initialize(lapBuffer, KI_OCTREE_VOLUME_QUERY_NUM_VOLUMES,
                                                             KI_OCTREE_VOLUME_QUERY_NUM_RESULTS));
        }

        // 0x828C9FE8..0x828CA09C -- the walk's two query volumes, built in place in their buffers with a
        // resource table {buffer, 0, 0, 0, 0}: a BoxVolume of half extents {1, 1, 1} (v1 = {flt_82001C98 x3,
        // 0}, table var_1C0 -> +0x8D6C0) stored at +0x8D8C4, then a SphereVolume of radius 1 (f1 =
        // flt_82001C98, table var_1A0 -> +0x8D7C0) stored at +0x8D8C0. VolumeTestRecursive rewrites the box's
        // half extents per node and the sphere's radius per entity.
        {
            rw::Resource lBoxResource = {};
            lBoxResource.m_baseResources[0] = macBoxVolumeBuffer;
            mpNodeVolume = rw::collision::BoxVolume::Initialize(lBoxResource, 1.0f, 1.0f, 1.0f);

            rw::Resource lSphereResource = {};
            lSphereResource.m_baseResources[0] = macSphereVolumeBuffer;
            mpEntityVolume = rw::collision::SphereVolume::Initialize(lSphereResource, 1.0f);
        }

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
    // halfHeight, halfSize)) is expressed relative to the line origin (params +0x00,
    // mLineStart); the per-axis entry/exit params are invDir * bbMin and invDir * bbMax
    // (invDir at params +0x30, mLineReciprocal). The segment [0,1] overlaps iff, on every
    // axis, max(t0,t1) >= 0 and min(t0,t1) <= 1 -- each axis on its own.
    bool LooseOctree::TestLineAgainstNodeBoundingBox(
        const LooseOctreeNode* lpNode,
        const SpatialPartition::LineTestRecursiveFuncParams* lpParams) const
    {
        const Vector3& lLineOrigin = lpParams->mLineStart;       // lvx128 v13, r0, r5    (+0x00)
        const Vector3& lInvDir     = lpParams->mLineReciprocal;  // lvx128 v11, r5, 0x30  (+0x30)

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
    // FrustumTestEntities -- the NARROWING frustum query.
    //
    // Handed an explicit run of entity indices (an earlier query's published results)
    // rather than a tree root, it re-runs only the per-entity accept test on them:
    // no node classification, no traversal, no recursion. Every survivor was already
    // in the run it was given, so the answer is always a subset of that run.
    //
    // Store for store this is the leaf of the tree walk with the chain link replaced
    // by an array index -- the same type-flag gate on the entity's link, the same
    // eight-lane SoA plane batch against the entity's bounding sphere, the same
    // PushResult on accept. The tree walk's node counter is NOT touched: nothing here
    // visits a node.
    // ===========================================================================
    void LooseOctree::FrustumTestEntities(const CgsGeometric::Frustum& lrFrustum,
                                          u32 lx32EntityTypeMask,
                                          const u16* lpu16Entities, s32 liNumEntities,
                                          CoarseQueryResultBuffer<16384>* lpResultBuffer)
    {
        if (liNumEntities == 0)
        {
            return;
        }

        // The console copies the eight swizzled planes into its frame once, before the
        // loop, and keeps them in registers for its whole length.
        FrustumTestParams lParams;
        lParams.mpFrustum          = &lrFrustum;
        lParams.mx32EntityTypeMask = lx32EntityTypeMask;
        lParams.mpResultBuffer     = lpResultBuffer;
        lParams.mpJobResultBuffer  = 0;
        lParams.muNumNodesVisited  = 0;
        lParams.CachePlanes();

        for (s32 liEntity = 0; liEntity < liNumEntities; ++liEntity)
        {
            const u16 lu16Entity = lpu16Entities[liEntity];

            CGS_ASSERT(lu16Entity < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");

            const SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16Entity);
            if ((lrLink.mx32TypeFlags & lx32EntityTypeMask) == 0)
            {
                continue;
            }

            CGS_ASSERT(lu16Entity < KI_MAX_NUM_ENTITIES, "lu16Index < KI_MAX_NUM_ENTITIES");

            const Vector4& lrSphere =
                GetEntityBoundingSphereConst(lu16Entity).mPositionRadius;
            const f32 lfCx = lrSphere.x, lfCy = lrSphere.y;
            const f32 lfCz = lrSphere.z, lfR  = lrSphere.w;

            bool lbInside = true;
            for (u32 luPlane = 0; luPlane < 8; ++luPlane)
            {
                if (lParams.maNx[luPlane] * lfCx + lParams.maNy[luPlane] * lfCy
                    + lParams.maNz[luPlane] * lfCz - lParams.maD[luPlane] > lfR)
                {
                    lbInside = false;
                    break;
                }
            }

            if (lbInside)
            {
                lpResultBuffer->PushResult(lu16Entity);
            }
        }
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
            TestNodeEntities(lrNode.muHeadIndex, lpParams);
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
    // TestNodeEntities -- the leaf accept loop the walkers share.
    //
    // Per entity in the node's chain:
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
    void LooseOctree::TestNodeEntities(u16 lu16FirstEntity, FrustumTestParams* lpParams)
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

    // ===========================================================================
    // SphereTest -- the coarse sphere query (slot 5).
    //
    // Stage the traversal parameters the recursion reads by pointer -- the centre, the
    // radius, the entity-type mask and the result buffer -- walk from the root bracketed by
    // the octree's own CPU monitor, and report whether anything was attempted.
    // ===========================================================================
    bool LooseOctree::SphereTest(u32 lx32EntityTypeFlags, Vector3 lCentre, f32 lfRadius,
                                 CoarseQueryResultBuffer<16384>* lpResultBufferOut)
    {
        SphereTestParams lParams;
        lParams.mCentre             = lCentre;
        lParams.mfRadius            = lfRadius;
        lParams.mx32EntityTypeFlags = lx32EntityTypeFlags;
        lParams.mpResultBuffer      = lpResultBufferOut;

        const s32 liMonitor = _miSphereTestPerfMon;
        CgsDev::PerfMonCpu::StartMonitor(liMonitor);
        SphereTestRecursive(0, &lParams);
        CgsDev::PerfMonCpu::StopMonitor(liMonitor);

        return lpResultBufferOut->GetNumResultsAttempted() > 0;
    }

    // ===========================================================================
    // SphereTestRecursive
    //
    //   reject unless the query sphere overlaps this node's loose box on ALL THREE axes:
    //       |nodeCentre[i] - queryCentre[i]| <= nodeHalfExtent[i] + radius
    //     with the half extents { HalfSize, HalfHeight, HalfSize } the walk rebuilds out of
    //     mParams0.w / mParams1.z rather than reading mHalfDimensions;
    //   test this node's own entity chain when the node's OR-of-type-masks intersects the
    //     query and the chain is non-empty -- an entity is accepted when the centres are no
    //     further apart than the two radii summed (compared squared, never rooted);
    //   recurse into each of the FOUR children whose sub-tree type mask intersects the query.
    // Unlike the frustum walk there is no trivial-accept arm: a sphere has no cheap
    // "node entirely inside" classification here.
    // ===========================================================================
    void LooseOctree::SphereTestRecursive(u16 lu16NodeIndex, SphereTestParams* lpParams)
    {
        const LooseOctreeNode& lrNode = mpNodes[lu16NodeIndex];

        const f32 lfHalfSize   = lrNode.GetHalfSize();
        const f32 lfHalfHeight = lrNode.GetHalfHeight();

        const f32 lfDx = lrNode.mPosition.x - lpParams->mCentre.x;
        const f32 lfDy = lrNode.mPosition.y - lpParams->mCentre.y;
        const f32 lfDz = lrNode.mPosition.z - lpParams->mCentre.z;

        if (!(lfHalfSize   + lpParams->mfRadius >= std::fabs(lfDx)) ||
            !(lfHalfHeight + lpParams->mfRadius >= std::fabs(lfDy)) ||
            !(lfHalfSize   + lpParams->mfRadius >= std::fabs(lfDz)))
        {
            return;
        }

        if ((lrNode.mxNodeEntityFlags & lpParams->mx32EntityTypeFlags) != 0 &&
            lrNode.muNumElements > 0)
        {
            u16 lu16Entity = lrNode.muHeadIndex;
            while (lu16Entity != KU_INVALID_NODE)
            {
                const SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16Entity);
                if ((lrLink.mx32TypeFlags & lpParams->mx32EntityTypeFlags) != 0)
                {
                    const Vector4& lrSphere =
                        GetEntityBoundingSphereConst(lu16Entity).mPositionRadius;

                    const f32 lfEx = lpParams->mCentre.x - lrSphere.x;
                    const f32 lfEy = lpParams->mCentre.y - lrSphere.y;
                    const f32 lfEz = lpParams->mCentre.z - lrSphere.z;

                    const f32 lfSumRadii = lrSphere.w + lpParams->mfRadius;

                    if (lfSumRadii * lfSumRadii >=
                        lfEx * lfEx + lfEy * lfEy + lfEz * lfEz)
                    {
                        lpParams->mpResultBuffer->PushResult(lu16Entity);
                    }
                }
                lu16Entity = lrLink.mu16NextEntity;
            }
        }

        if (lrNode.muFirstChildIndex != KU_INVALID_NODE)
        {
            for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
            {
                const u16 lu16ChildIndex = static_cast<u16>(lrNode.muFirstChildIndex + luChild);
                if ((mpNodesEntityInfo[lu16ChildIndex].mxSubTreeEntityFlags &
                     lpParams->mx32EntityTypeFlags) != 0)
                {
                    SphereTestRecursive(lu16ChildIndex, lpParams);
                }
            }
        }
    }
}

// ============================================================================
// FOLDED FROM CgsLooseOctree_wSQ1.cpp (wave SQ1) on 2026-09-15 by tools/work/fold_partfiles.py.
// The partfile's own header follows verbatim (its address annotations are the
// evidence trail); its bodies come after it.
// ============================================================================
// =============================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsLooseOctree_wSQ1.cpp
//
// LooseOctree's coarse LINE query entry (scene-query wave 1, 2026-09-02). Reconstructed from
// BURNOUT_X360_ARTIST.XEX:
//
//   LooseOctree::LineTest          @ 0x828D01F0   (32 insns)   -- REAL
//   LooseOctree::LineTestOptimized @ 0x828CA5F8   (128 insns)  -- REAL since 2026-09-25 (FX-FOLLOWUPS;
//                                                               it was a LOUD TRAP until then)
//
// Split out of CgsLooseOctree.cpp (the mounted frustum/update body) so the new slot lands as
// its own mount and the shared TU is not rewritten.
//
// LineTest @0x828D01F0, instruction for instruction:
//   0x828D01F0..0x828D0210  save v126/v127 (the two Vector3 VALUE parameters ride in v1/v2 and
//                           must survive the StartMonitor call), r3 = this, r4 = flags, r5 = out
//   0x828D0224  lwz r31, dword_82F33F20   -> _miVPLineTestPerfMon ("Octree VP LineTest")
//   0x828D0228  bl  PerfMonCpu::StartMonitor(r31)        (UNCONDITIONAL -- no `> -1` guard here,
//                                                          unlike the SceneManagerModule passes)
//   0x828D0238  bl  LooseOctree::LineTestOptimized(this, flags, out, v1 = start, v2 = end)
//   0x828D0244  bl  PerfMonCpu::StopMonitor(r31)
//   return the LineTestOptimized result (r30 -> r3).
// The tree's PerfMonCpu::StartMonitor/StopMonitor no-op on an invalid handle, so the
// unregistered (-1) monitor is inert exactly as an unregistered console monitor would be.
// =============================================================================


namespace CgsSceneManager
{
    // DWARF CgsLooseOctree.h:120 / X360 dword_82F33F20 -- not yet registered by the tree's
    // Construct (see the header note); -1 == invalid handle.
    s32 LooseOctree::_miVPLineTestPerfMon = -1;

    // @ 0x828D01F0
    bool LooseOctree::LineTest(u32 lx32EntityTypeFlags, Vector3 lLineStart, Vector3 lLineEnd,
                               CoarseQueryResultBuffer<16384>* lpResultBufferOut)
    {
        const s32 liMonitor = _miVPLineTestPerfMon;                       // lwz r31, dword_82F33F20
        CgsDev::PerfMonCpu::StartMonitor(liMonitor);
        const bool lbResult = LineTestOptimized(lx32EntityTypeFlags, lLineStart, lLineEnd,
                                                lpResultBufferOut);
        CgsDev::PerfMonCpu::StopMonitor(liMonitor);
        return lbResult;
    }

    namespace
    {
        // X360 flt_82002540 (read from the image: 0x38D1B717 == 1.0e-4f). LineTestOptimized's shortest
        // walked segment, and the radius of the sphere query that answers anything shorter.
        const f32 KF_LINE_TEST_MIN_LENGTH = 1.0e-4f;

        // X360 flt_820F5E68 (0x33D6BF95 == 1.0e-7f). A direction lane whose |d| is below this takes the
        // refined 1/eps as its reciprocal instead of 1/d.
        const f32 KF_LINE_RECIPROCAL_EPSILON = 1.0e-7f;

        // ROUNDING (ROUNDING_RULE.md, 2026-09-25 -- REVIEW-J on 335639ce): every operation below rounds as the
        // console's instruction at that site does, not as the plain C expression would.
        //   rule 1  `vmsum3fp128`          ONE rounding of the f64 sum of the exact f32 products (LineDot3).
        //   rule 3  `vmaddfp` / `vnmsubfp` ONE rounding each: std::fma, and LineVnmsub's -(a*c - b) negated
        //                                  after the rounding (an exact cancellation is -0, a NaN keeps its sign).
        //   rule 4  `vmulfp` / `vsubfp`    rounded separately, as written.
        //   rule 5  `vrsqrtefp` / `vrefp`  FLAG (model): the estimate is its correctly rounded value; the two
        //                                  refinement steps that follow then run as the console runs them.
        // Until this change the two chains below were written unfused (a*b + c, 1 - a*b) and the dot summed in
        // f32 left to right -- one rounding per operation, where the console rounds once per fused instruction.

        // `vmsum3fp128` (0x828CA674) -- rule 1. FLAG (model): vmsum = one rounding of the f64 sum (xenia
        // DOT_PRODUCT_3/4). xenia also turns an f64 sum that overflows f32 into a QNaN; not modelled here --
        // a segment would have to be ~1.8e19 long.
        inline f32 LineDot3(f32 lfX, f32 lfY, f32 lfZ)
        {
            return static_cast<f32>(static_cast<f64>(lfX) * lfX + static_cast<f64>(lfY) * lfY
                                  + static_cast<f64>(lfZ) * lfZ);
        }

        // `vnmsubfp vD, vA, vB, vC` (IDA's raw field order) -- rule 3: -(vA*vC - vB), rounded once, then negated
        // (EffectsModule.cpp's Vnmsub).
        inline f32 LineVnmsub(f32 lfA, f32 lfC, f32 lfB)
        {
            const f32 lfDifference = std::fma(lfA, lfC, -lfB);
            return (lfDifference != lfDifference) ? lfDifference : -lfDifference;
        }

        // `vrsqrtefp` + two Newton-Raphson steps y' = y + 0.5 * y * (1 - x * y * y) (0x828CA678..0x828CA69C):
        //   0x828CA680 / 0x828CA690  vmulfp128 y*y            rule 4
        //   0x828CA684 / 0x828CA694  vmulfp128 y*0.5          rule 4 (0.5 = vcfsx {1}, 1)
        //   0x828CA688 / 0x828CA698  vnmsubfp  1 - x*(y*y)    rule 3 (LineVnmsub(x, yy, 1))
        //   0x828CA68C / 0x828CA69C  vmaddfp   (0.5y)*e + y   rule 3 (std::fma)
        // The estimate: rule 5. A zero x refines to NaN, which the caller's vsel replaces.
        inline f32 NewtonRaphsonReciprocalSqrt2(f32 lfValue)
        {
            f32 lfEstimate = static_cast<f32>(1.0 / std::sqrt(static_cast<f64>(lfValue)));   // vrsqrtefp
            for (s32 liStep = 0; liStep < 2; ++liStep)
            {
                const f32 lfSquare = lfEstimate * lfEstimate;                     // vmulfp128
                const f32 lfHalf   = lfEstimate * 0.5f;                           // vmulfp128 (vcfsx 1, 1)
                const f32 lfError  = LineVnmsub(lfValue, lfSquare, 1.0f);         // vnmsubfp
                lfEstimate         = std::fma(lfHalf, lfError, lfEstimate);       // vmaddfp
            }
            return lfEstimate;
        }

        // `vrefp` + two Newton-Raphson steps x' = x * (1 - d * x) + x (0x828CA6CC..0x828CA754), the same chain for
        // the length (0x828CA714 / 0x828CA720, 0x828CA730 / 0x828CA738), the epsilon (0x828CA71C / 0x828CA72C,
        // 0x828CA734 / 0x828CA73C) and the direction lanes (0x828CA70C / 0x828CA724, 0x828CA744 / 0x828CA754):
        // `vnmsubfp` 1 - x*d, rule 3 (LineVnmsub(x, d, 1)); `vmaddfp` x*e + x, rule 3 (std::fma).
        // The estimate: rule 5.
        inline f32 NewtonRaphsonReciprocal2(f32 lfValue)
        {
            f32 lfEstimate = static_cast<f32>(1.0 / static_cast<f64>(lfValue));  // vrefp
            for (s32 liStep = 0; liStep < 2; ++liStep)
            {
                const f32 lfError = LineVnmsub(lfEstimate, lfValue, 1.0f);        // vnmsubfp
                lfEstimate        = std::fma(lfEstimate, lfError, lfEstimate);    // vmaddfp
            }
            return lfEstimate;
        }

        // [DIAG] NOT IN THE X360 BINARY (2026-09-25, crash parity FX-FOLLOWUPS). Opt-in
        // BRN_OCTREE_LINE_DIAG=1: one `[octree-line]` line per LineTestOptimized query, capped at 64 --
        // the flags, the segment, its length, the arm taken (sphere / gated / walked) and the results
        // attempted. It proves the walk is dispatched; with the variable unset the cost is one static
        // bool test.
        inline void NoteOctreeLineTest(u32 lx32EntityTypeFlags, const Vector3& lrStart, const Vector3& lrEnd,
                                       f32 lfLength, const char* lpcArm, s32 liAttempted)
        {
            static const bool sbEnabled = []() {
                const char* lpcValue = std::getenv("BRN_OCTREE_LINE_DIAG");
                return lpcValue != 0 && lpcValue[0] == '1';
            }();
            static u32 suLines = 0;
            if (!sbEnabled || suLines >= 64 || CgsDev::Log::gpDebugPrint == 0)
            {
                return;
            }
            ++suLines;
            char lacLine[256];
            std::snprintf(lacLine, sizeof(lacLine),
                          "[octree-line] #%u flags 0x%X (%.2f,%.2f,%.2f)->(%.2f,%.2f,%.2f) len %.4f arm %s attempted %d\n",
                          suLines, lx32EntityTypeFlags, lrStart.x, lrStart.y, lrStart.z, lrEnd.x, lrEnd.y, lrEnd.z,
                          lfLength, lpcArm, liAttempted);
            *CgsDev::Log::gpDebugPrint << lacLine;
        }

        // A VMX compare-result lane reads as set when it is not +0.0 -- the console's
        // `vspltw ; vcmpeqfp128. lane, 0.0` skip, which for an all-ones / zero mask is the bit test.
        inline bool Mask4LaneSet(const Vector4& lrMask, u32 luLane)
        {
            u32 luBits;
            std::memcpy(&luBits, &(&lrMask.x)[luLane], sizeof(u32));
            return luBits != 0u;
        }
    }

    // @ 0x828CA5F8 -- LineTestOptimized (DWARF CgsLooseOctree.h:328, 128 insns).
    //
    // Builds the walk's parameter block on the stack (sp+0x70, SpatialPartition::
    // LineTestRecursiveFuncParams) and runs the root:
    //   0x828CA640  +0x00 mLineStart = v1                0x828CA658  +0x10 mLineEnd = v2
    //   0x828CA65C  +0x50 flags = r4                     0x828CA660  +0x54 result buffer = r5
    //   0x828CA60C  d = end - start (vsubfp)
    //   0x828CA674  len2 = vmsum3fp128(d, d)             0x828CA678..0x828CA6A0  len = len2 * rsqrt(len2)
    //   0x828CA67C / 0x828CA6A4  len = 0 where len2 == 0 (vcmpeqfp 0 ; vsel)
    //   0x828CA6A8  +0x40 mfLineLength = len (every lane)
    //   0x828CA6AC  `vcmpgtfp. len, 1e-4` -- NOT all lanes greater: answer as SphereTest(flags, start,
    //               1e-4, buffer) through the vtable (+0x14, slot 5; f1 = flt_82002540 still) and return
    //               its answer (0x828CA7D0).
    //   0x828CA6CC..0x828CA74C  +0x20 mLineDirection = d * NR2(1/len)
    //   0x828CA6F0..0x828CA768  +0x30 mLineReciprocal, per lane: (d > -eps) & !(d >= eps) ? NR2(1/eps)
    //               : NR2(1/d) (vcmpgtfp against eps ^ 0x80000000, vnot(vcmpgefp), vsel), both x 1.0
    //   0x828CA75C  r4 = mpRootNode (+0x446A0); TestLineAgainstNodeBoundingBox(root, &params)
    //   0x828CA78C  on a hit, LineTestRecursive(0, &params)
    //   0x828CA794  return GetNumResultsAttempted() > 0
    // Every lane is computed, as the console does; the walk reads x/y/z (and the length splat).
    bool LooseOctree::LineTestOptimized(u32 lx32EntityTypeFlags, Vector3 lLineStart, Vector3 lLineEnd,
                                        CoarseQueryResultBuffer<16384>* lpResultBufferOut)
    {
        SpatialPartition::LineTestRecursiveFuncParams lParams;
        lParams.mLineStart          = lLineStart;
        lParams.mLineEnd            = lLineEnd;
        lParams.mx32EntityTypeFlags = lx32EntityTypeFlags;
        lParams.mpResultBufferOut   = lpResultBufferOut;

        const f32 lafDelta[4] = { lLineEnd.x - lLineStart.x, lLineEnd.y - lLineStart.y,
                                  lLineEnd.z - lLineStart.z, lLineEnd.w - lLineStart.w };

        const f32 lfLength2 = LineDot3(lafDelta[0], lafDelta[1], lafDelta[2]);       // vmsum3fp128 (rule 1)
        const f32 lfLength  = (lfLength2 == 0.0f)
                            ? 0.0f                                                    // vsel on len2 == 0
                            : lfLength2 * NewtonRaphsonReciprocalSqrt2(lfLength2);
        lParams.mfLineLength.x = lfLength;
        lParams.mfLineLength.y = lfLength;
        lParams.mfLineLength.z = lfLength;
        lParams.mfLineLength.w = lfLength;

        if (!(lfLength > KF_LINE_TEST_MIN_LENGTH))
        {
            const bool lbSphereAnswer =
                SphereTest(lx32EntityTypeFlags, lLineStart, KF_LINE_TEST_MIN_LENGTH, lpResultBufferOut);
            NoteOctreeLineTest(lx32EntityTypeFlags, lLineStart, lLineEnd, lfLength, "sphere",
                               lpResultBufferOut->GetNumResultsAttempted());               // [DIAG]
            return lbSphereAnswer;
        }

        const f32 lfInverseLength     = NewtonRaphsonReciprocal2(lfLength);
        const f32 lfEpsilonReciprocal = NewtonRaphsonReciprocal2(KF_LINE_RECIPROCAL_EPSILON) * 1.0f;

        f32* const lpfDirection  = &lParams.mLineDirection.x;
        f32* const lpfReciprocal = &lParams.mLineReciprocal.x;
        for (s32 liLane = 0; liLane < 4; ++liLane)
        {
            const f32  lfDelta = lafDelta[liLane];
            const bool lbTiny  = (lfDelta > -KF_LINE_RECIPROCAL_EPSILON) && !(lfDelta >= KF_LINE_RECIPROCAL_EPSILON);
            lpfDirection[liLane]  = lfInverseLength * lfDelta;
            lpfReciprocal[liLane] = lbTiny ? lfEpsilonReciprocal : NewtonRaphsonReciprocal2(lfDelta) * 1.0f;
        }

        const bool lbRootCrossed = TestLineAgainstNodeBoundingBox(mpRootNode, &lParams);
        if (lbRootCrossed)
        {
            LineTestRecursive(0, &lParams);
        }
        NoteOctreeLineTest(lx32EntityTypeFlags, lLineStart, lLineEnd, lfLength,
                           lbRootCrossed ? "walked" : "gated",
                           lpResultBufferOut->GetNumResultsAttempted());                   // [DIAG]
        return lpResultBufferOut->GetNumResultsAttempted() > 0;
    }

    // @ 0x828BCF50 -- LineTestRecursive (DWARF CgsLooseOctree.cpp:1654, const, 731 insns).
    //
    // ONE NODE'S OWN ENTITIES (0x828BCF7C..0x828BD4F0), when the node's type mask meets the query's:
    //   the chain is walked muNumElements times from muHeadIndex (0x828BCFEC..0x828BD328); an entity
    //   whose link mask meets the query's is filed into a four-slot batch -- its index recovered from
    //   the link address (CalcEntityIndex, "luIndex < (uint32_t)KI_MAX_NUM_ENTITIES",
    //   CgsSpatialPartition.h:417) and its bounding sphere copied in (GetEntityBoundingSphere,
    //   "lu16Index < KI_MAX_NUM_ENTITIES", :400);
    //   a FULL batch runs TestLineSphere4 (:172) at once and pushes the hit lanes 0..3 in order
    //   (0x828BD23C..0x828BD2D4), then starts a new batch;
    //   a partial batch left at the end runs the :305 twin into a 16-byte-aligned stack array and
    //   pushes its first `count` hit lanes in order (0x828BD4C0..0x828BD4F0).
    // THE FOUR CHILDREN (0x828BD4F4..0x828BDAA4), when the node has any: a child whose sub-tree type
    //   mask meets the query's gets its box, centre -/+ {HalfSize, HalfHeight, HalfSize, HalfSize}
    //   (mParams0.w / mParams1.z, merged by vperm unk_82CDA350 + vrlimi128), and its enable lane
    //   (vrlimi128 into v124); the others keep the parking box {0 .. 1} (0x828BD54C) and a zero lane.
    //   One TestLineBoundingBoxAgainstAxisAlignedBox4 against the start and reciprocal, ANDed with the
    //   enable lanes (vand128 v127, v0, v124), then a recursion into each set lane in child order.
    void LooseOctree::LineTestRecursive(u16 lu16NodeIndex,
                                        const SpatialPartition::LineTestRecursiveFuncParams* lpParams) const
    {
        const LooseOctreeNode&          lrNode         = mpNodes[lu16NodeIndex];
        CoarseQueryResultBuffer<16384>* lpResultBuffer = lpParams->mpResultBufferOut;

        if ((lrNode.mxNodeEntityFlags & lpParams->mx32EntityTypeFlags) != 0)
        {
            const s32 liNumElements = static_cast<s32>(lrNode.muNumElements);           // cmpwi (signed)
            if (liNumElements > 0)
            {
                alignas(16) CgsGeometric::Sphere laBatch[4] = {};                           // sp+0xD0
                u16 lau16BatchEntities[4] = { 0, 0, 0, 0 };                                // sp+0x60
                s32 liBatchCount = 0;

                u16 lu16Link = lrNode.muHeadIndex;
                for (u32 luRemaining = static_cast<u32>(liNumElements); luRemaining != 0; --luRemaining)
                {
                    // NOT X360: the console trusts the count; a chain that ends early leaves it a null
                    // link (0x828BD31C) to dereference. The host stops instead of reading past the pool.
                    if (lu16Link == KU_INVALID_ENTITY_LINK)
                    {
                        CGS_ASSERT(false, "LooseOctree::LineTestRecursive: node entity chain shorter than muNumElements");
                        break;
                    }

                    const SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16Link);
                    if ((lpParams->mx32EntityTypeFlags & lrLink.mx32TypeFlags) != 0)
                    {
                        const u16 lu16Entity = CalcEntityIndex(lrLink);
                        lau16BatchEntities[liBatchCount] = lu16Entity;
                        laBatch[liBatchCount]            = GetEntityBoundingSphereConst(lu16Entity);
                        ++liBatchCount;

                        if (liBatchCount == 4)
                        {
                            const Vector4 lHits = CgsGeometric::TestLineSphere4(
                                laBatch[0], laBatch[1], laBatch[2], laBatch[3],
                                lpParams->mLineStart, lpParams->mLineDirection, lpParams->mfLineLength);
                            for (u32 luLane = 0; luLane < 4; ++luLane)
                            {
                                if (Mask4LaneSet(lHits, luLane))
                                {
                                    lpResultBuffer->PushResult(lau16BatchEntities[luLane]);
                                }
                            }
                            liBatchCount = 0;
                        }
                    }
                    lu16Link = lrLink.mu16NextEntity;
                }

                if (liBatchCount > 0)
                {
                    alignas(16) s32 laiResults[4];
                    CgsGeometric::TestLineSphere4(laBatch[0], laBatch[1], laBatch[2], laBatch[3],
                                                  lpParams->mLineStart, lpParams->mLineDirection,
                                                  lpParams->mfLineLength, laiResults);
                    for (s32 liLane = 0; liLane < liBatchCount; ++liLane)
                    {
                        if (laiResults[liLane] != 0)
                        {
                            lpResultBuffer->PushResult(lau16BatchEntities[liLane]);
                        }
                    }
                }
            }
        }

        const u16 lu16FirstChild = lrNode.muFirstChildIndex;
        if (lu16FirstChild == KU_INVALID_NODE)
        {
            return;
        }

        CgsGeometric::AxisAlignedBox laChildBoxes[KU_NUM_SUBNODES];
        bool                         labEnabled[KU_NUM_SUBNODES];
        for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
        {
            CgsGeometric::AxisAlignedBox& lrBox = laChildBoxes[luChild];
            lrBox.mMin.x = lrBox.mMin.y = lrBox.mMin.z = lrBox.mMin.w = 0.0f;           // v126
            lrBox.mMax.x = lrBox.mMax.y = lrBox.mMax.z = lrBox.mMax.w = 1.0f;           // v125
            labEnabled[luChild] = false;

            const u16 lu16Child = static_cast<u16>(lu16FirstChild + luChild);
            if ((mpNodesEntityInfo[lu16Child].mxSubTreeEntityFlags & lpParams->mx32EntityTypeFlags) != 0)
            {
                const LooseOctreeNode& lrChild = mpNodes[lu16Child];
                const f32 lfHalfSize   = lrChild.GetHalfSize();
                const f32 lfHalfHeight = lrChild.GetHalfHeight();
                lrBox.mMin.x = lrChild.mPosition.x - lfHalfSize;
                lrBox.mMin.y = lrChild.mPosition.y - lfHalfHeight;
                lrBox.mMin.z = lrChild.mPosition.z - lfHalfSize;
                lrBox.mMin.w = lrChild.mPosition.w - lfHalfSize;
                lrBox.mMax.x = lrChild.mPosition.x + lfHalfSize;
                lrBox.mMax.y = lrChild.mPosition.y + lfHalfHeight;
                lrBox.mMax.z = lrChild.mPosition.z + lfHalfSize;
                lrBox.mMax.w = lrChild.mPosition.w + lfHalfSize;
                labEnabled[luChild] = true;
            }
        }

        const Vector4 lCrossed = CgsGeometric::TestLineBoundingBoxAgainstAxisAlignedBox4(
            laChildBoxes[0], laChildBoxes[1], laChildBoxes[2], laChildBoxes[3],
            lpParams->mLineStart, lpParams->mLineEnd, lpParams->mLineReciprocal);

        for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)
        {
            if (labEnabled[luChild] && Mask4LaneSet(lCrossed, luChild))
            {
                LineTestRecursive(static_cast<u16>(lu16FirstChild + luChild), lpParams);
            }
        }
    }

    // =============================================================================================================
    // THE VOLUME WALK (2026-09-25, crash parity FX-FOLLOWUPS item 2) -- the entity arm of the scene manager's
    // volume tests: which entities' bounding spheres does a caller's rwcollision volume intersect? The narrow
    // test is the octree's own VolumeVolumeQuery (built by Construct in macVolumeVolumeQueryBuffer) run with the
    // caller's volume as its single input and the octree's moving BoxVolume / SphereVolume as its query volume.
    // =============================================================================================================

    // DWARF CgsLooseOctree.cpp:126 / X360 dword_82F33F40 ("Octree Volume Test") -- not registered by the tree's
    // Construct yet (the header note), so -1 == invalid handle and both PerfMonCpu calls are inert.
    s32 LooseOctree::_miVolumeTestPerfMon = -1;

    // @ 0x828CA910 -- VolumeTest (slot 8, DWARF CgsLooseOctree.h:284).
    //   0x828CA928..0x828CA938  the parameter block on the stack (sp+0x50): volume r5 -> +0x00, flags r4 ->
    //                           +0x04, buffer r7 -> +0x08, transform r6 -> +0x0C
    //   0x828CA93C / 0x828CA954 PerfMonCpu::StartMonitor / StopMonitor(dword_82F33F40, re-read) around
    //   0x828CA950              VolumeTestRecursive(0, &params)
    //   0x828CA95C..0x828CA9B0  the inlined GetNumResultsAttempted (its :348 batch assert, then +0x808C) > 0
    bool LooseOctree::VolumeTest(u32 lx32EntityTypeFlags, const VolRef::Volume* lpVolume,
                                 const Matrix44Affine* lpTransform,
                                 CoarseQueryResultBuffer<16384>* lpResultBufferOut)
    {
        SpatialPartition::VolumeTestRecursiveFuncParams lParams;
        lParams.mpVolume            = lpVolume;
        lParams.mx32EntityTypeFlags = lx32EntityTypeFlags;
        lParams.mpResultBufferOut   = lpResultBufferOut;
        lParams.mpTransform         = lpTransform;

        CgsDev::PerfMonCpu::StartMonitor(_miVolumeTestPerfMon);   // lwz r3, dword_82F33F40
        VolumeTestRecursive(0, &lParams);
        CgsDev::PerfMonCpu::StopMonitor(_miVolumeTestPerfMon);    // lwz r3, dword_82F33F40 (re-read)

        return lpResultBufferOut->GetNumResultsAttempted() > 0;
    }

    namespace
    {
        // The nine stores VolumeTestRecursive primes the octree's volume query with before each test (the node:
        // 0x828BDF00..0x828BDF20; each entity: 0x828BE030..0x828BE050, the same nine): the caller's volume is
        // the single INPUT -- the parameter block's own address is the one-entry input-volume array and
        // &mpTransform the one-entry matrix array -- and the octree's moving box / sphere is the QUERY volume.
        // The VolRef::Volume -> rw::collision::Volume step is the one DWARF volume.h:39 makes (the same cast
        // OverlapCullingModule::DoPairQuery and CgsVolumeManager.cpp:212 make).
        void PrimeOctreeVolumeQuery(rw::collision::VolumeVolumeQuery* lpQuery,
                                    SpatialPartition::VolumeTestRecursiveFuncParams* lpParams,
                                    const rw::collision::Volume* lpQueryVolume,
                                    const Matrix44Affine* lpQueryTransform)
        {
            lpQuery->m_padding         = 0.0f;                                        // +0x14 (flt_82001CC0)
            lpQuery->m_inputVols       =
                reinterpret_cast<const rw::collision::Volume**>(&lpParams->mpVolume);  // +0x00
            lpQuery->m_inputMats       = &lpParams->mpTransform;                      // +0x04
            lpQuery->m_numInputs       = 1;                                           // +0x08
            lpQuery->m_currInput       = 0;                                           // +0x0C
            lpQuery->m_volRefPairCount = 0;                                           // +0x1C
            lpQuery->m_queryVol        = lpQueryVolume;                               // +0x38
            lpQuery->m_queryMtx        = lpQueryTransform;                            // +0x3C
            lpQuery->m_cullTable       = 0;                                           // +0x10
        }
    }

    // @ 0x828BDE28 -- VolumeTestRecursive (DWARF CgsLooseOctree.cpp:2123).
    //
    // THE NODE (0x828BDE7C..0x828BDF30): the node's loose box becomes the query BoxVolume -- half extents
    //   {HalfSize, HalfHeight, HalfSize} (mParams0.w and mParams1.z, merged by vperm unk_82CDA350 + vrlimi128
    //   and stored at +0x44 / +0x48 / +0x4C) at the node centre (mPosition, all four lanes, stvx128 into
    //   mNodeTransform's translation row +0x8D900). If the caller's volume intersects none of it
    //   (GetPrimitiveIntersections == 0) the whole sub-tree is pruned. There is NO node type-mask gate here,
    //   unlike the line walk.
    // THE ENTITIES (0x828BDF34..0x828BE08C), when muNumElements > 0: the node's link list is walked from
    //   muHeadIndex through mu16NextEntity until the 0xFFFF sentinel (the count only gates the entry; the
    //   console also tests each link POINTER for null, `cmplwi r20, 0`, which a host array element never is).
    //   A link whose type mask meets the query's recovers its entity index (CalcEntityIndex, :417) and bounding
    //   sphere (GetEntityBoundingSphere, :392), which becomes the query SphereVolume -- the radius (w) into
    //   +0x50, the whole sphere vector into mEntityTransform's translation row +0x8D940 (so its w carries the
    //   radius) -- and the entity is pushed when the caller's volume intersects it.
    // THE CHILDREN (0x828BE090..0x828BE0F0): each of the four children whose sub-tree mask meets the query is
    //   recursed into, in child order; the child's own box test is its first step.
    void LooseOctree::VolumeTestRecursive(u16 lu16NodeIndex, SpatialPartition::VolumeTestRecursiveFuncParams* lpParams)
    {
        const LooseOctreeNode&          lrNode         = mpNodes[lu16NodeIndex];
        CoarseQueryResultBuffer<16384>* lpResultBuffer = lpParams->mpResultBufferOut;   // lwz r15, 8(r28)

        mpNodeVolume->mBoxData.mfHx = lrNode.GetHalfSize();                               // stfs +0x44
        mpNodeVolume->mBoxData.mfHy = lrNode.GetHalfHeight();                             // stfs +0x48
        mpNodeVolume->mBoxData.mfHz = lrNode.GetHalfSize();                               // stfs +0x4C
        mNodeTransform.wAxis.x = lrNode.mPosition.x;                                      // stvx128 +0x8D900
        mNodeTransform.wAxis.y = lrNode.mPosition.y;
        mNodeTransform.wAxis.z = lrNode.mPosition.z;
        mNodeTransform.wAxis.w = lrNode.mPosition.w;

        PrimeOctreeVolumeQuery(mpVolumeVolumeQuery, lpParams, mpNodeVolume, &mNodeTransform);
        if (mpVolumeVolumeQuery->GetPrimitiveIntersections() == 0)                        // cmplwi r3, 0 ; beq
        {
            return;
        }

        if (static_cast<s32>(lrNode.muNumElements) > 0)                                   // cmpwi / ble
        {
            u16 lu16Link = lrNode.muHeadIndex;                                            // lhz 0x4C(r26)
            for (;;)
            {
                const SpatialPartitionEntityLink& lrLink = GetEntityLink(lu16Link);
                if ((lpParams->mx32EntityTypeFlags & lrLink.mx32TypeFlags) != 0)
                {
                    const u16 lu16Entity = CalcEntityIndex(lrLink);
                    const CgsGeometric::Sphere& lrSphere = GetEntityBoundingSphere(lu16Entity);

                    mpEntityVolume->mfRadius = lrSphere.mPositionRadius.w;                // stfs +0x50
                    mEntityTransform.wAxis.x = lrSphere.mPositionRadius.x;                // stvx128 +0x8D940
                    mEntityTransform.wAxis.y = lrSphere.mPositionRadius.y;
                    mEntityTransform.wAxis.z = lrSphere.mPositionRadius.z;
                    mEntityTransform.wAxis.w = lrSphere.mPositionRadius.w;

                    PrimeOctreeVolumeQuery(mpVolumeVolumeQuery, lpParams, mpEntityVolume, &mEntityTransform);
                    if (mpVolumeVolumeQuery->GetPrimitiveIntersections() != 0)
                    {
                        lpResultBuffer->PushResult(lu16Entity);
                    }
                }

                lu16Link = lrLink.mu16NextEntity;                                         // lhz 4(r20)
                if (lu16Link == KU_INVALID_ENTITY_LINK)                                   // cmplwi 0xFFFF ; beq
                {
                    break;
                }
            }
        }

        const u16 lu16FirstChild = lrNode.muFirstChildIndex;                              // lhz 0x42(r26)
        if (lu16FirstChild == KU_INVALID_NODE)
        {
            return;
        }
        for (u32 luChild = 0; luChild < KU_NUM_SUBNODES; ++luChild)                       // li r27, 4
        {
            const u16 lu16Child = static_cast<u16>(lu16FirstChild + luChild);
            if ((lpParams->mx32EntityTypeFlags & mpNodesEntityInfo[lu16Child].mxSubTreeEntityFlags) != 0)
            {
                VolumeTestRecursive(lu16Child, lpParams);
            }
        }
    }
}
