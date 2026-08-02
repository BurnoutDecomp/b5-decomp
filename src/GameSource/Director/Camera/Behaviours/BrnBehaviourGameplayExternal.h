#ifndef GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_EXTERNAL_H
#define GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_EXTERNAL_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                            // Vector3 / Matrix44Affine
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"           // THE Behaviour base
#include "GameSource/Director/Camera/BrnCollisionPolicy.h"             // CollisionPolicyAttachedToVehicle
#include "GameSource/Director/Camera/Utils/CameraUtils.h"              // Utils::VersionNumber
#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"           // Utils::CameraShake(+Params)
                                                                       //   + CameraShakeICEController
#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h"

// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h
//
// BrnDirector::Camera::BehaviourGameplayExternal -- the default external ("chase") gameplay
// camera behaviour: the third-person camera that trails the player car, with spherical
// look-around, pitch/yaw springs, slide/drift response, jump handling, air + impact shakes,
// its own FOV / boost-FOV and a vehicle-attached collision policy. The other of the TWO
// SHARED gameplay cameras SharedCameraContainer::Prepare @0x82263D50 allocates.
//
// ⭐ RE-BASED (2026-07-29). This class used to be a raw-offset SLICE: a `void* mpVTable` head,
// two reserved byte spans (one of them 0xAEC bytes wide), an invented `mpcCachedName` member
// and a local type-tag enum. It now derives the canonical BrnDirector::Camera::Behaviour and
// carries the DWARF member list by name. See BrnBehaviourGameplayBumper.h's banner for WHY
// (placement-new into a pool slot installs no vtable for a non-polymorphic class, so
// BehaviourHelper::Prepare's slot-0 dispatch faulted on a null vptr).
//
// LAYOUT AUTHORITY: the DECFIGS DWARF (BehaviourGameplayExternal.h:52, members :116..:160;
// Parameters :219, members :222..:274), with these X360 anchors re-derived from the asm:
//
//   Construct @0x82224A18:
//     *(this+8..12)=0, *(this+4)=0, *(this+16)=0     <- the inlined base Construct
//     stvx128 0, this+32                             <- mRotationController.mStickVector @0x020
//     *(this+48/52/56/60)=0.0f, *(this+64/65/66)=0   <- the rest of mRotationController
//     CollisionPolicyAttachedToVehicle::Construct(this+80, 1)   <- mCollisionPolicy   @0x050
//     *(this+672..684)=0.0f                          <- mAirShake                     @0x2A0
//     *(this+688..700)=0.0f                          <- mImpactShake                  @0x2B0
//     stvx128 0, this+2784 ; stvx128 0, this+2800    <- mLastCarPos @0xAE0 / mLastDisplacement
//                                                       @0xAF0
//   Prepare @0x82240738:
//     CameraShakeICEController::Construct(this+704)  <- mBoostShake                   @0x2C0
//     four vec stores at this+2720 (+0/16/32/48)     <- mLastPlayerTransform          @0xAA0
//     *(this+2904)=80.0f                             <- mfJumpFOV                     @0xB58
//     *(this+2909)=1                                 <- mbSnapToCar                   @0xB5D
//     *(this+8)=1 ; return 1                         <- mbIsPrepared
//   SetParameters @0x821F91A8: v3[704]=params (byte +0xB00) / v3[4]=params[1] (byte +0x10)
//
// ⭐ TWO SIZES FALL OUT OF THOSE ANCHORS AND ARE USED AS PINS, not guesses:
//   * Utils::CameraShakeICEController is 0x7E0 -- here 0x2C0..0xA9F (mLastPlayerTransform
//     starts at 0xAA0); independently the same 0x7E0 in BehaviourGameplayBumper (0x030..0x80F).
//   * CollisionPolicyAttachedToVehicle is 0x250, not the 0x24C its own header modelled:
//     0x050 + 0x250 == 0x2A0 == mAirShake. Prepare's two stores at +0x290 (FLT_MAX) and
//     +0x29E (a flag) land inside that extra tail, and both are exactly the two offsets
//     BrnSharedCameraContainer.h's ForcePrimaryGameplayBehaviourToFinish note quotes. Grown
//     in BrnCollisionPolicy.h with that provenance.
//
// x64: parity is BY NAMED MEMBER (pointers widen); the console offsets above are provenance.
// ============================================================================

namespace BrnDirector
{
namespace Camera
{

// RETIRED (2026-07-29): the local `enum EBehaviourTypeGameplayExternal` slice. The tag now
// lives on Behaviour::Parameters::mType; the enumerator is kept with its X360-pinned VALUE (0,
// from the `cmplwi r11, 0` in SetParameters @0x821F91A8) as this behaviour's own tag constant.
// (Sibling tags, each observable only in its own assert: bumper 1, rig 2, helicam 6,
// passenger 7. There is still no single homed EBehaviourType enum.)
enum EBehaviourTypeGameplayExternal
{
    eBehaviourGameplayExternal = 0
};

class BehaviourGameplayExternal : public Behaviour
{
public:

    // ------------------------------------------------------------------------
    // The external-cam parameter block (DWARF BehaviourGameplayExternal.h:219, deriving
    // Behaviour::Parameters). Console span +0x00 .. +0xAC; every 4-byte slot is written by Set.
    //
    // ⭐ EVERY FIELD NAME BELOW IS DOUBLY ATTESTED: the DWARF member order, and this block's
    // own (fully labelled) serialiser -- the TextFileWriteSerialiser instance @0x8224D418
    // walks the fields with the labels quoted per field. The retired slice called these
    // miField08 / mfField0C .. mfFieldA8.
    // ------------------------------------------------------------------------
    class Parameters : public Behaviour::Parameters
    {
    public:
        // X360 visitor: `void Serialise<S>(S&)` -- the VERSIONED field walk (latest = 3).
        // Bodied in BrnBehaviourGameplayExternalParameters.cpp.
        template<class TSerialiser> void Serialise(TSerialiser& lrSerialiser);

        EBehaviourTypeGameplayExternal GetType() const
        {
            return static_cast<EBehaviourTypeGameplayExternal>(mType);
        }

        // The source block this seeds from: a small object whose +0x04 slot (mpfValues) holds
        // a pointer to the f32 source array Parameters::Set copies out of (the asm re-loads
        // lwz r11,4(r4) before each lfs ...(r11)).
        struct Source
        {
            const f32* mpfValues;   // +0x04 of the source object: the f32 source array
        };

        // Seed this block from an attribute-system source object, applying the default
        // external-cam tunables, then assert the two FOV tunables are positive. @0x821F9228.
        void Set(const Source* lpSource);

        // DWARF h:277 -- declaration-only (its own ledger function).
        void Construct();

        // ---- layout (DWARF h:222..:274; console offsets after the 8-byte base) ----------
        Utils::VersionNumber           muVersion;      // :222 +0x08 "Version Number (dont change)"
        Utils::CameraShake::Parameters mAirShakeParams;    // :224 +0x0C "Air Shake Params"
        Utils::CameraShake::Parameters mImpactShakeParams; // :225 +0x1C "Impact Shake Params"

        f32 mrPitchLimit;               // :227 +0x2C "Pitch Limit"                (default 8.0f)
        f32 mrRollLimit;                // :228 +0x30 "Roll Limit"                 (default 8.0f)
        f32 mrPitchCoeff;               // :230 +0x34 "Pitch Coeff"                (default 0.75f)
        f32 mrRollCoeff;                // :231 +0x38 "Roll Coeff"                 (default 0.0f)
        f32 mrPitchSpring;              // :233 +0x3C "Pitch Spring"       <- source[0x2C]
        f32 mrYawSpring;                // :234 +0x40 "Yaw Spring"         <- source[0x08]
        f32 mrAccelerationPitchAmount;  // :236 +0x44 "Acceleration Pitch"         (default -0.5f)
        f32 mrAccelerationSensitivity;  // :237 +0x48 "Acceleration Sensitivity"   (default 0.015f)
        f32 mrPivotY;                   // :239 +0x4C "Pivot Y"/"Pivot Height"      <- source[0x28]
        f32 mrPivotZ;                   // :240 +0x50 "Pivot Z"/"Pivot Length"      <- source[0x24]
        f32 mrPivotZOffset;             // :241 +0x54 "Pivot Z Offset[ Along Car]"  <- source[0x20]
        f32 mrSlideXScale;              // :243 +0x58 "Slide X Scale"      <- source[0x1C]
        f32 mrSlideYScale;              // :244 +0x5C "Slide Y Scale"      <- source[0x18]
        f32 mrSlideZScale;              // :245 +0x60 "Slide Z Scale"              (default 17.0f)
        f32 mrSlideZInputForHalf;       // :246 +0x64 "Slide Z Input for 50%slide" (default 0.25f)
        f32 mrSlideZOutputMax;          // :247 +0x68 "Slide Z Max"        <- source[0x14]
        f32 mrFOV;                      // :249 +0x6C "FOV"                <- source[0x30]  (>0)
        f32 mfInFrontFOVMax;            // :251 +0x70 "Look Front FOV Offset"      (default 60.0f)
        f32 mfFrontInAmount;            // :252 +0x74 "Look Front Towards Factor"  (default 0.0f)
        f32 mfBoostFOV;                 // :254 +0x78 "FOV during boost"   <- source[0x40]  (>0)
        f32 mfSpeedDisplacementHalf;    // :256 +0x7C "Slide Z Speed Half limit"   (default 0.01f)
        f32 mfAccelZLerpAmount;         // :257 +0x80 "Accel Z Lerp Amount"        (default 0.1f)
        f32 mfZLerpAmount;              // :258 +0x84 "Z Lerp Amount"              (default 0.7f)
        f32 mfZDistanceScale;           // :259 +0x88 "Z Distance Scale"   <- source[0x00]
        f32 mfDriftYawSpring;           // :261 +0x8C "[Drift ]Yaw Spring[ in Drift]" <- source[0x34]
        f32 mfBoostFOVZoomCompensation; // :263 +0x90 "FOV Anti-Zoom in Boost"  <- source[0x3C]
        f32 mfDownAngle;                // :265 +0x94 "Down Angle"         <- source[0x38]
        f32 mfVelocitySlideZFactor0To1; // :267 +0x98 "Velocity Slide Factor 0to1" (default 0.0f)
        f32 mfZAndTiltCutoffSpeedMPH;   // :268 +0x9C "Z and Tilt Cutoff Speed MPH" <- source[0x04]
        f32 mfSlideYScaleJump;          // :269 +0xA0 "Slide Y Scale Jump"  <- source[0x0C], then -1
        f32 mfTiltAroundCarScale;       // :270 +0xA4 "Tilt Around Car Scale"  <- source[0x10]
        f32 mfDropFactor;               // :272 +0xA8                              (default 0.5f)
        bool mbIsValid;                 // :274 +0xAC                              (Set stores 1)
    };

