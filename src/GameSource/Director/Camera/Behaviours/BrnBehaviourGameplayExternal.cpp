// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGameplayExternal::Parameters slice
// this TU owns:
//   - BehaviourGameplayExternal::Parameters::Set @0x821F9228  (defined here)
//   - BehaviourGameplayExternal::ModifyTargetAngles @0x82225580 (added 2026-08-02, helper 1/8)
//   - BehaviourGameplayExternal::CalcSpringCoeffs   @0x8220E5D0 (added 2026-08-02, helper 2/8)
//   - BehaviourGameplayExternal::InterpolateLastPlayerTransform @0x82224BF0
//                                                   (added 2026-08-02, helper 3/8)
//   - BehaviourGameplayExternal::UpdateJumping      @0x8220EAD0 (added 2026-08-02, helper 4/8)
//   - BehaviourGameplayExternal::ApplySlideyEffects @0x822260A8 (added 2026-08-02, helper 7/8)
//   - BehaviourGameplayExternal::UpdateLooking      @0x82225630 (added 2026-08-02, helper 5/8
//                                                    -- THE LAST HELPER; the eight are done)
// (each has its own banner below; none has a caller yet -- Update is the only one, and
//  Update STILL cannot link even now that all eight helpers exist: it makes a direct call to
//  Utils::CameraShakeICEController::Update @0x82241C6C and that symbol has no definition
//  anywhere in the tree. See the header's LINK-CLOSURE SET.)
//
// Parameters::Set is the seeding step the main director runs (ProcessNewVehicleEvents /
// UpdateAttribSys) and the replay director runs (PreSceneQueryUpdate) to populate an
// external ("chase") gameplay-camera block from the vehicle's attribute-system source block
// before installing it.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"

#include "GameSource/Director/Utils/BrnDirectorVehicleTracker.h"
                                                        // VehicleTracker::GetImplicitVelocity()
                                                        //   (Behaviour.h only forward-declares
                                                        //    it; ApplySlideyEffects calls it)
#include "GameShared/GameClasses/Numeric/CgsRandom.h"   // CgsNumeric::Random::RandomBool
                                                        //   (Behaviour.h only forward-declares it;
                                                        //    UpdateJumping needs the complete type)
#include "rw/math/fpu/scalar_operation.h"               // Clamp / Min / Abs / IsValid --
                                                        //   the console's fsel / fabs / fcmpu
                                                        //   self-compare idioms
#include "rw/math/vpu/vector2_operation.h"              // Dot(Vector2, Vector2) -- UpdateLooking's
                                                        //   two 2-lane dots against constant axes
#include "rw/math/vpu/vector3_operation.h"              // Mult(Vector3, Vector3) / operator+
                                                        //   (CalculateCameraTransform's arms)
#include "rw/math/vpu/matrix44affine_operation.h"       // Mult / MakeRotationX / MakeRotationY /
                                                        //   MakeRotationZ (the SDK's three
                                                        //   elementary rotation builders)
                                                        //   + SLerp / IsValid(Matrix44Affine)

#include "GameSource/Director/Camera/Camera.h"           // Camera (Update publishes into it)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"
                                                        // CgsDev::DebugInterface -- Update's
                                                        //   debug-render arm (.cpp:364/:370)
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"
                                                        // CgsDev::DebugRender::DrawBox/DrawLine

#include <cmath>                                        // std::acos -- the console's XMVectorACos
                                                        //   + std::atan / std::sqrt (Update)

// ----------------------------------------------------------------------------
// NOTE -- BehaviourGameplayExternal::Parameters::Serialise<S> (the versioned field-walk visitor:
//   Serialise<DebugMenuSerialiser> @0x8224BBB0, <TextFileWriteSerialiser> @0x8224D418,
//   <TextFileReadSerialiser> @0x822312E8) is bodied in the sibling TU file
//   BrnBehaviourGameplayExternalParameters.cpp (mirroring BrnBehaviourAftertouchCamParameters.cpp).
//   It is kept out of THIS file so the widely-consumed BrnBehaviourGameplayExternal.h stays free of
//   the BehaviourRig.h include (Utils::CameraShake::Parameters), and so the reviewed Parameters::Set
//   below (which spells the +0x0C/+0x1C shake sub-blocks as individual f32s) is left untouched -- the
//   two "Air Shake Params"/"Impact Shake Params" sub-sections are driven as CameraShake::Parameters
//   over that same storage in the serialiser TU.
// ----------------------------------------------------------------------------

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGameplayExternal::Parameters::Set @0x821F9228
//
// Seeds this external-cam parameter block: stamps the type tag + debug name + integer slot +
// the fixed default tunables, then copies the per-vehicle tunables out of the attribute-system
// source array (reached through lpSource->mpfValues, i.e. *(a2+4)). Asserts mfBoostFOV and
// mrFOV are positive, then overrides mfSlideYScaleJump to -1.0f as the final store.
//
// Float constants (decoded from the rodata loaded by the asm):
//   flt_82001CC0 = 0.0f     flt_82001C98 = 1.0f      flt_82001DA0 = 0.5f
//   flt_82002138 = 0.01f    flt_82003F40 = 0.25f     flt_82004014 = 0.1f
//   flt_82004018 = 0.75f    flt_82004270 = 3.0f      flt_820047B8 = 0.06f (0.059999999)
//   flt_820047BC = 1.15f    flt_820047C0 = 0.11f     flt_82004C68 = 0.7f (0.69999999)
//   flt_82004C6C = 60.0f    flt_82004C70 = 17.0f     flt_82004C74 = 0.015f
//   flt_82004C78 = -0.5f    flt_82004C88 = 8.0f      flt_820037C8 = -1.0f
//
// Field offsets verified store-for-store: the source-copied slots are the lfs ...(r11) loads
// off lpSource->mpfValues, and the default slots are the lfs flt_...(r10) rodata loads. Every
// 4-byte slot in +0x00..+0xAC is written.
// ----------------------------------------------------------------------------
void BehaviourGameplayExternal::Parameters::Set(const Source* lpSource)
{
    mType    = eBehaviourGameplayExternal;        // stw r9(=0), 0x00(r31)

    // --- fixed default tunables (rodata constant stores) ---
    mAirShakeParams.mfWobbleCenteringFactor = 0.11f;                             // stfs flt_820047C0, 0x18
    mAirShakeParams.mfZShakeMagnitudeDegs = 0.0f;                              // stfs flt_82001CC0, 0x10
    mAirShakeParams.mfXYWobbleMagnitudeDegs = 1.15f;                             // stfs flt_820047BC, 0x14
    mAirShakeParams.mfXYShakeMagnitudeDegs = 0.06f;                             // stfs flt_820047B8, 0x0C  (0.059999999)
    mImpactShakeParams.mfWobbleCenteringFactor = 0.11f;                             // stfs flt_820047C0, 0x28  (seed, overridden)
    mImpactShakeParams.mfXYWobbleMagnitudeDegs = 1.15f;                             // stfs flt_820047BC, 0x24  (seed, overridden)
    mImpactShakeParams.mfXYShakeMagnitudeDegs = 0.0f;                              // stfs flt_82001CC0, 0x1C
    mImpactShakeParams.mfZShakeMagnitudeDegs = 0.0f;                              // stfs flt_82001CC0, 0x20
    mbIsValid = true;                                 // stb  r8(=1), 0xAC
    mImpactShakeParams.mfWobbleCenteringFactor = 1.0f;                              // stfs flt_82001C98, 0x28  (override)
    SetDebugName("Gameplay");                        // stw  "Gameplay", 0x04
    mImpactShakeParams.mfXYWobbleMagnitudeDegs = 3.0f;                              // stfs flt_82004270, 0x24  (override)
    muVersion.muVersion = 3;                       // stw  r7(=3), 0x08
    mImpactShakeParams.mfXYShakeMagnitudeDegs = 0.0f;                              // stfs flt_82001CC0, 0x1C
    mImpactShakeParams.mfZShakeMagnitudeDegs = 0.0f;                              // stfs flt_82001CC0, 0x20
    mrPitchLimit = 8.0f;                              // stfs flt_82004C88, 0x2C
    mrRollLimit = 8.0f;                              // stfs flt_82004C88, 0x30
    mrPitchCoeff = 0.75f;                             // stfs flt_82004018, 0x34
    mrRollCoeff = 0.0f;                              // stfs flt_82001CC0, 0x38
    mrAccelerationPitchAmount = -0.5f;                             // stfs flt_82004C78, 0x44
    mfFrontInAmount = 0.0f;                              // stfs flt_82001CC0, 0x74
    mrAccelerationSensitivity = 0.015f;                            // stfs flt_82004C74, 0x48
    mfVelocitySlideZFactor0To1 = 0.0f;                              // stfs flt_82001CC0, 0x98
    mrSlideZScale = 17.0f;                             // stfs flt_82004C70, 0x60
    mrSlideZInputForHalf = 0.25f;                             // stfs flt_82003F40, 0x64
    mfInFrontFOVMax = 60.0f;                             // stfs flt_82004C6C, 0x70
    mfDropFactor = 0.5f;                              // stfs flt_82001DA0, 0xA8
    mfSpeedDisplacementHalf = 0.01f;                             // stfs flt_82002138, 0x7C  (0.0099999998)
    mfAccelZLerpAmount = 0.1f;                              // stfs flt_82004014, 0x80
    mfZLerpAmount = 0.7f;                              // stfs flt_82004C68, 0x84  (0.69999999)

    // --- per-vehicle tunables copied from the attribute-system source array ---
    const f32* lpfSrc = lpSource->mpfValues;       // lwz r11, 4(r4)  (re-loaded each store on X360)
    mfBoostFOV = lpfSrc[0x40 / 4];                 // <- source[+0x40]  (asserted > 0)
    mfBoostFOVZoomCompensation  = lpfSrc[0x3C / 4];                 // <- source[+0x3C]
    mfDownAngle  = lpfSrc[0x38 / 4];                 // <- source[+0x38]
    mfDriftYawSpring  = lpfSrc[0x34 / 4];                 // <- source[+0x34]
    mrFOV      = lpfSrc[0x30 / 4];                 // <- source[+0x30]  (asserted > 0)
    mrPitchSpring  = lpfSrc[0x2C / 4];                 // <- source[+0x2C]
    mrPivotY  = lpfSrc[0x28 / 4];                 // <- source[+0x28]
    mrPivotZ  = lpfSrc[0x24 / 4];                 // <- source[+0x24]
    mrPivotZOffset  = lpfSrc[0x20 / 4];                 // <- source[+0x20]
    mrYawSpring  = lpfSrc[0x08 / 4];                 // <- source[+0x08]
    mrSlideXScale  = lpfSrc[0x1C / 4];                 // <- source[+0x1C]
    mrSlideYScale  = lpfSrc[0x18 / 4];                 // <- source[+0x18]
    mrSlideZOutputMax  = lpfSrc[0x14 / 4];                 // <- source[+0x14]
    mrYawSpring  = lpfSrc[0x08 / 4];                 // <- source[+0x08]  (re-stored on X360)
    mfZAndTiltCutoffSpeedMPH  = lpfSrc[0x04 / 4];                 // <- source[+0x04]
    mfSlideYScaleJump  = lpfSrc[0x0C / 4];                 // <- source[+0x0C]  (overridden below)
    mfTiltAroundCarScale  = lpfSrc[0x10 / 4];                 // <- source[+0x10]
    mfZDistanceScale  = lpfSrc[0x00 / 4];                 // <- source[+0x00]

    CGS_ASSERT(mfBoostFOV > 0.0f, "mfBoostFOV > 0.0f");
    CGS_ASSERT(mrFOV > 0.0f, "mrFOV > 0.0f");

    mfSlideYScaleJump = -1.0f;                             // stfs flt_820037C8, 0xA0  (final override)
}

// ============================================================================
// BehaviourGameplayExternal::Construct @0x82224A18   (vtable slot 0)
//
// Stand the chase cam up. asm, grouped by destination (byte displacements off `this`):
//   +8..+12, +4, +16                       the INLINED Behaviour::Construct
//   stvx128 0,+32 ; +48/52/56/60 = 0.0f ;  the INLINED CameraSphericalRotationController::
//   +64/65/66 = 0 ; +72/76 = 0.0f            Construct over mRotationController @+0x020
//   CollisionPolicyAttachedToVehicle::Construct(this+80, 1)   mCollisionPolicy @+0x050
//   +672/676/680/684 = 0.0f                mAirShake    @+0x2A0 (CameraShake::Construct)
//   +688/692/696/700 = 0.0f                mImpactShake @+0x2B0 (CameraShake::Construct)
//   stvx128 0,+2784 ; stvx128 0,+2800      mLastCarPos @+0xAE0 / mLastDisplacement @+0xAF0
//   +2820..+2900                           the f32 tail (all 0.0f except the four 1.0f below)
//   +2908/2910/2912 = 0 ; +2911 = 1        the bool tail
//
// The four ONEs are the only non-zero seeds: mfSlideYScale (+0xB10), mfPitchCoefficient
// (+0xB34), mfYawSpring (+0xB38), mfPitchSpring (+0xB3C); plus mbEnableBoostEffects (+0xB5F).
// mfTimeInJump (+0xB4C) and mfJumpFOV (+0xB58) are deliberately NOT stored here (the jump
// machinery and Prepare own them) -- reproduced as-is, nothing extra is invented.
//
// FLAG (three sub-object Constructs NOT reproduced):
//   * mRotationController -- the console INLINES CameraSphericalRotationController::Construct
//     here, and its tail stores (+72/+76) fall inside the controller's embedded SmoothMover,
//     whose interior is not byte-mapped. Calling the (declaration-only) member instead of
//     poking those offsets is the faithful shape; the member is invoked below and link-stubbed
//     in DirectorLinkStubs.cpp until BrnCameraSphericalRotationController.cpp lands.
//   * mBoostShake -- Constructed by Prepare on the console, not by Construct. Reproduced.
// Both of those sub-objects are unread today (Update is not transcribed) and the pool's
// placement-new is `new (slot) T()`, i.e. value-initialisation, so they start zeroed anyway.
// DELETE-WHEN: CameraSphericalRotationController::Construct is homed.
//
// ⭐ RESOLVED 2026-08-01 -- mCollisionPolicy USED TO BE A THIRD ENTRY IN THAT FLAG LIST.
//   `CollisionPolicyAttachedToVehicle::Construct(this+0x50, 1)` @0x82224AB0 is a real call
//   and it is now reproduced below: the type gained a declared+bodied `Construct(bool)` in
//   BrnCollisionPolicy.h and, as of the 2026-08-01 DWARF tail carve, named members for every
//   byte it seeds except mPitchMover's two floats. (The old note's "four further stores the
//   console makes right after it" was wrong -- +664/+665/+668/+669 are stores made BY the
//   policy's own Construct at policy +0x248/+0x249/+0x24C/+0x24D, not by this function; the
//   asm here goes straight from the call to the mLastCarPos/mLastDisplacement zeroing.)
//   ⚠️ CONSEQUENCE OF THE OLD OMISSION: the chase cam's policy was never seeded at all --
//   in particular mfMaxRadius stayed 0 instead of FLT_MAX, i.e. "the camera is always
//   further out than allowed", and mbAutoElevate stayed false.
// ============================================================================
void BehaviourGameplayExternal::Construct()
{
    Behaviour::Construct();              // the inlined base head

    mRotationController.Construct();     // stvx128 0,+32 / +48..+66 / +72 / +76
    mCollisionPolicy.Construct(true);    // 0x82224AB0: Construct(this+0x50, r4 = 1)
    mAirShake.Construct();               // *(this + 672/676/680/684) = 0.0f
    mImpactShake.Construct();            // *(this + 688/692/696/700) = 0.0f

    mLastCarPos.SetZero();               // stvx128 v13(=0), this+2784
    mLastDisplacement.SetZero();         // stvx128 v13(=0), this+2800

    mfCenteringFactor       = 0.0f;      // +2820
    mfDesiredZDisplacement  = 0.0f;      // +2824
    mfSmoothedZDisplacement = 0.0f;      // +2828
    mfSlideYScale           = 1.0f;      // +2832
    mfDriftScale            = 0.0f;      // +2836
    mOverrideScale          = 0.0f;      // +2840
    mfDutchVelocity         = 0.0f;      // +2844
    mfDutchDrift            = 0.0f;      // +2848
    mfYawVelocity           = 0.0f;      // +2852
    mfYawDrift              = 0.0f;      // +2856
    mfZVelocity             = 0.0f;      // +2860
    mfZDrift                = 0.0f;      // +2864
    mfPitchCoefficient      = 1.0f;      // +2868
    mfYawSpring             = 1.0f;      // +2872
    mfPitchSpring           = 1.0f;      // +2876
    mfWobbleScale           = 0.0f;      // +2880
    mfImpactShakeFactor     = 0.0f;      // +2884
    mfTimeDelta             = 0.0f;      // +2888
    mfDropAmount            = 0.0f;      // +2896
    mfFOVAdjustment         = 0.0f;      // +2900

    mbLastCarPosInitialised = false;     // +2908
    mbEnableDebugRender     = false;     // +2910
    mbEnableBoostEffects    = true;      // +2911
    mbJumping               = false;     // +2912
    // FLAG: the console also stores the same zero byte at +2913 (0xB61). The DWARF member list
    //   (BehaviourGameplayExternal.h:116..:160) ends at mbJumping @+0xB60, so that sixth byte
    //   has no name and is NOT modelled -- nothing reads it, and inventing a member for it
    //   would be a guess. DELETE-WHEN: the DWARF gains the trailing member (or it is proven to
    //   be tail padding the compiler zeroes).
}

