#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"  // Utils::CameraShake::Parameters (embedded "Shake Params" sub-block)
#include "GameSource/AttribSys/Generated/classes/aftertouchcam.h" // Attrib::Gen::aftertouchcam (the adopted source shot)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"         // the Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"           // CollisionPolicyAttachedToVehicle
#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"         // Utils::PositionLag
#include "GameShared/GameClasses/Numeric/CgsRandom.h"               // CgsNumeric::Random

#include <cstddef>   // offsetof (the layout pins)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h
//
// BrnDirector::Camera::BehaviourAftertouchCam -- the "aftertouch cam" camera behaviour (the
// slow-motion crash-aftertouch follow camera the testbed / behaviour-manager installs). It
// derives the canonical Camera::Behaviour, so the behaviour manager can pool it and dispatch the
// base's eight-slot vtable.
//
// Layout (declaration order; console offsets, the host widens the base head and the pointer):
//   +0x020 mCollisionPolicy   +0x270 mCurrentTargetPos   +0x280 mWorldSpaceNormalizedVectorFromCar
//   +0x290 mDesiredWorldSpaceNormalizedVectorFromCar      +0x2A0 mCrashPoint
//   +0x2B0 mPositionLag       +0x2E0 mRandom             +0x310 mShake
//   +0x320 mfHeight  +0x324 mfDistance  +0x328 mfBlendFactor  +0x32C mfTimeSinceLastDecision
//   +0x330 mpParameters       +0x334 mSourceShot         (console size 0x350)
//
// FLAG partial: SetupTweaker and GetName are this class's; Construct, Prepare, Update and
//   GetCollisionPolicy (with the private AssignIfBetterRival / CalculateDesiredTargetPos) are not
//   reconstructed yet and keep the base defaults (no collision policy, the camera is not driven).
//   The collision-policy override returns &mCollisionPolicy, which only Construct seeds, so the
//   two land together.
// Parameters::Construct: the block's authored defaults, transcribed in full -- see it below.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. Each behaviour carries a type id in the
//   leading word of its Parameters block; SetParameters asserts the block's id is the
//   aftertouch-cam one. The console value for eBehaviourAftertouchCam is 10 (the asm at
//   0x821F3EC0 compares the block's first word against 0xA). Replace with the real
//   EBehaviourType enum when the Behaviour base TU lands; the enumerator's VALUE (10) is asm.
enum EBehaviourTypeAftertouchCam
{
    eBehaviourAftertouchCam = 10
};

class BehaviourAftertouchCam : public Behaviour
{
public:

    // The aftertouch-cam parameter block: a type tag in its leading word plus behaviour-specific
    // data. GetType returns the tag SetParameters asserts on.
    //
    // Layout pinned from the Parameters::Serialise<S> field-walk asm (the three visitors at
    // 0x8224C530 / 0x8224E458 / 0x822321B0): each Process<float>/fscanf/fprintf displacement off
    // the block pointer names an f32 slot; the leading `CameraShake::Parameters::Serialise(a1+8, a2)`
    // recursion names an embedded CameraShake::Parameters at +0x08 (the "Shake Params" sub-section).
    // meType(+0x00)/miParamWord1(+0x04) are the pre-existing behaviour header words SetParameters
    // reads. All three visitors walk the SAME field sequence in the SAME order, so the offsets below
    // are authoritative. The block's WIDTH and the slots the visitors skip come from the second
    // witness, Parameters::Construct below: the parameter bank calls it on this block and then
    // constructs the next block 108 bytes further on, and Construct itself seeds every word in
    // the +0x1C..+0x28 and +0x58..+0x68 runs that no visitor walks.
    class Parameters
    {
    public:
        // Walk this block's fields into a camera serialiser (DebugMenu / TextFile{Read,Write}).
        // Body + instantiations: BrnBehaviourAftertouchCamParameters.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeAftertouchCam GetType() const
        {
            return static_cast<EBehaviourTypeAftertouchCam>(meType);
        }

