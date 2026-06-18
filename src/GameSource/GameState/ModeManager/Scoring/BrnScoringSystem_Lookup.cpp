#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// BrnScoringSystem_Lookup.cpp
// ============================================================================
// Bodies for the "Lookup" group of BrnGameState::ScoringSystem (keystone header
// BrnScoringSystem.h lines 477-541): the network-round-data save, the per-car record
// lookups (GetCarData / GetCarDataFromPlayerScoringIndex / GetPlayerScoringIndex /
// IsPlayerInScoringSystem), the network-player roster counts, team iteration, the
// burnout-skillz tally, and the online finish-position query.
//
// Each body is reconstructed from its X360 pseudocode (addresses in the per-method
// comments) and accesses ScoringSystem members BY NAME. The X360 search loops walk the
// per-car records (maCarData[8]) comparing each record's stored race-car index or network
// player id; the optimizer's pointer-stride form (++ptr by 86 dwords == 344 bytes ==
// sizeof(CarData) on X360) is reconstructed back to a clean indexed loop.
//
// CgsNetwork::K_INVALID_PLAYER_ID has no committed home in the tree; the file-local -1
// (the BrnGameStateSharedIO.cpp / FlybyManager precedent) stands in for it in the asserts.
// ============================================================================

namespace BrnGameState
{
namespace
{
    // CgsNetwork::K_INVALID_PLAYER_ID stand-in (no committed home; -1 per existing precedent).
    const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

// ----------------------------------------------------------------------------
// per-car record lookup -- the keystone surface the cluster-1 scorers call.
// ----------------------------------------------------------------------------

// X360 0x8231DC18 (non-const). Linear search over maCarData[] for the slot whose stored
// active-race-car index matches leActiveRaceCarIndex; NULL if none. The X360 bound assert
// is the combined "(>= 0) && (< COUNT)" form on the unsigned argument.
CarData* ScoringSystem::GetCarData(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT((leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
               "(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetActiveRaceCarIndex() == leActiveRaceCarIndex)
        {
            return &maCarData[liSlot];
        }
    }
    return NULL;
}

// X360 0x8231DCD0 (const twin of the above).
const CarData* ScoringSystem::GetCarData(EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    CGS_ASSERT((leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
               "(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)");

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetActiveRaceCarIndex() == leActiveRaceCarIndex)
        {
            return &maCarData[liSlot];
        }
    }
    return NULL;
}

// X360 0x8231DD88 (non-const, by network id). Linear search for the slot whose stored network
// player id matches lID; NULL if none.
CarData* ScoringSystem::GetCarData(BrnNetwork::NetworkPlayerID lID)
{
    CGS_ASSERT(lID != K_INVALID_PLAYER_ID, "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetNetworkPlayerID() == lID)
        {
            return &maCarData[liSlot];
        }
    }
    return NULL;
}

// X360 0x8231DD88 (const twin, by network id).
const CarData* ScoringSystem::GetCarData(BrnNetwork::NetworkPlayerID lID) const
{
    CGS_ASSERT(lID != K_INVALID_PLAYER_ID, "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetNetworkPlayerID() == lID)
        {
            return &maCarData[liSlot];
        }
    }
    return NULL;
}

// X360 0x82310E30 (non-const). Direct index by player scoring slot -- no match search.
CarData* ScoringSystem::GetCarDataFromPlayerScoringIndex(GameStateModuleIO::EPlayerScoringIndex leIndex)
{
    CGS_ASSERT(leIndex < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT, "lePlayerScoringIndex < E_PLAYER_SCORING_INDEX_COUNT");
    return &maCarData[leIndex];
}

// const twin.
const CarData* ScoringSystem::GetCarDataFromPlayerScoringIndex(GameStateModuleIO::EPlayerScoringIndex leIndex) const
{
    CGS_ASSERT(leIndex < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT, "lePlayerScoringIndex < E_PLAYER_SCORING_INDEX_COUNT");
    return &maCarData[leIndex];
}

// X360 0x8231DE38 (the single exported GetPlayerScoringIndex matches by stored race-car index;
// only the network-id sibling below carries distinct X360 code). Player scoring slot whose stored
// race-car index matches leRaceCarIndex; returns slot 0 if absent.
GameStateModuleIO::EPlayerScoringIndex ScoringSystem::GetPlayerScoringIndex(EActiveRaceCarIndex leRaceCarIndex) const
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetActiveRaceCarIndex() == leRaceCarIndex)
        {
            return static_cast<GameStateModuleIO::EPlayerScoringIndex>(liSlot);
        }
    }

    CGS_ASSERT(false, "RaceCarIndex not found in scoring system");
    return static_cast<GameStateModuleIO::EPlayerScoringIndex>(0);
}

