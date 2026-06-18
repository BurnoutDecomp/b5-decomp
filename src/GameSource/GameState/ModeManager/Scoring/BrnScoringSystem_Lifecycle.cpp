#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"

// ============================================================================
// BrnScoringSystem_Lifecycle.cpp
// ============================================================================
// Bodies for the "Lifecycle" group of BrnGameState::ScoringSystem (keystone header
// BrnScoringSystem.h lines 280-303): construct / prepare / release / mode hooks and
// the player roster (AddPlayer / SetPlayerRaceCarIndex / RemovePlayer).
//
// Each body is reconstructed from its X360 pseudocode (addresses in the per-method
// comments) and the DecFIGS dwarfdump variable hints, accessing ScoringSystem members
// BY NAME. The X360 search/scan loops walk the per-car records (maCarData[8]); the
// optimizer's pointer-stride form (++ptr by 86 dwords == 344 bytes == sizeof(CarData)
// on X360) is reconstructed back to clean indexed loops over E_PLAYER_SCORING_INDEX_COUNT.
//
// CgsNetwork::K_INVALID_PLAYER_ID has no committed home in the tree; the file-local -1
// (the BrnScoringSystem_Lookup.cpp / FlybyManager precedent) stands in for it.
//
// SCOPE NOTE -- the keystone is a SEMANTIC slice, not a byte-exact X360 layout. The X360
// ScoringSystem also embeds members the keystone deliberately omits (a CgsDev::DebugComponent
// base, a second StuntModeScoring instance, an OnlineGameResults aggregate and the
// OnlineStuntRunModeScoring sub-scorer). Construct/Prepare touch some of those; the bodies
// here reconstruct the operations that map to keystone-named members and delegate the rest to
// the embedded sub-scorers' own lifecycle calls. OnModeStart (dereferences GameModeParams),
// WriteDataToOutput (writes the opaque ScoringOutputInterface slice + calls undeclared
// GetTeamStuntScore/GetLeadingStuntTeam) and SetRivalEliminated (no recoverable body) are
// FLAGGED BLOCKED and left declare-only.
// ============================================================================

namespace BrnGameState
{
namespace
{
    // CgsNetwork::K_INVALID_PLAYER_ID stand-in (no committed home; -1 per existing precedent).
    const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

// ----------------------------------------------------------------------------
// lifecycle / mode hooks
// ----------------------------------------------------------------------------

// X360 0x82337FE0. One-time construction: bring each embedded sub-scorer up (the crash scorer
// clears, the stunt scorer takes the achievement manager, the three online scorers construct),
// reset all scoring state via ClearData(true), clear the disconnected-player set, and empty the
// burnout-skillz tally (every record cleared, every parallel id slot freed to INVALID).
void ScoringSystem::Construct(StuntModeScoring::AchievementManager* lpAchievementManager)
{
    mCrashModeScoring.ClearData();
    mStuntModeScoring.Construct(lpAchievementManager);

    mOnlineRaceModeScoring.Construct();
    mOnlineRoadRageScoring.Construct();
    mOnlineBurningHomeRunScoring.Construct();

    ClearData(true);
    ClearDisconnectedPlayers();

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maBurnoutSkillzData[liSlot].Clear();
        maBurnoutSkillzPlayerIDs[liSlot] = K_INVALID_PLAYER_ID;
    }

    meCurrentMedalTarget   = E_CURRENT_MEDAL_TARGET_TIME_NONE;
    meCurrentMedalAchieved = E_CURRENT_MEDAL_TARGET_TIME_NONE;
}

// X360 0x8232A430. Allocate-time prepare: ready the stunt sub-scorer, then prepare each of the
// eight per-car records against the network heap allocator. Always returns true on the X360.
bool ScoringSystem::Prepare(CgsMemory::HeapMalloc* lpHeapMalloc)
{
    mStuntModeScoring.Prepare();

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maCarData[liSlot].Prepare(lpHeapMalloc);
    }

    return true;
}

// X360 0x823124A0. Release every per-car record (each frees its road-rule high-score table back
// to the network heap). Always returns true on the X360.
bool ScoringSystem::Release()
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maCarData[liSlot].Release();
    }

    return true;
}

// :333. The mode-end hook has no out-of-line work in the recovered build (the DecFIGS dwarfdump
// body is empty); the per-mode teardown lives in the callers.
void ScoringSystem::OnModeEnd(bool /*lbAbort*/)
{
}

// X360 0x8231F140. Per-car cumulative-data reset across all eight slots. The DecFIGS dwarfdump
// SHAPE for this method is exactly the per-car CarData::ClearCumulativeData() sweep.
void ScoringSystem::ClearCumulativeData()
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        maCarData[liSlot].ClearCumulativeData();
    }
}

