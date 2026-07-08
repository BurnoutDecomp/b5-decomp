#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_DEBUG_FLY_WORLD_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_DEBUG_FLY_WORLD_H

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourDebugFlyWorld.h
//
// BrnDirector::Camera::BehaviourDebugFlyWorld -- the developer "fly the world" debug
// camera. It holds a free-fly rig (position + yaw/pitch/roll + per-axis move/rotate
// speeds + FOV) plus four toggle flags (warp-to-car / look-at-car / attached-to-car /
// slo-mo), exposes a live camera Tweaker binding so a dev can nudge the rig on the pad,
// and each frame integrates the rig into the produced camera transform.
//
// HOME for the batch bodied in the matching .cpp:
//   Construct           @0x82210088 (virtual)
//   Prepare             @0x821FB738 (virtual)
//   SetupTweaker        @0x822100E0 (virtual)
//   GetName             @0x821FB728 (virtual)
//   WarpToLookAt        @0x8222CB10
//   ChangeMovingSpeed   @0x821FB790 (static tweaker callback)
//   WarpToCar           @0x821FB860 (static tweaker callback)
//   LookAtCar           @0x821FB870 (static tweaker callback)
//   LevelOut            @0x821FB888 (static tweaker callback)
//   ToggleCarAttachment @0x821FB8A0 (static tweaker callback)
//   ToggleSloMo         @0x821FB8B8 (static tweaker callback)
// Also declared (bodies land with their own ledger rows):
//   Update              @0x8222C618 (virtual) -- BLOCKED: reads un-homed Camera /
//       BehaviourSharedInfo interior + a GetImplicitVelocity(out,vehicle) overload
//       that contradicts the committed VehicleTracker home (see the .cpp).
//
// Member layout is DWARF-attested (BrnBehaviourDebugFlyWorld.h member NAMES + order) with
// byte offsets pinned by the X360 asm:
//   Construct zeroes base bytes +8..+0xC and words +4/+0x10, sets mbWarpToCar@+0x7C,
//     zeroes mbAttachedToCar@+0x7E / mbUseSloMo@+0x7F / meMoveSpeedState@+0x74 / mpParameters@+0x78.
//   Prepare  writes mfYaw@+0x40, mfPitch@+0x44, mfRoll@+0x48, mfX@+0x4C, mfY@+0x50, mfZ@+0x54,
//     mfFOV@+0x70, meMoveSpeedState@+0x74, the base "active" byte @+8, and zeroes
//     mPosition@+0x20 / mCurrentPosition@+0x30.
//   ChangeMovingSpeed writes the six speed floats mfMoveXSpeed@+0x58 .. mfRollSpeed@+0x6C.
//   SetupTweaker binds the rig floats (this+0x40..+0x6C) and &mfFOV (+0x70).
// The Behaviour base (vtable + shared flag block) has no committed home yet, so the
// +0x00..+0x1F head is modelled inline with reserved bytes to pin the member offsets the
// bodies store to; the two Vector3 rig positions start at the next 16-byte slot (+0x20).
// Offsets in comments are X360 (4-byte-pointer) provenance; the x64 recon accesses every
// member BY NAME (semantic parity, not byte-matching).
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                       // Vector3
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"   // Utils::Tweaker + Utils::DebugController::EControl

namespace BrnDirector
{
namespace Camera
{

// FLAG: forward slices the batch references by name. The full Behaviour base and the
//   shared-info types land with their own TUs.
struct BehaviourSharedPrepareReleaseInfo;   // Prepare parameter (opaque here)
struct BehaviourSharedInfo;                 // Update parameter (opaque here)
class  Camera;                              // Update target (opaque here)

class BehaviourDebugFlyWorld
{
public:
    // DWARF: BrnBehaviourDebugFlyWorld.h:108. Which of three speed presets the fly rig is on;
    // ChangeMovingSpeed cycles SLOW -> NORMAL -> FAST -> SLOW.
    enum EMoveSpeedType
    {
        E_MOVE_SPEED_TYPE_SLOW   = 0,
        E_MOVE_SPEED_TYPE_NORMAL = 1,
        E_MOVE_SPEED_TYPE_FAST   = 2,

        E_MOVE_SPEED_TYPE_MAX    = 3,
    };

    // The fly-rig parameter block (Behaviour::Parameters derivative). DWARF:
    // struct Parameters : Behaviour::Parameters { void Construct(); }. Declaration-only here.
    class Parameters;

    // ------------------------------------------------------------------------
    // Virtual interface (DWARF vtable order). Bodies not in this batch are declaration-only.
    // ------------------------------------------------------------------------
    virtual void        Construct();                                              // @0x82210088
    virtual bool        Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo); // @0x821FB738
    virtual bool        Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo); // @0x8222C618 (BLOCKED body)
    virtual void        SetupTweaker(Utils::Tweaker& lrTweaker);                  // @0x822100E0
    virtual const char* GetName() const;                                         // @0x821FB728