// ============================================================================
// BehaviourGameplayExternal::Prepare @0x82240738   (vtable slot 1)
//
//   CameraShakeICEController::Construct(this+704)  -> mBoostShake
//   four vec stores at this+2720 (+0/16/32/48)     -> mLastPlayerTransform = identity
//                                                     (the four rows come from stack temps the
//                                                      prologue built; the console's own
//                                                      Matrix44Affine identity)
//   *(this+2904) = 80.0f                           -> mfJumpFOV
//   *(this+2909) = 1                               -> mbSnapToCar
//   *(this+8)    = 1                               -> mbIsPrepared
//   li r3,1                                        -> cannot fail
//
//   *(this+656) = FLT_MAX                          -> mCollisionPolicy.ResetRadiusSmoothing()
//   *(this+670) = 1                                -> mCollisionPolicy.ResetTrafficCollision()
//
// ⭐ THE TWO POLICY STORES ARE REPRODUCED AS OF 2026-08-01 (they were a documented FLAG:
//   "offsets whose members are not identified"). Behaviour +656 == 0x290 and +670 == 0x29E
//   are mCollisionPolicy (@+0x50) +0x240 and +0x24E -- mfMaxRadius and
//   mbResetVehicleCollision, now carved into BrnCollisionPolicy.h from the DecFIGS DWARF.
//   The identical triple is what SharedCameraContainer::ForcePrimaryGameplayBehaviourToFinish
//   re-issues from outside; see BrnDirectorArbitratorSharedCameraContainer.cpp.
// ============================================================================
bool BehaviourGameplayExternal::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mBoostShake.Construct();                  // CameraShakeICEController::Construct(this+704)
    mLastPlayerTransform.SetIdentity();       // four vec stores at this+2720

    mfJumpFOV = 80.0f;                        // *(this + 2904)
    SnapToCar(true);                          // *(this + 2909) = 1

    mCollisionPolicy.ResetRadiusSmoothing();  // *(this + 656) = FLT_MAX   (policy +0x240)
    mCollisionPolicy.ResetTrafficCollision(); // *(this + 670) = 1         (policy +0x24E)

    SetPrepared();                            // *(this + 8) = 1
    return true;
}

// ============================================================================
// BehaviourGameplayExternal::GetCollisionPolicy @0x821F9138   (vtable slot 5)
//
// Store-for-store from the asm (r31 == this):
//   lwz  r11, 0xB00(r31) ; cmplwi ; bne  -> if (!mpParameters) FireAssert(
//                                             "calling GetCollisionPolicy() with no
//                                              parameters", ...Camera/Behaviours/
//                                              BehaviourGameplayExternal.cpp, line 0x78==120)
//   lwz  r11, 0xB00(r31)                  -> mpParameters (re-loaded)
//   addi r3,  r31, 0x50                   -> the return value is ALWAYS formed first:
//                                            &mCollisionPolicy
//   lbz  r11, 0xAC(r11) ; cmplwi ; bne    -> mpParameters->mbIsValid
//   li   r3, 0                            -> ... and only replaced by null when it is false
//
// ⭐ Both operands land exactly on the named members (mpParameters @+0xB00,
// Parameters::mbIsValid @+0xAC, mCollisionPolicy @+0x50) -- an independent confirmation of
// the header's layout that costs nothing.
//
// ⚠️ THE ASSERT DOES NOT GATE. CgsAssert.cpp:34's FireAssert logs and returns (the
// __debugbreak is commented out), and so does the console's: the very next instruction
// re-loads mpParameters and dereferences it. Reproduced with that shape -- assert, then
// carry on -- rather than an early return the console does not make.
//
// ⚠️ SCOPE OF THE FIX, stated honestly: handing the policy back is now CORRECT, but it is
// still INERT on this build, and for a reason that has nothing to do with this function.
// CollisionPolicyAttachedToVehicle does not yet override CollisionPolicy::
// GenerateSceneQueries / ::ProcessSceneQueryResults (BrnCollisionPolicy.h:115-116 -- the base
// slice's two `{}`), so the scene-query pass reaches the right object and asks it to do
// nothing. What this DOES buy is that the divergence is closed at the seam the console draws
// it at, so when those two overrides land the scene-query pass reaches them -- instead of a
// null return two layers up dropping them without a trace.
// ============================================================================
CollisionPolicy* BehaviourGameplayExternal::GetCollisionPolicy()
{
    CGS_ASSERT(mpParameters != 0, "calling GetCollisionPolicy() with no parameters");

    if (!mpParameters->mbIsValid)
        return 0;

    return &mCollisionPolicy;
}

// ============================================================================
// BehaviourGameplayExternal::SetParameters @0x821F91A8   (vtable slot 7)
//
//   if (*a2) assert "lpParameters->GetType() == eBehaviourGameplayExternal"
//                   (BehaviourGameplayExternal.cpp:138)
//   v3[704] = a2        ; byte +0xB00 -> mpParameters
//   v3[4]   = a2[1]     ; byte +0x10  -> the BASE's mpcDebugParametersName, from the block's
//                         byte +0x04 -> Behaviour::Parameters::mpcDebugName
//
// ⭐ Both of those used to be modelled as an invented `mpcCachedName` member on a raw-offset
// slice. With the real base in place the whole tail is the base's own protected
// SetDebugParametersName(lpParameters->GetDebugName()).
//
// The DWARF declares this taking the BASE Parameters type (.cpp:135), which is what makes it
// the slot-7 override (the bumper's same-named function takes its DERIVED block and so only
// hides the base name). The type assert is what narrows it back down.
// ============================================================================
void BehaviourGameplayExternal::SetParameters(const Behaviour::Parameters* lpParameters)
{
    CGS_ASSERT(lpParameters->GetType() == static_cast<u32>(eBehaviourGameplayExternal),
               "lpParameters->GetType() == eBehaviourGameplayExternal");

    mpParameters = static_cast<const Parameters*>(lpParameters);   // v3[704] = a2
    SetDebugParametersName(lpParameters->GetDebugName());          // v3[4]   = a2[1]
}

// ----------------------------------------------------------------------------
// BehaviourGameplayExternal::Parameters::Construct   (DWARF h:277)
//
// ⭐ BODIED 2026-08-02 (camera parameter-chain wave), from the console's own INLINED copy
// inside BehaviourParameterBank::Construct @0x8223DC90 (block base r11 = r31 + 0x2488).
// Same reason as its bumper sibling: the bank is now homed, so this actually runs, and
// SharedCameraContainer::Prepare binds the chase camera to this block long before any car's
// attribs arrive. It is what leaves mbIsValid FALSE -- the state the whole camera chain
// turns on.
//   0x8223DCD4  stw  r30(=0), 4(r11)     -> mpcDebugName = 0
//   0x8223DCE0  stw  r30(=0), 0(r11)     -> mType        = eBehaviourGameplayExternal (0)
//   0x8223DCE4  stb  r30(=0), 0xAC(r11)  -> mbIsValid    = false
//   0x8223DCF0/DCF4/DCEC/DCE8            -> the inlined CameraShake::Parameters::Construct
//                                           over mAirShakeParams @+0x0C (0.06/0.0/1.15/0.11,
//                                           kept -- no override follows)
//   0x8223DD08/DD14/DD04/DD00 then       -> the same seed over mImpactShakeParams @+0x1C,
//   0x8223DD1C/DD20/DD2C/DD18               then the four overrides leaving
//                                           {0.0f, 0.0f, 3.0f, 1.0f}
//   0x8223DD34  stfs f24(0.5f), 0xA8(r11)-> mfDropFactor = 0.5f
// Nothing else in the block is written -- the per-vehicle tunables are Set's job. Note the
// three values that are the SAME as Set's fixed-default prefix (air shake, impact shake,
// mfDropFactor): the bank pre-seeds them so the block is coherent before any car exists.
// ----------------------------------------------------------------------------
void BehaviourGameplayExternal::Parameters::Construct()
{
    mType = eBehaviourGameplayExternal;    // stw r30(=0), 0x00
    SetDebugName(0);                       // stw r30(=0), 0x04

    mAirShakeParams.Construct();           // 0.06f / 0.0f / 1.15f / 0.11f, no override

    mImpactShakeParams.Construct();
    mImpactShakeParams.mfXYShakeMagnitudeDegs  = 0.0f;   // stfs flt_82001CC0, 0x1C
    mImpactShakeParams.mfZShakeMagnitudeDegs   = 0.0f;   // stfs flt_82001CC0, 0x20
    mImpactShakeParams.mfXYWobbleMagnitudeDegs = 3.0f;   // stfs flt_82004270, 0x24
    mImpactShakeParams.mfWobbleCenteringFactor = 1.0f;   // stfs flt_82001C98, 0x28

    mfDropFactor = 0.5f;                   // stfs flt_82001DA0, 0xA8

    mbIsValid = false;                     // stb r30(=0), 0xAC
}

// ============================================================================
// BehaviourGameplayExternal::ModifyTargetAngles @0x82225580 / PS3 @0x18E24 (.cpp:622..:634)
//
// Helper 1 of the 8. Clamp the target pitch (X) and roll (Z) into an authored band, then
// scale the roll by its coefficient. 43 X360 asm lines, no calls, no asserts -- the whole
// function is two Clamps and a multiply, so it is settled by inspection.
//
// The VMX, statement for statement (r4 == lrCameraAttribs, r5 == lrTargetAngles):
//   0x82225584  lfs f13, 0x2C(r4)                 -- mrPitchLimit
//   0x8222559C  lfs f0,  flt_8200174C (3.1415927) -- rw::math::PI (the SAME constant
//               GetSmallestDifferenceBetweenRadAngles' second assert names by text, which
//               is how it is known to be PI and not a coincidental 3.14159)
//   0x822255A4  fdivs f13, f0, f13                -- PI / mrPitchLimit
//   0x8222558C/A0  vspltisw v13,-1 ; vslw v9,v13,v13  -- 0x80000000 in every lane, i.e. the
//               SIGN MASK: the negative bound is built by `vxor`, not by a second divide
//   0x822255DC  vmaxfp v13, v9(-bound), v10(angles.x)
//   0x822255E0  vminfp v13, v12(+bound), v13
//   0x822255E4  vrlimi128 v0, v13, 8, 0           -- write lane X only
//   ... the identical pair for lane Z against 0x30(r4) == mrRollLimit, mask 2
//   0x82225600  lfs f0, 0x38(r4)                  -- mrRollCoeff
//   0x82225620  vmulfp128 v0, v0, v12             -- all lanes multiplied ...
//   0x82225624  vrlimi128 v13, v0, 2, 0           -- ... but only lane Z written back
//
// ⚠️ THE TUNABLE IS A DIVISOR, NOT A BOUND. `PI / mrPitchLimit` with the authored default
// 8.0f is +/- 0.3927 rad == +/- 22.5 degrees. Reading mrPitchLimit as the bound itself
// would open the band to +/- 458 degrees.
// ⚠️ mrRollCoeff's authored default is 0.0f (Parameters::Set stores flt_82001CC0 there),
// so on a stock car this function kills roll entirely -- worth knowing before reading a
// "the camera never rolls" symptom as a bug.
// ============================================================================
void BehaviourGameplayExternal::ModifyTargetAngles(const Parameters& lrCameraAttribs,
                                                   Vector3& lrTargetAngles)
{
    const f32 KF_PI = 3.1415927f;                                  // flt_8200174C

    const f32 lfPitchBound = KF_PI / lrCameraAttribs.mrPitchLimit;
    const f32 lfRollBound  = KF_PI / lrCameraAttribs.mrRollLimit;

    // .cpp:625 / :626 -- Min(+bound, Max(-bound, v)), in that order on both builds.
    lrTargetAngles.x = rw::math::fpu::Clamp(lrTargetAngles.x, -lfPitchBound, lfPitchBound);
    lrTargetAngles.z = rw::math::fpu::Clamp(lrTargetAngles.z, -lfRollBound,  lfRollBound);

    // A separate statement AFTER both clamps (the X360 stores the clamped vector, then
    // multiplies and stores lane Z again). Its DecFIGS line is absorbed into the inlined
    // VecFloat spans -- the own-line set for this function is {625, 626, 634}, and 634 is
    // the closing brace -- so no .cpp:NNN is claimed for it.
    lrTargetAngles.z *= lrCameraAttribs.mrRollCoeff;
}

// ============================================================================
// BehaviourGameplayExternal::CalcSpringCoeffs @0x8220E5D0 / PS3 @0x3A37C (.cpp:644..:665)
//
// Helper 2 of the 8. Turn two per-frame spring RATES into the pair of blend vectors the
// chase rig lerps with: one holding the rate (clamped, and speed-scaled on X) and one
// holding its complement.
//
// 153 X360 asm lines, ~110 of which are the two NaN tripwires. The arithmetic tail
// (0x8220E7A0..0x8220E824) is 35 instructions and every one of them lands:
//   0x8220E7B0  fabs  f11, f29                       -- |lfSpeedMPH|
//   0x8220E7D8  fmuls f11, f11, flt_82005574 (0.02)  -- saturates at 50 MPH
//   0x8220E7F8  fsel  f11, f11-1, 1.0, f11           -- Min(that, 1.0)  == lfSpeedFactor
//   0x8220E7C8  fsel  f13, -x, 0.0, x                -- Max(lfXSpringCoeff, 0)
//   0x8220E7E8  fsel  f13, 1-f13, f13, 1.0           -- Min(that, 1.0)
//   0x8220E7F4  fsubs f13, 1.0, f13                  -- 1 - Clamp(x, 0, 1)
//   0x8220E804  fmuls f0,  f13, f11                  -- ... times lfSpeedFactor  -> lane X
//   (the same Max/Min pair on lfYSpringCoeff, WITHOUT the speed scale)             -> lane Y
//   0x8220E7C0  stfs  0.0f -> lane Z ; 0x8220E80C stw 0 -> lane W
//   0x8220E818  vsubfp v13, 1.0, v0                  -- the complement, all four lanes
//   0x8220E81C  stvx128 v0,  r0, r19  (== r8, the FIFTH argument)
//   0x8220E820  stvx128 v13, r0, r20  (== r7, the FOURTH argument)
//
// ⚠️⚠️ THE OUTPUTS ARE REVERSED RELATIVE TO THE DWARF'S OWN ARGUMENT NAMES -- the 4th
// parameter is named `lSpring` and receives `1 - v`; the 5th is named `lInverseSpring` and
// receives `v`. That is not a transcription slip, it is what `mr r20, r7` / `mr r19, r8`
// plus the two stores above say, and DecFIGS' parameter names are what let it be stated at
// all. Update's only call site feeds (speedMPH, lfTimeStepMod * mfZVelocity,
// lfTimeStepMod * mfYawDrift).
//
// ⚠️ THE W LANE IS WRITTEN ON BOTH OUTPUTS: 0 on the fifth argument, 1.0 on the fourth
// (the `vsubfp` is a full 4-lane subtract from a splatted 1.0f). Reproduced, since these
// are Vector3s with a live w lane in this tree.
//
// ⚠️ BOTH ASSERTS STREAM THE SAME MESSAGE. An earlier note said the first streams
// "lfXSpringCoeff: " and the second " lfYSpringCoeff: "; the asm says both build the full
// "lfXSpringCoeff: <x> lfYSpringCoeff: <y>\n" buffer and differ ONLY in the line they cite
// (X360 0x2D0 == 720 and 0x2D1 == 721; DecFIGS :646 / :647 -- the two builds' line numbers
// for this file differ, and the DecFIGS ones are what the rest of this cluster quotes).
// Both non-gating.
// ============================================================================
void BehaviourGameplayExternal::CalcSpringCoeffs(f32 lfSpeedMPH,
                                                 f32 lfXSpringCoeff,
                                                 f32 lfYSpringCoeff,
                                                 Vector3& lSpring,
                                                 Vector3& lInverseSpring)
{
    // .cpp:646 / :647 -- the console's `fcmpu fN, fN` NaN self-compare, which is exactly
    // rw::math::fpu::IsValid. Streamed message; the text below names both operands as the
    // console's buffer does.
    CGS_ASSERT(rw::math::fpu::IsValid(lfXSpringCoeff), "lfXSpringCoeff / lfYSpringCoeff");
    CGS_ASSERT(rw::math::fpu::IsValid(lfYSpringCoeff), "lfXSpringCoeff / lfYSpringCoeff");

    // .cpp:653 -- flt_82005574 == 0.02f, flt_82001C98 == 1.0f.
    const f32 lfSpeedFactor =
        rw::math::fpu::Min(rw::math::fpu::Abs(lfSpeedMPH) * 0.019999999f, 1.0f);

    lInverseSpring.x = (1.0f - rw::math::fpu::Clamp(lfXSpringCoeff, 0.0f, 1.0f)) * lfSpeedFactor;
    lInverseSpring.y = (1.0f - rw::math::fpu::Clamp(lfYSpringCoeff, 0.0f, 1.0f));
    lInverseSpring.z = 0.0f;
    lInverseSpring.w = 0.0f;

    // .cpp:664 -- one `vsubfp` against a splatted 1.0f, so all four lanes.
    lSpring.x = 1.0f - lInverseSpring.x;
    lSpring.y = 1.0f - lInverseSpring.y;
    lSpring.z = 1.0f - lInverseSpring.z;
    lSpring.w = 1.0f - lInverseSpring.w;
}

