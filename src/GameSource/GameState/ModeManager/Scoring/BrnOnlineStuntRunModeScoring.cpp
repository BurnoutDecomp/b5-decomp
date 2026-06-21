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

// X360 @ 0x82321D20. Sums the per-car online stunt scores of every active race car assigned to
// liTeam. Walks every active-race-car slot through ScoringSystem::GetCarData and, for the cars whose
// team matches, accumulates the embedded CarScoreData's online stunt score (X360 `*(CarData + 212)`
// == CarScoreData +0xD4, GetOnlineStuntScore). Empty slots (GetCarData == NULL) are skipped. The
// ScoringSystem keystone's WriteDataToOutput, GetWinnerTeam and UpdatePlayerPoints call this.
s32 OnlineStuntRunModeScoring::GetTeamScore(s32 liTeam, const ScoringSystem* lpScoringSystem) const
{
    s32 liTeamScore = 0;

    for (s32 leEnumIndex = 0; leEnumIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++leEnumIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(leEnumIndex);
        const CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        if (lpCarData != NULL && static_cast<s32>(lpCarData->GetTeam()) == liTeam)
        {
            liTeamScore += lpCarData->GetScoreData()->GetOnlineStuntScore();
        }

        CGS_ASSERT(leEnumIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    return liTeamScore;
}

// X360 @ 0x82338B90. End-of-event scoring for online stunt-run. Three passes:
//   (1) GATHER -- build one PlayerSortData row per active race car: its slot index, its online stunt
//       score (CarScoreData +0xD4), whether its team is the winning team (GetWinnerTeam == GetTeam),
//       and its disconnected flag (CarScoreData +0x69).
//   (2) TEAM TALLY -- for every contesting team (GetNumPlayersOnTeam > 0) build a TeamSortData row of
//       {team stunt-score, team index}; sort the car rows by Compare (best first) and the team rows by
//       CompareTeams (highest score first); then derive a per-team standings rank: the highest-scoring
//       team gets rank == liNumberOfContestingTeams, counting down.
//   (3) WRITEBACK -- walk the sorted car rows; each car gets its team's standings rank as online points
//       (0 if disconnected) at CarScoreData +0x58, its finishing position (sorted order) at +0x5C, and
//       its finishing position recorded into the base player-position table.
// The two team-range bounds asserts (E_PLAYER_TEAM_COUNT == 9) and the active-race-car bounds asserts
// (E_ACTIVE_RACE_CAR_INDEX_COUNT == 8) fire as the X360 build had them (never early-return).
void OnlineStuntRunModeScoring::UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumberOfCars)
{
    const s32 liWinningTeam = GetWinnerTeam(lpScoringSystem);

    // ---- (1) gather one sortable row per active race car ----
    PlayerSortData laPlayerData[E_ACTIVE_RACE_CAR_INDEX_COUNT];

    for (s32 liIndex = 0; liIndex < liNumberOfCars; ++liIndex)
    {
        const EActiveRaceCarIndex leSlot = static_cast<EActiveRaceCarIndex>(liIndex);
        CarData* lpCarData = lpScoringSystem->GetCarData(leSlot);
        const GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();

        PlayerSortData& lRow = laPlayerData[liIndex];
        lRow.miRaceCarIndex  = liIndex;
        lRow.mbOnWinningTeam = (static_cast<s32>(lpCarData->GetTeam()) == liWinningTeam);
        lRow.miScore         = lpScore->GetOnlineStuntScore();
        lRow.mbDisconnected  = lpScore->GetDisconnected();

        CGS_ASSERT(liIndex + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }

    // ---- (2) team tally: one row per contesting team ----
    TeamSortData laTeamData[KI_MAX_PLAYER_TEAMS];
    s32 liNumContestingTeams = 0;

    for (s32 leTeam = 0; leTeam < KI_MAX_PLAYER_TEAMS; ++leTeam)
    {
        if (GetNumPlayersOnTeam(leTeam, lpScoringSystem) > 0)
        {
            laTeamData[liNumContestingTeams].miTeamScore = GetTeamScore(leTeam, lpScoringSystem);
            laTeamData[liNumContestingTeams].miTeam      = leTeam;
            ++liNumContestingTeams;
        }

        CGS_ASSERT(leTeam + 1 <= KI_MAX_PLAYER_TEAMS, "leEnumIndex <= E_PLAYER_TEAM_COUNT");
    }

    qsort(laPlayerData, liNumberOfCars, sizeof(PlayerSortData), Compare);
    qsort(laTeamData, liNumContestingTeams, sizeof(TeamSortData), CompareTeams);

    // ---- derive a standings rank per team (highest-scoring team ranks highest) ----
    s32 laTeamStandings[KI_MAX_PLAYER_TEAMS];
    for (s32 li = 0; li < KI_MAX_PLAYER_TEAMS; ++li)
    {
        laTeamStandings[li] = 0;
    }

    s32 liRankValue = liNumContestingTeams;
    for (s32 liTeamRank = 0; liTeamRank < liNumContestingTeams; ++liTeamRank)
    {
        const s32 liTeam = laTeamData[liTeamRank].miTeam;
        CGS_ASSERT(liTeam >= 0, "lePlayerTeam >= GsmIO::E_PLAYER_TEAM_START");
        CGS_ASSERT(liTeam < KI_MAX_PLAYER_TEAMS, "lePlayerTeam < GsmIO::E_PLAYER_TEAM_COUNT");
        laTeamStandings[liTeam] = liRankValue;
        --liRankValue;
    }

    // ---- (3) writeback: online points (team standings rank) + finishing position ----
    for (s32 liPosition = 0; liPosition < liNumberOfCars; ++liPosition)
    {
        const PlayerSortData& lRow = laPlayerData[liPosition];
        CarData* lpCarData = lpScoringSystem->GetCarData(static_cast<EActiveRaceCarIndex>(lRow.miRaceCarIndex));
        CGS_ASSERT(lpCarData, "lpCarData");
        GameStateModuleIO::CarScoreData* lpScore = lpCarData->GetScoreData();
        CGS_ASSERT(lpScore, "lpCarScoreData");

        if (lRow.mbDisconnected)
        {
            lpScore->SetOnlineRacePoints(0);
        }
        else
        {
            const s32 liTeam = static_cast<s32>(lpCarData->GetTeam());
            CGS_ASSERT(liTeam >= 0, "lpCarData->GetTeam() >= GsmIO::E_PLAYER_TEAM_START");
            CGS_ASSERT(liTeam < KI_MAX_PLAYER_TEAMS, "lpCarData->GetTeam() < GsmIO::E_PLAYER_TEAM_COUNT");
            lpScore->SetOnlineRacePoints(laTeamStandings[liTeam]);
        }

        SetPlayerPosition(lRow.miRaceCarIndex, liPosition);
        lpScore->SetOnlineFinishPosition(liPosition);

        CGS_ASSERT(liPosition + 1 <= E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT");
    }
}
}
