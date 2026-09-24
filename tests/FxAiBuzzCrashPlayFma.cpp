// FX-AIBUZZ item 3 (crash parity 2026-09-24): the CrashPlayManager fused sites FX-TAILS-A left as follow-ups,
// run on the PRODUCTION bodies (UpdateMomentum, GetShowtimeTrafficDensityScale, ClampBoostLevel, the file's
// tunables, its [crashplay] witness and its console IsZero helper -- extracted from BrnCrashPlayManager.cpp by
// run_fxaibuzz_crashplay_fma.py) on the REAL CrashPlayManager / ActiveRaceCar structs, against the asm:
//   GetShowtimeTrafficDensityScale @0x822A8088   fmadds f31, (MAX - MIN), (1 - d), MIN        @0x822A8110
//   UpdateMomentum @0x823020D0
//     distance award   fmadds f0, K_DISTANCE(0x82FAD2FC), |d|2D, boost                         @0x82302174
//     ground cost      fnmsubs f13, K_GROUND(0x82CDB530), dt, boost   == boost - K*dt, fused    @0x823021F8
//     airtime award    fmadds f0, K_AIRTIME(0x82CDB51C), dt, boost                              @0x82302260
//     aftertouch bleed fnmsubs f0, dt, rate, aftertouch ; fsel >= 0                            @0x82302338
//     no-boost bleed   fnmsubs f0, dt, 0.5 (flt_820147FC), aftertouch ; fsel >= 0              @0x82302374
//     and the award's IsZero(mLastPlayerPos): vcmpgtfp. |v| > splat(flt_82014460 FLT_EPSILON), CR6 all-false
//     (0x82302128..0x82302160) -- a NaN lane counts as zero, and the tolerance is FLT_EPSILON, not 1e-6.
// fmadds / fnmsubs round ONCE (std::fmaf; fnmsubs == fmaf(-a, c, b) because round-to-nearest is symmetric).
// Every input below is one where one rounding and two differ (exhaustive search at dt = 1/60), except the
// no-boost bleed: dt * 0.5 is exact, so both spellings agree there and that check can only pin the form.
// KF_BOOST_FOR_DISTANCE_TRAVELLED is 0.0 in the shipped tuning (0 * d is exact), so its site is exercised with a
// tuned K = 0.1 written into the extracted (non-const) tunable, then restored.
#include "GameSource/World/EntityModules/RaceCarEntityModule/CrashPlay/BrnCrashPlayDebugComponent.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModuleIO.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "rw/math/vpu/vector3_operation.h"
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

// ---- fixtures for what UpdateMomentum calls outside the extracted bodies -------------------------------------
static Vector3 gPlayerPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
static f32     gfDistance2D    = 0.0f;
static int     giMagnitudeCalls = 0;
static int     giPromptCalls = 0;
static bool    gbLastPrompt = false;

Vector3 BrnWorld::ActiveRaceCar::GetPosition() const { return gPlayerPosition; }
// |xz| of (position - last): the fixed distance a case sets, or NaN when the vector carries one (as the real
// Magnitude2D would return).
namespace BrnMath
{
f32 Magnitude2D(Vector3 lVector)
{
    ++giMagnitudeCalls;
    return (std::isnan(lVector.x) || std::isnan(lVector.z)) ? std::numeric_limits<f32>::quiet_NaN() : gfDistance2D;
}
}

namespace BrnWorld
{
void CrashPlayManager::SetBouncePromptNeeded(bool lbPromptNeeded, RaceCarEntityModuleIO::OutputBuffer_PrePhysics*)
{
    ++giPromptCalls;
    gbLastPrompt = lbPromptNeeded;
}

// The production tunables, witness, helper and bodies, extracted verbatim.
#include "fxaibuzz_crashplay_fma.inc"
}

using BrnWorld::CrashPlayManager;
using BrnWorld::ActiveRaceCar;

namespace
{
    bool Same(f32 a, f32 b)
    {
        u32 x, y;
        std::memcpy(&x, &a, 4);
        std::memcpy(&y, &b, 4);
        return x == y || (std::isnan(a) && std::isnan(b));
    }

    void Check(bool lbPass, const char* lpcName)
    {
        ++gChecks;
        if (!lbPass) ++gFailures;
        std::printf("  %s %s\n", lbPass ? "ok  " : "FAIL", lpcName);
    }

    // The console's fsel-form clamp / max (ClampBoostLevel @0x82302268.., the aftertouch `fsel f0, f0, f0, 0.0`).
    f32 Fsel(f32 lfTest, f32 lfIfGe, f32 lfElse) { return (lfTest >= 0.0f) ? lfIfGe : lfElse; }
    f32 ConsoleClamp100(f32 lfX)
    {
        const f32 lf0 = Fsel(-lfX, 0.0f, lfX);
        return Fsel(100.0f - lf0, lf0, 100.0f);
    }
    f32 ConsoleMax0(f32 lfX) { return Fsel(lfX, lfX, 0.0f); }

    const f32 KF_DT = 1.0f / 60.0f;   // 0.016666668

