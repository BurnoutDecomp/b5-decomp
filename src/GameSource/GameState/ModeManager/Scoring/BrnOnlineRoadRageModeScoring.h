#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnOnlineRoadRageModeScoring.h
// ============================================================================
// Online ROAD-RAGE (team-elimination) mode scorer. Derives from BaseOnlineModeScoring and
// overrides the scoring virtuals. DWARF home BrnOnlineRoadRageModeScoring.h:46
// (struct BrnGameState::OnlineRoadRageModeScoring : public BrnGameState::BaseOnlineModeScoring).
//
// MINIMAL SLICE -- this is a compilable placeholder so the ScoringSystem keystone can embed the
// type BY VALUE and call its public virtuals. The real layout/bodies land with this type's own TU.
//   * The base (BaseOnlineModeScoring) is already reconstructed, so we derive from it for real --
//     ScoringSystem reaches the scorer through the base vtable and the public virtuals below.
//   * The DWARF data members it ADDS over the base are named in order (mbRedTeamWon,
//     maiEliminationCount[8], maeBlueTeamFinishTypes[8]); offsets are not X360-faithful on the
//     x64 PC gate (vptr is 8 bytes) but ORDER + named access are preserved for semantic parity.
//   * The three nested sort-data structs (RedTeamSortData / BlueTeamSortData / TablePositionData)
//     and the private worker/qsort helpers are PRIVATE INTERNALS of this scorer's own TU -- the
//     keystone never names them -- so they are deferred (collapsed away) rather than modelled here.
//     That keeps this header self-contained: it pulls in no not-yet-reconstructed types
//     (RoadRulesRecvData::NetworkPlayerID, CarScoreData*, etc.). They reappear when this TU lands.
//
// DWARF added-member run (BrnOnlineRoadRageModeScoring.h:144-146), private:
//   bool                              mbRedTeamWon;
//   int32_t                           maiEliminationCount[8];
//   GameStateModuleIO::EBlueTeamFinishType maeBlueTeamFinishTypes[8];

#include "types.hpp"
#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h" // base + KI_MAX_ACTIVE_RACE_CARS, ScoringSystem (fwd)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                         // GameStateModuleIO::EBlueTeamFinishType, OnlineScoringOutputInterface

namespace BrnGameState
{
class OnlineRoadRageModeScoring : public BaseOnlineModeScoring
{
public:
    // ---- virtual lifecycle (declared-only; bodies belong to this scorer's own TU) ----
    // Override order matches the base/sibling vtable so the slots line up in this TU's relative order.
    virtual void Construct();   // BrnOnlineRoadRageModeScoring.cpp:41
    virtual bool Prepare();     //                              .cpp:75
    virtual bool Release();     //                              .cpp:90
    virtual void Destruct();    //                              .cpp:58
    virtual void ClearData();   //                              .cpp:102

    // X360 BrnOnlineRoadRageModeScoring.cpp:145. DWARF types the ScoringSystem param CONST here.
    virtual void Update(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // X360 BrnOnlineRoadRageModeScoring.cpp:373. DWARF types the ScoringSystem param NON-const here
    // (Update's is const); honored -- the keystone calls both spellings.
    virtual void UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // X360 BrnOnlineRoadRageModeScoring.cpp:121. declared-only.
    virtual void WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface* lpOutput);

private:
    // DWARF BrnOnlineRoadRageModeScoring.h:144. Set when the red (chaser) team meets the win condition.
    bool mbRedTeamWon;

    // DWARF BrnOnlineRoadRageModeScoring.h:145. Per-slot running elimination tally for the red team.
    s32 maiEliminationCount[KI_MAX_ACTIVE_RACE_CARS];

    // DWARF BrnOnlineRoadRageModeScoring.h:146. Per-slot finish classification copied to the output.
    GameStateModuleIO::EBlueTeamFinishType maeBlueTeamFinishTypes[KI_MAX_ACTIVE_RACE_CARS];

    // NOMINAL reserved tail. The three private nested sort-data structs and the private workers /
    // qsort comparators (InitialisePlayerArrayData, CalculateRedTeamEliminations,
    // CalculateBlueTeamFinishTypes, CheckForRedTeamWin, CalculatePositionsInResultsTable,
    // _RedTeamPlayerCompare, _BlueTeamPlayerCompare, _CompareForTablePosition) add no further
    // INSTANCE data over the members above -- they are pure code -- so no extra storage is reserved.
    // This comment marks where this scorer's own TU GROWS the home when it lands.
};
}
