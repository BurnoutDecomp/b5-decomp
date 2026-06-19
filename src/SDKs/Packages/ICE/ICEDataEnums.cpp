#include "SDKs/Packages/ICE/ICEDataEnums.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEDataEnums.cpp
//
// Out-of-line ICE data definitions. Currently just ICE::ICEParameter::SetValue
// (the one exported function in this slice, @0x8252ACE0); other ICEDataEnums
// definitions (category-name tables, etc.) join this file as their TUs land.
// ============================================================================

namespace ICE
{

// ICE::ICEParameter::SetValue  @ 0x8252ACE0
//
// Quantize a unit-interval scalar into muPacked. Faithful to the X360 asm:
//   - Two fsel ops clamp lfValue into [0.0, 1.0]:
//       fsel f13, (-value), 0.0, value   -> f13 = (-value >= 0) ? 0 : value
//                                              = max(value, 0.0)
//       fsel f12, (1.0 - f13), f13, 1.0  -> f12 = (1.0 - f13 >= 0) ? f13 : 1.0
//                                              = min(f13, 1.0)
//     so lfClamped = min(max(value, 0.0), 1.0).
//   - lfScaled = lfClamped * 65535.0; fctiwz truncates toward zero to liTrunc,
//     then liTrunc is floored (the lfFloor > lfScaled correction handles
//     truncation; a no-op for the non-negative values here), and the unit is
//     incremented when the fractional part is >= 0.5 (round half up).
//   - muPacked = (u16) of the rounded result.
void ICEParameter::SetValue(f32 lfValue)
{
    // Clamp into [0,1] (the two fsel ops above).
    f32 lfClamped = lfValue;
    if (lfClamped < 0.0f)
    {
        lfClamped = 0.0f;
    }
    if (lfClamped > 1.0f)
    {
        lfClamped = 1.0f;
    }

    // Scale across the full u16 range and round half up.
    f32 lfScaled = lfClamped * (f32)ICE_PARAMETER_MAX;   // * 65535.0
    s32 liRounded = (s32)lfScaled;                       // truncate toward zero (fctiwz)
    f32 lfFloor   = (f32)liRounded;
    if (lfFloor > lfScaled)                              // negative-truncation fix-up -> floor
    {
        lfFloor -= 1.0f;
    }
    if ((lfScaled - lfFloor) >= 0.5f)                    // fractional part >= 0.5 -> round up
    {
        ++liRounded;
    }

    muPacked = (u16)liRounded;
}

// ICE::ICEChannel::EnforceSpacing(u16 lu16Interval, f32 lfMinSpacing)  @0x8252C4F0
//
// Enforce a minimum parameter spacing of lfMinSpacing on either side of the keyed
// interval lu16Interval. Forward sweep pushes each interval parameter up to at
// least param[n-1]+spacing (max); backward sweep pulls each down to at most
// param[n+1]-spacing (min). Only writable interior slots (1 <= n < mu16Intervals)
// are touched; the store is ICEParameter::SetValue (clamp [0,1] + round-half-up
// pack), which the backward sweep inlined in the asm.
void ICEChannel::EnforceSpacing(u16 lu16Interval, f32 lfMinSpacing)
{
    const s32 liLastInterval = (s32)mu16Intervals - 1;

    // Forward: param[n] = max(param[n], param[n-1] + lfMinSpacing).
    for (s32 liInterval = (s32)lu16Interval; liInterval < liLastInterval; ++liInterval)
    {
        const u16 lu16Next = (u16)(liInterval + 1);
        const f32 lfPrev   = GetIntervalParameter((u16)liInterval);
        const f32 lfThis   = GetIntervalParameter(lu16Next);
        const f32 lfFloor  = lfPrev + lfMinSpacing;
        if (lu16Next != 0 && lu16Next < mu16Intervals)
        {
            const f32 lfNew = (lfThis >= lfFloor) ? lfThis : lfFloor;   // max
            mpParameters[lu16Next - 1].SetValue(lfNew);
        }
    }

    // Backward: param[n] = min(param[n], param[n+1] - lfMinSpacing).
    for (s32 liInterval = (s32)lu16Interval; liInterval > 1; --liInterval)
    {
        const u16 lu16Prev = (u16)(liInterval - 1);
        const f32 lfNext   = GetIntervalParameter((u16)liInterval);
        const f32 lfThis   = GetIntervalParameter(lu16Prev);
        const f32 lfCeil   = lfNext - lfMinSpacing;
        if (lu16Prev != 0 && lu16Prev < mu16Intervals)
        {
            const f32 lfNew = (lfThis <= lfCeil) ? lfThis : lfCeil;   // min
            mpParameters[lu16Prev - 1].SetValue(lfNew);
        }
    }
}

// ICE::ICEChannel::GetIntervalStart(u16) const  @0x82531810
//
// The parameter value at the START of the channel's CURRENT interval. Like the
// keystone GetIntervalBracket overload, the u16 argument is part of the frozen
// overload signature but the body operates on the cached mu16CurrentInterval (the
// asm reads *(this+4), not the argument): GetIntervalParameter(mu16CurrentInterval)
// with the sentinel rules (0 -> 0.0, >= mu16Intervals -> 1.0).
f32 ICEChannel::GetIntervalStart(u16 lu16Interval) const
{
    (void)lu16Interval;   // unused: the body works off the cached current interval

    const u16 lu16Current = mu16CurrentInterval;
    if (lu16Current == 0)
        return 0.0f;
    if (lu16Current >= mu16Intervals)
        return 1.0f;
    return mpParameters[lu16Current - 1].GetValue();
}

} // namespace ICE
