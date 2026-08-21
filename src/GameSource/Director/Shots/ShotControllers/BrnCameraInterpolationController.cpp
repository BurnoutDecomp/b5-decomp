// ============================================================================
// GameSource/Director/Shots/ShotControllers/BrnCameraInterpolationController.cpp
//
// BrnDirector::CameraInterpolationController -- the ROTATE-ABOUT-PIVOT family.
//
//   Matrix44AffineFromRota                @0x821F8220
//   RotateAboutPivotParams::Interpolate   @0x8221E9D0
//   ExtractRotateAboutPivotParams         @0x8221EAC0
//   RotateAboutPivot                      @0x8223DA28
//
// ⭐ WHY THIS FAMILY EXISTS. BehaviourInterpolate's blend has two methods, chosen by the
// camera's mu8InterpolateType (+0x11E): method 0 is a plain camera SLERP, method 1 is
// E_METHOD_ROTATE_ABOUT_PLAYER_CAR -- and method 1 is what the junkyard car-select shot uses.
// A plain slerp between two cameras that both look at the same car cuts a CHORD through the
// car; this family instead re-expresses each camera as (orientation-in-look-at-frame,
// look-at basis, orbit radius) relative to the pivot, blends those three, and rebuilds. That
// is what makes the shot ORBIT the car.
//
// SOURCE: BURNOUT_X360_ARTIST.XEX raw asm for all four. The pseudocode for every one of them
// is VMX soup with the operand order scrambled, so the decode below is anchored on the
// STORE OFFSETS and on the round trip (extract . rebuild == identity), not on Hex-Rays.
//
// ⚠️ THE TWO vmaddfp SPELLINGS DIFFER, and getting them backwards silently transposes a
// matrix multiply. Measured against a known-good case (the matrix-row cascade in
// Matrix44AffineFromRota, where only one reading produces a valid row-major multiply):
//     `vmaddfp   vD, vA, vB, vC`  ==  vD = vA * vC + vB
//     `vmaddfp128 vD, vA, vB, vC` ==  vD = vA * vB + vC
// Both appear in this family, sometimes in the same basic block.
//
// ⛔ NOT YET REACHABLE. CameraInterpolationController::Update @0x822513D8 -- the caller that
// selects between the two methods and then blends CameraState / CameraEffects / the
// DepthOfField block / FOV / near-clip -- is NOT in this TU yet, and neither is the
// direction-preserving slerp this family's Interpolate leans on
// (Camera::Utils::DirectionPreservingSLerp @0x82205558 + rw::math::vpu::QueryRotate
// @0x822038F0 + QueryRotateDegenerateUnitAxis @0x82203768). Until those land,
// BehaviourInterpolate::PostCollisionUpdate keeps its documented t == 1 cut.
// DELETE-WHEN: those four land; then the in-between un-gates.
// ============================================================================

#include "GameSource/Director/Shots/ShotControllers/BrnCameraInterpolationController.h"

#include "GameSource/Director/Camera/Camera.h"                 // Camera::Camera (mTransform)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"      // Utils::CreateLookAt
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT
#include "rw/math/vpu/matrix44affine_operation.h"              // Mult / InverseOfMatrixWithOrthonormal3x3
#include "rw/math/vpu/vector3_operation.h"                     // Magnitude / Mult
#include "GameSource/Director/Camera/BrnCameraState.h"          // CameraState::Interpolate
#include "GameSource/Director/Camera/BrnCameraEffects.h"        // CameraEffects::Interpolate
#include "GameSource/Director/Camera/BrnDepthOfField.h"         // DepthOfField accessors
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"  // Begin/Fire/EndAssert
#include <cmath>                                                // std::pow (mapping 3)

namespace BrnDirector
{

using rw::math::vpu::Matrix44Affine;
using rw::math::vpu::Vector3;

namespace
{
    // The assert file every function in this family names.
    const char* const KAC_CIC_FILE =
        "..\\..\\..\\GameSource\\Director/Shots/ShotControllers/"
        "BrnCameraInterpolationController.cpp";

