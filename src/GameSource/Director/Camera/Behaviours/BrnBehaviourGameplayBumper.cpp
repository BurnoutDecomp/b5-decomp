// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGameplayBumper slices this TU set
// owns:
//   - BehaviourGameplayBumper::Construct    @0x82242418   (vtable slot 0)
//   - BehaviourGameplayBumper::Prepare      @0x821F9640   (vtable slot 1)
//   - BehaviourGameplayBumper::GetName      @0x821F9670   (vtable slot 9)
//   - BehaviourGameplayBumper::SetParameters @0x821F39C0  (inline in the header; the
//     out-of-line anchor below forces its emission)
//   - BehaviourGameplayBumper::Parameters::Set @0x821F94C8  (defined here)
//
// SetParameters is adopted by the replay director and the roaming arbitrator state when they
// install a bumper-cam parameter block; Parameters::Set is the seeding step the main director
// runs (ProcessNewVehicleEvents / UpdateAttribSys) to populate a bumper-cam block from the
// vehicle's attribute-system source block before installing it.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"
#include "GameSource/Director/Camera/Behaviours/Behaviour.h"        // BehaviourSharedInfo
#include "GameSource/Director/Camera/Camera.h"                      // Camera::SetFOV / mState_uFlags
#include "GameSource/Director/Camera/Utils/CameraUtils.h"           // the Euler / rotate / angle-diff helpers
#include "GameSource/Director/Camera/SharedIO/BrnPlayerInfo.h"      // VehicleInfo (mAABB)
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "rw/math/fpu/scalar_operation.h"                         // Abs / Cos / Sin
#include "rw/math/vpu/vector3_operation.h"                        // operator+/-/* , Cross, Normalize, IsValid
#include "rw/math/vpu/matrix44affine_operation.h"                 // Mult(Matrix44Affine, Matrix44Affine)

// NOTE -- BehaviourGameplayBumper::Parameters::Serialise<S> (the field-walk visitor:
//   Serialise<DebugMenuSerialiser> @0x822308B8, <TextFileWriteSerialiser> @0x82230B68,
//   <TextFileReadSerialiser> @0x82214C70) moved out to the sibling TU file
//   BrnBehaviourGameplayBumperParameters.cpp when this TU was mounted in the game link
//   (2026-07-29, with the RE-BASE). It is the same split the external cam already had
//   (BrnBehaviourGameplayExternalParameters.cpp) and the aftertouch cam before that: the
//   visitor drags the three camera-tunings serialisers in, none of which is on the runtime
//   director path, so keeping it here would have forced them all into the exe.

