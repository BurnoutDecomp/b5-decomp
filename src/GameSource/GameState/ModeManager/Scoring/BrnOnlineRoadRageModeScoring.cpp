#include "GameSource/GameState/ModeManager/Scoring/BrnOnlineRoadRageModeScoring.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameStateTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

#include <stdlib.h> // qsort
#include <cstring>  // std::memset (ClearData fills the blue-team finish-type array)

namespace BrnGameState
{
// X360 @ 0x82314E48. Zero the per-slot red-team elimination tally. The base members + the other
// derived members are reset by ClearData; Construct only touches maiEliminationCount[8].
void OnlineRoadRageModeScoring::Construct()
{
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        maiEliminationCount[li] = 0;
    }
}

// X360 @ 0x82314B28. Reset the per-slot team table to NONE and the per-slot finishing position to
// 8 (== one past last of an 8-car field). Both are BASE (BaseOnlineModeScoring) members:
// maePlayerTeams @ this+0x44 and maiPlayerPositions @ this+0x64 (written via SetPlayerPosition).
bool OnlineRoadRageModeScoring::Prepare()
{
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        maePlayerTeams[li] = GameStateModuleIO::E_PLAYER_TEAM_NONE;
        SetPlayerPosition(li, KI_MAX_ACTIVE_RACE_CARS);
    }
    return true;
}

// X360 @ 0x82314E68. Reset every per-slot scoring array to its cleared sentinel:
//   maOnlineAwards[8]          (base +0x04) <- E_ONLINE_AWARD_INVALID (-1)
//   maePlayerTeams[8]          (base +0x44) <- E_PLAYER_TEAM_NONE (0)
//   mbRedTeamWon                            <- false
//   maiEliminationCount[8]                  <- 0
//   maeBlueTeamFinishTypes[8]               <- 0x03030303 per slot (byte-fill with
//                                              E_BLUE_TEAM_FINISH_TYPE_COUNT == 3; X360 stw 0x03030303)
void OnlineRoadRageModeScoring::ClearData()
{
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        maOnlineAwards[li] = E_ONLINE_AWARD_INVALID;
    }
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        maePlayerTeams[li] = GameStateModuleIO::E_PLAYER_TEAM_NONE;
    }

    mbRedTeamWon = false;

    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        maiEliminationCount[li] = 0;
    }

    // X360 fills each s32 slot with 0x03030303 via a byte-wide fill of the COUNT sentinel.
    std::memset(maeBlueTeamFinishTypes, GameStateModuleIO::E_BLUE_TEAM_FINISH_TYPE_COUNT,
                sizeof(maeBlueTeamFinishTypes));
}

// X360 @ 0x82314EE0. Copy this scorer's per-slot arrays into the network output interface. The
// X360 code is five unrolled 8-word copy loops + one scalar store; reconstructed as the equivalent
// named array copies. Unlike the Burning-Home-Run override, the Road-Rage scorer also writes
// maiNumEliminations[] (from maiEliminationCount[]), mbRedTeamWon and maeBlueTeamFinishTypes[].
//
//   dst maiNumEliminations[8]   (+0x00) <- maiEliminationCount[8]   (this+0x88)
//   dst maOnlineAwards[8]       (+0x20) <- maOnlineAwards[8]        (this+0x04, base)
//   dst maiOnlineAwardVariables (+0x40) <- maiOnlineAwardVariables  (this+0x24, base)
//   dst maePlayerTeam[8]        (+0x60) <- maePlayerTeams[8]        (this+0x44, base)
//   dst maeBlueTeamFinishTypes  (+0x80) <- maeBlueTeamFinishTypes   (this+0xA8)
//   dst mbRedTeamWon            (+0xA0) <- mbRedTeamWon             (this+0x84)
void OnlineRoadRageModeScoring::WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface* lpOutput)
{
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        lpOutput->maOnlineAwards[li]          = maOnlineAwards[li];
    }
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        lpOutput->maiOnlineAwardVariables[li] = maiOnlineAwardVariables[li];
    }
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        lpOutput->maePlayerTeam[li]           = maePlayerTeams[li];
    }

    lpOutput->mbRedTeamWon = mbRedTeamWon;

    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        lpOutput->maiNumEliminations[li]      = maiEliminationCount[li];
    }
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        lpOutput->maeBlueTeamFinishTypes[li]  = maeBlueTeamFinishTypes[li];
    }
}

