#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                    // Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Core/CgsAssert.h"             // CGS_ASSERT (SetParameters' tripwire)
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"   // ⭐ THE REAL Camera::Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"     // CollisionPolicyAttachedToVehicle (embedded @+0x50)
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h" // mRotationController (+0x20)
#include "GameSource/Director/Camera/Utils/BrnLooker.h"        // Utils::Looker + Looker::Parameters (mLooker @+0x2B0)
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"   // Utils::CameraShake + its Parameters (mShake @+0x2D0)

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h
//
// BrnDirector::Camera::BehaviourRotateAboutVehicle -- THE ORBIT-ABOUT-CAR CAMERA. It is the
// behaviour the junkyard / car-select states drive while the player looks around the car
// they are choosing (ArbStateCarSelect::Prepare, ArbStateOnlineCarSelect::Prepare and
// ArbStateTestbed::Update are its only three console call sites).
//
// ⭐⭐ CORRECTED 2026-08-01 (orbit-camera wave) -- THE CLASS DECLARED ONE VIRTUAL OUT OF EIGHT.
//   The DecFIGS DWARF for this TU lists eight virtual overrides; this header declared only
//   `Construct`. So `Update`, `GetName` AND `GetCollisionPolicy` all silently resolved to
//   `Camera::Behaviour`'s defaults -- `Behaviour::Update` is `return true;` WITHOUT TOUCHING
//   THE CAMERA, so the behaviour allocated, prepared, reported ready and produced a camera
//   every frame: the identity-at-the-origin one `Camera::Construct` left there. That is
//   exactly the failure the sibling `BehaviourInterpolate` shipped with until the wave
//   before this one. An omitted override is invisible to the compiler, the linker and every
//   boot test -- CHECK THE DECLARATIONS, not just the bodies.
//
// ⭐ THE SHAPE OF THE SHOT (Update @0x822493C0, recovered in full):
//   1. advance the free-look stick controller (yaw/pitch, clamped to KF_MIN/MAX_PITCH)
//   2. build a local orbit direction, rotate it by pitch then yaw, then re-express it in the
//      frame `CreateLookAt(mNormalisedOffsetDir, origin)` -- i.e. about the direction
//      BecomeSimilarTo last re-seated
//   3. NORMALISE IT IN THE CAR'S HALF-EXTENT SPACE and scale back by the half-extent, so the
//      eye lands on the car's bounding ELLIPSOID, then push out by KF_STEP_BACK_DISTANCE
//   4. look at the car, fit the FOV to the authored on-screen subject size (twice: once
//      before and once after the screen-offset adjust), clamp to [10, 85] degs
//   5. request motion blur, fold in the camera shake through a SLerp against last frame's
//   6. hold the transform/FOV steady when the frame-to-frame delta is below
//      KF_GENEROUS_EPSILON (an anti-jitter latch), then stash both for next frame
//
// ----------------------------------------------------------------------------
// LAYOUT AUTHORITY: the DecFIGS DWARF member list for this class (h:105..:118), every entry
// of which is independently pinned by the X360 asm of Construct @0x8222BEC0 and/or
// Update @0x822493C0:
//
//   +0x000  Camera::Behaviour base (vptr + the six base fields)
//   +0x020  Utils::CameraSphericalRotationController mRotationController  (0x30)
//   +0x050  CollisionPolicyAttachedToVehicle         mCollisionPolicy     (0x250)
//   +0x2A0  Behaviour::VehicleRef                    mAttachedTo
//   +0x2B0  Utils::Looker                            mLooker              (0x20)
//   +0x2D0  Utils::CameraShake                       mShake               (0x10)
//   +0x2E0  Matrix44Affine                           mLastShakeTransform  (0x40)
//   +0x320  Matrix44Affine                           mLastTransform       (0x40)
//   +0x360  Vector3                                  mNormalisedOffsetDir (0x10)
//   +0x370  f32                                      mfLastFOV
//   +0x374  const Parameters*                        mpParameters
//
// x64 NOTE: the console offsets above are 4-byte-pointer and are PROVENANCE ONLY -- on the
// host the vptr and mpParameters widen. Parity here is BY NAMED MEMBER (the project's x64
// rule); nothing indexes this class by offset.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
// The per-frame vehicle data the re-seat resolve reads. Its real home
// (GameSource/Director/Utils/BrnDirectorAllVehicleData.h) is already pulled in through
// Behaviour.h above, so no forward declaration is repeated here -- the one that used to
// sit here spelled it `class` while the definition spells it `struct` (MSVC C4099).