        s32 meType;        // +0x00  the behaviour type tag (eBehaviour*)
        s32 miParamWord1;  // +0x04  first behaviour-specific word

        // +0x08  embedded shake post-process tunings; walked first as the "Shake Params"
        //   sub-section (CameraShake::Parameters::Serialise(a1+8, a2) in every visitor).
        Utils::CameraShake::Parameters mShakeParams;   // +0x08 .. +0x18 (four f32)

        // +0x18 .. +0x2C  aftertouch-cam members that none of the three Serialise<S> instances
        //   walk. Construct below DOES seed four of the five words, so they are named slots
        //   rather than one reserved span; +0x18 is the only word nothing in this class
        //   writes or reads. FLAG: the four names are ours (no label survives for them);
        //   their offsets and their seeded values are attested.
        u8  maReserved18[4];                // +0x18  (never written, never walked)
        f32 mfField1C;                      // +0x1C
        f32 mfField20;                      // +0x20
        f32 mfField24;                      // +0x24
        f32 mfField28;                      // +0x28

        f32 mfSlowDistance;                 // +0x2C  "Slow Distance"
        f32 mfSlowHeight;                   // +0x30  "Slow Height"
        f32 mfFastDistance;                 // +0x34  "Fast Distance"
        f32 mfFastHeight;                   // +0x38  "Fast Height"
        f32 mfPitch;                        // +0x3C  "Pitch"
        f32 mfFOV;                          // +0x40  "FOV" (walked before "Pitch")
        f32 mfBlendFactorBlendFactor;       // +0x44  "Blend Factor Blend Factor"
        f32 mfMinimumBlendFactor;           // +0x48  "Minimum Blend Factor"
        f32 mfMaximumBlendFactor;           // +0x4C  "Maximum Blend Factor"
        f32 mfHeightDistanceBlendFactor;    // +0x50  "Height Distance Blend Factor"
        f32 mfHeightDistanceVelocityRange;  // +0x54  "Height Distance Velocity Range"

        // +0x58 .. +0x6C  the block's tail. Like the +0x1C..+0x28 run above, none of the
        //   three Serialise<S> instances walk these, but Construct seeds every one of them,
        //   so they are named slots at their attested offsets. The block is 108 bytes: the
        //   parameter bank places the next block (an aftertouch-crash one) immediately after
        //   it, which is what fixes the size. FLAG: the five names are ours.
        f32 mfField58;                      // +0x58
        f32 mfField5C;                      // +0x5C
        f32 mfField60;                      // +0x60
        f32 mfField64;                      // +0x64
        f32 mfField68;                      // +0x68

        // ------------------------------------------------------------------
        // Parameters::Construct -- the block's authored defaults, store for store.
        //
        // The parameter bank's own Construct calls this on its FIRST named block; it is a
        // straight-line run of constant stores with no control flow, so the transcription is
        // complete rather than a slice. The four shake words are the shared
        // CameraShake::Parameters seed, spelled as the call the compiler inlined there.
        // Field order below follows the block's offsets, not the emitted store order.
        // ------------------------------------------------------------------
        void Construct()
        {
            meType       = eBehaviourAftertouchCam;   // the tag SetParameters asserts on
            miParamWord1 = 0;

            mShakeParams.Construct();                 // +0x08 .. +0x14

            mfField1C = 1.0f;
            mfField20 = 1.0f;
            mfField24 = 1.0f;
            mfField28 = 0.5f;

            mfSlowDistance                = 4.0f;
            mfSlowHeight                  = 1.75f;
            mfFastDistance                = 8.0f;
            mfFastHeight                  = 2.0f;
            mfPitch                       = 15.0f;
            mfFOV                         = 90.0f;
            mfBlendFactorBlendFactor      = 0.01f;
            mfMinimumBlendFactor          = 0.001f;
            mfMaximumBlendFactor          = 0.01f;
            mfHeightDistanceBlendFactor   = 0.1f;
            mfHeightDistanceVelocityRange = 30.0f;

            mfField58 = 1.0f;
            mfField5C = 91.666664f;
            mfField60 = 0.5f;
            mfField64 = 10.0f;
            mfField68 = 1.0f;
        }
    };

