#include "GameSource/GameState/ModeManager/Scoring/BrnOnlineStuntRunModeScoring.h"

#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <stdlib.h> // qsort

namespace BrnGameState
{
// X360 @ 0x823159C8. qsort comparator establishing the per-car stunt ranking between two rows.
// Precedence (best first): a connected car ranks ahead of a disconnected one; among cars of equal
// connection status the higher stunt score ranks ahead. The X360 body tests p2's disconnected flag
// first (p1 then ranks ahead), then p1's, then compares the scores (p1.score <= p2.score sorts p1
// behind). Returns -1 if lpPlayer1 ranks ahead, +1 otherwise.
int OnlineStuntRunModeScoring::Compare(const void* lpPlayer1, const void* lpPlayer2)
{
    const PlayerSortData* lpP1 = static_cast<const PlayerSortData*>(lpPlayer1);
    const PlayerSortData* lpP2 = static_cast<const PlayerSortData*>(lpPlayer2);

    if (lpP2->mbDisconnected)
    {
        return -1; // a disconnected p2 always ranks behind p1
    }
    if (lpP1->mbDisconnected)
    {
        return 1; // a disconnected p1 always ranks behind p2
    }
    // Both connected: higher score ranks ahead (descending sort).
    if (lpP1->miScore <= lpP2->miScore)
    {
        return 1;
    }
    return -1;
}

// X360 @ 0x82315A08. qsort comparator establishing the team ranking between two team rows: the
// higher team stunt-score ranks ahead (descending sort). Returns -1 if lpTeam1 ranks ahead, +1 otherwise.
int OnlineStuntRunModeScoring::CompareTeams(const void* lpTeam1, const void* lpTeam2)
{
    const TeamSortData* lpT1 = static_cast<const TeamSortData*>(lpTeam1);
    const TeamSortData* lpT2 = static_cast<const TeamSortData*>(lpTeam2);

    if (lpT1->miTeamScore <= lpT2->miTeamScore)
    {
        return 1;
    }
    return -1;
}

// X360 @ 0x82321DB0. Counts the active race cars currently assigned to liTeam. Walks every
// active-race-car slot through ScoringSystem::GetCarData and tallies those whose team matches; empty
// slots (GetCarData == NULL) are skipped. UpdatePlayerPoints uses this to decide which teams are
// contesting (count > 0).
s32 OnlineStuntRunModeScoring::GetNumPlayersOnTeam(s32 liTeam, const ScoringSystem* lpScoringSystem) const
{
    s32 liNumPlayers = 0;

    for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(leEnumIndex);
        const CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        if (lpCarData != NULL && static_cast<s32>(lpCarData->GetTeam()) == liTeam)
        {
            ++liNumPlayers;
        }

        CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    return liNumPlayers;
}

// X360 @ 0x8232F950 (Hex-Rays truncated the name to "GetWinnerTea"). Scans the team stunt-scores and
// returns the leading team -- the first team that reaches the running maximum score. With no scoring
// team the default leader is team 0. Called by UpdatePlayerPoints (to flag the winning team on each
// car row) and by the mode's finish logic (ShouldFinish / ModeManager::FinishCurrentMode).
s32 OnlineStuntRunModeScoring::GetWinnerTeam(const ScoringSystem* lpScoringSystem) const
{
    s32 liWinningTeam = 0;
    s32 liBestScore   = 0;

    for (s32 leEnumIndex = 0; leEnumIndex < KI_MAX_PLAYER_TEAMS; ++leEnumIndex)
    {
        const s32 liTeamScore = GetTeamScore(leEnumIndex, lpScoringSystem);
        if (liTeamScore > liBestScore)
        {
            liWinningTeam = leEnumIndex;
            liBestScore   = liTeamScore;
        }

        CGS_ASSERT(leEnumIndex + 1 <= KI_MAX_PLAYER_TEAMS,
                   "leEnumIndex <= E_PLAYER_TEAM_COUNT");
    }

    return liWinningTeam;
}

// ----------------------------------------------------------------------------------------------------
// DECLARE-ONLY (BLOCKED) -- intentionally NO body here:
//   * GetTeamScore           (X360 0x82321D20)
//   * UpdatePlayerPoints     (X360 0x82338B90)
// Both read the per-car online stunt score at CarScoreData +0xD4 (X360 `*(CarData + 212)`), which has
// no committed named accessor -- it is the un-named `maStorageD4[4]` storage in the CarScoreData home
// (BrnGameStateSharedIO.h). Reconstructing their bodies semantically requires a
// CarScoreData::GetOnlineStuntScore() (+0xD4) accessor; adding it would GROW that OTHER subsystem's
// home, which is out of this work item's scope. They land once CarScoreData exposes that field.
// (The remaining lifecycle virtuals Construct/Prepare/Release/Destruct/ClearData/Update/
// WriteDataToOutput are likewise declared-only -- their bodies belong to the wider class TU.)
// ----------------------------------------------------------------------------------------------------
}
