// FX-LADDER (crash parity 2026-09-25): CrashPlayManager::UpdateMomentum @0x823020D0, the deferred bounce
// penalty's grace-period gate, run on the PRODUCTION bodies (UpdateMomentum, ClampBoostLevel, the file's
// tunables, its [crashplay] witness and its IsZeroVmx helper -- extracted from BrnCrashPlayManager.cpp by
// run_fxladder_grace_nan.py) on the REAL CrashPlayManager / ActiveRaceCar structs.
//
// ARTIST, read off the asm:
//   0x82302288  lbz +0x151 (mbAboutToLoseBoost) ; beq 0x823022D0     -- not about to lose: skip
//   0x82302294  lfs +0x144 ; fsubs f0, f0, f30(dt) ; stfs +0x144      -- mfLoseBoostGracePeriod -= dt
//   0x823022A0  fcmpu cr6, f0, f31   (f31 = flt_82001CC0 = 0.0, loaded at 0x82302188)
//   0x823022A4  bgt cr6, 0x823022D0  -- skip ONLY for an ordered grace > 0; an unordered (NaN) grace falls through
//   0x823022A8  lwz +0x148 (miConsecutiveBouncesOnGround) ; lfs +0x134 (boost) ; stb r25 (0) -> +0x151 ;
//               == 1 ? flt_82FAD300 : flt_82FAD304 ; fsubs ; stfs +0x134  -- the lose-boost arm
// then ClampBoostLevel (the Clamp(0, 100) fsel pair). The two costs are 0.0 in the shipped image (BSS, no
// initialiser: see the TU banner), so the arm is exercised with tuned costs written into the extracted
// (non-const) tunables, then restored; the shipped-tuning case pins the flag alone.
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
Vector3 BrnWorld::ActiveRaceCar::GetPosition() const { return Vector3{ 0.0f, 0.0f, 0.0f, 0.0f }; }
namespace BrnMath
{
f32 Magnitude2D(Vector3) { return 0.0f; }   // unreached: mLastPlayerPos stays zero, so the award is skipped
}

namespace BrnWorld
{
void CrashPlayManager::SetBouncePromptNeeded(bool, RaceCarEntityModuleIO::OutputBuffer_PrePhysics*) {}

// The production tunables, witness, helper and bodies, extracted verbatim.
#include "fxladder_grace_nan.inc"
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

    const f32 KF_DT = 1.0f / 60.0f;   // 0.016666668

    alignas(16) unsigned char gManagerStorage[sizeof(CrashPlayManager)];
    alignas(16) unsigned char gCarStorage[sizeof(ActiveRaceCar)];

    // A car on the ground with no ground cost due (mfTimeSinceLastInAir 0 -> dt, under KF_TIME_ON_GROUND_NO_PENALTY),
    // no airtime award, no distance award (mLastPlayerPos zero): the bounce penalty is the only meter move.
    CrashPlayManager& Armed(f32 lfGrace, s32 liBounces)
    {
        std::memset(gManagerStorage, 0, sizeof(gManagerStorage));
        std::memset(gCarStorage, 0, sizeof(gCarStorage));
        CrashPlayManager& m = *reinterpret_cast<CrashPlayManager*>(gManagerStorage);
        m.mfBoostPercentage            = 50.0f;
        m.mbAboutToLoseBoost           = true;
        m.mfLoseBoostGracePeriod       = lfGrace;
        m.miConsecutiveBouncesOnGround = liBounces;
        return m;
    }
    void Step(CrashPlayManager& m)
    {
        ActiveRaceCar* lpCar = reinterpret_cast<ActiveRaceCar*>(gCarStorage);
        lpCar->GetPhysicsState()->mfTimeInAir = 0.0f;
        m.UpdateMomentum(KF_DT, lpCar, nullptr);
    }
}

int main()
{
    const f32 lfNan = std::numeric_limits<f32>::quiet_NaN();
    const f32 lfShipped1 = BrnWorld::KF_COST_FOR_1_BOUNCE_ON_GROUND;
    const f32 lfShipped2 = BrnWorld::KF_COST_FOR_2_BOUNCES_ON_GROUND;
    BrnWorld::KF_COST_FOR_1_BOUNCE_ON_GROUND  = 5.0f;   // tuned, so the arm moves the meter
    BrnWorld::KF_COST_FOR_2_BOUNCES_ON_GROUND = 7.0f;

    std::printf("the NaN grace period (0x823022A0 fcmpu ; 0x823022A4 bgt not taken -> the arm at 0x823022A8):\n");
    {
        CrashPlayManager& m = Armed(lfNan, 1);
        Step(m);
        Check(!m.mbAboutToLoseBoost, "NaN grace, 1 bounce: the lose-boost arm clears mbAboutToLoseBoost");
        Check(Same(m.mfBoostPercentage, 45.0f), "NaN grace, 1 bounce: the 1-bounce cost comes off the meter (50 -> 45)");
        Check(std::isnan(m.mfLoseBoostGracePeriod), "NaN grace: the decrement ran and kept the NaN (control)");
    }
    {
        CrashPlayManager& m = Armed(lfNan, 2);
        Step(m);
        Check(!m.mbAboutToLoseBoost, "NaN grace, 2 bounces: the lose-boost arm clears mbAboutToLoseBoost");
        Check(Same(m.mfBoostPercentage, 43.0f), "NaN grace, 2 bounces: the 2-bounce cost comes off the meter (50 -> 43)");
    }

    std::printf("ordered controls:\n");
    {
        CrashPlayManager& m = Armed(0.5f, 1);
        Step(m);
        Check(m.mbAboutToLoseBoost, "grace 0.5: still inside the grace period, the penalty stays armed");
        Check(Same(m.mfLoseBoostGracePeriod, 0.5f - KF_DT), "grace 0.5: decremented by dt, one fsubs (0x82302298)");
        Check(Same(m.mfBoostPercentage, 50.0f), "grace 0.5: the meter is untouched");
    }
    {
        CrashPlayManager& m = Armed(0.01f, 1);
        Step(m);
        Check(!m.mbAboutToLoseBoost, "grace 0.01 < dt: the period runs out, the arm fires");
        Check(Same(m.mfBoostPercentage, 45.0f), "grace 0.01 < dt: 50 -> 45");
    }
    {
        CrashPlayManager& m = Armed(KF_DT, 2);
        Step(m);
        Check(!m.mbAboutToLoseBoost, "grace == dt: 0.0 is not > 0.0, the arm fires");
        Check(Same(m.mfBoostPercentage, 43.0f), "grace == dt, 2 bounces: 50 -> 43");
    }

    BrnWorld::KF_COST_FOR_1_BOUNCE_ON_GROUND  = lfShipped1;
    BrnWorld::KF_COST_FOR_2_BOUNCES_ON_GROUND = lfShipped2;
    std::printf("the shipped tuning (both costs 0.0: 0x82FAD300 / 0x82FAD304 in BSS, no initialiser):\n");
    {
        CrashPlayManager& m = Armed(lfNan, 1);
        Step(m);
        Check(!m.mbAboutToLoseBoost, "shipped, NaN grace: the arm still runs and clears the flag");
        Check(Same(m.mfBoostPercentage, 50.0f) && lfShipped1 == 0.0f && lfShipped2 == 0.0f,
              "shipped: the costs are 0.0, so the meter is unchanged");
    }
    Check(gAsserts == 0, "no tripwire (control)");

    std::printf("FxLadderGraceNan: %u checks, %u failures\n", gChecks, gFailures);
    return gFailures ? 1 : 0;
}
