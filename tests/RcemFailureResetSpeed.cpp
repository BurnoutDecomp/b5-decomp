// FX-RCEM (crash-parity 2026-09-22): the FAILURE arm of
// RaceCarEntityModule::ProcessResetOnTrackResultQueue (ARTIST 0x822F4580), extracted VERBATIM
// (the speed statement plus its TU-local cap) from BrnRaceCarEntityModule_ResetPump.cpp by
// run_rcem_failure_reset_speed.py.
//   0x822F46BC  lfs f0, 0x28(result)            -- mfResetSpeed
//   0x822F46C4  lfs f13, flt_82FAD610           -- 0.44704f * 10.0f (CRT 0x82C4BB10..0x82C4BB2C)
//   0x822F46C8  fsubs f12, f0, f13
//   0x822F46DC  fsel f1, f12, f13, f0           -- (speed - K >= 0) ? K : speed
#include "types.hpp"
#include <cstdio>

namespace Fixture {
struct ResetOnTrackResult {
    f32 mfResetSpeed;
    f32 GetResetSpeed() const { return mfResetSpeed; }
};
#include "rcem_failure_reset_speed.inc"
}

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

int main() {
    const f32 kfCap = 0.44704f * 10.0f;          // the fmuls the CRT thunk performs
    const f32 kfFsel[] = { 0.44704f * 120.0f,     // a Road Rage turning reset (type 5)
                           0.44704f * 50.0f,      // a crash-exit reset
                           kfCap,                 // exactly the cap: f12 == 0 -> K
                           2.0f,                  // below the cap: passes through
                           0.0f };                // a crash-exit reset at rest
    for (f32 lfSpeed : kfFsel) {
        const f32 lfExpected = (lfSpeed - kfCap >= 0.0f) ? kfCap : lfSpeed;   // fsel
        Fixture::ResetOnTrackResult lResult{ lfSpeed };
        char lacName[96];
        std::snprintf(lacName, sizeof(lacName), "FAILURE arm: requested %.4f m/s -> %.4f (min with flt_82FAD610)",
                      lfSpeed, lfExpected);
        Check(Fixture::FailureArmSpeed(lResult) == lfExpected, lacName);
    }
    std::printf("RcemFailureResetSpeed: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
