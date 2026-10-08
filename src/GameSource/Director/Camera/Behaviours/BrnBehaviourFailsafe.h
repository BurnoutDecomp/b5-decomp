#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FAILSAFE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FAILSAFE_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the SetParameters type assert)
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"  // Utils::CameraShake::Parameters + Utils::Looker::Parameters (embedded param sub-blocks)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"            // the Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"              // CollisionPolicyAttachedToVehicle
#include "GameSource/Director/Camera/Utils/BrnOrientationLag.h"         // Utils::OrientationLag (+ ::Parameters)
#include "GameSource/Director/Shots/ShotControllers/BrnKeyAnimController.h" // KeyAnimController

#include <cstddef>   // offsetof (the layout pins)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h
//
// BrnDirector::Camera::BehaviourFailsafe -- the "failsafe" camera behaviour the arbitrator
// testbed allocates: it plays the first take of the "Failsafe" ICE shot group around the player's
// car, re-anchored to an upright, orientation-lagged copy of the car's frame and spun slowly about
// the vertical. It owns a vehicle-attached collision policy. It derives the canonical
// Camera::Behaviour, so the behaviour manager can pool it and dispatch the base's eight-slot
// vtable; it overrides slots 0, 1, 2, 5, 6 and 7 (bodies in BrnBehaviourFailsafe.cpp).
//
// Layout (declaration order; console offsets, the host widens the base head and the pointers):
//   +0x020 mOrientationLag   +0x070 mOrientationLagParams   +0x090 mCollisionPolicy
//   +0x2E0 mKeyAnimController                               +0xA50 mpParameters
//   +0xA54 mfRotationOffset                                 (console size 0xA60)
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// FLAG: minimal slice of the camera-behaviour type tag. The behaviours each carry a type id in
//   the leading word of their Parameters block; SetParameters asserts the block's id is the
//   failsafe one. The console value for eBehaviourFailsafe is 12 (the asm at 0x821F4328 compares
//   the block's first word against 0xC). Replace with the real EBehaviourType enum when the
//   Behaviour base TU lands; the failsafe enumerator's VALUE (12) is pinned from the asm.
enum EBehaviourTypeFailsafe
{
    eBehaviourFailsafe = 12
};

class BehaviourFailsafe : public Behaviour
{
public:

    // The failsafe parameter block: the Behaviour::Parameters head (type tag + debug name), three
    // embedded camera parameter sub-blocks (shake, position lag, looker), then the failsafe-specific
    // distance/height/pitch/blend tunables. GetType returns the tag SetParameters asserts on.
    //
    // Console layout (196 bytes) from the three Parameters::Serialise<S> walks: the shake sub-block
    //   at +0x08, the looker sub-block at +0x2C, then f32 fields at +0x90..+0xBC and a trailing
    //   bool at +0xC0. On the host the head's debug-name pointer is 8 bytes wide, so the derived
    //   members sit 8 bytes later and keep the console spacing (pinned in
    //   BrnBehaviourFailsafeSerialise.cpp).
    class Parameters : public Behaviour::Parameters
    {
    public:
        // Walk this block's fields into a camera serialiser (DebugMenu / TextFile{Read,Write}).
        // Body + instantiations: BrnBehaviourFailsafeSerialise.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeFailsafe GetType() const
        {
            return static_cast<EBehaviourTypeFailsafe>(mType);
        }

        // The head only: the parameter bank's Construct inlines exactly these two stores (the
        // cleared debug name and the tag) for its failsafe block and seeds nothing else.
        void Construct()
        {
            Behaviour::Parameters::Construct();
            mType = eBehaviourFailsafe;
        }

        // ---- embedded camera post-process parameter sub-blocks (serialised as nested sections) ----
        Utils::CameraShake::Parameters mShakeParams;   // +0x08  "Shake Params"  (CameraShake::Parameters, 16B)

        Utils::PositionLag::Parameters mLagParams;     // +0x18  (PositionLag::Parameters, 20B; not walked)
        Utils::Looker::Parameters mLookerParams;       // +0x2C  "Looker Params" (Looker::Parameters, 100B)

