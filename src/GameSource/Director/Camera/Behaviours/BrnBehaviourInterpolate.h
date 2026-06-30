#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_INTERPOLATE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_INTERPOLATE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the Setup asserts)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h
//
// BrnDirector::Camera::BehaviourInterpolate -- the transition behaviour: it blends the
// camera from one source ("from") camera reference to a "to" camera reference over a fixed
// duration (slerp or rotate-about-the-player-car, with a choice of time mappings). The ICE
// movie player drives it. HOME for the BehaviourInterpolate slice this TU bodies. The full
// behaviour (Construct/Prepare/Update/Release and the interpolation math) and its Behaviour
// base land with their own TUs; this header models -- BY NAME -- the members the bodied
// accessors/setters reach.
//
// ----------------------------------------------------------------------------
// LAYOUT (member NAMES from the DecFIGS DWARF outline for this class; OFFSETS are pinned
// from the bodied functions' member accesses in the X360 asm:
//   GetCameraAForSetup @0x821F3D18  -> &mFromCamera   (this + 0x2B0), asserts !mbSetup
//   GetCameraBForSetup @0x821F3D70  -> &mToCamera     (this + 0x420), asserts !mbSetup
//   GetCollisionPolicy @0x821F9EE8  -> &mCollisionPolicy (this + 0x20) when mbUseCollisionPolicy
//   GetParametricTime  @0x822065F8  -> clamp(mfRunningTime/mfDuration, 0, 1); reads 0x2A4/0x590
//   SetupDuration      @0x821F3CB0  -> mfDuration = lfDuration (0x2A4), asserts !mbSetup
//
// Members are accessed BY NAME; the struct-relative offsets in comments are provenance only,
// never used as casts. The embedded collision policy and interpolation controller are
// modelled by reserved byte spans (their rebuilt sizes differ from this build, so the spans
// pin the asm-attested offsets; offsetof pins in the .cpp verify the exact layout). The
// DWARF order is:
//   +0x0000  Behaviour                        (base)  vtable + shared flag/state block
//   +0x0020  VisibilityCollisionPolicy        mCollisionPolicy            (GetCollisionPolicy -> &this)
//   +....    CameraInterpolationController     mInterpolator
//   +....    const Parameters*                mpParameters
//   +0x02A4  float32_t                        mfDuration
//   +0x02B0  CameraReference                  mFromCamera
//   +0x0420  CameraReference                  mToCamera
//   +0x0590  float32_t                        mfRunningTime
//   +0x0594  bool                             mbUseCollisionPolicy
//   +0x0595  bool                             mbSetup
//   +0x0596  bool                             mbHasFinished
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera reference the behaviour interpolates between. The full
//   CameraReference (a resolvable handle to a behaviour-helper camera or a captured camera)
//   has a real home under GameSource/Director/Camera; the no-arg Setup only needs IsValid(),
//   which in the console body is a "the reference's source selector is in range (0, 4)"
//   check. The selector is the leading int of the reference. Replace with the real
//   CameraReference when the Camera-reference TU lands; the IsValid semantics are pinned from
//   the asm. (Modelled at the full reference size so the +0x2B0 / +0x420 offsets hold.)
class CameraReference
{
public:
    // True when this reference currently names a valid camera source (selector in (0, 4)).
    bool IsValid() const { return miSourceSelector > 0 && miSourceSelector < 4; }

    s32 miSourceSelector;         // +0x00  the camera-source selector the validity check reads
    u8  maReserved[0x170 - 0x4];  // +0x04  rest of the reference (mToCamera follows at +0x420 - 0x2B0 = 0x170)
};

// FLAG: minimal slice of the camera-behaviour base. The full Behaviour base (vtable + shared
//   flag/state block) lands with its own TU; the bodied accessors only need the derived
//   members, so the base is modelled as a sized opaque head ending at +0x20 (where
//   mCollisionPolicy begins).
class Behaviour
{
public:
    enum EInterpolationMethod
    {
        E_METHOD_SLERP,
        E_METHOD_ROTATE_ABOUT_PLAYER_CAR,
    };

    enum EInterpolationMapping
    {
        E_MAPPING_LINEAR,
        E_MAPPING_SINUSOIDAL,
        E_MAPPING_EXPONENTIAL_SYMMETRICAL,
        E_MAPPING_EXPONENTIAL_OUT_X_CUBED,
    };

protected:
    void* mpVTable;            // +0x00  behaviour vtable (opaque base head)
    u8    maBaseHead[0x20 - sizeof(void*)]; // +0x04..+0x1F  rest of the shared base (mCollisionPolicy @+0x20)
};

class BehaviourInterpolate : public Behaviour
{
public:

    // FLAG: the collision-policy sub-object GetCollisionPolicy exposes is mCollisionPolicy
    //   (+0x20, DWARF :166 VisibilityCollisionPolicy). Its concrete CollisionPolicy base type
    //   lands with the collision-policy TU; here it is forward-declared and the policy reached
    //   by name as an opaque sub-object span. Replace with the real type when it lands.
    class CollisionPolicy;