namespace Camera
{

struct Camera;   // the per-frame camera the behaviour produces (full type: Camera.h)

class BehaviourRotateAboutVehicle : public Behaviour
{
public:

    // ------------------------------------------------------------------------
    // BehaviourRotateAboutVehicle::Parameters (DWARF BrnBehaviourRotateAboutVehicle.h:127..:136)
    //
    // ⭐⭐ RECOVERED IN FULL 2026-08-01. The FLAG that used to sit here said the block was
    //   "AT LEAST 0x80 bytes" with "twenty unnamed floats at console displacements" whose
    //   names were unrecoverable, and transcribed only the type tag. That was wrong: the
    //   DWARF names all three members, and BOTH sub-blocks are already fully named in this
    //   tree, so every field `Parameters::Construct` @0x821FB300 seeds lands on a named
    //   member and nothing is poked by offset.
    //
    //   Behaviour::Parameters head  0x08  (mType @+0x00, mpcDebugName @+0x04)
    //   Looker::Parameters          0x64  @+0x08   -> +0x08 .. +0x6B
    //   CameraShake::Parameters     0x10  @+0x6C   -> +0x6C .. +0x7B
    //   f32 mfShakeBlending0to1           @+0x7C
    //   ------------------------------------------- sizeof == 0x80, as the old FLAG guessed.
    //
    //   INDEPENDENTLY CORROBORATED by Update @0x822493C0's own four parameter reads, none of
    //   which knew about the DWARF: params+0x18/+0x1C are handed to
    //   GetFOVDegsToFitObjectToScreenSize as the target subject size (== mLookerParams
    //   .mfTargetSubjectXSize/YSize), params+0x20/+0x24 to CreateAdjustedLookAt as the screen
    //   offset (== .mfTargetSubjectX/YScreenOffset), params+0x6C to CameraShake::Update as its
    //   parameter block, and params+0x7C as the shake SLerp blend.
    // ------------------------------------------------------------------------
    class Parameters : public Behaviour::Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` for the camera-tunings serialiser S.
        //
        // FLAG: the text-serialise field-walk for this block is ATTESTED EMPTY. The X360
        //   instantiation @0x82214D48 emits only the section-header label line + recursion-depth
        //   accounting; it discards the parameter-block register (mr r5,r4 overwrites the params
        //   ptr before FormatName) and makes NO `bl` to any inner field walker -- the compiler
        //   inlined the inner visitor to nothing because it serialises zero fields to text.
        //   ⚠️ That is a statement about the TEXT WRITER instantiation only. It is NOT evidence
        //   that the block has no fields (it plainly has three); the fields are reached through
        //   the two sub-blocks' own Serialise visitors, which the writer instantiation folded.
        template<class TSerialiser> void Serialise(TSerialiser& /*lrSerialiser*/) {}

        // ⭐ Parameters::Construct @0x821FB300 -- THE WHOLE BODY (2026-08-01).
        // Seed the block, then re-tune the two sub-blocks the way this camera wants them.
        // Every store below is at an asm-attested displacement AND lands on a member the
        // sub-block headers already name -- see the class banner.
        void Construct()
        {
            // `stw 0x12, 0(r8)` / `stw 0, 4(r8)` -- the Behaviour::Parameters head. 18 is
            // eBehaviourRotateAboutVehicle, the tag SetParameters' tripwire compares.
            mType        = 18u;
            SetDebugName(0);

            // ---- CameraShake::Parameters::Construct(this + 0x6C), inlined by the console --
            // The compiler emitted the sub-block's four defaults and this function's three
            // overrides interleaved (0x821FB328..0x821FB370); the overrides win because they
            // are the later store to each address -- +0x6C is written 0.06 then 0.0, +0x74
            // 1.15 then 1.0, +0x78 0.11 then 0.25.
            mShakeParams.Construct();
            mShakeParams.mfXYShakeMagnitudeDegs  = 0.0f;   // stfs 0.0  @0x6C  (default 0.06)
            mShakeParams.mfXYWobbleMagnitudeDegs = 1.0f;   // stfs 1.0  @0x74  (default 1.15)
            mShakeParams.mfWobbleCenteringFactor = 0.25f;  // stfs 0.25 @0x78  (default 0.11)
            //   mfZShakeMagnitudeDegs is left at the sub-block's own 0.0 (`stfs 0.0 @0x70`).

            // ---- Looker::Parameters::Construct(this + 8), a real `bl` @0x821FB374 ---------
            mLookerParams.Construct();

            // ---- then thirteen re-tunes, @0x821FB37C..0x821FB3E0 -------------------------
            // (displacements are off the OUTER block; subtract 8 for the Looker-local offset)
            mLookerParams.mfTargetSubjectXSize             = 1.0f;   // +0x18 -> Looker +0x10
            mLookerParams.mfTargetSubjectYSize             = 1.0f;   // +0x1C -> +0x14
            mLookerParams.mfTargetSubjectXScreenOffset     = 0.0f;   // +0x20 -> +0x18
            mLookerParams.mfTargetSubjectYScreenOffset     = 0.0f;   // +0x24 -> +0x1C
            mLookerParams.mfTrackingTolerance              = 0.1f;   // +0x28 -> +0x20
            mLookerParams.mfTrackingSpeed                  = 0.9f;   // +0x2C -> +0x24
            mLookerParams.mfMinFOVVelocity                 = 60.0f;  // +0x34 -> +0x2C
            mLookerParams.mfMaxFOVVelocity                 = 130.0f; // +0x38 -> +0x30
            mLookerParams.mfToleranceForDistanceFromIdeal  = 10.0f;  // +0x44 -> +0x3C
            mLookerParams.mfToleranceForDistanceFromTarget = 0.1f;   // +0x48 -> +0x40
            mLookerParams.mfOvershootFactor                = 1.05f;  // +0x4C -> +0x44
            mLookerParams.mbUseZoom                        = true;   // +0x67 -> +0x5F (stb 1)
            mLookerParams.meZoomType =
                Utils::Looker::Parameters::E_ZOOM_SCREEN_REGION;     // +0x68 -> +0x60 (stw 2)

            // `stfs 0.0, 0x7C(r8)`
            mfShakeBlending0to1 = 0.0f;
        }

        // ---- layout (DWARF h:130 / :131 / :133) --------------------------------
        Utils::Looker::Parameters      mLookerParams;         // +0x08
        Utils::CameraShake::Parameters mShakeParams;          // +0x6C
        f32                            mfShakeBlending0to1;   // +0x7C
    };

    // ⭐ Construct @0x8222BEC0 -- THE WHOLE BODY (completed 2026-08-01 from the 113-line asm).
    // The base's virtual slot 0, which BehaviourManager::BehaviourHelper::Prepare dispatches
    // on every freshly pooled behaviour.
    void Construct() override;

    // ⭐⭐ Update @0x822493C0 -- vtable slot 2, the whole per-frame job. BODIED 2026-08-01 in
    // this behaviour's .cpp from the full 995-line asm. IT WAS NOT DECLARED AT ALL until
    // then, so `Behaviour::Update`'s `return true;` was this behaviour's real Update and the
    // orbit camera has never once moved on PC. See the class banner.
    bool Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo) override;

    // ⭐ GetCollisionPolicy @0x821FB410 -- vtable slot 5. `addi r3, r3, 0x50 ; blr`.
    // ⚠️ IT IS VIRTUAL (DWARF cpp:272) and used NOT to be declared virtual here, so every
    //   dispatch reached `Behaviour::GetCollisionPolicy`'s `return 0;` and this behaviour
    //   presented itself to the manager as having no collision policy at all.
    //   The covariant return type is this class's own embedded policy (the base returns
    //   `CollisionPolicy*`); that is a legal C++ covariant override and keeps the committed
    //   out-of-line anchor in the .cpp compiling unchanged.
    //   The BODY is verified; the NAME GetCollisionPolicy is inferred (high confidence): the
    //   IDB's own symbol for @0x821FB410 is the truncated
    //   "…BehaviourRotateAboutVehicle::" (50 chars) and @0x821FACE8's is
    //   "…BehaviourIceAnim::GetCollisio" -- the same 50-char truncation, and Construct
    //   @0x8222BEDC proves the member at +0x50 is the CollisionPolicyAttachedToVehicle.
    CollisionPolicyAttachedToVehicle* GetCollisionPolicy() override { return &mCollisionPolicy; }

    // ⭐ GetName @0x821FB488 -- vtable slot 9. Three instructions: return the literal.
    const char* GetName() const override { return "BehaviourRotateAboutVehicle"; }

    // ⛔⛔ THREE DWARF VIRTUALS ARE DELIBERATELY *NOT* DECLARED HERE -- READ BEFORE ADDING THEM.
    //   The DWARF lists `Prepare` (cpp:118), `GetParameters` (cpp:284) and
    //   `SetupTweaker` (cpp:312) as virtual overrides of this class, but NONE of the three
    //   appears anywhere in the X360 ARTIST export -- not under its own symbol and not under
    //   the 50-char truncation that swallowed GetCollisionPolicy's name (there is exactly one
    //   such truncated entry for this class, and Construct proves it is the +0x50 accessor).
    //   Per AGENTS.md's DWARF rule ("DWARF supplies names/types; the X360 ledger decides what
    //   exists"), a DWARF method absent from the X360 ledger is not declared, and a body may
    //   never be invented.
    //   ⚠️ STATE THE COST PLAINLY, because it is the same shape as the defect this wave fixed:
    //   with them undeclared, `Behaviour::Prepare` (`return true;`, and note it does NOT call
    //   SetPrepared), `Behaviour::GetParameters` (`return 0;`) and `Behaviour::SetupTweaker`
    //   run instead. Nothing on the live car-select path is known to read them -- Update
    //   latches its own first-frame state through the base's `mbIsPrepared`, and the other two
    //   are debug/tweaker paths -- but that is an ASSUMPTION, and this campaign has now had it
    //   go stale four times.
    //   DELETE-WHEN: the three functions are located in the X360 image (they are ICF-fold
    //   candidates -- a `return true;` Prepare folds onto any identical sibling), or a
    //   PS3/DecFIGS body is admitted as the authority for them.

    // ⭐ SetParameters @0x821F55B8 -- the TYPED, NON-VIRTUAL overload (DWARF h:148); bodied
    // inline at the bottom of this file.
    //
    // ⚠️ THE PARAMETER WAS ONCE TYPED `const void*` AND IS NOT: the console asserts
    //   `lpParameters->GetType() == eBehaviourRotateAboutVehicle` against tag 18 (0x12) before
    //   the store, so it is a real BehaviourRotateAboutVehicle::Parameters*.
    // ⚠️ AND IT IS *ONE* STORE, NOT TWO. The sibling BehaviourGameplayExternal::SetParameters
    //   @0x821F91A8 does two (mpParameters AND the debug-name word into base +0x10); this one
    //   writes only mpParameters @+0x374. That asymmetry is real, not a transcription gap --
    //   all three call sites (ArbStateTestbed::Update @0x8226B638,
    //   ArbStateCarSelect::Prepare @0x8226EFA0, ArbStateOnlineCarSelect::Prepare @0x82271020)
    //   show the same single store.
    // ⚠️ The DWARF ALSO lists a virtual `SetParameters(const Behaviour::Parameters*)`
    //   (cpp:297). That one is in the not-declared set above: @0x821F55B8 is provably the
    //   typed h:148 overload, because its assert cites BrnBehaviourRotateAboutVehicle.h:150.
    void SetParameters(const Parameters* lpParameters);

    // The camera this behaviour produced this frame. The arbitrator states copy it into their
    // own mCamera while this behaviour is driving them (the X360 reaches it via the manager pool
    // slot the BehaviourHandle resolves to -- sub_821FDF38 reads slot+0x10, the same role the
    // ICE-anim behaviour exposes as GetProducedCamera). DECLARATION-ONLY: the body (and the
    // produced-camera member it returns) land with the manager's handle TU; modelled here BY
    // NAME so consumers never reach the camera by offset.
    const Camera& GetProducedCamera() const;

    // ⭐ BecomeSimilarTo @0x8224A350 -- bodied in this behaviour's own .cpp, from the full
    // 116-line asm. Re-seat this orbit behaviour so it starts from the camera another
    // behaviour is currently producing (the junkyard states keep the look-around-car cam
    // aligned with the ICE movie so the later interpolation onto the car has no
    // discontinuity). Called from BrnArbStateCarSelect::Update @0x8226F5D0 in several arms.
    //
    // ⚠️ ARITY / ORDER RECOVERED FROM THE ASM, and the tree's four call sites are CORRECT:
    //   r3 = this ; r4 -> the source Camera (only read: `lvx128 v0, r30, 0x30` == mTransform
    //   .wAxis, the camera's world position) ; r5 -> the AllVehicleData, forwarded straight
    //   into `VehicleRef::Get(this + 0x2A0, r5)`. Three arguments total, exactly the two the
    //   PC declaration carries. Hex-Rays' own prototype (`int(int,int,int)`) drops nothing
    //   here, but it types every one of them as `int`.
    void BecomeSimilarTo(const Camera& lrSourceCamera, const AllVehicleData& lrAllVehicleData);

private:

    // ⭐⭐ IDENTIFIED 2026-08-01 -- the "+0x020, 0x30-byte sub-object of unknown type" IS
    //   `Utils::CameraSphericalRotationController mRotationController`, i.e. the stick-driven
    //   yaw/pitch free-look controller. The earlier Utils::Looker guess is REFUTED for THIS
    //   member (Looker is 0x20 bytes and its bool latches sit at +0x1C..+0x1F, which the store
    //   set does not fit) -- though the class does embed a real Looker, four members further on.
    //   THREE independent attestations:
    //     1. The ten stores Construct @0x8222BF68 and BecomeSimilarTo @0x8224A4E4 both make map
    //        one-for-one onto the controller's members, INCLUDING the gap:
    //           stvx128 0,+0x20                -> mStickVector            (+0x00, Vector2 16B)
    //           stfs 0, +0x30/+0x34/+0x38/+0x3C-> mfYawDegs / mfYawVelocity /
    //                                             mfYawReturnRate / mfUnRotatedTime (+0x10..+0x1C)
    //           stb  0, +0x40/+0x41/+0x42      -> mbIsLookback / mbWasLookbackLastFrame /
    //                                             mbIsRotated              (+0x20..+0x22)
    //           (+0x44 NOT written)            -> mPitchMover.mfCenteringRate (+0x24) -- the one
    //                                             SmoothMover field a reset legitimately keeps
    //           stfs 0, +0x48/+0x4C            -> mPitchMover.mfCurrentSpeed / .mfCurrentValue
    //     2. The SIBLING behaviour BehaviourGameplayExternal carries the same member at the
    //        SAME offset under the DWARF name `mRotationController`
    //        (BrnBehaviourGameplayExternal.h:217, DWARF :116 +0x020) and its Construct
    //        @0x82224A18 emits the identical store set (already transcribed in that .cpp).
    //     3. sizeof(CameraSphericalRotationController) == 0x30 == 0x50 - 0x20 exactly.
    Utils::CameraSphericalRotationController mRotationController;  // h:105  +0x020 (0x30)

    // Construct @0x8222BEDC: `addi r3, r31, 0x50 ; li r4, 0 ; bl CollisionPolicyAttachedToVehicle
    // ::Construct`, then four post-Construct re-tunes inside the policy (+0x298 = 0, +0x299 = 1,
    // +0x29C = 1, +0x29D = 1) that this class does not name -- see the .cpp.
    CollisionPolicyAttachedToVehicle mCollisionPolicy;             // h:106  +0x050 (0x250)

