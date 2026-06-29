#include "vendor/renderware/collision/Feature.hpp"

#include <cmath>

// ===========================================================================
// rw::collision::Feature -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
//   Feature::BuildEdgePlanes  @ 0x82BB9D68
//
// The X360 body is hand-vectorised VMX/AltiVec. Following the sibling
// rw::collision::FeatureEdge precedent in this directory, this is a SEMANTIC
// reconstruction lowered to portable scalar float maths, preserving the per-
// record store offset and the store order. See Feature.hpp for the layout and
// the flagged inferred .rdata epsilon.
// ===========================================================================

namespace rw
{
namespace collision
{

// .rdata constant reached only by reference in the asm (no value in the export).
// FLAGGED as inferred: unk_8327EFD0 is the vcmpgtfp degenerate-edge guard -- the
// squared cross-product length must exceed it for the normal to be valid. Same
// role and inferred value as FeatureEdge::KF_DEGENERATE_EPSILON.
const f32 Feature::KF_DEGENERATE_EPSILON = 1.0e-12f;

namespace
{
    // dot3 of the xyz lanes (the asm's vmsum3fp128).
    inline f32 Dot3(const Feature::Vec4& a, const Feature::Vec4& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    // Cross product e x n (the VMX two-permute / vnmsubfp idiom in the asm).
    //   v0 = e * perm_yzx(n)
    //   v0 = e * perm_yzx(n) - perm_yzx(e) * n        (vnmsubfp: -(vA*vC)+vB)
    //   v0 = perm_yzx(v0)
    // which evaluates lane-for-lane to the cross product e x n. The w lane
    // cancels to 0 (e.w*n.w - e.w*n.w), exactly as the asm produces.
    inline Feature::Vec4 Cross(const Feature::Vec4& e, const Feature::Vec4& n)
    {
        Feature::Vec4 lCross;
        lCross.x = e.y * n.z - e.z * n.y;
        lCross.y = e.z * n.x - e.x * n.z;
        lCross.z = e.x * n.y - e.y * n.x;
        lCross.w = e.w * n.w - e.w * n.w;   // == 0 (carried for store fidelity)
        return lCross;
    }
}

// ---------------------------------------------------------------------------
// Feature::BuildEdgePlanes @ 0x82BB9D68
//
// For each of the *(this+0x230) edge records, compute the edge plane normal as
// the normalised cross product of the record's edge direction (this+0x20+0x40*i)
// with the reference normal at lpReferenceNormal (r5, re-loaded each iteration),
// and store it back at this+0x30+0x40*i. abFlip (r4) selects the winding: the
// asm has two structurally identical loops -- the abFlip==0 loop first negates
// the reference normal (vxor against the 0x80000000 sign mask built by
// vslw -1,-1) before the cross product, the abFlip!=0 loop uses it as-is.
//
// Per record (matching the asm op-for-op):
//   cross  = edgeDir x N          (N negated when !abFlip)
//   lenSq  = dot3(cross, cross)   (vmsum3fp128)
//   guard  = lenSq > epsilon      (vcmpgtfp against unk_8327EFD0)
//   invLen = guard ? 1/sqrt(lenSq) : 0   (vrsqrtefp + one Newton-Raphson refine,
//                                         then vsel against the guard mask)
//   normal = cross * invLen       (vmulfp128) -> stored at +0x30
// The asm stores the raw cross to the +0x30 slot first (stvx128) and then
// overwrites it with the normalised normal; only the final value is meaningful,
// so a single assignment is reconstructed.
// ---------------------------------------------------------------------------
Feature* Feature::BuildEdgePlanes(int abFlip, const Vec4* lpReferenceNormal)
{
    const s32 liCount = mi32EdgePlaneCount;   // lwz r11, 0x230(r3)

    for (s32 li = 0; li < liCount; ++li)
    {
        // Reference normal, re-loaded each iteration (lvx128 v0, r0, r5).
        Vec4 lNormal = *lpReferenceNormal;

        // abFlip==0 negates the reference normal (vxor against the sign mask).
        if (!abFlip)
        {
            lNormal.x = -lNormal.x;
            lNormal.y = -lNormal.y;
            lNormal.z = -lNormal.z;
            lNormal.w = -lNormal.w;
        }

        EdgePlaneRecord& lRecord = mEdgePlanes[li];

        // cross = edgeDir x N.
        const Vec4 lCross = Cross(lRecord.mEdgeDir, lNormal);

        // lenSq = dot3(cross, cross), guarded against the degenerate epsilon.
        const f32 lfLenSq = Dot3(lCross, lCross);
        const bool lbValid = (lfLenSq > KF_DEGENERATE_EPSILON);

        // invLen = 1/sqrt(lenSq) (vrsqrtefp + Newton-Raphson refine), zeroed when
        // the cross is degenerate (the vsel masks).
        const f32 lfInvLen = lbValid ? (1.0f / std::sqrt(lfLenSq)) : 0.0f;

        // normal = cross * invLen, stored at the record's +0x10 plane slot.
        lRecord.mPlaneNormal.x = lCross.x * lfInvLen;
        lRecord.mPlaneNormal.y = lCross.y * lfInvLen;
        lRecord.mPlaneNormal.z = lCross.z * lfInvLen;
        lRecord.mPlaneNormal.w = lCross.w * lfInvLen;
    }

    return this;   // X360 body returns r3 unchanged
}

} // namespace collision
} // namespace rw