    void SetParameters(const Parameters* lpParameters);

    // Snap the fly camera to lEye looking at lLookAt (both world-space; the X360 passes the
    // two Vector3s in VMX registers straight through the arbitrator wrapper). @0x8222CB10.
    void WarpToLookAt(Vector3 lEye, Vector3 lLookAt);

private:
    // ------------------------------------------------------------------------
    // Tweaker just-pressed callbacks. The X360 stores a plain function pointer with no
    // this-adjust into a void(*)(void*) slot, so these are static void(void*) callbacks;
    // lpData is the BehaviourDebugFlyWorld* userData bound in SetupTweaker. DWARF lists them
    // without the static flag (a known DWARF drop for static members); the asm proves the
    // plain-address form, so static is required to compile the faithful &Class::Fn binding.
    // ------------------------------------------------------------------------
    static void ChangeMovingSpeed(void* lpData);    // @0x821FB790 (also called by Construct)
    static void WarpToCar(void* lpData);            // @0x821FB860
    static void LookAtCar(void* lpData);            // @0x821FB870
    static void LevelOut(void* lpData);             // @0x821FB888
    static void ToggleCarAttachment(void* lpData);  // @0x821FB8A0
    static void ToggleSloMo(void* lpData);          // @0x821FB8B8
    // The "Take Screenshot" callback SetupTweaker binds. Its X360 body was ICF-folded onto an
    // identical empty routine (the export names it BaseCollisionGenerator::Destruct); DWARF
    // attests it here as BrnBehaviourDebugFlyWorld.cpp:345. Declaration-only (body not in this
    // TU's function set); required so SetupTweaker can take its address faithfully.
    static void TakeScreenshot(void* lpData);

    // ------------------------------------------------------------------------
    // Members. Base Behaviour occupies +0x00..+0x1F (vtable + shared flag block; Construct
    // zeroes +4, bytes +8..+0xC, word +0x10). mbActive is the base byte @+8 that Prepare sets
    // to 1. The two 16-byte-aligned Vector3 rig positions start at the next 16-byte slot
    // (+0x20); the owned scalar rig fields (DWARF names + order) follow at +0x40.
    // ------------------------------------------------------------------------
    void* mpVTable;                      // +0x00  Behaviour vtable (base head)
    u32   mBaseResetWord;                // +0x04  base reset word (Construct-zeroed)
    bool  mbActive;                      // +0x08  base "active/prepared" flag (Prepare sets 1)
    u8    maBaseFlags[4];                // +0x09..+0x0C base flag bytes (Construct-zeroed)
    u8    maBasePad0D[3];                // +0x0D..+0x0F (untouched by the bodies)
    u32   mBaseWord10;                   // +0x10  base word (Construct-zeroed)
    u8    maBasePad14[0x20 - 0x14];      // +0x14..+0x1F alignment pad to the Vector3 slot

    Vector3 mPosition;                   // +0x20  fly-rig eye position (Prepare/WarpToLookAt)
    Vector3 mCurrentPosition;            // +0x30  the smoothed/current eye position (Prepare zeroes)

    f32   mfYaw;                         // +0x40  rig yaw   (about Y)
    f32   mfPitch;                       // +0x44  rig pitch (about X; LevelOut zeroes)
    f32   mfRoll;                        // +0x48  rig roll  (about Z; LevelOut zeroes)

    f32   mfX;                           // +0x4C  translate left/right
    f32   mfY;                           // +0x50  translate up/down
    f32   mfZ;                           // +0x54  translate forwards/backwards

    f32   mfMoveXSpeed;                  // +0x58  left/right speed  (ChangeMovingSpeed)
    f32   mfMoveYSpeed;                  // +0x5C  up/down speed
    f32   mfMoveZSpeed;                  // +0x60  fwd/back speed
    f32   mfYawSpeed;                    // +0x64  yaw speed
    f32   mfPitchSpeed;                  // +0x68  pitch speed
    f32   mfRollSpeed;                   // +0x6C  roll speed

    f32   mfFOV;                         // +0x70  field of view (Prepare = 90.0)

    EMoveSpeedType meMoveSpeedState;     // +0x74  current speed preset

    const Parameters* mpParameters;      // +0x78  adopted parameter block (Construct-zeroed)

    bool  mbWarpToCar;                   // +0x7C  warp-to-car request (Construct sets true)
    bool  mbLookAtCar;                   // +0x7D  look-at-car toggle
    bool  mbAttachedToCar;               // +0x7E  attached-to-car toggle
    bool  mbUseSloMo;                    // +0x7F  slo-mo toggle
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_DEBUG_FLY_WORLD_H
