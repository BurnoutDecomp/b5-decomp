#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                            // Vector3 / Vector4 / VecFloat / Matrix44
#include "GameShared/GameClasses/Core/CgsAssert.h"                                     // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                     // CgsGeometric::Sphere
#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"                    // CgsGeometric::Frustum (the 0x80 SoA plane block)
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsSpatialPartition.h" // SpatialPartition
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/CgsJobCoarseResultBuffer.h"              // JobCoarseResultBuffer

// ============================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/
//   CgsLooseOctree.{h,cpp}   +   CgsLooseOctreeNode.h
//
// CgsSceneManager::LooseOctree -- the scene manager's loose broad-phase partition
// (derives from SpatialPartition). Reconstructed from BURNOUT_X360_ARTIST.XEX; the
// member names are DWARF-attested (references/DecFIGS/dwarfdump/.../CgsLooseOctree.h
// + CgsLooseOctreeNode.h) and every field is pinned by an asm displacement.
//
// Despite the name the tree fans out FOUR ways -- the subdivision is in the world XZ
// plane and each node tracks a LOOSE [minY, maxY] band instead of splitting in Y
// (Construct's node-count loop sums 4^k, FrustumTestRecursive loops `c < 4`, and
// EntityInsideNodeBounds tests only X and Z).
//
// PER-PROJECT x64 RULE: members are NAMED, not offset-pinned (pointers widen on the
// host). The X360 byte offsets every reader used are recorded per member.
//
// X360 MAP (attested):
//   +0x00080  maEntityLinks[10000]            (SpatialPartition, 8B stride)
//   +0x13900  maEntityBoundingSpheres[10000]  (SpatialPartition, 16B stride)
//   +0x3AA00  maEntityInfo[10000]             (SpatialPartition, 4B: the owning node)
//   +0x44680  muDepth      +0x44684 miNumStaticNodes
//   +0x44688  mfBaseSize   +0x4468C mfLooseness   +0x44690 mCentrePos
//   +0x446A0  mpRootNode   +0x446A4 mpNodes
//   +0x446A8  mFreeNodeGroupPool (elements / free-indices / used / free / capacity)
//   +0x446B8  mpNodesEntityInfo (one u32 sub-tree type mask per node)
//   +0x8D950  muAdaptiveNodeSplitThreshold   +0x8D954 muAdaptiveMaxDepth
//   +0x8D958  mpEntityListNodes (== &maEntityLinks[0])
//   +0x8D960  maFrustumTestJobs[4]     (EA::Jobs::Job, stride 0x350)
//   +0x8E700  maFrustumTestJobData[4]  (stride 0x800)
//   +0x90700  maJobResultBuffers[4]    (JobCoarseResultBuffer, stride 0x100)
//   +0x90B00  mabFrustumJobActive[4]   +0x90B04 mabFrustumJobRequested[4]
//   sizeof == 0x90B80 (the literal SpatialPartitionManager::Prepare @0x828D0054 passes
//   to operator new), alignof 128.
// ============================================================================

namespace rw { struct IResourceAllocator; }

namespace CgsSceneManager
{
    // The shared coarse-query result buffer the frustum queries drain into.
    template <u32 KU_MaxResults> struct CoarseQueryResultBuffer;

    // CgsLooseOctreeNode.h:37 (DWARF)
    static const u16 KU_INVALID_NODE = 0xFFFF;

    // CgsLooseOctree.h:99..101 (DWARF) -- node flag bits.
    static const u32 KU_OCTREE_NODE_FLAG_NEEDS_UPDATE   = 1;
    static const u32 KU_OCTREE_NODE_FLAG_ENTITY_REMOVED = 2;
    static const u32 KU_OCTREE_NODE_FLAG_ENTITY_ADDED   = 4;

    // CgsLooseOctree.h:69 / 83 (DWARF).
    static const u32 KU_NUM_SUBNODES          = 4;
    static const u32 KU_NUM_FRUSTUM_TEST_JOBS = 4;

    // Construct's node-count loop reserves this many spare four-node GROUPS beyond the
    // static tree for the adaptive-depth refinement (`(subnodes + 0x2003) & ~3`, i.e.
    // roundup4(subnodes) + 2048*4).
    static const u32 KU_LOOSE_OCTREE_ADAPTIVE_NODEARRAY_POOL_SIZE = 2048;

