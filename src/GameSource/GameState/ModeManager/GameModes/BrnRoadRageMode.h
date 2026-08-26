#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// RoadRageMode is a concrete game mode. The base types (OfflineGameMode -> GameMode) are #included from
// their own owning headers rather than forked locally. Only GetName is owned by this TU;
// RoadRageMode's remaining members/methods belong to the BrnRoadRageMode.cpp TU.
//
// NOT-YET-RECONSTRUCTED OVERRIDES (vtable 0x820D05E8, checked 2026-08-26 against the image and the
// DWARF, which agree exactly): RoadRageMode also overrides slot 2 PreWorldUpdate (0x823448C0),
// slot 5 Start (0x82330678), slot 10 OnPlayerInShortCut (0x823160A0), slot 12 SendEvent
// (0x82330A38), slot 14 ShouldFinish (0x82315D60), slot 15 FillInGameModeSpecificResults
// (0x82315D40) and slot 22 HandleGameEvents (0x82315FF8). None of those has a body in this tree,
// so none is declared here -- a declaration with no definition is an unresolved external the
// moment the vtable is emitted. The two that ARE declared below are the two whose console bodies
// are recoverable as one-line folded leaves.
class RoadRageMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6, X360 0x827E24D8

    // Slot 13 (vtbl+52). Folded leaf 0x827E2F38 (`li r3,0; blr`) -- road rage never idle-exits.
    // DWARF BrnRoadRageMode.h:79 declares this override. ADDED 2026-08-26 with the 26-slot base,
    // because GameMode::ShouldExit is now wired to the real ScoringSystem idle timers and without
    // this the mode would start exiting itself after 4 s of no input.
    virtual bool ShouldExit(const ScoringSystem* lpScoringSystem) const;

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`); the base is 0x82C296C8
    // (`li r3,1`). DWARF BrnRoadRageMode.h:109 declares this override. SetupGameMode @0x8234B158
    // gates the streaming wait on it.
    virtual bool RequiresStreaming() const;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (RoadRageMode::*)() const>(&RoadRageMode::GetName)) != 0,
              "RoadRageMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<bool (RoadRageMode::*)(const ScoringSystem*) const>(&RoadRageMode::ShouldExit)) != 0,
              "RoadRageMode::ShouldExit must bind GameMode vtable slot 13");
static_assert(sizeof(static_cast<bool (RoadRageMode::*)() const>(&RoadRageMode::RequiresStreaming)) != 0,
              "RoadRageMode::RequiresStreaming must bind GameMode vtable slot 23");
}
