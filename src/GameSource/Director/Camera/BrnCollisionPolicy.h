#ifndef GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H
#define GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H

#include "types.hpp"
#include <cfloat>                                    // FLT_MAX (ResetRadiusSmoothing; XEX rodata @0x8200173C)
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
    // Set the desired camera height above the vehicle. @0x821F3950: raises
    // mbUseGroundConstraint (+0x24B), asserts the height is positive, then stores it at
    // +0x210 -- which the DWARF member order puts INSIDE mGroundConstraint, i.e. the console
    // spelling is `mbUseGroundConstraint = true; mGroundConstraint.SetDesiredHeight(h);`.
    // Modelled here as the flat pair until GroundConstraint gets a home.
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
    // ⭐ NAME SETTLED 2026-08-01: the DWARF's `Construct(bool)` parameter lands on +0x24F,
    // which its member list names mbDoVehicleCollision (h:143, the last of the eight bools).
    // The earlier `lbUseVehicleFrustumCollision` guess conflated it with mbUseFrustrumResolver
    // (+0x24D), which is a DIFFERENT bool Construct always zeroes.
    // (VERIFIED: width, call-site values, and the DWARF name. INFERRED: nothing.)
    void Construct(bool lbDoVehicleCollision);

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

    // ⭐ ResetRadiusSmoothing (DWARF BrnCollisionPolicyAttachedToVehicle.h:112) -- BODIED
    // 2026-08-01. Re-arm the radius smoother by parking the max radius at FLT_MAX, so the
    // next UpdateRadius @0x8220E4D0 SNAPS to the collision-limited radius instead of easing
    // toward it. Three console sites emit exactly `stfs FLT_MAX, +0x240`:
    //   CollisionPolicyAttachedToVehicle::Construct  @0x82224934 (the initial seed)
    //   BehaviourGameplayExternal::Prepare           @0x82240814 (beh +0x290)
    //   ArbStateRaceIntro::Update cases 1 and 3      @0x8226E64C (beh +0x290, the inlined
    //                                                SharedCameraContainer re-arm)
    // (VERIFIED: the offset, the value, and that +0x240 is the radius -- UpdateRadius
    //  @0x8220E4D0 is the only other function in the image that touches it and it is
    //  IDB-named. The METHOD NAME is the DWARF's.)
    void ResetRadiusSmoothing() { mfMaxRadius = FLT_MAX; }

    // ⭐ ResetTrafficCollision (DWARF BrnCollisionPolicyAttachedToVehicle.h:116) -- BODIED
    // 2026-08-01. Raise the one-shot that makes the next GenerateSceneQueries @0x82252798
    // ZERO mfTrafficCollisionResolution (and clear the flag again) instead of ramping it.
    // Same three console sites as ResetRadiusSmoothing (`stb 1, +0x24E`).
    void ResetTrafficCollision() { mbResetVehicleCollision = true; }

    // ⭐ THE FOUR AUTHORED-FLAG SETTERS (DWARF BrnCollisionPolicyAttachedToVehicle.h:94/:97/
    // :100/:103) -- ADDED 2026-08-01 (orbit-camera wave). Each is a single `stb` on the
    // DWARF-named bool this header already carries at its asm-attested offset, and each is
    // INLINED at every console call site (no standalone symbol exists for any of the four).
    // The names are the DWARF's, not invented.
    // FIRST CONSUMER: BehaviourRotateAboutVehicle::Construct @0x8222BF14..0x8222BF54, which
    // re-tunes exactly these four right after CollisionPolicyAttachedToVehicle::Construct
    // returns (`stb 0, 0x298(beh)` / `stb 1, 0x299` / `stb 1, 0x29C` / `stb 1, 0x29D`, i.e.
    // policy +0x248/+0x249/+0x24C/+0x24D with the policy embedded at behaviour +0x50).
    // Without them that behaviour could only have reached these bools by offset, which the
    // x64 rule forbids.
    void SetAutoElevate(bool lbAutoElevate)               { mbAutoElevate         = lbAutoElevate; }
    void SetSmoothRadiusChanges(bool lbSmooth)            { mbSmoothRadiusChanges = lbSmooth; }
    void SetTestAgainstWorldOnly(bool lbWorldOnly)        { mbTestAgainstWorldOnly = lbWorldOnly; }
    void SetUseFrustrumResolver(bool lbUseResolver)       { mbUseFrustrumResolver = lbUseResolver; }

