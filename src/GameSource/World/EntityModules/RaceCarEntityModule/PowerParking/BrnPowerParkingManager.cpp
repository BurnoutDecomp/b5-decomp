#include "GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Math/BrnMathUtils.h"            // BrnMath::GetPointToInfiniteLineDistance (declared there)
#include "rw/math/vpu/vector3_operation.h"           // rw::math::vpu::Dot / Magnitude / MagnitudeSquared
#include "rw/math/fpu/scalar_operation.h"            // rw::math::fpu::Min / Max / Clamp (the console's fsel forms)
#include "GameSource/GameState/BrnGameEvents.h"      // PowerParkResultEvent / E_EVENT_POWER_PARK_RESULT (54)
#include "GameSource/World/EntityModules/RaceCarEntityModule/BrnActiveRaceCar.h"                  // ActiveRaceCar
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnPlayerVehicleControls.h" // PlayerVehicleControls
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleIOQueues.h" // RaceCarEntityModuleIO::GameEventQueue

#include <cmath>                                     // std::sqrt / std::fmaf

// ============================================================================
// GameSource/World/EntityModules/RaceCarEntityModule/PowerParking/BrnPowerParkingManager.cpp
//
// PowerParkingManager members (reconstructed from BURNOUT_X360_ARTIST.XEX).
//
// The free candidacy test BrnWorld::CheckVehicleForPowerPark (X360 @0x822B1FA0) is a header
// inline (DWARF BrnPowerParkingManager.h:182) and lives there, transcribed from the ARTIST dump
// (crash parity FX-RCEM4 2026-09-24). Its one callee without a body, BrnMath::
// GetPointToInfiniteLineDistance (@0x82540448), is bodied at the foot of this file -- see the
// MOVE-WHEN note there.
// ============================================================================
namespace BrnWorld
{

    // ---- the tuning globals (DWARF BrnPowerParkingManager.cpp:25..:53) -----------------------------
    // Plain (tweakable, non-const) float32_t file-scope globals. The image keeps the non-zero ones in
    // .data at 0x82CDB4C4..0x82CDB504 in DWARF line order (x360rd reads each below). The four zero
    // weights are .bss (0x82FAD2E4..0x82FAD2F0): nothing in the image writes them -- their only
    // readers are UpdateScoring and KF_TOTAL_SCORE_WEIGHTS' initializer -- so they are 0.0f and those
    // four scores never count. KF_TOTAL_SCORE_WEIGHTS is itself dynamically initialised
    // (0x82C4C128..0x82C4C174: the six weights summed in the order below, times 0.98f,
    // flt_82020A90 == 0x3F7AE148, stored to flt_82FAD3FC).
    f32 KF_MIN_LINEAR_VELOCITY_TO_START_POWER_PARK  = 15.0f;    // :25 flt_82CDB4C4 (Update 0x822F8728)
    f32 KF_MIN_HANDBRAKE_TO_START_POWER_PARK        = 0.2f;     // :26 flt_82CDB4C8 (0x822F860C)
    f32 KF_MAX_LINEAR_VELOCITY_TO_END_POWER_PARK    = 1.0f;     // :28 flt_82CDB4CC (0x822F8808)
    f32 KF_MAX_ANGULAR_VELOCITY_TO_END_POWER_PARK   = 0.2f;     // :29 flt_82CDB4D0 (0x822F8814)
    // Max perpendicular distance (from the player to the target vehicle's heading line) that still
    // counts as "aligned". DetermineOutcome loads it at 0x822A74D0, UpdateScoring at 0x822A72A0.
    f32 KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT = 3.0f;     // :30 flt_82CDB4D4
    f32 KF_MAX_PERPENDICULAR_DISTANCE_FOR_PERFECT   = 0.4f;     // :31 flt_82CDB4D8 (UpdateScoring 0x822A72F4)
    f32 KF_PROMIXITY_IDEAL_1ST_CAR_DISTANCE         = 6.0f;     // :32 flt_82CDB4DC (0x822A7194) [sic, DWARF spelling]
    f32 KF_PROMIXITY_IDEAL_2ND_CAR_DISTANCE         = 11.0f;    // :33 flt_82CDB4E0 (0x822A720C)
    f32 KF_DISTANCE_SCORE_SCALE                     = 30.0f;    // :35 flt_82CDB4E4
    f32 KF_PROXIMITY_SCORE_SCALE                    = 100.0f;   // :36 flt_82CDB4E8
    f32 KF_SPEED_SCORE_SCALE                        = 5.0f;     // :37 flt_82CDB4EC
    f32 KF_ROTATION_SCORE_SCALE                     = 800.0f;   // :38 flt_82CDB4F0
    f32 KF_POSITION_ALIGNMENT_SCORE_SCALE           = 100.0f;   // :39 flt_82CDB4F4
    f32 KF_ANGLE_ALIGNMENT_SCORE_SCALE              = 100.0f;   // :40 flt_82CDB4F8
    f32 KF_DISTANCE_SCORE_WEIGHT                    = 0.0f;     // :43 .bss flt_82FAD2E4
    f32 KF_PROXIMITY_SCORE_WEIGHT                   = 0.0f;     // :44 .bss flt_82FAD2E8
    f32 KF_SPEED_SCORE_WEIGHT                       = 0.0f;     // :45 .bss flt_82FAD2EC
    f32 KF_ROTATION_SCORE_WEIGHT                    = 0.0f;     // :46 .bss flt_82FAD2F0
    f32 KF_ANGLE_ALIGNMENT_SCORE_WEIGHT             = 0.5f;     // :47 flt_82CDB4FC
    f32 KF_POSITION_ALIGNMENT_SCORE_WEIGHT          = 0.5f;     // :48 flt_82CDB500
    f32 KF_TOTAL_SCORE_WEIGHTS = ( KF_ANGLE_ALIGNMENT_SCORE_WEIGHT + KF_POSITION_ALIGNMENT_SCORE_WEIGHT
                                 + KF_ROTATION_SCORE_WEIGHT + KF_SPEED_SCORE_WEIGHT
                                 + KF_PROXIMITY_SCORE_WEIGHT + KF_DISTANCE_SCORE_WEIGHT ) * 0.98f;   // :49 .bss flt_82FAD3FC
    f32 KF_WAIT_FOR_OUTCOME_TIME                    = 1.5f;     // :53 flt_82CDB504 (Update 0x822F888C)

