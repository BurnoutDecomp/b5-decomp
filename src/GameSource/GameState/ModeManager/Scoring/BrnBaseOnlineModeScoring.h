#pragma once

#include "types.hpp"
#include "GameSource/BurnoutConstants.h"                  // EActiveRaceCarIndex, E_ACTIVE_RACE_CAR_INDEX_COUNT (== 8)
#include "GameSource/GameState/BrnGameStateSharedIO.h"    // GameStateModuleIO::EPlayerTeam

namespace BrnGameState
{
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

protected:
    // X360 @ 0x82310770 (BrnBaseOnlineModeScoring.h:354). Store the finishing position for the slot.
    void SetPlayerPosition(s32 liRaceCarIndex, s32 liPlayerPosition);

private:
    // Opaque stand-in for the two 8-element scoring arrays the DWARF places before maePlayerTeams
    // (maOnlineAwards[8], maiOnlineAwardVariables[8]); none touched by this TU's functions. Sized for
    // relative ordering only -- replace with the real members when their accessors are reconstructed.
    u8 maPrecedingScoringBlob[2 * 8 * sizeof(s32)];

    // DWARF BrnBaseOnlineModeScoring.h (word 17). Read by GetCurrentPlayerTeam.
    GameStateModuleIO::EPlayerTeam maePlayerTeams[KI_MAX_ACTIVE_RACE_CARS];

    // DWARF BrnBaseOnlineModeScoring.h:199 (word 25). Written by SetPlayerPosition, read by GetPlayerPosition.
    s32 maiPlayerPositions[KI_MAX_ACTIVE_RACE_CARS];
};
}