    const f32 KF_ZERO = 0.0f;   // flt_82001CC0
    const f32 KF_ONE  = 1.0f;
    const f32 KF_HALF = 0.5f;

    // The two selector bytes BehaviourInterpolate stamps into the camera effects.
    // mu8BlendCurve values, from the jump table at 0x82251418:
    const u8 KU_MAPPING_LINEAR      = 0;
    const u8 KU_MAPPING_SINE        = 1;
    const u8 KU_MAPPING_EXPONENTIAL = 2;
    const u8 KU_MAPPING_CUBIC_POWER = 3;
    // mu8InterpolateType values, from the second switch at 0x82251520:
    const u8 KU_METHOD_SLERP                   = 0;
    const u8 KU_METHOD_ROTATE_ABOUT_PLAYER_CAR = 1;

    // CameraState bit 2 (asm mask 4 against camera+320): "do not interpolate me".
    const u32 KU_STATE_FLAG_NO_INTERPOLATE = 2u;

    // The params carry their two 3x3s inside Matrix44Affine storage (rows 0..2 used, row 3
    // scratch), matching the console's 0x70-byte record. These two convert at the boundary
    // so the Matrix33 slerp overload is the one selected.
    inline Matrix33 To33(const Matrix44Affine& lrMatrix)
    {
        Matrix33 lResult;
        lResult.xAxis = lrMatrix.xAxis;
        lResult.yAxis = lrMatrix.yAxis;
        lResult.zAxis = lrMatrix.zAxis;
        return lResult;
    }
    inline Matrix44Affine ToAffine(const Matrix33& lrMatrix)
    {
        Matrix44Affine lResult;
        lResult.xAxis = lrMatrix.xAxis;
        lResult.yAxis = lrMatrix.yAxis;
        lResult.zAxis = lrMatrix.zAxis;
        lResult.wAxis = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };
        return lResult;
    }
    // The console emits every scalar lerp inline as (to - from) * t + from.
    inline f32 KLerp(f32 lfFrom, f32 lfTo, f32 lfT) { return (lfTo - lfFrom) * lfT + lfFrom; }
}

// ----------------------------------------------------------------------------
// Matrix44AffineFromRota @ 0x821F8220
//
// Rebuild a WORLD transform from a pivot-relative description. The inverse of
// ExtractRotateAboutPivotParams below, and the last thing RotateAboutPivot does.
//
// asm walk (r3 = sret, r4 = this UNUSED, r5 = lrParams, r6 = lrPivot):
//   0x821F8224  lfs f0, 0x60(r5) ; fneg f0, f0     -- load mfDistance and NEGATE it
//   0x821F823C  lvx v11, r5+0x00                   -- mRotation rows ...
//   0x821F825C  lvx v8,  r5+0x30                   -- mLookAt rows ...
//   0x821F8290..0x821F82E0   the row cascade producing R = mRotation3x3 * mLookAt3x3
//   0x821F82B0  vmulfp128 v11, v5, v11             -- mLookAt.zAxis * (-mfDistance)
//   0x821F82E4..0x821F8334   R and that offset put through lrPivot (vector transform for
//                            the three axes, POINT transform for the offset)
//   0x821F8338..0x821F8344   store the four rows to the sret
//
// ⚠️ THE OFFSET USES mLookAt's zAxis (r5 + 0x50), NOT the rotation's -- the orbit radius is
// measured along the LOOK-AT frame's forward axis, which is the whole point of carrying a
// separate look-at basis. And it is NEGATED, because the eye sits BEHIND the look-at target
// by mfDistance.
// ⚠️ `this` (r4) is never read. Kept as a const member for call-shape parity with the
// console, which dispatches it as one.
// ----------------------------------------------------------------------------
Matrix44Affine CameraInterpolationController::Matrix44AffineFromRota(
        const RotateAboutPivotParams& lrParams,
        const Matrix44Affine&         lrPivot) const
{
    // R = mRotation3x3 * mLookAt3x3. Mult's wAxis lane is scratch here -- it is overwritten
    // on the next line, exactly as the console's row cascade never computes one.
    Matrix44Affine lRota = rw::math::vpu::Mult(lrParams.mRotation, lrParams.mLookAt);

    // The eye offset in the look-at frame: straight back along its forward axis.
    lRota.wAxis = rw::math::vpu::Mult(lrParams.mLookAt.zAxis, -lrParams.mfDistance);

    // Into world space. Mult transforms the three axes as VECTORS and wAxis as a POINT,
    // which is exactly the console's split (three vmaddfp cascades against lrPivot's rows,
    // and a fourth that folds in lrPivot's own translation row).
    return rw::math::vpu::Mult(lRota, lrPivot);
}

