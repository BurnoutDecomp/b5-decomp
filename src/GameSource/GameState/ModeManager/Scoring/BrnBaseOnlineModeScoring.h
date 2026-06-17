#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                  // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
#include "GameSource/GameState/BrnGameStateSharedIO.h"    // GameStateModuleIO::EPlayerTeam
#include "GameSource/GameState/BrnGameStateTypes.h"       // BrnGameState::EOnlineAwardID (maOnlineAwards[])
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::NetworkPlayerID (Compare* params)
#include "GameShared/GameClasses/System/Timer/CgsTime.h"  // CgsSystem::Time (Compare* params)

namespace BrnGameState
{
// Forward-only: the scoring system is reached by the derived scorers only through the typed base
// methods declared below (Update/UpdatePlayerTeams take it by pointer), so an incomplete
// declaration suffices for this header. Full layout lands with BrnScoringSystem's own TU.
class ScoringSystem;

// Max players in a network game (== BrnWorld::KI_MAX_ACTIVE_RACE_CARS on this build). Modelled as a
// file-visible constant rather than pulling in the BrnWorld header for a single bound
// (BrnGameModeParams precedent).
const s32 KI_MAX_ACTIVE_RACE_CARS = 8;

// Base scorer for the online game modes (OnlineRaceModeScoring, OnlineRoadRageModeScoring, ...). Root
// polymorphic type (DWARF: vptr at offset 0, no explicit base). MINIMAL SLICE: only the members +
// methods touched by this TU's three reconstructed functions are declared. The award tables, the rest
// of the virtual lifecycle (Construct/Prepare/Release/Update/...) and the ~40 other methods belong to
// the wider BrnBaseOnlineModeScoring TU. Byte offsets are NOT X360-faithful on the x64 PC gate (the
// vptr is 8 bytes); member ORDER + named access are preserved for semantic parity.
//
// DWARF layout (BrnBaseOnlineModeScoring.h:46/193-199):
//   off 0x00  vptr
//   off 0x04  EOnlineAwardID  maOnlineAwards[8]
//   off 0x24  s32             maiOnlineAwardVariables[8]
//   off 0x44  EPlayerTeam     maePlayerTeams[8]      <-- GetCurrentPlayerTeam
//   off 0x64  s32             maiPlayerPositions[8]  <-- Get/SetPlayerPosition
class BaseOnlineModeScoring
{
public:
    // X360 @ 0x823106F8 (BrnBaseOnlineModeScoring.h:341). Finishing position recorded for slot.
    s32 GetPlayerPosition(s32 liRaceCarIndex);

    // X360 @ 0x82314638 (BrnBaseOnlineModeScoring.cpp:1005). Team assigned to the slot. Virtual:
    // derived online-mode scorers override the team-assignment policy.
    virtual GameStateModuleIO::EPlayerTeam GetCurrentPlayerTeam(s32 liRaceCarIndex);

    // X360 @ 0x823219B8 (BrnBaseOnlineModeScoring.cpp:205). Public non-virtual. Assigns/refreshes
    // the per-slot team table from the scoring system. The race-mode Update forwards to this.
    // Declared-only here (body belongs to the BrnBaseOnlineModeScoring TU).
    void UpdatePlayerTeams(const ScoringSystem* lpScoringSystem, s32 liNumberOfCars);

protected:
    // X360 @ 0x82310770 (BrnBaseOnlineModeScoring.h:354). Store the finishing position for the slot.
    void SetPlayerPosition(s32 liRaceCarIndex, s32 liPlayerPosition);

    // Per-field comparison helpers the derived qsort comparators chain together. Each folds one
    // decisive comparison into the running *lpResult, but ONLY while *lpResult is still 0 (so the
    // first non-zero comparison in the chain wins). DWARF-attested arg/return signatures (protected,
    // void -- the IDA "returns a pointer" is the unused return-register artifact). Declared `static`:
    // the X360 bodies (verified) take their first Time/scalar in r3 and never touch `this`, so they
    // are callable from the derived classes' static qsort comparators (same deviation as those
    // comparators being static). Bodies belong to the BrnBaseOnlineModeScoring TU.
    static void CompareDistanceToFinish(f32 lfDistance1, f32 lfDistance2, s32* lpResult);                       // X360 0x823146B0 (.cpp:1023)
    static void CompareNetworkPlayerID(BrnNetwork::NetworkPlayerID lID1, BrnNetwork::NetworkPlayerID lID2,
                                       s32* lpResult);                                                          // X360 0x82314748 (.cpp:1058)
    static void CompareFinishTime(const CgsSystem::Time* lpTime1, const CgsSystem::Time* lpTime2, s32* lpResult); // X360 0x823147C8 (.cpp:1092)
    static void CompareCheckpointsReached(s32 liCheckpoints1, s32 liCheckpoints2, s32* lpResult);               // X360 0x82314928 (.cpp:1129)
    static void CompareTakedowns(s32 liTakedowns1, s32 liTakedowns2, s32* lpResult);                            // X360 0x823149A8 (.cpp:1163)
    static void CompareTimeAsRunner(CgsSystem::Time lTime1, CgsSystem::Time lTime2, s32* lpResult);             // X360 0x82314A28 (.cpp:1231) -- by value per DWARF

    // DWARF BrnBaseOnlineModeScoring.h:193 (this+0x04). Per-slot end-of-event award id. The online
    // scorers reset these to E_ONLINE_AWARD_INVALID in ClearData and copy them to the output. Protected
    // so derived scorers read them directly.
    EOnlineAwardID maOnlineAwards[KI_MAX_ACTIVE_RACE_CARS];

    // DWARF BrnBaseOnlineModeScoring.h:194 (this+0x24). Per-slot award parameter value.
    s32 maiOnlineAwardVariables[KI_MAX_ACTIVE_RACE_CARS];

    // DWARF BrnBaseOnlineModeScoring.h:195 (this+0x44). Read by GetCurrentPlayerTeam and by the
    // derived scorers' ClearData / WriteDataToOutput. Protected.
    GameStateModuleIO::EPlayerTeam maePlayerTeams[KI_MAX_ACTIVE_RACE_CARS];

private:
    // DWARF BrnBaseOnlineModeScoring.h:199 (this+0x64). Written by SetPlayerPosition, read by GetPlayerPosition.
    s32 maiPlayerPositions[KI_MAX_ACTIVE_RACE_CARS];
};
}