// ----------------------------------------------------------------------------
// player roster
// ----------------------------------------------------------------------------

// X360 0x8231E288. Claim the first free per-car slot (a slot is free when its stored race-car
// index is the COUNT sentinel), mark it pending (race-car index + network id both INVALID), reset
// its road-rule high scores, and return the slot as the new player's scoring index. Fires the
// "no free slots" assert and returns the COUNT sentinel when the roster is full.
GameStateModuleIO::EPlayerScoringIndex ScoringSystem::AddPlayer()
{
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        if (maCarData[liSlot].GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_COUNT)
        {
            maCarData[liSlot].SetActiveRaceCarIndex(E_ACTIVE_RACE_CAR_INDEX_INVALID);
            maCarData[liSlot].SetNetworkPlayerID(K_INVALID_PLAYER_ID);
            maCarData[liSlot].ResetRoadRulesScores();
            return static_cast<GameStateModuleIO::EPlayerScoringIndex>(liSlot);
        }
    }

    CGS_ASSERT(false, "No more free slots for a player!");
    return GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
}

// X360 sub_8231E340 (the AddPlayer(NetworkPlayerID, EPlayerTeam) overload). Claim a free slot via
// the no-arg AddPlayer(), assert the id is not already registered, then stamp the slot's team,
// round-start team and network id. Returns the new scoring index.
GameStateModuleIO::EPlayerScoringIndex ScoringSystem::AddPlayer(BrnNetwork::NetworkPlayerID lID,
                                                                GameStateModuleIO::EPlayerTeam leTeam)
{
    GameStateModuleIO::EPlayerScoringIndex leScoringIndex = AddPlayer();

    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        CGS_ASSERT((maCarData[liSlot].GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_COUNT) ||
                   (lID != maCarData[liSlot].GetNetworkPlayerID()),
                   "lNetworkPlayerID != maCarData[leCheckPlayerIndex].GetNetworkPlayerID()");
    }

    CarData* lpCarData = GetCarDataFromPlayerScoringIndex(leScoringIndex);
    lpCarData->SetTeam(leTeam);
    lpCarData->SetRoundStartTeam(leTeam);
    lpCarData->SetNetworkPlayerID(lID);

    return leScoringIndex;
}

// X360 0x82310FB0. Bind a scoring slot to its active-race-car index. Asserts the slot is either
// unbound (INVALID) or already bound to the same race car, then stores the index.
void ScoringSystem::SetPlayerRaceCarIndex(GameStateModuleIO::EPlayerScoringIndex leScoringIndex,
                                          EActiveRaceCarIndex leRaceCarIndex)
{
    CarData* lpCarData = GetCarDataFromPlayerScoringIndex(leScoringIndex);

    CGS_ASSERT((lpCarData->GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID) ||
               (lpCarData->GetActiveRaceCarIndex() == leRaceCarIndex),
               "(maCarData[ lePlayerIndex ].GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID) || "
               "(maCarData[ lePlayerIndex ].GetActiveRaceCarIndex() == leActiveRaceCarIndex)");

    lpCarData->SetActiveRaceCarIndex(leRaceCarIndex);
}

