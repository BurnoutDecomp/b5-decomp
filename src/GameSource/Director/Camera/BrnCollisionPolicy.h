#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H

#include "types.hpp"
#include "BrnCommonTypes.h"                          // Matrix44Affine / Vector3 (SetTarget / SetVelocity)
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (the policy sanity tripwires)
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                // CgsSceneManager::EntityId (SetTarget)
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                   // Camera::AABBox (SetTarget)
#include "GameSource/Director/Camera/Utils/BrnVehicleCollisionPredictor.h"  // Utils::VehicleCollisionPredictor (embedded)
#include "GameSource/Director/Utils/BrnVehicleRef.h"                        // BrnDirector::VehicleRef (SetVehicleRef)

// ============================================================================
// GameSource/Director/Camera/BrnCollisionPolicy.h
//
// CANONICAL HOME for the director camera COLLISION-POLICY family (BrnCollisionPolicy.h on the
// X360 -- every one of the tripwires below quotes that filename, which is where these NAMES
// come from):
//   BrnDirector::Camera::CollisionPolicy                    (the abstract base)
//   BrnDirector::Camera::VisibilityCollisionPolicy          (the "free" policy)
//   BrnDirector::Camera::CollisionPolicyAttachedToVehicle   (the car-attached policy)
//   BrnDirector::Camera::GeometryCollisionPredictor
//   BrnDirector::Camera::VisibilityTest
//
// DE-FORK (2026-07-30). Until this wave the family had THREE partial definitions:
//   * this file          -- GeometryCollisionPredictor / VisibilityTest /
//                           CollisionPolicyAttachedToVehicle (no base, SetDesiredHeight only);
//   * Behaviours/BehaviourRig.h        -- CollisionPolicy + VisibilityCollisionPolicy;
//   * Behaviours/BrnBehaviourIceAnim.h -- its own CollisionPolicy + VisibilityCollisionPolicy
//                           + CollisionPolicyAttachedToVehicle.
// Any TU that pulled the named-parameter bank (-> BehaviourPassengerCam.h -> BehaviourRig.h)
// AND the ICE-anim behaviour hit C2011 on all three -- which is what kept the ICE-anim
// arbitrator states (CarSelect / OnlineCarSelect / RaceIntro / RankUp / PostEvent /
// DriveThru / OnlineRaceIntro) out of the build. One home settles it: the two policy classes
// MOVED here (BehaviourRig.h includes this file already), and the ICE-anim forks are retired.
// The merge is ADDITIVE -- every member/method either slice named is carried forward:
//   * VisibilityCollisionPolicy gains the three see-through state bytes the ICE-anim
//     behaviour's Update gate reads (policy +0x1A0..+0x1A2), carved out of the existing
//     [+0xE8, +0x210) reserved span at their asm-attested offsets;
//   * CollisionPolicyAttachedToVehicle gains the `CollisionPolicy` base (proved by
//     BehaviourIceAnim::GetCollisionPolicy @0x82246460 returning `&mAttachedToCarCollisionPolicy`
//     as a `CollisionPolicy*`) and its `Construct(s32)`.
//
// HOME for the class slices owned by this TU set:
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
//
// x64 NOTE: parity here is BY NAMED MEMBER (the project rule). The console displacements
// quoted throughout are provenance only -- the PC vptr/embedded-type widths differ, so the
// absolute offsets shift and nothing indexes these by offset.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// The two by-reference/by-pointer arguments the policy interface takes. Pointer/reference-only
// here, so forward declarations are correct (Camera.h and Behaviours/Behaviour.h are the homes;
// including either would create a cycle -- Behaviour.h's behaviours embed policies).
struct Camera;
struct BehaviourSharedInfo;

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

// ============================================================================
// BrnDirector::Camera::CollisionPolicy -- the abstract collision-policy interface every
// camera behaviour's GetCollisionPolicy() hands back.
//
// MOVED HERE (2026-07-30) from Behaviours/BehaviourRig.h, verbatim. That copy is retired in
// favour of this home; BehaviourRig.h includes this file already. FLAG: minimal slice -- the
// full method set lands with the CollisionPolicy TU.
// ============================================================================
class CollisionPolicy
{
public:
    virtual ~CollisionPolicy() {}
    virtual void GenerateSceneQueries(const void*, Camera&) {}
    virtual void ProcessSceneQueryResults(const void*, Camera&) {}

