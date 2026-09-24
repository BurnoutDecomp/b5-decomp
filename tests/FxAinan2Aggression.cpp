// FX-AINAN2 regression (crash parity 2026-09-24): NaN branch polarity in the rival aggression state
// machine (BrnAIAggression.cpp). The production bodies are extracted verbatim by
// run_fxainan2_aggression.py; the geometry queries they call are fixtures returning configured
// answers, so each check isolates one console branch.
//
// Console polarity (PowerPC: after fcmpu, ble/bge/bne are TAKEN on unordered, blt/bgt/beq are not;
// fsel d,a,b,c = a >= 0 ? b : c picks c on NaN):
//   StateHasTimedOut(), inlined 12x as `fcmpu t,-1 ; bne` + `fcmpu t,0 ; ble -> timed out`:
//     a NaN state time HAS timed out -- Passive 0x827938B4, BeFodder 0x8277DC34, ClipOffBehind
//     0x82770BD4, ComeSlowFromBehind 0x8278B63C, DropBackToSlam 0x82796930, FallPast 0x82793648,
//     OvertakeFast 0x8278B518, OvertakeToSlam 0x82793A30, SpurtForward 0x82770E2C, Veer 0x8277DD1C,
//     VeerExtreme 0x82770EF4, Wait 0x82770E84
//   AttackSlam      blt 0x82793BB4 (lead vs -3.0)       NaN lead -> the lineup arm
//   OutOfRange      bge 0x827966B0 (aheadness vs 20.0)  NaN -> the far-ahead arm (HANG_AROUND_AHEAD)
//                   ble 0x82796744 (lead vs 4.0)        NaN -> OVERTAKE_TO_SLAM (12 s)
//                   bge 0x8279679C (lead vs 0.0)        NaN -> the "ahead" arm (FALL_PAST / PASSIVE)
//                   bge 0x827967EC (schedule offset)    NaN -> PASSIVE (12 s)
//   AcrossSeparationTooBig  ble 0x8277DE60              NaN separation -> the across test
//   CheckForCarVeeringAwayFromPlayer ble 0x82770A30     NaN contact -> short-touch arm -> no veer
//   SetSlowOvertakingSpeed  fsel 0x8277DBB0 (max - s, s, max)  NaN speed -> GetMaxOvertakeSpeed()
//   StopAttacking   bge 0x82793DF0 skips "lfWaitTime >= 0.0f" for a NaN wait
//   CurveToKeepLarge bge 0x827669F0 / ble 0x82766A1C skip both range asserts for a NaN
// Every NaN check below fails on the pre-fix body (run with --rev <fix>~1); the ordered controls
// pass on both, pinning that the re-spelling changed nothing for real numbers.
#include "GameSource/World/AI/BrnAIAggression.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/BrnAIUtils.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Numeric/CgsRandom.h"
#include "rw/math/vpu/vector3_operation.h"
#include <cmath>
#include <cstdio>
#include <limits>

static unsigned gAssertions = 0;
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char*, const char*, int) { ++gAssertions; return 0; }
void* EndAssert() { return nullptr; }
} }

// Configured answers of the queries the bodies under test call.
static f32 gfSeparation = 0.0f, gfAcross = 0.0f, gfLead = 0.0f, gfAheadness = 0.0f, gfDecentSpeed = 0.0f;
static bool gbOutOfRange = false, gbCanSlam = false, gbTooSlow = false, gbFindTarget = false, gbAttack = false;
static unsigned guRangeCalls = 0, guAcrossCalls = 0, guLineups = 0;