    // ---- the virtual interface (DWARF h:52) --------------------------------------------

    // @0x82224A18 -- zero the base, the rotation controller and the two shakes, Construct the
    // vehicle-attached collision policy, and clear the last-frame car tracking.
    virtual void Construct();

    // @0x82240738 -- Construct the boost shake, identity the last player transform, seed the
    // jump FOV, snap to the car, report ready (cannot fail).
    virtual bool Prepare(const BehaviourSharedPrepareReleaseInfo& lrInfo);

    // @0x821F9138 -- the slot-5 override: the vehicle-attached collision policy, but only
    // once the parameter block is bound AND valid.
    //
    // ⭐⭐ DECLARED + BODIED 2026-08-02. It used to be a FLAG ("recovered but NOT declared")
    // resting on TWO reasons, and BOTH have now expired -- the second one was already
    // retired in place on 2026-08-02, the first had been dead since 2026-07-30 and nobody
    // re-read it:
    //   (1) "CollisionPolicyAttachedToVehicle does not (yet) DERIVE the abstract
    //       CollisionPolicy: that base's only definition is a minimal slice inside
    //       Behaviours/BehaviourRig.h, which BrnCollisionPolicy.h cannot include
    //       (BehaviourRig.h includes IT). Returning the member would need a cast through a
    //       base the type does not have."
    //       ⛔ FALSE since the ICE-anim de-fork wave: `class CollisionPolicy` has lived in
    //       BrnCollisionPolicy.h:111 since 2026-07-30 (BehaviourRig.h:73 carries the
    //       "they MOVED there unchanged" note), and BrnCollisionPolicy.h:142 reads
    //       `class CollisionPolicyAttachedToVehicle : public CollisionPolicy`. The type HAS
    //       the base; no cast is needed and none is used below.
    //   (2) "null is what the console itself returns whenever the parameters are unset,
    //       which is the state every allocated gameplay camera is in here ... so today the
    //       two agree."
    //       ⛔ FALSE since the parameter-chain wave (f1f28351): SharedCameraContainer::
    //       Prepare's binds are restored and MainDirector::ProcessNewVehicleEvents raises
    //       mbIsValid, measured, on the first frame the player's car exists. The console
    //       returns &mCollisionPolicy from that moment; this build returned null.
    // ⇒ it was a REAL DIVERGENCE, and the reason given for tolerating it had been wrong for
    //   three days. The chase camera had no collision policy in the scene-query pass at all,
    //   so nothing could ever pull it in out of geometry.
    virtual CollisionPolicy* GetCollisionPolicy();

    // @0x821F91A8 -- the slot-7 override (the DWARF declares it taking the BASE Parameters
    // type, .cpp:135, which is what makes it an override rather than a hiding overload the
    // way the bumper's is).
    virtual void SetParameters(const Behaviour::Parameters* lpParameters);

    // @0x821F9218.
    virtual const char* GetName() const;

    // DWARF h:449 -- force the camera to jump straight to the car next frame instead of
    // easing in. ⭐ This is the +0xB5D byte store the committed
    // SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish note quotes as an
    // unidentified "finished flag": it is mbSnapToCar, and Prepare raises it too.
    void SnapToCar(bool lbSnap) { mbSnapToCar = lbSnap; }