    // X360 0x822C24B8 -- reset the per-attempt scoring state, then register the scorer's debug
    // component with the debug menu. Returns true (the RaceCarEntityModule::Prepare convention).
    // Grounded store-for-store on the asm: only the fields the asm writes are cleared here (it
    // leaves miOverallRating, the traffic/parked counts, last-frame pose and the closest-*
    // measurements untouched).
    bool PowerParkingManager::Prepare()
    {
        mfProximityScore                 = 0.0f;
        mfRotationScore                  = 0.0f;
        mfDistanceScore                  = 0.0f;
        mfSpeedScore                     = 0.0f;
        mfPositionAlignmentScore         = 0.0f;
        mfAngleAlignmentScore            = 0.0f;

        miWeightedDistanceScore          = 0;
        miWeightedProximityScore         = 0;
        miWeightedSpeedScore             = 0;
        miWeightedRotationScore          = 0;
        miWeightedPositionAlignmentScore = 0;
        miWeightedAngleAlignmentScore    = 0;

        mfLowestSpeedThisPark            = 3.4028235e38f;   // FLT_MAX (flt_8201442C)

        mePowerParkOutcome               = E_PPO_TO_BE_DETERMINED;
        mfTimeUntilDisplayOutcome        = 0.0f;
        mbPowerParkInProgress            = false;

        mPowerParkingDebugComponent.Register();
        return true;
    }