// X360 0x8231DE38 (network-id form). Player scoring slot whose stored network player id matches
// lID; fires the "not found in scoring system" assert and returns slot 0 if absent.
GameStateModuleIO::EPlayerScoringIndex ScoringSystem::GetPlayerScoringIndex(BrnNetwork::NetworkPlayerID lID) const
{
    CGS_ASSERT(lID != K_INVALID_PLAYER_ID, "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetNetworkPlayerID() == lID)
        {
            return static_cast<GameStateModuleIO::EPlayerScoringIndex>(liSlot);
        }
    }

    CGS_ASSERT(false, "NetworkPlayerID not found in scoring system");
    return static_cast<GameStateModuleIO::EPlayerScoringIndex>(0);
}

// X360 0x8231DFC8. A scoring slot is "in the system" when its stored race-car index is not the
// COUNT sentinel.
bool ScoringSystem::IsPlayerInScoringSystem(GameStateModuleIO::EPlayerScoringIndex leIndex) const
{
    CGS_ASSERT(leIndex >= GameStateModuleIO::E_PLAYER_SCORING_INDEX_0, "lePlayerScoringIndex >= GsmIO::E_PLAYER_SCORING_INDEX_0");
    CGS_ASSERT(leIndex < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT, "lePlayerScoringIndex < GsmIO::E_PLAYER_SCORING_INDEX_COUNT");

    return maCarData[leIndex].GetActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_COUNT;
}

// X360 0x8231E050. True if any scoring slot's stored network player id matches lID.
bool ScoringSystem::IsNetworkPlayerInScoringSystem(BrnNetwork::NetworkPlayerID lID) const
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetNetworkPlayerID() == lID)
        {
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// network-player roster counts.
// ----------------------------------------------------------------------------

// X360 0x82311020. Count of scoring slots whose race-car index is not the COUNT sentinel.
s32 ScoringSystem::GetNumberOfNetworkPlayers() const
{
    s32 liCount = 0;
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_COUNT)
        {
            ++liCount;
        }
    }
    return liCount;
}

// X360 0x823560B8. Count of assigned slots that have not disconnected.
s32 ScoringSystem::GetNumberOfNetworkPlayersStillConnected() const
{
    s32 liCount = 0;
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        const CarData& lCar = maCarData[liSlot];
        if (lCar.GetActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_COUNT && !lCar.GetScoreData()->GetDisconnected())
        {
            ++liCount;
        }
    }
    return liCount;
}

// X360 0x82311098. All race cars are setup once no scoring slot still holds the INVALID (-1)
// race-car index; returns false on the first unassigned slot.
bool ScoringSystem::AreAllRaceCarsSetup() const
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// team iteration.
// ----------------------------------------------------------------------------

// X360 0x82320270. Next active-race-car (after leRaceCarIndex, or from slot 0 when INVALID) that
// belongs to team leTeam; E_ACTIVE_RACE_CAR_INDEX_INVALID if none.
EActiveRaceCarIndex ScoringSystem::GetNextTeamMember(EActiveRaceCarIndex leRaceCarIndex,
                                                     GameStateModuleIO::EPlayerTeam leTeam) const
{
    CGS_ASSERT((leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0) || (leRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID),
               "( leLastActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 ) || ( leLastActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID )");
    CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leLastActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
    CGS_ASSERT(leTeam >= GameStateModuleIO::E_PLAYER_TEAM_NONE, "leTeam >= GsmIO::E_PLAYER_TEAM_START");
    CGS_ASSERT(leTeam < GameStateModuleIO::E_PLAYER_TEAM_COUNT, "leTeam < GsmIO::E_PLAYER_TEAM_COUNT");

    s32 liStart = (leRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID) ? 0 : (leRaceCarIndex + 1);

    for (s32 liNext = liStart; liNext < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liNext)
    {
        const CarData* lpCarData = GetCarData(static_cast<EActiveRaceCarIndex>(liNext));
        if (lpCarData != NULL && lpCarData->GetTeam() == leTeam)
        {
            return static_cast<EActiveRaceCarIndex>(liNext);
        }
    }
    return E_ACTIVE_RACE_CAR_INDEX_INVALID;
}

