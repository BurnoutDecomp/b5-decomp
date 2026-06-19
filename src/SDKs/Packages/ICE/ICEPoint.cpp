#include "SDKs/Packages/ICE/ICEPoint.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEPoint.cpp
//
// ICE::Cubic1D -- derivative clamps + the per-frame Update for the ICE camera
// system's 1-D cubic follower. Out-of-line definitions for the three exported
// functions in this slice:
//   ICE::Cubic1D::ClampDerivative        @0x8252CD30
//   ICE::Cubic1D::ClampSecondDerivative  @0x8252CD98
//   ICE::Cubic1D::Update                 @0x8252CE70 (inlines MakeCoeffs)
//
// The cubic is f(t) = Coeff[0]*t^3 + Coeff[1]*t^2 + Coeff[2]*t + Coeff[3] over
// the normalised parameter t in [0,1] (t == time). Its derivatives w.r.t. real
// time pick up a factor (1/duration) per order of differentiation:
//   f'(t)  = ( 3*Coeff[0]*t^2 + 2*Coeff[1]*t + Coeff[2] ) / duration
//   f''(t) = ( 6*Coeff[0]*t   + 2*Coeff[1]              ) / duration^2
//
// The X360 compiler left algebraically-redundant normalised-time factors of the
// form duration*(1/duration) (== 1.0) in the instruction stream; they are the
// un-simplified (t/duration) terms. They are folded to their closed form here
// (semantic parity preserved) -- flagged inline where folded.
// ============================================================================

