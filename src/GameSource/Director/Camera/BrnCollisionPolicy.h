#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the policy sanity tripwires)

// ============================================================================
// GameSource/Director/Camera/BrnCollisionPolicy.h
//
// The director camera collision-policy types (BrnCollisionPolicy.h on the X360). HOME for the
// class slices owned by this TU set:
//   - BrnDirector::Camera::GeometryCollisionPredictor::GetTimeUntilCollision @0x821F36C0
//       (CollisionPolicy.h:206 assert -> mbWillCollide)
//   - BrnDirector::Camera::CollisionPolicyAttachedToVehicle::SetDesiredHeight @0x821F3950
//       (CollisionPolicy.h:489 assert -> lfDesiredHeight > 0.0f)
//   - BrnDirector::Camera::VisibilityTest::GetOffscreenTime @0x821F3718
//       (BrnCollisionPolicy.h:248 assert -> mbTestLookingAt)
//   - BrnDirector::Camera::VisibilityTest::IsOnScreen @0x821F3770
//       (BrnCollisionPolicy.h:269 assert -> mbTestLookingAt)
//
// Each function bodies in its own .cpp next to this header. Only the members each function
// touches are modelled, BY NAME, at their asm-attested offsets; the full policy rigs land with
// their own TUs. Reserved spans place the written/read members exactly.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::GeometryCollisionPredictor
//
// Predicts whether the camera ray is about to hit geometry and, if so, when. The visibility
// collision policy queries GetTimeUntilCollision after a scene query, guarded by the
// mbWillCollide flag.
// ----------------------------------------------------------------------------
class GeometryCollisionPredictor
{
public:
    // Return the predicted time (seconds) until the camera collides with geometry. @0x821F36C0:
    // asserts a collision was actually predicted (mbWillCollide), then returns mfTimeUntilCollision.
    f32 GetTimeUntilCollision() const;

    // GROWN for VisibilityCollisionPolicy::TimeUntilCollisionWithGeometry @0x821F37C8
    // (its h:425 wrapper assert reads this flag through the embedded predictor).
    bool WillCollide() const { return mbWillCollide != 0; }

private:
    // FLAG: only the two members GetTimeUntilCollision reads are modelled at their asm-attested
    //   offsets; the rest of the predictor rig lands with its full TU.
    //     +0x00 .. +0x5F  predictor rig (ray / hit data) not modelled here
    //     +0x60           mfTimeUntilCollision (lfs f1, 0x60)
    //     +0x64           mbWillCollide        (lbz 0x64; asserted set)
    u8  maReserved00[0x60];   // +0x00 .. +0x5F  rig members not modelled here
    f32 mfTimeUntilCollision; // +0x60           predicted time-until-collision (returned)
    u8  mbWillCollide;        // +0x64           a collision was predicted (asserted set)
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CollisionPolicyAttachedToVehicle
//
// A camera collision policy that keeps the camera attached at a desired height above the tracked
// vehicle. The gyro-cam Update seeds the policy's desired height each frame.
// ----------------------------------------------------------------------------
class CollisionPolicyAttachedToVehicle
{
public:
    // Set the desired camera height above the vehicle. @0x821F3950: marks the desired-height
    // override active (mbHaveDesiredHeight = 1), asserts the height is positive, then stores it.
    void SetDesiredHeight(f32 lfDesiredHeight);

private:
    // FLAG: only the two members SetDesiredHeight writes are modelled at their asm-attested
    //   offsets; the rest of the policy rig lands with its full TU.
    //     +0x000 .. +0x20F  policy rig not modelled here
    //     +0x210            mfDesiredHeight     (stfs f31, 0x210)
    //     +0x211 .. +0x24A  rig members not modelled here
    //     +0x24B            mbHaveDesiredHeight (stb 1, 0x24B)
    u8  maReserved000[0x210];                 // +0x000 .. +0x20F  rig members not modelled here
    f32 mfDesiredHeight;                      // +0x210            desired camera height (stored)
    u8  maReserved214[0x24B - 0x214];         // +0x214 .. +0x24A  rig members not modelled here
    u8  mbHaveDesiredHeight;                  // +0x24B            desired-height override active flag
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::VisibilityTest
//
// The per-frame visibility result the visibility collision policy produces from a scene query:
// whether the tracked subject is currently on screen and, if not, for how long it has been
// off screen. Both reads are guarded by mbTestLookingAt (the policy only publishes these when
// it was actually asked to test line-of-sight); VisibilityCollisionPolicy::ProcessSceneQueryResults
// reads them after the query completes.
// ----------------------------------------------------------------------------
class VisibilityTest
{
public:
    // Return how long (seconds) the subject has been off screen. @0x821F3718: asserts the
    // looking-at test was enabled (mbTestLookingAt), then returns mfOffscreenTime.
    f32 GetOffscreenTime() const;

    // Return whether the subject is currently on screen. @0x821F3770: asserts the looking-at
    // test was enabled (mbTestLookingAt), then returns mbOnScreen.
    bool IsOnScreen() const;

private:
    // FLAG: only the members the two getters touch are modelled at their asm-attested offsets;
    //   the rest of the visibility-test rig lands with its full TU.
    //     +0x000 .. +0x0A3  rig members not modelled here
    //     +0x0A4            mfOffscreenTime  (lfs f1, 0xA4 in GetOffscreenTime)
    //     +0x0A5 .. +0x0AF  rig members not modelled here
    //     +0x0B0            mbTestLookingAt  (lbz 0xB0; asserted set by both getters)
    //     +0x0B1            rig member not modelled here
    //     +0x0B2            mbOnScreen       (lbz 0xB2 in IsOnScreen)
    u8  maReserved000[0xA4];                 // +0x000 .. +0x0A3  rig members not modelled here
    f32 mfOffscreenTime;                     // +0x0A4            time spent off screen (returned)
    u8  maReserved0A8[0xB0 - 0xA8];          // +0x0A8 .. +0x0AF  rig members not modelled here
    u8  mbTestLookingAt;                     // +0x0B0            line-of-sight test enabled (asserted)
    u8  maReserved0B1;                       // +0x0B1            rig member not modelled here
    u8  mbOnScreen;                          // +0x0B2            subject currently on screen (returned)
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H
