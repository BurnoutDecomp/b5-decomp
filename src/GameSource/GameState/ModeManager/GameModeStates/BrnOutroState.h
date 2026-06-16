#pragma once

#include "types.hpp"

#include "GameSource/GameState/ModeManager/GameModeStates/BrnGameModeState.h"

namespace BrnGameState
{
// The "outro" sub-state of a game mode: a short fixed countdown that runs after the mode
// has finished, before control passes on to the results/next phase. Reconstructed from
// the DecFIGS DWARF (BrnOutroState.h / BrnOutroState.cpp) and the X360 pseudocode for the
// two attested functions (OnEnter @0x82316610, Update @0x82316650).
//
// Derives from GameModeState (DWARF BrnOutroState.h:49). OnEnter and Update are the only
// methods the X360 build attests for this class; they override the base virtuals in
// slots 0 and 2 (OnLeave, slot 1, is not overridden -- the base default applies).
struct OutroState : public GameModeState
{
    // Outro lifecycle hooks (override GameModeState).
    virtual void OnEnter();
    virtual void Update();

private:
    // Seconds remaining in the outro countdown. Seeded in OnEnter from the owning mode's
    // GetOutroTimeout(), counted down each Update by the frame time-step. (DWARF
    // BrnOutroState.h:63.)
    f32 mfOutroSeconds;
};

// Default outro length, in seconds, used when a mode does not override GetOutroTimeout().
// DWARF BrnOutroState.h:59 (extern const, file-scope). Defined in BrnOutroState.cpp.
extern const f32 KF_OUTRO_TIME_SECONDS;
}
