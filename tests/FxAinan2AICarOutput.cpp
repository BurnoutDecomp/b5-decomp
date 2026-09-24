// FX-AINAN2 regression (crash parity 2026-09-24): AICarOutputInterface::SetAICarDistanceToCheckpoint
// @0x827644F0, extracted verbatim from SharedIO/BrnAICarOutputInterface.cpp by
// run_fxainan2_ai_car_output.py.
//
// Console: `fcmpu d, flt_82001CC0 (0.0) ; bge -> skip the assert` @0x82764548/0x8276454C. bge is
// taken on an unordered compare, so a NaN distance skips "The lfDistToRouteEnd is less than 0!";
// the old `d >= 0` fired it (AIModule::ExportCarData calls this for every car, every frame). The
// value is stored either way.
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"
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

using namespace BrnAI::AIModuleIO;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    alignas(16) unsigned char gaOutput[sizeof(AICarOutputInterface)];
    AICarOutputInterface& Output() { return *reinterpret_cast<AICarOutputInterface*>(gaOutput); }

    unsigned AssertsFor(f32 lfDistance)
    {
        gAssertions = 0;
        Output().SetAICarDistanceToCheckpoint(3, lfDistance);
        return gAssertions;
    }
}

int main()
{
    std::memset(gaOutput, 0, sizeof(gaOutput));
    Check(AssertsFor(120.0f) == 0u, "control: 120 m is fine");
    Check(AssertsFor(0.0f) == 0u, "control: 0 m is fine (bge taken)");
    Check(AssertsFor(-1.0f) == 1u, "control: -1 m fires (bge not taken)");
    Check(AssertsFor(KF_NAN) == 0u, "a NaN takes bge 0x8276454C -> no assert");
    Check(Output().mafDistanceToCheckpoint[3] != Output().mafDistanceToCheckpoint[3],
          "control: ... and the NaN is stored");
    std::printf("FxAinan2AICarOutput: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
