// FX-TAILS-A item 4 (crash parity 2026-09-24): CrashPlayManager::OnBounce @0x822A7EF8 and
// OnVehicleHitConfirmed @0x822C3348, run on the PRODUCTION bodies (extracted from BrnCrashPlayManager.cpp by
// run_fxtailsa_crash_play.py, with the file's tuning constants and its [crashplay] witness) against the REAL
// BrnWorld::CrashPlayManager struct (BrnCrashPlayDebugComponent.h), compared bit for bit with a model of the
// ARTIST asm written here independently:
//   OnBounce       cost = fmadds(HARD - EASY, mfDifficultyLevel, EASY)   @0x822A7F6C (ONE rounding)
//                  boost = boost - cost @0x822A7F70, then the Clamp(0,100) fsel pair
//                  stationary: sum = power + 0.5 ; fsel(sum - 1.0, 1.0, sum) @0x822A8018 (a NaN sum is KEPT)
//   OnVehicleHitConfirmed
//                  chain == 0: award = fmadds(scaled, HIGH - LOW, LOW) @0x822C33AC ; boost = award + boost
//                  total > 0 : d = Clamp(total / 100, 0, 1) ; total % 10 == 0 ->
//                              boost = fmadds(HI - LO, d, boost) @0x822C3448 + LO @0x822C344C
// fmadds = the exact product-sum rounded once to single = std::fmaf. The inputs are ones where one rounding
// and two differ (found by exhaustive search): difficulty 0.48 (cost 14.8000002 vs 14.7999992), base score
// 1256 (award 20.960001 vs 20.959999), 30 cars with boost 8.15 (49.1499977 vs 49.1500015).
#include "GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/fpu/scalar_operation.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gChecks = 0, gFailures = 0, gAsserts = 0;