    // X360 0x822A74A0 -- resolve the final power-park outcome once the attempt has settled.
    void PowerParkingManager::DetermineOutcome()
    {
        // Need at least two nearby parked cars, and the player must be well aligned to the kerb
        // (perpendicular distance below the alignment threshold); otherwise the outcome is left
        // undetermined and nothing is scored.
        // 0x822A74B8 `cmplwi 2 ; bge` and 0x822A74D4 `fcmpu f13(+0x80), f0(3.0) ; bge` -> outcome 0.
        // That bge is bc 4,lt: TAKEN on NaN, so a NaN distance is "not aligned" -- the negated
        // compare keeps that (the old `>=` spelling went on to score a NaN park as a SUCCESS).
        if (muNearbyParkedCarCount < 2u ||
            !(mfClosestPerpendicularDist < KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT))
        {
            mePowerParkOutcome = E_PPO_TO_BE_DETERMINED;
            return;
        }

        miOverallRating = miWeightedDistanceScore
                        + miWeightedProximityScore
                        + miWeightedSpeedScore
                        + miWeightedRotationScore
                        + miWeightedPositionAlignmentScore
                        + miWeightedAngleAlignmentScore;

        CGS_ASSERT(miOverallRating >= 0, "miOverallRating >= 0");

        if (miOverallRating >= 100)
            miOverallRating = 100;

        // Only promote a still-undetermined attempt to a success (a FAILURE stays a FAILURE).
        if (mePowerParkOutcome == E_PPO_TO_BE_DETERMINED)
            mePowerParkOutcome = E_PPO_SUCCESS;
    }

    // DWARF :346. No out-of-line symbol: Update inlines it three times as the same run of stores
    // (0x822F8454..0x822F84AC, 0x822F84D8..0x822F852C, 0x822F8748..0x822F878C) -- the six scores 0.0
    // (flt_82001CC0), the six weighted scores 0, mfLowestSpeedThisPark FLT_MAX (flt_8201442C), the
    // outcome TO_BE_DETERMINED, the display countdown 0.0 and mbPowerParkInProgress false. The
    // traffic counts, the parked-car measurements and the last-frame pose are left alone.
    void PowerParkingManager::ClearData()
    {
        mfProximityScore                 = 0.0f;
        mfRotationScore                  = 0.0f;
        mfDistanceScore                  = 0.0f;
        mfSpeedScore                     = 0.0f;
        mfPositionAlignmentScore         = 0.0f;
        mfAngleAlignmentScore            = 0.0f;

        miWeightedDistanceScore          = 0;
        miWeightedProximityScore         = 0;
        miWeightedSpeedScore             = 0;
        miWeightedRotationScore          = 0;
        miWeightedPositionAlignmentScore = 0;
        miWeightedAngleAlignmentScore    = 0;

        mfLowestSpeedThisPark            = 3.4028235e38f;   // FLT_MAX (flt_8201442C)

        mePowerParkOutcome               = E_PPO_TO_BE_DETERMINED;
        mfTimeUntilDisplayOutcome        = 0.0f;
        mbPowerParkInProgress            = false;
    }

    // DWARF :420 / :433. No out-of-line symbol: RaceCarEntityModule inlines each as a bare
    // increment of the manager's tally (UpdateTrafficAndRaceCarNearMisses bumps +0x68 per drained
    // near-traffic record, UpdateRaceCarContacts +0x64 per player contact). The entity id is not
    // stored.
    void PowerParkingManager::AddNearTraffic(u32 luEntityId)
    {
        (void)luEntityId;
        ++miNearTrafficCount;
    }

    void PowerParkingManager::AddContactTraffic(u32 luEntityId)
    {
        (void)luEntityId;
        ++miContactTrafficCount;
    }

    // DWARF :451. No out-of-line symbol: ProcessPowerParking inlines it (0x822CE118..0x822CE13C,
    // six stores at +0x6C..+0x80) behind its IsPowerParking test.
    void PowerParkingManager::SetNearbyParkedTrafficData(u32 luNearbyParkedCarCount,
                                                         u32 luNearbyParkedPlayerCount,
                                                         f32 lfClosestDistanceSq,
                                                         f32 lfSecondClosestDistanceSq,
                                                         f32 lfClosestAngleDiff,
                                                         f32 lfClosestPerpendicularDist)
    {
        muNearbyParkedCarCount     = luNearbyParkedCarCount;
        muNearbyParkedPlayerCount  = luNearbyParkedPlayerCount;
        mfClosestDistanceSq        = lfClosestDistanceSq;
        mfSecondClosestDistanceSq  = lfSecondClosestDistanceSq;
        mfClosestAngleDiff         = lfClosestAngleDiff;
        mfClosestPerpendicularDist = lfClosestPerpendicularDist;
    }