    // The per-job query capacity. The X360 asserts the query index against
    // KU_JOB_BUFFER_MAX_NUM_QUERIES (16), but FrustumJobQueryInfo's three parallel
    // arrays are packed for TEN and sixteen would overrun the 0x800 block
    // (0x20 + 10*0x80 = 0x520, + 10*0x40 = 0x7A0, + 10*4 = 0x7C8 == muNumQueries).
    // The packing is authoritative; both constants are modelled under their own names.
    static const u32 KU_MAX_QUERIES_PER_JOB = 10;

    // The empty-node Y sentinels PrepareRecursive / UpdateRecursive seed the loose band
    // with: MinY starts at +1e6 and is Min'd down, MaxY at -1e6 and Max'd up, so the band
    // is empty until an entity widens it. UpdateRecursive also uses MaxY == -1e6 as the
    // EMPTY-SUBTREE test (vcmpeqfp. @0x828B1630), so the value is load-bearing.
    // (X360 unk_820F271C / flt_820F27C0.)
    static const f32 KF_LOOSE_OCTREE_MAX_Y =  1000000.0f;
    static const f32 KF_LOOSE_OCTREE_MIN_Y = -1000000.0f;

    // X360 flt_820F2718 -- the slack UpdateRecursive adds to the loose Y band. Applied
    // INSIDE the per-entity loop against the already-padded field, so a node's band grows
    // by 5 * (number of entities filed on it); reproduced literally.
    static const f32 KF_NODE_HEIGHT_OFFSET = 5.0f;

    // ------------------------------------------------------------------
    // LooseOctreeNode -- one cell (0x60, proven by FlagBranchForUpdate's
    // `nodeAddr = mpNodes + 96 * parentIndex`).
    // ------------------------------------------------------------------
    struct alignas(16) LooseOctreeNode
    {
        Vector3 mPosition;          // +0x00
        Vector3 mHalfDimensions;    // +0x10 {HalfSize, HalfHeight, HalfSize}
        // +0x20 x = MaxEntityRadius, y = HalfBaseSize, z = MaxRadiusThreshold, w = HalfSize
        Vector4 mParams0;
        // +0x30 x = MinY, y = MaxY, z = HalfHeight
        Vector4 mParams1;

        u16 muParentIndex;          // +0x40
        u16 muFirstChildIndex;      // +0x42 (KU_INVALID_NODE == leaf)

        // +0x44 IndexedLinkedList<SpatialPartitionEntity,u16> mEntityList
        u32 muNumElements;          // +0x44
        u32 muListElementsPad;      // +0x48 (X360 mpElements -- the entity-link pool base;
                                    //        the host reaches the pool by name instead)
        u16 muHeadIndex;            // +0x4C
        u16 muTailIndex;            // +0x4E

        u32 muFlags;                // +0x50
        u32 mxNodeEntityFlags;      // +0x54 : OR of THIS node's own entity type masks
        u32 muSubTreeEntityCount;   // +0x58
        u32 muPad5C;                // +0x5C

        Vector3 GetPosition()      const { return mPosition; }
        f32 GetMaxEntityRadius()   const { return mParams0.x; }
        f32 GetHalfBaseSize()      const { return mParams0.y; }
        f32 GetMaxRadiusThreshold()const { return mParams0.z; }
        f32 GetHalfSize()          const { return mParams0.w; }
        f32 GetMinY()              const { return mParams1.x; }
        f32 GetMaxY()              const { return mParams1.y; }
        f32 GetHalfHeight()        const { return mParams1.z; }
        void SetMaxEntityRadius(f32 lf) { mParams0.x = lf; }
        void SetMinY(f32 lf)            { mParams1.x = lf; }
        void SetMaxY(f32 lf)            { mParams1.y = lf; }
        void SetHalfHeight(f32 lf)      { mParams1.z = lf; }

