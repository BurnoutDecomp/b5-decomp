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
// Every function declared here is bodied in the matching .cpp.
//
// Derives Camera::Behaviour (DWARF BrnBehaviourDebugFlyWorld.h:48); the base owns
// +0x00..+0x13 (Prepare sets mbIsPrepared, ARTIST 0x821FB76C `stb r9, 8(r11)`).
//
// Offsets in comments are original-build (4-byte-pointer) provenance; the reconstruction
// accesses every member BY NAME (semantic parity, not byte-matching).
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                       // Vector3
#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"   // Utils::Tweaker + Utils::DebugController::EControl
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"     // the Camera::Behaviour base

namespace BrnDirector
{
namespace Camera
{

// FLAG: forward slices the batch references by name. The full Behaviour base and the
//   shared-info types land with their own TUs.
struct BehaviourSharedPrepareReleaseInfo;   // Prepare parameter (opaque here)
struct BehaviourSharedInfo;                 // Update parameter (opaque here)
class  Camera;                              // Update target (opaque here)

class BehaviourDebugFlyWorld : public Behaviour
{
public:
    // Which of three speed presets the fly rig is on;
    // ChangeMovingSpeed cycles SLOW -> NORMAL -> FAST -> SLOW.
    enum EMoveSpeedType
    {
        E_MOVE_SPEED_TYPE_SLOW   = 0,
        E_MOVE_SPEED_TYPE_NORMAL = 1,
        E_MOVE_SPEED_TYPE_FAST   = 2,

        E_MOVE_SPEED_TYPE_MAX    = 3,
    };

    // The fly-rig parameter block (DWARF BrnBehaviourDebugFlyWorld.h:177).
    class Parameters : public Behaviour::Parameters
    {
    public:
        void Construct();                                                        // :181
    };

    // ------------------------------------------------------------------------
    // Virtual interface, in vtable order.
    // ------------------------------------------------------------------------
    virtual void        Construct();                                              //
    virtual bool        Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo); //
    virtual bool        Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo); //
    virtual void        SetupTweaker(Utils::Tweaker& lrTweaker);                  //
    virtual const char* GetName() const;                                         //

    void SetParameters(const Parameters* lpParameters);

    // Snap the fly camera to lEye looking at lLookAt (both world-space; the original build passes the
    // two Vector3s in VMX registers straight through the arbitrator wrapper).
    void WarpToLookAt(Vector3 lEye, Vector3 lLookAt);

private:
    // ------------------------------------------------------------------------
    // Tweaker just-pressed callbacks. The original-build stores a plain function pointer with no
    // this-adjust into a void(*)(void*) slot, so these are static void(void*) callbacks;
    // lpData is the BehaviourDebugFlyWorld* userData bound in SetupTweaker.
    // ------------------------------------------------------------------------
    static void ChangeMovingSpeed(void* lpData);    // (also called by Construct)
    static void WarpToCar(void* lpData);            //
    static void LookAtCar(void* lpData);            //
    static void LevelOut(void* lpData);             //
    static void ToggleCarAttachment(void* lpData);  //
    static void ToggleSloMo(void* lpData);          //
    // The "Take Screenshot" callback SetupTweaker binds. The shipped build folded it onto an
    // identical empty routine, so its recovered body is empty (see the .cpp).
    static void TakeScreenshot(void* lpData);

    // ------------------------------------------------------------------------
    // Members (DWARF BrnBehaviourDebugFlyWorld.h:116..:143), after the Behaviour base; the
    // 16-byte-aligned Vector3 rig positions start at +0x20.
    // ------------------------------------------------------------------------
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
