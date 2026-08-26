#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"

namespace BrnGameState
{
// SurvivorMode is a concrete offline game mode. Bases (OfflineGameMode -> GameMode) are #included
// from their owning headers, never forked. Only GetName and GetOutroTimeout are owned by this
// TU (DWARF-attested); the rest of the mode belongs to BrnSurvivor.cpp.
//
// NOT-YET-RECONSTRUCTED OVERRIDES (vtable 0x820D0788, checked 2026-08-26; the image and the DWARF
// agree): SurvivorMode also overrides slot 2 PreWorldUpdate (0x8234D188), slot 5 Start
// (0x823322B8), slot 10 OnPlayerInShortCut (0x82316398), slot 13 ShouldExit (0x82316318), slot 15
// FillInGameModeSpecificResults (0x823163A8) and slot 25 OnPlayerUsesPaintShop (0x827E2580). None
// has a body in this tree, so none is declared here.
//
// [!] BEHAVIOUR NOTE FOR SLOT 13. Survivor's ShouldExit @0x82316318 is NOT a folded `return false`
// leaf like Crash/RoadRage/Pursuit/StuntAttack: it is a real body that returns TRUE when either
// idle timer is below 3.0f, and that also reads a SurvivorMode field at this+0xD0 (compared
// against flt_820054D0) which this class does not model yet. Because GameMode::ShouldExit is now
// wired to the real ScoringSystem timers, SurvivorMode currently inherits the BASE predicate --
// an approximation, not the console body. It is strictly closer than the previous state (where
// the base was hard-coded inert for every mode), but it is a divergence and belongs on the
// re-wire list together with the +0xD0 member.
class SurvivorMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;                               // slot 6,  X360 0x827E2560
    virtual f32         GetOutroTimeout() const;                       // slot 16, X360 0x827E2570

private:
    // DWARF: BrnSurvivor.cpp:27. The mode's fixed outro timeout; the X360 GetOutroTimeout body
    // returns 0.0, so this constant is 0.0f for this build (same shape as RaceMode).
    static const f32 KF_OUTRO_TIME_SECONDS;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (SurvivorMode::*)() const>(&SurvivorMode::GetName)) != 0,
              "SurvivorMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<f32 (SurvivorMode::*)() const>(&SurvivorMode::GetOutroTimeout)) != 0,
              "SurvivorMode::GetOutroTimeout must bind GameMode vtable slot 16");
}
