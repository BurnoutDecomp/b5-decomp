// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourInterpolate slice this TU owns.
// The trivial accessors/setters (GetCameraAForSetup/GetCameraBForSetup/SetupDuration/Setup/
// HasFinished) are inline in the header; the two non-trivial members -- GetCollisionPolicy
// (returns the forward-declared collision-policy sub-object) and GetParametricTime (the
// fsel-clamped blend parameter) -- are bodied out of line here. The rest of the behaviour
// (Construct/Prepare/Update/Release and the interpolation math) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h"

#include <cstddef>   // offsetof (layout pins)

namespace BrnDirector
{
namespace Camera
{

// Pin the asm-attested member offsets so the reserved-span layout above stays honest.
static_assert(offsetof(BehaviourInterpolate, mfDuration)    == 0x2A4, "mfDuration @0x2A4");
static_assert(offsetof(BehaviourInterpolate, mFromCamera)   == 0x2B0, "mFromCamera @0x2B0");
static_assert(offsetof(BehaviourInterpolate, mToCamera)     == 0x420, "mToCamera @0x420");
static_assert(offsetof(BehaviourInterpolate, mfRunningTime) == 0x590, "mfRunningTime @0x590");
static_assert(offsetof(BehaviourInterpolate, mbUseCollisionPolicy) == 0x594, "mbUseCollisionPolicy @0x594");
static_assert(offsetof(BehaviourInterpolate, mbSetup)       == 0x595, "mbSetup @0x595");
static_assert(offsetof(BehaviourInterpolate, mbHasFinished) == 0x596, "mbHasFinished @0x596");

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetCollisionPolicy @0x821F9EE8
//   lbz   r11, 0x594(r3)     ; mbUseCollisionPolicy
//   addi  r3, r3, 0x20       ; r3 = &mCollisionPolicy (computed unconditionally)
//   cmplwi r11, 0
//   bnelr cr6                ; if (mbUseCollisionPolicy) return &mCollisionPolicy
//   li    r3, 0              ; else return NULL
// ----------------------------------------------------------------------------
BehaviourInterpolate::CollisionPolicy*
BehaviourInterpolate::GetCollisionPolicy()
{
    if (mbUseCollisionPolicy)
    {
        return reinterpret_cast<CollisionPolicy*>(maCollisionPolicy); // this + 0x20
    }
    return 0;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetParametricTime @0x822065F8
//   lfs   f0,  0x2A4(r31)    ; mfDuration
//   ... assert mfDuration != 0.0f ...
//   lfs   f13, 0x2A4(r31)    ; mfDuration
//   lfs   f0,  0x590(r31)    ; mfRunningTime
//   fdivs f0, f0, f13        ; ratio = mfRunningTime / mfDuration
//   fneg  f13, f0            ; -ratio
//   fsel  f0, f13, f31, f0   ; f0 = (-ratio >= 0) ? 0.0 : ratio   == max(ratio, 0)
//   fsubs f12, f13(=1.0), f0 ; 1.0 - f0
//   fsel  f1, f12, f0, f13   ; result = (1.0 - f0 >= 0) ? f0 : 1.0 == min(f0, 1.0)
//   -> result = clamp(mfRunningTime / mfDuration, 0.0f, 1.0f)
// (fsel a,b,c selects b when a >= 0, else c. f31 = 0.0f, the value the duration assert
//  compares against; the 1.0f comes from rodata flt_82001C98.)
// ----------------------------------------------------------------------------
f32
BehaviourInterpolate::GetParametricTime() const
{
    CGS_ASSERT(mfDuration != 0.0f, "mfDuration != 0.0f");

    f32 lfRatio = mfRunningTime / mfDuration;

    // fsel f0, -ratio, 0.0, ratio  -> clamp the low end to 0.0
    f32 lfClampedLow = (-lfRatio >= 0.0f) ? 0.0f : lfRatio;

    // fsel f1, 1.0 - f0, f0, 1.0   -> clamp the high end to 1.0
    f32 lfResult = (1.0f - lfClampedLow >= 0.0f) ? lfClampedLow : 1.0f;

    return lfResult;
}

} // namespace Camera
} // namespace BrnDirector
