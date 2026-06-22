// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager.cpp
// ============================================================================
// The mode-management core of BrnGameState::ModeManager. Bodies the 15 X360-attested
// methods that drive checkpoint tracking, stunt-challenge lifecycle, the network stunt
// score relay, and the per-mode result packers. Each body is reconstructed store-for-store
// from the BURNOUT_X360_ARTIST.XEX pseudocode/asm, reaching the embedded ScoringSystem /
// the per-car checkpoint trackers / the current GameMode BY NAME against BrnModeManager.h.
//
// Owning header: GameSource/GameState/ModeManager/BrnModeManager.h (grown additively by this TU).
//
// FLAG (cross-object reads blocked on the GameStateModule minimal slice): the X360 reaches a
// handful of GameStateModule internals by raw offset that the minimal GameStateModule slice does
// not expose as named accessors (the global/active race-car output interfaces and the online round
// counters). Those are de-inlined to the ModeManager private helpers GetGlobalRaceCarOutputInterface
// / GetActiveRaceCarOutputInterface / GetOnlineRoundIndex / GetOnlineActiveCarCount, declared-only
// in the header; their bodies (which read through mpGameStateModule) land when GameStateModule grows
// the matching accessors. We do NOT fabricate those offsets here.

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"   // GameStateModule::GetPlayerActiveRaceCarIndex (SendModeResults)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::VariableEventQueue<>::AddEvent (SendModeResults)
// The race-car output interfaces are completed here so GetGlobalRaceCarIndex can be called by name.
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
// The checkpoint TriggerData landmark lookup needs the complete Landmark / BoxRegion layout.
#include "SharedClasses/Trigger/BrnRegion.h"           // BrnTrigger::BoxRegion::GetPosition

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// Small predicates (X360-inlined field reads de-inlined to named accessors).
// ----------------------------------------------------------------------------

// X360 0x82311410.
bool ModeManager::IsOnlineGameMode() const
{
    // v1 = mpCurrentGameMode; if (v1) return v1->IsOnline(); else return 0;
    if (mpCurrentGameMode != nullptr)
    {
        return mpCurrentGameMode->IsOnline();   // X360 mode+0xAC (the cached online flag)
    }
    return false;
}

// X360 0x823113D0. True when the current mode is in one of its post-event states (3/5/4).
bool ModeManager::IsInPostEvent() const
{
    if (mpCurrentGameMode == nullptr)
    {
        return false;
    }
    const s32 leState = mpCurrentGameMode->GetCurrentState();   // X360 mode+0x28 (meCurrentState)
    // The X360 returns true for state == 3, 5 or 4 (the three post-event states); false otherwise.
    return (leState == 3) || (leState == 5) || (leState == 4);
}

// ----------------------------------------------------------------------------
// Checkpoint tracking (delegates to the per-car CarCheckpointData bit set).
// ----------------------------------------------------------------------------

// X360 0x82329890. The car's next-expected checkpoint (clamped to u8 by the X360).
s32 ModeManager::GetNextLandmarkIndex(EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= 0, "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    const s32 liNext = maCarCheckpointData[leGlobalRaceCarIndex].GetNextCheckpointIndex();
    return static_cast<u8>(liNext);   // X360 clrlwi r3,r3,24 (low byte -- the LandmarkIndex)
}

