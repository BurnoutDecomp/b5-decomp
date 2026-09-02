#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"  // CgsGeometric::Sphere

// ============================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/
//   CgsSpatialPartition.{h,cpp}
//
// CgsSceneManager::SpatialPartition -- the scene manager's broad-phase base class:
// a fixed pool of per-entity links (type mask + intrusive list link) plus a parallel
// pool of bounding spheres, subclassed by the concrete partition (LooseOctree).
// Reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// X360 LAYOUT (byte offsets, all asm-attested):
//   +0x00000  vtable + per-partition bookkeeping (0x80 opaque prefix)
//   +0x00080  SpatialPartitionEntityLink maEntityLinks[KI_MAX_NUM_ENTITIES]  (8-byte stride)
//   +0x13900  CgsGeometric::Sphere maEntityBoundingSpheres[KI_MAX_NUM_ENTITIES] (16-byte)
// (0x80 + 10000*8 == 0x13900, so the two pools are back to back.)
//
// PER-PROJECT x64 RULE: this header models the pools as NAMED members rather than
// preserving the X360 byte offsets (pointers widen on the host). Every X360 site that
// reached them by displacement is reproduced through the named member:
//   * LooseOctree::FrustumTestEntities @0x828B1CA0 reads `*(u32*)(this + 8*(i+16))`
//     == maEntityLinks[i].mx32TypeFlags  (the FIRST word of the 8-byte link) and
//     `*(Vector4*)(this + 16*(i+5008))` == maEntityBoundingSpheres[i];
//   * LooseOctree::FrustumTestRecursive @0x828CA9D0 walks a node's entity chain
//     through the SAME array, taking `(link - this - 0x80) / 8` as the entity index;
//   * LooseOctree::StartFrustumTestJobs @0x828B23E0 publishes `this + 0x80` and
//     `this + 0x13900` into the job data block as the two pool base pointers.
//
// The X360's virtual table is documented per method below (slot numbers from the
// SpatialPartitionManager's asm dispatches); on the host the calls go through named
// virtuals, which is the project's semantic-parity form.
// ============================================================================

namespace rw { struct IResourceAllocator; }

namespace CgsSceneManager
{
    struct SpatialPartitionConstructParams;
    template <u32 KU_MaxResults> struct CoarseQueryResultBuffer;
    namespace SceneManagerIO { struct Frustum; }
}

namespace CgsGeometric { struct Frustum; }

namespace CgsSceneManager
{
    typedef f32 float32_t;   // packet signatures spell the radius float32_t (== f32)

    // The 8-byte per-entity broad-phase link (X360 `this + 0x80 + 8*index`) -- DWARF
    // SpatialPartitionEntity, the element type of the node's
    // IndexedLinkList<SpatialPartitionEntity,u16>. The type mask is the FIRST word:
    // FrustumTestEntities @0x828B1CA0 masks the query's entity-type flags against it
    // before it will even look at the sphere. The two u16 links are indices into this
    // same pool (KU_INVALID_ENTITY_LINK == 0xFFFF terminates).
    struct SpatialPartitionEntityLink
    {
        u32 mx32TypeFlags;      // +0x00  (AllocEntity writes ONLY this word)
        u16 mu16NextEntity;     // +0x04
        u16 mu16PrevEntity;     // +0x06
    };

    struct SpatialPartition
    {
        // The fixed-pool sizing (DWARF/asm-attested constants).
        static const s32 KI_MAX_NUM_ENTITIES = 10000;   // pool capacity
        // Terminator of a node's intrusive entity chain (the same 0xFFFF sentinel the
        // node child index uses; FrustumTestRecursive breaks on it).
        static const u16 KU_INVALID_ENTITY_LINK = 0xFFFF;

        // Kept as a named alias so the committed call sites that spell the old name
        // still resolve; the record IS the 8-byte link above.
        typedef SpatialPartitionEntityLink SpatialPartitionEntityNode;

        // The recursive line-test traversal's per-call parameter block. Consumers
        // (LooseOctree::TestLineAgainstNodeBoundingBox @0x828B0FC8) read it by raw vector
        // offset -- line origin at +0x00, inverse line direction at +0x30.
        struct alignas(16) LineTestRecursiveFuncParams
        {
            unsigned char maBytes[0x40];   // +0x00 origin (vec), +0x30 inverse direction (vec)
        };

