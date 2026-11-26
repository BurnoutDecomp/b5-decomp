#pragma once

// Native Xbox 360 RenderWare rwmath scalar (FPU) helpers
// (EARenderWare rwmath 1.02.00, rw/math/fpu/*). Only the templated/scalar helpers the game
// spells `rw::math::fpu::*` at the call sites are provided here (the camera-utils / BrnLooker
// family uses Clamp/Abs/Min/Max/Cos/Tan/IsZero).
//
// Min / Max / Clamp follow the ORIGINAL source, rwmath 1.02.00 include/rw/math/fpu/scalar.h
// (references/LegacySrc/BrnEntityModuleUnity/EARenderWare/stable/external/rwmath/1.02.00/...):
// the float and double forms are branchless fsel on the DIFFERENCE of the operands, and
// `fsel d, t, x, y` = (t >= 0) ? x : y takes y when t is NaN and counts -0 as >= 0. So they are
// NOT std::min / std::max: with a NaN in EITHER operand Min returns its first operand and Max
// its second; on equal operands (+0 against -0 included) Min returns its second and Max its
// first; and Clamp(NaN, lo, hi) is hi. The XDK __fsel intrinsic preserves these
// selections directly. Measured inlines:
// Min 0x822B21F8/0x822B21FC `fsubs f13,a,b ; fsel f0,f13,b,a` (CheckVehicleForPowerPark),
// Max 0x8271F574 `fsel f0,d,d,0.0` (Max(d, 0.0f), the `d - 0.0` folded).
//
// Grounding for IsZero: the tolerance is FLT_EPSILON (2^-23 = 1.1920929e-7), the exact
// immediate the BrnLooker.cpp Zoom asm compares against (0.00000011920929).

#include <ppcintrinsics.h>
#include <cmath>
#include "rw/math/fpu/constants.h"   // EPSILON (IsZero's default tolerance)

namespace rw
{
namespace math
{
namespace fpu
{
    // The console IsZero tolerance: FLT_EPSILON. Pinned from the BrnLooker Zoom asm, which
    // compares |x| against this immediate before asserting the timestep is non-zero.
    static const float KF_IS_ZERO_TOLERANCE = 1.1920929e-7f;

    // scalar.h:136-139 / :203-206 -- the generic forms (integral T): `a < b ? a : b`, `a > b ? a : b`.
    // Every Min/Max takes its operands by const reference, as scalar.h declares them (the argument
    // evaluation and the fsel then sit where ARTIST has them, PFXHookNodeBlender::Update 0x824FB75C).
    template <typename T> inline T Min(const T& lLhs, const T& lRhs) { return (lLhs < lRhs) ? lLhs : lRhs; }
    template <typename T> inline T Max(const T& lLhs, const T& lRhs) { return (lLhs > lRhs) ? lLhs : lRhs; }

    // scalar.h:141-167 -- Min<float>: test = a - b ; fsel(test, b, a).
    template <> inline float Min<float>(const float& lfLhs, const float& lfRhs)
    {
        const float lfTest = lfLhs - lfRhs;
        return static_cast<float>(__fsel(lfTest, lfRhs, lfLhs));
    }

    // scalar.h:208-235 -- Max<float>: test = a - b ; fsel(test, a, b).
    template <> inline float Max<float>(const float& lfLhs, const float& lfRhs)
    {
        const float lfTest = lfLhs - lfRhs;
        return static_cast<float>(__fsel(lfTest, lfLhs, lfRhs));
    }

    // scalar.h:169-196 / :237-264 -- the double forms are the same fsel, with the difference held
    // in a float (`float test = a - b;`); the selected double operand is returned as it is. No
    // game call site instantiates them today.
    template <> inline double Min<double>(const double& ldLhs, const double& ldRhs)
    {
        const float lfTest = static_cast<float>(ldLhs - ldRhs);
        return __fsel(lfTest, ldRhs, ldLhs);
    }

    template <> inline double Max<double>(const double& ldLhs, const double& ldRhs)
    {
        const float lfTest = static_cast<float>(ldLhs - ldRhs);
        return __fsel(lfTest, ldLhs, ldRhs);
    }

    // scalar.h:335-338 -- Clamp(value, min, max) = Min(max, Max(min, value)), through the forms
    // above. For float: v' = fsel(min - value, min, value) ; fsel(max - v', v', max). A NaN value
    // therefore comes back as max, a NaN min is ignored and a NaN max comes back as NaN.
    template <typename T> inline T Clamp(T lValue, T lMin, T lMax)
    {
        return Min(lMax, Max(lMin, lValue));
    }

