#define CGS_ARTIST_UNITY_BRNAICAR_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/BrnAICar.cpp"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"       // the .bss tunables, recovered from their start-up initialisers
#include "GameSource/World/AI/Route/BrnRoute.h"                 // BrnAI::Route, RouteNode, Route::GetNode
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h" // RaceBalancingManager::ComputeTargetSpeed
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\World/AI/BrnAICar.cpp"
#include "GameSource/Math/BrnMathUtils.h"                      // BrnMath::IsNormal (SetRight unit-length check)

#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"              // CGS_ASSERT + KI_MESSAGEBUFFERSIZE
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
// File-local tuning constants: the 0x820Cxxxx / 0x8200xxxx ones are the big-endian rodata
// literal pool and are read straight out of image.bin. The 0x8300Dxxx ones are .bss, written at
// start-up by the CRT initialiser bank at 0x82C685B8..0x82C69488; they now live in
// BrnAICar_Constants.h with their initialiser address and derivation, so there are NO 0.0f
// placeholder tunables left in this TU. Literal immediates Hex-Rays decoded from the float bits
// (0.0/0.5/0.6/0.55/0.4/1.0/3.0/10.0/20.0/30.0/50.0/80.0/100.0/10000.0) are used inline.

static const char KAC_ARTIST_ASSERT_FILE[] = "..\\..\\..\\GameSource\\World/AI/BrnAICar.h";
static const char* const KPC_AICAR_CPP = "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/BrnAICar.cpp";

namespace BrnAI
{
    namespace vpu = rw::math::vpu;

    // ARTIST race-balancing HUD checks the driver before reading forced speed.
    bool AICar::IsSpeedMatching() const
    {
        return mpDriver != 0 && mpDriver->UseForcedSpeed();
    }

    f32 AICar::GetSpeedMatchSpeed() const
    {
        return mpDriver->GetForcedSpeed();
    }

    // ===== file-local tuning constants =====
    // The .bss tunables (0x8300Dxxx) live in BrnAICar_Constants.h -- KF_DESIRED_*,
    // KF_PERSONALITY_SPEED_OFFSET / KF_PERSONALITY_CLAMP_*, KF_ROAD_RAGE_BEHIND_BIAS /
    // SLOW_FLOOR / AHEAD_DROP and KF_RACE_DISTANCE_MIN_SPEED -- recovered 2026-09-04 from their
    // start-up initialisers in the CRT bank at 0x82C685B8..0x82C69488.
    //
    // GetDecentSpeed @0x82766030 -- per-route-finding-style "decent" fraction of
    // mfMaxPlayerSpeed. The asm dispatches on meRouteFindingStyle 2 / 3 / 6 / default; the four
    // rodata floats sit in one pool addressed off 0x820C4164: 0x820C4164 = 0.55 (the default
    // arm), 0x820C4168 = 0.5 (ROAD_RAGE), 0x820C416C = 0.6 (PURSUIT), 0x820C4174 = 0.4
    // (MARKED_MAN with a non-zero opponent index).
    const f32 KF_DECENT_SPEED_ROAD_RAGE = 0.5f;        // flt_820C4168 -- style 2
    const f32 KF_DECENT_SPEED_PURSUIT   = 0.6f;        // flt_820C416C -- style 3 (0.60000002)
    const f32 KF_DECENT_SPEED_DEFAULT   = 0.55f;       // flt_820C4164 -- default arm (0.55000001)
    const f32 KF_DECENT_SPEED_MARKED    = 0.4f;        // flt_820C4174 -- style 6 w/ opponent (0.40000001)

    // CalcDesiredSpeed -- the two multipliers that are rodata, not .bss.
    const f32 KF_DESIRED_PLAYER_DRIVEN_MUL  = 0.7f;    // flt_820C41A8 == 0.69999999 (read from image.bin @0xC41A8, 2026-09-03; was a 0.0 placeholder)
    const f32 KF_DESIRED_DEFAULT_MUL        = 4.0f;    // flt_820C41C0 == 4.0 (read from image.bin @0xC41C0, 2026-09-03; was a 0.0 placeholder; the same word is Update's wrong-way limit)

    // CalcPersonalitySpeed -- the mph -> m/s factor the x50/x80/x100 bases are scaled by.
    const f32 KF_PERSONALITY_BASE_SCALE     = 0.44704f; // flt_82F31928 == 0.44704 (mph -> m/s; read from image.bin @0xF31928, 2026-09-03) -- the x50/x80/x100 are mph

    // CalcRoadRageSpeed -- the separation blend.
    const f32 KF_ROAD_RAGE_BLEND_RATE       = 0.05f;   // flt_820047C8 == 0.05 (== 1/20 m, read from image.bin @0x47C8, 2026-09-03) -- lerp over the [0,20] band
    const f32 KF_ROAD_RAGE_SEPARATION_RANGE = 20.0f;   // flt_820C4890 (== 20.0, separation upper bound)

    // UpdateRaceDistance -- speed/distance thresholds.
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
                                       CGS_ASSERT_LOCATION(CGS_ARTIST_FILE, 1487));
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
        CGS_ASSERT_AT(IsActive(), "IsActive()", CGS_ARTIST_UNITY_BRNAICAR_CPP_FILE, 1654);

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
        CGS_ASSERT_AT(IsActive(), "IsActive()", CGS_ARTIST_UNITY_BRNAICAR_CPP_FILE, 1543);

        // meBehaviour == 1: opponent-rank-scaled speed, returned without consulting the
        // speed-selection switch. (The dossier's Hex-Rays pseudocode dropped this branch; the
        // X360 asm @0x827960E8 is authoritative.)
        if (meBehaviour == 1)
        {
            if (mbIsPlayer)
                return KF_DESIRED_INRANGE_PLAYER;

            // ROUNDING_RULE rule 3 (L6 AIDRIVE, owner list 2026-09-27): both arms are `fnmsubs f1, f12
            // (opponent index), f0 (scale), f13 (base)` = -(index * scale - base), ONE rounding
            // (@0x82796138 style 2, @0x82796168 otherwise). Spelt as the negated fma so a zero result
            // keeps fnmsubs' sign (-(+0) = -0), as FX-GATE spells ComputeSpeedRatio's fnmsubs.
            const f32 lfOpponent = static_cast<f32>(miOpponentIndex);
            if (meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE)
                return -(lfOpponent * KF_DESIRED_OPP_SCALE_RACE - KF_DESIRED_OPP_BASE_RACE);
            return -(lfOpponent * KF_DESIRED_OPP_SCALE_DEFAULT - KF_DESIRED_OPP_BASE_DEFAULT);
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
    // NaN polarity (FX-AINAN2): `fcmpu fwd,0.0 ; ble` 0x8276FB14/0x8276FB18 takes the in-front arm
    // on an unordered compare; there `li r11,3 ; fcmpu facing,0.0 ; bgt` 0x8276FB90/0x8276FB94 keeps
    // 3 only for an ordered facing > 0 (a NaN is 2), and behind `fcmpu facing,0.0 ; ble`
    // 0x8276FB48/0x8276FB4C picks 1 for a NaN. Spelt as the negated strict compares.
    // ==================================================================================
    void AICar::UpdateRelativePositionToPlayer(const AICar* lpPlayerCar)
    {
        const Vector3 lThisPos   = GetPosition();
        const Vector3 lPlayerPos = lpPlayerCar->GetPosition();
        const Vector3 lPlayerDir = lpPlayerCar->GetDirection();

        const f32 lfForwardSep = vpu::Dot(lPlayerPos - lThisPos, lPlayerDir);

        s32 liRelative;
        if (!(lfForwardSep > 0.0f))                                // ble 0x8276FB18
        {
            const Vector3 lPlayerDir2 = lpPlayerCar->GetDirection();
            const Vector3 lThisDir    = GetDirection();
            const f32 lfFacingDot = vpu::Dot(lThisDir, lPlayerDir2);
            liRelative = E_RELATIVE_INFRONT_SEPARATING;            // 3
            if (!(lfFacingDot > 0.0f))                             // bgt 0x8276FB94 keeps 3
                liRelative = E_RELATIVE_INFRONT_APPROACHING;       // 2
        }
        else
        {
            const Vector3 lPlayerDir2 = lpPlayerCar->GetDirection();
            const Vector3 lThisDir    = GetDirection();
            const f32 lfFacingDot = vpu::Dot(lThisDir, lPlayerDir2);
            liRelative = !(lfFacingDot > 0.0f) ? E_RELATIVE_BEHIND_SEPARATING   // 1 (ble 0x8276FB4C)
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
        CGS_ASSERT_AT(IsActive(), "IsActive()", CGS_ARTIST_UNITY_BRNAICAR_CPP_FILE, 1712);
        CGS_ASSERT_AT(lpPlayerCar != NULL, "lpPlayerCar != NULL", CGS_ARTIST_UNITY_BRNAICAR_CPP_FILE, 1713);
        CGS_ASSERT_AT(lpAISectionsData != NULL, "lpAISectionsData != NULL", CGS_ARTIST_UNITY_BRNAICAR_CPP_FILE, 1714);

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
                                           CGS_ASSERT_LOCATION(CGS_ARTIST_FILE, 1722));
                CgsDev::Assert::EndAssert();
            }

            // `fcmpu prev, dist ; bgt -> reset` 0x8278B058/0x8278B05C: only an ordered prev > dist
            // resets, so a NaN distance keeps the wrong-way clock running (FX-AINAN2; `<=` reset it).
            if (mbIsDrivenByPlayer && mbIsInGameMode && !mbIsCrashing &&
                GetSpeed() > KF_RACE_DISTANCE_MIN_SPEED &&
                !(lfPrevDistance > mfDistanceToCheckpoint))
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
        const Route* lpRoute = GetRoute();
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
        CGS_ASSERT_AT(IsActive(), "IsActive()", CGS_ARTIST_UNITY_BRNAICAR_CPP_FILE, 1290);

        mbRouteRequested = false;

        Route* lpThisRoute = GetRoute();
        lpThisRoute->Construct(lpRoute);

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
                CGS_ASSERT(false, CGS_ASSERT_MESSAGE_AT("Invalid route finding style", KPC_AICAR_CPP, 1008));
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

        const f32 lfHoldTime = KF_ALT_ROUTE_HOLD_BASE + static_cast<f32>(miOpponentIndex) * KF_ALT_ROUTE_HOLD_PER_OPP;
        if (mfAlternativeRouteTimer < lfHoldTime)                      // bgelr 0x82765E7C
        {
            mfAlternativeRouteTimer = mfAlternativeRouteTimer + lfTimeStep;
            // `bltlr` 0x82765E8C: only an ordered timer < hold returns, so a NaN (a NaN step)
            // requests the route (FX-AINAN2; `>=` did not).
            if (!(mfAlternativeRouteTimer < lfHoldTime))
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
        const Route* lpThisRoute = GetRoute();
        if (miNextRouteNodeIndex >= lpThisRoute->GetNodeCount())
            mfTimeInInvalidSection = 1.0f;

        const f32 lfNewTime = mfTimeInInvalidSection + lfTimeStep;
        mfTimeInInvalidSection = lfNewTime;
        // `fcmpu t, flt_82001C98 (1.0) ; blt` 0x8277C084/0x8277C088 skips only an ordered t < 1,
        // so a NaN dwell time drops the route (FX-AINAN2; `>=` kept it).
        if (!(lfNewTime < 1.0f))
        {
            mfWrongWayTime = 0.0f;
            Route* lpMutableRoute = GetRoute();
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
        const Route* lpThisRoute = GetRoute();
        const bool lbHasValidRoute = (lpThisRoute->GetStatus() != Route::E_STATUS_UNINITIALISED) &&
                                     (lpThisRoute->GetNodeCount() > 0);
        if (!lbHasValidRoute)
            return true;

        return luBest == muDefaultSectionIndex;
    }

    // ==================================================================================
    // GetSectionIndexOfLastSectionInRoute @0x8276B710
    //
    // The AI-section index of the route's final node (the checkpoint section). Asserts the car
    // has a valid, non-empty route, then reads node[count-1].muSectionIndex via Route::GetNode.
    // ==================================================================================
    u16 AICar::GetSectionIndexOfLastSectionInRoute() const
    {
        const Route* lpThisRoute = GetRoute();

        CGS_ASSERT_AT(HasValidRoute(), "HasValidRoute()", KAC_ARTIST_ASSERT_FILE, 978);
        CGS_ASSERT_AT(lpThisRoute->GetNodeCount() > 0, "mRoute.GetNodeCount() > 0", KAC_ARTIST_ASSERT_FILE, 979);

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

        CGS_ASSERT(meCarState == E_AI_CAR_STATE_OUT_OF_RANGE,
            CGS_ASSERT_MESSAGE_AT("AI car in bad state", KAC_ARTIST_ASSERT_FILE, 1105 /* ARTIST 0x82764DB0 */));
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
        CGS_ASSERT(vpu::IsValid(mVelocity), CGS_ASSERT_MESSAGE_AT("Invalid velocity", KAC_ARTIST_ASSERT_FILE, 865));
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
        CGS_ASSERT(vpu::IsValid(mLastGoodPosition), CGS_ASSERT_MESSAGE_AT("Invalid car last good position", KAC_ARTIST_ASSERT_FILE, 872));
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
        CGS_ASSERT(meCarState != E_AI_CAR_STATE_INACTIVE,
            CGS_ASSERT_MESSAGE_AT(
                "GetState() != E_AI_CAR_STATE_INACTIVE", KAC_ARTIST_ASSERT_FILE, 1272 /* ARTIST 0x82764E90 */));
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
        CGS_ASSERT(leBehaviour >= 0,
            CGS_ASSERT_MESSAGE_AT("leBehaviour >= 0", KAC_ARTIST_ASSERT_FILE, 1193 /* ARTIST 0x82764E0C */));
        CGS_ASSERT(leBehaviour < E_AI_BEHAVIOUR_COUNT,
            CGS_ASSERT_MESSAGE_AT(
                "leBehaviour < E_AI_BEHAVIOUR_COUNT", KAC_ARTIST_ASSERT_FILE, 1194 /* ARTIST 0x82764E30 */));

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
        CGS_ASSERT_AT(vpu::IsValid(lRight), "RwMath::IsValid( lRight )", KAC_ARTIST_ASSERT_FILE, 1311);

        if (!BrnMath::IsNormal(lRight))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Right is  (" << lRight.mV.x << "," << lRight.mV.y << "," << lRight.mV.z << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer,
                                       CGS_ASSERT_LOCATION("..\\..\\..\\GameSource\\World/AI/BrnAICar.h", 1312));
            CgsDev::Assert::EndAssert();
        }

        mRight = lRight;
    }
}

