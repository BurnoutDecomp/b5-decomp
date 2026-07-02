#include "GameSource/Director/Camera/Utils/BrnCameraSmoothMover.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT
#include "rw/math/fpu/scalar_operation.h"              // rw::math::fpu::{Abs, Clamp, IsValid}

// BrnDirector::Camera::Utils::SmoothMover -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (3 ledger functions + the two attested assert stubs, DWARF primary file
// GameSource/Director/Camera/Utils/BrnCameraSmoothMover.cpp):
//   SmoothMover::Update                      @0x82223800
//   SmoothMover::UpdateWithLimitsNoCentering @0x8220D820
//   SmoothMover::UpdateWithLimitsWithCentering @0x8220D608
//
// Update (asm walk): dispatch on lParameters.mbUseLimits (+0x25) / mbUseCentering (+0x24);
// the no-limits legs collapse to the "Not implemented yet" asserts at cpp:82 / cpp:93 --
// those are the two private no-limits variants' whole bodies, inlined; de-inlined back to
// the DWARF-declared privates here.
//
// UpdateWithLimitsNoCentering (asm walk):
//   * assert 0 < lfTimestep < 1 (cpp:152; the X360 streams the timestep value into the
//     message -- folded to a static CGS_ASSERT per convention);
//   * inside the dead zone (|force| <= mfDeadZoneHalfSize): decay the speed by the normal
//     lag (speed += mfNormalLag * -speed);
//   * otherwise pick the drive speed: within mfDampeningRange of the max limit while
//     pushing up (force > 0), or of the min limit while pushing down (force < 0), the max
//     speed scales down linearly with the remaining distance
//     (adj = (1 - |limit - value| / range) * -maxSpeed + maxSpeed) and the BRAKING lag
//     blends toward it; else the plain maxSpeed target with the NORMAL lag:
//     speed += lag * (adj * force - speed);
//   * commit: mfCurrentSpeed = speed, mfCenteringRate = 0, integrate
//     mfCurrentValue = Clamp(value + speed * dt, min, max);
//   * six tripwire asserts (cpp:187-193): |speed| < 10000, |value| < 10000 (the value
//     overflow assert streams a full diagnostic -- folded static), IsValid(speed),
//     IsValid(value). NOTE: Hex-Rays drops the two IsValid asserts; they are in the asm
//     (fcmpu f0,f0 NaN self-compares @0x8220DB44/@0x8220DB80).
//
// UpdateWithLimitsWithCentering (asm walk):
//   * blend the live centering rate toward the parameter rate:
//     mfCenteringRate += (lParameters.mfCenteringRate - mfCenteringRate) *
//     mfCenteringRateBlend (the PS3 hint's Lerp);
//   * inside the dead zone: recentre the VALUE directly
//     (value += mfCenteringRate * -value); the speed is left alone;
//   * otherwise the same band-dampened drive speed as the no-centering variant but with
//     NO lag blend (mfCurrentSpeed = adj * force) and no force-sign gate on the bands,
//     then integrate + clamp;
//   * the same six tripwire asserts (cpp:135-141).

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{
namespace
{
    namespace fpu = rw::math::fpu;

    const f32 KF_SANITY_LIMIT = 10000.0f;   // flt_82005D9C / flt_82006C48 (+/-)
}

// @ 0x82223800
void SmoothMover::Update(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne,
                         const Parameters& lParameters)
{
    if (lParameters.mbUseLimits)
    {
        if (lParameters.mbUseCentering)
            UpdateWithLimitsWithCentering(lfTimestep, lfForceAppliedMinusOneToOne, lParameters);
        else
            UpdateWithLimitsNoCentering(lfTimestep, lfForceAppliedMinusOneToOne, lParameters);
    }
    else
    {
        if (lParameters.mbUseCentering)
            UpdateNoLimitsWithCentering(lfTimestep, lfForceAppliedMinusOneToOne, lParameters);
        else
            UpdateNoLimitsNoCentering(lfTimestep, lfForceAppliedMinusOneToOne, lParameters);
    }
}

// cpp:80 -- the whole X360 body is the assert (cpp:82; inlined into Update @0x82223854).
void SmoothMover::UpdateNoLimitsWithCentering(f32, f32, const Parameters&)
{
    CGS_ASSERT(false, "Not implemented yet");
}

// cpp:91 -- the whole X360 body is the assert (cpp:93; inlined into Update @0x82223860).
void SmoothMover::UpdateNoLimitsNoCentering(f32, f32, const Parameters&)
{
    CGS_ASSERT(false, "Not implemented yet");
}

// @ 0x8220D820
void SmoothMover::UpdateWithLimitsNoCentering(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne,
                                              const Parameters& lParameters)
{
    // cpp:152 -- the X360 streams the offending timestep into the message; folded static.
    CGS_ASSERT(lfTimestep > 0.0f && lfTimestep < 1.0f,
               "lfTimestep > 0.0f && lfTimestep < 1.0f");

    f32 lfNewSpeed;
    if (fpu::Abs(lfForceAppliedMinusOneToOne) <= lParameters.mfDeadZoneHalfSize)
    {
        // No meaningful input: brake the speed toward zero at the normal lag.
        lfNewSpeed = mfCurrentSpeed + lParameters.mfNormalLag * -mfCurrentSpeed;
    }
    else
    {
        f32 lfAdjustedSpeed = lParameters.mfMaxSpeed;
        f32 lfLag           = lParameters.mfNormalLag;

        if (mfCurrentValue > lParameters.mfMaxValue - lParameters.mfDampeningRange &&
            lfForceAppliedMinusOneToOne > 0.0f)
        {
            // Pushing up into the max-limit dampening band: scale the drive speed down
            // linearly with the remaining distance and brake-lag toward it.
            lfAdjustedSpeed = (1.0f - fpu::Abs(lParameters.mfMaxValue - mfCurrentValue)
                                          / lParameters.mfDampeningRange)
                                  * -lParameters.mfMaxSpeed
                              + lParameters.mfMaxSpeed;
            lfLag = lParameters.mfBrakingLag;
        }
        else if (mfCurrentValue < lParameters.mfMinValue + lParameters.mfDampeningRange &&
                 lfForceAppliedMinusOneToOne < 0.0f)
        {
            lfAdjustedSpeed = (1.0f - fpu::Abs(lParameters.mfMinValue - mfCurrentValue)
                                          / lParameters.mfDampeningRange)
                                  * -lParameters.mfMaxSpeed
                              + lParameters.mfMaxSpeed;
            lfLag = lParameters.mfBrakingLag;
        }

        lfNewSpeed = (lfAdjustedSpeed * lfForceAppliedMinusOneToOne - mfCurrentSpeed) * lfLag
                     + mfCurrentSpeed;
    }

    mfCurrentSpeed  = lfNewSpeed;
    mfCenteringRate = 0.0f;
    mfCurrentValue  = fpu::Clamp(mfCurrentValue + lfNewSpeed * lfTimestep,
                                 lParameters.mfMinValue, lParameters.mfMaxValue);

    CGS_ASSERT(mfCurrentSpeed < KF_SANITY_LIMIT,  "mfCurrentSpeed < 10000.0f");
    // cpp:188 -- the X360 streams value/speed/params/force into the message; folded static.
    CGS_ASSERT(mfCurrentValue < KF_SANITY_LIMIT,  "mfCurrentValue < 10000.0f");
    CGS_ASSERT(mfCurrentSpeed > -KF_SANITY_LIMIT, "mfCurrentSpeed > -10000.0f");
    CGS_ASSERT(mfCurrentValue > -KF_SANITY_LIMIT, "mfCurrentValue > -10000.0f");
    CGS_ASSERT(fpu::IsValid(mfCurrentSpeed), "RwMathFPU::IsValid(mfCurrentSpeed)");
    CGS_ASSERT(fpu::IsValid(mfCurrentValue), "RwMathFPU::IsValid(mfCurrentValue)");
}

// @ 0x8220D608
void SmoothMover::UpdateWithLimitsWithCentering(f32 lfTimestep, f32 lfForceAppliedMinusOneToOne,
                                                const Parameters& lParameters)
{
    // Blend the live centering rate toward the requested one.
    mfCenteringRate += (lParameters.mfCenteringRate - mfCenteringRate)
                       * lParameters.mfCenteringRateBlend;

    if (fpu::Abs(lfForceAppliedMinusOneToOne) <= lParameters.mfDeadZoneHalfSize)
    {
        // No meaningful input: recentre the value directly (the speed is left alone).
        mfCurrentValue += mfCenteringRate * -mfCurrentValue;
    }
    else
    {
        f32 lfAdjustedSpeed = lParameters.mfMaxSpeed;

        if (mfCurrentValue > lParameters.mfMaxValue - lParameters.mfDampeningRange)
        {
            lfAdjustedSpeed = (1.0f - fpu::Abs(lParameters.mfMaxValue - mfCurrentValue)
                                          / lParameters.mfDampeningRange)
                                  * -lParameters.mfMaxSpeed
                              + lParameters.mfMaxSpeed;
        }
        else if (mfCurrentValue < lParameters.mfMinValue + lParameters.mfDampeningRange)
        {
            lfAdjustedSpeed = (1.0f - fpu::Abs(lParameters.mfMinValue - mfCurrentValue)
                                          / lParameters.mfDampeningRange)
                                  * -lParameters.mfMaxSpeed
                              + lParameters.mfMaxSpeed;
        }

        // Direct drive (no lag blend in the centering variant), integrate and clamp.
        mfCurrentSpeed = lfAdjustedSpeed * lfForceAppliedMinusOneToOne;
        mfCurrentValue = fpu::Clamp(mfCurrentValue + mfCurrentSpeed * lfTimestep,
                                    lParameters.mfMinValue, lParameters.mfMaxValue);
    }

    CGS_ASSERT(mfCurrentSpeed < KF_SANITY_LIMIT,  "mfCurrentSpeed < 10000.0f");
    CGS_ASSERT(mfCurrentValue < KF_SANITY_LIMIT,  "mfCurrentValue < 10000.0f");
    CGS_ASSERT(mfCurrentSpeed > -KF_SANITY_LIMIT, "mfCurrentSpeed > -10000.0f");
    CGS_ASSERT(mfCurrentValue > -KF_SANITY_LIMIT, "mfCurrentValue > -10000.0f");
    CGS_ASSERT(fpu::IsValid(mfCurrentSpeed), "RwMathFPU::IsValid(mfCurrentSpeed)");
    CGS_ASSERT(fpu::IsValid(mfCurrentValue), "RwMathFPU::IsValid(mfCurrentValue)");
}
}
}
}