        u16  GetParentIndex()     const { return muParentIndex; }
        bool HasParent()          const { return muParentIndex != KU_INVALID_NODE; }
        bool IsLeaf()             const { return muFirstChildIndex == KU_INVALID_NODE; }
        u32  CountElements()      const { return muNumElements; }
    };

    // CgsLooseOctreeNode.h:158 (DWARF) -- the per-node sub-tree type mask
    // (mpNodesEntityInfo @ X360 +0x446B8); FrustumTestRecursive gates each child
    // recursion on it.
    struct LooseOctreeNodeEntityInfo
    {
        u32 mxSubTreeEntityFlags;   // +0x00
    };

    // ------------------------------------------------------------------
    // FrustumJobQueryInfo -- the staged query set inside one job's 0x800 data block
    // (X360 block +0x20; AddJobFrustumTest @0x828AA958 writes it, the job entry
    // FrustumTestJob::Execute @0x82BE0158 reads the same three arrays back).
    // ------------------------------------------------------------------
    struct alignas(16) FrustumJobQueryInfo
    {
        CgsGeometric::Frustum maFrustums[KU_MAX_QUERIES_PER_JOB];           // +0x000 (0x80 each)
        Matrix44              maViewProjections[KU_MAX_QUERIES_PER_JOB];    // +0x500 (0x40 each)
        u32                   max32EntityTypeMasks[KU_MAX_QUERIES_PER_JOB]; // +0x780
        u32                   muNumQueries;                                 // +0x7A8
        u32                   muPad7AC;                                     // +0x7AC
    };

    // ------------------------------------------------------------------
    // FrustumTestJobData -- the 0x800 block Job::SetData publishes to the job
    // (StartFrustumTestJobs @0x828B23E0 fills the header, AddJobFrustumTest the query
    // info). The leading pointers/counters are what let the job walk the tree without
    // a `this`; FrustumTestJobData::operator= @0x82BDFC48 copies exactly this set.
    // ------------------------------------------------------------------
    struct alignas(16) FrustumTestJobData
    {
        LooseOctreeNode*            mpNodes;          // +0x00  <- mpRootNode
        LooseOctreeNodeEntityInfo*  mpNodeTypeMasks;  // +0x04  <- mpNodesEntityInfo
        SpatialPartitionEntityLink* mpEntityLinks;    // +0x08  <- &maEntityLinks[0]
        CgsGeometric::Sphere*       mpEntitySpheres;  // +0x0C  <- &maEntityBoundingSpheres[0]
        u32                         muNumNodes;       // +0x10  <- capacity << 2
        u32                         muMaxEntities;    // +0x14  <- KI_MAX_NUM_ENTITIES
        u32                         muPad18;          // +0x18
        u32                         muPad1C;          // +0x1C
        FrustumJobQueryInfo         mQueryInfo;       // +0x20
        JobCoarseResultBuffer*      mpResultBuffer;   // +0x7D0
        u32                         muMaxResults;     // +0x7D4 <- KU_JOB_RESULT_BUFFER_SIZE
        u32                         muPad7D8;         // +0x7D8
        u32                         muPad7DC;         // +0x7DC
    };

    // ------------------------------------------------------------------
    // LooseOctree.
    // ------------------------------------------------------------------
    struct LooseOctree : public SpatialPartition
    {
        LooseOctree();

        // @ 0x828BADD8 -- placement-new: carve a LooseOctree out of the scene resource
        // allocator (a 16-byte-aligned main-memory block).
        static void* operator new(size_t luSize, rw::IResourceAllocator* lpAllocator);
        static void  operator delete(void*, rw::IResourceAllocator*) {}

        // ---- SpatialPartition virtuals (X360 vtable slots in CgsSpatialPartition.h) ----
        virtual void Construct(SpatialPartitionConstructParams* lpParams,
                               rw::IResourceAllocator* lpAllocator);   // @ 0x828C99D8
        virtual void Destruct();
        virtual bool Prepare();                                        // @ 0x828CA2D0
        virtual bool Release();
        virtual void Update();                                         // @ 0x828D0180
        virtual void SetEntityPosition(u16 lu16Id, Vector3 lPosition); // @ 0x828C9820
        virtual void SetEntityRadius(u16 lu16Id, f32 lfRadius);        // @ 0x828BC740
        virtual void AddEntityToGraph(u16 lu16Id);                     // @ 0x828BB648
        virtual void RemoveEntityFromGraph(u16 lu16Id);                // @ 0x828C9818

