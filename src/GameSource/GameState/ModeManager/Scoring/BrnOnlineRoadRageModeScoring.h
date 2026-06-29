#pragma once

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Scoring/BrnOnlineRoadRageModeScoring.h
// ============================================================================
// Online ROAD-RAGE (team-elimination) mode scorer. Derives from BaseOnlineModeScoring and
// overrides the scoring virtuals. DWARF home BrnOnlineRoadRageModeScoring.h:46
// (struct BrnGameState::OnlineRoadRageModeScoring : public BrnGameState::BaseOnlineModeScoring).
//
// This TU's own home: the three private nested sort-data structs and the private worker / qsort
// comparators are now declared in full (their bodies live in BrnOnlineRoadRageModeScoring.cpp).
//   * The base (BaseOnlineModeScoring) is already reconstructed, so we derive from it for real --
//     ScoringSystem reaches the scorer through the base vtable and the public virtuals below.
//   * The DWARF data members it ADDS over the base are named in order (mbRedTeamWon,
//     maiEliminationCount[8], maeBlueTeamFinishTypes[8]); offsets are not X360-faithful on the
//     x64 PC gate (vptr is 8 bytes) but ORDER + named access are preserved for semantic parity.
//   * The nested sort-data structs are modeled with DWARF-authoritative field order + the
//     X360-asm-proven element strides (RedTeamSortData == 16, BlueTeamSortData == 28,
//     TablePositionData == 20), used as the qsort element sizes in UpdatePlayerPoints /
//     CalculatePositionsInResultsTable.
//
// DWARF added-member run (BrnOnlineRoadRageModeScoring.h:144-146), private:
//   bool                              mbRedTeamWon;
//   int32_t                           maiEliminationCount[8];
//   GameStateModuleIO::EBlueTeamFinishType maeBlueTeamFinishTypes[8];

#include "types.hpp"
#include "GameSource/GameState/ModeManager/Scoring/BrnBaseOnlineModeScoring.h" // base + KI_MAX_ACTIVE_RACE_CARS, ScoringSystem (fwd)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                         // GameStateModuleIO::{EBlueTeamFinishType, EPlayerTeam, CarScoreData}, OnlineScoringOutputInterface
#include "GameSource/BurnoutConstants.h"                                       // EActiveRaceCarIndex
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"                    // BrnNetwork::NetworkPlayerID (== s32)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                       // CgsSystem::Time

// This header intentionally does NOT include BrnScoringSystem.h. The keystone embeds this scorer BY
// VALUE, so it must see this full class definition; if this header pulled the keystone back in, the
// `#pragma once` guard would leave one of the two types incomplete at the embed point (mutual-include
// cycle). ScoringSystem / CarData are named only BY POINTER in the virtuals/workers below (the base
// header forward-declares ScoringSystem); CarData is touched only in the .cpp, which includes the
// keystone itself.

namespace BrnGameState
{
class OnlineRoadRageModeScoring : public BaseOnlineModeScoring
{
public:
    // ---- virtual lifecycle (Construct/Prepare/Release/ClearData/UpdatePlayerPoints/WriteDataToOutput
    //      have real bodies in the .cpp; Destruct/Update are declared-only here) ----
    // Override order matches the base/sibling vtable so the slots line up in this TU's relative order.
    virtual void Construct();   // BrnOnlineRoadRageModeScoring.cpp:41  / X360 0x82314E48
    virtual bool Prepare();     //                              .cpp:75 / X360 0x82314B28
    virtual bool Release();     //                              .cpp:90
    virtual void Destruct();    //                              .cpp:58
    virtual void ClearData();   //                              .cpp:102 / X360 0x82314E68