private:
    // FLAG: only the members the bodied functions reach are modelled at their asm-attested
    //   offsets; the rest of the policy rig lands with its full TU.
    //     +0x000 .. +0x20F  policy rig not modelled here (the DWARF puts
    //                       mFrustrumCollisionResolver @+0x010, mCarToCamera @+0x170 and
    //                       mGroundConstraint @+0x1C0 in here -- see GenerateSceneQueries
    //                       @0x82252690, which reaches all three by those displacements)
    //     +0x210            mfDesiredHeight     (stfs f31, 0x210)
    //     +0x214 .. +0x21F  rig members not modelled here
    //     +0x220            mVehicleRef         (16 bytes; DWARF name mAttachedTo)
    //     +0x230 .. +0x23B  mPitchMover (Utils::SmoothMover; DWARF h:126)
    //     +0x23C            mfDesiredNearClip            (DWARF h:128)
    //     +0x240            mfMaxRadius                  (DWARF h:129)
    //     +0x244            mfTrafficCollisionResolution (DWARF h:130)
    //     +0x248 .. +0x24F  the EIGHT bools, DWARF h:136..h:143, in declaration order
    //
    // ⭐ TAIL CARVED 2026-08-01 from references/DecFIGS/dwarfdump/GameSource/Director/Camera/
    // CollisionPolicies/BrnCollisionPolicyAttachedToVehicle.h, which lists the whole member
    // set in order. Three floats then eight bools fill +0x23C..+0x24F EXACTLY -- which is an
    // independent third confirmation of the 0x250 size. Each name is also asm-attested:
    //   +0x23C mfDesiredNearClip   Construct seeds the .data global @0x82CDA560 (0.15) and
    //                              GenerateSceneQueries splats it into
    //                              FrustrumCollisionResolver::GenerateSceneQueries @0x82252824.
    //   +0x240 mfMaxRadius         UpdateRadius @0x8220E4D0 (IDB-named) is its smoother.
    //   +0x244 mfTrafficCollisionResolution  GenerateSceneQueries @0x822527C4 ramps it toward
    //                              1.0 at 0.05/frame while the attached vehicle's speed
    //                              (+0x3CC) is under 35.0, else toward 0.0 at 0.01/frame --
    //                              i.e. literally the DWARF's kfSpeedLimitForTrafficCollision
    //                              / kfTrafficCollisionRampUp / kfTrafficCollisionRampDown.
    //   +0x248 mbAutoElevate       Construct seeds 1 (@0x82224928).
    //   +0x249 mbSmoothRadiusChanges  Construct seeds 0.
    //   +0x24A mbFailOnContact     Construct seeds 0 (DWARF has SetFailOnContact).
    //   +0x24B mbUseGroundConstraint  gates GroundConstraint::GenerateSceneQueries @0x82252750
    //                              (and ::ProcessSceneQueryResults) -- which is why
    //                              SetDesiredHeight raises it. ⚠️ RENAMED from the old
    //                              `mbHaveDesiredHeight` guess.
    //   +0x24C mbTestAgainstWorldOnly  @0x82252774 selects the SceneQueryInterface collision
    //                              mask handed to LineTestNearest: 0x1E when clear, 0x02
    //                              (world only) when set.
    //   +0x24D mbUseFrustrumResolver  @0x82252778 picks the FrustrumCollisionResolver arm over
    //                              the plain LineTestNearest arm.
    //   +0x24E mbResetVehicleCollision  the one-shot ResetTrafficCollision raises.
    //   +0x24F mbDoVehicleCollision  Construct's ARGUMENT. ⚠️ RENAMED from the old
    //                              `mbUseVehicleFrustumCollision` guess -- the DWARF's
    //                              Construct(bool) parameter lands on the LAST bool, and
    //                              GenerateSceneQueries @0x82252814 forwards it to
    //                              FrustrumCollisionResolver::GenerateSceneQueries.
    //
    // ⛔ CORRECTED 2026-08-01 -- THE OLD `maReserved214[0x214 .. 0x24A]` SPAN SWALLOWED A
    // NAMED MEMBER. It covered +0x220, where the policy's own BrnDirector::VehicleRef lives
    // (attested seven independent ways, see SetVehicleRef above) and +0x230, where a
    // Utils::SmoothMover sits (`SmoothMover::Update(this + 0x230, ...)` @0x822406D0). With the
    // span in place SetVehicleRef had no member to write at all -- it could only ever have been
    // a reinterpret_cast into reserved bytes. The VehicleRef is carved out by name; only the
    // SmoothMover (+0x230..+0x23B, whose own +0x234/+0x238 seeds are Construct's) is still a
    // span. The three floats at +0x23C/+0x240/+0x244 are named as of the 2026-08-01 DWARF
    // carve above -- and the SAME defect applied to +0x240: with the span in place,
    // ResetRadiusSmoothing() (and therefore SharedCameraContainer::
    // ForcePrimaryGameplayBehaviourToFinish, whose whole job is that store) had no member to
    // write either.
    //
    // ⭐ SIZE 0x250, GROWN 2026-07-29 (was 0x24C, which was 4 bytes short -- the old tail
    // simply stopped at the last member this header names). Pinned from
    // BehaviourGameplayExternal, which embeds one of these at +0x50 and whose next member
    // (mAirShake) the asm puts at +0x2A0: 0x50 + 0x250 == 0x2A0 exactly. The DWARF tail
    // carved in 2026-08-01 (3 floats + 8 bools filling +0x23C..+0x24F) is the third
    // independent agreement on that size; the IceAnim fork's retired slice was the second.
    // (the leading span starts AFTER the CollisionPolicy base sub-object -- the console
    //  vptr that used to sit inside maReserved000 is the base's; same convention BehaviourRig.h's
    //  VisibilityCollisionPolicy uses. Console displacements in the comments are unchanged.)
    u8  maReserved000[0x210 - sizeof(CollisionPolicy)];  // .. +0x20F  rig members not modelled here
    f32 mfDesiredHeight;                      // +0x210            desired camera height (stored)
    u8  maReserved214[0x220 - 0x214];         // +0x214 .. +0x21F  rig members not modelled here
    BrnDirector::VehicleRef mVehicleRef;      // +0x220            the vehicle the camera hangs off
                                              //                   (DWARF h:124 mAttachedTo)
    u8  maReserved230[0x23C - 0x230];         // +0x230 .. +0x23B  mPitchMover (Utils::SmoothMover)
    f32 mfDesiredNearClip;                    // +0x23C            DWARF h:128
    f32 mfMaxRadius;                          // +0x240            DWARF h:129 (ResetRadiusSmoothing)
    f32 mfTrafficCollisionResolution;         // +0x244            DWARF h:130 (0..1, ramped)
    u8  mbAutoElevate;                        // +0x248            DWARF h:136 (Construct seeds 1)
    u8  mbSmoothRadiusChanges;                // +0x249            DWARF h:137
    u8  mbFailOnContact;                      // +0x24A            DWARF h:138
    u8  mbUseGroundConstraint;                // +0x24B            DWARF h:139 (SetDesiredHeight raises)
    u8  mbTestAgainstWorldOnly;               // +0x24C            DWARF h:140
    u8  mbUseFrustrumResolver;                // +0x24D            DWARF h:141
    u8  mbResetVehicleCollision;              // +0x24E            DWARF h:142 (the one-shot)
    u8  mbDoVehicleCollision;                 // +0x24F            DWARF h:143 (Construct's argument)
};