    // ⚠️ RENAMED 2026-08-01 (was `mVehicleRef`) and RETYPED to the nested
    //   `Behaviour::VehicleRef`, both per the DWARF (h:107). Construct seeds it to the player
    //   car @0x8222BF5C..64; BecomeSimilarTo and Update both resolve through it.
    Behaviour::VehicleRef mAttachedTo;                             // h:107  +0x2A0

    // ⭐ IDENTIFIED 2026-08-01 -- the span +0x2B0..+0x2CF was an unnamed reserve. It is the
    //   authored `Looker`: Construct writes +0x2B0 (0.0f), +0x2C0 (0.2f) and the four latch
    //   bytes +0x2CC..+0x2CF ({1,1,1,0}), which land exactly on the committed Looker layout's
    //   mfSlerpFactor / mfAssessmentTime / {mbFirstFrame, mbAssessingFOV, mbConstructed,
    //   mbForceZoomTargetUpdate}.
    // ⚠️ Update @0x822493C0 NEVER CALLS Looker::Update -- it inlines its own zoom (the two
    //   GetSizeOnScreen / GetFOVDegsToFitObjectToScreenSize passes) instead. The member is
    //   constructed and then not used on the live path; that is the console's own shape, not
    //   a reconstruction gap.
    Utils::Looker mLooker;                                         // h:108  +0x2B0 (0x20)

