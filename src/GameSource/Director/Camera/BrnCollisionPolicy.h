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
#include "GameSource/Director/Utils/BrnDirectorPostOfficeTypes.h"          // the post boxes the policies wait on
#include "GameSource/Director/Utils/BrnDirectorTimestep.h"                 // BrnDirector::Timestep (CollisionPolicySharedInfo)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                 // CgsContainers::BitArray<8> (mUsedRaceCars)
#include "GameSource/BurnoutConstants.h"                                   // EActiveRaceCarIndex

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

namespace CgsNumeric { class Random; }

namespace BrnDirector
{
// Pointer-only members of CollisionPolicySharedInfo. The class keys are the homes' (MSVC mangles
// the key: BrnSceneQueryInterface.h / BrnDirectorAllVehicleData.h / BrnDirectorModuleDebugPrinter.h
// all say `struct`).
struct SceneQueryInterface;
struct AllVehicleData;
struct DebugPrinter;

namespace Camera
{

// The two by-reference/by-pointer arguments the policy interface takes. Pointer/reference-only
// here, so forward declarations are correct (Camera.h and Behaviours/Behaviour.h are the homes;
// including either would create a cycle -- Behaviour.h's behaviours embed policies).
struct Camera;
struct BehaviourSharedInfo;
struct VehicleInfo;

// DWARF Camera.h:30 -- `extern bool IsLookingAtTarget(const Camera&, Matrix44Affine, const AABBox&)`
// @0x822331F0. The body lives in Behaviours/BrnBehaviourIceAnim.cpp (its first consumer), which
// declares it with this same signature; declared here too for VisibilityTest::GenerateSceneQueries.
bool IsLookingAtTarget(const Camera& lrCamera,
                       const rw::math::vpu::Matrix44Affine& lrTargetTransform,
                       const AABBox& lrTargetBounds);

// ============================================================================
// BrnDirector::Camera::CollisionPolicySharedInfo (DWARF BrnCollisionPolicy.h:54) -- the per-frame
// block every collision policy's GenerateSceneQueries / ProcessSceneQueryResults receives. Added
// 2026-09-25 (FX-DIRECTOR2). The director builds it on its own stack, twice a frame:
//   MainDirector::UpdateCameraBehavioursPreScene  0x822557B4..0x82255834 (then GenerateSceneQueries)
//   MainDirector::UpdateCameraBehavioursPostScene 0x8224FE18..0x8224FF30 (then ProcessSceneQueryResults)
// Console offsets, pinned by those two builds (provenance only; access is by name):
//   +0x00 mUsedRaceCars          `std` of the input's used-race-car word
//   +0x08 mpRequestInterface     DirectorInputOutput::mpSceneQueryInterface (lpIO + 0x10)
//   +0x0C mpRaceCars             DirectorIO::InputBuffer::GetRaceCarInfo
//   +0x10 mpPlayerCar            mpRaceCars + 0x4F0 * player index
//   +0x14 mpPlayerCarTransform   mpPlayerCar + 0x1F0 (mRaceCarState.mTransform)
//   +0x18 mePlayerCarIndex
//   +0x1C mpAllVehicleData       MainDirector + 0x12C80
//   +0x20 mpDebugPrinter         MainDirector + 0x337B0
//   +0x24 mpRandom               MainDirector + 0x32EE0 (the director's camera Random)
//   +0x30 mTimestep              the same three timesteps the BehaviourSharedInfo carries
// The policies read mTimestep.Get(E_WORLD) (+0x60) for the ground constraint and
// Get(E_WORLD_NO_SLOMO) (+0x64) for the visibility and prediction timers.
// ============================================================================
struct CollisionPolicySharedInfo
{
    CgsContainers::BitArray<8u>            mUsedRaceCars;          // :57
    const SceneQueryInterface*             mpRequestInterface;     // :59
    const VehicleInfo*                     mpRaceCars;             // :60
    const VehicleInfo*                     mpPlayerCar;            // :61
    const rw::math::vpu::Matrix44Affine*   mpPlayerCarTransform;   // :62
    EActiveRaceCarIndex                    mePlayerCarIndex;       // :63
    const AllVehicleData*                  mpAllVehicleData;       // :65
    BrnDirector::DebugPrinter*             mpDebugPrinter;         // :67
    CgsNumeric::Random*                    mpRandom;               // :69
    BrnDirector::Timestep                  mTimestep;              // :70
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::GeometryCollisionPredictor (DWARF BrnCollisionPolicy.h:133)
//
// Predicts whether the camera, moving at its current velocity, is about to hit the WORLD within
// KF_LOOKAHEAD_TIME, and if so how soon: one world-only nearest line test from the camera to
// camera + velocity * lookahead, answered through mLineTest.
// ⭐ FULL LAYOUT 2026-09-25 (FX-DIRECTOR2): the old `maReserved00[0x60]` span is the DWARF's
// mVelocity (+0x00) and mLineTest (+0x10, an 80-byte nearest-result post box) -- which is what
// puts the two scalars at +0x60 / +0x64.
// ----------------------------------------------------------------------------
class GeometryCollisionPredictor
{
public:
    // DWARF :185. Inlined in every VisibilityCollisionPolicy::Construct copy (e.g.
    // BehaviourGyroCam::Construct 0x82244B90..0x82244B9C): the velocity lane zeroed (`stvx`), the
    // post box emptied (`stw 0, 0x10`), mbWillCollide cleared (`stb 0, 0x64`).
    void Construct()
    {
        mVelocity.SetZero();
        mLineTest.Construct();
        mbWillCollide = false;
    }

