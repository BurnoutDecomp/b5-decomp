#pragma once

#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// CrashMode is the offline Crash/Showtime game mode. The base chain (OfflineGameMode -> GameMode)
// is #included from the owning headers; meCurrentState / SetCurrentState / EGameModeEvent come from
// the GameMode base. Minimal slice (BrnPursuitMode.h precedent): only the SendEvent override this
// TU reconstructs is declared; the mode's other DWARF methods land with their own functions.
//
// NOT-YET-RECONSTRUCTED OVERRIDE (vtable 0x820D0570, checked 2026-08-26): CrashMode also overrides
// slot 6 GetName (0x827E24C8 -> "CrashMode") and slot 8 GetIntroDurationSeconds (0x827E2600). No
// body exists in the tree for either, so they are not declared here (a declaration with no
// definition is an unresolved external as soon as the vtable is emitted) and the mode inherits the
// GameMode base for both -- which is why GameMode::GetName now has a base body at all.
class CrashMode : public OfflineGameMode
{
public:
    virtual void SendEvent(EGameModeEvent leEvent);                    // slot 12, X360 0x82330A58

    // X360 0x82322210. Builds the offline crash/showtime GameModeParams.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);                // slot 5

    // Slot 13 (vtbl+52). Folded leaf 0x827E2F38 (`li r3,0; blr`) -- the crash/showtime mode never
    // idle-exits, so it overrides the base's stationary-timer test with a flat false. The DWARF
    // (BrnCrashMode.h:14) declares this override; ADDED 2026-08-26 with the 26-slot base, because
    // GameMode::ShouldExit is now wired to the real ScoringSystem timers and without this the mode
    // would start exiting itself after 4 s of no input.
    virtual bool ShouldExit(const ScoringSystem* lpScoringSystem) const;

    // Slot 23 (vtbl+92). Folded leaf 0x827E2F38 (`li r3,0; blr`) -- crash junctions are already
    // resident, so SetupGameMode's WaitForStreaming gate is skipped. Base is TRUE; DWARF
    // BrnCrashMode.h:20 declares this override. ADDED 2026-08-26 with the 26-slot base.
    virtual bool RequiresStreaming() const;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<void (CrashMode::*)(EGameModeEvent)>(&CrashMode::SendEvent)) != 0,
              "CrashMode::SendEvent must bind GameMode vtable slot 12");
static_assert(sizeof(static_cast<void (CrashMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&CrashMode::Start)) != 0,
              "CrashMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<bool (CrashMode::*)(const ScoringSystem*) const>(&CrashMode::ShouldExit)) != 0,
              "CrashMode::ShouldExit must bind GameMode vtable slot 13");
static_assert(sizeof(static_cast<bool (CrashMode::*)() const>(&CrashMode::RequiresStreaming)) != 0,
              "CrashMode::RequiresStreaming must bind GameMode vtable slot 23");
}
