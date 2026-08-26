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

// ----------------------------------------------------------------------------
// full scoring-state reset.
// ----------------------------------------------------------------------------

namespace
{
    // X360 ClearData (0x8232A4A8) seeds the race distance-to-finish scalars (mfTotalRaceDistance
    // @ss+0x5CE4 and mfPlayerDistanceToFinishLastFrame @ss+0x5CE8) with the rodata float
    // flt_82CDB7D0 -- a "no valid distance yet" sentinel (the same constant another scoring body
    // compares against as `v != flt_82CDB7D0` for distances already > 200000.0). The exact magnitude
    // is recovered from the PS3 DecFIGS ScoringSystem::ClearData (0x1E364C, same source): both
    // scalars (and the per-car positioning scratch) are seeded with 3.4028235e38 == FLT_MAX. The
    // X360 0x8232A4A8 confirms the same flt_82CDB7D0 store path; the PS3 supplies the literal value.
    // RaceCarPositioningData's own distances are seeded with the same sentinel via its Construct()
    // (kept named, not hard-coded here).
    const f32 KF_INVALID_RACE_DISTANCE = 3.4028235e38f;   // flt_82CDB7D0 == FLT_MAX (PS3 0x1E364C)
}

// X360 0x8232A4A8. Full scoring-state reset. NAMED-MEMBER semantic-parity reconstruction (NOT a
// byte-exact raw-offset rewrite): the X360 build inlines each embedded sub-object's ClearData/
// Construct and the per-field setters into one flat store block; here those operations are restored
// to their named-member form, cross-checked store-for-store against the ASM. The bool argument
// (X360 a2, DWARF lbResetCarData) gates the per-car CarData reset; the per-car positioning data is
// reset on every slot regardless. Members the semantic slice omits are simply not part of the reset.
void ScoringSystem::ClearData(bool lbResetCarData)
{
    // ---- mode timer / time limit back to the inactive (negative-seconds) state ----
    ClearModeTimer();   // mStartTime.SetSeconds(-1)  (ASM stw r27,0(r31))
    ClearTimeLimit();   // mEndTime.SetSeconds(-1)    (ASM stw r27,8(r31))

    // ---- bring every embedded sub-scorer down (ASM: the three online scorers via vtable+0x10
    //      ClearData @ss+0x4B74/0x4BF8/0x4CC0; the road-rage / crash / stunt scorers below) ----
    mOnlineRaceModeScoring.ClearData();
    mOnlineRoadRageScoring.ClearData();
    mOnlineBurningHomeRunScoring.ClearData();

    // mRoadRageModeScoring @ss+0x4B40: the ASM inlines its ClearData (zeroes the takedown counters /
    // extension state, miTargetNumTakedowns := -1, clears the four damage/active bools).
    mRoadRageModeScoring.ClearData();
    miCurrentPlayerCrashedNumber = 0;                       // ASM stw r30,0x4B5C

    mCrashModeScoring.ClearData();                          // ASM bl CrashModeScoring::ClearData (ss+0x20)
    mStuntModeScoring.ClearData();                          // ASM vtable+0x10 @ss+0x350 (offline stunt)
    mOnlineStuntModeScoring.ClearData();                   // ASM vtable+0x10 @ss+0x2620 (online stunt)

    mTotalTime = CgsSystem::Time(0.0f);                    // ASM CgsSystem::Time::operator=(ss+0x10, 0.0)

    muTotalLaps         = 0;                                // ASM stw r30,0x4ED8
    muNumCarsFinishedRace = 0;                             // ASM stw r30,0x4EDC
    mbACarHasFinishedTheRace = false;                      // ASM stb r30,0x4EFA

    // ---- per-slot reset: CarData (gated on lbResetCarData) + RaceCarPositioningData (always) ----
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (lbResetCarData)
        {
            CarData& lCar = maCarData[liSlot];
            lCar.GetScoreData()->ClearData();                          // ASM bl CarScoreData::ClearData(&maCarData[i])
            lCar.SetCarID(0);                                          // ASM std r30,-0x2C(r29)  mCarId := 0
            lCar.SetDriftDistance(0.0f);                               // ASM stfs f31,-0x1C(r29) mfCurrentDriftDistance := 0
            // NOTE: the X360 ClearData does NOT reset per-car status here (verifier-confirmed); the
            // earlier SetStatus(E_PLAYER_STATUS_PLAYING) was a fabricated store -- removed.
            lCar.SetTeam(GameStateModuleIO::E_PLAYER_TEAM_NONE);       // ASM stw r30 (=0)
            lCar.SetRoundStartTeam(GameStateModuleIO::E_PLAYER_TEAM_NONE);
            lCar.SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_COUNT); // ASM stw r25 (=8)  the COUNT free-slot sentinel
            lCar.SetNetworkPlayerID(K_INVALID_PLAYER_ID);              // ASM stw r27 (=-1)
            lCar.SetCurrentCheckPoint(0);                              // ASM stb r30 (=0)
        }

        // RaceCarPositioningData::Construct() inlined per slot (ASM r28 block @ss+0x59C0+i*0x18):
        // meActiveRaceCarIndex := -1, distances := large sentinel, miCurrentCheckpoint := -1,
        // miFinishPosition := 8, mbDisconnected := false. Kept as the named Construct() so the
        // sentinel-distance init stays owned by RaceCarPositioningData's own TU.
        maRaceCarPositioningData[liSlot].Construct();
    }

    // ---- checkpoint distance tables both cleared (ASM 0x5C60 loop: 16 floats at +0x00 and +0x40) ----
    for (s32 liCheckpoint = 0; liCheckpoint < 16; ++liCheckpoint)
    {
        mafCheckpointSeparations[liCheckpoint]       = 0.0f;   // ASM stfs f31,0(r11)
        mafCheckpointDistancesToFinish[liCheckpoint] = 0.0f;   // ASM stfs f31,0x40(r11)
    }
    SetCheckPointDistancesToFinishReady(false);                // ASM stb r30,0x5CE0

    // ---- race / player distance + timing scalars ----
    mfTotalRaceDistance                = KF_INVALID_RACE_DISTANCE;  // ASM stfs f0(flt_82CDB7D0==FLT_MAX),0x5CE4
    mfPlayerDistanceToFinishLastFrame  = KF_INVALID_RACE_DISTANCE;  // ASM stfs f0,0x5CE8
    mfPlayerTimeHeadingTheWrongWay     = 0.0f;                      // ASM stfs f31,0x5CEC
    mfPlayerTimeStationary             = 0.0f;                      // ASM stfs f31,0x5CF0
    mfPlayerTimeWithoutInput           = 0.0f;                      // ASM stfs f31,0x5CF4

    // ---- car-count + lead/last bookkeeping ----
    muCarsInCurrentMode               = 0;                          // ASM stw r30,0x4EE8
    muActualNumberOfCarsInCurrentMode = 0;                          // ASM stw r30,0x4EEC
    meLeadRaceCarIndex                = E_ACTIVE_RACE_CAR_INDEX_INVALID;  // ASM stw r27,0x4EF0
    meLastRaceCarIndex                = E_ACTIVE_RACE_CAR_INDEX_INVALID;  // ASM stw r27,0x4EF4
    mbNewLeader                       = false;                      // ASM stb r30,0x4EF8
    mbNewLastPlace                    = false;                      // ASM stb r30,0x4EF9

    // ASM stb r30,0x5D21 -- a single byte zeroed in the elimination-state region (between
    // mbEliminationRequired @ss+0x5D20 and muCarsThatHaventBeenEliminated @ss+0x5D24). Its field
    // identity is NOT confidently recoverable (the semantic slice models mbEliminationRequired at
    // that anchor but has no named bool at +0x5D21); rather than risk a confidently-wrong member
    // name (the OnModeStart-revert lesson), this single store is left UNMODELED and FLAGGED.

    // ASM stw r30,0x5D40 -- the eliminated-race-car index list reset (DWARF SHAPE:
    // CgsContainers::Array<EActiveRaceCarIndex,7u>::Construct), i.e. drive its live count to 0.
    maEliminatedActiveRaceCarIndexs.Construct();
}