namespace ICE
{

// ICE::Cubic1D::ClampDerivative  @0x8252CD30
//
// Limit the curve's first derivative at the end of the seek (t == 1) to +/-limit.
// Pseudocode evaluates f'(1) = (3*Coeff[0] + 2*Coeff[1] + Coeff[2]) / duration
// (the duration*(1/duration) terms in the asm are == 1.0 and are folded). The
// asm clamp `fsel f12,f0,f0,f12` with f12 = -f0 computes (deriv>=0 ? deriv : -deriv)
// == fabs(deriv). When the magnitude exceeds the limit, dValDesired (result[3])
// is reset so the rescaled target derivative is sign(deriv)*duration*limit -- the
// asm divides fabs(deriv) by deriv to recover the sign, then * duration * limit.
void Cubic1D::ClampDerivative(f32 limit)
{
    // f'(1) in real time. duration*(1/duration) == 1.0 folded (FLAG: closed form).
    const f32 lfDerivative =
        (3.0f * Coeff[0] + 2.0f * Coeff[1] + Coeff[2]) / duration;

    const f32 lfMagnitude = (lfDerivative >= 0.0f) ? lfDerivative : -lfDerivative;

    if (lfMagnitude > limit)
    {
        // (magnitude / derivative) == sign(derivative); rescale the target slope.
        const f32 lfSign = lfMagnitude / lfDerivative;
        dValDesired = lfSign * duration * limit;
    }
}

// ICE::Cubic1D::ClampSecondDerivative  @0x8252CD98
//
// Limit the curve's second derivative at both ends of the seek (t == 0 and t == 1)
// to +/-limit, then -- if either end was clamped -- re-derive Coeff[0]/Coeff[1]
// from the clamped accelerations.
//   f''(0) = 2*Coeff[1]              / duration^2   (the (1/duration)*0.0 term is t=0)
//   f''(1) = (6*Coeff[0] + 2*Coeff[1]) / duration^2 (duration*(1/duration) == 1.0 folded)
// The two `fsel` ops are fabs() (operand negated into the false slot). For each
// end, if |f''| > limit, replace f'' with sign(f'')*limit and mark clamped. When
// clamped, back-solve: 2*Coeff[1] = duration^2*f''(0) -> Coeff[1] = that * 0.5;
// 6*Coeff[0] = duration^2*f''(1) - duration^2*f''(0) -> Coeff[0] = that * (1/6).
void Cubic1D::ClampSecondDerivative(f32 limit)
{
    const f32 lfDurationSq = duration * duration;

    // FLAG: closed form -- (1/duration)*0.0*... == 0, so f''(0) = 2*Coeff[1]/dur^2;
    //       duration*(1/duration) == 1.0 in the f''(1) expression, both folded.
    //       (The asm forms these as x*(1/dur)*(1/dur) via one reciprocal; the divide
    //       here differs only at sub-ULP FP-rounding level -- semantic parity.)
    f32 lfSecondDerivAtStart = (2.0f * Coeff[1]) / lfDurationSq;
    f32 lfSecondDerivAtEnd   = (6.0f * Coeff[0] + 2.0f * Coeff[1]) / lfDurationSq;

    bool lbClamped = false;

    const f32 lfMagStart = (lfSecondDerivAtStart >= 0.0f) ? lfSecondDerivAtStart
                                                          : -lfSecondDerivAtStart;
    if (lfMagStart > limit)
    {
        lbClamped = true;
        // (magnitude / value) == sign(value); clamp magnitude to limit.
        lfSecondDerivAtStart = (lfMagStart / lfSecondDerivAtStart) * limit;
    }

    const f32 lfMagEnd = (lfSecondDerivAtEnd >= 0.0f) ? lfSecondDerivAtEnd
                                                      : -lfSecondDerivAtEnd;
    if (lfMagEnd > limit)
    {
        lbClamped = true;
        lfSecondDerivAtEnd = (lfMagEnd / lfSecondDerivAtEnd) * limit;
    }

    if (lbClamped)
    {
        // Back-solve the quadratic/cubic coefficients from the clamped accelerations.
        const f32 lfTwoCoeff1 = lfDurationSq * lfSecondDerivAtStart;            // 2*Coeff[1]
        const f32 lfSixCoeff0 = lfDurationSq * lfSecondDerivAtEnd - lfTwoCoeff1; // 6*Coeff[0]
        Coeff[1] = lfTwoCoeff1 * 0.5f;          // / 2
        Coeff[0] = lfSixCoeff0 * 0.16666667f;   // / 6
    }
}

// ICE::Cubic1D::Update  @0x8252CE70
//
// Per-frame advance. State machine on `state`:
//   state != 1 && state != 2 -> idle, nothing to do.
//   state == 2 -> (re)start: reset time, (re)build the cubic coefficients from
//                 the current/target value+derivative (inlined MakeCoeffs), apply
//                 the optional first/second-derivative clamps, and drop to the
//                 running state (unless flags requests otherwise).
// Then advance `time` by dt/duration (snapping to 1.0 for a degenerate duration),
// clamp to 1.0 (snapping Val/dVal to the target and marking arrived on overshoot),
// and evaluate the cubic + its derivative at the new time into Val/dVal.
void Cubic1D::Update(f32 dt, f32 derivativeLimit, f32 secondDerivativeLimit)
{
    if (state == 1 || state == 2)
    {
        if (state == 2)
        {
            // (Re)start the seek.
            time = 0.0f;
            if (flags == 0)
            {
                state = 1;
            }

            if (derivativeLimit > 0.0f)
            {
                ClampDerivative(derivativeLimit);
            }

            // Inlined MakeCoeffs: fit the Hermite-style cubic so that
            //   f(0)=Val, f'(0)=dVal, f(1)=ValDesired, f'(1)=dValDesired
            // (derivatives here are in normalised-t units).
            const f32 lfDeltaVal = ValDesired - Val;
            const f32 lfdVal      = dVal;
            const f32 lfdValDesired = dValDesired;

            Coeff[3] = Val;    // f(0)
            Coeff[2] = lfdVal; // f'(0) in normalised t
            // Coeff[0] = -((delta*2) - (dVal + dValDesired))
            Coeff[0] = -((lfDeltaVal * 2.0f) - (lfdVal + lfdValDesired));
            // Coeff[1] = -((dVal*2) - ((delta*3) - dValDesired))
            Coeff[1] = -((lfdVal * 2.0f) - ((lfDeltaVal * 3.0f) - lfdValDesired));

            if (secondDerivativeLimit > 0.0f)
            {
                ClampSecondDerivative(secondDerivativeLimit);
            }
        }
    }
    else
    {
        return;
    }

    // Advance normalised time.
    if (duration <= 9.9999997e-6f)
    {
        time = 1.0f;
    }
    else
    {
        time += dt / duration;
    }

    // Overshoot -> snap to the target and mark arrived.
    if (time > 1.0f)
    {
        const f32 lfTargetVal  = ValDesired;
        const f32 lfTargetdVal = dValDesired;
        time = 1.0f;
        Val = lfTargetVal;
        dVal = lfTargetdVal;
        state = 0;
    }

    // Evaluate the cubic and its (normalised-t) derivative at the new time.
    //   Val  = ((((Coeff[1] + Coeff[0]*t)*t) + Coeff[2])*t) + Coeff[3]  (Horner)
    //   dVal = (3*Coeff[0]*t + 2*Coeff[1])*t + Coeff[2]
    const f32 lfT = time;

    Val = (((Coeff[1] + Coeff[0] * lfT) * lfT + Coeff[2]) * lfT) + Coeff[3];
    dVal = ((Coeff[0] * lfT) * 3.0f + Coeff[1] * 2.0f) * lfT + Coeff[2];
}

} // namespace ICE
