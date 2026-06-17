#include "GameSource/GameState/ModeManager/Scoring/BrnOnlineBurningHomeRunModeScoring.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"
#include "GameSource/BurnoutConstants.h"

#include <stdlib.h> // qsort

namespace BrnGameState
{
// ===== ComparisonData -- file-local helper (DWARF home BrnOnlineBurningHomeRunModeScoring.cpp:37) =====
// One row per active race car; an array of 8 is built on the stack, sorted by
// _BurningHomeRunPlayerFinishTimesCompare, then walked to assign finishing positions.
//
// X360-AUTHORITATIVE 36-byte stride: qsort(..., 8, 36, ...) in UpdatePlayerPoints pins the element
// size, and the comparator's byte offsets (+4 NetworkPlayerID, +8/+12 finish Time, +16/+20 runner
// Time, +24 takedowns, +28 checkpoints, +32/+33/+34 bool flags) pin every field position. Trailing
// 1 byte of pad lands at +35 to reach the 36-byte stride.
struct ComparisonData
{
    EActiveRaceCarIndex         meRaceCarIndex;                  // +0x00 (4)
    BrnNetwork::NetworkPlayerID mNetworkPlayerID;                // +0x04 (4)  sentinel -1 == invalid/empty slot
    CgsSystem::Time             mFinishTime;                     // +0x08 (8)  {miSeconds@+8, mfFraction@+12}
    CgsSystem::Time             mTimeAsRunner;                   // +0x10 (8)  {miSeconds@+16, mfFraction@+20}
    s32                         miTakedowns;                     // +0x18 (4)
    s32                         miNumberOfCumlativeCheckpoints;  // +0x1C (4)
    bool                        mbCompletedBurningHomeRun;       // +0x20 (1)
    bool                        mbTimedOut;                      // +0x21 (1)
    bool                        mbDisconnected;                  // +0x22 (1)
    // +0x23 (1) implicit tail pad -> 36-byte (0x24) stride
};

// X360 @ 0x82315650. Push this scorer's three online-result arrays into the network output
// interface. Unlike the base WriteDataToOutput (which also writes maiNumEliminations[]), this
// Burning-Home-Run override copies only the award id / award variable / player team arrays.
// The X360 code is three unrolled 8-word copy loops (src this+0x04/0x24/0x44 -> dst +0x20/0x40/0x60);
// reconstructed as the equivalent named array copies (each KI_MAX_ACTIVE_RACE_CARS == 8 wide).
void
OnlineBurningHomeRunModeScoring::WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface* lpOutput)
{
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        lpOutput->maOnlineAwards[li]          = maOnlineAwards[li];
        lpOutput->maiOnlineAwardVariables[li] = maiOnlineAwardVariables[li];
        lpOutput->maePlayerTeam[li]           = maePlayerTeams[li];
    }
}