    // @0x82206450 (class TU; body in BrnCameraCollisionPolicy.cpp) -- give up:
    // record the failure reason in the shared info's validity account (+0x138),
    // drop the info's follow-request bit (the +0x140 u64, bit 1), raise mbFailed.
    void Fail(BehaviourSharedInfo* lpInfo, s32 liReason);

protected:
    // The counterpart store: every derived policy's Construct opens with `stb 0, 4(this)`
    // (e.g. CollisionPolicyAttachedToVehicle::Construct @0x822248D0). Named so the derived
    // bodies clear the base's own latch instead of reaching a private member.
    void ClearFailed() { mbFailed = false; }

private:
    // ADDITIVE GROW (Fail @0x82206450 `stb 1,4(this)`): the policy's failed latch.
    bool mbFailed;   // +0x04 (X360; right after the vptr)
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::CollisionPolicyAttachedToVehicle
//
// A camera collision policy that keeps the camera attached at a desired height above the tracked
// vehicle. The gyro-cam Update seeds the policy's desired height each frame; the ICE-anim
// behaviour embeds one and Constructs it, and hands it back through GetCollisionPolicy when the
// take's eye space is car-relative.
// ----------------------------------------------------------------------------
class CollisionPolicyAttachedToVehicle : public CollisionPolicy
{
public:
    // Set the desired camera height above the vehicle. @0x821F3950: marks the desired-height
    // override active (mbHaveDesiredHeight = 1), asserts the height is positive, then stores it.
    void SetDesiredHeight(f32 lfDesiredHeight);

    // ⭐ Construct @0x82224890 -- BODIED 2026-08-01 (below). BehaviourIceAnim::Construct
    // @0x822561E4 calls it on the policy it embeds at +0x260 with a trailing 0.
    //
    // ⚠️ THE PARAMETER IS A BOOL, NOT A SELECTOR. The store is `stb r4, 0x24F(r3)`
    // (@0x82224924) and the value is later consumed as an `lbz` handed to
    // FrustrumCollisionResolver::GenerateSceneQueries. All NINE call sites in the image pass a
    // literal 0 except BehaviourGameplayExternal::Construct @0x82224A44, which passes 1. An
    // `s32` narrowed to a byte member emits the same store, so the old spelling was not
    // contradicted by the ABI -- but {0,1} + a byte consumer is a flag.
    // (VERIFIED: width and call-site values. INFERRED: the bool type and the name.)
    void Construct(bool lbUseVehicleFrustumCollision);

