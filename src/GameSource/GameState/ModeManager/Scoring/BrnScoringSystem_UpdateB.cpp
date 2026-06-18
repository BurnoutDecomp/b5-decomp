// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnScoringSystem_UpdateB.cpp
// ============================================================================
// Out-of-line method BODIES for the ScoringSystem "UpdateB" group
// (BrnScoringSystem.h lines 420-434): the part-B per-frame update pass --
// network/cumulative results, the race-car distance comparator, the
// player-driving detectors and the per-mode score / general-stat updates.
//
// SHAPE = DecFIGS DWARF; BODY = X360 pseudocode (overrides DWARF on conflict).
// Members accessed BY NAME against the keystone layout -- no offset casts.
//
// ----------------------------------------------------------------------------
// LANDED (compiling bodies):
//   CompareRaceCarDistances    (0x823125B8)
//   CheckRoadRageMedalAwarded  (0x82312840)
//   UpdateCumulativeResults    (0x8231FCA0)  [FULLY LANDED -- CarScoreData/CarData part + the
//                                             AwardNetworkRatings vtable tail, now that slot 7 exists]
//   DetectPlayerDrivingWrongWay(0x8232B6B8)  [lpOutput resolves to the real active interface]
//   DetectPlayerStationary     (0x82320008)  [lpOutput resolves to the real active interface]
//   UpdateDistanceToPlayer     (0x8232B408)  [NEW -- CarScoreData grown with mfDistanceToPlayer/
//                                             SetDistanceToPlayer (+0x1C); Vector3 ops via ADL]
//
// BLOCKED (body omitted -- left declare-only; each needs a type/method that has
// no committed definition reachable from a body agent; the dependency RULE
// forbids ad-hoc-slicing a shared keystone). Ledger:
//
//   UpdateNetworkPlayerResults (0x8231FA90)
//       Walks the lpResults per-player record stream (X360: a2+24, stride 28) reading
//       PlayerResultsData fields (per record: finish Time @+0x00/+0x04, race-car index @+0x08,
//       eliminator index @+0x0C, distance-to-finish @+0x10, eliminations @+0x14, valid flag @+0x18,
//       timed-out @+0x19, eliminated @+0x1A) and writing them onto each car's CarScoreData
//       (mTotalTime / mfDistanceToFinish @+0x48 / meEliminatorRaceCarIndex @+0x60 / mbTimedOut @+0x68
//       / miNumEliminations @+0x64 / mbEliminated @+0xD9, plus the +0x6B flag), then dispatches
//       mpCurrentOnlineModeScoring->Update(this, muCarsInCurrentMode) (vtable+0x18, slot 5).
//       The slot-5 Update virtual is NOW DECLARED on the grown BaseOnlineModeScoring vtable, so that
//       half is resolved -- BUT the per-player record is still an unnamed u8[8*28] blob on the
//       committed PlayerResultsInterface (BrnNetworkModuleIO.h): PlayerResultsData has no typed
//       layout and no GetPlayerResultsData accessor. Reading those fields by name requires homing
//       PlayerResultsData in the NETWORK keystone (BrnNetworkModuleIO.h, "offsets/stride must not
//       move" -- reserved for that subsystem's own TU). Cross-subsystem keystone grow -> out of scope.
//       MISSING: typed BrnNetwork::...::PlayerResultsData (named fields) + GetPlayerResultsData
//                accessor on PlayerResultsInterface.   [vtable+0x18 dependency now RESOLVED]
//
//   UpdateGeneralStats         (0x8232B8C0)
//       Per-frame general per-car stat roll-up (lead/last time-in-place, per-car drift distance,
//       longest drift, boost time). lpOutput resolves; the lead/last/boost Time fields it touches DO
//       have committed CarScoreData accessors (Get/SetTimeInFirstPlace / Get/SetTimeInLastPlace /
//       Get/SetTimeBoosting), the per-frame distance accumulator @+0xDC now has Get/SetDistanceAccumulator,
//       and the current-drift slot is CarData::mfCurrentDriftDistance (Get/Set/IncrementDriftDistance).
//       HOWEVER the longest-drift commit reads+writes CarScoreData +0xF8 (the "if currentDrift >
//       longestDrift: longestDrift = currentDrift; currentDrift = 0" branch), and +0xF8 has NO
//       committed named member/accessor -- it is still inside maStorageF8[48]. Writing it by name
//       needs CarScoreData (BrnGameStateSharedIO.h keystone) grown with a longest-drift field.
//       MISSING: GameStateModuleIO::CarScoreData longest-drift field (+0xF8, named member/accessor).
//
// (UpdateCrashModeScore / UpdateStuntAttackModeScore / UpdateRoadRageModeScore /
//  SetRoadRageDetails at header lines 426-431 carry only a ':NNN' DWARF line and
//  NO X360 0x82 address comment, so they are not targets of this addressed-body
//  pass and have no dossier to reconstruct from.)
// ----------------------------------------------------------------------------