// ============================================================================
// The five file-scope VecFloat tunables InterpolateLastPlayerTransform reads.
//
// ⭐⭐ NAMES *AND* VALUES, AND NEITHER CAME FROM THE PLACE YOU WOULD LOOK FIRST.
//
// NAMES: the DecFIGS DWARF puts all five in this .cpp's ANONYMOUS namespace at
// BehaviourGameplayExternal.cpp:565..:569, in this order
// (references/DecFIGS/dwarfdump/.../BehaviourGameplayExternal.cpp), and the PS3 export carries
// them as named TOC symbols. They are not class members, so they are spelled here the way the
// DWARF spells them.
//
// VALUES: ⚠️⚠️ THE X360 IMAGE HOLDS ZERO FOR ALL FIVE, AND NOTHING IN THE EXPORTED CODE STORES
// TO THEM. `VecFloat` has a non-trivial constructor, so these live in a BSS-like span
// (0x82FAA700..0x82FAAC00 -- the whole director-wide VecFloat constant table reads back as
// zeros) and are filled by a compiler-generated dynamic initialiser that the IDA export set
// does not cover: a literal scan for `unk_82FAA940` finds exactly ONE function, the READER
// below. Reading a value straight out of the image here would have produced five plausible
// 0.0f "constants" -- a max interp of zero, a divisor guard of zero -- and nothing downstream
// would have complained.
// ⇒ recovered by BYTE-SCANNING .text for `lis rD, 0x82FB` and following each one to its
//   completing `addi`, which lands on an unexported initialiser block at 0x82C49390..0x82C49478
//   (scratchpad\fh_lis.py + fh_dis.py). Each entry there is
//     lfs f0, <rodata>  /  stfs f0, -16(r1)  /  lvlx  /  vspltw  /  stvx v0, r0, <target>
//   so the rodata float and the destination are read off the SAME block. Verified two ways:
//     * the initialiser emits them in DECLARATION ORDER, and that order is exactly the DWARF's
//       :565, :566, :567, :568, :569;
//     * every value's ROLE in the formula matches its NAME independently (the one that is
//       subtracted from a speed is the ..._MPS one, the vmaxfp bound is MIN, the vminfp bound
//       is MAX, the divisor guard is MIN_DIV_ANGLE) -- and MIN < MAX holds.
//   The PS3 export names the two loads inside the :584 statement (r11 = CAR_SPEED_FACTOR,
//   r9 = the subtracted one), which pins that pair a third way.
//
//   .cpp | X360 store target | rodata     | value        | name
//   -----+-------------------+------------+--------------+--------------------------------
//   :565 | 0x82FAAB40        | 0x8200D5F4 | 0.59999996   | CAR_SPEED_FACTOR
//   :566 | 0x82FAA940        | 0x82004270 | 3.0          | SPEED_TO_INTERP_MPS
//   :567 | 0x82FAA770        | 0x820047C8 | 0.05         | MAX_INTERP   (radians/frame)
//   :568 | 0x82FAA7C0        | 0x82002138 | 0.01         | MIN_INTERP   (radians/frame)
//   :569 | 0x82FAABF0        | 0x82004884 | 0.00001      | MIN_DIV_ANGLE
// ⚠️ 0x8200D5F4 is 0x3F199999, i.e. the float BELOW 0.6f (0x3F19999A). Transcribed as the
//   image has it, the way this file already spells 0.019999999f / 0.069999999f.
// ============================================================================
namespace
{
    const f32 KVF_LAST_PLAYER_TRANSFORM_CAR_SPEED_FACTOR    = 0.59999996f;
    const f32 KVF_LAST_PLAYER_TRANSFORM_SPEED_TO_INTERP_MPS = 3.0f;
    const f32 KVF_LAST_PLAYER_TRANSFORM_MAX_INTERP          = 0.05f;
    const f32 KVF_LAST_PLAYER_TRANSFORM_MIN_INTERP          = 0.01f;
    const f32 KVF_LAST_PLAYER_TRANSFORM_MIN_DIV_ANGLE       = 0.00001f;
}

// ============================================================================
// InterpolateLastPlayerTransform @0x82224BF0 / PS3 @0x39B74  (.cpp:575..:600)
// helper 3/8 -- BODIED 2026-08-02 (final-helpers wave).
//
// The chase rig does NOT follow the car's own frame: it follows a LAGGED copy of it,
// mLastPlayerTransform, and this is the function that drags that copy toward the real one.
// The lag is rate-limited in ANGLE, not in time -- which is why the camera swings smoothly
// through a hard corner instead of snapping.
//
// 307 X360 asm lines, ELEVEN statements; ~200 of those lines are the three NaN tripwires
// (one of which streams three floats) and two inlined rsqrt Newton-Raphson normalises.
//
// STATEMENT MAP -- DecFIGS line numbers, X360 addresses:
//   :579  lvfOutAngle = ACos(Clamp(Dot(Normalize(lPlayerTransform.zAxis),
//                                      Normalize(mLastPlayerTransform.zAxis)), -1, +1))
//         X360 0x82224C2C loads lPlayerTransform+0x20 and 0x82224C38 loads this+0xAC0
//         (mLastPlayerTransform's zAxis row -- mLastPlayerTransform is at +0xAA0), two
//         vmsum3fp128 self-dots, vrsqrtefp + one Newton-Raphson step each, one vmsum3fp128
//         cross-dot, vmaxfp against vcfsx(-1), vminfp against vcsxwfp(+1), then
//         `bl XMVectorACos` @0x82224C8C.
//         ⭐ IT IS THE SAME FOUR LINES rw::math::vpu::SLerp OPENS WITH -- and SLerp is
//         called at :598 with the same two matrices, so the console computes this angle
//         TWICE per frame. Reproduced as written rather than folded, because the two uses
//         diverge (this one is clamped and rate-limited; SLerp's is not).
//   :580  CGS_ASSERT(IsValid(lvfOutAngle))                      X360 line 0x28E == 654
//   :584  lvfSpeedMod = Sqr(Max(lvfCarSpeed - SPEED_TO_INTERP_MPS, 0))
//                       * CAR_SPEED_FACTOR * lvfTimestep
//         X360 0x82224CEC..0x82224D04; PS3 0x39D98..0x39DA8, where the three `vmaddfp`s
//         against a zero register are the SDK's Sqr (vec_float_operation_inline.h:1540) and
//         two scalar multiplies, and `v22`/`v21` are the two VecFloat arguments in order.
//   :585  CGS_ASSERT(IsValid(lvfSpeedMod))                      X360 line 0x293 == 659
//   :589  lvfAngleToRotate        = lvfOutAngle * Clamp(lvfSpeedMod, 0, 1)
//   :590  lvfAngleToRotateClamped = Clamp(lvfAngleToRotate, MIN_INTERP, MAX_INTERP)
//         (Max first, then Min, on both builds: X360 0x82224D80 / 0x82224D90.)
//   :590/:591  lvfSpeedMod = lvfAngleToRotateClamped / Max(lvfOutAngle, MIN_DIV_ANGLE)
//         ⚠️ NO OWN-LINE ATTESTATION: on PS3 every instruction of this statement is charged
//         to the inlined vec_float_operation_inline.h:1689 (the reciprocal + its two
//         Newton-Raphson steps) and scalar_operation_inline.h:150, so DWARF never names its
//         .cpp line. It is bracketed between :590 and :592 and that is all that is attested.
//         ⭐ THE VARIABLE IS REUSED: lvfSpeedMod stops being a speed here and becomes the
//         SLerp blend fraction. The X360 stack slot the :592 assert streams as "lvfSpeedMod"
//         (var_100) holds THIS value, not the :584 one -- which is how the reuse is known.
//   :592  CGS_ASSERT(IsValid(lvfSpeedMod)) streaming all three floats, in the order
//         "lvfSpeedMod: " / " lvfAngleToRotate: " / " lvfAngleToRotateClamped: "
//                                                              X360 line 0x29A == 666
//   :598  mLastPlayerTransform = SLerp(mLastPlayerTransform, lPlayerTransform,
//                                      Clamp(lvfSpeedMod, 0, 1), lvfAngleToRotateClamped)
//         X360 0x82224ED4: r4 = this+0xAA0 (lFrom), r5 = the by-value player transform (lTo),
//         v1 = the clamped blend, r6 = &lvfAngleToRotateClamped. DecFIGS names SLerp's first
//         two parameters lFrom / lTo, which fixes the direction.
//   :599  CGS_ASSERT(IsValid(mLastPlayerTransform)) -- the console open-codes it as twelve
//         `vspltw` + `vcmpeqfp.` self-compares, four rows by three lanes (0x82224F04..
//         0x82225088), i.e. exactly matrix44affine_operation.h's IsValid.
//
// ⚠️ SLerp's FOURTH argument is an OUT parameter and the console hands it the storage of
//   lvfAngleToRotateClamped, which nothing reads afterwards. Our SLerp declares it
//   `Vector3* lpvAngleOut` (a corrected signature recorded in that vendor header) rather than
//   the DWARF's `VecFloat&`, so it is spelled here as a named throwaway local. The value
//   written back is discarded either way; the reuse is recorded, not reproduced by aliasing.
//
// ⚠️ PARAMETER NAMES: `lPlayerTransform` IS the DWARF's own (the PS3 export carries it on r4).
//   The two VecFloats get no DWARF name -- they arrive in vector registers -- so `lvfCarSpeed`
//   and `lvfTimestep` are DESCRIPTIONS read off what the asm does with them: the first is what
//   SPEED_TO_INTERP_MPS is subtracted from, the second is a bare multiplier, and Update's call
//   site @0x82240E38 builds the second by `vspltw`-broadcasting a scalar stack float. Re-read
//   them that way rather than trusting the names.
//
// ⚠️ VecFloat is a BROADCAST lane here (all four lanes equal), so the body is the portable
//   scalar math on lane x -- the same de-vectorisation rw::math::vpu::SLerp itself uses for
//   its blend argument. Never a placeholder: every operation below is one console instruction.
//
// ⚠️⚠️ `::VecFloat` IS QUALIFIED ON PURPOSE -- there are TWO VecFloats in scope here and the
//   unqualified spelling silently picks the wrong one. See the declaration's note in the
//   header: BrnDirector::VecFloat (BrnDirectorTimestep.h:39) shadows the global
//   BrnCommonTypes alias inside this namespace, and because both are 16 bytes wide the
//   shadowed form compiles cleanly in either direction -- it just means a DIFFERENT TYPE in
//   TUs that pull in the Timestep header than in TUs that do not.
// ============================================================================
void BehaviourGameplayExternal::InterpolateLastPlayerTransform(Matrix44Affine lPlayerTransform,
                                                               ::VecFloat lvfCarSpeed,
                                                               ::VecFloat lvfTimestep)
{
    // .cpp:579 -- the angle between the two forward axes.
    const f32 lfCos = rw::math::fpu::Clamp(
        rw::math::vpu::Dot(rw::math::vpu::Normalize(lPlayerTransform.zAxis),
                           rw::math::vpu::Normalize(mLastPlayerTransform.zAxis)),
        -1.0f, 1.0f);
    const f32 lfOutAngle = std::acos(lfCos);
    CGS_ASSERT(rw::math::fpu::IsValid(lfOutAngle), "IsValid( lvfOutAngle )");   // .cpp:580

    // .cpp:584 -- how much of the gap to close this frame, as a fraction. Below 3 m/s the
    // camera does not chase the car's heading at all; the response then grows with the SQUARE
    // of the excess speed, so it saturates (Clamp below) at about 8 m/s at 60 fps.
    const f32 lfExcessSpeed =
        rw::math::fpu::Max(lvfCarSpeed.x - KVF_LAST_PLAYER_TRANSFORM_SPEED_TO_INTERP_MPS, 0.0f);
    f32 lfSpeedMod = (lfExcessSpeed * lfExcessSpeed)
                   * KVF_LAST_PLAYER_TRANSFORM_CAR_SPEED_FACTOR * lvfTimestep.x;
    CGS_ASSERT(rw::math::fpu::IsValid(lfSpeedMod), "IsValid( lvfSpeedMod )");   // .cpp:585

    // .cpp:589 / :590 -- turn that fraction into an ANGLE, then rate-limit the angle into
    // [0.01, 0.05] radians per frame (0.57 to 2.9 degrees).
    const f32 lfAngleToRotate = lfOutAngle * rw::math::fpu::Clamp(lfSpeedMod, 0.0f, 1.0f);
    const f32 lfAngleToRotateClamped =
        rw::math::fpu::Clamp(lfAngleToRotate,
                             KVF_LAST_PLAYER_TRANSFORM_MIN_INTERP,
                             KVF_LAST_PLAYER_TRANSFORM_MAX_INTERP);

    // .cpp:590/:591 -- and back into a blend fraction of the remaining gap.
    lfSpeedMod = lfAngleToRotateClamped
               / rw::math::fpu::Max(lfOutAngle, KVF_LAST_PLAYER_TRANSFORM_MIN_DIV_ANGLE);
    CGS_ASSERT(rw::math::fpu::IsValid(lfSpeedMod),                              // .cpp:592
               "lvfSpeedMod / lvfAngleToRotate / lvfAngleToRotateClamped");

    // .cpp:598 -- drag the lagged frame that far toward the real one.
    Vector3 lvUnusedAngleOut;   // the console reuses lvfAngleToRotateClamped's slot here
    mLastPlayerTransform = rw::math::vpu::SLerp(mLastPlayerTransform,
                                                lPlayerTransform,
                                                rw::math::fpu::Clamp(lfSpeedMod, 0.0f, 1.0f),
                                                &lvUnusedAngleOut);
    CGS_ASSERT(rw::math::vpu::IsValid(mLastPlayerTransform),                    // .cpp:599
               "IsValid( mLastPlayerTransform )");
}

// ============================================================================
// The file-scope jump tuning constants (DecFIGS-NAMED, X360-VALUED).
//
// The DecFIGS PS3 export keeps these as named symbols
// (`BrnDirector::Camera::BehaviourGameplayExternal::kfJumpParams...`), so the NAME of each
// one is read, not guessed. The VALUE of each was then read out of the X360 ARTIST image
// (scratchpad\afw_id1b.py) at the address the SAME STATEMENT loads -- so every pairing below
// is anchored on a statement both builds agree on, not on address order (the two builds lay
// the block out differently).
//
//   PS3 statement | X360 address | value    | name
//   --------------+--------------+----------+--------------------------------------
//   :1011/:1045   | flt_82CDA6E8 | 0.1      | kfJumpParamsBlendFactor
//   :1041         | flt_82CDA6EC | 0.25     | kfJumpParamsDutchBlendFactor
//   :993          | flt_82CDA6F0 | 0.02     | kfJumpParamsDutchCooloffRate
//   :977/:981     | flt_82CDA6F4 | 5.0      | kfJumpParamsDutchInitialVelocity
//   :1010         | flt_82CDA708 | 0.2      | kfJumpParamsTimeDelta
//   :1010         | flt_82CDA70C | 0.001    | kfJumpParamsTimeDeltaBlendInFactor
//   :1044         | flt_82CDA710 | 0.1      | kfJumpParamsTimeDeltaBlendOutFactor
//   :1015         | flt_82CDA714 | 0.01     | kfSlideYScaleScaleUpFactor
//   :991          | flt_82CDAD10 | 0.261799 | kfJumpParamsDutchMax
//
// ⭐ kfJumpParamsDutchMax reads as 0.2617994 == 15 degrees in radians, and
// kfJumpParamsDutchInitialVelocity is 5.0 multiplied by KF_DEGS_TO_RADS at the point of use
// (i.e. 5 degrees/second) -- two independent sanity checks that these are angular tunables
// in the units the rest of the class uses.
// ============================================================================
const f32 BehaviourGameplayExternal::kfJumpParamsBlendFactor             = 0.1f;
const f32 BehaviourGameplayExternal::kfJumpParamsDutchBlendFactor        = 0.25f;
const f32 BehaviourGameplayExternal::kfJumpParamsDutchCooloffRate        = 0.019999999f;
const f32 BehaviourGameplayExternal::kfJumpParamsDutchInitialVelocity    = 5.0f;
const f32 BehaviourGameplayExternal::kfJumpParamsTimeDelta               = 0.2f;
const f32 BehaviourGameplayExternal::kfJumpParamsTimeDeltaBlendInFactor  = 0.001f;
const f32 BehaviourGameplayExternal::kfJumpParamsTimeDeltaBlendOutFactor = 0.1f;
const f32 BehaviourGameplayExternal::kfSlideYScaleScaleUpFactor          = 0.0099999998f;
const f32 BehaviourGameplayExternal::kfJumpParamsDutchMax                = 0.2617994f;