        // ---- failsafe distance/height/pitch + blend-factor tunables (f32 leaves, ascending offset) ----
        f32 mfSlowDistance;                 // +0x90 (144)  "Slow Distance"
        f32 mfSlowHeight;                   // +0x94 (148)  "Slow Height"
        f32 mfSlowPitch;                    // +0x98 (152)  "Slow Pitch"
        f32 mfFastDistance;                 // +0x9C (156)  "Fast Distance"
        f32 mfFastHeight;                   // +0xA0 (160)  "Fast Height"
        f32 mfFastPitch;                    // +0xA4 (164)  "Fast Pitch"
        f32 mfFOV;                          // +0xA8 (168)  "FOV"
        f32 mfBlendFactorBlendFactor;       // +0xAC (172)  "Blend Factor Blend Factor" (label verbatim from asm)
        f32 mfMinimumBlendFactor;           // +0xB0 (176)  "Minimum Blend Factor"
        f32 mfMaximumBlendFactor;           // +0xB4 (180)  "Maximum Blend Factor"
        f32 mfHeightDistanceBlendFactor;    // +0xB8 (184)  "Height Distance Blend Factor"
        f32 mfHeightDistanceVelocityRange;  // +0xBC (188)  "Height Distance Velocity Range"
        bool mbStickToGround;               // +0xC0 (192)  "Stick to ground"
    };

    // ---- the Behaviour virtual interface -----------------------------------------------------
    // Seed a freshly pooled instance (policy, controller playback, orientation lag, spin).
    //                                                                                  (slot 0)
    void Construct() override;

    // Bind the key-anim controller to the first "Failsafe" take and point the orientation lag at
    // its tunables. Always true.                                                       (slot 1)
    bool Prepare(const BehaviourSharedPrepareReleaseInfo& lSharedInfo) override;

    // Run the take, re-anchor it to the lagged upright car frame and spin it.          (slot 2)
    bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrSharedInfo) override;

    // The vehicle-attached policy Construct seeded.                                    (slot 5)
    CollisionPolicy* GetCollisionPolicy() override;

    // Reset the tweaker it is handed; the failsafe maps nothing onto it.              (slot 6)
    void SetupTweaker(Utils::Tweaker& lrTweaker) override;

    //                                                                                  (slot 7)
    const char* GetName() const override;

    // Adopt a failsafe parameter block: assert it carries the failsafe type tag, store the
    // pointer and take the block's debug name. Declared over the derived Parameters, so it hides
    // the base's pair.
    void SetParameters(const Parameters* lpParameters);

private:
    Utils::OrientationLag               mOrientationLag;
    Utils::OrientationLag::Parameters   mOrientationLagParams;
    CollisionPolicyAttachedToVehicle    mCollisionPolicy;
    KeyAnimController                   mKeyAnimController;
    const Parameters*                   mpParameters;
    f32                                 mfRotationOffset;

public:
    // NEVER CALLED. Pins the member order the console layout fixes.
    static void _AssertLayout()
    {
        typedef BehaviourFailsafe T;
        static_assert(offsetof(T, mOrientationLag) < offsetof(T, mOrientationLagParams) &&
                      offsetof(T, mOrientationLagParams) < offsetof(T, mCollisionPolicy) &&
                      offsetof(T, mCollisionPolicy) < offsetof(T, mKeyAnimController) &&
                      offsetof(T, mKeyAnimController) < offsetof(T, mpParameters) &&
                      offsetof(T, mpParameters) < offsetof(T, mfRotationOffset),
                      "BehaviourFailsafe: members in console order");
    }
};

// ----------------------------------------------------------------------------
// SetParameters: the type assert, the pointer, then the block's debug name into the base's
// debug-parameters name (it feeds the tweaker and the debug printers).
// ----------------------------------------------------------------------------
inline void
BehaviourFailsafe::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == eBehaviourFailsafe,
               "lpParameters->GetType() == eBehaviourFailsafe");
    mpParameters = lpParameters;
    SetDebugParametersName(lpParameters->GetDebugName());
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_FAILSAFE_H