    // ⭐ SetVehicleRef -- BODIED 2026-08-01 (below). X360 BehaviourIceAnim::Update
    // @0x82247568..0x822475A4 copies the 16-byte VehicleRef (the four words at ref
    // +0x00/+0x04/+0x08/+0x0C) into behaviour +0x480 -- which is this policy's +0x220, since
    // the policy sits at behaviour +0x260 -- immediately before raising
    // mbUseAttachedToCarCollisionPolicy. It is a POLICY write, not a Camera write (a retired
    // `IceAnimCameraOps::SetEyeSpaceRows` placeholder mis-attributed it to the camera).
    // ⚠️ The destination is now a NAMED member: mVehicleRef @+0x220, carved out of the old
    // maReserved214 span below -- see the ⛔ note on that span.
    // FLAG: the METHOD NAME is inferred from the role (no symbol survives); the four-word copy
    // at policy +0x220 is asm-attested, and +0x220 is independently attested six more times by
    // `VehicleRef::Get(this + 0x220, lpAllVehicleData)` in UpdateMinElevation @0x82240668,
    // GenerateSceneQueries @0x822526CC/@0x82252738/@0x822527B4 and ProcessSceneQueryResults
    // @0x822528BC -- plus by Construct itself, which seeds it with a verbatim inline of
    // VehicleRef::Construct() + VehicleRef::Set's E_PLAYER_CAR arm.
    void SetVehicleRef(const BrnDirector::VehicleRef& lrVehicleRef);

private:
    // FLAG: only the members the bodied functions reach are modelled at their asm-attested
    //   offsets; the rest of the policy rig lands with its full TU.
    //     +0x000 .. +0x20F  policy rig not modelled here
    //     +0x210            mfDesiredHeight     (stfs f31, 0x210)
    //     +0x214 .. +0x21F  rig members not modelled here
    //     +0x220            mVehicleRef         (16 bytes)
    //     +0x230 .. +0x24A  rig members not modelled here
    //     +0x24B            mbHaveDesiredHeight (stb 1, 0x24B)
    //     +0x24C .. +0x24E  rig members not modelled here
    //     +0x24F            mbUseVehicleFrustumCollision (Construct's argument)
    //
    // ⛔ CORRECTED 2026-08-01 -- THE OLD `maReserved214[0x214 .. 0x24A]` SPAN SWALLOWED A
    // NAMED MEMBER. It covered +0x220, where the policy's own BrnDirector::VehicleRef lives
    // (attested seven independent ways, see SetVehicleRef above) and +0x230, where a
    // Utils::SmoothMover sits (`SmoothMover::Update(this + 0x230, ...)` @0x822406D0). With the
    // span in place SetVehicleRef had no member to write at all -- it could only ever have been
    // a reinterpret_cast into reserved bytes. The VehicleRef is carved out by name; the
    // SmoothMover and the five named floats between +0x234 and +0x244 stay inside the two
    // residual spans until that TU lands.
    //
    // ⭐ SIZE 0x250, GROWN 2026-07-29 (was 0x24C, which was 4 bytes short -- the old tail
    // simply stopped at the last member this header names). Pinned from
    // BehaviourGameplayExternal, which embeds one of these at +0x50 and whose next member
    // (mAirShake) the asm puts at +0x2A0: 0x50 + 0x250 == 0x2A0 exactly. Two further console
    // stores land inside the new tail and nowhere else -- Construct @0x82224A18's *(beh+668)
    // and *(beh+669) (policy +0x24C/+0x24D) and Prepare @0x82240738's *(beh+670) (policy
    // +0x24E) -- and that last one is the "+0x29E" the committed
    // SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish note quotes as an
    // unidentified behaviour-relative flag. It is a COLLISION-POLICY field, not a
    // behaviour field. The IceAnim fork's independent slice agrees on 0x250.
    // (the leading span now starts AFTER the CollisionPolicy base sub-object -- the console
    //  vptr that used to sit inside maReserved000 is the base's; same convention BehaviourRig.h's
    //  VisibilityCollisionPolicy uses. Console displacements in the comments are unchanged.)
    u8  maReserved000[0x210 - sizeof(CollisionPolicy)];  // .. +0x20F  rig members not modelled here
    f32 mfDesiredHeight;                      // +0x210            desired camera height (stored)
    u8  maReserved214[0x220 - 0x214];         // +0x214 .. +0x21F  rig members not modelled here
    BrnDirector::VehicleRef mVehicleRef;      // +0x220            the vehicle the camera hangs off
    u8  maReserved230[0x24B - 0x230];         // +0x230 .. +0x24A  SmoothMover @+0x230 + 5 floats
    u8  mbHaveDesiredHeight;                  // +0x24B            desired-height override active flag
    u8  maReserved24C[0x24F - 0x24C];         // +0x24C .. +0x24E  two selects + the blend-reset one-shot
    u8  mbUseVehicleFrustumCollision;         // +0x24F            Construct's argument
};

// ----------------------------------------------------------------------------
// CollisionPolicyAttachedToVehicle::Construct @0x82224890 -- BODIED 2026-08-01, from the asm.
//
// ⚠️ ONLY THE MEMBERS THIS HEADER NAMES ARE WRITTEN. The console body also zeroes the
// FrustrumCollisionResolver sub-object (+0x10/+0x60/+0xB0/+0x100 record heads, a Vector4 at
// +0x150 and an f32 0.01f at +0x160), the LineTestNearest post-box head (+0x170), the
// GroundConstraint head (+0x1C0), and seeds five more floats (+0x234 = 0.0f, +0x238 = -89.0f
// min elevation, +0x23C = the near-clip global flt_82CDA560, +0x240 = FLT_MAX collision
// radius) plus three gate bytes (+0x248 = 1, +0x249/+0x24A/+0x24C/+0x24D = 0). Every one of
// those lands inside a reserved span above, so writing them would mean poking bytes by
// offset -- which is exactly what the x64 rule forbids. They are recorded here and GATED.
// ⚠️ CONSEQUENCE: a policy constructed through this body has an UNSEEDED collision radius and
// min elevation. Anything that starts resolving collisions against it before the full policy
// TU lands will behave as if those tunings were whatever the memory held.
// ⚠️ Construct does NOT write +0x00 -- the vptr is installed by the C++ constructor, not here.
// DELETE-WHEN: the collision-policy rig TU lands and the spans become named members.
// ----------------------------------------------------------------------------
inline void CollisionPolicyAttachedToVehicle::Construct(bool lbUseVehicleFrustumCollision)
{
    // 0x822248D0  stb 0, 4(this)  -- the CollisionPolicy base's own failure flag.
    ClearFailed();

    // 0x822248D4..0x822248E4 -- an inlined VehicleRef::Construct() followed by the
    // E_PLAYER_CAR arm of VehicleRef::Set: byte-for-byte the same four stores, same order.
    mVehicleRef.Construct();
    mVehicleRef.Set(BrnDirector::VehicleRef::E_PLAYER_CAR,
                    static_cast<EActiveRaceCarIndex>(0), 0u);

    // 0x822248EC  stfs -1.0f, 0x210(this)
    mfDesiredHeight      = -1.0f;
    // 0x82224914  stb 0, 0x24B(this)
    mbHaveDesiredHeight  = 0;
    // 0x82224924  stb r4, 0x24F(this)  <- THE ARGUMENT
    mbUseVehicleFrustumCollision = lbUseVehicleFrustumCollision ? 1u : 0u;

    // ⚠️ GATE: the resolver / post-box / ground-constraint zeroing and the tuning seeds
    //   listed in the banner above.
}

// ----------------------------------------------------------------------------
// CollisionPolicyAttachedToVehicle::SetVehicleRef -- BODIED 2026-08-01. A whole-record
// assignment: the console copies all four aligned words (@0x82247598..0x822475A4), not a
// partial write, so this is memberwise VehicleRef assignment.
// ----------------------------------------------------------------------------
inline void CollisionPolicyAttachedToVehicle::SetVehicleRef(const BrnDirector::VehicleRef& lrVehicleRef)
{
    mVehicleRef = lrVehicleRef;
}

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

// ============================================================================
// BrnDirector::Camera::VisibilityCollisionPolicy -- the "free" (not car-attached) camera
// collision policy: it runs the scene queries, keeps the geometry/vehicle collision
// predictors, and owns the see-through state the behaviours' Update gates read.
//
// MOVED HERE (2026-07-30) from Behaviours/BehaviourRig.h. The opaque blob is CARVED around
// the members the class-TU bodies touch (X360 offsets in comments; ORDER preserved, PC offsets
// differ -- all access is BY NAME). The remaining reserved spans still need the unrecovered
// types (LineTestNearestPostBox, VolumeTestDeepestPostBox, GroundConstraint etc.).
// Nominal X360 size 0x240 bytes -- which the retired BrnBehaviourIceAnim.h slice independently
// agreed on (its own reserved tail also ended at 0x240).
//
// FLAG: minimal slice -- the full policy layout/method set lands with its own TU.
// ============================================================================
class VisibilityCollisionPolicy : public CollisionPolicy
{
public:
    void Construct();
    void SetCanFail(bool lbCanFail);
    void SetTarget(Matrix44Affine lTargetTransform, AABBox lTargetAABB,
                   CgsSceneManager::EntityId lTargetEntityId);
    void SetTestLookingAt(bool lbTestLookingAt);
    void SetVelocity(Vector3 lVelocity);
    bool IsVisibilityInterrupted() const;
    float GetMinTimeToVisibilityFailure() const;

