#include "GameSource/World/AI/BrnAIAggression.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIUtils.h"                 // BrnAI::StepTo (step-toward helper)

#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Numeric/CgsRandom.h"       // CgsNumeric::Random (mRandom.RandomFloat)
#include "rw/math/vpu/vector3_operation.h"                  // rw::math::vpu vector ops

#include <cmath>   // std::fabs, std::sqrt where the de-SIMD'd math needs scalar helpers

// BrnAI::AIAggression -- the rival-AI aggression / slam-lineup state machine (Burnout's
// "shunting" AI brain). This TU bodies the 35 state-machine functions against the AICar
// minimal-slice foundation (BrnAICar.h); the AIAggression layout + method interface live in
// BrnAIAggression.h, and GetTargetPos is bodied separately in BrnAIAggression.cpp. Bodies were
// reconstructed from the X360 pseudocode/asm and adversarially verified per function group.
//
// FLAGGED file-local constants below: where a rodata tuning value (KF_*) is not present in the
// per-function exports it is shipped as a flagged placeholder (NOT fabricated); resolve from the
// XEX .rdata before relying on its magnitude.

namespace BrnAI
{
    namespace vpu = rw::math::vpu;


    // ===== file-local tuning constants (see header note; flagged where unrecovered) =====
// --- G1-dispatch ---
// Post-attack wait-time bounds, lerped by the car's aggression level in StopAttacking.
// X360 rodata at unk_820C426C / unk_820C4270 (BrnAIAggression.cpp file-statics) -- values
// not recoverable from the dossier asm.
const f32 KF_MIN_POST_ATTACK_WAIT_TIME = 0.0f; // FLAG: rodata value unrecovered (unk_820C426C)
const f32 KF_MAX_POST_ATTACK_WAIT_TIME = 0.0f; // FLAG: rodata value unrecovered (unk_820C4270)

// Minimum speed below which a candidate car is considered too slow to target (CarIsTooSlow).
// X360 rodata at flt_8300D9A0 -- value not recoverable from the dossier asm.
const f32 KF_CAR_TOO_SLOW_SPEED = 0.0f; // FLAG: rodata value unrecovered (flt_8300D9A0)

// --- G2-geometry ---
// CalcSeparationAcrossToTarget uses a small epsilon to reject a degenerate (near-zero) right
// vector before normalising. The X360 loads it from rodata flt_820C3B70, an un-valued .rdata
// float not present in the available exports.
const f32 KF_QUERY_POS_EPSILON = 0.0f; // FLAG: rodata value (flt_820C3B70) unrecovered -- placeholder; guards an effectively-unreachable degenerate-right-vector branch

// --- G3-speedmatch ---
// --- BrnAIAggression.cpp file-local speed-match tuning constants (group G3-speedmatch) ---
// All of these are rodata floats referenced by ADDRESS in the X360 build (flt_8300xxxx /
// unk_820C42xx). Their numeric VALUES are NOT present in the dossier (no immediate in the
// asm), so they are declared here by KF_ name with the value FLAGGED UNKNOWN. Recover the
// real values from the X360 .rdata at the listed addresses before final commit.

// GetMinFallBackSpeed -- per-route-finding-style minimum fall-back speed.
const f32 KF_MIN_FALLBACK_SPEED_PURSUIT     = 0.0f; // flt_8300D988 -- FLAG: rodata value unrecovered
const f32 KF_MIN_FALLBACK_SPEED_ROAD_RAGE   = 0.0f; // flt_8300D744 -- FLAG: rodata value unrecovered
const f32 KF_MIN_FALLBACK_SPEED_MARKED_MAN  = 0.0f; // flt_8300DB2C -- FLAG: rodata value unrecovered
const f32 KF_MIN_FALLBACK_SPEED_DEFAULT     = 0.0f; // flt_8300D7DC -- FLAG: rodata value unrecovered

// GetMaxOvertakeSpeed -- aggression-level lerp endpoints (lerp(lo, hi, aggressionLevel)).
const f32 KF_MAX_OVERTAKE_SPEED_MARKED_MAN_HI = 0.0f; // flt_8300D714 -- FLAG: rodata value unrecovered
const f32 KF_MAX_OVERTAKE_SPEED_MARKED_MAN_LO = 0.0f; // flt_8300D6F0 -- FLAG: rodata value unrecovered
const f32 KF_MAX_OVERTAKE_SPEED_DEFAULT_HI    = 0.0f; // flt_8300D7A0 -- FLAG: rodata value unrecovered
const f32 KF_MAX_OVERTAKE_SPEED_DEFAULT_LO    = 0.0f; // flt_8300D970 -- FLAG: rodata value unrecovered

// Shared "speed match disabled / no passing" speed (returned when there is no player car or
// the AI car is not in range). Appears in GetSpeedMatchSpeed, SetSlowOvertakingSpeed and
// SetSlowFallbackSpeed.
const f32 KF_NO_PASSING_SPEED = 0.0f; // flt_8300D6F4 -- FLAG: rodata value unrecovered

// GetSpeedMatchSpeed -- OVERTAKE_FAST (case 4).
const f32 KF_OVERTAKE_FAST_MIN_SPEED   = 0.0f; // flt_8300D71C -- FLAG: rodata value unrecovered (shared w/ SetSlowOvertakingSpeed)
const f32 KF_OVERTAKE_FAST_SPEED_BIAS  = 0.0f; // flt_8300DBE0 -- FLAG: rodata value unrecovered

// GetSpeedMatchSpeed -- SLOW_TO_CLIP (case 3).
const f32 KF_SLOW_TO_CLIP_SPEED_DROP   = 0.0f; // flt_8300D7F0 -- FLAG: rodata value unrecovered
const f32 KF_SLOW_TO_CLIP_FALLBACK     = 0.0f; // flt_8300D754 -- FLAG: rodata value unrecovered

// GetSpeedMatchSpeed -- SLOWER (case 2).
const f32 KF_SLOWER_BEHIND_SPEED       = 0.0f; // flt_8300D784 -- FLAG: rodata value unrecovered

// GetSpeedMatchSpeed -- default (aggressive fall-back) branch.
const f32 KF_AGG_FALLBACK_POS_MAGNITUDE = 0.0f; // flt_8300D830 -- FLAG: rodata value unrecovered (used when curve >= 0)
const f32 KF_AGG_FALLBACK_RELSPEED_HI   = 0.0f; // flt_8300DB08 -- FLAG: rodata value unrecovered (relative-speed lerp hi)
const f32 KF_AGG_FALLBACK_RELSPEED_LO   = 0.0f; // flt_8300D72C -- FLAG: rodata value unrecovered (relative-speed lerp lo)
const f32 KF_AGG_FALLBACK_BASE_MARKED   = 0.0f; // flt_8300D6E8 -- FLAG: rodata value unrecovered (marked-man base offset)
const f32 KF_AGG_FALLBACK_BASE_DEFAULT  = 0.0f; // flt_8300D7D0 -- FLAG: rodata value unrecovered (default base offset)

// SetSlowOvertakingSpeed.
const f32 KF_SLOW_OVERTAKE_SPEED_BIAS  = 0.0f; // flt_8300D758 -- FLAG: rodata value unrecovered
const f32 KF_SLOW_OVERTAKE_CAP_BIAS    = 0.0f; // flt_8300D778 -- FLAG: rodata value unrecovered

// SetSlowFallbackSpeed -- relative-speed lerp endpoints (lerp(lo, hi, relSpeedForMatch)).
const f32 KF_SLOW_FALLBACK_RELSPEED_HI = 0.0f; // flt_8300D7D8 -- FLAG: rodata value unrecovered
const f32 KF_SLOW_FALLBACK_RELSPEED_LO = 0.0f; // flt_8300D728 -- FLAG: rodata value unrecovered

// Shared speed-match RANGE tuning quad (proximity-lerped). Same four rodata floats are used by
// OutOfSpeedMatchRange and the GetSpeedMatchSpeed default branch.
//   lerp(*_PROX0, *_PROX1, mpCar->GetProximityToSpeedMatch())
const f32 KF_SPEED_MATCH_LEAD_PROX1 = 0.0f; // unk_820C42E4 -- FLAG: rodata value unrecovered (leading-sep upper, prox=1)
const f32 KF_SPEED_MATCH_LEAD_PROX0 = 0.0f; // unk_820C42E8 -- FLAG: rodata value unrecovered (leading-sep upper, prox=0)
const f32 KF_SPEED_MATCH_SEP_PROX1  = 0.0f; // unk_820C42EC -- FLAG: rodata value unrecovered (separation threshold, prox=1)
const f32 KF_SPEED_MATCH_SEP_PROX0  = 0.0f; // unk_820C42F0 -- FLAG: rodata value unrecovered (separation threshold, prox=0)

// --- G4-states-slam ---
// No NEW file-local constants required by these 7 functions -- every numeric used is a recoverable literal immediate visible in the asm/rodata (immediates: -1.0, 0.0, 1.0, 1.5, 2.0, 2.5, 4.0, 6.0, 7.5, -3.0, -8.0, 12.0, 16.0, 20.0, 1000.0; and the integer float-bit constants decoded: 1092616192=10.0f, 1086324736=6.0f, 1090519040=8.0f, 0x40000000=2.0f, -1082130432=-1.0f). The randomised state times use the static `mRandom` (CgsNumeric::Random, declared in BrnAIAggression.h) via mRandom.RandomFloat().

// --- G5-states-misc ---
// --- BrnAIAggression.cpp file-static const floats used by the G5 state handlers ---
// Values flagged UNKNOWN are rodata constants not recoverable from the available
// pseudocode/asm; they are named here per AGENTS.md rule 5 and must be back-filled when
// the rodata is dumped. Literal immediates visible in the pseudocode (2.0/3.0/8.0/10.0/
// 12.0/20.0/30.0/5.0/130.0/1.0) are used inline in the bodies, not listed here.

const f32 KF_FALL_PAST_SPURT_MIN_TIME = 0.0f;   // FLAG: rodata value unrecovered -- fsel floor (f31) for the re-rolled SPURT_FORWARD time in FallPast
const f32 KF_FALL_PAST_TIME_LERP_LO   = 0.0f;   // FLAG: rodata value unrecovered -- low endpoint of the BE_FODDER state-time lerp in FallPast (&unk_820C4288)
const f32 KF_FALL_PAST_TIME_LERP_HI   = 0.0f;   // FLAG: rodata value unrecovered -- high endpoint of the BE_FODDER state-time lerp in FallPast (&unk_820C428C)
const f32 KF_CLIP_OFF_MIN_SPEED       = 0.0f;   // FLAG: rodata value unrecovered -- min target speed before CLIP_OFF_BEHIND bails (flt_8300D720)
const f32 KF_SPURT_PASSING_SPEED_SCALE = 0.0f;  // FLAG: rodata value unrecovered -- multiplier (x130.0) for SpurtForward mFixedPassingSpeed (flt_82F31928)

