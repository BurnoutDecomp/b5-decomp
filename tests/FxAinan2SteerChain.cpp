// FX-AINAN2 regression (crash parity 2026-09-24): NaN branch polarity along the AI STEERING chain,
// PID -> clamp -> actuator. The production bodies are extracted verbatim by
// run_fxainan2_steer_chain.py:
//   BrnAI::StepTo                          (BrnAIUtils.cpp)            ARTIST @0x82766BA8
//   PIDController::GetError/GetErrorDerivative/GetOutput/Record/Prepare (BrnPIDController.cpp)
//                                                                      @0x827683A0/0x82768440/...
//   AIDriver::CalculateSteeringAngle + UpdateSteeringAngle and the TU helpers (BrnAIDriver.cpp)
//                                                                      @0x8277CD18 / @0x827708F0
// FindUnsigned/FindSignedAngleBetween2DVectors are the real BrnAIUtils_Angles.cpp.
//
// Console polarity (PowerPC: after fcmpu, ble/bge are TAKEN on unordered, blt/bgt are not;
// fsel d,a,b,c = a >= 0 ? b : c picks c on NaN):
//   StepTo      0x82766BDC bge skips the "Negative Step" assert  -> a NaN step does not fire
//               0x82766C04 ble -> C10 (no snap)                    -> a NaN |delta| never snaps
//               0x82766C14 bge -> C20 current + step              -> a NaN target steps UP
//   GetErrorDerivative 0x827684F8 ble -> 999999.0 (flt_820C5864)  -> a NaN dt is the sentinel
//   CalculateSteeringAngle 0x8277D0B8..0x8277D0C4 fpu::Clamp<float>(out, -1, +1):
//               fsel(-1 - out, -1, out) then fsel(1 - t, t, 1)    -> a NaN PID output is +1.0
// A NaN recorded into the PID keeps its integral NaN for good (the I coefficient is 0.0, and
// 0 * NaN is NaN), so on the console a poisoned controller steers +1.0 every frame; the old PC
// clamp passed NaN to UpdateSteeringAngle, whose old StepTo stepped it DOWN (-1.0 for a rival).
//
// The same group carries the two THROTTLE-side sites of BrnAIDriver.cpp:
//   AttemptToDriveAtDesiredSpeed @0x827706D8  0x8277070C ble -> the CheckForBoosting arm, so a NaN
//               boost timer asks CheckForBoosting (the old `timer <= 0` counted a NaN down)
//   Saturate (fpu::Clamp<float>(x, 0, 1), the fsel ladder, e.g. ProximitySpeed 0x82770858/64)
//               -> a NaN ramp is 1.0 (the old if/if returned NaN)
//
// Fixtures: AICar::GetDirection/GetUsefulDirection/GetSpeed/IsPlayerCar/GetNextRouteNodeIndex/
// GetOpponentIndex return their members; AIDriver::GetTargetPosition returns a configured point;
// AIDriver::CheckForBoosting returns a configured answer and counts its calls.
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "GameSource/World/AI/PID/BrnPIDController.h"
#include "GameSource/World/AI/Route/BrnRoute.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
// UpdateSteeringAngle's [steer] witness streams into gpDebugPrint; a null sink keeps it silent
// and these operators only have to link.
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
StrStreamBase& StrStreamBase::operator<<(s32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(u32) { return *this; }
StrStreamBase& StrStreamBase::operator<<(f32) { return *this; }
}