// FOLDED FROM BrnAICar_Update.cpp
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"       // the .bss tunables, recovered from their start-up initialisers
#include "GameSource/World/AI/BrnAIDriver.h"   // AIDriver::ResetPIDTuningState / GetRacingLine (UpdateInRangeData / OnModeStart)
#include "GameSource/World/AI/Route/BrnRoute.h"                          // BrnAI::Route, RouteNode
#include "GameSource/World/AI/RaceBalancing/BrnRaceBalancingManager.h"   // RaceBalancingManager::CalculateScheduleOffset
#include "GameSource/World/AI/BrnAIPortal.h"                             // BrnAI::Portal (GetCurrentNodeY reads the portal height)
#include "SharedClasses/AI/AISectionsResourceType.h"                     // BrnAI::AISectionsData / AISection
#include "GameSource/Math/BrnMathUtils.h"                                // BrnMath::IsNormal / Flatten

#include "GameSource/AttribSys/Generated/classes/burnoutcarasset.h"              // SetDriver: key -> car asset
#include "GameSource/AttribSys/Generated/classes/physicsvehiclehandling.h"       // SetDriver: handling record
#include "GameSource/AttribSys/Generated/classes/physicsvehicleboostattribs.h"   // SetDriver: MaxBoostSpeed

#include "GameShared/GameClasses/Numeric/CgsRandom.h"           // CgsNumeric::Random (the BrnAICar.cpp file-scope RNG @.data 0x8300D5D0)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"              // CGS_ASSERT + KI_MESSAGEBUFFERSIZE
#include "GameShared/GameClasses/Development/CgsStrStream.h"    // CgsDev::StrStream (streamed assert messages)
#include "rw/math/vpu/vector3_operation.h"                     // rw::math::vpu Dot / Magnitude / Normalize / Cross / IsValid / IsZero

#include <cmath>   // std::sqrt (the de-SIMD'd rsqrt sequences)

// BrnAI::AICar -- the per-car AI BRAIN TICK and the module-facing state writers.
// Partfile of BrnAICar.cpp (which owns the speed + route-progress core). This unit bodies
// AICar::Update @0x82798F68 and every callee of it that was absent from the tree, plus the
// writers AIModule / ResetOnTrackManager / AIDriver use to feed a car (UpdateInRangeData,
// UpdateOutOfRangeData, UpdateResetOnTrackSection, Reset, OnModeStart, OnModeStartRacing,
// SetDriver, SetRoadRageMadness). Reconstructed store-for-store from the X360 asm; the
// inlined SIMD sequences are reversed into the rw::math::vpu calls they were compiled from.
//
// ---- AICar::Update @0x82798F68 -- callee order (445 insns, 19 callees) --------------------
//   0x82798FFC  BrnAI::AICar::GetPosition                      @0x8276B1F0  (player car)
//   0x827990E0  BrnAI::AICar::UpdateShortcut                   @0x82765D70  (this UNIT; no IDA export -- image.bin disasm)
//   0x827990EC  BrnAI::AICar::UpdateRelativePositionToPlayer   @0x8276FA88  (BrnAICar.cpp)
//   0x8279910C  BrnAI::AICar::CalcDesiredSpeed                 @0x82796078  (BrnAICar.cpp)      [!lbIsOnline]
//   0x82799170  BrnAI::RaceBalancingManager::CalculateScheduleOffset @0x82789E00              [!lbIsOnline && opponent && !player]
//   0x82799218  BrnAI::AICar::GetDirection                     @0x8276B488  (validity asserts, OUT_OF_RANGE arm)
//   0x827992B0  BrnAI::AICar::GetRight                         @0x8276B998  (this UNIT; no IDA export -- image.bin disasm)
//   0x8279934C  BrnAI::AICar::UpdatePositionOutOfRange         @0x8276F060  (this UNIT)        [OUT_OF_RANGE]
//   0x827993DC / 0x8279946C  GetDirection / GetRight again (post-move validity asserts)
//   0x827994FC  BrnAI::AICar::CheckForSectionChange            @0x8277BFD0  (BrnAICar.cpp)     [not OUT_OF_RANGE]
//   0x82799530..0x82799574  streamed "AI car in unknown state " assert (BasePriorityQueue::Clear = StrStream ctor, operator<<)
//   0x827995C8  BrnAI::AICar::UpdateRouteFinding               @0x8278ADB8  (BrnAICar.cpp)
//   0x82799620  BrnAI::AICar::UpdateRaceDistance               @0x8278AEB8  (BrnAICar.cpp)     [style != FREE_ROAM && HasValidRoute && dest valid]
//   0x82799648  BrnAI::AICar::CheckForFreeRoamSwapToPursuit    @0x8276EE68  (this UNIT)
//   (+ CgsDev::Assert::BeginAssert/FireAssert/EndAssert, __savegprlr_22)
//
// Register map @0x82798F7C: r29=this, r31=r4 lpRaceBalancingManager, f31=f1 lfTimeStep,
// r5 = the PPC float-arg skip slot, r23=r6 lpPlayerCar, r22=r7 lpAISectionsData, r28=r8 lpRoute,
// r30=r9 lbIsOnline. (DWARF BrnAICar.h:95: Update(const RaceBalancingManager*, float32_t,
// const AICar*, AISectionsData*, const Route*, bool).) The pseudocode's a4 is the skip slot.
//
// ---- second-hand callees that had NO IDA export (recovered from image.bin with capstone) ----
//   UpdateShortcut          @0x82765D70 (38 insns)   GetRight             @0x8276B998
//   MoveToSectionOnRoute    @0x8276ED40 (74 insns)   SetNextRouteNodeIndex @0x82764C48 (bl target of UpdatePositionOutOfRange 0x8276F288)
//   UpdateRouteFindingFreeRoam @0x8277C0C0 (22 insns)
//
// ---- rodata constants read from image.bin (BE, VA-0x82000000) ----------------------------
//   flt_820C41C0 = 4.0   (wrong-way limit)       flt_820C3B70 = 1.1920929e-7 (FLT_EPSILON, IsZero tolerance)
//   flt_820C3FAC = 100.0 (free-roam -> pursuit)  flt_820C5E90 = 750.0 (pursuit -> free-roam)
//   flt_820C4318 = 200.0 (AI route-old distance) flt_820C4244 = 50.0 (player route-old distance)
//   flt_820C5E94 = 1/130 (section speed -> buzz ratio)   flt_82F31928 = 0.44704 (mph -> m/s)
//   flt_820C3D90 = 1.1   (max-player-speed scale)        flt_82F302F4 = FLT_MAX
//   flt_820C41A0[2] = {0.0, 0.5} (Reset: per-personality base aggression)
//   flt_82F30170 / flt_82F30174 = 0.2 / 1.0 (OnModeStart MARKED_MAN aggression band)
//   flt_820C4188[2] = {0.1, 0.1} / flt_820C4190[2] = {0.6, 0.6} (OnModeStart per-personality band)
//   unk_82181510 / unk_82181520 = gJVector (0,1,0,0) / gKVector (0,0,1,0); gIVector @0x82181500 = (1,0,0,0)

namespace BrnAI
{
    namespace vpu = rw::math::vpu;

    Vector3 AICar::GetVelocityDirection() const
    {
        if (vpu::MagnitudeSquared(mVelocity) > KF_USEFUL_DIRECTION_MIN_SPEED * KF_USEFUL_DIRECTION_MIN_SPEED)
            return vpu::Normalize(GetVelocity());
        return GetDirection();
    }

    // ===== file-local constants (every value read from the image; addresses in the banner) =====
    const f32 KF_AICAR_FLOAT_EPSILON              = 1.1920929e-7f;   // flt_820C3B70
    const f32 KF_WRONG_WAY_TIME_LIMIT             = 4.0f;            // flt_820C41C0
    const f32 KF_FREE_ROAM_TO_PURSUIT_DISTANCE    = 100.0f;          // flt_820C3FAC
    const f32 KF_PURSUIT_TO_FREE_ROAM_DISTANCE    = 750.0f;          // flt_820C5E90
    const f32 KF_FREE_ROAM_ROUTE_OLD_DISTANCE     = 300.0f;          // IsFreeRoamingCarsRouteOld literal (Hex-Rays decoded)
    const f32 KF_ROUTE_OLD_DISTANCE_AI            = 200.0f;          // flt_820C4318
    const f32 KF_ROUTE_OLD_DISTANCE_PLAYER        = 50.0f;           // flt_820C4244
    const f32 KF_SECTION_SPEED_TO_BUZZ_RATIO      = 0.0076923077f;   // flt_820C5E94 (1/130)
    const f32 KF_MPH_TO_MPS                       = 0.44704f;        // flt_82F31928
    const f32 KF_MAX_PLAYER_SPEED_SCALE           = 1.1f;            // flt_820C3D90
    const f32 KF_AICAR_FLT_MAX                    = 3.4028234663852886e+38f;  // flt_82F302F4 (0x7F7FFFFF)
    // DWARF BrnAICar.cpp:32 KF_DEFAULT_OUT_OF_RANGE_SPEED. flt_8300D7FC is .bss (hence the 0
    // in image.bin); its ONLY writer is the start-up leaf initialiser at 0x82C68580, which does
    //   lfs f0, flt_82F31928(=0.44704) ; lfs f13, flt_82004A18(=80.0) ; fmuls ; stfs flt_8300D7FC
    // == 80 mph -> 35.7632 m/s. AICar::Construct seeds mfSpeedOutOfRange (mfDesiredSpeed) with it.
    const f32 KF_DEFAULT_OUT_OF_RANGE_SPEED       = 80.0f * 0.44704f;   // flt_8300D7FC (dyn-init 0x82C68580)
    const f32 KAF_PERSONALITY_BASE_AGGRESSION[E_PERSONALITY_TYPE_COUNT] = { 0.0f, 0.5f };   // flt_820C41A0[personality]
    f32 gfMarkedManMinAggression = 0.2f; // ARTIST 0x82F30170
    f32 gfMarkedManMaxAggression = 1.0f; // ARTIST 0x82F30174
    const f32 KAF_RANK_AGGRESSION_LO[E_PERSONALITY_TYPE_COUNT] = { 0.1f, 0.1f };   // unk_820C4188[personality]
    const f32 KAF_RANK_AGGRESSION_HI[E_PERSONALITY_TYPE_COUNT] = { 0.6f, 0.6f };   // unk_820C4190[personality]
    // GetUsefulDirection's "velocity is trustworthy above this speed" threshold lives in
    // BrnAICar_Constants.h: flt_8300D964 is .bss (hence the 0 in image.bin), written at start-up
    // by the leaf initialiser 0x82C685B8 as flt_820C4890 (20 mph) * 0.44704 == 8.9408 m/s.
    // (The 1.0f placeholder shipped here before 2026-09-04 was ~9x too permissive.)
    const f32 KF_USEFUL_DIRECTION_MIN_LENGTH      = 0.0099999998f;   // literal (Hex-Rays decoded)

    // vpu::IsZero(v, tolerance) as the console inlines it at UpdatePositionOutOfRange 0x8276F194
    // and UpdateOutOfRangeData 0x8276F550: vandc (|v|), vrlimi128 (w := x), vcmpgtfp. |v| >
    // splat(tolerance), then the CR6 "all false" bit (extrwi r11,r11,1,26). "No lane above the
    // tolerance" is what counts as zero -- and a NaN lane is never above it. The shared
    // vpu::IsZero tests |lane| <= tolerance and calls a NaN lane non-zero, so these two sites
    // spell the console form locally (crash parity FX-AINAN2).
    static inline bool IsZeroVmx(const Vector3& lrVector, f32 lfTolerance)
    {
        return !(std::fabs(lrVector.X().GetFloat()) > lfTolerance) &&
               !(std::fabs(lrVector.Y().GetFloat()) > lfTolerance) &&
               !(std::fabs(lrVector.Z().GetFloat()) > lfTolerance);
    }

