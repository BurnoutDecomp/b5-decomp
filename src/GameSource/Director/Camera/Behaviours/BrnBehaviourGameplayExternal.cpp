// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGameplayExternal::Parameters slice
// this TU owns:
//   - BehaviourGameplayExternal::Parameters::Set @0x821F9228  (defined here)
//   - BehaviourGameplayExternal::UpdateJumping   @0x8220EAD0  (added 2026-08-02; the fourth
//     of the eight helpers Update drives -- see its own banner below)
//
// Parameters::Set is the seeding step the main director runs (ProcessNewVehicleEvents /
// UpdateAttribSys) and the replay director runs (PreSceneQueryUpdate) to populate an
// external ("chase") gameplay-camera block from the vehicle's attribute-system source block
// before installing it.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h"

#include "GameShared/GameClasses/Numeric/CgsRandom.h"   // CgsNumeric::Random::RandomBool
                                                        //   (Behaviour.h only forward-declares it;
                                                        //    UpdateJumping needs the complete type)

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

// @0x821F9218.
const char* BehaviourGameplayExternal::GetName() const
{
    return "GameplayExternal";
}

} // namespace Camera
} // namespace BrnDirector
