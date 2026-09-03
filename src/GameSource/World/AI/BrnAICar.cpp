#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/Route/BrnRoute.h"                 // BrnAI::Route, RouteNode, Route::GetNode
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h" // RaceBalancingManager::ComputeTargetSpeed

#include "GameSource/Math/BrnMathUtils.h"                      // BrnMath::IsNormal (SetRight unit-length check)

#include "GameShared/GameClasses/Core/CgsAssert.h"              // CGS_ASSERT + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"    // CgsDev::StrStream (streamed assert message)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"      // CgsDev::Log::gpDebugPrint, Message::gxMessageFilterFlags
#include "rw/math/vpu/vector3_operation.h"                     // rw::math::vpu vector ops (dot / length / IsValid)

#include <cmath>   // std::sqrt for the de-SIMD'd velocity magnitude

// BrnAI::AICar -- the speed + route-progress CORE. This TU bodies the 14 AICar members that
// bridge navigation, the race rubber band and the car's physics target speed: it picks the
// per-frame desired speed (personality / road-rage / par / rubber-band), maintains the car's
// race-progress + ahead/behind-player state the rubber band's schedule consumes, and runs the
// per-frame route bookkeeping that decides when to ask for a fresh route and which section/node
// the car is on. Reconstructed store-for-store from the X360 pseudocode/asm; inlining is
// reversed into LOGICAL CALLS to the already-bodied/declared callees (RaceBalancingManager::
// ComputeTargetSpeed, Route::GetNode, the AICar accessors).
//
// FLAGGED file-local constants: where a rodata tuning float is referenced only by ADDRESS in
// the X360 build (flt_8300xxxx / flt_820Cxxxx with no immediate decoded by Hex-Rays) its VALUE
// is NOT recoverable from the dossier, so it is shipped as a flagged placeholder (KF_*, value
// 0.0f) and listed in open_questions. Literal immediates Hex-Rays decoded from the float bits
// (0.0/0.5/0.6/0.55/0.4/1.0/3.0/10.0/20.0/30.0/50.0/80.0/100.0/10000.0) are used inline.

namespace BrnAI
{
    namespace vpu = rw::math::vpu;

    // ===== file-local tuning constants (rodata referenced by address; values FLAGGED) =====
    // GetDecentSpeed -- per-route-finding-style "decent" fraction of mfMaxPlayerSpeed. These
    // four ARE recoverable (Hex-Rays decoded the float bits), kept named for readability.
    const f32 KF_DECENT_SPEED_ROAD_RAGE = 0.5f;        // ROAD_RAGE  (flt_820C4168)
    const f32 KF_DECENT_SPEED_PURSUIT   = 0.6f;        // PURSUIT    (flt_820C416C, 0.60000002)
    const f32 KF_DECENT_SPEED_DEFAULT   = 0.55f;       // default    (flt_820C4174, 0.55000001)
    const f32 KF_DECENT_SPEED_MARKED    = 0.4f;        // MARKED_MAN w/ opponent (0.40000001)

    // CalcDesiredSpeed -- proximity/cornering speed clamps (rodata by address; UNRECOVERED).
    const f32 KF_DESIRED_MAX_SPEED          = 0.0f;    // flt_8300DB04 -- FLAG: rodata value unrecovered (upper clamp)
    const f32 KF_DESIRED_PURSUIT_FALLBEHIND = 0.0f;    // flt_8300D700 -- FLAG: rodata value unrecovered
    const f32 KF_DESIRED_PURSUIT_FLOOR      = 0.0f;    // flt_8300DC3C -- FLAG: rodata value unrecovered
    const f32 KF_DESIRED_FREEROAM_BIAS      = 0.0f;    // flt_8300D97C -- FLAG: rodata value unrecovered
    const f32 KF_DESIRED_OPP_SCALE_RACE     = 0.0f;    // flt_8300DC08 -- FLAG: rodata value unrecovered (opponent-index scale, race)
    const f32 KF_DESIRED_OPP_BASE_RACE      = 0.0f;    // flt_8300D7D4 -- FLAG: rodata value unrecovered (opponent-index base, race)
    const f32 KF_DESIRED_OPP_SCALE_DEFAULT  = 0.0f;    // flt_8300D968 -- FLAG: rodata value unrecovered (opponent-index scale, default)
    const f32 KF_DESIRED_OPP_BASE_DEFAULT   = 0.0f;    // flt_8300D708 -- FLAG: rodata value unrecovered (opponent-index base, default)
    const f32 KF_DESIRED_PLAYER_DRIVEN_MUL  = 0.7f;    // flt_820C41A8 == 0.69999999 (read from image.bin @0xC41A8, 2026-09-03; was a 0.0 placeholder)
    const f32 KF_DESIRED_DEFAULT_MUL        = 4.0f;    // flt_820C41C0 == 4.0 (read from image.bin @0xC41C0, 2026-09-03; was a 0.0 placeholder; the same word is Update's wrong-way limit)
    const f32 KF_DESIRED_INRANGE_PLAYER     = 0.0f;    // flt_8300D980 -- FLAG: rodata value unrecovered (meBehaviour==1 player-car speed)

