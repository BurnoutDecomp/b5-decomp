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
// movie player drives it. HOME for the BehaviourInterpolate slice this TU bodies (the no-arg
// Setup inline). The full behaviour (Construct/Prepare/Update/Release and the interpolation
// math) and its Behaviour base land with their own TUs; this header models only the slice
// the no-arg Setup needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is Setup() @0x821F3DC8 (the no-arg overload, used when the two
// camera references were already populated via GetCameraAForSetup()/GetCameraBForSetup()):
// it asserts it has not already been set up and that both camera references are valid, then
// latches mbSetup. The members below are the minimal named scaffolding that inline needs.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera reference the behaviour interpolates between. The full
//   CameraReference (a resolvable handle to a behaviour-helper camera or a captured camera)
//   has a real home under GameSource/Director/Camera; Setup() only needs IsValid(), which in
//   the console body is a "the reference's source selector is in range (0, 4)" check. The
//   selector is the leading int of the reference. Replace with the real CameraReference when
//   the Camera-reference TU lands; the IsValid semantics are pinned from the asm.
class CameraReference
{
public:
    // True when this reference currently names a valid camera source (selector in (0, 4)).
    bool IsValid() const { return miSourceSelector > 0 && miSourceSelector < 4; }

    s32 miSourceSelector;   // +0x00  the camera-source selector the validity check reads
};

// FLAG: minimal slice of the camera-behaviour base. The full Behaviour base (vtable + shared
//   flag/state block) lands with its own TU; the no-arg Setup only needs the derived
//   members, so the base is modelled as an opaque head here.
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
    void* mpVTable;   // +0x00  behaviour vtable (opaque base head)
};

class BehaviourInterpolate : public Behaviour
{
public:

    // The source / destination camera references the blend runs between (populated before
    // the no-arg Setup via the GetCameraAForSetup / GetCameraBForSetup accessors).
    CameraReference& GetCameraAForSetup() { return mFromCamera; }
    CameraReference& GetCameraBForSetup() { return mToCamera; }

    // Latch the behaviour as set up, asserting it was not already and that both camera
    // references are valid. @0x821F3DC8.
    void Setup();

    bool HasFinished() const { return mbHasFinished; }

private:

    // FLAG: only the members the no-arg Setup reads/writes are modelled (the two camera
    //   references and the setup latch). The rest of the interpolation state (duration,
    //   running time, the rotate-about-car params) lands with the full behaviour TU.
    CameraReference mFromCamera;   // the "from" camera
    CameraReference mToCamera;     // the "to" camera
    bool            mbSetup;       // latched once Setup has run
    bool            mbHasFinished; // the blend has reached its end
};



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
