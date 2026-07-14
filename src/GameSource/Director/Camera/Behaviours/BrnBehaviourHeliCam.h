#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"        // Utils::VersionNumber (the +0x7C tag)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"  // Utils::CameraShake::Parameters (mShakeParams @+0x08)
#include "GameSource/Director/Camera/Utils/BrnLooker.h"          // Utils::Looker::Parameters (mLookerParams @+0x18)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h
//
// BrnDirector::Camera::BehaviourHeliCam -- the "helicopter cam" camera behaviour: a high,
// distant tracking camera that orbits/follows the tracked car as if shot from a circling
// helicopter (used by scripted moments and the arbitrator testbed). HOME for the
// BehaviourHeliCam class slice this TU bodies (the SetParameters inline). The full behaviour
// (Construct/Prepare/Update and the rest of the rig) and its Behaviour base land with their
// own TUs; this header models only the slice SetParameters needs, BY NAME.
//
// ----------------------------------------------------------------------------
// The ONLY function homed here is SetParameters @0x821F3AA0: it asserts the supplied parameter
// block is a heli-cam block (its type tag == eBehaviourHeliCam == 6) then caches the block's
// +0x04 word at +0x10 and stores the block pointer at +0xE0. The members below are the minimal
// named scaffolding that inline needs to compile.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block; SetParameters asserts the block's id is the
//   heli-cam one. The console value for eBehaviourHeliCam is 6 (the asm at 0x821F3AC0 compares
//   the block's first word against 6). Replace with the real EBehaviourType enum when the
//   Behaviour base TU lands; the heli-cam enumerator's VALUE (6) is pinned from the asm.
enum EBehaviourTypeHeliCam
{
    eBehaviourHeliCam = 6
};

class BehaviourHeliCam
{
public:

    // The heli-cam parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    //
    // ------------------------------------------------------------------------
    // Full field-walk layout, pinned from the three Parameters::Serialise<S> instances
    // (Serialise<DebugMenuSerialiser> @0x8224C110, Serialise<TextFileReadSerialiser> @0x82231D30,
    // Serialise<TextFileWriteSerialiser> @0x8224DE90). The serialisers reference the members at
    // these X360 store/load displacements:
    //   +0x08 mShakeParams   (CameraShake::Parameters, 0x10 bytes -> ends +0x18)  "Shake Parameters"
    //   +0x18 mLookerParams  (Looker::Parameters, 0x64 bytes -> ends +0x7C)       "Looker Parameters"
    //   +0x7C muVersion      (VersionNumber; stamped = 2)         "Version Number (dont change)"
    //   +0x80 mfFOV          (Process<float>(a1+128); SetRange 1..150; label @0x820051C0 unrecovered)
    //   +0x84 mfHeight              "Height (KM)"              (SetStep 0.01)
    //   +0x88 mfInitialDistanceX    "Initial Distance X (KM)"  (SetStep 0.01)
    //   +0x8C mfInitialDistanceZ    "Initial Distance Z (KM)"  (SetStep 0.01)
    //   +0x90 mfVelocityMPS         "Velocity MPS"             (SetStep 0.01)
    // The mShakeParams/mLookerParams/muVersion nested types are reused BY NAME from their homes
    // (CameraShake in BehaviourRig.h, Looker in BrnLooker.h, VersionNumber in CameraUtils.h) so the
    // 0x08/0x18/0x7C offsets follow from their real sizes, not hand-inserted padding.
    // ------------------------------------------------------------------------
    class Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- walks this block's fields into the camera-tunings
        // serialiser S (DebugMenuSerialiser / TextFile{Read,Write}Serialiser); the body + its three
        // explicit instantiations live in BrnBehaviourHeliCamSerialise.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeHeliCam GetType() const
        {
            return static_cast<EBehaviourTypeHeliCam>(meType);
        }

        s32                             meType;             // +0x00  the behaviour type tag (eBehaviour*)
        s32                             miParamWord1;       // +0x04  first behaviour-specific word
        Utils::CameraShake::Parameters  mShakeParams;       // +0x08  "Shake Parameters"  (v1 + v2)
        Utils::Looker::Parameters       mLookerParams;      // +0x18  "Looker Parameters" (v2 only)
        Utils::VersionNumber            muVersion;          // +0x7C  version tag (code version = 2)
        f32                             mfFOV;              // +0x80  <unk_820051C0> field (range 1..150)
        f32                             mfHeight;           // +0x84  "Height (KM)"
        f32                             mfInitialDistanceX; // +0x88  "Initial Distance X (KM)"
        f32                             mfInitialDistanceZ; // +0x8C  "Initial Distance Z (KM)"
        f32                             mfVelocityMPS;      // +0x90  "Velocity MPS"
    };

    // Adopt a heli-cam parameter block: assert it carries the heli-cam type tag, then cache its
    // first word and store the pointer. @0x821F3AA0.
    void SetParameters(const Parameters* lpParameters);

private:

    // FLAG: only the members SetParameters writes are modelled at their asm-attested offsets;
    //   the rest of the heli-cam rig lands with the full behaviour TU. The vtable/base head
    //   occupies +0x00; the cached param word is at +0x10 (stw r11, 0x10(this)) and the param
    //   pointer is at +0xE0 (stw r31, 0xE0(this)). Reserved byte spans place them exactly.
    void*             mpVTable;                       // +0x00  behaviour vtable (opaque base head)
    u8                maReserved04[0x10 - 0x04];      // +0x04 .. +0x0F (rig members not modelled here)
    s32               mParamWord1;                    // +0x10  cached lpParameters->miParamWord1
    u8                maReserved14[0xE0 - 0x14];      // +0x14 .. +0xDF (rig members not modelled here)
    const Parameters* mpParameters;                   // +0xE0  the adopted parameter block
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourHeliCam::SetParameters @0x821F3AA0
//   lwz  r11, 0(r4)         ; lpParameters->meType
//   cmplwi r11, 6           ; == eBehaviourHeliCam
//   ... assert on mismatch ...
//   lwz  r11, 4(r4)         ; lpParameters->miParamWord1
//   stw  r4,  0xE0(r3)      ; mpParameters = lpParameters
//   stw  r11, 0x10(r3)      ; mParamWord1  = lpParameters->miParamWord1
// ----------------------------------------------------------------------------
inline void
BehaviourHeliCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourHeliCam,
               "lpParameters->GetType() == eBehaviourHeliCam");
    mpParameters = lpParameters;                   // stw r4,  0xE0(this)
    mParamWord1  = lpParameters->miParamWord1;      // lwz r11,4(lp); stw r11, 0x10(this)
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H