// ----------------------------------------------------------------------------
// ExtractRotateAboutPivotParams @ 0x8221EAC0
//
// Describe lrCameraTransform relative to a pivot, given that pivot's INVERSE.
//
// asm walk (r4 = lrCameraTransform, r5 = lrInversePivot, r6 = lrOut):
//   0x8221EAD8..0x8221EB8C  the full 4x4 affine multiply M = camera * inversePivot,
//                           leaving rows in v127 / v126 / v125 / v124
//   0x8221EB50..0x8221EB7C  a ZEROED Vector4 is built on the stack and loaded into v2
//   0x8221EBA0  vmr128 v1, v124 ; bl Utils::CreateLookAt
//                           -- i.e. CreateLookAt(eye = M.wAxis, target = the ORIGIN). The
//                              two-argument overload; v2 being the zero vector is what
//                              pins the target, and it is why the look-at is built in
//                              PIVOT SPACE where the pivot sits at the origin.
//   0x8221EBC8..0x8221EBEC  M's three axes -> out+0x00/0x10/0x20,
//                           the look-at's three -> out+0x30/0x40/0x50
//   0x8221EBF4..0x8221EC84  a 3x3 TRANSPOSE of the look-at (the vmrghw/vmrglw lane merges)
//                           followed by a row cascade, overwriting out+0x00/0x10/0x20 with
//                           M3x3 * lookAt^T
//   0x8221EC50..0x8221ECB8  vmsum3fp128 v0, v124, v124 (dot3 of the position with itself),
//                           vrsqrtefp + TWO Newton-Raphson refinements, multiplied back by
//                           the input == |M.wAxis|; vsel forces 0 when the square is 0.
//                           Stored to out+0x60.
//
// ⭐ THE TRANSPOSE IS THE POINT. Storing M's raw 3x3 would make the blend re-derive the
// orbit every frame; storing it IN the look-at frame means the residual rotation is small
// and slerps cleanly. Since the look-at basis is orthonormal, transpose == inverse, so
// InverseOfMatrixWithOrthonormal3x3's 3x3 is the same lane shuffle the console emits (its
// translation lane is computed and then discarded here, exactly as the console's cascade
// never reads one).
// ----------------------------------------------------------------------------
void CameraInterpolationController::ExtractRotateAboutPivotParams(
        const Matrix44Affine&   lrCameraTransform,
        const Matrix44Affine&   lrInversePivot,
        RotateAboutPivotParams& lrOut) const
{
    // The camera, expressed in pivot space.
    const Matrix44Affine lCameraInPivotSpace =
        rw::math::vpu::Mult(lrCameraTransform, lrInversePivot);

    // The look-at basis for that eye position, aimed at the pivot -- which is the origin of
    // this space. (Utils::CreateLookAt's two-argument overload, @0x8220C4F8.)
    const Vector3 lOrigin = { KF_ZERO, KF_ZERO, KF_ZERO, KF_ZERO };
    const Matrix44Affine lLookAt =
        Camera::Utils::CreateLookAt(lCameraInPivotSpace.wAxis, lOrigin);

    lrOut.mLookAt = lLookAt;

    // The camera's orientation expressed IN that look-at frame: M3x3 * lookAt^T.
    const Matrix44Affine lLookAtTranspose =
        rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lLookAt);
    lrOut.mRotation = rw::math::vpu::Mult(lCameraInPivotSpace, lLookAtTranspose);

    // The orbit radius. Magnitude() is the SDK's own rsqrt-with-Newton pipeline and carries
    // the same zero-length guard the console's vsel provides.
    lrOut.mfDistance = rw::math::vpu::Magnitude(lCameraInPivotSpace.wAxis);
}

