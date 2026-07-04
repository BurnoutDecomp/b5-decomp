#include "GameSource/World/AI/PID/BrnPIDController.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// =============================================================================
// BrnAI::PIDController -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   PIDController::GetError           @ 0x827683A0
//   PIDController::GetErrorDerivative @ 0x82768440
//   PIDController::GetOutput          @ 0x82768550
//   PIDController::Prepare            @ 0x82775AE8
//   PIDController::Record             @ 0x82775B48
// =============================================================================

namespace BrnAI
{

// PIDController::GetError [X360 0x827683A0]
// Latest recorded error, or 0 if nothing recorded yet.
f32 PIDController::GetError()
{
    if (mn8NumErrorsRecorded < 1)
        return 0.0f;

    CGS_ASSERT((mn8CurrentIndex >= 0) && (mn8CurrentIndex < KI_PIDCONTROLLER_NUM_SLOTS),
               "(mn8CurrentIndex >= 0) && (mn8CurrentIndex < KI_PIDCONTROLLER_NUM_SLOTS)");

    return mafError[mn8CurrentIndex];
}

// PIDController::GetErrorDerivative [X360 0x82768440]
// (currentError - previousError) / currentTimeStep. Guards a tiny/zero time step by
// returning a large sentinel derivative (999999.0f).
f32 PIDController::GetErrorDerivative()
{
    if (mn8NumErrorsRecorded < 2)
        return 0.0f;

    CGS_ASSERT((mn8CurrentIndex >= 0) && (mn8CurrentIndex < KI_PIDCONTROLLER_NUM_SLOTS),
               "(mn8CurrentIndex >= 0) && (mn8CurrentIndex < KI_PIDCONTROLLER_NUM_SLOTS)");
    CGS_ASSERT((mn8PreviousIndex >= 0) && (mn8PreviousIndex < KI_PIDCONTROLLER_NUM_SLOTS),
               "(mn8PreviousIndex >= 0) && (mn8PreviousIndex < KI_PIDCONTROLLER_NUM_SLOTS)");

    const s32 lnCurrentIndex = mn8CurrentIndex;
    const f32 lfTimeInterval = mafTimeStep[lnCurrentIndex];
    if (lfTimeInterval <= 0.001f)
        return 999999.0f;

    const f32 lfDifference = mafError[lnCurrentIndex] - mafError[mn8PreviousIndex];
    return lfDifference / lfTimeInterval;
}

// PIDController::GetOutput [X360 0x82768550]
// PID law: P*error + I*integral + D*derivative. The integral term is snapshotted into a
// register before GetErrorDerivative() is evaluated (matches the X360 evaluation order).
f32 PIDController::GetOutput()
{
    const f32 lfIntegral = mfCurrentIntegral;
    const f32 lfDerivativeTerm = GetErrorDerivative() * mfDCoefficient;
    return (GetError() * mfPCoefficient) + (mfICoefficient * lfIntegral) + lfDerivativeTerm;
}

// PIDController::Prepare [X360 0x82775AE8]
// Initialise the controller: store the {P,I,D} coefficients, clear the integral and the
// error/time-step history, and reset the ring-buffer indices to "empty" (-1 / 0).
void PIDController::Prepare(const f32* lafCoefficientValues)
{
    mfPCoefficient = lafCoefficientValues[E_COEFFICIENT_P];
    mfICoefficient = lafCoefficientValues[E_COEFFICIENT_I];
    mfDCoefficient = lafCoefficientValues[E_COEFFICIENT_D];

    mn8CurrentIndex       = -1;
    mn8PreviousIndex      = -1;
    mfCurrentIntegral     = 0.0f;
    mn8NumErrorsRecorded  = 0;

    for (s32 i = 0; i < KI_PIDCONTROLLER_NUM_SLOTS; ++i)
    {
        mafError[i]    = 0.0f;
        mafTimeStep[i] = 0.0f;
    }
}

// PIDController::Record [X360 0x82775B48]
// Push one (error, timeStep) sample into the ring buffer and maintain the running integral.
// When the buffer is already full the oldest sample's contribution is removed (the slot about
// to be overwritten) before the newest error*timeStep contribution is added -- a sliding-window
// rectangular integral. Indices advance modulo KI_PIDCONTROLLER_NUM_SLOTS.
void PIDController::Record(f32 lfError, f32 lfTimeStep)
{
    const s8   ln8PreviousIndex = mn8CurrentIndex;
    const bool lbBufferFull     = (mn8NumErrorsRecorded == KI_PIDCONTROLLER_NUM_SLOTS);

    mn8PreviousIndex = ln8PreviousIndex;
    mn8CurrentIndex  = static_cast<s8>((ln8PreviousIndex + 1) % KI_PIDCONTROLLER_NUM_SLOTS);

    if (lbBufferFull)
    {
        // Remove the sample that is about to be overwritten from the running integral.
        mfCurrentIntegral -= mafError[mn8CurrentIndex] * mafTimeStep[mn8CurrentIndex];
    }

    mafError[mn8CurrentIndex]    = lfError;
    mafTimeStep[mn8CurrentIndex] = lfTimeStep;

    mfCurrentIntegral += lfError * lfTimeStep;

    s32 ln32NumRecorded = mn8NumErrorsRecorded + 1;
    if (ln32NumRecorded >= KI_PIDCONTROLLER_NUM_SLOTS)
        ln32NumRecorded = KI_PIDCONTROLLER_NUM_SLOTS;
    mn8NumErrorsRecorded = static_cast<s8>(ln32NumRecorded);
}

} // namespace BrnAI
