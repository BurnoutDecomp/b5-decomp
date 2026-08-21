#include "GameSource/Director/Camera/Utils/BrnCameraSphericalRotationController.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT
#include "rw/math/fpu/scalar_operation.h"                    // rw::math::fpu::{Abs, IsValid}

// BrnDirector::Camera::Utils::CameraSphericalRotationController -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions):
//   CameraSphericalRotationController::Update    @0x8223F808
//   CameraSphericalRotationController::Construct (inlined on the console -- see its banner)
//
// Update (asm walk):
//   * build the pitch SmoothMover::Parameters block (mMax/mMin = the passed pitch limits,
//     the remaining fields are the fixed tuning constants stored store-for-store);
//   * assert IsValid(lfMaxPitch) -- the console self-compares f31 (fcmpu f31,f31) and, on
//     NaN, streams a diagnostic through the assert manager; folded to a static CGS_ASSERT;
//   * roll the lookback edge flags (mbWasLookbackLastFrame = mbIsLookback; mbIsLookback =
//     lbLookback);
//   * YAW: when NOT paused, ease the return rate toward 0.075, decay the yaw toward zero
//     by it, and -- if the look stick is pushed past the dead zone -- spring the yaw by
//     stick*spring*dt (the spring softens near +/-179 degs) then clamp to [-179,179]; the
//     yaw velocity is the resulting per-frame delta / dt, and the pitch mover runs WITH
//     centering. When paused, drive the yaw VELOCITY toward stick*90 (or toward 0 inside
//     the dead zone) at 0.1, integrate it, wrap via GetSmallestDifferenceBetweenDegsAngles,
//     run the pitch mover with NO centering, and zero the return rate;
//   * TAIL: if |mfYawDegs| <= 1.1920929e-7 the controller is un-rotated
//     (mbIsRotated = false, accumulate mfUnRotatedTime); otherwise it is rotated
//     (mbIsRotated = true, mfUnRotatedTime = 0).

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
    // Forward decl for the yaw-wrap helper (its own ledger function @ CameraUtils.cpp,
    // not yet declared in CameraUtils.h). Returns the signed smallest angular delta (degs).
    f32 GetSmallestDifferenceBetweenDegsAngles(f32 lfFrom, f32 lfTo);

namespace
{
    namespace fpu = rw::math::fpu;

    // --- pitch SmoothMover tuning (stored into lPitchMoverParams, store-for-store) -------
    const f32 KF_PITCH_DAMPENING_RANGE  = 8.0f;    // flt_82004C88  (mfDampeningRange)
    const f32 KF_PITCH_SPEED            = 45.0f;   // flt_82009B80  (mfMaxSpeed)
    const f32 KF_DEAD_ZONE              = 0.1f;    // flt_82004014  (mfDeadZoneHalfSize)
    const f32 KF_PITCH_BRAKING_LAG      = 0.5f;    // flt_82001DA0  (mfBrakingLag)
    const f32 KF_PITCH_RETURN_RATE      = 0.1f;    // flt_82004014  (mfNormalLag)
    const f32 KF_PITCH_CENTERING_RATE   = 0.05f;   // flt_820047C8  (mfCenteringRate)
    const f32 KF_PITCH_CENTERING_BLEND  = 0.05f;   // flt_820047C8  (mfCenteringRateBlend)

    // --- yaw tuning ----------------------------------------------------------------------
    const f32 KF_MOVING_YAW_RETURN_RATE = 0.1f;     // flt_82004014 (stick dead-zone threshold)
    const f32 KF_YAW_RETURN_TARGET      = 0.075f;   // flt_82001B34 (return rate eased toward this)
    const f32 KF_YAW_SPEED              = 90.0f;    // flt_82004F64 (paused: stick*90 -> yaw velocity)
    const f32 KF_YAW_RETURN_BLEND       = 0.1f;     // flt_82004014 rate the yaw VELOCITY chases its target (paused)
    const f32 KF_YAW_SPRING             = 750.0f;   // flt_82001B2C (base yaw spring)
    const f32 KF_YAW_SPRING_FALLOFF     = 0.1f;     // flt_82004014-scaled softening near the limits
    const f32 KF_YAW_MAX                = 179.0f;   // flt_82001B3C (clamp / softening limit, +deg)
    const f32 KF_YAW_MIN                = -179.0f;  // flt_82001B40 (clamp / softening limit, -deg)
    const f32 KF_YAW_SPRING_BAND        = 169.0f;   // flt_82009C84/flt_82009C80 (+/- soft-spring band)
    const f32 KF_YAW_ROTATED_EPSILON    = 1.1920929e-7f;  // flt_82001770 / flt_82002514 (+/-)