    // CalcPersonalitySpeed -- velocity-magnitude offset + speed scales + clamp band (UNRECOVERED).
    const f32 KF_PERSONALITY_SPEED_OFFSET   = 0.0f;    // flt_8300D788 -- FLAG: rodata value unrecovered (added to |velocity|)
    const f32 KF_PERSONALITY_BASE_SCALE     = 0.44704f; // flt_82F31928 == 0.44704 (mph -> m/s; read from image.bin @0xF31928, 2026-09-03) -- the x50/x80/x100 are mph
    const f32 KF_PERSONALITY_CLAMP_LO       = 0.0f;    // flt_8300D93C -- FLAG: rodata value unrecovered (lower floor)
    const f32 KF_PERSONALITY_CLAMP_HI       = 0.0f;    // flt_8300D740 -- FLAG: rodata value unrecovered (upper cap)

    // CalcRoadRageSpeed -- separation-blended speed offsets (UNRECOVERED).
    const f32 KF_ROAD_RAGE_BEHIND_BIAS      = 0.0f;    // flt_8300D724 -- FLAG: rodata value unrecovered (added when behind / slow)
    const f32 KF_ROAD_RAGE_SLOW_FLOOR       = 0.0f;    // flt_8300D810 -- FLAG: rodata value unrecovered (min speed when player slow)
    const f32 KF_ROAD_RAGE_AHEAD_DROP       = 0.0f;    // flt_8300D83C -- FLAG: rodata value unrecovered (subtracted when ahead)
    const f32 KF_ROAD_RAGE_BLEND_RATE       = 0.05f;   // flt_820047C8 == 0.05 (== 1/20 m, read from image.bin @0x47C8, 2026-09-03) -- lerp over the [0,20] band
    const f32 KF_ROAD_RAGE_SEPARATION_RANGE = 20.0f;   // flt_820C4890 (== 20.0, separation upper bound)

    // UpdateRaceDistance -- speed/distance thresholds.
    const f32 KF_RACE_DISTANCE_MIN_SPEED    = 0.0f;    // flt_8300D814 -- FLAG: rodata value unrecovered (min speed to accrue wrong-way time)
    const f32 KF_RACE_DISTANCE_AHEAD_GAP    =  30.0f;  // flt_820C3FA8 (== 30.0)  ahead-of-player synthetic gap
    const f32 KF_RACE_DISTANCE_BEHIND_GAP   = -30.0f;  // flt_820951A8 (== -30.0) behind-player synthetic gap

    // UpdateRouteFindingRace -- alt-route hold time = base + opponentIndex * scale.
    const f32 KF_ALT_ROUTE_HOLD_BASE        = 10.0f;   // flt_820C4150 (== 10.0)
    const f32 KF_ALT_ROUTE_HOLD_PER_OPP     = 3.0f;    // flt_820C4154 (== 3.0)

    // NeedsNewRoute -- pursuit-route "chased car moved" distance-squared threshold.
    const f32 KF_NEEDS_NEW_ROUTE_DIST_SQ    = 10000.0f; // flt_82005D9C (== 10000.0)

    // Shared zero / one constants (flt_82001CC0 / flt_82001C98) used as the universal 0.0f/1.0f.


    // ==================================================================================
    // GetDecentSpeed @0x82766030
    //
    // The "decent" floor speed for the car's current route-finding style: a fraction of the
    // max player speed. ROAD_RAGE x0.5, PURSUIT x0.6, MARKED_MAN x0.4 (only with a valid
    // opponent index, else 0), everything else x0.55.
    // ==================================================================================
    f32 AICar::GetDecentSpeed() const
    {
        if (meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE)
            return mfMaxPlayerSpeed * KF_DECENT_SPEED_ROAD_RAGE;
        if (meRouteFindingStyle == E_ROUTE_FINDING_PURSUIT)
            return mfMaxPlayerSpeed * KF_DECENT_SPEED_PURSUIT;
        if (meRouteFindingStyle != E_ROUTE_FINDING_MARKED_MAN)
            return mfMaxPlayerSpeed * KF_DECENT_SPEED_DEFAULT;
        if (miOpponentIndex != 0)
            return mfMaxPlayerSpeed * KF_DECENT_SPEED_MARKED;
        return mfMaxPlayerSpeed * 0.0f;
    }