// ----------------------------------------------------------------------------
// RotateAboutPivotParams::Interpolate @ 0x8221E9D0
//
// asm walk (r3 = lrFrom, r4 = lrTo, r5/r6 = the two Interpolaters, r7 = out, v1 = t):
//   0x8221EA08  DirectionPreservingSLerp(&scratch, lrFrom+0x00, lrTo+0x00,
//                                        r5, r5+0x10, t)          -- the ROTATION
//   0x8221EA34..0x8221EA50   its three rows -> out+0x00/0x10/0x20
//   0x8221EA54  DirectionPreservingSLerp(&scratch, lrFrom+0x30, lrTo+0x30,
//                                        r6, r6+0x10, t)          -- the LOOK-AT basis
//   0x8221EA60..0x8221EA80   its three rows -> out+0x30/0x40/0x50
//   0x8221EA88..0x8221EAA8   lerp(lrFrom+0x60, lrTo+0x60, t)      -- the RADIUS
//
// ⚠️ TWO SEPARATE INTERPOLATERS, ONE PER MATRIX. r5 and r6 are distinct objects (Update
// hands over `this` and `this + 32`), and each keeps its OWN remembered rotation axis. Share
// one and the two blends fight over the axis-inversion latch, which is exactly the flip this
// whole "direction preserving" machinery exists to prevent.
// ⚠️ The two trailing arguments of each call are the Interpolater's two members -- +0x00
// (mLastAxis) and +0x10 (mbWasInvertedLastTime) -- reached by offset on the console because
// DirectionPreservingSLerp takes them as loose pointers.
// ----------------------------------------------------------------------------
CameraInterpolationController::RotateAboutPivotParams
CameraInterpolationController::RotateAboutPivotParams::Interpolate(
        const RotateAboutPivotParams&      lrFrom,
        const RotateAboutPivotParams&      lrTo,
        Camera::Utils::Interpolater&       lrRotationInterpolater,
        Camera::Utils::Interpolater&       lrLookAtInterpolater,
        f32                                lfT)
{
    RotateAboutPivotParams lResult;

    // The console keeps the parametric time in v1 as a SPLATTED lane for the whole function
    // (`vmr128 v127, v1` on entry, `vmr128 v1, v127` before each call), which is what
    // Interpolater::Interpolate's VecFloat parameter is. BrnDirector::VecFloat -- the one
    // BrnInterpolater.h now pins -- broadcasts a scalar in its converting constructor, so
    // this IS the splat, not a narrowing.
    const VecFloat lvT = VecFloat(lfT);

    // ⚠️ THE Matrix33 OVERLOAD, DELIBERATELY. The console hands DirectionPreservingSLerp
    // @0x82205558 (the THREE-row one, CameraUtils.h:742) three-row blocks here and stores
    // exactly three rows back (asm 0x8221EA34..0x8221EA50 / 0x8221EA60..0x8221EA80). Letting
    // overload resolution pick the AFFINE version instead would silently add a translation
    // lerp these params do not have and drop the small-angle arm this path does use.
    lResult.mRotation = ToAffine(lrRotationInterpolater.Interpolate(To33(lrFrom.mRotation),
                                                                   To33(lrTo.mRotation), lvT));
    lResult.mLookAt   = ToAffine(lrLookAtInterpolater.Interpolate(To33(lrFrom.mLookAt),
                                                                 To33(lrTo.mLookAt), lvT));

    lResult.mfDistance = lrFrom.mfDistance + (lrTo.mfDistance - lrFrom.mfDistance) * lfT;

    return lResult;
}

