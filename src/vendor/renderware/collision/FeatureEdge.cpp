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
// to portable scalar float maths, preserving the store ORDER and offsets. See
// FeatureEdge.hpp for the layout and the flagged inferred .rdata constants.
// ===========================================================================

namespace rw
{
namespace collision
{

// .rdata constants reached only by reference in the asm (no value in the
// export). FLAGGED as inferred:
//   * unk_8327EEA0 -- the ctor's vcmpgtfp guard against a tiny "degenerate
//     edge" epsilon (lenSq must exceed it for the direction to be valid).
//   * flt_82001CC0 -- the constrain_point start-side threshold (t <= it means
//     the projection falls before the edge start).
const f32 FeatureEdge::KF_DEGENERATE_EPSILON = 1.0e-12f;
const f32 FeatureEdge::KF_START_THRESHOLD    = 0.0f;

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128).
    inline f32 Dot3(const FeatureEdge::Vec4& a, const FeatureEdge::Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
}

// ---------------------------------------------------------------------------
// FeatureEdge::FeatureEdge @ 0x82BA85B8
//
// Stores (in asm store order):
//   stvx128 v13, r0, r3        -> mOrigin = *lpPointA          (this+0x00)
//   ... compute direction ...
//   stvx128 v0,  r3, r11(0x30) -> mExtent = length-guarded     (this+0x30)
//   stvx128 v0,  r3, r10(0x10) -> mUnitDir = (B-A) * 1/len      (this+0x10)
//
// The direction maths mirrors the VMX idiom:
//   delta   = B - A
//   lenSq   = dot3(delta, delta)                 (vmsum3fp128)
//   recip   = 1/sqrt(lenSq) refined once (vrsqrtefp + Newton-Raphson)
//   extent  = lenSq * recip   (== |delta|)       guarded: 0 when lenSq==0,
//                                                clamped via the epsilon mask
//   invLen  = 1/extent refined once (vrefp + Newton-Raphson)
//   unitDir = delta * invLen                     guarded by the same mask
// ---------------------------------------------------------------------------
FeatureEdge::FeatureEdge(const Vec4* lpPointA, const Vec4* lpPointB)
{
    // mOrigin = endpoint A (stored first, this+0x00).
    mOrigin = *lpPointA;

    // delta = B - A
    Vec4 lDelta;
    lDelta.x = lpPointB->x - lpPointA->x;
    lDelta.y = lpPointB->y - lpPointA->y;
    lDelta.z = lpPointB->z - lpPointA->z;
    lDelta.w = lpPointB->w - lpPointA->w;

    const f32 lfLenSq = Dot3(lDelta, lDelta);

    // Degenerate-edge guard: vcmpeqfp (lenSq == 0) and vcmpgtfp (lenSq > eps).
    const bool lbValid = (lfLenSq != 0.0f) && (lfLenSq > KF_DEGENERATE_EPSILON);

    // extent = sqrt(lenSq) (vrsqrtefp + NR refine, then lenSq * recip). Zeroed
    // when the edge is degenerate (the vsel masks).
    const f32 lfExtent = lbValid ? std::sqrt(lfLenSq) : 0.0f;

    // mExtent (this+0x30): xyz hold the extent value broadcast through the
    // VMX lanes; w follows the same select.
    mExtent.x = lfExtent;
    mExtent.y = lfExtent;
    mExtent.z = lfExtent;
    mExtent.w = lfExtent;

    // unitDir = delta / extent (vrefp + NR refine of extent, * delta), zeroed
    // when degenerate.
    const f32 lfInvLen = lbValid ? (1.0f / lfExtent) : 0.0f;
    mUnitDir.x = lDelta.x * lfInvLen;
    mUnitDir.y = lDelta.y * lfInvLen;
    mUnitDir.z = lDelta.z * lfInvLen;
    mUnitDir.w = lDelta.w * lfInvLen;
}

// ---------------------------------------------------------------------------
// FeatureEdge::constrain_point @ 0x82BB7728
//
//   delta = *lpPoint - mOrigin                   (v13 = v13 - v0)
//   t     = dot3(delta, mUnitDir)                (vmsum3fp128 v13,v13,v12)
//   if ( KF_START_THRESHOLD > t )                (vcmpgtfp. v11, threshold, t)
//       result = 1;  *lpPoint = mOrigin          (v0 still holds A; stvx128)
//   else if ( t > mExtent )                      (vcmpgtfp. v10, t, mExtent)
//       result = 3;  *lpPoint = mUnitDir*mOrigin + mExtent   (vmaddfp v0,v12,v0,v11)
//   else
//       result = 2;  *lpPoint = mUnitDir*mOrigin + t-broadcast (vmaddfp v0,v12,v0,v13)
//
// The two fused-multiply-adds reproduce the asm verbatim (vmaddfp vD = vA*vB+vC
// with vA=mUnitDir, vB=mOrigin, vC = mExtent or the dot3 broadcast). FLAGGED:
// the endpoint/segment vmaddfp formula is transcribed from the asm; its
// geometric intent (projection onto the edge) is documented but the literal
// fma is what the binary computes.
// ---------------------------------------------------------------------------
int FeatureEdge::constrain_point(Vec4* lpPoint) const
{
    // delta = point - origin
    Vec4 lDelta;
    lDelta.x = lpPoint->x - mOrigin.x;
    lDelta.y = lpPoint->y - mOrigin.y;
    lDelta.z = lpPoint->z - mOrigin.z;
    lDelta.w = lpPoint->w - mOrigin.w;

    // t = dot3(delta, unitDir), broadcast across lanes (vmsum3fp128).
    const f32 lfT = Dot3(lDelta, mUnitDir);

    int liResult;

    if (KF_START_THRESHOLD > lfT)
    {
        // Before the start: clamp to the origin (v0 is unchanged == mOrigin).
        liResult = 1;
        *lpPoint = mOrigin;
    }
    else if (lfT > mExtent.x)
    {
        // Past the extent: *lpPoint = mUnitDir * mOrigin + mExtent (vmaddfp).
        liResult = 3;
        lpPoint->x = mUnitDir.x * mOrigin.x + mExtent.x;
        lpPoint->y = mUnitDir.y * mOrigin.y + mExtent.y;
        lpPoint->z = mUnitDir.z * mOrigin.z + mExtent.z;
        lpPoint->w = mUnitDir.w * mOrigin.w + mExtent.w;
    }
    else
    {
        // On the segment: *lpPoint = mUnitDir * mOrigin + t (vmaddfp; vC = t
        // broadcast across all four lanes).
        liResult = 2;
        lpPoint->x = mUnitDir.x * mOrigin.x + lfT;
        lpPoint->y = mUnitDir.y * mOrigin.y + lfT;
        lpPoint->z = mUnitDir.z * mOrigin.z + lfT;
        lpPoint->w = mUnitDir.w * mOrigin.w + lfT;
    }

    return liResult;
}

} // namespace collision
} // namespace rw
