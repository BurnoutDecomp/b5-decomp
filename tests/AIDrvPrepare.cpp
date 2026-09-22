// FX-AIDRV regression G03-D1 (crash parity 2026-09-22): the production AIDriver::Prepare,
// extracted verbatim from src/GameSource/World/AI/BrnAIDriver.cpp by run_aidrv_prepare.py, run
// the way AIModule::Prepare @0x82798070 runs it: eight drivers, one shared &AIModule::mRandom.
//
// Console @0x82792CA8 (r30 = r6 = lpRandom, r28 = this+0xF20 = the embedded RacingLine):
//   0x82792D10..0x82792D84  inlined Random::RandomFloat: f13 = ring[oldest] ; refill that slot
//     0x3F800000 | (oldSeed.hi >> 9) ; seed = seed*0x5851F42D4C957F2D + 1 ; oldest = (oldest+1)&7 ;
//     t = f13 - 1.0 (flt_82001C98)
//   0x82792D9C vsubfp  v12 = FAR - CLOSE ; 0x82792DA0 vmaddfp v0 = (FAR - CLOSE)*t + CLOSE
//     CLOSE = flt_820C3DDC = 0x3F7F5C29 (0.9975), FAR = 0x820C3DE0 = 0x3F7C28F6 (0.985)
//   0x82792DB0 stfs v, 0xC04(r28)                        -> RacingLine::mfCentreLineAhead
//   0x82792DAC fsubs 1.0 - v ; 0x82792DB4 fdivs ; 0x82792DB8 stfs 0xC08(r28)
//                                                        -> RacingLine::mfCentreLineAheadRecip
// The fixtures are the three callees whose bodies are not under test (ResetAttribSysValues,
// RacingLine::ClearSectionCache, SteeringFan::Prepare); the Random is the real CgsRandom.cpp.
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/Route/BrnRacingLine.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

static unsigned guClearSectionCache = 0, guFanPrepare = 0, guResetAttrib = 0;
namespace BrnAI {
void AIDriver::ResetAttribSysValues() { ++guResetAttrib; }
RacingLine* RacingLine::ClearSectionCache() { ++guClearSectionCache; return this; }
void SteeringFan::Prepare() { ++guFanPrepare; }
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
    f32 FromBits(u32 luBits) { f32 lfValue; std::memcpy(&lfValue, &luBits, sizeof(lfValue)); return lfValue; }
    bool SameBits(f32 lfA, f32 lfB) { return std::memcmp(&lfA, &lfB, sizeof(lfA)) == 0; }

    // A freshly Construct'ed Random's ring (the words AICar::Reset stores at 0x82792900..0x82792964
    // are the same Construct): the value each successive driver draws is ring[i] - 1.0.
    const u32 KAU_FRESH_RING[8] = { 0x3F800000u, 0x3FE43E6Cu, 0x3F98B09Cu, 0x3FDA23E0u,
                                    0x3FE21EDCu, 0x3FDDEB96u, 0x3F9C9A72u, 0x3F923D76u };
    const f32 KF_CLOSE = FromBits(0x3F7F5C29u);   // flt_820C3DDC
    const f32 KF_FAR   = FromBits(0x3F7C28F6u);   // 0x820C3DE0

    AIDriver gaDrivers[8];
}

int main()
{
    Check(SameBits(KF_CENTRE_LINE_AHEAD_CLOSE, KF_CLOSE), "KF_CENTRE_LINE_AHEAD_CLOSE is the image word 0x3F7F5C29");
    Check(SameBits(KF_CENTRE_LINE_AHEAD_FAR, KF_FAR), "KF_CENTRE_LINE_AHEAD_FAR is the image word 0x3F7C28F6");

    CgsNumeric::Random lRandom;
    lRandom.Construct();
    const u64 luSeed0 = lRandom.muSeed;

    for (int liDriver = 0; liDriver < 8; ++liDriver)
    {
        // Stale values from a previous race must be overwritten, not kept.
        gaDrivers[liDriver].GetRacingLine().mfCentreLineAhead = -7.0f;
        gaDrivers[liDriver].GetRacingLine().mfCentreLineAheadRecip = -7.0f;
        gaDrivers[liDriver].Prepare(nullptr, liDriver, &lRandom);
    }

    char lacLabel[200];
    for (int liDriver = 0; liDriver < 8; ++liDriver)
    {
        const f32 lfT = FromBits(KAU_FRESH_RING[liDriver]) - 1.0f;
        const f32 lfExpected = (KF_FAR - KF_CLOSE) * lfT + KF_CLOSE;   // the vmaddfp @0x82792DA0
        const f32 lfAhead = gaDrivers[liDriver].GetRacingLine().mfCentreLineAhead;
        const f32 lfRecip = gaDrivers[liDriver].GetRacingLine().mfCentreLineAheadRecip;
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "driver %d: mfCentreLineAhead == (FAR-CLOSE)*(ring[%d]-1)+CLOSE = %.9g (got %.9g)",
                      liDriver, liDriver, lfExpected, lfAhead);
        Check(std::fabs(lfAhead - lfExpected) <= 1.0e-7f, lacLabel);   // fused vs unfused: <= 1 ulp
        std::snprintf(lacLabel, sizeof(lacLabel), "driver %d: mfCentreLineAhead in [0.985, 0.9975]", liDriver);
        Check(lfAhead >= KF_FAR && lfAhead <= KF_CLOSE, lacLabel);
        std::snprintf(lacLabel, sizeof(lacLabel),
                      "driver %d: mfCentreLineAheadRecip == 1/(1 - v) (fsubs + fdivs @0x82792DAC/DB4)", liDriver);
        Check(SameBits(lfRecip, 1.0f / (1.0f - lfAhead)), lacLabel);
    }
    // Driver 0 draws the fresh ring's 1.0 -> t = 0 -> exactly CLOSE, recip ~400.0004 (the same
    // pair AICar::OnModeStart writes for the player car at 0x8277BEBC/0x8277BEC4).
    Check(SameBits(gaDrivers[0].GetRacingLine().mfCentreLineAhead, KF_CLOSE), "driver 0: exactly CLOSE (t = 0)");
    Check(std::fabs(gaDrivers[0].GetRacingLine().mfCentreLineAheadRecip - 400.0004f) < 0.001f,
          "driver 0: recip 400.0004");

    // Eight Prepares = eight RandomFloat steps on the shared Random: the cursor wraps to 0 and
    // the seed has taken eight LCG steps.
    u64 luSeed = luSeed0;
    for (int liStep = 0; liStep < 8; ++liStep)
        luSeed = luSeed * 0x5851F42D4C957F2Dull + 1u;
    Check(lRandom.muOldestBufferIndex == 0, "the shared Random's cursor advanced 8 slots (wrapped to 0)");
    Check(lRandom.muSeed == luSeed, "the shared Random's seed took 8 LCG steps (std 0x20 @0x82792D54)");
    Check(lRandom.mauIntegerBuffer[0] == (0x3F800000u | (static_cast<u32>(luSeed0 >> 32) >> 9)),
          "slot 0 refilled from the pre-draw seed's high word (inslwi 23,9 ; stwx @0x82792D5C)");

    Check(guResetAttrib == 8 && guClearSectionCache == 8 && guFanPrepare == 8, "each Prepare ran its three callees once");
    Check(gAssertions == 0, "no assertion fired");
    std::printf("AIDrvPrepare: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
