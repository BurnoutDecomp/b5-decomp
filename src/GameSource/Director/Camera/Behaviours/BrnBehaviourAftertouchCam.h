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
// crash-aftertouch follow camera the testbed / behaviour-manager installs from an aftertouchcam
// shot). It frames the lagged player car from the side away from a target point -- the rival car
// it is most likely to hit, re-chosen every mfTimeBetweenDecisions -- at a height and distance
// eased by the car's speed between the shot's slow and fast values, pitched by the shot's pitch
// and steered toward the car's travel by how much the aftertouch input agrees with it. It derives
// the canonical Camera::Behaviour, so the behaviour manager can pool it and dispatch the base's
// eight-slot vtable; it overrides slots 0, 1, 2, 5, 6 and 7 (bodies in
// BrnBehaviourAftertouchCam.cpp).
//
// Layout (declaration order; console offsets, the host widens the base head and the pointer):
//   +0x020 mCollisionPolicy   +0x270 mCurrentTargetPos   +0x280 mWorldSpaceNormalizedVectorFromCar
//   +0x290 mDesiredWorldSpaceNormalizedVectorFromCar      +0x2A0 mCrashPoint
//   +0x2B0 mPositionLag       +0x2E0 mRandom             +0x310 mShake
//   +0x320 mfHeight  +0x324 mfDistance  +0x328 mfBlendFactor  +0x32C mfTimeSinceLastDecision
//   +0x330 mpParameters       +0x334 mSourceShot         (console size 0x350)
//
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

    // The aftertouch-cam parameter block: the Behaviour::Parameters head (type tag + debug name)
    // plus behaviour-specific data. GetType returns the tag SetParameters asserts on.
    //
    // Member names and order are the declaration reference's; the console offsets in the
    // comments come from the three Parameters::Serialise<S> field walks (each displacement off
    // the block pointer names an f32 slot; the leading shake recursion sits at +0x08) and from
    // Parameters::Construct, which seeds every word from +0x00 to +0x68 except the lag block's
    // version word. The console block is 108 bytes; on the host the head's debug-name pointer is
    // 8 bytes wide, so the derived members sit 8 bytes later and keep the console spacing among
    // themselves (pinned in BrnBehaviourAftertouchCamParameters.cpp).
    class Parameters : public Behaviour::Parameters
    {
    public:
        // Walk this block's fields into a camera serialiser (DebugMenu / TextFile{Read,Write}).
        // Body + instantiations: BrnBehaviourAftertouchCamParameters.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeAftertouchCam GetType() const
        {
            return static_cast<EBehaviourTypeAftertouchCam>(mType);
        }

        // "Shake Params", walked first by every visitor.
        Utils::CameraShake::Parameters mShakeParams;   // console +0x08 .. +0x17
        // Not walked; Construct seeds its four response/smoothing words.
        Utils::PositionLag::Parameters mLagParams;     // console +0x18 .. +0x2B

        f32 mfMinDistance;                              // console +0x2C  "Slow Distance"
        f32 mfMinHeight;                                // console +0x30  "Slow Height"
        f32 mfMaxDistance;                              // console +0x34  "Fast Distance"
        f32 mfMaxHeight;                                // console +0x38  "Fast Height"
        f32 mfPitch;                                    // console +0x3C  "Pitch"
        f32 mfFOV;                                      // console +0x40  "FOV" (walked before "Pitch")
        f32 mfBlendFactorBlendFactor;                   // console +0x44  "Blend Factor Blend Factor"
        f32 mfMinimumBlendFactor;                       // console +0x48  "Minimum Blend Factor"
        f32 mfMaximumBlendFactor;                       // console +0x4C  "Maximum Blend Factor"
        f32 mfHeightDistanceBlendFactor;                // console +0x50  "Height Distance Blend Factor"
        f32 mfHeightDistanceVelocityRange;              // console +0x54  "Height Distance Velocity Range"

        // The rival-selection tail: not walked, every word seeded by Construct.
        f32 mfTimeToRivalImpactUncertaintyPadding;      // console +0x58
        f32 mfMaximumDistanceForConsiderationOfRivals;  // console +0x5C
        f32 mfTimingSimilarityThreshold;                // console +0x60
        f32 mfDistanceSimilarityThreshold;              // console +0x64
        f32 mfTimeBetweenDecisions;                     // console +0x68

        // ------------------------------------------------------------------
        // Parameters::Construct -- the block's authored defaults, store for store.
        //
        // The parameter bank's own Construct calls this on its FIRST named block; it is a
        // straight-line run of constant stores with no control flow. The head is the
        // Behaviour::Parameters seed plus the tag; the four shake words are the shared
        // CameraShake::Parameters seed, spelled as the call the compiler inlined there; the lag
        // block gets its four response/smoothing words and its version word is left alone.
        // Field order below follows the block's offsets, not the emitted store order.
        // ------------------------------------------------------------------
        void Construct()
        {
            Behaviour::Parameters::Construct();       // stw 0, +0x04
            mType = eBehaviourAftertouchCam;          // the tag SetParameters asserts on

            mShakeParams.Construct();                 // console +0x08 .. +0x14

            mLagParams.mfXResponse = 1.0f;            // console +0x1C
            mLagParams.mfYResponse = 1.0f;            // console +0x20
            mLagParams.mfZResponse = 1.0f;            // console +0x24
            mLagParams.mfSmoothing = 0.5f;            // console +0x28

            mfMinDistance                 = 4.0f;
            mfMinHeight                   = 1.75f;
            mfMaxDistance                 = 8.0f;
            mfMaxHeight                   = 2.0f;
            mfPitch                       = 15.0f;
            mfFOV                         = 90.0f;
            mfBlendFactorBlendFactor      = 0.01f;
            mfMinimumBlendFactor          = 0.001f;
            mfMaximumBlendFactor          = 0.01f;
            mfHeightDistanceBlendFactor   = 0.1f;
            mfHeightDistanceVelocityRange = 30.0f;

            mfTimeToRivalImpactUncertaintyPadding     = 1.0f;
            mfMaximumDistanceForConsiderationOfRivals = 91.666664f;
            mfTimingSimilarityThreshold               = 0.5f;
            mfDistanceSimilarityThreshold             = 10.0f;
            mfTimeBetweenDecisions                    = 1.0f;
        }
    };

    // ---- the Behaviour virtual interface -----------------------------------------------------
    // Seed a freshly pooled instance (policy, position lag, random stream, shake).    (slot 0)
    void Construct() override;

    // Drop the prepared latch (Update re-seeds on its first frame) and start from the block's
    // fast height / distance and minimum blend factor. Always true.                    (slot 1)
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo) override;

    // Frame the lagged player car away from the chosen target and publish the camera. (slot 2)
    bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo) override;

    // The vehicle-attached policy Construct seeded.                                    (slot 5)
    CollisionPolicy* GetCollisionPolicy() override;

    // Reset the tweaker it is handed; the aftertouch cam maps nothing onto it.        (slot 6)
    void SetupTweaker(Utils::Tweaker& lrTweaker) override;

    //                                                                                  (slot 7)
    const char* GetName() const override;

    // Adopt an aftertouch-cam parameter block: assert it carries the aftertouch-cam type tag,
    // store the pointer and take the block's debug name. Declared over the derived Parameters, so
    // it hides the base's pair.
    void SetParameters(const Parameters* lpParameters);

    // Adopt the authored shot this camera was created from. The behaviour factory builds a
    // generated aftertouchcam instance over the shot's reference spec and assigns it into the
    // behaviour's own instance member, immediately after SetParameters.
    void SetSourceShot(const Attrib::Gen::aftertouchcam& lrShot)
    {
        mSourceShot = lrShot;
    }