    // The source / destination camera references the blend runs between (populated before
    // the no-arg Setup via these accessors). The asm asserts the behaviour has NOT yet been
    // set up before handing the reference out. @0x821F3D18 / @0x821F3D70.
    CameraReference& GetCameraAForSetup();
    CameraReference& GetCameraBForSetup();

    // Return this behaviour's active collision policy: &mCollisionPolicy when
    // mbUseCollisionPolicy is set, otherwise NULL. @0x821F9EE8.
    CollisionPolicy* GetCollisionPolicy();

    // The clamped blend parameter t = clamp(mfRunningTime / mfDuration, 0, 1). Asserts the
    // duration is non-zero first. @0x822065F8.
    f32 GetParametricTime() const;

    // Set the blend duration, asserting the behaviour has not already been set up. @0x821F3CB0.
    void SetupDuration(f32 lfDuration);

    // Latch the behaviour as set up, asserting it was not already and that both camera
    // references are valid. @0x821F3DC8.
    void Setup();

    bool HasFinished() const { return mbHasFinished; }

    // Layout members are public-of-layout so the offsetof pins in the .cpp can reach them
    // (they verify the asm-attested offsets); they are not part of the behaviour's API.
    // Layout pinned by the bodied functions' asm-attested offsets (see header banner). The
    // collision policy / interpolation controller / parameter pointer occupy the span up to
    // mfDuration @+0x2A4; they are reached by name (mCollisionPolicy) or are not touched by
    // this TU's functions, so they are modelled as reserved spans / opaque slots.
    u8  maCollisionPolicy[0x2A4 - 0x20];  // +0x020  mCollisionPolicy + mInterpolator + mpParameters (&mCollisionPolicy = this+0x20)

    f32             mfDuration;     // +0x2A4  blend duration (SetupDuration writes; GetParametricTime reads)
    u8  maReserved2A8[0x2B0 - 0x2A8]; // +0x2A8  (gap to mFromCamera; interpolation state not touched here)
    CameraReference mFromCamera;    // +0x2B0  the "from" camera (GetCameraAForSetup)
    CameraReference mToCamera;      // +0x420  the "to" camera   (GetCameraBForSetup)
    f32             mfRunningTime;  // +0x590  elapsed blend time (GetParametricTime reads)

    bool            mbUseCollisionPolicy; // +0x594  selects whether GetCollisionPolicy returns the policy
    bool            mbSetup;        // +0x595  latched once Setup has run
    bool            mbHasFinished;  // +0x596  the blend has reached its end
};



// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetCameraAForSetup @0x821F3D18
//   lbz  r11, 0x595(r31)     ; mbSetup
//   ... assert !mbSetup ("Can't setup camera A after Setup() has been called") ...
//   addi r3, r31, 0x2B0      ; return &mFromCamera
// ----------------------------------------------------------------------------
inline CameraReference&
BehaviourInterpolate::GetCameraAForSetup()
{
    CGS_ASSERT(!mbSetup, "Can't setup camera A after Setup() has been called");
    return mFromCamera;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::GetCameraBForSetup @0x821F3D70
//   lbz  r11, 0x595(r31)     ; mbSetup
//   ... assert !mbSetup ("Can't setup camera B after Setup() has been called") ...
//   addi r3, r31, 0x420      ; return &mToCamera
// ----------------------------------------------------------------------------
inline CameraReference&
BehaviourInterpolate::GetCameraBForSetup()
{
    CGS_ASSERT(!mbSetup, "Can't setup camera B after Setup() has been called");
    return mToCamera;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::SetupDuration @0x821F3CB0
//   fmr  f31, f1             ; latch lfDuration
//   lbz  r11, 0x595(r31)     ; mbSetup
//   ... assert !mbSetup ("Can't setup duration after Setup() has been called") ...
//   stfs f31, 0x2A4(r31)     ; mfDuration = lfDuration
// ----------------------------------------------------------------------------
inline void
BehaviourInterpolate::SetupDuration(f32 lfDuration)
{
    CGS_ASSERT(!mbSetup, "Can't setup duration after Setup() has been called");
    mfDuration = lfDuration;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourInterpolate::Setup @0x821F3DC8
//   The no-arg overload: both camera references must already be valid. The console body
//   asserts !mbSetup, then mFromCamera valid, then mToCamera valid, then latches mbSetup.
//   (The asserts carry a copy-pasted "Can't setup twice" message in the shipped strings.)
// ----------------------------------------------------------------------------
inline void
BehaviourInterpolate::Setup()
{
    CGS_ASSERT(!mbSetup, "Can't setup twice");
    CGS_ASSERT(mFromCamera.IsValid(), "Can't setup twice");
    CGS_ASSERT(mToCamera.IsValid(), "Can't setup twice");
    mbSetup = true;
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_INTERPOLATE_H
