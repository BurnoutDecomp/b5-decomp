// FX-TAILS-A item 6 (crash parity 2026-09-24): the AI driver's planar normalises have the console's arithmetic --
// NO zero guard. run_fxtailsa_driver_normalise.py extracts the production bodies verbatim:
//   BrnAIDriver.cpp         Normalize2D, To2D, ClampFsel, Saturate, AIDriver::CorneringTopSpeed      @0x8277D0F0
//   BrnAIDriver_Update.cpp  Normalize2DU, To2DU, AIDriver::GetQuickTurnSteering                        @0x8277C600
//                           AIDriver::UpdateBrakingAnticipationData             @0x827964C0 (image bytes)
// FindUnsigned/FindSignedAngleBetween2DVectors are the real BrnAIUtils_Angles.cpp.
//
// Every console caller normalises inline -- `vrsqrtefp` + two Newton-Raphson steps + vmulfp -- and none tests the
// length first (no vcmpeqfp/vsel after the chain): CorneringTopSpeed 0x8277D17C (useful direction) and 0x8277D1E8
// (anticipation point - car position), GetQuickTurnSteering 0x8277C67C, UpdateBrakingAnticipationData 0x827965AC,
// CalculateSteeringAngle 0x8277CE8C / 0x8277D004, ComputeRouteDirection 0x8276662C, GetTargetPosition 0x8277CCA0,
// DetermineDriftSteeringAngle 0x827933DC, UpdateQuickTurn 0x8278B168. The chain, as 0x8277D17C..0x8277D1A4 orders it:
//   y0 = vrsqrtefp(lenSq)
//   e  = -(lenSq * (y*y) - 1.0)        vnmsubfp  (fused; 1.0 = vcsxwfp 1,0)
//   y' = (y * 0.5) * e + y             vmaddfp   (fused; 0.5 = vcsxwfp 1,1)       -- twice
//   out = v * y2                       vmulfp128
// For lenSq = +0, vrsqrtefp gives +inf (AltiVec PEM), 0 * inf = NaN makes e and every later value NaN, and the
// output lanes are NaN. The old helpers answered (0, 0) (`lenSq > 0 ? 1 / sqrt : 0`).
//
// One level up (all unreachable with the shipped data -- FX-AINAN2):
//   CorneringTopSpeed, anticipation point ON the car, road straight ahead: the console's second angle is
//     FindUnsignedAngleBetween2DVectors(useful, NaN) = 0.0 (the blt 0x82766B5C is not taken on an unordered |dot|),
//     so the cap stays the input speed; the old (0, 0) gave acos(0) = 90 deg, ramp 1.0 and the 0.75 cap
//     (flt_820C41FC = 0x3F400000).
//   UpdateBrakingAnticipationData's no-future fallback stores the useful direction's (x, y) LANES as the road
//     direction (0x82796578..0x82796580, normalised at 0x827965AC); a car facing exactly +Z has (0, 0) there,
//     NaN on the console, so CorneringTopSpeed's first angle is 0.0 instead of 90 deg.
//   GetQuickTurnSteering, facing straight up (flattened (0, 0)) with the centre line known: the 2D cross is NaN
//     and `fsel f1, cross, 1.0 (flt_82001C98), -1.0 (flt_820037C8)` @0x8277C70C answers -1.0; the old 0.0 cross
//     answered +1.0.
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
}

static Vector3 gUseful{};
static bool    gbFutureFound = false;
namespace BrnAI {
Vector3 AICar::GetDirection() const       { return mDirection; }
Vector3 AICar::GetUsefulDirection() const { return gUseful; }
Vector3 AICar::GetPosition() const        { return mPosition; }
bool AIDriver::FindPositionInFuture(Vector2&, Vector2&, f32, f32, f32) { return gbFutureFound; }
}

#include "restored_methods.inc"

using namespace BrnAI;

