// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraShake.cpp
//
// Compilation home for the BrnDirector::Camera::Utils::CameraShake slice this TU owns:
//   - CameraShake::Update                                        @0x82221310
//   - CameraShake::Parameters::Serialise<DebugMenuSerialiser>    @0x82215540
//   - CameraShake::Parameters::Serialise<TextFileReadSerialiser> @0x82202EE0
//   - CameraShake::Parameters::Serialise<TextFileWriteSerialiser> @0x82215D90
//
// CameraShake::Parameters itself (the four f32 shake tunables) is homed in the canonical
// Utils/BrnCameraShake.h (the BehaviourRig.h copy that this banner used to name was retired
// in 2026-07-29); this TU bodies the per-frame Update and the ONE field-walk visitor, and
// instantiates the visitor over the three camera-tunings serialisers. The block is the shake
// post-process tunings shared by the gameplay/bystander/gyro/aftertouch/heli camera
// behaviours (each embeds a CameraShake and saves/loads/menus this block through its own
// Serialise<T>).
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"             // THE home: CameraShake + ::Parameters
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                // Utils::RotateMatrix44AffineByEulerAnglesZXY
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"          // (kept: the serialiser slice's original include)
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h" // DebugMenuSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"  // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"   // TextFileReadSerialiser

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

namespace
{
    // ------------------------------------------------------------------------
    // rw::math::vpu has no operators (project rule), so the lane-wise primitives the shake's
    // VMX block reduces to are spelled out here.
    // ------------------------------------------------------------------------
    typedef rw::math::vpu::Vector3     Vector3;
    typedef rw::math::vpu::Matrix44Affine Matrix44Affine;

    // The .rdata scalar at 0x82001744, splatted at 0x8222166C. pi/180: the Parameters'
    // magnitudes are authored in DEGREES (all four serialiser labels say so) and
    // RotateMatrix44AffineByEulerAnglesZXY takes RADIANS.
    const f32 KF_DEGREES_TO_RADIANS = 0.017453292f;

    Vector3 Add(const Vector3& lrA, const Vector3& lrB)          // vaddfp
    {
        Vector3 lResult;
        lResult.x = lrA.x + lrB.x;
        lResult.y = lrA.y + lrB.y;
        lResult.z = lrA.z + lrB.z;
        lResult.w = lrA.w + lrB.w;
        return lResult;
    }

    Vector3 Scale(const Vector3& lrV, f32 lfScale)               // vmulfp128 by a splat
    {
        Vector3 lResult;
        lResult.x = lrV.x * lfScale;
        lResult.y = lrV.y * lfScale;
        lResult.z = lrV.z * lfScale;
        lResult.w = lrV.w * lfScale;
        return lResult;
    }

    // `vmaddfp vD, splat(lfScale), vD, lrV` -- IDA prints vmaddfp in RAW FIELD ORDER
    // (VD, VA, VB, VC) with the semantic VD = VA * VC + VB.
    Vector3 ScaleAdd(const Vector3& lrV, f32 lfScale, const Vector3& lrAddend)
    {
        Vector3 lResult;
        lResult.x = lrV.x * lfScale + lrAddend.x;
        lResult.y = lrV.y * lfScale + lrAddend.y;
        lResult.z = lrV.z * lfScale + lrAddend.z;
        lResult.w = lrV.w * lfScale + lrAddend.w;
        return lResult;
    }

    // One row of the row-vector affine product `lrRow * lrRight` (no translation term).
    Vector3 TransformRow(const Vector3& lrRow, const Matrix44Affine& lrRight)
    {
        Vector3 lResult;
        lResult.x = lrRow.x * lrRight.xAxis.x + lrRow.y * lrRight.yAxis.x + lrRow.z * lrRight.zAxis.x;
        lResult.y = lrRow.x * lrRight.xAxis.y + lrRow.y * lrRight.yAxis.y + lrRow.z * lrRight.zAxis.y;
        lResult.z = lrRow.x * lrRight.xAxis.z + lrRow.y * lrRight.yAxis.z + lrRow.z * lrRight.zAxis.z;
        lResult.w = lrRow.x * lrRight.xAxis.w + lrRow.y * lrRight.yAxis.w + lrRow.z * lrRight.zAxis.w;
        return lResult;
    }
}

