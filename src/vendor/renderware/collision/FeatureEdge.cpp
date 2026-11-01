#include "vendor/renderware/collision/FeatureEdge.hpp"

#include <cmath>
#include <cfloat>

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
// volume.h:37; ARTIST 82C73BD0 initializes 8327EEA0 from 0x34000000.
const rw::math::vpu::VecFloat VEC_EPSILON(FLT_EPSILON);
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
FeatureEdge::FeatureEdge(const rw::math::vpu::Vector3& arP1, const rw::math::vpu::Vector3& arP2)
{
    base = arP1;
    const __vector4 zero = __vspltisw(0);
    const __vector4 one = __vcfsx(__vspltisw(1), 0);
    const __vector4 half = __vcfsx(__vspltisw(1), 1);
    const __vector4 delta = __vsubfp(arP2.mV, arP1.mV);
    const __vector4 square = __vmsum3fp(delta, delta);
    const __vector4 estimate = __vrsqrtefp(square);
    const __vector4 equalZero = __vcmpeqfp(zero, square);
    const __vector4 valid = __vcmpgtfp(square, VEC_EPSILON.mV);
    const __vector4 squareEstimate = __vmulfp(estimate, estimate);
    const __vector4 halfEstimate = __vmulfp(estimate, half);
    const __vector4 correction = __vnmsubfp(square, squareEstimate, one);
    const __vector4 refined = __vmaddfp(halfEstimate, correction, estimate);
    __vector4 distance = __vmulfp(square, refined);
    distance = __vsel(distance, zero, equalZero);
    distance = __vsel(zero, distance, valid);
    length.mV = distance;
    const __vector4 reciprocal = __vrefp(distance);
    const __vector4 reciprocalCorrection = __vnmsubfp(reciprocal, distance, one);
    const __vector4 inverse = __vmaddfp(reciprocal, reciprocalCorrection, reciprocal);
    dir.mV = __vsel(zero, __vmulfp(delta, inverse), valid);
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
u32 FeatureEdge::constrain_point(rw::math::vpu::Vector3& arPoint) const
{
    const __vector4 delta = __vsubfp(arPoint.mV, base.mV);
    const __vector4 projection = __vmsum3fp(delta, dir.mV);
    const __vector4 zero = __vspltisw(0);
    unsigned int comparison = 0;
    __vcmpgtfpR(zero, projection, &comparison);
    if ((comparison & 0x80u) != 0)
    {
        arPoint = base;
        return 1;
    }
    __vcmpgtfpR(projection, length.mV, &comparison);
    if ((comparison & 0x80u) != 0)
    {
        arPoint.mV = __vmaddfp(dir.mV, length.mV, base.mV);
        return 3;
    }
    arPoint.mV = __vmaddfp(dir.mV, projection, base.mV);
    return 2;
}

} // namespace collision
} // namespace rw
