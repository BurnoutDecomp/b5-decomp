// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraRigConstruct.cpp
//
// BrnDirector::Camera::Utils::CameraRig::Construct @0x8220B0E8 (DWARF BrnCameraRig.cpp:31). [FX-DIRECTOR2
// 2026-09-25] A partfile of BrnCameraRig.cpp -- that TU carries the three Params::Serialise<S> visitors and is
// not mounted; this body is split out so the rig behaviour can be mounted without them (the same split
// BrnCameraShakeUpdate.cpp is for CameraShake).
//
// WHAT IT BUILDS: the camera's transform RELATIVE TO THE TARGET CAR (BehaviourRig::Update composes it with the
// car's world transform every frame), from an authored preset (BrnCameraRigParams.cpp), the car's bounding box
// and a mirror flag:
//   lSize        = box.max - box.min                                               (:33)
//   lTarget      = offsetFromTarget * lSize + (box.min + lSize * 0.5)               -- the look-at point, in
//                  units of the car's own size, about the box centre
//   lRotation    = Rz(roll) * Rx(pitch) * Ry(yaw), degrees -> radians               (:39)
//   the rig      = the rotation, with the camera offsetFromRotationCentre along it, stretched by the car's size
//                  per world axis and moved to lTarget:
//                      rows = lRotation
//                      pos  = lTarget + lSize * (offsetFromRotationCentre * lRotation)
//   lbReverse    = mirror the whole rig in X (M -> S M S, S = diag(-1, 1, 1)).
//
// THE CONSOLE BODY is 732 instructions of VMX with no call in it -- every helper is inlined. It was read with
// the help of a small PPC/VMX128 emulator run on the raw words (FX-DIRECTOR2 scratch), which is also where the
// regression test's reference outputs come from (run_fxdirector2_camera_rig.py). The shape:
//   0x8220B0F4..0x8220B208  mfFOV (`stfs f12, 0x40(r3)`); lSize (vsubfp); the centre (`vmaddfp` size * 0.5
//                           (flt_82001DA0) + min, fused); lTarget (vmulfp128 offsetFromTarget * lSize, vaddfp128);
//                           and the identity rows of the locals (the DWARF's lOffsetFromCar :38,
//                           lRotation :39, lOffsetFromRotationCentre :40)
//   0x8220B164 / 0x8220B4C4 / 0x8220B638  the three angles, each `fmuls` by flt_82001744 (0.017453292), in the
//                           order yaw (+0x2C), pitch (+0x28), roll (+0x24)
//   three inlined Matrix44AffineFromAxisRotationAngle -- about gJVector (0x82181510) for yaw, gIVector
//                           (0x82181500) for pitch and gKVector (0x82181520) for roll: an inlined XMVectorSinCos
//                           each (SDKs/XboxMath/XMVectorSinCos.h), then `t = 1 - c` and the Rodrigues terms
//                           (Utils/BrnConsoleVpu.h), packed with vperm (control 0x82CDA350) / vrlimi128
//   0x8220B6A0..0x8220B704  Rx x Ry and 0x8220B8A4..0x8220B97C Rz x that -- each the SDK Mult's vmaddfp cascade
//   0x8220B98C..0x8220BB34  the composition, stored into this (+0x00..+0x30) in four passes: the identity, then
//                           x lRotation, then lOffsetFromRotationCentre x (its w row, offsetFromRotationCentre,
//                           rotated), then the w row scaled by lSize (`vmulfp128 v11, v8, v29`) and x lOffsetFromCar
//                           (its w row, lTarget, added)
//   0x8220BB38..0x8220BC50  lbReverse (`clrlwi r7, r6, 24 ; cmplwi`): five lane negations, each a multiply by
//                           -1.0 (flt_820037C8) inserted with vrlimi128 -- xAxis.y, xAxis.z, yAxis.x, zAxis.x,
//                           wAxis.x, in that order.
// The DWARF's inline list agrees: SetIdentity, Pos, Vector3::Set, Matrix44AffineFromAxisRotationAngle x3, Mult,
// and for the mirror Up/At/SetX/SetY/SetZ with operator*<VectorAxisX/Y/Z>.
//
// ROUNDING (the campaign rule, scratch/CRASHPARITY_0922/ROUNDING_RULE.md, rules 3 and 4): the sine / cosine are the
// XDK polynomial, every vmaddfp is one rounding and every vmulfp / vsubfp / vaddfp its own. Written that way the body
// reproduces the emulator's 104 reference transforms BIT FOR BIT; the vendor SDK forms it used before (std::sin /
// std::cos, unfused mul + add) missed them by up to 3.9e-6.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"   // Utils::CameraRig
#include "GameSource/Director/Camera/Utils/BrnConsoleVpu.h"       // ConsoleVpu::Mult / Matrix44AffineFromAxisRotationAngle
#include "rw/math/vpu/matrix44affine_operation.h"                 // Matrix44Affine::SetIdentity / Pos
#include "rw/math/vpu/vector3_operation.h"                        // Vector3 +, -, Mult, GetVector3_*Axis

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