    // DWARF :192. No out-of-line X360 copy: VisibilityCollisionPolicy::GenerateSceneQueries
    // inlines it (0x82240468..0x822404AC). The timestep and the Random are part of the DWARF
    // signature and unused by the body. Body: BrnGeometryCollisionPredictor.cpp.
    void GenerateSceneQueries(const Camera& lrCamera, f32 lfTimestep, CgsNumeric::Random& lrRandom,
                              const SceneQueryInterface* lpRequestInterface);

    // DWARF :196 -- @0x8220E1B8. Body: BrnGeometryCollisionPredictor.cpp.
    void ProcessSceneQueryResults(f32 lfTimestep);

    // DWARF :200 -- inlined (VisibilityCollisionPolicy::GenerateSceneQueries 0x8224047C/0x82240484
    // copies the policy's velocity lane into +0x00 before the test).
    void SetVelocity(Vector3 lVelocity) { mVelocity = lVelocity; }

    // DWARF :203.
    bool WillCollide() const { return mbWillCollide; }

    // DWARF :206 -- @0x821F36C0: asserts a collision was predicted, returns the time.
    f32 GetTimeUntilCollision() const;

    // KF_LOOKAHEAD_TIME_FLOAT (DWARF :208). Its VecFloat twin KF_LOOKAHEAD_TIME_VECFLOAT (:209)
    // lives at 0x82FAA9A0 and is written by the dyn-init at 0x82C49170 as the splat of the float
    // at 0x82001B6C == 0x3F800000 == 1.0f.
    static const f32 KF_LOOKAHEAD_TIME_FLOAT;

private:
    Vector3                mVelocity;               // :213  +0x00
    LineTestNearestPostBox mLineTest;               // :214  +0x10 (80 bytes: state + pad + the 64-byte result)
    f32                    mfTimeUntilCollision;    // :215  +0x60
    bool                   mbWillCollide;           // :216  +0x64
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

    // DWARF :333 / :338 -- the two per-frame virtuals, in the console's slot order. The base
    // bodies do nothing: a policy that does not override them (CollisionPolicyAttachedToVehicle and
    // FrustrumCollisionResolver on this build) issues and consumes no queries.
    // ⭐ SIGNATURES CORRECTED 2026-09-25 (FX-DIRECTOR2): `(const void*, Camera&)` was a stand-in for
    // the DWARF's `(const CollisionPolicySharedInfo&, Camera&)`.
    virtual void GenerateSceneQueries(const CollisionPolicySharedInfo& lrSharedInfo, Camera& lrCamera)
    {
        (void)lrSharedInfo;
        (void)lrCamera;
    }
    virtual void ProcessSceneQueryResults(const CollisionPolicySharedInfo& lrSharedInfo, Camera& lrCamera)
    {
        (void)lrSharedInfo;
        (void)lrCamera;
    }

    // DWARF :504. BehaviourManager::ProcessSceneQueryResults @0x8221F70C reads it (`lbz 4(policy)`)
    // straight after the policy's ProcessSceneQueryResults and fails the behaviour when it is set.
    bool HasFailed() const { return mbFailed; }