namespace BrnDirector
{
namespace Camera
{


// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourGameplayBumper::Parameters::Set @0x821F94C8
//
// Seeds this bumper-cam parameter block: stamps the type tag + debug name + the fixed default
// tunables, then copies the per-vehicle tunables out of the attribute-system source array
// (reached through lpSource->mpfValues, i.e. *(a2+4)). Finally asserts the two FOV tunables
// are positive. Store-for-store from the asm: the default-tunable stores are issued with the
// X360's intermediate overwrites (e.g. +0x44 is seeded 0.11f then overwritten 1.0f); the
// final committed values are 0.0f/0.0f/3.0f/1.0f at +0x38/+0x3C/+0x40/+0x44.
//
// Float constants (decoded from the rodata loaded by the asm):
//   flt_820047C0 = 0.11f   flt_820047BC = 1.15f   flt_820047B8 = 0.06f (0.059999999)
//   flt_82001CC0 = 0.0f    flt_82001C98 = 1.0f    flt_82004270 = 3.0f
// ----------------------------------------------------------------------------
void BehaviourGameplayBumper::Parameters::Set(const Source* lpSource)
{
    mType = eBehaviourGameplayBumper;             // stw r9(=1), 0(r31)  (Behaviour::Parameters)

    // --- default tunables, in asm store order (intermediate values are overwritten) ---
    // ⭐ +0x38..+0x47 is the embedded CameraShake::Parameters (mImpactShakeParams); the four
    // stores below are its four fields, and their final values (0/0/3/1) are the SAME impact
    // shake BehaviourGameplayExternal::Parameters carries at its own +0x1C, where that
    // block's serialiser labels the quadruple "Impact Shake Params".
    mImpactShakeParams.mfWobbleCenteringFactor  = 0.11f;  // stfs flt_820047C0, 0x44  (seed)
    mImpactShakeParams.mfXYWobbleMagnitudeDegs  = 1.15f;  // stfs flt_820047BC, 0x40  (seed)
    mImpactShakeParams.mfXYShakeMagnitudeDegs   = 0.06f;  // stfs flt_820047B8, 0x38  (seed)
    mImpactShakeParams.mfZShakeMagnitudeDegs    = 0.0f;   // stfs flt_82001CC0, 0x3C
    SetDebugName("Bumper Cam");                           // stw  "Bumper Cam", 0x04
    mImpactShakeParams.mfXYShakeMagnitudeDegs   = 0.0f;   // stfs flt_82001CC0, 0x38  (override)
    mbIsValid = true;                                     // stb  r9(=1), 0x34
    mImpactShakeParams.mfZShakeMagnitudeDegs    = 0.0f;   // stfs flt_82001CC0, 0x3C
    mImpactShakeParams.mfWobbleCenteringFactor  = 1.0f;   // stfs flt_82001C98, 0x44  (override)
    mImpactShakeParams.mfXYWobbleMagnitudeDegs  = 3.0f;   // stfs flt_82004270, 0x40  (override)

    // --- per-vehicle tunables copied from the attribute-system source array ---
    const f32* lpfSrc = lpSource->mpfValues;      // lwz r11, 4(r4)  (re-loaded each store on X360)
    mfAccelerationDampening = lpfSrc[0x28 / 4];   // <- source[0x28]
    mfAccelerationResponse  = lpfSrc[0x24 / 4];   // <- source[0x24]
    mfBodyPitchScale        = lpfSrc[0x20 / 4];   // <- source[0x20]
    mfBodyRollScale         = lpfSrc[0x1C / 4];   // <- source[0x1C]
    mfBoostFOV              = lpfSrc[0x18 / 4];   // <- source[0x18]  (asserted > 0)
    mfFOV                   = lpfSrc[0x14 / 4];   // <- source[0x14]  (asserted > 0)
    mfPitchSpring           = lpfSrc[0x10 / 4];   // <- source[0x10]
    mfRollSpring            = lpfSrc[0x0C / 4];   // <- source[0x0C]
    mfYawSpring             = lpfSrc[0x08 / 4];   // <- source[0x08]
    mfYOffset               = lpfSrc[0x04 / 4];   // <- source[0x04]
    mfZOffset               = lpfSrc[0x00 / 4];   // <- source[0x00]

    CGS_ASSERT(mfBoostFOV > 0.0f, "mfBoostFOV > 0.0f");
    CGS_ASSERT(mfFOV > 0.0f, "mfFOV > 0.0f");
}

// ----------------------------------------------------------------------------
// BehaviourGameplayBumper::Construct @0x82242418  (vtable slot 0)
//
// asm, in issue order:
//   *(this+8)=0 *(this+9)=0 *(this+10)=0 *(this+11)=0 *(this+12)=0 *(this+4)=0 *(this+16)=0
//                                              <- the INLINED Behaviour::Construct (six fields
//                                                 + the debug-parameters name)
//   *(this+2088) = 0                            <- mpParameters
//   CameraShakeICEController::Construct(this+48)<- mBoostShake
//   *(this+32/36/40/44) = 0.0f                  <- mImpactShake's four wobble words (its own
//                                                 CameraShake::Construct, inlined)
//   *(this+20) = 0.0f                           <- mfImpactShakeFactor
//
// NOTE what Construct does NOT touch: mfTimeInJump, mbJumping, mLastCameraAngles, mfLastSpeed
// and mfDampenedAcceleration. The last three are Prepare's job; the first two are seeded by
// the (not-yet-transcribed) Update's jump machinery. Reproduced exactly -- no extra zeroing is
// invented.
// ----------------------------------------------------------------------------
void BehaviourGameplayBumper::Construct()
{
    Behaviour::Construct();          // the six base fields + mpcDebugParametersName

    mpParameters = 0;                // *(this + 2088) = 0
    mBoostShake.Construct();         // CameraShakeICEController::Construct(this + 48)
    mImpactShake.Construct();        // *(this + 32/36/40/44) = 0.0f
    mfImpactShakeFactor = 0.0f;      // *(this + 20) = 0.0f
}

// ----------------------------------------------------------------------------
// BehaviourGameplayBumper::Prepare @0x821F9640  (vtable slot 1)
//
// asm: stvx128 v0(=0), this+2064   -> mLastCameraAngles = (0,0,0)
//      *(this+2080) = 0.0f          -> mfLastSpeed
//      *(this+2084) = 0.0f          -> mfDampenedAcceleration
//      *(this+8)    = 1             -> mbIsPrepared
//      li r3,1; blr                 -> cannot fail
// The shared prepare/release block is not read (r4 is untouched).
// ----------------------------------------------------------------------------
bool BehaviourGameplayBumper::Prepare(const BehaviourSharedPrepareReleaseInfo& /*lrInfo*/)
{
    mLastCameraAngles.SetZero();                          // stvx128 v0(=0), this+2064
    mfLastSpeed            = 0.0f;                        // *(this + 2080)
    mfDampenedAcceleration = 0.0f;                        // *(this + 2084)

    SetPrepared();                                        // *(this + 8) = 1
    return true;
}

// @0x821F9670.
const char* BehaviourGameplayBumper::GetName() const
{
    return "GameplayBumper";
}

// ----------------------------------------------------------------------------
// BehaviourGameplayBumper::Parameters::Construct   (DWARF h:132)
//
// ⭐ BODIED 2026-08-02 (camera parameter-chain wave). It used to be declaration-only with the
// note "nothing on the live director path calls it, Set is what the attribute pump uses".
// That was true only while the parameter BANK had no layout: the console's
// BehaviourParameterBank::Construct @0x8223DC90 calls exactly this, INLINED, over its bumper
// block at bank+0x2538 -- and the bank is what SharedCameraContainer::Prepare binds the
// bumper camera to LONG BEFORE any car arrives.
//
// ⚠️⚠️ AND ONE OF ITS STORES IS LOAD-BEARING, MEASURED. The block's TYPE TAG is stamped HERE,
// not by Set. With the bank zero-filled instead (CAM_RUN2), BehaviourGameplayBumper::
// SetParameters' own tripwire `lpParameters->GetType() == eBehaviourGameplayBumper` fired
// four times per run, because a zeroed tag reads as the EXTERNAL camera's tag (0), not the
// bumper's (1). The external block gets away with it by accident -- its tag IS 0.
//
// The body is read straight off the bank's inlined copy (block base r10 = r31 + 0x2538):
//   0x8223DD38  stw  r30(=0), 4(r10)    -> mpcDebugName          = 0
//   0x8223DD3C  stw  r29(=1), 0(r10)    -> mType                 = eBehaviourGameplayBumper
//   0x8223DD50  stb  r30(=0), 0x34(r10) -> mbIsValid             = false
//   0x8223DD48  stfs f28,     0x38(r10) -> the inlined CameraShake::Parameters::Construct
//   0x8223DD4C  stfs f31,     0x3C(r10)    seed (0.06 / 0.0 / 1.15 / 0.11) over
//   0x8223DD44  stfs f22,     0x40(r10)    mImpactShakeParams @+0x38 ...
//   0x8223DD40  stfs f23,     0x44(r10)
//   0x8223DD5C  stfs f31(0.0f),  0x38(r10)  ... then the four overrides that leave it
//   0x8223DD60  stfs f31(0.0f),  0x3C(r10)      {0.0f, 0.0f, 3.0f, 1.0f} -- exactly the
//   0x8223DD58  stfs f0 (3.0f),  0x40(r10)      quadruple this header already identified as
//   0x8223DD54  stfs f27(1.0f),  0x44(r10)      one CameraShake::Parameters. Independent
//                                               confirmation of that identification.
// (rodata: flt_820047B8 0.06, flt_82001CC0 0.0, flt_820047BC 1.15, flt_820047C0 0.11,
//  flt_82001C98 1.0, flt_82004270 3.0.)
// Nothing else in the block is written -- the per-vehicle tunables are Set's job.
// ----------------------------------------------------------------------------
void BehaviourGameplayBumper::Parameters::Construct()
{
    mType = eBehaviourGameplayBumper;      // stw r29(=1), 0x00
    SetDebugName(0);                       // stw r30(=0), 0x04

    mImpactShakeParams.Construct();        // the inlined CameraShake::Parameters::Construct
    mImpactShakeParams.mfXYShakeMagnitudeDegs  = 0.0f;   // stfs flt_82001CC0, 0x38
    mImpactShakeParams.mfZShakeMagnitudeDegs   = 0.0f;   // stfs flt_82001CC0, 0x3C
    mImpactShakeParams.mfXYWobbleMagnitudeDegs = 3.0f;   // stfs flt_82004270, 0x40
    mImpactShakeParams.mfWobbleCenteringFactor = 1.0f;   // stfs flt_82001C98, 0x44

    mbIsValid = false;                     // stb r30(=0), 0x34
}

// Out-of-line anchor: forces BehaviourGameplayBumper::SetParameters (inline in the header) to
// be emitted in this TU.
void BehaviourGameplayBumper_SetParametersAnchor(
    BehaviourGameplayBumper& lrBehaviour,
    const BehaviourGameplayBumper::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}


// ============================================================================================
// BehaviourGameplayBumper::Update  @0x82226778   (686 insns, 0x82226778..0x8222722C)
//
// THE BUMPER CAMERA -- and, because SharedCameraContainer::GetSelectedGameplayCamera hands out
// mGameplayBumper whenever mbLookbackOverride is set, THE REAR VIEW. It was never transcribed
// and not even declared, so the vtable slot kept Behaviour::Update's default (return true,
// touch nothing) and the camera froze wherever BehaviourHelper::Prepare left it. That is what
// the owner saw the first time lookback could actually engage (b5 e82b28f5).
//
// PROVENANCE. Control flow and every scalar expression are transcribed from the ARTIST
// Hex-Rays export (.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82226778.json), which lifts the
// scalar half exactly and leaves the VMX as __asm. The vector half is therefore written as the
// OPERATIONS that asm performs, through this tree's own named helpers, rather than instruction
// for instruction: a normalise, an orthonormal basis, a constant-angle yaw, and a transform
// compose. Each is marked below with the asm range it stands for.
//
// The two asserts and their source lines are the console's own (:115, :179, :204), as is the
// unconditional `return true` (li r3, 1 @0x82227214) -- including the invalid-parameters path,
// which branches straight to it.
// ============================================================================================
bool BehaviourGameplayBumper::Update(Camera& lrCamera, const BehaviourSharedInfo& lrInfo)
{
    // @0x822267A0..0x822267DC
    CGS_ASSERT(mpParameters != 0, "Updating with no parameters");   // :115
    if (mpParameters == 0 || !mpParameters->mbIsValid)
        return true;

    const BrnPhysics::Vehicle::RaceCarState& lrCar = lrInfo.mPlayerInfo.mRaceCarState;
    const Matrix44Affine& lrCarTransform = lrCar.mTransform;

    // ---- 1. the surface the FRONT wheels are standing on -----------------------------------
    // @0x822267E4..0x82226850. Each front wheel contributes its road normal only while it is
    // actually on the ground; with neither grounded the car's own up axis stands in.
    const BrnPhysics::Vehicle::Wheel::RoadContact& lrFrontL = lrCar.maWheels[0].mRoadContact;
    const BrnPhysics::Vehicle::Wheel::RoadContact& lrFrontR = lrCar.maWheels[1].mRoadContact;

    Vector3 lSurfaceUp = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (lrFrontL.mbIsOnGround)
        lSurfaceUp = lSurfaceUp + lrFrontL.mNormal;
    if (lrFrontR.mbIsOnGround)
        lSurfaceUp = lSurfaceUp + lrFrontR.mNormal;
    if (!lrFrontL.mbIsOnGround && !lrFrontR.mbIsOnGround)
        lSurfaceUp = lrCarTransform.yAxis;

    // @0x82226854..0x822268F4. `vandc` against the splatted sign mask is a per-lane absolute
    // value; the compare is `vcmpgtfp.` against a splat of flt_82001770 (dumped: FLT_EPSILON)
    // and the branch taken is the CR6 "none true" bit -- i.e. fall back only when NO lane
    // exceeds it. The fallback is world up, built on the stack as {0, 1, 0}.
    const f32 KF_DEGENERATE_EPSILON = 1.1920928955078125e-07f;   // flt_82001770, dumped
    Vector3 lBlendedUp;
    if (rw::math::fpu::Abs(lSurfaceUp.x) > KF_DEGENERATE_EPSILON
        || rw::math::fpu::Abs(lSurfaceUp.y) > KF_DEGENERATE_EPSILON
        || rw::math::fpu::Abs(lSurfaceUp.z) > KF_DEGENERATE_EPSILON)
    {
        lBlendedUp = rw::math::vpu::Normalize(lSurfaceUp);   // vrsqrtefp + two Newton steps
    }
    else
    {
        lBlendedUp = Vector3{ 0.0f, 1.0f, 0.0f, 0.0f };
    }

    // ---- 2. lean that surface back toward the car's own up ---------------------------------
    // @0x82226900..0x82226960:  vsubfp(carUp, surfaceUp) then vmaddcfp128 against a splat of
    // unk_82008758 (dumped: 0.6f) with the surface up as the addend -- a plain lerp.
    const f32 KF_UP_BLEND_TOWARD_CAR = 0.6f;                     // unk_82008758, dumped
    lBlendedUp = lBlendedUp
               + (lrCarTransform.yAxis - lBlendedUp) * KF_UP_BLEND_TOWARD_CAR;

    // ---- 3. the orthonormal frame the camera starts from -----------------------------------
    // @0x82226964..0x82226A4C. The console open-codes it: two cross products (the vpermwi128
    // 0x63 swizzles) with a vrsqrtefp normalise between them, against the car's forward
    // (mTransform.zAxis) and the blended up. Written here as those operations.
    Vector3 lForward = lrCarTransform.zAxis;
    Vector3 lRight   = rw::math::vpu::Normalize(rw::math::vpu::Cross(lBlendedUp, lForward));
    Vector3 lUp      = rw::math::vpu::Cross(lForward, lRight);

    Matrix44Affine lCameraFrame;
    lCameraFrame.xAxis = lRight;
    lCameraFrame.yAxis = lUp;
    lCameraFrame.zAxis = lForward;
    lCameraFrame.wAxis = Vector3{ 0.0f, 0.0f, 0.0f, 1.0f };

    // @0x8222691C..0x82226994: `ld r7, 0x140(camera)` / `ori r6, r7, 2` / `std r6, 0x140` --
    // the camera's own state-flag word gains bit 1 for this frame.
    lrCamera.mState_uFlags |= 2;

    // ---- 4. the angles that frame implies --------------------------------------------------
    // @0x82226A50. lpLastAngles disambiguates the near-vertical branch toward last frame's
    // answer; the epsilon is flt_82002138 (dumped 0.01f), which is this helper's own default.
    const Vector3 lTargetCameraAngles =
        Utils::EulerAnglesZXYFromMatrix44Affine(lCameraFrame, &mLastCameraAngles, 0.0099999998f);

    // @0x82226A54..0x82226ADC -- three per-lane self-compares (vcmpeqfp., the NaN test).
    CGS_ASSERT(rw::math::vpu::IsValid(lTargetCameraAngles), "IsValid(lTargetCameraAngles)");  // :179

    // ---- 5. the springs --------------------------------------------------------------------
    // @0x82226AE0..0x82226B30. While the car is crashing every axis springs at 1 (the camera
    // is pinned to the target); otherwise each axis uses its authored rate, and ROLL is scaled
    // by a tenth (`lfs +0x20` then fmuls against flt_82004014, dumped 0.1f).
    const f32 KF_ROLL_SPRING_SCALE = 0.1f;                       // flt_82004014, dumped
    Vector3 lSprings;
    if (lrCar.mbResetCarTransform)                                 // sharedInfo +0x4AE
    {
        lSprings = Vector3{ 1.0f, 1.0f, 1.0f, 0.0f };
    }
    else
    {
        lSprings = Vector3{ mpParameters->mfPitchSpring,
                            mpParameters->mfYawSpring,
                            mpParameters->mfRollSpring * KF_ROLL_SPRING_SCALE,
                            0.0f };
    }
    const Vector3 lOneMinusSprings = Vector3{ 1.0f, 1.0f, 1.0f, 0.0f } - lSprings;

    // @0x82226B64 -- sub_82222598, which splats three lanes through the scalar
    // Utils::GetSmallestDifferenceBetweenRadAngles @0x821F8988.
    Vector3 lCameraDiffAngles =
        Utils::GetSmallestDifferenceBetweenRadAngles(mLastCameraAngles, lTargetCameraAngles);
    CGS_ASSERT(rw::math::vpu::IsValid(lCameraDiffAngles), "IsValid(lCameraDiffAngles)");      // :204

    // ---- 6. the acceleration the camera pitches against ------------------------------------
    // @0x82226BF0..0x82226C2C, transcribed from the Hex-Rays scalar line verbatim: a one-pole
    // low-pass on (this frame's speed - last frame's speed).
    mfDampenedAcceleration =
        ((lrCar.mfSpeedMPH - mfLastSpeed) - mfDampenedAcceleration)
            * mpParameters->mfAccelerationDampening
        + mfDampenedAcceleration;

    // @0x82226C40..0x82226C50: the response lands on the PITCH lane only (`vrlimi128 v0, v13,
    // 8, 0` inserts lane 0 of the broadcast into the diff angles).
    lCameraDiffAngles.x += mpParameters->mfAccelerationResponse * mfDampenedAcceleration;

    // ---- 7. LOOKBACK: swing the frame through half a turn -----------------------------------
    // @0x82226C54..0x82226E14, gated on sharedInfo +0x26 == mRotationController.mbIsLookback.
    // The console feeds the CONSTANT flt_82004964 (dumped: -pi) through the inlined
    // XMVectorSinCos polynomial and rotates the frame's xAxis and zAxis by the result --
    //     xAxis' = xAxis*cos - zAxis*sin
    //     zAxis' = xAxis*sin + zAxis*cos
    // -- which for -pi is exactly a 180-degree yaw about the frame's up. THIS is the rear view.
    const bool lbLookback = lrInfo.mRotationController.IsLookback();
    if (lbLookback)
    {
        const f32 KF_LOOKBACK_YAW_RADS = -3.1415927410125732f;   // flt_82004964, dumped
        const f32 lfCos = rw::math::fpu::Cos(KF_LOOKBACK_YAW_RADS);
        const f32 lfSin = rw::math::fpu::Sin(KF_LOOKBACK_YAW_RADS);

        const Vector3 lRotatedX = lCameraFrame.xAxis * lfCos - lCameraFrame.zAxis * lfSin;
        const Vector3 lRotatedZ = lCameraFrame.xAxis * lfSin + lCameraFrame.zAxis * lfCos;
        lCameraFrame.xAxis = lRotatedX;
        lCameraFrame.zAxis = lRotatedZ;
    }

    // ---- 8. where the eye sits ---------------------------------------------------------------
    // @0x82226E18..0x82226EB8. The Z reach is (mHalfExtent.z - mfZOffset), measured from the
    // REAR corner of the car's AABB while looking back and from the FRONT corner otherwise --
    // so the rear view rides the back bumper. Y is the authored offset; X is zero.
    const f32 lfZReach = lrCar.mHalfExtent.z - mpParameters->mfZOffset;
    const f32 lfEyeZ = lbLookback ? (lrInfo.mPlayerInfo.mAABB.mMin.z + lfZReach)
                                  : (lrInfo.mPlayerInfo.mAABB.mMax.z - lfZReach);

    lCameraFrame.wAxis = Vector3{ 0.0f, mpParameters->mfYOffset, lfEyeZ, 1.0f };

    // @0x82226EB0/0x82226EBC -- the sprung angles are carried forward, and the frame is turned
    // back by the UNsprung remainder (`vxor` against the sign mask is the negation).
    mLastCameraAngles = mLastCameraAngles + rw::math::vpu::Mult(lCameraDiffAngles, lSprings);
    const Vector3 lResidualAngles = rw::math::vpu::Mult(
        Vector3{ 0.0f, 0.0f, 0.0f, 0.0f } - lCameraDiffAngles, lOneMinusSprings);

    // @0x82226EC0
    Utils::RotateMatrix44AffineByEulerAnglesZXY(lCameraFrame, lResidualAngles);

    // ---- 9. compose into world space and publish --------------------------------------------
    // @0x82226EC4..0x82226F80 -- the vmaddfp128 ladder over v126/v127/v120 is the frame times
    // the car's transform, row by row; the console then validates what it wrote.
    lrCamera.mTransform = rw::math::vpu::Mult(lCameraFrame, lrCarTransform);
    lrCamera.ValidateTransformWithDebugInfo();

    // ---- 10. the FOV -------------------------------------------------------------------------
    // @0x82226F84..0x82226FB8. The boosted FOV is the authored one plus the director's own
    // temp-boost amount scaled by flt_820054CC (dumped 20.0f), and the frame's FOV is a lerp
    // from the resting FOV toward it by mfSpeedRatio.
    mfLastSpeed = lrCar.mfSpeedMPH;                               // stfs 0x42C -> this+0x820

    const f32 KF_FOV_BOOST_SCALE = 20.0f;                         // flt_820054CC, dumped
    const f32 lfBoostedFOV =
        lrInfo.mfTempFOVBoostAmount * KF_FOV_BOOST_SCALE + mpParameters->mfBoostFOV;
    lrCamera.SetFOV((lfBoostedFOV - mpParameters->mfFOV) * lrInfo.mfSpeedRatio
                    + mpParameters->mfFOV);

    return true;                                                  // li r3, 1 @0x82227214
}

} // namespace Camera
} // namespace BrnDirector
