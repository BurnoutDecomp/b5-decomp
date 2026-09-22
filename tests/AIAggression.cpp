// FX-AGG regression (crash parity 2026-09-22): the production AIAggression bodies, extracted
// verbatim from src/GameSource/World/AI/BrnAIAggression.cpp by run_ai_aggression.py, checked
// against numbers derived from the ARTIST assembly. The fixtures below are the AICar accessors
// (each returns the plain member it reads) and a pinned aggression RNG draw; nothing else is
// replaced. Every expected value is worked out from the console instructions cited beside it.
#include "GameSource/World/AI/BrnAIAggression.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/vector3_operation.h"
#include <excpt.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

static f32 gfDecentSpeed = 0.0f;
namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
Vector3 AICar::GetDirection() const { return mDirection; }
Vector3 AICar::GetUsefulDirection() const { return mDirection; }
Vector3 AICar::GetRight() const { return mRight; }
f32 AICar::GetSpeed() const { return mfSpeedInRange; }
f32 AICar::GetDecentSpeed() const { return gfDecentSpeed; }
f32 Aggressiveness::GetAggressionLevel() const { return mfAggressionLevel; }
CgsNumeric::Random AIAggression::mRandom;
}
namespace CgsNumeric { f32 Random::RandomFloat() { return 0.5f; } }

#include "aggression_methods.inc"

using namespace BrnAI;

namespace
{
    const char* gpcGroup = "";
    unsigned guChecks = 0, guFailures = 0, guGroupChecks = 0, guGroupFailures = 0;

    void EndGroup()
    {
        if (guGroupChecks != 0)
            std::printf("  %-44s %u checks, %u failures\n", gpcGroup, guGroupChecks, guGroupFailures);
        guGroupChecks = guGroupFailures = 0;
    }
    void BeginGroup(const char* lpcGroup) { EndGroup(); gpcGroup = lpcGroup; }
    void Check(bool lbPass, const char* lpcLabel)
    {
        ++guChecks; ++guGroupChecks;
        if (!lbPass) { ++guFailures; ++guGroupFailures; std::printf("FAIL [%s] %s\n", gpcGroup, lpcLabel); }
    }
    Vector3 V(f32 lfX, f32 lfY, f32 lfZ) { return Vector3{ lfX, lfY, lfZ, 0.0f }; }
    bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-4f * (1.0f + std::fabs(lfB)); }
    bool NearV(const Vector3& lrA, const Vector3& lrB) { return Near(lrA.x, lrB.x) && Near(lrA.y, lrB.y) && Near(lrA.z, lrB.z); }
}

// ------------------------------------------------------------------------------------------------
// G00-D1  GetPositionNextToTarget @0x827714E8. The console passes (r4 = lpCarB, r5 = lpCarA) to
// DetermineAttackSide (`mr r4,r6` @0x8277150C, r5 untouched), which returns the sign of
// dot(pos(p2) - pos(p1), right(p2)) (`vsubfp128 v13,v126,v127` @0x82771480). So the side is
// dot(pos(target) - pos(car), right(target)); > 0 negates the offset, and the point is
// pos(target) + right(target) * offset (vmaddcfp128 @0x82771578).
// ------------------------------------------------------------------------------------------------
static void GroupLineupSide()
{
    BeginGroup("G00-D1 GetPositionNextToTarget side");
    const unsigned luAssertions = gAssertions;
    AIAggression lAggression{};
    AICar lTarget{}, lCar{};
    lTarget.mPosition = V(100.0f, 5.0f, 200.0f);
    lTarget.mRight = V(1.0f, 0.0f, 0.0f);
    lTarget.mDirection = V(0.0f, 0.0f, 1.0f);

    // The car sits 3 m on the target's -right side, same heading: side = +3, offset negated.
    lCar.mPosition = V(97.0f, 5.0f, 200.0f);
    lCar.mRight = V(1.0f, 0.0f, 0.0f);
    lCar.mDirection = V(0.0f, 0.0f, 1.0f);
    Check(NearV(lAggression.GetPositionNextToTarget(&lTarget, &lCar, -8.0f), V(108.0f, 5.0f, 200.0f)),
          "ATTACK_SLAM (-8, 0x820C26C0) aims 8 m past the target, on its far side");
    Check(NearV(lAggression.GetPositionNextToTarget(&lTarget, &lCar, 6.0f), V(94.0f, 5.0f, 200.0f)),
          "VEER (+6, 0x820C4250) lines up 6 m out on the car's own side");
    Check(NearV(lAggression.GetPositionNextToTarget(&lTarget, &lCar, 4.0f), V(96.0f, 5.0f, 200.0f)),
          "OVERTAKE_TO_SLAM (+4, 0x820C41C0) lines up on the car's own side");
    Check(NearV(lAggression.GetPositionNextToTarget(&lTarget, &lCar, 7.5f), V(92.5f, 5.0f, 200.0f)),
          "DROP_BACK_TO_SLAM (+7.5, 0x820C42D4) lines up on the car's own side");

    // The car on the target's +right side: side = -3, offset kept, -8 lands on the target's left.
    lCar.mPosition = V(103.0f, 5.0f, 200.0f);
    Check(NearV(lAggression.GetPositionNextToTarget(&lTarget, &lCar, -8.0f), V(92.0f, 5.0f, 200.0f)),
          "car on the right: the slam point is 8 m on the target's left");

    // Different headings: the side is taken along the TARGET's right, not the car's.
    lCar.mPosition = V(97.0f, 5.0f, 195.0f);
    lCar.mRight = V(0.0f, 0.0f, 1.0f);
    Check(NearV(lAggression.GetPositionNextToTarget(&lTarget, &lCar, -8.0f), V(108.0f, 5.0f, 200.0f)),
          "the side is measured on the target's right vector");

    // Opposed headings (the pre-fix reading happens to agree here).
    lCar.mPosition = V(97.0f, 5.0f, 200.0f);
    lCar.mRight = V(-1.0f, 0.0f, 0.0f);
    Check(NearV(lAggression.GetPositionNextToTarget(&lTarget, &lCar, -8.0f), V(108.0f, 5.0f, 200.0f)),
          "opposed headings still aim through the target");

    // DetermineAttackSide(car, target) returns the console's two constants, not the raw dot.
    Check(lAggression.DetermineAttackSide(&lCar, &lTarget) == 1.0f,
          "DetermineAttackSide: car on the target's -right returns +1.0 (flt_82001C98)");
    lCar.mPosition = V(103.0f, 5.0f, 200.0f);
    Check(lAggression.DetermineAttackSide(&lCar, &lTarget) == -1.0f,
          "DetermineAttackSide: car on the target's +right returns -1.0 (flt_820037C8)");
    Check(gAssertions == luAssertions, "valid positions raise no RwMath::IsValid assertion");
}