namespace BrnAI {
Vector3 AICar::GetPosition() const { return mPosition; }
f32 AICar::GetSpeed() const { return mfSpeedInRange; }
f32 AICar::GetDecentSpeed() const { return gfDecentSpeed; }
f32 Aggressiveness::GetAggressionLevel() const { return mfAggressionLevel; }
CgsNumeric::Random AIAggression::mRandom;
f32  AIAggression::GetSeparation(const AICar*, const AICar*)              { return gfSeparation; }
f32  AIAggression::GetAcrossSeparation(const AICar*, const AICar*)        { ++guAcrossCalls; return gfAcross; }
f32  AIAggression::GetLeadingSeparation(const AICar*, const AICar*) const { return gfLead; }
f32  AIAggression::GetAheadness(const AICar*, Vector3)                    { return gfAheadness; }
bool AIAggression::OutOfSpeedMatchRange(const AICar*, const AICar*)       { ++guRangeCalls; return gbOutOfRange; }
bool AIAggression::CanSlam()                                              { return gbCanSlam; }
bool AIAggression::CarIsTooSlow(const AICar*)                             { return gbTooSlow; }
bool AIAggression::DecideToAttack()                                       { return gbAttack; }
bool AIAggression::FindTarget(const AICar* lpCandidate)
{
    if (gbFindTarget)
        mpTargetCar = lpCandidate;
    return gbFindTarget;
}
Vector3 AIAggression::GetPositionNextToTarget(const AICar*, const AICar*, f32 lfAlignment)
{
    ++guLineups;
    return Vector3{ lfAlignment, 0.0f, 0.0f, 0.0f };
}
}
// The [1,2) ring draw minus one; pinned so the spurt times are exact.
namespace CgsNumeric { f32 Random::RandomFloat() { return 0.5f; } }

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
    bool Near(f32 lfA, f32 lfB) { return std::fabs(lfA - lfB) <= 1.0e-4f * (1.0f + std::fabs(lfB)); }
    const f32 KF_NAN = std::numeric_limits<f32>::quiet_NaN();

    // One rival (mCar) and the player (mPlayer, driven by the player, 40 m/s), the rival IN_RANGE
    // in a RACE, far from its checkpoint, aggression level 0.5. Every query answers "5 m apart,
    // level, in speed-match range" unless a check says otherwise.
    struct Scene
    {
        AICar        mCar{};
        AICar        mPlayer{};
        AIAggression mAgg{};

        Scene(EAIAggressionState leState, f32 lfStateTime)
        {
            mCar.meRouteFindingStyle = E_ROUTE_FINDING_RACE;
            mCar.meCarState = E_AI_CAR_STATE_IN_RANGE;
            mCar.mfSpeedInRange = 30.0f;
            mCar.mAggressiveness.mfAggressionLevel = 0.5f;
            mCar.miProximityIndex = 1;
            mCar.mfDistanceToCheckpoint = 5000.0f;
            mCar.mfScheduleOffset1 = 1.0f;
            mPlayer.mfSpeedInRange = 40.0f;
            mPlayer.mbIsPlayer = true;
            mPlayer.mbIsDrivenByPlayer = true;
            mAgg.mpCar = &mCar;
            mAgg.mpPlayerCar = &mPlayer;
            mAgg.mpTargetCar = nullptr;
            mAgg.meAggressionState = leState;
            mAgg.mfStateTime = lfStateTime;
            mAgg.mfNonSpeedMatchedSpeed = 1000.0f;
            gfSeparation = 5.0f; gfAcross = 5.0f; gfLead = 0.0f; gfAheadness = 0.0f; gfDecentSpeed = 10.0f;
            gbOutOfRange = gbCanSlam = gbTooSlow = gbFindTarget = gbAttack = false;
            guRangeCalls = guAcrossCalls = guLineups = 0;
        }
        EAIAggressionState State() const { return mAgg.meAggressionState; }
    };
}