// ============================================================================
// BehaviourGameplayExternal::UpdateJumping @0x8220EAD0 / PS3 @0x1CFFC  (.cpp:948..:1054)
//
// The jump ("air") state machine of the chase camera: while the car is genuinely airborne it
// starts and integrates a DUTCH ROLL (a left-or-right camera tilt whose direction is a coin
// flip), lets the yaw drift accumulate and eases the spring/scale tunables toward their
// in-air targets; the moment the car is not airborne it converts the time spent in the jump
// into an IMPACT SHAKE and eases everything back toward the authored parameter block.
//
// ---- how the shared-info offsets were NAMED (a chain, every link already committed) -----
//   lrInfo + 0x60 == mPlayerInfo (Behaviour.h's KEYSTONE: mPlayerInfo at console +96, and
//                    96 + sizeof(RaceCarState) lands on mTimestep at +1360)
//   +0x088 (x4, stride 0x70)  = mPlayerInfo.mRaceCarState.maWheels[i]        (WheelLite,
//                               sizeof 0x70) .mRoadContact.mbIsOnGround      (RoadContact
//                               +0x28: two Vector3s + f32 + CollisionTag)
//   +0x240 = mRaceCarState.mAboveGroundTestResult.mfVerticalDistance         (448 + 32)
//   +0x248 = mRaceCarState.mAboveGroundTestResult.mbValid                    (448 + 40)
//   +0x464 = mRaceCarState.mfTimeInAir                                       (== 1028)
//   +0x5D4 = mpRandom                                                        (== 1492)
// Every one of those five lands exactly on a NAMED member of an already-committed struct --
// no reserved span, no offset poke, and mfTimeInAir landing on the jump gate is the kind of
// agreement that cannot happen by accident.
//
// ---- the X360 / PS3 cross-check --------------------------------------------------------
// The two builds do NOT share this class's tail offsets: every member from mfSlideYScale
// onward is 0x10 LOWER on PS3 (e.g. mbJumping is X360 +0xB60 / PS3 +0xB50, mpParameters is
// X360 +0xB00 / PS3 +0xAF0). Every statement below matches under that single constant shift,
// in both directions -- which is what lets the DecFIGS LINE NUMBERS be trusted to split the
// X360's heavily-scheduled instruction stream back into statements. X360 wins on constants
// and offsets; DecFIGS wins on names and statement boundaries.
//
// ⚠️ TWO SHAPES WORTH FLAGGING, both reproduced rather than tidied:
//   * `lbHighEnough` is TRUE when the above-ground test is INVALID (.cpp:964 is
//     `!mbValid || mfVerticalDistance >= 1.0f`). Both builds agree; the console treats "no
//     ground result" as "nothing below us", which is the conservative reading for a jump.
//   * the impact-shake conversion at .cpp:1032 runs on the frame the car STOPS being
//     airborne, using the mfTimeInJump accumulated up to that frame -- and mfTimeInJump is
//     only reset when the NEXT jump starts (.cpp:984), never here.
// ============================================================================
void BehaviourGameplayExternal::UpdateJumping(const BehaviourSharedInfo& lrInfo,
                                              f32 lfTimestep,
                                              Camera& lrCamera)
{
    const BrnPhysics::Vehicle::RaceCarState& lrCarState = lrInfo.mPlayerInfo.mRaceCarState;

    // .cpp:955/:957 -- X360 unrolls this to four `lbz`/`cmplwi` pairs at +0x88 stride 0x70;
    // PS3 keeps the `mtctr 4` loop. Same four bytes either way.
    bool lbAllWheelsOffGround = true;
    for (s32 liWheel = 0; liWheel < 4; ++liWheel)
    {
        lbAllWheelsOffGround = lbAllWheelsOffGround
                            && !lrCarState.maWheels[liWheel].mRoadContact.mbIsOnGround;
    }

    // .cpp:964 -- flt_82001C98 == 1.0f (one metre of clearance).
    const bool lbHighEnough = !lrCarState.mAboveGroundTestResult.mbValid
                            || lrCarState.mAboveGroundTestResult.mfVerticalDistance >= 1.0f;

    // .cpp:970 -- flt_82001CC0 == 0.0f. The three tests are evaluated in this order on X360.
    if (lrCarState.mfTimeInAir > 0.0f && lbHighEnough && lbAllWheelsOffGround)
    {
        // .cpp:972
        if (!mbJumping)
        {
            mbJumping = true;                                            // .cpp:974

            // .cpp:975 -- the console INLINES CgsNumeric::Random::RandomBool here (the LCG
            // step + the old seed's bit 32); see CgsRandom.h for that block's attestation.
            // KF_DEGS_TO_RADS is flt_82001744 == +0.017453292 on one arm and flt_82006D74 ==
            // -0.017453292 on the other -- i.e. the coin flip picks the SIGN of the roll.
            if (lrInfo.mpRandom->RandomBool())
            {
                mfDutchVelocity = kfJumpParamsDutchInitialVelocity * -0.017453292f; // .cpp:977
            }
            else
            {
                mfDutchVelocity = kfJumpParamsDutchInitialVelocity *  0.017453292f; // .cpp:981
            }

            mfTimeInJump  = 0.0f;                                        // .cpp:984
            mfYawVelocity = 0.0f;                                        // .cpp:986
        }
        else
        {
            mfDutchDrift += mfDutchVelocity * lfTimestep;                // .cpp:990

            if (mfDutchDrift > kfJumpParamsDutchMax)                     // .cpp:991
            {
                // .cpp:993 -- X360 `fneg f12, f0` then `fmadds f0, f12, rate, f0`, i.e. the
                // same blend-toward-zero shape the tail below uses everywhere.
                mfDutchVelocity += (0.0f - mfDutchVelocity) * kfJumpParamsDutchCooloffRate;
            }

            mfYawDrift   += mfYawVelocity * lfTimestep;                  // .cpp:996
            mfTimeInJump += lfTimestep;                                  // .cpp:997
        }

        // .cpp:1000 -- dead in this arm on both builds (we are only here because
        // mfTimeInAir > 0), and reproduced anyway: the console really does re-test it.
        if (lrCarState.mfTimeInAir <= 0.0f)
        {
            lrCamera.SetRequestedTimeDilation(1.0f);
        }

        // ---- ease the tunables toward their IN-AIR targets ----------------------------
        // .cpp:1010 -- note the DIFFERENT (much slower) rate for the time delta.
        mfTimeDelta        += (kfJumpParamsTimeDelta - mfTimeDelta)
                            * kfJumpParamsTimeDeltaBlendInFactor;
        // .cpp:1011..:1013 -- the three literal targets are flt_82001C98 / flt_82004744 /
        // flt_82002138; the PS3 shows all three as unnamed dwords, so they are literals.
        mfPitchCoefficient += (1.0f  - mfPitchCoefficient) * kfJumpParamsBlendFactor;
        mfPitchSpring      += (0.2f  - mfPitchSpring)      * kfJumpParamsBlendFactor;
        mfYawSpring        += (0.01f - mfYawSpring)        * kfJumpParamsBlendFactor;

        // .cpp:1015 -- flt_82006D70 == -2.0f (also an unnamed literal on PS3). Recall
        // Parameters::Set forces mfSlideYScaleJump to -1.0f as its final store, so the
        // target here is +2.0 for every authored car.
        mfSlideYScale      += ((mpParameters->mfSlideYScaleJump * -2.0f) - mfSlideYScale)
                            * kfSlideYScaleScaleUpFactor;

        mfWobbleScale = 0.0f;                                            // .cpp:1017
        return;
    }

    // ---- NOT airborne ------------------------------------------------------------------
    // .cpp:1024
    if (mbJumping)
    {
        // .cpp:1032 -- both `fsel`s are the console's Min/Max idiom (scalar.h:155/:222 in the
        // PS3 attribution). flt_82001D9C == 2.0f, flt_82004270 == 3.0f, both unnamed on PS3.
        const f32 lfImpactForce = ((mfTimeInJump > 2.0f) ? 2.0f : mfTimeInJump) * 3.0f;

        // .cpp:1037
        mfImpactShakeFactor = (mfImpactShakeFactor > lfImpactForce) ? mfImpactShakeFactor
                                                                    : lfImpactForce;
    }

    mbJumping = false;                                                   // .cpp:1040

    // ---- ease everything back toward the AUTHORED parameter block ----------------------
    mfDutchDrift += (0.0f - mfDutchDrift) * kfJumpParamsDutchBlendFactor;        // .cpp:1041
    mfYawDrift    = 0.0f;                                                        // .cpp:1042
    mfTimeDelta  += (0.0f - mfTimeDelta) * kfJumpParamsTimeDeltaBlendOutFactor;  // .cpp:1044

    mfPitchCoefficient += (mpParameters->mrPitchCoeff  - mfPitchCoefficient)     // .cpp:1045
                        * kfJumpParamsBlendFactor;
    mfPitchSpring      += (mpParameters->mrPitchSpring - mfPitchSpring)          // .cpp:1046
                        * kfJumpParamsBlendFactor;

    // .cpp:1051 -- the yaw spring's target is the DRIFT-blended spring: Lerp(mrYawSpring,
    // mfDriftYawSpring, mfDriftScale). X360 `fmadds f0, f12, f6, f0` with
    // f12 == mfDriftYawSpring - mrYawSpring, f6 == mfDriftScale, f0 == mrYawSpring.
    mfYawSpring   += (((mpParameters->mfDriftYawSpring - mpParameters->mrYawSpring)
                        * mfDriftScale + mpParameters->mrYawSpring) - mfYawSpring)
                   * kfJumpParamsBlendFactor;

    mfSlideYScale += (mpParameters->mrSlideYScale - mfSlideYScale)               // .cpp:1053
                   * kfJumpParamsBlendFactor;
    mfWobbleScale += (0.0f - mfWobbleScale) * kfJumpParamsBlendFactor;           // .cpp:1054

    // The unconditional tail store both builds end on (X360 0x8220EDFC / PS3 0x1D134).
    lrCamera.SetRequestedTimeDilation(1.0f);
}

// ============================================================================
// CalculateCameraTransform @0x8220E838 / PS3 @0x276FC  (.cpp:813..:832)
// helper 6/8 -- BODIED 2026-08-02 (rotate-helper wave).
//
// THE KEYSTONE OF THE CHASE RIG: given the car's frame and a pile of per-frame angles and
// offsets, this is the function that produces the camera's world transform. Six statements.
// Its ONLY callee is Utils::RotateMatrix44AffineByEulerAnglesZXY, which it calls twice, and
// which was declaration-only until this same wave -- that is the entire reason this helper
// sat unwritten while the three scalar ones went in.
//
// ⭐ THE SHAPE IS A TWO-JOINT ARM, and reading it that way is what makes every store obvious:
//     seat the frame pitched DOWN by the authored down-angle, put the pivot arm on it,
//     swing the whole thing by the first angles, extend by the camera arm,
//     swing again by the second angles, and finally translate onto the car.
//   ⚠️⚠️ IT ONLY WORKS BECAUSE RotateMatrix44AffineByEulerAnglesZXY ROTATES THE TRANSLATION
//   ROW TOO. That is documented at that function's own definition as a deliberate
//   preservation of console behaviour, and THIS is the caller that proves it is not a slip:
//   both arms are stored into wAxis BEFORE a rotate and are expected to swing with it. If
//   that helper is ever "fixed" to leave the position alone, this camera collapses onto the
//   car's origin and nothing else in the tree will report it.
//
// STATEMENT MAP -- DecFIGS line numbers, X360 addresses:
//   .cpp:817  lfDownAngleRads = mpParameters->mfDownAngle * 0.017453292f
//             (flt_82001744, DUMPED as 0.01745329238 -- read at 0x8220E880/0x8220E8AC from
//             mpParameters at this+0xB00, member +0x94), then ONE inlined SinCos of it and
//             the three rotation rows of an X-axis rotation by that angle -- the SDK's
//             MakeRotationX, which DecFIGS tags matrix44affine_operation_platform_inline.h
//             :239/:240 and which is the ONLY rotation builder this function inlines.
//             ⭐ THE SIGN LANE ASSIGNMENT, which the header's decode carried as INFERRED, is
//             now VERIFIED: row1 = {0, cos, sin} and row2 = {0, -sin, cos}. Settled three
//             ways -- (a) at 0x8220EA00..0x8220EA08 the console negates the SIN chain (the
//             chain seeded from x^3 at 0x8220E900) and vrlimi128s it into row2's y lane,
//             while the COS chain (seeded from 1.0f at 0x8220E9D4) lands unnegated in both
//             diagonal slots; (b) it is byte-for-byte the same packing the newly bodied
//             RotateMatrix44AffineByEulerAnglesZXY emits for its own X matrix; (c) it
//             reproduces the hand-de-inlined XMMatrixRotationX that
//             BrnBehaviourRotateAboutVehicle.cpp has carried, independently, since 2026-07.
//             ⚠️ PS3 writes all four rows here (row3 = 0, matrix44affine_type_inline.h:28..31
//             at 0x27928..0x27940) and then overwrites row3 at :819; the X360 compiler elided
//             the dead zero-store. Same source, two schedules.
//   .cpp:819  lrCameraMatrix.wAxis = lPivotOffset * lCarScale
//             (X360 `vmulfp128 v8, v3, v127` at 0x8220EA3C; PS3 hoists the identical
//             component-wise Mult to 0x277C8, tagged vector3_operation_inline.h:105 -- the
//             SDK's Mult(Vector3, Vector3), which it emits as `vmaddfp v4, v4, 0, v3`.)
//   .cpp:822  Utils::RotateMatrix44AffineByEulerAnglesZXY(lrCameraMatrix, lFirstRotationAngles)
//             (PS3 0x277F0 `vmr v2, v5` -- the FOURTH Vector3 -- tagged :822.)
//   .cpp:82x  lrCameraMatrix.wAxis += lCameraOffset * lCarScale
//             ⚠️ NO OWN-LINE ATTESTATION: every instruction of this statement is attributed
//             to the inlined vector3_operation_inline.h (:105 for the Mult, :693 for the
//             add), so DWARF never names its .cpp line. It is bracketed between :822 and
//             :828 and that is all that is attested. (X360 folds it to one
//             `vmaddfp128 v0, v125, v127, v0` at 0x8220EA88; PS3 keeps the Mult hoisted at
//             0x277D8 and adds at 0x2795C. Both compute offset*scale + wAxis.)
//   .cpp:828  Utils::RotateMatrix44AffineByEulerAnglesZXY(lrCameraMatrix, lSecondRotationAngles)
//             (PS3 0x27954 `vmr v2, v22`, v22 being the FIRST Vector3, tagged :828.)
//   .cpp:832  lrCameraMatrix.wAxis += lCarMatrix.wAxis
//             (X360 0x8220EAA0 loads r29+0x30, the by-value car matrix's translation row.)
//
// ARGUMENT MAP -- how the six Vector3s were separated, in BOTH builds (X360 passes vector
// arguments from v1, PS3 from v2; the two agreeing member for member IS the cross-check):
//     #1 -> the SECOND rotate's angles      (X360 v1  saved to v126 @0x8220E874)
//     #2 -> the component-wise multiplier   (X360 v2  saved to v127 @0x8220E870)
//     #3 -> the pivot arm                   (X360 v3, multiplied by #2 @0x8220EA3C)
//     #4 -> the FIRST rotate's angles       (X360 v4, moved to v1 @0x8220E9F8)
//     #5 -> DEAD                            (X360 clobbers v5 @0x8220E8CC before any read)
//     #6 -> the camera arm                  (X360 v6  saved to v125 @0x8220E868)
// ⚠️ THE NAMES BELOW ARE DESCRIPTIONS, NOT ATTESTATIONS. DWARF records no parameter names for
//   this function; the only one with outside support is lCarScale, which is what the DWARF
//   calls the caller's own stack slot for argument #2. Everything else is named for what the
//   asm demonstrably does with it, and should be re-read that way rather than trusted.
// ⚠️ lfSpeedMPH / lfTimestep are DEAD here even though Update genuinely passes them
//   (PS3 0x6AC70 `lfs f1, 0x42C(lrSharedInfo)` and 0x6AC78). Kept in the signature because
//   the DWARF declares them.
// ============================================================================
void BehaviourGameplayExternal::CalculateCameraTransform(const Parameters& /*lrCameraAttribs*/,
                                                         Matrix44Affine& lrCameraMatrix,
                                                         Matrix44Affine lCarMatrix,
                                                         Vector3 lSecondRotationAngles,
                                                         Vector3 lCarScale,
                                                         Vector3 lPivotOffset,
                                                         Vector3 lFirstRotationAngles,
                                                         Vector3 /*lUnusedVector*/,
                                                         Vector3 lCameraOffset,
                                                         f32 /*lfSpeedMPH*/,
                                                         f32 /*lfTimestep*/)
{
    const f32 KF_DEGS_TO_RADS = 0.017453292f;   // flt_82001744, dumped

    // .cpp:817 -- seat the frame as a pure rotation about X by the authored down-angle.
    const f32 lfDownAngleRads = mpParameters->mfDownAngle * KF_DEGS_TO_RADS;
    lrCameraMatrix = rw::math::vpu::MakeRotationX(lfDownAngleRads);

    // .cpp:819 -- the pivot arm, scaled.
    lrCameraMatrix.wAxis = rw::math::vpu::Mult(lPivotOffset, lCarScale);

    // .cpp:822 -- swing the whole frame (arm included) by the first angles.
    Utils::RotateMatrix44AffineByEulerAnglesZXY(lrCameraMatrix, lFirstRotationAngles);

    // .cpp:82x -- extend by the camera arm, scaled.
    lrCameraMatrix.wAxis = lrCameraMatrix.wAxis + rw::math::vpu::Mult(lCameraOffset, lCarScale);

    // .cpp:828 -- swing again by the second angles.
    Utils::RotateMatrix44AffineByEulerAnglesZXY(lrCameraMatrix, lSecondRotationAngles);

    // .cpp:832 -- and place the result on the car.
    lrCameraMatrix.wAxis = lrCameraMatrix.wAxis + lCarMatrix.wAxis;
}

