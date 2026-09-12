// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.cpp
//
// Compilation home for BrnDirector::Camera::BehaviourDebugFlyWorld -- the developer
// "fly the world" debug camera. This TU bodies the whole class:
//   Construct / Prepare / Update / SetupTweaker / GetName   (the virtual interface)
//   WarpToLookAt                                            (snap eye + look-at frame)
//   ChangeMovingSpeed / WarpToCar / LookAtCar / LevelOut /
//   ToggleCarAttachment / ToggleSloMo / TakeScreenshot      (tweaker callbacks)
//
// Update reads three fields of the canonical BehaviourSharedInfo (Behaviour.h): the tracked
// car's world position (mPlayerInfo.mRaceCarState.mTransform.Pos()), the world frame delta
// (mTimestep.Get(E_WORLD)) that scales the follow velocity, and the player's VehicleTracker
// (mpPlayerTracker). All three used to be reached through file-local `SharedInfo_*` shims
// with no body anywhere; they are named members now.
//
// FLAG: the class is still a PRE-BASE FORK -- it carries its own vtable pointer and base-head
// padding instead of deriving from Camera::Behaviour. Nothing allocates it (the arbitrator's
// NewBehaviour<> calls are gated), so the fork is inert; retiring it is its own job, the same
// one BehaviourRoadRunner and BehaviourInterpolate already had.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h"
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"  // BehaviourSharedInfo (the canonical home)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"   // Utils::CreateLookAt / EulerAnglesZXYFromMatrix44Affine / RotateMatrix44AffineByEulerAnglesZXY
#include "GameSource/Director/Camera/Camera.h"              // BrnDirector::Camera::Camera (Update target -- homed)
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h" // VehicleTracker::GetImplicitVelocity() const
#include "rw/math/vpu/matrix44affine_operation.h"           // rw::math::vpu::TransformVector
#include "rw/math/vpu/vector3_operation.h"                  // Vector3 operator+ / MultAdd / Lerp