        // ---- geometry helpers ----
        void CalcNodeCorners(const LooseOctreeNode* lpNode, Vector3* lpCornersOut) const; // @0x828B0D40
        bool EntityInsideNodeBounds(u16 lu16EntityIndex, LooseOctreeNode* lpNode);        // @0x828C9768
        void FlagBranchForUpdate(LooseOctreeNode* lpNode);                                // @0x828AA800
        bool TestLineAgainstNodeBoundingBox(
            const LooseOctreeNode* lpNode,
            const SpatialPartition::LineTestRecursiveFuncParams* lpParams) const;         // @0x828B0FC8
        void UpdateNodeYBounds(LooseOctreeNode* lpNode, u16 lu16EntityIndex);             // @0x828B0EC8
        u32  CalcNextSubNode(const LooseOctreeNode* lpNode, const Vector4& lrPosition) const; // @0x828B11A0

        // ---- the frustum-test job path ----
        void AddJobFrustumTest(u32 lx32EntityTypeMask,                                     // @0x828AA958
                               const CgsGeometric::Frustum* lpFrustum,
                               const Matrix44* lpViewProjection,
                               u32 luJobIndex);
        void StartFrustumTestJobs();                                                       // @0x828B23E0
        void WaitForFrustumTestJobResults(CoarseQueryResultBuffer<16384>* lpResultBufferOut); // @0x828B2558

        // @ 0x828CA7F8 -- the synchronous view-projection frustum query.
        bool FrustumTestVp(u32 lx32EntityTypeMask,
                           const CgsGeometric::Frustum& lrFrustum,
                           const Matrix44& lrViewProjection,
                           CoarseQueryResultBuffer<16384>* lpResultBuffer);

    private:
        // The per-query traversal parameter block (X360 stack image; see FrustumTestVp).
        // Exactly one of the two sinks is set: the synchronous entry point collects into
        // the caller's shared CoarseQueryResultBuffer through PushResult, the job path
        // into its own JobCoarseResultBuffer run pool (which
        // WaitForFrustumTestJobResults later copies into the shared buffer as one batch
        // per query -- FrustumTestJob::TestEntitiesBulk @0x82BDFF70 writes the run pool
        // the same way).
        struct FrustumTestParams
        {
            const CgsGeometric::Frustum*    mpFrustum;
            u32                             mx32EntityTypeMask;
            CoarseQueryResultBuffer<16384>* mpResultBuffer;
            JobCoarseResultBuffer*          mpJobResultBuffer;
            u32                             muNumNodesVisited;

            // The query's 8 planes de-swizzled ONCE per query into plain arrays. The
            // stored form is SoA across four float lanes, so reading it per entity means
            // a lane select per component; the console gets that for free (the whole
            // batch is one VMX register and the test is eight instructions), the host
            // does not. Hoisting the de-swizzle out of the per-entity loop is a pure
            // code-shape change -- the values, and therefore every accept/reject, are
            // identical.
            f32 maNx[8];
            f32 maNy[8];
            f32 maNz[8];
            f32 maD[8];

            void CachePlanes()
            {
                for (u32 luBatch = 0; luBatch < 2; ++luBatch)
                {
                    const Vector4& lrNx = mpFrustum->maSwizzledPlanes[luBatch * 4 + 0];
                    const Vector4& lrNy = mpFrustum->maSwizzledPlanes[luBatch * 4 + 1];
                    const Vector4& lrNz = mpFrustum->maSwizzledPlanes[luBatch * 4 + 2];
                    const Vector4& lrD  = mpFrustum->maSwizzledPlanes[luBatch * 4 + 3];
                    const f32 laNx[4] = { lrNx.x, lrNx.y, lrNx.z, lrNx.w };
                    const f32 laNy[4] = { lrNy.x, lrNy.y, lrNy.z, lrNy.w };
                    const f32 laNz[4] = { lrNz.x, lrNz.y, lrNz.z, lrNz.w };
                    const f32 laD [4] = { lrD.x,  lrD.y,  lrD.z,  lrD.w  };
                    for (u32 luLane = 0; luLane < 4; ++luLane)
                    {
                        const u32 luPlane = luBatch * 4 + luLane;
                        maNx[luPlane] = laNx[luLane];
                        maNy[luPlane] = laNy[luLane];
                        maNz[luPlane] = laNz[luLane];
                        maD [luPlane] = laD [luLane];
                    }
                }
            }
        };