// ============================================================================
// ApplySlideyEffects @0x822260A8 / PS3 @0x59E18  (.cpp:841..:940)
// helper 7/8 -- BODIED 2026-08-02 (final-helpers wave).
//
// THE SLIDE / DRIFT RESPONSE. Everything that makes the chase camera feel like it is being
// dragged behind a car rather than bolted to one lives here: take the car's implicit
// velocity, express it in the CAR's own axes, shape each axis through a saturating
// tend-to-limit curve, smooth the fore/aft term, and push the camera along the result --
// then swing that push by the player's free-look yaw and pitch.
//
// 434 X360 asm lines, ~24 statements. It looks bigger than it is: ~120 lines are the two
// IsValid tripwires and the Timestep::Get assert, and another ~120 are two inlined SinCos
// expansions plus four inlined vector transforms.
//
// ⚠️⚠️ FIVE EXTERNAL SYMBOLS HAD TO BE BODIED FIRST AND NONE OF THEM EXISTED IN THE TREE --
//   Utils::PositiveValueTendToLimit, Utils::TendToLimits, Utils::SineLerp (all three now in
//   Camera/Utils/CameraUtils.cpp) and the rotation controller's two RADIANS accessors. Found
//   by grepping for a DEFINITION, not a declaration. Two of them are shapes that would have
//   compiled and linked wrong rather than failed: TendToLimits has NO X360 SYMBOL AT ALL (it
//   is inlined into both call sites here, so its five arguments had to be separated out of
//   THIS function's asm), and the two accessors carry a LOOKBACK special case that reading
//   them as "the Degs accessor times pi/180" would have discarded without any diagnostic.
//
// STATEMENT MAP -- DecFIGS line numbers, X360 addresses:
//   :844/:846/:847  first frame only: seed mLastCarPos from the car and latch the flag
//                   (X360 0x822260EC reads this+0xB5C, 0x82226108 stores this+0xAE0).
//   :856  lDisplacement = the tracker's implicit velocity IN CAR SPACE.
//         ⭐ THE CONSOLE SPELLS THIS AS A 3x3 TRANSPOSE: six vmrghw/vmrglw at
//         0x82226144..0x82226170 build the car matrix's three COLUMNS, and the vmulfp +
//         two vmaddfp cascade at 0x82226190 then computes column0*v.x + column1*v.y +
//         column2*v.z -- which is Dot(row_i, v) per lane, i.e. the INVERSE rotation, not
//         TransformVector. Written below as the three dots it is; folding it into
//         TransformVector would rotate the wrong way and still look plausible.
//         (The zero vector fed as the transpose's fourth row is what makes the w lane 0.)
//   :857  CGS_ASSERT(IsValid(lDisplacement))                       X360 line 0x3A3 == 931
//   :859  mLastDisplacement = lDisplacement                        (this+0xAF0)
//   :862  lDisplacement.y = <a SECOND GetImplicitVelocity call>.y
//         ⚠️ The console really does call the tracker twice (0x82226174 and 0x82226244) and
//         keeps only lane Y of the second (`vrlimi128 v0, v13, 4, 0`, mask 4 == lane Y). So
//         Y is WORLD-space vertical velocity while X and Z stay car-space, and
//         mLastDisplacement keeps the pure car-space triple from before the swap.
//   :863  CGS_ASSERT(IsValid(lDisplacement))                       X360 line 0x3A9 == 937
//   :869  lDisplacement.y = TendToLimits(y, 30, mfSlideYScale, 15, 0)
//         X360 0x822262E4..0x82226314 is TendToLimits inlined: `blt` on the sign, `fneg`,
//         and the two (halfway, limit) pairs -- 30.0f/mfSlideYScale for the negative side,
//         15.0f/0.0f for the positive. ⇒ upward motion contributes NOTHING (posLimit 0) and
//         only downward motion raises the camera.
//   :873  lDisplacement.z = GetTimestep(GetTimestepType()) * (speedMPH - STATIC_last_mph)
//         i.e. a raw per-frame acceleration proxy. The Timestep::Get assert
//         ("leType > E_TIMESTEP_INVALID && leType < E_TIMESTEP_MAX") is open-coded at
//         0x82226328..0x8222637C, exactly as in ApplyJumpEffects.
//   :87x  lDisplacement = Mult(lDisplacement, {mrSlideXScale, 1, mrSlideZScale})
//         ⚠️ NO OWN-LINE ATTESTATION (one `vmulfp128` at 0x822263E0 against a vector the
//         compiler HOISTED to the function head, 0x8222612C..0x82226160). Bracketed
//         between :873 and :885, and that is all that is attested.
//   :885  lDisplacement.z = TendToLimits(z, mrSlideZInputForHalf, +1,
//                                           mrSlideZInputForHalf, -1)
//         (second inlined expansion, 0x822263EC..0x8222640C; both halfway args are the same
//         tunable, only the limits' signs differ -- accelerating pushes the camera back,
//         braking pulls it in.)
//   :888  mfDesiredZDisplacement += (z - mfDesiredZDisplacement) * mfAccelZLerpAmount
//         and lDisplacement.z becomes that smoothed value.
//   :894  if (mfAbsDriftScale > 0.1f)                              (shared info +0x45C)
//   :896      mOverrideScale = 0
//   :898  else if (speedMPH > mpParameters->mfZAndTiltCutoffSpeedMPH)
//   :900      mOverrideScale += (STATIC_override_max_speed - mOverrideScale)
//                             * STATIC_override_scale_lerp
//         else
//   :904      mOverrideScale += (1 - mOverrideScale) * STATIC_override_scale_lerp
//   :909  if (speedMPH > mpParameters->mfZAndTiltCutoffSpeedMPH) lDisplacement.z = 0
//   :919  mfSmoothedZDisplacement += (z - mfSmoothedZDisplacement)
//                                   * (SineLerp(STATIC_MinLerp, 1, Abs(z) * mOverrideScale)
//                                      * mfZLerpAmount)
//         (the Abs is the console's `vspltisw -1` / `vslw` sign-mask + `vandc` at
//         0x82226548..0x82226574, not a call.)
//   :922  STATIC_last_mph = speedMPH
//   :927  lDisplacement.z  = mrSlideZOutputMax * mfSmoothedZDisplacement
//   :930  lDisplacement.z += mfZDistanceScale * mrSlideZOutputMax
//   :934  push = TransformVector(MakeRotationX(pitchRads),
//                                TransformVector(lrCameraMatrix, lDisplacement))
//   :935  lrCameraMatrix.wAxis += TransformVector(MakeRotationY(yawRads), push)
//   :939  mLastCarPos = lCarMatrix.wAxis
//
// ⚠️ THE FOUR `STATIC_*` NAMES ARE THE DWARF'S OWN -- they are FUNCTION-LOCAL STATICS and
//   the PS3 export carries their full mangled `_ZZ...E<name>` symbols. Their X360 storage is
//   three consecutive .data floats at 0x82CDAD24/28/2C plus one BSS float at 0x82FAAD24, and
//   the name-to-address mapping is fixed twice over: the PS3 block is consecutive in the same
//   declaration order (0x10206C4C/50/54), and each one's ROLE matches at its use site (the
//   SineLerp argument is MinLerp on both builds, the over-cutoff target is override_max_speed
//   on both, the blend rate is override_scale_lerp on both).
// ⚠️⚠️ STATIC_last_mph IS MUTABLE AND PER-CLASS, NOT PER-INSTANCE -- both shared gameplay
//   cameras write it, exactly like the function-local `mLastCameraAngles` in Update. Making
//   it a member would change behaviour whenever two of these run in one frame.
// ⚠️ STATIC_override_max_speed is 0.01f despite its name (it is a SCALE target, not a speed).
//   Transcribed as the image has it; the name is the DWARF's, the value is the image's, and
//   they simply disagree.
// ============================================================================
namespace
{
    // DecFIGS-NAMED function-local statics; X360-VALUED (flt_82CDAD24/28/2C, all 0.01f).
    // Kept at file scope rather than inside the body only so the banner above can document
    // them in one place -- they are `static` in the console source and have no other consumer.
    const f32 KF_SLIDEY_STATIC_MIN_LERP             = 0.01f;   // STATIC_MinLerp
    const f32 KF_SLIDEY_STATIC_OVERRIDE_MAX_SPEED   = 0.01f;   // STATIC_override_max_speed
    const f32 KF_SLIDEY_STATIC_OVERRIDE_SCALE_LERP  = 0.01f;   // STATIC_override_scale_lerp
}

void BehaviourGameplayExternal::ApplySlideyEffects(const Parameters& lrCameraAttribs,
                                                   Matrix44Affine& lrCameraMatrix,
                                                   Matrix44Affine lCarMatrix,
                                                   const BehaviourSharedInfo& lrSharedInfo)
{
    // The one MUTABLE console static: last frame's speed, shared by both gameplay cameras.
    static f32 STATIC_last_mph = 0.0f;                         // X360 flt_82FAAD24 (BSS)

    // .cpp:844..:847
    if (!mbLastCarPosInitialised)
    {
        mLastCarPos = lCarMatrix.wAxis;
        mbLastCarPosInitialised = true;
    }

    const f32 lfSpeedMPH = lrSharedInfo.mPlayerInfo.mRaceCarState.mfSpeedMPH;   // info +0x42C

    // .cpp:856 -- the car's implicit velocity in the CAR's own axes (the console's inlined
    // 3x3 transpose; see the banner -- these three dots ARE that transpose's cascade).
    const Vector3 lVelocity = lrSharedInfo.mpPlayerTracker->GetImplicitVelocity();
    Vector3 lDisplacement;
    lDisplacement.x = rw::math::vpu::Dot(lCarMatrix.xAxis, lVelocity);
    lDisplacement.y = rw::math::vpu::Dot(lCarMatrix.yAxis, lVelocity);
    lDisplacement.z = rw::math::vpu::Dot(lCarMatrix.zAxis, lVelocity);
    lDisplacement.w = 0.0f;
    CGS_ASSERT(rw::math::vpu::IsValid(lDisplacement), "IsValid(lDisplacement)");  // .cpp:857

    mLastDisplacement = lDisplacement;                                           // .cpp:859

    // .cpp:862 -- lane Y only, and from a SECOND call: world-space vertical velocity.
    lDisplacement.y = lrSharedInfo.mpPlayerTracker->GetImplicitVelocity().y;
    CGS_ASSERT(rw::math::vpu::IsValid(lDisplacement), "IsValid(lDisplacement)");  // .cpp:863

    // .cpp:869 -- flt_82004F5C == 30.0f, flt_820047C4 == 15.0f, both dumped.
    lDisplacement.y = Utils::TendToLimits(lDisplacement.y,
                                          30.0f, mfSlideYScale,
                                          15.0f, 0.0f);

    // .cpp:873 -- the fore/aft term starts life as an acceleration proxy.
    lDisplacement.z = lrSharedInfo.GetTimestep(GetTimestepType())
                    * (lfSpeedMPH - STATIC_last_mph);

    // .cpp:87x (no own line) -- the authored per-axis slide scales, Y left alone.
    lDisplacement = rw::math::vpu::Mult(lDisplacement,
                                        Vector3{ lrCameraAttribs.mrSlideXScale,
                                                 1.0f,
                                                 lrCameraAttribs.mrSlideZScale,
                                                 0.0f });

    // .cpp:885
    lDisplacement.z = Utils::TendToLimits(lDisplacement.z,
                                          lrCameraAttribs.mrSlideZInputForHalf,  1.0f,
                                          lrCameraAttribs.mrSlideZInputForHalf, -1.0f);

    // .cpp:888
    mfDesiredZDisplacement += (lDisplacement.z - mfDesiredZDisplacement)
                            * lrCameraAttribs.mfAccelZLerpAmount;
    lDisplacement.z = mfDesiredZDisplacement;

    // .cpp:894..:904 -- flt_82004014 == 0.1f. NOTE mpParameters, not lrCameraAttribs.
    if (lrSharedInfo.mPlayerInfo.mRaceCarState.mfAbsDriftScale > 0.1f)
    {
        mOverrideScale = 0.0f;                                                   // .cpp:896
    }
    else if (lfSpeedMPH > mpParameters->mfZAndTiltCutoffSpeedMPH)
    {
        mOverrideScale += (KF_SLIDEY_STATIC_OVERRIDE_MAX_SPEED - mOverrideScale) // .cpp:900
                        * KF_SLIDEY_STATIC_OVERRIDE_SCALE_LERP;
    }
    else
    {
        mOverrideScale += (1.0f - mOverrideScale)                                // .cpp:904
                        * KF_SLIDEY_STATIC_OVERRIDE_SCALE_LERP;
    }

    // .cpp:909 -- above the cutoff speed the fore/aft slide is switched off entirely.
    if (lfSpeedMPH > mpParameters->mfZAndTiltCutoffSpeedMPH)
    {
        lDisplacement.z = 0.0f;
    }

    // .cpp:919
    mfSmoothedZDisplacement += (lDisplacement.z - mfSmoothedZDisplacement)
        * (Utils::SineLerp(KF_SLIDEY_STATIC_MIN_LERP, 1.0f,
                           rw::math::fpu::Abs(lDisplacement.z) * mOverrideScale)
           * lrCameraAttribs.mfZLerpAmount);

    STATIC_last_mph = lfSpeedMPH;                                                // .cpp:922

    // .cpp:927 / :930
    lDisplacement.z  = lrCameraAttribs.mrSlideZOutputMax * mfSmoothedZDisplacement;
    lDisplacement.z += lrCameraAttribs.mfZDistanceScale * lrCameraAttribs.mrSlideZOutputMax;

    // .cpp:934 / :935 -- into camera space, then swung by the free-look pitch and yaw.
    Vector3 lPush = rw::math::vpu::TransformVector(lrCameraMatrix, lDisplacement);
    lPush = rw::math::vpu::TransformVector(
                rw::math::vpu::MakeRotationX(mRotationController.GetPitchRotationAngleRads()),
                lPush);
    lPush = rw::math::vpu::TransformVector(
                rw::math::vpu::MakeRotationY(mRotationController.GetYawRotationAngleRads()),
                lPush);

    lrCameraMatrix.wAxis = lrCameraMatrix.wAxis + lPush;

    mLastCarPos = lCarMatrix.wAxis;                                              // .cpp:939
}

// ============================================================================
// ApplyJumpEffects @0x822250C0 / PS3 @0x598AC  (.cpp:607..:615)
// helper 8/8 -- BODIED 2026-08-02 (rotate-helper wave).
//
// 303 X360 asm lines that are THREE statements: ~250 of those lines are two inlined SinCos
// expansions and two inlined 4x4 affine products, and the rest is the shake call's argument
// setup. It was blocked on nothing but the rotation vocabulary.
//
// ⭐⭐ THE MULTIPLY IS THE OTHER WAY ROUND FROM CalculateCameraTransform'S, AND THAT IS THE
//   WHOLE POINT OF THE FUNCTION. Here the rotation is on the LEFT --
//   `Mult(rotation, cameraTransform)` -- so the camera spins about ITS OWN axes and its
//   position does not move. Verified from the store cascade: the new row_i is
//   `rot.row_i.x*cam.xAxis + rot.row_i.y*cam.yAxis + rot.row_i.z*cam.zAxis`, and the
//   translation row comes out as `0*cam.xAxis + 0*cam.yAxis + 0*cam.zAxis + cam.wAxis`,
//   i.e. UNCHANGED (X360 0x82225328..0x82225338 and again 0x82225500..0x82225518). Getting
//   this backwards would fling the camera around the world origin every frame the car drifts.
//
// STATEMENT MAP:
//   (a) yaw:   Mult(MakeRotationY(mfYawDrift),   transform)
//       X360 reads this+0xB28 (mfYawDrift) at 0x822250F0, SinCos, packs
//       row0 = (cos, 0, -sin) / row1 = (0,1,0) / row2 = (sin, 0, cos) / row3 = 0
//       at 0x82225288..0x822252CC, multiplies and stores all four camera rows.
//   (b) dutch: Mult(MakeRotationZ(mfDutchDrift), transform)
//       X360 reads this+0xB20 (mfDutchDrift) at 0x82225340, SinCos, packs
//       row0 = (cos, sin, 0) / row1 = (-sin, cos, 0) / row2 = (0,0,1) / row3 = 0
//       at 0x82225488..0x822254D4, multiplies and stores again.
//   ⭐ WHICH AXIS IS WHICH IS NOT READ OFF THE PACKING ALONE. DecFIGS tags this function's
//     inlined rotation builders matrix44affine_operation_platform_inline.h :253 and :269 and
//     NOTHING ELSE -- exactly the two lines that CalculateCameraTransform (X only, :239/:240)
//     does not use and that RotateMatrix44AffineByEulerAnglesZXY (all three) does. Three
//     functions, one consistent assignment; see MakeRotationX's note in that vendor header.
//   (c) .cpp:614  mAirShake.Update(transform, mpParameters->mAirShakeParams, *mpRandom,
//                                  lrSharedInfo.GetTimestep(GetTimestepType()), mfWobbleScale)
//       Every argument is offset-pinned in both builds:
//         this+0x2A0  -> mAirShake            (X360 0x8222556C `addi r3, r30, 0x2A0`)
//         mpParameters+0x0C -> mAirShakeParams (0x82225568 `addi r5, r11, 0xC`) -- and it is
//              the AIR params that pair with the AIR shake, which is the check that the
//              +0x0C/+0x1C pair in the Parameters block is the right way round
//         lrSharedInfo+0x5D4 -> mpRandom       (0x82225564)
//         lrSharedInfo+0x580 + leType*4 -> the timestep array; the console open-codes
//              Timestep::Get including its `leType > E_TIMESTEP_INVALID && leType <
//              E_TIMESTEP_MAX` assert (0x82225520..0x8222554C), which is why this is spelled
//              as the accessor rather than as a raw index
//         this+0xB40 -> mfWobbleScale          (0x8222555C; PS3 reads its own +0xB30, the
//              constant -0x10 tail shift between the two builds)
//
// ⚠️⚠️ AND THIS IS THE CALL THAT MADE THE SHAKE STUB URGENT. `Utils::CameraShake::Update`
//   had an empty `{}` in DirectorLinkStubs.cpp; the line above would have LINKED AND DONE
//   NOTHING, invisibly. That stub is retired in this same commit (BrnCameraShake.cpp is now
//   mounted) precisely so that this body is not the thing that quietly re-arms it.
//
// ⚠️ The first parameter is a `Camera&` and is handed straight to CameraShake::Update as a
//   `Matrix44Affine&` (DecFIGS names the callee's parameter `lMatrixInOut`). That is legal
//   because Camera::mTransform is at +0x00 -- there is a static_assert on exactly that in
//   Camera.h -- and it is spelled here as the member it actually is.
// ⚠️ PARAMETER NAMES: `lCameraInOut` and `lrSharedInfo` ARE the DWARF's own (the PS3 export
//   carries them on r4/r5). Not invented.
// ============================================================================
void BehaviourGameplayExternal::ApplyJumpEffects(Camera& lCameraInOut,
                                                 const BehaviourSharedInfo& lrSharedInfo)
{
    lCameraInOut.mTransform = rw::math::vpu::Mult(rw::math::vpu::MakeRotationY(mfYawDrift),
                                                  lCameraInOut.mTransform);

    lCameraInOut.mTransform = rw::math::vpu::Mult(rw::math::vpu::MakeRotationZ(mfDutchDrift),
                                                  lCameraInOut.mTransform);

    mAirShake.Update(lCameraInOut.mTransform,                                  // .cpp:614
                     mpParameters->mAirShakeParams,
                     *lrSharedInfo.mpRandom,
                     lrSharedInfo.GetTimestep(GetTimestepType()),
                     mfWobbleScale);
}

