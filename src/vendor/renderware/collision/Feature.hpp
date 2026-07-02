#pragma once

#include "types.hpp"
#include "vendor/renderware/collision/FeatureEdge.hpp"

// ===========================================================================
// rw::collision::Feature -- a RenderWare collision "feature": the maximal
// contact feature (point / edge / convex face) a GP primitive presents along a
// query direction. A face feature is a convex polygon whose boundary edges
// each carry an extruded prism-wall plane (FeatureEdge::pn).
//
// OWNING HOME for the single Feature function the X360 binary defines:
//     rw::collision::Feature::BuildEdgePlanes  @ 0x82BB9D68
//       (called by rw::collision::GPTriangle::GetMaximumFeature)
//
// CANONICAL SHAPE (supersedes the earlier pad-based reconstruction): the
// Feb-2007 vendor header (rwccore.h:930) and the DecFIGS DWARF (volume.h:202)
// agree:
//     uint32_t region;                       // +0x000
//     FeatureEdge edges[MAXEDGECOUNT];       // +0x010  (MAXEDGECOUNT == 8)
//     rw::math::Vector3 ownNormal;           // +0x210  face plane normal
//     rw::math::Vector3 pt;                  // +0x220  point-feature position
//     int32_t numedges;                      // +0x230
// Every offset is X360-asm-attested across this wave's rw::collision family:
//   * BuildEdgePlanes reads edges[i].dir @+0x20+0x40*i, writes edges[i].pn
//     @+0x30+0x40*i, count @+0x230;
//   * the Find*Prism workers read edges[i].base/dir/pn/length, ownNormal
//     @+0x210, pt @+0x220 and read/modify region @+0x000;
//   * MAXEDGECOUNT == 8 is pinned by ownNormal at 0x10 + 8*0x40 == 0x210.
// The console object is 16-byte aligned (sizeof 0x240) -- the trailing pad
// keeps the PrimitivePairIntersectResult f1/f2 embedding at its 0x240 stride.
// ===========================================================================

namespace rw
{
namespace collision
{

struct Feature
{
    enum
    {
        MAXEDGECOUNT = 8
    };

    // @ 0x82BB9D68 -- for every edge record [0, numedges), set the record's
    // prism-wall plane normal (pn) to the normalised cross product of the
    // record's edge direction with the extrusion direction. abCcw selects the
    // winding: nonzero uses +arExtrusionDir, zero the negated direction.
    // Canonical declaration rwccore.h:960 / DWARF volume.h:283
    // (void BuildEdgePlanes(RwBool, const Vector3&)).
    // (X360 __fastcall: r3=this, r4=abCcw, r5=&arExtrusionDir.)
    void BuildEdgePlanes(RwBool abCcw, const Vec4& arExtrusionDir);

    // .rdata constant reached only by reference in the asm (its address
    // unk_8327EFD0 appears in the body but the export carries no value). It is
    // the degenerate-length-squared epsilon the vcmpgtfp guards against -- the
    // same role as FeatureEdge's KF_DEGENERATE_EPSILON (canonical build_plane
    // guards with VEC_EPSILON). FLAGGED as inferred.
    static const f32 KF_DEGENERATE_EPSILON;   // X360 unk_8327EFD0

    u32         region;                // +0x000  accumulated region/feature id
                                       //         (read-modify-written by the
                                       //         Find*Prism workers)
    u32         mPad004[3];            // +0x004  16-byte alignment of edges[0]
                                       //         (implicit on console)
    FeatureEdge edges[MAXEDGECOUNT];   // +0x010  boundary edges, stride 0x40
    Vec4        ownNormal;             // +0x210  face plane unit normal
    Vec4        pt;                    // +0x220  point-feature position
    s32         numedges;              // +0x230  0 = point, 1 = edge, >1 = face
    u8          mPad234[0xC];          // +0x234  16-byte tail pad (console
                                       //         sizeof/stride is 0x240)
};

static_assert(sizeof(FeatureEdge) == 0x40, "FeatureEdge record stride must be 0x40");
static_assert(sizeof(Feature) == 0x240, "Feature console stride must be 0x240");

} // namespace collision
} // namespace rw
