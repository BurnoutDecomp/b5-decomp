// FX-TRAFFIC5 (crash parity wave 5, 2026-09-24): the sign the three traffic response arms take.
// The PRODUCTION helper SignumLane, extracted from
// src/GameSource/Physics/VehicleManager/BrnPhysicalTrafficManager_CrashResponse.cpp by
// run_fxtraffic5_signum.py (the runner also checks that SetTrafficVehicleChecked / SetTrafficVehicleSlammed /
// TestForNearMissFreakOut all route their side and drive signs through it).
//
// Expectations, read off the ARTIST asm (never off the reconstruction):
//   checked / slammed (0x8262D908..0x8262D914, 0x825EFFD0..0x825EFFDC, drive 0x8262D9C8.., 0x825F0090..):
//     vcmpgtfp x,0 ; vcmpgefp x,0 ; vsel(0, 1, >) ; vsel(-1, that, >=)
//   near miss 0x82637C30 -> rw::math::fpu::Sgn<VecFloat> @0x825BC920:
//     vcmpeqfp. 0 -> 0.0 (flt_82001CC0) ; vcmpgefp. 0 -> 1.0 (flt_82001C98) ; else -1.0 (flt_820037C8)
//   => +x -> 1, -x -> -1, +-0 -> 0, NaN -> -1 (both compares fail), +inf -> 1, -inf -> -1
#include "types.hpp"
#include <cmath>
#include <cstdio>
#include <limits>

#include "signum.inc"

static unsigned gChecks = 0, gFailures = 0;

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
    {
        ++gFailures;
        std::fprintf(stderr, "FAIL: %s\n", lpcName);
    }
    else
    {
        std::printf("ok: %s\n", lpcName);
    }
}

int main()
{
    const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
    const f32 lfInf = std::numeric_limits<f32>::infinity();
    volatile f32 lfNegZero = -0.0f;

    Check(SignumLane(2.5f) == 1.0f, "S1 positive -> 1");
    Check(SignumLane(-2.5f) == -1.0f, "S2 negative -> -1");
    Check(SignumLane(0.0f) == 0.0f, "S3 +0 -> 0 (vcmpeqfp. / vcmpgefp. >= selects the 0 lane)");
    Check(SignumLane(lfNegZero) == 0.0f, "S4 -0 -> 0");
    Check(SignumLane(lfNaN) == -1.0f, "S5 NaN -> -1: both compares fail and the -1 (flt_820037C8) select wins");
    Check(SignumLane(lfInf) == 1.0f && SignumLane(-lfInf) == -1.0f, "S6 +-inf -> +-1");
    Check(SignumLane(1e-38f) == 1.0f && SignumLane(-1e-38f) == -1.0f, "S7 tiny values keep their sign");

    std::printf("FxTraffic5Signum: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