    // Legacy rwmath scalar.h:25-90. ARTIST 0x82863440 uses the same RenderWare
    // floor sequence: sign-select the double mantissa bound, then correct
    // a rounded-up result using the XDK floating-point select intrinsic.
    template <typename T> inline T Floor(T lValue)
    {
        const double ldMantissaBound = 4503599627370496.0;
        const double ldValue = static_cast<double>(lValue);
        const double ldBias = __fsel(ldValue, -ldMantissaBound, ldMantissaBound);
        const double ldRounded = (ldValue - ldBias) + ldBias;
        return static_cast<T>(ldRounded - __fsel(ldValue - ldRounded, 0.0, 1.0));
    }

    template <typename T> inline int IntFloor(T lValue)
    {
        return static_cast<int>(Floor(lValue));
    }

    // Legacy rwmath scalar.h:77-80. ARTIST's network rounders inline the float form.
    template <typename T> inline T Round(T lValue)
    {
        return Floor(lValue + 0.5f);
    }

    template <typename T> inline int IntRound(T lValue)
    {
        return IntFloor(lValue + 0.5f);
    }

    // Scalar magnitude (fabs).
    inline float Abs(float lfValue) { return std::fabs(lfValue); }

    // rwmath 1.02.00 fpu/constants.h:53 (SMALL_FLOAT = 1/65536, 0x37800000) and fpu/scalar.h:393:
    // IsSimilar(a, b, epsilon = SMALL_FLOAT) is |a - b| < epsilon. DWARF names it in
    // SpliceSample::FreeVoice (ARTIST 0x826A38C0..0x826A38E8).
    const float SMALL_FLOAT = (1.0f / 65536.0f);
    inline bool IsSimilar(float a, float b, float epsilon = SMALL_FLOAT) { return Abs(a - b) < epsilon; }

    // rwmath 1.02.00 fpu/scalar.h:268: Lerp(a, b, t) = a + (b - a) * t. DWARF names Lerp<float> in
    // BrnAI::SteeringFan::GetSpeedRatio (ARTIST 0x82779C08..0x82779C18: Lerp(1, 0, t) folds to 1 - t).
    template <class T> inline T Lerp(T a, T b, T t)
    {
        return a + (b - a) * t;
    }

    // scalar.h:289-297 -- Sqrt<float> is the XDK __fsqrts intrinsic (one fsqrts; e.g. inlined by
    // Magnitude in BrnAI::AStar::EuclideanDistance 0x8276E0F8).
    template <class T> inline T Sqrt(T lValue);
    template <> inline float Sqrt<float>(float lfValue) { return __fsqrts(lfValue); }

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
    // rwmath 1.02.00 fpu/scalar.h:390 `IsZero(float value, float tolerance = EPSILON)`: Looker::Zoom
    // and the camera utils' GetZoomFromFOVDegs / GetFOVDegsFromZoom read rwmath's EPSILON copy at
    // 0x82001770. KF_IS_ZERO_TOLERANCE above stays for its direct users; IsZero no longer
    // references it, so it is not emitted into every including TU's .rdata block.
    inline bool IsZero(float lfValue, float lfTolerance = EPSILON)
    {
        return !(lfValue > lfTolerance) && !(lfValue < -lfTolerance);
    }

    // ADDITIVE GROW (SmoothMover::UpdateWithLimits* @0x8220D608/@0x8220D820, the
    // "RwMathFPU::IsValid(...)" tripwires): finite-value check -- the console emits it
    // as the fcmpu f0,f0 NaN self-compare, reproduced exactly.
    // rwmath 1.02.00 fpu/scalar.h:387 `IsValid(float value) { return !IsNaN(value); }`, with the
    // platform IsNaN pair (DWARF BrnGameStateScoringAndModesUnity lists IsValid(float),
    // IsNaN(float) and IsNaN(double) with the local `bool result`; LegacySrc has only the PS3
    // scalar_platform.h, whose body is inline asm). ARTIST materialises that bool at every
    // inlined site: li 1 ; fcmpu ; bne ; li 0 ; clrlwi (SetCheckpointDistances 0x82310C4C,
    // SmoothMover 0x8220D790, IsValid<float>(Vector3) 0x8231A5CC). It compares the float register
    // itself, with no fmr for a conversion, so the Xenon IsNaN(float) tests the float directly
    // rather than forwarding to IsNaN(double) as the PS3 header does.
    inline bool IsNaN(double value)
    {
        bool result = (value != value);
        return result;
    }

    inline bool IsNaN(float value)
    {
        bool result = (value != value);
        return result;
    }

    inline bool IsValid(float lfValue) { return !IsNaN(lfValue); }

    // rwmath 1.02.00 include/rw/math/fpu/scalar.h:375, verbatim. AIDriver::StartDrift calls it
    // (DWARF BrnAIDriver.cpp:1450 lists rw::math::fpu::SgnNonZero<float>); the console folds it
    // to `fsel f1, a, 1.0, -1.0` (0x8277C8B4).
    template <class T> inline T SgnNonZero(T a)
    {
        if( a >= 0.0f )
        {
            return 1.0f;
        }
        else
        {
            return -1.0f;
        }
    }
}
}
}
