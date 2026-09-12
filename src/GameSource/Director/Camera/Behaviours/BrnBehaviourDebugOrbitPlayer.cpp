// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourDebugOrbitPlayer slices.
// This TU bodies:
//   Construct (virtual; zero the rig head + cached param word + mpParameters)
//   GetName (virtual; the literal behaviour name)
//   LookAtFront (static dev-tweaker preset callback)
//   LookAtBack (static dev-tweaker preset callback)
//   LookAtLeftSide (static dev-tweaker preset callback)
//   LookAtRightSide (static dev-tweaker preset callback)
//   Prepare (virtual; seed the orbit rig, flag active)
//   SetupTweaker (virtual; wire the rig into the camera tweaker)
//   Update (virtual; produce this frame's orbit camera around the tracked car)
// The remaining behaviour (SetParameters / Parameters::Construct) lands with sibling waves.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h"
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"   // BehaviourSharedInfo (the canonical home)
#include "GameSource/Director/Camera/Camera.h"                 // BrnDirector::Camera::Camera (Update target)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"      // Utils::CreateLookAt / RotateMatrix44AffineByEulerAnglesZXY
#include "rw/math/vpu/matrix44affine_operation.h"              // TransformPoint / Mult
#include "rw/math/vpu/vector3_operation.h"                     // Vector3 operator+ / Mult / Lerp

