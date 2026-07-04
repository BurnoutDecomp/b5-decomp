#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                            // Vector3 / Vector4 / VecFloat
#include "GameShared/GameClasses/Core/CgsAssert.h"                                     // CGS_ASSERT
#include "GameShared/GameClasses/Geometric/Primitives/CgsSphere.h"                     // CgsGeometric::Sphere
#include "GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/CgsSpatialPartition.h" // CgsSceneManager::SpatialPartition
#include "SDKs/EATech/eajobs/job.h"                                                    // EA::Jobs::Job (embedded by value x4)

// ============================================================================
// GameShared/GameClasses/SceneManager/SpatialPartitionModule/SpatialPartitions/
//   CgsLooseOctree.{h,cpp}   +   CgsLooseOctreeNode.h
//
// CgsSceneManager::LooseOctree -- a loose octree broad-phase spatial partition
// (derives from SpatialPartition). Reconstructed from BURNOUT_X360_ARTIST.XEX.
// Shape, member names + byte offsets are DWARF-attested
// (references/DecFIGS/dwarfdump/.../CgsLooseOctree.h + CgsLooseOctreeNode.h);
// the accessor lane assignments are grounded in the batch's inlined SIMD asm.
//
// LooseOctreeNode packs its geometry into four 16-byte vectors then a small
// bookkeeping tail; total stride is 96 bytes (0x60), proven by FlagBranchForUpdate's
//   nodeAddr = mpNodes + 96 * parentIndex          (asm: slwi r11,r11,5 over 3*idx)
// and the +0x50 flag word it OR-updates.
//
//   +0x00  Vector3 mPosition          node centre
//   +0x10  Vector3 mHalfDimensions    (loose) half extents
//   +0x20  Vector4 mParams0           lane1(y)=half base size, lane3(w)=half size
//   +0x30  Vector4 mParams1           lane0(x)=minY, lane1(y)=maxY, lane2(z)=half height
//   +0x40  u16     muParentIndex      KU_INVALID_NODE (0xFFFF) == root
//   +0x42  u16     muFirstChildIndex
//   +0x44  SpatialPartitionEntityList mEntityList   (12B intrusive list)
//   +0x50  u32     muFlags            bit0 = KU_OCTREE_NODE_FLAG_NEEDS_UPDATE
//   +0x54  u32     mxNodeEntityFlags
//   +0x58  u32     muSubTreeEntityCount
//   sizeof == 0x60
//
// Node lane map (DWARF gives Get*/Set* names; the asm pins which lane each reads):
//   mParams0.y  == GetHalfBaseSize     (EntityInsideNodeBounds, splat lane 1)
//   mParams0.w  == GetHalfSize         (CalcNodeCorners / TestLine, splat lane 3)
//   mParams1.x  == GetMinY            (UpdateNodeYBounds, splat lane 0)
//   mParams1.y  == GetMaxY            (UpdateNodeYBounds, splat lane 1)
//   mParams1.z  == GetHalfHeight      (CalcNodeCorners, splat lane 2)
//
// LooseOctree's own node-array pointer mpNodes sits at this+0x446A4 (attested by
// FlagBranchForUpdate's lwz off *(this+280228)); DWARF names it mpNodes @ CgsLooseOctree.h:580.
// The class is huge (a 299008-byte volume-query scratch buffer plus 4 embedded
// 848-byte EA::Jobs::Jobs -- DWARF maFrustumTestJobs[4] -- at this+0x8D960, stride
// 0x350, proven by the constructor loop). The full 32-bit member layout is not modelled
// here field-for-field; the two batch anchors used by the reconstructed bodies -- the
// inherited bounding-sphere pool (this+0x13900) and mpNodes (this+0x446A4) -- are reached
// through the base accessor and an attested-offset helper respectively. Storage is
// allocator-backed: SpatialPartitionManager::Prepare carves the octree out of the scene
// resource allocator via the placement `operator new` (X360 @ 0x828BADD8, body in the
// .cpp), then runs the LooseOctree constructor (@ 0x828C9718) on the carved buffer.
// ============================================================================

namespace rw { struct IResourceAllocator; }

namespace CgsSceneManager
{
    // CgsLooseOctreeNode.h:37 (DWARF)
    static const u16 KU_INVALID_NODE = 0xFFFF;

    // CgsLooseOctree.h:99..101 (DWARF) -- node flag bits.
    static const u32 KU_OCTREE_NODE_FLAG_NEEDS_UPDATE   = 1;
    static const u32 KU_OCTREE_NODE_FLAG_ENTITY_REMOVED = 2;
    static const u32 KU_OCTREE_NODE_FLAG_ENTITY_ADDED   = 4;

    // CgsLooseOctree.h:69 / 83 (DWARF)
    static const u32 KU_NUM_SUBNODES          = 4;
    static const u32 KU_NUM_FRUSTUM_TEST_JOBS = 4;

