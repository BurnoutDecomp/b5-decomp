#pragma once

// Home for BrnDirector::CameraInterpolationController -- the Director's camera-blend /
// rotate-about-pivot interpolation helper. DWARF home (nominal):
// GameSource/Director/Camera/BrnCameraInterpolationController.h.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX @0x821F8220 (Matrix44AffineFromRota),
// semantic-parity (not byte-matching). Called by
// CameraInterpolationController::RotateAboutPivot.
//
// KEYSTONE (VMX): the single ledger function is a fully hand-vectorised AltiVec/VMX matrix
// pipeline -- lvx128 gathers of six 16-byte rows from one source and four from another,
// vspltw lane broadcasts, a chain of vmulfp128 / vmaddfp products, and four stvx128 stores
// of the result rows. It composes a rotation-about-pivot affine from two input matrices and
// a negated scalar at src1+0x60. Per project policy, a multi-stage VMX pipeline is NOT
// paraphrased to scalar (the lane-by-lane formula cannot be recovered store-for-store from
// the splat/madd graph without fabrication), so the body here is an honest documented floor
// rather than a guessed scalar recurrence. The signature + the input/output matrix shape are
// faithful; the math awaits a VMX-lowering pass (or the RW vpu *_operation intrinsics being
// brought in). See BrnCameraInterpolationController.cpp for the asm-level register map.

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44Affine / Matrix33 / Vector4 (VecFloat lane)
#include "BrnCommonTypes.h"      // Matrix33 / VecFloat aliases
#include "GameSource/Director/Camera/Utils/BrnInterpolater.h"   // Camera::Utils::Interpolater

namespace BrnDirector
{
    // BrnDirector::CameraInterpolationController -- the Director's camera-blend helper. Carries
    // two direction-preserving Interpolaters (mInterpolaterA/mInterpolaterB, DWARF h:71/:72);
    // the rotate-about-pivot math helpers below take their matrices by pointer/ref and touch no
    // instance state.
    struct CameraInterpolationController
    {
        // The blend parameter set of the rotate-about-pivot camera interpolation mode
        // (DWARF BrnCameraInterpolationController.h:75 -- the DWARF file attribution for
        // this class is Shots/ShotControllers/; this committed home predates it).
        struct RotateAboutPivotParams
        {
            Matrix33 mLookatLocalRotation;   // h:76 (+0x00)
            Matrix33 mLookAtRotation;        // h:77 (+0x30)
            f32      mfRadius;               // h:79 (+0x60)

            // @0x8221E9D0 (DWARF h:88; static -- the X360 ABI carries no `this`:
            // r3/r4 = the two sources, v1 = t, r5/r6 = the two interpolaters, r7 = out).
            // Blend lhs->rhs: each rotation via its direction-preserving interpolater,
            // the radius linearly. Bodied in BrnCameraInterpolationController.cpp
            // (ledger TU SDKs/.../matrix33_type_inline.h, a DWARF misattribution).
            static void Interpolate(const RotateAboutPivotParams& lrFrom,
                                    const RotateAboutPivotParams& lrTo,
                                    VecFloat lvT,
                                    Camera::Utils::Interpolater& lrInterpolaterA,
                                    Camera::Utils::Interpolater& lrInterpolaterB,
                                    RotateAboutPivotParams* lpOut);
        };

        // @0x821F8220. Build a rotation-about-pivot affine in *lpResult from the interpolated
        // pivot params and the car transform. r3=lpResult (four stvx128 rows @ stride 16),
        // r5=lpParams (the blended RotateAboutPivotParams -- rotation rows at +0x00/+0x10/+0x20/
        // +0x30/+0x50 and the negated radius scalar at +0x60), r6=lpPivot (lvx128 rows at
        // +0x00/+0x10/+0x20/+0x30). Returns lpResult. (DWARF h:109 spells the source param as
        // `const RotateAboutPivotParams&`; VMX keystone -- see the .cpp, body is an honest floor.)
        rw::math::vpu::Matrix44Affine* Matrix44AffineFromRota(
            rw::math::vpu::Matrix44Affine* lpResult,
            const RotateAboutPivotParams* lpParams,
            const rw::math::vpu::Matrix44Affine* lpPivot) const;

        // @0x8223DA28 (DWARF h:98). Blend between two camera transforms (lFrom -> lTo by lvT)
        // about a pivot expressed by lCarTransform: assert lvT in [0,1], move both transforms
        // into car-local space (via the orthonormal inverse of lCarTransform), extract each
        // one's rotate-about-pivot params, blend those (rotations through the two direction-
        // preserving interpolaters, radius linearly), then rebuild a world affine. Returns the
        // blended transform.
        Matrix44Affine RotateAboutPivot(const Matrix44Affine& lCarTransform,
                                        const Matrix44Affine& lFrom,
                                        const Matrix44Affine& lTo,
                                        VecFloat lvT,
                                        Camera::Utils::Interpolater& lrInterpolaterA,
                                        Camera::Utils::Interpolater& lrInterpolaterB);

        // @0x8221EAC0 (DWARF h:104). Extract the rotate-about-pivot params of lIn expressed in
        // car-local space (lInverseCarTransform). DECLARATION-ONLY: the body is a dense hand-
        // vectorised VMX pipeline (a look-at build + Matrix33 transpose/compose + rsqrt
        // normalisation) whose lane routing is not reconstructable store-for-store without
        // guessing -- left as its own ledger target (same floor as Matrix44AffineFromRota).
        void ExtractRotateAboutPivotParams(const Matrix44Affine& lIn,
                                           const Matrix44Affine& lInverseCarTransform,
                                           RotateAboutPivotParams* lpOut);

    private:
        Camera::Utils::Interpolater mInterpolaterA;   // DWARF h:71 (+0x00)
        Camera::Utils::Interpolater mInterpolaterB;   // DWARF h:72 (+0x20)
    };
}
