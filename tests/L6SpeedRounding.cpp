// L6 AIDRIVE regression (owner list 2026-09-27): ROUNDING_RULE rule 3 on the AI speed path. The production bodies
// are extracted verbatim by run_l6_speed_rounding.py:
//   * RaceBalancingRoute::GetAISectionSpeed @0x827697A0 -- the rubber band's par-speed lerp,
//       `fsubs f13, max, min ; fmadds f1, f13, f31 (ratio), f0 (min)`  @0x8276980C/0x82769810
//   * AIAggression::CalcSpeedMatchSpeed @0x8278B7A8 -- the speed-match acceleration cap,
//       `fmadds f0, f12 (knob), f0 (flt_820C4238 15.0), f13 (flt_820C488C 5.0)`  @0x8278B824
//   * AICar::CalcDesiredSpeed @0x82796078 -- the meBehaviour == 1 opponent-rank speed,
//       `fnmsubs f1, f12 (index), f0 (scale), f13 (base)` = -(index * scale - base)  @0x82796138 / 0x82796168
// Each rounds ONCE; the PC used to round the product first. Every input is self-checked to separate one rounding
// from two. Callees not under test are fixtures (StepTo records the step it is handed).
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/BrnAIAggression.h"
#include "GameSource/World/AI/BrnAIAggressiveness.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingRoute.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <new>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

static f32 gfStepToMaxStep = -1.0f;
static f32 gfSpeedMatchSpeed = 0.0f;
namespace BrnAI {
f32 AICar::GetSpeed() const { return mfSpeedInRange; }
f32 AICar::GetDecentSpeed() const { return 0.0f; }
f32 AICar::CalcRoadRageSpeed(const AICar*) { return 0.0f; }
f32 AICar::CalcPersonalitySpeed(const AICar*) { return 0.0f; }
f32 RaceBalancingManager::ComputeTargetSpeed(const AICar*, const AISectionsData*, bool) const { return 0.0f; }
f32 AIAggression::GetSpeedMatchSpeed(f32) { return gfSpeedMatchSpeed; }
f32 StepTo(f32, f32 lfTarget, f32 lfStep) { gfStepToMaxStep = lfStep; return lfTarget; }
}

#include "l6_speed_rounding.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        if (!lbPass) { ++guFailures; std::printf("FAIL %s\n", lpcLabel); }
    }
    u32 Bits(f32 lfValue) { u32 luBits; std::memcpy(&luBits, &lfValue, 4); return luBits; }
    bool Same(f32 lfA, f32 lfB) { return Bits(lfA) == Bits(lfB); }
    f32 TwoRoundings(f32 lfA, f32 lfB, f32 lfC)
    {
        volatile f32 lfProduct = lfA * lfB;
        return lfProduct + lfC;
    }
    alignas(16) unsigned char gacCar[sizeof(AICar)];
    alignas(16) unsigned char gacAggression[sizeof(AIAggression)];
    alignas(16) unsigned char gacRoute[sizeof(RaceBalancingRoute)];
    alignas(16) unsigned char gacSections[sizeof(AISectionsData)];
    alignas(16) unsigned char gacSection[sizeof(AISection)];
}

static void GroupSectionSpeed()
{
    std::memset(gacRoute, 0, sizeof(gacRoute));
    std::memset(gacSections, 0, sizeof(gacSections));
    std::memset(gacSection, 0, sizeof(gacSection));
    RaceBalancingRoute* lpRoute = reinterpret_cast<RaceBalancingRoute*>(gacRoute);
    AISectionsData* lpSections = reinterpret_cast<AISectionsData*>(gacSections);
    AISection* lpSection = reinterpret_cast<AISection*>(gacSection);
    // the shipped AI.DAT speed bands (FX-AINAN2's read: min 67.05 / 24.585 / ..., max 71.52 / ...), class 1
    lpSections->mafSectionMinSpeeds[1] = 24.585f;
    lpSections->mafSectionMaxSpeeds[1] = 58.115f;
    lpSection->muSpeed = 1;
    const f32 lafRatios[] = { 0.033f, 0.086f, 0.37f };   // 0.033 / 0.086 separate one rounding from two
    s32 liDiscriminating = 0;
    bool lbAll = true;
    for (f32 lfRatio : lafRatios)
    {
        const f32 lfSpan = 58.115f - 24.585f;
        const f32 lfFused = std::fmaf(lfSpan, lfRatio, 24.585f);
        if (!Same(lfFused, TwoRoundings(lfSpan, lfRatio, 24.585f)))
            ++liDiscriminating;
        const f32 lfGot = lpRoute->GetAISectionSpeed(lpSection, lpSections, lfRatio);
        if (!Same(lfGot, lfFused))
        {
            lbAll = false;
            std::printf("  ratio %.9g: got %.9g, one rounding %.9g\n", lfRatio, lfGot, lfFused);
        }
    }
    Check(liDiscriminating > 0, "self-check: a par-speed ratio separates one rounding from two");
    Check(lbAll, "GetAISectionSpeed: the par speed is one rounding of (max - min) * ratio + min (fmadds 0x82769810)");
}

