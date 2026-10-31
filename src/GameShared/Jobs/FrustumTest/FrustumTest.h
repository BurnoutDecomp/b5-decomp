#pragma once

// CgsSceneManager::FrustumJobQueryInfo — the per-frame batch of frustum-cull queries the
// FrustumTest job runs (up to KU_FRUSTUM_JOB_MAX_NUM_QUERIES == 10): for each query, the
// swizzled view frustum, its view-projection matrix, and the entity-type filter flags, plus
// the live query count. Layout + member names recovered from the DecFIGS DWARF
// (FrustumTest.h:49-54).
//
// Byte layout (validated against the FrustumJobQueryInfo::operator= block copy @0x82BDF950,
// which copies the whole 0x7AC-byte struct):
//   +0x000  Frustum   maFrustum[10]            ; 10 * 0x80  -> 10 memcpy(0x80) blocks (0..0x500)
//   +0x500  Matrix44  maViewProjection[10]     ; 10 * 0x40  -> VMX lvx128/stvx128 copies (0x500..0x780)
//   +0x780  uint32_t  max32EntityTypeFlags[10] ; 10 * 4     -> 10 lwz/stw word copies (0x780..0x7A8)
//   +0x7A8  uint32_t  muNumQueries             ; 1  * 4     -> the 11th lwz/stw word copy (0x7A8)
// sizeof == 0x7B0 (0x7AC rounded up to the 16-byte alignment of the embedded vec4/matrix lanes).

#include "types.hpp"
#include "BrnCommonTypes.h"  // Matrix44 (64-byte / 16-aligned)
#include "GameShared/GameClasses/Geometric/Primitives/CgsFrustum.h"  // CgsGeometric::Frustum (128 bytes)
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"   // CgsGeometric::Sphere (bounding-sphere narrow test)

namespace CgsSceneManager
{
    struct LooseOctreeNode;
    struct LooseOctreeNodeEntityInfo;
    struct SpatialPartitionEntityLink;
    struct JobCoarseResultBuffer;
    // FrustumTest.h:46 (DWARF).
    const u32 KU_FRUSTUM_JOB_MAX_NUM_QUERIES = 10;

    struct FrustumJobQueryInfo
    {
        CgsGeometric::Frustum maFrustum[KU_FRUSTUM_JOB_MAX_NUM_QUERIES];            // +0x000
        Matrix44              maViewProjection[KU_FRUSTUM_JOB_MAX_NUM_QUERIES];     // +0x500
        u32                   max32EntityTypeFlags[KU_FRUSTUM_JOB_MAX_NUM_QUERIES]; // +0x780
        u32                   muNumQueries;                                         // +0x7A8

        // operator= @ 0x82BDF950 — full memberwise copy of all 0x7AC live bytes (the X360
        // lowers it as memcpy(0x80) x10 over maFrustum, then VMX lvx128/stvx128 lane copies
        // over maViewProjection, then 11 lwz/stw word copies over max32EntityTypeFlags +
        // muNumQueries). No field is skipped; semantic parity == a complete struct copy.
        FrustumJobQueryInfo& operator=(const FrustumJobQueryInfo& lrSource);
    };

    // ARTIST 82BDFC48 copies these six header and three trailer members.
    struct CGS_ALIGN(64) FrustumTestJobData
    {
        LooseOctreeNode* mpNodes;
        LooseOctreeNodeEntityInfo* mpNodeTypeMasks;
        SpatialPartitionEntityLink* mpEntityLinks;
        CgsGeometric::Sphere* mpEntitySpheres;
        u32 muNumNodes;
        u32 muMaxEntities;
        FrustumJobQueryInfo mQueryInfo;
        JobCoarseResultBuffer* mpResultBuffer;
        u32 muMaxResults;
        s32 miDebugMode;

        FrustumTestJobData& operator=(const FrustumTestJobData& lrSource);
    };

    // ========================================================================
    // CgsSceneManager::FrustumTestJob -- the FrustumTest worker-job object. Owns the
    // per-frame job payload (mJobData) plus per-query working scratch fields the recursive
    // octree cull fills in. All member OFFSETS are X360-attested from the four job methods
    // (Execute/FrustumTestRecursive/TestEntitiesBulk/TrivialAcceptRecursive
    // @ 0x82BDFCD8..0x82BE0158). Member names are reconstructed.
    //
    //
    // KU_MAX_OCTREE_NODES == 0x2800 (10240): the muNumNodes tripwire in Execute.
    // ========================================================================
    const u32 KU_MAX_OCTREE_NODES = 0x2800;  // 10240

    class FrustumTestJob
    {
    public:
        // @ 0x82BE0158 -- run every live query in mJobData.mQueryInfo. Returns the result buffer.
        void* Execute(const FrustumTestJobData& lrJobData);
        // @ 0x82BDFFC8 -- classify+descend a node against the current query frustum. Returns last test result.
        int   FrustumTestRecursive(u16 lu16NodeIndex);
        // @ 0x82BDFE70 -- SIMD sphere-vs-frustum narrow test over the candidate list. Returns this.
        void* TestEntitiesBulk();
        // @ 0x82BDFCD8 -- accept a fully-inside subtree with no per-sphere test. Node passed as raw byte ptr.
        void* TrivialAcceptRecursive(const LooseOctreeNode* lpNode);

    private:
        int  NodeInsideFrustumVp(const LooseOctreeNode* lpNode, const Matrix44* lpViewProjection);
        void PushEntityForTesting(u16 lu16Entity);
        SpatialPartitionEntityLink& GetEntityNode(u16 lu16Entity);
        bool SphereInsideCurrentFrustum(const CgsGeometric::Sphere* lpSphere) const;

        // ---- attested layout (byte offsets pinned by the four job methods) ------------------
        u16                mau16EntitiesToTest[(0x4E20) / 2];   // +0x0000  candidate index scratch list
        u32                muNumEntitiesToTest;                 // +0x4E20  scratch-list count / loop bound
        CGS_ALIGN(128) FrustumTestJobData mJobData;                            // +0x4E80  (sizeof 0x800)
        LooseOctreeNodeEntityInfo* mpNodeEntityInfoArray;               // +0x5680  cache of mJobData header[1]
        SpatialPartitionEntityLink* mpEntityArray;                       // +0x5684  cache of mJobData header[2]
        Matrix44           mCurrentViewProjection;              // +0x5690  (0x40) working VP for active query
        CgsGeometric::Frustum mCurrentFrustum;             // +0x56D0  working swizzled frustum
        u32                mxCurrentEntityTypeFlags;            // +0x5750  active query entity-type mask
    };
}