// ----------------------------------------------------------------------------
// burnout-skillz tally.
// ----------------------------------------------------------------------------

// X360 0x82311110. Slot of maBurnoutSkillzData[] whose parallel player-id matches leRaceCarIndex
// (the id arg); NULL when no slot matches.
BurnoutSkillzData* ScoringSystem::GetBurnoutSkillzData(EActiveRaceCarIndex leRaceCarIndex)
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maBurnoutSkillzPlayerIDs[liSlot] == leRaceCarIndex)
        {
            return &maBurnoutSkillzData[liSlot];
        }
    }
    return NULL;
}

// By-network-id twin (same parallel-array search).
BurnoutSkillzData* ScoringSystem::GetBurnoutSkillzData(BrnNetwork::NetworkPlayerID lID)
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maBurnoutSkillzPlayerIDs[liSlot] == lID)
        {
            return &maBurnoutSkillzData[liSlot];
        }
    }
    return NULL;
}

// X360 0x823561D0. Register lID in the skillz tally: if already present, just clear its data;
// otherwise claim the first free (-1) parallel slot, record the id, and clear the data.
void ScoringSystem::AddPlayerBurnoutSkillz(BrnNetwork::NetworkPlayerID lID, BrnNetwork::NetworkPlayerID lOtherID)
{
    BurnoutSkillzData* lpData = GetBurnoutSkillzData(lID);
    if (lpData != NULL)
    {
        CGS_ASSERT(lID == lOtherID, "WARNING: Player already in burnout skillz");
        lpData->Clear();
        return;
    }

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maBurnoutSkillzPlayerIDs[liSlot] == K_INVALID_PLAYER_ID)
        {
            maBurnoutSkillzPlayerIDs[liSlot] = lID;
            maBurnoutSkillzData[liSlot].Clear();
            return;
        }
    }

    CGS_ASSERT(false, "Could not find room in burnout skillz");
}

// X360 0x82311198. Clear every skillz record and free every parallel id slot (-1).
void ScoringSystem::ClearAllBurnoutSkillzData()
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maBurnoutSkillzData[liSlot].Clear();
        maBurnoutSkillzPlayerIDs[liSlot] = K_INVALID_PLAYER_ID;
    }
}

// X360 0x82311210. Clear and release the skillz slot matching lID (no-op if not present).
void ScoringSystem::ClearPlayersBurnoutSkillzData(BrnNetwork::NetworkPlayerID lID)
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maBurnoutSkillzPlayerIDs[liSlot] == lID)
        {
            maBurnoutSkillzData[liSlot].Clear();
            maBurnoutSkillzPlayerIDs[liSlot] = K_INVALID_PLAYER_ID;
            return;
        }
    }
}

