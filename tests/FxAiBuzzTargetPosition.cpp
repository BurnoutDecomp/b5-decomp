// FX-AIBUZZ item 3 (crash parity 2026-09-24): AIDriver::GetTargetPosition @0x8277CBF8, the direct-target arm
// (the car's mbIsDrivenByPlayer, +0x154A), run on the PRODUCTION body (extracted from BrnAIDriver.cpp with its
// Normalize2D / To2D helpers by run_fxaibuzz_target_position.py) against the asm:
//   pos.xz  = vrlimi128 pair (lane0 <- .x, lane1 <- .z)                                  0x8277CC50 / 0x8277CC58
//   dir.xz  = the same pair on GetDirection                                                0x8277CC74 / 0x8277CC80
//   lenSq   = vmulfp128 dir.xz^2 ; vspltw ; vaddfp                                         0x8277CC88..0x8277CC94
//   y2      = vrsqrtefp + two Newton steps (no vcmpeqfp / vsel: no zero guard)             0x8277CCA0..0x8277CCC0
//   target  = vmaddcfp128 v0, v13(dir.xz), v0(y2), v127(pos.xz) == pos + dir * y2, ONE rounding   0x8277CCC4
// FX-TAILS-A left this as a follow-up: the PC normalised first (dir * y2 rounded on its own) and then added.
// The rsqrt estimate is modelled as 1 / sqrt(lenSq) on both sides (the campaign's convention), so the only
// difference under test is the fused multiply-add; the inputs are ones where one rounding and two differ.
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIDriver_Constants.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev {
namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
}
namespace Log { DebugPrint* gpDebugPrint = nullptr; }
}

namespace BrnAI {
Vector3 AICar::GetDirection() const { return mDirection; }
Vector3 AICar::GetPosition() const  { return mPosition; }
// Only the steering-fan / fallback arm and the opt-in [ai-road] diag reach these; the car below is driven by the
// player, so none of them runs -- they exist for the link.
bool AIDriver::ComputeRouteDirection(Vector2&) { return false; }
Vector2 SteeringFan::GetDrivingTarget(AICar*, RacingLine*, RacingLineGenerator*, const NearbyVehicles*, bool)
{
    return Vector2{ 0.0f, 0.0f, 0.0f, 0.0f };
}
f32  AICar::GetSpeed() const    { return 0.0f; }
bool AICar::IsPlayerCar() const { return mbIsPlayer; }
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
    u32  Bits(f32 lf) { u32 lu; std::memcpy(&lu, &lf, 4); return lu; }
    f32  FromBits(u32 lu) { f32 lf; std::memcpy(&lf, &lu, 4); return lf; }
    bool Same(f32 a, f32 b) { return Bits(a) == Bits(b) || (a != a && b != b); }

    AICar    gCar{};
    AIDriver gDriver;

    Vector2 Target(f32 lfDirX, f32 lfDirY, f32 lfDirZ, f32 lfPosX, f32 lfPosZ)
    {
        gCar = AICar{};
        gCar.mbIsDrivenByPlayer = true;                       // lbz 0x154A -- the direct-target arm
        gCar.mPosition  = Vector3{ lfPosX, 7.0f, lfPosZ, 0.0f };
        gCar.mDirection = Vector3{ lfDirX, lfDirY, lfDirZ, 0.0f };
        gDriver.mpCarHost = &gCar;
        return gDriver.GetTargetPosition();
    }

    // The asm's lanes: one rounding for pos + dir * y2 (y2 modelled as 1 / sqrt(lenSq)).
    void Expect(const char* lpcCase, u32 luDirX, u32 luDirZ, u32 luPosX, u32 luPosZ)
    {
        const f32 lfDx = FromBits(luDirX), lfDz = FromBits(luDirZ), lfPx = FromBits(luPosX), lfPz = FromBits(luPosZ);
        const Vector2 lTarget = Target(lfDx, 0.0f, lfDz, lfPx, lfPz);
        const f32 lfInv = 1.0f / std::sqrt(lfDx * lfDx + lfDz * lfDz);
        const f32 lfX = std::fmaf(lfDx, lfInv, lfPx), lfY = std::fmaf(lfDz, lfInv, lfPz);
        const f32 lfTwoX = lfPx + lfDx * lfInv, lfTwoY = lfPz + lfDz * lfInv;
        char lacLabel[256];
        std::snprintf(lacLabel, sizeof(lacLabel), "%s: (%.9g, %.9g) one rounding (two roundings: (%.9g, %.9g)); got (%.9g, %.9g)",
                      lpcCase, lfX, lfY, lfTwoX, lfTwoY, lTarget.x, lTarget.y);
        Check(Same(lTarget.x, lfX) && Same(lTarget.y, lfY), lacLabel);
        Check(!(Same(lfX, lfTwoX) && Same(lfY, lfTwoY)), "  (the case is one where the two spellings differ)");
    }
}

int main()
{
    Expect("heading NW at (103.0, 78.2)",     0xBF350727u, 0x3F3502BFu, 0x42CE0812u, 0x429C6929u);
    Expect("heading ENE at (-0.95, -0.27)",   0x3F76B95Eu, 0x3E889357u, 0xBF74134Fu, 0xBE89CCDAu);
    Expect("heading ENE at (78.6, -2739.9)",  0x3F7352BEu, 0x3E9F1EC9u, 0x429D1B74u, 0xC52B3ED4u);

    // Only the ground plane counts: a climbing car's (0.6, 0.8, 0) heading flattens to (0.6, 0) and aims one
    // metre along +X; the result's z / w lanes are 0.
    {
        const Vector2 lTarget = Target(0.6f, 0.8f, 0.0f, 10.0f, 20.0f);
        Check(Same(lTarget.x, 11.0f) && Same(lTarget.y, 20.0f) && lTarget.z == 0.0f && lTarget.w == 0.0f,
              "a climbing heading flattens to the ground plane: (10, 20) -> (11, 20), z/w 0");
    }
    // No zero guard (FX-TAILS-A item 6): a car pointing straight up has a (0, 0) ground heading, y2 is +inf and
    // 0 * inf is NaN in both lanes -- here as fmaf(0, inf, pos).
    {
        const Vector2 lTarget = Target(0.0f, 1.0f, 0.0f, 10.0f, 20.0f);
        Check(lTarget.x != lTarget.x && lTarget.y != lTarget.y, "a vertical heading gives NaN lanes (no zero guard)");
    }

    Check(gAssertions == 0, "no assertion");
    std::printf("FxAiBuzzTargetPosition: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