namespace
{
    const f32 KF_DEGREES_TO_RADIANS = 0.017453292f;   // flt_82001744 == 0x3C8EFA35
    const f32 KF_HALF               = 0.5f;           // flt_82001DA0 == 0x3F000000 (the box centre)
    const f32 KF_MIRROR             = -1.0f;          // flt_820037C8 == 0xBF800000 (lbReverse)
}

void CameraRig::Construct(const Params& lrData, const AABBox& lrAABB, bool lbReverse)
{
    using namespace rw::math::vpu;

    mfFOV = lrData.mfFOV;                                                               // stfs f12, 0x40(r3)

    const Vector3 lSize = lrAABB.mMax - lrAABB.mMin;                                    // :33

    // :38 -- the look-at point on the car: the preset's offset, in units of the car's size, from the box centre
    // (the centre one fused vmaddfp, the offset a vmulfp128, their sum a vaddfp128).
    Matrix44Affine lOffsetFromCar;
    lOffsetFromCar.SetIdentity();
    lOffsetFromCar.Pos() = Mult(lrData.mOffsetFromTarget, lSize)
                         + ConsoleVpu::MultiplyAdd(lSize, KF_HALF, lrAABB.mMin);

    // :39 -- yaw about Y first, then pitch about X, then roll about Z (row vectors: Rz * Rx * Ry). Each angle is one
    // fmuls to radians.
    Matrix44Affine lRotation;
    lRotation = ConsoleVpu::Mult(
        ConsoleVpu::Matrix44AffineFromAxisRotationAngle(GetVector3_ZAxis(), lrData.mfRoll * KF_DEGREES_TO_RADIANS),
        ConsoleVpu::Mult(
            ConsoleVpu::Matrix44AffineFromAxisRotationAngle(GetVector3_XAxis(), lrData.mfPitch * KF_DEGREES_TO_RADIANS),
            ConsoleVpu::Matrix44AffineFromAxisRotationAngle(GetVector3_YAxis(), lrData.mfYaw * KF_DEGREES_TO_RADIANS)));

    // :40 -- where the camera sits relative to the point it rotates about.
    Matrix44Affine lOffsetFromRotationCentre;
    lOffsetFromRotationCentre.SetIdentity();
    lOffsetFromRotationCentre.Pos() = lrData.mOffsetFromRotationCentre;

    mRigTransform.SetIdentity();
    mRigTransform = ConsoleVpu::Mult(mRigTransform, lRotation);
    mRigTransform = ConsoleVpu::Mult(lOffsetFromRotationCentre, mRigTransform);   // pos = offsetFromRotationCentre * R
    mRigTransform.Pos() = Mult(mRigTransform.Pos(), lSize);                       // stretched by the car's size
    mRigTransform = ConsoleVpu::Mult(mRigTransform, lOffsetFromCar);              // pos += the look-at point

    if (lbReverse)
    {
        // Mirror in X, one lane at a time in the console's order.
        mRigTransform.Right().y = mRigTransform.Right().y * KF_MIRROR;
        mRigTransform.Right().z = mRigTransform.Right().z * KF_MIRROR;
        mRigTransform.Up().x    = mRigTransform.Up().x * KF_MIRROR;
        mRigTransform.At().x    = mRigTransform.At().x * KF_MIRROR;
        mRigTransform.Pos().x   = mRigTransform.Pos().x * KF_MIRROR;
    }
}

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
