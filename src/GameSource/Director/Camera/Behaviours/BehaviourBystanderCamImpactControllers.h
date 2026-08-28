#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_IMPACT_CONTROLLERS_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_IMPACT_CONTROLLERS_H

#include "types.hpp"
#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"  // Utils::CameraImpactEffect (by value)
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"         // Utils::Random typedef (CgsNumeric::Random)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourBystanderCamImpactControllers.h
//
// ⭐ A HEADER PARTFILE OF BehaviourBystanderCam.h, NOT A NEW FILE IN THE SOURCE BUILD.
// The DecFIGS DWARF homes BOTH impact controllers in BehaviourBystanderCam.h (:44 and :75),
// alongside the bystander-cam behaviour itself, and that is still where they belong. They are
// carved out here for ONE reason, recorded so nobody re-merges them by accident:
//
//   THIS TREE CARRIES TWO RECONSTRUCTIONS OF BehaviourBystanderCam.h.
//   `Behaviours/BehaviourBystanderCam.h`     -- the full-behaviour slice (self-contained,
//                                               opaque reserved spans, the Construct/Update rig)
//   `Behaviours/BrnBehaviourBystanderCam.h`  -- the GetCol/SetParameters/SetTarget slice
//   Both define `BrnDirector::Camera::BehaviourBystanderCam`, so any TU that reaches both is a
//   hard C2011. They coexist today only because no TU does. ArbStateCrashing embeds BOTH
//   controllers BY VALUE, and its header is #included by the arbitrator STATE CONTAINER, which
//   is in turn reached by a large part of the director -- including TUs that already reach
//   BrnBehaviourParameterBank.h -> BrnBehaviourBystanderCam.h. Pulling the whole
//   BehaviourBystanderCam.h in behind the container would therefore have detonated that fork
//   across the build. Splitting the two controllers into their own header costs nothing (they
//   share no member with the behaviour) and keeps both reconstructions untouched.
//
// DELETE-WHEN: the two BehaviourBystanderCam reconstructions are merged into one home. Then
// fold these two classes back into it and delete this file.
//
// The bodies live in the matching .cpp partfile, BehaviourBystanderCamImpactControllers.cpp.
//
// WHAT THESE TWO ARE. They are the crash camera's *feel*: the pair of per-frame controllers
// that ArbStateCrashing::ApplySlomoAndShake @0x8224F8D8 drives every frame of a crash.
//   ImpactSlomoController -- THE CRASH SLOW MOTION. On a vertical flip at speed it writes
//     Camera::mEffects.mfSimTimeScale = 0.2857143 for a 2.0 s burst, which MainDirector's
//     slow-motion channel turns into a 0.016667 -> 0.004762 sim timestep.
//   ImpactShakeController -- the impact camera shake, scaled by collision force, speed and
//     distance to the crashing car.
// ============================================================================

namespace BrnDirector
{
// Foreign types both Updates thread through by reference. Their real homes land with their own
// TUs and the .cpp partfile #includes them; pointer/reference-only use here, so the
// forward declarations suffice and nothing is forked.
// ⚠️ CLASS-KEY: `struct` for DebugPrinter, matching its real home
// (BrnDirectorModuleDebugPrinter.h:40) and every other forward declaration in the director.
// MSVC mangles the class-key into the symbol, so a `class` here would emit
// `AEAUDebugPrinter`-vs-`AEAVDebugPrinter` and open an unresolved external at link.
struct DebugPrinter;
class  AllVehicleData;
class  VehicleTracker;
struct VehicleRef;

namespace Camera
{

class Camera;   // the camera being driven (its mEffects.mfSimTimeScale is the slow-motion slot)

// ----------------------------------------------------------------------------
// BrnDirector::Camera::ImpactSlomoController (DWARF BehaviourBystanderCam.h:44)
//
// ⭐⭐ THIS CLASS IS THE CRASH SLOW MOTION. Update decides, from the tracked car's
// linear-velocity journal and its above-ground test, whether to enter / sustain / leave a short
// slow-motion burst, and drives it through the camera's requested sim-time scale.
//
// Only Update has an X360 symbol (@0x82227230). Construct and IsFirstFrameOfSlomo are inlined
// at every site; Construct's body is recovered from two independent inlined copies -- the
// bystander cam's own Construct and ArbStateCrashing's ApplySlomoAndShake / Update, which both
// emit the identical `mfTimeSinceLastSlomo = FLT_MAX; mfTimeInSlomo = 0; mbFirstFrameOfSlomo =
// false` triple (asm @0x8224FA10..0x8224FA20).
// ----------------------------------------------------------------------------
class ImpactSlomoController
{
public:
    // Seed the controller to "no burst, cool-down already expired". The FLT_MAX seed is what
    // lets the very first qualifying impact of a session start a burst immediately (the entry
    // gate is `mfTimeSinceLastSlomo >= 2.0f`).
    void Construct()
    {
        mfTimeSinceLastSlomo = 3.4028235e38f;   // stfs flt_8200173C (FLT_MAX), 0x1A8(state)
        mfTimeInSlomo        = 0.0f;            // stfs flt_82001CC0 (0.0),    0x1AC(state)
        mbFirstFrameOfSlomo  = false;           // stb  0,                     0x1B0(state)
    }