// X360 @ 0x823156C0. qsort comparator that ranks two cars for the Burning-Home-Run finishing order.
// Ordering policy (faithful to the X360 control flow):
//   * A car whose NetworkPlayerID == -1 (empty slot) always sorts last.
//   * "Unresolved" = the car is still running: it has NOT recorded a finish time AND has not timed
//     out AND has not disconnected.
//   * Both unresolved -> tie-break by checkpoints reached, then takedowns, then time-as-runner, then
//     network player id.
//   * Exactly one unresolved -> the unresolved (still racing) car sorts AFTER the resolved one.
//   * Neither unresolved -> precedence chain: completed-the-run beats timed-out beats disconnected;
//     within a tier, same checkpoint/takedown/runner/id tie-break.
// Returns <0 if lpPlayer1 ranks first, >0 if lpPlayer2 ranks first.
s32
OnlineBurningHomeRunModeScoring::_BurningHomeRunPlayerFinishTimesCompare(const void* lpPlayer1, const void* lpPlayer2)
{
    s32 liResult = 0;

    CGS_ASSERT(lpPlayer1, "lpPlayer1");
    CGS_ASSERT(lpPlayer2, "lpPlayer2");

    const ComparisonData* lpP1 = static_cast<const ComparisonData*>(lpPlayer1);
    const ComparisonData* lpP2 = static_cast<const ComparisonData*>(lpPlayer2);

    // "Has finished" == finish time is non-zero (X360 sums the Time's two words and tests != 0).
    const bool lbP1Finished = (lpP1->mFinishTime.GetFloatVal() != 0.0f);
    const bool lbP2Finished = (lpP2->mFinishTime.GetFloatVal() != 0.0f);

    // Still actively racing -> no result yet, and not knocked out of the running.
    const bool lbP1StillRunning =
        !(lbP1Finished || lpP1->mbTimedOut || lpP1->mbDisconnected);
    const bool lbP2StillRunning =
        !(lbP2Finished || lpP2->mbTimedOut || lpP2->mbDisconnected);

    // Empty slots (id == -1) always sort to the back.
    if (lpP1->mNetworkPlayerID == -1)
    {
        return 1;
    }
    if (lpP2->mNetworkPlayerID == -1)
    {
        return -1;
    }

    if (lbP1StillRunning && lbP2StillRunning)
    {
        // Both unresolved: rank on progress within the run.
        CompareCheckpointsReached(lpP1->miNumberOfCumlativeCheckpoints,
                                  lpP2->miNumberOfCumlativeCheckpoints, &liResult);
        CompareTakedowns(lpP1->miTakedowns, lpP2->miTakedowns, &liResult);
        CompareTimeAsRunner(lpP1->mTimeAsRunner, lpP2->mTimeAsRunner, &liResult);
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }

    if (lbP2StillRunning)
    {
        // Only P2 is still running -> P1 (already resolved) ranks first.
        return -1;
    }

    if (!lbP1StillRunning)
    {
        // Both cars are resolved (finished / timed-out / disconnected). Walk the precedence tiers:
        // completed-run, then timed-out, then disconnected. Equal tier -> shared tie-break.
        if (lpP1->mbDisconnected && lpP2->mbDisconnected)
        {
            // both disconnected (same tier): fall to the shared tie-break tail
        }
        else if (lpP2->mbDisconnected)
        {
            return -1;                       // P1 outranks a disconnected P2
        }
        else if (lpP1->mbDisconnected)
        {
            return 1;                        // P1 disconnected, P2 not -> P1 ranks after P2 (X360 worse-tier return 1)
        }
        else
        {
            // neither disconnected
            if (lpP1->mbTimedOut && lpP2->mbTimedOut)
            {
                // both timed out (same tier): fall to the shared tie-break tail
            }
            else if (lpP2->mbTimedOut)
            {
                return -1;                   // P1 outranks a timed-out P2
            }
            else if (lpP1->mbTimedOut)
            {
                return 1;                    // P1 timed out, P2 not -> P1 ranks after P2 (X360 worse-tier return 1)
            }
            else
            {
                // Neither timed out nor disconnected -> both must have completed the run.
                CGS_ASSERT(!(lpP1->mbCompletedBurningHomeRun && lpP2->mbCompletedBurningHomeRun),
                           "!( lpPlayer1->mbCompletedBurningHomeRun && lpPlayer2->mbCompletedBurningHomeRun )");
                if (lpP1->mbCompletedBurningHomeRun)
                {
                    liResult = -1;           // P1 completed -> ranks first
                }
                else if (lpP2->mbCompletedBurningHomeRun)
                {
                    liResult = 1;            // P2 completed -> ranks first
                }
                // else: neither completed -> fall through to shared tie-break with liResult==0
            }
        }

        // Shared tie-break tail (X360 LABEL_39 -> LABEL_40).
        CompareCheckpointsReached(lpP1->miNumberOfCumlativeCheckpoints,
                                  lpP2->miNumberOfCumlativeCheckpoints, &liResult);
        CompareTakedowns(lpP1->miTakedowns, lpP2->miTakedowns, &liResult);
        CompareTimeAsRunner(lpP1->mTimeAsRunner, lpP2->mTimeAsRunner, &liResult);
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }

    // Only P1 is still running -> P1 (unresolved) sorts after the resolved P2.
    return 1;
}