// ----------------------------------------------------------------------------
// RotateAboutPivot @ 0x8223DA28
//
// asm walk (r3 = sret, r4 = this, r5 = lrPivot (== _R31), r6/r7 = the two cameras,
//           plus the two Interpolaters, v1 = t):
//   0x8223DA3C..0x8223DAA8  assert lT >= 0.0f && lT <= 1.0f            (cpp:212)
//   0x8223DAB0..0x8223DB30  build the INVERSE of lrPivot: `vsubfp v13, 0, pivot.wAxis`
//                           (negate the translation), a vmrghw/vmrglw 3x3 transpose, then a
//                           vmaddfp cascade of the transposed rows against the negated
//                           position broadcasts -- i.e. the orthonormal affine inverse.
//   0x8223DB3C  ExtractRotateAboutPivotParams(this, lrFrom, inverse, paramsFrom)
//   0x8223DB44  ExtractRotateAboutPivotParams(this, lrTo,   inverse, paramsTo)
//   0x8223DB50  RotateAboutPivotParams::Interpolate(paramsFrom, paramsTo, i0, i1, out, t)
//   0x8223DB58  return Matrix44AffineFromRota(sret, this, out, lrPivot)
//
// ⚠️ THE ASSERT IS TWO COMPARES, NOT ONE, and the console fires it on the FAILING side of
// either -- `vcmpgefp128. t, 0` then `vcmpgefp128. 1, t`. Non-gating as always: the body runs
// regardless, so an out-of-range t extrapolates the orbit rather than clamping.
// ⚠️ THE INVERSE IS COMPUTED ONCE and handed to BOTH extractions. Doing it per-extraction
// would be numerically identical but is not the console's shape.
// ----------------------------------------------------------------------------
Matrix44Affine CameraInterpolationController::RotateAboutPivot(
        const Matrix44Affine&        lrPivot,
        const Camera::Camera&        lrFrom,
        const Camera::Camera&        lrTo,
        Camera::Utils::Interpolater& lrRotationInterpolater,
        Camera::Utils::Interpolater& lrLookAtInterpolater,
        f32                          lfT)
{
    CGS_ASSERT(lfT >= KF_ZERO && lfT <= KF_ONE, "lT >= 0.0f && lT <= 1.0f");   // cpp:212

    const Matrix44Affine lInversePivot =
        rw::math::vpu::InverseOfMatrixWithOrthonormal3x3(lrPivot);

    RotateAboutPivotParams lFromParams;
    RotateAboutPivotParams lToParams;
    ExtractRotateAboutPivotParams(lrFrom.mTransform, lInversePivot, lFromParams);
    ExtractRotateAboutPivotParams(lrTo.mTransform,   lInversePivot, lToParams);

    const RotateAboutPivotParams lBlended =
        RotateAboutPivotParams::Interpolate(lFromParams, lToParams,
                                            lrRotationInterpolater, lrLookAtInterpolater, lfT);

    return Matrix44AffineFromRota(lBlended, lrPivot);
}