// X360 @ 0x823151A8. Seed both sort arrays with the empty/invalid sentinel rows so that, after the
// gather pass overwrites the rows for real cars, any untouched (empty) slots still sort to the back.
//   Red[i]:  mNetworkPlayerID = -1, miEliminations = 0
//   Blue[i]: mNetworkPlayerID = -1, mFinishTime = Time(0.0f), mfDistanceToFinish = 0.0f,
//            mbTimedOut = false
void OnlineRoadRageModeScoring::InitialisePlayerArrayData(RedTeamSortData* lpaRedRows,
                                                          BlueTeamSortData* lpaBlueRows)
{
    for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
    {
        BlueTeamSortData& lBlue = lpaBlueRows[leEnumIndex];
        lBlue.mfDistanceToFinish = 0.0f;
        lBlue.mNetworkPlayerID   = -1;
        lBlue.mFinishTime        = CgsSystem::Time(0.0f);
        lBlue.mbTimedOut         = false;

        RedTeamSortData& lRed = lpaRedRows[leEnumIndex];
        lRed.mNetworkPlayerID  = -1;
        lRed.miEliminations    = 0;

        CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }
}

// X360 @ 0x82321C38. For every car, read who eliminated it (CarScoreData::meEliminatorRaceCarIndex)
// and, when that is a valid race-car index (!= -1), increment maiEliminationCount[eliminator].
void OnlineRoadRageModeScoring::CalculateRedTeamEliminations(const ScoringSystem* lpScoringSystem)
{
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        maiEliminationCount[li] = 0;
    }

    for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(leEnumIndex);

        const CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        if (lpCarData != NULL)
        {
            const EActiveRaceCarIndex leEliminator = lpCarData->GetScoreData()->GetEliminatorRaceCarIndex();
            if (leEliminator != E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                CGS_ASSERT((leEliminator > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                               (leEliminator < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                           "( lpCarData->GetScoreData()->meEliminatorRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )"
                           " && ( lpCarData->GetScoreData()->meEliminatorRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");
                ++maiEliminationCount[leEliminator];
            }
        }

        CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }
}