    // ⭐ IDENTIFIED 2026-08-01 -- pinned by Update @0x82249EF0, which passes `this + 0x2D0`
    //   as the `this` of Utils::CameraShake::Update (whose parameter block argument is
    //   `mpParameters + 0x6C` == Parameters::mShakeParams).
    // ⚠️ Construct does NOT zero it -- the pool's placement-new leaves it zeroed, which is
    //   what CameraShake::Construct would have written anyway. No store to +0x2D0..+0x2DF
    //   appears in Construct's asm.
    Utils::CameraShake mShake;                                     // h:109  +0x2D0 (0x10)

    // ⭐ IDENTIFIED 2026-08-01 -- Construct @0x8222C050..0x8222C070 writes four rows of an
    //   identity affine to +0x2E0, and Update @0x82249F24 writes the shake SLerp result to the
    //   same four rows. (The DWARF pair order puts mLastShakeTransform first.)
    Matrix44Affine mLastShakeTransform;                            // h:111  +0x2E0 (0x40)

    // ⭐ IDENTIFIED 2026-08-01 -- Update copies the produced camera transform into +0x320 on
    //   the first frame and again at the end of every frame, and compares against it for the
    //   KF_GENEROUS_EPSILON anti-jitter latch. NOT written by Construct.
    Matrix44Affine mLastTransform;                                 // h:112  +0x320 (0x40)