// ----------------------------------------------------------------------------
// Update @ 0x822513D8   (228 asm lines)
//
// THE WHOLE PER-FRAME BLEND. BehaviourInterpolate::PostCollisionUpdate stamps three fields
// into the live camera's effects -- mfGameCameraBlend (+0x108), mu8BlendCurve (+0x11D) and
// mu8InterpolateType (+0x11E) -- and then calls this, which reads them back and does the work.
//
// asm walk (a1 = this, a2 = lrCamera == _R31, a3 = lrTo, a4 = lrEyeTarget):
//   0x82251400  v12 = camera+264 == mfGameCameraBlend; the ENTIRE body is inside `!= 0.0`
//   0x82251418  switch (camera+285 == mu8BlendCurve):
//                 0 -> linear         1 -> SineLerp(0, 1, t)
//                 2 -> ExponentialLerp(0, 1, t)
//                 3 -> 1.0 - pow(0.9f, t*t*t*100.0)
//                 default -> assert "Unknown interpolation mapping"        (cpp:112)
//   0x822514E8  if (either camera's state has bit 2 set) -> CUT at t >= 0.5, skip everything
//   0x82251520  switch (camera+286 == mu8InterpolateType):
//                 0 -> the plain camera SLERP (sub_82217C08)
//                 1 -> RotateAboutPivot(..., this, this + 32, t)
//                 else -> assert "Unknown interpolation method"            (cpp:156)
//               then the four transform rows are copied over lrCamera
//   0x8225159C  Camera::ValidateTransformWithDebugInfo(lrCamera)
//   0x822515A8  CameraState::Interpolate  -> all three qwords back over camera+312
//   0x822515C8  CameraEffects::Interpolate -> operator= back over camera+104
//   0x822515E8  the five DepthOfField floats at camera+292..+308, each a plain lerp
//   0x82251638  Camera::SetFOV(lerp(camera+88, to+88, t))
//   0x82251650  camera+349 = 1; camera+336 = lerp(GetNearClipDistance pair)
//   0x82251670  camera+264 = 0.0                                  <- the blend is CONSUMED
//
// THE t >= 0.5 CUT IS A REAL CONSOLE PATH, not a shortfall. State bit 2 on EITHER camera
// means "do not interpolate me", and the console then simply swaps to lrTo once the blend is
// past halfway. A transition that snaps despite everything below being live is very likely
// this, not a missing piece -- check the bit before assuming a regression.
//
// THE BLEND FIELD IS ZEROED ON THE WAY OUT. That is the handshake with the producer:
// PostCollisionUpdate re-stamps mfGameCameraBlend every frame, so a frame in which the
// behaviour does not run leaves this function inert instead of re-applying a stale blend.
//
// CURVE 3 HAS NO NAMED HELPER -- it is `1 - pow(0.9f, t^3 * 100)` written inline at
// 0x82251490, with the base loaded as the DOUBLE 0.8999999761581421 (== (double)0.9f) and
// the exponent formed as ((t*t)*t)*100.0. At t == 1 the power underflows to ~2.6e-5, so the
// curve reaches 1.0 for all practical purposes but is NOT exactly 1 -- which is why the
// caller's own `>= 1.0` endpoint test is on the RAW parametric time, not on this output.
// ----------------------------------------------------------------------------
void CameraInterpolationController::Update(Camera::Camera&       lrCamera,
                                           const Camera::Camera& lrTo,
                                           const Matrix44Affine& lrEyeTarget)
{
    const f32 lfRawBlend = lrCamera.GetEffects().mfGameCameraBlend;
    if (lfRawBlend == KF_ZERO)
    {
        return;
    }

    // ---- the easing curve (mu8BlendCurve) ------------------------------------------------
    f32 lfT = lfRawBlend;
    switch (lrCamera.GetEffects().mu8BlendCurve)
    {
    case KU_MAPPING_LINEAR:
        break;
    case KU_MAPPING_SINE:
        lfT = Camera::Utils::SineLerp(KF_ZERO, KF_ONE, lfRawBlend);
        break;
    case KU_MAPPING_EXPONENTIAL:
        lfT = Camera::Utils::ExponentialLerp(KF_ZERO, KF_ONE, lfRawBlend);
        break;
    case KU_MAPPING_CUBIC_POWER:
    {
        const f64 ldExponent = static_cast<f64>((lfRawBlend * lfRawBlend) * lfRawBlend) * 100.0;
        lfT = KF_ONE - static_cast<f32>(std::pow(static_cast<f64>(0.9f), ldExponent));
        break;
    }
    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Unknown interpolation mapping", KAC_CIC_FILE, 112);
        CgsDev::Assert::EndAssert();
        break;
    }

    // ---- the "do not interpolate me" cut -------------------------------------------------
    // asm reads camera+320 (== mState_uFlags, the current-flag low word) and tests mask 4.
    if (lrCamera.GetState().IsFlagSet(KU_STATE_FLAG_NO_INTERPOLATE) ||
        lrTo.GetState().IsFlagSet(KU_STATE_FLAG_NO_INTERPOLATE))
    {
        if (lfT >= KF_HALF)
        {
            lrCamera = lrTo;
        }
        lrCamera.GetEffects().mfGameCameraBlend = KF_ZERO;
        return;
    }

    // ---- the transform (mu8InterpolateType) ----------------------------------------------
    const u8 lu8Method = lrCamera.GetEffects().mu8InterpolateType;
    if (lu8Method == KU_METHOD_ROTATE_ABOUT_PLAYER_CAR)
    {
        lrCamera.mTransform = RotateAboutPivot(lrEyeTarget, lrCamera, lrTo,
                                               mRotationInterpolater, mPivotInterpolater, lfT);
        lrCamera.ValidateTransformWithDebugInfo();
    }
    else if (lu8Method == KU_METHOD_SLERP)
    {
        // The console's method-0 arm, now real: DirectionPreservingSLerp's AFFINE overload
        // @0x82217C08, handed this controller's FIRST interpolater as its two loose state
        // pointers (asm `a1`, `a1 + 16` == mLastAxis and mbWasInvertedLastTime).
        lrCamera.mTransform = Camera::Utils::DirectionPreservingSLerp(
            lrCamera.mTransform, lrTo.mTransform,
            mRotationInterpolater.GetLastAxis(),
            mRotationInterpolater.GetWasInvertedLastTime(), VecFloat(lfT));
        lrCamera.ValidateTransformWithDebugInfo();    }
    else
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Unknown interpolation method", KAC_CIC_FILE, 156);
        CgsDev::Assert::EndAssert();
    }

    // ---- everything else blends regardless of method -------------------------------------
    lrCamera.GetState() =
        Camera::CameraState::Interpolate(lrCamera.GetState(), lrTo.GetState(), lfT);

    lrCamera.GetEffects() =
        Camera::CameraEffects::Interpolate(lrCamera.GetEffects(), lrTo.GetEffects(), lfT);

    // The five DepthOfField floats at camera+292..+308, each a plain lerp (asm 0x822515E8:
    // five `(to - from) * t + from` in a row, then a 5-word copy loop back into the camera).
    const Camera::DepthOfField& lrFromDof = lrCamera.GetDepthOfField();
    const Camera::DepthOfField& lrToDof   = lrTo.GetDepthOfField();
    const f32 lfFocusStart   = KLerp(lrFromDof.GetFocusStartDistanceMeters(),
                                     lrToDof.GetFocusStartDistanceMeters(), lfT);
    const f32 lfPerfectStart = KLerp(lrFromDof.GetPerfectFocusStartDistanceMeters(),
                                     lrToDof.GetPerfectFocusStartDistanceMeters(), lfT);
    const f32 lfPerfectEnd   = KLerp(lrFromDof.GetPerfectFocusEndDistanceMeters(),
                                     lrToDof.GetPerfectFocusEndDistanceMeters(), lfT);
    const f32 lfFocusEnd     = KLerp(lrFromDof.GetFocusEndDistanceMeters(),
                                     lrToDof.GetFocusEndDistanceMeters(), lfT);
    const f32 lfBlurriness   = KLerp(lrFromDof.GetBlurriness(),
                                     lrToDof.GetBlurriness(), lfT);
    lrCamera.GetDepthOfField().SetParams(lfFocusStart, lfPerfectStart, lfPerfectEnd,
                                         lfFocusEnd, lfBlurriness);

    lrCamera.SetFOV(KLerp(lrCamera.GetFOV(), lrTo.GetFOV(), lfT));

    // asm 0x82251650: the custom-near-clip LATCH is raised first (camera+349 ==
    // mbHasCustomNearClipDistance), then the distance itself (camera+336) is written from the
    // lerp of the two GetNearClipDistance() results -- note it reads the ACCESSOR on both
    // sides, which already resolves the custom-vs-default selection, rather than the fields.
    const f32 lfNearClip = KLerp(lrCamera.GetNearClipDistance(), lrTo.GetNearClipDistance(), lfT);
    lrCamera.mbHasCustomNearClipDistance = true;
    lrCamera.mfCustomNearClipDistance    = lfNearClip;

    // The handshake: consume the blend so a frame without a producer is inert.
    lrCamera.GetEffects().mfGameCameraBlend = KF_ZERO;
}

} // namespace BrnDirector