    // flt_82CDAD50 -- the lerp amount easing mfYawReturnRate toward KF_YAW_RETURN_TARGET.
    // ⛔ VALUE CORRECTION 2026-08-20: this was a PLACEHOLDER 0.1f carrying an explicit
    // "FLAG: value unattested" note (Hex-Rays left the constant symbolic). DUMPED now from
    // the decrypted ARTIST image -- file_off = 0x3000 + 0x82CDAD50 - 0x82000000 = 0xCDDD50,
    // big-endian word 0x3D4CCCCD == 0.05f. The guess was 2x TOO LARGE, i.e. the camera's
    // yaw returned to centre at twice the console's rate, which reads as the chase camera
    // being pulled back in line with the car instead of trailing it.
    const f32 KF_YAW_RETURN_RATE_LERP   = 0.05f;   // flt_82CDAD50 (0x3D4CCCCD, dumped)

    const f32 KF_ZERO = 0.0f;   // flt_82001CC0
}

// @ 0x8223F808  (DWARF declares void; the X360 tail-returns the SmoothMover::Update result
// through r3, which the void DWARF prototype discards.)
void CameraSphericalRotationController::Update(f32 lfTimestep, Vector2 lStick,
                                               bool lbLookback, bool lbPaused,
                                               f32 lfMinPitch, f32 lfMaxPitch)
{
    // Build the pitch SmoothMover parameters (with-limits; centering decided per branch).
    SmoothMover::Parameters lPitchMoverParams;
    lPitchMoverParams.mfMaxValue           = lfMaxPitch;
    lPitchMoverParams.mfMinValue           = lfMinPitch;
    lPitchMoverParams.mfDampeningRange     = KF_PITCH_DAMPENING_RANGE;
    lPitchMoverParams.mfMaxSpeed           = KF_PITCH_SPEED;
    lPitchMoverParams.mfDeadZoneHalfSize   = KF_DEAD_ZONE;
    lPitchMoverParams.mfBrakingLag         = KF_PITCH_BRAKING_LAG;
    lPitchMoverParams.mfNormalLag          = KF_PITCH_RETURN_RATE;
    lPitchMoverParams.mfCenteringRate      = KF_PITCH_CENTERING_RATE;
    lPitchMoverParams.mfCenteringRateBlend = KF_PITCH_CENTERING_BLEND;
    lPitchMoverParams.mbUseCentering       = false;
    lPitchMoverParams.mbUseLimits          = true;

    // fcmpu f31,f31 -- NaN self-compare on the max-pitch limit; the console streams a
    // diagnostic on failure (folded to a static assert per convention).
    CGS_ASSERT(fpu::IsValid(lfMaxPitch), "RwMathFPU::IsValid(lfMaxPitch)");

    mStickVector = lStick;

    // Roll the lookback edge flags.
    mbWasLookbackLastFrame = mbIsLookback;
    mbIsLookback           = lbLookback;

    if (!lbPaused)
    {
        // Ease the return rate toward its target, then decay the yaw toward zero by it.
        const f32 lfOldYawDegs = mfYawDegs;
        mfYawReturnRate += (KF_YAW_RETURN_TARGET - mfYawReturnRate) * KF_YAW_RETURN_RATE_LERP;
        mfYawDegs = -lfOldYawDegs * mfYawReturnRate + lfOldYawDegs;

        if (fpu::Abs(mStickVector.x) > KF_MOVING_YAW_RETURN_RATE)
        {
            // Stick pushed past the dead zone: spring the yaw. The spring softens as the
            // yaw approaches +/-179 degs (outside the +/-169 band).
            f32 lfSpring = KF_YAW_SPRING;
            if (mfYawDegs > KF_YAW_SPRING_BAND || mfYawDegs < -KF_YAW_SPRING_BAND)
            {
                const f32 lfLimit = (mfYawDegs > KF_YAW_SPRING_BAND) ? KF_YAW_MAX : KF_YAW_MIN;
                lfSpring = -((-((fpu::Abs(lfLimit - mfYawDegs) * KF_YAW_SPRING_FALLOFF) - 1.0f)
                                 * KF_YAW_SPRING)
                             - KF_YAW_SPRING);
            }

            mfYawDegs += mStickVector.x * lfSpring * lfTimestep;

            // Clamp to [-179, 179] (two fsel, in asm order: low side then high side).
            mfYawDegs = (KF_YAW_MIN - mfYawDegs >= 0.0f) ? KF_YAW_MIN : mfYawDegs;  // max(yaw,-179)
            mfYawDegs = (KF_YAW_MAX - mfYawDegs >= 0.0f) ? mfYawDegs : KF_YAW_MAX;  // min(yaw, 179)
        }

        mfYawVelocity = (mfYawDegs - lfOldYawDegs) / lfTimestep;

        lPitchMoverParams.mbUseCentering = true;
        mPitchMover.Update(lfTimestep, mStickVector.y, lPitchMoverParams);
    }
    else
    {
        // Paused: drive the yaw VELOCITY toward stick*90 (or toward 0 in the dead zone),
        // integrate, and wrap into [-180, 180].
        f32 lfTargetDelta;
        if (fpu::Abs(mStickVector.x) > KF_MOVING_YAW_RETURN_RATE)
            lfTargetDelta = mStickVector.x * KF_YAW_SPEED - mfYawVelocity;
        else
            lfTargetDelta = -mfYawVelocity;

        mfYawVelocity += lfTargetDelta * KF_YAW_RETURN_BLEND;
        mfYawDegs     += mfYawVelocity * lfTimestep;
        mfYawDegs      = GetSmallestDifferenceBetweenDegsAngles(KF_ZERO, mfYawDegs);

        lPitchMoverParams.mbUseCentering = false;
        mPitchMover.Update(lfTimestep, mStickVector.y, lPitchMoverParams);
        mfYawReturnRate = KF_ZERO;
    }

    // Rotated tracking: near-zero yaw counts as un-rotated (accumulate the un-rotated time).
    if (mfYawDegs > KF_YAW_ROTATED_EPSILON || mfYawDegs < -KF_YAW_ROTATED_EPSILON)
    {
        mbIsRotated     = true;
        mfUnRotatedTime = KF_ZERO;
    }
    else
    {
        mbIsRotated      = false;
        mfUnRotatedTime += lfTimestep;
    }
}