static void GroupSpeedMatch()
{
    std::memset(gacCar, 0, sizeof(gacCar));
    std::memset(gacAggression, 0, sizeof(gacAggression));
    AICar* lpCar = reinterpret_cast<AICar*>(gacCar);
    AIAggression* lpAggression = reinterpret_cast<AIAggression*>(gacAggression);
    lpAggression->mpCar = lpCar;
    lpCar->meRouteFindingStyle = E_ROUTE_FINDING_RACE;
    const f32 lafKnobs[] = { 0.016f, 0.073f, 0.47f };      // 0.016 / 0.073 separate one rounding from two
    s32 liDiscriminating = 0;
    bool lbAll = true;
    for (f32 lfKnob : lafKnobs)
    {
        lpCar->GetAggressiveness()->mfAcclerationRateForSpeedMatch = lfKnob;
        const f32 lfFused = std::fmaf(lfKnob, 15.0f, 5.0f);
        if (!Same(lfFused, TwoRoundings(lfKnob, 15.0f, 5.0f)))
            ++liDiscriminating;
        gfStepToMaxStep = -1.0f;
        lpAggression->CalcSpeedMatchSpeed(0.0f, 1.0f);    // target-speed multiplier 1.0: the step IS the cap
        if (!Same(gfStepToMaxStep, lfFused))
        {
            lbAll = false;
            std::printf("  knob %.9g: step %.9g, one rounding %.9g\n", lfKnob, gfStepToMaxStep, lfFused);
        }
    }
    Check(liDiscriminating > 0, "self-check: a speed-match knob separates one rounding from two");
    Check(lbAll, "CalcSpeedMatchSpeed: the acceleration cap is one rounding of knob * 15 + 5 (fmadds 0x8278B824)");
    lpCar->meRouteFindingStyle = E_ROUTE_FINDING_ROAD_RAGE;
    lpAggression->CalcSpeedMatchSpeed(0.0f, 1.0f);
    Check(gfStepToMaxStep == 20.0f, "control: Road Rage keeps the flat 20.0 cap (flt_820C4890)");
}

static void GroupOpponentSpeed()
{
    std::memset(gacCar, 0, sizeof(gacCar));
    AICar* lpCar = reinterpret_cast<AICar*>(gacCar);
    lpCar->meCarState = E_AI_CAR_STATE_IN_RANGE;   // IsActive()
    lpCar->meBehaviour = static_cast<EAIBehaviour>(1);
    lpCar->mbIsPlayer = false;
    s32 liDiscriminating = 0;
    bool lbAll = true;
    for (s32 liStyle = 0; liStyle < 2; ++liStyle)
    {
        lpCar->meRouteFindingStyle = (liStyle == 0) ? E_ROUTE_FINDING_ROAD_RAGE : E_ROUTE_FINDING_RACE;
        const f32 lfScale = (liStyle == 0) ? KF_DESIRED_OPP_SCALE_RACE : KF_DESIRED_OPP_SCALE_DEFAULT;
        const f32 lfBase  = (liStyle == 0) ? KF_DESIRED_OPP_BASE_RACE  : KF_DESIRED_OPP_BASE_DEFAULT;
        // Swept: over the race range 0..7 the two spellings agree for both styles (value-neutral in play);
        // they part from index 13 (Road Rage) / 10 (otherwise) up, which is what makes this group RED.
        const s32 kaiIndices[] = { 1, 2, 3, 4, 5, 6, 7, 10, 11, 13, 14 };
        for (s32 liOpponent : kaiIndices)
        {
            lpCar->miOpponentIndex = static_cast<s8>(liOpponent);
            const f32 lfIndex = static_cast<f32>(liOpponent);
            const f32 lfFused = -std::fmaf(lfIndex, lfScale, -lfBase);
            volatile f32 lfProduct = lfIndex * lfScale;
            if (!Same(lfFused, lfBase - lfProduct))
                ++liDiscriminating;
            const f32 lfGot = lpCar->CalcDesiredSpeed(nullptr, nullptr, lpCar);
            if (!Same(lfGot, lfFused))
            {
                lbAll = false;
                std::printf("  style %d opponent %d: got %.9g, one rounding %.9g\n", liStyle, liOpponent, lfGot, lfFused);
            }
        }
    }
    Check(liDiscriminating > 0, "self-check: an opponent index separates one rounding from two");
    Check(lbAll, "CalcDesiredSpeed (behaviour 1): base - index * scale is one rounding (fnmsubs 0x82796138 / 0x82796168)");
}

int main()
{
    GroupSectionSpeed();
    GroupSpeedMatch();
    GroupOpponentSpeed();
    Check(gAssertions == 0, "no assertion fired");
    std::printf("L6SpeedRounding: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
