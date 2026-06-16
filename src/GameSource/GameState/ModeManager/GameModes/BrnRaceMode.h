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
// Virtual / const / return-type / vtable order are taken from the DWARF declaration shape,
// not from the Hex-Rays pseudocode (which renders the virtuals as plain functions and drops
// const). DWARF source order is GetName, Start, GetOutroTimeout, so they are declared in that
// order to keep the override slots aligned with the base GameMode vtable.
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
}
