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

} // namespace ICE
