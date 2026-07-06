// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourDebugOrbitPlayer slices.
// This TU bodies:
//   Construct       @0x821FB610 (virtual; zero the rig head + cached param word + mpParameters)
//   GetName         @0x821FB680 (virtual; the literal behaviour name)
//   LookAtFront     @0x821FB690 (static dev-tweaker preset callback)
//   LookAtBack      @0x821FB6B8 (static dev-tweaker preset callback)
//   LookAtLeftSide  @0x821FB6D8 (static dev-tweaker preset callback)
//   LookAtRightSide @0x821FB700 (static dev-tweaker preset callback)
//   Prepare         @0x821FB638 (virtual; seed the orbit rig, flag active)
//   SetupTweaker    @0x8220FE00 (virtual; wire the rig into the camera tweaker)
// The remaining behaviour (Update / SetParameters / Parameters::Construct) lands with
// sibling waves.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h"

namespace BrnDirector
{
namespace Camera
{

// ---- X360 rodata constants (out-of-line definitions) -----------------------
const f32 BehaviourDebugOrbitPlayer::KF_LOOK_AT_DISTANCE = 2.5f;        // flt_82005548
const f32 BehaviourDebugOrbitPlayer::KF_HALF_PI          = 1.5707964f;  // flt_82001754
const f32 BehaviourDebugOrbitPlayer::KF_DEFAULT_FOV      = 90.0f;       // flt_82004F64

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::Construct @0x821FB610
//   li  r11,0
//   stb r11,8/9/0xA/0xB/0xC(this)  ; base head flag bytes
//   stw r11,4(this)                ; base head word
//   stw r11,0x10(this)             ; cached param word
//   stw r11,0x30(this)             ; mpParameters = NULL
// The named model folds the base flag/word head into reserved bytes; the stores land
// as the zero-init of that reserved head plus mbActive + mpParameters.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::Construct()
{
    for (unsigned liI = 0; liI < sizeof(maReserved04); ++liI) maReserved04[liI] = 0; // +0x04..+0x07 (stw 0, 4)
    mbActive = false;                                                                // stb 0, 8
    for (unsigned liI = 0; liI < sizeof(maReserved09); ++liI) maReserved09[liI] = 0; // +0x09..+0x13 (stb 0 x4, stw 0, 0x10)
    mpParameters = 0;                                                                // stw 0, 0x30
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::GetName @0x821FB680
//   lis/addi r3, aDebugorbitplay ; blr  -> the literal behaviour name
// ----------------------------------------------------------------------------
const char* BehaviourDebugOrbitPlayer::GetName() const
{
    return "DebugOrbitPlayer";
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::LookAtFront @0x821FB690
// Tweaker "Look at front" preset: distance 2.5, yaw pi, pitch 0.
//   stfs 2.5,       0x18(this)  ; mfDistance
//   stfs 3.1415927, 0x1C(this)  ; mfYaw = pi
//   stfs 0.0,       0x20(this)  ; mfPitch
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::LookAtFront(void* lpData)
{
    BehaviourDebugOrbitPlayer* const lpThis = static_cast<BehaviourDebugOrbitPlayer*>(lpData);
    lpThis->mfDistance = KF_LOOK_AT_DISTANCE;   // stfs 2.5       -> 0x18
    lpThis->mfYaw      = 3.1415927f;            // stfs pi        -> 0x1C
    lpThis->mfPitch    = 0.0f;                  // stfs 0.0       -> 0x20
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::LookAtBack @0x821FB6B8
// Tweaker "Look at back" preset: distance 2.5, yaw 0, pitch 0.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::LookAtBack(void* lpData)
{
    BehaviourDebugOrbitPlayer* const lpThis = static_cast<BehaviourDebugOrbitPlayer*>(lpData);
    lpThis->mfDistance = KF_LOOK_AT_DISTANCE;   // stfs 2.5 -> 0x18
    lpThis->mfYaw      = 0.0f;                  // stfs 0.0 -> 0x1C
    lpThis->mfPitch    = 0.0f;                  // stfs 0.0 -> 0x20
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::LookAtLeftSide @0x821FB6D8
// D-pad left: orbit to the car's LEFT side. Yaw -pi/2.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::LookAtLeftSide(void* lpData)
{
    BehaviourDebugOrbitPlayer* const lpThis = static_cast<BehaviourDebugOrbitPlayer*>(lpData);
    lpThis->mfDistance = KF_LOOK_AT_DISTANCE;   // stfs 2.5        -> 0x18
    lpThis->mfYaw      = -KF_HALF_PI;           // stfs -1.5707964 -> 0x1C
    lpThis->mfPitch    = 0.0f;                  // stfs 0.0        -> 0x20
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::LookAtRightSide @0x821FB700
// D-pad right: orbit to the car's RIGHT side. Yaw +pi/2.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::LookAtRightSide(void* lpData)
{
    BehaviourDebugOrbitPlayer* const lpThis = static_cast<BehaviourDebugOrbitPlayer*>(lpData);
    lpThis->mfDistance = KF_LOOK_AT_DISTANCE;   // stfs 2.5       -> 0x18
    lpThis->mfYaw      = KF_HALF_PI;            // stfs 1.5707964 -> 0x1C
    lpThis->mfPitch    = 0.0f;                  // stfs 0.0       -> 0x20
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::Prepare @0x821FB638
// Seed the orbit rig: distance + FOV, flag the behaviour active (base byte +8), and
// zero every orbit angle; report readiness (true). Store order follows the asm exactly.
// ----------------------------------------------------------------------------
bool BehaviourDebugOrbitPlayer::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mfDistance        = KF_LOOK_AT_DISTANCE;  // stfs 2.5  -> 0x18
    mbActive          = true;                 // stb  1    -> 8
    mfYaw             = 0.0f;                  // stfs 0.0  -> 0x1C
    mfPitch           = 0.0f;                  // stfs 0.0  -> 0x20
    mfSecondaryPitch  = 0.0f;                  // stfs 0.0  -> 0x28
    mfSecondaryYaw    = 0.0f;                  // stfs 0.0  -> 0x24
    mfFOV             = KF_DEFAULT_FOV;        // stfs 90.0 -> 0x14
    mfSecondaryRoll   = 0.0f;                  // stfs 0.0  -> 0x2C
    return true;                              // li r3, 1
}

// ----------------------------------------------------------------------------
// BehaviourDebugOrbitPlayer::SetupTweaker @0x8220FE00
// Wire the orbit-player debug cam into the live camera tweaker: reset the tweaker,
// then bind FOV / distance / yaw / pitch to controller axes and the four "look at
// <side>" snap callbacks to the D-pad. The X360 inlines every AddMapping /
// AddJustPressedMapping body (the NULL-var / NULL-func asserts land INSIDE those
// helper bodies, not here); the faithful source form of SetupTweaker is just the calls.
// ----------------------------------------------------------------------------
void BehaviourDebugOrbitPlayer::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();

    // Axis bindings (constant scale). "" is the &unk_820051C0 empty-name sentinel.
    lrTweaker.AddMapping("",                &mfFOV,      -0.5f,  Utils::Tweaker::E_AXIS_LOWER_TRIGGERS, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Distance to car", &mfDistance, -0.05f, Utils::Tweaker::E_AXIS_LEFT_STICK_Y,  Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Yaw",             &mfYaw,       0.02f, Utils::Tweaker::E_AXIS_RIGHT_STICK_X, Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddMapping("Pitch",           &mfPitch,     0.02f, Utils::Tweaker::E_AXIS_RIGHT_STICK_Y, Utils::Tweaker::E_MAP_NORMAL);

    // D-pad snap callbacks (just-pressed). userData is this behaviour instance.
    lrTweaker.AddJustPressedMapping("Look at front", &BehaviourDebugOrbitPlayer::LookAtFront,     this, Utils::DebugController::E_CONTROL_UP_DPAD,    Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Look at back",  &BehaviourDebugOrbitPlayer::LookAtBack,      this, Utils::DebugController::E_CONTROL_DOWN_DPAD,  Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Look at left",  &BehaviourDebugOrbitPlayer::LookAtLeftSide,  this, Utils::DebugController::E_CONTROL_LEFT_DPAD,  Utils::Tweaker::E_MAP_NORMAL);
    lrTweaker.AddJustPressedMapping("Look at right", &BehaviourDebugOrbitPlayer::LookAtRightSide, this, Utils::DebugController::E_CONTROL_RIGHT_DPAD, Utils::Tweaker::E_MAP_NORMAL);
}

} // namespace Camera
} // namespace BrnDirector
