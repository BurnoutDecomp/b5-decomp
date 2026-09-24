// FX-AINAN2 regression (crash parity 2026-09-24): NaN branch polarity in the AIDriver partfile
// src/GameSource/World/AI/BrnAIDriver_Update.cpp. run_fxainan2_driver_update.py extracts the
// production bodies verbatim and feeds them NaNs:
//   SaturateU (fpu::Clamp<float>(x,0,1) fsel ladder) through
//     HardShoulderSpeed @0x827930B8  fsel 0x82793110/0x8279311C  NaN road percentage -> ramp 1.0
//     DoSlowTurn        @0x8277CA88  fsel 0x8277CB68/74, 0x8277CBA8/B4  NaN speed -> ramp 1.0
//   FindPositionInFuture @0x82792F80 fsel 0x82793004 (max - d, d, max): NaN distance -> max
//   AttemptToDriveAtDesiredSpeedInDrift @0x8277C8E0 bge 0x8277C910: NaN ratio -> full throttle
//   UpdatePlayerTimers @0x82770320 bge 0x82770380: NaN player speed -> decay, protection not asked
//   ChooseAggressiveSteeringFan @0x82766150 blt 0x82766224: NaN slow-speed time is considered
//   UpdateStuck @0x82766440 blt 0x82766498 (NaN stuck time -> SLOW_TURN), bge 0x827664CC
//     (NaN speed -> timer reset)
// PowerPC: after fcmpu, ble/bge are TAKEN on unordered and blt/bgt are not; fsel d,a,b,c is
// a >= 0 ? b : c and picks c on NaN.
// Fixtures return members / configured values and count calls.
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

static f32      gfRoadPercentage   = 0.0f;
static f32      gfPointFarDistance = -1.0f;
static unsigned guPointFarCalls    = 0;
static f32      gfSpeedRatio       = 1.0f;
static unsigned guProtectedCalls   = 0;
static bool     gbProtected        = false;
static f32      gfSteerTarget      = -99.0f;
static bool     gbRouteDirection   = true;
static Vector2  gRouteDirection{};
namespace BrnAI {
f32     AICar::GetSpeed() const          { return mfSpeedInRange; }
f32     AICar::GetDecentSpeed() const    { return mfSpeedOutOfRange; }
Vector3 AICar::GetRight() const          { return mRight; }
bool    AIDriver::IsPlayerProtected(AICar*) { ++guProtectedCalls; return gbProtected; }
bool    AIDriver::ComputeRouteDirection(Vector2& lrOut) { lrOut = gRouteDirection; return gbRouteDirection; }
void    AIDriver::UpdateSteeringAngle(f32 lfTargetAngle) { gfSteerTarget = lfTargetAngle; }
f32     SteeringFan::GetSpeedRatio()     { return gfSpeedRatio; }
f32     RacingLineGenerator::GetRoadPositionAsPercentage(RacingLine*, AICar*) { return gfRoadPercentage; }
bool    RacingLineGenerator::GetPointFarAhead(RacingLine*, f32 lfDistance, Vector2, Vector2&, Vector2&)
{
    ++guPointFarCalls;
    gfPointFarDistance = lfDistance;
    return true;
}
void WitnessBehaviourTransition(const AIDriver*, const AICar*, s32, s32, const char*) {}
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
    bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-4f; }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    AICar    gCar{};
    AICar    gPlayer{};
    AIDriver gDriver;

    void Reset()
    {
        gCar = AICar{};
        gPlayer = AICar{};
        gCar.mfSpeedInRange    = 20.0f;
        gCar.mfSpeedOutOfRange = 25.0f;     // GetDecentSpeed
        gCar.mRight = Vector3{ 1.0f, 0.0f, 0.0f, 0.0f };
        gPlayer.mfSpeedInRange = 30.0f;
        gDriver.mpCarHost = &gCar;
        gDriver.GetRacingLine().mbIsInitialised = true;
        gRouteDirection.x = 0.0f; gRouteDirection.y = 1.0f; gRouteDirection.z = 0.0f; gRouteDirection.w = 0.0f;
        gbRouteDirection = true;
        guProtectedCalls = 0; gbProtected = false;
        guPointFarCalls = 0; gfPointFarDistance = -1.0f;
        gfSteerTarget = -99.0f;
    }
}