    // The console's file strings (the unity .cpp path for .cpp-line asserts, the relative path
    // for the inlined BrnAICar.h accessors).
    static const char* const KPC_AICAR_H   = "..\\..\\..\\GameSource\\World/AI/BrnAICar.h";

    // ===== the BrnAICar.cpp file-scope RNG (.data 0x8300D5D0) ==============================
    // IDENTIFIED 2026-09-04 (this lane), replacing the opaque "11-word block" twin that stood
    // here: 0x8300D5D0 is a `CgsNumeric::Random`, and the unrolled 11-store blob that AICar::
    // Construct @0x82792754..0x827927C8 and AICar::Reset @0x827928E8..0x8279296C both emit is
    // `CgsNumeric::Random::Construct()` INLINED. Proof (exact, not a resemblance): running the
    // committed CgsRandom.h Construct -- seed KU_RANDOM_DEFAULT_SEED 0xC87CD8C91AD0891B, slot 0
    // = 1.0f, then seven AddRandomFloatToBuffer draws with KU_RANDOM_MULTIPLIER -- reproduces
    // all eleven stored words bit for bit:
    //   mauIntegerBuffer[0..7] = 3F800000 3FE43E6C 3F98B09C 3FDA23E0 3FE21EDC 3FDDEB96
    //                            3F9C9A72 3F923D76   (the ring's 8 floats in [1,2))
    //   muSeed (+0x20, `std`)  = B5E330D0_2EC654DA   muOldestBufferIndex (+0x28) = 0
    // ⚠️ THE OLD TWIN HAD THE SEED'S HIGH WORD WRONG (0x00000001 instead of 0xB5E330D0): the
    // `rldimi r10, r4, 0x20, 0` at 0x82792954 packs r4 -- built by `lis r4,-0x4A1D ; ori
    // r4,r4,0x30D0` at 0x827928C8/0x827928D0 == 0xB5E330D0 -- into the HIGH half. Fixed here.
    // ⭐ THE CONSUMER IS `AICar::GetRandomNumber()` (DWARF BrnAICar.h:476, dump :550), bodied just
    // below. The console INLINES it into its callers -- which is why the only ARTIST code that
    // reads 0x8300D5D0 is BrnAI::ResetOnTrackManager (findinit: ResetAwayFromPlayer 0x827841BC,
    // ResetAheadFromSideTurnings 0x82790A60, and the ScanForwardsAndAlongJunction export hole
    // 0x82785314 -- the three DWARF BrnResetOnTrackManager.cpp AICar::GetRandomNumber call sites),
    // while the only writers are AICar::Construct 0x827926F8 and AICar::Reset 0x827928F0. Its
    // sibling at 0x8300D5A0 is BuzzBy's, already homed as BrnAIBuzzBy.cpp's mRandom.
    // DWARF HOME (PS3 DecFIGS, for the eventual rename -- NOT a behaviour difference): the PS3
    // symbol for this object is `BrnAI::Aggressiveness::mRandom` (_ZN5BrnAI14Aggressiveness7mRandomE).
    // PS3 AICar::Reset @0x9C68EC calls Aggressiveness::Construct(this+5132) (@0x9BB010: re-seeds
    // mRandom), and ResetAwayFromPlayer @0xA122A8 draws via Aggressiveness::GetRandomNumber
    // (lpPlayerAICar+5132) @0x9BADD8 -- i.e. AICar::GetRandomNumber is mAggressiveness.
    // GetRandomNumber(). Neither Aggressiveness body exists in this tree yet (BrnAIAggressiveness.cpp),
    // so the stream stays homed here; moving it there is a pure refactor of this block + the two
    // Construct() call sites below.
    // ONE STREAM (crash parity 2026-09-22, G01-D3 / G07-D6): all three console readers draw
    // lpPlayerAICar->GetRandomNumber() (BrnResetOnTrackManager_Strategies.cpp) --
    // ResetAwayFromPlayer (its private never-re-seeded copy is gone), ResetAheadFromSideTurnings
    // (0x82790A60: draw * 40 + 160) and ScanForwardsAndAlongJunction (hole 0x82785314, draw
    // discarded) -- so an ATTACH_AI_CONTROL Reset restarts the reset-on-track draws exactly as on
    // the console.
    static CgsNumeric::Random gAICarRandom;

    // GetRandomNumber (DWARF BrnAICar.h:476, inlined on the console). The inline expansion in
    // ResetAwayFromPlayer 0x827841C0..0x82784220 is exactly Random::RandomFloat() on 0x8300D5D0:
    // ld seed +0x20 ; lwz oldest +0x28 ; lfsx f31 = ring[oldest] ; stwx ring[oldest] =
    // 0x3F800000 | (oldSeed.hi >> 9) (inslwi 23,9) ; std seed*0x5851F42D4C957F2D+1 ;
    // stw (oldest+1)&7 ; fsubs f0 = f31 - 1.0 (flt_82001C98).
    f32 AICar::GetRandomNumber() const
    {
        return gAICarRandom.RandomFloat();
    }

