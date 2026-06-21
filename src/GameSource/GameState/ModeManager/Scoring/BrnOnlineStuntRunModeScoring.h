#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnOnlineStuntRunModeScoring.h
// ============================================================================
// Online STUNT-RUN (free-for-all team-stunt) mode scorer. Derives from BaseOnlineModeScoring and
// overrides the scoring lifecycle. This is the FOURTH online sub-scorer the ScoringSystem keystone
// reaches through WriteDataToOutput / StartOnlineGameModeScoring (alongside OnlineRaceModeScoring,
// OnlineRoadRageModeScoring and OnlineBurningHomeRunModeScoring). No Feb-2007 partial source and NO DecFIGS
// DWARF exists for this TU, so the layout below is recovered purely from the X360 pseudocode of its
// six functions (Compare/CompareTeams/GetNumPlayersOnTeam/GetTeamScore/GetWinnerTeam/
// UpdatePlayerPoints @ 0x823159C8 / 0x82315A08 / 0x82321DB0 / 0x82321D20 / 0x8232F950 / 0x82338B90).
//
// LAYOUT: every one of the six X360 bodies touches only (a) BASE members through the inherited
// SetPlayerPosition, and (b) the ScoringSystem / CarData record returned by ScoringSystem::GetCarData.
// None of them reads or writes a data member that would live ON this derived class. So -- exactly like
// the sibling OnlineRaceModeScoring / OnlineBurningHomeRunModeScoring scorers -- this class adds NO
// data members of its own; its object layout equals the base. NOMINAL un-accessed storage: none is
// required (no own instance state is reached), so none is reserved. When this scorer's full TU lands
// it GROWS this home additively if the wider build proves it carries private state.
//
// MINIMAL SLICE: of the lifecycle virtuals only UpdatePlayerPoints is reconstructed by this TU; the
// remaining lifecycle virtuals (Construct/Prepare/Release/Destruct/ClearData/Update/WriteDataToOutput)
// are declared-only so the vtable slot order matches the base/siblings -- their bodies land with the
// rest of this class's TU. The two file-private qsort comparators (Compare / CompareTeams) and the two
// public team-tally helpers (GetNumPlayersOnTeam / GetTeamScore / GetWinnerTeam) the keystone and this
// TU call are declared here.

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                                       // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h" // base class + KI_MAX_ACTIVE_RACE_CARS + fwd-decl `class ScoringSystem`
#include "GameSource/GameState/BrnGameStateSharedIO.h"                         // OnlineScoringOutputInterface

// This header intentionally does NOT include BrnScoringSystem.h. The keystone embeds this scorer BY
// VALUE, so it must see this full class definition; if this header pulled the keystone back in, the
// `#pragma once` guard would leave one of the two types incomplete at the embed point (mutual-include
// cycle). ScoringSystem is named only BY POINTER in the methods below (the base header forward-
// declares it); CarData / CarScoreData are touched only in the .cpp, which includes the keystone.

namespace BrnGameState
{
// Number of online stunt-run "teams". In the X360 stunt-run mode each active player races on their
// own team (free-for-all), so the team index ranges over the per-player slots and the bodies bound
// their team loops at 9 (X360 asm: `< 9`, assert "leEnumIndex <= E_PLAYER_TEAM_COUNT" from
// ..\GameSource\GameState\BrnGameStateSharedEnums.h:112). The committed
// GameStateModuleIO::EPlayerTeam enum (BrnGameStateSharedIO.h) is the 3-value red/blue/none team
// selector of the *team* online modes -- a DIFFERENT, drifted enum -- so the stunt-run bound is
// modelled file-locally here rather than retyping that committed enum (same file-visible-constant
// precedent as KI_MAX_ACTIVE_RACE_CARS in the base header).
const s32 KI_MAX_PLAYER_TEAMS = 9;

class OnlineStuntRunModeScoring : public BaseOnlineModeScoring
{
public:
    // ---- virtual lifecycle (declared-only here; bodies belong to the wider class TU) ----
    // Override ORDER mirrors the base/sibling vtable so the slots line up in this TU's relative order.
    virtual void Construct();
    virtual bool Prepare();
    virtual bool Release();
    virtual void Destruct();
    virtual void ClearData();
    virtual void Update(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // X360 @ 0x82338B90 (BrnOnlineStuntRunModeScoring.cpp:163). Builds one sortable row per active
    // race car (index + per-car stunt score + winning-team flag + disconnected flag), sorts the cars
    // by score (disconnected last) and the contesting teams by team-stunt-score, derives a per-team
    // standings rank, then writes each car its online-points (its team's rank, or 0 if disconnected)
    // and its finishing position back into the scoring system + the base position table.
    // BODIED in BrnOnlineStuntRunModeScoring.cpp. Reads the per-car online stunt score through
    // CarScoreData::GetOnlineStuntScore() (+0xD4, now carved into the CarScoreData home). VTABLE
    // override of base slot 6 (BaseOnlineModeScoring::UpdatePlayerPoints); signature matches the base
    // exactly (ScoringSystem*, s32). The X360 leading `_DWORD* result` is the unused return-register
    // artifact -- the function is void.
    virtual void UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // X360 BrnOnlineStuntRunModeScoring.cpp WriteDataToOutput. Declared-only.
    virtual void WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface* lpOutput);