    // ==================================================================================
    // CalcPersonalitySpeed @0x8276F610
    //
    // The racer-personality-driven base speed. Chooses among:
    //   - IN_RANGE & INFRONT_SEPARATING(3): a fixed x80 base;
    //   - IN_RANGE & other relative pos: track the PASSED car's speed (|velocity|) plus a small
    //     offset, floored by a one-time-cached minimum (x100);
    //   - NOT IN_RANGE & approaching (relative 0 or 2): |velocity| + offset (no floor);
    //   - NOT IN_RANGE & separating: a fixed x50 base.
    // The result is finally clamped to [KF_PERSONALITY_CLAMP_LO, KF_PERSONALITY_CLAMP_HI].
    // ==================================================================================
    f32 AICar::CalcPersonalitySpeed(const AICar* lpPlayerCar)
    {
        // NOTE: the X360 body reads the VELOCITY of the passed car (r27, the const AICar* arg),
        // not `this` -- the speed being tracked is lpPlayerCar's.
        //
        // The cached "minimum personality speed" (X360 flt_8300DC68 guarded by dword_8300DC6C):
        // computed once on first use, then reused. Modelled as a function-local one-time init.
        static bool sbMinSpeedInitialised = false;
        static f32  sfCachedMinSpeed = 0.0f;

        // X360 streams "Unknown relative position\n" into the assert buffer on a bad enum.
        if (meRelativeLocation == E_RELATIVE_UNKNOWN)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Unknown relative position\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "..\\..\\..\\GameSource\\World/AI/BrnAICar.cpp", 1487);
            CgsDev::Assert::EndAssert();
        }

        const ELocationRelativeToPlayer leRelative = meRelativeLocation;
        f32 lfSpeed;

        if (meCarState != E_AI_CAR_STATE_IN_RANGE)
        {
            // NOT IN_RANGE branch (X360 loc_8276F7BC).
            if (leRelative == E_RELATIVE_BEHIND_APPROACHING ||
                leRelative == E_RELATIVE_INFRONT_APPROACHING)
            {
                const Vector3 lVelocity = lpPlayerCar->GetVelocity();
                lfSpeed = std::sqrt(vpu::Dot(lVelocity, lVelocity)) + KF_PERSONALITY_SPEED_OFFSET;
            }
            else
            {
                lfSpeed = KF_PERSONALITY_BASE_SCALE * 50.0f;
            }
        }
        else if (leRelative == E_RELATIVE_INFRONT_SEPARATING)
        {
            lfSpeed = KF_PERSONALITY_BASE_SCALE * 80.0f;
        }
        else
        {
            const Vector3 lVelocity = lpPlayerCar->GetVelocity();
            lfSpeed = std::sqrt(vpu::Dot(lVelocity, lVelocity)) + KF_PERSONALITY_SPEED_OFFSET;

            f32 lfMinSpeed;
            if (sbMinSpeedInitialised)
            {
                lfMinSpeed = sfCachedMinSpeed;
            }
            else
            {
                sbMinSpeedInitialised = true;
                lfMinSpeed = KF_PERSONALITY_BASE_SCALE * 100.0f;
                sfCachedMinSpeed = lfMinSpeed;
            }
            if (lfSpeed < lfMinSpeed)
                lfSpeed = lfMinSpeed;
        }

        // Final clamp: min(KF_PERSONALITY_CLAMP_HI, max(KF_PERSONALITY_CLAMP_LO, lfSpeed)).
        f32 lfClamped = (KF_PERSONALITY_CLAMP_LO >= lfSpeed) ? KF_PERSONALITY_CLAMP_LO : lfSpeed;
        return (KF_PERSONALITY_CLAMP_HI >= lfClamped) ? lfClamped : KF_PERSONALITY_CLAMP_HI;
    }

    // ==================================================================================
    // CalcRoadRageSpeed @0x8276F8A0
    //
    // The Road-Rage-mode speed. The player car drives the target: when the AI is the player,
    // just return its decent speed. Otherwise project the player->AI separation onto the
    // player's facing (lfForwardSep) and:
    //   - if the player is at/above the AI's decent speed: drop when the player is well ahead
    //     (sep > 20), boost when behind (sep < 0), else lerp between the two across the [0,20]
    //     band;
    //   - if the player is slower than decent: floor to KF_ROAD_RAGE_SLOW_FLOOR.
    // The result is finally floored by GetDecentSpeed().
    // ==================================================================================
    f32 AICar::CalcRoadRageSpeed(const AICar* lpPlayerCar)
    {
        CGS_ASSERT(IsActive(), "IsActive()");

        if (mbIsPlayer)
            return GetDecentSpeed();

        // lfForwardSep = dot( playerDirection, (thisPos - playerPos) ).
        // X360: v127=GetPosition(player), v126=GetPosition(this); vsubfp128 v0 = v126 - v127
        // (thisPos - playerPos); vmsum3fp128(playerDir, v0). Positive => this car leads along the
        // player's facing.
        const Vector3 lPlayerPos = lpPlayerCar->GetPosition();
        const Vector3 lThisPos   = GetPosition();
        const Vector3 lPlayerDir = lpPlayerCar->GetDirection();
        const f32 lfForwardSep   = vpu::Dot(lPlayerDir, lThisPos - lPlayerPos);

        const f32 lfPlayerSpeed = lpPlayerCar->GetSpeed();
        const f32 lfDecentSpeed = GetDecentSpeed();

        f32 lfSpeed;
        if (lfPlayerSpeed >= lfDecentSpeed)
        {
            if (lfForwardSep > KF_ROAD_RAGE_SEPARATION_RANGE)
            {
                lfSpeed = lpPlayerCar->GetSpeed() - KF_ROAD_RAGE_AHEAD_DROP;
            }
            else if (lfForwardSep < 0.0f)
            {
                lfSpeed = lpPlayerCar->GetSpeed() + KF_ROAD_RAGE_BEHIND_BIAS;
            }
            else
            {
                const f32 lfAhead  = lpPlayerCar->GetSpeed() - KF_ROAD_RAGE_AHEAD_DROP;
                const f32 lfBehind = lpPlayerCar->GetSpeed() + KF_ROAD_RAGE_BEHIND_BIAS;
                lfSpeed = (lfAhead - lfBehind) * lfForwardSep * KF_ROAD_RAGE_BLEND_RATE + lfBehind;
            }
        }
        else
        {
            const f32 lfBehind = lpPlayerCar->GetSpeed() + KF_ROAD_RAGE_BEHIND_BIAS;
            lfSpeed = (KF_ROAD_RAGE_SLOW_FLOOR >= lfBehind) ? KF_ROAD_RAGE_SLOW_FLOOR : lfBehind;
        }

        const f32 lfFloor = GetDecentSpeed();
        return (lfSpeed >= lfFloor) ? lfSpeed : lfFloor;
    }

    // ==================================================================================
    // CalcDesiredSpeed @0x82796078
    //
    // The car's per-frame target speed. While meBehaviour == 1 it returns an opponent-rank-
    // scaled speed directly (player car -> a fixed speed; otherwise a per-route-finding-style
    // base minus opponentIndex*scale). Otherwise it dispatches on the speed-selection method:
    //   0 (proximity): IN_RANGE only; dispatch on meRelativeLocation -- BEHIND_APPROACHING(0)
    //      caps (playerSpeed + bias); INFRONT_SEPARATING(3) clamps (playerSpeed - fallBehind);
    //      any other relative position returns the upper clamp speed; not-IN_RANGE -> max clamp.
    //   1 (rubber band): query RaceBalancingManager::ComputeTargetSpeed, then -- once the race
    //      has settled (mfRaceTimer > 15) and the car is within a sane lead (0 < ahead < 80) --
    //      floor it by GetDecentSpeed();
    //   2 (road rage): CalcRoadRageSpeed; 3 (personality): CalcPersonalitySpeed;
    //   4: mfMaxPlayerSpeed (raw if IN_RANGE, else * DEFAULT_MUL);
    //   default: 0.0f (X360 returns flt_82001CC0, the universal zero -- NOT the max clamp).
    // The recovered value is returned in f1 (the X360 helper for the clamps is fsel).
    // ==================================================================================
    f32 AICar::CalcDesiredSpeed(const RaceBalancingManager* lpRaceBalancingManager,
                                AISectionsData* lpAISectionsData, const AICar* lpPlayerCar)
    {
        CGS_ASSERT(IsActive(), "IsActive()");

        // meBehaviour == 1: opponent-rank-scaled speed, returned without consulting the
        // speed-selection switch. (The dossier's Hex-Rays pseudocode dropped this branch; the
        // X360 asm @0x827960E8 is authoritative.)
        if (meBehaviour == 1)
        {
            if (mbIsPlayer)
                return KF_DESIRED_INRANGE_PLAYER;

            const f32 lfOpponent = static_cast<f32>(miOpponentIndex);
            if (meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE)
                return KF_DESIRED_OPP_BASE_RACE - lfOpponent * KF_DESIRED_OPP_SCALE_RACE;
            return KF_DESIRED_OPP_BASE_DEFAULT - lfOpponent * KF_DESIRED_OPP_SCALE_DEFAULT;
        }

        switch (meSpeedSelectionMethod)
        {
            case 0:
            {
                // Proximity-clamped speed, dispatched on where the car sits relative to the
                // player. Only runs while IN_RANGE; otherwise falls through to the max clamp.
                if (meCarState != E_AI_CAR_STATE_IN_RANGE)
                    return KF_DESIRED_MAX_SPEED;

                if (meRelativeLocation == E_RELATIVE_BEHIND_APPROACHING) // relative == 0
                {
                    // min(KF_DESIRED_MAX_SPEED, playerSpeed + bias)
                    const f32 lfSpeed = lpPlayerCar->GetSpeed() + KF_DESIRED_FREEROAM_BIAS;
                    return (KF_DESIRED_MAX_SPEED >= lfSpeed) ? KF_DESIRED_MAX_SPEED : lfSpeed;
                }
                if (meRelativeLocation == E_RELATIVE_INFRONT_SEPARATING) // relative == 3
                {
                    // clamp(playerSpeed - fallBehind) into [KF_DESIRED_PURSUIT_FLOOR, KF_DESIRED_MAX_SPEED]
                    const f32 lfSpeed  = lpPlayerCar->GetSpeed() - KF_DESIRED_PURSUIT_FALLBEHIND;
                    const f32 lfCapped = (KF_DESIRED_MAX_SPEED >= lfSpeed) ? KF_DESIRED_MAX_SPEED : lfSpeed;
                    return (KF_DESIRED_PURSUIT_FLOOR >= lfCapped) ? lfCapped : KF_DESIRED_PURSUIT_FLOOR;
                }
                // Any other relative position -> the upper clamp speed.
                return KF_DESIRED_MAX_SPEED;
            }

            case 1:
            {
                // Rubber-band target speed.
                if (mbIsPlayer)
                    return mfMaxPlayerSpeed * KF_DESIRED_PLAYER_DRIVEN_MUL;

                // Restore the inlined RaceBalancingManager::ComputeTargetSpeed call by name.
                f32 lfTargetSpeed = lpRaceBalancingManager->ComputeTargetSpeed(
                    this, lpAISectionsData, lpPlayerCar->IsCrashing());

                // Once the race has settled and the car holds a sane lead, floor by decent speed.
                if (mfRaceTimer > 15.0f)
                {
                    const f32 lfAhead = mfDistanceAheadOfPlayer;
                    if (lfAhead > 0.0f && lfAhead < 80.0f)
                    {
                        const f32 lfDecent = GetDecentSpeed();
                        if (lfDecent >= lfTargetSpeed)
                            lfTargetSpeed = lfDecent;
                    }
                }
                return lfTargetSpeed;
            }

            case 2:
                return CalcRoadRageSpeed(lpPlayerCar);

            case 3:
                return CalcPersonalitySpeed(lpPlayerCar);

            case 4:
            {
                // mfMaxPlayerSpeed raw while IN_RANGE, else scaled by KF_DESIRED_DEFAULT_MUL.
                if (meCarState == E_AI_CAR_STATE_IN_RANGE)
                    return mfMaxPlayerSpeed;
                return mfMaxPlayerSpeed * KF_DESIRED_DEFAULT_MUL;
            }

            default:
                // X360 @0x82796330 returns flt_82001CC0 == 0.0f (the universal zero),
                // NOT the proximity max-speed clamp. Store-for-store: a bare 0.0f.
                return 0.0f;
        }
    }

    // ==================================================================================
    // UpdateRelativePositionToPlayer @0x8276FA88
    //
    // Classifies where this car sits relative to the player and stores it in meRelativeLocation.
    //   - forwardSep = dot( (playerPos - thisPos), playerDirection )
    //   - then a second dot of the two cars' facings decides approaching vs separating.
    // forwardSep <= 0 => INFRONT_SEPARATING(3) unless facingDot <= 0 -> INFRONT_APPROACHING(2)
    // forwardSep >  0 => facingDot <= 0 ? BEHIND_SEPARATING(1) : BEHIND_APPROACHING(0)
    // (the enum labels follow the X360 result values 3/2/1/0 exactly.)
    // ==================================================================================
    void AICar::UpdateRelativePositionToPlayer(const AICar* lpPlayerCar)
    {
        const Vector3 lThisPos   = GetPosition();
        const Vector3 lPlayerPos = lpPlayerCar->GetPosition();
        const Vector3 lPlayerDir = lpPlayerCar->GetDirection();

        const f32 lfForwardSep = vpu::Dot(lPlayerPos - lThisPos, lPlayerDir);

        s32 liRelative;
        if (lfForwardSep <= 0.0f)
        {
            const Vector3 lPlayerDir2 = lpPlayerCar->GetDirection();
            const Vector3 lThisDir    = GetDirection();
            const f32 lfFacingDot = vpu::Dot(lThisDir, lPlayerDir2);
            liRelative = E_RELATIVE_INFRONT_SEPARATING;            // 3
            if (lfFacingDot <= 0.0f)
                liRelative = E_RELATIVE_INFRONT_APPROACHING;       // 2
        }
        else
        {
            const Vector3 lPlayerDir2 = lpPlayerCar->GetDirection();
            const Vector3 lThisDir    = GetDirection();
            const f32 lfFacingDot = vpu::Dot(lThisDir, lPlayerDir2);
            liRelative = (lfFacingDot <= 0.0f) ? E_RELATIVE_BEHIND_SEPARATING   // 1
                                               : E_RELATIVE_BEHIND_APPROACHING; // 0
        }

        meRelativeLocation = static_cast<ELocationRelativeToPlayer>(liRelative);
    }

    // ==================================================================================
    // UpdateRaceDistance @0x8278AEB8
    //
    // Maintains the car's race progress + ahead/behind-player state the rubber band's schedule
    // consumes. Recomputes mfDistanceToCheckpoint; while the car is in-game, not crashing and
    // moving above a floor speed (and its distance is not decreasing) it accrues mfWrongWayTime
    // by the time step, else resets it. Then derives mfDistanceAheadOfPlayer + mbIsAheadOfPlayer:
    //   - same checkpoint: ahead-distance = player's checkpoint distance - own distance;
    //   - different checkpoint: ahead iff own checkpoint index > player's, with a synthetic
    //     +/-30 distance gap.
    // (Arg order from the assert strings: a2 = lpAISectionsData, a3 = lpPlayerCar.)
    // ==================================================================================
    void AICar::UpdateRaceDistance(const AISectionsData* lpAISectionsData,
                                   const AICar* lpPlayerCar, f32 lfTimeStep)
    {
        CGS_ASSERT(IsActive(), "IsActive()");
        CGS_ASSERT(lpPlayerCar != NULL, "lpPlayerCar != NULL");
        CGS_ASSERT(lpAISectionsData != NULL, "lpAISectionsData != NULL");

        const f32 lfPrevDistance = mfDistanceToCheckpoint;

        if (ComputeDistanceToCheckpoint(lpAISectionsData, lpPlayerCar, &mfDistanceToCheckpoint))
        {
            if (mfDistanceToCheckpoint < 0.0f)
            {
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "Distance is wrong\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessageBuffer,
                                           "..\\..\\..\\GameSource\\World/AI/BrnAICar.cpp", 1722);
                CgsDev::Assert::EndAssert();
            }

            if (mbIsDrivenByPlayer && mbIsInGameMode && !mbIsCrashing &&
                GetSpeed() > KF_RACE_DISTANCE_MIN_SPEED &&
                lfPrevDistance <= mfDistanceToCheckpoint)
            {
                mfWrongWayTime = mfWrongWayTime + lfTimeStep;
            }
            else
            {
                mfWrongWayTime = 0.0f;
            }

            const s32 liPlayerCheckpoint = lpPlayerCar->miCurrentCheckpoint;
            const s32 liThisCheckpoint   = miCurrentCheckpoint;
            if (liThisCheckpoint == liPlayerCheckpoint)
            {
                const f32 lfAhead = lpPlayerCar->mfDistanceToCheckpoint - mfDistanceToCheckpoint;
                mfDistanceAheadOfPlayer = lfAhead;
                mbIsAheadOfPlayer = lfAhead > 0.0f;
            }
            else
            {
                mbIsAheadOfPlayer = liThisCheckpoint > liPlayerCheckpoint;
                mfDistanceAheadOfPlayer = (liThisCheckpoint > liPlayerCheckpoint)
                                              ? KF_RACE_DISTANCE_AHEAD_GAP
                                              : KF_RACE_DISTANCE_BEHIND_GAP;
            }
        }
    }

    // ==================================================================================
    // NeedsNewRoute @0x82765F80
    //
    // True when the car's route is stale and a fresh request must be issued. Never asks while
    // it has no valid best/default section, or while crashing. For a BLOCKED (pursuit/
    // extrapolated) route it additionally checks whether the chased-car target has moved more
    // than 100 units (distance-squared > 10000) from the last route position -- if not it does
    // NOT ask; the standing request flag (mbRouteRequested) carries every other case.
    // ==================================================================================
    bool AICar::NeedsNewRoute() const
    {
        u16 luSection = muBestSectionIndex;
        if (luSection == KI_INVALID_SECTION_INDEX)
            luSection = muDefaultSectionIndex;
        if (luSection == KI_INVALID_SECTION_INDEX)
            return false;

        if (mbIsCrashing)
            return false;

        // mRoute.meStatus @+0x1408 == E_STATUS_BLOCKED(3) is the pursuit/extrapolated case.
        const Route* lpRoute = reinterpret_cast<const Route*>(this);
        if (lpRoute->GetStatus() != Route::E_STATUS_BLOCKED)
            return mbRouteRequested;

        // Distance-squared between where the route was built (mLastRoutePosition @+0x1480)
        // and the car now (mPosition @+0x1430): when the car has NOT moved far enough the route
        // is still good -> do not re-ask. (Named members since the Update wave, 2026-09-03.)
        const Vector3 lDelta = mLastRoutePosition - mPosition;
        const f32 lfDistSq = vpu::Dot(lDelta, lDelta);

        // vcmpgtfp.: branch taken when KF_NEEDS_NEW_ROUTE_DIST_SQ > lfDistSq (still close).
        if (KF_NEEDS_NEW_ROUTE_DIST_SQ > lfDistSq)
            return false;

        return mbRouteRequested;
    }

    // ==================================================================================
    // UpdateRoute @0x8276F2C8
    //
    // Builds this car's route from lpRoute and re-seeds the per-route bookkeeping. Copy-
    // constructs mRoute; if the new route is empty it flags mbRouteRequested (ask again), else
    // seeds the next node index from the route's default start node and bumps miRouteTimeStamp.
    // Clears the block-section / master-route flags, copies the cached transform vector at
    // this+0x1430 -> this+0x1480, and the route + wrong-way timers.
    // ==================================================================================
    void AICar::UpdateRoute(const Route* lpRoute, const AISectionsData* lpAISectionsData)
    {
        CGS_ASSERT(IsActive(), "IsActive()");

        mbRouteRequested = false;

        Route* lpThisRoute = reinterpret_cast<Route*>(this);
        lpThisRoute->Construct(*lpRoute);

        if (lpThisRoute->GetNodeCount() <= 0)
        {
            mbRouteRequested = true;
        }
        else
        {
            SetNextRouteNodeIndex(lpThisRoute->GetDefaultStartNode());
            ++miRouteTimeStamp;
        }

        mbHasBlockCheckpoints = 0;
        mbIsInMasterRoute     = false;

        // X360 snapshots mPosition (+0x1430) into mLastRoutePosition (+0x1480): the route-age
        // tests measure the car's drift from here. (Named members since the Update wave.)
        mLastRoutePosition = mPosition;   // lvx128 this+0x1430 -> stvx128 this+0x1480

        mfRouteTimer   = 0.0f;
        mfWrongWayTime = 0.0f;

        (void)lpAISectionsData; // present in the signature; the X360 body does not read it.
    }

    // ==================================================================================
    // UpdateRouteFinding @0x8278ADB8
    //
    // Per-frame route-finding dispatch. If the route is invalid it flags mbRouteRequested, then
    // fans out by route-finding style: FREE_ROAM and RACE have dedicated updaters; the pursuit-
    // family styles (2..6) re-request when their extrapolated route is getting old.
    // ==================================================================================
    void AICar::UpdateRouteFinding(f32 lfTimeStep, const AICar* lpPlayerCar)
    {
        if (!HasValidRoute())
            mbRouteRequested = true;

        switch (meRouteFindingStyle)
        {
            case E_ROUTE_FINDING_FREE_ROAM:
                UpdateRouteFindingFreeRoam(lpPlayerCar);
                break;

            case E_ROUTE_FINDING_RACE:
                UpdateRouteFindingRace(lfTimeStep, lpPlayerCar);
                break;

            case E_ROUTE_FINDING_ROAD_RAGE:
            case E_ROUTE_FINDING_PURSUIT:
            case E_ROUTE_FINDING_AVOID_PLAYER:
            case E_ROUTE_FINDING_ALWAYS_STRAIGHT:
            case E_ROUTE_FINDING_MARKED_MAN:
                if (IsExtrapolatedRouteGettingOld())
                    mbRouteRequested = true;
                break;

            default:
                CGS_ASSERT(false, "Invalid route finding style");
                break;
        }
    }

    // ==================================================================================
    // UpdateRouteFindingRace @0x82765E08
    //
    // Race-mode route bookkeeping. Only runs while in a real route-finding style (non-FREE_ROAM).
    // When forced to the standard route it zeroes the alt-route timer. Otherwise, for a non-
    // player car that wants an alternative line, it counts mfAlternativeRouteTimer up to a hold
    // time of (10 + opponentIndex*3) seconds; once it crosses that it flags mbRouteRequested so
    // a fresh (alternative) route is requested. (The timestep arrives in FPR f1 -- it survives
    // UpdateRouteFinding's `mr r3`-only tail call.)
    // ==================================================================================
    void AICar::UpdateRouteFindingRace(f32 lfTimeStep, const AICar* lpPlayerCar)
    {
        if (meRouteFindingStyle == E_ROUTE_FINDING_FREE_ROAM)
            return;

        if (mbForceStandardRoute)
        {
            mfAlternativeRouteTimer = 0.0f;
            return;
        }

        if (mbIsPlayer)
            return;

        if (!mbWantsAlternativeRoute)
            return;

        const f32 lfHoldTime = static_cast<f32>(miOpponentIndex) * KF_ALT_ROUTE_HOLD_PER_OPP +
                               KF_ALT_ROUTE_HOLD_BASE;
        if (mfAlternativeRouteTimer < lfHoldTime)
        {
            mfAlternativeRouteTimer = mfAlternativeRouteTimer + lfTimeStep;
            if (mfAlternativeRouteTimer >= lfHoldTime)
                mbRouteRequested = true;
        }

        (void)lpPlayerCar; // present in the signature; the X360 body reads only `this`'s state.
    }

    // ==================================================================================
    // CheckForSectionChange @0x8277BFD0
    //
    // Tracks which section the car is on. While forced to the standard route, crashing, or the
    // current section is still OK, it just clears the invalid-section dwell timer. Otherwise it
    // tries to move to the best section on the route; on success it adopts that section and
    // clears the timer. On failure it accrues the dwell timer (forced to >= 1s once the car has
    // run past its route nodes) and, when a second of invalid-section time has elapsed, drops the
    // route entirely (clears the wrong-way timer, node count and status).
    // ==================================================================================
    void AICar::CheckForSectionChange(f32 lfTimeStep, AISectionsData* lpAISectionsData,
                                      const Route* lpRoute)
    {
        if (mbForceStandardRoute || mbIsCrashing || CurrentSectionIsOK())
        {
            mfTimeInInvalidSection = 0.0f;
            return;
        }

        if (MoveToSectionOnRoute(muBestSectionIndex, lpRoute))
        {
            muDefaultSectionIndex  = muBestSectionIndex;
            mfTimeInInvalidSection = 0.0f;
            return;
        }

        // mRoute.miNodeCount @+0x1400; miNextRouteNodeIndex @+0x1524.
        const Route* lpThisRoute = reinterpret_cast<const Route*>(this);
        if (miNextRouteNodeIndex >= lpThisRoute->GetNodeCount())
            mfTimeInInvalidSection = 1.0f;

        const f32 lfNewTime = mfTimeInInvalidSection + lfTimeStep;
        mfTimeInInvalidSection = lfNewTime;
        if (lfNewTime >= 1.0f)
        {
            mfWrongWayTime = 0.0f;
            Route* lpMutableRoute = reinterpret_cast<Route*>(this);
            lpMutableRoute->meStatus     = Route::E_STATUS_UNINITIALISED;
            lpMutableRoute->miNodeCount  = 0;
        }

        (void)lpAISectionsData; // present in the signature; the X360 body does not read it here.
    }

    // ==================================================================================
    // CurrentSectionIsOK @0x82765D18
    //
    // True when the car's best section is still consistent with its committed section. The
    // invalid sentinel (0x7FFF) is trivially OK; a car with no valid route is OK; otherwise the
    // committed default section must equal the best section.
    // ==================================================================================
    bool AICar::CurrentSectionIsOK()
    {
        const u16 luBest = muBestSectionIndex;
        if (luBest == KI_INVALID_SECTION_INDEX)
            return true;

        // HasValidRoute(): mRoute.meStatus != UNINITIALISED && mRoute.miNodeCount > 0.
        const Route* lpThisRoute = reinterpret_cast<const Route*>(this);
        const bool lbHasValidRoute = (lpThisRoute->GetStatus() != Route::E_STATUS_UNINITIALISED) &&
                                     (lpThisRoute->GetNodeCount() > 0);
        if (!lbHasValidRoute)
            return true;

        return muDefaultSectionIndex == luBest;
    }

    // ==================================================================================
    // GetSectionIndexOfLastSectionInRoute @0x8276B710
    //
    // The AI-section index of the route's final node (the checkpoint section). Asserts the car
    // has a valid, non-empty route, then reads node[count-1].muSectionIndex via Route::GetNode.
    // ==================================================================================
    u16 AICar::GetSectionIndexOfLastSectionInRoute() const
    {
        const Route* lpThisRoute = reinterpret_cast<const Route*>(this);

        CGS_ASSERT(HasValidRoute(), "HasValidRoute()");
        CGS_ASSERT(lpThisRoute->GetNodeCount() > 0, "mRoute.GetNodeCount() > 0");

        return lpThisRoute->GetNode(lpThisRoute->GetNodeCount() - 1)->GetSectionIndex();
    }

    // ==================================================================================
    // GetSpeed @0x82764D68
    //
    // The car's current speed. IN_RANGE reads the cached in-range speed; OUT_OF_RANGE reads the
    // cached out-of-range speed (asserting it really is one of those two states). The X360 asm
    // tests meCarState @+0x14C8: ==0 (IN_RANGE) -> mfSpeedInRange @+0x14E4; otherwise it asserts
    // the state is 1 (OUT_OF_RANGE) and returns mfSpeedOutOfRange @+0x14E8.
    // ==================================================================================
    f32 AICar::GetSpeed() const
    {
        if (meCarState == E_AI_CAR_STATE_IN_RANGE)
            return mfSpeedInRange;

        CGS_ASSERT(meCarState == E_AI_CAR_STATE_OUT_OF_RANGE, "AI car in bad state");
        return mfSpeedOutOfRange;
    }

    // ==================================================================================
    // GetVelocity @0x8276B570
    //
    // The car's world-space velocity (mVelocity @+0x1470), validity-asserted. The X360 body is
    // the inlined sret accessor: it runs the per-lane rw::math::vpu::IsValid NaN check, asserts
    // "Invalid velocity", then copies the stored vector into the return slot.
    // ==================================================================================
    Vector3 AICar::GetVelocity() const
    {
        CGS_ASSERT(vpu::IsValid(mVelocity), "Invalid velocity");
        return mVelocity;
    }

    // ==================================================================================
    // GetLastGoodPosition @0x8276B640
    //
    // The car's last known on-track position (mLastGoodPosition @+0x1460), validity-asserted.
    // Same inlined sret-accessor shape as GetVelocity, asserting "Invalid car last good position".
    // ==================================================================================
    Vector3 AICar::GetLastGoodPosition() const
    {
        CGS_ASSERT(vpu::IsValid(mLastGoodPosition), "Invalid car last good position");
        return mLastGoodPosition;
    }

    // ==================================================================================
    // IsOnStartLine @0x82764E68
    //
    // Whether the car is sitting on the start line (mbIsOnStartLine @+0x1547). Asserts the car is
    // not in the INACTIVE state first (meCarState @+0x14C8 != 2).
    // ==================================================================================
    bool AICar::IsOnStartLine() const
    {
        CGS_ASSERT(meCarState != E_AI_CAR_STATE_INACTIVE, "GetState() != E_AI_CAR_STATE_INACTIVE");
        return mbIsOnStartLine;
    }

    // ==================================================================================
    // SetBehaviour @0x82764DE0
    //
    // Sets the car's per-frame behaviour. Asserts the value is in range, shifts the current
    // behaviour into mePreviousBehaviour, stores the new one, and resets the per-behaviour timer
    // (mfBehaviourTimer @+0x14E0) to 0. (X360 stores meBehaviour @+0x14B4 and the previous one
    // @+0x14B8; the cleared float is flt_82001CC0 == 0.0f.)
    // ==================================================================================
    void AICar::SetBehaviour(EAIBehaviour leBehaviour)
    {
        CGS_ASSERT(leBehaviour >= 0, "leBehaviour >= 0");
        CGS_ASSERT(leBehaviour < E_AI_BEHAVIOUR_COUNT, "leBehaviour < E_AI_BEHAVIOUR_COUNT");

        mePreviousBehaviour = meBehaviour;
        meBehaviour         = leBehaviour;
        mfBehaviourTimer    = 0.0f;
    }

    // ==================================================================================
    // SetIsInJunkyard @0x82764EC0
    //
    // Records whether the car is currently inside a junkyard (mbIsInJunkYard @+0x154F) and logs
    // the transition through the debug-print stream, gated by the message filter. The log line is
    // prefixed "<AI> Player -> " / "<AI> AI -> " depending on mbIsPlayer @+0x1549, then either
    // "In junkyard\n" or "Exit junkyard\n".
    // ==================================================================================
    void AICar::SetIsInJunkyard(bool lbInJunkyard)
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
        {
            if (mbIsPlayer)
                *CgsDev::Log::gpDebugPrint << "<AI> Player -> ";
            else
                *CgsDev::Log::gpDebugPrint << "<AI> AI -> ";

            if (lbInJunkyard)
                *CgsDev::Log::gpDebugPrint << "<AI> In junkyard\n";
            else
                *CgsDev::Log::gpDebugPrint << "<AI> Exit junkyard\n";
        }

        mbIsInJunkYard = lbInJunkyard;
    }

    // ==================================================================================
    // SetRight @0x8276B7D0
    //
    // Stores the car's world-space right vector (mRight @+0x1450). Asserts the input is a valid
    // (non-NaN) vector and that it is unit length (BrnMath::IsNormal); the unit-length failure
    // streams the offending components into the assert message ("Right is  (x,y,z)\n").
    // ==================================================================================
    void AICar::SetRight(Vector3 lRight)
    {
        CGS_ASSERT(vpu::IsValid(lRight), "RwMath::IsValid( lRight )");

        if (!BrnMath::IsNormal(lRight))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Right is  (" << lRight.x << "," << lRight.y << "," << lRight.z << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       "..\\..\\..\\GameSource\\World/AI/BrnAICar.h", 1312);
            CgsDev::Assert::EndAssert();
        }

        mRight = lRight;
    }
}