// ------------------------------------------------------------------------------------------------
// G00-D2  CalcSeparationAcrossToTarget @0x82771248 and its only caller CanSlam @0x8277DFC8.
// Console: diff = flat(pos(mpCar) - pos(mpTargetCar)); right = GetRight(mpCar) with y = 0
// (stfs @0x82771324) BEFORE the degenerate test and the normalise; no flat lane above
// flt_820C3B70 -> flt_8204F664 (0x7F7FFFFF, FLT_MAX); else fabs(dot(diff, normalise(right)))
// (vandc @0x827713D0). CanSlam: lead > -3 (ble exit), lead < 2 (bge exit), across < 30 (blt).
// ------------------------------------------------------------------------------------------------
static bool IsFltMax(f32 lfValue)
{
    u32 luBits = 0;
    std::memcpy(&luBits, &lfValue, sizeof(luBits));
    return luBits == 0x7F7FFFFFu;
}

static void GroupAcrossSeparation()
{
    BeginGroup("G00-D2 CalcSeparationAcrossToTarget");
    const unsigned luAssertions = gAssertions;
    const f32 lfNaN = std::numeric_limits<f32>::quiet_NaN();
    AIAggression lAggression{};
    AICar lCar{}, lTarget{};
    lAggression.mpCar = &lCar;
    lAggression.mpTargetCar = &lTarget;
    lCar.mPosition = V(0.0f, 0.0f, 0.0f);
    lCar.mDirection = V(0.0f, 0.0f, 1.0f);
    lCar.mRight = V(1.0f, 0.0f, 0.0f);
    lCar.miProximityIndex = 1;

    lTarget.mPosition = V(35.0f, 0.0f, 0.5f);
    Check(Near(lAggression.CalcSeparationAcrossToTarget(), 35.0f), "a target 35 m on the car's +right reads 35 (fabs)");
    lTarget.mPosition = V(-35.0f, 0.0f, 0.5f);
    Check(Near(lAggression.CalcSeparationAcrossToTarget(), 35.0f), "a target 35 m on the car's -right reads 35");

    // Banked 60 degrees: the right axis is flattened before it is normalised.
    lCar.mRight = V(0.5f, 0.8660254f, 0.0f);
    lTarget.mPosition = V(-10.0f, 4.0f, 0.0f);
    Check(Near(lAggression.CalcSeparationAcrossToTarget(), 10.0f), "a banked right axis is flattened before normalising");

    lCar.mRight = V(0.0f, 1.0f, 0.0f);
    Check(IsFltMax(lAggression.CalcSeparationAcrossToTarget()), "a vertical right axis returns FLT_MAX (flt_8204F664)");
    lCar.mRight = V(1.0e-8f, 1.0f, -1.0e-8f);
    Check(IsFltMax(lAggression.CalcSeparationAcrossToTarget()), "a flat right axis under flt_820C3B70 returns FLT_MAX");
    lCar.mRight = V(lfNaN, 0.0f, lfNaN);
    Check(IsFltMax(lAggression.CalcSeparationAcrossToTarget()), "a NaN right axis returns FLT_MAX (vcmpgtfp all-false)");

    // CanSlam: lead 0.5 m is inside (-3, 2), so only the across test decides.
    lCar.mRight = V(1.0f, 0.0f, 0.0f);
    lTarget.mPosition = V(35.0f, 0.0f, 0.5f);
    Check(!lAggression.CanSlam(), "CanSlam refuses a target 35 m to the car's right");
    lTarget.mPosition = V(-35.0f, 0.0f, 0.5f);
    Check(!lAggression.CanSlam(), "CanSlam refuses a target 35 m to the car's left");
    lTarget.mPosition = V(10.0f, 0.0f, 0.5f);
    Check(lAggression.CanSlam(), "CanSlam accepts a target 10 m across");
    lCar.mRight = V(0.0f, 1.0f, 0.0f);
    Check(!lAggression.CanSlam(), "CanSlam refuses while the car's right axis is vertical");
    lCar.mRight = V(1.0f, 0.0f, 0.0f);
    lTarget.mPosition = V(lfNaN, 0.0f, 0.5f);
    Check(!lAggression.CanSlam(), "CanSlam refuses an unordered (NaN) separation (ble/bge/blt polarity)");
    Check(gAssertions == luAssertions, "valid cars raise no assertion");
}

int main()
{
    std::printf("AIAggression regression\n");
    GroupLineupSide();
    GroupAcrossSeparation();
    EndGroup();
    std::printf("%s: %u checks, %u failures\n", guFailures ? "FAIL" : "PASS", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