    // ---- team-tally helpers (public non-virtual; the keystone + this TU call them) ----

    // X360 @ 0x82321DB0. Counts how many active race cars are currently assigned to liTeam.
    // Reconstructed (self-contained): walks the per-car records via ScoringSystem::GetCarData and
    // tallies those whose CarData::GetTeam() == liTeam.
    s32 GetNumPlayersOnTeam(s32 liTeam, const ScoringSystem* lpScoringSystem) const;

    // X360 @ 0x82321D20. Sums the per-car online stunt scores of every active race car on liTeam.
    // The ScoringSystem keystone's WriteDataToOutput calls this, as do GetWinnerTeam and
    // UpdatePlayerPoints. BODIED in BrnOnlineStuntRunModeScoring.cpp: the accumulator
    // `v5 += *(CarData + 212)` reads CarScoreData +0xD4 via CarScoreData::GetOnlineStuntScore().
    s32 GetTeamScore(s32 liTeam, const ScoringSystem* lpScoringSystem) const;

    // X360 @ 0x8232F950 (Hex-Rays truncated the name to "GetWinnerTea"). Returns the team with the
    // highest team stunt-score (the first team reaching the running maximum; 0 if no team scores).
    // Reconstructed (self-contained): scans GetTeamScore across the team range and keeps the leader.
    s32 GetWinnerTeam(const ScoringSystem* lpScoringSystem) const;

private:
    // ---- file-local qsort element records (recovered from the X360 strides + comparator offsets) ----

    // 12-byte stride row, one per active race car. X360 UpdatePlayerPoints builds an array of these on
    // the stack and sorts it with `Compare`. The element size is pinned by qsort(..., a3, 12, Compare)
    // and the comparator's byte offsets (+4 score, +9 disconnected flag).
    struct PlayerSortData
    {
        s32  miRaceCarIndex;     // +0x00 (4)  active-race-car slot index
        s32  miScore;            // +0x04 (4)  per-car online stunt score (CarScoreData +0xD4)
        bool mbOnWinningTeam;    // +0x08 (1)  set when CarData::GetTeam() == the winning team
        bool mbDisconnected;     // +0x09 (1)  CarScoreData::GetDisconnected() (CarData +0x69)
        // +0x0A..+0x0C tail pad to the X360-proven 12-byte stride
        u8   maPad0A[2];         // +0x0A (2)
    };

    // 8-byte stride row, one per contesting team. X360 UpdatePlayerPoints builds an array of these and
    // sorts it with `CompareTeams`. Element size pinned by qsort(..., v10, 8, CompareTeams) and the
    // comparator's `*a1 <= *a2` test on the +0x00 word.
    struct TeamSortData
    {
        s32 miTeamScore;         // +0x00 (4)  GetTeamScore for the team
        s32 miTeam;              // +0x04 (4)  team index
    };

    // X360 @ 0x823159C8. qsort comparator over PlayerSortData. Ranks (best first): connected before
    // disconnected, then higher miScore first. The X360 build emits it as a non-static member (no
    // `this` use); reconstructed `static` so it has the (const void*, const void*) free-function shape
    // qsort requires. Returns -1 (lpPlayer1 ranks ahead) or +1.
    static int Compare(const void* lpPlayer1, const void* lpPlayer2);

    // X360 @ 0x82315A08. qsort comparator over TeamSortData. Ranks higher miTeamScore first.
    // Reconstructed `static` for the same reason as Compare. Returns -1 or +1.
    static int CompareTeams(const void* lpTeam1, const void* lpTeam2);
};
}
