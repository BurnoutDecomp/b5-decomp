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
//   UpdateNetworkPlayerResults (0x8231FA90)  [NEW -- network keystone grown with the typed
//                                             PlayerResultsData record + GetPlayerResultsData accessor
//                                             (BrnNetworkModuleIO.h), CarScoreData grown with the
//                                             +0x6B SetHasNetworkResults flag; dispatches the slot-5
//                                             BaseOnlineModeScoring::Update virtual]
//   UpdateGeneralStats         (0x8232B8C0)  [NEW -- CarScoreData grown with mfLongestDrift/
//                                             Get/SetLongestDrift (+0xF8) + mfDistanceAccumulator/
//                                             Get/SetDistanceAccumulator (+0xDC) + the
//                                             Time-in-first/last/boosting accessors (+0xE0/+0xE8/+0xF0);
//                                             the per-frame drift run accumulator is CarData::
//                                             mfCurrentDriftDistance (+0x138, Get/Set/IncrementDriftDistance).
//                                             RaceCarState fields read BY NAME (mfMaxSpeedMPH +0x3CC,
//                                             mfTimeInAir +0x400, mfInProgressBarrelRollAngle +0x418).
//                                             FLAG: the +0xF8/+0x138 pair's gameplay identity is inferred
//                                             from the max-of-a-conditional-accumulator pattern -- the
//                                             committed names (mfLongestDrift / mfCurrentDriftDistance)
//                                             are provisional (the run is gated on mfTimeInAir, which
//                                             reads more like an air/jump-distance run than a drift);
//                                             reproduced verbatim against the committed names, not retyped.]
//   SetNetworkStuntMultiplier  (0x8231FC50)  [NEW -- CarScoreData grown with the chainable-trio
//                                             accessors (Get/SetChainableScore +0xC8, Get/Set
//                                             CurrentChainableMultiplier +0xD0, Get/SetChainActive +0xD8)]
//   PlayerPerformedBarrelRolls (0x823634D0)  [NEW -- CarScoreData grown with the carved per-car
//                                             barrel-roll tally miBarrelRollCount (+0xFC) + AddBarrelRolls]
//   UpdateOnlineStuntModeScorePreWorld (0x8234CE48) [NEW -- per-car chainable expire/gather loop into a
//                                             local Array<CarScoreData::ChainableMultiplierInfo,8>, then
//                                             (unless suppressed) drives mOnlineStuntModeScoring.
//                                             PreWorldUpdate(queue) (ScoringSystem friended into the
//                                             scorer for the protected call) + hands the 132-byte table to
//                                             the scorer's display buffer]
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

// Completes BrnGameState::PlayerResultsInterface (a typedef alias of
// BrnNetwork::BrnNetworkModuleIO::PlayerResultsInterface): UpdateNetworkPlayerResults dereferences it
// BY NAME (GetPlayerResultsData -> PlayerResultsData fields). This homes the typed PlayerResultsData
// record + the GetPlayerResultsData accessor (network keystone grow).
#include "GameSource/Network/BrnNetworkModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (Bad-case-of-Road-Rage assert)