// ============================================================================
// BrnDirector::Camera::BehaviourGameplayExternal::UpdateLooking @0x82225630
//   (DWARF h:325 / BehaviourGameplayExternal.cpp:675; PS3 @0x3A870)  -- helper 5/8, the last.
//
// THE FREE-LOOK RESPONSE. Update hands it the requested rotation, the pivot the chase rig
// orbits, the camera's car-space offset and the car's own bounding box; it folds the look
// stick into the rotation and then re-shapes the pivot four ways -- centering, the car's
// width/length aspect, a pull-in when the player looks backwards, and a vertical drop --
// before publishing an FOV adjustment through its in/out float.
//
// ⚠️⚠️ THE X360 SOURCE LINE NUMBERS ARE DecFIGS + 74. Pinned by three asserts whose
//   condition and streamed value are identical on both builds: X360 0x2EF/751 == DecFIGS
//   :677, 0x2F7/759 == :685, 0x302/770 == :696. Every line quoted below is the DecFIGS one
//   (the numbering the rest of this file already uses); the X360 line is given with each
//   assert so the mapping stays checkable.
//
// STATEMENT MAP -- all 32 of the DecFIGS own-line set are accounted for:
//   :677  CGS_ASSERT(IsValid(lRotation))                          X360 line 0x2EF == 751
//         (the console builds the message by STREAMING lRotation into the assert buffer --
//          `bl sub_82203F70`, the Vector3 stream operator; there is no literal text.)
//   :679  if (mRotationController.IsLookback())            X360 0x82225744 `lbz this+0x40`
//   :681/:683  lRotation.y += GetYawRotationAngleRads();
//              lRotation.x += GetPitchRotationAngleRads();
//         (`vrlimi128 v0,v13,4,0` == lane Y then `...,8,0` == lane X, 0x822257B8/0x82225814.)
//   :685  CGS_ASSERT(IsValid(lRotation))                          X360 line 0x2F7 == 759
//   :688  mfCenteringFactor = 1.0f                       (flt_82001C98, this+0xB04)
//   :692/:694  THE SAME TWO ADDS AGAIN, in the else arm
//   :696  CGS_ASSERT(IsValid(lRotation))                          X360 line 0x302 == 770
//   :701  mfCenteringFactor += (1.0f - mfCenteringFactor) * 0.2f  when the controller is
//         rotated or in lookback           (flt_82004744 == 0.2f, DUMPED)
//   :705  mfCenteringFactor += (0.0f - mfCenteringFactor) * 0.2f  otherwise
//   :710  lPivot += lCarSpaceOffset * mfCenteringFactor
//         X360 0x82225AC0 `vmaddfp v0, v0, v13, v12` -- SEMANTICS vD = vA*vC + vB, so
//         lCarSpaceOffset * broadcast(centering) + lPivot. (See the header's operand-order
//         note; the field order IDA prints is NOT the operand order.)
//   :711  CGS_ASSERT(!IsZero(lPivot))                             X360 line 0x311 == 785
//   :713  lCarSpaceOffset *= (1.0f - mfCenteringFactor)
//   :~715 lLookDirection = Vector2(Sin(yawRads), Cos(yawRads))
//   :~717 lfLength = lrAABBox.mMax.z - lrAABBox.mMin.z
//   :~718 lfWidth  = lrAABBox.mMax.x - lrAABBox.mMin.x
//   :721  CGS_ASSERT(!IsZero(lfLength))                           X360 line 0x31B == 795
//   :722  CGS_ASSERT(!IsZero(lfWidth))                            X360 line 0x31C == 796
//   :723  lfWidth /= lfLength            (it is a RATIO from here on)
//   :~72x lrSideScaleAmount = Abs(Dot(lLookDirection, Vector2(1,0)))   == |sin(yaw)|
//   :731  CGS_ASSERT(!IsZero(lPivot))                             X360 line 0x325 == 805
//   :732  lPivot *= 1.0f + (lfWidth - 1.0f) * lrSideScaleAmount * 0.25f   (flt_82003F40)
//   :733  CGS_ASSERT(!IsZero(lPivot))                             X360 line 0x327 == 807
//   :~74x lrDotBackwards = Clamp(Dot(lLookDirection, Vector2(0,-1)), 0, 1)  == clamped -cos
//   :745  CGS_ASSERT(!IsZero(lPivot))                             X360 line 0x333 == 819
//   :749  lfPivotLength = Magnitude(lPivot); then SET the pivot's length to
//           lfPivotLength - (lfPivotLength - lfLength*0.5f) * mfFrontInAmount * lrDotBackwards
//         by scaling lPivot by that over lfPivotLength.
//   :753  lfDesiredDropAmount = (lrDotBackwards > 0.0f) ? 1.0f : lrSideScaleAmount
//   :756  if (IsLookback()) lfDesiredDropAmount = 0.0f
//   :761/:764/:772  latch-or-tend mfDropAmount at 0.09f  (flt_820086A4)
//   :776  lPivot.y -= lPivot.y * mpParameters->mfDropFactor * mfDropAmount
//   :780  lfDesiredFOVAdjustment = mpParameters->mfInFrontFOVMax - lfFOVInOut
//   :781  lfDesiredFOVAdjustmentScaled = lfDesiredFOVAdjustment
//                                      * ((lrDotBackwards > 0.0f) ? 1.0f : lrSideScaleAmount)
//   :782/:785/:793  latch-or-tend mfFOVAdjustment at 0.1f     (flt_82004014)
//   :796  lfFOVInOut += mfFOVAdjustment
//
// FLOAT CONSTANTS -- every one DUMPED from the X360 image at the address the SAME statement
// loads (scratchpad\fc_flt.py, the per-segment `naive()` mapping; the segs[0]-based form
// silently reads STRING bytes as floats and I hit exactly that before fixing it):
//   flt_82001744 = 0.017453292 (pi/180)   flt_82001770 = 1.1920929e-07 (FLT_EPSILON)
//   flt_82001C98 = 1.0                    flt_82001CC0 = 0.0
//   flt_82001DA0 = 0.5                    flt_82002514 = -1.1920929e-07
//   flt_820025FC = 180.0                  flt_820037C8 = -1.0
//   flt_82003F40 = 0.25                   flt_82004014 = 0.1   <- FOV blend rate
//   flt_82004744 = 0.2   <- centering blend rate     flt_820086A4 = 0.09  <- drop blend rate
//   ⚠️ 0x82004014 is 0.1f and 0x82004018 is 0.75f -- adjacent, and this file's own
//     Parameters::Set banner already names both. Reading the wrong one would have made the
//     FOV chase 7.5x too fast.
//
// ⚠️⚠️ THE TWO "IsLookback" ARMS REALLY DO REPEAT THE SAME TWO ADDS, and that is a
//   TWO-BUILD finding, not a reading of one export. X360 emits the yaw/pitch fold once at
//   0x8222575C..0x82225818 and again at 0x822258D0..0x82225990, and DecFIGS charges the two
//   copies to DIFFERENT source lines (:685 vs :696 for their closing asserts) with both arms
//   inlining BrnCameraSphericalRotationController.h:82 and :88. A compiler duplicating a
//   common prefix would reuse one line number; two line numbers means two source regions.
//   Neither arm is specialised either -- both re-load mbIsLookback inside the inlined
//   accessor -- so the console is genuinely doing the work twice.
//
// ⚠️ THE TWO 2-LANE DOTS ARE SDK CALLS, NOT OPEN-CODED ARITHMETIC. DecFIGS charges both to
//   rw/math/vpu/detail/vector2_operation_inline.h:108, and the Vector2 build to
//   vector2_type_inline.h:39. `rw::math::vpu::Dot(Vector2, Vector2)` did not exist in the
//   vendored tree and is added by this commit with that provenance.
//
// ⚠️ THE AXES ARE READ OFF THE CONSTANT VECTORS THE CONSOLE BUILDS ON THE STACK, not
//   guessed: {1.0f, 0.0f} at X360 0x82225C7C/0x82225C84 and {0.0f, -1.0f} at 0x82225D8C/
//   0x82225DA8 (flt_820037C8 == -1.0f). With lLookDirection == {sin(yaw), cos(yaw)} those
//   give |sin(yaw)| (how far to the SIDE the player is looking) and clamped -cos(yaw) (how
//   far BEHIND). Both fall out of the yaw alone; the pivot maths is all trigonometry of the
//   free-look angle.
//
// ⚠️ mfCenteringFactor RUNS BACKWARDS FROM ITS NAME: it is 1 when the stick is deflected
//   (or in lookback) and decays to 0 when it is not. :710/:713 then use it as a CROSSFADE --
//   the pivot gains the camera's car-space offset while that same offset is faded out. So
//   "centering" is how far the rig has moved its orbit centre ONTO the car.
//
// ⚠️ THE FOUR-ARGUMENT ASSERT MESSAGES ARE OURS. The console builds each of the three
//   IsValid messages by streaming lRotation's value into the shared assert buffer with no
//   literal prefix at all (`stb 0, gpcMessageBuffer` then `bl sub_82203F70`), so there is no
//   console text to copy. The six IsZero asserts DO have literal text and it is quoted
//   verbatim ("!IsZero(lPivot)", "!rw::math::fpu::IsZero(lfLength)", "...(lfWidth)").
//
// ⚠️ PARAMETER NAMES ARE THE DWARF'S OWN -- the PS3 export carries lfFOVInOut / lRotation /
//   lPivot / lCarSpaceOffset / lrAABBox on the stack slots arg_38..arg_58, in that order.
//   Likewise the locals lfLength / lfWidth / lfPivotLength / lrDotBackwards /
//   lrSideScaleAmount / lfDesiredDropAmount / lfDesiredFOVAdjustment(+Scaled). Where DWARF
//   reuses one register under two names the DEFINING instruction's name is taken (lfWidth
//   at :723 is the same value DWARF calls lrSideScale at :732; it is kept as lfWidth here
//   and the alias is recorded rather than a second variable invented).
//
// ⚠️ NO CALLER YET, by construction: Update is the only one, and Update still cannot link --
//   Utils::CameraShakeICEController::Update (a direct `bl` from Update @0x82241C6C) has no
//   definition anywhere in the tree. See the header's LINK-CLOSURE SET.
// ============================================================================
void BehaviourGameplayExternal::UpdateLooking(f32& lfFOVInOut,
                                              Vector3& lRotation,
                                              Vector3& lPivot,
                                              Vector3& lCarSpaceOffset,
                                              const AABBox& lrAABBox)
{
    CGS_ASSERT(rw::math::vpu::IsValid(lRotation), "IsValid( lRotation )");        // .cpp:677

    // .cpp:679 -- the two arms fold the look stick in identically and then part ways on how
    // the centering factor moves. See the banner: the repetition is the console's, not ours.
    if (mRotationController.IsLookback())
    {
        lRotation.y += mRotationController.GetYawRotationAngleRads();             // .cpp:681
        lRotation.x += mRotationController.GetPitchRotationAngleRads();           // .cpp:683
        CGS_ASSERT(rw::math::vpu::IsValid(lRotation), "IsValid( lRotation )");    // .cpp:685

        mfCenteringFactor = 1.0f;                                                 // .cpp:688
    }
    else
    {
        lRotation.y += mRotationController.GetYawRotationAngleRads();             // .cpp:692
        lRotation.x += mRotationController.GetPitchRotationAngleRads();           // .cpp:694
        CGS_ASSERT(rw::math::vpu::IsValid(lRotation), "IsValid( lRotation )");    // .cpp:696

        if (mRotationController.IsRotated() || mRotationController.IsLookback())
        {
            mfCenteringFactor += (1.0f - mfCenteringFactor) * 0.2f;               // .cpp:701
        }
        else
        {
            mfCenteringFactor += (0.0f - mfCenteringFactor) * 0.2f;               // .cpp:705
        }
    }

    // .cpp:710/:713 -- crossfade the camera's car-space offset INTO the orbit centre.
    lPivot = lPivot + lCarSpaceOffset * mfCenteringFactor;
    CGS_ASSERT(!rw::math::vpu::IsZero(lPivot, rw::math::fpu::KF_IS_ZERO_TOLERANCE),
               "!IsZero(lPivot)");                                                // .cpp:711
    lCarSpaceOffset = lCarSpaceOffset * (1.0f - mfCenteringFactor);               // .cpp:713

    // The free-look direction in the car's ground plane. The console evaluates the accessor
    // once per trig call (two `lbz`+`fmuls` pairs, 0x82225B48 and 0x82225B6C) and computes
    // cos before sin; the value is the same either way.
    const f32 lfYawRads = mRotationController.GetYawRotationAngleRads();
    const Vector2 lLookDirection = Vector2{ rw::math::fpu::Sin(lfYawRads),
                                            rw::math::fpu::Cos(lfYawRads), 0.0f, 0.0f };

    // The car's own footprint, from the look-target box Update passes down.
    f32 lfLength = lrAABBox.mMax.z - lrAABBox.mMin.z;
    f32 lfWidth  = lrAABBox.mMax.x - lrAABBox.mMin.x;
    CGS_ASSERT(!rw::math::fpu::IsZero(lfLength),
               "!rw::math::fpu::IsZero(lfLength)");                               // .cpp:721
    CGS_ASSERT(!rw::math::fpu::IsZero(lfWidth),
               "!rw::math::fpu::IsZero(lfWidth)");                                // .cpp:722
    lfWidth /= lfLength;   // a WIDTH-TO-LENGTH RATIO from here on (DWARF: lrSideScale)

    CGS_ASSERT(!rw::math::vpu::IsZero(lPivot, rw::math::fpu::KF_IS_ZERO_TOLERANCE),
               "!IsZero(lPivot)");                                                // .cpp:731

    // .cpp:732 -- looking sideways widens (or narrows) the orbit by the car's own aspect
    // ratio, a quarter of the way. lrSideScaleAmount is a NAMED local, not an inline
    // sub-expression: the console computes it once (X360 `fabs f30` @0x82225CE0) and keeps
    // it in f30 across all THREE of its uses, here and at :753 and :781.
    const f32 lrSideScaleAmount =
        rw::math::fpu::Abs(rw::math::vpu::Dot(lLookDirection,
                                              Vector2{ 1.0f, 0.0f, 0.0f, 0.0f }));
    lPivot = lPivot * (1.0f + (lfWidth - 1.0f) * lrSideScaleAmount * 0.25f);
    CGS_ASSERT(!rw::math::vpu::IsZero(lPivot, rw::math::fpu::KF_IS_ZERO_TOLERANCE),
               "!IsZero(lPivot)");                                                // .cpp:733

    // How far behind the car the player is looking, 0..1.
    const f32 lrDotBackwards =
        rw::math::fpu::Clamp(rw::math::vpu::Dot(lLookDirection,
                                                Vector2{ 0.0f, -1.0f, 0.0f, 0.0f }),
                             0.0f, 1.0f);
    CGS_ASSERT(!rw::math::vpu::IsZero(lPivot, rw::math::fpu::KF_IS_ZERO_TOLERANCE),
               "!IsZero(lPivot)");                                                // .cpp:745

    // .cpp:749 -- pull the orbit centre in toward the car's midpoint as the player looks
    // back, by "Look Front Towards Factor". The divide by the old length is what turns the
    // scale into a straight SET of the pivot's distance; :745 is the guard that makes it safe.
    const f32 lfPivotLength = rw::math::vpu::Magnitude(lPivot);
    const f32 lfNewPivotLength =
        lfPivotLength - (lfPivotLength - lfLength * 0.5f)
                      * mpParameters->mfFrontInAmount * lrDotBackwards;
    lPivot = lPivot * (lfNewPivotLength / lfPivotLength);

    // .cpp:753/:756 -- how much of the vertical drop to ask for this frame.
    f32 lfDesiredDropAmount = (lrDotBackwards > 0.0f) ? 1.0f : lrSideScaleAmount;
    if (mRotationController.IsLookback())
    {
        lfDesiredDropAmount = 0.0f;
    }

    // .cpp:761 -- do not smooth ACROSS a lookback edge or a snap; latch instead.
    if (mRotationController.IsEndingLookbackThisFrame()
        || mRotationController.IsStartingLookbackThisFrame()
        || mbSnapToCar)
    {
        mfDropAmount = lfDesiredDropAmount;                                       // .cpp:764
    }
    else
    {
        mfDropAmount += (lfDesiredDropAmount - mfDropAmount) * 0.09f;             // .cpp:772
    }

    // .cpp:776 -- and drop the orbit centre, proportionally to how high it already is.
    lPivot.y -= lPivot.y * mpParameters->mfDropFactor * mfDropAmount;

    // .cpp:780/:781 -- widen the FOV toward "Look Front FOV Offset" by the same side/back
    // weighting, then latch or ease it in exactly as the drop above.
    const f32 lfDesiredFOVAdjustment = mpParameters->mfInFrontFOVMax - lfFOVInOut;
    const f32 lfDesiredFOVAdjustmentScaled =
        ((lrDotBackwards > 0.0f) ? 1.0f : lrSideScaleAmount) * lfDesiredFOVAdjustment;

    if (mRotationController.IsEndingLookbackThisFrame()                           // .cpp:782
        || mRotationController.IsStartingLookbackThisFrame()
        || mbSnapToCar)
    {
        mfFOVAdjustment = lfDesiredFOVAdjustmentScaled;                           // .cpp:785
    }
    else
    {
        mfFOVAdjustment += (lfDesiredFOVAdjustmentScaled - mfFOVAdjustment)       // .cpp:793
                         * 0.1f;
    }

    lfFOVInOut += mfFOVAdjustment;                                                // .cpp:796
}

// @0x821F9218.
const char* BehaviourGameplayExternal::GetName() const
{
    return "GameplayExternal";
}