namespace
{
    unsigned guChecks = 0, guFailures = 0;
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks;
        std::printf("%s  %s\n", lbPass ? "PASS" : "FAIL", lpcLabel);
        if (!lbPass) { ++guFailures; }
    }
    bool IsNaN(f32 lfValue) { return lfValue != lfValue; }
    bool Same(f32 lfA, f32 lfB) { return std::memcmp(&lfA, &lfB, sizeof(f32)) == 0; }

    // The console chain above, operation for operation. The estimate of a nonzero lenSq is taken as the exact
    // 1/sqrt (the hardware estimate is 12-bit; two steps converge either way) -- it is compared here only through
    // NaN-ness and a 2-ulp window, never bit for bit.
    f32 ConsoleRsqrtNewton(f32 lfLenSq)
    {
        f32 lfY = (lfLenSq == 0.0f) ? std::numeric_limits<f32>::infinity() : 1.0f / std::sqrt(lfLenSq);
        for (int li = 0; li < 2; ++li)
        {
            const f32 lfYY   = lfY * lfY;                       // vmulfp128 v10, v0, v0
            const f32 lfHalf = lfY * 0.5f;                      // vmulfp128 v9, v0, v126
            const f32 lfE    = -std::fmaf(lfLenSq, lfYY, -1.0f); // vnmsubfp128 v8, lenSq, v10, 1.0
            lfY = std::fmaf(lfHalf, lfE, lfY);                  // vmaddfp v0, v9, v0, v8
        }
        return lfY;
    }
    Vector2 V2(f32 lfX, f32 lfY) { Vector2 lV; lV.x = lfX; lV.y = lfY; lV.z = 0.0f; lV.w = 0.0f; return lV; }
    bool WithinUlps(f32 lfA, f32 lfB, int liUlps)
    {
        int liA, liB;
        std::memcpy(&liA, &lfA, 4); std::memcpy(&liB, &lfB, 4);
        return (liA - liB <= liUlps) && (liB - liA <= liUlps);
    }

    AICar    gCar{};
    AIDriver gDriver;

    void Reset(Vector3 lPosition, Vector3 lDirection, Vector3 lUseful)
    {
        gCar = AICar{};
        gCar.mPosition  = lPosition;
        gCar.mDirection = lDirection;
        gUseful = lUseful;
        gbFutureFound = false;
        gDriver.mpCarHost = &gCar;
        gDriver.m2DCarPos = V2(lPosition.x, lPosition.z);   // (x, z): the driver's ground-plane position
        gDriver.mfAngleToBrakingTarget = -1.0f;
        gAssertions = 0;
    }
}

