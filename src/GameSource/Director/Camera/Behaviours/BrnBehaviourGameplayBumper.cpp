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

// Out-of-line anchor: forces BehaviourGameplayBumper::SetParameters (inline in the header) to
// be emitted in this TU.
void BehaviourGameplayBumper_SetParametersAnchor(
    BehaviourGameplayBumper& lrBehaviour,
    const BehaviourGameplayBumper::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

} // namespace Camera
} // namespace BrnDirector