// ---- Update's own constants ------------------------------------------------------------
// VALUES read out of the X360 image one statement at a time (see the Update banner for the
// address of each). The three at the top are the BYTE-SCANNED ones -- they read 0.0f in the
// image because they are dynamically-initialised file-scope VecFloats, and taking them at
// face value would give a zero pitch band, a zero speed cap and a zero drift rate.
// NAMES: `kvfMaxPitchLimit` is the DWARF's own (BehaviourGameplayExternal.cpp:40); the rest
// are descriptive -- DWARF records no name for them and they are spelled here for what the
// asm demonstrably does with each.
namespace
{
    const f32 KVF_MAX_PITCH_LIMIT              = 5.0f;         // 0x82FAA800 (byte-scanned)
    const f32 KF_MPH_TO_METRES_PER_SECOND      = 0.44704f;     // 0x83017FE0 (byte-scanned)
    const f32 KVF_VELOCITY_BLEND_RATE          = 0.1f;         // 0x82FAAC40 (byte-scanned)

    const u8  KU8_BOOST_SHAKE_TYPE             = 2;            // byte_82CDAD60
    const f32 KF_BOOST_SHAKE_FREQUENCY         = 1.0f;         // flt_82CDAD5C
    const f32 KF_BOOST_SHAKE_AMPLITUDE         = 9.0f;         // flt_82CDAD58

    const f32 KF_CAMERA_DISTANCE_SCALE_MIN     = 2.3f;         // flt_82CDAD64
    const f32 KF_CAMERA_DISTANCE_SCALE_MAX     = 2.3f;         // flt_82CDAD68 (same value)

    const f32 KF_DEGREES_TO_HALF_ANGLE_RADIANS = 0.0087266462f; // flt_82002518 == PI/360
    const f32 KF_DEGREES_TO_RADIANS            = 0.017453292f;  // flt_82001744
    const f32 KF_RADIANS_TO_DEGREES            = 57.295780f;    // flt_82001748
}

// The three DecFIGS-named file-scope tunables Update reads that the header declares but the
// UpdateJumping block above does not define (BehaviourGameplayExternal.cpp:32/:33/:34).
const f32 BehaviourGameplayExternal::kfShakeDropoffFactor                = 0.059999999f; // flt_82CDA6FC
const f32 BehaviourGameplayExternal::kfJumpParamsImpactShakeMaxAmplitude = 2.0f;         // flt_82CDA700
const f32 BehaviourGameplayExternal::kfJumpParamsImpactShakeMaxFreqMul   = 8.0f;         // flt_82CDA704

