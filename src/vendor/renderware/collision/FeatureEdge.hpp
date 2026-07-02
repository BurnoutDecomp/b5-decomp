#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::FeatureEdge -- a RenderWare collision "feature edge": one
// boundary edge of a contact feature (base vertex, unit direction, the edge's
// extruded prism-wall plane normal, and the edge length).
//
// OWNING HOME for the two FeatureEdge functions the X360 binary defines:
//     rw::collision::FeatureEdge::FeatureEdge      @ 0x82BA85B8  (ctor)
//     rw::collision::FeatureEdge::constrain_point  @ 0x82BB7728
//
// CANONICAL SHAPE (supersedes the earlier pad-based reconstruction): the
// Feb-2007 vendor header (references/Feb-2007/BrnEntityModuleUnity/EARenderWare/
// stable/platform/include/native/ps3-ppu-gcc/rwccore.h:862) and the DecFIGS
// DWARF (SDKs/EATech/include/cmn/rw/collision/volume.h:53) agree on the member
// set and names:
//     rw::math::Vector3 base, dir, pn;    // +0x00 / +0x10 / +0x20
//     rw::math::VecFloat length;          // +0x30 (broadcast across the lanes)
// All offsets are X360-asm-attested (the ctor stores base @+0x00, length
// @+0x30, dir @+0x10; Feature::BuildEdgePlanes writes pn @+0x20 of each
// record; the Find*Prism family reads all four rows). The 16-byte rows are
// carried as the scalar 4-lane Vec4 below per this directory's precedent.
//
// constrain_point() projects a query point onto the edge segment and writes
// the constrained point back, returning a region code:
//     1  the projection falls before the base   (pt = base)
//     3  the projection falls past `length`     (pt = base + dir*length)
//     2  the projection lies on the segment     (pt = base + dir*t)
// (Canonical body rwccore.h:912-929; the X360 asm's two vmaddfp's are exactly
// base + dir*length and base + dir*t -- vmaddfp vD,vA,vB,vC computes
// vD = vA*vC + vB with vA=dir, vB=base, the operand rule attested at three
// independent sites of this TU's asm.)
//
// .rdata constants recovered only by reference (their addresses appear in the
// asm but the export carries no value) are flagged as inferred; the canonical
// source uses VEC_EPSILON (rw::math::EPSILON) for the ctor guard.
// ===========================================================================

namespace rw
{
namespace collision
{

// The 4-lane (xyzw) scalar stand-in for a 16-byte VMX register row
// (rw::math::Vector3 / rw::math::VecFloat in the canonical vendor source) --
// the shared vector vocabulary of the rw::collision reconstruction in this
// directory. Only xyz participate in the dot3 folds; w is carried through the
// per-lane arithmetic exactly like the hardware lane.
struct Vec4
{
    f32 x;
    f32 y;
    f32 z;
    f32 w;
};

// RenderWare boolean (canonical RwBool; the vendor typedef is a 32-bit int).
typedef s32 RwBool;

class FeatureEdge
{
public:
    // Canonical default ctor (rwccore.h:866) -- members left uninitialised;
    // needed by Feature's edges[8] array and the batch kernels' stack Features.
    FeatureEdge() {}

    // @ 0x82BA85B8 -- construct the edge from endpoints arP1 / arP2
    // (canonical rwccore.h:869). X360 __fastcall: r3=this, r4=&arP1, r5=&arP2.
    FeatureEdge(const Vec4& arP1, const Vec4& arP2);

    // @ 0x82BB7728 -- constrain arPoint onto this edge segment in place;
    // returns the region code (1 = before base, 2 = on segment, 3 = past the
    // length). Canonical rwccore.h:912 (uint32_t return).
    u32 constrain_point(Vec4& arPoint) const;

    // .rdata constants recovered only by reference (flagged as inferred).
    static const f32 KF_DEGENERATE_EPSILON;   // X360 unk_8327EEA0 (ctor guard)
    static const f32 KF_START_THRESHOLD;      // X360 flt_82001CC0 (constrain)

    Vec4 base;     // +0x00  edge base vertex (endpoint A)
    Vec4 dir;      // +0x10  unit direction (B - A)/|B - A|
    Vec4 pn;       // +0x20  extruded prism-wall plane normal
                   //        (written by Feature::BuildEdgePlanes @ 0x82BB9D68)
    Vec4 length;   // +0x30  edge length (VecFloat: broadcast across the lanes)
};

} // namespace collision
} // namespace rw
