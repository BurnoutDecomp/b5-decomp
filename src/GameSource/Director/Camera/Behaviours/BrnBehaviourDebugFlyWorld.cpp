// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourDebugFlyWorld slices.
// This TU bodies:
//   Construct           @0x82210088 (virtual; zero the base head + rig, warp-to-car on)
//   Prepare             @0x821FB738 (virtual; seed the fly rig, flag active)
//   SetupTweaker        @0x822100E0 (virtual; wire the rig into the camera tweaker)
//   GetName             @0x821FB728 (virtual; the literal behaviour name)
//   WarpToLookAt        @0x8222CB10 (snap eye + derive yaw/pitch/roll from a look-at frame)
//   ChangeMovingSpeed   @0x821FB790 (static tweaker callback; cycle the speed preset)
//   WarpToCar           @0x821FB860 (static tweaker callback)
//   LookAtCar           @0x821FB870 (static tweaker callback)
//   LevelOut            @0x821FB888 (static tweaker callback)
//   ToggleCarAttachment @0x821FB8A0 (static tweaker callback)
//   ToggleSloMo         @0x821FB8B8 (static tweaker callback)
//
//   Update              @0x8222C618 (virtual; integrate the fly rig into the produced camera)
//
// Update's three original blockers are all cleared:
//   (1) The request-flag word it `ld/ori 2/std`s is on the CAMERA, not the shared info
//       (asm: `ld r6,0x140(r27)` where r27 == lrCamera). Camera +0x140 == the committed
//       BrnDirector::Camera::Camera::mState_uFlags (Camera.h). The rest of its camera writes --
//       transform rows @+0x00..+0x30, mfFOV @+0x58 (with the "lfFOV > 0.0f" assert, Camera.h:424,
//       carried by SetFOV), the slo-mo time-dilation request @+0x104 (== mEffects+0x9C ==
//       SetRequestedTimeDilation), and BrnDirector::Camera::Camera::ValidateTransformWithDebugInfo
//       @0x8220A850 -- all resolve to the now-homed Camera.
//   (2) The `GetImplicitVelocity(out, veh)` form is NOT a contradicting overload: it is the
//       committed VehicleTracker::GetImplicitVelocity() const returning a Vector3 by value
//       (PPC struct-return: r3 = hidden return slot = &out, r4 = the VehicleTracker* `this`).
//       Hex-Rays rendered the sret+this pair as `(a1, a2)`. No fork -- called as `tracker->
//       GetImplicitVelocity()` on the tracker pointer read from the shared info.
//   (3) The BrnDirector::Camera::BehaviourSharedInfo interior has NO canonical committed home --
//       only divergent temporary per-behaviour minimal slices (BehaviourRig.h / BrnBehaviourIceAnim.h)
//       modelling unrelated accessors. Update reads three foreign fields: the tracked-car world
//       position @+0x280 (CreateLookAt target / warp-to-car source), a per-frame float @+0x580 whose
//       ROLE IS NOT RECOVERED (the velocity-integration scale), and the VehicleTracker* @+0x5E0.
//       These are reached BY NAMED ACCESSOR through the file-local `detail::` shim below -- the
//       accepted pattern the committed sibling BehaviourBystanderCam.cpp uses for the same foreign
//       BehaviourSharedInfo, keeping the type opaque (the owning TU pins the offsets). No fork.
//
// Source-of-truth: ARTIST X360 pseudocode + ASM at the addresses above.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"   // Utils::CreateLookAt / EulerAnglesZXYFromMatrix44Affine / RotateMatrix44AffineByEulerAnglesZXY
#include "GameSource/Director/Camera/Camera.h"              // BrnDirector::Camera::Camera (Update target -- homed)
#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h" // VehicleTracker::GetImplicitVelocity() const
#include "rw/math/vpu/matrix44affine_operation.h"           // rw::math::vpu::TransformVector
#include "rw/math/vpu/vector3_operation.h"                  // Vector3 operator+ / MultAdd / Lerp

