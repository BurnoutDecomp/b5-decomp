// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGameplayExternal::Parameters slice
// this TU owns:
//   - BehaviourGameplayExternal::Parameters::Set @0x821F9228  (defined here)
//
// Parameters::Set is the seeding step the main director runs (ProcessNewVehicleEvents /
// UpdateAttribSys) and the replay director runs (PreSceneQueryUpdate) to populate an
// external ("chase") gameplay-camera block from the vehicle's attribute-system source block
// before installing it.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"

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

// @0x821F9218.
const char* BehaviourGameplayExternal::GetName() const
{
    return "GameplayExternal";
}

} // namespace Camera
} // namespace BrnDirector