    // DWARF :523 -- @0x82206450: record the failure reason in the CAMERA's validity account
    // (camera +0x138), drop the camera's follow request (the +0x140 flag word's bit 1) and raise
    // mbHasFailed. ⭐ SIGNATURE CORRECTED 2026-09-25: the first argument is the Camera (both
    // callers pass the camera they were handed), not a BehaviourSharedInfo*. The body lives in
    // BrnVisibilityCollisionPolicy.cpp (see the note there on why not its DWARF home).
    void Fail(Camera& lrCamera, s32 leFailedFlag);

protected:
    // The counterpart store: every derived policy's Construct opens with `stb 0, 4(this)`
    // (e.g. CollisionPolicyAttachedToVehicle::Construct @0x822248D0). Named so the derived
    // bodies clear the base's own latch instead of reaching a private member.
    void ClearFailed() { mbFailed = false; }

private:
    // DWARF :371 mbHasFailed. Fail @0x82206450 `stb 1,4(this)`.
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
// BrnDirector::Camera::VisibilityTest (DWARF BrnCollisionPolicy.h:175)
//
// Can the camera SEE its target? Two nearest line tests between the camera and the target
// (camera->target and target->camera, both excluding the target entity and all its parts) decide
// OCCLUDED; the target's 0.75-scaled bounds against the camera frustum, plus a zoom-scaled distance
// cap, decide ON SCREEN. The two timers count how long each bad state has lasted.
// ⭐ FULL LAYOUT 2026-09-25 (FX-DIRECTOR2): the DWARF members at the console offsets both committed
// accessors already read (+0xA4 / +0xB0 / +0xB2), and the whole method set.
// ----------------------------------------------------------------------------
class VisibilityTest
{
public:
    // DWARF :226. Inlined in every VisibilityCollisionPolicy::Construct copy (policy +0xF0..+0x1A2,
    // e.g. BehaviourGyroCam::Construct 0x82244BA4..0x82244BC4): both boxes emptied, the three
    // timers zeroed, mfMaxTimeBetweenTests = 0.5 (flt_82001DA0), mbTestLookingAt = true,
    // mbOccluded = false, mbIsOnScreen = true.
    void Construct()
    {
        mLineTestA.Construct();
        mLineTestB.Construct();
        mfOccludedTime        = 0.0f;
        mfOffscreenTime       = 0.0f;
        mfTimeSinceLastTest   = 0.0f;
        mfMaxTimeBetweenTests = 0.5f;     // flt_82001DA0 == 0x3F000000
        mbTestLookingAt       = true;
        mbOccluded            = false;
        mbIsOnScreen          = true;
    }

    // DWARF :237 -- @0x822400B0. Body: BrnVisibilityTest.cpp.
    void GenerateSceneQueries(const Camera& lrCamera, f32 lfTimestep, CgsNumeric::Random& lrRandom,
                              const SceneQueryInterface* lpRequestInterface,
                              const rw::math::vpu::Matrix44Affine& lrTargetTransform,
                              const AABBox& lrTargetAABB, bool lbDoTest,
                              CgsSceneManager::EntityId lTargetEntityId);

    // DWARF :241 -- @0x8220E290. Body: BrnVisibilityTest.cpp.
    void ProcessSceneQueryResults(f32 lfTimestep);

    f32  GetOccludedTime() const        { return mfOccludedTime; }          // :244
    f32  GetOffscreenTime() const;                                          // :247  @0x821F3718
    f32  GetOffscreenTimeUnsafe() const { return mfOffscreenTime; }         // :251
    void SetTestLookingAt(bool lbTestLookingAt) { mbTestLookingAt = lbTestLookingAt; }   // :256
    bool WillTestLookingAt() const      { return mbTestLookingAt; }         // :259
    // :262 -- `+0xB1 || (+0xB0 && !+0xB2)`, inlined by every consumer (BehaviourFixedCam::Update
    // 0x8222A338..0x8222A368, BehaviourIceAnim::Update, VisibilityCollisionPolicy::
    // ProcessSceneQueryResults 0x82224734..0x8222474C).
    bool IsVisibilityInterrupted() const { return mbOccluded || (mbTestLookingAt && !mbIsOnScreen); }
    bool IsOccluded() const             { return mbOccluded; }              // :266
    bool IsOnScreen() const;                                                // :269  @0x821F3770

private:
    LineTestNearestPostBox mLineTestA;              // :277  +0x00 (camera -> target)
    LineTestNearestPostBox mLineTestB;              // :278  +0x50 (target -> camera)
    f32                    mfOccludedTime;          // :279  +0xA0
    f32                    mfOffscreenTime;         // :280  +0xA4
    f32                    mfTimeSinceLastTest;     // :281  +0xA8
    f32                    mfMaxTimeBetweenTests;   // :282  +0xAC
    bool                   mbTestLookingAt;         // :283  +0xB0
    bool                   mbOccluded;              // :284  +0xB1
    bool                   mbIsOnScreen;            // :285  +0xB2
};

// ----------------------------------------------------------------------------
// BrnDirector::Camera::GroundConstraint (DWARF BrnCollisionPolicy.h:241)
//
// Keeps the camera at mfDesiredHeight above the ground: one world-only nearest line test straight
// down through the camera; when it hits, the camera's height is set to hit + desired height.
// Embedded by both VisibilityCollisionPolicy (+0x1C0) and CollisionPolicyAttachedToVehicle
// (+0x1C0 -- still a reserved span there; its ResolveCollisions @0x82224948 is not in this closure).
// ----------------------------------------------------------------------------
class GroundConstraint
{
public:
    // DWARF :298. Inlined (policy +0x1C0 `stw 0` and +0x210 `stfs -1.0f`, flt_820037C8).
    void Construct()
    {
        mLineTest.Construct();
        mfDesiredHeight = -1.0f;          // flt_820037C8 == 0xBF800000
    }