        virtual ~SpatialPartition() {}

        // ---- the X360 vtable, by slot (SpatialPartitionManager's asm dispatches) ----
        // slot  0  Construct(params, allocator)
        // slot  1  Destruct()
        // slot  2  Prepare()
        // slot  3  Release()
        // slot 10  Update()
        // slot 11  SetEntityPosition(id, position)
        // slot 12  SetEntityRadius(id, radius)
        // slot 14  AddEntityToGraph(id)
        // slot 15  RemoveEntityFromGraph(id)
        virtual void Construct(SpatialPartitionConstructParams* lpParams,
                               rw::IResourceAllocator* lpAllocator) = 0;
        virtual void Destruct() = 0;
        virtual bool Prepare() = 0;
        virtual bool Release() = 0;
        // slot  6  LineTest(entityTypeFlags, lineStart, lineEnd, resultBufferOut)
        //   ADDED 2026-09-02 (scene-query wave 1). X360 vtbl+24, dispatched by
        //   SceneManagerModule::ProcessLineTestNearest @0x828D3970 / ProcessLineTestFastDoubleSided
        //   @0x828D3EA0 (`lwz r11,0x280(this) ; lwz r10,0(r11) ; lwz r11,0x18(r10) ; bctrl` with
        //   r4 = mx32EntityTypeFlags, r5 = the coarse result buffer, v1/v2 = start/end). DWARF
        //   CgsSpatialPartition.h:221: `virtual bool LineTest(EntityTypeFlags, Vector3, Vector3,
        //   CoarseQueryResultBufferDefault*)`. The DWARF puts DebugRender (slot 4) and SphereTest
        //   (slot 5) before it and FrustumTest / VolumeTest / FrustumTestVp (slots 7/8/9) after it;
        //   those five are still undeclared here (no mounted caller yet) -- add them IN THAT ORDER
        //   when one lands. Host vtable order is not load-bearing (named virtual calls).
        virtual bool LineTest(u32 lx32EntityTypeFlags, Vector3 lLineStart, Vector3 lLineEnd,
                              CoarseQueryResultBuffer<16384>* lpResultBufferOut) = 0;
        virtual void Update() = 0;
        virtual void SetEntityPosition(u16 lu16Id, Vector3 lPosition) = 0;
        virtual void SetEntityRadius(u16 lu16Id, float32_t lfRadius) = 0;
        virtual void AddEntityToGraph(u16 lu16Id) = 0;
        virtual void RemoveEntityFromGraph(u16 lu16Id) = 0;

        // @ 0x828BA3B0 -- allocate the entity's pool record then splice it into the graph.
        void AddEntity(u16 lu16Id, u32 lxTypeFlags, Vector3 lPosition, float32_t lfRadius);

        // @ 0x828AA038 -- recover a link's pool index from its address.
        u16 CalcEntityIndex(const SpatialPartitionEntityLink& lrEntity) const;

        // @ 0x828A9F68 / 0x828A9FD0 -- index the bounding-sphere pool.
        CgsGeometric::Sphere&       GetEntityBoundingSphere(u16 lu16Index);
        const CgsGeometric::Sphere& GetEntityBoundingSphereConst(u16 lu16Index) const;

        // @ 0x828B1078 -- fill the entity's pool record (type mask + bounding sphere) and
        // return it; null when the id is out of range.
        SpatialPartitionEntityLink* AllocEntity(u16 lu16Id, u32 lxTypeFlags,
                                                Vector3 lPosition, float32_t lfRadius);

        SpatialPartitionEntityLink&       GetEntityLink(u16 lu16Index)       { return maEntityLinks[lu16Index]; }
        const SpatialPartitionEntityLink& GetEntityLink(u16 lu16Index) const { return maEntityLinks[lu16Index]; }

    protected:
        SpatialPartitionEntityLink maEntityLinks[KI_MAX_NUM_ENTITIES];             // X360 +0x00080
        CgsGeometric::Sphere       maEntityBoundingSpheres[KI_MAX_NUM_ENTITIES];   // X360 +0x13900
    };
}