    // ------------------------------------------------------------------
    // LooseOctreeNode -- one cell of the loose octree. Geometry lives in the
    // four leading 16-byte vectors; lane accessors mirror the DWARF Get*/Set*.
    // Member order + offsets are DWARF-attested (CgsLooseOctreeNode.h).
    // ------------------------------------------------------------------
    struct alignas(16) LooseOctreeNode
    {
        Vector3 mPosition;          // +0x00
        Vector3 mHalfDimensions;    // +0x10
        Vector4 mParams0;           // +0x20 : y = half base size, w = half size
        Vector4 mParams1;           // +0x30 : x = minY, y = maxY, z = half height

        u16 muParentIndex;          // +0x40
        u16 muFirstChildIndex;      // +0x42

        // 12-byte intrusive entity list (IndexedLinkedList<SpatialPartitionEntity,u16>);
        // opaque here -- its body lives with the container TUs.
        unsigned char maEntityList[0x0C]; // +0x44

        u32 muFlags;                // +0x50
        u32 mxNodeEntityFlags;      // +0x54
        u32 muSubTreeEntityCount;   // +0x58
        u32 muPad5C;                // +0x5C : pad to 0x60 stride

        Vector3 GetPosition()    const { return mPosition; }
        // Lane-splat accessors (VecFloat = one broadcast float lane).
        VecFloat GetHalfBaseSize() const { VecFloat lf; lf.x = lf.y = lf.z = lf.w = mParams0.y; return lf; }
        VecFloat GetHalfSize()     const { VecFloat lf; lf.x = lf.y = lf.z = lf.w = mParams0.w; return lf; }
        VecFloat GetHalfHeight()   const { VecFloat lf; lf.x = lf.y = lf.z = lf.w = mParams1.z; return lf; }
        VecFloat GetMinY()         const { VecFloat lf; lf.x = lf.y = lf.z = lf.w = mParams1.x; return lf; }
        VecFloat GetMaxY()         const { VecFloat lf; lf.x = lf.y = lf.z = lf.w = mParams1.y; return lf; }
        void SetMinY(VecFloat lf) { mParams1.x = lf.x; }
        void SetMaxY(VecFloat lf) { mParams1.y = lf.x; }

        u16  GetParentIndex()     const { return muParentIndex; }
        bool HasParent()          const { return muParentIndex != KU_INVALID_NODE; }
    };

    // CgsLooseOctreeNode.h:158 (DWARF)
    struct LooseOctreeNodeEntityInfo
    {
        u32 mxSubTreeEntityFlags;   // +0x00
    };

    // ------------------------------------------------------------------
    // LooseOctree -- the loose octree partition.
    // ------------------------------------------------------------------
    struct LooseOctree : public SpatialPartition
    {
        // Attested offset of the node-array pointer within the (huge) class layout:
        // FlagBranchForUpdate reads *(this + 0x446A4) as mpNodes (DWARF :580).
        static const u32 KU_NODES_PTR_BYTE_OFFSET = 0x446A4;
        // The 4 embedded frustum-test jobs begin here (ctor loop stride 0x350).
        static const u32 KU_FRUSTUM_TEST_JOBS_BYTE_OFFSET = 0x8D960;

        LooseOctree();

        // @ 0x828BADD8 -- placement-new: carve a LooseOctree out of the scene
        // resource allocator (a 16-byte-aligned main-memory block). Body in the .cpp.
        static void* operator new(size_t luSize, rw::IResourceAllocator* lpAllocator);

        // @ 0x828B0D40 -- compute the 8 world-space corners of a node's box.
        void CalcNodeCorners(const LooseOctreeNode* lpNode, Vector3* lpCornersOut) const;

        // @ 0x828C9768 -- is the entity's bounding sphere centre inside the node's
        // horizontal (X/Z) half-base extent?
        bool EntityInsideNodeBounds(u16 lu16EntityIndex, LooseOctreeNode* lpNode);

        // @ 0x828AA800 -- OR the NEEDS_UPDATE flag up the branch from a node to the root.
        void FlagBranchForUpdate(LooseOctreeNode* lpNode);

        // @ 0x828B0FC8 -- slab test of a line segment against a node's AABB.
        bool TestLineAgainstNodeBoundingBox(
            const LooseOctreeNode* lpNode,
            const SpatialPartition::LineTestRecursiveFuncParams* lpParams) const;

        // @ 0x828B0EC8 -- if the entity's vertical extent exceeds the node's [minY,maxY],
        // flag the branch for update.
        void UpdateNodeYBounds(LooseOctreeNode* lpNode, u16 lu16EntityIndex);

    private:
        // mpNodes lives at KU_NODES_PTR_BYTE_OFFSET; reached via the attested offset
        // rather than a modelled member (the intervening layout is not field-modelled).
        LooseOctreeNode* GetNodes() const
        {
            return *reinterpret_cast<LooseOctreeNode* const*>(
                reinterpret_cast<const unsigned char*>(this) + KU_NODES_PTR_BYTE_OFFSET);
        }
    };
}