namespace BrnDirector
{
namespace Camera
{

namespace detail
{
    // ---- foreign per-frame BehaviourSharedInfo fields Update reads (the owning shared-info TU
    //      pins these offsets; the type stays opaque here). Named to read faithfully against the
    //      asm, mirroring the committed BehaviourBystanderCam.cpp `SharedInfo_*` precedent.
    //      Declaration-only -- the per-TU `cl /c` gate does not link; bodies land with the owner. ----
    Vector3 SharedInfo_GetTrackedCarPosition(const BehaviourSharedInfo* lpInfo);   // info +0x280 (lvx  r26,0x280)
    f32     SharedInfo_GetFollowVelocityScale(const BehaviourSharedInfo* lpInfo);  // info +0x580 (lfs  0x580; role unrecovered)
    const BrnDirector::VehicleTracker*
            SharedInfo_GetVehicleTracker(const BehaviourSharedInfo* lpInfo);       // info +0x5E0 (lwz  0x5E0)
}
using namespace detail;

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::Construct @0x82210088
// Zero the Behaviour base head (reset word +4, flag bytes +8..+0xC, word +0x10), default the
// fly rig to "warp to the car" and speed-preset SLOW, then compute the SLOW speeds via
// ChangeMovingSpeed. Store order follows the asm.
//   stb 0, 8/9/0xA/0xB/0xC ; stw 0, 4 ; stw 0, 0x10   -- base head
//   stb 0, 0x7E (mbAttachedToCar) ; stb 1, 0x7C (mbWarpToCar) ; stb 0, 0x7F (mbUseSloMo)
//   stw 0, 0x74 (meMoveSpeedState = SLOW) ; bl ChangeMovingSpeed ; stw 0, 0x78 (mpParameters)
// The asm does NOT touch mbLookAtCar@+0x7D -- left as-is here too.
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::Construct()
{
    // Base head zero-init (Behaviour base modelled as reserved; exact asm stores).
    mBaseResetWord = 0;                                                           // stw 0, 4
    mbActive       = false;                                                       // stb 0, 8
    for (unsigned liI = 0; liI < sizeof(maBaseFlags); ++liI) maBaseFlags[liI] = 0; // stb 0, 9..0xC
    mBaseWord10    = 0;                                                           // stw 0, 0x10

    mbAttachedToCar  = false;                        // stb 0, 0x7E
    mbWarpToCar      = true;                         // stb 1, 0x7C
    mbUseSloMo       = false;                        // stb 0, 0x7F
    meMoveSpeedState = E_MOVE_SPEED_TYPE_SLOW;       // stw 0, 0x74

    ChangeMovingSpeed(this);                         // bl ChangeMovingSpeed (r3 = this)

    mpParameters = 0;                                // stw 0, 0x78
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::Prepare @0x821FB738
// Seed the fly rig: zero the eye position + current position + the yaw/pitch/roll and the
// three translate axes, snap the speed preset to NORMAL, set the default FOV and flag the
// behaviour active (base byte +8); report readiness (true). Store order follows the asm.
// ----------------------------------------------------------------------------
bool BehaviourDebugFlyWorld::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mfYaw            = 0.0f;                     // stfs 0.0, 0x40
    mfPitch          = 0.0f;                     // stfs 0.0, 0x44
    meMoveSpeedState = E_MOVE_SPEED_TYPE_NORMAL; // stw  1,   0x74
    mfRoll           = 0.0f;                     // stfs 0.0, 0x48
    mbActive         = true;                     // stb  1,   8
    mfX              = 0.0f;                     // stfs 0.0, 0x4C
    mfY              = 0.0f;                     // stfs 0.0, 0x50
    mfZ              = 0.0f;                     // stfs 0.0, 0x54
    mfFOV            = 90.0f;                    // stfs 90.0, 0x70 (flt_82004F64)

    mPosition.SetZero();                         // stvx 0, 0x20
    mCurrentPosition.SetZero();                  // stvx 0, 0x30

    return true;                                 // li r3, 1
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::GetName @0x821FB728
//   lis/addi r3, aDebugflyworld ; blr -> the literal behaviour name.
// ----------------------------------------------------------------------------
const char* BehaviourDebugFlyWorld::GetName() const
{
    return "DebugFlyWorld";
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::ChangeMovingSpeed @0x821FB790
// Tweaker "Change Moving Speed" callback: cycle the speed preset SLOW -> NORMAL -> FAST ->
// SLOW and recompute the six per-axis rig speeds from a per-preset translation/rotation
// scale. The asm materialises the base speeds, cycles meMoveSpeedState = (state+1) % 3,
// picks (translationScale, rotationScale) from it, then multiplies the base speeds by them.
//   SLOW(0)  -> (0.125, 0.5) ; NORMAL(1) -> (1.0, 1.0) ; FAST(2) -> (15.0, 1.0)
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::ChangeMovingSpeed(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);

    // Base per-axis speeds (before the per-preset scale).
    lpThis->mfMoveXSpeed = -0.75f;          // flt_8200557C
    lpThis->mfMoveYSpeed =  0.75f;          // flt_82004018
    lpThis->mfMoveZSpeed =  0.75f;
    lpThis->mfYawSpeed   = -0.039999999f;   // flt_82005578
    lpThis->mfPitchSpeed = -0.039999999f;
    lpThis->mfRollSpeed  =  0.02f;          // flt_82005574

    // Cycle SLOW -> NORMAL -> FAST -> SLOW.
    lpThis->meMoveSpeedState =
        static_cast<EMoveSpeedType>((lpThis->meMoveSpeedState + 1) % 3);

    f32 lfTranslationScale;
    f32 lfRotationScale;
    switch (lpThis->meMoveSpeedState)
    {
    case E_MOVE_SPEED_TYPE_SLOW:  lfTranslationScale = 0.125f; lfRotationScale = 0.5f; break; // flt_82004010 / flt_82001DA0
    case E_MOVE_SPEED_TYPE_FAST:  lfTranslationScale = 15.0f;  lfRotationScale = 1.0f; break; // flt_820047C4 / flt_82001C98
    default:                      lfTranslationScale = 1.0f;   lfRotationScale = 1.0f; break; // NORMAL (flt_82001C98)
    }

    // Apply the scale (asm store order: mfMoveY, mfMoveX, mfMoveZ, mfYaw, mfRoll, mfPitch).
    lpThis->mfMoveYSpeed *= lfTranslationScale;
    lpThis->mfMoveXSpeed *= lfTranslationScale;
    lpThis->mfMoveZSpeed *= lfTranslationScale;
    lpThis->mfYawSpeed   *= lfRotationScale;
    lpThis->mfRollSpeed  *= lfRotationScale;
    lpThis->mfPitchSpeed *= lfRotationScale;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::WarpToCar @0x821FB860
//   li r11,1 ; stb r11,0x7C -> request a warp to the car next frame.
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::WarpToCar(void* lpData)
{
    static_cast<BehaviourDebugFlyWorld*>(lpData)->mbWarpToCar = true;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::LookAtCar @0x821FB870
//   lbz/cntlzw/extrwi/stb 0x7D -> mbLookAtCar = (mbLookAtCar == 0)  (a toggle).
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::LookAtCar(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);
    lpThis->mbLookAtCar = !lpThis->mbLookAtCar;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::LevelOut @0x821FB888
//   stfs 0.0, 0x44 (mfPitch) ; stfs 0.0, 0x48 (mfRoll) -> level the rig.
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::LevelOut(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);
    lpThis->mfPitch = 0.0f;   // stfs 0.0, 0x44
    lpThis->mfRoll  = 0.0f;   // stfs 0.0, 0x48
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::ToggleCarAttachment @0x821FB8A0
//   lbz/cntlzw/extrwi/stb 0x7E -> mbAttachedToCar = (mbAttachedToCar == 0)  (a toggle).
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::ToggleCarAttachment(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);
    lpThis->mbAttachedToCar = !lpThis->mbAttachedToCar;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::ToggleSloMo @0x821FB8B8
//   lbz/cntlzw/extrwi/stb 0x7F -> mbUseSloMo = (mbUseSloMo == 0)  (a toggle).
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::ToggleSloMo(void* lpData)
{
    BehaviourDebugFlyWorld* const lpThis = static_cast<BehaviourDebugFlyWorld*>(lpData);
    lpThis->mbUseSloMo = !lpThis->mbUseSloMo;
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::WarpToLookAt @0x8222CB10
// Snap the rig to lEye looking at lLookAt: build the look-at frame, read back its ZXY Euler
// angles (near-vertical epsilon 0.0099999998), store the eye as the rig position, and split
// the angles across pitch/yaw/roll (asm: angles[0]->mfPitch@+0x44, angles[1]->mfYaw@+0x40,
// angles[2]->mfRoll@+0x48).
// ----------------------------------------------------------------------------
void BehaviourDebugFlyWorld::WarpToLookAt(Vector3 lEye, Vector3 lLookAt)
{
    const Matrix44Affine lLookAtFrame = Utils::CreateLookAt(lEye, lLookAt);

    const Vector3 lAngles =
        Utils::EulerAnglesZXYFromMatrix44Affine(lLookAtFrame, 0, 0.0099999998f);

    mPosition = lEye;             // stvx128 v127(eye), 0x20

    mfPitch = lAngles.x;          // stfs angles[0], 0x44
    mfYaw   = lAngles.y;          // stfs angles[1], 0x40
    mfRoll  = lAngles.z;          // stfs angles[2], 0x48
}

// ----------------------------------------------------------------------------
// BehaviourDebugFlyWorld::SetupTweaker @0x822100E0
// Wire the fly-world debug cam into the live camera tweaker: bind the six rig axes to
// controller sticks/triggers (each with its own live per-axis speed as the scale source),
// bind FOV to the left/right buttons with a constant scale, then bind the seven action
// callbacks to controller buttons. The X360 out-of-lines the six live-scale AddMapping calls
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

    // FOV binding (constant scale). "" is the &unk_820051C0 empty-name sentinel.
    lrTweaker.AddMapping("", &mfFOV, 0.5f, Utils::Tweaker::E_AXIS_BUTTONS_LEFT_RIGHT, Utils::Tweaker::E_MAP_NORMAL);

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
// BehaviourDebugFlyWorld::Update @0x8222C618
// Integrate the free-fly rig into the produced camera this frame:
//   - flag the camera as a live "following" camera (mState_uFlags |= 2),
//   - clamp the rig FOV into [5, 120] degrees,
//   - advance the eye by the accumulated per-axis translate input through a yaw frame, ease the
//     produced eye toward it, and consume the input,
//   - build the full pitch/yaw/roll orientation,
//   - honour the warp-to-car (snap 4m behind the car), attach-to-car (add the car's implicit
//     velocity to the eye) and look-at-car (re-derive yaw/pitch/roll from a look-at) requests,
//   - assert + write the FOV, write the rig transform, validate it, and request the slo-mo
//     time-dilation. Always returns true (li r3,1).
//
// FLAG: the X360 body inlines the matrix->vector integration as a register-allocation-failed VMX
//   vmaddfp chain ("local variable allocation has failed" in Hex-Rays, as in the sibling
//   BehaviourRig::Update). The inlined matrix-vector math is reconstructed through the rw math
//   value operations for semantic parity; every store, branch, early flag-clear, call and
//   side-effect below is asm-attested, and only asm-attested constants are used (5.0 flt_8200426C,
//   120.0 flt_82004A28, -4.0 flt_820089E0, 0.2 flt_82004744, 1/30 flt_82001778, 1.0 flt_82001CC0,
//   look-at epsilon 0.0099999998 flt_82002138).
// ----------------------------------------------------------------------------
bool BehaviourDebugFlyWorld::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    const BehaviourSharedInfo* const lpInfo = &lrInfo;

    // (1) Flag the produced camera as a live following camera this frame.
    //     asm 0x8222C654: ld r6,0x140(cam); ori r6,r6,2; std r6,0x140(cam).
    lrCamera.mState_uFlags |= 2;

    // (2) Clamp the rig field-of-view into the sensible [5, 120] degrees band (asm 0x8222C72C:
    //     fsel max(fov,5.0) then fsel min(.,120.0)).
    mfFOV = (mfFOV < 5.0f)   ? 5.0f   : mfFOV;
    mfFOV = (mfFOV > 120.0f) ? 120.0f : mfFOV;

    // (3) Move the eye by the accumulated local translate input, rotated into world space through
    //     a yaw-only frame (asm 0x8222C748 rotates by euler {0, mfYaw, 0}); then ease the produced
    //     eye toward the target eye (asm broadcast lane flt_82004744 = 0.2) and consume the input
    //     (asm 0x8222C8D8: stfs 0 -> +0x4C/+0x50/+0x54).
    Matrix44Affine lYawFrame;
    lYawFrame.SetIdentity();
    Utils::RotateMatrix44AffineByEulerAnglesZXY(lYawFrame, Vector3{ 0.0f, mfYaw, 0.0f, 0.0f });

    const Vector3 lLocalMove{ mfX, mfY, mfZ, 0.0f };
    mPosition        = mPosition + rw::math::vpu::TransformVector(lYawFrame, lLocalMove);
    mCurrentPosition = rw::math::vpu::Lerp(mCurrentPosition, mPosition, 0.2f);

    mfX = 0.0f;
    mfY = 0.0f;
    mfZ = 0.0f;

    // (4) Build the full rig orientation (asm 0x8222C8F0 rotates by euler {mfPitch, mfYaw, mfRoll}).
    Matrix44Affine lOrientation;
    lOrientation.SetIdentity();
    Utils::RotateMatrix44AffineByEulerAnglesZXY(lOrientation, Vector3{ mfPitch, mfYaw, mfRoll, 0.0f });

    // (5) Warp-to-car request: clear it and snap the eye 4m behind the car along the rig's forward
    //     axis (asm 0x8222C914: if (mbWarpToCar) { stb 0,0x7C; eye = fwd*(-4.0) + carPos }).
    if (mbWarpToCar)
    {
        mbWarpToCar = false;                                              // stb 0, 0x7C
        const Vector3 lCarPosition = SharedInfo_GetTrackedCarPosition(lpInfo);   // lvx r26,0x280
        mPosition = rw::math::vpu::MultAdd(lOrientation.At(),
                                           Vector3{ -4.0f, -4.0f, -4.0f, -4.0f },
                                           lCarPosition);                 // vmaddfp128 v0,v126,broadcast(-4.0),carPos
    }

    // (6) Attach-to-car request: add the tracked car's implicit velocity (scaled by the per-frame
    //     factor @+0x580) into both the target and produced eye (asm 0x8222C958: two
    //     VehicleTracker::GetImplicitVelocity() reads on the tracker @+0x5E0, each vmaddfp of the
    //     velocity by the eye plus the broadcast scale).
    if (mbAttachedToCar)
    {
        const BrnDirector::VehicleTracker* const lpTracker = SharedInfo_GetVehicleTracker(lpInfo); // lwz r26,0x5E0

        const f32 lfScaleA = SharedInfo_GetFollowVelocityScale(lpInfo);  // lfs r26,0x580
        const Vector3 lVelocityA = lpTracker->GetImplicitVelocity();     // r3=&out, r4=tracker
        mPosition = rw::math::vpu::MultAdd(lVelocityA, mPosition,
                                           Vector3{ lfScaleA, lfScaleA, lfScaleA, lfScaleA });

        const f32 lfScaleB = SharedInfo_GetFollowVelocityScale(lpInfo);  // lfs r26,0x580 (re-read)
        const Vector3 lVelocityB = lpTracker->GetImplicitVelocity();     // r3=&out, r4=tracker (re-read)
        mCurrentPosition = rw::math::vpu::MultAdd(lVelocityB, mCurrentPosition,
                                                  Vector3{ lfScaleB, lfScaleB, lfScaleB, lfScaleB });
    }

    // (7) Look-at-car request: re-derive the rig yaw/pitch/roll from a look-at frame that points
    //     the eye at the car (asm 0x8222C9F4). Angle split matches WarpToLookAt: angles[0]->mfPitch,
    //     angles[1]->mfYaw, angles[2]->mfRoll (near-vertical epsilon 0.0099999998, flt_82002138).
    if (mbLookAtCar)
    {
        const Vector3 lCarPosition = SharedInfo_GetTrackedCarPosition(lpInfo);   // lvx r26,0x280
        const Matrix44Affine lLookAt = Utils::CreateLookAt(mPosition, lCarPosition);
        const Vector3 lAngles = Utils::EulerAnglesZXYFromMatrix44Affine(lLookAt, 0, 0.0099999998f);

        mfPitch = lAngles.x;   // stfs var_1A0, 0x44
        mfYaw   = lAngles.y;   // stfs var_19C, 0x40
        mfRoll  = lAngles.z;   // stfs var_198, 0x48
    }

    // (8) Assert + publish the FOV (asm 0x8222CA7C: the "lfFOV > 0.0f" assert is SetFOV's, Camera.h:424;
    //     the clamp above already guarantees it), publish the produced transform (orientation rows +
    //     the smoothed eye as the translation) and validate it (asm 0x8222CAC4, @0x8220A850).
    Matrix44Affine lCameraTransform;
    lCameraTransform.Right() = lOrientation.Right();   // v90  -> cam +0x00
    lCameraTransform.Up()    = lOrientation.Up();      // v91  -> cam +0x10
    lCameraTransform.At()    = lOrientation.At();      // v126 -> cam +0x20
    lCameraTransform.Pos()   = mCurrentPosition;       // v127 -> cam +0x30

    lrCamera.SetFOV(mfFOV);                            // stfs f29, 0x58(cam) (carries the FOV>0 assert)
    lrCamera.SetTransform(lCameraTransform);           // stvx rows -> cam +0x00..+0x30
    lrCamera.ValidateTransformWithDebugInfo();         // bl 0x8220A850

    // (9) Request the slo-mo time-dilation (asm 0x8222CAD4: mbUseSloMo ? 1/30 : 1.0 -> cam +0x104 ==
    //     mEffects+0x9C == SetRequestedTimeDilation).
    lrCamera.SetRequestedTimeDilation(mbUseSloMo ? 0.033333335f : 1.0f);

    return true;                                       // li r3, 1
}

} // namespace Camera
} // namespace BrnDirector