    // ---- class-TU surface (bodies in BrnVisibilityCollisionPolicy.cpp) ----

    // The guards the two time queries assert on (the X360 inlines the embedded
    // predictors' flag reads into the wrappers).
    bool WillCollideWithGeometry() const { return mGeometryCollisionPredictor.WillCollide(); }
    bool WillCollideWithVehicle() const  { return mVehicleCollisionPredictor.HasPredictedCollision(); }

    // @0x821F38E0 (BrnCollisionPolicy.h:489) -- raise the desired-height override
    // latch, assert the height positive, store it.
    void SetDesiredHeight(f32 lfDesiredHeight);

    // @0x821F37C8 (BrnCollisionPolicy.h:425 wrapper + the embedded geometry
    // predictor's own :206 tripwire) -- predicted time until the camera hits
    // geometry.
    f32 TimeUntilCollisionWithGeometry() const;

    // @0x821F3858 (BrnCollisionPolicy.h:431 wrapper + the embedded vehicle
    // predictor's own BrnVehicleCollisionPredictor.h:69 tripwire) -- predicted
    // time until the camera hits the tracked vehicle.
    f32 TimeUntilCollisionWithVehicle() const;

    // ---- the see-through state block -------------------------------------------------
    // ADDITIVE GROW (de-fork 2026-07-30, carried forward from the retired
    // BrnBehaviourIceAnim.h slice). BehaviourIceAnim::Construct @0x82246048 seeds all three
    // bytes (`stb 1 / stb 0 / stb 1` at policy +0x1A0/+0x1A1/+0x1A2) and its Update
    // @0x82247108 consults exactly this predicate before raising the camera's see-through
    // request. Exposed as named accessors so no caller pokes the (private) bytes.
    bool ShouldRaiseSeeThrough() const
    {
        return mbSeeThroughAlways || (mbSeeThroughEnabled && !mbSeeThroughSuppressed);
    }

