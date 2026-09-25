// FX-DIRECTOR2 (crash parity 2026-09-25): BehaviourInterpolate's inlined rw::math::fpu::IsZero, by NaN polarity.
//
// BehaviourInterpolate::PostCollisionUpdate @0x82252AB8 asserts "!rw::math::fpu::IsZero(mfDuration)" (:145) through
// the inlined scalar IsZero (0x82252B84..0x82252BB0):
//     lfs f0, 0x2A4 (mfDuration) ; fcmpu cr6, f0, flt_82001770 (+1.1920928955078125e-07, 0x34000000) ; bgt -> 0
//     li r11, 1 ; fcmpu cr6, f0, flt_82002514 (-1.1920928955078125e-07, 0xB4000000) ; bge -> keep 1 ; li r11, 0
// An unordered compare (NaN) is neither gt nor lt, so it falls through the first branch and takes the second:
// IsZero(NaN) is TRUE on the console and the :145 assert fires for a NaN duration. run_fxdirector2_interpolate_iszero.py
// extracts the revision's own helper (and its epsilon) from BrnBehaviourInterpolate.cpp into interpolate_iszero.inc.
#include "types.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gChecks = 0, gFailures = 0;

namespace FxInterpolate
{
#include "interpolate_iszero.inc"
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::printf("FAIL  %s\n", lpcName);
    }
}

static u32 Bits(f32 lfValue)
{
    u32 luBits;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits;
}

int main()
{
    using FxInterpolate::IsZero;
    const f32 KF_NAN  = std::numeric_limits<f32>::quiet_NaN();
    const f32 KF_INF  = std::numeric_limits<f32>::infinity();
    const f32 KF_EPS  = FxInterpolate::KF_IS_ZERO_EPSILON;

    Check(Bits(KF_EPS) == 0x34000000u, "the band is flt_82001770 = 0x34000000 (FLT_EPSILON); its negative is flt_82002514");
    Check(IsZero(KF_NAN), "NaN is ZERO: unordered skips `bgt` (0x82252B94) and takes `bge` (0x82252BA8) -- the :145 "
                          "assert fires for a NaN duration, as on the console");
    Check(IsZero(-KF_NAN), "a negative-signed NaN is ZERO as well");
    Check(IsZero(0.0f) && IsZero(-0.0f), "+0.0 and -0.0 are zero");
    Check(IsZero(KF_EPS) && IsZero(-KF_EPS), "the band is closed: +EPS is not > EPS, -EPS is not < -EPS");
    Check(!IsZero(std::nextafter(KF_EPS, KF_INF)) && !IsZero(std::nextafter(-KF_EPS, -KF_INF)),
          "one ulp outside the band either side is not zero");
    Check(IsZero(std::numeric_limits<f32>::denorm_min()) && IsZero(-std::numeric_limits<f32>::denorm_min()),
          "the denormals are zero");
    Check(!IsZero(1.0f) && !IsZero(-1.0f) && !IsZero(KF_INF) && !IsZero(-KF_INF),
          "1, -1 and the infinities are not zero (a live duration never trips :145)");

    std::printf("FxDirector2InterpolateIsZero: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