    alignas(16) unsigned char gManagerStorage[sizeof(CrashPlayManager)];
    alignas(16) unsigned char gCarStorage[sizeof(ActiveRaceCar)];

    CrashPlayManager& Fresh()
    {
        std::memset(gManagerStorage, 0, sizeof(gManagerStorage));
        std::memset(gCarStorage, 0, sizeof(gCarStorage));
        giMagnitudeCalls = 0;
        giPromptCalls = 0;
        return *reinterpret_cast<CrashPlayManager*>(gManagerStorage);
    }
    ActiveRaceCar* Car(f32 lfTimeInAir)
    {
        ActiveRaceCar* lpCar = reinterpret_cast<ActiveRaceCar*>(gCarStorage);
        lpCar->GetPhysicsState()->mfTimeInAir = lfTimeInAir;
        return lpCar;
    }
}

int main()
{
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();

    std::printf("GetShowtimeTrafficDensityScale (fmadds @0x822A8110):\n");
    {
        CrashPlayManager& m = Fresh();
        const f32 lfSpan = 1.0f - 0.4f;   // fsubs MAX(0x82CDB558), MIN(0x82CDB554)
        const f32 lafDifficulty[3] = { 1.0f / 100.0f, 3.0f / 100.0f, 10.0f / 100.0f };   // 1, 3, 10 cars hit
        const char* lapcLabel[3] = { "difficulty 0.01: fmadds(0.6, 0.99, 0.4) = 0.994 (two roundings: 0.99400008)",
                                     "difficulty 0.03: one rounding, bit for bit (two differ)",
                                     "difficulty 0.10: one rounding, bit for bit (two differ)" };
        for (int li = 0; li < 3; ++li)
        {
            m.mfDifficultyLevel = lafDifficulty[li];
            Check(Same(m.GetShowtimeTrafficDensityScale(), std::fmaf(lfSpan, 1.0f - lafDifficulty[li], 0.4f)),
                  lapcLabel[li]);
        }
        m.mfDifficultyLevel = 0.0f;
        Check(Same(m.GetShowtimeTrafficDensityScale(), 1.0f), "difficulty 0: full density 1.0");
        m.mfDifficultyLevel = 1.0f;
        Check(Same(m.GetShowtimeTrafficDensityScale(), 0.4f), "difficulty 1: the minimum 0.4");
    }

    std::printf("UpdateMomentum -- the ground cost (fnmsubs @0x823021F8):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mfTimeSinceLastInAir = 2.0f;       // past KF_TIME_ON_GROUND_NO_PENALTY (1.0): charged
        m.mfBoostPercentage    = 0.34f;
        m.mfAftertouchPower    = 0.5f;
        gPlayerPosition = { 5.0f, 0.0f, 5.0f, 0.0f };
        m.UpdateMomentum(KF_DT, Car(0.0f), nullptr);
        const f32 lfBoost = ConsoleClamp100(std::fmaf(-20.0f, KF_DT, 0.34f));
        Check(Same(m.mfBoostPercentage, lfBoost), "boost 0.34: 0.34 - 20 * dt rounded once = 0.00666665 (two: 0.00666666)");
        Check(giPromptCalls == 1 && gbLastPrompt, "the prompt leg runs (> 0.4 s on the ground) with boost > 0");
        Check(Same(m.mfTimeSinceLastInAir, 2.0f + KF_DT) && m.mfTimeSinceLastOnGround == 0.0f,
              "ground arm: in-air timer += dt, on-ground timer = 0 (0x823021D4..0x823021E4)");
        Check(Same(m.mLastPlayerPos.x, 5.0f) && Same(m.mLastPlayerPos.z, 5.0f), "mLastPlayerPos takes this frame's position");
        Check(giMagnitudeCalls == 0, "a zero last position skips the distance award (IsZero)");
    }

    std::printf("UpdateMomentum -- the airtime award (fmadds @0x82302260):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mbEarningAirTimeBoost = true;
        m.mfBoostPercentage     = 1.0371f;
        m.mfAftertouchPower     = 0.5f;
        m.UpdateMomentum(KF_DT, Car(0.5f), nullptr);
        Check(Same(m.mfBoostPercentage, ConsoleClamp100(std::fmaf(40.0f, KF_DT, 1.0371f))),
              "boost 1.0371: + 40 * dt rounded once = 1.7037667 (two: 1.7037666)");
        Check(m.mbEarningAirTimeBoost && Same(m.mfTimeSinceLastOnGround, KF_DT), "still airborne: the award stays armed");
    }

    std::printf("UpdateMomentum -- the aftertouch bleed, ground rate (fnmsubs @0x82302338):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mfTimeSinceLastInAir = 0.2f;       // on the ground, below both thresholds: not zero -> GROUND rate
        m.mfBoostPercentage    = 50.0f;
        m.mfAftertouchPower    = 0.04257f;
        m.UpdateMomentum(KF_DT, Car(0.0f), nullptr);
        Check(Same(m.mfAftertouchPower, ConsoleMax0(std::fmaf(-KF_DT, 0.66666669f, 0.04257f))),
              "aftertouch 0.04257: - dt * 0.6667 rounded once = 0.031458888 (two: 0.031458884)");
        Check(Same(m.mfBoostPercentage, 50.0f) && giPromptCalls == 0, "no cost and no prompt below the thresholds");
    }

    std::printf("UpdateMomentum -- the aftertouch bleed, air rate (fnmsubs @0x82302338):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mfTimeSinceLastInAir = 0.0f;       // IsZero -> the AIR rate flt_8201FA48
        m.mfBoostPercentage    = 50.0f;
        m.mfAftertouchPower    = 0.0012f;
        m.UpdateMomentum(KF_DT, Car(0.5f), nullptr);
        Check(Same(m.mfAftertouchPower, ConsoleMax0(std::fmaf(-KF_DT, 0.071428575f, 0.0012f))),
              "aftertouch 0.0012: - dt * 0.0714 rounded once = 9.523751e-06 (two: 9.523705e-06)");
    }

    std::printf("UpdateMomentum -- the no-boost bleed (fnmsubs @0x82302374):\n");
    {
        CrashPlayManager& m = Fresh();
        m.mfTimeSinceLastInAir = 0.2f;
        m.mfBoostPercentage    = 0.0f;       // fpu::IsZero -> the second bleed runs
        m.mfAftertouchPower    = 0.5f;
        m.UpdateMomentum(KF_DT, Car(0.0f), nullptr);
        const f32 lfFirst = ConsoleMax0(std::fmaf(-KF_DT, 0.66666669f, 0.5f));
        Check(Same(m.mfAftertouchPower, ConsoleMax0(std::fmaf(-KF_DT, 0.5f, lfFirst))),
              "an empty meter bleeds twice: the ground rate, then 0.5 (dt * 0.5 is exact: both spellings agree here)");
    }

    std::printf("UpdateMomentum -- the distance award (fmadds @0x82302174) and its IsZero screen:\n");
    {
        const f32 lfShipped = BrnWorld::KF_BOOST_FOR_DISTANCE_TRAVELLED;
        Check(lfShipped == 0.0f, "KF_BOOST_FOR_DISTANCE_TRAVELLED is 0.0 in the shipped tuning (0x82FAD2FC)");
        BrnWorld::KF_BOOST_FOR_DISTANCE_TRAVELLED = 0.1f;   // tuned, to make the fused form observable

        CrashPlayManager& m = Fresh();
        m.mfTimeSinceLastInAir = 0.2f;
        m.mfBoostPercentage    = 37.5f;
        m.mfAftertouchPower    = 0.5f;
        m.mLastPlayerPos       = { 3.0f, 0.0f, 4.0f, 0.0f };
        gfDistance2D = 1.243f;
        m.UpdateMomentum(KF_DT, Car(0.0f), nullptr);
        Check(giMagnitudeCalls == 1 && Same(m.mfBoostPercentage, std::fmaf(0.1f, 1.243f, 37.5f)),
              "tuned K 0.1, 1.243 m: 37.5 + 0.1 * 1.243 rounded once = 37.624302 (two: 37.624298)");

        CrashPlayManager& m2 = Fresh();
        m2.mfTimeSinceLastInAir = 0.2f;
        m2.mfBoostPercentage    = 37.5f;
        m2.mLastPlayerPos       = { 5.0e-7f, 0.0f, 0.0f, 0.0f };   // above FLT_EPSILON, below the shared 1e-6
        m2.UpdateMomentum(KF_DT, Car(0.0f), nullptr);
        Check(giMagnitudeCalls == 1 && Same(m2.mfBoostPercentage, std::fmaf(0.1f, 1.243f, 37.5f)),
              "a 5e-7 last position is NOT zero at FLT_EPSILON (splat flt_82014460): the award runs");

        CrashPlayManager& m3 = Fresh();
        m3.mfTimeSinceLastInAir = 0.2f;
        m3.mfBoostPercentage    = 37.5f;
        m3.mLastPlayerPos       = { 1.0e-7f, -1.1e-7f, 0.0f, 0.0f };   // every lane at or under FLT_EPSILON
        m3.UpdateMomentum(KF_DT, Car(0.0f), nullptr);
        Check(giMagnitudeCalls == 0 && Same(m3.mfBoostPercentage, 37.5f), "lanes under FLT_EPSILON are zero: no award");

        BrnWorld::KF_BOOST_FOR_DISTANCE_TRAVELLED = lfShipped;

        CrashPlayManager& m4 = Fresh();
        m4.mfTimeSinceLastInAir = 0.2f;
        m4.mfBoostPercentage    = 50.0f;
        m4.mLastPlayerPos       = { lfNan, 0.0f, 0.0f, 0.0f };      // vcmpgtfp is false on NaN: the lane is zero
        m4.UpdateMomentum(KF_DT, Car(0.0f), nullptr);
        Check(giMagnitudeCalls == 0 && Same(m4.mfBoostPercentage, 50.0f),
              "a NaN last position counts as zero: no award, the meter keeps 50 (the shared IsZero ran 0 * NaN -> 0)");
    }

    Check(gAsserts == 0, "no assertion on any of these");
    std::printf("FxAiBuzzCrashPlayFma: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