    // DWARF :304 -- @0x82240200. Body: BrnVisibilityCollisionPolicy.cpp.
    void GenerateSceneQueries(const Camera& lrCamera, f32 lfTimestep,
                              const SceneQueryInterface* lpRequestInterface);

    // DWARF :309 -- @0x8220E3A0; false when the ground was not found. Body:
    // BrnVisibilityCollisionPolicy.cpp.
    bool ProcessSceneQueryResults(f32 lfTimestep, Camera& lrCamera);

    void SetDesiredHeight(f32 lfDesiredHeight) { mfDesiredHeight = lfDesiredHeight; }   // :313
    f32  GetDesiredHeight() const              { return mfDesiredHeight; }             // :316

    // DWARF :294 / :295 -- the line runs from KF_MIN_TEST_ABOVE_LENGTH above the camera down to
    // max(KF_MIN_TEST_BELOW_LENGTH, mfDesiredHeight) below that start (the two float literals the
    // body loads: flt_82001C98 == 1.0f and flt_8200426C == 5.0f).
    static const f32 KF_MIN_TEST_BELOW_LENGTH;
    static const f32 KF_MIN_TEST_ABOVE_LENGTH;

private:
    LineTestNearestPostBox mLineTest;               // :320  +0x00
    f32                    mfDesiredHeight;         // :321  +0x50
};

// ============================================================================
// BrnDirector::Camera::VisibilityCollisionPolicy (DWARF BrnCollisionPolicy.h:377) -- the "free"
// (not car-attached) camera collision policy: it runs the visibility test, the ground constraint and
// the two collision predictors, and fails the camera (CollisionPolicy::Fail) when the target is
// occluded or off screen, the ground is missing, or a collision is imminent.
//
// ⭐ DWARF LAYOUT 2026-09-25 (FX-DIRECTOR2, the camera scene-query closure). Every member is now the
// DWARF's, in the DWARF's order. Two corrections fall out:
//   * the three "see-through" bytes at policy +0x1A0..+0x1A2 were never policy members -- they are
//     mVisibilityTest's mbTestLookingAt / mbOccluded / mbIsOnScreen (mVisibilityTest sits at +0xF0;
//     ProcessSceneQueryResults @0x822246C0 calls VisibilityTest::ProcessSceneQueryResults on
//     this + 0xF0 and then reads +0xB0/+0xB1/+0xB2 off that pointer). The SetSeeThrough* accessors
//     are retired; their callers use Construct() / SetTestLookingAt / IsVisibilityInterrupted;
//   * the scalars the old slice named mfDesiredHeight / mfMinHeight / mfCollisionRadius /
//     mbHaveDesiredHeight are mGroundConstraint's mfDesiredHeight (+0x210) and the policy's
//     mfOcclusionTimeout (+0x234) / mfOffscreenTimeout (+0x238) / mbUseGroundConstraint (+0x23C) --
//     the offsets and the Construct seeds (1.5 / 0.5) are unchanged.
// Nominal console size 0x240.
// ============================================================================
class VisibilityCollisionPolicy : public CollisionPolicy
{
public:
    // DWARF BrnCollisionPolicy.cpp:763. No out-of-line X360 copy; inlined in every owner's
    // Construct (BehaviourGyroCam 0x82244B88..0x82244BEC, BehaviourIceAnim 0x822561F0..
    // 0x8225625C, BehaviourFixedCam, BehaviourBystanderCam ...). Body: BrnVisibilityCollisionPolicy.cpp.
    void Construct();