    // ADDED 2026-08-01 (de-inlining aid, NOT a DWARF member). The console reaches this
    // behaviour's own mCollisionPolicy directly from two places that are not members of this
    // class after inlining: BehaviourGameplayExternal::Prepare @0x82240814/@0x82240818 (a
    // member, fine) and ArbStateRaceIntro::Update cases 1 and 3 @0x8226E64C/@0x8226E654 --
    // the inlined SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish, which is NOT
    // a member and so needs a way in. The DWARF's only public route is the virtual
    // GetCollisionPolicy(), which hands back the abstract base (and returns NULL when the
    // parameter block is unset), so it cannot express the two policy resets. Exposed by name
    // here so the container never pokes the policy by offset.
    // FLAG: the ACCESSOR NAME is ours; the member and the two operations it reaches are
    // DWARF-named and asm-attested.
    CollisionPolicyAttachedToVehicle&       GetVehicleCollisionPolicy()       { return mCollisionPolicy; }
    const CollisionPolicyAttachedToVehicle& GetVehicleCollisionPolicy() const { return mCollisionPolicy; }

    // FLAG (not transcribed): the DWARF also declares `virtual bool Update(Camera&, const
    //   BehaviourSharedInfo&)` (.cpp:188, X360 @0x82240828) and `virtual void
    //   SetupTweaker(Tweaker&)` (.cpp:148), plus the eight private helpers Update drives --
    //   with the DWARF's own signatures (BehaviourGameplayExternal.h:300..:337):
    //     InterpolateLastPlayerTransform(Matrix44Affine, VecFloat, VecFloat)  @0x82224BF0 .cpp:575
    //     ApplyJumpEffects(Camera&, const BehaviourSharedInfo&)               @0x822250C0 .cpp:607
    //     ModifyTargetAngles(const Parameters&, Vector3&)                     @0x82225580 .cpp:622
    //     CalcSpringCoeffs(f32, f32, f32, Vector3&, Vector3&)                 @0x8220E5D0 .cpp:644
    //     UpdateLooking(f32&, Vector3&, Vector3&, Vector3&, const AABBox&)    @0x82225630 .cpp:675
    //     CalculateCameraTransform(const Parameters&, Matrix44Affine&, Matrix44Affine,
    //                              Vector3 x6, f32, f32)                      @0x8220E838 .cpp:813
    //     ApplySlideyEffects(const Parameters&, Matrix44Affine&, Matrix44Affine,
    //                        const BehaviourSharedInfo&)                       @0x822260A8 .cpp:841
    //     UpdateJumping(const BehaviourSharedInfo&, f32, Camera&)              @0x8220EAD0 .cpp:948
    //   None is declared here, so slots 2 and 8 keep the base's defaults (Update returns true
    //   and leaves the camera untouched). That is a DOCUMENTED GAP, not a fabrication:
    //   transcribing the chase rig is its own wave (Update alone is 91 source statements
    //   spanning .cpp:188..:561, plus ~360 more across the helpers).
    //
    // ⭐⭐ THE DECODE OF THAT CLUSTER, AS FAR AS IT IS *VERIFIED* (2026-08-02). Recorded here
    //   -- and NOT turned into bodies -- for the same reason f669b437 gave: a body with no
    //   caller is dead code, and Update cannot link until all eight helpers exist. Every line
    //   below is read off the asm of BOTH exports; where the two disagree the X360 wins on
    //   constants/offsets and DecFIGS wins on names (it carries the DWARF's own register and
    //   local names). Anything still unsettled is marked INFERRED.
    //
    //   ⚠️⚠️ THE OPERAND CONVENTION THAT MAKES THE VECTOR MATH READABLE -- get this wrong and
    //   every multiply-add in the cluster inverts. In BOTH exports IDA prints AltiVec
    //   `vmaddfp` in FIELD order `vD, vA, vB, vC` while the SEMANTICS are `vD = vA*vC + vB`
    //   (correspondingly `vnmsubfp d,a,b,c` is `b - a*c`). Settled by three independent
    //   witnesses, not assumed:
    //     * ModifyTargetAngles PS3 0x18EF4 `vmaddfp v0, v0, v1(=0), v11` HAS to be
    //       `rollCoeff * angles`, because the X360 twin @0x82225620 spells the same statement
    //       `vmulfp128 v0, v0, v12`.
    //     * Update PS3 0x699D8 `vmaddfp v12, v8, v27(=0), v8` HAS to be `v8*v8`, because the
    //       next four instructions rotate-and-add its lanes (a squared length).
    //     * the rsqrt Newton-Raphson at PS3 0x69A2C..0x69A38 only reads as the textbook
    //       `x0*(1.5 - 0.5*a*x0^2)` under this convention.
    //   ⚠️ the X360's OWN `vmulfp128` / `vmaddfp128` print in ASSEMBLER order instead -- the
    //   two forms sit side by side inside CalculateCameraTransform.
    //
    //   ---- CalculateCameraTransform @0x8220E838 / PS3 @0x276FC (.cpp:813..:839) ----------
    //   The keystone: this is the function that decides where the chase camera IS. Small --
    //   164 X360 asm lines, of which ~90 are one inlined VMX SinCos.
    //   VERIFIED argument map (X360 passes vector args from v1, PS3 from v2; the two agree
    //   member for member, which is itself the cross-check):
    //     r3  this            -- and the body reads `this->mpParameters->mfDownAngle` (+0x94),
    //                            NOT the rCameraAttribs reference it is handed
    //     r4  rCameraAttribs  -- DEAD in the body (the caller does pass mpParameters)
    //     r5  lCameraMatrix   -- Matrix44Affine&, the OUTPUT
    //     r6  lrCarMatrix     -- Matrix44Affine by value (big vector struct => hidden pointer)
    //     V1  euler angles for the SECOND rotate
    //     V2  multiplied component-wise into BOTH translations (VERIFIED as the multiplier;
    //         "a scale" is INFERRED -- the caller's own stack slot is DWARF-named lCarScale)
    //     V3  the first translation (row3 = V3 * V2)
    //     V4  euler angles for the FIRST rotate
    //     V5  DEAD -- X360 clobbers v5 at 0x8220E8CC before any read; PS3 clobbers ITS v6
    //         (same ordinal) at 0x27708. Two independent builds agree the 5th Vector3 is unused.
    //     V6  the second translation offset (row3 += V6 * V2)
    //     f1  lfSpeedMPH, f2 lfTimestep -- BOTH DEAD in the body (the caller does pass them:
    //         PS3 0x6AC70 `lfs f1, 0x42C(lrSharedInfo)` / 0x6AC78 the timestep)
    //   VERIFIED body shape, statement for statement:
    //     .cpp:817  lfDownAngleRads = mpParameters->mfDownAngle * 0.0174533f  (flt_82001744
    //               == KF_DEGS_TO_RADS) and SinCos of it -- the inlined minimax at
    //               unk_82000BD0..82000C20 with the range-reduction table unk_82000C60 ==
    //               { PI, 2*PI, 1/PI, 1/(2*PI) } (read out of the image, all four words).
    //               The three rows written are an X-axis rotation by that angle:
    //               row0 {1,0,0}, row1 {0,c,s}, row2 {0,-s,c}.
    //               ⚠️ INFERRED: the exact sign lane assignment -- the two `vrlimi128`s pick
    //               lanes out of the sin/cos pair and I have not settled which row takes the
    //               negated one. Everything else here is store-for-store.
    //     .cpp:819  lCameraMatrix.SetTranslation(V3 * V2)          (`vmulfp128 v8, v3, v127`)
    //     .cpp:822  Utils::RotateMatrix44AffineByEulerAnglesZXY(lCameraMatrix, V4)
    //     .cpp:82x  lCameraMatrix.SetTranslation(GetTranslation() + V6 * V2)
    //                                                    (`vmaddfp128 v0, v125, v127, v0`)
    //     .cpp:828  Utils::RotateMatrix44AffineByEulerAnglesZXY(lCameraMatrix, V1)
    //     .cpp:832  lCameraMatrix.SetTranslation(GetTranslation() + lrCarMatrix.GetTranslation())
    //   ⇒ pivot-then-rotate-then-offset-then-rotate-then-place-on-the-car. Both callees are
    //   ALREADY BODIED here (CameraUtils.cpp), so this helper has no unresolved dependency.
    //
    //   ---- ModifyTargetAngles @0x82225580 / PS3 @0x18E24 (.cpp:622..:642) ----------------
    //   VERIFIED complete (43 X360 asm lines, no asserts, no calls):
    //       lrTargetAngles.x = Clamp(x, -PI/mrPitchLimit, +PI/mrPitchLimit);   // .cpp:625
    //       lrTargetAngles.z = Clamp(z, -PI/mrRollLimit,  +PI/mrRollLimit);    // .cpp:626
    //       lrTargetAngles.z *= mrRollCoeff;                                   // .cpp:633
    //   (flt_8200174C == 3.14159; the defaults mrPitchLimit == mrRollLimit == 8.0f make the
    //   band +/-22.5 degrees, and mrRollCoeff's default 0.0f kills roll entirely.)
    //
    //   ---- CalcSpringCoeffs @0x8220E5D0 / PS3 @0x3A37C (.cpp:644..:673) -----------------
    //   VERIFIED complete:
    //       CGS_ASSERT(!IsNaN(lfXSpringCoeff), ...);   // .cpp:646, streams "lfXSpringCoeff: "
    //       CGS_ASSERT(!IsNaN(lfYSpringCoeff), ...);   // .cpp:647, streams " lfYSpringCoeff: "
    //       lfSpeedFactor   = Min(Abs(lfSpeedMPH) * 0.02f, 1.0f);   // saturates at 50 MPH
    //       lrInverseSpring = Vector3((1 - Clamp(lfXSpringCoeff,0,1)) * lfSpeedFactor,
    //                                  1 - Clamp(lfYSpringCoeff,0,1), 0);
    //       lrSpring        = Vector3(1,1,1) - lrInverseSpring;
    //   ⚠️ THE OUTPUTS ARE REVERSED RELATIVE TO THE ARGUMENT NAMES: the 4th parameter
    //   (lrSpring) receives `1 - v` and the 5th (lrInverseSpring) receives `v`. Update's only
    //   call site feeds it (speedMPH, lfTimeStepMod * mfZVelocity, lfTimeStepMod * mfYawDrift).
    //
    //   ---- Update @0x82240828 itself: the statement map that IS settled ------------------
    //     :190  if (!mpParameters) assert("Updating with no parameters")   -- non-gating
    //     :192  if (mpParameters->mbIsValid) {                             -- the whole body
    //     :197  lfTimestep = lrInfo.GetTimestep(GetTimestepType())  -- and the same statement
    //           raises bit 1 of the camera's own state bit array (`ld/ori 2/std` at
    //           lrCamera + 0x140, CgsBitArray.h:213 over BrnCameraState.h:114)
    //     :201  lfTimeStepMod = lfTimestep * 60.0f      (X360 flt_82004C6C, read out of .rdata)
    //     :207  Normalize(lrInfo.mPlayerInfo.<+0x330, i.e. shared-info +0x390> velocity) with
    //           a zero-length guard (`vmsum3fp128 v0,v12,v12` then the rsqrt NR pair)
    //     :226  mRotationController.Update(lfTimestep, lrInfo.mCameraModifier,
    //                                      lrInfo.mbLookback, lrInfo.mbUseControlPauseBehaviour,
    //                                      lfMinPitch, lfMaxPitch)   <- ALREADY BODIED
    //     :235..:240  build a car-velocity frame from lrInfo.GetEyeTarget() (+0x250)
    //     :260/:261/:264  rw::math::vpu::SLerp x2 + OrthoNormalize3x3, rate
    //                     kvfVelocityTransformSlerpSpeed
    //     :287  InterpolateLastPlayerTransform(...)
    //     :301  a FUNCTION-LOCAL STATIC `mLastCameraAngles` (Vector3) with a real guard
    //           variable -- it is per-class, not per-instance, and both gameplay cameras
    //           share it
    //     :307  Utils::EulerAnglesZXYFromMatrix44Affine(...)            <- ALREADY BODIED
    //     :321  ModifyTargetAngles(*mpParameters, lTargetAngles)
    //     :331  CalcSpringCoeffs(speedMPH, lfTimeStepMod*mfZVelocity, lfTimeStepMod*mfYawDrift,
    //                            lSpring, lInverseSpring)
    //     :337  Utils::GetSmallestDifferenceBetweenRadAngles(...)  <- NOT in CameraUtils.h yet
    //     :397  UpdateLooking(lfFOVInOut, lRotation, lPivot, lCarSpaceOffset, lrAABBox)
    //     :409  CalculateCameraTransform(...)  (see the argument map above)
    //     :445  mBoostShake.Update(lrInfo.mpDirectorResourceManager, lfTimestep,
    //                              ln8ShakeType, lfShakeFrequency, <amplitude>)
    //           with the file statics ln8ShakeType == 2, lfShakeFrequency == 1.0f and the
    //           amplitude scaled by 9.0f (X360 flt_82CDAD58/5C/60 read out of .data)
    //     :453  UpdateJumping(lrInfo, lfTimestep, lrCamera)
    //     :469..:474  mfImpactShakeFactor = Max(mfImpactShakeFactor,
    //                     Min(Max(<speed/impact term>, 0), 0.8f));  ApplySlideyEffects(...)
    //     :498  ApplyJumpEffects(lrCamera, lrInfo)
    //     :500  mfPitchCoefficient *= kfShakeDropoffFactor
    //     :505  mImpactShake.Update(matrix, params, *lrInfo.mpRandom,
    //                               kfJumpParamsImpactShakeMaxAmplitude,
    //                               kfJumpParamsImpactShakeMaxFreqMul)
    //     :515  mbLastCarPosInitialised = false ... :520/:522 the FOV publish
    //   ⇒ the remaining unknowns are concentrated in FOUR helpers (UpdateLooking 669 asm
    //     lines, ApplySlideyEffects 434, InterpolateLastPlayerTransform 307, ApplyJumpEffects
    //     303, UpdateJumping 205) plus Update's own .cpp:246..:296 branch tail.
    //   ⛔ ONE MISSING DEPENDENCY, and it is not in this file: `Utils::
    //     GetSmallestDifferenceBetweenRadAngles(Vector3, Vector3)` (.cpp:337) has no
    //     declaration in Camera/Utils/CameraUtils.h -- only the DEGREES scalar sibling
    //     GetSmallestDifferenceBetweenDegsAngles does. It must be added and bodied before
    //     Update can link.
    //
    // ⚠️⚠️ THE OLD CLOSING CLAUSE OF THIS FLAG WAS WRONG, AND IT WAS LOAD-BEARING (retired
    //   2026-08-02). It read: "nothing dispatches slot 2 today anyway (MainDirector::
    //   UpdateCameraBehavioursPostScene @0x8224FD30, the only caller of UpdateAllBehaviours,
    //   is itself still gated)". BOTH clauses are false since the 2026-08-01 PreScene/PostScene
    //   split: `BrnMainDirector.cpp:1153` calls `mBehaviourManager.UpdateAllBehaviours(...)`
    //   from UpdateCameraBehavioursPreScene @0x82255318, un-gated, on every frame
    //   PreSceneQueryUpdate runs -- and PostScene now correctly drives slot 3 instead. SLOT 2
    //   IS DISPATCHED. Anyone reading the old text would conclude this class is inert for a
    //   reason that expired; it is inert for a DIFFERENT reason (below).
    //
    // ⭐⭐ THE PARAMETER CHAIN IS CLOSED AS OF 2026-08-02. Update's ENTIRE body sits inside
    //   `if (mpParameters->mbIsValid)` (PS3 @0x6985C tests *(params + 0xAC) right after the
    //   .cpp:190 "Updating with no parameters" assert). That byte is now TRUE on this build,
    //   and the block is bound. State of every link, MEASURED (CAM_RUN4):
    //     [BRING-UP]  the producer. RaceCarEntityModule::ProcessCreateVehicleEvents
    //                   @0x822FF620 is NOT transcribed and must not be: it drains
    //                   VehicleManagerOutputInterface::mCreateVehicleResultQueue, whose ONLY
    //                   producer in the whole XEX is the physics
    //                   VehicleManager::ProcessCreateEvents @0x82616770 -- absent here. A
    //                   flagged stand-in publishes the player car's leg at the console's own
    //                   frame position: RaceCarEntityModule::
    //                   PublishNewVehicleToDirectorWithoutPhysicsBringUp.
    //     ✅ REAL      BrnDirectorVehicleInputInterface::NewVehicle @0x822CBA90 (its own TU).
    //     ✅ REAL      BridgeEntityModulesToOutput_PostPhysics ->
    //                   UpdateOutputBuffer::SetDirectorVehicleInputInterface @0x827AD1A0.
    //     ✅ REAL      BridgeWorldToDirector @0x823E3AB0 step 6 (the queue merge).
    //     ✅ REAL      MainDirector::ProcessNewVehicleEvents @0x8221A6B0 -- THE only primary
    //                   writer of mbIsValid on this build. Resolves the car's burnoutcarasset
    //                   collection, builds cameraexternalbehaviour from RefSpec(data + 0x1A0)
    //                   / camerabumperbehaviour from +0x1B8, calls both Parameters::Set, and
    //                   latches the key at bank+0x2480.
    //     ✅ REAL      SharedCameraContainer::Prepare @0x82263D50 -- both SetParameters binds
    //                   restored, plus the per-frame re-bind in ArbStateRoaming::Update.
    //     ⚠️ INERT     MainDirector::UpdateAttribSys @0x8221AFD0 is transcribed but is NOT the
    //                   "per-frame re-seed" earlier notes called it: its single gate is
    //                   ControllerInfo +0x01 == mbGameTalkRefreshRequest (DecFIGS DWARF
    //                   BrnDirectorControllerInfo.h:49), the live-tuning tool's pulse. Nothing
    //                   on a PC/retail build sets it.
    //     ✅ REAL      GetCollisionPolicy @0x821F9138 (2026-08-02) -- was a divergence the
    //                   moment mbIsValid went true; the console hands back &mCollisionPolicy
    //                   and this build handed back null, so the chase camera had no policy in
    //                   the scene-query pass at all.
    //     [MISSING]   this Update + the eight helpers above.   <- THE ONLY REMAINING LINK HERE
    //                   ⛔ AND ONE DEPENDENCY OUTSIDE THIS FILE:
    //                   Utils::GetSmallestDifferenceBetweenRadAngles(Vector3, Vector3), which
    //                   Update calls at .cpp:337 and which CameraUtils.h does not declare
    //                   (only the DEGREES scalar sibling exists). Verified by grep over the
    //                   whole tree, not assumed.
    //     [BRING-UP]  BrnGameModule.cpp:1177 gates the director->world camera handover on
    //                   `flyby || meJunkyardState != E_JY_INACTIVE`, which goes FALSE exactly
    //                   when car-select exits -- so the world still falls back to
    //                   sBringUpCamera even once Update lands.
    //   MEASURED END TO END, one boot: "[newveh] SharedCameraContainer::Prepare: both gameplay
    //   cameras bound to the bank (external valid 0, bumper valid 0 at bind time)" ... then
    //   "[newveh] MainDirector::ProcessNewVehicleEvents: seeded ... externalValid 1 FOV
    //   80.000000 boostFOV 95.000000 | bumperValid 1". The bind legitimately precedes the seed
    //   -- it is a POINTER bind into the bank block the seed then fills, which is the console's
    //   own ordering.
    //   ✅ THE DATA WAS NEVER THE PROBLEM: hash64("cameraexternalbehaviour") ==
    //   0xE9EDA3B8C4EA3C84 (low word == the generated header's KI_CAMERAEXTERNALBEHAVIOUR_CLASS)
    //   and that collection is present little-endian at +0x470 of our own ported
    //   build/game/VEHICLES/VEH_PUSMC01_AT.BIN. The car key the chain carries
    //   (0xC61649FE42DCF854, hash64 of the Hunter Cavalry's AttribSysCollectionKey) is present
    //   at +0x398 of the same file. The 80/95 degree FOVs above are that file's authored values.
    //   ⚠️ ORDERING IS LOAD-BEARING: the publish must wait until the car's AT bundle has been
    //   streamed and its vault registered. Publishing at attach time instead (measured,
    //   CAM_RUN1) made every resolve miss, substituted Attrib::DefaultDataArea zeros, and
    //   produced a VALID-but-all-zero block plus fourteen asserts.
    //   DELETE-WHEN: @0x82240828 and its helper cluster are transcribed.

private:
    // ---- layout (DWARF h:116..:160; the anchors are asm-pinned -- see the file banner) ---
    Utils::CameraSphericalRotationController mRotationController;   // :116 +0x020
    CollisionPolicyAttachedToVehicle         mCollisionPolicy;      // :118 +0x050 (0x250)

