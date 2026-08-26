#pragma once

#include "types.hpp"
#include "GameSource/GameState/ModeManager/GameModes/BrnOfflineGameMode.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"

namespace BrnGameState
{
// RaceMode is a concrete offline game mode (the standard point-to-point / circuit race).
// The base types (OfflineGameMode -> GameMode) are #included from their own owning headers
// rather than forked locally. Reconstructed from the DecFIGS DWARF (BrnRaceMode.h:48) with
// each declaration GATED on the X360 ledger.
//
// X360-attested members of this TU:
//   GetName          @ 0x827E2488  -> const char* GetName() const            (returns "Race")
//   GetOutroTimeout  @ 0x827E2498  -> f32         GetOutroTimeout() const     (returns 0.0)
//   Start            @ 0x82330018  -> void        Start(...)                  (mode set-up)
//
// The DWARF also declares HasTimedIntro() and a default constructor for RaceMode, but neither
// appears in the X360 ledger for this TU, so they are deliberately left out (see "DWARF
// supplies names; the X360 ledger decides what exists" in AGENTS.md). They self-add if/when
// the X360 build is later shown to attest them.
//
// Virtual / const / return-type are taken from the DWARF declaration shape, not from the Hex-Rays
// pseudocode (which renders the virtuals as plain functions and drops const).
//
// SLOT NOTE (2026-08-26): declaration ORDER in a derived class does not choose a slot -- the
// SIGNATURE does, by matching a base declaration. RaceMode's vtable is 0x820D0498 and it overrides
// exactly three of GameMode's 26 slots: 6 GetName (0x827E2488 -> "Race"), 5 Start (0x82330018) and
// 16 GetOutroTimeout (0x827E2498 -> [0x820211EC] == 0.0f). Everything else it inherits, including
// slot 9 HasTimedIntro -- the DWARF declares that override but it folded onto the same
// `li r3,1; blr` leaf (0x82C296C8) as the base, so it is invisible in the image and is not
// re-declared here. The tripwire block after the class is what proves the three DO bind.
class RaceMode : public OfflineGameMode
{
public:
    virtual const char* GetName() const;

    // X360 Start(this, StartGameModeParams*, GameModeParams*) -- the third DWARF parameter
    // (ScoringSystem*) is unused by the body and dropped by Hex-Rays; it is kept in the
    // declaration to match the DWARF shape. Builds the mutable GameModeParams the mode runs
    // with from the immutable StartGameModeParams + its rank/event data.
    virtual void Start(const StartGameModeParams* lpStartGameModeParams,
                       GameModeParams* lpGameModeParams,
                       ScoringSystem* lpScoringSystem);

    virtual f32 GetOutroTimeout() const;

private:
    // BrnRaceMode.h:73-74 (DWARF). Written at the end of Start(); read elsewhere in the mode.
    s32 miNumRivalsInRace;
    f32 mfNearestPlayerDistToFinish;

    // BrnRaceMode.cpp:27 (DWARF) -- the race mode's fixed outro timeout. The X360
    // GetOutroTimeout body returns 0.0, i.e. this constant is 0.0f for the X360 build.
    static const f32 KF_OUTRO_TIME_SECONDS;
};

// ---- VTABLE-BINDING TRIPWIRE (see the explanation in BrnOfflineGameMode.h) ----------------------
static_assert(sizeof(static_cast<const char* (RaceMode::*)() const>(&RaceMode::GetName)) != 0,
              "RaceMode::GetName must bind GameMode vtable slot 6");
static_assert(sizeof(static_cast<void (RaceMode::*)(const StartGameModeParams*, GameModeParams*, ScoringSystem*)>(&RaceMode::Start)) != 0,
              "RaceMode::Start must bind GameMode vtable slot 5");
static_assert(sizeof(static_cast<f32 (RaceMode::*)() const>(&RaceMode::GetOutroTimeout)) != 0,
              "RaceMode::GetOutroTimeout must bind GameMode vtable slot 16");
}
