#pragma once

// Portable PC reconstruction of the RenderWare rwmath scalar (FPU) math helpers
// (EARenderWare rwmath 1.02.00, rw/math/fpu/*). The console SDK implements these over
// the PPC FPU (fsel-based branchless Min/Max/Clamp, fabs, the platform sin/cos/tan); on
// PC they reduce to the obvious std:: scalar math with identical results. Only the
// templated/scalar helpers the game spells `rw::math::fpu::*` at the call sites are
// provided here (the camera-utils / BrnLooker family uses Clamp/Abs/Min/Max/Cos/Tan/IsZero).
//
// ADDITIVE GROW: this is the first reconstruction of the fpu scalar-op home; no existing
// user is disturbed. Grounding: the IsZero tolerance is FLT_EPSILON (2^-23 = 1.1920929e-7),
// the exact immediate the BrnLooker.cpp Zoom asm compares against (0.00000011920929).

#include <cmath>

namespace rw
{
namespace math
{
namespace fpu
{
    // The console IsZero tolerance: FLT_EPSILON. Pinned from the BrnLooker Zoom asm, which
    // compares |x| against this immediate before asserting the timestep is non-zero.
    inline constexpr float KF_IS_ZERO_TOLERANCE = 1.1920929e-7f;

    // fsel-branchless Min/Max on console; std::min/std::max-equivalent on PC.
    template <typename T> inline T Min(T lLhs, T lRhs) { return (lRhs < lLhs) ? lRhs : lLhs; }
    template <typename T> inline T Max(T lLhs, T lRhs) { return (lLhs < lRhs) ? lRhs : lLhs; }

    // Clamp a value into [lo, hi] (the console double-fsel form).
    template <typename T> inline T Clamp(T lValue, T lLo, T lHi)
    {
        return (lValue < lLo) ? lLo : ((lHi < lValue) ? lHi : lValue);
    }

    // Scalar magnitude (fabs).
    inline float Abs(float lfValue) { return std::fabs(lfValue); }

    // Platform trig (radians).
    inline float Cos(float lfRadians) { return std::cos(lfRadians); }
    inline float Sin(float lfRadians) { return std::sin(lfRadians); }
    inline float Tan(float lfRadians) { return std::tan(lfRadians); }

    // True when |value| is within FLT_EPSILON of zero (the console fcmpu-against-tolerance).
    inline bool IsZero(float lfValue)
    {
        return lfValue <= KF_IS_ZERO_TOLERANCE && lfValue >= -KF_IS_ZERO_TOLERANCE;
    }
}
}
}
