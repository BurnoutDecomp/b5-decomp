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
    // BrnDirector::CameraInterpolationController -- stateless math helper (the bodied
    // function takes its matrices by pointer; no instance members are touched by it).
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

        // @0x821F8220. Build a rotation-about-pivot affine in *lpResult from the two source
        // matrices. r3=lpResult (four stvx128 rows @ stride 16), r5=lpSource (lvx128 rows at
        // +0x00/+0x10/+0x20/+0x30/+0x50/+0x60 -- the +0x60 scalar is negated into the build),
        // r6=lpPivot (lvx128 rows at +0x00/+0x10/+0x20/+0x30). Returns lpResult.
        rw::math::vpu::Matrix44Affine* Matrix44AffineFromRota(
            rw::math::vpu::Matrix44Affine* lpResult,
            const rw::math::vpu::Matrix44Affine* lpSource,
            const rw::math::vpu::Matrix44Affine* lpPivot) const;
    };
}
