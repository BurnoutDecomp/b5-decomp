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

// ------------------------------------------------------------------------------------------------
// G00-D3 / G00-D4  GetSpeedMatchSpeed @0x8277E058, arms 3 and 2. Both measure OUR lead in the
// PLAYER's frame: GetLeadingSeparation(r4 = lwz 0x10 mpPlayerCar, r5 = lwz 8 mpCar), where
// GetLeadingSeparation @0x8277DEA0 is flat(pos(r5) - pos(r4)) on the flat unit heading of r4.
//   arm 3 @0x8277E100: NULL player or lead < flt_82013FB4 (-15) -> flt_8300D754, else
//         StepTo(speed(car), speed(player) - flt_8300D7F0, dt * flt_820C4150 (10)).
//   arm 2 @0x8277E178: lead >= 0 -> lfs 0x48 (mFixedPassingSpeed), else flt_8300D784;
//         StepTo(speed(car), that, dt * flt_820C42C0 (90)).
// Player at the origin heading +Z at 30 m/s, the rival at 25 m/s, dt 0.1 -- every StepTo
// below snaps onto its target.
// ------------------------------------------------------------------------------------------------
static bool GuardedSpeedMatch(AIAggression* lpAggression, f32 lfTimeStep, f32* lpfResult)
{
    __try
    {
        *lpfResult = lpAggression->GetSpeedMatchSpeed(lfTimeStep);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

static void PlaceSpeedMatchCars(AICar& lrCar, AICar& lrPlayer, AICar& lrDecoy)
{
    lrPlayer.mPosition = V(0.0f, 0.0f, 0.0f);
    lrPlayer.mDirection = V(0.0f, 0.0f, 1.0f);
    lrPlayer.mfSpeedInRange = 30.0f;
    lrCar.mDirection = V(0.0f, 0.0f, 1.0f);
    lrCar.mfSpeedInRange = 25.0f;
    lrDecoy.mPosition = V(0.0f, 0.0f, 500.0f);
    lrDecoy.mDirection = V(0.0f, 0.0f, 1.0f);
}

static void GroupSlowToClip()
{
    BeginGroup("G00-D3 GetSpeedMatchSpeed SlowToClip");
    const unsigned luAssertions = gAssertions;
    AIAggression lAggression{};
    AICar lCar{}, lPlayer{}, lDecoy{};
    PlaceSpeedMatchCars(lCar, lPlayer, lDecoy);
    lAggression.mpCar = &lCar;
    lAggression.mpPlayerCar = &lPlayer;
    lAggression.mpTargetCar = &lPlayer;
    lAggression.meSpeedMatchType = ESpeedMatch_SlowToClip;
    const f32 lfTracked = 30.0f - KF_SLOW_TO_CLIP_SPEED_DROP;

    lCar.mPosition = V(0.0f, 0.0f, -20.0f);
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), KF_SLOW_TO_CLIP_FALLBACK),
          "20 m behind the player: gives up to flt_8300D754 (20 mph)");
    lCar.mPosition = V(0.0f, 0.0f, 20.0f);
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), lfTracked),
          "20 m ahead of the player: steps to player speed - flt_8300D7F0");
    lCar.mPosition = V(0.0f, 0.0f, -10.0f);
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), lfTracked), "10 m behind the player: still tracks");

    lCar.mPosition = V(0.0f, 0.0f, -20.0f);
    lAggression.mpTargetCar = &lDecoy;
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), KF_SLOW_TO_CLIP_FALLBACK),
          "mpTargetCar is not read (a decoy 500 m ahead changes nothing)");
    lAggression.mpTargetCar = nullptr;
    f32 lfResult = 0.0f;
    const bool lbRan = GuardedSpeedMatch(&lAggression, 0.1f, &lfResult);
    Check(lbRan && Near(lfResult, KF_SLOW_TO_CLIP_FALLBACK),
          "no target (VEER_EXTREME entered from WAIT/OUT_OF_RANGE): no fault, gives up");
    lAggression.mpPlayerCar = nullptr;
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), KF_SLOW_TO_CLIP_FALLBACK), "no player car: flt_8300D754");
    Check(gAssertions == luAssertions, "no GetLeadingSeparation NULL-car assertion");
}

