#include "GameSource/GameState/ModeManager/Scoring/BrnOnlineRaceModeScoring.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameStateTypes.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"

#include <stdlib.h> // qsort

namespace BrnGameState
{
// X360 @ 0x8232F948. Online-race Update is a thin forward to the base team-tracking pass
// (tail-call to BaseOnlineModeScoring::UpdatePlayerTeams with both arguments unchanged).
void OnlineRaceModeScoring::Update(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars)
{
    UpdatePlayerTeams(lpScoringSystem, liNumberOfCars);
}

// X360 @ 0x82315610. Resets the per-slot online-award table to INVALID and the per-slot team
// table to NONE. Both arrays are BASE (BaseOnlineModeScoring) members: maOnlineAwards @ this+0x04
// (8 dwords <- -1) and maePlayerTeams @ this+0x44 (8 dwords <- 0); asm confirms the two stw loops.
// The award-variables array (+0x24) and the player-positions array (+0x64) are deliberately NOT
// touched here. This override does NOT chain to BaseOnlineModeScoring::ClearData.
void OnlineRaceModeScoring::ClearData()
{
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        maOnlineAwards[li] = E_ONLINE_AWARD_INVALID;
    }
    for (s32 li = 0; li < KI_MAX_ACTIVE_RACE_CARS; ++li)
    {
        maePlayerTeams[li] = GameStateModuleIO::E_PLAYER_TEAM_NONE;
    }
}

