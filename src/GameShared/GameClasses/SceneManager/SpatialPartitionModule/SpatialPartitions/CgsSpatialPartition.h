#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"  // CgsGeometric::Sphere

// ============================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/
//   CgsSpatialPartition.{h,cpp}
//
// CgsSceneManager::SpatialPartition -- the broad-phase spatial partition: a fixed
// pool of entity nodes (KI_MAX_NUM_ENTITIES) plus a parallel pool of bounding
// spheres, spliced into a broad-phase graph. Reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// X360 LAYOUT (grounded in the accessor asm displacements):
//   +0x00000  header (vtable + partition bookkeeping; opaque, 0x80 bytes)
//   +0x00080  SpatialPartitionEntityNode maEntities[KI_MAX_NUM_ENTITIES]   (8-byte stride)
//   +0x13900  CgsGeometric::Sphere       maEntityBoundingSpheres[KI_MAX_NUM_ENTITIES] (16-byte)
// The entity-array byte base (this + 0x80) is what GetBaseNode() returns; the
// bounding-sphere pool begins at 0x80 + 10000*8 == 0x13900 (16 * 5008).
//
// FLAG (foreign types): SpatialPartitionEntityNode's real body lives with the
// FrustumTest / broad-phase job TUs; it is modelled here as correctly-sized (8-byte)
// opaque storage so the pool stride and the two pointer-arithmetic accessors
// (CalcEntityIndex / GetEntityBoundingSphere) are exact. The vtable header and the
// per-partition bookkeeping preceding maEntities are modelled as a 0x80-byte opaque
// prefix. AllocEntity + the virtual AddEntityToGraph are declared only (their bodies
// live in their own TUs, as the rest of this class's ledger functions).
// ============================================================================

namespace CgsSceneManager
{
    typedef f32 float32_t;   // packet signatures spell the radius float32_t (== f32)

    struct SpatialPartition
    {
        // The fixed-pool sizing (DWARF/asm-attested constants).
        static const s32 KI_MAX_NUM_ENTITIES        = 10000;   // pool capacity
        static const u32 KU_ENTITY_ARRAY_BYTE_OFFSET = 0x80;   // this -> maEntities byte base

        // FLAG: foreign node body (broad-phase entity node). Opaque, 8-byte stride so the
        // pool addressing in CalcEntityIndex is exact.
        struct SpatialPartitionEntityNode
        {
            unsigned char maBytes[8];
        };

        // The recursive line-test traversal's per-call parameter block. Consumers
        // (LooseOctree::TestLineAgainstNodeBoundingBox @0x828B0FC8) read it by raw vector
        // offset -- line origin at +0x00, inverse line direction at +0x30 -- so it is modelled
        // here as a documented opaque block sized to those attested offsets (two 16-byte vectors
        // plus the intervening 0x20 span). Its full field layout has its real home with the
        // line-test recursion TU; this minimal opaque shell only sizes the pointer target so the
        // slab-test's raw-offset reads land correctly.
        struct alignas(16) LineTestRecursiveFuncParams
        {
            unsigned char maBytes[0x40];   // +0x00 origin (vec), +0x30 inverse direction (vec)
        };

        // @ 0x828BA3B0 -- allocate a new entity node, then splice it into the graph.
        void AddEntity(u16 lu16Id, u32 lxTypeFlags, Vector3 lPosition, float32_t lfRadius);

        // @ 0x828AA038 -- recover a node's pool index from its address.
        u16 CalcEntityIndex(const SpatialPartitionEntityNode& lrEntity) const;

        // @ 0x828A9F68 / 0x828A9FD0 -- index the bounding-sphere pool.
        CgsGeometric::Sphere&       GetEntityBoundingSphere(u16 lu16Index);
        const CgsGeometric::Sphere& GetEntityBoundingSphereConst(u16 lu16Index) const;

        // @ 0x828B1078 -- allocate a node in the fixed pool (own TU; declared only).
        SpatialPartitionEntityNode* AllocEntity(u16 lu16Id, u32 lxTypeFlags,
                                                Vector3 lPosition, float32_t lfRadius);

        // Virtual AddEntityToGraph @ X360 vtable slot +0x38 (14 virtuals precede it in
        // DWARF order). Splices an allocated node into the broad-phase graph by id.
        // (Own TU; declared only -- selfcheck is a compile gate, so no body is needed.)
        virtual void AddEntityToGraph(u16 lu16Id);

    private:
        // 0x80-byte opaque prefix: the vtable pointer + per-partition bookkeeping that
        // precede the entity pool (this + 0x80 == GetBaseNode()).
        unsigned char maHeader[KU_ENTITY_ARRAY_BYTE_OFFSET];

        SpatialPartitionEntityNode maEntities[KI_MAX_NUM_ENTITIES];             // @ +0x00080
        CgsGeometric::Sphere       maEntityBoundingSpheres[KI_MAX_NUM_ENTITIES]; // @ +0x13900
    };
}