static void GroupSlower()
{
    BeginGroup("G00-D4 GetSpeedMatchSpeed Slower");
    AIAggression lAggression{};
    AICar lCar{}, lPlayer{}, lDecoy{};
    PlaceSpeedMatchCars(lCar, lPlayer, lDecoy);
    lAggression.mpCar = &lCar;
    lAggression.mpPlayerCar = &lPlayer;
    lAggression.mpTargetCar = &lPlayer;
    lAggression.meSpeedMatchType = ESpeedMatch_Slower;
    lAggression.mFixedPassingSpeed = 22.0f;
    lCar.meCarState = E_AI_CAR_STATE_IN_RANGE;

    lCar.mPosition = V(0.0f, 0.0f, 10.0f);
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), 22.0f),
          "10 m ahead of the player: eases to the passing speed (lfs 0x48)");
    lCar.mPosition = V(0.0f, 0.0f, -10.0f);
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), KF_SLOWER_BEHIND_SPEED),
          "10 m behind the player: drops to flt_8300D784 (40 mph)");
    lCar.mPosition = V(5.0f, 0.0f, 0.0f);
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), 22.0f), "level with the player (lead 0): bge keeps the passing speed");
    lCar.mPosition = V(0.0f, 0.0f, -10.0f);
    lAggression.mpTargetCar = &lDecoy;
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), KF_SLOWER_BEHIND_SPEED), "mpTargetCar is not read");
    lCar.meCarState = E_AI_CAR_STATE_OUT_OF_RANGE;
    Check(Near(lAggression.GetSpeedMatchSpeed(0.1f), KF_NO_PASSING_SPEED), "a car out of range: flt_8300D6F4");
}

// ------------------------------------------------------------------------------------------------
// G00-D5 / D6 / C1 / C2 / C3 / C4: six state handlers byte-store 0 to +0x44 (mbTargetPosValid)
// and never touch +0x68 (mfHangingAroundTimer) -- Hex-Rays prints `*(this + 68) = 0`, and that
// 68 is DECIMAL. Each case arms the flag plus a +0x68 sentinel and runs the handler once.
// ------------------------------------------------------------------------------------------------
static const f32 KF_TEST_SENTINEL = 7.0f;

static void Arm(AIAggression& lrAggression, EAIAggressionState leState, f32 lfStateTime)
{
    lrAggression.meAggressionState = leState;
    lrAggression.mfStateTime = lfStateTime;
    lrAggression.mbTargetPosValid = true;
    lrAggression.mfHangingAroundTimer = KF_TEST_SENTINEL;
}

static void PlaceStateCars(AICar& lrCar, AICar& lrPlayer)
{
    lrCar.mPosition = V(0.0f, 0.0f, 0.0f);
    lrCar.mDirection = V(0.0f, 0.0f, 1.0f);
    lrCar.mRight = V(1.0f, 0.0f, 0.0f);
    lrCar.mfSpeedInRange = 30.0f;
    lrCar.meRouteFindingStyle = E_ROUTE_FINDING_FREE_ROAM;
    lrCar.mAggressiveness.mfProximitySpeedMatch = 1.0f;   // speed-match window: 60 m apart, lead in [-60, 40]
    lrPlayer.mPosition = V(0.0f, 0.0f, 5.0f);             // 5 m ahead of the rival
    lrPlayer.mDirection = V(0.0f, 0.0f, 1.0f);
    lrPlayer.mfSpeedInRange = 40.0f;                      // above KF_CLIP_OFF_MIN_SPEED (flt_8300D720, 80 mph)
}

