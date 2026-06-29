#pragma once

#include "types.hpp"

// ===========================================================================
// rw::collision::Feature -- a RenderWare collision "feature" (the contact /
// closest-feature record built by the GPTriangle queries). It owns a run of
// per-edge records; BuildEdgePlanes fills each record's plane normal from the
// record's edge direction crossed with a supplied reference normal.
//
// OWNING HOME for the single Feature function the X360 binary defines:
//     rw::collision::Feature::BuildEdgePlanes  @ 0x82BB9D68
//       (called by rw::collision::GPTriangle::GetMaximumFeature)
//
// No Feb-2007 leak source and no DWARF exist for this TU, so the LAYOUT below is
// reconstructed purely from the X360 VMX/AltiVec asm of BuildEdgePlanes. The asm
// proves only three things about the object:
//   * the edge count is read from this+0x230 (lwz r11, 0x230(r3));
//   * the record stride is 0x40 (addi r11, r11, 0x40 each iteration);
//   * within record i the SOURCE edge direction is loaded from this+0x20+0x40*i
//     (lvx128 v?, r11, -0x10 with r11 = this+0x30+0x40*i) and the OUTPUT plane
//     normal is stored to this+0x30+0x40*i (stvx128 to r11).
// Everything else in the object (this+0x00..0x1F, the rest of each 0x40-byte
// record, and the slot between the array and the count) is not touched by this
// function; it is modelled as opaque padding sized only to pin the two offsets
// the asm actually uses. FLAGGED: the record CAPACITY (8) is a layout slot count
// chosen only to place mi32EdgePlaneCount at exactly +0x230 -- the real maximum
// is unknown from this one function.
//
// The body is hand-vectorised VMX (lvx128 / vpermwi128 / vmulfp128 / vnmsubfp /
// vmsum3fp128 / vrsqrtefp + one Newton-Raphson refine / vcmpgtfp / vsel) so --
// following the rw::collision::FeatureEdge and CgsGeometric::AxisAlignedBox
// precedents in this directory -- it is lowered to a SEMANTIC, portable,
// named-member float reconstruction rather than an inline-asm transcription. The
// store offset and store order are preserved exactly.
// ===========================================================================

namespace rw
{
namespace collision
{

class Feature
{
public:
    // A 4-lane (xyzw) vector row, matching the 16-byte VMX register lanes the
    // asm loads/stores. Only xyz participate in the cross product / dot3; the w
    // lane falls out of the same arithmetic (it evaluates to 0 for the cross).
    struct Vec4
    {
        f32 x;
        f32 y;
        f32 z;
        f32 w;
    };

    // One 0x40-byte edge record. The asm only reaches two of its rows:
    //   +0x00  mEdgeDir      the input edge direction (lvx128 from this+0x20+..)
    //   +0x10  mPlaneNormal  the output, written by BuildEdgePlanes
    // The remaining 0x20 bytes are not touched here; carried as opaque padding.
    struct EdgePlaneRecord
    {
        Vec4 mEdgeDir;       // +0x00 within the record
        Vec4 mPlaneNormal;   // +0x10 within the record (BuildEdgePlanes output)
        Vec4 mPad20;         // +0x20 within the record (untouched)
        Vec4 mPad30;         // +0x30 within the record (untouched)
    };

    // @ 0x82BB9D68 -- for every edge record [0, mi32EdgePlaneCount), set the
    // record's plane normal to the normalised cross product of the record's edge
    // direction with the reference normal *lpReferenceNormal. abFlip selects the
    // winding: when true the normal is mEdgeDir x N, when false it is
    // mEdgeDir x (-N) (i.e. negated). Returns this (the X360 body returns r3).
    // (X360 __fastcall: r3=this, r4=abFlip, r5=lpReferenceNormal.)
    Feature* BuildEdgePlanes(int abFlip, const Vec4* lpReferenceNormal);

    // .rdata constant reached only by reference in the asm (its address
    // unk_8327EFD0 appears in the body but the export carries no value). It is
    // the degenerate-length-squared epsilon the vcmpgtfp guards against -- the
    // same role as FeatureEdge's KF_DEGENERATE_EPSILON, so the same inferred
    // value is used. FLAGGED as inferred.
    static const f32 KF_DEGENERATE_EPSILON;   // X360 unk_8327EFD0

    Vec4            mPad00[2];           // +0x00  untouched by this function
    EdgePlaneRecord mEdgePlanes[8];      // +0x20  edge-plane records (stride 0x40)
    u8              mPad220[0x10];       // +0x220 untouched gap before the count
    s32             mi32EdgePlaneCount;  // +0x230 number of valid edge records
};

} // namespace collision
} // namespace rw