    // DWARF BrnCollisionPolicy.cpp:805 / :865 -- @0x822402F8 / @0x82224530. Bodies:
    // BrnVisibilityCollisionPolicy.cpp.
    void GenerateSceneQueries(const CollisionPolicySharedInfo& lrSharedInfo, Camera& lrCamera) override;
    void ProcessSceneQueryResults(const CollisionPolicySharedInfo& lrSharedInfo, Camera& lrCamera) override;

    // DWARF :534. mbCanFail is the master gate on every Fail() the policy makes. The seven ICE-anim
    // arbitrator states emit `stb 0, 0x28(behaviour)` right after NewBehaviour<BehaviourIceAnim>
    // (behaviour +0x20 + 0x08), i.e. SetCanFail(false).
    void SetCanFail(bool lbCanFail) { mbCanFail = lbCanFail; }

    // DWARF :545 -- inlined (BehaviourIceAnim::Update 0x82247204..0x82247258 re-targets the policy
    // at the player every frame: mbTargetSet, the transform, the bounds, the entity id).
    void SetTarget(Matrix44Affine lTargetTransform, AABBox lTargetAABB,
                   CgsSceneManager::EntityId lTargetEntityId);

    // DWARF :404. The owners' "can't cut to me" gate (BehaviourFixedCam / BehaviourGyroCam /
    // BehaviourIceAnim / BehaviourBystanderCam).
    bool IsVisibilityInterrupted() const { return mVisibilityTest.IsVisibilityInterrupted(); }

    // DWARF :556 -- inlined into ProcessSceneQueryResults (0x822247D4..0x82224804): the time left
    // before either visibility timeout, floored at 0 -- the two `fsel`s are exactly min then max-0.
    f32 GetMinTimeToVisibilityFailure() const
    {
        const f32 lfOcclusionLeft = mfOcclusionTimeout - mVisibilityTest.GetOccludedTime();
        const f32 lfOffscreenLeft = mfOffscreenTimeout - mVisibilityTest.GetOffscreenTimeUnsafe();
        const f32 lfMin = ((lfOcclusionLeft - lfOffscreenLeft) >= 0.0f) ? lfOffscreenLeft : lfOcclusionLeft;
        return (-lfMin >= 0.0f) ? 0.0f : lfMin;
    }

    // DWARF :570 -- a forwarder onto the embedded visibility test (BehaviourFixedCam::Construct
    // @0x82229D20 re-stores `stb 1` to policy +0x1A0 after the inlined Construct).
    void SetTestLookingAt(bool lbTestLookingAt) { mVisibilityTest.SetTestLookingAt(lbTestLookingAt); }

    // DWARF :579 -- @0x821F38E0. Body: BrnVisibilityCollisionPolicy.cpp.
    void SetDesiredHeight(f32 lfDesiredHeight);

    // DWARF :419.
    void SetVelocity(Vector3 lVelocity);

    // DWARF :422 / :425 / :428 / :431.
    bool WillCollideWithGeometry() const { return mGeometryCollisionPredictor.WillCollide(); }
    f32  TimeUntilCollisionWithGeometry() const;     // @0x821F37C8
    bool WillCollideWithVehicle() const  { return mVehicleCollisionPredictor.HasPredictedCollision(); }
    f32  TimeUntilCollisionWithVehicle() const;      // @0x821F3858

private:
    bool                             mbCanFail;                           // :436  +0x08
    bool                             mbFirstFrame;                        // :437  +0x09
    bool                             mbTargetSet;                         // :438  +0x0A
    Matrix44Affine                   mTargetTransform;                    // :440  +0x10
    AABBox                           mTargetAABB;                         // :441  +0x50
    Utils::VehicleCollisionPredictor mVehicleCollisionPredictor;          // :443  +0x70
    GeometryCollisionPredictor       mGeometryCollisionPredictor;         // :444  +0x80
    VisibilityTest                   mVisibilityTest;                     // :445  +0xF0
    VolumeTestDeepestPostBox         mVolumeTest;                         // :451  +0x1B0
    GroundConstraint                 mGroundConstraint;                   // :453  +0x1C0
    Vector3                          mVelocity;                           // :455  +0x220
    CgsSceneManager::EntityId        mTargetEntityId;                     // :456  +0x230
    f32                              mfOcclusionTimeout;                  // :458  +0x234
    f32                              mfOffscreenTimeout;                  // :459  +0x238
    bool                             mbUseGroundConstraint;               // :461  +0x23C
    bool                             mbDoingVisibilityTestThisTime;       // :462  +0x23D
    bool                             mbDoingCollisionPredictionThisTime;  // :463  +0x23E
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BRN_COLLISION_POLICY_H
