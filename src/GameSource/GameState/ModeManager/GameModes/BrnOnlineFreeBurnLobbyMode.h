#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineFreeBurnLobbyMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// OnlineFreeBurnLobbyMode's remaining members/methods belong to the BrnOnlineFreeBurnLobbyMode.cpp TU.
//
// NOT-YET-RECONSTRUCTED OVERRIDES (vtable 0x820D0A68, checked 2026-08-26). This is the mode IDA
// names the most slots on, and every one of them lands on its DWARF-predicted index -- which is
// what makes it the third independent witness for the 26-slot base map: slot 0 Construct
// (0x82339450), slot 2 PreWorldUpdate (0x82350300), slot 3 PostWorldUpdate (0x82340F40), slot 12
// SendEvent (0x823315B8), slot 19 PlayerHasSpawned (0x823315A8), slot 20 ProcessNewRoadScore
// (0x8234CF98), slot 21 OnEnterRoad (0x82331700). None of those has a body in the tree, so none is
// declared here -- a declaration with no definition is an unresolved external once the vtable is
// emitted.
class OnlineFreeBurnLobbyMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6, X360 0x827E25E0

    // X360 0x82322338. Builds the online free-burn-lobby GameModeParams + copies the network ids.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);                // slot 5

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`); the GameMode base is 0x82C296C8
    // (`li r3,1`). DWARF BrnOnlineFreeBurnLobbyMode.h:48 declares this override; ADDED 2026-08-26
    // with the 26-slot base.
    virtual bool RequiresStreaming() const;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (OnlineFreeBurnLobbyMode::*)() const>(&OnlineFreeBurnLobbyMode::GetName)) != 0,
              "OnlineFreeBurnLobbyMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<void (OnlineFreeBurnLobbyMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&OnlineFreeBurnLobbyMode::Start)) != 0,
              "OnlineFreeBurnLobbyMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<bool (OnlineFreeBurnLobbyMode::*)() const>(&OnlineFreeBurnLobbyMode::RequiresStreaming)) != 0,
              "OnlineFreeBurnLobbyMode::RequiresStreaming must bind GameMode vtable slot 23");
}