// X360 @ 0x82321B60. Classify the finish of each blue-team (runner) car listed in the sort rows
// into maeBlueTeamFinishTypes[raceCarIndex]:
//   * eliminator index != -1  -> the car was eliminated:
//       disconnected -> ESCAPED (the X360 +0x68/+0x69 reads: GetDisconnected selects ESCAPED==2,
//                       otherwise ELIMINATED==... ); see the asm: *(result+104) (mbTimedOut@+0x68)
//                       picks SURVIVED(1) vs ELIMINATED(2).
//   * eliminator index == -1  -> the runner was never eliminated -> ELIMINATED(0).
// NOTE: the X360 reads CarScoreData +0x60 (eliminator) for the "was eliminated" test, and +0x68
// (mbTimedOut) to split the eliminated case; the literals stored are 1 then 2 (else 0), matching
// the EBlueTeamFinishType enumerators SURVIVED(1)/ESCAPED(2)/ELIMINATED(0).
void OnlineRoadRageModeScoring::CalculateBlueTeamFinishTypes(const ScoringSystem* lpScoringSystem,
                                                             BlueTeamSortData* lpaBlueRows,
                                                             s32 liNumBlue)
{
    for (s32 li = 0; li < liNumBlue; ++li)
    {
        const EActiveRaceCarIndex leRaceCarIndex = lpaBlueRows[li].meRaceCarIndex;

        CGS_ASSERT((leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                       (leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "( leRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )"
                   " && ( leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

        const CarData* lpCarData = lpScoringSystem->GetCarData(leRaceCarIndex);
        if (lpCarData != NULL)
        {
            const GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
            if (lpScore->GetEliminatorRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                maeBlueTeamFinishTypes[leRaceCarIndex] = GameStateModuleIO::E_BLUE_TEAM_FINISH_TYPE_ELIMINATED;
            }
            else if (lpScore->GetTimedOut())
            {
                maeBlueTeamFinishTypes[leRaceCarIndex] = GameStateModuleIO::E_BLUE_TEAM_FINISH_TYPE_SURVIVED;
            }
            else
            {
                maeBlueTeamFinishTypes[leRaceCarIndex] = GameStateModuleIO::E_BLUE_TEAM_FINISH_TYPE_ESCAPED;
            }
        }
    }
}

// X360 @ 0x82321AA0. The red (chaser) team wins if EITHER the blue team is fully eliminated OR
// every blue-team (runner) car has timed out. The X360 walks all cars: GetTeam() == 2 (blue) and
// !GetTimedOut() (CarScoreData +0x68) means a runner is still in the game, which clears the
// "all blue timed out" flag.
bool OnlineRoadRageModeScoring::CheckForRedTeamWin(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars)
{
    (void)liNumberOfCars;

    bool lbAllBlueTimedOut = true;

    for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(leEnumIndex);

        const CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        if (lpCarData != NULL)
        {
            if (lpCarData->GetTeam() == GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM &&
                !lpCarData->GetScoreData()->GetTimedOut())
            {
                lbAllBlueTimedOut = false;
                break;
            }
        }

        CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    return lpScoringSystem->IsBlueTeamEliminated() || lbAllBlueTimedOut;
}

// X360 @ 0x82314FA0. qsort comparator that ranks two results-table rows. Empty slots (id == -1)
// sort last. Otherwise the precedence (mirroring the X360 control flow) ranks a SETTLED car
// (finished / timed-out / disconnected) ahead of a still-active one; within the settled tier it
// orders by the assigned standings position (+0x58 / miOnlineFinishPositionScore) then timed-out flag,
// with the row pointer ordering as the final deterministic tie-break.
int OnlineRoadRageModeScoring::_CompareForTablePosition(const void* lpPlayer1, const void* lpPlayer2)
{
    CGS_ASSERT(lpPlayer1, "lpPlayer1");
    CGS_ASSERT(lpPlayer2, "lpPlayer2");

    const TablePositionData* lpP1 = static_cast<const TablePositionData*>(lpPlayer1);
    const TablePositionData* lpP2 = static_cast<const TablePositionData*>(lpPlayer2);

    const GameStateModuleIO::CarScoreData* lpScore1 = lpP1->mpScoreData;
    const GameStateModuleIO::CarScoreData* lpScore2 = lpP2->mpScoreData;

    // "Still active" == has no finish time AND has not timed out AND has not disconnected.
    const bool lbP1Active =
        !((lpScore1->GetFinishTime().GetFloatVal() != 0.0f) || lpScore1->GetTimedOut() ||
          lpScore1->GetDisconnected());
    const bool lbP2Active =
        !((lpScore2->GetFinishTime().GetFloatVal() != 0.0f) || lpScore2->GetTimedOut() ||
          lpScore2->GetDisconnected());

    // Empty slots sort last.
    if (lpP1->mNetworkPlayerID == -1)
    {
        return 1;
    }
    if (lpP2->mNetworkPlayerID == -1)
    {
        return -1;
    }

    if (lbP1Active)
    {
        if (lbP2Active)
        {
            // Both still active: order by row pointer (deterministic tie-break).
            if (lpP1 <= lpP2)
            {
                return (lpP1 >= lpP2) ? 0 : -1;
            }
        }
        // P1 active, P2 settled -> P2 ranks ahead.
        return 1;
    }

    if (lbP2Active)
    {
        // P1 settled, P2 active -> P1 ranks ahead.
        return -1;
    }

    // Both settled. Disconnected cars sort behind connected ones; equal-tier falls to standings.
    if (lpScore1->GetDisconnected() && lpScore2->GetDisconnected())
    {
        // both disconnected (same tier): fall to the shared tie-break tail
    }
    else if (lpScore2->GetDisconnected())
    {
        return -1;
    }
    else if (lpScore1->GetDisconnected())
    {
        return 1;
    }
    else
    {
        // Neither disconnected: order by the assigned standings position (+0x58).
        const s32 liPos1 = lpScore1->GetOnlineFinishPositionScore();
        const s32 liPos2 = lpScore2->GetOnlineFinishPositionScore();
        if (liPos1 > liPos2)
        {
            return -1;
        }
        if (liPos1 < liPos2)
        {
            return 1;
        }
        // Equal positions: timed-out cars sort behind (asm reads +0x68 mbTimedOut, NOT
        // +0xD9 mbEliminated), else fall to the shared tie-break tail.
        if (lpScore1->GetTimedOut() && lpScore2->GetTimedOut())
        {
            // both timed-out (same tier): fall to the shared tie-break tail
        }
        else if (lpScore2->GetTimedOut())
        {
            return -1;
        }
        else if (lpScore1->GetTimedOut())
        {
            return 1;
        }
    }

    // Shared tie-break tail: order by row pointer (deterministic).
    if (lpP1 >= lpP2)
    {
        return (lpP2 < lpP1) ? 1 : 0;
    }
    return -1;
}

// X360 @ 0x82315258. qsort comparator for the RED-team rows. A disconnected car sorts behind a
// connected one; otherwise the car with MORE eliminations ranks first, ties broken by network id.
int OnlineRoadRageModeScoring::_RedTeamPlayerCompare(const void* lpPlayer1, const void* lpPlayer2)
{
    s32 liResult = 0;

    CGS_ASSERT(lpPlayer1, "lpPlayer1");
    CGS_ASSERT(lpPlayer2, "lpPlayer2");

    const RedTeamSortData* lpP1 = static_cast<const RedTeamSortData*>(lpPlayer1);
    const RedTeamSortData* lpP2 = static_cast<const RedTeamSortData*>(lpPlayer2);

    if (lpP1->mbDisconnected && lpP2->mbDisconnected)
    {
        // both disconnected: tie-break by id
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }
    if (lpP2->mbDisconnected)
    {
        return -1;                       // P1 connected outranks a disconnected P2
    }
    if (lpP1->mbDisconnected)
    {
        return 1;                        // P1 disconnected ranks behind P2
    }

    // Both connected: more eliminations ranks first.
    if (lpP1->miEliminations > lpP2->miEliminations)
    {
        return -1;
    }
    if (lpP1->miEliminations < lpP2->miEliminations)
    {
        return 1;
    }

    // Equal eliminations: tie-break by id.
    CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
    return liResult;
}

// X360 @ 0x82315338. qsort comparator for the BLUE-team (runner) rows. Tiers (per the asm):
// still-running (finishTime==0 && !timedOut && !disconnected) ranks BEHIND any settled car
// (settled-vs-running -> -1/+1, both-running -> distance+id); then among settled cars,
// disconnected pairs tie-break by distance+id (disconnected outranked by connected);
// then among connected-settled cars, "finished" (finishTime>0 && !timedOut) ranks ahead
// (both finished -> finish-time+id); then NEITHER-finished orders by takedowns (more first),
// then eliminated tier (distance+id), then timed-out tier (distance+id), then finish-time+id.
int OnlineRoadRageModeScoring::_BlueTeamPlayerCompare(const void* lpPlayer1, const void* lpPlayer2)
{
    s32 liResult = 0;

    CGS_ASSERT(lpPlayer1, "lpPlayer1");
    CGS_ASSERT(lpPlayer2, "lpPlayer2");

    const BlueTeamSortData* lpP1 = static_cast<const BlueTeamSortData*>(lpPlayer1);
    const BlueTeamSortData* lpP2 = static_cast<const BlueTeamSortData*>(lpPlayer2);

    // Top-level "still running": finish time exactly zero AND not timed-out AND not disconnected.
    const bool lbP1StillRunning =
        (lpP1->mFinishTime.GetFloatVal() == 0.0f) && !lpP1->mbTimedOut && !lpP1->mbDisconnected;
    const bool lbP2StillRunning =
        (lpP2->mFinishTime.GetFloatVal() == 0.0f) && !lpP2->mbTimedOut && !lpP2->mbDisconnected;

    if (lbP1StillRunning)
    {
        if (lbP2StillRunning)
        {
            // Both still running: nearer the finish ranks ahead, then id.
            CompareDistanceToFinish(lpP1->mfDistanceToFinish, lpP2->mfDistanceToFinish, &liResult);
            CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
            return liResult;
        }
        return 1;   // P1 running, P2 settled -> P2 ranks ahead
    }
    if (lbP2StillRunning)
    {
        return -1;  // P1 settled ranks ahead of the still-running P2
    }

    // Both settled. Disconnected cars tie-break by DISTANCE then id (asm LABEL_15, NOT finish time).
    if (lpP1->mbDisconnected && lpP2->mbDisconnected)
    {
        CompareDistanceToFinish(lpP1->mfDistanceToFinish, lpP2->mfDistanceToFinish, &liResult);
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }
    if (lpP2->mbDisconnected)
    {
        return -1;
    }
    if (lpP1->mbDisconnected)
    {
        return 1;
    }

    // Both connected & settled. Here "finished" is a SECOND predicate: finish time strictly > 0
    // AND not timed-out (distinct from the top-level still-running test, asm 0x823154D0/0x823154F0).
    const bool lbP1Finished = (lpP1->mFinishTime.GetFloatVal() > 0.0f) && !lpP1->mbTimedOut;
    const bool lbP2Finished = (lpP2->mFinishTime.GetFloatVal() > 0.0f) && !lpP2->mbTimedOut;
    if (lbP1Finished)
    {
        if (!lbP2Finished)
        {
            return -1;
        }
        // both finished: earlier finish time ranks ahead, then id.
        CompareFinishTime(&lpP1->mFinishTime, &lpP2->mFinishTime, &liResult);
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }
    if (lbP2Finished)
    {
        return 1;
    }

    // Neither finished: MORE takedowns ranks first (asm +0x14 cmpw at 0x8231557C).
    if (lpP1->miTakedowns > lpP2->miTakedowns)
    {
        return -1;
    }
    if (lpP1->miTakedowns < lpP2->miTakedowns)
    {
        return 1;
    }

    // Equal takedowns: eliminated tier (distance+id), then timed-out tier (distance+id), then finish-time+id.
    if (lpP1->mbEliminated && lpP2->mbEliminated)
    {
        CompareDistanceToFinish(lpP1->mfDistanceToFinish, lpP2->mfDistanceToFinish, &liResult);
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }
    if (lpP2->mbEliminated)
    {
        return -1;
    }
    if (lpP1->mbEliminated)
    {
        return 1;
    }
    if (lpP1->mbTimedOut && lpP2->mbTimedOut)
    {
        CompareDistanceToFinish(lpP1->mfDistanceToFinish, lpP2->mfDistanceToFinish, &liResult);
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }
    if (lpP2->mbTimedOut)
    {
        return -1;
    }
    if (lpP1->mbTimedOut)
    {
        return 1;
    }
    // Final fallback: finish time then id.
    CompareFinishTime(&lpP1->mFinishTime, &lpP2->mFinishTime, &liResult);
    CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
    return liResult;
}

// X360 @ 0x8232ED70. Build one results-table row per car (falling back to a cleared CarScoreData
// for empty slots), sort the rows with _CompareForTablePosition, then walk the sorted list writing
// each car's final standings position into its CarScoreData (+0x5C) and into the base player-position
// table.
void OnlineRoadRageModeScoring::CalculatePositionsInResultsTable(ScoringSystem* lpScoringSystem,
                                                                 s32 liNumberOfCars)
{
    GameStateModuleIO::CarScoreData lDefaultScoreData;
    lDefaultScoreData.ClearData();

    TablePositionData laRows[E_ACTIVE_RACE_CAR_INDEX_COUNT];

    for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(leEnumIndex);

        // The const GetCarData twin (X360 sub_8231DCD0); fall back to the cleared default for empties.
        const CarData* lpConstCarData = const_cast<const ScoringSystem*>(lpScoringSystem)->GetCarData(leSlot);
        GameStateModuleIO::CarScoreData* lpScoreData =
            (lpConstCarData != NULL) ? const_cast<CarData*>(lpConstCarData)->GetScoreData()
                                     : &lDefaultScoreData;

        TablePositionData& lRow = laRows[leEnumIndex];
        lRow.meRaceCarIndex   = leSlot;
        lRow.mNetworkPlayerID = (lpConstCarData != NULL) ? lpConstCarData->GetNetworkPlayerID() : -1;
        lRow.mbRedTeamWon     = mbRedTeamWon;
        lRow.mpScoreData      = lpScoreData;

        CGS_ASSERT(leSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && "
                   "(leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        // The X360 records the player's CURRENT team on the row here (GetCarData non-const twin).
        CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        lRow.mePlayerTeam = (lpCarData != NULL) ? lpCarData->GetTeam()
                                                : GameStateModuleIO::E_PLAYER_TEAM_NONE;

        CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    qsort(laRows, E_ACTIVE_RACE_CAR_INDEX_COUNT, sizeof(TablePositionData),
          OnlineRoadRageModeScoring::_CompareForTablePosition);

    for (s32 liPosition = 0; liPosition < liNumberOfCars; ++liPosition)
    {
        const TablePositionData& lRow = laRows[liPosition];
        // X360 writes liPosition into the standings slot (+0x5C) then reads it straight back to
        // feed SetPlayerPosition, so the value passed is liPosition itself.
        lRow.mpScoreData->SetOnlineFinishPosition(liPosition);
        SetPlayerPosition(lRow.meRaceCarIndex, liPosition);
    }
}

// X360 @ 0x8232EF00. Drives the full Road-Rage end-of-update scoring pass: refresh the base team
// table, decide whether the red team won, seed + gather the per-team sort rows, count eliminations,
// classify blue-team finishes, sort each team, assign per-team standings, then resolve the combined
// results table.
void OnlineRoadRageModeScoring::UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumberOfCars)
{
    CGS_ASSERT(lpScoringSystem, "lpScoringSystem");

    UpdatePlayerTeams(lpScoringSystem, liNumberOfCars);

    mbRedTeamWon = CheckForRedTeamWin(lpScoringSystem, liNumberOfCars);

    RedTeamSortData  laRedRows[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    BlueTeamSortData laBlueRows[E_ACTIVE_RACE_CAR_INDEX_COUNT];
    InitialisePlayerArrayData(laRedRows, laBlueRows);

    CalculateRedTeamEliminations(lpScoringSystem);

    // ---- gather phase: one row per real car into its team's row array ----
    s32 liNumRed  = 0;
    s32 liNumBlue = 0;
    for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(leEnumIndex);

        const CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        if (lpCarData != NULL)
        {
            const GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
            const GameStateModuleIO::EPlayerTeam   leTeam  = lpCarData->GetTeam();

            if (leTeam == GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM)
            {
                RedTeamSortData& lRed = laRedRows[liNumRed];
                ++liNumRed;
                lRed.meRaceCarIndex  = lpCarData->GetActiveRaceCarIndex();
                lRed.miEliminations  = maiEliminationCount[leEnumIndex];
                lRed.mbTimedOut      = false;
                lRed.mNetworkPlayerID = lpCarData->GetNetworkPlayerID();
                lRed.mbDisconnected  = lpScore->GetDisconnected();
            }
            else if (leTeam == GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
            {
                BlueTeamSortData& lBlue = laBlueRows[liNumBlue];
                ++liNumBlue;
                lBlue.meRaceCarIndex     = lpCarData->GetActiveRaceCarIndex();
                lBlue.mNetworkPlayerID   = lpCarData->GetNetworkPlayerID();
                lBlue.mbTimedOut         = lpScore->GetTimedOut();
                lBlue.mbEliminated       = (lpScore->GetEliminatorRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID);
                lBlue.mbDisconnected     = lpScore->GetDisconnected();
                lBlue.mfDistanceToFinish = lpScore->GetDistanceToFinishLive();
                lBlue.mFinishTime        = lpScore->GetFinishTime();
                lBlue.miTakedowns        = lpScore->GetTakedowns();
            }
            else
            {
                CGS_ASSERT(false, "Online Road Rage player with no team\n");
            }
        }

        CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    if (liNumBlue != 0 && liNumRed != 0)
    {
        CalculateBlueTeamFinishTypes(lpScoringSystem, laBlueRows, liNumBlue);

        CGS_ASSERT(liNumBlue > 0, "Blue team has members.");
        CGS_ASSERT(liNumBlue <= 7, "Blue team has members.");
        CGS_ASSERT(liNumRed > 0, "Red team has members.");
        CGS_ASSERT(liNumRed <= 7, "Red team has members.");

        qsort(laBlueRows, liNumBlue, sizeof(BlueTeamSortData),
              OnlineRoadRageModeScoring::_BlueTeamPlayerCompare);
        qsort(laRedRows, liNumRed, sizeof(RedTeamSortData),
              OnlineRoadRageModeScoring::_RedTeamPlayerCompare);

        // The winning team takes the top positions; the loser takes the tail. The red team wins ->
        // red occupies positions [a3-numRed+1 .. a3] counting DOWN from a3, blue counts down from
        // a3-numRed. Otherwise blue takes the top and red the tail.
        s32 liRedPosition;
        s32 liBluePosition;
        if (mbRedTeamWon)
        {
            liRedPosition  = liNumberOfCars;
            liBluePosition = liNumberOfCars - liNumRed;
        }
        else
        {
            liRedPosition  = liNumberOfCars - liNumBlue;
            liBluePosition = liNumberOfCars;
        }

        // ---- assign red-team standings ----
        for (s32 li = 0; li < liNumRed; ++li)
        {
            const EActiveRaceCarIndex leRaceCarIndex = laRedRows[li].meRaceCarIndex;
            CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            CarData* lpCarData = lpScoringSystem->GetCarData(leRaceCarIndex);
            CGS_ASSERT(lpCarData, "lpCarData");
            if (lpCarData != NULL)
            {
                GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
                lpScore->SetOnlineRacePoints(liRedPosition);

                // Cars on the same team that are tied (equal elimination tally) share the position
                // of the previous (better-ranked) car.
                if (li > 0 &&
                    maiEliminationCount[leRaceCarIndex] == maiEliminationCount[laRedRows[li - 1].meRaceCarIndex])
                {
                    CarData* lpPreviousCarData = lpScoringSystem->GetCarData(laRedRows[li - 1].meRaceCarIndex);
                    CGS_ASSERT(lpPreviousCarData, "lpPreviousPlayerCarData");
                    if (lpPreviousCarData != NULL)
                    {
                        lpScore->SetOnlineRacePoints(lpPreviousCarData->GetScoreData()->GetOnlineFinishPositionScore());
                    }
                }

                if (lpScore->GetDisconnected())
                {
                    lpScore->SetOnlineRacePoints(0);
                }
            }
            --liRedPosition;
        }

        // ---- assign blue-team standings ----
        for (s32 li = 0; li < liNumBlue; ++li)
        {
            const EActiveRaceCarIndex leRaceCarIndex = laBlueRows[li].meRaceCarIndex;
            CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

            CarData* lpCarData = lpScoringSystem->GetCarData(leRaceCarIndex);
            CGS_ASSERT(lpCarData, "lpCarData");
            if (lpCarData != NULL)
            {
                GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
                lpScore->SetOnlineRacePoints(liBluePosition);

                // Tied runners (both timed out at the same distance-to-finish) share the previous
                // car's position. (X360: current.mbTimedOut@+0x19 && previous.mbTimedOut@+0x19 &&
                // current.mfDistanceToFinish@+0x10 == previous.mfDistanceToFinish@+0x10.)
                if (li > 0 &&
                    laBlueRows[li].mbTimedOut && laBlueRows[li - 1].mbTimedOut &&
                    laBlueRows[li].mfDistanceToFinish == laBlueRows[li - 1].mfDistanceToFinish)
                {
                    CarData* lpPreviousCarData = lpScoringSystem->GetCarData(laBlueRows[li - 1].meRaceCarIndex);
                    CGS_ASSERT(lpPreviousCarData, "lpPreviousPlayerCarData");
                    if (lpPreviousCarData != NULL)
                    {
                        lpScore->SetOnlineRacePoints(lpPreviousCarData->GetScoreData()->GetOnlineFinishPositionScore());
                    }
                }

                if (lpScore->GetDisconnected())
                {
                    lpScore->SetOnlineRacePoints(0);
                }
            }
            --liBluePosition;
        }

        CalculatePositionsInResultsTable(lpScoringSystem, liNumberOfCars);

        // ---- post-pass validation: every real car must have a team, and any car NOT on the red
        //      team must have been given a blue-team finish classification. ----
        for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
        {
            const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(leEnumIndex);

            const CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
            if (lpCarData != NULL)
            {
                CGS_ASSERT(GetCurrentPlayerTeam(leSlot) != GameStateModuleIO::E_PLAYER_TEAM_NONE,
                           "GetCurrentPlayerTeam(leRaceCarIndex) != GsmIO::E_PLAYER_TEAM_NONE");
                CGS_ASSERT((GetCurrentPlayerTeam(leSlot) == GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM) ||
                               (maeBlueTeamFinishTypes[leSlot] != GameStateModuleIO::E_BLUE_TEAM_FINISH_TYPE_COUNT),
                           "(GsmIO::E_PLAYER_TEAM_RED_TEAM == GetCurrentPlayerTeam(leRaceCarIndex)) || "
                           "(GsmIO::E_BLUE_TEAM_FINISH_TYPE_COUNT != maeBlueTeamFinishTypes[leRaceCarIndex])");
            }

            CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }
    }
}
}
