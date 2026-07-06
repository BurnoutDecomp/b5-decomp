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
// HOME for the batch bodied in the matching .cpp:
//   Construct @0x821FB610, GetName @0x821FB680, LookAtFront @0x821FB690,
//   LookAtBack @0x821FB6B8, LookAtLeftSide @0x821FB6D8, LookAtRightSide @0x821FB700,
//   Prepare @0x821FB638, SetupTweaker @0x8220FE00.
// Also declared (bodies land with their own ledger rows): Update, SetParameters.
//
// Member layout is DWARF-attested (BrnBehaviourDebugOrbitPlayer.h member NAMES +
// order) with byte offsets pinned by the X360 asm:
//   Construct zeroes base bytes +8..+0xC and words +4/+0x10/+0x30.
//   Prepare  writes mfFOV@+0x14, mfDistance@+0x18, mfYaw@+0x1C, mfPitch@+0x20,
//            mfSecondaryYaw@+0x24, mfSecondaryPitch@+0x28, mfSecondaryRoll@+0x2C
//            and the base "active" byte @+8.
//   SetupTweaker binds &mfFOV/&mfDistance/&mfYaw/&mfPitch (this+0x14..+0x20).
// The Behaviour base (vtable + shared flag block) has no committed home yet, so
// the +0x00..+0x13 head is modelled inline with reserved bytes to pin the member
// offsets the bodies actually store to.
// ============================================================================

#include "types.hpp"
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"   // Utils::Tweaker + Utils::DebugController::EControl

namespace BrnDirector
{
namespace Camera
{

// FLAG: forward slices the batch references by name. The full Behaviour base and
//   the shared-info types land with their own TUs.
struct BehaviourSharedPrepareReleaseInfo;   // Prepare parameter (opaque here)
struct BehaviourSharedInfo;                 // Update parameter (opaque here)
class  Camera;                              // Update target (opaque here)

class BehaviourDebugOrbitPlayer
{
public:
    // The orbit parameter block (Behaviour::Parameters derivative). DWARF:
    // struct Parameters : Behaviour::Parameters { void Construct(); }.
    class Parameters;

    // ------------------------------------------------------------------------
    // Virtual interface (DWARF vtable order). Bodies not in this batch are
    // declaration-only.
    // ------------------------------------------------------------------------
    virtual void        Construct();                                              // @0x821FB610
    virtual bool        Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo); // @0x821FB638
    virtual bool        Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo);
    virtual void        SetupTweaker(Utils::Tweaker& lrTweaker);                  // @0x8220FE00
    virtual const char* GetName() const;                                         // @0x821FB680

    void SetParameters(const Parameters* lpParameters);

private:
    // Tweaker just-pressed callbacks (D-pad snaps). The X360 stores a plain
    // function pointer with no this-adjust into a void(*)(void*) slot, so these
    // are static void(void*) callbacks; lpData is the BehaviourDebugOrbitPlayer*
    // userData bound in SetupTweaker. DWARF lists them without the static flag
    // (a known DWARF drop for static members); the asm proves the plain-address
    // form, so static is required to compile the faithful &Class::Fn binding.
    // @0x821FB690 / @0x821FB6B8 / @0x821FB6D8 / @0x821FB700.
    static void LookAtFront(void* lpData);
    static void LookAtBack(void* lpData);
    static void LookAtLeftSide(void* lpData);
    static void LookAtRightSide(void* lpData);

    // ------------------------------------------------------------------------
    // Members. Base Behaviour occupies +0x00..+0x13 (vtable + shared flag block;
    // Construct zeroes +4, bytes +8..+0xC, word +0x10). mbActive is the base
    // byte @+8 that Prepare sets to 1. Owned orbit fields (DWARF names + order)
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
    // Constants (X360 rodata).
    // ------------------------------------------------------------------------
    static const f32 KF_LOOK_AT_DISTANCE;   // 2.5f       (flt_82005548)
    static const f32 KF_HALF_PI;            // 1.5707964f (flt_82001754; left uses -flt_82005560)
    static const f32 KF_DEFAULT_FOV;        // 90.0f      (flt_82004F64)
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_DEBUG_ORBIT_PLAYER_H