    // ⚠️ RENAMED 2026-08-01 (was `mOrbitDirection`) per the DWARF (h:114). Same member, same
    //   offset, same 16 bytes: Construct seeds it from XMMatrixRotationY(-pi/2 * 0.25f)'s "at"
    //   row, BecomeSimilarTo re-seats it, and Update @0x822495E0 hands it to Utils::CreateLookAt
    //   as a Vector3 eye against a zero target. The w lane is Vector3's don't-care 4th lane in
    //   all three.
    // ⚠️ The DWARF NAME IS A MISNOMER on the re-seat path -- see the SHIPPED-CONSOLE QUIRK
    //   banner in the .cpp: BecomeSimilarTo validates the normalised direction and then stores
    //   the UN-normalised one, so this member is only actually normalised on the two authored
    //   seeds. It is kept because it is the console's own name, and it is benign because every
    //   reader normalises again.
    Vector3 mNormalisedOffsetDir;                                  // h:114  +0x360

    // ⭐ IDENTIFIED 2026-08-01 -- the span +0x370..+0x373 was an unnamed reserve. Update
    //   @0x8224A1C8/@0x8224A310 stores the produced FOV here and @0x8224A1CC reads it back for
    //   the KF_GENEROUS_EPSILON FOV latch.
    f32 mfLastFOV;                                                 // h:116  +0x370

