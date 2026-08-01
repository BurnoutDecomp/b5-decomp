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

    // FLAG (recovered but NOT declared): `virtual CollisionPolicy* GetCollisionPolicy()`
    //   @0x821F9138 is fully decoded --
    //       CGS_ASSERT(mpParameters, "calling GetCollisionPolicy() with no parameters");
    //                                                  // .cpp:120, tests *(this+2816)
    //       if (!mpParameters->mbIsValid) return 0;     // *(params + 172) == +0xAC
    //       return &mCollisionPolicy;                   // this + 80
    //   -- and both operands land exactly on the named members above, which is a nice
    //   independent confirmation of the layout. It is NOT declared here because
    //   CollisionPolicyAttachedToVehicle does not (yet) DERIVE the abstract CollisionPolicy:
    //   that base's only definition is a minimal slice inside Behaviours/BehaviourRig.h,
    //   which BrnCollisionPolicy.h cannot include (BehaviourRig.h includes IT). Returning the
    //   member would need a cast through a base the type does not have. The slot therefore
    //   keeps Behaviour::GetCollisionPolicy's default (null) -- and null is what the console
    //   itself returns whenever the parameters are unset, which is the state every allocated
    //   gameplay camera is in here (SharedCameraContainer::Prepare's parameter binds are
    //   gated on BehaviourParameterBank). So today the two agree.
    //   DELETE-WHEN: the CollisionPolicy base is moved to BrnCollisionPolicy.h (its natural
    //   home) and CollisionPolicyAttachedToVehicle derives it -- Step 0 #3, the IceAnim /
    //   BehaviourRig fork family.

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
    //   SetupTweaker(Tweaker&)` (.cpp:148), plus the seven private helpers Update drives
    //   (ModifyTargetAngles @0x82225580, CalcSpringCoeffs @0x8220E5D0, UpdateLooking
    //   @0x82225630, UpdateJumping @0x8220EAD0, CalculateCameraTransform @0x8220E838,
    //   ApplySlideyEffects @0x822260A8, ApplyJumpEffects @0x822250C0,
    //   InterpolateLastPlayerTransform @0x82224BF0). None is declared here, so slots 2 and 8
    //   keep the base's defaults (Update returns true and leaves the camera untouched).
    //   That is a DOCUMENTED GAP, not a fabrication: transcribing the chase rig is its own
    //   wave, and nothing dispatches slot 2 today anyway (MainDirector::
    //   UpdateCameraBehavioursPostScene @0x8224FD30, the only caller of UpdateAllBehaviours,
    //   is itself still gated).
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
