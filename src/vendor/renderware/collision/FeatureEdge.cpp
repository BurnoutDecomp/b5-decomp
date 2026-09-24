#include "vendor/renderware/collision/FeatureEdge.hpp"

#include <cmath>

// ===========================================================================
// rw::collision::FeatureEdge -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   FeatureEdge::FeatureEdge      @ 0x82BA85B8
//   FeatureEdge::constrain_point  @ 0x82BB7728
//
// The X360 bodies are hand-vectorised VMX/AltiVec. Following the project's
// CgsGeometric::Triangle4 precedent, this is a SEMANTIC reconstruction lowered
// to portable scalar float maths, preserving the store ORDER and offsets. The
// member names/shape are the canonical vendor ones (Feb-2007 rwccore.h:862 /
// DecFIGS DWARF volume.h:53) -- see FeatureEdge.hpp.
// ===========================================================================

namespace rw
{
namespace collision
{

// The two constants, read from the image (crash parity H2-D3, 2026-09-24):
//   * unk_8327EEA0 -- the ctor's degenerate-edge row. A .bss splat written at startup by the
//     CRT dyn-init thunk 0x82C73BD0 (initializer table slot 0x82CD3C3C): lvlx flt_821801B4,
//     vspltw 0, stvx128 -> 0x8327EEA0; x360rd 0x821801B4 = 0x34000000 = 1.1920929e-07 =
//     FLT_EPSILON (canonical rwccore.h:876 guards with VEC_EPSILON = rw::math::EPSILON). The
//     ctor's vcmpgtfp (0x82BA8608, lenSq > row) drives both the length and the dir vsel's.
//     It was an inferred 1.0e-12f, which kept a direction and length for edges of lenSq in
//     (1e-12, 1.19e-7] (about 1e-6 .. 3.45e-4 m) that the console zeroes.
//   * flt_82001CC0 -- the constrain_point start-side threshold: x360rd 0x00000000 = 0.0
//     (canonical compares t < 0.0f).
const f32 FeatureEdge::KF_DEGENERATE_EPSILON = 1.1920929e-07f;
const f32 FeatureEdge::KF_START_THRESHOLD    = 0.0f;

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128).
    inline f32 Dot3(const Vec4& a, const Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
}

// ---------------------------------------------------------------------------
// FeatureEdge::FeatureEdge @ 0x82BA85B8
//
// Stores (in asm store order):
//   stvx128 v13, r0, r3        -> base = arP1                  (this+0x00)
//   ... compute direction ...
//   stvx128 v0,  r3, r11(0x30) -> length = length-guarded      (this+0x30)
//   stvx128 v0,  r3, r10(0x10) -> dir = (P2-P1) * 1/len        (this+0x10)
// (pn @+0x20 is NOT written by the ctor; Feature::BuildEdgePlanes fills it.)
//
// The direction maths mirrors the VMX idiom (canonical rwccore.h:869-880):
//   delta   = P2 - P1
//   lenSq   = dot3(delta, delta)                 (vmsum3fp128)
//   recip   = 1/sqrt(lenSq) refined once (vrsqrtefp + Newton-Raphson)
//   length  = lenSq * recip   (== |delta|)       guarded: 0 when degenerate
//   invLen  = 1/length refined once (vrefp + Newton-Raphson)
//   dir     = delta * invLen                     guarded by the same mask
// ---------------------------------------------------------------------------
FeatureEdge::FeatureEdge(const Vec4& arP1, const Vec4& arP2)
{
    // base = endpoint P1 (stored first, this+0x00).
    base = arP1;

    // delta = P2 - P1
    Vec4 lDelta;
    lDelta.x = arP2.x - arP1.x;
    lDelta.y = arP2.y - arP1.y;
    lDelta.z = arP2.z - arP1.z;
    lDelta.w = arP2.w - arP1.w;

    const f32 lfLenSq = Dot3(lDelta, lDelta);

    // Degenerate-edge guard: vcmpeqfp (lenSq == 0) and vcmpgtfp (lenSq > eps).
    const bool lbValid = (lfLenSq != 0.0f) && (lfLenSq > KF_DEGENERATE_EPSILON);

    // length = sqrt(lenSq) (vrsqrtefp + NR refine, then lenSq * recip). Zeroed
    // when the edge is degenerate (the vsel masks).
    const f32 lfLength = lbValid ? std::sqrt(lfLenSq) : 0.0f;

    // length (this+0x30): the value broadcast through the VMX lanes.
    length.x = lfLength;
    length.y = lfLength;
    length.z = lfLength;
    length.w = lfLength;

    // dir = delta / length (vrefp + NR refine of length, * delta), zeroed
    // when degenerate.
    const f32 lfInvLen = lbValid ? (1.0f / lfLength) : 0.0f;
    dir.x = lDelta.x * lfInvLen;
    dir.y = lDelta.y * lfInvLen;
    dir.z = lDelta.z * lfInvLen;
    dir.w = lDelta.w * lfInvLen;
}

// ---------------------------------------------------------------------------
// FeatureEdge::constrain_point @ 0x82BB7728
//
//   delta = arPoint - base                       (v13 = v13 - v0)
//   t     = dot3(delta, dir)                     (vmsum3fp128 v13,v13,v12)
//   if ( KF_START_THRESHOLD > t )                (vcmpgtfp. v11, threshold, t)
//       result = 1;  arPoint = base              (v0 still holds base; stvx128)
//   else if ( t > length )                       (vcmpgtfp. v10, t, length)
//       result = 3;  arPoint = base + dir*length (vmaddfp v0,v12,v0,v11)
//   else
//       result = 2;  arPoint = base + dir*t      (vmaddfp v0,v12,v0,v13)
//
// vmaddfp vD,vA,vB,vC computes vD = vA*vC + vB (vB, the SECOND displayed
// operand, is the addend) -- the operand rule attested by the Newton-Raphson
// idioms elsewhere in this TU -- so with vA=dir(v12), vB=base(v0) and
// vC=length/t-broadcast these are exactly the canonical endpoint()/segment
// projections of rwccore.h:912-929. (This corrects the earlier transcription
// that read the addend as the third operand.)
// ---------------------------------------------------------------------------
u32 FeatureEdge::constrain_point(Vec4& arPoint) const
{
    // delta = point - base
    Vec4 lDelta;
    lDelta.x = arPoint.x - base.x;
    lDelta.y = arPoint.y - base.y;
    lDelta.z = arPoint.z - base.z;
    lDelta.w = arPoint.w - base.w;

    // t = dot3(delta, dir), broadcast across lanes (vmsum3fp128).
    const f32 lfT = Dot3(lDelta, dir);

    if (KF_START_THRESHOLD > lfT)
    {
        // Before the base: clamp to the base vertex (v0 unchanged == base).
        arPoint = base;
        return 1;
    }

    if (lfT > length.x)
    {
        // Past the length: arPoint = base + dir*length (the endpoint;
        // vmaddfp per lane, w included -- length is lane-broadcast).
        arPoint.x = dir.x * length.x + base.x;
        arPoint.y = dir.y * length.y + base.y;
        arPoint.z = dir.z * length.z + base.z;
        arPoint.w = dir.w * length.w + base.w;
        return 3;
    }

    // On the segment: arPoint = base + dir*t (vmaddfp; vC = the t broadcast).
    arPoint.x = dir.x * lfT + base.x;
    arPoint.y = dir.y * lfT + base.y;
    arPoint.z = dir.z * lfT + base.z;
    arPoint.w = dir.w * lfT + base.w;
    return 2;
}

} // namespace collision
} // namespace rw
