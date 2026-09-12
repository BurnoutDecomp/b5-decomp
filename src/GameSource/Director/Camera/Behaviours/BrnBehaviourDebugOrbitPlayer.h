#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_DEBUG_ORBIT_PLAYER_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_DEBUG_ORBIT_PLAYER_H

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h
//
// BrnDirector::Camera::BehaviourDebugOrbitPlayer -- the developer "orbit the
// player car" debug camera. Holds an orbit rig (FOV + distance + yaw/pitch and
// a secondary yaw/pitch/roll) and exposes a live camera Tweaker binding so a dev
// can nudge them on the pad and snap to the car's front/back/left/right.
//
// Bodied in the matching .cpp: Construct, Prepare, SetupTweaker, GetName and the four
// LookAt* snap callbacks. Still declaration-only: Update (its vtable slot resolves through
// DirectorLinkStubs.cpp until it is reconstructed) and SetParameters.
//
// Member layout is attested member names + order, with byte offsets pinned by the
// original build:
//   Construct zeroes base bytes +8..+0xC and words +4/+0x10/+0x30.
//   Prepare  writes mfFOV@+0x14, mfDistance@+0x18, mfYaw@+0x1C, mfPitch@+0x20,
//            mfSecondaryYaw@+0x24, mfSecondaryPitch@+0x28, mfSecondaryRoll@+0x2C
//            and the base "active" byte @+8.
//   SetupTweaker binds &mfFOV/&mfDistance/&mfYaw/&mfPitch (this+0x14..+0x20).
// FLAG: the class is still a PRE-BASE FORK -- it carries its own vtable pointer and the
// Behaviour base head as reserved bytes (+0x00..+0x13) instead of deriving from
// Camera::Behaviour. Nothing allocates it (the arbitrator's NewBehaviour<> calls are
// gated), so the fork is inert; retiring it onto the real base is its own job.
// ============================================================================

#include "types.hpp"
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"   // Utils::Tweaker + Utils::DebugController::EControl

namespace BrnDirector
{
namespace Camera
{

// Forward slices this header references by name; the .cpp pulls their real homes.
struct BehaviourSharedPrepareReleaseInfo;   // Prepare parameter (opaque here)
struct BehaviourSharedInfo;                 // Update parameter (opaque here)
class  Camera;                              // Update target (opaque here)

class BehaviourDebugOrbitPlayer
{
public:
    // The orbit parameter block (a Behaviour::Parameters derivative).
    class Parameters;

    // ------------------------------------------------------------------------
    // Virtual interface, in vtable order.
    // ------------------------------------------------------------------------
    virtual void        Construct();
    virtual bool        Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo);
    virtual bool        Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo);
    virtual void        SetupTweaker(Utils::Tweaker& lrTweaker);
    virtual const char* GetName() const;

    void SetParameters(const Parameters* lpParameters);

private:
    // Tweaker just-pressed callbacks (D-pad snaps). The original build stores a plain
    // function pointer with no this-adjust into a void(*)(void*) slot, so these are
    // static void(void*) callbacks; lpData is the BehaviourDebugOrbitPlayer* userData
    // bound in SetupTweaker.
    static void LookAtFront(void* lpData);
    static void LookAtBack(void* lpData);
    static void LookAtLeftSide(void* lpData);
    static void LookAtRightSide(void* lpData);

    // ------------------------------------------------------------------------
    // Members. Base Behaviour occupies +0x00..+0x13 (vtable + shared flag block;
    // Construct zeroes +4, bytes +8..+0xC, word +0x10). mbActive is the base
    // byte @+8 that Prepare sets to 1. Owned orbit fields
    // start at +0x14.
    // ------------------------------------------------------------------------
    void* mpVTable;                      // +0x00  Behaviour vtable (base head)
    u8    maReserved04[0x08 - 0x04];     // +0x04 .. +0x07 (base flag word, Construct-zeroed)
    bool  mbActive;                      // +0x08  base "active/prepared" flag (Prepare sets 1)
    u8    maReserved09[0x14 - 0x09];     // +0x09 .. +0x13 (base flags/word @+0x10, Construct-zeroed)

    f32   mfFOV;                         // +0x14  field of view (Prepare = 90.0)
    f32   mfDistance;                    // +0x18  orbit distance to car (Prepare/LookAt* = 2.5)
    f32   mfYaw;                         // +0x1C  orbit yaw   (LookAt* set +/-pi/2, pi, 0)
    f32   mfPitch;                       // +0x20  orbit pitch
    f32   mfSecondaryYaw;                // +0x24
    f32   mfSecondaryPitch;              // +0x28
    f32   mfSecondaryRoll;               // +0x2C

    const Parameters* mpParameters;      // +0x30  adopted parameter block (Construct-zeroed)

    // ------------------------------------------------------------------------
    // Constants (original-build rodata).
    // ------------------------------------------------------------------------
    static const f32 KF_LOOK_AT_DISTANCE;   // 2.5f
    static const f32 KF_HALF_PI;            // 1.5707964f (the left-side preset uses its negation)
    static const f32 KF_DEFAULT_FOV;        // 90.0f
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_DEBUG_ORBIT_PLAYER_H
