#pragma once

// Home for BrnDirector::BoostShakeController -- the camera boost-shake intensity driver.
//
// ⚠️⚠️ RE-TYPED 2026-08-02 (drive-handover wave). THE PREVIOUS SHAPE OF THIS HEADER WAS
// FABRICATED AND WRONG IN FOUR PLACES, AND IT IS WHY BehaviourGameplayExternal::Update
// deliberately omitted its `.cpp:512` call. It modelled three invented blocks --
// `BoostShakeOutput` (+0x114/+0x11C), `BoostShakeCameraState` (+0x3CC/+0x3D4) and
// `BoostShakeParamsRef` (+0x04) -- as opaque-padded views with offsetof static_asserts on
// CONSOLE offsets. All three are real, already-committed, already-NAMED types, and every one
// of those offsets moves on x64, so calling through the old declaration was an offset poke.
//
// ⭐ THE DECFIGS DWARF SETTLES IT OUTRIGHT. Its home is NOT this directory --
//   references/DecFIGS/dwarfdump/GameSource/Director/Shots/ShotControllers/BrnBoostShakeController.h
// (the file stays here; only the DWARF path is recorded, per the tree's TU-path-misattribution
// note). It declares exactly:
//       void Construct();
//       void Update(Camera *, const RaceCarState &, const cameradefaults &);
//
// ---- HOW EACH ARGUMENT WAS PINNED (asm, not inference) --------------------------------
// CALLEE  BURNOUT_X360_ARTIST.XEX @0x8220E548. r3 (`this`) is NEVER touched -- the controller
// is stateless. r4 is the WRITE target (`stfs 0x114(r4)`, `stb 0x11C(r4)`), r5 the READ source
// (`lfs 0x3D4(r5)`, `lfs 0x3CC(r5)`), r6 the params (`lwz r11, 4(r6)` then 0x18/0x1C/0x20/0x24).
// ⚠️ Hex-Rays renders this as `Update(result, a2, a3)` -- it drops `this` and renames r4
// "result", so the pseudocode argument NUMBERS are off by one. The asm is the authority.
//
// CALL SITE  @0x8224229C..0x822422B0, inside BehaviourGameplayExternal::Update:
//       lbz  r11, 0xB5F(r20)   ; mbEnableBoostEffects -- the gate
//       lwz  r11, 0x5A8(r19)   ; lrSharedInfo.mpDirectorResourceManager (console +1448)
//       addi r5,  r19, 0x60    ; &lrSharedInfo.mPlayerInfo             (console +96)
//       mr   r4,  r29          ; lCamera
//       addi r6,  r11, 0x5D8   ; DirectorResourceManager::mCameraDefaults (console +1496)
//       addi r3,  r1, var_370  ; `this` == a STACK TEMPORARY
//
//   r4  Camera*                     +0x114 == mEffects.mfShakeAmplitude, +0x11C ==
//                                   mEffects.mu8ShakeType (Camera::mEffects @+0x68 and
//                                   CameraEffects +0xAC/+0xB4 -- 0x68+0xAC == 0x114 and
//                                   0x68+0xB4 == 0x11C, both already NAMED in
//                                   BrnCameraEffects.h).
//   r5  const RaceCarState&         &mPlayerInfo == &mPlayerInfo.mRaceCarState, because
//                                   Camera::VehicleInfo's first member IS the RaceCarState
//                                   (BrnPlayerInfo.h, @0x000, sizeof 0x460). That is exactly
//                                   what the DWARF's `const RaceCarState&` means.
//   r6  const cameradefaults&       an Attrib::Gen::* instance; `*(r6+4)` is
//                                   Attrib::Instance::mpAttributeData -- ⚠️ +0x04 on console,
//                                   +0x08 on x64, which is why this MUST go through
//                                   GetLayoutPointer() and never through a modelled head.
//
// ---- ⚠️⚠️ AND THE TWO STATE FIELDS WERE NAMED BACKWARDS ---------------------------------
// The old header called +0x3CC/+0x3D4 `mfBoostElapsed` / `mfBoostDuration` and described the
// body as "an elapsed time over a duration". BrnVehicleEvents.h names both, from the
// physics-side publish it reconstructs:
//       +0x3CC (972) == RaceCarState::mfSpeedMPH
//       +0x3D4 (980) == RaceCarState::mfMaxBoostSpeedMPH
// So the first ratio is a SPEED RATIO against the car's boost top speed, not a ramp clock,
// and the divide-by-zero guard means "this car has no boost top speed", not "the boost window
// is zero-length". (+0x3D0 == mfMaxSpeedMPH is deliberately SKIPPED by the asm.) A plausible
// wrong name that nothing downstream could contradict -- this subsystem's signature failure.
//
// The four curve fields at ATTRIBUTE-DATA +0x18/+0x1C/+0x20/+0x24 keep their console offsets
// because they index SERIALIZED resource bytes, not a host struct -- the same rule (and the
// same idiom) as BrnMainDirector.cpp's KU_EXTERNAL_SOURCE_BOOST_FOV_OFFSET.
//
// `Construct()` (DWARF BrnBoostShakeController.h:46) has NO body in the X360 export set and
// nothing to construct on a stateless object. It is NOT declared here: an undeclared function
// cannot be called, whereas a declared-and-undefined one is an unresolved external waiting to
// happen, and an empty stub would be one more armed no-op.

#include "types.hpp"

namespace BrnDirector { namespace Camera { struct Camera; } }
namespace BrnPhysics { namespace Vehicle { struct RaceCarState; } }
namespace Attrib { namespace Gen { class cameradefaults; } }

namespace BrnDirector
{
    // BrnDirector::BoostShakeController -- a stateless driver. The console builds one on the
    // stack per call and never reads it; keep it empty so that stays true.
    struct BoostShakeController
    {
        // @0x8220E548. Map the car's speed against its boost top speed (clamped to [0,1])
        // through the cameradefaults curve (remap into [rampStart, rampEnd], clamp to [0,1],
        // scale by amplitude) into the camera's shake-amplitude request, and copy the shake
        // TYPE index across as a byte. A zero boost top speed forces the amplitude to zero.
        void Update(Camera::Camera* lpCamera,
                    const BrnPhysics::Vehicle::RaceCarState& lrCarState,
                    const Attrib::Gen::cameradefaults& lrCameraDefaults) const;
    };
}