    // ---- the Behaviour virtual interface (see the FLAG in the banner) ----------------------
    // Reset the tweaker it is handed; the aftertouch cam maps nothing onto it.        (slot 6)
    void SetupTweaker(Utils::Tweaker& lrTweaker) override;

    //                                                                                  (slot 7)
    const char* GetName() const override;

    // Adopt an aftertouch-cam parameter block: assert it carries the aftertouch-cam type tag,
    // then store the pointer. Declared over the derived Parameters, so it hides the base's pair.
    void SetParameters(const Parameters* lpParameters);

    // Adopt the authored shot this camera was created from. The behaviour factory builds a
    // generated aftertouchcam instance over the shot's reference spec and assigns it into the
    // behaviour's own instance member, immediately after SetParameters.
    void SetSourceShot(const Attrib::Gen::aftertouchcam& lrShot)
    {
        mSourceShot = lrShot;
    }

private:
    CollisionPolicyAttachedToVehicle mCollisionPolicy;
    Vector3                          mCurrentTargetPos;
    Vector3                          mWorldSpaceNormalizedVectorFromCar;
    Vector3                          mDesiredWorldSpaceNormalizedVectorFromCar;
    Vector3                          mCrashPoint;
    Utils::PositionLag               mPositionLag;
    CgsNumeric::Random               mRandom;
    Utils::CameraShake               mShake;
    f32                              mfHeight;
    f32                              mfDistance;
    f32                              mfBlendFactor;
    f32                              mfTimeSinceLastDecision;
    const Parameters*                mpParameters;

    // The authored shot this camera came into existence through.
    Attrib::Gen::aftertouchcam       mSourceShot;

public:
    // NEVER CALLED. Pins the member order the console layout fixes.
    static void _AssertLayout()
    {
        typedef BehaviourAftertouchCam T;
        static_assert(offsetof(T, mCollisionPolicy) < offsetof(T, mCurrentTargetPos) &&
                      offsetof(T, mCurrentTargetPos) < offsetof(T, mCrashPoint) &&
                      offsetof(T, mCrashPoint) < offsetof(T, mPositionLag) &&
                      offsetof(T, mPositionLag) < offsetof(T, mRandom) &&
                      offsetof(T, mRandom) < offsetof(T, mShake) &&
                      offsetof(T, mShake) < offsetof(T, mfHeight) &&
                      offsetof(T, mfTimeSinceLastDecision) < offsetof(T, mpParameters) &&
                      offsetof(T, mpParameters) < offsetof(T, mSourceShot),
                      "BehaviourAftertouchCam: members in console order");
        static_assert(offsetof(T, mCrashPoint) - offsetof(T, mCurrentTargetPos) == 0x30 &&
                      offsetof(T, mfHeight) - offsetof(T, mCrashPoint) == 0x80,
                      "BehaviourAftertouchCam: the pointer-free run +0x270..+0x320 keeps the console spacing");
    }
};

// ----------------------------------------------------------------------------
// SetParameters. The console also copies the block's +0x04 word into the base's debug-name slot;
// that store is omitted here: the aftertouch-cam block is one of the host head forks (it does
// not derive Behaviour::Parameters, so its +0x04 word is not a host name pointer). It feeds only
// the tweaker and the debug printers.
// ----------------------------------------------------------------------------
inline void
BehaviourAftertouchCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourAftertouchCam,
               "lpParameters->GetType() == eBehaviourAftertouchCam");
    mpParameters = lpParameters;
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H