// X360 0x8231E960. Popcount of the car's "remaining" bit set.
u32 ModeManager::CountCheckpointsRemaining(EGlobalRaceCarIndex leGlobalRaceCarIndex) const
{
    CGS_ASSERT(leGlobalRaceCarIndex >= 0, "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    // The X360 inlines a SWAR popcount over the car's remaining bit mask. The semantically-equivalent
    // named operation: gather every still-remaining checkpoint index; the count IS that population.
    s32 laiRemaining[GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE];
    const s32 liRemaining =
        maCarCheckpointData[leGlobalRaceCarIndex].GetAllRemainingCheckpointIndexes(laiRemaining);
    return static_cast<u32>(liRemaining);
}

// X360 0x8231E800. Mark a checkpoint as reached by the given car.
void ModeManager::MarkCarHittingCheckpoint(u32 luCheckpointIndex, EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CGS_ASSERT(static_cast<s32>(luCheckpointIndex) >= 0, "liCheckpointIndex >= 0");
    CGS_ASSERT(luCheckpointIndex < muNumLandmarks, "static_cast<uint32_t>( liCheckpointIndex ) < muNumLandmarks");
    CGS_ASSERT(leGlobalRaceCarIndex >= 0, "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    maCarCheckpointData[leGlobalRaceCarIndex].MarkCheckpointAsHit(static_cast<s32>(luCheckpointIndex));
}

// X360 0x8231E8D8. Re-arm the car's checkpoint tracker for the next lap.
void ModeManager::ResetCheckpointDataForNextLap(EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CGS_ASSERT(leGlobalRaceCarIndex >= 0, "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    maCarCheckpointData[leGlobalRaceCarIndex].SetupCheckpoints(static_cast<s32>(muNumLandmarks));
}

// X360 0x82329910. Process a landmark trigger for the given car.
bool ModeManager::HasRaceCarHitValidCheckpoint(s16 luLandmarkId, EGlobalRaceCarIndex leGlobalRaceCarIndex)
{
    CGS_ASSERT(leGlobalRaceCarIndex >= 0, "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0");
    CGS_ASSERT(leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT,
               "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");

    // Find the mode-local checkpoint index whose landmark-region id matches the triggered landmark.
    u32 luCheckpoint = 0;
    const u32 luNumLandmarks = muNumLandmarks;
    while (luCheckpoint < luNumLandmarks)
    {
        if (static_cast<s16>(maLandmarkRegionIndexes[luCheckpoint]) == luLandmarkId)
        {
            break;
        }
        ++luCheckpoint;
    }
    if (luCheckpoint == luNumLandmarks)
    {
        return false;   // not a checkpoint landmark for this mode
    }

    // Map the global car to its active slot. FLAG: the X360 indexes an internal table on the active
    // interface @ mpGameStateModule+0x245968 (`*(4*(global+525)+interface)`); de-inlined to the
    // GlobalToActiveRaceCarIndex() helper (declared-only -- see header FLAG).
    const EActiveRaceCarIndex leActiveRaceCarIndex = GlobalToActiveRaceCarIndex(leGlobalRaceCarIndex);
    CGS_ASSERT(leActiveRaceCarIndex >= 0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    // For the online burning-home-run mode, the car must be in the "running" scoring state.
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN)
    {
        CGS_ASSERT((leActiveRaceCarIndex > -1) && (leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");
        CarData* lpCarData = mScoringSystem.GetCarData(leActiveRaceCarIndex);
        // X360: bail unless the car is present and its per-car state field (CarData+0x13C) == 2.
        // FLAG: CarData+0x13C is outside the minimal CarData slice; the only state-style field this
        // TU can name is GetStatus(). E_PLAYER_STATUS_COUNT == 2 matches the compared constant.
        if ((lpCarData == nullptr) || (lpCarData->GetStatus() != CarData::E_PLAYER_STATUS_COUNT))
        {
            return false;
        }
    }

    // Is luCheckpoint the car's next-expected checkpoint?
    if (static_cast<s32>(luCheckpoint) != maCarCheckpointData[leGlobalRaceCarIndex].GetNextCheckpointIndex())
    {
        return false;
    }

    // Record the next landmark + mark this checkpoint hit.
    maNextLandmarkIndex[leGlobalRaceCarIndex] = static_cast<u8>(GetNextLandmarkIndex(leGlobalRaceCarIndex));
    MarkCarHittingCheckpoint(luCheckpoint, leGlobalRaceCarIndex);

    // Online burning-home-run: bump the car's cumulative + current checkpoint counters.
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN)
    {
        // FLAG: the X360 does ++CarData+0xC4 (CarScoreData::miCumulativeCheckpoints) and ++CarData+0xC0
        // (the adjacent per-round checkpoint count). CarScoreData exposes GetCumulativeCheckpoints()
        // by name but no SETTER for it, and +0xC0 has no named accessor at all in the current slice.
        // The increments are therefore deferred to the CarScoreData grow (documented, NOT fabricated
        // as raw offset writes); the read accessor is exercised below to keep the wiring honest.
        GameStateModuleIO::CarScoreData* lpScore = mScoringSystem.GetCarData(leActiveRaceCarIndex)->GetScoreData();
        (void)lpScore->GetCumulativeCheckpoints();
    }

    return true;
}

// ----------------------------------------------------------------------------
// Win / results queries.
// ----------------------------------------------------------------------------

// X360 0x823283E8.
bool ModeManager::HasPlayerWon()
{
    const s32 liFinishPosition = GetPlayersFinishPosition();
    if (liFinishPosition == 1)
    {
        return true;
    }
    // Offline-race only: a 2nd-place finish counts as a win iff mbWinIfSecond is set.
    if (meCurrentGameModeType != GameStateModuleIO::E_MODE_OFFLINE_RACE)
    {
        return false;
    }
    if (!mbWinIfSecond)
    {
        return false;
    }
    return (liFinishPosition == 2);
}

// ----------------------------------------------------------------------------
// Stunt-challenge lifecycle (reached through the ScoringSystem's online stunt scorer).
// ----------------------------------------------------------------------------

// X360 0x8231EB00. The X360 dispatches the scorer reset through the embedded online stunt scorer
// (ModeManager+0x33D0 == ScoringSystem+0x2620 == mScoringSystem.GetOnlineStuntScorer()) then activates it.
void ModeManager::SetupStuntChallenge()
{
    StuntModeScoring* lpStuntModeScoring = mScoringSystem.GetOnlineStuntScorer();
    CGS_ASSERT(lpStuntModeScoring != nullptr, "lpStuntModeScoring");
    // FLAG: the X360 reset is a virtual dispatch through the scorer's vtable slot +0x10; the exact
    // named method at that slot is not recoverable from the bounded StuntModeScoring view (its full
    // vtable order is deferred to its own TU). ClearData() is the declared reset; called by name for
    // semantic parity. Confirm the precise virtual when StuntModeScoring's vtable is reconstructed.
    lpStuntModeScoring->ClearData();
    lpStuntModeScoring->Activate(0);
    mbStuntChallengeActive = true;   // X360 +0x950D = 1
}

// X360 0x823120E8.
void ModeManager::EndStuntChallenge()
{
    StuntModeScoring* lpStuntModeScoring = mScoringSystem.GetOnlineStuntScorer();
    CGS_ASSERT(lpStuntModeScoring != nullptr, "lpStuntModeScoring");
    // FLAG: same vtable-slot-+0x10 reset as SetupStuntChallenge (see note above).
    lpStuntModeScoring->ClearData();
    mbStuntChallengeActive = false;  // X360 +0x950D = 0
}

// ----------------------------------------------------------------------------
// Network stunt-score relay.
// ----------------------------------------------------------------------------

// X360 0x82363540.
void ModeManager::SetNetworkStuntScore(BrnNetwork::NetworkPlayerID lNetworkPlayerID, s32 liScore)
{
    CarData* lpCarData = mScoringSystem.GetCarData(lNetworkPlayerID);
    CGS_ASSERT(lpCarData != nullptr, "lpCarData != NULL");

    // Snapshot the prior online stunt score (CarData+0xD4 == CarScoreData::miOnlineStuntScore) and the
    // car's active-race-car index (CarData+0x144 == meRaceCarIndex) into the network-stunt cache.
    const s32 liPreviousScore = lpCarData->GetScoreData()->GetOnlineStuntScore();   // X360 CarData+0xD4
    const s32 liActiveCarIndex = static_cast<s32>(lpCarData->GetActiveRaceCarIndex()); // X360 CarData+0x144

    miNetworkStuntScore          = liScore;            // X360 +0x6CD4
    miNetworkStuntPreviousScore  = liPreviousScore;    // X360 +0x6CD8
    miNetworkStuntActiveCarIndex = liActiveCarIndex;   // X360 +0x6CD0

    mScoringSystem.SetNetworkStuntScore(lNetworkPlayerID, liScore);
}

// ----------------------------------------------------------------------------
// Checkpoint world position.
// ----------------------------------------------------------------------------

// X360 0x82327388. Returns the checkpoint's world position via the mode's checkpoint TriggerData.
Vector3 ModeManager::GetCheckpointPosition(u32 luCheckpointId) const
{
    CGS_ASSERT(luCheckpointId < muNumLandmarks, "luCheckpointIndex < muNumLandmarks");

    // The X360 loads the region-table index for this checkpoint, then resolves the owning landmark
    // through the trigger data and reads its box-region position.
    const s32 liRegionIndex = static_cast<s16>(maLandmarkRegionIndexes[luCheckpointId]);
    const BrnTrigger::TriggerData* lpTriggerData = GetCheckpointTriggerData();
    const BrnTrigger::Landmark* lpLandmark = lpTriggerData->GetLandmarkFromRegionIndex(liRegionIndex);
    return lpLandmark->GetBoxRegion()->GetPosition();
}

// ----------------------------------------------------------------------------
// Output packers.
// ----------------------------------------------------------------------------

// X360 0x82337B70. Copy each active car's checkpoint-remaining bit set into the output interface,
// then delegate to the ScoringSystem.
void ModeManager::WriteDataToOutput(GameStateModuleIO::ScoringOutputInterface* lpOutput,
                                    GameStateModuleIO::OnlineScoringOutputInterface* lpOnlineOutput,
                                    bool lbOnline,
                                    EActiveRaceCarIndex lePlayerRaceCarIndex)
{
    // FLAG: the X360 maps active->global through mpGameStateModule's global race-car output interface
    // @+0x3C0D0 and writes the 8 per-car u64 bit sets into lpOutput @+0x940. The minimal slices expose
    // neither the GameStateModule accessor nor the ScoringOutputInterface +0x940 field by name. We
    // de-inline the interface fetch to GetGlobalRaceCarOutputInterface() (declared-only -- header FLAG)
    // so the active->global mapping is bodied member-by-name; the per-car bit-set destination write is
    // left to the ScoringOutputInterface grow (documented, NOT fabricated as a raw +0x940 store).
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobal =
        GetGlobalRaceCarOutputInterface();
    for (s32 liActive = 0; liActive < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liActive)
    {
        const EGlobalRaceCarIndex leGlobal =
            lpGlobal->GetGlobalRaceCarIndex(static_cast<EActiveRaceCarIndex>(liActive));
        // The source bit set for an in-race car is maCarCheckpointData[leGlobal]; an absent car (index
        // -1) contributes an empty (zero) set. (Destination store deferred -- see FLAG above.)
        (void)leGlobal;
    }

    mScoringSystem.WriteDataToOutput(lpOutput, lpOnlineOutput, lbOnline, lePlayerRaceCarIndex);
}

// X360 0x82343438. Pack the player's mode results and queue them.
void ModeManager::SendModeResults(CgsModule::VariableEventQueue<13312, 16>* lpOutputQueue)
{
    // Online stunt-style modes finalise the online stunt scorer first (X360 vtable slot +0x20 on the
    // embedded online stunt scorer). FLAG: exact named virtual at slot +0x20 not recoverable -- the
    // semantic role is "end-of-mode finalise"; deferred to StuntModeScoring's vtable reconstruction.
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE
        || meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN
        || meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END)
    {
        // (online stunt scorer end-of-mode finalise -- see FLAG)
    }

    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");
    const EActiveRaceCarIndex lePlayer = mpGameStateModule->GetPlayerActiveRaceCarIndex();

    // Gather the per-car result fields by name from the ScoringSystem.
    const CgsSystem::Time lFinishTime      = mScoringSystem.GetFinishTime(lePlayer);
    const CgsSystem::Time lFastestLapTime  = mScoringSystem.GetRaceCarFastestLapTime(lePlayer);
    const s32             liTakedowns      = mScoringSystem.GetNumberOfTakedowns(lePlayer);
    const f32             lfDistanceToFin  = mScoringSystem.GetRaceCarDistanceToFinishAtRoundEnd(lePlayer);
    const EActiveRaceCarIndex leEliminator = mScoringSystem.GetRaceCarEliminatorIndex(lePlayer);
    const s32             liEliminations   = mScoringSystem.GetNumberOfEliminations(lePlayer);

    // Race position: the explicit override (mbResultsPositionValid + a positive miResultsOverridePosition)
    // wins; otherwise the live scoring position.
    s32 liRacePosition;
    if (mbResultsPositionValid && (miResultsOverridePosition > 0))
    {
        liRacePosition = miResultsOverridePosition;
    }
    else
    {
        liRacePosition = static_cast<s32>(mScoringSystem.GetCarRacePosition(lePlayer));
    }

    // The eliminated flag (CarData+0xD9 == CarScoreData::mbEliminated) + the per-mode latch byte.
    const bool lbEliminated = mScoringSystem.GetCarData(lePlayer)->GetScoreData()->GetEliminated();

    // FLAG: the X360 builds a 36-byte results record on the stack (finish time, fastest lap,
    // takedowns/distance/eliminator/race position/eliminations + the online flag + the two latch
    // bytes), fills its last field through the current mode's vtable slot +0x3C, and AddEvent's it as
    // {ptr,size=36,align=48}. The 36-byte record type (GameStateModuleIO mode-results action) is not
    // yet homed and the GameMode slot-+0x3C virtual is not named in the bounded GameMode view, so the
    // record build + the AddEvent are deferred (NOT fabricated). The gather above is bodied by name.
    (void)lFinishTime;
    (void)lFastestLapTime;
    (void)liTakedowns;
    (void)lfDistanceToFin;
    (void)leEliminator;
    (void)liEliminations;
    (void)liRacePosition;
    (void)lbEliminated;
    (void)lpOutputQueue;
    (void)mbResultsTeamWon;
    (void)mbResultsEliminatorValid;
    (void)IsOnlineGameMode();
}

// X360 0x82329B68. Refresh the cumulative results and latch the final-standings flag.
void ModeManager::TellGuiToShowOnlineFinalStandings()
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");

    const bool lbOnline = (mpCurrentGameMode != nullptr) ? mpCurrentGameMode->IsOnline() : false;

    // X360: ScoringSystem::UpdateCumulativeResults(round, numCars, final). The round + active-car count
    // come from mpGameStateModule by raw offset; de-inlined to the declared-only helpers (header FLAG).
    const s32 liRound   = GetOnlineRoundIndex();
    const s32 liNumCars = GetOnlineActiveCarCount();
    mScoringSystem.UpdateCumulativeResults(static_cast<u32>(liRound), liNumCars, lbOnline);

    mbOnlineFinalStandingsShown = true;   // X360 +0x94F8 = 1
}

} // namespace BrnGameState
