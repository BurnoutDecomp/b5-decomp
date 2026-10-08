#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"        // Utils::VersionNumber (the +0x7C tag)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"  // Utils::CameraShake::Parameters (mShakeParams @+0x08)
#include "GameSource/Director/Camera/Utils/BrnLooker.h"          // Utils::Looker::Parameters (mLookerParams @+0x18)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"     // the Camera::Behaviour base
#include "GameShared/GameClasses/Numeric/CgsRandom.h"           // CgsNumeric::Random (mRandom)

#include <cstddef>   // offsetof (the layout pins)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h
//
// BrnDirector::Camera::BehaviourHeliCam -- the "helicopter cam" camera behaviour: a high,
// distant tracking camera that orbits/follows the tracked car as if shot from a circling
// helicopter (used by scripted moments and the arbitrator testbed). It derives the canonical
// Camera::Behaviour: the behaviour manager pools it and drives it through the base's eight-slot
// vtable (Construct, Prepare, Update, SetupTweaker and GetName are this class's; the other three
// slots keep the base defaults). The bodies are in BrnBehaviourHeliCam.cpp.
//
// Layout (declaration order; console offsets, the host widens the base head and the pointer):
//   +0x020 mTransform   +0x060 mLooker   +0x080 mPosition   +0x090 mTarget   +0x0A0 mVelocity
//   +0x0B0 mRandom      +0x0E0 mpParameters                 +0x0E4 mCameraShake
//   +0x0F4 mbCalculatePosition                              (console size 0x100)
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

class BehaviourHeliCam : public Behaviour
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
    //   +0x80 mFOV           (Utils::FOV; Process<float>; SetRange 1..150) "FOV"
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

        void Construct();   // the parameter bank's seed; body: BrnBehaviourHeliCam_wS34_05.cpp

        EBehaviourTypeHeliCam GetType() const
        {
            return static_cast<EBehaviourTypeHeliCam>(meType);
        }

        s32                             meType;             // +0x00  the behaviour type tag (eBehaviour*)
        s32                             miParamWord1;       // +0x04  first behaviour-specific word
        Utils::CameraShake::Parameters  mShakeParams;       // +0x08  "Shake Parameters"  (v1 + v2)
        Utils::Looker::Parameters       mLookerParams;      // +0x18  "Looker Parameters" (v2 only)
        Utils::VersionNumber            muVersion;          // +0x7C  version tag (code version = 2)
        Utils::FOV                      mFOV;               // +0x80  "FOV" (debug-menu range 1..150)
        f32                             mfHeight;           // +0x84  "Height (KM)"
        f32                             mfInitialDistanceX; // +0x88  "Initial Distance X (KM)"
        f32                             mfInitialDistanceZ; // +0x8C  "Initial Distance Z (KM)"
        f32                             mfVelocityMPS;      // +0x90  "Velocity MPS"
    };

    // ---- the Behaviour virtual interface ---------------------------------------------------
    // Seed the base head, the shake, the looker and the random ring.                   (slot 0)
    void Construct() override;

    // Reset the rig to the identity and arm the position seed for the next Update.    (slot 1)
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;

    // Seed the position off the player car once, fly it along the velocity, look at the car and
    // shake. Always returns true.                                                      (slot 2)
    bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;

    // Reset the tweaker it is handed; the heli cam maps nothing onto it.              (slot 6)
    void SetupTweaker(Utils::Tweaker& lrTweaker) override;

    //                                                                                  (slot 7)
    const char* GetName() const override;

    // Adopt a heli-cam parameter block: assert it carries the heli-cam type tag, then store the
    // pointer. Declared over the derived Parameters, so it hides the base's pair.
    void SetParameters(const Parameters* lpParameters);

private:
    Matrix44Affine           mTransform;           // the rig's own look transform
    Utils::Looker            mLooker;
    Vector3                  mPosition;            // the flying camera position
    Vector3                  mTarget;              // the player car position the flight started from
    Vector3                  mVelocity;            // the flight velocity (world units per second)
    CgsNumeric::Random       mRandom;
    const Parameters*        mpParameters;
    Utils::CameraShake       mCameraShake;
    bool                     mbCalculatePosition;  // seed the position on the next Update

public:
    // NEVER CALLED. Pins the member order the console layout fixes.
    static void _AssertLayout()
    {
        typedef BehaviourHeliCam T;
        static_assert(offsetof(T, mTransform) < offsetof(T, mLooker) &&
                      offsetof(T, mLooker) < offsetof(T, mPosition) &&
                      offsetof(T, mPosition) < offsetof(T, mTarget) &&
                      offsetof(T, mTarget) < offsetof(T, mVelocity) &&
                      offsetof(T, mVelocity) < offsetof(T, mRandom) &&
                      offsetof(T, mRandom) < offsetof(T, mpParameters) &&
                      offsetof(T, mpParameters) < offsetof(T, mCameraShake) &&
                      offsetof(T, mCameraShake) < offsetof(T, mbCalculatePosition),
                      "BehaviourHeliCam: members in console order");
        static_assert(offsetof(T, mLooker) - offsetof(T, mTransform) == 0x40 &&
                      offsetof(T, mPosition) - offsetof(T, mLooker) == 0x20 &&
                      offsetof(T, mRandom) - offsetof(T, mPosition) == 0x30 &&
                      offsetof(T, mpParameters) - offsetof(T, mRandom) == 0x30,
                      "BehaviourHeliCam: the pointer-free run +0x20..+0xE0 keeps the console spacing");
    }
};

// ----------------------------------------------------------------------------
// SetParameters. The console also copies the block's +0x04 word into the base's debug-name slot;
// that store is omitted here: the heli-cam block is one of the host head forks (it does not
// derive Behaviour::Parameters, so its +0x04 word is not a host name pointer). It feeds only the
// tweaker and the debug printers.
// ----------------------------------------------------------------------------
inline void
BehaviourHeliCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourHeliCam,
               "lpParameters->GetType() == eBehaviourHeliCam");
    mpParameters = lpParameters;
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_HELI_CAM_H