// X360 0x82356138. Copy lpData into the skillz slot matching lID (no-op if not present). The
// X360 build does a raw 56-byte memcpy; the clean form is a value copy of the record.
void ScoringSystem::SetBurnoutSkillzData(BrnNetwork::NetworkPlayerID lID, const BurnoutSkillzData* lpData)
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maBurnoutSkillzPlayerIDs[liSlot] == lID)
        {
            maBurnoutSkillzData[liSlot] = *lpData;
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// online finish position.
// ----------------------------------------------------------------------------

// X360 0x823112A8. Forward to the current online-mode scorer's per-car finishing position.
s32 ScoringSystem::GetOnlineFinishPosition(EActiveRaceCarIndex leRaceCarIndex)
{
    CGS_ASSERT(mpCurrentOnlineModeScoring != NULL, "mpCurrentOnlineModeScoring");
    CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    return mpCurrentOnlineModeScoring->GetPlayerPosition(leRaceCarIndex);
}

// ----------------------------------------------------------------------------
// network round data -- the per-round save into the online-results aggregate.
// ----------------------------------------------------------------------------

// X360 0x8232BC78. End-of-round bookkeeping into mOnlineGameResults. Looks up the round's car
// (by network player id, or by race-car index when lID is the invalid -1), then:
//   * stamps mCarUsed with the car id,
//   * accumulates the per-car cumulative stats (meters driven + the five takedown counters),
//   * advances mSecondsInEvent by the round's elapsed time (the car's recorded finish time when it
//     has one, else the time since the mode timer started), and
//   * dispatches by online game mode into SetRaceResults (online race) or SetStuntResults
//     (online fugitive / free-burn / mode-end) keyed by the post-incremented round-write cursor.
//
// The X360 binary takes the game-mode discriminant as a 4th register argument the keystone's
// 3-parameter signature drops; semantically it is the EGameModeType the results aggregate already
// records (mOnlineGameResults.miEventType, stamped by OnModeStart -- X360 stw r30,0x4E04), so the
// switch reads it back from there. The X360 free-build's distinct mNetworkRoundData::SetPosition
// path (PS3 DWARF) is not present in this 0x8232BC78 body, which writes the results aggregate.
//
// The round-time pointer handed to SetRaceResults follows that method's committed convention: a
// two-float {seconds-as-f32, fraction} pair (SetRaceResults re-narrows word[0] back to s32 seconds).
void ScoringSystem::SaveNetworkRoundData(BrnNetwork::NetworkPlayerID lID, CgsSystem::Time lTime,
                                         EActiveRaceCarIndex leRaceCarIndex)
{
    CarData* lpCarData = (lID == K_INVALID_PLAYER_ID) ? GetCarData(leRaceCarIndex) : GetCarData(lID);
    CGS_ASSERT(lpCarData != NULL, "lpCarData");

    if (lpCarData != NULL)
    {
        const GameStateModuleIO::CarScoreData* lpScoreData = lpCarData->GetScoreData();

        // ---- car id + cumulative-stat header ----
        mOnlineGameResults.mCarUsed = lpCarData->GetCarID();
        mOnlineGameResults.mfMetersDriven += lpScoreData->GetDistanceAccumulator();
        mOnlineGameResults.miTakedownsFor += lpScoreData->GetTakedowns();
        mOnlineGameResults.miTakedownsAgainst += lpScoreData->GetTakedownsAgainst();
        mOnlineGameResults.miTraitorousTakedownsFor += lpScoreData->GetTraitorousTakedownsFor();
        mOnlineGameResults.miTraitorousTakedownsAgainst += lpScoreData->GetTraitorousTakedownsAgainst();
        mOnlineGameResults.miMarkedManTakedownsFor += lpScoreData->GetMarkedManTakedownsFor();

        // ---- accumulate this round's time: the car's recorded finish time if it has one,
        //      else the time elapsed since the mode timer started. ----
        const CgsSystem::Time lFinishTime = lpScoreData->GetFinishTime();
        const f32 lfFinishTimeAsScalar = static_cast<f32>(lFinishTime.GetSeconds()) + lFinishTime.GetFraction();
        const CgsSystem::Time lRoundTime = (lfFinishTimeAsScalar == 0.0f) ? GetElapsedTime(lTime) : lFinishTime;
        mOnlineGameResults.mSecondsInEvent += lRoundTime;

        const s32 liRoundIndex = mOnlineGameResults.miReserved0x2C;

        // ---- mode dispatch (discriminant == the recorded online event type) ----
        switch (mOnlineGameResults.miEventType)
        {
            case GameStateModuleIO::E_MODE_ONLINE_RACE:
            {
                if (lpScoreData->GetTimedOut())
                {
                    // Timed out: zero finish time, real distance-to-finish.
                    const f32 lfDistanceToFinish = GetRaceCarDistanceToFinish(leRaceCarIndex);
                    const CgsSystem::Time lZeroTime(0.0f);
                    const f32 lafRoundTime[2] = { static_cast<f32>(lZeroTime.GetSeconds()), lZeroTime.GetFraction() };
                    mOnlineGameResults.SetRaceResults(liRoundIndex, lafRoundTime, lfDistanceToFinish);
                }
                else
                {
                    // Finished: real finish time, zero distance.
                    const CgsSystem::Time lFinish = GetFinishTime(leRaceCarIndex);
                    const f32 lafRoundTime[2] = { static_cast<f32>(lFinish.GetSeconds()), lFinish.GetFraction() };
                    mOnlineGameResults.SetRaceResults(liRoundIndex, lafRoundTime, 0.0f);
                }
                break;
            }

            case GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
            case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
            case GameStateModuleIO::E_MODE_ONLINE_MODE_END:
            {
                const s32 liTeamStuntScore = GetTeamStuntScore(lpCarData->GetTeam());
                mOnlineGameResults.SetStuntResults(liRoundIndex, lpScoreData->GetOnlineStuntScore(), liTeamStuntScore);
                break;
            }

            case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY:
                // No round results recorded for the free-burn lobby.
                break;

            default:
                CGS_ASSERT(false, "No results for this game mode");
                break;
        }
    }

    ++mOnlineGameResults.miReserved0x2C;
}

}