    // BrnAI::AIAggression::GetTargetPos @0x827656D0. Returns the cached world-space target
    // position by value; asserts mbTargetPosValid, then copies mTargetPos into the (ABI-hidden)
    // return slot (the X360 build does a single 16-byte lvx128/stvx128 move from this+0x30).
    // Called by AIDriver::GenerateRacingLine. (Belongs to the class:BrnAI::AIAggression TU; kept
    // here so the whole AIAggression body set lives in one file.)
    Vector3 AIAggression::GetTargetPos()
    {
        CGS_ASSERT(mbTargetPosValid, "mbTargetPosValid");
        return mTargetPos;
    }

// CurveToKeepLarge @0x827669C0.
//
// Shapes a signed [-1,1] distance scale through a quadratic ease that keeps magnitudes
// large near the centre: negative inputs map via (x+1)^2-1, positive inputs via the
// odd-symmetric -((1-x)^2-1). Endpoints map to +/-1, zero maps to zero.
f32 CurveToKeepLarge(f32 lfInput)
{
    CGS_ASSERT(lfInput >= -1.0f, "lfDistScale >= -1.0f");
    CGS_ASSERT(lfInput <= 1.0f, "lfDistScale <= 1.0f");

    if ( lfInput <= 0.0f )
        return ((lfInput + 1.0f) * (lfInput + 1.0f)) - 1.0f;
    else
        return -(((1.0f - lfInput) * (1.0f - lfInput)) - 1.0f);
}

// BrnAI::AIAggression::AcrossSeparationTooBig @0x8277DE38.
//
// True when the target is laterally too far to line up a slam: either the straight-line
// separation already exceeds 20 units, or (when close enough) the across-track component
// of that separation exceeds 20 units.
bool AIAggression::AcrossSeparationTooBig(const AICar* lpThisCar, const AICar* lpOtherCar)
{
    if ( GetSeparation(lpThisCar, lpOtherCar) <= 20.0f )
        return GetAcrossSeparation(mpCar, lpOtherCar) > 20.0f;

    return true;
}

// BrnAI::AIAggression::CalcSeparationAcrossToTarget @0x82771248.
//
// Lateral (across-track) separation between mpCar and mpTargetCar: the ground-plane offset
// from the target to our car projected onto our car's normalised right axis. Sign indicates
// which side the target sits on.
//
// X360 ASM: diff = pos(mpCar) - pos(mpTargetCar) with its Y lane explicitly zeroed (flatten
// to XZ); right = mpCar->GetRight(). A vcmpgtfp epsilon guard on |right| skips the projection
// when the right vector is degenerate (~zero); otherwise vrsqrtefp + two Newton steps
// normalise right and vmsum3fp128 dots the flattened diff with the unit right axis.
// De-SIMD'd to a guarded XZ dot.
f32 AIAggression::CalcSeparationAcrossToTarget()
{
    CGS_ASSERT(mpTargetCar != NULL, "mpTargetCar != NULL");
    CGS_ASSERT(mpCar != NULL, "mpCar != NULL");

    // Ground-plane offset from the target car to our car.
    Vector3 lvAcross = mpCar->GetPosition() - mpTargetCar->GetPosition();
    lvAcross.y = 0.0f;

    // Our car's right axis; the X360 guards against a degenerate (near-zero) right vector
    // before normalising it -- if it is effectively zero, there is no across component.
    const Vector3 lvRight = mpCar->GetRight();
    if (rw::math::vpu::MagnitudeSquared(lvRight) <= KF_QUERY_POS_EPSILON)
    {
        return lvAcross.x;
    }

    return rw::math::vpu::Dot(lvAcross, rw::math::vpu::Normalize(lvRight));
}

// ===== CalcSpeedMatchSpeed =====
// BrnAI::AIAggression::CalcSpeedMatchSpeed @0x8278B7A8.
//
// Drives the AI car's speed one frame toward its desired speed-match speed. Caches the
// raw (non-speed-matched) target speed in mfNonSpeedMatchedSpeed, asks GetSpeedMatchSpeed
// for the desired speed this frame, then StepTo()s the car's current speed toward it,
// capping the per-frame change by an acceleration rate. E_ROUTE_FINDING_ROAD_RAGE / E_ROUTE_FINDING_MARKED_MAN use a flat
// 20.0 cap; every other style scales the car's tuned acceleration-rate knob (15*rate + 5).
//
// Register/param binding (authoritative DWARF signature is (lfTimeStep, lfTargetSpeed)):
// the X360 build stores a2 (== first param == lfTimeStep) into mfNonSpeedMatchedSpeed, and
// uses a3 (== second param == lfTargetSpeed) both as the GetSpeedMatchSpeed argument and as
// the accel-time multiplier. Bodied faithfully to that binding (do NOT swap to read nicer).
f32 AIAggression::CalcSpeedMatchSpeed(f32 lfTimeStep, f32 lfTargetSpeed)
{
    mfNonSpeedMatchedSpeed = lfTimeStep;

    const f32 lfDesiredSpeed = GetSpeedMatchSpeed(lfTargetSpeed);

    f32 lfMaxAccel;
    if (mpCar->meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE ||
        mpCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)   // == 2 || == 6
    {
        lfMaxAccel = 20.0f;
    }
    else
    {
        lfMaxAccel = mpCar->GetAggressiveness()->GetAcclerationRateForSpeedMatch() * 15.0f + 5.0f;
    }

    const f32 lfMaxStep = lfMaxAccel * lfTargetSpeed;
    return StepTo(mpCar->GetSpeed(), lfDesiredSpeed, lfMaxStep);
}

// BrnAI::AIAggression::CanSlam @0x8277DFC8.
//
// Decides whether the AI is well enough lined up on its target to attempt a slam. Requires:
//   - the target leads us by a small along-track band (-3 < leadingSep < 2),
//   - our car has a valid proximity index (> 0),
//   - the across-track separation is under 30 units.
//
// X360: leadingSep = GetLeadingSeparation(mpCar, mpTargetCar) (computed first, its result held
// in fp1 across the early-outs); the proximity gate reads mpCar->miProximityIndex.
bool AIAggression::CanSlam()
{
    const f32 lfLeadingSeparation = GetLeadingSeparation(mpCar, mpTargetCar);

    if (mpCar->miProximityIndex <= 0)
    {
        return false;
    }
    if (lfLeadingSeparation <= -3.0f)
    {
        return false;
    }
    if (lfLeadingSeparation >= 2.0f)
    {
        return false;
    }

    if (CalcSeparationAcrossToTarget() >= 30.0f)
    {
        return false;
    }
    return true;
}

// BrnAI::AIAggression::CarIsTooSlow @0x82766948.
//
// True when the candidate car's speed is below the minimum required to be worth targeting.
bool AIAggression::CarIsTooSlow(const AICar* lpCar)
{
    CGS_ASSERT(lpCar != nullptr, "lpCar");
    return lpCar->GetSpeed() < KF_CAR_TOO_SLOW_SPEED;
}

// BrnAI::AIAggression::CheckForCarVeeringAwayFromPlayer @0x827709C0.
//
// Tracks how long this car has been continuously touching the player and, on contact,
// kicks it into a veer: a short touch (0.2s..1.0s) triggers a normal VEER, a long touch
// (>1.0s) an extreme veer. Each frame the recent-hit timer ages down; while it is
// positive the continuous-contact timer accumulates, and resets to zero once it lapses.
void AIAggression::CheckForCarVeeringAwayFromPlayer(f32 lfTimeStep)
{
    if ( mpCar->mbIsTouchingPlayer )
        mfRecentHitTimer = 0.25f;

    if ( mfRecentHitTimer > 0.0f )
    {
        const f32 lfRemaining = mfRecentHitTimer - lfTimeStep;
        mfRecentHitTimer = lfRemaining;
        if ( lfRemaining > 0.0f )
            mfContinuousContactTimer += lfTimeStep;
        else
            mfContinuousContactTimer = 0.0f;
    }

    if ( mpCar->mbIsTouchingPlayer )
    {
        const f32 lfContact = mfContinuousContactTimer;
        if ( lfContact <= 1.0f )
        {
            if ( lfContact > 0.2f )
            {
                mfStateTime = 1.0f;
                meAggressionState = E_AI_AGGRESSION_STATE_VEER;
            }
        }
        else
        {
            mfStateTime = 1.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_VEER_EXTREME;
        }
    }
}

// BrnAI::AIAggression::DetermineAttackSide @0x82771408.
//
// Returns the signed projection of (pos(lpCarB) - pos(lpCarA)) onto lpCarB's right axis.
// The caller (GetPositionNextToTarget) only tests the sign: > 0 means lpCarA is on lpCarB's
// left, so the offset is flipped to approach from that side.
//
// X360 ASM: GetPosition(lpCarA)=v127, GetPosition(lpCarB)=v126, right=lpCarB->GetRight()=v12;
// vsubfp128 v13 = v126 - v127; vmsum3fp128(v13, v12) = full 3D dot; the trailing vcmpgtfp vs
// a splat-0 is just the caller's >0 test materialised. De-SIMD'd to a plain Dot.
f32 AIAggression::DetermineAttackSide(const AICar* lpCarA, const AICar* lpCarB)
{
    const Vector3 lvDelta = lpCarB->GetPosition() - lpCarA->GetPosition();
    return rw::math::vpu::Dot(lvDelta, lpCarB->GetRight());
}

// ===== FindTarget =====
// BrnAI::AIAggression::FindTarget @0x82793C60.
//
// Validates a candidate target car and, if accepted, records it as mpTargetCar and caches the
// along-track separation. Rejects: null candidate; the candidate being our own car; a
// human-player car that is not actually driven by the player; anything out of speed-match
// range; and (unless we are in E_ROUTE_FINDING_ROAD_RAGE / E_ROUTE_FINDING_MARKED_MAN route-finding) a candidate that is too
// slow. Returns true when a target was set.
//
// X360: the speed-match gate is OutOfSpeedMatchRange(mpCar, lpCandidateTarget) -- the asm
// shows OutOfSpeedMatchRange(a1) with args dropped, but the callee tests its second car arg
// (a3) and sibling call sites pass (mpCar, carToTest); a NULL candidate would early-out as
// out-of-range, so the candidate MUST be threaded through.
bool AIAggression::FindTarget(const AICar* lpCandidateTarget)
{
    mpTargetCar = NULL;

    if (lpCandidateTarget == NULL ||
        lpCandidateTarget == mpCar ||
        (lpCandidateTarget->mbIsPlayer && !lpCandidateTarget->mbIsDrivenByPlayer) ||
        OutOfSpeedMatchRange(mpCar, lpCandidateTarget))
    {
        return false;
    }

    const ERouteFindingStyle leStyle = mpCar->meRouteFindingStyle;
    const bool lbAggressiveStyle = (leStyle == E_ROUTE_FINDING_ROAD_RAGE ||
                                    leStyle == E_ROUTE_FINDING_MARKED_MAN);

    if (!lbAggressiveStyle && CarIsTooSlow(lpCandidateTarget))
    {
        return false;
    }

    mpTargetCar = lpCandidateTarget;
    mfTargetSeparationAlong = GetLeadingSeparation(mpCar, lpCandidateTarget);
    return true;
}

// BrnAI::AIAggression::GetLeadingSeparation @0x8277DEA0 (const).
//
// Signed along-track separation: how far lpOtherCar is AHEAD of lpThisCar, measured by
// projecting the ground-plane (XZ) separation onto lpThisCar's normalised ground-plane
// forward direction. Positive => other car is in front.
//
// X360 ASM: diff = pos(lpOtherCar) - pos(lpThisCar), passed through BrnMath::Flatten (drop Y,
// keep XZ); dir = Flatten(lpThisCar->GetUsefulDirection()); the vrsqrtefp + two Newton steps
// normalise the flattened dir; the trailing vmulfp/vspltw/vaddfp chain is a 2-lane dot of the
// flattened diff with the normalised flattened dir. De-SIMD'd to a clean XZ dot of the
// separation against the unit forward vector.
f32 AIAggression::GetLeadingSeparation(const AICar* lpThisCar, const AICar* lpOtherCar) const
{
    CGS_ASSERT(lpThisCar != NULL, "lpThisCar != NULL");
    CGS_ASSERT(lpOtherCar != NULL, "lpOtherCar != NULL");

    // Ground-plane (XZ) separation between the two cars.
    Vector3 lvSeparation = lpOtherCar->GetPosition() - lpThisCar->GetPosition();
    lvSeparation.y = 0.0f;

    // Ground-plane forward direction of the reference car, normalised.
    Vector3 lvForward = lpThisCar->GetUsefulDirection();
    lvForward.y = 0.0f;
    lvForward = rw::math::vpu::Normalize(lvForward);

    return rw::math::vpu::Dot(lvSeparation, lvForward);
}

// BrnAI::AIAggression::GetMaxOvertakeSpeed @0x82771618.
//
// Returns the aggression-level-scaled maximum overtake speed. The X360 build reads the
// AI car's aggression level (mpCar->GetAggressiveness()->GetAggressionLevel()) and
// linearly interpolates between a (lo, hi) pair of rodata constants -- one pair for the
// E_ROUTE_FINDING_MARKED_MAN route-finding style, another for everything else. The VMX vsubfp/vmaddfp
// chain is the standard lerp lo + (hi - lo) * t. Endpoint values are unrecovered -> KF_.
f32 AIAggression::GetMaxOvertakeSpeed() const
{
    const f32 lfAggressionLevel = mpCar->GetAggressiveness()->GetAggressionLevel();

    f32 lfHi;
    f32 lfLo;
    if (mpCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)   // == 6
    {
        lfHi = KF_MAX_OVERTAKE_SPEED_MARKED_MAN_HI;
        lfLo = KF_MAX_OVERTAKE_SPEED_MARKED_MAN_LO;
    }
    else
    {
        lfHi = KF_MAX_OVERTAKE_SPEED_DEFAULT_HI;
        lfLo = KF_MAX_OVERTAKE_SPEED_DEFAULT_LO;
    }

    return lfLo + (lfHi - lfLo) * lfAggressionLevel;
}

// BrnAI::AIAggression::GetMinFallBackSpeed @0x82770A68.
//
// Returns the per-route-finding-style minimum speed the AI car is allowed to fall back
// to while speed-matching. The X360 build switches on mpCar->meRouteFindingStyle and
// returns one of four rodata-tuned floats (E_ROUTE_FINDING_PURSUIT / E_ROUTE_FINDING_ROAD_RAGE / E_ROUTE_FINDING_MARKED_MAN, else a
// default). The rodata values are not recoverable from the dossier -> KF_ constants.
f32 AIAggression::GetMinFallBackSpeed()
{
    switch (mpCar->meRouteFindingStyle)
    {
        case E_ROUTE_FINDING_PURSUIT:    return KF_MIN_FALLBACK_SPEED_PURSUIT;     // 3
        case E_ROUTE_FINDING_ROAD_RAGE:  return KF_MIN_FALLBACK_SPEED_ROAD_RAGE;   // 2
        case E_ROUTE_FINDING_MARKED_MAN: return KF_MIN_FALLBACK_SPEED_MARKED_MAN;  // 6
        default:         return KF_MIN_FALLBACK_SPEED_DEFAULT;
    }
}

// ===== GetPositionNextToTarget =====
// BrnAI::AIAggression::GetPositionNextToTarget @0x827714E8.
//
// Computes a world position alongside lpCarA, offset by lfOffset along lpCarA's right axis.
// The offset sign is chosen by DetermineAttackSide(lpCarA, lpCarB) so the AI lines up on the
// correct side of lpCarA relative to lpCarB before a slam.
//
// X360 ASM: if DetermineAttackSide(lpCarA, lpCarB) > 0 the offset is negated; then
// vmaddcfp128 computes pos(lpCarA) + right(lpCarA) * offset (the asm uses a3 == lpCarA for
// both GetPosition and GetRight; a4 == lpCarB is only consumed by DetermineAttackSide). A
// per-lane vcmpeqfp self-equality cascade is the inlined RwMath::IsValid NaN guard (folded
// into one assert here). De-SIMD'd to scalar Vector3 math.
Vector3 AIAggression::GetPositionNextToTarget(const AICar* lpCarA, const AICar* lpCarB, f32 lfOffset)
{
    f32 lfSignedOffset = lfOffset;
    if (DetermineAttackSide(lpCarA, lpCarB) > 0.0f)
    {
        lfSignedOffset = -lfSignedOffset;
    }

    const Vector3 lvTargetPosition = lpCarA->GetPosition() + (lpCarA->GetRight() * lfSignedOffset);

    CGS_ASSERT(rw::math::vpu::IsValid(lvTargetPosition), "RwMath::IsValid( lTargetPosition )");
    return lvTargetPosition;
}

// BrnAI::AIAggression::GetSeparation @0x82770F28.
//
// Returns the straight-line 3D distance between two cars: |pos(lpOtherCar) - pos(lpThisCar)|.
// X360 ASM: vsubfp128 of the two GetPosition() results, vmsum3fp128(diff,diff) = mag^2,
// then vrsqrtefp + two Newton-Raphson refinement steps build 1/mag and vmulfp recovers the
// magnitude (mag^2 * 1/mag); the vcmpeqfp/vsel pair selects 0 when mag^2 is exactly 0 to
// avoid the rsqrt(0) NaN. De-SIMD'd here to a single vpu::Magnitude (exact std::sqrt), which
// is the same value with the zero case handled implicitly.
f32 AIAggression::GetSeparation(const AICar* lpThisCar, const AICar* lpOtherCar)
{
    CGS_ASSERT(lpThisCar != NULL, "lpThisCar != NULL");
    CGS_ASSERT(lpOtherCar != NULL, "lpOtherCar != NULL");

    const Vector3 lvDelta = lpOtherCar->GetPosition() - lpThisCar->GetPosition();
    return rw::math::vpu::Magnitude(lvDelta);
}

// ===== GetSpeedMatchSpeed =====
// BrnAI::AIAggression::GetSpeedMatchSpeed @0x8277E058.
//
// Computes the AI car's desired speed for the current speed-match mode (meSpeedMatchType).
// Each mode produces a target speed, several StepTo()-toward a player/target-derived speed.
// The default (aggressive fall-back) branch shapes the along/ahead error through
// CurveToKeepLarge, scales it, then derives a fall-back speed clamped to
// [GetMinFallBackSpeed, (E_ROUTE_FINDING_MARKED_MAN only) GetMaxOvertakeSpeed]. lfTimeStep scales the
// StepTo step in the per-frame stepping modes. All VMX lerps de-SIMD'd to lo + (hi-lo)*t.
//
// NOTE: the ahead (error >= 0) span lerps the LEAD rodata pair (unk_820C42E4/E8); the
// behind (error < 0) span lerps the SEP pair (unk_820C42EC/F0) -- they are different quads.
f32 AIAggression::GetSpeedMatchSpeed(f32 lfTimeStep)
{
    switch (meSpeedMatchType)
    {
        case ESpeedMatch_OvertakeSlowly:    // 5
            return mFixedPassingSpeed;

        case ESpeedMatch_OvertakeFast:      // 4
            if (mpPlayerCar != NULL && mpCar->meCarState == E_AI_CAR_STATE_IN_RANGE)
            {
                const f32 lfMaxOvertake = GetMaxOvertakeSpeed();
                const f32 lfRaw = mpPlayerCar->GetSpeed() + KF_OVERTAKE_FAST_SPEED_BIAS;
                f32 lfSpeed = (lfRaw < KF_OVERTAKE_FAST_MIN_SPEED) ? KF_OVERTAKE_FAST_MIN_SPEED : lfRaw;
                if (lfSpeed > lfMaxOvertake)
                    lfSpeed = lfMaxOvertake;
                return lfSpeed;
            }
            return KF_NO_PASSING_SPEED;

        case ESpeedMatch_SlowToClip:        // 3
            if (mpPlayerCar != NULL && GetLeadingSeparation(mpCar, mpTargetCar) >= -15.0f)
            {
                const f32 lfTarget = mpPlayerCar->GetSpeed() - KF_SLOW_TO_CLIP_SPEED_DROP;
                return StepTo(mpCar->GetSpeed(), lfTarget, lfTimeStep * 10.0f);
            }
            return KF_SLOW_TO_CLIP_FALLBACK;

        case ESpeedMatch_Slower:            // 2
        {
            if (mpPlayerCar == NULL || mpCar->meCarState != E_AI_CAR_STATE_IN_RANGE)
                return KF_NO_PASSING_SPEED;

            f32 lfTarget;
            if (GetLeadingSeparation(mpCar, mpTargetCar) >= 0.0f)
                lfTarget = mFixedPassingSpeed;
            else
                lfTarget = KF_SLOWER_BEHIND_SPEED;

            return StepTo(mpCar->GetSpeed(), lfTarget, lfTimeStep * 90.0f);
        }

        default:
            // ESpeedMatch_Disabled (0) / Enabled (1) / Count (6): the aggressive fall-back path.
            if (meSpeedMatchType != ESpeedMatch_Disabled && mpTargetCar != NULL)
            {
                CGS_ASSERT(mpCar != NULL, "mpCar != NULL");

                const f32 lfProximity = mpCar->GetAggressiveness()->GetProximityToSpeedMatch();
                const f32 lfError = mfRelativePositionAhead + mfTargetSeparationAlong;

                // Clamp the normalised error into [0.2, 1.0] (ahead) or [-1.0, -0.2] (behind),
                // normalising by a proximity-lerped span, then shape it through CurveToKeepLarge.
                // Ahead uses the LEAD quad; behind uses the SEP quad (distinct rodata pairs).
                f32 lfClamped;
                if (lfError >= 0.0f)
                {
                    const f32 lfSpan = KF_SPEED_MATCH_LEAD_PROX0 +
                                       (KF_SPEED_MATCH_LEAD_PROX1 - KF_SPEED_MATCH_LEAD_PROX0) * lfProximity;
                    f32 lfRatio = lfError / lfSpan;
                    if (lfRatio < 0.2f) lfRatio = 0.2f;
                    if (lfRatio > 1.0f) lfRatio = 1.0f;
                    lfClamped = lfRatio;
                }
                else
                {
                    const f32 lfSpan = KF_SPEED_MATCH_SEP_PROX0 +
                                       (KF_SPEED_MATCH_SEP_PROX1 - KF_SPEED_MATCH_SEP_PROX0) * lfProximity;
                    f32 lfRatio = lfError / lfSpan;
                    if (lfRatio < -1.0f) lfRatio = -1.0f;
                    if (lfRatio > -0.2f) lfRatio = -0.2f;
                    lfClamped = lfRatio;
                }

                const f32 lfCurve = CurveToKeepLarge(lfClamped);

                f32 lfMagnitude;
                if (lfCurve >= 0.0f)
                {
                    lfMagnitude = KF_AGG_FALLBACK_POS_MAGNITUDE;
                }
                else
                {
                    const f32 lfRelSpeed = mpCar->GetAggressiveness()->GetRelativeSpeedForMatch();
                    lfMagnitude = KF_AGG_FALLBACK_RELSPEED_LO +
                                  (KF_AGG_FALLBACK_RELSPEED_HI - KF_AGG_FALLBACK_RELSPEED_LO) * lfRelSpeed;
                }

                f32 lfOffset = lfMagnitude * lfCurve;
                if ((lfOffset < 0.0f ? -lfOffset : lfOffset) < 1.0f)
                    lfOffset = (lfOffset <= 0.0f) ? -1.0f : 1.0f;

                const f32 lfBase = (mpCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)
                                       ? KF_AGG_FALLBACK_BASE_MARKED
                                       : KF_AGG_FALLBACK_BASE_DEFAULT;

                const f32 lfRaw = mpTargetCar->GetSpeed() + lfOffset;
                f32 lfFallBackSpeed = (lfRaw < lfBase) ? lfBase : lfRaw;

                const f32 lfMinFallBack = GetMinFallBackSpeed();
                if (lfFallBackSpeed < lfMinFallBack)
                    lfFallBackSpeed = lfMinFallBack;

                if (mpCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)
                {
                    const f32 lfMaxOvertake = GetMaxOvertakeSpeed();
                    if (lfFallBackSpeed > lfMaxOvertake)
                        return lfMaxOvertake;
                }
                return lfFallBackSpeed;
            }
            return mfNonSpeedMatchedSpeed;
    }
}

// BrnAI::AIAggression::NotSuitableForAggression @0x827668E8.
//
// True when this car must not behave aggressively: it is in a non-default car-state, it
// is the (human) player, or it is sitting on the start line.
bool AIAggression::NotSuitableForAggression()
{
    return mpCar->meCarState != 0
        || mpCar->mbIsPlayer
        || mpCar->IsOnStartLine();
}

// BrnAI::AIAggression::OutOfSpeedMatchRange @0x8278B680.
//
// True when the candidate car (lpCarB) is too far from the AI car (mpCar) to be worth
// speed-matching. Tests the lateral/total separation and the leading (along-direction)
// separation against proximity-scaled thresholds. The threshold spans are lerped over the
// shared speed-match RANGE tuning quad by mpCar's proximity-to-speed-match knob; the VMX
// lerps are de-SIMD'd to lo + (hi-lo)*t.
//
// NOTE: the X360 asm references mpCar and lpCarB only -- lpCarA is in the DWARF signature
// but unused here (it equals the AI car). See flags.
bool AIAggression::OutOfSpeedMatchRange(const AICar* lpCarA, const AICar* lpCarB)
{
    (void)lpCarA;

    if (lpCarB == NULL)
        return true;

    const f32 lfProximity = mpCar->GetAggressiveness()->GetProximityToSpeedMatch();
    const f32 lfSepThreshold = KF_SPEED_MATCH_SEP_PROX0 +
                               (KF_SPEED_MATCH_SEP_PROX1 - KF_SPEED_MATCH_SEP_PROX0) * lfProximity;

    const f32 lfSeparation = GetSeparation(mpCar, lpCarB);
    if (lfSeparation > lfSepThreshold)
        return true;

    const f32 lfLeadingSeparation = GetLeadingSeparation(mpCar, lpCarB);
    const f32 lfLeadUpper = KF_SPEED_MATCH_LEAD_PROX0 +
                            (KF_SPEED_MATCH_LEAD_PROX1 - KF_SPEED_MATCH_LEAD_PROX0) * lfProximity;

    if (lfLeadingSeparation < -lfSepThreshold || lfLeadingSeparation > lfLeadUpper)
        return true;

    return false;
}

// BrnAI::AIAggression::SetSlowFallbackSpeed @0x82770AB8.
//
// Sets mFixedPassingSpeed for a slow fall-back behind the player: the player's speed minus
// a relative-speed-scaled drop, floored at GetMinFallBackSpeed(). Falls back to the
// no-passing speed when there is no player car or the AI car is out of range. The VMX chain
// is the lerp drop = lo + (hi-lo)*relSpeed; endpoints are rodata -> KF_.
//
// NOTE: the X360 GetSpeed() call has its argument elided by Hex-Rays; it reads the player
// car (the car being fallen back from) -- see flags.
void AIAggression::SetSlowFallbackSpeed()
{
    if (mpPlayerCar == NULL || mpCar->meCarState != E_AI_CAR_STATE_IN_RANGE)
    {
        mFixedPassingSpeed = KF_NO_PASSING_SPEED;
        return;
    }

    const f32 lfRelSpeed = mpCar->GetAggressiveness()->GetRelativeSpeedForMatch();
    const f32 lfDrop = KF_SLOW_FALLBACK_RELSPEED_LO +
                       (KF_SLOW_FALLBACK_RELSPEED_HI - KF_SLOW_FALLBACK_RELSPEED_LO) * lfRelSpeed;

    mFixedPassingSpeed = mpPlayerCar->GetSpeed() - lfDrop;

    const f32 lfMinFallBack = GetMinFallBackSpeed();
    if (mFixedPassingSpeed < lfMinFallBack)
        mFixedPassingSpeed = lfMinFallBack;
}

// BrnAI::AIAggression::SetSlowOvertakingSpeed @0x8277DB38.
//
// Sets mFixedPassingSpeed for a slow overtake: the player's speed plus a small bias,
// floored at a minimum overtake speed and ceilinged at GetMaxOvertakeSpeed(), then further
// capped so it never exceeds the cached non-speed-matched speed (plus a bias). Falls back
// to the no-passing speed when there is no player car or the AI car is out of range. The
// two fsel ops are the floor/ceiling clamps. Bias/floor constants are rodata -> KF_.
void AIAggression::SetSlowOvertakingSpeed()
{
    if (mpPlayerCar == NULL || mpCar->meCarState != E_AI_CAR_STATE_IN_RANGE)
    {
        mFixedPassingSpeed = KF_NO_PASSING_SPEED;
        return;
    }

    const f32 lfMaxOvertake = GetMaxOvertakeSpeed();
    const f32 lfCap = mfNonSpeedMatchedSpeed + KF_SLOW_OVERTAKE_CAP_BIAS;
    const f32 lfRaw = mpPlayerCar->GetSpeed() + KF_SLOW_OVERTAKE_SPEED_BIAS;

    f32 lfSpeed = (lfRaw < KF_OVERTAKE_FAST_MIN_SPEED) ? KF_OVERTAKE_FAST_MIN_SPEED : lfRaw;
    if (lfSpeed > lfMaxOvertake)
        lfSpeed = lfMaxOvertake;

    mFixedPassingSpeed = lfSpeed;
    if (lfSpeed > lfCap)
        mFixedPassingSpeed = lfCap;
}

// ===== StopAttacking =====
// BrnAI::AIAggression::StopAttacking @0x82793D48.
//
// Disengages from an attack. With STEERAWAY it simply veers off; otherwise it computes a
// post-attack wait time (min..max lerped by this car's aggression level), and -- for
// ATTACKAGAIN with a very short wait and a non-crashing target -- re-arms an immediate
// overtake-to-slam instead. Either way it clears the slam bookkeeping and re-seeds the
// leading separation while a target is held.
void AIAggression::StopAttacking(EStopAttack leStopAttack)
{
    if ( leStopAttack != E_AGGRESSION_STEERAWAY )
    {
        const f32 lfAggressionLevel = mpCar->GetAggressiveness()->GetAggressionLevel();
        const f32 lfWaitTime = KF_MIN_POST_ATTACK_WAIT_TIME
            + lfAggressionLevel * (KF_MAX_POST_ATTACK_WAIT_TIME - KF_MIN_POST_ATTACK_WAIT_TIME);

        CGS_ASSERT(lfWaitTime >= 0.0f, "lfWaitTime >= 0.0f");

        if ( leStopAttack == E_AGGRESSION_ATTACKAGAIN && lfWaitTime < 0.1f )
        {
            if ( mpTargetCar != nullptr && !mpTargetCar->mbIsCrashing )
            {
                SetSlowOvertakingSpeed();
                mfStateTime = 1.0f;
                meAggressionState = E_AI_AGGRESSION_STATE_OVERTAKE_TO_SLAM;
                return;
            }
        }

        meAggressionState = E_AI_AGGRESSION_STATE_WAIT;
        mfStateTime = (lfWaitTime < 0.0f) ? -1.0f : lfWaitTime;
        mbTargetPosValid = false;
        mpTargetCar = nullptr;
    }
    else
    {
        mfStateTime = 1.5f;
        meAggressionState = E_AI_AGGRESSION_STATE_VEER;
    }

    mfLastSpeed = -1.0f;
    mfLastSpeedTarget = -1.0f;
    mpPrevSpeedMatchTarget = nullptr;
    meSpeedMatchType = ESpeedMatch_Disabled;
    if ( mpTargetCar != nullptr )
        mfTargetSeparationAlong = GetLeadingSeparation(mpCar, mpTargetCar);
}

// ===== Update =====
// BrnAI::AIAggression::Update @0x82799A98.
//
// Per-frame driver for the rival-aggression state machine. Skips entirely if our own
// car is the (human) player. Otherwise: lets the veer-away check run, latches the player
// car, bails if not suitable for aggression, ages the state timer, then -- if our car is
// crashing -- snaps into the VEER state; else, when racing within 1000 units of the
// checkpoint it resets to OUT_OF_RANGE, and finally dispatches to the current state's
// per-state update handler.
void AIAggression::Update(f32 lfTimeStep, const AICar* lpPlayerCar)
{
    if ( mpCar->mbIsPlayer )
        return;

    CheckForCarVeeringAwayFromPlayer(lfTimeStep);
    mpPlayerCar = lpPlayerCar;

    if ( NotSuitableForAggression() )
        return;

    if ( mfStateTime >= 0.0f )
        mfStateTime -= lfTimeStep;

    const bool lbHaveTarget = (mpTargetCar != nullptr);

    if ( mpCar->mbIsCrashing )
    {
        // Our car is crashing -- veer out of the way and forget any slam plan.
        mfStateTime = 1.5f;
        mpPrevSpeedMatchTarget = nullptr;
        meAggressionState = E_AI_AGGRESSION_STATE_VEER;
        meSpeedMatchType = ESpeedMatch_Disabled;
        mfLastSpeed = -1.0f;
        mfLastSpeedTarget = -1.0f;
        if ( lbHaveTarget )
            mfTargetSeparationAlong = GetLeadingSeparation(mpCar, mpTargetCar);
        return;
    }

    if ( lbHaveTarget )
        mfTargetSeparationAlong = GetLeadingSeparation(mpCar, mpTargetCar);

    // While racing and close to a checkpoint, drop straight back to OUT_OF_RANGE.
    EAIAggressionState leState = meAggressionState;
    if ( mpCar->meRouteFindingStyle == E_ROUTE_FINDING_RACE
         && mpCar->mfDistanceToCheckpoint < 1000.0f )
    {
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
        mfStateTime = -1.0f;
        leState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
    }

    switch ( leState )
    {
        case E_AI_AGGRESSION_STATE_OUT_OF_RANGE:
            UpdateAggressionStateOutOfRange(mpPlayerCar);
            break;
        case E_AI_AGGRESSION_STATE_OVERTAKE_TO_SLAM:
            UpdateAggressionStateOvertakeToSlam(mpPlayerCar, lfTimeStep);
            break;
        case E_AI_AGGRESSION_STATE_DROP_BACK_TO_SLAM:
            UpdateAggressionStateDropBackToSlam(mpPlayerCar, lfTimeStep);
            break;
        case E_AI_AGGRESSION_STATE_ATTACK_SLAM:
            UpdateAggressionStateAttackSlam();
            break;
        case E_AI_AGGRESSION_STATE_WAIT:
            UpdateAggressionStateWait();
            break;
        case E_AI_AGGRESSION_STATE_VEER:
            UpdateAggressionStateVeer();
            break;
        case E_AI_AGGRESSION_STATE_PASSIVE:
            UpdateAggressionPassive(mpPlayerCar);
            break;
        case E_AI_AGGRESSION_STATE_FALL_PAST:
            UpdateAggressionStateFallPast(mpPlayerCar);
            break;
        case E_AI_AGGRESSION_STATE_BE_FODDER:
            UpdateAggressionStateBeFodder();
            break;
        case E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND:
            UpdateAggressionStateClipOffBehind();
            break;
        case E_AI_AGGRESSION_STATE_OVERTAKE_FAST:
            UpdateAggressionStateOvertakeFast();
            break;
        case E_AI_AGGRESSION_STATE_OVERTAKE_SLOWLY:
            UpdateAggressionStateComeSlowFromBehind();
            break;
        case E_AI_AGGRESSION_STATE_SPURT_FORWARD:
            UpdateAggressionStateSpurtForward();
            break;
        case E_AI_AGGRESSION_STATE_VEER_EXTREME:
            UpdateAggressionStateVeerExtreme();
            break;
        case E_AI_AGGRESSION_STATE_HANG_AROUND_AHEAD:
            UpdateAggressionStateHangAboutAhead(mpPlayerCar);
            break;
        default:
            CGS_ASSERT(false, "Unknown aggression state");
            break;
    }
}

// ===== UpdateAggressionPassive =====
// BrnAI::AIAggression::UpdateAggressionPassive @0x82793830.
//
// PASSIVE state handler. Clears speed-matching and target-pos validity. If a valid
// non-self player car is engaged (and it is either not the local player or is a
// human-driven player), then once this state has timed out or speed-matching has dropped
// out of range, it resets the machine back to OUT_OF_RANGE.
void BrnAI::AIAggression::UpdateAggressionPassive(const AICar* lpPlayerCar)
{
    meSpeedMatchType = ESpeedMatch_Disabled;
    mbTargetPosValid = false;

    if (lpPlayerCar != nullptr &&
        lpPlayerCar != mpCar &&
        (!lpPlayerCar->mbIsPlayer || lpPlayerCar->mbIsDrivenByPlayer))
    {
        const f32 lfStateTime = mfStateTime;
        const bool lbTimedOut = (lfStateTime != -1.0f && lfStateTime <= 0.0f);

        if (lbTimedOut || OutOfSpeedMatchRange(mpCar, mpTargetCar))
        {
            mfStateTime       = -1.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
        }
    }
}

// BrnAI::AIAggression::UpdateAggressionStateAttackSlam @0x82793AE8.
//
// ATTACK_SLAM state handler. With a valid target it drives the slam: keeps speed-matching
// enabled with a -3 lead bias; on timeout reverts to OUT_OF_RANGE. While still in time, if
// the player is leading by enough (leading separation >= -3) it steers to a point past the
// target (mTargetPos via GetPositionNextToTarget, -8 offset). If speed-matching has dropped
// out of range it bails to OUT_OF_RANGE; if the across-separation is still too big it slows
// to overtaking speed and reverts to OVERTAKE_TO_SLAM.
void BrnAI::AIAggression::UpdateAggressionStateAttackSlam()
{
    if (mpTargetCar == nullptr)
    {
        mfStateTime       = -1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
        return;
    }

    const f32 lfStateTime   = mfStateTime;
    mfRelativePositionAhead = -3.0f;
    meSpeedMatchType        = ESpeedMatch_Enabled;

    if (lfStateTime == -1.0f || lfStateTime > 0.0f)
    {
        if (GetLeadingSeparation(mpPlayerCar, mpCar) >= -3.0f)
        {
            mTargetPos       = GetPositionNextToTarget(mpTargetCar, mpCar, -8.0f);
            mbTargetPosValid = true;

            if (OutOfSpeedMatchRange(mpCar, mpTargetCar))
            {
                mfStateTime       = -1.0f;
                meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
                return;
            }

            if (!AcrossSeparationTooBig(mpCar, mpTargetCar))
                return;
        }

        SetSlowOvertakingSpeed();
        meAggressionState = E_AI_AGGRESSION_STATE_OVERTAKE_TO_SLAM;
        mfStateTime       = 1.0f;
    }
    else
    {
        mfStateTime       = 1.5f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
    }
}

// BrnAI::AIAggression::UpdateAggressionStateBeFodder @0x8277DBF0.
//
// BE_FODDER handler: the AI offers itself as a takedown target. On state-timeout it
// either (E_ROUTE_FINDING_PURSUIT target) sets the slow-overtaking speed and transitions to
// OVERTAKE_SLOWLY (state 11, 8s), or otherwise drops to CLIP_OFF_BEHIND (state 9, 3s).
// Each frame it forces speed-match Enabled, a +2.0 relative-position-ahead bias, and
// clears the hanging-around timer.
void BrnAI::AIAggression::UpdateAggressionStateBeFodder()
{
    if (mfStateTime != -1.0f && mfStateTime <= 0.0f)
    {
        if (mpCar->meRouteFindingStyle == E_ROUTE_FINDING_PURSUIT)   // ==3
        {
            SetSlowOvertakingSpeed();
            mfStateTime = 8.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_OVERTAKE_SLOWLY;   // 11
        }
        else
        {
            mfStateTime = 3.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_CLIP_OFF_BEHIND;   // 9
        }
    }

    mfRelativePositionAhead = 2.0f;          // +0x5C
    meSpeedMatchType = ESpeedMatch_Enabled;  // +0x58 = 1
    mfHangingAroundTimer = 0.0f;             // +0x68
}

// BrnAI::AIAggression::UpdateAggressionStateClipOffBehind @0x82770B88.
//
// CLIP_OFF_BEHIND handler: shadow-clip the target from behind. With no valid target,
// or once the state times out, or if the target slows below KF_CLIP_OFF_MIN_SPEED, it
// resets to OUT_OF_RANGE. Otherwise it holds speed-match mode SlowToClip and clears the
// hanging-around timer.
void BrnAI::AIAggression::UpdateAggressionStateClipOffBehind()
{
    if (mfStateTime != -1.0f && mfStateTime <= 0.0f)
    {
        mfStateTime = -1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
    }

    if (mpTargetCar != NULL)
    {
        if (mpTargetCar->GetSpeed() < KF_CLIP_OFF_MIN_SPEED)
        {
            mfStateTime = -1.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
        }
        mfHangingAroundTimer = 0.0f;             // +0x68
        meSpeedMatchType = ESpeedMatch_SlowToClip;   // +0x58 = 3
    }
    else
    {
        mfStateTime = -1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
    }
}

// ===== UpdateAggressionStateDropBackToSlam =====
// BrnAI::AIAggression::UpdateAggressionStateDropBackToSlam @0x82796880.
//
// DROP_BACK_TO_SLAM state handler. With a valid target it: bails to WAIT if speed-matching
// dropped out of range; on state-timeout disengages (StopAttacking ATTACKAGAIN); if the
// target itself is now too slow it waits; otherwise it lines up just behind the target
// (mTargetPos via GetPositionNextToTarget, +7.5 offset) and, if a slam is on, escalates to
// ATTACK_SLAM. In E_ROUTE_FINDING_MARKED_MAN, if the target has out-run this car's decent speed it spurts
// forward to catch up.
void BrnAI::AIAggression::UpdateAggressionStateDropBackToSlam(const AICar* /*lpPlayerCar*/, f32 /*lfTimeStep*/)
{
    if (mpTargetCar == nullptr)
    {
        mfStateTime       = -1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
        return;
    }

    meSpeedMatchType        = ESpeedMatch_Enabled;
    mfRelativePositionAhead = -3.0f;

    if (OutOfSpeedMatchRange(mpCar, mpTargetCar))
    {
        mfStateTime       = 1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_WAIT;
        return;
    }

    if (mfStateTime != -1.0f && mfStateTime <= 0.0f)
    {
        StopAttacking(E_AGGRESSION_ATTACKAGAIN);
        return;
    }

    if (CarIsTooSlow(mpTargetCar))
    {
        mfStateTime       = 1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_WAIT;
        return;
    }

    mTargetPos       = GetPositionNextToTarget(mpTargetCar, mpCar, 7.5f);
    mbTargetPosValid = true;

    if (CanSlam())
    {
        mfStateTime       = 2.5f;
        meAggressionState = E_AI_AGGRESSION_STATE_ATTACK_SLAM;
    }

    if (mpCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)
    {
        const f32 lfTargetSpeed = mpTargetCar->GetSpeed();
        const f32 lfDecentSpeed = mpCar->GetDecentSpeed();
        if (lfTargetSpeed < lfDecentSpeed)
        {
            mfStateTime       = 10.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_SPURT_FORWARD;
        }
    }
}

// BrnAI::AIAggression::UpdateAggressionStateFallPast @0x82793568.
//
// FALL_PAST handler: the AI deliberately drops behind the player. Forces speed-match
// mode 2 (Slower), clears the boost flag, and -- for a E_ROUTE_FINDING_MARKED_MAN target going slower
// than its decent speed -- diverts to SPURT_FORWARD (state 12). Bails to OUT_OF_RANGE
// when the speed-match window is exceeded. On state-timeout it either re-rolls a short
// SPURT_FORWARD time (E_ROUTE_FINDING_ROAD_RAGE/E_ROUTE_FINDING_MARKED_MAN) via the shared mRandom draw, or resets to
// OUT_OF_RANGE. While close behind a non-player-driven player it can flip to
// CLIP_OFF_BEHIND geometry (state 8) and, for a separating road-rage/marked target,
// to HANG_AROUND_AHEAD (state 14).
void BrnAI::AIAggression::UpdateAggressionStateFallPast(const AICar* lpPlayerCar)
{
    AICar* const lpThisCar = mpCar;

    meSpeedMatchType = ESpeedMatch_Slower;   // +0x58 = 2
    mfHangingAroundTimer = 0.0f;             // +0x68

    // E_ROUTE_FINDING_MARKED_MAN target that has dropped below its decent cruising speed -> spurt past.
    if (lpThisCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)   // ==6
    {
        const f32 lfTargetSpeed = mpTargetCar->GetSpeed();
        const f32 lfDecentSpeed = lpThisCar->GetDecentSpeed();
        if (lfTargetSpeed < lfDecentSpeed)
        {
            mfStateTime = 10.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_SPURT_FORWARD;   // 12
        }
    }

    if (OutOfSpeedMatchRange(lpThisCar, mpTargetCar))
    {
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
        mfStateTime = -1.0f;
        return;
    }

    // State timed out?
    if (mfStateTime != -1.0f && mfStateTime <= 0.0f)
    {
        const ERouteFindingStyle leStyle = mpCar->meRouteFindingStyle;
        const bool lbAggressiveStyle = (leStyle == E_ROUTE_FINDING_ROAD_RAGE) ||   // 2
                                       (leStyle == E_ROUTE_FINDING_MARKED_MAN);    // 6
        if (lbAggressiveStyle)
        {
            // Re-roll a short spurt-forward duration from the shared random generator.
            // X360 inlines mRandom's [1,2) ring draw; the (((r-1)+1)*0.5) folds to r*0.5,
            // the fsel clamps to a minimum floor.
            meAggressionState = E_AI_AGGRESSION_STATE_SPURT_FORWARD;   // 12
            const f32 lfRoll = (1.0f + mRandom.RandomFloat()) * 0.5f;
            mfStateTime = (lfRoll >= 0.0f) ? lfRoll : KF_FALL_PAST_SPURT_MIN_TIME;
            return;
        }
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
        mfStateTime = -1.0f;
        return;
    }

    if (lpPlayerCar == NULL)
        return;

    AICar* const lpCar = mpCar;
    if (lpPlayerCar == lpCar)
        return;
    if (lpPlayerCar->mbIsPlayer && !lpPlayerCar->mbIsDrivenByPlayer)
        return;

    if (mpPlayerCar == NULL)
        return;

    const f32 lfSeparation = GetSeparation(lpCar, lpPlayerCar);
    if (lfSeparation > 0.0f && lfSeparation < 10.0f)
    {
        // Lerp a clip-off-behind state time using the target's speed-match time knob.
        // KF_FALL_PAST_TIME_LERP_LO/HI are the two rodata endpoints; the blend factor is
        // mpCar->GetTimeForSpeedMatch().
        const f32 lfTimeFactor = mpCar->GetAggressiveness()->GetTimeForSpeedMatch();
        meAggressionState = E_AI_AGGRESSION_STATE_BE_FODDER;   // 8 (X360 *a1 = 8)
        const f32 lfClipTime = KF_FALL_PAST_TIME_LERP_LO +
                               (KF_FALL_PAST_TIME_LERP_HI - KF_FALL_PAST_TIME_LERP_LO) * lfTimeFactor;
        mfStateTime = (lfClipTime >= 0.0f) ? lfClipTime : 0.0f;
    }

    const AICar* const lpStyleCar = mpCar;
    const ERouteFindingStyle leStyle2 = lpStyleCar->meRouteFindingStyle;
    const bool lbAggressive2 = (leStyle2 == E_ROUTE_FINDING_ROAD_RAGE) ||
                               (leStyle2 == E_ROUTE_FINDING_MARKED_MAN);
    if (lbAggressive2 &&
        lpStyleCar->miProximityIndex < 0 &&
        lpStyleCar->meRelativeLocation == E_RELATIVE_INFRONT_SEPARATING)   // ==3
    {
        meAggressionState = E_AI_AGGRESSION_STATE_HANG_AROUND_AHEAD;   // 14
        mfStateTime = -1.0f;
    }
}

// BrnAI::AIAggression::UpdateAggressionStateHangAboutAhead @0x82770D88.
//
// HANG_AROUND_AHEAD handler: loiter just ahead of the player so they can be re-engaged.
// While the car is ahead-and-separating (meRelativeLocation == INFRONT_SEPARATING) with
// a negative proximity index, it holds a large +30.0 relative-position-ahead bias and
// speed-match Enabled; otherwise it resets to OUT_OF_RANGE. The dispatched player-car
// argument is unused by this state (the X360 body reads only mpCar).
void BrnAI::AIAggression::UpdateAggressionStateHangAboutAhead(const AICar* lpPlayerCar)
{
    (void)lpPlayerCar;
    const AICar* const lpThisCar = mpCar;

    if (lpThisCar->miProximityIndex < 0 &&
        lpThisCar->meRelativeLocation == E_RELATIVE_INFRONT_SEPARATING)   // ==3
    {
        mfRelativePositionAhead = 30.0f;         // +0x5C
        meSpeedMatchType = ESpeedMatch_Enabled;  // +0x58 = 1
    }
    else
    {
        mfStateTime = -1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
    }
}

// BrnAI::AIAggression::UpdateAggressionStateOutOfRange @0x827965E8.
//
// OUT_OF_RANGE state handler. While no target is engaged it: clears the speed-match
// mode and target-pos validity; in E_ROUTE_FINDING_MARKED_MAN mode, if the player is crashing, spurts
// forward; in E_ROUTE_FINDING_ROAD_RAGE mode with a negative proximity index it either hangs around
// ahead (if already past & separating) or drops to passive; otherwise it tries to
// acquire a target via FindTarget and, on success, decides whether to attack (-> set up
// an overtake/slam) or fall back / overtake-fast based on the leading separation and the
// route-finding style.
void BrnAI::AIAggression::UpdateAggressionStateOutOfRange(const AICar* lpPlayerCar)
{
    AICar* const lpThisCar = mpCar;

    mbTargetPosValid  = false;
    meSpeedMatchType  = ESpeedMatch_Disabled;

    if (lpThisCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)
    {
        if (mpPlayerCar != nullptr && mpPlayerCar->mbIsCrashing)
        {
            mfStateTime       = 10.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_SPURT_FORWARD;
        }
    }

    if (lpThisCar->meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE && lpThisCar->miProximityIndex < 0)
    {
        // (In the X360 this re-tests style == E_ROUTE_FINDING_ROAD_RAGE || E_ROUTE_FINDING_MARKED_MAN; inside this branch
        // it is always E_ROUTE_FINDING_ROAD_RAGE, so the guard is always taken.)
        if (lpThisCar->meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE ||
            lpThisCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)
        {
            if (GetAheadness(mpPlayerCar, mpCar->GetPosition()) >= 20.0f)
            {
                if (mpCar->meRelativeLocation == E_RELATIVE_INFRONT_SEPARATING)
                {
                    mfStateTime       = -1.0f;
                    meAggressionState = E_AI_AGGRESSION_STATE_HANG_AROUND_AHEAD;
                }
            }
            else
            {
                mfStateTime       = 6.0f;
                meAggressionState = E_AI_AGGRESSION_STATE_PASSIVE;
            }
        }
    }
    else if (FindTarget(lpPlayerCar))
    {
        if (DecideToAttack())
        {
            SetSlowOvertakingSpeed();

            if (GetLeadingSeparation(mpPlayerCar, mpCar) <= 4.0f)
            {
                mfStateTime       = 12.0f;
                meAggressionState = E_AI_AGGRESSION_STATE_OVERTAKE_TO_SLAM;
            }
            else
            {
                mfStateTime       = 16.0f;
                meAggressionState = E_AI_AGGRESSION_STATE_DROP_BACK_TO_SLAM;
            }
        }
        else
        {
            const f32 lfLeadingSeparation = GetLeadingSeparation(lpPlayerCar, mpCar);
            AICar* const lpCar = mpCar;

            if (lfLeadingSeparation >= 0.0f)
            {
                if (lpCar->meRouteFindingStyle == E_ROUTE_FINDING_RACE)
                {
                    if (lpCar->mfScheduleOffset1 >= 0.0f || lpCar->mfDistanceToCheckpoint < 1000.0f)
                    {
                        mfStateTime       = 12.0f;
                        meAggressionState = E_AI_AGGRESSION_STATE_PASSIVE;
                    }
                    else
                    {
                        SetSlowFallbackSpeed();
                        mfStateTime       = 8.0f;
                        meAggressionState = E_AI_AGGRESSION_STATE_FALL_PAST;
                    }
                }
                else
                {
                    SetSlowFallbackSpeed();
                    mfStateTime       = 6.0f;
                    meAggressionState = E_AI_AGGRESSION_STATE_FALL_PAST;
                }
            }
            else
            {
                mfStateTime = 12.0f;
                if (lpCar->meRouteFindingStyle == E_ROUTE_FINDING_RACE)
                    meAggressionState = E_AI_AGGRESSION_STATE_PASSIVE;
                else
                    meAggressionState = E_AI_AGGRESSION_STATE_OVERTAKE_FAST;
            }
        }
    }
}

// BrnAI::AIAggression::UpdateAggressionStateOvertakeFast @0x8278B440.
//
// OVERTAKE_FAST handler: blast past on the outside. With no player car it resets to
// OUT_OF_RANGE. Otherwise it measures the leading separation to the player; if that
// exceeds the style-dependent cap (5.0 for E_ROUTE_FINDING_ROAD_RAGE/E_ROUTE_FINDING_MARKED_MAN, else 20.0) it diverts
// to FALL_PAST (state 7, 12s). On state-timeout it resets to OUT_OF_RANGE. Each frame it
// forces speed-match OvertakeFast and clears the hanging-around timer.
void BrnAI::AIAggression::UpdateAggressionStateOvertakeFast()
{
    const AICar* const lpPlayerCar = mpPlayerCar;

    meSpeedMatchType = ESpeedMatch_OvertakeFast;   // +0x58 = 4
    mfHangingAroundTimer = 0.0f;                   // +0x68

    if (lpPlayerCar == NULL)
    {
        mfStateTime = -1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
        return;
    }

    const f32 lfLeadingSeparation = GetLeadingSeparation(lpPlayerCar, mpCar);

    const ERouteFindingStyle leStyle = mpCar->meRouteFindingStyle;
    const bool lbAggressive = (leStyle == E_ROUTE_FINDING_ROAD_RAGE) ||   // 2
                              (leStyle == E_ROUTE_FINDING_MARKED_MAN);    // 6
    const f32 lfSeparationCap = lbAggressive ? 5.0f : 20.0f;

    if (lfLeadingSeparation > lfSeparationCap)
    {
        mfStateTime = 12.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_FALL_PAST;   // 7
    }

    if (mfStateTime != -1.0f && mfStateTime <= 0.0f)
    {
        mfStateTime = -1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
    }
}

// BrnAI::AIAggression::UpdateAggressionStateOvertakeToSlam @0x82793908.
//
// OVERTAKE_TO_SLAM state handler. With a valid target it: bails to WAIT if speed-matching
// has dropped out of range; if it has fallen too far back (leading separation > 4) it
// reverts to DROP_BACK_TO_SLAM; otherwise it slots in beside the target (mTargetPos via
// GetPositionNextToTarget, +4 offset) at overtake-fast speed, and -- once the state has
// timed out, in E_ROUTE_FINDING_ROAD_RAGE/E_ROUTE_FINDING_MARKED_MAN -- spurts forward for a short random burst.
void BrnAI::AIAggression::UpdateAggressionStateOvertakeToSlam(const AICar* /*lpPlayerCar*/, f32 /*lfTimeStep*/)
{
    if (mpTargetCar == nullptr)
    {
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
        mfStateTime       = -1.0f;
        return;
    }

    if (OutOfSpeedMatchRange(mpCar, mpTargetCar))
    {
        meAggressionState = E_AI_AGGRESSION_STATE_WAIT;
        mfStateTime       = 1.0f;
        return;
    }

    if (GetLeadingSeparation(mpPlayerCar, mpCar) > 4.0f)
    {
        meAggressionState = E_AI_AGGRESSION_STATE_DROP_BACK_TO_SLAM;
        mfStateTime       = 16.0f;
        return;
    }

    meSpeedMatchType = ESpeedMatch_OvertakeFast;
    mTargetPos       = GetPositionNextToTarget(mpTargetCar, mpCar, 4.0f);
    mbTargetPosValid = true;

    if (mpCar->meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE ||
        mpCar->meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)
    {
        if (mfStateTime != -1.0f && mfStateTime <= 0.0f)
        {
            meAggressionState = E_AI_AGGRESSION_STATE_SPURT_FORWARD;
            mfStateTime       = (1.0f + mRandom.RandomFloat()) * 0.5f;
        }
    }
}

// BrnAI::AIAggression::UpdateAggressionStateSpurtForward @0x82770DD8.
//
// SPURT_FORWARD handler: a timed burst to pull ahead. Forces speed-match mode
// OvertakeSlowly, clears the hanging-around timer, and sets the fixed passing speed to
// KF_SPURT_PASSING_SPEED_SCALE * 130.0. On state-timeout it resets to OUT_OF_RANGE.
void BrnAI::AIAggression::UpdateAggressionStateSpurtForward()
{
    const f32 lfStateTime = mfStateTime;

    meSpeedMatchType = ESpeedMatch_OvertakeSlowly;   // +0x58 = 5
    mfHangingAroundTimer = 0.0f;                     // +0x68
    mFixedPassingSpeed = KF_SPURT_PASSING_SPEED_SCALE * 130.0f;   // +0x48

    if (lfStateTime != -1.0f && lfStateTime <= 0.0f)
    {
        mfStateTime = -1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;   // 0
    }
}

// BrnAI::AIAggression::UpdateAggressionStateVeer @0x8277DCB8.
//
// VEER state handler. Keeps a +2 lead bias with speed-matching enabled. On timeout: if this
// car is no longer touching the player it drops to WAIT, otherwise it spurts forward for a
// short random burst. While still veering, it steers to a point beside its target
// (defaulting to the player car if no explicit target), placing mTargetPos via
// GetPositionNextToTarget at a +6 offset. With no target at all it likewise spurts forward.
void BrnAI::AIAggression::UpdateAggressionStateVeer()
{
    const f32 lfStateTime   = mfStateTime;
    mbTargetPosValid        = false;
    mfRelativePositionAhead = 2.0f;
    meSpeedMatchType        = ESpeedMatch_Enabled;

    bool lbSpurtForward = false;

    if (lfStateTime != -1.0f && lfStateTime <= 0.0f)
    {
        if (!mpCar->mbIsTouchingPlayer)
        {
            mfStateTime       = 1.0f;
            meAggressionState = E_AI_AGGRESSION_STATE_WAIT;
            return;
        }
        lbSpurtForward = true;
    }
    else if (mpTargetCar == nullptr)
    {
        if (mpPlayerCar == nullptr)
            lbSpurtForward = true;
        else
            mpTargetCar = mpPlayerCar;
    }

    if (lbSpurtForward)
    {
        meAggressionState = E_AI_AGGRESSION_STATE_SPURT_FORWARD;
        mfStateTime       = 1.0f + mRandom.RandomFloat();
        return;
    }

    mTargetPos       = GetPositionNextToTarget(mpTargetCar, mpCar, 6.0f);
    mbTargetPosValid = true;
}

// BrnAI::AIAggression::UpdateAggressionStateVeerExtreme @0x82770EB8.
//
// VEER_EXTREME handler: a hard swerve. Forces speed-match SlowToClip and clears the
// hanging-around timer. On state-timeout it transitions to WAIT (state 4, 1s).
void BrnAI::AIAggression::UpdateAggressionStateVeerExtreme()
{
    const f32 lfStateTime = mfStateTime;

    meSpeedMatchType = ESpeedMatch_SlowToClip;   // +0x58 = 3
    mfHangingAroundTimer = 0.0f;                 // +0x68

    if (lfStateTime != -1.0f && lfStateTime <= 0.0f)
    {
        mfStateTime = 1.0f;
        meAggressionState = E_AI_AGGRESSION_STATE_WAIT;   // 4
    }
}

// BrnAI::AIAggression::UpdateAggressionStateWait @0x82770E50.
//
// WAIT state handler. Disables speed-matching; once the wait time has elapsed it fully
// resets the aggression machine back to OUT_OF_RANGE -- clearing the state time, relative
// position bias, the cached target car and target-position validity.
void BrnAI::AIAggression::UpdateAggressionStateWait()
{
    const f32 lfStateTime = mfStateTime;
    meSpeedMatchType = ESpeedMatch_Disabled;

    if (lfStateTime != -1.0f && lfStateTime <= 0.0f)
    {
        mfStateTime             = -1.0f;
        meAggressionState       = E_AI_AGGRESSION_STATE_OUT_OF_RANGE;
        mfRelativePositionAhead = 0.0f;
        mpTargetCar             = nullptr;
        mbTargetPosValid        = false;
        meSpeedMatchType        = ESpeedMatch_Disabled;
    }
}

}
