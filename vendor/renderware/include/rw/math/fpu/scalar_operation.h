#pragma once

// Portable PC reconstruction of the RenderWare rwmath scalar (FPU) math helpers
// (EARenderWare rwmath 1.02.00, rw/math/fpu/*). Only the templated/scalar helpers the game
// spells `rw::math::fpu::*` at the call sites are provided here (the camera-utils / BrnLooker
// family uses Clamp/Abs/Min/Max/Cos/Tan/IsZero).
//
// Min / Max / Clamp follow the ORIGINAL source, rwmath 1.02.00 include/rw/math/fpu/scalar.h
// (references/Feb-2007/BrnEntityModuleUnity/EARenderWare/stable/external/rwmath/1.02.00/...):
// the float and double forms are branchless fsel on the DIFFERENCE of the operands, and
// `fsel d, t, x, y` = (t >= 0) ? x : y takes y when t is NaN and counts -0 as >= 0. So they are
// NOT std::min / std::max: with a NaN in EITHER operand Min returns its first operand and Max
// its second; on equal operands (+0 against -0 included) Min returns its second and Max its
// first; and Clamp(NaN, lo, hi) is hi. The PC spelling is the compare on the difference itself,
// which the build's default /fp:precise keeps exact for NaN and signed zero. Measured inlines:
// Min 0x822B21F8/0x822B21FC `fsubs f13,a,b ; fsel f0,f13,b,a` (CheckVehicleForPowerPark),
// Max 0x8271F574 `fsel f0,d,d,0.0` (Max(d, 0.0f), the `d - 0.0` folded).
//
// Grounding for IsZero: the tolerance is FLT_EPSILON (2^-23 = 1.1920929e-7), the exact
// immediate the BrnLooker.cpp Zoom asm compares against (0.00000011920929).

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

    // scalar.h:136-139 / :203-206 -- the generic forms (integral T): `a < b ? a : b`, `a > b ? a : b`.
    template <typename T> inline T Min(T lLhs, T lRhs) { return (lLhs < lRhs) ? lLhs : lRhs; }
    template <typename T> inline T Max(T lLhs, T lRhs) { return (lLhs > lRhs) ? lLhs : lRhs; }

    // scalar.h:141-167 -- Min<float>: test = a - b ; fsel(test, b, a).
    template <> inline float Min<float>(float lfLhs, float lfRhs)
    {
        const float lfTest = lfLhs - lfRhs;
        return (lfTest >= 0.0f) ? lfRhs : lfLhs;
    }

    // scalar.h:208-235 -- Max<float>: test = a - b ; fsel(test, a, b).
    template <> inline float Max<float>(float lfLhs, float lfRhs)
    {
        const float lfTest = lfLhs - lfRhs;
        return (lfTest >= 0.0f) ? lfLhs : lfRhs;
    }

    // scalar.h:169-196 / :237-264 -- the double forms are the same fsel, with the difference held
    // in a float (`float test = a - b;`); the selected double operand is returned as it is. No
    // game call site instantiates them today.
    template <> inline double Min<double>(double ldLhs, double ldRhs)
    {
        const float lfTest = static_cast<float>(ldLhs - ldRhs);
        return (lfTest >= 0.0f) ? ldRhs : ldLhs;
    }

    template <> inline double Max<double>(double ldLhs, double ldRhs)
    {
        const float lfTest = static_cast<float>(ldLhs - ldRhs);
        return (lfTest >= 0.0f) ? ldLhs : ldRhs;
    }

    // scalar.h:335-338 -- Clamp(value, min, max) = Min(max, Max(min, value)), through the forms
    // above. For float: v' = fsel(min - value, min, value) ; fsel(max - v', v', max). A NaN value
    // therefore comes back as max, a NaN min is ignored and a NaN max comes back as NaN.
    template <typename T> inline T Clamp(T lValue, T lMin, T lMax)
    {
        return Min(lMax, Max(lMin, lValue));
    }

    // Scalar magnitude (fabs).
    inline float Abs(float lfValue) { return std::fabs(lfValue); }

    // Platform trig (radians).
    inline float Cos(float lfRadians) { return std::cos(lfRadians); }
    inline float Sin(float lfRadians) { return std::sin(lfRadians); }
    inline float Tan(float lfRadians) { return std::tan(lfRadians); }

    // True when |value| is within FLT_EPSILON of zero. scalar.h:390 spells it `(value <= tolerance) &&
    // (value >= -tolerance)`, and the X360 compiler turns each `<=` / `>=` into ONE condition bit of an
    // fcmpu. TrafficLaneTruck::Update, for one:
    //   0x82247C18 fcmpu x, flt_82001770 (+2^-23) ; 0x82247C1C bgt 0x82247C34 -> r11 = 0 (not zero)
    //   0x82247C28 li r11, 1 ; 0x82247C2C fcmpu x, flt_82002514 (-2^-23) ; 0x82247C30 bge -> keep 1 (zero)
    // bgt is taken only on an ORDERED greater and bge is taken unless the compare is an ORDERED less, so
    // an unordered (NaN) value falls through the one and takes the other: on the console IsZero(NaN) is
    // TRUE. The same shape at CrashPlayManager::UpdateMomentum 0x82302300/0x8230230C, BrnLooker
    // 0x82222D0C/0x82222D20, VehiclePhysics 0x825FDC38/0x825FDC44, BehaviourGameplayExternal
    // 0x82241BEC/0x82241C00. The literal `<=` / `>=` spelling answered FALSE for a NaN on x64, so it is
    // spelled as the two branch tests (crash parity FX-GATE).
    inline bool IsZero(float lfValue)
    {
        return !(lfValue > KF_IS_ZERO_TOLERANCE) && !(lfValue < -KF_IS_ZERO_TOLERANCE);
    }

    // ADDITIVE GROW (SmoothMover::UpdateWithLimits* @0x8220D608/@0x8220D820, the
    // "RwMathFPU::IsValid(...)" tripwires): finite-value check -- the console emits it
    // as the fcmpu f0,f0 NaN self-compare, reproduced exactly.
    inline bool IsValid(float lfValue) { return lfValue == lfValue; }
}
}
}