int main()
{
    const Vector2 lZero = V2(0.0f, 0.0f);
    const Vector2 lNegZero = V2(-0.0f, 0.0f);

    // ---- the helpers themselves: a zero vector -----------------------------------------------------------------
    {
        const f32 lfConsole = 0.0f * ConsoleRsqrtNewton(0.0f);
        Check(IsNaN(lfConsole), "model: the console chain turns lenSq = +0 into NaN (vrsqrtefp(+0) = +inf, 0 * inf)");
        const Vector2 lA = TestNormalize2D(lZero);
        Check(IsNaN(lA.x) && IsNaN(lA.y), "Normalize2D(0, 0) is (NaN, NaN), as the console's unguarded rsqrt chain");
        const Vector2 lB = TestNormalize2DU(lZero);
        Check(IsNaN(lB.x) && IsNaN(lB.y), "Normalize2DU(0, 0) is (NaN, NaN), as the console's unguarded rsqrt chain");
        const Vector2 lC = TestNormalize2D(lNegZero);
        const Vector2 lD = TestNormalize2DU(lNegZero);
        Check(IsNaN(lC.x) && IsNaN(lC.y) && IsNaN(lD.x) && IsNaN(lD.y),
              "(-0, 0) squares to +0 and is NaN too, in both helpers");
    }

    // ---- nonzero vectors are unchanged (the fix touches only lenSq == 0) ------------------------------------------
    {
        const f32 laInputs[][2] = { { 3.0f, 4.0f }, { -0.3f, 0.95f }, { 1.0e-3f, -2.0e-3f }, { 1234.5f, -0.25f },
                                    { 1.2e-7f, 0.0f } };
        bool lbSame = true, lbNear = true;
        for (const auto& lrIn : laInputs)
        {
            const Vector2 lV = V2(lrIn[0], lrIn[1]);
            const f32 lfLenSq = lV.x * lV.x + lV.y * lV.y;
            const f32 lfInv = 1.0f / std::sqrt(lfLenSq);           // the pre-fix nonzero arm
            const Vector2 lA = TestNormalize2D(lV), lB = TestNormalize2DU(lV);
            lbSame = lbSame && Same(lA.x, lV.x * lfInv) && Same(lA.y, lV.y * lfInv)
                            && Same(lB.x, lV.x * lfInv) && Same(lB.y, lV.y * lfInv);
            const f32 lfConsole = ConsoleRsqrtNewton(lfLenSq);
            lbNear = lbNear && WithinUlps(lA.x, lV.x * lfConsole, 2) && WithinUlps(lA.y, lV.y * lfConsole, 2);
        }
        Check(lbSame, "nonzero vectors: both helpers give v * (1 / sqrt(lenSq)) bit for bit, as before the fix");
        Check(lbNear, "nonzero vectors: within 2 ulps of the modelled console chain");
    }

    // ---- CorneringTopSpeed @0x8277D0F0: the anticipation point ON the car (0x8277D1CC..0x8277D1E8) ---------------
    {
        Reset(Vector3{ 100.0f, 5.0f, 200.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 1.0f, 0.0f });
        gDriver.mBrakingRoadDir = V2(0.0f, 1.0f);                  // the road runs along the car's useful direction
        gDriver.mBrakingAnticipationPos = gDriver.m2DCarPos;       // zero (anticipation - car) vector
        const f32 lfCap = gDriver.CorneringTopSpeed(40.0f);
        Check(Same(gDriver.mfAngleToBrakingTarget, 0.0f),
              "CorneringTopSpeed: NaN anticipation direction -> FindUnsignedAngle 0.0, cornering angle 0.0 (was 90 deg)");
        Check(Same(lfCap, 40.0f), "CorneringTopSpeed: the cap stays the 40.0 input (was 30.0 = 40 * 0.75)");
    }

    // ---- UpdateBrakingAnticipationData @0x827964C0 -> CorneringTopSpeed: the (x, y)-lane fallback -------------------
    {
        Reset(Vector3{ 100.0f, 5.0f, 200.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 1.0f, 0.0f });
        gDriver.UpdateBrakingAnticipationData();                   // FindPositionInFuture fails -> fallback
        Check(IsNaN(gDriver.mBrakingRoadDir.x) && IsNaN(gDriver.mBrakingRoadDir.y),
              "UpdateBrakingAnticipationData: useful (0, 0, 1) -> road dir lanes (x, y) = (0, 0) -> NaN (0x827965AC)");
        Check(gDriver.mBrakingAnticipationPos.x == 100.0f && gDriver.mBrakingAnticipationPos.y == 5.0f,
              "UpdateBrakingAnticipationData: fallback point = position + useful over the (x, y) lanes (control)");
        const f32 lfCap = gDriver.CorneringTopSpeed(40.0f);
        Check(Same(gDriver.mfAngleToBrakingTarget, 0.0f) && Same(lfCap, 40.0f),
              "...then CorneringTopSpeed: angle 0.0 and the 40.0 input speed (was 90 deg and 30.0)");
        Check(gAssertions == 0, "no assertion fired (mpCarHost is set)");
    }

    // ---- GetQuickTurnSteering @0x8277C600: facing straight up, centre line known ----------------------------------
    {
        Reset(Vector3{ 100.0f, 5.0f, 200.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f, 0.0f }, Vector3{ 0.0f, 1.0f, 0.0f, 0.0f });
        gDriver.GetRacingLine().mbCentreLineHereKnown = true;
        gDriver.GetRacingLine().mCentreHere = V2(110.0f, 210.0f);
        const f32 lfLock = gDriver.GetQuickTurnSteering(V2(1.0f, 0.0f));
        Check(Same(lfLock, -1.0f), "GetQuickTurnSteering: NaN facing -> NaN cross -> fsel answers -1.0 (was +1.0)");

        // control: an ordinary facing is unchanged -- +Z facing, centre line to the right (+X): cross < 0 -> -1.0
        Reset(Vector3{ 100.0f, 5.0f, 200.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 1.0f, 0.0f }, Vector3{ 0.0f, 0.0f, 1.0f, 0.0f });
        gDriver.GetRacingLine().mbCentreLineHereKnown = true;
        gDriver.GetRacingLine().mCentreHere = V2(110.0f, 200.0f);
        Check(Same(gDriver.GetQuickTurnSteering(V2(1.0f, 0.0f)), -1.0f),
              "GetQuickTurnSteering control: facing +Z, centre line at +X -> cross -10 -> -1.0");
        gDriver.GetRacingLine().mCentreHere = V2(90.0f, 200.0f);
        Check(Same(gDriver.GetQuickTurnSteering(V2(1.0f, 0.0f)), 1.0f),
              "GetQuickTurnSteering control: facing +Z, centre line at -X -> cross +10 -> +1.0");
    }

    std::printf("FxTailsADriverNormalise: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