    void SetSeeThroughEnabled(bool lbEnabled)       { mbSeeThroughEnabled    = lbEnabled; }   // +0x1A0
    void SetSeeThroughAlways(bool lbAlways)         { mbSeeThroughAlways     = lbAlways; }    // +0x1A1
    void SetSeeThroughSuppressed(bool lbSuppressed) { mbSeeThroughSuppressed = lbSuppressed; }// +0x1A2

private:
    // FLAG: reserved spans = rig members not yet recovered (LineTestNearestPostBox,
    //   VolumeTestDeepestPostBox, GroundConstraint etc.); the named members are the
    //   asm-attested carves from the class-TU bodies + the ICE-anim behaviour.
    u8 maReservedToVehiclePredictor[0x70 - 0x08];              // X360 [+0x08, +0x70)
    Utils::VehicleCollisionPredictor mVehicleCollisionPredictor;   // X360 +0x70 (flag/time @+0x70/+0x74)
    u8 maReserved78[0x80 - 0x78];                              // X360 [+0x78, +0x80)
    GeometryCollisionPredictor mGeometryCollisionPredictor;    // X360 +0x80 (its +0x60/+0x64 pair == policy +0xE0/+0xE4)
    u8 maReservedE8[0x1A0 - 0xE8];                             // X360 [+0xE8, +0x1A0)
    bool mbSeeThroughEnabled;                                  // X360 +0x1A0 (default true)
    bool mbSeeThroughAlways;                                   // X360 +0x1A1 (default false)
    bool mbSeeThroughSuppressed;                               // X360 +0x1A2 (default true)
    u8 maReserved1A3[0x210 - 0x1A3];                           // X360 [+0x1A3, +0x210)
    f32 mfDesiredHeight;                                       // X360 +0x210 (SetDesiredHeight stores)
    u8 maReserved214[0x23C - 0x214];                           // X360 [+0x214, +0x23C)
    u8 mbHaveDesiredHeight;                                    // X360 +0x23C (SetDesiredHeight raises)
    u8 maReservedTail[0x240 - 0x23D];                          // X360 [+0x23D, +0x240)
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H