// ============================================================================
// CameraShake::Update @0x82221310   (300 asm lines)
//
// Add one frame of camera shake to lrTransform: three independent random angular jitters
// (pitch/yaw from the XY magnitude, roll from the Z magnitude) plus a damped random-walk
// "wobble" that the shake carries between frames, converted from degrees to radians, scaled by
// the caller's amount, turned into a ZXY-Euler rotation and PRE-multiplied onto the transform
// (so the camera rotates about its own origin -- the rotation's wAxis is zero, so the position
// row comes through untouched).
//
// ---- ARITY / ORDER, RECOVERED FROM THE ASM (Hex-Rays types every one of them `int`) -------
//   r3  this               -> the four live wobble f32 (read/written at +0/+4/+8/+0xC)
//   r4  Matrix44Affine&    -> read+written at +0/+0x10/+0x20/+0x30 in the tail
//   r5  const Parameters&  -> read at +0/+4/+8/+0xC, i.e. all four tunables
//   r6  Random&            -> muSeed @+0x20, muOldestBufferIndex @+0x28, ring union @+0
//   f1  f32                -> multiplies the wobble VELOCITY into the wobble POSITION == dt
//   f2  f32                -> multiplies the final angle vector == the caller's shake amount
// The committed declaration in BrnCameraShake.h matches exactly; nothing was dropped.
//
// ---- THE PARAMETERS BLOCK IS FULLY RECOVERED -- the "four unrecovered field names" note that
//   earlier waves carried is STALE. Each DWARF name is corroborated twice over by this body:
//     +0x00 mfXYShakeMagnitudeDegs  -> drawn TWICE, into Euler lanes 0 and 1 (pitch + yaw);
//                                     serialiser label "Yaw/Pitch Angular shake in degrees"
//     +0x04 mfZShakeMagnitudeDegs   -> drawn ONCE, into Euler lane 2 (roll);
//                                     label "Roll Angular shake in degrees"
//     +0x08 mfXYWobbleMagnitudeDegs -> the wobble impulse range, x/y only (z of the range is 0)
//     +0x0C mfWobbleCenteringFactor -> the spring gain on (-wobble - wobbleVelocity)
//   ...and "Degs" is confirmed by the pi/180 the result is multiplied by before it reaches
//   RotateMatrix44AffineByEulerAnglesZXY, which takes radians.
//   CameraShake's own 0x10 is likewise NOT an opaque span: the body reads and writes exactly
//   the four named f32 at +0/+4/+8/+0xC and nothing else in the object.
//
// ---- the draws --------------------------------------------------------------
//   The three scalar angles are RandomFloat(-mag, +mag): the asm inlines the ring read
//   (buffer[muOldestBufferIndex]), the refill (0x3F800000 | (muSeed >> 32 >> 9), then
//   muSeed = muSeed * 0x5851F42D4C957F2D + 1), the index bump ((idx + 1) & 7), the -1.0f that
//   maps [1,2) onto [0,1), and finally `(max - min) * t + min` as one fmadds.
//   The wobble impulse is the bounded VECTOR draw (DWARF RandomVector(Vector3, Vector3)):
//   TWO LCG steps whose two high words are packed across THREE ring slots at the Vector slot
//   ((idx + 3) & 4), returning the quad primed by the PREVIOUS call. That packing
//   (insrwi 21,9 / insrwi 10,9 + srwi 19 / inslwi 23,9) is byte-identical to the already
//   committed BrnEffects::Utils::Vector3Randomiser::RandomiseXYZ @0x82277EC8, which is what
//   identifies it. Called BY NAME here; nothing is poked into the Random by offset.
// ============================================================================
void CameraShake::Update(Matrix44Affine& lrTransform, const Parameters& lrParams,
                         Random& lrRandom, f32 lfTimestep, f32 lfSpeedRatio)
{
    // ---- the per-frame angular jitter, in degrees ---------------------------
    // Three independent draws; the vrlimi128 masks put them in Euler lanes 0, 1 and 2 (the
    // ZXY convention's pitch, yaw and roll). The w lane is Vector3's don't-care.
    Vector3 lShakeAngles;
    lShakeAngles.x = lrRandom.RandomFloat(-lrParams.mfXYShakeMagnitudeDegs,   // vrlimi128 v13, v0,  8, 0
                                           lrParams.mfXYShakeMagnitudeDegs);
    lShakeAngles.y = lrRandom.RandomFloat(-lrParams.mfXYShakeMagnitudeDegs,   // vrlimi128 v13, v12, 4, 3
                                           lrParams.mfXYShakeMagnitudeDegs);
    lShakeAngles.z = lrRandom.RandomFloat(-lrParams.mfZShakeMagnitudeDegs,    // vrlimi128 v13, v10, 2, 2
                                           lrParams.mfZShakeMagnitudeDegs);
    lShakeAngles.w = 0.0f;

    // ---- the carried wobble: a critically-ish damped random walk ------------
    // The four live f32 are lifted into two Vector3 temporaries with the z/w lanes zeroed
    // (the asm's stfs 0.0 into the third slot of each 16-byte stack block) -- the wobble only
    // ever has pitch and yaw, never roll.
    const Vector3 lWobble    = { mfCurrentWobbleX,    mfCurrentWobbleY,    0.0f, 0.0f };
    const Vector3 lWobbleVel = { mfCurrentWobbleXVel, mfCurrentWobbleYVel, 0.0f, 0.0f };

    // The impulse range: the XY wobble magnitude in x and y, ZERO in z, negated for the
    // minimum by a sign-bit vxor.
    const Vector3 lWobbleMax = {  lrParams.mfXYWobbleMagnitudeDegs,  lrParams.mfXYWobbleMagnitudeDegs, 0.0f, 0.0f };
    const Vector3 lWobbleMin = { -lrParams.mfXYWobbleMagnitudeDegs, -lrParams.mfXYWobbleMagnitudeDegs, 0.0f, 0.0f };
    const Vector3 lImpulse   = lrRandom.RandomVector(lWobbleMin, lWobbleMax);   // vmaddfp v12, v8, v12, v6

    // acceleration = centeringFactor * (-wobble - wobbleVelocity) + impulse
    //   vxor v0, v9, v0 (sign flip) ; vsubfp v0, v0, v10 ; vmaddfp v0, v0, v12, v8
    const Vector3 lRestoring = { -lWobble.x - lWobbleVel.x,
                                 -lWobble.y - lWobbleVel.y,
                                 -lWobble.z - lWobbleVel.z,
                                 -lWobble.w - lWobbleVel.w };
    const Vector3 lAcceleration = ScaleAdd(lRestoring, lrParams.mfWobbleCenteringFactor, lImpulse);

    // The velocity takes the acceleration WHOLE (no timestep on this edge -- the console's
    // `vaddfp v0, v10, v0`); only the position integrates over lfTimestep.
    const Vector3 lNewWobbleVel = Add(lWobbleVel, lAcceleration);
    const Vector3 lNewWobble    = ScaleAdd(lNewWobbleVel, lfTimestep, lWobble);   // vmaddfp v0, v0, v9, v5

    mfCurrentWobbleXVel = lNewWobbleVel.x;   // stfs f13, 8(r10)
    mfCurrentWobbleYVel = lNewWobbleVel.y;   // stfs f0, 0xC(r10)
    mfCurrentWobbleX    = lNewWobble.x;      // stfs f0, 0(r10)
    mfCurrentWobbleY    = lNewWobble.y;      // stfs f0, 4(r10)

    // ---- degrees -> radians, scaled by the caller's amount ------------------
    // ⚠️ lfSpeedRatio is the COMMITTED parameter name and is INFERRED, not attested: the asm
    // only shows it multiplying the final angle. The two call sites that were read
    // (BehaviourGameplayBumper::Update @0x82227134 passes a behaviour f32 * 3.0f;
    // ImpactShakeController::Update @0x822438AC passes *this * 15.0f) both read as a shake
    // AMOUNT rather than a speed ratio. Left alone rather than churned.
    const Vector3 lEulerAnglesRads = Scale(Add(lShakeAngles, lNewWobble),
                                           KF_DEGREES_TO_RADIANS * lfSpeedRatio);

    // ---- build the rotation and pre-multiply --------------------------------
    // The console materialises an IDENTITY on the stack (three 1.0f from flt_82001C98 on the
    // diagonal, a zero wAxis) and rotates it IN PLACE -- r3 is both the argument and the
    // buffer it reads back from afterwards.
    Matrix44Affine lShakeRotation;
    lShakeRotation.SetIdentity();
    RotateMatrix44AffineByEulerAnglesZXY(lShakeRotation, lEulerAnglesRads);

    // lrTransform = lShakeRotation * lrTransform (row-vector convention). Only the wAxis row
    // picks up the transform's own translation, which is why a pure rotation leaves the camera
    // position exactly where it was.
    const Matrix44Affine lOriginal = lrTransform;
    lrTransform.xAxis = TransformRow(lShakeRotation.xAxis, lOriginal);
    lrTransform.yAxis = TransformRow(lShakeRotation.yAxis, lOriginal);
    lrTransform.zAxis = TransformRow(lShakeRotation.zAxis, lOriginal);
    lrTransform.wAxis = Add(TransformRow(lShakeRotation.wAxis, lOriginal), lOriginal.wAxis);
}

