// FX-RCEM4 (crash parity 2026-09-24, FX-FPUMAX leftover): CrashPlayManager::ClampBoostLevel, inlined at
// all six console sites as the same two fsels (e.g. OnBounce 0x822A7F78..0x822A7F88):
//   fneg  f13, x          ; fsel f0, f13, ZERO, x        -x >= 0 ? 0 : x          (Max(0, x))
//   lfs   f13, 100.0      ; fsubs f12, f13, f0 ; fsel f0, f12, f0, f13   100 - f0 >= 0 ? f0 : 100   (Min(100, f0))
// ZERO = flt_82001CC0 (0.0), 100.0 = flt_82014808. fsel takes its THIRD operand when the test is NaN.
// run_fxrcem4_clamp_boost_level.py extracts the production body VERBATIM and runs it against the
// production rw::math::fpu header (the rwmath fsel forms, b27e1448).
#include "types.hpp"
#include "rw/math/fpu/scalar_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace Fixture {
static const f32 KF_MAX_BOOST = 100.0f;   // flt_82014808
struct CrashPlayManager {
    f32 mfBoostPercentage = 0.0f;
    void ClampBoostLevel();
};
#include "fxrcem4_cbl_body.inc"
}   // namespace Fixture

// The console's two instructions pairs, fsel fD,fA,fC,fB = (fA >= 0.0) ? fC : fB (NaN -> fB).
static f32 Fsel(f32 lfA, f32 lfC, f32 lfB) { return (lfA >= 0.0f) ? lfC : lfB; }
static f32 Console(f32 lfX)
{
    const f32 lfZero = 0.0f, lfHundred = 100.0f;
    const f32 lf0 = Fsel(-lfX, lfZero, lfX);            // fneg ; fsel f0, f13, ZERO, x
    return Fsel(lfHundred - lf0, lf0, lfHundred);       // fsubs ; fsel f0, f12, f0, 100
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}
static f32 Run(f32 lfX) { Fixture::CrashPlayManager m; m.mfBoostPercentage = lfX; m.ClampBoostLevel(); return m.mfBoostPercentage; }
static bool Same(f32 a, f32 b) { u32 x, y; std::memcpy(&x, &a, 4); std::memcpy(&y, &b, 4); return x == y || (std::isnan(a) && std::isnan(b)); }

int main() {
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();
    const f32 lfInf = std::numeric_limits<f32>::infinity();

    Check(Run(50.0f) == 50.0f, "an in-range meter is kept");
    Check(Run(-5.0f) == 0.0f, "a negative meter clamps to 0");
    Check(Run(150.0f) == 100.0f, "an over-full meter clamps to KF_MAX_BOOST");
    Check(Run(lfNan) == 100.0f, "a NaN meter comes out 100 (the first fsel keeps the NaN, the second takes 100)");
    const f32 lfNegZero = Run(-0.0f);
    Check(lfNegZero == 0.0f && !std::signbit(lfNegZero), "-0 comes out +0 (fneg -0 = +0 >= 0 selects ZERO)");
    Check(Run(lfInf) == 100.0f && Run(-lfInf) == 0.0f, "+inf -> 100, -inf -> 0");

    // Bit-for-bit against the console's two fsels over the awkward inputs.
    const f32 laInputs[] = { 0.0f, -0.0f, 1.0e-45f, -1.0e-45f, 99.99999f, 100.0f, 100.00001f, -100.0f,
                             lfNan, -lfNan, lfInf, -lfInf, 3.0e38f, -3.0e38f, 42.5f };
    bool lbAll = true;
    for (f32 lfX : laInputs) {
        if (!Same(Run(lfX), Console(lfX))) {
            lbAll = false;
            std::fprintf(stderr, "  input %g: production %g, console %g\n", lfX, Run(lfX), Console(lfX));
        }
    }
    Check(lbAll, "matches the console's fsel pair bit for bit on every sample");

    std::printf("FxRcem4ClampBoostLevel: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
