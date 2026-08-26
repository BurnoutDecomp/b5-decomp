#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOnlineGameMode.h"

namespace BrnGameState
{
// OnlineRaceMode is a concrete game mode. The base types (OnlineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. GetName is owned by the GetName-only sibling TU
// (BrnOnlineRaceMode GetName); this header additionally declares the virtuals owned by the
// BrnOnlineRaceMode.cpp TU as overrides of the GameMode base slots.
//
// NOT-YET-RECONSTRUCTED OVERRIDE (vtable 0x820D07F0, checked 2026-08-26): OnlineRaceMode also
// overrides slot 5 Start (0x82338EB8). No body exists in the tree, so it is not declared here.
class OnlineRaceMode : public OnlineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6, X360 0x827E2590

    // X360: BrnGameState::OnlineRaceMode::PreWorldUpdate (0x82330EB0). Overrides GameMode
    // vtable slot 2 (vtbl+8). Per-frame: runs the base tick, then -- while the race is in
    // progress -- recomputes the post-finish time limit from the winner's finish time.
    //
    // SIGNATURE WIDENED 2026-08-26 (wave-B fix round). It was declared no-arg, matching the
    // committed base; the console base takes SIX arguments (UpdateCurrentMode @0x82350EC8
    // dispatches `(*(**(a1+3480)+8))(mode, a2, a3, a8, a28, a30, a1+3504)`), so the no-arg form
    // would have MINTED A NEW SLOT instead of binding to slot 2 the moment the base was corrected.
    virtual void PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                bool lbPaused,
                                const ScoringSystem* lpScoringSystem);

    // X360: BrnGameState::OnlineRaceMode::GetOutroTimeout (0x82330FD0). Overrides GameMode vtable
    // slot 16 (vtbl+64). Returns the post-race outro hold time for an online race, derived from
    // how far behind the race winner the local player finished.
    virtual f32 GetOutroTimeout() const;

    // Slot 24 (vtbl+96). 0x82C296C8 (`li r3,1; blr`) at slot 24 of vtable 0x820D07F0, against the
    // GameMode base's 0x827E2F38 (`li r3,0`). SetupGameMode @0x8234B158 reads it twice and
    // HandleLoadingScreenLoaded @0x8234B8A8 once. DWARF BrnOnlineRaceMode.h:15 declares this
    // override; ADDED 2026-08-26 with the 26-slot base.
    virtual bool HasLoadingScreen() const;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (OnlineRaceMode::*)() const>(&OnlineRaceMode::GetName)) != 0,
              "OnlineRaceMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<void (OnlineRaceMode::*)(GameStateModuleIO::OutputBuffer*,
                                                          const GameStateModuleIO::PreWorldInputBuffer*,
                                                          const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*,
                                                          const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*,
                                                          bool,
                                                          const ScoringSystem*)>(&OnlineRaceMode::PreWorldUpdate)) != 0,
              "OnlineRaceMode::PreWorldUpdate must bind GameMode vtable slot 2");
static_assert(sizeof(static_cast<f32 (OnlineRaceMode::*)() const>(&OnlineRaceMode::GetOutroTimeout)) != 0,
              "OnlineRaceMode::GetOutroTimeout must bind GameMode vtable slot 16");
static_assert(sizeof(static_cast<bool (OnlineRaceMode::*)() const>(&OnlineRaceMode::HasLoadingScreen)) != 0,
              "OnlineRaceMode::HasLoadingScreen must bind GameMode vtable slot 24");
}