// ----------------------------------------------------------------------------
// CameraShake::Parameters::Serialise<S> -- the ONE shake-tunings field-walk visitor body.
//
// Walks the block's four f32 tunables in ascending-offset order, handing each to the
// serialiser S by name. S supplies the per-field direction:
//   - Serialise<DebugMenuSerialiser>    @0x82215540: each field -> Process<float>(name, &field)
//       then SetStep(0.01) on the debug component (mpDebugComponent @+0x48; the scalar
//       DebugMenuSerialiser::Serialise(const char*, f32&) inlines to exactly that Process+SetStep
//       pair -- de-inlined here to the uniform Serialise(name, field) call).
//   - Serialise<TextFileWriteSerialiser> @0x82215D90: each field -> FormatName + fprintf
//       "%s : %f\n" when mpFile is open (FormatName + the write inlined into the walk).
//   - Serialise<TextFileReadSerialiser>  @0x82202EE0: each field -> fscanf "%s : %f\n" while
//       the wrapped FILE* is open (the read short-circuits the moment the handle is null).
// The field sequence + labels are identical across all three instances' asm; only S's inlined
// scalar helper differs -- so the source is this single uniform body (mirrors the committed
// BehaviourGameplayBumper::Parameters::Serialise design).
//
// Field/label map (from the DebugMenu/write/read asm, ascending offset):
//   +0x00 mfXYShakeMagnitudeDegs   "Yaw/Pitch Angular shake in degrees"
//   +0x04 mfZShakeMagnitudeDegs    "Roll Angular shake in degrees"
//   +0x08 mfXYWobbleMagnitudeDegs  "Wobble in degrees"
//   +0x0C mfWobbleCenteringFactor  "Wobble centering factor"
// All four labels are fully recovered rodata (no unresolved unk_82xxxxxx address in any of the
// three instances) -- no extern label flag needed.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void CameraShake::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Yaw/Pitch Angular shake in degrees", mfXYShakeMagnitudeDegs);
    lrSerialiser.Serialise("Roll Angular shake in degrees", mfZShakeMagnitudeDegs);
    lrSerialiser.Serialise("Wobble in degrees", mfXYWobbleMagnitudeDegs);
    lrSerialiser.Serialise("Wobble centering factor", mfWobbleCenteringFactor);
}

// Explicit instantiations -- one per serialiser this shake block is menu'd / saved / loaded
// through. All three serialiser types are reconstructed and homed, so (unlike the bumper-cam
// TU that predated the DebugMenuSerialiser home) the debug-menu instance is emitted here too.
template void CameraShake::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void CameraShake::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void CameraShake::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