// ---- the twelve inlined StateHasTimedOut() tests ------------------------------------------------
static void GroupTimedOut()
{
    {
        Scene s(E_AI_AGGRESSION_STATE_PASSIVE, KF_NAN);
        s.mAgg.UpdateAggressionPassive(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mfStateTime == -1.0f && guRangeCalls == 0,
              "Passive: a NaN state time has timed out (bne 0x82793898 / ble 0x827938B4) -- no range query");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_PASSIVE, 0.5f);
        s.mAgg.UpdateAggressionPassive(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_PASSIVE && guRangeCalls == 1,
              "Passive control: 0.5 s left is not timed out and asks OutOfSpeedMatchRange");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_BE_FODDER, KF_NAN);
        s.mAgg.UpdateAggressionStateBeFodder();
        Check(s.State() == E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND && s.mAgg.mfStateTime == 3.0f,
              "BeFodder: a NaN state time has timed out (ble 0x8277DC34) -> CLIP_OFF_BEHIND 3 s");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND, KF_NAN);
        s.mPlayer.mfSpeedInRange = 1000.0f;   // well above KF_CLIP_OFF_MIN_SPEED
        s.mAgg.mpTargetCar = &s.mPlayer;
        s.mAgg.UpdateAggressionStateClipOffBehind();
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mfStateTime == -1.0f,
              "ClipOffBehind: a NaN state time has timed out (ble 0x82770BD4) -> OUT_OF_RANGE");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OVERTAKE_SLOWLY, KF_NAN);
        s.mAgg.UpdateAggressionStateComeSlowFromBehind();
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mfStateTime == -1.0f,
              "ComeSlowFromBehind: a NaN state time has timed out (ble 0x8278B63C) -> OUT_OF_RANGE");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_DROP_BACK_TO_SLAM, KF_NAN);
        s.mAgg.mpTargetCar = &s.mPlayer;
        s.mAgg.UpdateAggressionStateDropBackToSlam(&s.mPlayer, 0.033f);
        Check(s.State() == E_AI_AGGRESSION_STATE_WAIT && s.mAgg.mpTargetCar == nullptr && guLineups == 0,
              "DropBackToSlam: a NaN state time has timed out (ble 0x82796930) -> StopAttacking -> WAIT");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_FALL_PAST, KF_NAN);
        s.mAgg.mpTargetCar = &s.mPlayer;
        s.mAgg.UpdateAggressionStateFallPast(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mfStateTime == -1.0f,
              "FallPast: a NaN state time has timed out (ble 0x82793648) -> OUT_OF_RANGE for a racer");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_FALL_PAST, KF_NAN);
        s.mCar.meRouteFindingStyle = E_ROUTE_FINDING_ROAD_RAGE;
        s.mAgg.mpTargetCar = &s.mPlayer;
        s.mAgg.UpdateAggressionStateFallPast(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_SPURT_FORWARD && Near(s.mAgg.mfStateTime, 0.75f),
              "FallPast: a timed-out road-rager re-rolls a (1 + 0.5) * 0.5 s spurt");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OVERTAKE_FAST, KF_NAN);
        s.mAgg.UpdateAggressionStateOvertakeFast();
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mfStateTime == -1.0f,
              "OvertakeFast: a NaN state time has timed out (ble 0x8278B518) -> OUT_OF_RANGE");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OVERTAKE_TO_SLAM, KF_NAN);
        s.mCar.meRouteFindingStyle = E_ROUTE_FINDING_ROAD_RAGE;
        s.mAgg.mpTargetCar = &s.mPlayer;
        s.mAgg.UpdateAggressionStateOvertakeToSlam(&s.mPlayer, 0.033f);
        Check(s.State() == E_AI_AGGRESSION_STATE_SPURT_FORWARD && Near(s.mAgg.mfStateTime, 0.75f),
              "OvertakeToSlam: a NaN state time has timed out (ble 0x82793A30) -> SPURT_FORWARD 0.75 s");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_SPURT_FORWARD, KF_NAN);
        s.mAgg.UpdateAggressionStateSpurtForward();
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mfStateTime == -1.0f,
              "SpurtForward: a NaN state time has timed out (ble 0x82770E2C) -> OUT_OF_RANGE");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_VEER, KF_NAN);
        s.mCar.mbIsTouchingPlayer = false;
        s.mAgg.UpdateAggressionStateVeer();
        Check(s.State() == E_AI_AGGRESSION_STATE_WAIT && s.mAgg.mfStateTime == 1.0f && guLineups == 0,
              "Veer: a NaN state time has timed out (ble 0x8277DD1C) -> WAIT 1 s when not touching");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_VEER, KF_NAN);
        s.mCar.mbIsTouchingPlayer = true;
        s.mAgg.UpdateAggressionStateVeer();
        Check(s.State() == E_AI_AGGRESSION_STATE_SPURT_FORWARD && Near(s.mAgg.mfStateTime, 1.5f),
              "Veer: a timed-out veer still touching the player spurts 1 + 0.5 s");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_VEER_EXTREME, KF_NAN);
        s.mAgg.UpdateAggressionStateVeerExtreme();
        Check(s.State() == E_AI_AGGRESSION_STATE_WAIT && s.mAgg.mfStateTime == 1.0f,
              "VeerExtreme: a NaN state time has timed out (ble 0x82770EF4) -> WAIT 1 s");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_WAIT, KF_NAN);
        s.mAgg.mpTargetCar = &s.mPlayer;
        s.mAgg.UpdateAggressionStateWait();
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mpTargetCar == nullptr,
              "Wait: a NaN state time has timed out (ble 0x82770E84) -> OUT_OF_RANGE, target dropped");
    }
    // Ordered controls: 0.0 has timed out, -1.0 (no timer) and 2.0 have not.
    {
        Scene s(E_AI_AGGRESSION_STATE_WAIT, 0.0f);
        s.mAgg.UpdateAggressionStateWait();
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE, "Wait control: 0.0 has timed out");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_WAIT, -1.0f);
        s.mAgg.UpdateAggressionStateWait();
        Check(s.State() == E_AI_AGGRESSION_STATE_WAIT, "Wait control: -1.0 is 'no timer', never timed out");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_SPURT_FORWARD, 2.0f);
        s.mAgg.UpdateAggressionStateSpurtForward();
        Check(s.State() == E_AI_AGGRESSION_STATE_SPURT_FORWARD, "SpurtForward control: 2 s left keeps spurting");
    }
}