// ============================================================================================
// ScoringSystem::HasBeatenRoadRageTarget -- DWARF BrnScoringSystem.h:1180
// ============================================================================================
// [stuntrace waveB CLOSURE round, 2026-08-26] Bodied. It was DECLARE-ONLY tree-wide, and the
// cross-seam audit found FOUR live call sites across TWO partfiles, not the two the fix-round
// sheet listed: BrnModeManager_Finish.cpp:290 (GetPlayersFinishPosition's road-rage arm) and
// :492 (FinishCurrentMode's road-rage arm), plus BrnModeManager_UpdateMode.cpp:540 and :581.
// One missing body blocked both partfiles.
//
// [!] NOT the same symbol as RoadRageModeScoring::HasBeatenRoadRageTarget
// (BrnRoadRageModeScoring.h:108, bodied in BrnRoadRageModeScoringLinkStubs.cpp:175). That one is
// on the SUB-SCORER; this one is on ScoringSystem and forwards to it. The audit flags the mix-up
// explicitly because the stub file's presence makes the ScoringSystem symbol look bodied.
//
// X360: INLINED at every call site, so there is no standalone export to transcribe -- but the
// inlining is unambiguous. ModeManager::FinishCurrentMode @0x8234B978, jumptable case 3
// (E_MODE_ROAD_RAGE):
//
//   0x8234BBB4  lwz  r11, 0x58F0(r31)     ; r31 == the ModeManager
//   0x8234BBB8  lwz  r10, 0x58FC(r31)
//   0x8234BBBC  cmpw cr6, r11, r10
//   0x8234BBC4  bge  cr6, loc_8234BBCC    ; TRUE  <=>  takedowns >= target   (SIGNED, >=)
//
// mScoringSystem sits at ModeManager+0xDB0 -- independently attested by
// SendModeStopMessages @0x8234C6B0's `addi r3, r28, 0xDB0` (== &mScoringSystem, quoted in
// BrnScoringSystem.h's ClearData banner) -- so 0x58F0 == ss+0x4B40 and 0x58FC == ss+0x4B4C.
//
// OFFSET -> NAME, both ends, so no raw offset survives into the body:
//   * mRoadRageModeScoring is the embedded sub-scorer at ss+0x4B40. Pinned by its NEIGHBOURS in
//     BrnScoringSystem.h:727-729: miMaximumPlayerCrashedNumber is X360 ss+0x4B58 and
//     miCurrentPlayerCrashedNumber ss+0x4B5C, and the struct that ends just before them starts
//     at 0x4B40.
//   * Inside it, BrnRoadRageModeScoring.h's DWARF-authoritative member run puts
//     miNumTakedownsAchieved at +0x00 and miTargetNumTakedowns at +0x0C
//     (miNumTakedownsAchievedForNextExtention +0x04, muRoadRageTriggerExtension +0x08,
//     muRoadRageExtensionTime +0x0A). 0x4B40 + 0x00 == 0x4B40 and 0x4B40 + 0x0C == 0x4B4C --
//     the two words the asm reads, exactly.
// So the console's inlined pair IS RoadRageModeScoring::HasBeatenRoadRageTarget()'s own body
// (`miNumTakedownsAchieved >= miTargetNumTakedowns`), reached through the sub-scorer. Written as
// the named forward rather than re-deriving the compare here, so the two symbols cannot drift.
//
// Non-const because the DWARF declares it non-const (:1180); GetRoadRageScoring() has both
// overloads, so the non-const one binds.
// ============================================================================================
bool ScoringSystem::HasBeatenRoadRageTarget()
{
    return GetRoadRageScoring()->HasBeatenRoadRageTarget();
}

}
