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
// BLOCKED (not bodied here): Update @0x8222C618. Its asm reads the un-homed
// BrnDirector::Camera::BehaviourSharedInfo interior directly (a request-flag word @+0x140,
// a car transform matrix @+0x280, a float @+0x580 and a vehicle-info pointer @+0x5E0) and
// calls a GetImplicitVelocity(Vector3* out, VehicleInfo*) form whose asm signature
// contradicts the committed VehicleTracker::GetImplicitVelocity() const home. Bodying it
// faithfully would require forking those foreign types / fabricating accessors, so it is
// left to the Behaviour shared-info TU that homes them.
//
// Source-of-truth: ARTIST X360 pseudocode + ASM at the addresses above.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h"
#include "GameSource/Director/Camera/Utils/CameraUtils.h"   // Utils::CreateLookAt / EulerAnglesZXYFromMatrix44Affine

namespace BrnDirector
{
namespace Camera
{

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

} // namespace Camera
} // namespace BrnDirector