    // Advance the slow-motion state machine for one frame. @0x82227230.
    //   lrCamera            the camera whose requested sim-time scale is written (+0x104 ==
    //                       Camera::mEffects.mfSimTimeScale -- mEffects is at camera +0x68 and
    //                       mfSimTimeScale at effects +0x9C)
    //   lfTimestep          this frame's delta (the f1 argument)
    //   lrVehicles          all-vehicle data; the tracked car's record comes from GetPlayer()
    //   lrPlayerTracker     the tracked car's per-frame tracker (its linear-velocity journal)
    //   lrDebugPrinter      dev print sink (threaded only; the console body never prints)
    //   lbDontSetRealTime   when set, suppress the "return to real time" write
    void Update(Camera& lrCamera, f32 lfTimestep, const AllVehicleData& lrVehicles,
                const VehicleTracker& lrPlayerTracker, DebugPrinter& lrDebugPrinter,
                bool lbDontSetRealTime);

    // True only on the first frame of an active burst. ArbStateCrashing::Update reads it to
    // decide whether a tumbling moment should be told this is a good time to plant a camera.
    bool IsFirstFrameOfSlomo() const { return mbFirstFrameOfSlomo; }

private:
    f32  mfTimeSinceLastSlomo;   // :67  +0x00  cool-down accumulator since the last burst
    f32  mfTimeInSlomo;          // :68  +0x04  elapsed time inside the current burst
    bool mbFirstFrameOfSlomo;    // :69  +0x08  first-frame latch
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::ImpactShakeController (DWARF BehaviourBystanderCam.h:75)
//
// Computes an impact strength from the tracked car's hardest impact this frame, attenuates it
// by speed and by camera-to-car distance, registers it on the embedded CameraImpactEffect and
// advances that effect's shake.
//
// ⚠️ IT IS 20 BYTES, NOT 68. The reconstruction this replaced modelled the member as
// `f32 mfImpactScalar + u8 maImpactEffect[0x40]` and FLAGged that as a guess. Two independent
// witnesses say 20: the DWARF gives the class exactly one member, a CameraImpactEffect; and
// ArbStateCrashing::ApplySlomoAndShake's suppression arm clears exactly five floats at
// +0x1B4..+0x1C4 with mMomentSelector starting at +0x1C8.
// ----------------------------------------------------------------------------
class ImpactShakeController
{
public:
    // Clear the impact accumulator and the shake's wobble state. Inlined at every site; the
    // body is the five-float clear ApplySlomoAndShake emits at 0x8224FA30..0x8224FA44.
    void Construct()
    {
        mImpactEffect.SetImpactFactor(0.0f);
        mImpactEffect.GetCameraShake().Construct();
    }

    // Register and advance the impact shake for one frame. @0x82243720.
    //   lrVehicleRef  the reference naming the impacting car. ArbStateCrashing builds it on the
    //                 stack as a default player-car reference (asm 0x8224FA78..0x8224FA88:
    //                 { E_PLAYER_CAR, -1, 0, mbSet = true }).
    void Update(Camera& lrCamera, f32 lfTimestep, const AllVehicleData& lrVehicles,
                const VehicleTracker& lrPlayerTracker, Utils::Random& lrRandom,
                DebugPrinter& lrDebugPrinter, const VehicleRef& lrVehicleRef);

private:
    Utils::CameraImpactEffect mImpactEffect;   // DWARF: the class's only member
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BEHAVIOUR_BYSTANDER_CAM_IMPACT_CONTROLLERS_H