// ============================================================================
// BehaviourGameplayExternal::Update @0x82240828 / PS3 @0x6985C   (.cpp:188..:561)
// ⭐⭐⭐ THE CHASE CAMERA'S PER-FRAME BODY -- BODIED 2026-08-02 (update-transcribe wave).
// 1792 X360 asm lines; 91 DecFIGS own-line statements; drives all eight helpers, both
// CameraShakes, the ICE boost shake, the debug-render arm and the FOV publish.
//
// PROVENANCE / METHOD. Every statement below was read off the X360 asm (constants, offsets,
// branch senses) with the DecFIGS PS3 per-instruction line attribution used ONLY to place
// statement boundaries and to name the inlined SDK calls. The inline-name resolution is
// mechanical and worth reusing: a lineinfo row pointing at `<sdk header>:<N>` lies inside
// whichever declaration the dwarfdump's own `// <file>:<M>` comment puts at the greatest
// M <= N (scratchpad ut_resolve.py). That is what turned :207 from "a normalise and a length"
// into the single SDK call `NormalizeReturnMagnitude`, and :238/:239 into Lerp + Cross.
//
// ⚠️⚠️ THE X360 SOURCE-LINE OFFSET IS **NOT CONSTANT** IN THIS FUNCTION, and the file-wide
//   "DecFIGS + 74" the header used to state would mis-anchor every assert in the head:
//     X360 251 / 270 / 278            -> DecFIGS :190 / :209 / :217      (+61)
//     X360 409/410/412/414/415/422 and 473/474/485/501/524/550/569/594
//                                     -> :335/:336/:338/:340/:341/:348 and
//                                        :399/:400/:411/:427/:450/:476/:495/:520   (+74)
//   Fourteen exact hits at +74 and three at +61 -- so the X360 source gained 13 lines
//   somewhere between :217 and :335 relative to the DecFIGS revision. Lines quoted below are
//   the DecFIGS ones (the numbering the rest of this file uses).
//
// ---- THREE FLAGGED OMISSIONS, all deliberate, none silent -----------------------------
//  (a) .cpp:512 -- `BoostShakeController::Update` IS NOT CALLED. ⚠️⚠️ NOT because the body is
//      missing (BrnBoostShakeController.cpp is bodied AND mounted) but because the committed
//      BrnBoostShakeController.h MISTYPED BOTH OF ITS BLOCKS, and calling it through those
//      types would be an offset poke that silently writes the wrong x64 fields:
//        * X360 0x822422A4 `mr r4, r29` -- argument 1 is the **Camera**, not a separate
//          "shake-output block". Its +0x114/+0x11C are `mEffects.mfShakeAmplitude` /
//          `mEffects.mu8ShakeType` (Camera::mEffects @+0x68, CameraEffects +0xAC/+0xB4 --
//          both already NAMED in BrnCameraEffects.h and cross-attested there by
//          PerlinShakeController::Update and Camera::SetImpactShake).
//        * X360 0x822422A0 `addi r5, r19, 0x60` -- argument 2 is
//          **lrSharedInfo.mPlayerInfo** (a VehicleInfo), not "the gameplay-external camera
//          state". Its +0x3CC/+0x3D4 are inside mRaceCarState.
//        * argument 3 is `mpDirectorResourceManager + 0x5D8`; `this` is a stack temporary
//          (the controller is stateless, so the console never reads it).
//      Consequence of the omission: the camera's BOOST-shake amplitude/type request is not
//      published, so there is no boost shake. That is a MISSING effect, not a corrupting one,
//      and it is the honest state until that header is re-typed against named members.
//  (b) .cpp:392 -- an assert that exists in the DecFIGS build and NOT in the X360 one (the
//      X360 assert-line set jumps 0x1A6 -> 0x1D9 with nothing between). Not transcribed: the
//      X360 is this build's target.
//  (c) every assert's streamed VALUE. The console builds each message by streaming a Vector3
//      / Matrix44Affine / float into the shared assert buffer; CGS_ASSERT takes a literal, so
//      the predicate is transcribed and the value is dropped -- the same convention
//      UpdateLooking and CameraUtils::CreateLookAt already use. Where the console DOES have
//      literal text ("Spring: ", "FOV is ", " after scaling, was ") it is quoted verbatim.
//
// ---- THE CONSTANTS, AND THE TWO THAT READ ZERO IN THE IMAGE ---------------------------
//   flt_82004C6C 60.0f  flt_82004A20 10.0f  flt_820054CC 20.0f  flt_82001B48 -10.0f
//   flt_820049E0 100.0f flt_82004014 0.1f   flt_820047C8 0.05f  flt_82002138 0.01f
//   flt_82001C98 1.0f   flt_82001CC0 0.0f   flt_820047C4 15.0f  flt_82004D0C 40.0f
//   flt_82002540 1e-4f  flt_82008714 0.025f flt_820054C8 0.8f   flt_82001D9C 2.0f
//   flt_82009B84 1.2f   flt_82002518 pi/360 flt_82001744 deg2rad flt_82001748 rad2deg
//   flt_82006948 2.79252672f (== 160 degrees) flt_82001770 / flt_82002514 +/- FLT_EPSILON
//   flt_82CDAD58 9.0f   flt_82CDAD5C 1.0f   byte_82CDAD60 2   flt_82CDAD64/68 2.3f
//   flt_82CDA6FC 0.06f  flt_82CDA700 2.0f   flt_82CDA704 8.0f
// ⚠️⚠️ `kvfMaxPitchLimit` (flt_82FAA800, DWARF-named at .cpp:40) and the 0.1f broadcast at
//   unk_82FAAC40 BOTH READ 0.0f IN THE IMAGE -- the same dynamically-initialised file-scope
//   VecFloat shape as the five KVF_LAST_PLAYER_TRANSFORM_* constants above. Recovered by
//   byte-scanning .text for the initialiser that STORES to them (scratchpad ut_init.py, the
//   `lis/addi -> lvlx -> vspltw -> stvx128` idiom):
//     0x82FAA800 <- init @0x82C49380, rodata 0x8200426C == 5.0f    (kvfMaxPitchLimit)
//     0x82FAAC40 <- init @0x82C493A8, rodata 0x82004014 == 0.1f
//   and `unk_83017FE0` -- the VecFloat :211 multiplies 100.0f by -- <- init @0x82C6D174,
//   rodata 0x82F31928 == 0.44704f, i.e. the MPH->m/s factor. Taking any of the three from the
//   image at face value gives a pitch limit of zero, a drift crossfade rate of zero and a
//   speed cap of zero, and nothing downstream complains.
// ⚠️ `kvfVelocityTransformSlerpSpeed` (DWARF .cpp:42) -- the trap the predecessor flagged --
//   is NOT read by this function. :260's blend is `Min(1, (speed - 1) * 0.1)` built from
//   unk_82FAAC40 (above) and :264's is `mfDriftScale`. Neither touches that symbol, and no
//   instruction in 0x82240828..0x82242418 references it. Whatever reads it, it is not Update.
//
// ---- THE MEMBERS THIS BODY WRITES, AND THE THREE CORRECTIONS THAT CAME OUT OF IT -------
//   The committed statement map in the header put :271/:290/:292/:296 on mfCenteringFactor,
//   :267 on mfYawSpring and CalcSpringCoeffs' inputs on mfZVelocity/mfYawDrift. All three
//   were wrong; the first two because the PS3 offsets in this tail are X360 MINUS 0x10 (a
//   shift the committed UpdateJumping banner already documents) and the third by reading the
//   wrong pair of `lfs`. The corrected members are mfDriftScale (+0xB14), mfTimeDelta
//   (+0xB48) and mfPitchSpring/mfYawSpring (+0xB3C/+0xB38) -- and the corrected reading is
//   self-consistent, because mfDriftScale is exactly what :264's SLerp blends on.
//
// ---- THE SHAPE, IN ONE PARAGRAPH ------------------------------------------------------
//   Build a frame from the car's VELOCITY (its forward is the velocity direction, its up
//   lerped between world-up and the car's own -Z by the velocity's y), SLerp from the car's
//   own frame toward it by how fast the car is going, ortho-normalise, then SLerp back toward
//   the car's frame by how hard it is drifting, and pull the origin back along the velocity.
//   Take that lagged frame's ZXY Euler angles, spring them toward the last frame's, hand the
//   remainder to the free-look pass, build the two-joint arm through CalculateCameraTransform,
//   dolly out by the FOV, shake it, jump it, slide it, and publish transform + FOV.
// ============================================================================
bool BehaviourGameplayExternal::Update(Camera& lCamera, const BehaviourSharedInfo& lrSharedInfo)
{
    // .cpp:190 -- NON-GATING: the console asserts and then dereferences anyway (X360
    // 0x82240884 re-loads mpParameters immediately after EndAssert).
    CGS_ASSERT(mpParameters != 0, "Updating with no parameters");

    // .cpp:192 -- THE WHOLE BODY sits inside this test (`lbz r11, 0xAC(params)`).
    if (mpParameters->mbIsValid)
    {
        const Parameters& lrCameraAttribs = *mpParameters;
        const BrnPhysics::Vehicle::RaceCarState& lrCarState = lrSharedInfo.mPlayerInfo.mRaceCarState;

        // .cpp:197 -- one statement, two effects: sample this behaviour's declared timestep
        // flavour AND raise the camera's "behaviour ran" state bit (X360
        // `ld/ori 2/std` at lCamera + 0x140 == CameraState's current-flag word, bit 1).
        const f32 lfTimestep = lrSharedInfo.GetTimestep(GetTimestepType());
        lCamera.GetState().SetFlag(1, true);

        // .cpp:201 -- frames-at-60Hz, the unit every spring rate below is authored in.
        const f32 lfTimeStepMod = lfTimestep * 60.0f;

        // .cpp:207 -- ONE SDK call publishing both outputs (see the vendored
        // NormalizeReturnMagnitude); the zero-length guard is the console's own vsel.
        Vector3 lPlayerDirection;
        const f32 lfPlayerSpeed =
            rw::math::vpu::NormalizeReturnMagnitude(lrCarState.mLinearVelocity, lPlayerDirection);

        // .cpp:209 -- `vcmpeqfp128. v124, v124` : the NaN self-compare on the SPEED.
        CGS_ASSERT(rw::math::fpu::IsValid(lfPlayerSpeed), "IsValid(lvfPlayerSpeed)");

        // .cpp:211/:214 -- how much of the authored pitch band this speed eats. The cap is
        // 100 MPH expressed in m/s (see the banner); the reduction saturates there.
        const f32 kfPitchLimitMaxSpeed = 100.0f * KF_MPH_TO_METRES_PER_SECOND;
        const f32 lfPitchLimitReduction =
            (rw::math::fpu::Min(lfPlayerSpeed, kfPitchLimitMaxSpeed) / kfPitchLimitMaxSpeed)
            * KVF_MAX_PITCH_LIMIT;

        // .cpp:217 -- the console streams all four values; only the predicate is transcribed.
        CGS_ASSERT(rw::math::fpu::IsValid(lfPitchLimitReduction), "IsValid(lfPitchLimitReduction)");

        // .cpp:226 -- the free-look stick integrator. ⚠️ THE PITCH BAND IS ASYMMETRIC AND
        // SPEED-DEPENDENT: the floor is a flat -10 degrees, the ceiling is 10 degrees MINUS
        // the reduction above, so the faster you go the less you can look up. When mbJumping
        // the band is forced to 20 degrees and the "paused" flag is forced true instead of
        // being read from the shared info (X360 0x82240B30..0x82240B50).
        // ⚠️ The timestep here is the GAME lane read DIRECTLY (`lfs 0x588` == mTimestep
        // + 0x38 == mafTimestep[2] == E_GAME), NOT this behaviour's declared flavour -- the
        // console indexes the array with a constant and the compiler folded the accessor away.
        mRotationController.Update(
            lrSharedInfo.mTimestep.Get(BrnDirector::Timestep::E_GAME),
            lrSharedInfo.mCameraModifier,
            lrSharedInfo.mbLookback,
            mbJumping ? true : lrSharedInfo.mbUseControlPauseBehaviour,
            -10.0f,
            mbJumping ? 20.0f : (10.0f - lfPitchLimitReduction));

        // ---- .cpp:230..:240 -- THE VELOCITY FRAME -------------------------------------
        // A right-handed frame whose forward IS the car's velocity direction. Its up axis is
        // Lerp(world-up, -car.zAxis, velocity.y): level ground gives world-up, and the more
        // vertically the car is moving the more the frame rolls onto the car's own back.
        const Matrix44Affine& lrEyeTarget = lrSharedInfo.GetEyeTarget();

        const Vector3 lUpReference =                                       // .cpp:238
            rw::math::vpu::Lerp(Vector3{ 0.0f, 1.0f, 0.0f, 0.0f },
                                Vector3{ -lrEyeTarget.zAxis.x, -lrEyeTarget.zAxis.y,
                                         -lrEyeTarget.zAxis.z, -lrEyeTarget.zAxis.w },
                                lPlayerDirection.y);

        const Vector3 lVelocityRight =                                     // .cpp:238
            rw::math::vpu::NormalizeFast(rw::math::vpu::Cross(lUpReference, lPlayerDirection));
        const Vector3 lVelocityUp =                                        // .cpp:239
            rw::math::vpu::NormalizeFast(rw::math::vpu::Cross(lPlayerDirection, lVelocityRight));

        Matrix44Affine lVelocityFrame;                                     // .cpp:235
        lVelocityFrame.xAxis = lVelocityRight;
        lVelocityFrame.yAxis = lVelocityUp;
        lVelocityFrame.zAxis = lPlayerDirection;
        lVelocityFrame.wAxis = lrEyeTarget.wAxis;

        // .cpp:240/:246 -- the car's own frame, YAWED 180 DEGREES WHEN THE GEARBOX IS IN
        // SLOT 0 (`lbz 0x4A4` == mPlayerInfo.mRaceCarState.mi8Gear; the X360 branch at
        // 0x82240C7C SKIPS the negation when the gear is non-zero). Reversing runs the car
        // backwards along its own -Z, so the reference frame the camera blends toward has to
        // flip with it or the chase camera ends up in front of the bonnet.
        Matrix44Affine lCarFrame = lrEyeTarget;
        if (lrCarState.mi8Gear == 0)
        {
            lCarFrame.xAxis = rw::math::vpu::Mult(lCarFrame.xAxis, -1.0f);
            lCarFrame.zAxis = rw::math::vpu::Mult(lCarFrame.zAxis, -1.0f);
        }

        // .cpp:260 -- blend from the car's frame TOWARD the velocity frame, by speed: nothing
        // below 1 m/s, fully by 11 m/s. .cpp:261 re-orthonormalises the result.
        Vector3 lUnusedAngle;
        Matrix44Affine lVelocityTransform =
            rw::math::vpu::SLerp(lCarFrame, lVelocityFrame,
                                 rw::math::fpu::Min(rw::math::fpu::Max((lfPlayerSpeed - 1.0f)
                                                                       * KVF_VELOCITY_BLEND_RATE,
                                                                       0.0f),
                                                    1.0f),
                                 &lUnusedAngle);
        lVelocityTransform = rw::math::vpu::OrthoNormalize3x3(lVelocityTransform);   // .cpp:261

        // .cpp:264 -- and blend BACK toward the car's own frame by how hard it is drifting.
        lVelocityTransform = rw::math::vpu::SLerp(lVelocityTransform, lrEyeTarget,
                                                  mfDriftScale, &lUnusedAngle);

        // .cpp:267 -- pull the frame's origin back along the car's velocity by one
        // mfTimeDelta's worth of travel (the rig's lag budget).
        lVelocityTransform.wAxis =
            lVelocityTransform.wAxis
            - rw::math::vpu::Mult(lrCarState.mLinearVelocity, mfTimeDelta);

        // .cpp:269..:273 -- a snap request throws away the accumulated drift and re-seats the
        // lagged transform on the car, then re-constructs the free-look integrator.
        if (mbSnapToCar)
        {
            mfDriftScale = 0.0f;                                            // .cpp:271
            mLastPlayerTransform = lCarFrame;                               // .cpp:272
            mRotationController.Construct();                                // .cpp:273
        }

        // .cpp:277..:282 -- while the car is CRASHING the rig stops following the velocity
        // frame's ROTATION and re-uses the lagged one (the translation row keeps updating).
        if (lrCarState.mbCrashing)
        {
            lVelocityTransform.xAxis = mLastPlayerTransform.xAxis;          // .cpp:280
            lVelocityTransform.yAxis = mLastPlayerTransform.yAxis;          // .cpp:281
            lVelocityTransform.zAxis = mLastPlayerTransform.zAxis;          // .cpp:282
        }

        // .cpp:287 -- drag mLastPlayerTransform toward that frame, rate-limited in ANGLE.
        InterpolateLastPlayerTransform(lVelocityTransform,
                                       ::VecFloat{ lfPlayerSpeed, lfPlayerSpeed,
                                                   lfPlayerSpeed, lfPlayerSpeed },
                                       ::VecFloat{ lfTimestep, lfTimestep,
                                                   lfTimestep, lfTimestep });

        // .cpp:290..:296 -- the drift crossfade. Drifting hard drives mfDriftScale to 1 (at
        // 0.1 per frame), releasing lets it decay to 0 (at 0.05) -- and mfDriftScale is what
        // :264 above blends the camera frame back onto the car with.
        if (lrCarState.mfAbsDriftScale < 0.1f)
            mfDriftScale += (0.0f - mfDriftScale) * 0.05f;                  // .cpp:292
        else
            mfDriftScale += (1.0f - mfDriftScale) * 0.1f;                   // .cpp:296

        // .cpp:301 -- ⚠️ A FUNCTION-LOCAL STATIC, with a real guard variable on console
        // (unk_82FAAD40 behind the bit-0 guard at dword_82FAAD50). It is PER-CLASS, not
        // per-instance: both shared gameplay cameras run through this same storage.
        static Vector3 sLastCameraAngles = Vector3{ 0.0f, 0.0f, 0.0f, 0.0f };

        // .cpp:307 -- the lagged frame's ZXY Euler angles, disambiguated toward last frame's.
        Vector3 lTargetAngles =
            Utils::EulerAnglesZXYFromMatrix44Affine(mLastPlayerTransform, &sLastCameraAngles, 0.01f);

        if (mbSnapToCar)
            sLastCameraAngles = lTargetAngles;                              // .cpp:309
        if (lrCarState.mbCrashing)
            lTargetAngles = sLastCameraAngles;                              // .cpp:317

        // .cpp:321 -- clamp the pitch/roll into the authored band and scale the roll.
        ModifyTargetAngles(lrCameraAttribs, lTargetAngles);

        // .cpp:331 -- ⚠️ THE OUTPUTS ARE THE OTHER WAY ROUND FROM THEIR NAMES (see that
        // helper's banner): lSpring receives `1 - v` and lInverseSpring receives `v`.
        Vector3 lSpring;
        Vector3 lInverseSpring;
        CalcSpringCoeffs(lrCarState.mfSpeedMPH,
                         mfPitchSpring * lfTimeStepMod,
                         mfYawSpring * lfTimeStepMod,
                         lSpring, lInverseSpring);

        CGS_ASSERT(rw::math::vpu::IsValid(sLastCameraAngles), "IsValid(mLastCameraAngles)"); // :335
        CGS_ASSERT(rw::math::vpu::IsValid(lTargetAngles), "IsValid(lTargetAngles)");         // :336

        // .cpp:337 -- the wrapped (shortest-way-round) angular error.
        const Vector3 lAngleDelta =
            Utils::GetSmallestDifferenceBetweenRadAngles(sLastCameraAngles, lTargetAngles);
        CGS_ASSERT(rw::math::vpu::IsValid(lAngleDelta), "IsValid(lAngleDifference)");        // :338

        // .cpp:340/:341 -- both stream "Spring: <lSpring> Inverse Spring: <lInverseSpring>\n";
        // the PREDICATES are on different vectors and in this order (X360 checks var_380 for
        // line 414 and var_360 for 415).
        CGS_ASSERT(rw::math::vpu::IsValid(lInverseSpring),
                   "Spring / Inverse Spring: IsValid(lInverseSpring)");
        CGS_ASSERT(rw::math::vpu::IsValid(lSpring),
                   "Spring / Inverse Spring: IsValid(lSpring)");

        // .cpp:344 -- the spring integrator: the static angles take the sprung share of the
        // error, and .cpp:346 hands the REMAINDER to the free-look pass as its base rotation.
        sLastCameraAngles = sLastCameraAngles + rw::math::vpu::Mult(lAngleDelta, lSpring);
        Vector3 lRotation = rw::math::vpu::Mult(                            // .cpp:346
            Vector3{ -lAngleDelta.x, -lAngleDelta.y, -lAngleDelta.z, -lAngleDelta.w },
            lInverseSpring);
        CGS_ASSERT(rw::math::vpu::IsValid(lRotation), "IsValid(lRotation)");                 // :348

        // .cpp:351/:357 -- the authored orbit arm and the camera arm, in car space.
        // ⚠️ THE PIVOT'S Z IS NEGATED and its X is dropped: the orbit centre sits ABOVE and
        // BEHIND the car's origin. The scale vector is a literal (1,1,1) here -- the hook
        // CalculateCameraTransform multiplies both arms by.
        Vector3 lPivot          = Vector3{ 0.0f, lrCameraAttribs.mrPivotY,
                                           -lrCameraAttribs.mrPivotZ, 0.0f };
        const Vector3 lCarScale = Vector3{ 1.0f, 1.0f, 1.0f, 0.0f };
        Vector3 lCarSpaceOffset = Vector3{ 0.0f, 0.0f, lrCameraAttribs.mrPivotZOffset, 0.0f };

        // .cpp:364..:375 -- THE DEBUG-RENDER ARM, gated on mbEnableDebugRender (X360
        // `lbz r7, 0xB5E` then `beq` over the whole arm). Nothing on this build raises that
        // member, so this never runs; it is transcribed rather than dropped so the two
        // CgsDev::DebugRender entry points stay on the record. See WorldLinkStubs.cpp for
        // their (one-shot-logging) stubs -- neither has a reconstructed body anywhere.
        if (mbEnableDebugRender)
        {
            CgsDev::DebugInterface lDebugInterface;
            lDebugInterface.GetRender().DrawBox(&lrEyeTarget.xAxis.x, 0xFF00FF00u,
                                                Vector4{ 0.0f, lrCameraAttribs.mrPivotY,
                                                         -lrCameraAttribs.mrPivotZ, 0.0f },
                                                Vector4{ 0.0f, 0.0f,
                                                         lrCameraAttribs.mrPivotZOffset, 0.0f });

            const Vector3 lDebugFrom = rw::math::vpu::TransformPoint(lrEyeTarget, lPivot);
            lDebugInterface.GetRender().DrawLine(0xFF00FF00u, lDebugFrom,
                                                 lDebugFrom + Vector3{ 0.0f, 2.0f, 0.0f, 0.0f });
        }

        // .cpp:375/:377 -- the FOV this frame: the authored FOV lerped toward the authored
        // BOOST FOV (plus a temporary offset the effects path can push) by the speed ratio.
        // ⚠️ mbEnableBoostEffects gates only the OFFSET, not the lerp.
        const f32 lfBoostFOVOffset =
            mbEnableBoostEffects
            ? (lrSharedInfo.mfSpeedRatio * lrSharedInfo.mfTempFOVBoostAmount * 15.0f)
            : 0.0f;
        f32 lfFOV = lrCameraAttribs.mrFOV
                  + ((lrCameraAttribs.mfBoostFOV + lfBoostFOVOffset) - lrCameraAttribs.mrFOV)
                    * lrSharedInfo.mfSpeedRatio;

        // .cpp:383/:386 -- while the car is airborne the FOV is FROZEN at whatever it was on
        // take-off; on the ground the latch tracks.
        if (lrCarState.mfTimeInAir > 0.0f)
            lfFOV = mfJumpFOV;
        else
            mfJumpFOV = lfFOV;

        // .cpp:397 -- the free-look pass: folds the stick into lRotation, re-shapes the pivot
        // and the car-space offset, and adjusts lfFOV through its in/out reference.
        UpdateLooking(lfFOV, lRotation, lPivot, lCarSpaceOffset, lrSharedInfo.GetLookTarget());

        CGS_ASSERT(rw::math::vpu::IsValid(lTargetAngles), "IsValid(lTargetAngles)");         // :399
        CGS_ASSERT(rw::math::vpu::IsValid(lRotation), "IsValid(lRotation)");                 // :400

        // .cpp:409 -- THE KEYSTONE. ⚠️ ARGUMENT ORDER IS X360 v1..v6 (see that helper's
        // banner): #1 the SECOND rotate's angles, #2 the scale, #3 the pivot arm, #4 the
        // FIRST rotate's angles, #5 DEAD, #6 the camera arm. The dead slot really is fed
        // lInverseSpring by the console (v5 still holds it from :340) -- kept, because the
        // DWARF declares the parameter and the caller passes it.
        Matrix44Affine lCameraMatrix;
        CalculateCameraTransform(lrCameraAttribs, lCameraMatrix, lVelocityTransform,
                                 lTargetAngles, lCarScale, lPivot,
                                 lRotation, lInverseSpring, lCarSpaceOffset,
                                 lrCarState.mfSpeedMPH, lfTimestep);
        CGS_ASSERT(rw::math::vpu::IsValid(lCameraMatrix.xAxis)                               // :411
                   && rw::math::vpu::IsValid(lCameraMatrix.yAxis)
                   && rw::math::vpu::IsValid(lCameraMatrix.zAxis)
                   && rw::math::vpu::IsValid(lCameraMatrix.wAxis),
                   "IsValid(lCameraMatrix)");

        // .cpp:417/:424/:427 -- DOLLY THE RIG TO KEEP THE CAR THE SAME SIZE ON SCREEN. As the
        // FOV widens, 1/tan(halfFOV) shrinks and the camera is pushed FORWARD along its own
        // Z by the difference, scaled by a distance constant.
        // ⚠️ flt_82CDAD64 and flt_82CDAD68 are BOTH 2.3f, so the Lerp between them is a
        //   constant on the shipped image. Transcribed as the lerp the console evaluates
        //   rather than folded, because the two ARE separate authored slots.
        const f32 lfDistanceScale =
            KF_CAMERA_DISTANCE_SCALE_MIN
            + (KF_CAMERA_DISTANCE_SCALE_MAX - KF_CAMERA_DISTANCE_SCALE_MIN)
              * lrSharedInfo.mfTempFOVBoostAmount;
        const f32 lfHalfFOVRads = lfFOV * KF_DEGREES_TO_HALF_ANGLE_RADIANS;
        lCameraMatrix.wAxis =                                                // .cpp:427
            lCameraMatrix.wAxis
            + rw::math::vpu::Mult(lCameraMatrix.zAxis,
                                  lfDistanceScale
                                  - (1.0f / rw::math::fpu::Tan(lfHalfFOVRads)) * lfDistanceScale);
        CGS_ASSERT(rw::math::vpu::IsValid(lCameraMatrix.wAxis), "IsValid(lCameraMatrix)");   // :427

        // .cpp:435..:445 -- the AUTHORED (ICE) boost shake. ⚠️ Shake type 2, frequency 1.0f
        // and the 9.0f amplitude scale are the three .data constants the ICE-shake wave
        // already attested independently. The amplitude falls off to nothing while airborne.
        if (mbEnableBoostEffects && !rw::math::fpu::IsZero(lrSharedInfo.mfSpeedRatio))
        {
            const f32 lfGrounded =                                           // .cpp:440
                1.0f - rw::math::fpu::Min(rw::math::fpu::Max(lrCarState.mfTimeInAir, 0.0f), 1.0f);
            mBoostShake.Update(lrSharedInfo.mpDirectorResourceManager,       // .cpp:445
                               lfTimestep,
                               KU8_BOOST_SHAKE_TYPE,
                               lfGrounded
                               * (lrSharedInfo.mfSpeedRatio + lrSharedInfo.mfTempFOVBoostAmount)
                               * lrSharedInfo.mfSpeedRatio * KF_BOOST_SHAKE_AMPLITUDE,
                               KF_BOOST_SHAKE_FREQUENCY);

            // .cpp:450 -- the shake matrix PRE-multiplies (it is expressed in camera space,
            // so it rocks the camera about its own axes and leaves the position alone
            // except for the shake's own translation row).
            lCameraMatrix = rw::math::vpu::Mult(mBoostShake.GetMatrix(), lCameraMatrix);
        }
        CGS_ASSERT(rw::math::vpu::IsValid(lCameraMatrix.wAxis), "IsValid(lCameraMatrix)");   // :450

        // .cpp:453 -- the jump/landing dutch + FOV integrator.
        UpdateJumping(lrSharedInfo, lfTimestep, lCamera);

        // .cpp:460..:469 -- the IMPACT shake request: the hardest impact this frame, ramped
        // in below 40 MPH (|speed| * 0.025 is exactly 1.0 at 40, so the ramp is continuous),
        // clamped to 0.8 and LATCHED (only ever raised here; :500 decays it).
        f32 lfImpactShake =
            std::sqrt(lrSharedInfo.mPlayerInfo.mfHardestImpact) * 0.0001f;
        const f32 lfSpeedMPHAbs = rw::math::fpu::Abs(lrCarState.mfSpeedMPH);
        if (lfSpeedMPHAbs < 40.0f)
            lfImpactShake = lfSpeedMPHAbs * lfImpactShake * 0.025f;
        mfImpactShakeFactor =
            rw::math::fpu::Max(mfImpactShakeFactor,
                               rw::math::fpu::Min(rw::math::fpu::Max(lfImpactShake, 0.0f), 0.8f));

        // .cpp:474 -- the slide/drift push.
        ApplySlideyEffects(lrCameraAttribs, lCameraMatrix, lVelocityTransform, lrSharedInfo);
        CGS_ASSERT(rw::math::vpu::IsValid(lCameraMatrix.wAxis), "IsValid(lCameraMatrix)");   // :476

        // .cpp:479 -- PUBLISH. The anti-zoom compensation pushes the camera along its own Z
        // proportionally to the speed ratio, cancelling the boost FOV's apparent pull-in.
        lCamera.mTransform.xAxis = lCameraMatrix.xAxis;
        lCamera.mTransform.yAxis = lCameraMatrix.yAxis;
        lCamera.mTransform.zAxis = lCameraMatrix.zAxis;
        lCamera.mTransform.wAxis =
            lCameraMatrix.wAxis
            + rw::math::vpu::Mult(lCameraMatrix.zAxis,
                                  lrCameraAttribs.mfBoostFOVZoomCompensation
                                  * lrSharedInfo.mfSpeedRatio);
        lCamera.ValidateTransformWithDebugInfo();

        // .cpp:495 -- flag 3: this behaviour has published a transform this frame.
        lCamera.GetState().SetFlag(3, true);
        CGS_ASSERT(lfFOV > 0.0f, "FOV is <value> before scaling");

        lCamera.SetFOV(lfFOV);                                              // .cpp:495 tail
        ApplyJumpEffects(lCamera, lrSharedInfo);                            // .cpp:498

        // .cpp:500 -- the impact-shake latch decays toward zero every frame.
        mfImpactShakeFactor += (0.0f - mfImpactShakeFactor) * kfShakeDropoffFactor;

        // .cpp:505 -- ⚠️ the two constants are SCALES, not values: the frequency argument is
        // the timestep times 8, the amplitude is the latched factor times 2.
        mImpactShake.Update(lCamera.mTransform,
                            lrCameraAttribs.mImpactShakeParams,
                            *lrSharedInfo.mpRandom,
                            lrSharedInfo.GetTimestep(GetTimestepType())
                                * kfJumpParamsImpactShakeMaxFreqMul,
                            mfImpactShakeFactor * kfJumpParamsImpactShakeMaxAmplitude);

        // .cpp:508 -- the snap request is consumed.
        mbSnapToCar = false;

        // .cpp:512 -- ⛔ OMITTED: BoostShakeController::Update. See flag (a) in the banner.

        // .cpp:515/:522 -- WIDEN THE PUBLISHED FOV BY 1.2x IN TANGENT SPACE (not in degrees),
        // then clamp to [1, 160] degrees. Reading lCamera's own FOV back is deliberate: it is
        // the value SetFOV above just committed (and clamped).
        const f32 lfScaledFOVRads =
            2.0f * std::atan(rw::math::fpu::Tan(lCamera.mfFOV * KF_DEGREES_TO_HALF_ANGLE_RADIANS)
                             * 1.2f);
        CGS_ASSERT(lfFOV > 0.0f, "FOV is <value> after scaling, was <value> before");  // :520
        lCamera.SetFOV(rw::math::fpu::Clamp(lfScaledFOVRads,
                                            KF_DEGREES_TO_RADIANS, 2.79252672f)
                       * KF_RADIANS_TO_DEGREES);

        // .cpp:561 -- flag 6 tracks a lookback TRANSITION. ⚠️ The console's two arms are
        // (!starting && ending) and (starting && !ending), i.e. an EXCLUSIVE test -- both set
        // at once falls through to the Clear. Transcribed as read; the two are mutually
        // exclusive in practice, so the XOR and the OR agree on every reachable state.
        if (mRotationController.IsStartingLookbackThisFrame()
            != mRotationController.IsEndingLookbackThisFrame())
        {
            lCamera.GetState().SetFlag(6, true);
        }
        else
        {
            lCamera.GetState().ClearFlag(6);
        }
    }

    return true;                                                            // X360 `li r3, 1`
}

} // namespace Camera
} // namespace BrnDirector