    // X360 0x822F8400 (DWARF :122; locals lvPos :124, lvFacing :125, lfCurrentLinearVelocity :126,
    // lfCurrentAngularVelocity :127, lfPositionChange :128, lfAngleChange :129, lPowerParkResult
    // :167). Its one caller is RaceCarEntityModule::UpdatePowerParking (0x822FF610).
    void PowerParkingManager::Update(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType,
                                     f32 lfSimTimerStep,
                                     ActiveRaceCar* lpPlayerActiveRaceCar,
                                     PlayerVehicleControls* lpPlayerControls,
                                     RaceCarEntityModuleIO::GameEventQueue* lpEventQueue)
    {
        mPowerParkingDebugComponent.Update();                                   // 0x822F8434 vtable slot 0

        // 0x822F8448: the debug menu's "FORCE PARKING" toggle (+0x94) starts a park every update.
        if (mbDebugForcePowerPark)
        {
            ClearData();
            mbPowerParkInProgress = true;                                        // stb r10(=1), 0(r31)
            return;
        }

        // 0x822F84B4..0x822F84C8: a park only exists outside a game mode (-1), in Stunt Run (7) and in
        // the online free-burn lobby (15); anywhere else one in progress is dropped.
        if (leGameModeType != BrnGameState::GameStateModuleIO::E_MODE_NONE
            && leGameModeType != BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK
            && leGameModeType != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
        {
            if (mbPowerParkInProgress)
            {
                ClearData();
            }
            CGS_ASSERT(!mbPowerParkInProgress, "!mbPowerParkInProgress");         // :150 (li r5, 0x96)
            return;
        }

        // 0x822F8560..0x822F85C8: the result goes out once the countdown DetermineOutcome armed runs
        // out (ble: a NaN countdown is not running). Accelerating away first (+0x20 > 0.2,
        // flt_82014A98) drops it unsent.
        if (mfTimeUntilDisplayOutcome > 0.0f)
        {
            mfTimeUntilDisplayOutcome -= lfSimTimerStep;

            if (lpPlayerControls->mfAcceleration > 0.2f)
            {
                mfTimeUntilDisplayOutcome = 0.0f;
            }
            else if (!(mfTimeUntilDisplayOutcome > 0.0f))                         // bgt skips; NaN posts
            {
                BrnGameState::GameStateModuleIO::PowerParkResultEvent lPowerParkResult;
                lPowerParkResult.meOutcome              = static_cast<s32>(mePowerParkOutcome);  // lwz 4
                lPowerParkResult.miOverallRating        = miOverallRating;                       // lwz 0xC
                lPowerParkResult.miOtherPlayersInvolved = static_cast<s32>(muNearbyParkedPlayerCount); // lwz 0x70
                lpEventQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPowerParkResult),
                                       BrnGameState::GameStateModuleIO::E_EVENT_POWER_PARK_RESULT,
                                       static_cast<s32>(sizeof(lPowerParkResult)));             // li r5, 0x36 ; li r6, 0xC
                mfTimeUntilDisplayOutcome = 0.0f;
            }
        }

        // 0x822F85CC..0x822F85F0
        if (!lpPlayerActiveRaceCar->IsActive())
            return;
        if (lpPlayerActiveRaceCar->IsCrashing())
            return;

        // 0x822F85F4..0x822F8614: a park starts on the handbrake (+0x28 > 0.2, flt_82CDB4C8; ble returns,
        // so a NaN handbrake does not start one).
        if (!mbPowerParkInProgress
            && !(lpPlayerControls->mfHandBrake > KF_MIN_HANDBRAKE_TO_START_POWER_PARK))
        {
            return;
        }

        // 0x822F8618..0x822F8720. The facing is the transform's Z row (lvx128 v123, r11, 0x20); both
        // speeds are Magnitude (vmsum3fp + rsqrte + two Newton steps; std::sqrt here).
        const Vector3 lvPos    = lpPlayerActiveRaceCar->GetPosition();
        const Vector3 lvFacing = lpPlayerActiveRaceCar->GetTransform().zAxis;
        const f32 lfCurrentLinearVelocity  = rw::math::vpu::Magnitude(lpPlayerActiveRaceCar->GetVelocity());
        const f32 lfCurrentAngularVelocity =
            rw::math::vpu::Magnitude(lpPlayerActiveRaceCar->GetPhysicsState()->mAngularVelocity);   // +0x340