static void GroupLineupPointDrop()
{
    AICar lCar{}, lPlayer{};
    PlaceStateCars(lCar, lPlayer);
    {
        BeginGroup("G00-D5 SpurtForward");
        AIAggression lAggression{};
        lAggression.mpCar = &lCar;
        Arm(lAggression, E_AI_AGGRESSION_STATE_SPURT_FORWARD, 0.5f);
        lAggression.UpdateAggressionStateSpurtForward();
        Check(!lAggression.mbTargetPosValid, "drops the lineup point (stb 0,0x44 @0x82770DF4)");
        Check(lAggression.mfHangingAroundTimer == KF_TEST_SENTINEL, "does not write +0x68");
        Check(lAggression.meSpeedMatchType == ESpeedMatch_OvertakeSlowly && Near(lAggression.mFixedPassingSpeed, 130.0f * 0.44704f) &&
              lAggression.meAggressionState == E_AI_AGGRESSION_STATE_SPURT_FORWARD && lAggression.mfStateTime == 0.5f,
              "speed match 5, passing speed 130 mph (flt_82F31928 * flt_820C436C), state held");
        Arm(lAggression, E_AI_AGGRESSION_STATE_SPURT_FORWARD, 0.0f);
        lAggression.UpdateAggressionStateSpurtForward();
        Check(!lAggression.mbTargetPosValid && lAggression.meAggressionState == E_AI_AGGRESSION_STATE_OUT_OF_RANGE &&
              lAggression.mfStateTime == -1.0f, "timeout: OUT_OF_RANGE / -1 with the point dropped");
    }
    {
        BeginGroup("G00-D6 FallPast");
        AIAggression lAggression{};
        lAggression.mpCar = &lCar;
        lAggression.mpTargetCar = &lPlayer;
        lAggression.mpPlayerCar = &lPlayer;
        Arm(lAggression, E_AI_AGGRESSION_STATE_FALL_PAST, 5.0f);
        lAggression.UpdateAggressionStateFallPast(nullptr);   // in range, not timed out: returns at the lpPlayerCar test
        Check(!lAggression.mbTargetPosValid, "drops the lineup point (stb r26,0x44 @0x8279359C)");
        Check(lAggression.mfHangingAroundTimer == KF_TEST_SENTINEL, "does not write +0x68");
        Check(lAggression.meSpeedMatchType == ESpeedMatch_Slower && lAggression.meAggressionState == E_AI_AGGRESSION_STATE_FALL_PAST,
              "speed match 2, state held");
        Arm(lAggression, E_AI_AGGRESSION_STATE_FALL_PAST, 5.0f);
        lAggression.mpTargetCar = nullptr;
        lAggression.UpdateAggressionStateFallPast(nullptr);
        Check(!lAggression.mbTargetPosValid && lAggression.meAggressionState == E_AI_AGGRESSION_STATE_OUT_OF_RANGE,
              "out of speed-match range: the point is dropped before the exit");
    }
    {
        BeginGroup("G00-C1 VeerExtreme");
        AIAggression lAggression{};
        lAggression.mpCar = &lCar;
        Arm(lAggression, E_AI_AGGRESSION_STATE_VEER_EXTREME, 0.5f);
        lAggression.UpdateAggressionStateVeerExtreme();
        Check(!lAggression.mbTargetPosValid, "drops the stale VEER point every frame (stb 0,0x44 @0x82770EC8)");
        Check(lAggression.mfHangingAroundTimer == KF_TEST_SENTINEL, "does not write +0x68");
        Check(lAggression.meSpeedMatchType == ESpeedMatch_SlowToClip &&
              lAggression.meAggressionState == E_AI_AGGRESSION_STATE_VEER_EXTREME && lAggression.mfStateTime == 0.5f,
              "speed match 3, state held");
        Arm(lAggression, E_AI_AGGRESSION_STATE_VEER_EXTREME, 0.0f);
        lAggression.UpdateAggressionStateVeerExtreme();
        Check(!lAggression.mbTargetPosValid && lAggression.meAggressionState == E_AI_AGGRESSION_STATE_WAIT &&
              lAggression.mfStateTime == 1.0f, "timeout: WAIT for flt_82001C98 (1 s), point dropped");
    }
    {
        BeginGroup("G00-C2 BeFodder");
        AIAggression lAggression{};
        lAggression.mpCar = &lCar;
        Arm(lAggression, E_AI_AGGRESSION_STATE_BE_FODDER, 0.5f);
        lAggression.UpdateAggressionStateBeFodder();
        Check(!lAggression.mbTargetPosValid, "drops the lineup point (stb 0,0x44 @0x8277DC9C)");
        Check(lAggression.mfHangingAroundTimer == KF_TEST_SENTINEL, "does not write +0x68");
        Check(lAggression.meSpeedMatchType == ESpeedMatch_Enabled && lAggression.mfRelativePositionAhead == 2.0f &&
              lAggression.meAggressionState == E_AI_AGGRESSION_STATE_BE_FODDER, "speed match 1, +2 m ahead, state held");
        Arm(lAggression, E_AI_AGGRESSION_STATE_BE_FODDER, 0.0f);
        lAggression.UpdateAggressionStateBeFodder();
        Check(!lAggression.mbTargetPosValid && lAggression.meAggressionState == E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND &&
              lAggression.mfStateTime == 3.0f, "timeout (not PURSUIT): CLIP_OFF_BEHIND for 3 s, point dropped");
    }
    {
        BeginGroup("G00-C3 ClipOffBehind");
        AIAggression lAggression{};
        lAggression.mpCar = &lCar;
        lAggression.mpTargetCar = &lPlayer;
        Arm(lAggression, E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND, 0.5f);
        lAggression.UpdateAggressionStateClipOffBehind();
        Check(!lAggression.mbTargetPosValid, "with a target: drops the lineup point (stb r30,0x44 @0x82770C2C)");
        Check(lAggression.mfHangingAroundTimer == KF_TEST_SENTINEL, "does not write +0x68");
        Check(lAggression.meSpeedMatchType == ESpeedMatch_SlowToClip &&
              lAggression.meAggressionState == E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND, "speed match 3, state held");
        lAggression.mpTargetCar = nullptr;
        Arm(lAggression, E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND, 0.5f);
        lAggression.UpdateAggressionStateClipOffBehind();
        Check(lAggression.mbTargetPosValid && lAggression.mfHangingAroundTimer == KF_TEST_SENTINEL &&
              lAggression.meAggressionState == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && lAggression.mfStateTime == -1.0f,
              "no target: OUT_OF_RANGE and neither field is stored (0x82770C00..0x82770C08)");
    }
    {
        BeginGroup("G00-C4 OvertakeFast");
        AIAggression lAggression{};
        lAggression.mpCar = &lCar;
        Arm(lAggression, E_AI_AGGRESSION_STATE_OVERTAKE_FAST, 5.0f);
        lAggression.UpdateAggressionStateOvertakeFast();   // mpPlayerCar NULL
        Check(!lAggression.mbTargetPosValid, "drops the lineup point before the player test (stb r30,0x44 @0x8278B468)");
        Check(lAggression.mfHangingAroundTimer == KF_TEST_SENTINEL, "does not write +0x68");
        Check(lAggression.meSpeedMatchType == ESpeedMatch_OvertakeFast &&
              lAggression.meAggressionState == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && lAggression.mfStateTime == -1.0f,
              "no player: speed match 4, OUT_OF_RANGE / -1");
        lAggression.mpPlayerCar = &lPlayer;
        Arm(lAggression, E_AI_AGGRESSION_STATE_OVERTAKE_FAST, 5.0f);
        lAggression.UpdateAggressionStateOvertakeFast();
        Check(!lAggression.mbTargetPosValid && lAggression.meAggressionState == E_AI_AGGRESSION_STATE_OVERTAKE_FAST,
              "player 5 m ahead: state held, point dropped");
    }
}