namespace BrnDirector
{
namespace Camera
{

// ---- original-build rodata constants (out-of-line definitions) -----------------------
const f32 BehaviourDebugOrbitPlayer::KF_LOOK_AT_DISTANCE = 2.5f;
const f32 BehaviourDebugOrbitPlayer::KF_HALF_PI          = 1.5707964f;
const f32 BehaviourDebugOrbitPlayer::KF_DEFAULT_FOV      = 90.0f;

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::Construct
// The named model folds the base flag/word head into reserved bytes; the stores land
// as the zero-init of that reserved head plus mbActive + mpParameters.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::Construct()
{
    for (unsigned liI = 0; liI < sizeof(maReserved04); ++liI) maReserved04[liI] = 0; // +0x04..+0x07
    mbActive = false;
    for (unsigned liI = 0; liI < sizeof(maReserved09); ++liI) maReserved09[liI] = 0; // +0x09..+0x13
    mpParameters = 0;
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::GetName
// ----------------------------------------------------------------------------
const char* BehaviourDebugOrbitPlayer::GetName() const
{
    return "DebugOrbitPlayer";
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::LookAtFront
// Tweaker "Look at front" preset: distance 2.5, yaw pi, pitch 0.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::LookAtFront(void* lpData)
{
    BehaviourDebugOrbitPlayer* const lpThis = static_cast<BehaviourDebugOrbitPlayer*>(lpData);
    lpThis->mfDistance = KF_LOOK_AT_DISTANCE;
    lpThis->mfYaw      = 3.1415927f;
    lpThis->mfPitch    = 0.0f;
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::LookAtBack
// Tweaker "Look at back" preset: distance 2.5, yaw 0, pitch 0.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::LookAtBack(void* lpData)
{
    BehaviourDebugOrbitPlayer* const lpThis = static_cast<BehaviourDebugOrbitPlayer*>(lpData);
    lpThis->mfDistance = KF_LOOK_AT_DISTANCE;
    lpThis->mfYaw      = 0.0f;
    lpThis->mfPitch    = 0.0f;
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::LookAtLeftSide
// D-pad left: orbit to the car's LEFT side. Yaw -pi/2.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::LookAtLeftSide(void* lpData)
{
    BehaviourDebugOrbitPlayer* const lpThis = static_cast<BehaviourDebugOrbitPlayer*>(lpData);
    lpThis->mfDistance = KF_LOOK_AT_DISTANCE;
    lpThis->mfYaw      = -KF_HALF_PI;
    lpThis->mfPitch    = 0.0f;
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::LookAtRightSide
// D-pad right: orbit to the car's RIGHT side. Yaw +pi/2.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::LookAtRightSide(void* lpData)
{
    BehaviourDebugOrbitPlayer* const lpThis = static_cast<BehaviourDebugOrbitPlayer*>(lpData);
    lpThis->mfDistance = KF_LOOK_AT_DISTANCE;
    lpThis->mfYaw      = KF_HALF_PI;
    lpThis->mfPitch    = 0.0f;
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::Prepare
// Seed the orbit rig: distance + FOV, flag the behaviour active (base byte +8), and
// zero every orbit angle; report readiness (true). Store order follows the original build exactly.
// ----------------------------------------------------------------------------
bool BehaviourDebugOrbitPlayer::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mfDistance        = KF_LOOK_AT_DISTANCE;
    mbActive          = true;
    mfYaw             = 0.0f;
    mfPitch           = 0.0f;
    mfSecondaryPitch  = 0.0f;
    mfSecondaryYaw    = 0.0f;
    mfFOV             = KF_DEFAULT_FOV;
    mfSecondaryRoll   = 0.0f;
    return true;
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::SetupTweaker
// Wire the orbit-player debug cam into the live camera tweaker: reset the tweaker,
// then bind FOV / distance / yaw / pitch to controller axes and the four "look at
// <side>" snap callbacks to the D-pad. The original-build inlines every AddMapping /
// AddJustPressedMapping body (the NULL-var / NULL-func asserts land INSIDE those
// helper bodies, not here); the faithful source form of SetupTweaker is just the calls.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();

    // Axis bindings (constant scale).
    lrTweaker.AddMapping("FOV",             &mfFOV,      -0.5f,  Utils::Tweaker::E_AXIS_LOWER_TRIGGERS, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Distance to car", &mfDistance, -0.05f, Utils::Tweaker::E_AXIS_LEFT_STICK_Y,  Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Yaw",             &mfYaw,       0.02f, Utils::Tweaker::E_AXIS_RIGHT_STICK_X, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Pitch",           &mfPitch,     0.02f, Utils::Tweaker::E_AXIS_RIGHT_STICK_Y, Utils::Tweaker::E_MAP_NORMAL);

    // D-pad snap callbacks (just-pressed). userData is this behaviour instance.
    lrTweaker.AddJustPressedMapping("Look at front", &BehaviourDebugOrbitPlayer::LookAtFront,     this, Utils::DebugController::E_CONTROL_UP_DPAD,    Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Look at back",  &BehaviourDebugOrbitPlayer::LookAtBack,      this, Utils::DebugController::E_CONTROL_DOWN_DPAD,  Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Look at left",  &BehaviourDebugOrbitPlayer::LookAtLeftSide,  this, Utils::DebugController::E_CONTROL_LEFT_DPAD,  Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Look at right", &BehaviourDebugOrbitPlayer::LookAtRightSide, this, Utils::DebugController::E_CONTROL_RIGHT_DPAD, Utils::Tweaker::E_MAP_NORMAL);
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::Update
// Produce this frame's orbit camera around the tracked car:
//   - flag the produced camera as a live "following" camera (mState_uFlags |= 2),
//   - clamp the three tweakable rig values into their authored bands,
//   - build the orbit orientation from the primary pitch/yaw (no roll) and push the eye out
//     along its BACKWARD axis by distance SQUARED,
//   - look from that eye at the orbit centre, re-base the resulting frame onto the car (offset
//     by the car's world AABB centre, then compose with the car's world transform),
//   - apply the secondary pitch/yaw/roll on top, publish + validate the transform, publish the
//     clamped FOV. Always returns true.
//
// The original build splits this across an entry routine (the flag, the three clamps, the FOV
// publish) and an outlined vector-math tail that receives the already-clamped values, the car
// transform and the two AABB corners; the two are fused back into one function here, which is
// what the source shape was. Every constant below (the +/-1.5533431 pitch limit, the [0.1, 20]
// distance band, the [30, 120] FOV band, the -1 backward axis and the 0.5 AABB midpoint) is one
// the original build carries.
// ----------------------------------------------------------------------------
bool BehaviourDebugOrbitPlayer::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    // (1) Flag the produced camera as a live following camera this frame.
    lrCamera.mState_uFlags |= 2;

    // (2) Clamp the three tweakable rig values (store order follows the original build:
    //     pitch, then distance, then FOV; each is a lower clamp followed by an upper one).
    mfPitch    = (mfPitch    < -1.5533431f) ? -1.5533431f : mfPitch;
    mfPitch    = (mfPitch    >  1.5533431f) ?  1.5533431f : mfPitch;

    mfDistance = (mfDistance <  0.1f)  ?  0.1f  : mfDistance;
    mfDistance = (mfDistance > 20.0f)  ? 20.0f  : mfDistance;

    mfFOV      = (mfFOV      < 30.0f)  ? 30.0f  : mfFOV;
    mfFOV      = (mfFOV      > 120.0f) ? 120.0f : mfFOV;

    // (3) The orbit orientation: identity rotated by the primary pitch/yaw, roll pinned to 0.
    Matrix44Affine lOrbitRotation;
    lOrbitRotation.SetIdentity();
    Utils::RotateMatrix44AffineByEulerAnglesZXY(lOrbitRotation, Vector3{ mfPitch, mfYaw, 0.0f, 0.0f });

    // (4) The eye: the orbit frame's BACKWARD axis, pushed out by distance SQUARED (the squaring
    //     is what makes the stick-driven distance slider feel linear across its [0.1, 20] band).
    const Vector3 lEye =
        rw::math::vpu::Mult(
            rw::math::vpu::TransformPoint(lOrbitRotation, Vector3{ 0.0f, 0.0f, -1.0f, 0.0f }),
            mfDistance * mfDistance);

    // (5) Look from that eye back at the orbit centre (the frame's own origin).
    const Matrix44Affine lLookAt = Utils::CreateLookAt(lEye, Vector3{ 0.0f, 0.0f, 0.0f, 0.0f });

    // (6) Re-base onto the tracked car: shift the look-at frame's origin to the car's world AABB
    //     centre, then compose it with the car's world transform so the whole rig orbits the car.
    Matrix44Affine lOrbitFrame = lLookAt;
    lOrbitFrame.wAxis = lLookAt.wAxis
                      + rw::math::vpu::Lerp(lrInfo.mPlayerInfo.mAABB.mMin,
                                            lrInfo.mPlayerInfo.mAABB.mMax,
                                            0.5f);

    const Matrix44Affine lCarRelative =
        rw::math::vpu::Mult(lOrbitFrame, lrInfo.mPlayerInfo.mRaceCarState.mTransform);

    // (7) The secondary pitch/yaw/roll rides on top of the finished orbit frame.
    Matrix44Affine lSecondaryRotation;
    lSecondaryRotation.SetIdentity();
    Utils::RotateMatrix44AffineByEulerAnglesZXY(
        lSecondaryRotation, Vector3{ mfSecondaryPitch, mfSecondaryYaw, mfSecondaryRoll, 0.0f });

    // (8) Publish + validate the transform, then publish the clamped FOV (SetFOV carries the
    //     "FOV > 0" assert, which the clamp above already guarantees).
    lrCamera.SetTransform(rw::math::vpu::Mult(lSecondaryRotation, lCarRelative));
    lrCamera.ValidateTransformWithDebugInfo();

    lrCamera.SetFOV(mfFOV);

    return true;
}

} // namespace Camera
} // namespace BrnDirector
