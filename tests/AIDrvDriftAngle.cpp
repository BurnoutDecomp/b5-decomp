// FX-AIDRV regression G03-D3 (crash parity 2026-09-22): the production
// AIDriver::DetermineDriftSteeringAngle (and the TU's To2DU / Normalize2DU), extracted verbatim from
// src/GameSource/World/AI/BrnAIDriver_Update.cpp by run_aidrv_drift_angle.py, checked against the
// ARTIST assembly @0x827931D0. FindSignedAngleBetween2DVectors is the real BrnAIUtils_Angles.cpp.
//
// Console:
//   0x827931E8  !mbIsInitialised (lbz 0x1AF0) -> loc_827931F4: lfs f1, flt_82001CC0 (0.0)
//   CAR_MOVING (r4 == 0): lFrom = (vel.x, vel.z) ; 0x82793270..0x827932E0 per-lane
//     vandc(sign) + vcmpgtfp > flt_820C3B70 (FLT_EPSILON); BOTH lanes fail -> 0x827932E4
//     `b 0x827932F8` -> lFrom = (dir.x, dir.z) (GetDirection)
//   CAR_FACING (r4 == 1): lFrom = (dir.x, dir.z)
//   0x82793310..0x827933AC the same test on the chosen vector: degenerate -> 0.0, and
//     FindFinalDriftDirection (0x827933F8, writes this+0x1C70) is NOT called
//   FindFinalDriftDirection false -> 0.0 ; else FindSignedAngleBetween2DVectors(unit lFrom, target)
// Fixtures: AICar::GetVelocity / GetDirection return their members; FindFinalDriftDirection
// writes a configured direction into its out slot and returns a configured answer.
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include <cmath>
#include <cstdio>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

static unsigned guFindCalls = 0;
static bool     gbFindAnswer = true;
static Vector2 gFindDirection{};
namespace BrnAI {
Vector3 AICar::GetVelocity() const  { return mVelocity; }
Vector3 AICar::GetDirection() const { return mDirection; }
bool AIDriver::FindFinalDriftDirection(Vector2& lrOutDirection)
{
    ++guFindCalls;
    lrOutDirection = gFindDirection;
    return gbFindAnswer;
}
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
    Vector3 V3(f32 lfX, f32 lfY, f32 lfZ) { return Vector3{ lfX, lfY, lfZ, 0.0f }; }
    Vector2 V2(f32 lfX, f32 lfY) { Vector2 lV; lV.x = lfX; lV.y = lfY; lV.z = 0.0f; lV.w = 0.0f; return lV; }

    const f32 KF_HALF_PI = 1.57079637f;
    AICar    gCar{};
    AIDriver gDriver;

    // One call with a fresh fixture; returns the angle, leaves the call count / out slot readable.
    f32 Angle(EDriftDirectionSelection leSelection, Vector3 lVelocity, Vector3 lFacing, Vector2 lTarget,
              bool lbFound = true, bool lbInitialised = true)
    {
        gCar.mVelocity = lVelocity;
        gCar.mDirection = lFacing;
        gDriver.mpCarHost = &gCar;
        gDriver.GetRacingLine().mbIsInitialised = lbInitialised;
        gDriver.mSteeringTargetVector = V2(-9.0f, -9.0f);   // sentinel: untouched unless the finder runs
        gFindDirection = lTarget;
        gbFindAnswer = lbFound;
        guFindCalls = 0;
        return gDriver.DetermineDriftSteeringAngle(leSelection);
    }
}

int main()
{
    const EDriftDirectionSelection keMoving = E_DRIFT_DIRECTION_SELECTION_CAR_MOVING;
    const EDriftDirectionSelection keFacing = E_DRIFT_DIRECTION_SELECTION_CAR_FACING;

    // ---- CAR_MOVING with no planar speed falls back to the FACING (b 0x827932F8) ---------------
    Check(Near(Angle(keMoving, V3(0, 0, 0), V3(1, 0, 0), V2(0, 1)), KF_HALF_PI),
          "stopped, facing +x, drift target +z(2D y): +pi/2 from the facing (old: 0 from a (0,0) vector)");
    Check(Near(Angle(keMoving, V3(0, -20, 0), V3(0, 0, 1), V2(1, 0)), -KF_HALF_PI),
          "falling straight down (planar speed 0), facing +z: -pi/2 from the facing");
    Check(Near(Angle(keMoving, V3(1.1920929e-7f, 3, -1.1920929e-7f), V3(0, 0, 1), V2(1, 0)), -KF_HALF_PI),
          "planar lanes exactly FLT_EPSILON (not > flt_820C3B70): facing fallback");
    Check(guFindCalls == 1, "the fallback path still asks FindFinalDriftDirection once");

    // ---- the common degenerate test returns 0.0 BEFORE FindFinalDriftDirection ---------------
    Check(Angle(keMoving, V3(0, 0, 0), V3(0, 1, 0), V2(1, 0)) == 0.0f, "stopped + vertical facing: 0.0");
    Check(guFindCalls == 0, "stopped + vertical facing: FindFinalDriftDirection NOT called (bne loc_827931F4 @0x827933AC)");
    Check(gDriver.mSteeringTargetVector.x == -9.0f && gDriver.mSteeringTargetVector.y == -9.0f,
          "stopped + vertical facing: mSteeringTargetVector (this+0x1C70) untouched");
    Check(Angle(keFacing, V3(30, 0, 0), V3(0, -1, 0), V2(1, 0)) == 0.0f && guFindCalls == 0,
          "CAR_FACING with a vertical facing: 0.0 and no FindFinalDriftDirection");

    // ---- ordinary paths (unchanged) ------------------------------------------------------------
    Check(Near(Angle(keMoving, V3(10, 4, 10), V3(0, 0, 1), V2(1, 0)), -0.785398185f),
          "moving (10,*,10) -> unit (0.707,0.707), target +x: -pi/4 (velocity, not facing)");
    Check(Near(Angle(keMoving, V3(2.5e-7f, 0, 0), V3(0, 0, 1), V2(0, 1)), KF_HALF_PI),
          "planar x lane 2.5e-7 > FLT_EPSILON: the velocity is kept (+x), +pi/2 to +z");
    Check(Near(Angle(keFacing, V3(10, 0, 0), V3(0, 0, 1), V2(1, 0)), -KF_HALF_PI),
          "CAR_FACING uses the facing even when moving");
    Check(Angle(keMoving, V3(10, 0, 0), V3(0, 0, 1), V2(0, 1), false) == 0.0f && guFindCalls == 1,
          "FindFinalDriftDirection false -> 0.0");
    Check(Angle(keMoving, V3(10, 0, 0), V3(0, 0, 1), V2(0, 1), true, false) == 0.0f && guFindCalls == 0,
          "racing line not initialised -> 0.0 (explicit lfs f1, flt_82001CC0), no finder call");

    Check(gAssertions == 0, "no assertion fired");
    std::printf("AIDrvDriftAngle: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