    // ==================================================================================
    // Update @0x82798F68 -- the per-frame AI brain tick. See the banner for the callee order.
    // ==================================================================================
    void AICar::Update(const RaceBalancingManager* lpRaceBalancingManager, f32 lfTimeStep,
                       const AICar* lpPlayerCar, AISectionsData* lpAISectionsData,
                       const Route* lpRoute, bool lbIsOnline)
    {
        CGS_ASSERT(IsActive(), CGS_ASSERT_MESSAGE_AT("IsActive()", __FILE__, 866));                                              // :866

        // 0x82798FE8  mfBehaviourTime += dt
        mfBehaviourTimer = mfBehaviourTimer + lfTimeStep;

        // 0x82798FFC..0x82799068  mfDistanceToPlayer = |mPosition - playerPos| (zero-guarded rsqrt)
        const Vector3 lPlayerPos = lpPlayerCar->GetPosition();
        mfBuzzDistanceToPlayer = vpu::Magnitude(mPosition - lPlayerPos);

        // 0x8279906C  the console tests lane X only (vspltw v0,v0,0 ; vcmpeqfp.)
        CGS_ASSERT(mPosition.X().GetFloat() == mPosition.X().GetFloat(), CGS_ASSERT_MESSAGE_AT("AI car with nonsense position", __FILE__, 871));            // :871

        // 0x827990A4  race timer: held at 0 while starting, else accumulates (not in free roam)
        if (meRouteFindingStyle != E_ROUTE_FINDING_FREE_ROAM)
        {
            if (mbForceStandardRoute)            // == DWARF mbIsStartingRace @+0x1548
                mfRaceTimer = 0.0f;
            else
                mfRaceTimer = mfRaceTimer + lfTimeStep;
        }

        UpdateShortcut(lpAISectionsData);                                                   // 0x827990E0
        UpdateRelativePositionToPlayer(lpPlayerCar);                                        // 0x827990EC

        if (!lbIsOnline)                                                                    // 0x827990F0
        {
            const f32 lfDesiredSpeed = CalcDesiredSpeed(lpRaceBalancingManager, lpAISectionsData, lpPlayerCar);
            mfSpeedOutOfRange = lfDesiredSpeed;   // == DWARF mfDesiredSpeed @+0x14E8 (stfs f1,0x14E8)
            // `fcmpu f1, 0.0 ; bge` 0x82799114/0x82799118 skips the assert on an unordered compare.
            CGS_ASSERT(!(lfDesiredSpeed < 0.0f), CGS_ASSERT_MESSAGE_AT("mfDesiredSpeed >= 0.0f", __FILE__, 897));                 // :897

            // 0x82799138  opponents only (miOpponentIndex != -1 && !mbIsPlayer)
            if (miOpponentIndex != -1 && !mbIsPlayer)
                lpRaceBalancingManager->CalculateScheduleOffset(this, &mfScheduleOffset0);  // &mafScheduleOffsets[0] (+0x1518)
        }

        const EAICarState leCarState = meCarState;                                          // 0x82799174 (read BEFORE the store below)
        mbUseChosenDistanceFunction = lbIsOnline ? 1 : 0;   // == DWARF mbIsOnline @+0x1550 (stb r30,0x1550)

        if (leCarState == E_AI_CAR_STATE_OUT_OF_RANGE)
        {
            CGS_ASSERT(vpu::IsValid(mPosition), CGS_ASSERT_MESSAGE_AT("RwMath::IsValid( mPosition )", __FILE__, 912));            // :912
            CGS_ASSERT(vpu::IsValid(GetDirection()), CGS_ASSERT_MESSAGE_AT("RwMath::IsValid( GetDirection() )", __FILE__, 913));   // :913
            CGS_ASSERT(vpu::IsValid(GetRight()), CGS_ASSERT_MESSAGE_AT("RwMath::IsValid( GetRight() )", __FILE__, 914));           // :914

            UpdatePositionOutOfRange(lfTimeStep, lpAISectionsData);                         // 0x8279934C

            CGS_ASSERT(vpu::IsValid(mPosition), CGS_ASSERT_MESSAGE_AT("RwMath::IsValid( mPosition )", __FILE__, 919));            // :919
            CGS_ASSERT(vpu::IsValid(GetDirection()), CGS_ASSERT_MESSAGE_AT("RwMath::IsValid( GetDirection() )", __FILE__, 920));   // :920
            CGS_ASSERT(vpu::IsValid(GetRight()), "RwMath::IsValid( GetRight() )");           // :921
        }
        else
        {
            CheckForSectionChange(lfTimeStep, lpAISectionsData, lpRoute);                   // 0x827994FC

            if (meCarState != E_AI_CAR_STATE_IN_RANGE)                                      // 0x82799500
            {
                // 0x8279950C..0x82799588  streamed assert through the shared message buffer
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStream << "AI car in unknown state " << static_cast<s32>(meCarState) << "\n";
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_AICAR_CPP, 928));
                CgsDev::Assert::EndAssert();
            }
        }

        // 0x8279958C  last good position: on the ground and not crashing, or in showtime
        if ((!mbIsCrashing && !mbIsInAir) || mbIsInShowtime)
            mLastGoodPosition = mPosition;

        UpdateRouteFinding(lfTimeStep, lpPlayerCar);                                        // 0x827995C8

        // 0x827995CC  race-distance bookkeeping: any real (non-free-roam) style with a live route
        // and a valid destination section
        if (meRouteFindingStyle != E_ROUTE_FINDING_FREE_ROAM &&
            HasValidRoute() &&
            muDestinationSectionIndex != KI_INVALID_SECTION_INDEX)
        {
            UpdateRaceDistance(lpAISectionsData, lpPlayerCar, lfTimeStep);                  // 0x82799620

            // 0x82799624  wrong way for more than 4 s -> drop the route (it will be re-requested)
            if (mfWrongWayTime > KF_WRONG_WAY_TIME_LIMIT)
            {
                mfWrongWayTime = 0.0f;
                Route* lpThisRoute = GetRoute();
                lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;   // stw 0,0x1408
                lpThisRoute->miNodeCount = 0;                                // stw 0,0x1400
            }
        }

        CheckForFreeRoamSwapToPursuit();                                                    // 0x82799648
    }

    // ==================================================================================
    // HasValidRoute -- DWARF BrnAICar.h:262. X360-inlined at every site as
    //   (mRoute.meStatus != E_STATUS_UNINITIALISED && mRoute.miNodeCount > 0)
    // (e.g. Update @0x827995D8: lwz 0x1408 / lwz 0x1400 / cmpwi ,0 / bgt). No standalone symbol.
    // ==================================================================================
    bool AICar::HasValidRoute() const
    {
        const Route* lpThisRoute = GetRoute();
        return lpThisRoute->GetStatus() != Route::E_STATUS_UNINITIALISED &&
               lpThisRoute->GetNodeCount() > 0;
    }

    // ==================================================================================
    // GetPosition @0x8276B1F0 / GetDirection @0x8276B488 / GetRight @0x8276B998
    //
    // The sret transform accessors. Each runs the per-lane rw::math::vpu::IsValid NaN test on
    // the stored vector, asserts, (direction/right also assert BrnMath::IsNormal) and copies the
    // vector into the return slot. GetRight has no IDA export; its body was disassembled from
    // image.bin (0x8276B998..0x8276BA7C) and is the exact twin of GetDirection over mUnitRight
    // (assert strings @0x820C5C20 / @0x820C5C00, BrnAICar.h:1320 / :1321).
    // ==================================================================================
    Vector3 AICar::GetPosition() const
    {
        CGS_ASSERT(vpu::IsValid(mPosition), CGS_ASSERT_MESSAGE_AT("Invalid car position", KAC_ARTIST_ASSERT_FILE, 839));
        return mPosition;
    }

    Vector3 AICar::GetDirection() const
    {
        CGS_ASSERT(vpu::IsValid(mDirection), CGS_ASSERT_MESSAGE_AT("Invalid car direction", KAC_ARTIST_ASSERT_FILE, 856));
        CGS_ASSERT(BrnMath::IsNormal(mDirection), CGS_ASSERT_MESSAGE_AT("BrnMath::IsNormal( mDirection )", KAC_ARTIST_ASSERT_FILE, 857));
        return mDirection;
    }

    Vector3 AICar::GetRight() const
    {
        CGS_ASSERT(vpu::IsValid(mRight), "RwMath::IsValid( mUnitRight )");                  // BrnAICar.h:1320
        CGS_ASSERT(BrnMath::IsNormal(mRight), "BrnMath::IsNormal( mUnitRight )");            // BrnAICar.h:1321
        return mRight;
    }

    // ==================================================================================
    // SetDirection @0x8276B2C0
    //
    // Stores the car's facing (mDirection @+0x1440). Asserts the input is a valid (non-NaN)
    // vector; a non-unit input streams its components into the assert message
    // ("Diretion is  (x,y,z\n" -- the console's own spelling). The store is UNCONDITIONAL.
    // ==================================================================================
    void AICar::SetDirection(Vector3 lDirection)
    {
        CGS_ASSERT_AT(vpu::IsValid(lDirection), "RwMath::IsValid( lDirection )", KAC_ARTIST_ASSERT_FILE, 847);              // BrnAICar.h:847

        if (!BrnMath::IsNormal(lDirection))
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Diretion is  (" << lDirection.X().GetFloat() << "," << lDirection.Y().GetFloat() << "," << lDirection.Z().GetFloat() << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_AICAR_H, 848));
            CgsDev::Assert::EndAssert();
        }

        mDirection = lDirection;                                                            // stvx128 v127,r26,0x1440
    }

    // ==================================================================================
    // GetUsefulDirection @0x82770028 (DWARF BrnAICar.cpp:2268, local `Vector3 lXZSpeed` :2270)
    //
    // A direction the aggression geometry can trust. BOTH gates measure a FLATTENED copy (the
    // DWARF's Vector3::SetY(0) before each Magnitude); what they return is the full 3D vector:
    //   0x82770050  bl GetVelocity -> sp+0x60 ; 0x8277008C stvx128 the copy back to sp+0x60
    //   0x827700A4  stfs f31 (flt_82001CC0 == 0.0) -> sp+0x64, the copy's Y lane
    //   0x827700BC  vmsum3fp128 v0,v0,v0 on the reloaded copy; rsqrt magnitude, vsel 0 guard
    //   0x827700FC  vcmpgefp. |xz| >= splat(flt_8300D964 == KF_USEFUL_DIRECTION_MIN_SPEED)
    //   0x82770118  bl GetVelocity AGAIN -> Normalize of the unflattened velocity
    //   0x82770164  bl GetDirection -> sp+0x70 ; 0x827701A0 stfs f31 (0.0) -> sp+0x74 (Y lane)
    //   0x827701F4  vcmpgtfp. |xz| > flt_82002138 (0.01)
    //   0x82770210  bl GetDirection into the sret -> the unflattened facing
    // then Cross(direction, right) if THAT has length (0x8277027C: unflattened, unnormalised;
    // vpermwi 0x63 = yzx twice around a vmulfp/vnmsubfp = the SDK cross), and finally the X axis
    // (1,0,0). So a car whose speed is mostly vertical (bounced by a hit, landing) still reports
    // its facing, and a near-vertical facing falls through to the cross.
    // ==================================================================================
    Vector3 AICar::GetUsefulDirection() const
    {
        Vector3 lXZSpeed = GetVelocity();
        lXZSpeed.SetY(0.0f);                                                                  // 0x827700A4
        if (vpu::Magnitude(lXZSpeed) >= KF_USEFUL_DIRECTION_MIN_SPEED)                      // 0x827700FC
            return vpu::Normalize(GetVelocity());                                           // 0x82770118

        lXZSpeed = GetDirection();
        lXZSpeed.SetY(0.0f);                                                                  // 0x827701A0
        if (vpu::Magnitude(lXZSpeed) > KF_USEFUL_DIRECTION_MIN_LENGTH)                      // 0x827701F4
            return GetDirection();                                                          // 0x82770210

        const Vector3 lRight = GetRight();
        const Vector3 lCross = vpu::Cross(GetDirection(), lRight);
        if (vpu::Magnitude(lCross) > KF_USEFUL_DIRECTION_MIN_LENGTH)
            return lCross;                                                                  // NOT normalised on the console

        { const Vector3 lResult(1.0f, 0.0f, 0.0f); return lResult; }
    }

    // ==================================================================================
    // UpdateShortcut @0x82765D70 (no IDA export; image.bin disasm, 38 insns)
    //
    //   0x82765D88  lhz best(0x1534) ; 0x7FFF -> lhz default(0x1532)   == GetBestSectionIndex()
    //   0x82765DA4  == 0x7FFF -> mbIsInShortcut = 0
    //   0x82765DC4  bl AISectionsData::GetAISection(sections, best)
    //   0x82765DC8  lbz flags(section+0x17) ; bit0 (clrlwi 31) || bit 0x40 (rlwinm mask 25..25)
    //   0x82765DEC  stb -> mbIsInShortcut (0x154C)
    // The flag test is the same pair AISection::IsUnsuitableForResetOnTrackLink reads.
    // ==================================================================================
    void AICar::UpdateShortcut(AISectionsData* lpAISectionsData)
    {
        const u16 luSection = GetBestSectionIndex();
        if (luSection == KI_INVALID_SECTION_INDEX)
        {
            mbIsInShortcut = false;
            return;
        }

        const AISection* lpSection = lpAISectionsData->GetAISection(luSection);
        const u8 lu8Flags = lpSection->mx8Flags;
        mbIsInShortcut = ((lu8Flags & 0x01) != 0) || ((lu8Flags & 0x40) != 0);
    }

    // ==================================================================================
    // CheckForFreeRoamSwapToPursuit @0x8276EE68
    //
    // FREE_ROAM -> PURSUIT when the player is within 100 m and this car has a proximity slot
    // (miProximityIndex >= 0); PURSUIT -> FREE_ROAM when the player is beyond 750 m OR the
    // proximity slot is gone. Either swap drops the route (status/count), zeroes the wrong-way
    // timer, bumps the route time stamp and sets the speed-selection method to match
    // (PERSONALITY(3) for pursuit, FREE_ROAM(0) for free roam).
    // ==================================================================================
    void AICar::CheckForFreeRoamSwapToPursuit()
    {
        Route* lpThisRoute = GetRoute();

        if (meRouteFindingStyle == E_ROUTE_FINDING_FREE_ROAM)
        {
            // 0x8276EE80 `fcmpu dist, 100.0 ; bgelr` returns on >= AND on unordered: a NaN
            // distance never swaps to pursuit (FX-AINAN2; `>=` let it through).
            if (!(mfBuzzDistanceToPlayer < KF_FREE_ROAM_TO_PURSUIT_DISTANCE))
                return;
            if (miProximityIndex < 0)                                          // 0x8276EE90 bltlr
                return;

            mfWrongWayTime          = 0.0f;
            lpThisRoute->miNodeCount = 0;
            lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;
            ++miRouteTimeStamp;
            meRouteFindingStyle     = E_ROUTE_FINDING_PURSUIT;
            meSpeedSelectionMethod  = E_AI_SPEED_SELECTION_METHOD_PERSONALITY;
            return;
        }

        if (meRouteFindingStyle != E_ROUTE_FINDING_PURSUIT)                    // 0x8276EECC bnelr
            return;

        // 0x8276EED0  swap back when far away, or (else) when the proximity slot is gone
        if (!(mfBuzzDistanceToPlayer > KF_PURSUIT_TO_FREE_ROAM_DISTANCE) && miProximityIndex >= 0)
            return;

        mfWrongWayTime          = 0.0f;
        meRouteFindingStyle     = E_ROUTE_FINDING_FREE_ROAM;
        ++miRouteTimeStamp;
        meSpeedSelectionMethod  = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM;
        lpThisRoute->miNodeCount = 0;
        lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;
    }

    // ==================================================================================
    // GetCurrentNodeY @0x8276EFD8
    //
    // The height of the current route node: the node's section portal's Y. The route node's
    // byte at node+0xE is the portal index within the node's section (lbz @0x8276F008).
    // ==================================================================================
    f32 AICar::GetCurrentNodeY(AISectionsData* lpAISectionsData)
    {
        const RouteNode*  lpNode    = GetRoute()->GetNode(miNextRouteNodeIndex);            // lwz 0x1524
        const AISection*  lpSection = lpAISectionsData->GetAISection(lpNode->muSectionIndex);   // lhz node+0xC
        const Portal*     lpPortal  = lpSection->GetPortal(lpNode->muPortalIndex); // lbz node+0xE
        return lpPortal->GetPositionY();                                                    // Portal[1]
    }

    // ==================================================================================
    // SetNextRouteNodeIndex @0x82764C48 (no IDA export; image.bin disasm)
    //
    // Range-checked setter for miNextRouteNodeIndex. An index >= the node count streams
    // "Trying to set <i> of <count>\n" (:968); a negative one asserts (:969). The store is
    // unconditional.
    // ==================================================================================
    void AICar::SetNextRouteNodeIndex(s32 liNodeIndex)
    {
        const s32 liNodeCount = GetRoute()->GetNodeCount();
        if (liNodeIndex >= liNodeCount)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Trying to set " << liNodeIndex << " of " << liNodeCount << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_AICAR_H, 968));
            CgsDev::Assert::EndAssert();
        }
        CGS_ASSERT(liNodeIndex >= 0, "liNextRouteNodeIndex >= 0");                          // BrnAICar.h:969

        miNextRouteNodeIndex = liNodeIndex;                                                 // stw r28,0x1524
    }

    // ==================================================================================
    // MoveToSectionOnRoute @0x8276ED40 (no IDA export; image.bin disasm, 74 insns)
    //
    // Two searches for luSectionIndex:
    //   1. (player car only, not yet on the master route, master route non-empty) scan the
    //      MASTER route (lpRoute) from node 0; on a hit copy it into mRoute (Route::Construct
    //      @0x82767B28), set mbIsInMasterRoute, seed the next node and zero the wrong-way timer.
    //   2. otherwise scan this car's own route in the window
    //      [max(0, next-4) .. min(count-1, next+32)]; on a hit just seed the next node.
    // Returns whether a section was found.
    // ==================================================================================
    bool AICar::MoveToSectionOnRoute(u16 luSectionIndex, const Route* lpRoute)
    {
        if (!mbIsInMasterRoute && mbIsPlayer && lpRoute->GetNodeCount() > 0)                // 0x8276ED58..0x8276ED7C
        {
            for (s32 liNode = 0; liNode < lpRoute->GetNodeCount(); ++liNode)
            {
                if (lpRoute->GetNode(liNode)->GetSectionIndex() == luSectionIndex)          // 0x8276ED90
                {
                    Route* lpThisRoute = GetRoute();
                    lpThisRoute->Construct(lpRoute);                                       // 0x8276EE24 bl Route::Construct
                    mbIsInMasterRoute = true;                                               // 0x8276EE30
                    SetNextRouteNodeIndex(liNode);                                          // 0x8276EE34
                    mfWrongWayTime = 0.0f;                                                  // 0x8276EE44
                    return true;
                }
            }
        }

        // 0x8276EDAC  the local window around the current next node
        const s32 liNext  = miNextRouteNodeIndex;
        s32 liFirst = liNext - 4;
        if (liFirst < 0)
            liFirst = 0;
        s32 liLast = GetRoute()->GetNodeCount() - 1;
        if (liLast >= liNext + 0x20)
            liLast = liNext + 0x20;
        if (liFirst > liLast)
            return false;

        for (s32 liNode = liFirst; liNode <= liLast; ++liNode)
        {
            if (GetRoute()->GetNode(liNode)->GetSectionIndex() == luSectionIndex)           // 0x8276EDF8
            {
                SetNextRouteNodeIndex(liNode);                                              // 0x8276EE58
                return true;
            }
        }
        return false;
    }

    // ==================================================================================
    // UpdatePositionOutOfRange @0x8276F060
    //
    // The out-of-range car is not simulated: it is walked along its route. With a live route
    // and a next node still ahead, the target is (node.x, portalY, node.z); the node's section
    // becomes both the default and best section. Unless the car is already on the node
    // (|delta| <= FLT_EPSILON on every lane), it turns to face the node and moves along its
    // PREVIOUS facing by desiredSpeed*dt; if that step covers the remaining distance (or the
    // car was already on the node) the next node is advanced -- past the last node the route
    // is dropped (status/count/wrong-way).
    // ==================================================================================
    void AICar::UpdatePositionOutOfRange(f32 lfTimeStep, AISectionsData* lpAISectionsData)
    {
        CGS_ASSERT(IsActive(), CGS_ASSERT_MESSAGE_AT("IsActive()", __FILE__, 1218));                                              // :1218

        if (!HasValidRoute())                                                               // 0x8276F0DC
            return;
        Route* lpThisRoute = GetRoute();
        if (miNextRouteNodeIndex >= lpThisRoute->GetNodeCount())                            // 0x8276F110
            return;

        const RouteNode* lpNode = lpThisRoute->GetNode(miNextRouteNodeIndex);               // 0x8276F11C
        const f32 lfNodeX = lpNode->GetX();
        const f32 lfNodeZ = lpNode->GetY();                                                 // lfs f31,4(node)
        const f32 lfNodeY = GetCurrentNodeY(lpAISectionsData);                              // 0x8276F138
        const Vector3 lTarget(lfNodeX, lfNodeY, lfNodeZ);

        muDefaultSectionIndex = lpNode->GetSectionIndex();                                  // sth 0x1532
        muBestSectionIndex    = lpNode->GetSectionIndex();                                  // sth 0x1534

        const Vector3 lDelta = lTarget - mPosition;                                         // vsubfp (target - mPosition)
        bool lbAdvance = true;
        if (!IsZeroVmx(lDelta, KF_AICAR_FLOAT_EPSILON))                                     // 0x8276F194 vcmpgtfp. |delta| > eps
        {
            const f32 lfDistanceSq = vpu::Dot(lDelta, lDelta);                              // vmsum3fp128 v127
            const Vector3 lStep = mDirection * (mfSpeedOutOfRange * lfTimeStep);            // OLD facing (lvx 0x1440 before the call) * desired speed * dt
            SetDirection(lDelta * (1.0f / std::sqrt(lfDistanceSq)));                        // 0x8276F240 -- refined rsqrt, no zero guard (delta is non-zero here)
            mPosition = mPosition + lStep;                                                  // stvx128 0x1430
            lbAdvance = vpu::Dot(lStep, lStep) >= lfDistanceSq;                             // 0x8276F258 vcmpgefp.
        }

        if (lbAdvance)                                                                      // 0x8276F26C
        {
            if (miNextRouteNodeIndex >= lpThisRoute->GetNodeCount() - 1)
            {
                lpThisRoute->miNodeCount = 0;                                               // stw 0,0x1400
                lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;                   // stw 0,0x1408
                mfWrongWayTime = 0.0f;                                                      // stfs 0x14F4
            }
            else
            {
                SetNextRouteNodeIndex(miNextRouteNodeIndex + 1);                            // 0x8276F288
            }
        }
    }

    // ==================================================================================
    // IsExtrapolatedRouteGettingOld @0x8276FD50
    //
    // True when the (pursuit/road-rage/extrapolated) route should be re-requested: the car
    // has drifted more than 200 m (50 m for the player) from where the route was built
    // (mLastRoutePosition), or it has no usable route, or the node after the current one
    // (clamped to the last node) is BEHIND the car (dot(direction, node - position) < 0).
    // The pseudocode dropped the `min(next+1, count-1)` node choice (0x8276FE7C..0x8276FE88).
    // ==================================================================================
    bool AICar::IsExtrapolatedRouteGettingOld()
    {
        const f32 lfDrift = vpu::Magnitude(mLastRoutePosition - mPosition);                 // lvx 0x1480 - lvx 0x1430
        const f32 lfLimit = mbIsPlayer ? KF_ROUTE_OLD_DISTANCE_PLAYER : KF_ROUTE_OLD_DISTANCE_AI;
        // `fcmpu f13, f0 ; ble 0x8276FE0C` @0x8276FDFC. ble is `bc 4,gt` -- taken whenever GT is
        // clear, which after fcmpu includes the UNORDERED result -- so only an ordered drift ABOVE
        // the limit falls through to `li r3, 1` ("old"); a NaN drift goes on to the route tests below,
        // exactly as `lfDrift > lfLimit` reads it. (cccfeed8 had inverted this for NaN.)
        if (lfDrift > lfLimit)
            return true;

        const Route* lpThisRoute = GetRoute();
        if (!(HasValidRoute() && lpThisRoute->GetNodeCount() > 1))                          // 0x8276FE0C..0x8276FE64
            return true;
        if (lpThisRoute->GetNodeCount() - miNextRouteNodeIndex < 1)                         // 0x8276FE74
            return true;

        s32 liNode = miNextRouteNodeIndex + 1;                                              // 0x8276FE7C
        if (liNode >= lpThisRoute->GetNodeCount())
            liNode = lpThisRoute->GetNodeCount() - 1;
        const RouteNode* lpNode = lpThisRoute->GetNode(liNode);

        const Vector3 lNodePos(lpNode->GetX(), 0.0f, lpNode->GetY());
        const Vector3 lToNode = lNodePos - GetPosition();                                   // vsubfp128 v0 = node - pos
        const f32 lfAhead = vpu::Dot(GetDirection(), lToNode);                              // vmsum3fp128
        return 0.0f > lfAhead;                                                              // vcmpgtfp. zero > dot
    }

    // ==================================================================================
    // IsFreeRoamingCarsRouteOld @0x8276FBD0
    //
    // A free-roaming AI car's route is "old" when it has no usable route (count <= 1), or
    // when it is within 300 m of the route's LAST node -- in which case the destination
    // section is also invalidated so a new one gets picked. A crashing car never re-asks.
    // ==================================================================================
    bool AICar::IsFreeRoamingCarsRouteOld()
    {
        if (mbIsCrashing)
            return false;

        const Route* lpThisRoute = GetRoute();
        if (!(HasValidRoute() && lpThisRoute->GetNodeCount() > 1))
            return true;

        const RouteNode* lpLast = lpThisRoute->GetNode(lpThisRoute->GetNodeCount() - 1);
        const Vector3 lLastPos(lpLast->GetX(), 0.0f, lpLast->GetY());
        const f32 lfDistance = vpu::Magnitude(lLastPos - GetPosition());
        // `fcmpu dist, flt_820C4160 (300.0) ; bge -> return false` 0x8276FD0C/0x8276FD10: a NaN
        // distance is "not old" and keeps its destination (FX-AINAN2; `>=` invalidated it).
        if (!(lfDistance < KF_FREE_ROAM_ROUTE_OLD_DISTANCE))
            return false;

        muDestinationSectionIndex = KI_INVALID_SECTION_INDEX;                               // sth 0x7FFF,0x1536
        return true;
    }

    // ==================================================================================
    // UpdateRouteFindingFreeRoam @0x8277C0C0 (no IDA export; image.bin disasm, 22 insns)
    //
    // The player's car uses the extrapolated-route test, every other car the free-roam one;
    // either "old" verdict raises mbRouteRequested. The DWARF's player-car argument is not
    // read (the body only reads `this`).
    // ==================================================================================
    void AICar::UpdateRouteFindingFreeRoam(const AICar* lpPlayerCar)
    {
        const bool lbRouteOld = mbIsPlayer ? IsExtrapolatedRouteGettingOld()               // 0x8277C0E0
                                           : IsFreeRoamingCarsRouteOld();                   // 0x8277C0E8
        if (lbRouteOld)
            mbRouteRequested = true;                                                        // stb 1,0x154D
        (void)lpPlayerCar;
    }

    // ==================================================================================
    // ComputeDistanceToCheckpoint @0x8277C118
    //
    // Distance from this car to the current checkpoint along a route. The route used is the
    // PLAYER's (when this is not the player, a player route exists and has > 1 node, and one
    // of its nodes is this car's best section -- that node), else this car's own route at its
    // next node (when it has > 1 node). Distance = node.distanceToCheckpoint + the car's
    // projection back along the segment behind the node (clamped at 0; segment = previous
    // node - node, or node0 - node1 for the first node; all in the (x, z) ground plane -- the
    // vperm mask @0x82CDA450 = (pos.x, pos.z, .., ..)). For a PARTIAL route with a valid
    // destination section the flat distance from the route's last node to that section's
    // middle is added. Asserts the result is non-negative and writes it; returns "found".
    // ==================================================================================
    bool AICar::ComputeDistanceToCheckpoint(const AISectionsData* lpAISectionsData,
                                            const AICar* lpPlayerCar, f32* lpfOutDistance) const
    {
        bool         lbFound = false;
        const Route* lpRoute = 0;
        s32          liNode  = 0;

        // 0x8277C158..0x8277C1D0  search the player's route for this car's best section
        if (!mbIsPlayer && lpPlayerCar != 0 && lpPlayerCar->GetRoute()->GetNodeCount() > 1)
        {
            const u16    luSection     = GetBestSectionIndex();
            const Route* lpPlayerRoute = lpPlayerCar->GetRoute();
            for (liNode = 0; liNode < lpPlayerRoute->GetNodeCount(); ++liNode)
            {
                if (lpPlayerRoute->GetNode(liNode)->GetSectionIndex() == luSection)
                {
                    lbFound = true;
                    lpRoute = lpPlayerRoute;
                    break;
                }
            }
        }

        // 0x8277C1EC  fall back to this car's own route at its next node
        if (!lbFound && GetRoute()->GetNodeCount() > 1)
        {
            liNode  = miNextRouteNodeIndex;
            lpRoute = GetRoute();
            lbFound = true;
        }

        if (!lbFound)                                                                       // 0x8277C214
            return false;

        CGS_ASSERT(lpRoute != 0, CGS_ASSERT_MESSAGE_AT("lpRoute != NULL", __FILE__, 1927));                                        // :1927

        const RouteNode* lpNode = lpRoute->GetNode(liNode);                                 // 0x8277C24C
        f32 lfSegX;
        f32 lfSegZ;
        if (liNode <= 0)
        {
            const RouteNode* lpNext = lpRoute->GetNode(1);                                  // 0x8277C27C
            lfSegX = lpNode->GetX() - lpNext->GetX();                                       // node0 - node1
            lfSegZ = lpNode->GetY() - lpNext->GetY();
        }
        else
        {
            const RouteNode* lpPrev = lpRoute->GetNode(liNode - 1);                         // 0x8277C2AC
            lfSegX = lpPrev->GetX() - lpNode->GetX();                                       // prev - node
            lfSegZ = lpPrev->GetY() - lpNode->GetY();
        }

        // 0x8277C2F0..0x8277C3B8  along = max(0, dot2D(pos.xz - node, seg / |seg|))
        const f32 lfToNodeX = mPosition.X().GetFloat() - lpNode->GetX();
        const f32 lfToNodeZ = mPosition.Z().GetFloat() - lpNode->GetY();
        const f32 lfInvSegLength = 1.0f / std::sqrt(lfSegX * lfSegX + lfSegZ * lfSegZ);      // vrsqrtefp + 2 refinements, no zero guard
        f32 lfAlong = (lfToNodeX * lfSegX + lfToNodeZ * lfSegZ) * lfInvSegLength;
        if (-lfAlong >= 0.0f)                                                               // fneg / fsel -> 0
            lfAlong = 0.0f;
        f32 lfDistance = lfAlong + lpNode->GetDistanceToCheckpoint();                       // fadds f31,f0,f13(node+8)

        // 0x8277C3BC  partial route: add the flat gap from the last node to the destination
        if (lpRoute->GetStatus() == Route::E_STATUS_PARTIAL &&
            muDestinationSectionIndex != KI_INVALID_SECTION_INDEX)
        {
            // AISectionsData::GetMiddle(dest) is inlined on the console as
            // GetAISection(dest)->GetMiddle() (0x8277C3D0 / 0x8277C3DC), then BrnMath::Flatten.
            const AISection* lpDestination = lpAISectionsData->GetAISection(muDestinationSectionIndex);
            const Vector2 lMiddle = BrnMath::Flatten(lpDestination->GetMiddle());           // 0x8277C3E4
            const RouteNode* lpLast = lpRoute->GetNode(lpRoute->GetNodeCount() - 1);        // 0x8277C3F8
            const f32 lfGapX = lMiddle.mV.x - lpLast->GetX();
            const f32 lfGapZ = lMiddle.mV.y - lpLast->GetY();
            const f32 lfGapSq = lfGapX * lfGapX + lfGapZ * lfGapZ;
            // vcmpeqfp v12, 0, lenSq @0x8277C44C ; vsel v0, lenSq*rsqrt, 0, mask @0x8277C478: only an
            // EXACT zero selects 0, a NaN gap stays NaN (FX-AINAN2; `lenSq > 0 ? .. : 0` gave 0).
            lfDistance = lfDistance + ((lfGapSq == 0.0f) ? 0.0f : std::sqrt(lfGapSq));
        }

        // `fcmpu f31, 0.0 ; bge` 0x8277C490/0x8277C494 skips the assert on an unordered compare.
        CGS_ASSERT(!(lfDistance < 0.0f), CGS_ASSERT_MESSAGE_AT("lfDistance >= 0.0f", __FILE__, 1966));                             // :1966
        *lpfOutDistance = lfDistance;
        return true;
    }

    // ==================================================================================
    // UpdateInRangeData @0x82792A18 -- the physics-driven car feeds its frame data in.
    //
    // Register map: r31=this, r4=lpAISectionsData (unused by the body), r30=r5 &transform,
    // v127=v1 velocity, f31=f1 speed, r28=r7 under-car section (r6 = float skip slot),
    // r26/r25/r27 = r8/r9/r10 = in-air / crashing / in-showtime, then the six stack bools
    // arg_6F on-start-line, arg_77 is-player, arg_7F driven-by-player, arg_87 drifting,
    // arg_8F touching-race-car, arg_97 touching-player.
    // ==================================================================================
    void AICar::UpdateInRangeData(AISectionsData* lpAISectionsData, const Matrix44Affine& lrTransform,
                                  Vector3 lVelocity, f32 lfSpeed, u16 luUnderCarSectionIndex,
                                  bool lbIsInAir, bool lbIsCrashing, bool lbIsInShowtime,
                                  bool lbIsOnStartLine, bool lbIsPlayer, bool lbIsDrivenByPlayer,
                                  bool lbIsDrifting, bool lbIsTouchingRaceCar, bool lbIsTouchingPlayer)
    {
        CGS_ASSERT(IsActive(), CGS_ASSERT_MESSAGE_AT("IsActive()", __FILE__, 1372));                                              // :1372

        const Vector3& lrPosition = lrTransform.wAxis;                                      // lvx128 v0,r30,0x30 -> stvx128 0x1430
        const Vector3& lrAt       = lrTransform.zAxis;                                      // lvx128 v1,r30,0x20
        const Vector3& lrRight    = lrTransform.xAxis;                                      // lvx128 v1,r0,r30
        mPosition = lrPosition;
        SetDirection(lrAt);                                                                 // 0x82792AB8
        SetRight(lrRight);                                                                  // 0x82792AC4

        muBestSectionIndex = luUnderCarSectionIndex;                                        // sth r28,0x1534 (== muUnderCarSectionIndex)
        mVelocity = lVelocity;                                                              // stvx128 v127,0x1470

        CGS_ASSERT(lbIsPlayer || !lbIsInShowtime, CGS_ASSERT_MESSAGE_AT("lbIsPlayer || !lbIsInShowtime", __FILE__, 1382));        // :1382

        mfSpeedInRange      = lfSpeed;                                                      // stfs f31,0x14E4 (== mfActualInRangeSpeed)
        mbIsInAir           = lbIsInAir;                                                    // stb 0x1541
        mbIsCrashing        = lbIsCrashing;                                                 // stb 0x1542
        mbIsInShowtime      = lbIsInShowtime;                                               // stb 0x1543
        mbIsDrifting        = lbIsDrifting;                                                 // stb 0x1544
        mbIsTouchingRaceCar = lbIsTouchingRaceCar;                                          // stb 0x1545
        mbIsTouchingPlayer  = lbIsTouchingPlayer;                                           // stb 0x1546
        mbIsOnStartLine     = lbIsOnStartLine;                                              // stb 0x1547
        mbIsDrivenByPlayer  = lbIsDrivenByPlayer;                                           // stb 0x154A

        if (meCarState == E_AI_CAR_STATE_IN_RANGE)                                          // 0x82792B4C
        {
            // 0x82792B50  lwz r3,0x14B0(this) ; bl BrnAI::AIDriver::ResetPIDTuningState @0x8277DA48
            mpDriver->ResetPIDTuningState();
        }

        mbIsPlayer = lbIsPlayer;                                                            // stb r30,0x1549
        (void)lpAISectionsData;
    }

    // ==================================================================================
    // UpdateResetOnTrackSection @0x82765EA0 -- the reset-on-track manager tells the car where
    // it will be put back (section + the two portals bounding the reset spot) and how fast.
    // ==================================================================================
    void AICar::UpdateResetOnTrackSection(EResetSpeedType leResetSpeedType, u16 luResetOnTrackSectionIndex,
                                          u8 luStartPortal, u8 luEndPortal)
    {
        CGS_ASSERT(IsActive(), CGS_ASSERT_MESSAGE_AT("IsActive()", __FILE__, 1415));                                              // :1415
        CGS_ASSERT(!IsInAir() || mbIsInShowtime, CGS_ASSERT_MESSAGE_AT("!IsInAir() || mbIsInShowtime", __FILE__, 1416));            // :1416
        CGS_ASSERT(luResetOnTrackSectionIndex != KI_INVALID_SECTION_INDEX,
                   CGS_ASSERT_MESSAGE_AT("luResetOnTrackSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX", __FILE__, 1417));     // :1417

        meResetSpeedType           = leResetSpeedType;                                      // stw 0x14D0
        muResetOnTrackSectionIndex = luResetOnTrackSectionIndex;                            // sth 0x1530
        muResetOnTrackStartPortal  = luStartPortal;                                         // stb 0x1538
        muResetOnTrackEndPortal    = luEndPortal;                                           // stb 0x1539
    }

    // ==================================================================================
    // UpdateOutOfRangeData @0x8276F398 -- the module places an out-of-range car.
    //
    // v126=v1 position, v127=v2 at vector, r25=r4 section index, r24=r5 section speed byte.
    // Stores the position, asserts the at vector is valid (streamed "Bad At vector"), sets
    // the facing, derives the right vector as Normalize(Cross(Y axis, at)) -- falling back to
    // the Z axis when that cross is degenerate (every |lane| <= FLT_EPSILON) -- and seeds the
    // default/best section and the buzz-frequency ratio min(1, sectionSpeed / 130).
    // ==================================================================================
    void AICar::UpdateOutOfRangeData(Vector3 lPosition, Vector3 lAtVector, u16 luSectionIndex,
                                     u8 luSectionSpeed)
    {
        CGS_ASSERT(IsActive(), CGS_ASSERT_MESSAGE_AT("IsActive()", __FILE__, 1440));                                              // :1440

        mPosition = lPosition;                                                              // stvx128 v126,0x1430

        if (!vpu::IsValid(lAtVector))                                                       // 0x8276F41C..0x8276F484 (per-lane vcmpeqfp.)
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStream << "Bad At vector";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessageBuffer, CGS_ASSERT_LOCATION(KPC_AICAR_CPP, 1444));
            CgsDev::Assert::EndAssert();
        }

        SetDirection(lAtVector);                                                            // 0x8276F500

        // 0x8276F504..0x8276F5B0  right = Cross(gJVector, at) (vpermwi 0x63 = yzx on both
        // operands around vmulfp/vnmsubfp), degenerate -> gKVector, else Normalize.
        const Vector3 lCross = vpu::Cross(vpu::GetVector3_YAxis(), lAtVector);
        Vector3 lRight;
        if (IsZeroVmx(lCross, KF_AICAR_FLOAT_EPSILON))                                      // 0x8276F550 vcmpgtfp., all false
            lRight = vpu::GetVector3_ZAxis();                                               // lvx128 unk_82181520
        else
            lRight = vpu::Normalize(lCross);
        SetRight(lRight);                                                                   // 0x8276F5B8

        muDefaultSectionIndex = luSectionIndex;                                             // sth r25,0x1532
        muBestSectionIndex    = luSectionIndex;                                             // sth r25,0x1534

        // 0x8276F5BC..0x8276F5F4  ratio = min(1.0, float(speed) * (1/130))  (fsel on ratio-1)
        const f32 lfRatio = static_cast<f32>(luSectionSpeed) * KF_SECTION_SPEED_TO_BUZZ_RATIO;
        mfBuzzFrequencyRatio = (lfRatio - 1.0f >= 0.0f) ? 1.0f : lfRatio;                   // stfs 0x1510
    }

    // ==================================================================================
    // SetRoadRageMadness @0x8276ECB0 -- road-rage aggression tuning (madness in [0,1]).
    //   aggression = madness * 0.6 (flag set), proximity 0.5, time 1.0, relative speed 0.666,
    //   acceleration 1.0. Called by OnModeStart (ROAD_RAGE) and AIModule::HandleGameActions.
    // ==================================================================================
    void AICar::SetRoadRageMadness(f32 lfMadness)
    {
        // [PC HARNESS, NOT X360] BRN_AI_MADNESS=<0..1>: override the madness every rival receives, so
        // the slam chain (DecideToAttack roll -> OVERTAKE/DROP_BACK_TO_SLAM -> ATTACK_SLAM ->
        // eBiasMode_Slam -> IncludeSmashIntoPlayer -> race-car contact -> TakedownManager) can be
        // exercised on demand. A fresh profile's madness ceiling is 0.1 (KF_EARLY_RANK_MAX_MADNESS),
        // i.e. a 6% attack roll -- faithful, but useless as a test. Same precedent as
        // BRN_FORCE_TAKEDOWN (GameStateModule_gTD_00.cpp). DELETE-WHEN the harness grows a proper
        // debug-menu equivalent of the console's aggression sliders.
        {
        }
        mAggressiveness.SetAggression(lfMadness * 0.6f);                                    // stfs 0x140C ; stb 1,0x1410
        mAggressiveness.SetProximityToSpeedMatch(0.5f);
        mAggressiveness.SetTimeForSpeedMatch(1.0f);
        mAggressiveness.SetRelativeSpeedForMatch(0.666f);                                   // stfs 0x141C (0.66600001)
        mAggressiveness.SetAcclerationRateForSpeedMatch(1.0f);
    }

    // ==================================================================================
    // OnModeStartRacing @0x82765C50 -- the race-start gun: behaviour -> CRUISING (the inlined
    // SetBehaviour: previous <- current, timer <- 0) and the starting-race flag drops.
    // ==================================================================================
    void AICar::OnModeStartRacing()
    {
        CGS_ASSERT(IsActive(), CGS_ASSERT_MESSAGE_AT("IsActive()", __FILE__, 491));                                              // :491
        CGS_ASSERT(mbIsInGameMode, CGS_ASSERT_MESSAGE_AT("mbIsInGameMode", __FILE__, 492));                                       // :492

        SetBehaviour(E_AI_BEHAVIOUR_CRUISING);                                              // 0x14E0=0 ; 0x14B8<-0x14B4 ; 0x14B4=3
        mbForceStandardRoute = 0;                                                           // stb 0,0x1548 (== mbIsStartingRace)
    }

    // ==================================================================================
    // OnModeStart @0x8277BD20 -- a game mode starts for this car.
    //
    // r4 speed-selection method, r5 opponent index, r6 route-finding style, r7/r8 the two
    // route bools (-> +0x153F mbCanDeviateFromRoute / +0x153E mbCanUseAIShortcuts), r9 the
    // destination section, f1 the progression rank ratio. Resets the timers/route/checkpoint
    // state, enters game mode, marks the car as starting, sets STOP behaviour, then tunes the
    // aggression block by style: ROAD_RAGE -> SetRoadRageMadness(0); MARKED_MAN -> a 0.2..1.0
    // band by rank; everything else -> a per-personality 0.1..0.6 band by rank with 0.7s.
    // ==================================================================================
    void AICar::OnModeStart(EAISpeedSelectionMethod leSpeedSelectionMethod, s32 liOpponentIndex,
                            ERouteFindingStyle leRouteFindingStyle, bool lbCanDeviateFromRoute,
                            bool lbCanUseAIShortcuts, u16 luDestinationSectionIndex,
                            f32 lfProgressionRankAsRatio)
    {
        CGS_ASSERT(IsActive(), CGS_ASSERT_MESSAGE_AT("IsActive()", __FILE__, 396));                                              // :396
        CGS_ASSERT(static_cast<u32>(liOpponentIndex) < 8u,
                   CGS_ASSERT_MESSAGE_AT("liOpponentIndex >= 0 && liOpponentIndex < BrnWorld::KI_MAX_ACTIVE_RACE_CARS", __FILE__, 397)); // :397 (cmplwi ,8)
        // `fcmpu r,0.0 ; blt -> fire` 0x8277BDE8/0x8277BDF0 then `fcmpu r,1.0 ; ble -> skip`
        // 0x8277BDF4/0x8277BDF8: an unordered ratio skips the assert (FX-AINAN2).
        CGS_ASSERT(!(lfProgressionRankAsRatio < 0.0f) && !(lfProgressionRankAsRatio > 1.0f),
                   CGS_ASSERT_MESSAGE_AT("lfProgressionRankAsRatio >= 0.0f && lfProgressionRankAsRatio <= 1.0f", __FILE__, 398));  // :398

        Route* lpThisRoute = GetRoute();

        mfRaceTimer             = 0.0f;                                                     // 0x14FC
        mfAlternativeRouteTimer = 0.0f;                                                     // 0x1500
        mfWrongWayTime          = 0.0f;                                                     // 0x14F4
        mfBehaviourTimer        = 0.0f;                                                     // 0x14E0
        meRouteFindingStyle     = leRouteFindingStyle;                                      // 0x14C0
        mbWantsAlternativeRoute = lbCanDeviateFromRoute ? 1 : 0;                            // 0x153F (== mbCanDeviateFromRoute)
        ++miRouteTimeStamp;                                                                 // 0x1528
        mfDistanceToCheckpoint  = KF_AICAR_FLT_MAX;                                         // 0x14F0 <- flt_82F302F4
        mbUseAIShortcuts        = lbCanUseAIShortcuts ? 1 : 0;                              // 0x153E (== mbCanUseAIShortcuts)
        lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;                           // 0x1408
        lpThisRoute->miNodeCount = 0;                                                       // 0x1400
        mbIsInGameMode          = true;                                                     // 0x154B
        muDefaultSectionIndex   = KI_INVALID_SECTION_INDEX;                                 // 0x1532
        mbHasBlockCheckpoints   = (leRouteFindingStyle == E_ROUTE_FINDING_RACE) ? 1 : 0;    // 0x153D (== mbUseBlockSections)
        miCurrentCheckpoint     = 0;                                                        // 0x1520
        mbForceStandardRoute    = 1;                                                        // 0x1548 (== mbIsStartingRace)
        mePreviousBehaviour     = meBehaviour;                                              // 0x14B8 <- 0x14B4
        meBehaviour             = E_AI_BEHAVIOUR_STOP;                                      // 0x14B4
        meSpeedSelectionMethod  = leSpeedSelectionMethod;                                   // 0x14BC
        muDestinationSectionIndex = luDestinationSectionIndex;                              // 0x1536
        miOpponentIndex         = static_cast<s8>(liOpponentIndex);                         // 0x153A
        // stb 1,0x1410 -- mAggressiveness.mbAggressionLevelSet; every arm below re-sets it
        // through SetAggression, so the console's early store is subsumed.

        if (mbIsPlayer)                                                                     // 0x8277BE28
        {
            // 0x8277BE30  if (mpDriver) { driver+0x1B24 = 0.9975f (0x3F7F5C29); driver+0x1B28 = 400.0004f }
            // driver+0x1B24 / +0x1B28 fall inside the driver's embedded RacingLine (@driver+0xF20),
            // at RacingLine +0xC04 / +0xC08 == mfCentreLineAhead / mfCentreLineAheadRecip
            // (BrnRacingLine.h :130 / :133); 400.0004 == 1 / (1 - 0.9975), which is what pins the
            // pairing. On the host the RacingLine object lives in AIDriver's trailing member, so
            // this is the de-inlined form of the two console stores.
            if (mpDriver != 0)
            {
                mpDriver->GetRacingLine().mfCentreLineAhead      = 0.9975f;
                mpDriver->GetRacingLine().mfCentreLineAheadRecip = 400.0004f;
            }
        }

        if (meRouteFindingStyle == E_ROUTE_FINDING_ROAD_RAGE)                               // 0x8277BE58
        {
            SetRoadRageMadness(0.0f);
            return;
        }

        f32 lfRelativeSpeed;
        f32 lfAcceleration;
        if (meRouteFindingStyle == E_ROUTE_FINDING_MARKED_MAN)                              // 0x8277BE84
        {
            const f32 lfLo = gfMarkedManMinAggression;
            const f32 lfHi = gfMarkedManMaxAggression;
            mAggressiveness.SetAggression((lfHi - lfLo) * lfProgressionRankAsRatio + lfLo);
            mAggressiveness.SetProximityToSpeedMatch(1.0f);
            mAggressiveness.SetTimeForSpeedMatch(1.0f);
            lfRelativeSpeed = 0.5f;
            lfAcceleration  = 1.0f;
        }
        else
        {
            // 0x8277BEE0  lvlx unk_820C4190[personality] / unk_820C4188[personality] ; lerp by rank
            const f32 lfLo = KAF_RANK_AGGRESSION_LO[mePersonalityType];
            const f32 lfHi = KAF_RANK_AGGRESSION_HI[mePersonalityType];
            mAggressiveness.SetAggression((lfHi - lfLo) * lfProgressionRankAsRatio + lfLo);  // vmaddfp (hi-lo)*ratio + lo
            mAggressiveness.SetProximityToSpeedMatch(0.7f);
            mAggressiveness.SetTimeForSpeedMatch(0.7f);
            lfRelativeSpeed = 0.7f;
            lfAcceleration  = 0.7f;
        }
        mAggressiveness.SetRelativeSpeedForMatch(lfRelativeSpeed);                          // stfs 0x141C
        mAggressiveness.SetAcclerationRateForSpeedMatch(lfAcceleration);
    }

    // ==================================================================================
    // Construct @0x82792620 (DWARF BrnAICar.h:86 `void Construct(EGlobalRaceCarIndex)`)
    //
    // The ONE-TIME per-car seed. AIModule::Construct @0x82794D08 runs it 35 times
    // (0x827951BC..0x827951F8: `mr r4,r29 ; mr r3,r31 ; bl GetAICar ; mr r4,r29 ;
    // bl AICar::Construct ; addi r29,r29,1 ; blt`), BEFORE any Reset and before the 8
    // AIDriver::Constructs. It has NO IDA export -- the address is known only through that
    // caller's xrefs_from -- so this body is recovered store for store from image.bin
    // (0x82792620..0x827927F8, big-endian; the four VMX128 words capstone cannot decode are
    // lvx128 / stvx128, extended opcode 0x0C3 / 0x1C3 in bits 21-31).
    //
    // THIS IS WHERE A CAR LEARNS WHICH GLOBAL SLOT IT IS: `stw r28, 0x14C4(r31)` at
    // 0x827926F4, r28 == r4 == the caller's loop index. miRaceCarIndex @+0x14C4 is DWARF
    // :676 meGlobalRaceCarIndex. While this function was parked, all 35 maAICars read index
    // 0, so (a) every route request was stamped owner 0 and every RouteResponse was applied
    // to car 0 (`GetAICar(lpRouteResponse->GetEventId())` in UpdateCarRoutes), (b) the
    // `[rival] slot S global 0` witness printed 0 for every slot, and (c) the
    // `lpDriver->GetGlobalRaceCarIndex() == leGlobalRaceCarIndex` assert in
    // HandleManagementEvents fired for every slot but 0.
    //
    // Register map @0x82792620: r31 = this, r28 = r4 (the index), r29 = this + 0x1430
    // (&mPosition), r30 = 0, r9 = -1, r10 = 0x7FFF, r7 = 2, r8 = 0x1460, v0 = vspltisw 0,
    // f31 = flt_82001CC0 = 0.0f. Calls, in order: SetDirection @0x8276B2C0 with
    // `lvx128 unk_82181520` (gKVector 0,0,1,0), SetRight @0x8276B7D0 with `lvx128 gIVector`
    // (0x82181500 = 1,0,0,0), SetIsInJunkyard @0x82764EC0 with r4 = 0.
    //
    // NOTE what Construct does NOT touch (the console leaves them at their .bss zero and lets
    // Reset settle them): mAggressiveness, mVelocity, mLastRoutePosition, mPlaceOnTrack*,
    // mePersonalityType, meResetSpeedType, meRelativeLocation, mCarAssetAttribKey,
    // mfDistanceAheadOfPlayer, mfBuzzDistanceToPlayer, mfPlaceOnTrackSpeed,
    // mfTimeInInvalidSection, miRouteTimeStamp, miProximityIndex, muResetOnTrackSectionIndex,
    // the two reset portals, mbSectionChangedFlag, mbIsTouchingPlayer, mbIsInShortcut,
    // mbIsInMasterRoute. Reproduced as-is -- this IS the console's steady state.
    // ==================================================================================
    void AICar::Construct(EGlobalRaceCarIndex leGlobalRaceCarIndex)
    {
        Route* lpThisRoute = GetRoute();

        mfBehaviourTimer                = 0.0f;                                  // 0x82792650 stfs f31,0x14E0
        lpThisRoute->meStatus           = Route::E_STATUS_UNINITIALISED;         // 0x82792654 stw 0,0x1408
        lpThisRoute->miNodeCount        = 0;                                     // 0x82792658 stw 0,0x1400
        {
            const Vector3 lAggregateValue(0.0f, 0.0f, 0.0f);
            mPosition = lAggregateValue;
        }     // 0x8279265C stvx128 v0,0(r29)
        lpThisRoute->miDefaultStartNode = 0;                                     // 0x82792660 stw 0,0x1404
        meBehaviour                     = E_AI_BEHAVIOUR_CRUISING;               // 0x82792664 stw 3,0x14B4
        mePreviousBehaviour             = E_AI_BEHAVIOUR_CRUISING;               // 0x82792668 stw 3,0x14B8
        meRouteFindingStyle             = E_ROUTE_FINDING_FREE_ROAM;             // 0x82792670 stw 0,0x14C0
        meSpeedSelectionMethod          = E_AI_SPEED_SELECTION_METHOD_FREE_ROAM; // 0x82792678 stw 0,0x14BC

        SetDirection(vpu::GetVector3_ZAxis());                                   // 0x82792680 bl 0x8276B2C0 (lvx128 unk_82181520)
        SetRight(vpu::GetVector3_XAxis());                                       // 0x82792694 bl 0x8276B7D0 (lvx128 gIVector)

        // 0x82792698 lvx128 v0,0(r29) reloads mPosition (the two calls clobbered v0) and
        // 0x827926D8 stvx128 v0,r31,r8(=0x1460) parks it in mLastGoodPosition.
        mLastGoodPosition               = mPosition;

        mfSpeedInRange                  = 0.0f;                                  // 0x827926A4 stfs f31,0x14E4
        mfWrongWayTime                  = 0.0f;                                  // 0x827926AC 0x14F4
        mfRaceTimer                     = 0.0f;                                  // 0x827926B4 0x14FC
        mfAlternativeRouteTimer         = 0.0f;                                  // 0x827926BC 0x1500
        miNextRouteNodeIndex            = -1;                                    // 0x827926C8 stw r9(-1),0x1524
        mfSpeedOutOfRange               = KF_DEFAULT_OUT_OF_RANGE_SPEED;         // 0x827926CC stfs flt_8300D7FC,0x14E8 (== mfDesiredSpeed)
        miOpponentIndex                 = -1;                                    // 0x827926D0 stb r9(-1),0x153A
        mpDriver                    = 0;                                     // 0x827926DC stw r30(0),0x14B0 (the guest mpDriver slot)
        meCarState                      = E_AI_CAR_STATE_INACTIVE;               // 0x827926E0 stw r7(2),0x14C8
        mfDistanceToCheckpoint          = KF_AICAR_FLT_MAX;                      // 0x827926F0 stfs flt_82F302F4,0x14F0
        miRaceCarIndex                  = static_cast<s32>(leGlobalRaceCarIndex);// 0x827926F4 stw r28,0x14C4  <-- the car's own global slot
        muBestSectionIndex              = KI_INVALID_SECTION_INDEX;              // 0x827926FC sth r10(0x7FFF),0x1534
        muDefaultSectionIndex           = KI_INVALID_SECTION_INDEX;              // 0x82792700 sth r10(0x7FFF),0x1532

        mbIsInAir                       = false;                                 // 0x82792708 stb r30,0x1541
        mbIsCrashing                    = false;                                 // 0x8279270C 0x1542
        mbIsInShowtime                  = false;                                 // 0x82792710 0x1543
        mbIsDrifting                    = false;                                 // 0x82792714 0x1544
        mbIsTouchingRaceCar             = false;                                 // 0x82792718 0x1545
        mbHasBlockCheckpoints           = 0;                                     // 0x8279271C 0x153D
        mbUseAIShortcuts                = 0;                                     // 0x82792720 0x153E
        mbWantsAlternativeRoute         = 0;                                     // 0x82792724 0x153F

        // 0x82792728 / 0x8279272C store 0 to 0x1408 / 0x1400 a SECOND time -- the console's own
        // repeat (the InvalidateRoute the first pair already covered). Kept as-is.
        lpThisRoute->meStatus           = Route::E_STATUS_UNINITIALISED;         // 0x82792728
        lpThisRoute->miNodeCount        = 0;                                     // 0x8279272C

        mbIsAheadOfPlayer               = false;                                 // 0x82792730 0x153B
        mbIsPlayer                      = false;                                 // 0x82792734 0x1549
        mbIsDrivenByPlayer              = false;                                 // 0x82792738 0x154A
        mbIsOnStartLine                 = false;                                 // 0x8279273C 0x1547
        mbForceStandardRoute            = 0;                                     // 0x82792740 0x1548 (== mbIsStartingRace)
        mbPlaceOnTrackRequested         = false;                                 // 0x82792744 0x153C
        mbIsInGameMode                  = false;                                 // 0x82792748 0x154B
        mbUseChosenDistanceFunction     = 0;                                     // 0x8279274C 0x1550 (== mbIsOnline)
        mbIsInResetPairSection          = false;                                 // 0x82792750 0x1551

        // 0x82792754..0x827927C8  CgsNumeric::Random::Construct() inlined on the file-scope RNG
        // at .data 0x8300D5D0 (the same blob Reset re-emits) -- see gAICarRandom's banner.
        gAICarRandom.Construct();

        mfMaxPlayerSpeed                = 0.0f;                                  // 0x827927CC stfs f31,0x1504
        mfRouteTimer                    = 0.0f;                                  // 0x827927D0 0x14EC
        muDestinationSectionIndex       = KI_INVALID_SECTION_INDEX;              // 0x827927D4 sth r10(0x7FFF),0x1536
        mfScheduleOffset0               = 0.0f;                                  // 0x827927D8 0x1518
        miCurrentCheckpoint             = 0;                                     // 0x827927DC stw r30,0x1520
        mfScheduleOffset1               = 0.0f;                                  // 0x827927E0 0x151C
        mbRouteRequested                = false;                                 // 0x827927E4 0x154D
        mfBuzzFrequencyRatio            = 0.0f;                                  // 0x827927E8 0x1510

        SetIsInJunkyard(false);                                                  // 0x827927EC bl 0x82764EC0 (r4 = 0)
    }

    // ==================================================================================
    // Reset @0x82792800 -- the console's re-initialiser (AIModule::HandleManagementEvents).
    //
    // r4 personality, r5 lbKeepTransform (the reset-on-track/position block is skipped when
    // set). Facing -> gKVector (0,0,1), right -> gIVector (1,0,0), every timer/flag/section/
    // route field to its idle value, the driver unbound, INACTIVE, relative location UNKNOWN,
    // aggression = per-personality base with 1.0/1.0/0.5/1.0 speed-match knobs,
    // mfDistanceToCheckpoint = FLT_MAX, proximity index -100, mLastRoutePosition = 0.
    // ==================================================================================
    void AICar::Reset(EPersonalityType lePersonalityType, bool lbKeepTransform)
    {
        SetDirection(vpu::GetVector3_ZAxis());                                              // lvx128 unk_82181520
        SetRight(vpu::GetVector3_XAxis());                                                  // lvx128 gIVector

        Route* lpThisRoute = GetRoute();

        mfWrongWayTime           = 0.0f;                                                    // 0x14F4
        mbIsOnStartLine          = false;                                                   // 0x1547
        mfTimeInInvalidSection   = 0.0f;                                                    // 0x1514
        muBestSectionIndex       = KI_INVALID_SECTION_INDEX;                                // 0x1534
        muDefaultSectionIndex    = KI_INVALID_SECTION_INDEX;                                // 0x1532
        mbForceStandardRoute     = 0;                                                       // 0x1548 (== mbIsStartingRace)
        lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;                           // 0x1408
        lpThisRoute->miNodeCount = 0;                                                       // 0x1400
        lpThisRoute->miDefaultStartNode = 0;                                                // 0x1404
        muDestinationSectionIndex = KI_INVALID_SECTION_INDEX;                               // 0x1536
        meRouteFindingStyle      = E_ROUTE_FINDING_FREE_ROAM;                               // 0x14C0
        mbUseAIShortcuts         = 0;                                                       // 0x153E
        mbWantsAlternativeRoute  = 0;                                                       // 0x153F
        mpDriver             = 0;                                                       // stw 0,0x14B0 (guest mpDriver)

        if (!lbKeepTransform)                                                               // 0x82792898
        {
            muResetOnTrackSectionIndex = KI_INVALID_SECTION_INDEX;                          // 0x827928A8 sth r11(0x7FFF),0x1530
            muResetOnTrackStartPortal  = 0;                                                 // 0x827928AC stb r30(li 0 @0x82792848),0x1538
            muResetOnTrackEndPortal    = 1;                                                 // 0x827928B0 stb r9(li 1 @0x82792850),0x1539
            meResetSpeedType           = E_RESET_SPEED_TYPE_COUNT;                          // 0x827928B4 stw r8(0x14),0x14D0 <- 20
            {
                const Vector3 lAggregateValue(0.0f, 0.0f, 0.0f);
                mPosition = lAggregateValue;
            }                                          // vspltisw 0 -> stvx128 0x1430
        }

        mbPlaceOnTrackRequested = false;                                                    // 0x153C

        // 0x827928E8..0x8279296C  CgsNumeric::Random::Construct() inlined on the file-scope RNG
        // at .data 0x8300D5D0 -- see gAICarRandom's banner for the word-for-word proof (and for
        // the seed high-word this used to get wrong).
        gAICarRandom.Construct();

        mfBuzzDistanceToPlayer = 0.0f;                                                      // 0x1508 (== mfDistanceToPlayer)
        mePersonalityType      = lePersonalityType;                                         // 0x14CC
        meCarState             = E_AI_CAR_STATE_INACTIVE;                                   // 0x14C8 <- 2
        meRelativeLocation     = E_RELATIVE_UNKNOWN;                                        // 0x14D4 <- 4

        mAggressiveness.SetAggression(KAF_PERSONALITY_BASE_AGGRESSION[lePersonalityType]);  // flt_820C41A0[personality] ; stb 1,0x1410
        mAggressiveness.SetProximityToSpeedMatch(1.0f);
        mAggressiveness.SetTimeForSpeedMatch(1.0f);
        mAggressiveness.SetRelativeSpeedForMatch(0.5f);                                     // 0x141C <- 0x3F000000
        mAggressiveness.SetAcclerationRateForSpeedMatch(1.0f);

        mfDistanceAheadOfPlayer = 0.0f;                                                     // 0x14F8
        mbIsInShortcut          = false;                                                    // 0x154C
        mfWrongWayTime          = 0.0f;                                                     // 0x14F4 (again)
        mbRouteRequested        = false;                                                    // 0x154D
        mfDistanceToCheckpoint  = KF_AICAR_FLT_MAX;                                         // 0x14F0
        mfRouteTimer            = 0.0f;                                                     // 0x14EC
        mbIsInMasterRoute       = false;                                                    // 0x154E
        mfScheduleOffset0       = 0.0f;                                                     // 0x1518
        mfScheduleOffset1       = 0.0f;                                                     // 0x151C
        miProximityIndex        = -100;                                                     // 0x152C
        {
            const Vector3 lAggregateValue(0.0f, 0.0f, 0.0f);
            mLastRoutePosition = lAggregateValue;
        }                                // stfs f31 x3 + stw 0 -> stvx128 0x1480
    }

    // ==================================================================================
    // SetDriver @0x8276EF20 -- bind the driver and derive the car's top speed from its attribs.
    //
    //   stw r4 -> +0x14B0 (mpDriver)
    //   burnoutcarasset(mCarAssetAttribKey @+0x14D8, NULL)                   (sub_82204998)
    //   handling = physicsvehiclehandling(RefSpec(carData+0x158).GetCollection(), NULL)
    //   boost    = physicsvehicleboostattribs(RefSpec(handlingData+0x78).GetCollection(), NULL)
    //   mfMaxPlayerSpeed = boostData[0] (MaxBoostSpeed, mph) * 0.44704 * 1.1
    // (boostData+0x00 == MaxBoostSpeed per VehicleAttribs::SetupAttribs' BP_VA_SRC_F(lpData, 0x00).)
    // ==================================================================================
    void AICar::SetDriver(AIDriver* lpDriver)
    {
        mpDriver = lpDriver;                                                            // stw r11,0x14B0

        Attrib::Gen::burnoutcarasset lCarAsset(mCarAssetAttribKey, 0);                      // ld r4,0x14D8 ; bl sub_82204998
        Attrib::Gen::physicsvehiclehandling lHandling(
            const_cast<Attrib::Collection*>(lCarAsset.GetPhysicsVehicleHandlingRefSpec()->GetCollection()), 0);
        const Attrib::Collection* const lpBoostCollection =
            const_cast<Attrib::RefSpec&>(lHandling.PhysicsVehicleBoostAttribs()).GetCollection();
        Attrib::Gen::physicsvehicleboostattribs lBoost(const_cast<Attrib::Collection*>(lpBoostCollection), 0);

        const f32* lpBoostData = static_cast<const f32*>(lBoost.GetLayoutPointer());
        mfMaxPlayerSpeed = lpBoostData[0] * KF_MPH_TO_MPS * KF_MAX_PLAYER_SPEED_SCALE;   // stfs 0x1504

        // (2026-09-05, aiwave2: a first-8 witness here proved every rival resolves its key to the
        // right car -- mfMaxPlayerSpeed 78..85 m/s. The "rival top speed ~10 m/s" symptom was
        // AIDriver::ProximitySpeed halving the desired speed because no fan contributor wrote
        // RacingLine::mfImmediateDistanceToTrafficImpact yet; closed by IncludeConstantBearing.)
    }

    // ==================================================================================
    // [FLAG relocate] Aggressiveness::SetAggression (DWARF BrnAIAggressiveness.h:39) and
    // SetRelativeSpeedForMatch (:68) are declared in BrnAIAggressiveness.h but have NO body in
    // the tree and NO X360 symbol -- the console inlines both as `stfs level,+0 ; stb 1,+4`
    // and `stfs +0x10` at every call site (Reset 0x8279297C, OnModeStart 0x8277BEC8/0x8277BF60,
    // SetRoadRageMadness 0x8276ECCC..0x8276ED14). Their canonical home is
    // BrnAIAggressiveness.cpp (no lane owns it this wave); bodied here so this unit links.
    // DELETE-WHEN moved to BrnAIAggressiveness.cpp.
    // ==================================================================================
    void Aggressiveness::SetAggression(f32 lfAggression)
    {
        mfAggressionLevel    = lfAggression;   // stfs +0x00
        mbAggressionLevelSet = true;           // stb 1,+0x04
    }

    void Aggressiveness::SetRelativeSpeedForMatch(f32 lfValue)
    {
        mfRelativeSpeedForSpeedMatch = lfValue;   // stfs +0x10

}

    // ==================================================================================
    // (AISectionsData::GetAISection @0x8230F6D0 -- the [FLAG relocate] copy that stood here was removed
    //  2026-09-03: the canonical body is SharedClasses/AI/AISectionsResourceType.cpp:74, mounted -- LNK2005.)
    // ---- the four declared-only accessors the route / drive lanes link against (conductor, 2026-09-03).
    // DWARF BrnAICar.h :185 / :371 / :275 / :326; each is the console's inlined member read (no export).
    s32         AICar::GetNextRouteNodeIndex() const { return miNextRouteNodeIndex; }
    s8          AICar::GetOpponentIndex() const      { return miOpponentIndex; }
    EAICarState AICar::GetState() const              { return meCarState; }
    bool        AICar::IsPlayerCar() const           { return mbIsPlayer; }

    // ---- three more declared-only accessors (DWARF :182 / :287 / :332), RaceBalancingManager links against them.
    // GetNextRouteNode: Route::GetNode(miNextRouteNodeIndex), null when out of range (the console's
    // `cmpw next, nodeCount ; bge -> li r3,0` at RaceBalancingManager::ComputeParSpeed @0x82789EC0).
    const RouteNode* AICar::GetNextRouteNode() const
    {
        if (miNextRouteNodeIndex < 0 || miNextRouteNodeIndex >= GetRoute()->GetNodeCount())
        {
            return 0;
        }
        return GetRoute()->GetNode(miNextRouteNodeIndex);
    }
    f32  AICar::GetMaxPlayerSpeed() const { return mfMaxPlayerSpeed; }
    bool AICar::IsAheadOfPlayer() const   { return mbIsAheadOfPlayer; }

    // ==================================================================================
    // The checkpoint hand-off (crash parity CC-10). No X360 symbol for any of the three:
    // AIModule::OnRaceCarReachedCheckpoint @0x8278A658 inlines them whole (decoded with
    // tools/re/ppcdis.py; register r4 = the AICar). The PS3 twin (DecFIGS @0x9CB6D4) is the same
    // shape, inlining OnReachedCheckpoint / IsOpponent and calling InvalidateRoute @0x9B4DA8.
    // ==================================================================================

    // DWARF BrnAICar.h:388. PS3 @0x9B4DA8 is exactly these three stores (+5364 = 0.0, +5120 = 0,
    // +5128 = 0); the console's inlined copy is 0x8278A698 `stfs f0(flt_82001CC0 = 0.0), 0x14F4`,
    // 0x8278A69C `stw 0, 0x1408` and 0x8278A6A0 `stw 0, 0x1400`.
    void AICar::InvalidateRoute()
    {
        Route* const lpThisRoute = GetRoute();
        mfWrongWayTime           = 0.0f;                                     // 0x14F4 <- flt_82001CC0
        lpThisRoute->meStatus    = Route::E_STATUS_UNINITIALISED;            // 0x1408
        lpThisRoute->miNodeCount = 0;                                        // 0x1400
    }

    // DWARF BrnAICar.h:507. The car passed checkpoint liCheckpointIndex: drop the route so the
    // route-request brain plans the next leg, aim at the next checkpoint's AI section, advance the
    // checkpoint cursor, forget the old distance, and block checkpoint sections only in a race.
    void AICar::OnReachedCheckpoint(s32 liCheckpointIndex, u16 luDestinationAISectionIndex)
    {
        InvalidateRoute();                                                   // 0x8278A698..0x8278A6A0
        muDestinationSectionIndex = luDestinationAISectionIndex;             // 0x8278A6C8 sth 0x1536
        miCurrentCheckpoint       = liCheckpointIndex + 1;                   // 0x8278A6B4 addi 1 / 0x8278A6CC stw 0x1520
        mfDistanceToCheckpoint    = KF_AICAR_FLT_MAX;                        // 0x8278A6C0 stfs flt_82F302F4, 0x14F0
        // 0x8278A694 `addi style,-1` / 0x8278A6A4 cntlzw / 0x8278A6BC extrwi 1,27 == (style == 1),
        // stored as a byte at 0x8278A6D0 (stb 0x153D).
        mbHasBlockCheckpoints     = (meRouteFindingStyle == E_ROUTE_FINDING_RACE) ? 1 : 0;
    }

    // DWARF BrnAICar.h:367. 0x8278A6B8 `lbz 0x153A` / 0x8278A6C4 `cmplwi 0xFF` / 0x8278A6D4 `beq ->
    // no`, then 0x8278A6D8 `lbz 0x1549` / 0x8278A6DC `cmplwi 0` / 0x8278A6E0 `bne -> no`: a car
    // holding an opponent slot that is not the player's.
    bool AICar::IsOpponent() const
    {
        return miOpponentIndex != -1 && !mbIsPlayer;
    }

}

#undef CGS_ARTIST_FILE