    const Parameters* mpParameters;                                // h:118  +0x374
};

// ----------------------------------------------------------------------------
// BehaviourRotateAboutVehicle::SetParameters @0x821F55B8 -- the whole function.
//   0x821F55D4  lwz    r11, 0(r31)        ; lpParameters->GetType()
//   0x821F55D8  cmplwi cr6, r11, 0x12     ; == 18 == eBehaviourRotateAboutVehicle
//   0x821F55E8  assert "lpParameters->GetType() == eBehaviourRotateAboutVehicle"
//               file ..\..\..\GameSource\Director/Camera/Behaviours/
//                    BrnBehaviourRotateAboutVehicle.h, line 150 (the HEADER, unlike the
//               GameplayExternal sibling whose tripwire quotes a .cpp)
//   0x821F5600  stw    r31, 0x374(r30)    ; mpParameters = lpParameters
// The tag 18 matches Parameters::Construct @0x821FB330's own `stw 0x12, 0(r8)`.
// ----------------------------------------------------------------------------
inline void BehaviourRotateAboutVehicle::SetParameters(const Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == 18u,
               "lpParameters->GetType() == eBehaviourRotateAboutVehicle");   // .h:150

    mpParameters = lpParameters;
}

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_ROTATE_ABOUT_VEHICLE_H