// CgsSystem::Time(f32) ctor + Time::operator+= -- UpdateGeneralStats folds the frame delta into the
// lead/last/boost Time accumulators (the X360 CgsSystem::Time::Time(deltaTime) + operator+_ pair).
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

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

    // ------------------------------------------------------------------------
    // UpdateNetworkPlayerResults  --  X360 0x8231FA90  (BrnScoringSystem.cpp:797)
    // ------------------------------------------------------------------------
    // Folds the network module's per-player result records (lpResults, a
    // BrnNetwork::BrnNetworkModuleIO::PlayerResultsInterface holding maPlayerResultsData[8]) onto each
    // car's CarScoreData, then drives the active online-mode scorer's per-frame Update.
    //
    // Only runs on the FINAL update (lbFinal). For each of the muCarsInCurrentMode result slots it
    // reads the record via GetPlayerResultsData(liIndex) and, when the record is valid and names a
    // real active race-car slot (meActiveRaceCarIndex != INVALID) that maps to a live CarData, copies:
    //   mFinishTime          -> CarScoreData.SetFinishTime           (+0x00)
    //   mfDistanceToFinish   -> CarScoreData.SetDistanceToFinish     (+0x48)
    //   meEliminatorIndex    -> CarScoreData.SetEliminatorRaceCarIndex (+0x60)
    //   mbTimedOut           -> CarScoreData.mbTimedOut (Set... n/a; written directly) (+0x68)
    //   miEliminations       -> CarScoreData.SetNumEliminations      (+0x64)
    //   mbEliminated         -> CarScoreData.SetEliminated           (+0xD9)
    //   (constant true)      -> CarScoreData.SetHasNetworkResults    (+0x6B)
    // (The X360 reloads the validity byte once more before the writes -- a redundant reload of the
    // same +0x18 flag; the single mbValid test reproduces it.) Finally it dispatches the slot-5
    // BaseOnlineModeScoring::Update virtual on the live online-mode scorer with the active-car count.
    // The X360 "result pointer" return is the void method's result-register artifact and is dropped.
    void ScoringSystem::UpdateNetworkPlayerResults(const PlayerResultsInterface* lpResults, bool lbFinal)
    {
        CGS_ASSERT(lpResults != NULL, "lpNetworkResultsInterface");

        if (lbFinal)
        {
            for (s32 liIndex = 0; liIndex < static_cast<s32>(muCarsInCurrentMode); ++liIndex)
            {
                const BrnNetwork::BrnNetworkModuleIO::PlayerResultsData* lpPlayerResultsData =
                    lpResults->GetPlayerResultsData(liIndex);
                CGS_ASSERT(lpPlayerResultsData != NULL, "lpPlayerResultsData");

                if (lpPlayerResultsData->mbValid)
                {
                    const EActiveRaceCarIndex leActiveRaceCarIndex =
                        static_cast<EActiveRaceCarIndex>(lpPlayerResultsData->meActiveRaceCarIndex);
                    if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                    {
                        CarData* lpCar = GetCarData(leActiveRaceCarIndex);
                        if (lpCar)
                        {
                            GameStateModuleIO::CarScoreData* lpScore = lpCar->GetScoreData();
                            lpScore->SetFinishTime(lpPlayerResultsData->mFinishTime);
                            lpScore->SetDistanceToFinish(lpPlayerResultsData->mfDistanceToFinish);
                            lpScore->SetEliminatorRaceCarIndex(
                                static_cast<EActiveRaceCarIndex>(lpPlayerResultsData->meEliminatorIndex));
                            lpScore->SetTimedOut(lpPlayerResultsData->mbTimedOut);
                            lpScore->SetHasNetworkResults(true);
                            lpScore->SetNumEliminations(lpPlayerResultsData->miEliminations);
                            lpScore->SetEliminated(lpPlayerResultsData->mbEliminated);
                        }
                    }
                }
            }

            CGS_ASSERT(mpCurrentOnlineModeScoring, "mpCurrentOnlineModeScoring");

            // X360: (*(*mpCurrentOnlineModeScoring + 24))(mpCurrentOnlineModeScoring, this, muCarsInCurrentMode)
            // vtable byte +0x18 (==24) == BaseOnlineModeScoring slot 6 == UpdatePlayerPoints (slot 5/+0x14 is Update).
            mpCurrentOnlineModeScoring->UpdatePlayerPoints(this, static_cast<s32>(muCarsInCurrentMode));
        }
    }

    // ------------------------------------------------------------------------
    // UpdateGeneralStats  --  X360 0x8232B8C0  (BrnScoringSystem.cpp:2098)
    // ------------------------------------------------------------------------
    // Per-frame general per-car stat roll-up, called from ModeManager::PostWorldUpdate.
    //
    // lpOutput is the active-race-car output interface; its maCarsInTheRace list gives the active-car
    // count (its GetLength()) and, per entry, the EActiveRaceCarIndex this car maps to. The X360 asserts
    // the count is in [1,8].
    //
    // Two phases:
    //
    //   (1) lead / last time-in-place (only when lbOnline). With both the lead (meLeadRaceCarIndex)
    //       and last (meLastRaceCarIndex) cars resolved to a CarData:
    //         * while NOBODY has finished the race (muNumCarsFinishedRace == 0) the lead car accrues
    //           the frame delta into its CarScoreData mTimeInFirstPlace (+0xE0);
    //         * while NOT EVERY active car has finished (muNumCarsFinishedRace < muCarsInCurrentMode)
    //           the last car accrues the frame delta into its mTimeInLastPlace (+0xE8).
    //       (X360 reads ss+0x4EF0/+0x4EF4 for the indices, ss+0x4EDC == muNumCarsFinishedRace and
    //       ss+0x4EE8 == muCarsInCurrentMode for the gates; the lead/last asserts pin the indices in
    //       [0,8). The Time accrue is CgsSystem::Time::Time(dt) + operator+= on +0xE0 / +0xE8.)
    //
    //   (2) per active-race-car loop. For each maCarsInTheRace[i] whose race car IS active
    //       (IsRaceCarActive) and that maps to a live CarData, and only while that car has NOT yet
    //       finished (CarScoreData mbHasFinished, +0x45):
    //         * mfDistanceAccumulator (+0xDC) += |RaceCarState.mfMaxSpeedMPH| * dt;
    //         * if mid barrel-roll (RaceCarState.mfInProgressBarrelRollAngle > 0) the car accrues the
    //           frame delta into mTimeBoosting (+0xF0);
    //         * drift/air run bookkeeping on the mfTimeInAir gate (RaceCarState +0x400):
    //             - while airborne (mfTimeInAir != 0) the per-car run accumulator CarData::
    //               mfCurrentDriftDistance (+0x138) += |mfMaxSpeedMPH| * dt;
    //             - when the run lapses (mfTimeInAir == 0) the run length is compared against the
    //               recorded maximum CarScoreData mfLongestDrift (+0xF8); if greater it is recorded and
    //               the run accumulator is reset to 0.
    //
    // FLAG: the +0xF8/+0x138 pair is gated on mfTimeInAir, so the committed names mfLongestDrift /
    // mfCurrentDriftDistance read more like an air/jump-distance run than a true drift -- the gameplay
    // identity is provisional. The body is reproduced VERBATIM against the committed names/accessors
    // (no retype, no home grow). The X360 "result pointer" return is the void method's result-register
    // artifact and is dropped.
    void ScoringSystem::UpdateGeneralStats(const ActiveRaceCarOutputInterface* lpOutput,
                                           f32 lfDeltaTime, bool lbOnline)
    {
        const u32 luNumberOfActiveRaceCars = lpOutput->maCarsInTheRace.GetLength();
        CGS_ASSERT(luNumberOfActiveRaceCars >= 1, "liNumberOfActiveRaceCars >= 1");
        CGS_ASSERT(luNumberOfActiveRaceCars <= 8, "Amount of active race cars is too many");

        // ---- (1) lead / last time-in-place (online only) ----
        if (lbOnline)
        {
            const EActiveRaceCarIndex leLeadIndex = meLeadRaceCarIndex;
            const EActiveRaceCarIndex leLastIndex = meLastRaceCarIndex;

            // The X360 only enters this block when BOTH indices are set (>= 0); the lead/last asserts
            // pin them in [E_ACTIVE_RACE_CAR_INDEX_0, E_ACTIVE_RACE_CAR_INDEX_COUNT).
            if (leLeadIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && leLastIndex >= E_ACTIVE_RACE_CAR_INDEX_0)
            {
                CGS_ASSERT(leLeadIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "meLeadRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                CGS_ASSERT(leLastIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "meLastRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                CGS_ASSERT(leLeadIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "meLeadRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                CGS_ASSERT(leLastIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "meLastRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

                CarData* lpLeadCar = GetCarData(leLeadIndex);
                CarData* lpLastCar = GetCarData(leLastIndex);

                if (lpLeadCar && muNumCarsFinishedRace == 0)
                {
                    GameStateModuleIO::CarScoreData* lpScore = lpLeadCar->GetScoreData();
                    CgsSystem::Time lTime = lpScore->GetTimeInFirstPlace();
                    lTime += CgsSystem::Time(lfDeltaTime);
                    lpScore->SetTimeInFirstPlace(lTime);
                }

                if (lpLastCar && muNumCarsFinishedRace < muCarsInCurrentMode)
                {
                    GameStateModuleIO::CarScoreData* lpScore = lpLastCar->GetScoreData();
                    CgsSystem::Time lTime = lpScore->GetTimeInLastPlace();
                    lTime += CgsSystem::Time(lfDeltaTime);
                    lpScore->SetTimeInLastPlace(lTime);
                }
            }
        }

        // ---- (2) per active-race-car stat loop ----
        for (u32 i = 0; i < luNumberOfActiveRaceCars; ++i)
        {
            const EActiveRaceCarIndex leActiveRaceCarIndex = lpOutput->maCarsInTheRace[i].meActiveRaceCarIndex;
            CarData* lpCar = GetCarData(leActiveRaceCarIndex);

            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            if (lpOutput->IsRaceCarActive(leActiveRaceCarIndex) && lpCar)
            {
                const BrnPhysics::Vehicle::RaceCarState* lpState =
                    lpOutput->GetRaceCarState(leActiveRaceCarIndex);

                GameStateModuleIO::CarScoreData* lpScore = lpCar->GetScoreData();
                if (!lpScore->GetHasFinished())
                {
                    // mfDistanceAccumulator (+0xDC) += |mfMaxSpeedMPH| * dt
                    lpScore->SetDistanceAccumulator(
                        std::fabs(lpState->mfMaxSpeedMPH) * lfDeltaTime + lpScore->GetDistanceAccumulator());

                    // mid barrel-roll -> accrue boost time (+0xF0)
                    if (lpState->mfInProgressBarrelRollAngle > 0.0f)
                    {
                        CgsSystem::Time lTime = lpScore->GetTimeBoosting();
                        lTime += CgsSystem::Time(lfDeltaTime);
                        lpScore->SetTimeBoosting(lTime);
                    }

                    // drift/air run accumulator gated on mfTimeInAir (+0x400)
                    if (lpState->mfTimeInAir == 0.0f)
                    {
                        // run lapsed: commit the longest-run maximum, then reset the run accumulator
                        const f32 lfRun = lpCar->GetDriftDistance();          // CarData +0x138
                        if (lfRun > lpScore->GetLongestDrift())               // CarScoreData +0xF8
                        {
                            lpScore->SetLongestDrift(lfRun);
                            lpCar->SetDriftDistance(0.0f);
                        }
                    }
                    else
                    {
                        // airborne: extend the run by |mfMaxSpeedMPH| * dt
                        lpCar->IncrementDriftDistance(std::fabs(lpState->mfMaxSpeedMPH) * lfDeltaTime);
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------------
    // SetPlayerStuntScore -- X360 0x8231F0C8
    // Store the per-car online stunt score (CarScoreData +0xD4) for the car at the
    // given active-race-car slot. No-op when the slot has no car record. (The X360
    // body re-runs the GetCarData lookup after the null guard; reproduced once here.)
    // ------------------------------------------------------------------------
    void ScoringSystem::SetPlayerStuntScore(EActiveRaceCarIndex leRaceCarIndex, s32 liScore)
    {
        CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 &&
                   leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 && leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        CarData* lpCar = GetCarData(leRaceCarIndex);
        if (lpCar)
        {
            lpCar->GetScoreData()->SetOnlineStuntScore(liScore);
        }
    }

    // ------------------------------------------------------------------------
    // SetNetworkStuntScore -- X360 0x8231FC20
    // Store the per-car online stunt score (CarScoreData +0xD4) for the car owned by
    // the given network player. The lookup (GetCarData(NetworkPlayerID), X360
    // sub_8231DD88) asserts a valid id and never returns NULL on the live path; the
    // X360 body writes the score unconditionally on the returned record.
    // ------------------------------------------------------------------------
    void ScoringSystem::SetNetworkStuntScore(BrnNetwork::NetworkPlayerID lID, s32 liScore)
    {
        CarData* lpCar = GetCarData(lID);
        lpCar->GetScoreData()->SetOnlineStuntScore(liScore);
    }

    // ------------------------------------------------------------------------
    // DealWithStunt -- X360 0x823384F0
    // Forward a completed world-stunt action to the active stunt-mode scorer: the
    // ONLINE scorer (mOnlineStuntModeScoring, X360 ss+0x2620) when lbOnline is set,
    // otherwise the offline scorer (mStuntModeScoring, ss+0x350). The keystone is a
    // thin selector around StuntModeScoring::DealWithStunt; the small WorldStuntAction
    // the X360 ABI splits across r4/r5 is modeled by pointer in the scorer's home.
    // ------------------------------------------------------------------------
    void ScoringSystem::DealWithStunt(const GameStateModuleIO::WorldStuntAction* lpAction, bool lbOnline)
    {
        StuntModeScoring* lpStuntModeScoring = lbOnline ? &mOnlineStuntModeScoring : &mStuntModeScoring;
        CGS_ASSERT(lpStuntModeScoring != NULL, "lpStuntModeScoring");

        lpStuntModeScoring->DealWithStunt(lpAction);
    }

    // ------------------------------------------------------------------------
    // SetNetworkStuntMultiplier -- X360 0x8231FC50
    // ------------------------------------------------------------------------
    // Arm the per-car "chainable stunt multiplier" trio for the car owned by the given
    // network player. The lookup (GetCarData(NetworkPlayerID), X360 sub_8231DD88) returns
    // NULL when no such car is registered; only on a hit does the X360 body write:
    //   miChainableScore (+0xC8) = liChainableScore   (asm std r30,0xC8 -- r30 == a4)
    //   miCurrentChainableMultiplier (+0xD0) = liMultiplier  (asm stw r31,0xD0 -- r31 == a3)
    //   mbChainActive (+0xD8) = 1                      (asm stb 1,0xD8)
    // Arg order reconciled from the register setup: r5(a3) is the multiplier (->+0xD0),
    // r6(a4) is the chainable score (->+0xC8); the declared signature names them
    // (lID, liMultiplier, liChainableScore). The +0xC8 store is a 64-bit `std` whose low
    // word is the chainable score; the high word (miChainableField, +0xCC) is the
    // store's incidental tail and is not a meaningful value -- only the named s32 score is
    // reproduced here.
    void ScoringSystem::SetNetworkStuntMultiplier(BrnNetwork::NetworkPlayerID lID, s32 liMultiplier, s32 liChainableScore)
    {
        CarData* lpCar = GetCarData(lID);
        if (lpCar)
        {
            GameStateModuleIO::CarScoreData* lpScore = lpCar->GetScoreData();
            lpScore->SetChainableScore(liChainableScore);
            lpScore->SetCurrentChainableMultiplier(liMultiplier);
            lpScore->SetChainActive(true);
        }
    }

    // ------------------------------------------------------------------------
    // PlayerPerformedBarrelRolls -- X360 0x823634D0
    // ------------------------------------------------------------------------
    // Add liCount to the per-car barrel-roll tally (CarScoreData +0xFC) for the car at the
    // given active-race-car slot. The X360 body asserts the index is in
    // (E_ACTIVE_RACE_CAR_INDEX_INVALID, E_ACTIVE_RACE_CAR_INDEX_COUNT), looks the car up via
    // GetCarData(EActiveRaceCarIndex) (sub_8231DCD0) and, on a hit, does
    // *(record + 0xFC) += liCount (asm lwz/add/stw 0xFC).
    void ScoringSystem::PlayerPerformedBarrelRolls(EActiveRaceCarIndex leRaceCarIndex, s32 liCount)
    {
        CGS_ASSERT(leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                   leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "( leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

        CarData* lpCar = GetCarData(leRaceCarIndex);
        if (lpCar)
        {
            lpCar->GetScoreData()->AddBarrelRolls(liCount);
        }
    }

    // ------------------------------------------------------------------------
    // UpdateOnlineStuntModeScorePreWorld -- X360 0x8234CE48
    // ------------------------------------------------------------------------
    // Per-frame online stunt pre-world step. a2 is the current time in ms; a3 is the per-frame
    // output-action queue (CgsModule::VariableEventQueue<13312,16>*). The IDA garbage middle arg
    // is dropped (the X360 ABI threads time in r4, queue in r5).
    //
    // Phase 1 -- per active-race-car slot loop (0..7, X360 asserts the running index <= 8):
    //   For each slot's CarData (GetCarData(slot), sub_8231DCD0):
    //     * EXPIRE: when the chain is active (mbChainActive, +0xD8) and it has gone stale --
    //       (current time - 300ms) > miCurrentChainableMultiplier (+0xD0) -- clear the whole
    //       chainable group: miChainableScore (+0xC8), miChainableField (+0xCC),
    //       miCurrentChainableMultiplier (+0xD0) and mbChainActive (+0xD8) all to 0.
    //       (The X360 clears +0xC8 with a word store and +0xCC..+0xD0 with two halfword stores;
    //       reproduced here as the named-field clears.)
    //     * GATHER: when the chain is (still) active and not yet at the cutoff
    //       (miCurrentChainableMultiplier < current time), append a ChainableMultiplierInfo to a
    //       local Array<...,8> (X360 Array::AddNew, sub_823177E8 -- asserts the element ptr) filled
    //       with { miChainableScore (+0xC8), miChainableField (+0xCC), the slot index, and
    //       miCurrentChainableMultiplier (+0xD0) }.
    //
    // Phase 2 -- unless the online scorer suppresses it (X360 lbz this+0x2620+0x2515): drive the
    //   online stunt scorer's PreWorldUpdate (mOnlineStuntModeScoring.PreWorldUpdate(queue)) and hand
    //   the gathered 132-byte table to its display buffer (X360 memcpy into +0x2620+0x23F0).
    //
    // The X360 "result pointer" return is the void method's result-register artifact and is dropped.
    void ScoringSystem::UpdateOnlineStuntModeScorePreWorld(s32 liCurrentTimeMs,
                                                           CgsModule::VariableEventQueue<13312, 16>* lpOutputActionQueue)
    {
        Array<GameStateModuleIO::CarScoreData::ChainableMultiplierInfo, 8> laChainableMultipliers;
        laChainableMultipliers.Clear();

        for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
        {
            CarData* lpCar = GetCarData(static_cast<EActiveRaceCarIndex>(liSlot));
            if (lpCar)
            {
                GameStateModuleIO::CarScoreData* lpScore = lpCar->GetScoreData();

                // EXPIRE a stale chain (current time - 300ms past the recorded multiplier slot).
                if (lpScore->GetChainActive() &&
                    (liCurrentTimeMs - 300) > lpScore->GetCurrentChainableMultiplier())
                {
                    lpScore->SetChainableScore(0);
                    lpScore->SetChainableField(0);
                    lpScore->SetCurrentChainableMultiplier(0);
                    lpScore->SetChainActive(false);
                }

                // GATHER an active chain that has not reached its cutoff time.
                if (lpScore->GetChainActive() &&
                    lpScore->GetCurrentChainableMultiplier() < liCurrentTimeMs)
                {
                    GameStateModuleIO::CarScoreData::ChainableMultiplierInfo* lpInfo =
                        laChainableMultipliers.AddNew();
                    CGS_ASSERT(lpInfo != NULL, "lpChainableMultiplierInfo");

                    lpInfo->miChainableScore = lpScore->GetChainableScore();
                    lpInfo->miChainableField = lpScore->GetChainableField();
                    lpInfo->miSuppliedValue  = liSlot;
                    lpInfo->miMultiplier     = lpScore->GetCurrentChainableMultiplier();
                }
            }
        }

        if (!mOnlineStuntModeScoring.IsOnlineChainableUpdateSuppressed())
        {
            mOnlineStuntModeScoring.PreWorldUpdate(lpOutputActionQueue);
            mOnlineStuntModeScoring.SetOnlineChainableMultiplierDisplay(
                &laChainableMultipliers,
                StuntModeScoring::KI_ONLINE_CHAINABLE_DISPLAY_BYTES);
        }
    }
}