// :358. Remove the player occupying a given active-race-car slot: reset the record and drop the
// lead / last bookkeeping if it referenced the removed car. (The X360 keys its single out-of-line
// RemovePlayer on the network id -- see the NetworkPlayerID overload below; this race-car-index
// overload mirrors that reset path through the by-index record lookup.)
void ScoringSystem::RemovePlayer(EActiveRaceCarIndex leRaceCarIndex)
{
    CarData* lpCarData = GetCarData(leRaceCarIndex);
    if (lpCarData != NULL)
    {
        lpCarData->Construct();
        lpCarData->ResetRoadRulesScores();

        EActiveRaceCarIndex leRemovedIndex = lpCarData->GetActiveRaceCarIndex();
        if (leRemovedIndex == meLastRaceCarIndex)
        {
            meLastRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
        if (leRemovedIndex == meLeadRaceCarIndex)
        {
            meLeadRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
    }
}

// :362. No recoverable out-of-line body (no X360 export and an empty DecFIGS dwarfdump entry);
// the rival-eliminated marking is fully inlined into its callers in the recovered build.
// FLAGGED BLOCKED -- left declare-only (no reconstruction source).

// :367. Remove the player with the given network id: look the record up by id, reset it, and drop
// the lead / last bookkeeping if it referenced the removed car. This is the X360 0x8236B0A0 body
// (sub_8231DD88 == GetCarData(NetworkPlayerID)).
void ScoringSystem::RemovePlayer(BrnNetwork::NetworkPlayerID lID)
{
    CarData* lpCarData = GetCarData(lID);
    if (lpCarData != NULL)
    {
        lpCarData->Construct();
        lpCarData->ResetRoadRulesScores();

        EActiveRaceCarIndex leRemovedIndex = lpCarData->GetActiveRaceCarIndex();
        if (leRemovedIndex == meLastRaceCarIndex)
        {
            meLastRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
        if (leRemovedIndex == meLeadRaceCarIndex)
        {
            meLeadRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
        }
    }
}

// :374 / X360 0x8232AE98 -- WriteDataToOutput.
// FLAGGED BLOCKED (re-decoded 2026-06-18 from the X360 ASM @ 0x8232AE98..0x8232B27C, after
// mOnlineGameResults landed by-value on the keystone).
//
// The blockers the previous note listed are now RESOLVED:
//   - GetTeamStuntScore / GetLeadingStuntTeam are declared on the keystone (BrnScoringSystem.h
//     :473/:474), so the per-team and LeadingStuntTeam stunt-score calls resolve.
//   - The two online virtuals dispatched through mpCurrentOnlineModeScoring are now declared on
//     BaseOnlineModeScoring: X360 vtable+0x14 == slot 5 == Update(const ScoringSystem*, s32) and
//     vtable+0x20 == slot 8 == WriteDataToOutput(OnlineScoringOutputInterface*) (BrnBaseOnlineModeScoring.h
//     :54/:57), so mpCurrentOnlineModeScoring->Update(this, idx) / ->WriteDataToOutput(lpOnlineOutput) compile.
//   - OnlineStuntRunModeScoring::GetTeamScore is still only declare-only, but that is fine for `cl /c`.
//   - mOnlineGameResults is now a named member -- and the X360 body @ 0x8232AE98 in fact NEVER reads
//     ss+0x4DC0 (mOnlineGameResults); the keystone-context note that WriteDataToOutput "fills from
//     mOnlineGameResults" is not borne out by the ASM. So that member is not a source here.
//
// TWO genuine blockers remain, BOTH unmodeled members in homes this partial must not grow:
//   (A) The ScoringSystem "player's active-race-car index" source member (X360 ss+0x4EE8). The body
//       reads it three times -- it is stored verbatim into out.mePlayerRaceCarIndex (ASM lwz 0x4EE8 /
//       stw 0xA38(out)), passed as the 2nd arg of the online Update virtual (lwz 0x4EE8 -> r5), and
//       drives the online GetCarData(idx) path that computes the per-team stunt-run team. The
//       SEMANTIC-SLICE keystone has NO named member for it (verified: no mePlayerRaceCarIndex / player-
//       active-race-car field on ScoringSystem). Adding it would mean editing the keystone -- out of scope.
//   (B) The per-team stunt-score OUTPUT array on ScoringOutputInterface (X360 out+0xA10). The body's
//       team loop `for t in [0, E_PLAYER_TEAM_COUNT): out[+0xA10 + 4*t] = GetTeamStuntScore(t)` writes
//       9 s32s (this X360 build's E_PLAYER_TEAM_COUNT == 9, the per-player team slots -- see the assert
//       "leEnumIndex <= E_PLAYER_TEAM_COUNT" with bound 9, and the GetTeamStuntScore note at
//       BrnScoringSystem.h :471-472). The committed ScoringOutputInterface (BrnGameStateSharedIO.h) and
//       its DWARF home BOTH go straight from mabValid[8] (:543) to mePlayerRaceCarIndex (:544) -- there
//       is no per-team stunt-score array member at all. Publishing this loop would require GROWING
//       ScoringOutputInterface, a different home -- out of scope for this partial.
//
// Everything else in the body DOES map onto named members and was fully decoded from the ASM (the
// per-car loop: out.maCarScoreData[i] = *GetCarData(i)const->GetScoreData(), out.maiCumulativeScoreData[i]
// = GetCumulativePoints() [CarData+0x130], out.maCarIds[i] = GetCarID() [CarData+0x128],
// out.mabPlayerEliminated[i] = GetScoreData()->GetEliminated() [CarData+0xD9]; the road-rage block via
// mRoadRageModeScoring.IsActive()/GetNumTakedownsAchieved()/GetTargetNumTakedowns(); the showtime/crash
// scalars; the current/target/combo/stunt + maStunts[0] tail via mStuntModeScoring's named getters +
// OutputStuntsToDisplay). But blockers (A) and (B) gate the player-race-car index, the entire online
// half of the function, and the per-team stunt-score publication -- writing the rest while silently
// dropping those is a semantic regression, not a reconstruction. Left declare-only until ScoringSystem
// gains a player-active-race-car-index member and ScoringOutputInterface gains the per-team stunt-score
// array (both additive grows in their own homes, not here).

}