static Vector2 gTarget{};
static bool gbBoostAnswer = false;
static unsigned guBoostCalls = 0;
namespace BrnAI {
s8      AICar::GetOpponentIndex() const    { return miOpponentIndex; }
bool    AIDriver::CheckForBoosting()       { ++guBoostCalls; return gbBoostAnswer; }
Vector3 AICar::GetDirection() const        { return mDirection; }
Vector3 AICar::GetUsefulDirection() const  { return mDirection; }
f32     AICar::GetSpeed() const            { return mfSpeedInRange; }
bool    AICar::IsPlayerCar() const         { return mbIsPlayer != 0; }
s32     AICar::GetNextRouteNodeIndex() const { return miNextRouteNodeIndex; }
Vector2 AIDriver::GetTargetPosition()      { return gTarget; }
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
    bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-5f; }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    AICar    gCar{};
    AIDriver gDriver;

    // A rival (not the player, rate KF_AI_STEERING_STEP == 1.0) at world (100, 0, 200) facing
    // world +Z, i.e. 2D heading (0, 1); the steering target is 10 m straight ahead.
    void ResetDriver()
    {
        gCar = AICar{};
        gCar.mPosition = Vector3{ 100.0f, 0.0f, 200.0f, 0.0f };
        gCar.mDirection = Vector3{ 0.0f, 0.0f, 1.0f, 0.0f };
        gCar.mfSpeedInRange = 10.0f;
        gCar.mbIsPlayer = 0;
        gCar.mbIsDrifting = false;
        gDriver.mpCarHost = &gCar;
        gDriver.mbIsRacingLineInitialised = 1;
        gDriver.m2DCarPos.x = 100.0f; gDriver.m2DCarPos.y = 200.0f;
        gDriver.m2DCarPos.z = 0.0f;   gDriver.m2DCarPos.w = 0.0f;
        gDriver.mfSteeringAngle = 0.0f;
        const f32 lafNormal[3] = { 1.5f, 0.0f, 0.5f };   // ResetPIDTuningState's {P, I, D}
        gDriver.mPIDController.Prepare(lafNormal);
        gDriver.mPIDControllerDrift.Prepare(lafNormal);
        gTarget.x = 100.0f; gTarget.y = 210.0f; gTarget.z = 0.0f; gTarget.w = 0.0f;
    }
}