// ------------------------------------------------------------------------------------------------
// G00-D6b  UpdateAggressionStateFallPast @0x82793568, the BE_FODDER state time: lerp(0.0 @0x820C4288,
// 2.0 @0x820C428C, mfTimeForSpeedMatch) then `fsel f0,f0,f0,f31` @0x827937C4 with f31 =
// flt_820037C8 (-1.0) loaded @0x82793620: negative or NaN -> -1.0.
// ------------------------------------------------------------------------------------------------
static void GroupFodderTimeFloor()
{
    BeginGroup("G00-D6b FallPast BE_FODDER time floor");
    AICar lCar{}, lPlayer{};
    PlaceStateCars(lCar, lPlayer);   // the player 5 m ahead: inside the (0, 10) m BE_FODDER band
    AIAggression lAggression{};
    lAggression.mpCar = &lCar;
    lAggression.mpTargetCar = &lPlayer;
    lAggression.mpPlayerCar = &lPlayer;
    const f32 lafFactor[] = { 0.7f, 0.0f, -0.25f, std::numeric_limits<f32>::quiet_NaN() };
    const f32 lafExpected[] = { 1.4f, 0.0f, -1.0f, -1.0f };
    const char* const lapcLabel[] = {
        "time knob 0.7 -> BE_FODDER for 1.4 s",
        "time knob 0.0 -> BE_FODDER for 0.0 s (fsel keeps +0)",
        "negative lerp falls back to f31 = flt_820037C8 (-1.0), not 0.0",
        "NaN lerp falls back to f31 = -1.0",
    };
    for (s32 liCase = 0; liCase < 4; ++liCase)
    {
        lAggression.meAggressionState = E_AI_AGGRESSION_STATE_FALL_PAST;
        lAggression.mfStateTime = 5.0f;
        lCar.mAggressiveness.mfTimeForSpeedMatch = lafFactor[liCase];
        lAggression.UpdateAggressionStateFallPast(&lPlayer);
        Check(lAggression.meAggressionState == E_AI_AGGRESSION_STATE_BE_FODDER &&
              Near(lAggression.mfStateTime, lafExpected[liCase]), lapcLabel[liCase]);
    }
}

