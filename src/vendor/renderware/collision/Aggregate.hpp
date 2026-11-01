#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::Aggregate -- the RenderWare collision-aggregate base. Concrete
// collision procedurals (ClusteredMesh, TriangleKDTreeProcedural, ...) derive
// from Aggregate and override its virtuals; Aggregate supplies the trivial base
// behaviour the derived classes fall back to.
//
// OWNING HOME for the single Aggregate function the X360 binary defines:
//     rw::collision::Aggregate::Fixup  @ 0x82BBBBD8
//         (called by rw::collision::TriangleKDTreeProcedural::Fixup and
//          rw::collision::ClusteredMesh::Fixup -- the derived fixups chain into
//          the base Fixup.)
//
// The X360 @ 0x82BBBBD8 is a single tail-branch with no prologue:
//     b      <trivial-sibling>
//     .long 0
// i.e. the function's machine code was IDENTICAL-COMDAT-FOLDED by the linker
// onto a code-identical trivial sibling (IDA renders the surviving symbol name
// at the fold target). The only recoverable fact is that Aggregate::Fixup is a
// trivial leaf that returns a constant -- it carries no aggregate-specific work
// (all real fix-up lives in the derived ClusteredMesh / KDTree overrides). It is
// reconstructed as that trivial base method; the fold target's unrelated symbol
// name is NOT attributed here (doing so would fabricate semantics). FLAGGED:
// the literal return value is inferred from the trivial-fold shape (success/0).
// ===========================================================================

#include "vendor/renderware/collision/AABBox.hpp"

namespace rw
{
namespace collision
{

enum ObjectType
{
    RWCOBJECTTYPE_NA = 524288,
    RWCOBJECTTYPE_VOLUME = 524289,
    RWCOBJECTTYPE_SIMPLEMAPPEDARRAY = 524290,
    RWCOBJECTTYPE_TRIANGLEKDTREEPROCEDURAL = 524291,
    RWCOBJECTTYPE_KDTREEMAPPEDARRAY = 524292,
    RWCOBJECTTYPE_BBOX = 524293,
    RWCOBJECTTYPE_CLUSTEREDMESH = 524294,
    RWCOBJECTTYPE_FORCEENUMSIZEINT = 2147483647
};
class VolumeBBoxQuery;
class VolumeLineQuery;

class Aggregate
{
public:
    typedef u32 (Aggregate::*GetSizeFn)();
    typedef void (Aggregate::*UpdateFn)();
    typedef u32 (Aggregate::*LineIntersectionQueryFn)(VolumeLineQuery*, const rw::math::vpu::Matrix44Affine*);
    typedef u32 (Aggregate::*BBoxOverlapQueryFn)(VolumeBBoxQuery*, const rw::math::vpu::Matrix44Affine*);
    struct VTable
    {
        ObjectType m_type;
        GetSizeFn m_GetSize;
        u32 m_alignment;
        s32 m_isProcedural;
        UpdateFn m_Update;
        LineIntersectionQueryFn m_LineIntersectionQuery;
        BBoxOverlapQueryFn m_BBoxOverlapQuery;
    };
    u32 LineIntersectionQuery(VolumeLineQuery* query, const rw::math::vpu::Matrix44Affine* transform)
    {
        return (this->*(m_vTable->m_LineIntersectionQuery))(query, transform);
    }
    u32 BBoxOverlapQuery(VolumeBBoxQuery* query, const rw::math::vpu::Matrix44Affine* transform)
    {
        return (this->*(m_vTable->m_BBoxOverlapQuery))(query, transform);
    }
    // @ 0x82BBBBD8 -- base aggregate fix-up. Trivial in the base; derived
    // procedurals (ClusteredMesh, TriangleKDTreeProcedural) override it.
    int Fixup();
    const AABBox& GetBBox() const { return m_AABB; }

protected:
    // The X360 bounding-box providers read this first field at +0.
    AABBox m_AABB;
    VTable* m_vTable;
    u32 m_numTagBits; // +0x24: ARTIST ClusteredMesh::LineIntersectionQueryThis 82BB24A8
    // Remaining Aggregate members at +0x28 onward are not yet recovered here.
};

} // namespace collision
} // namespace rw