// ---- AttackSlam @0x82793AE8 -------------------------------------------------------------------
static void GroupAttackSlam()
{
    {
        Scene s(E_AI_AGGRESSION_STATE_ATTACK_SLAM, 2.0f);
        s.mAgg.mpTargetCar = &s.mPlayer;
        gfLead = KF_NAN;
        s.mAgg.UpdateAggressionStateAttackSlam();
        Check(s.State() == E_AI_AGGRESSION_STATE_ATTACK_SLAM && s.mAgg.mbTargetPosValid && guLineups == 1,
              "AttackSlam: a NaN lead falls through blt 0x82793BB4 into the lineup arm and keeps slamming");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_ATTACK_SLAM, 2.0f);
        s.mAgg.mpTargetCar = &s.mPlayer;
        gfLead = -10.0f;
        s.mAgg.UpdateAggressionStateAttackSlam();
        Check(s.State() == E_AI_AGGRESSION_STATE_OVERTAKE_TO_SLAM && s.mAgg.mfStateTime == 1.0f,
              "AttackSlam control: 10 m behind re-arms OVERTAKE_TO_SLAM");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_ATTACK_SLAM, KF_NAN);
        s.mAgg.mpTargetCar = &s.mPlayer;
        s.mAgg.UpdateAggressionStateAttackSlam();
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mfStateTime == 1.5f,
              "AttackSlam control: `t == -1 || t > 0` already times a NaN out (1.5 s, flt_82004D04)");
    }
}