        // 0x822F8724..0x822F8730: ...and above 15 (flt_82CDB4C4).
        if (!mbPowerParkInProgress
            && !(lfCurrentLinearVelocity > KF_MIN_LINEAR_VELOCITY_TO_START_POWER_PARK))
        {
            return;
        }

        f32 lfPositionChange;
        f32 lfAngleChange;
        if (!mbPowerParkInProgress)
        {
            // 0x822F8748..0x822F8798: start the park; nothing has moved yet.
            ClearData();
            mbPowerParkInProgress = true;
            lfPositionChange = 0.0f;
            lfAngleChange    = 0.0f;
        }
        else
        {
            // 0x822F879C..0x822F87C8: squared (vmsum3fp128, no root) frame-to-frame deltas.
            lfPositionChange = rw::math::vpu::MagnitudeSquared(lvPos - mvPositionLastFrame);
            lfAngleChange    = rw::math::vpu::MagnitudeSquared(lvFacing - mvFacingLastFrame);
        }

        mvPositionLastFrame = lvPos;                                            // stvx128 +0x40
        mvFacingLastFrame   = lvFacing;                                         // stvx128 +0x50

        // 0x822F87DC..0x822F87E8: fsel(v - lowest, lowest, v).
        mfLowestSpeedThisPark = rw::math::fpu::Min(lfCurrentLinearVelocity, mfLowestSpeedThisPark);

        UpdateScoring(lfAngleChange, lfPositionChange, lfCurrentLinearVelocity);   // 0x822F87EC

        // 0x822F87F0..0x822F8868: how the park ends.
        if (miContactTrafficCount > 0)
        {
            // Touched a vehicle since the tally was last cleared: a failed park.
            mePowerParkOutcome    = E_PPO_FAILURE;
            mbPowerParkInProgress = false;
        }
        else if (!(lfCurrentLinearVelocity > KF_MAX_LINEAR_VELOCITY_TO_END_POWER_PARK)          // bgt
                 && !(lfCurrentAngularVelocity > KF_MAX_ANGULAR_VELOCITY_TO_END_POWER_PARK))    // ble ends (NaN too)
        {
            // Came to rest (1.0 flt_82CDB4CC, 0.2 flt_82CDB4D0).
            mbPowerParkInProgress = false;
        }
        else if (lpPlayerControls->mfAcceleration > 0.0f                          // ble skips
                 && !lpPlayerActiveRaceCar->IsDrifting()                          // lfs 0x4E0 > 0.0
                 && lfCurrentLinearVelocity > mfLowestSpeedThisPark * 1.3f)       // flt_8201ECC8 ; ble skips
        {
            // Drove off again.
            mbPowerParkInProgress = false;
        }

        // 0x822F886C..0x822F8890
        if (!mbPowerParkInProgress)
        {
            DetermineOutcome();
            if (mePowerParkOutcome != E_PPO_TO_BE_DETERMINED)
            {
                mfTimeUntilDisplayOutcome = KF_WAIT_FOR_OUTCOME_TIME;            // 1.5, flt_82CDB504
            }
        }