// ============================================================================
// CameraSphericalRotationController::Construct
//
// ⛔⛔ THIS WAS AN EMPTY STUB IN GameSource/Director/DirectorLinkStubs.cpp UNTIL 2026-08-01,
// and BOTH halves of the note that justified it had expired:
//   "the console INLINES it into each owner (so there is no standalone body to read...)"
//       -- true, and NOT a reason to leave it empty: the inlining is the body. Three owners
//          emit the identical ten stores, which is three independent transcriptions of it.
//   "Nothing reads either one ... MainDirector::UpdateCameraBehavioursPostScene (the only
//    path that would dispatch it) is gated."
//       -- the post-scene behaviour pass was un-gated on 2026-08-01, and
//          BehaviourRotateAboutVehicle::BecomeSimilarTo calls this on the LIVE car-select
//          path for the express purpose of discarding accumulated stick state. With the
//          empty stub the stale yaw/pitch survived every re-seat, and
//          BehaviourRotateAboutVehicle::Construct left the whole controller at whatever the
//          pool slot happened to hold.
//
// THE TEN STORES (identical in all three witnesses; displacements are controller-relative):
//   BehaviourRotateAboutVehicle::Construct   @0x8222BF68..0x8222BF90  (behaviour +0x20)
//   BehaviourRotateAboutVehicle::BecomeSimilarTo @0x8224A4D8..0x8224A508 (same member)
//   BehaviourGameplayExternal::Construct     @0x82224A18              (its own +0x20)
//     stvx128 0, +0x00                 -> mStickVector (all four lanes)
//     stfs    0, +0x10/+0x14/+0x18/+0x1C-> mfYawDegs / mfYawVelocity / mfYawReturnRate /
//                                          mfUnRotatedTime
//     stb     0, +0x20/+0x21/+0x22     -> mbIsLookback / mbWasLookbackLastFrame / mbIsRotated
//     stfs    0, +0x28/+0x2C           -> mPitchMover.mfCurrentSpeed / .mfCurrentValue
//
// ⚠️ mPitchMover.mfCenteringRate (+0x24) IS DELIBERATELY NOT WRITTEN -- it is the one
//   SmoothMover field absent from all three store sets, in the middle of a run that writes
//   its two neighbours, so its omission is a decision and not a transcription gap: a reset
//   keeps the centering rate the mover has eased to. Reproduced exactly.
// ============================================================================
void CameraSphericalRotationController::Construct()
{
    mStickVector.x = 0.0f;
    mStickVector.y = 0.0f;
    mStickVector.z = 0.0f;
    mStickVector.w = 0.0f;

    mfYawDegs       = 0.0f;
    mfYawVelocity   = 0.0f;
    mfYawReturnRate = 0.0f;
    mfUnRotatedTime = 0.0f;

    mbIsLookback           = false;
    mbWasLookbackLastFrame = false;
    mbIsRotated            = false;

    // NOT mPitchMover.mfCenteringRate -- see the banner.
    mPitchMover.mfCurrentSpeed = 0.0f;
    mPitchMover.mfCurrentValue = 0.0f;
}
}
}
}
