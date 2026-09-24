// FX-AINAN2 regression (crash parity 2026-09-24): the [0,1] asserts of the three Aggressiveness
// speed-match setters, extracted verbatim from BrnAIAggressiveness.cpp by
// run_fxainan2_aggressiveness.py.
//
// Console, each setter: `fcmpu v, 0.0 (flt_82001CC0) ; blt -> fire` then `fcmpu v, 1.0
// (flt_82001C98) ; ble -> skip` -- SetProximityToSpeedMatch @0x827645E0 (0x82764604 /
// 0x82764614), SetAcclerationRateForSpeedMatch @0x827646C8 (0x827646EC / 0x827646FC),
// SetTimeForSpeedMatch @0x827647B0 (0x827647D4 / 0x827647E4). blt is not taken on an unordered
// compare and ble is, so a NaN skips the assert; `v >= 0 && v <= 1` fired it. The value is
// stored either way.
#include "GameSource/World/AI/BrnAIAggressiveness.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    unsigned SetAll(Aggressiveness& lrAggressiveness, f32 lfValue)
    {
        gAssertions = 0;
        lrAggressiveness.SetProximityToSpeedMatch(lfValue);
        lrAggressiveness.SetAcclerationRateForSpeedMatch(lfValue);
        lrAggressiveness.SetTimeForSpeedMatch(lfValue);
        return gAssertions;
    }
}

int main()
{
    Aggressiveness lAggressiveness;
    std::memset(&lAggressiveness, 0, sizeof(lAggressiveness));

    Check(SetAll(lAggressiveness, 0.0f) == 0u, "control: 0.0 is in range");
    Check(SetAll(lAggressiveness, 1.0f) == 0u, "control: 1.0 is in range");
    Check(SetAll(lAggressiveness, 0.7f) == 0u, "control: 0.7 is in range");
    Check(SetAll(lAggressiveness, -0.5f) == 3u, "control: -0.5 fires all three (blt taken)");
    Check(SetAll(lAggressiveness, 1.5f) == 3u, "control: 1.5 fires all three (ble not taken)");
    Check(SetAll(lAggressiveness, KF_NAN) == 0u,
          "a NaN falls through blt and takes ble -> no assert in any of the three");
    Check(lAggressiveness.mfProximitySpeedMatch != lAggressiveness.mfProximitySpeedMatch &&
          lAggressiveness.mfAcclerationRateForSpeedMatch != lAggressiveness.mfAcclerationRateForSpeedMatch &&
          lAggressiveness.mfTimeForSpeedMatch != lAggressiveness.mfTimeForSpeedMatch,
          "control: ... and the NaN is stored (stfs after the assert block)");

    std::printf("FxAinan2Aggressiveness: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