int main()
{
    // ---- StepTo @0x82766BA8 ------------------------------------------------------------------
    gAssertions = 0;
    Check(Near(StepTo(0.2f, KF_NAN, 1.0f), 1.2f),
          "StepTo: a NaN target steps UP (bge 0x82766C14 is taken on unordered)");
    Check(Near(StepTo(-0.4f, KF_NAN, 0.25f), -0.15f),
          "StepTo: a NaN target steps UP from a negative angle too");
    Check(Near(StepTo(0.5f, 0.7f, 1.0f), 0.7f), "StepTo: ordered overshoot snaps to the target");
    Check(Near(StepTo(0.5f, 0.9f, 0.1f), 0.6f), "StepTo: ordered step up");
    Check(Near(StepTo(0.5f, -0.7f, 0.1f), 0.4f), "StepTo: ordered step down");
    Check(Near(StepTo(0.5f, 0.5f, 0.1f), 0.5f), "StepTo: already there");
    Check(gAssertions == 0, "StepTo: no assert on the ordered and NaN-target calls");
    gAssertions = 0;
    (void)StepTo(0.0f, 1.0f, KF_NAN);
    Check(gAssertions == 0, "StepTo: a NaN step does NOT fire \"Negative Step\" (bge 0x82766BDC)");
    gAssertions = 0;
    (void)StepTo(0.0f, 1.0f, -1.0f);
    Check(gAssertions == 1, "StepTo: an ordered negative step fires \"Negative Step\"");

    // ---- PIDController::GetErrorDerivative @0x82768440 ------------------------------------------
    {
        PIDController lPid;
        const f32 lafCoefficients[3] = { 1.5f, 0.0f, 0.5f };
        lPid.Prepare(lafCoefficients);
        lPid.Record(0.1f, 0.016f);
        lPid.Record(0.3f, KF_NAN);
        Check(lPid.GetErrorDerivative() == 999999.0f,
              "GetErrorDerivative: a NaN time step returns the 999999.0 sentinel (ble 0x827684F8)");
        lPid.Record(0.4f, 0.0005f);
        Check(lPid.GetErrorDerivative() == 999999.0f, "GetErrorDerivative: dt <= 0.001 -> sentinel");
        lPid.Record(0.6f, 0.1f);
        Check(Near(lPid.GetErrorDerivative(), (0.6f - 0.4f) / 0.1f), "GetErrorDerivative: ordered dt divides");
    }

    // ---- CalculateSteeringAngle @0x8277CD18 -> UpdateSteeringAngle @0x827708F0 ----------------
    // Ordered: a target 90 degrees off the heading saturates the clamp at +/-1 on both builds.
    ResetDriver();
    gTarget.x = 110.0f; gTarget.y = 200.0f;   // straight to the side
    gDriver.CalculateSteeringAngle(0.016f);
    Check(std::fabs(gDriver.mfPIDOutput) == 1.0f, "ordered: a 90-degree error saturates the PID output at +/-1");
    Check(std::fabs(gDriver.mfSteeringAngle) == 1.0f, "ordered: the rival's steering snaps to the clamped output");

    // Unordered: a NaN already in the PID integral (it never leaves: 0.0 * NaN is NaN).
    ResetDriver();
    gDriver.mPIDController.mfCurrentIntegral = KF_NAN;
    gAssertions = 0;
    gDriver.CalculateSteeringAngle(0.016f);
    Check(gDriver.mfPIDOutput == 1.0f,
          "NaN PID output: fpu::Clamp<float> fsel ladder 0x8277D0BC/0x8277D0C4 stores +1.0 to mfPIDOutput");
    Check(gDriver.mfSteeringAngle == 1.0f,
          "NaN PID output: the rival steers +1.0 (StepTo(0, +1, 1.0)), not -1.0");
    gDriver.CalculateSteeringAngle(0.016f);
    Check(gDriver.mfPIDOutput == 1.0f && gDriver.mfSteeringAngle == 1.0f,
          "NaN PID output: every later frame holds +1.0");

    // ---- AttemptToDriveAtDesiredSpeed @0x827706D8 -------------------------------------------------
    ResetDriver();
    gDriver.mfDesiredSpeed = 10.0f;               // == the car's speed: no accelerator / brake ramp
    gDriver.mfBoostTimeRemaining = KF_NAN;
    gbBoostAnswer = true; guBoostCalls = 0;
    gDriver.AttemptToDriveAtDesiredSpeed(0.016f);
    Check(guBoostCalls == 1 && gDriver.mfBoostTimeRemaining == 3.0f && gDriver.mbBoosting == 1,
          "NaN boost timer: ble 0x8277070C takes the CheckForBoosting arm, which latches the 3.0 s window");
    gDriver.mfBoostTimeRemaining = KF_NAN;
    gbBoostAnswer = false; guBoostCalls = 0;
    gDriver.AttemptToDriveAtDesiredSpeed(0.016f);
    Check(guBoostCalls == 1 && gDriver.mbBoosting == 0,
          "NaN boost timer: CheckForBoosting says no -> not boosting");
    gDriver.mfBoostTimeRemaining = 1.0f;
    gbBoostAnswer = true; guBoostCalls = 0;
    gDriver.AttemptToDriveAtDesiredSpeed(0.25f);
    Check(guBoostCalls == 0 && Near(gDriver.mfBoostTimeRemaining, 0.75f) && gDriver.mbBoosting == 1,
          "ordered boost timer > 0 counts down without asking");
    gDriver.mfBoostTimeRemaining = 0.0f;
    gbBoostAnswer = true; guBoostCalls = 0;
    gDriver.AttemptToDriveAtDesiredSpeed(0.25f);
    Check(guBoostCalls == 1 && gDriver.mfBoostTimeRemaining == 3.0f, "ordered boost timer == 0 asks CheckForBoosting");
    gDriver.mfBoostTimeRemaining = 0.0f;
    gbBoostAnswer = false;
    gDriver.mfDesiredSpeed = 10.0f + 0.5f * KF_ACCELERATION_MAX_DIFF;
    gDriver.AttemptToDriveAtDesiredSpeed(0.016f);
    Check(Near(gDriver.mfAccelerator, 0.5f) && gDriver.mfBrake == 0.0f, "ordered: half the 20 mph band -> half throttle");

    // ---- Saturate through ProximitySpeed @0x82770800 --------------------------------------------
    ResetDriver();
    gDriver.GetRacingLine().mfImmmediateApproachSpeedOfTrafficAhead = 0.0f;
    gDriver.GetRacingLine().mfImmediateDistanceToTrafficImpact = KF_NAN;
    Check(Near(gDriver.ProximitySpeed(30.0f), 30.0f),
          "Saturate(NaN) == 1.0 (fsel 0x82770858/0x82770864): a NaN proximity lerps all the way to lfMinSpeed");
    gDriver.GetRacingLine().mfImmediateDistanceToTrafficImpact = 4.0f;   // t = 0
    Check(Near(gDriver.ProximitySpeed(30.0f), 15.0f),
          "ordered t == 0: v = max(speed - 0 - 10 mph, 0.5 * min) == 15");
    gDriver.GetRacingLine().mfImmediateDistanceToTrafficImpact = 100.0f; // t saturates at 1
    Check(Near(gDriver.ProximitySpeed(30.0f), 30.0f), "ordered t > 1 saturates at 1.0");

    std::printf("FX-AINAN2 steering chain: %u/%u checks passed\n", guChecks - guFailures, guChecks);
    return guFailures == 0 ? 0 : 1;
}
