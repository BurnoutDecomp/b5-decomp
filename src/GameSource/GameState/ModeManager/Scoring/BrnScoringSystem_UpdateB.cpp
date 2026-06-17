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
//   UpdateCumulativeResults    (0x8231FCA0)  [CarScoreData/CarData part; one vtable call FLAGGED]
//   DetectPlayerDrivingWrongWay(0x8232B6B8)  [now lpOutput resolves to the real active interface]
//   DetectPlayerStationary     (0x82320008)  [now lpOutput resolves to the real active interface]
//
// BLOCKED (body omitted -- left declare-only; each needs a type/method that has
// no committed definition reachable from a body agent; the dependency RULE
// forbids ad-hoc-slicing a shared keystone). Ledger:
//
//   UpdateNetworkPlayerResults (0x8231FA90)
//       Walks the lpResults per-player record stream (X360: a2+24, stride 28)
//       reading PlayerResultsData fields (timed-out / race-car index / finish
//       time / eliminations) and writing them onto each CarData, then fires a
//       virtual on mpCurrentOnlineModeScoring (vtable+0x18). PlayerResultsData
//       is modelled as an unnamed u8[8*28] blob on the committed
//       PlayerResultsInterface (BrnNetworkModuleIO.h) -- no named fields/accessor
//       -- and the BaseOnlineModeScoring slice declares no such virtual.
//       MISSING: named BrnNetwork::...::PlayerResultsData fields (+ accessor)
//                and the BaseOnlineModeScoring vtable+0x18 virtual.
//
//   UpdateCumulativeResults    (0x8231FCA0)  [LANDED -- CarScoreData/CarData part]
//       Per car, accumulates the round's online points into miCumulativePoints
//       (miCumulativePoints += CarScoreData[+0x58]) and, when final, stamps the
//       disconnect round. The CarScoreData getter for the +0x58 slot now exists
//       (GetOnlineFinishPositionScore), so the per-car accumulation + disconnect
//       stamping are reconstructed below. The trailing vtable+0x1C call on
//       mpCurrentOnlineModeScoring is STILL FLAGGED -- the minimal
//       BaseOnlineModeScoring slice declares only one virtual and growing it with
//       a slot-7 virtual is a keystone/vtable-layout change out of scope for a
//       body agent; that call is omitted (see the in-body FLAG comment).
//       REMAINING DEPENDENCY: BaseOnlineModeScoring vtable+0x1C virtual
//       (called as `(*(**(this+19912)+28))(mpCurrentOnlineModeScoring, this, luRound)`).
//
//   UpdateDistanceToPlayer     (0x8232B408)
//       Per car in maCarsInTheRace, computes |GetPlayerPosition() - maCarsInTheRace[i].mPosition|
//       and stores it onto the car's CarScoreData distance-to-player slot. lpOutput now resolves
//       (the typedef -> RCEntityActiveRaceCarOutputInterface is complete here), so the interface
//       side is fine -- BUT the destination is CarScoreData +0x1C (X360 stfs 0x1C(GetCarData)),
//       which falls inside the unnamed maStorage1C[8] blob of GameStateModuleIO::CarScoreData;
//       there is no committed named member/accessor for it. Writing it by name needs CarScoreData
//       (a shared keystone in BrnGameStateSharedIO.h) GROWN with that distance-to-player field --
//       out of scope for this body agent.
//       MISSING: GameStateModuleIO::CarScoreData distance-to-player field (+0x1C, named member).
//
//   UpdateGeneralStats         (0x8232B8C0)
//       Per-frame general per-car stat roll-up (lead/last time-in-place, per-car drift distance,
//       longest drift, boost time). lpOutput now resolves, and the lead/last/boost Time fields it
//       touches DO have committed CarScoreData accessors (mTimeInFirstPlace/mTimeInLastPlace/
//       mTimeBoosting) as does CarData::mfCurrentDriftDistance. HOWEVER it also reads+writes two
//       CarScoreData fields with no committed name: +0xDC (per-frame distance accumulator; inside
//       maStorageDA[6]) and +0xF8 (longest-drift target; inside maStorageF8[48]). Both need
//       CarScoreData grown with named members -- out of scope for this body agent.
//       MISSING: GameStateModuleIO::CarScoreData fields at +0xDC and +0xF8 (named members).
//
// Both blocked methods now hinge solely on GROWING the CarScoreData keystone (the old
// "ActiveRaceCarOutputInterface has no definition" blocker is RESOLVED -- the keystone typedef
// now aliases the real RCEntityActiveRaceCarOutputInterface, completed by the include above).
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
    // (vtable+0x1C, args (this, luRound)) to let the active online-mode scorer post
    // its own per-round results. That virtual is NOT declared on the committed
    // minimal BaseOnlineModeScoring slice (which exposes only one virtual,
    // GetCurrentPlayerTeam); declaring a slot-7 virtual there is a vtable-layout
    // change to a shared keystone, out of scope for this body agent. The call is
    // therefore OMITTED and FLAGGED below -- everything that touches CarScoreData /
    // CarData (the part this agent owns) is reconstructed faithfully.
    void ScoringSystem::UpdateCumulativeResults(u32 luRound, s32 liNumCars, bool lbFinal)
    {
        (void)luRound;   // consumed only by the still-flagged vtable+0x1C call (see below)

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

            // FLAGGED REMAINING DEPENDENCY -- BaseOnlineModeScoring vtable+0x1C virtual.
            // X360: result = (*(**(this+19912)+28))(mpCurrentOnlineModeScoring, this, luRound);
            // i.e. a virtual call on mpCurrentOnlineModeScoring passing (this ScoringSystem*,
            // luRound) so the live online-mode scorer can record this round's standings/results.
            // The committed BrnBaseOnlineModeScoring slice declares no virtual at slot 7; wiring
            // it in requires growing that keystone's vtable (slot ordering not yet committed) and
            // is out of scope for a body agent. Reinstate once that virtual is declared.
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