    // X360 BrnOnlineRoadRageModeScoring.cpp:145. DWARF types the ScoringSystem param CONST here.
    // Declared-only (the team-tracking pass body belongs to a later slice).
    virtual void Update(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // X360 BrnOnlineRoadRageModeScoring.cpp:373 / 0x8232EF00. DWARF types the ScoringSystem param
    // NON-const here (Update's is const); honored -- the keystone calls both spellings.
    virtual void UpdatePlayerPoints(ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // X360 BrnOnlineRoadRageModeScoring.cpp:121 / 0x82314EE0. Pushes this scorer's per-slot arrays
    // into the network output interface.
    virtual void WriteDataToOutput(GameStateModuleIO::OnlineScoringOutputInterface* lpOutput);

private:
    // -------------------------------------------------------------------------------------------
    // Private nested sort-data rows (DWARF home BrnOnlineRoadRageModeScoring.h:96/114/135). Field
    // ORDER is DWARF-authoritative; the explicit element strides match the X360 qsort sizes.
    // -------------------------------------------------------------------------------------------

    // DWARF :96. One row per RED-team (chaser) car. X360 qsort element size 16 (UpdatePlayerPoints
    // @0x8232EF00). Offsets pinned by the X360 gather/compare asm: id@+0, index@+4, eliminations@+8,
    // timedOut@+0xC, disconnected@+0xD.
    struct RedTeamSortData
    {
        BrnNetwork::NetworkPlayerID mNetworkPlayerID;   // +0   (-1 == empty/invalid slot)
        EActiveRaceCarIndex         meRaceCarIndex;     // +4
        s32                         miEliminations;     // +8
        bool                        mbTimedOut;         // +0xC
        bool                        mbDisconnected;     // +0xD
        // +0xE..+0x10 tail pad -> 16-byte stride
    };

    // DWARF :114. One row per BLUE-team (runner) car. X360 qsort element size 28. Offsets pinned by
    // the X360 gather/compare asm: id@+0, index@+4, finishTime@+8, distance@+0x10, takedowns@+0x14,
    // eliminated@+0x18, timedOut@+0x19, disconnected@+0x1A.
    struct BlueTeamSortData
    {
        BrnNetwork::NetworkPlayerID mNetworkPlayerID;   // +0   (-1 == empty/invalid slot)
        EActiveRaceCarIndex         meRaceCarIndex;     // +4
        CgsSystem::Time             mFinishTime;        // +8   (miSeconds@+8, mfFraction@+0xC)
        f32                         mfDistanceToFinish; // +0x10
        s32                         miTakedowns;        // +0x14
        bool                        mbEliminated;       // +0x18
        bool                        mbTimedOut;         // +0x19
        bool                        mbDisconnected;     // +0x1A
        // +0x1B tail pad -> 28-byte stride
    };

    // DWARF :135. One sortable results-table row per car. X360 qsort element size 20
    // (CalculatePositionsInResultsTable @0x8232ED70). Offsets pinned by that function's asm:
    // redTeamWon@+0, scoreData@+4, team@+8, id@+0xC, index@+0x10.
    struct TablePositionData
    {
        bool                              mbRedTeamWon;    // +0
        GameStateModuleIO::CarScoreData*  mpScoreData;     // +4
        GameStateModuleIO::EPlayerTeam    mePlayerTeam;    // +8
        BrnNetwork::NetworkPlayerID       mNetworkPlayerID;// +0xC  (-1 == empty/invalid slot)
        EActiveRaceCarIndex               meRaceCarIndex;  // +0x10
        // +0x14 stride
    };

    // ---- private workers (bodies in the .cpp) ----
    // X360 0x823151A8 (.cpp:660). Seed both sort arrays with the empty/invalid sentinel rows.
    void InitialisePlayerArrayData(RedTeamSortData* lpaRedRows, BlueTeamSortData* lpaBlueRows);

    // X360 0x82321C38 (.cpp:684). Tally each car's eliminations into maiEliminationCount[eliminator].
    void CalculateRedTeamEliminations(const ScoringSystem* lpScoringSystem);

    // X360 0x82321B60 (.cpp:616). Classify each blue-team runner's finish into maeBlueTeamFinishTypes.
    void CalculateBlueTeamFinishTypes(const ScoringSystem* lpScoringSystem, BlueTeamSortData* lpaBlueRows,
                                      s32 liNumBlue);

    // X360 0x82321AA0 (.cpp:580). Has the red (chaser) team won this round?
    bool CheckForRedTeamWin(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // X360 0x8232ED70 (.cpp:322). Build + sort the combined results table, assign final positions.
    void CalculatePositionsInResultsTable(ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

    // ---- qsort comparators (X360 emits as non-static private members with no `this` use;
    //      reconstructed static so they are valid C function pointers for qsort). Return -1/0/+1. ----
    static int _RedTeamPlayerCompare(const void* lpPlayer1, const void* lpPlayer2);     // X360 0x82315258 (.cpp:717)
    static int _BlueTeamPlayerCompare(const void* lpPlayer1, const void* lpPlayer2);    // X360 0x82315338 (.cpp:767)
    static int _CompareForTablePosition(const void* lpPlayer1, const void* lpPlayer2);  // X360 0x82314FA0 (.cpp:160)

    // ---- data members (DWARF added-member run, BrnOnlineRoadRageModeScoring.h:144-146) ----
    // DWARF :144. Set when the red (chaser) team meets the win condition.
    bool mbRedTeamWon;

    // DWARF :145. Per-slot running elimination tally for the red team.
    s32 maiEliminationCount[KI_MAX_ACTIVE_RACE_CARS];

    // DWARF :146. Per-slot finish classification copied to the output.
    GameStateModuleIO::EBlueTeamFinishType maeBlueTeamFinishTypes[KI_MAX_ACTIVE_RACE_CARS];
};
}
