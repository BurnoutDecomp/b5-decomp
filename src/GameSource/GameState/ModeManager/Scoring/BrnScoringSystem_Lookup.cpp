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
// network round data -- FLAGGED BLOCKED (both methods, re-checked this round).
// ----------------------------------------------------------------------------
//
// SaveNetworkRoundData (X360 0x8232BC78) and ClearData (X360 0x8232A4A8) both operate on the
// OnlineGameResults aggregate that the SEMANTIC-SLICE keystone deliberately OMITS (see the SCOPE
// NOTE in BrnScoringSystem_Lifecycle.cpp and the WriteDataToOutput blocker there). Neither body is
// landable, for the SAME root cause that blocks WriteDataToOutput: there is no named-member path to
// the X360 ScoringSystem byte region the bodies read/write, and the keystone is a semantic slice,
// NOT a byte-exact layout, so the raw offsets cannot be mapped onto named members.
//
//   SaveNetworkRoundData -- the X360 body, on the found CarData*, accumulates per-car stats into the
//   cumulative block at this+0x4DE0..0x4DF4 and advances a CgsSystem::Time at this+0x4DD8, all inside
//   an embedded OnlineGameResults aggregate based at this+0x4DD0, then dispatches by game mode into
//   OnlineGameResults::SetRaceResults / SetStuntResults on that aggregate (passing the round-index
//   counter at this+0x4DFC) and post-increments that counter. OnlineGameResults itself IS a committed
//   type (BrnGameActions.h, with SetRaceResults/SetStuntResults committed in BrnGameActions.cpp), but
//   the keystone exposes NO member of it and NO accessor reaching it. The only adjacent named member,
//   mNetworkRoundData (NetworkRoundData, reached via GetNetworkRoundData()), is a DISTINCT slice object
//   with no SetRaceResults/SetStuntResults surface -- writing the accumulation into it instead would be
//   a fabrication, not a reconstruction. Plus GetRaceCarDistanceToFinish / GetFinishTime / GetTeamStuntScore
//   are themselves keystone declare-only helpers with no committed body to fold.
//
//   ClearData -- the X360 body resets the start/end/total/remaining timers and the per-car maCarData[8]
//   loop (CarData::ClearData) AND the omitted region: virtual ClearData on the three embedded online
//   sub-scorers, the OnlineGameResults aggregate, the RaceCarPositioningData scratch and the
//   per-checkpoint distance arrays -- but every write target sits in the X360 byte-offset space that the
//   semantic slice does not preserve (the omitted OnlineGameResults / second-StuntModeScoring / debug-base
//   members shift every offset). It also calls GameStateModuleIO::CarScoreData::ClearData inside the
//   per-car loop, which is fine, but the surrounding offsets cannot be re-expressed as named members
//   without the omitted aggregate and a byte-exact layout. A partial body that only cleared the
//   keystone-named scalars would silently drop the online-results / sub-scorer / positioning reset --
//   a semantic regression.
//
// Both remain declare-only. They land once the keystone grows a named OnlineGameResults member (and the
// surrounding cumulative-stats region) with accessors -- the same growth WriteDataToOutput is waiting on.

}
