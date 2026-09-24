// FX-RCEM4 (crash parity 2026-09-24, G61-D4): ActiveRaceCar::Update's start-line boost flame
// (ARTIST 0x822F7C58..0x822F7D14) on the module's non-deterministic RNG (+0x18490).
// run_fxrcem4_start_line_boost.py extracts the block VERBATIM from ActiveRaceCar::Update and runs it
// against a fixture car with the real CgsNumeric::Random (CgsRandom.h / CgsRandom.cpp).
//   0x822F7C60  IsOnRaceStartState(0) ; beq skip
//   0x822F7C70  lfs 0x734 ; fcmpu f30 (0.0) ; bge -> 0x822F7D0C   (a NaN does not flip)
//   0x822F7CA4  cntlzw / rlwinm 27,31,31 / stb 0x780              (mbIsDoingStartLineBoost = !it)
//   0x822F7CB4  ring draw: lfsx ; fsubs 1.0 ; refill ; mulld ; idx+1 & 7    (Random::RandomFloat)
//   0x822F7D04  fmadds t * 0.85 (flt_82013A78) + 0.25 (flt_82003F40) -> 0x734
//   0x822F7D0C  0x734 -= lfTimeStep
#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned guAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++guAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; void WriteToLog(const char*) {} }
namespace Message { u64 gxMessageFilterFlags = 0; }
}

namespace Fixture {
struct ActiveRaceCar {
    enum ERaceStartState : s32 { E_RACE_START_STATE_ON_START_LINE = 0, E_RACE_START_STATE_RACING = 2 };
    s32  meRaceStartState = E_RACE_START_STATE_ON_START_LINE;
    f32  mfTimeToStartLineBoostChange = -1.0f;   // Construct's flt_820037C8
    bool mbIsDoingStartLineBoost = false;
    bool IsOnRaceStartState(s32 liState) const { return meRaceStartState == liState; }
    void StartLineBlock(f32 lfTimeStep, CgsNumeric::Random* lpRandom);
};
void ActiveRaceCar::StartLineBlock(f32 lfTimeStep, CgsNumeric::Random* lpRandom)
{
    (void)lfTimeStep; (void)lpRandom;
#include "fxrcem4_slb_block.inc"
}
}   // namespace Fixture

static int giChecks = 0, giFailures = 0;
static void Check(bool lbPass, const char* lpcName) {
    ++giChecks;
    if (!lbPass) { ++giFailures; std::fprintf(stderr, "FAIL: %s\n", lpcName); }
}

#include "fxrcem4_slb_struct.inc"   // static const bool gbStructuralOk (computed by the runner)

int main() {
    using namespace Fixture;
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();

    CgsNumeric::Random lRandom; lRandom.Construct();
    CgsNumeric::Random lReference; lReference.Construct();

    ActiveRaceCar car;
    car.meRaceStartState = ActiveRaceCar::E_RACE_START_STATE_RACING;
    car.StartLineBlock(0.1f, &lRandom);
    Check(car.mfTimeToStartLineBoostChange == -1.0f && !car.mbIsDoingStartLineBoost,
          "a racing car skips the block (IsOnRaceStartState(0) ; beq)");

    car.meRaceStartState = ActiveRaceCar::E_RACE_START_STATE_ON_START_LINE;
    car.StartLineBlock(0.1f, &lRandom);
    const f32 lfFirst = std::fmaf(lReference.RandomFloat(), 0.85f, 0.25f);
    Check(car.mbIsDoingStartLineBoost, "on the start line with the countdown below 0: the flame flips on");
    Check(car.mfTimeToStartLineBoostChange == lfFirst - 0.1f,
          "re-armed from ONE ring draw, t * 0.85 + 0.25 rounded once (fmadds), then run down by dt");

    const f32 lfArmed = car.mfTimeToStartLineBoostChange;
    car.StartLineBlock(0.05f, &lRandom);
    Check(car.mbIsDoingStartLineBoost && car.mfTimeToStartLineBoostChange == lfArmed - 0.05f,
          "a running countdown only ticks down (bge skips the flip and the draw)");

    car.mfTimeToStartLineBoostChange = -0.01f;
    car.StartLineBlock(0.1f, &lRandom);
    const f32 lfSecond = std::fmaf(lReference.RandomFloat(), 0.85f, 0.25f);
    Check(!car.mbIsDoingStartLineBoost && car.mfTimeToStartLineBoostChange == lfSecond - 0.1f,
          "the next expiry flips it off and draws the NEXT ring value (the RNG is shared state)");

    car.mfTimeToStartLineBoostChange = 0.0f;
    car.StartLineBlock(0.1f, &lRandom);
    Check(!car.mbIsDoingStartLineBoost && car.mfTimeToStartLineBoostChange == -0.1f,
          "exactly 0.0 is not below 0.0: no flip, just the tick");

    car.mfTimeToStartLineBoostChange = lfNan;
    CgsNumeric::Random lBefore = lRandom;
    car.StartLineBlock(0.1f, &lRandom);
    Check(!car.mbIsDoingStartLineBoost && std::isnan(car.mfTimeToStartLineBoostChange)
          && std::memcmp(&lBefore, &lRandom, sizeof(lRandom)) == 0,
          "a NaN countdown does not flip or draw (bge is taken on NaN)");

    Check(gbStructuralOk, "the module seeds, steps and passes its RNG (Construct / PreSceneUpdate / UpdateActiveCars)");
    Check(guAssertions == 0, "no assertions");

    std::printf("FxRcem4StartLineBoost: %d checks, %d failures\n", giChecks, giFailures);
    return giFailures ? 1 : 0;
}
