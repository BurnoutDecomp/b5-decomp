// BrnDirector::CameraInterpolationController -- camera-blend / rotate-about-pivot helper.
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x821F8220, semantic-parity (not byte-matching).
//
// Ledger function (1):
//   CameraInterpolationController::Matrix44AffineFromRota   @0x821F8220   [VMX KEYSTONE]
//
// ===========================================================================================
// VMX KEYSTONE -- DOCUMENTED FLOOR, body intentionally NOT fabricated.
// ===========================================================================================
// The X360 body is a dense hand-vectorised AltiVec/VMX matrix pipeline. Register map from the
// asm (r3 = result, r5 = source, r6 = pivot):
//
//   lfs/fneg/stfs : negate *(r5 + 0x60) into a stack scratch (v11 lane 0 below).
//   lvx128 v11 <- r5+0x00 ; vspltw v4=v11.x, v3=v11.y, v28=v11.z       (source row 0 splats)
//   lvx128 v8  <- r5+0x30 ; v6 <- r5+0x30+0x20 ; v7 <- r5+0x30+0x10    (source rows 3..)
//   lvx128 v9  <- r5+0x10 ; v10 <- r5+0x20 ; v5 <- r5+0x50            (more source rows)
//   lvx128 v0  <- r6+0x00 ; v13 <- r6+0x10 ; v12 <- r6+0x20 ; v3 <- r6+0x30  (pivot rows)
//   lvx128 v29 <- scratch ; vspltw v11=v29.x                          (the negated scalar)
//   then a chain of vmulfp128 / vmaddfp accumulating per-row products with lane splats
//   (vspltw) of the intermediates, ending in four stvx128 of v10/v13/v9/v0 to r3 rows
//   @ +0x00/+0x10/+0x20/+0x30.
//
// This is a multi-stage splat/multiply-add graph (not a simple per-lane copy). Per project
// policy a VMX pipeline of this shape is not paraphrased to a scalar matrix product: the
// exact lane routing (which intermediate splat feeds which madd term, and how the +0x60
// scalar weights the fourth term) cannot be reproduced store-for-store from the pseudocode
// without guessing, and a wrong scalar body would be worse than an honest floor. The body
// below therefore leaves the result matrix untouched and is flagged in the group result as a
// VMX keystone awaiting a VMX-lowering pass (or the RW vpu *_operation intrinsics).
//
// The signature, the matrix-in/matrix-out shape, the caller (RotateAboutPivot) and the
// register-level map above are faithful and load-bearing for whoever lowers the math.

#include "GameSource/Director/Camera/BrnCameraInterpolationController.h"

namespace BrnDirector
{
    // @0x821F8220. VMX KEYSTONE -- see the file header. Honest floor: the rotate-about-pivot
    // matrix composition is not reconstructed (would require fabricating the VMX lane graph);
    // the result is returned unmodified so no incorrect transform is asserted as X360 fact.
    rw::math::vpu::Matrix44Affine* CameraInterpolationController::Matrix44AffineFromRota(
        rw::math::vpu::Matrix44Affine* lpResult,
        const rw::math::vpu::Matrix44Affine* lpSource,
        const rw::math::vpu::Matrix44Affine* lpPivot) const
    {
        (void)lpSource;
        (void)lpPivot;
        // [VMX KEYSTONE -- UNRECONSTRUCTED] no fabricated arithmetic; *lpResult unchanged.
        return lpResult;
    }

    // ------------------------------------------------------------------------------------
    // RotateAboutPivotParams::Interpolate @0x8221E9D0 (ledger TU SDKs/EATech/include/rw/
    // math/vpu/detail/matrix33_type_inline.h -- a DWARF file-attribution artifact; this is
    // the class home). Reconstructed from the X360 asm:
    //   DirectionPreservingSLerp(&out33, &lrFrom+0x00, &lrTo+0x00, &iA+0x00, &iA+0x10) v1=t
    //     -> copy 3 rows to lpOut+0x00        (mLookatLocalRotation)
    //   DirectionPreservingSLerp(&out33, &lrFrom+0x30, &lrTo+0x30, &iB+0x00, &iB+0x10) v1=t
    //     -> copy 3 rows to lpOut+0x30        (mLookAtRotation)
    //   lpOut+0x60 = from.mfRadius + (to.mfRadius - from.mfRadius) * t  (vsubfp/vmaddfp)
    // The X360 DirectionPreservingSLerp helper receives the interpolater's two members
    // (&mLastAxis, &mbWasInvertedLastTime) split out -- that is the inlined shell of
    // Camera::Utils::Interpolater::Interpolate(Matrix33, Matrix33, VecFloat) (DWARF
    // BrnInterpolater.h:57), expressed here through that named member.
    // ------------------------------------------------------------------------------------
    void CameraInterpolationController::RotateAboutPivotParams::Interpolate(
        const RotateAboutPivotParams& lrFrom, const RotateAboutPivotParams& lrTo,
        VecFloat lvT,
        Camera::Utils::Interpolater& lrInterpolaterA,
        Camera::Utils::Interpolater& lrInterpolaterB,
        RotateAboutPivotParams* lpOut)
    {
        lpOut->mLookatLocalRotation =
            lrInterpolaterA.Interpolate(lrFrom.mLookatLocalRotation, lrTo.mLookatLocalRotation, lvT);
        lpOut->mLookAtRotation =
            lrInterpolaterB.Interpolate(lrFrom.mLookAtRotation, lrTo.mLookAtRotation, lvT);

        // Radius: per-lane linear blend (the asm's vsubfp + vmaddfp on the splatted lane).
        lpOut->mfRadius = lrFrom.mfRadius + (lrTo.mfRadius - lrFrom.mfRadius) * lvT.x;
    }
}