// ---- OutOfRange @0x827965E8 -------------------------------------------------------------------
static void GroupOutOfRange()
{
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        s.mCar.meRouteFindingStyle = E_ROUTE_FINDING_ROAD_RAGE;
        s.mCar.miProximityIndex = -1;
        s.mCar.meRelativeLocation = E_RELATIVE_INFRONT_SEPARATING;
        gfAheadness = KF_NAN;
        s.mAgg.UpdateAggressionStateOutOfRange(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_HANG_AROUND_AHEAD && s.mAgg.mfStateTime == -1.0f,
              "OutOfRange: a NaN aheadness takes bge 0x827966B0 -> HANG_AROUND_AHEAD (not PASSIVE)");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        s.mCar.meRouteFindingStyle = E_ROUTE_FINDING_PURSUIT;
        gbFindTarget = gbAttack = true;
        gfLead = KF_NAN;
        s.mAgg.UpdateAggressionStateOutOfRange(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_OVERTAKE_TO_SLAM && s.mAgg.mfStateTime == 12.0f,
              "OutOfRange: an attacker with a NaN lead takes ble 0x82796744 -> OVERTAKE_TO_SLAM 12 s");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        s.mCar.meRouteFindingStyle = E_ROUTE_FINDING_PURSUIT;
        gbFindTarget = true;
        gfLead = KF_NAN;
        s.mAgg.UpdateAggressionStateOutOfRange(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_FALL_PAST && s.mAgg.mfStateTime == 6.0f,
              "OutOfRange: a non-attacker with a NaN lead takes bge 0x8279679C (ahead) -> FALL_PAST 6 s");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        gbFindTarget = true;
        gfLead = 5.0f;
        s.mCar.mfScheduleOffset1 = KF_NAN;
        s.mAgg.UpdateAggressionStateOutOfRange(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_PASSIVE && s.mAgg.mfStateTime == 12.0f,
              "OutOfRange: a racer with a NaN schedule offset takes bge 0x827967EC -> PASSIVE 12 s");
    }
    // Ordered controls (pass on both spellings).
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        gbFindTarget = true;
        gfLead = 5.0f;
        s.mCar.mfScheduleOffset1 = -1.0f;
        s.mAgg.UpdateAggressionStateOutOfRange(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_FALL_PAST && s.mAgg.mfStateTime == 8.0f,
              "OutOfRange control: a racer behind schedule, far from the checkpoint, falls past 8 s");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        gbFindTarget = true;
        gfLead = 5.0f;
        s.mCar.mfScheduleOffset1 = 0.0f;
        s.mAgg.UpdateAggressionStateOutOfRange(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_PASSIVE, "OutOfRange control: schedule offset 0.0 is PASSIVE");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        s.mCar.meRouteFindingStyle = E_ROUTE_FINDING_PURSUIT;
        gbFindTarget = gbAttack = true;
        gfLead = 4.5f;
        s.mAgg.UpdateAggressionStateOutOfRange(&s.mPlayer);
        Check(s.State() == E_AI_AGGRESSION_STATE_DROP_BACK_TO_SLAM && s.mAgg.mfStateTime == 16.0f,
              "OutOfRange control: an attacker 4.5 m ahead drops back to slam 16 s");
    }
}

// ---- AcrossSeparationTooBig @0x8277DE38 / CheckForCarVeeringAwayFromPlayer @0x827709C0 ---------
static void GroupSeparationAndVeer()
{
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        gfSeparation = KF_NAN;
        gfAcross = 5.0f;
        Check(!s.mAgg.AcrossSeparationTooBig(&s.mCar, &s.mPlayer) && guAcrossCalls == 1,
              "AcrossSeparationTooBig: a NaN separation takes ble 0x8277DE60 to the across test (5 m -> false)");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        gfSeparation = 30.0f;
        Check(s.mAgg.AcrossSeparationTooBig(&s.mCar, &s.mPlayer) && guAcrossCalls == 0,
              "AcrossSeparationTooBig control: 30 m apart is too big without an across test");
        gfSeparation = 5.0f; gfAcross = 30.0f;
        Check(s.mAgg.AcrossSeparationTooBig(&s.mCar, &s.mPlayer),
              "AcrossSeparationTooBig control: 30 m across is too big");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        s.mCar.mbIsTouchingPlayer = true;
        s.mAgg.mfContinuousContactTimer = KF_NAN;
        s.mAgg.CheckForCarVeeringAwayFromPlayer(0.1f);
        Check(s.State() == E_AI_AGGRESSION_STATE_OUT_OF_RANGE && s.mAgg.mfStateTime == -1.0f,
              "CheckForCarVeeringAwayFromPlayer: a NaN contact time takes ble 0x82770A30 then blelr -- no veer");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        s.mCar.mbIsTouchingPlayer = true;
        s.mAgg.mfContinuousContactTimer = 0.5f;
        s.mAgg.CheckForCarVeeringAwayFromPlayer(0.1f);
        Check(s.State() == E_AI_AGGRESSION_STATE_VEER && s.mAgg.mfStateTime == 1.0f,
              "CheckForCarVeeringAwayFromPlayer control: 0.6 s of contact veers");
        s.mAgg.mfContinuousContactTimer = 2.0f;
        s.mAgg.CheckForCarVeeringAwayFromPlayer(0.1f);
        Check(s.State() == E_AI_AGGRESSION_STATE_VEER_EXTREME,
              "CheckForCarVeeringAwayFromPlayer control: 2.1 s of contact veers extreme");
    }
}