#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

// Completes BrnGameState::ActiveRaceCarOutputInterface (a typedef for
// BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface): the player-detector
// bodies below dereference it BY NAME (maCarsInTheRace / GetPlayerActiveRaceCarIndex /
// IsPlayerCarActive / IsRaceCarActive / GetRaceCarState). This pulls the complete RaceCarState
// (BrnVehicleEvents.h) too, so the per-car physics fields (mfMaxSpeedMPH/mfGas/...) are named.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Bad-case-of-Road-Rage assert)

// rw::math::vpu Vector3 vocabulary (operator- / Length) used by UpdateDistanceToPlayer to turn the
// player->car offset into a scalar distance. Found by ADL on Vector3 (== rw::math::vpu::Vector3).
#include "SharedClasses/Maths/BrnVectorMaths.h"

#include <cmath>   // std::fabs (DetectPlayerStationary's boost-time magnitude test)

namespace BrnGameState
{
    // ------------------------------------------------------------------------
    // CompareRaceCarDistances  --  X360 0x823125B8  (BrnScoringSystem.cpp:533)
    // ------------------------------------------------------------------------
    // qsort(3) comparator over the maRaceCarPositioningData[8] scratch (the X360
    // call site is `qsort(&maRaceCarPositioningData, 8, sizeof(RaceCarPositioningData),
    // &CompareRaceCarDistances)`). It never touches `this`, so it is usable as a
    // C comparator even though the DWARF declares it as an ordinary member.
    //
    // Ordering (return >0 => lpA sorts AFTER lpB, the qsort convention):
    //   1. an unset slot (meActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID,
    //      i.e. -1) sorts last;
    //   2. a disconnected car sorts last;
    //   3. greater finish position sorts last;
    //   4. fewer checkpoints reached sorts last;
    //   5. greater distance-to-next-checkpoint sorts last;
    //   6. otherwise equal.
    int ScoringSystem::CompareRaceCarDistances(const void* lpA, const void* lpB)
    {
        const RaceCarPositioningData* lpPosDataA = static_cast<const RaceCarPositioningData*>(lpA);
        const RaceCarPositioningData* lpPosDataB = static_cast<const RaceCarPositioningData*>(lpB);

        if (lpPosDataA->meActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            return 1;
        if (lpPosDataB->meActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            return -1;

        if (lpPosDataA->mbDisconnected)
            return 1;
        if (lpPosDataB->mbDisconnected)
            return -1;

        if (lpPosDataA->miFinishPosition > lpPosDataB->miFinishPosition)
            return 1;
        if (lpPosDataA->miFinishPosition < lpPosDataB->miFinishPosition)
            return -1;

        if (lpPosDataA->miCurrentCheckpoint < lpPosDataB->miCurrentCheckpoint)
            return 1;
        if (lpPosDataA->miCurrentCheckpoint > lpPosDataB->miCurrentCheckpoint)
            return -1;

        if (lpPosDataA->mfDistanceToNextCheckpoint > lpPosDataB->mfDistanceToNextCheckpoint)
            return 1;
        if (lpPosDataA->mfDistanceToNextCheckpoint < lpPosDataB->mfDistanceToNextCheckpoint)
            return -1;

        return 0;
    }

    // ------------------------------------------------------------------------
    // CheckRoadRageMedalAwarded  --  X360 0x82312840  (BrnScoringSystem.cpp:2283)
    // ------------------------------------------------------------------------
    // Road-Rage medal ratchet, called as the player's takedown count climbs.
    // While a medal is still outstanding (meCurrentMedalAchieved != GOLD), and
    // the running takedown count has reached the current target's threshold,
    // record the achievement, advance the current target one notch toward gold
    // (BRONZE->SILVER->GOLD) and re-point the Road-Rage scorer's takedown target
    // at the next threshold.
    void ScoringSystem::CheckRoadRageMedalAwarded(u32 luTakedowns)
    {
        if (meCurrentMedalAchieved)
        {
            const ECurrentMedalTargetTime leTarget = meCurrentMedalTarget;
            if (luTakedowns >= mauiMedalScores[leTarget])
            {
                meCurrentMedalAchieved = leTarget;
                if (leTarget)
                {
                    ECurrentMedalTargetTime leNextTarget;
                    if (leTarget == E_CURRENT_MEDAL_TARGET_TIME_SILVER)
                    {
                        leNextTarget = E_CURRENT_MEDAL_TARGET_TIME_GOLD;
                    }
                    else
                    {
                        CGS_ASSERT(leTarget < E_CURRENT_MEDAL_TARGET_TIME_NONE, "Bad case of Rage Rage\n");
                        leNextTarget = E_CURRENT_MEDAL_TARGET_TIME_SILVER;
                    }
                    meCurrentMedalTarget = leNextTarget;
                }
                mRoadRageModeScoring.SetTakeDownTarget(static_cast<s32>(mauiMedalScores[meCurrentMedalTarget]));
            }
        }
    }

    // ------------------------------------------------------------------------
    // UpdateDistanceToPlayer  --  X360 0x8232B408  (BrnScoringSystem.cpp:1903)
    // ------------------------------------------------------------------------
    // Per-frame "how far is each car from the player?" pass. Snapshots the player's world
    // position once, then for every car currently in the race computes the straight-line
    // distance from the player to that car's position and stores it on the car's score record.
    //
    // X360 walks lpOutput->maCarsInTheRace (a checked Array; the loop reads its GetLength()),
    // reads each entry's meActiveRaceCarIndex (asserting it is in [0,8)), maps it to a CarData via
    // GetCarData, and -- when a record exists -- stores Length(playerPos - car.mPosition) into the
    // CarScoreData distance-to-player slot (X360 stfs 0x1C(GetCarData)). The SIMD body is the inlined
    // rw::math::vpu vector subtract + reciprocal-sqrt magnitude; reconstructed as operator-/Length.
    // The X360 "result pointer" return is the void method's result-register artifact and is dropped.
    void ScoringSystem::UpdateDistanceToPlayer(const ActiveRaceCarOutputInterface* lpOutput)
    {
        const Vector3 lv3PlayerPosition = lpOutput->GetPlayerPosition();

        const u32 luNumCarsInRace = lpOutput->maCarsInTheRace.GetLength();
        for (u32 i = 0; i < luNumCarsInRace; ++i)
        {
            const BrnWorld::RaceCarEntityModuleIO::CarsInTheRaceData& lCarInRace =
                lpOutput->maCarsInTheRace[i];

            CarData* lpCar = GetCarData(lCarInRace.meActiveRaceCarIndex);
            if (lpCar)
            {
                const Vector3 lv3Delta = lv3PlayerPosition - lCarInRace.mPosition;
                const f32 lfDistanceToPlayer = Length(lv3Delta);
                lpCar->GetScoreData()->SetDistanceToPlayer(lfDistanceToPlayer);
            }
        }
    }

    // ------------------------------------------------------------------------
    // UpdateCumulativeResults  --  X360 0x8231FCA0  (BrnScoringSystem.cpp:~1271)
    // ------------------------------------------------------------------------
    // End-of-round cumulative roll-up. For every active-race-car slot that has a
    // CarData record:
    //   * fold this round's online finish-position score (CarScoreData +0x58,
    //     GetOnlineFinishPositionScore) into the car's running cumulative points
    //     (CarData +0x130, IncrementCumulativePoints);
    //   * on the FINAL roll-up (lbFinal), if the car has not yet had a disconnect
    //     round stamped (miRoundDisconnectedIn == -1) AND the player is flagged
    //     disconnected, stamp the round number passed in (the X360 stores a3 ==
    //     liNumCars into +0x134; param name is DWARF-authoritative).
    //
    // The X360 body then, when lbFinal, fires a virtual on mpCurrentOnlineModeScoring
    // (vtable+0x1C == slot 7 == AwardNetworkRatings, args (this, luRound)) to let the active
    // online-mode scorer post its per-round network ratings. That slot is now declared on the
    // grown BaseOnlineModeScoring vtable, so the call is reinstated below.
    void ScoringSystem::UpdateCumulativeResults(u32 luRound, s32 liNumCars, bool lbFinal)
    {
        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            CarData* lpCar = GetCarData(static_cast<EActiveRaceCarIndex>(liSlot));
            if (lpCar)
            {
                // miCumulativePoints (+0x130) += miOnlineFinishPositionScore (+0x58)
                lpCar->IncrementCumulativePoints(lpCar->GetScoreData()->GetOnlineFinishPositionScore());

                if (lbFinal)
                {
                    if (lpCar->GetRoundDisconnected() == -1)
                    {
                        if (GetPlayerDisconnected(lpCar->GetNetworkPlayerID()))
                        {
                            lpCar->SetRoundDisconnected(liNumCars);
                        }
                    }
                }
            }
        }

        if (lbFinal)
        {
            CGS_ASSERT(mpCurrentOnlineModeScoring, "mpCurrentOnlineModeScoring");

            // X360: result = (*(**(this+19912)+28))(mpCurrentOnlineModeScoring, this, luRound);
            // vtable+0x1C == BaseOnlineModeScoring slot 7 == AwardNetworkRatings, dispatched on the
            // live online-mode scorer so it can post this round's network ratings. The slot is now
            // declared on the (grown) BrnBaseOnlineModeScoring vtable, so the call is reinstated.
            // The third X360 argument is a2 (== luRound), forwarded as luNumActiveRaceCars per the
            // declared signature.
            mpCurrentOnlineModeScoring->AwardNetworkRatings(this, luRound);
        }
    }

    // ------------------------------------------------------------------------
    // DetectPlayerDrivingWrongWay  --  X360 0x8232B6B8  (BrnScoringSystem.cpp:1932)
    // ------------------------------------------------------------------------
    // Per-frame "is the player heading away from the finish?" detector.
    // The player car must be set and active (asserts). We snapshot last frame's
    // distance-to-finish (mfPlayerDistanceToFinishLastFrame), re-evaluate the live
    // distance via GetRaceCarDistanceToFinish(lePlayerIndex) and store it back. If the
    // distance did NOT shrink (>= last frame), the player is going the wrong way, so we
    // accumulate the elapsed time into mfPlayerTimeHeadingTheWrongWay; otherwise we reset
    // that timer to zero. (lpOutput resolves to the RaceCarEntityModuleIO active interface;
    // its accessors are read BY NAME.)
    void ScoringSystem::DetectPlayerDrivingWrongWay(const ActiveRaceCarOutputInterface* lpOutput,
                                                    f32 lfDeltaTime)
    {
        CGS_ASSERT(lpOutput != NULL, "lpActiveRaceCarInterface != NULL");

        const EActiveRaceCarIndex lePlayerIndex = lpOutput->GetPlayerActiveRaceCarIndex();
        CGS_ASSERT(lePlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID, "Player car index hasn't been set");
        CGS_ASSERT(lpOutput->IsRaceCarActive(lePlayerIndex),
                   "lpActiveRaceCarInterface->IsRaceCarActive( lePlayerActiveRaceCarIndex )");

        const f32 lfDistanceLastFrame = mfPlayerDistanceToFinishLastFrame;
        const f32 lfDistanceThisFrame = GetRaceCarDistanceToFinish(lePlayerIndex);
        mfPlayerDistanceToFinishLastFrame = lfDistanceThisFrame;

        if (lfDistanceThisFrame >= lfDistanceLastFrame)
        {
            mfPlayerTimeHeadingTheWrongWay += lfDeltaTime;
        }
        else
        {
            mfPlayerTimeHeadingTheWrongWay = 0.0f;
        }
    }

    // ------------------------------------------------------------------------
    // DetectPlayerStationary  --  X360 0x82320008  (BrnScoringSystem.cpp:1968)
    // ------------------------------------------------------------------------
    // Per-frame "is the player sitting still / not giving input?" detector, maintaining two
    // timers: mfPlayerTimeStationary and mfPlayerTimeWithoutInput.
    //
    // When the player car is NOT active, both timers reset to zero. Otherwise both timers are
    // first advanced by the frame delta, then selectively cleared from the player's RaceCarState:
    //   * if the car was reset, or is on the gas (mfGas > 0), or mid barrel-roll
    //     (mfInProgressBarrelRollAngle > 0), or braking hard (mfHandBrake / mfBrake > 0.1) --
    //     it is clearly under control, so BOTH timers reset;
    //   * additionally, if the car is moving (mfMaxSpeedMPH > 2.0) the stationary timer resets;
    //   * additionally, if there is meaningful boost-time magnitude (|mfTimeBoosting| > 0.1) the
    //     no-input timer resets.
    // The X360 returns the result pointer (a this/result-register artifact of the void-returning
    // C++ method); the reconstruction drops it. RaceCarState fields are read BY NAME.
    void ScoringSystem::DetectPlayerStationary(const ActiveRaceCarOutputInterface* lpOutput,
                                               f32 lfDeltaTime)
    {
        CGS_ASSERT(lpOutput != NULL, "lpActiveRaceCarInterface != NULL");

        const EActiveRaceCarIndex lePlayerIndex = lpOutput->GetPlayerActiveRaceCarIndex();
        CGS_ASSERT(lePlayerIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        bool lbPlayerCarActive = false;
        if (lePlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            lbPlayerCarActive = lpOutput->IsPlayerCarActive();
        }

        if (lbPlayerCarActive)
        {
            CGS_ASSERT(lePlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID, "Player car index hasn't been set");

            mfPlayerTimeStationary  += lfDeltaTime;
            mfPlayerTimeWithoutInput += lfDeltaTime;

            const BrnPhysics::Vehicle::RaceCarState* lpState = lpOutput->GetRaceCarState(lePlayerIndex);

            if (lpState->mbResetCarTransform
                || lpState->mfGas > 0.0f
                || lpState->mfInProgressBarrelRollAngle > 0.0f
                || lpState->mfHandBrake > 0.1f
                || lpState->mfBrake > 0.1f)
            {
                mfPlayerTimeStationary  = 0.0f;
                mfPlayerTimeWithoutInput = 0.0f;
            }

            if (lpState->mfMaxSpeedMPH > 2.0f)
            {
                mfPlayerTimeStationary = 0.0f;
            }

            if (std::fabs(lpState->mfTimeBoosting) > 0.1f)
            {
                mfPlayerTimeWithoutInput = 0.0f;
            }
        }
        else
        {
            mfPlayerTimeStationary  = 0.0f;
            mfPlayerTimeWithoutInput = 0.0f;
        }
    }
}