// X360 @ 0x82314B58. qsort comparator establishing finishing order between two race-car rows.
// Ranking precedence (highest first): valid slot > finished-and-connected (earliest time) >
// finished-but-disconnected > timed-out (nearest the finish) > still racing (nearest the finish),
// with NetworkPlayerID as the final deterministic tie-break. Empty slots (mNetworkPlayerID == -1)
// always sort last. Returns <0 if lpPlayer1 ranks ahead, >0 if behind, 0 if equal.
//
// The base Compare* helpers take an in/out accumulator and only write it while it is still 0
// (first decisive comparison wins), so the CompareFinishTime-then-CompareNetworkPlayerID pairs
// below mean "order by finish time, break ties by player id".
int OnlineRaceModeScoring::_PlayerFinishTimesCompare(const void* lpPlayer1, const void* lpPlayer2)
{
    CGS_ASSERT(lpPlayer1, "lpPlayer1");
    CGS_ASSERT(lpPlayer2, "lpPlayer2");

    const IndexToFinishTuple* lpP1 = static_cast<const IndexToFinishTuple*>(lpPlayer1);
    const IndexToFinishTuple* lpP2 = static_cast<const IndexToFinishTuple*>(lpPlayer2);

    s32 liResult = 0;

    // "Has a finish time" is derived from the time value (GetFloatVal != 0), NOT from mbFinished.
    const bool lbP1HasFinishTime = (lpP1->mFinishTime.GetFloatVal() != 0.0f);
    const bool lbP2HasFinishTime = (lpP2->mFinishTime.GetFloatVal() != 0.0f);

    const bool lbP1StillRacing =
        !(lbP1HasFinishTime || lpP1->mbTimedOut || lpP1->mbDisconnected);
    const bool lbP2StillRacing =
        !(lbP2HasFinishTime || lpP2->mbTimedOut || lpP2->mbDisconnected);

    const bool lbP1FinishedButDisconnected = (lbP1HasFinishTime && lpP1->mbDisconnected);
    const bool lbP2FinishedButDisconnected = (lbP2HasFinishTime && lpP2->mbDisconnected);

    // Empty slots sort last.
    if (lpP1->mNetworkPlayerID == -1)
    {
        return 1;
    }
    if (lpP2->mNetworkPlayerID == -1)
    {
        return -1;
    }

    if (lbP1FinishedButDisconnected || lbP2FinishedButDisconnected)
    {
        if (!lbP1HasFinishTime || !lbP2HasFinishTime)
        {
            // Exactly one has a finish time: the one that finished ranks ahead.
            return lbP1HasFinishTime ? -1 : 1;
        }
        // Both finished (at least one was a disconnect): order by finish time, then id.
        CompareFinishTime(&lpP1->mFinishTime, &lpP2->mFinishTime, &liResult);
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }

    if (lbP1StillRacing && lbP2StillRacing)
    {
        // Both still racing: nearer the finish ranks ahead, then id.
        CompareDistanceToFinish(lpP1->mfDistanceToFinish, lpP2->mfDistanceToFinish, &liResult);
        CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
        return liResult;
    }
    if (lbP2StillRacing)
    {
        // P2 still racing, P1 settled -> P1 ranks ahead.
        return -1;
    }
    if (!lbP1StillRacing)
    {
        if (lpP1->mbDisconnected && lpP2->mbDisconnected)
        {
            // Both disconnected (neither finished): order by id.
            CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
            return liResult;
        }
        if (lpP2->mbDisconnected)
        {
            return -1;
        }
        if (!lpP1->mbDisconnected)
        {
            if (lpP1->mbTimedOut && lpP2->mbTimedOut)
            {
                // Both timed out: nearer the finish ranks ahead, then id.
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
            // Neither timed out nor disconnected -> both finished cleanly:
            // order by finish time, then id.
            CompareFinishTime(&lpP1->mFinishTime, &lpP2->mFinishTime, &liResult);
            CompareNetworkPlayerID(lpP1->mNetworkPlayerID, lpP2->mNetworkPlayerID, &liResult);
            return liResult;
        }
    }

    // Fall-through (P1 settled / disconnected against an out-ranked P2).
    return 1;
}

// X360 @ 0x8232EB70. Builds one IndexToFinishTuple per active-race-car slot from the scoring
// system's per-car record, sorts them into finishing order with _PlayerFinishTimesCompare, then
// walks the sorted list assigning each car its finish position and its online points (places
// score liNumberOfCars, liNumberOfCars-1, ... ; a disconnected car scores 0).
//
// The per-car record is ScoringSystem::GetCarData(slot) -> CarData* (X360 sub_8231DCD0; const twin
// 0x8231DCD0, non-const 0x8231DC18 -- identical &maCarData[slot] computation). The X360 reads the
// finish time / distance / flags off the CarData's embedded CarScoreData (offset 0, via
// GetScoreData) and the network player id off the CarData itself (field @ +0x148). The race-car
// index for the row comes from the slot loop counter (X360 asm: stw leEnumIndex). When GetCarData
// returns NULL the slot is empty: score fields fall back to a cleared CarScoreData and the id to
// the -1 empty sentinel so the row sorts last.
void OnlineRaceModeScoring::UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumberOfCars)
{
    GameStateModuleIO::CarScoreData lDefaultScoreData;

    CGS_ASSERT(lpScoringSystem, "lpScoringSystem");

    lDefaultScoreData.ClearData();

    IndexToFinishTuple laFinishTuples[E_ACTIVE_RACE_CAR_INDEX_COUNT];

    // ---- gather phase: one row per active-race-car slot ------------------------------------
    for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(leEnumIndex);

        CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        const GameStateModuleIO::CarScoreData* lpScore =
            (lpCarData != NULL) ? lpCarData->GetScoreData() : &lDefaultScoreData;

        IndexToFinishTuple& lTuple = laFinishTuples[leEnumIndex];
        lTuple.meRaceCarIndex     = leSlot;
        lTuple.mNetworkPlayerID   = (lpCarData != NULL) ? lpCarData->GetNetworkPlayerID() : -1;
        lTuple.mFinishTime        = lpScore->GetFinishTime();
        lTuple.mfDistanceToFinish = lpScore->GetDistanceToFinish();
        lTuple.mbTimedOut         = lpScore->GetTimedOut();
        lTuple.mbDisconnected     = lpScore->GetDisconnected();
        // mbFinished is left at its gather default; the sort derives "finished" from mFinishTime.

        CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    // ---- sort into finishing order ---------------------------------------------------------
    qsort(laFinishTuples, E_ACTIVE_RACE_CAR_INDEX_COUNT, sizeof(IndexToFinishTuple),
          OnlineRaceModeScoring::_PlayerFinishTimesCompare);

    // ---- assign positions + points ---------------------------------------------------------
    s32 liPointsToAward = liNumberOfCars;
    for (s32 liPosition = 0; liPosition < liNumberOfCars; ++liPosition)
    {
        const IndexToFinishTuple& lTuple = laFinishTuples[liPosition];
        const EActiveRaceCarIndex leSlot = lTuple.meRaceCarIndex;
        if (leSlot == E_ACTIVE_RACE_CAR_INDEX_COUNT)
        {
            continue; // empty slot sentinel (sorted to the tail)
        }

        CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        if (lpCarData != NULL)
        {
            GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
            if (lTuple.mbDisconnected)
            {
                lpScore->SetOnlineRacePoints(0);
            }
            else
            {
                lpScore->SetOnlineRacePoints(liPointsToAward);
                --liPointsToAward;
            }
            lpScore->SetOnlineFinishPosition(liPosition);
            SetPlayerPosition(leSlot, liPosition);
        }
    }
}
}