        // 0x822F8894 / 0x822F8898 -- only on this full pass; every early return above keeps the tallies.
        miContactTrafficCount = 0;
        miNearTrafficCount    = 0;
    }

    // X360 0x822A7140 (DWARF :266; locals lfScaledDistanceScore .. lfScaledPositionAlignmentScore
    // :268-:273, lfClosestDistance :279, lfClosestAngleDiff :302, lfPerpendicularDist :305). Its one
    // caller is Update (0x822F87EC).
    void PowerParkingManager::UpdateScoring(f32 lfAngleChange, f32 lfPositionChange, f32 lfCurrentLinearVelocity)
    {
        // 0x822A7154..0x822A7268: how snugly the car sits between the nearest two parked cars.
        // The first term SUBTRACTS the ideal distance (fsubs @0x822A71E8), the second DIVIDES by it
        // (fdivs @0x822A7254); each term is 0.5 (flt_820147FC) over Max(x, 1.0) (fsel @0x822A71F0 /
        // 0x822A725C). The roots are rsqrte + two Newton steps (0 at 0); std::sqrt here.
        mfProximityScore = 0.0f;
        if (muNearbyParkedCarCount != 0u)
        {
            f32 lfClosestDistance = std::sqrt(mfClosestDistanceSq);
            mfProximityScore = 0.5f / rw::math::fpu::Max(lfClosestDistance - KF_PROMIXITY_IDEAL_1ST_CAR_DISTANCE, 1.0f);

            if (muNearbyParkedCarCount > 1u)
            {
                lfClosestDistance = std::sqrt(mfSecondClosestDistanceSq);
                mfProximityScore += 0.5f / rw::math::fpu::Max(lfClosestDistance / KF_PROMIXITY_IDEAL_2ND_CAR_DISTANCE, 1.0f);
            }
        }

        // 0x822A726C..0x822A7294
        mfRotationScore += lfAngleChange;
        mfDistanceScore += lfPositionChange;
        mfSpeedScore     = rw::math::fpu::Max(lfCurrentLinearVelocity, mfSpeedScore);   // fsel(v - s, v, s)

        // 0x822A7298..0x822A733C: alignment with the closest parked car, only when there is one and the
        // car sits inside the 3.0 alignment band (bge: a NaN distance scores nothing).
        if (muNearbyParkedCarCount != 0u
            && mfClosestPerpendicularDist < KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT)
        {
            // :300 (li r5, 0x12C). blt 0.0 fires, ble HALF_PI skips: a NaN does not fire.
            CGS_ASSERT(!(mfClosestAngleDiff < 0.0f) && !(mfClosestAngleDiff > PowerParkingDetail::KF_HALF_PI),
                       "mfClosestAngleDiff >= 0 && mfClosestAngleDiff <= RwMathFPU::HALF_PI");

            const f32 lfClosestAngleDiff = mfClosestAngleDiff;

            // fmuls d*d ; fmsubs (d*d)*d - 0.4 (flt_82CDB4D8, one rounding) ; fsel against 0.0. The CUBE
            // of the distance, as the console computes it.
            const f32 lfPerpendicularDist = rw::math::fpu::Max(
                std::fmaf(mfClosestPerpendicularDist * mfClosestPerpendicularDist, mfClosestPerpendicularDist,
                          -KF_MAX_PERPENDICULAR_DISTANCE_FOR_PERFECT),
                0.0f);

            // fnmsubs a*a against 1.0 (one rounding) then the two-fsel Clamp to [0, 1].
            mfAngleAlignmentScore = rw::math::fpu::Clamp(std::fmaf(-lfClosestAngleDiff, lfClosestAngleDiff, 1.0f),
                                                         0.0f, 1.0f);
            mfPositionAlignmentScore = rw::math::fpu::Clamp(
                1.0f - lfPerpendicularDist / KF_MAX_PERPENDICULAR_DISTANCE_FOR_ALIGNMENT, 0.0f, 1.0f);
        }
        else
        {
            mfAngleAlignmentScore    = 0.0f;                                    // 0x822A7338
            mfPositionAlignmentScore = 0.0f;                                    // 0x822A733C
        }

        // 0x822A7340..0x822A748C: each score times its scale, clamped to [0, 100] (flt_82014808), times
        // the hoisted reciprocal of KF_TOTAL_SCORE_WEIGHTS (fdivs 1.0 / flt_82FAD3FC @0x822A7370),
        // times its weight, truncated (fctiwz).
        const f32 lfOneOverTotalWeights = 1.0f / KF_TOTAL_SCORE_WEIGHTS;

        const f32 lfScaledDistanceScore          = rw::math::fpu::Clamp(mfDistanceScore * KF_DISTANCE_SCORE_SCALE, 0.0f, 100.0f);
        const f32 lfScaledProximityScore         = rw::math::fpu::Clamp(KF_PROXIMITY_SCORE_SCALE * mfProximityScore, 0.0f, 100.0f);
        const f32 lfScaledSpeedScore             = rw::math::fpu::Clamp(mfSpeedScore * KF_SPEED_SCORE_SCALE, 0.0f, 100.0f);
        const f32 lfScaledRotationScore          = rw::math::fpu::Clamp(mfRotationScore * KF_ROTATION_SCORE_SCALE, 0.0f, 100.0f);
        const f32 lfScaledAngleAlignmentScore    = rw::math::fpu::Clamp(mfAngleAlignmentScore * KF_ANGLE_ALIGNMENT_SCORE_SCALE, 0.0f, 100.0f);
        const f32 lfScaledPositionAlignmentScore = rw::math::fpu::Clamp(mfPositionAlignmentScore * KF_POSITION_ALIGNMENT_SCORE_SCALE, 0.0f, 100.0f);

        miWeightedDistanceScore          = static_cast<s32>(lfScaledDistanceScore * lfOneOverTotalWeights * KF_DISTANCE_SCORE_WEIGHT);
        miWeightedProximityScore         = static_cast<s32>(lfScaledProximityScore * lfOneOverTotalWeights * KF_PROXIMITY_SCORE_WEIGHT);
        miWeightedSpeedScore             = static_cast<s32>(lfScaledSpeedScore * lfOneOverTotalWeights * KF_SPEED_SCORE_WEIGHT);
        miWeightedRotationScore          = static_cast<s32>(lfScaledRotationScore * lfOneOverTotalWeights * KF_ROTATION_SCORE_WEIGHT);
        miWeightedAngleAlignmentScore    = static_cast<s32>(lfScaledAngleAlignmentScore * lfOneOverTotalWeights * KF_ANGLE_ALIGNMENT_SCORE_WEIGHT);
        miWeightedPositionAlignmentScore = static_cast<s32>(lfScaledPositionAlignmentScore * lfOneOverTotalWeights * KF_POSITION_ALIGNMENT_SCORE_WEIGHT);
    }
}