// X360 @ 0x8232F6C0. Build a ComparisonData row for every active-race-car slot from its CarData
// record, sort the rows into finishing order, then write each car's finishing position (0 == DNF,
// otherwise a descending rank countdown) back into the score data and into the base player-position
// table.
//
// The per-car record is ScoringSystem::GetCarData(slot) -> CarData* (X360 sub_8231DCD0). The X360
// reads the runner/finish times, takedowns, checkpoints and flags off the CarData's embedded
// CarScoreData (offset 0, via GetScoreData), and the race-car index / network player id off the
// CarData itself (fields @ +0x144 / +0x148). When GetCarData returns NULL the slot is empty: score
// fields fall back to a cleared CarScoreData, the race-car index to the empty sentinel
// (E_ACTIVE_RACE_CAR_INDEX_COUNT) and the id to -1 so the row sorts last and is skipped on assign.
void
OnlineBurningHomeRunModeScoring::UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumActiveCars)
{
    GameStateModuleIO::CarScoreData lEmptyScoreData;
    CGS_ASSERT(lpScoringSystem, "lpScoringSystem");
    lEmptyScoreData.ClearData();

    ComparisonData laComparisonData[E_ACTIVE_RACE_CAR_INDEX_COUNT];

    // Populate one comparison row per active-race-car index from the per-car record.
    for (s32 liIndex = 0; liIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(liIndex);

        CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        const GameStateModuleIO::CarScoreData* lpScore =
            (lpCarData != NULL) ? lpCarData->GetScoreData() : &lEmptyScoreData;

        ComparisonData& lRow = laComparisonData[liIndex];
        lRow.meRaceCarIndex                 = (lpCarData != NULL) ? lpCarData->GetActiveRaceCarIndex()
                                                                  : E_ACTIVE_RACE_CAR_INDEX_COUNT;
        lRow.mNetworkPlayerID               = (lpCarData != NULL) ? lpCarData->GetNetworkPlayerID() : -1;
        lRow.mFinishTime                    = lpScore->GetFinishTime();
        lRow.mTimeAsRunner                  = lpScore->GetTimeAsRunner();
        lRow.miTakedowns                    = lpScore->GetTakedowns();
        lRow.miNumberOfCumlativeCheckpoints = lpScore->GetCumulativeCheckpoints();
        lRow.mbCompletedBurningHomeRun      = lpScore->GetCompletedBurningHomeRun();
        lRow.mbTimedOut                     = lpScore->GetTimedOut();
        lRow.mbDisconnected                 = lpScore->GetDisconnected();

        // BurnoutConstants.h:39 bound check carried over from the X360 build (post-increment guard:
        // leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT).
        CGS_ASSERT(liIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    qsort(laComparisonData, E_ACTIVE_RACE_CAR_INDEX_COUNT, sizeof(ComparisonData),
          _BurningHomeRunPlayerFinishTimesCompare);

    // Walk the sorted rows assigning finishing positions. Cars that did not finish (mbDisconnected
    // here, the X360 *(v15) flag) get position 0; finishers count DOWN from liNumActiveCars. Both
    // writeback slots are the same physical CarScoreData fields the Race scorer writes: the
    // descending points/finish-position value (+0x58, SetOnlineRacePoints) then the sorted rank
    // (+0x5C, SetOnlineFinishPosition).
    s32 liPositionCountdown = liNumActiveCars;
    for (s32 liRank = 0; liRank < liNumActiveCars; ++liRank)
    {
        const ComparisonData& lRow = laComparisonData[liRank];
        const EActiveRaceCarIndex leRaceCarIndex = lRow.meRaceCarIndex;
        if (leRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_COUNT)   // skip empty/sentinel slot (== 8)
        {
            CarData* lpCarData = lpScoringSystem->GetCarData(leRaceCarIndex);
            CGS_ASSERT(lpCarData, "lpCarData");

            if (lpCarData != NULL)
            {
                GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
                if (lRow.mbDisconnected)
                {
                    lpScore->SetOnlineRacePoints(0);
                }
                else
                {
                    lpScore->SetOnlineRacePoints(liPositionCountdown);
                    --liPositionCountdown;
                }
                lpScore->SetOnlineFinishPosition(liRank);

                SetPlayerPosition(leRaceCarIndex, liRank);
            }
        }
    }
}
}
