#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// PursuitMode is a concrete game mode. The base types (OfflineGameMode, GameMode)
// are #included from their own owning headers rather than forked locally. Only
// GetName is owned by this TU; PursuitMode's remaining members/methods belong to
// the BrnPursuitMode.cpp TU.
//
// NOT-YET-RECONSTRUCTED OVERRIDE (vtable 0x820D0650, checked 2026-08-26): PursuitMode also
// overrides slot 8 GetIntroDurationSeconds (0x827E24E8). No body exists in the tree, so it is not
// declared here and the mode inherits the GameMode base.
class PursuitMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6, X360 0x827E24F8

    // X360 0x823220A0. Builds the pursuit GameModeParams from the StartGameModeParams + rank data.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);                // slot 5

    // Slot 13 (vtbl+52). Folded leaf 0x827E2F38 (`li r3,0; blr`) -- a pursuit never idle-exits.
    // DWARF BrnPursuitMode.h:21 declares this override. ADDED 2026-08-26 with the 26-slot base,
    // because GameMode::ShouldExit is now wired to the real ScoringSystem idle timers.
    virtual bool ShouldExit(const ScoringSystem* lpScoringSystem) const;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (PursuitMode::*)() const>(&PursuitMode::GetName)) != 0,
              "PursuitMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<void (PursuitMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&PursuitMode::Start)) != 0,
              "PursuitMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<bool (PursuitMode::*)(const ScoringSystem*) const>(&PursuitMode::ShouldExit)) != 0,
              "PursuitMode::ShouldExit must bind GameMode vtable slot 13");
}
