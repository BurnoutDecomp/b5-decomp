// FX-AINAN2 regression (crash parity 2026-09-24): AIModule::SetSuitabilityForAggression @0x8276E7C0,
// extracted verbatim from BrnAIModule_Drive.cpp by run_fxainan2_aimodule.py.
//
// Console: both float tests are `fcmpu ; blt -> not suitable` -- AICar::GetSpeed against
// flt_8300D704 (60 mph) at 0x8276E8D4/0x8276E8D8, and mfRaceTimer (+0x14FC) against flt_820C4150
// (10.0) at 0x8276E8F4/0x8276E8F8 -- and blt is NOT taken on an unordered compare, so a NaN falls
// through to `li r11,1 ; stb r11,0x60(driver aggression)` (suitable). The old `>=` spellings said
// no. The ordered controls pass on both spellings.
//
// Fixtures: AIModule::GetAIDriver answers the one configured driver, AICar::GetSpeed returns
// mfSpeedInRange. The module lives in raw storage (its constructor is in another TU); the body
// under test reads only plain members.
#define BRNAI_AIWAVE_A3_LANDED 1
#include "GameSource/World/AI/BrnAIModule.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAICar.h"
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

static BrnAI::AIDriver* gpDriver = nullptr;
namespace BrnAI {
AIDriver* AIModule::GetAIDriver(EActiveRaceCarIndex) { return gpDriver; }
f32 AICar::GetSpeed() const { return mfSpeedInRange; }
}

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

    alignas(16) unsigned char gaModule[sizeof(AIModule)];
    AICar    gCar{};
    AIDriver gDriver;

    AIModule& Module() { return *reinterpret_cast<AIModule*>(gaModule); }

    // A racing rival (style RACE, active, offline, aggressive driving on, not the player's seat).
    bool Suitable(f32 lfSpeed, f32 lfRaceTimer, ERouteFindingStyle leStyle = E_ROUTE_FINDING_RACE)
    {
        gCar = AICar{};
        gCar.meRouteFindingStyle = leStyle;
        gCar.mfSpeedInRange = lfSpeed;
        gCar.mfRaceTimer = lfRaceTimer;
        gDriver.mpCarHost = &gCar;
        gDriver.mbIsActive = 1;
        gDriver.GetAggression()->mbIsSuitableForAggression = false;
        gpDriver = &gDriver;
        Module().mbIsInOnlineGameMode = false;
        Module().mbDoAggressiveDriving = true;
        Module().mePlayerActiveRaceCarIndex = static_cast<EActiveRaceCarIndex>(0);
        Module().SetSuitabilityForAggression(static_cast<EActiveRaceCarIndex>(3), nullptr);
        return gDriver.GetAggression()->mbIsSuitableForAggression;
    }
}

int main()
{
    std::memset(gaModule, 0, sizeof(gaModule));

    Check(Suitable(KF_NAN, 20.0f),
          "a NaN speed falls through blt 0x8276E8D8 -> suitable");
    Check(Suitable(100.0f, KF_NAN),
          "a NaN race timer falls through blt 0x8276E8F8 -> suitable");

    // Ordered controls.
    Check(!Suitable(10.0f, 20.0f), "control: below 60 mph (26.8224 m/s) is not suitable");
    Check(!Suitable(100.0f, 5.0f), "control: a racer 5 s into the race is not suitable");
    Check(Suitable(100.0f, 15.0f), "control: a racer 15 s in at speed is suitable");
    Check(Suitable(100.0f, 0.0f, E_ROUTE_FINDING_FREE_ROAM), "control: free roam skips the race-timer test");
    Check(Suitable(0.0f, 0.0f, E_ROUTE_FINDING_ROAD_RAGE), "control: road rage is always suitable");

    std::printf("FxAinan2AIModule: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