// ============================================================================
// BrnMath::GetPointToInfiniteLineDistance -- X360 0x82540448 (DWARF home Math/BrnMathUtils.cpp,
// locals lLineDir :116, lLineToPoint :120, lfDelta :122, lProjectedPointOnLine :124,
// lPointToProjectedPoint :126, lfDistance :128; its assert names BrnMathUtils.cpp:118,
// li r5, 0x76). Its only caller is CheckVehicleForPowerPark (0x822B2294).
//
// MOVE-WHEN: BrnMathUtils.cpp (declared in BrnMathUtils.h:75) belongs to no lane of the crash
// parity campaign, so the body is homed beside its only caller; move it into BrnMathUtils.cpp
// verbatim when that file is next opened, and delete it here in the same commit.
//
// The projection is NOT divided by |lLineDir|^2: the console takes lfDelta = Dot(lLineDir,
// lLineToPoint) (vmsum3fp128 at 0x82540504) and multiplies straight back (vmaddfp128 at
// 0x82540530). That is the perpendicular distance for a unit direction -- the traffic transform's
// Z row and ActiveRaceCar::GetDirection -- and the console's answer, not a distance, otherwise.
// ============================================================================
namespace BrnMath
{
    f32 GetPointToInfiniteLineDistance(Vector3 lPoint, Vector3 lPointOnLine1, Vector3 lPointOnLine2)
    {
        const Vector3 lLineDir = lPointOnLine2 - lPointOnLine1;                     // 0x82540484 vsubfp128

        // 0x825404A8 vmsum3fp128 ; 0x825404B0 vcmpgtfp. against 0.0 (flt_82001CC0) ; bne all-true
        // skips. So a zero-length or NaN direction fires.
        CGS_ASSERT(rw::math::vpu::MagnitudeSquared(lLineDir) > 0.0f, "RwMath::MagnitudeSquared( lLineDir ) > 0.0f");

        const Vector3 lLineToPoint = lPoint - lPointOnLine1;                         // 0x825404E4 vsubfp128
        const f32 lfDelta = rw::math::vpu::Dot(lLineDir, lLineToPoint);              // 0x82540504 vmsum3fp128
        const Vector3 lProjectedPointOnLine = lLineDir * lfDelta + lPointOnLine1;    // 0x82540530 vmaddfp128
        const Vector3 lPointToProjectedPoint = lProjectedPointOnLine - lPoint;       // 0x82540534 vsubfp128

        // 0x82540538..0x82540568: vmsum3fp128, vrsqrtefp + two Newton steps, x * rsqrt(x), vsel 0 at 0.
        // FLAG (PC-platform, numeric): the rsqrt estimate is de-optimised to std::sqrt (Magnitude).
        const f32 lfDistance = rw::math::vpu::Magnitude(lPointToProjectedPoint);
        return lfDistance;
    }
}