// ----------------------------------------------------------------------------
// CollisionPolicyAttachedToVehicle::Construct @0x82224890 -- BODIED 2026-08-01, from the asm.
// TAIL COMPLETED 2026-08-01 (second pass): the eight bools + three floats the DWARF names are
// now real members, so the seeds this banner used to list as GATED are reproduced below.
//
// ⚠️ STILL GATED (they land inside reserved spans, and poking them by offset is exactly what
// the x64 rule forbids):
//   * the FrustrumCollisionResolver sub-object zeroing (+0x10/+0x60/+0xB0/+0x100 record heads,
//     a Vector4 at +0x150 and an f32 0.01f at +0x160),
//   * the LineTestNearest post-box head (+0x170) and the GroundConstraint head (+0x1C0),
//   * mPitchMover's two seeds (+0x234 = 0.0f and +0x238 = -89.0f, the min elevation).
// ⚠️ CONSEQUENCE (narrowed): the collision RADIUS is now seeded; the MIN ELEVATION still is
// not, so UpdateMinElevation @0x82240668 will read whatever the memory held until the
// SmoothMover TU lands.
// ⚠️ Construct does NOT write +0x00 -- the vptr is installed by the C++ constructor, not here.
// ⚠️ Construct also does NOT write mfTrafficCollisionResolution (+0x244) or
// mbResetVehicleCollision (+0x24E) -- faithful: the console leaves both to the behaviour's
// Prepare, which calls ResetTrafficCollision().
// DELETE-WHEN: the collision-policy rig TU lands and the two residual spans become members.
// ----------------------------------------------------------------------------
inline void CollisionPolicyAttachedToVehicle::Construct(bool lbDoVehicleCollision)
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

    // 0x8222492C..0x8222493C -- the three tail floats, in the console's store order.
    // ⚠️ +0x23C is loaded from the .data global @0x82CDA560, NOT from an immediate: it is a
    //   tunable default near clip (the DWARF's FrustrumCollisionResolver carries an
    //   `extern VecFloat sDefaultDesiredNearClip` / `extern float32_t kfNearClipDistance`
    //   pair). Its shipped value is 0x3E19999A == 0.15f, read out of the IDB .id1; spelt as
    //   a literal here because the global has no home yet.
    //   FLAG: if that global is ever homed, take the value from it instead.
    mfDesiredNearClip    = 0.15f;                 // 0x8222493C stfs flt_82CDA560, 0x23C
    mfMaxRadius          = FLT_MAX;               // 0x82224934 stfs flt_8200173C, 0x240
    //   (+0x238 = -89.0f and +0x234 = 0.0f are mPitchMover's -- see the GATE above.)

    // 0x8222490C..0x82224928 -- the bool block, in the console's (scrambled) store order.
    mbFailOnContact       = 0;                    // 0x8222490C stb 0, 0x24A
    mbTestAgainstWorldOnly= 0;                    // 0x82224910 stb 0, 0x24C
    mbUseGroundConstraint = 0;                    // 0x82224914 stb 0, 0x24B
    mbUseFrustrumResolver = 0;                    // 0x82224918 stb 0, 0x24D
    mbSmoothRadiusChanges = 0;                    // 0x8222491C stb 0, 0x249
    mbDoVehicleCollision  = lbDoVehicleCollision ? 1u : 0u;   // 0x82224924 stb r4, 0x24F  <- THE ARGUMENT
    mbAutoElevate         = 1;                    // 0x82224928 stb 1, 0x248
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

    // ⭐ SetCanFail (DWARF BrnCollisionPolicy.h:534) -- BODIED 2026-08-01. Was
    // declaration-only. mbCanFail is the master gate on EVERY CollisionPolicy::Fail() call
    // this policy makes: VisibilityCollisionPolicy::ProcessSceneQueryResults @0x82224530
    // re-reads it (`lbz 8(this)`) before each of its six failure arms, so clearing it means
    // "this camera may not fail out for occlusion / collision / off-screen".
    //
    // ⛔ THIS IS THE FUNCTION `BehaviourIceAnim::ClearBaseFirstFrameGate` WAS GUESSING AT.
    // The seven ICE-anim arbitrator states emit `stb 0, 0x28(behaviour)` right after
    // NewBehaviour<BehaviourIceAnim> (e.g. ArbStateCarSelect::Prepare @0x8226F0C4 and
    // @0x8226F1A8, ArbStateRaceIntro::Update @0x8226E730). Behaviour +0x28 is
    // mCollisionPolicy (+0x20) + 0x08 -- i.e. mbCanFail, NOT a base-Behaviour "first frame"
    // field. See the member comments below.
    void SetCanFail(bool lbCanFail) { mbCanFail = lbCanFail; }

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
    //
    // ⚠️⚠️ MIS-ATTRIBUTED MEMBER NAMES (found 2026-08-01, NOT fixed here -- it would rename
    // public accessors BrnBehaviourIceAnim.cpp:352/353/354/559/689 uses, which this wave does
    // not own). These three bytes are NOT policy members: they live inside mVisibilityTest.
    // ProcessSceneQueryResults @0x822246C0 calls VisibilityTest::ProcessSceneQueryResults on
    // `this + 0xF0` and then reads +0xB0/+0xB1/+0xB2 OFF THAT SAME POINTER (@0x82224734..
    // @0x8222474C) -- 0xF0 + 0xB0 == 0x1A0. Cross-checked against VisibilityTest's own
    // committed slice, whose IsOnScreen @0x821F3770 asserts on +0xB0 (mbTestLookingAt) and
    // returns +0xB2 (mbOnScreen). So:
    //     mbSeeThroughEnabled    (+0x1A0) is really mVisibilityTest.mbTestLookingAt
    //     mbSeeThroughSuppressed (+0x1A2) is really mVisibilityTest.mbOnScreen
    //     mbSeeThroughAlways     (+0x1A1) is the unnamed VisibilityTest byte between them
    // The PREDICATE below is still byte-correct (the console computes the identical
    // `+0xB1 || (+0xB0 && !+0xB2)`), and so is the layout -- only the three member NAMES and
    // the three Set* accessors are wrong, and BehaviourIceAnim::Construct's seeding is
    // genuinely a VisibilityTest::Construct inline. Behaviour is unaffected.
    // DELETE-WHEN: VisibilityTest is embedded by name at +0xF0 and the three Set* accessors
    // are re-pointed at it.
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
    // ⭐ CARVED 2026-08-01 out of the head of the old maReservedToVehiclePredictor span.
    // The DWARF (BrnCollisionPolicy.h:436/:437/:438) lists these three bools as the FIRST
    // members after the CollisionPolicy base, and BehaviourIceAnim::Construct @0x82256100
    // seeds exactly three consecutive bytes at policy +0x08/+0x09/+0x0A (1 / 1 / 0) --
    // r11 = this+0x20 there, so @0x8225624C/@0x82256254/@0x82256258. Each is then
    // independently attested by VisibilityCollisionPolicy::ProcessSceneQueryResults
    // @0x82224530:
    //   +0x08 mbCanFail     `lbz 8(this)` guards all six Fail() arms.
    //   +0x09 mbFirstFrame  `lbz 9(this)` picks the first-frame arms, and the function's LAST
    //                       store is `stb 0, 9(this)` -- a latch cleared after the first
    //                       processed frame. (GenerateSceneQueries @0x822402F8 also ORs it
    //                       into the "do the test this time" dice roll.)
    //   +0x0A mbTargetSet   ASSERT-ATTESTED BY NAME: both virtuals open with
    //                       FireAssert("mbTargetSet", BrnCollisionPolicy.cpp, 0x327/0x363).
    bool mbCanFail;                                            // X360 +0x08 (default true)
    bool mbFirstFrame;                                         // X360 +0x09 (default true)
    bool mbTargetSet;                                          // X360 +0x0A (default false)
    u8 maReservedToVehiclePredictor[0x70 - 0x0B];              // X360 [+0x0B, +0x70)
                                                               //   mTargetTransform @+0x10,
                                                               //   mTargetAABB      @+0x50
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
