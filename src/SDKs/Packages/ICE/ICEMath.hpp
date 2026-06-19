#ifndef SDKS_PACKAGES_ICE_ICEMATH_HPP
#define SDKS_PACKAGES_ICE_ICEMATH_HPP

#include "types.hpp"

// ============================================================================
// SDKs/Packages/ICE/ICEMath.hpp
//
// ICE::ICEMath -- the small scalar-math helper surface the ICE camera-take code
// leans on (rounding, clamping, int<->float conversion). The real definitions
// live in the ICE math TU (SDKs/Packages/ICE/ICEMath.cpp, class:ICE::ICEMath);
// the per-TU `cl /c` gate does not link, so DECLARATION-ONLY suffices here for
// the non-trivial entry points.
//
// MINIMAL SLICE -- declares exactly what the ICEData.cpp bodies call:
//   * ICEMath::Round(f32)           -> f32   (ICEElementDescription::Prepare)
//   * ICEMath::Clamp(f32,f32,f32)   -> f32   (ICEElementDescription::Prepare assert)
//   * ICEMath::IntToFloat(s32)      -> f32   (declared for completeness; see note)
//
// Clamp is the trivially-obvious min(max(v,lo),hi) and is given an inline body so
// the (constant-folded) assert in Prepare needs no link. Round is the engine's
// round-to-nearest and is DECLARATION-ONLY (its exact half-way/banker's behaviour
// is owned by the ICEMath TU -- do not guess it here). When the ICEMath TU lands,
// GROW this header in place; do NOT fork a parallel ICEMath.
// ============================================================================

namespace ICE
{
namespace ICEMath
{
    // Round-to-nearest. DECLARATION-ONLY: defined out-of-line in ICEMath.cpp
    // (the rounding rule is owned there; cl /c does not link).
    f32 Round(f32 lfValue);

    // Clamp lfValue into [lfLo, lfHi]. Trivially obvious -> inline body.
    inline f32 Clamp(f32 lfValue, f32 lfLo, f32 lfHi)
    {
        f32 lfResult = (lfValue < lfLo) ? lfLo : lfValue;
        return (lfResult > lfHi) ? lfHi : lfResult;
    }

    // Signed-int to float. Not called by the current ICEData.cpp bodies; declared
    // per the ICEMath surface so a using-TU can extend without re-homing. The plain
    // numeric cast matches the obvious semantics. DECLARATION-ONLY would also be
    // fine; inline keeps it usable without the ICEMath TU.
    inline f32 IntToFloat(s32 liValue)
    {
        return (f32)liValue;
    }
}
}

#endif // SDKS_PACKAGES_ICE_ICEMATH_HPP