namespace CgsDev
{
namespace Assert
{
    int   BeginAssert() { return 0; }
    int   FireAssert(const char* lpcMessage, const char*, int)
    {
        ++gAsserts;
        std::printf("  [assert] %s\n", lpcMessage ? lpcMessage : "");
        return 0;
    }
    void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

namespace BrnWorld
{
// The production constants, witness and bodies, extracted verbatim.
#include "fxtailsa_crash_play.inc"
}

using BrnWorld::CrashPlayManager;
using BrnGameState::GameStateModuleIO::JustBouncedAction;

// ---- the console model, from the asm ----------------------------------------------------------------
static f32 Fsel(f32 lfTest, f32 lfIfGe, f32 lfElse) { return (lfTest >= 0.0f) ? lfIfGe : lfElse; }
static f32 ConsoleClamp100(f32 lfX)
{
    const f32 lf0 = Fsel(-lfX, 0.0f, lfX);               // fneg ; fsel f0, f13, 0.0, x
    return Fsel(100.0f - lf0, lf0, 100.0f);              // fsubs ; fsel f0, f12, f0, 100.0
}
static f32 ConsoleBounceBoost(f32 lfBoost, f32 lfDifficulty)
{
    const f32 lfCost = std::fmaf(20.0f - 10.0f, lfDifficulty, 10.0f);   // 0x82CDB52C - 0x82CDB528 ; fmadds
    return ConsoleClamp100(lfBoost - lfCost);
}
static f32 ConsoleStationary(f32 lfPower)
{
    const f32 lfSum = lfPower + 0.5f;                    // flt_820147FC
    return Fsel(lfSum - 1.0f, 1.0f, lfSum);              // flt_82001C98
}
static f32 ConsoleImpactBoost(f32 lfBoost, s32 liScore)
{
    const f32 lfScore  = static_cast<f32>(liScore);
    const f32 lfLow    = Fsel(1000.0f - lfScore, 1000.0f, lfScore);     // 0x82CDB55C
    const f32 lfClamp  = Fsel(5000.0f - lfLow, lfLow, 5000.0f);         // 0x82CDB560
    const f32 lfScaled = (lfClamp - 1000.0f) / (5000.0f - 1000.0f);
    const f32 lfAward  = std::fmaf(lfScaled, 35.0f - 20.0f, 20.0f);    // 0x82CDB50C - 0x82CDB508 ; fmadds
    return ConsoleClamp100(lfAward + lfBoost);
}
static f32 ConsoleDifficulty(s32 liTotal)
{
    const f32 lfD = static_cast<f32>(liTotal) / 100.0f;                 // 0x82CDB54C
    const f32 lf0 = Fsel(-lfD, 0.0f, lfD);
    return Fsel(1.0f - lf0, lf0, 1.0f);
}
static f32 ConsoleEveryTenBoost(f32 lfBoost, f32 lfDifficulty)
{
    const f32 lfSum = std::fmaf(20.0f - 50.0f, lfDifficulty, lfBoost);  // 0x82CDB514 - 0x82CDB510 ; fmadds
    return ConsoleClamp100(lfSum + 50.0f);                              // fadds + LO
}

// ---- the harness ------------------------------------------------------------------------------------
static bool Same(f32 a, f32 b)
{
    u32 x, y;
    std::memcpy(&x, &a, 4);
    std::memcpy(&y, &b, 4);
    return x == y || (std::isnan(a) && std::isnan(b));
}

static void Check(bool lbPass, const char* lpcName)
{
    ++gChecks;
    if (!lbPass)
        ++gFailures;
    std::printf("  %s %s\n", lbPass ? "ok  " : "FAIL", lpcName);
}

alignas(16) static unsigned char gStorage[sizeof(CrashPlayManager)];

static CrashPlayManager& Fresh()
{
    std::memset(gStorage, 0, sizeof(gStorage));
    return *reinterpret_cast<CrashPlayManager*>(gStorage);
}

static JustBouncedAction Bounce(bool lbOnCar, bool lbFromStationary)
{
    JustBouncedAction lAction;
    std::memset(&lAction, 0, sizeof(lAction));
    lAction.mbOnCar          = lbOnCar;
    lAction.mbFromStationary = lbFromStationary;
    return lAction;
}

int main()
{
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();

    std::printf("OnBounce -- the charged bounce's cost (fmadds @0x822A7F6C):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mfBounceBoostTimer = 0.5f; m.mbBoostChargePending = true;
        m.mfBoostPercentage = 15.0f; m.mfDifficultyLevel = 0.48f;
        const JustBouncedAction a = Bounce(true, false);
        m.OnBounce(&a);
        Check(Same(m.mfBoostPercentage, ConsoleBounceBoost(15.0f, 0.48f)),
              "difficulty 0.48: boost = 15 - fmadds(10, 0.48, 10) = 0.1999998 (one rounding; two give 0.2000008)");
        Check(!m.mbBoostChargePending, "the pending charge is consumed (stb 0 +0x153)");
    }
    {
        CrashPlayManager& m = Fresh();
        m.mfBounceBoostTimer = 0.5f; m.mbBoostChargePending = true;
        m.mfBoostPercentage = 20.0f; m.mfDifficultyLevel = 0.96f;
        const JustBouncedAction a = Bounce(true, false);
        m.OnBounce(&a);
        Check(Same(m.mfBoostPercentage, ConsoleBounceBoost(20.0f, 0.96f)),
              "difficulty 0.96: boost = 20 - fmadds(10, 0.96, 10) = 0.3999996 (two roundings give 0.4000015)");
    }
    {
        CrashPlayManager& m = Fresh();
        m.mfBounceBoostTimer = 0.0f; m.mbBoostChargePending = true; m.mfBoostPercentage = 50.0f;
        const JustBouncedAction a = Bounce(true, false);
        m.OnBounce(&a);
        Check(m.mfBoostPercentage == 50.0f && m.mbBoostChargePending,
              "not bounce-boosting (timer 0 is not > 0, bgt @0x822A7F24): nothing charged");
    }

    std::printf("OnBounce -- the stationary aftertouch top-up (fsel @0x822A8018):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mfAftertouchPower = 0.2f;
        const JustBouncedAction a = Bounce(true, true);
        m.OnBounce(&a);
        Check(Same(m.mfAftertouchPower, ConsoleStationary(0.2f)), "0.2 + 0.5 = 0.7 (below the 1.0 cap)");
        m.mfAftertouchPower = 0.7f;
        m.OnBounce(&a);
        Check(Same(m.mfAftertouchPower, 1.0f), "0.7 + 0.5 is capped at 1.0");
        m.mfAftertouchPower = lfNan;
        m.OnBounce(&a);
        Check(std::isnan(m.mfAftertouchPower), "a NaN power stays NaN (fsel keeps its third operand, the sum)");
    }

    std::printf("OnBounce -- ground / car bookkeeping:\n");
    {
        CrashPlayManager& m = Fresh();
        const JustBouncedAction lGround = Bounce(false, false);
        m.OnBounce(&lGround);
        Check(m.miConsecutiveBouncesOnGround == 1 && m.mbAboutToLoseBoost && m.mfLoseBoostGracePeriod == 0.25f,
              "a ground bounce counts and opens the 0.25 s grace (flt_82CDB538)");
        m.mfLoseBoostGracePeriod = 0.1f;
        m.OnBounce(&lGround);
        Check(m.miConsecutiveBouncesOnGround == 2 && m.mfLoseBoostGracePeriod == 0.1f,
              "a second ground bounce does not re-open the grace (lbz 0x151 ; bne)");
        const JustBouncedAction lCar = Bounce(true, false);
        m.OnBounce(&lCar);
        Check(m.miConsecutiveBouncesOnGround == 0 && !m.mbAboutToLoseBoost, "a car bounce resets both");
    }

    std::printf("OnVehicleHitConfirmed -- the per-vehicle award (fmadds @0x822C33AC):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mfBoostPercentage = 10.0f;
        m.OnVehicleHitConfirmed(1256, 0, 0);
        Check(Same(m.mfBoostPercentage, ConsoleImpactBoost(10.0f, 1256)),
              "score 1256: award = fmadds(0.064, 15, 20) = 20.960001 (one rounding) + 10");
        m.mfBoostPercentage = 10.0f;
        m.OnVehicleHitConfirmed(1369, 0, 0);
        Check(Same(m.mfBoostPercentage, ConsoleImpactBoost(10.0f, 1369)), "score 1369: the same, bit for bit");
        m.mfBoostPercentage = 10.0f;
        m.OnVehicleHitConfirmed(9000, 0, 0);
        Check(Same(m.mfBoostPercentage, ConsoleImpactBoost(10.0f, 9000)), "score 9000 clamps to 5000: +35");
        m.mfBoostPercentage = 10.0f;
        m.OnVehicleHitConfirmed(1256, 2, 0);
        Check(m.mfBoostPercentage == 10.0f, "a chained hit (bonus != 0) pays nothing (bne @0x822C3360)");
    }

    std::printf("OnVehicleHitConfirmed -- difficulty and the every-10-cars award (fmadds @0x822C3448, + LO):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mfBoostPercentage = 8.15f;
        m.OnVehicleHitConfirmed(0, 1, 30);
        Check(Same(m.mfDifficultyLevel, ConsoleDifficulty(30)), "30 cars: difficulty 0.3");
        Check(Same(m.mfBoostPercentage, ConsoleEveryTenBoost(8.15f, ConsoleDifficulty(30))),
              "30 cars, boost 8.15: (8.15 + (20 - 50) * 0.3) fused, + 50 = 49.1499977");
        m.mfBoostPercentage = 9.4f;
        m.OnVehicleHitConfirmed(0, 1, 30);
        Check(Same(m.mfBoostPercentage, ConsoleEveryTenBoost(9.4f, ConsoleDifficulty(30))),
              "30 cars, boost 9.4: the console's order, bit for bit");
        m.mfBoostPercentage = 40.0f;
        m.OnVehicleHitConfirmed(0, 1, 35);
        Check(m.mfBoostPercentage == 40.0f && Same(m.mfDifficultyLevel, ConsoleDifficulty(35)),
              "35 cars: difficulty 0.35, no award (35 % 10 != 0, bnelr @0x822C3434)");
        m.mfDifficultyLevel = 0.5f;
        m.OnVehicleHitConfirmed(0, 1, 0);
        Check(m.mfDifficultyLevel == 0.5f && m.mfBoostPercentage == 40.0f, "0 cars: nothing (blelr @0x822C33D4)");
        m.OnVehicleHitConfirmed(0, 1, 250);
        Check(m.mfDifficultyLevel == 1.0f, "250 cars: difficulty clamps to 1.0");
    }

    Check(gAsserts == 0, "no asserts");
    std::printf("FxTailsACrashPlay: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