// ---- SetSlowOvertakingSpeed @0x8277DB38 / StopAttacking @0x82793D48 / CurveToKeepLarge @0x827669C0
static void GroupSpeedsAndAsserts()
{
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        s.mPlayer.mfSpeedInRange = KF_NAN;
        s.mAgg.SetSlowOvertakingSpeed();
        const f32 lfMax = s.mAgg.GetMaxOvertakeSpeed();
        Check(s.mAgg.mFixedPassingSpeed == lfMax,
              "SetSlowOvertakingSpeed: a NaN player speed is capped to GetMaxOvertakeSpeed() by fsel 0x8277DBB0");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_OUT_OF_RANGE, -1.0f);
        s.mPlayer.mfSpeedInRange = 1000.0f;
        s.mAgg.SetSlowOvertakingSpeed();
        Check(s.mAgg.mFixedPassingSpeed == s.mAgg.GetMaxOvertakeSpeed(),
              "SetSlowOvertakingSpeed control: a fast player caps at the max overtake speed");
        s.mPlayer.mfSpeedInRange = 0.0f;
        s.mAgg.SetSlowOvertakingSpeed();
        Check(s.mAgg.mFixedPassingSpeed == KF_OVERTAKE_FAST_MIN_SPEED,
              "SetSlowOvertakingSpeed control: a stopped player floors at KF_OVERTAKE_FAST_MIN_SPEED");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_ATTACK_SLAM, 1.0f);
        s.mAgg.mpTargetCar = &s.mPlayer;
        s.mCar.mAggressiveness.mfAggressionLevel = KF_NAN;
        const unsigned luBefore = gAssertions;
        s.mAgg.StopAttacking(E_AGGRESSION_DELAYEDATTACK);
        Check(gAssertions == luBefore,
              "StopAttacking: a NaN wait skips \"lfWaitTime >= 0.0f\" (bge 0x82793DF0)");
        Check(s.State() == E_AI_AGGRESSION_STATE_WAIT && s.mAgg.mpTargetCar == nullptr,
              "StopAttacking: the NaN wait still goes to WAIT");
    }
    {
        Scene s(E_AI_AGGRESSION_STATE_ATTACK_SLAM, 1.0f);
        s.mAgg.mpTargetCar = &s.mPlayer;
        const unsigned luBefore = gAssertions;
        s.mAgg.StopAttacking(E_AGGRESSION_DELAYEDATTACK);
        Check(gAssertions == luBefore && s.State() == E_AI_AGGRESSION_STATE_WAIT && s.mAgg.mfStateTime == 1.0f,
              "StopAttacking control: level 0.5 waits 1.0 s without an assertion");
    }
    {
        const unsigned luBefore = gAssertions;
        const f32 lfCurve = CurveToKeepLarge(KF_NAN);
        Check(gAssertions == luBefore && lfCurve != lfCurve,
              "CurveToKeepLarge: a NaN skips both range asserts (bge 0x827669F0 / ble 0x82766A1C) and returns NaN");
    }
    {
        const unsigned luBefore = gAssertions;
        Check(Near(CurveToKeepLarge(0.5f), 0.75f) && Near(CurveToKeepLarge(-0.5f), -0.75f) && gAssertions == luBefore,
              "CurveToKeepLarge control: +/-0.5 -> +/-0.75, no assertion");
        (void)CurveToKeepLarge(2.0f);
        Check(gAssertions == luBefore + 1, "CurveToKeepLarge control: 2.0 fires the `<= 1.0f` assert once");
    }
}

int main()
{
    GroupTimedOut();
    GroupAttackSlam();
    GroupOutOfRange();
    GroupSeparationAndVeer();
    GroupSpeedsAndAsserts();
    std::printf("FxAinan2Aggression: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures == 0 ? 0 : 1;
}