// ------------------------------------------------------------------------------------------------
// G00-C5  UpdateAggressionPassive @0x82793830: `mr r5,r4` @0x82793850 keeps the ARGUMENT for
// bl OutOfSpeedMatchRange @0x827938D0 -- the range test is on the dispatched (player) car, never
// on the member mpTargetCar. Window at proximity 1.0: 60 m apart, lead in [-60, 40]
// (0x820C42EC / 0x820C42E4, OutOfSpeedMatchRange @0x8278B680).
// ------------------------------------------------------------------------------------------------
static void GroupPassive()
{
    BeginGroup("G00-C5 UpdateAggressionPassive");
    AICar lCar{}, lPlayer{}, lDecoy{};
    PlaceStateCars(lCar, lPlayer);   // the player 5 m ahead, inside the window
    lDecoy.mPosition = V(0.0f, 0.0f, 500.0f);
    lDecoy.mDirection = V(0.0f, 0.0f, 1.0f);
    AIAggression lAggression{};
    lAggression.mpCar = &lCar;
    lAggression.mpPlayerCar = &lPlayer;
    auto Rearm = [&](const AICar* lpTargetMember, f32 lfStateTime) {
        lAggression.mpTargetCar = lpTargetMember;
        lAggression.meAggressionState = E_AI_AGGRESSION_STATE_PASSIVE;
        lAggression.mfStateTime = lfStateTime;
        lAggression.mbTargetPosValid = true;
        lAggression.meSpeedMatchType = ESpeedMatch_Enabled;
    };

    // Road Rage PASSIVE entered from OUT_OF_RANGE's miProximityIndex < 0 arm: no target member.
    Rearm(nullptr, 3.0f);
    lAggression.UpdateAggressionPassive(&lPlayer);
    Check(lAggression.meAggressionState == E_AI_AGGRESSION_STATE_PASSIVE && lAggression.mfStateTime == 3.0f,
          "NULL mpTargetCar: the player inside the window holds PASSIVE");
    Check(lAggression.meSpeedMatchType == ESpeedMatch_Disabled && !lAggression.mbTargetPosValid,
          "speed match off and lineup point dropped every frame");
    Rearm(&lDecoy, 3.0f);
    lAggression.UpdateAggressionPassive(&lPlayer);
    Check(lAggression.meAggressionState == E_AI_AGGRESSION_STATE_PASSIVE && lAggression.mfStateTime == 3.0f,
          "a stale target member 500 m away does not end PASSIVE");
    Rearm(&lPlayer, 3.0f);
    lPlayer.mPosition = V(0.0f, 0.0f, 100.0f);
    lAggression.UpdateAggressionPassive(&lPlayer);
    Check(lAggression.meAggressionState == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && lAggression.mfStateTime == -1.0f,
          "the player 100 m ahead (out of the window) ends PASSIVE");
    lPlayer.mPosition = V(0.0f, 0.0f, 5.0f);
    Rearm(nullptr, 0.0f);
    lAggression.UpdateAggressionPassive(&lPlayer);
    Check(lAggression.meAggressionState == E_AI_AGGRESSION_STATE_OUT_OF_RANGE, "timeout ends PASSIVE");
    Rearm(nullptr, 3.0f);
    lAggression.UpdateAggressionPassive(nullptr);
    Check(lAggression.meAggressionState == E_AI_AGGRESSION_STATE_PASSIVE, "no dispatched car: PASSIVE held");
}

int main()
{
    std::printf("AIAggression regression\n");
    GroupLineupSide();
    GroupAcrossSeparation();
    GroupSlowToClip();
    GroupSlower();
    GroupLineupPointDrop();
    GroupFodderTimeFloor();
    GroupPassive();
    EndGroup();
    std::printf("%s: %u checks, %u failures\n", guFailures ? "FAIL" : "PASS", guChecks, guFailures);
    return guFailures ? 1 : 0;
}