int main()
{
    // ---- HardShoulderSpeed @0x827930B8 (SaturateU) ----------------------------------------------
    Reset();
    gfRoadPercentage = KF_NAN;
    Check(Near(gDriver.HardShoulderSpeed(40.0f), 30.0f),
          "HardShoulderSpeed: a NaN road percentage saturates the ramp at 1.0 -> 75% of the speed");
    gfRoadPercentage = 0.0f;
    Check(Near(gDriver.HardShoulderSpeed(40.0f), 40.0f), "HardShoulderSpeed: centre of the road -> no cut");
    gfRoadPercentage = 2.0f;
    Check(Near(gDriver.HardShoulderSpeed(40.0f), 30.0f), "HardShoulderSpeed: well onto the shoulder -> 75%");

    // ---- DoSlowTurn @0x8277CA88 (SaturateU) -------------------------------------------------------
    Reset();
    gCar.mfBehaviourTimer = 0.0f;                  // phase 0: the throttle phase
    gCar.mfSpeedInRange = KF_NAN;
    gDriver.mfAccelerator = -1.0f;
    gDriver.DoSlowTurn(0.016f);
    Check(gDriver.mfAccelerator == 0.0f, "DoSlowTurn: NaN speed -> ramp 1.0 -> accelerator 0.0 (not NaN)");
    gCar.mfBehaviourTimer = 3.5f;                  // phase 1: the brake phase
    gDriver.mfBrake = -1.0f;
    gDriver.DoSlowTurn(0.016f);
    Check(gDriver.mfBrake == 0.0f, "DoSlowTurn: NaN speed -> brake ramp 1.0 -> brake 0.0 (not NaN)");
    gCar.mfSpeedInRange = 0.5f * KF_SLOW_TURN_SPEED;
    gCar.mfBehaviourTimer = 0.0f;
    gDriver.DoSlowTurn(0.016f);
    Check(Near(gDriver.mfAccelerator, 0.5f), "DoSlowTurn: ordered half of 20 mph -> accelerator 0.5");

    // ---- FindPositionInFuture @0x82792F80 ---------------------------------------------------------
    Reset();
    Vector2 lPos{}, lDir{};
    gCar.mfSpeedInRange = KF_NAN;
    gDriver.FindPositionInFuture(lPos, lDir, 1.0f, 50.0f, 10.0f);
    Check(guPointFarCalls == 1 && gfPointFarDistance == 50.0f,
          "FindPositionInFuture: a NaN distance reaches GetPointFarAhead as lfMaxDistance (fsel 0x82793004)");
    gCar.mfSpeedInRange = 20.0f;
    gDriver.FindPositionInFuture(lPos, lDir, 1.0f, 50.0f, 10.0f);
    Check(gfPointFarDistance == 20.0f, "FindPositionInFuture: ordered distance inside the band");
    gDriver.FindPositionInFuture(lPos, lDir, 0.1f, 50.0f, 10.0f);
    Check(gfPointFarDistance == 10.0f, "FindPositionInFuture: ordered distance floored at the minimum");
    gDriver.FindPositionInFuture(lPos, lDir, 10.0f, 50.0f, 10.0f);
    Check(gfPointFarDistance == 50.0f, "FindPositionInFuture: ordered distance capped at the maximum");

    // ---- AttemptToDriveAtDesiredSpeedInDrift @0x8277C8E0 ------------------------------------------
    Reset();
    gfSpeedRatio = KF_NAN;
    gDriver.AttemptToDriveAtDesiredSpeedInDrift();
    Check(gDriver.mfAccelerator == 1.0f && gDriver.mfBrake == 0.0f,
          "InDrift: a NaN speed ratio takes the bge 0x8277C910 full-throttle arm");
    gfSpeedRatio = 0.5f;
    gDriver.AttemptToDriveAtDesiredSpeedInDrift();
    Check(gDriver.mfAccelerator == 0.0f && gDriver.mfBrake == 1.0f, "InDrift: ordered ratio < 0.75 -> full brake");
    gfSpeedRatio = 0.75f;
    gDriver.AttemptToDriveAtDesiredSpeedInDrift();
    Check(gDriver.mfAccelerator == 1.0f && gDriver.mfBrake == 0.0f, "InDrift: ordered ratio == 0.75 -> full throttle");

    // ---- UpdatePlayerTimers @0x82770320 ------------------------------------------------------------
    Reset();
    gDriver.mfPlayerSlowSpeedTime = 2.0f;
    gPlayer.mfSpeedInRange = KF_NAN;
    gDriver.UpdatePlayerTimers(0.5f, &gPlayer);
    Check(guProtectedCalls == 0 && Near(gDriver.mfPlayerSlowSpeedTime, 1.5f),
          "UpdatePlayerTimers: a NaN player speed decays the slow-speed time without asking IsPlayerProtected");
    gPlayer.mfSpeedInRange = 5.0f;                 // slower than the decent 25
    guProtectedCalls = 0;
    gDriver.mfPlayerSlowSpeedTime = 1.5f;
    gDriver.UpdatePlayerTimers(0.5f, &gPlayer);
    Check(guProtectedCalls == 1 && Near(gDriver.mfPlayerSlowSpeedTime, 2.0f),
          "UpdatePlayerTimers: ordered slow player, unprotected -> the time climbs");

    // ---- ChooseAggressiveSteeringFan @0x82766150 ---------------------------------------------------
    Reset();
    gCar.meRelativeLocation = static_cast<ELocationRelativeToPlayer>(0);   // behind, approaching
    gCar.mfBuzzDistanceToPlayer = 50.0f;
    gCar.mfSpeedInRange = 60.0f;                   // well over the player's 30 + 10 mph
    gDriver.mfPlayerSlowSpeedTime = KF_NAN;
    Check(gDriver.ChooseAggressiveSteeringFan(&gPlayer) == 2,
          "ChooseAggressiveSteeringFan: a NaN slow-speed time is considered (blt 0x82766224) -> spurt fan 2");
    gDriver.mfPlayerSlowSpeedTime = 1.0f;
    Check(gDriver.ChooseAggressiveSteeringFan(&gPlayer) == 0,
          "ChooseAggressiveSteeringFan: ordered short slow time -> no spurt");
    gDriver.mfPlayerSlowSpeedTime = 6.0f;
    Check(gDriver.ChooseAggressiveSteeringFan(&gPlayer) == 2,
          "ChooseAggressiveSteeringFan: ordered 6 s -> spurt fan 2");

    // ---- UpdateStuck @0x82766440 -------------------------------------------------------------------
    Reset();
    gCar.meBehaviour = static_cast<EAIBehaviour>(3);
    gCar.meRouteFindingStyle = static_cast<ERouteFindingStyle>(1);
    gDriver.mfStuckTime = KF_NAN;
    gCar.mfSpeedInRange = 1.0f;
    gDriver.UpdateStuck(0.016f);
    Check(static_cast<s32>(gCar.meBehaviour) == 6,
          "UpdateStuck: a NaN stuck time switches to SLOW_TURN (blt 0x82766498 not taken on unordered)");
    gCar.meBehaviour = static_cast<EAIBehaviour>(3);
    gDriver.mfStuckTime = 1.0f;
    gCar.mfSpeedInRange = KF_NAN;
    gDriver.UpdateStuck(0.016f);
    Check(gDriver.mfStuckTime == 0.0f, "UpdateStuck: a NaN speed resets the stuck timer (bge 0x827664CC)");
    gDriver.mfStuckTime = 1.0f;
    gCar.mfSpeedInRange = 1.0f;
    gDriver.UpdateStuck(0.5f);
    Check(Near(gDriver.mfStuckTime, 1.5f) && static_cast<s32>(gCar.meBehaviour) == 3,
          "UpdateStuck: ordered crawl accumulates");

    std::printf("FX-AINAN2 driver update: %u/%u checks passed\n", guChecks - guFailures, guChecks);
    return guFailures == 0 ? 0 : 1;
}