private:
    // The best rival found so far by one CalculateDesiredTargetPos sweep (console layout:
    // mbWillPass +0x00, mfTimeToPassing +0x04, mbWillPassIfHeadingAdjusted +0x08,
    // mfTimeToPassingIfHeadingAdjusted +0x0C, mfDotWithCurrentHeading +0x10, mfDistance +0x14,
    // mPosition +0x20, mbIsValid +0x30). Only mbIsValid is initialised before the sweep.
    struct AftertouchRival
    {
        bool    mbWillPass;                         // closing on the player within the crash time
        f32     mfTimeToPassing;
        bool    mbWillPassIfHeadingAdjusted;        // would close if the player's speed carried it
        f32     mfTimeToPassingIfHeadingAdjusted;
        f32     mfDotWithCurrentHeading;            // rival-to-player against the camera direction
        f32     mfDistance;                         // rival to player
        Vector3 mPosition;
        bool    mbIsValid;
    };

    // Weigh rival luRivalToConsiderForBest against the best so far and take it when it is better.
    void AssignIfBetterRival(const BehaviourSharedInfo& lrSharedInfo,
                             AftertouchRival& lCurrentBestRivalInOut, u32 luRivalToConsiderForBest);

    // The best rival's position, or lCurrentTargetPos when no race car qualifies; choosing a
    // rival restarts the decision clock.
    Vector3 CalculateDesiredTargetPos(const BehaviourSharedInfo& lrSharedInfo, Vector3 lCurrentTargetPos);

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
// SetParameters: the type assert, the pointer, then the block's debug name into the base's
// debug-parameters name (it feeds the tweaker and the debug printers).
// ----------------------------------------------------------------------------
inline void
BehaviourAftertouchCam::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourAftertouchCam,
               "lpParameters->GetType() == eBehaviourAftertouchCam");
    mpParameters = lpParameters;
    SetDebugParametersName(lpParameters->GetDebugName());
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_AFTERTOUCH_CAM_H
