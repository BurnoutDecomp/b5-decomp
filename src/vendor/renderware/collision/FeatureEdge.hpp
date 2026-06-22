#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::FeatureEdge -- a RenderWare collision "feature edge".
//
// OWNING HOME for the two FeatureEdge functions the X360 binary defines:
//     rw::collision::FeatureEdge::FeatureEdge      @ 0x82BA85B8  (ctor)
//     rw::collision::FeatureEdge::constrain_point  @ 0x82BB7728
//
// No Feb-2007 leak source and no DWARF exist for this TU, so the LAYOUT below is
// reconstructed from the X360 VMX/AltiVec asm. The bodies are hand-vectorised
// (lvx128 / vsubfp / vmsum3fp128 / vrsqrtefp + Newton-Raphson refine / vsel /
// vcmpgtfp.) so -- following the CgsGeometric::Triangle4 precedent in this
// project -- they are lowered to a SEMANTIC, portable, named-member float
// reconstruction rather than a non-portable inline-asm transcription. The
// store ORDER and the offsets each store lands at are preserved exactly.
//
// A FeatureEdge is built from two endpoints A and B. The asm stores three
// 16-byte (xyzw) rows into the object:
//   +0x00  mOrigin      : endpoint A, copied verbatim from the first argument.
//   +0x10  mUnitDir     : (B - A) scaled by the reciprocal length of (B - A),
//                         i.e. the unit direction, with a Newton-Raphson refined
//                         vrsqrtefp/vrefp reciprocal. Guarded to zero when the
//                         edge is degenerate (vsel against the lenSq==0 mask).
//   +0x30  mExtent      : the edge length value (lenSq * 1/len, refined),
//                         guarded against an .rdata epsilon (X360 unk_8327EEA0,
//                         not valued in the export -- see KF_DEGENERATE_EPSILON).
//
// constrain_point() projects a query point onto the edge and writes the
// constrained point back, returning a region code:
//     1  the projection falls before the start  (t <= start threshold)
//     3  the projection falls past mExtent       (t  >  mExtent)
//     2  the projection lies on the segment
// The start threshold is an .rdata float (X360 flt_82001CC0, not valued in the
// export -- see KF_START_THRESHOLD). FLAGGED: the two .rdata constants and the
// precise region-3 endpoint formula are inferred from the asm shape.
// ===========================================================================

namespace rw
{
namespace collision
{

class FeatureEdge
{
public:
    // A 4-lane (xyzw) vector row, matching the 16-byte VMX register lanes the
    // asm loads/stores. Only xyz participate in the dot3 maths; w is carried.
    struct Vec4
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    // @ 0x82BA85B8 -- construct the edge from endpoints lpPointA / lpPointB.
    // (X360 __fastcall: r3=this, r4=lpPointA, r5=lpPointB.)
    FeatureEdge(const Vec4* lpPointA, const Vec4* lpPointB);

    // @ 0x82BB7728 -- constrain lpPoint onto this edge in place; returns the
    // region code (1 = before start, 2 = on segment, 3 = past extent).
    int constrain_point(Vec4* lpPoint) const;

    // .rdata constants recovered only by reference (their addresses appear in
    // the asm but the export carries no value). Flagged as inferred.
    static const f32 KF_DEGENERATE_EPSILON;   // X360 unk_8327EEA0 (ctor guard)
    static const f32 KF_START_THRESHOLD;      // X360 flt_82001CC0 (constrain)

    Vec4 mOrigin;    // +0x00  endpoint A
    Vec4 mUnitDir;   // +0x10  unit direction (B - A)/|B - A|
    Vec4 mPad20;     // +0x20  not written by these two functions; layout slot
    Vec4 mExtent;    // +0x30  edge length value (guarded)
};

} // namespace collision
} // namespace rw