        void FrustumTestVpRecursive(u16 lu16NodeIndex, FrustumTestParams* lpParams);   // @0x828BDC38
        void TrivialAcceptRecursive(u16 lu16NodeIndex, FrustumTestParams* lpParams);   // @0x828B1B50
        void FrustumTestEntities(u16 lu16FirstEntity, FrustumTestParams* lpParams);    // @0x828B1CA0
        u32  NodeInsideFrustum(const LooseOctreeNode* lpNode,
                               const CgsGeometric::Frustum& lrFrustum) const;          // @0x828BDAC0
        void PushCoarseResult(FrustumTestParams* lpParams, u16 lu16EntityIndex);

        void AllocRecursive(u32 luDepth, u16 lu16NodeIndex, u16 lu16ParentIndex);      // @0x828BB4A0
        void PrepareRecursive(u16 lu16NodeIndex, Vector3 lPosition, f32 lfSize);       // @0x828BB1E8
        void AddEntityInternal(u16 lu16EntityIndex);                                   // @0x828BB648
        void RemoveEntityInternal(u16 lu16EntityIndex);                                // @0x828BB948
        void UpdateRecursive(u16 lu16NodeIndex);                                       // @0x828B12A0
        void LinkEntityToNode(LooseOctreeNode* lpNode, u16 lu16EntityIndex);
        void UnlinkEntityFromNode(LooseOctreeNode* lpNode, u16 lu16EntityIndex);

        u16 GetNodeIndex(const LooseOctreeNode* lpNode) const
        { return static_cast<u16>(lpNode - mpNodes); }

        // ---- members (X360 offsets above) ----
        u32              muDepth;                       // +0x44680
        s32              miNumStaticNodes;              // +0x44684
        f32              mfBaseSize;                    // +0x44688 (the ROOT's FULL size)
        f32              mfLooseness;                   // +0x4468C
        Vector3          mCentrePos;                    // +0x44690
        LooseOctreeNode* mpRootNode;                    // +0x446A0
        LooseOctreeNode* mpNodes;                       // +0x446A4
        LooseOctreeNodeEntityInfo* mpNodesEntityInfo;   // +0x446B8
        u32              muNumNodes;                    // (capacity << 2) + 1
        u32              muNumNodeGroups;               // mFreeNodeGroupPool.muCapacity
        u32              muAdaptiveNodeSplitThreshold;  // +0x8D950
        u32              muAdaptiveMaxDepth;            // +0x8D954

        // The per-entity owning-node back pointer (X360 SpatialPartition::maEntityInfo
        // @ +0x3AA00, one u32 per entity holding a raw LooseOctreeNode*). Modelled as the
        // node INDEX (host pointers widen; the index is what every reader derives anyway).
        u16              maEntityNodeIndex[KI_MAX_NUM_ENTITIES];

        FrustumTestJobData    maFrustumTestJobData[KU_NUM_FRUSTUM_TEST_JOBS];  // +0x8E700
        JobCoarseResultBuffer maJobResultBuffers[KU_NUM_FRUSTUM_TEST_JOBS];    // +0x90700
        // The 0x4000-byte block each result buffer's mpu16Buffer points at. On the X360
        // JobCoarseResultBuffer::Construct @0x828BB128 carves it from the same allocator;
        // held inline here so the buffer needs no second allocation.
        u16  maJobResultStorage[KU_NUM_FRUSTUM_TEST_JOBS][KU_JOB_RESULT_BUFFER_SIZE];
        bool mabFrustumJobActive[KU_NUM_FRUSTUM_TEST_JOBS];      // +0x90B00
        bool mabFrustumJobRequested[KU_NUM_FRUSTUM_TEST_JOBS];   // +0x90B04
    };
}