namespace BrnDirector
{
namespace Camera
{


// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::Construct
// Zero the Behaviour base head (reset word +4, flag bytes +8..+0xC, word +0x10), default the
// fly rig to "warp to the car" and speed-preset SLOW, then compute the SLOW speeds via
// ChangeMovingSpeed. Store order follows the original build.
// The original build does NOT touch mbLookAtCar@+0x7D -- left as-is here too.
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::Construct()
{
    // Base head zero-init (Behaviour base modelled as reserved; exact the original build stores).
    mBaseResetWord = 0;
    mbActive       = false;
    for (unsigned liI = 0; liI < sizeof(maBaseFlags); ++liI) maBaseFlags[liI] = 0;
    mBaseWord10    = 0;

    mbAttachedToCar  = false;
    mbWarpToCar      = true;
    mbUseSloMo       = false;
    meMoveSpeedState = E_MOVE_SPEED_TYPE_SLOW;

    ChangeMovingSpeed(this);

    mpParameters = 0;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::Prepare
// Seed the fly rig: zero the eye position + current position + the yaw/pitch/roll and the
// three translate axes, snap the speed preset to NORMAL, set the default FOV and flag the
// behaviour active (base byte +8); report readiness (true). Store order follows the original build.
// ----------------------------------------------------------------------------
bool BehaviourDebugFlyWorld::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mfYaw            = 0.0f;
    mfPitch          = 0.0f;
    meMoveSpeedState = E_MOVE_SPEED_TYPE_NORMAL;
    mfRoll           = 0.0f;
    mbActive         = true;
    mfX              = 0.0f;
    mfY              = 0.0f;
    mfZ              = 0.0f;
    mfFOV            = 90.0f;

    mPosition.SetZero();
    mCurrentPosition.SetZero();

    return true;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::GetName
// ----------------------------------------------------------------------------
const char* BehaviourDebugFlyWorld::GetName() const
{
    return "DebugFlyWorld";
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::ChangeMovingSpeed
// Tweaker "Change Moving Speed" callback: cycle the speed preset SLOW -> NORMAL -> FAST ->
// SLOW and recompute the six per-axis rig speeds from a per-preset translation/rotation
// scale. The original build materialises the base speeds, cycles meMoveSpeedState = (state+1) % 3,
// picks (translationScale, rotationScale) from it, then multiplies the base speeds by them.
//   SLOW(0)  -> (0.125, 0.5) ; NORMAL(1) -> (1.0, 1.0) ; FAST(2) -> (15.0, 1.0)
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::ChangeMovingSpeed(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);

    // Base per-axis speeds (before the per-preset scale).
    lpThis->mfMoveXSpeed = -0.75f;
    lpThis->mfMoveYSpeed =  0.75f;
    lpThis->mfMoveZSpeed =  0.75f;
    lpThis->mfYawSpeed   = -0.039999999f;
    lpThis->mfPitchSpeed = -0.039999999f;
    lpThis->mfRollSpeed  =  0.02f;

    // Cycle SLOW -> NORMAL -> FAST -> SLOW.
    lpThis->meMoveSpeedState =
        static_cast<EMoveSpeedType>((lpThis->meMoveSpeedState + 1) % 3);

    f32 lfTranslationScale;
    f32 lfRotationScale;
    switch (lpThis->meMoveSpeedState)
    {
    case E_MOVE_SPEED_TYPE_SLOW:  lfTranslationScale = 0.125f; lfRotationScale = 0.5f; break;
    case E_MOVE_SPEED_TYPE_FAST:  lfTranslationScale = 15.0f;  lfRotationScale = 1.0f; break;
    default:                      lfTranslationScale = 1.0f;   lfRotationScale = 1.0f; break; // NORMAL
    }

    // Apply the scale (the original build store order: mfMoveY, mfMoveX, mfMoveZ, mfYaw, mfRoll, mfPitch).
    lpThis->mfMoveYSpeed *= lfTranslationScale;
    lpThis->mfMoveXSpeed *= lfTranslationScale;
    lpThis->mfMoveZSpeed *= lfTranslationScale;
    lpThis->mfYawSpeed   *= lfRotationScale;
    lpThis->mfRollSpeed  *= lfRotationScale;
    lpThis->mfPitchSpeed *= lfRotationScale;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::WarpToCar
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::WarpToCar(void* lpData)
{
    static_cast<BehaviourDebugFlyWorld*>(lpData)->mbWarpToCar = true;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::LookAtCar
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::LookAtCar(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);
    lpThis->mbLookAtCar = !lpThis->mbLookAtCar;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::LevelOut
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::LevelOut(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);
    lpThis->mfPitch = 0.0f;
    lpThis->mfRoll  = 0.0f;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::ToggleCarAttachment
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::ToggleCarAttachment(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);
    lpThis->mbAttachedToCar = !lpThis->mbAttachedToCar;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::ToggleSloMo
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::ToggleSloMo(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);
    lpThis->mbUseSloMo = !lpThis->mbUseSloMo;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::WarpToLookAt
// Snap the rig to lEye looking at lLookAt: build the look-at frame, read back its ZXY Euler
// angles (near-vertical epsilon 0.0099999998), store the eye as the rig position, and split
// the angles across pitch/yaw/roll (the original build: angles[0]->mfPitch@+0x44, angles[1]->mfYaw@+0x40,
// angles[2]->mfRoll@+0x48).
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::WarpToLookAt(Vector3 lEye, Vector3 lLookAt)
{
    const Matrix44Affine lLookAtFrame = Utils::CreateLookAt(lEye, lLookAt);

    const Vector3 lAngles =
        Utils::EulerAnglesZXYFromMatrix44Affine(lLookAtFrame, 0, 0.0099999998f);

    mPosition = lEye;

    mfPitch = lAngles.x;
    mfYaw   = lAngles.y;
    mfRoll  = lAngles.z;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::TakeScreenshot -- the "Take Screenshot" tweaker callback.
// The shipped build folds this symbol onto an identical EMPTY routine (identical-code
// folding picked an unrelated empty function to carry it), so the recovered body is empty:
// the panorama capture it once drove is not part of this reconstruction.
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::TakeScreenshot(void* lpData)
{
    (void)lpData;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::SetupTweaker
// Wire the fly-world debug cam into the live camera tweaker: bind the six rig axes to
// controller sticks/triggers (each with its own live per-axis speed as the scale source),
// bind FOV to the left/right buttons with a constant scale, then bind the seven action
// callbacks to controller buttons. The original-build out-of-lines the six live-scale AddMapping calls
// and inlines the constant-scale FOV AddMapping + the seven AddJustPressedMapping bodies (the
// "lpfVariableToTweak != NULL" / "lpFunction != NULL" asserts land INSIDE those inlined
// helper bodies, not here); the faithful source is just the calls.
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    // Axis bindings (live per-axis speed as the scale source).
    lrTweaker.AddMapping("Yaw",                &mfYaw,   &mfYawSpeed,   Utils::Tweaker::E_AXIS_RIGHT_STICK_X, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Pitch",              &mfPitch, &mfPitchSpeed, Utils::Tweaker::E_AXIS_RIGHT_STICK_Y, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Roll",               &mfRoll,  &mfRollSpeed,  Utils::Tweaker::E_AXIS_UPPER_TRIGGERS, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Left/Right",         &mfX,     &mfMoveXSpeed, Utils::Tweaker::E_AXIS_LEFT_STICK_X, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Up/Down",            &mfY,     &mfMoveYSpeed, Utils::Tweaker::E_AXIS_LOWER_TRIGGERS, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Forwards/Backwards", &mfZ,     &mfMoveZSpeed, Utils::Tweaker::E_AXIS_LEFT_STICK_Y, Utils::Tweaker::E_MAP_NORMAL);

    // FOV binding (constant scale).
    lrTweaker.AddMapping("FOV", &mfFOV, 0.5f, Utils::Tweaker::E_AXIS_BUTTONS_LEFT_RIGHT, Utils::Tweaker::E_MAP_NORMAL);

    // Action callbacks (just-pressed). userData is this behaviour instance.
    lrTweaker.AddJustPressedMapping("Change Moving Speed", &BehaviourDebugFlyWorld::ChangeMovingSpeed,   this, Utils::DebugController::E_CONTROL_LEFT_STICK_BUTTON,  Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Attach To Car",       &BehaviourDebugFlyWorld::ToggleCarAttachment, this, Utils::DebugController::E_CONTROL_UP_DPAD,            Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Level Out",           &BehaviourDebugFlyWorld::LevelOut,            this, Utils::DebugController::E_CONTROL_DOWN_DPAD,          Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Warp To Car",         &BehaviourDebugFlyWorld::WarpToCar,           this, Utils::DebugController::E_CONTROL_LEFT_DPAD,          Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Look at Car",         &BehaviourDebugFlyWorld::LookAtCar,           this, Utils::DebugController::E_CONTROL_RIGHT_DPAD,         Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Toggle Slomo",        &BehaviourDebugFlyWorld::ToggleSloMo,         this, Utils::DebugController::E_CONTROL_DOWN_BUTTON,        Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Take Screenshot",     &BehaviourDebugFlyWorld::TakeScreenshot,      this, Utils::DebugController::E_CONTROL_RIGHT_STICK_BUTTON, Utils::Tweaker::E_MAP_NORMAL);
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::Update
// Integrate the free-fly rig into the produced camera this frame:
//   - flag the camera as a live "following" camera (mState_uFlags |= 2),
//   - clamp the rig FOV into [5, 120] degrees,
//   - advance the eye by the accumulated per-axis translate input through a yaw frame, ease the
//     produced eye toward it, and consume the input,
//   - build the full pitch/yaw/roll orientation,
//   - honour the warp-to-car (snap 4m behind the car), attach-to-car (add the car's implicit
//     velocity to the eye) and look-at-car (re-derive yaw/pitch/roll from a look-at) requests,
//   - assert + write the FOV, write the rig transform, validate it, and request the slo-mo
//     time-dilation. Always returns true.
//
// FLAG: the original build inlines the matrix/vector integration as one fused SIMD chain (the
//   same shape as the sibling BehaviourRig::Update). It is reconstructed here through the rw
//   math value operations for semantic parity; every store, branch, early flag-clear, call and
//   side-effect below is attested, and every constant used (5.0, 120.0, -4.0, 0.2, 1/30, 1.0
//   and the 0.0099999998 look-at epsilon) is one the original build carries.
// ----------------------------------------------------------------------------
bool BehaviourDebugFlyWorld::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    // (1) Flag the produced camera as a live following camera this frame.
    lrCamera.mState_uFlags |= 2;

    // (2) Clamp the rig field-of-view into the sensible [5, 120] degrees band.
    mfFOV = (mfFOV < 5.0f)   ? 5.0f   : mfFOV;
    mfFOV = (mfFOV > 120.0f) ? 120.0f : mfFOV;

    // (3) Move the eye by the accumulated local translate input, rotated into world space through
    //     a yaw-only frame; then ease the produced eye toward the target eye (lerp 0.2) and
    //     consume the input.
    Matrix44Affine lYawFrame;
    lYawFrame.SetIdentity();
    Utils::RotateMatrix44AffineByEulerAnglesZXY(lYawFrame, Vector3{ 0.0f, mfYaw, 0.0f, 0.0f });

    const Vector3 lLocalMove{ mfX, mfY, mfZ, 0.0f };
    mPosition        = mPosition + rw::math::vpu::TransformVector(lYawFrame, lLocalMove);
    mCurrentPosition = rw::math::vpu::Lerp(mCurrentPosition, mPosition, 0.2f);

    mfX = 0.0f;
    mfY = 0.0f;
    mfZ = 0.0f;

    // (4) Build the full rig orientation.
    Matrix44Affine lOrientation;
    lOrientation.SetIdentity();
    Utils::RotateMatrix44AffineByEulerAnglesZXY(lOrientation, Vector3{ mfPitch, mfYaw, mfRoll, 0.0f });

    // (5) Warp-to-car request: clear it and snap the eye 4m behind the car along the rig's forward
    //     axis: if (mbWarpToCar) { clear it; eye = forward * -4.0 + carPos }.
    if (mbWarpToCar)
    {
        mbWarpToCar = false;
        const Vector3 lCarPosition = lrInfo.mPlayerInfo.mRaceCarState.mTransform.Pos();
        mPosition = rw::math::vpu::MultAdd(lOrientation.At(),
                                           Vector3{ -4.0f, -4.0f, -4.0f, -4.0f },
                                           lCarPosition);
    }

    // (6) Attach-to-car request: add the tracked car's implicit velocity, scaled by this frame's
    //     world delta, into both the target and the produced eye. The original build reads the
    //     tracker's implicit velocity twice, once per eye, each a multiply-add by the broadcast
    //     scale.
    if (mbAttachedToCar)
    {
        const VehicleTracker* const lpTracker = lrInfo.mpPlayerTracker;

        const f32 lfScaleA = lrInfo.mTimestep.Get(BrnDirector::Timestep::E_WORLD);
        const Vector3 lVelocityA = lpTracker->GetImplicitVelocity();
        mPosition = rw::math::vpu::MultAdd(lVelocityA, mPosition,
                                           Vector3{ lfScaleA, lfScaleA, lfScaleA, lfScaleA });

        const f32 lfScaleB = lrInfo.mTimestep.Get(BrnDirector::Timestep::E_WORLD);   // re-read
        const Vector3 lVelocityB = lpTracker->GetImplicitVelocity();     // re-read
        mCurrentPosition = rw::math::vpu::MultAdd(lVelocityB, mCurrentPosition,
                                                  Vector3{ lfScaleB, lfScaleB, lfScaleB, lfScaleB });
    }

    // (7) Look-at-car request: re-derive the rig yaw/pitch/roll from a look-at frame that points
    //     the eye at the car. Angle split matches WarpToLookAt: angles[0]->mfPitch,
    //     angles[1]->mfYaw, angles[2]->mfRoll (near-vertical epsilon 0.0099999998).
    if (mbLookAtCar)
    {
        const Vector3 lCarPosition = lrInfo.mPlayerInfo.mRaceCarState.mTransform.Pos();
        const Matrix44Affine lLookAt = Utils::CreateLookAt(mPosition, lCarPosition);
        const Vector3 lAngles = Utils::EulerAnglesZXYFromMatrix44Affine(lLookAt, 0, 0.0099999998f);

        mfPitch = lAngles.x;
        mfYaw   = lAngles.y;
        mfRoll  = lAngles.z;
    }
    // (8) Publish the clamped FOV (SetFOV carries the "FOV > 0" assert the clamp already
    //     the clamp above already guarantees it), publish the produced transform (orientation rows +
    //     the smoothed eye as the translation) and validate it.
    Matrix44Affine lCameraTransform;
    lCameraTransform.Right() = lOrientation.Right();   // -> camera +0x00
    lCameraTransform.Up()    = lOrientation.Up();      // -> camera +0x10
    lCameraTransform.At()    = lOrientation.At();      // -> camera +0x20
    lCameraTransform.Pos()   = mCurrentPosition;       // -> camera +0x30

    lrCamera.SetFOV(mfFOV);
    lrCamera.SetTransform(lCameraTransform);
    lrCamera.ValidateTransformWithDebugInfo();         // bl

    // (9) Request the slo-mo time-dilation (the original build: mbUseSloMo ? 1/30 : 1.0 -> cam +0x104 ==
    //     mEffects+0x9C == SetRequestedTimeDilation).
    lrCamera.SetRequestedTimeDilation(mbUseSloMo ? 0.033333335f : 1.0f);

    return true;
}

} // namespace Camera
} // namespace BrnDirector