    Utils::CameraShake                       mAirShake;             // :120 +0x2A0
    Utils::CameraShake                       mImpactShake;          // :121 +0x2B0
    Utils::CameraShakeICEController          mBoostShake;           // :122 +0x2C0 (0x7E0)

    Matrix44Affine                           mLastPlayerTransform;  // :124 +0xAA0
    Vector3                                  mLastCarPos;           // :126 +0xAE0
    Vector3                                  mLastDisplacement;     // :127 +0xAF0

    const Parameters*                        mpParameters;          // :129 +0xB00

    f32  mfCenteringFactor;         // :131 +0xB04
    f32  mfDesiredZDisplacement;    // :132 +0xB08
    f32  mfSmoothedZDisplacement;   // :133 +0xB0C
    f32  mfSlideYScale;             // :134 +0xB10
    f32  mfDriftScale;              // :135 +0xB14
    f32  mOverrideScale;            // :136 +0xB18
    f32  mfDutchVelocity;           // :137 +0xB1C
    f32  mfDutchDrift;              // :138 +0xB20
    f32  mfYawVelocity;             // :139 +0xB24
    f32  mfYawDrift;                // :140 +0xB28
    f32  mfZVelocity;               // :141 +0xB2C
    f32  mfZDrift;                  // :142 +0xB30
    f32  mfPitchCoefficient;        // :143 +0xB34
    f32  mfYawSpring;               // :144 +0xB38
    f32  mfPitchSpring;             // :145 +0xB3C
    f32  mfWobbleScale;             // :146 +0xB40
    f32  mfImpactShakeFactor;       // :147 +0xB44
    f32  mfTimeDelta;               // :148 +0xB48
    f32  mfTimeInJump;              // :149 +0xB4C
    f32  mfDropAmount;              // :150 +0xB50
    f32  mfFOVAdjustment;           // :151 +0xB54
    f32  mfJumpFOV;                 // :152 +0xB58  (Prepare seeds 80.0f)

    bool mbLastCarPosInitialised;   // :154 +0xB5C
    bool mbSnapToCar;               // :155 +0xB5D  (Prepare raises it)
    bool mbEnableDebugRender;       // :157 +0xB5E
    bool mbEnableBoostEffects;      // :158 +0xB5F
    bool mbJumping;                 // :160 +0xB60
};

} // namespace Camera
} // namespace BrnDirector

#endif // GAMESOURCE_DIRECTOR_CAMERA_BEHAVIOURS_BRN_BEHAVIOUR_GAMEPLAY_EXTERNAL_H
