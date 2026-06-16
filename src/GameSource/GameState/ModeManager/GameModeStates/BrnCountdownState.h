#pragma once

#include "types.hpp"

#include "GameSource/GameState/ModeManager/GameModeStates/BrnGameModeState.h"

namespace BrnGameState
{
// The pre-race countdown state of the game-mode state machine. Reconstructed from the
// DecFIGS DWARF (GameSource/GameState/ModeManager/GameModeStates/BrnCountdownState.h:46)
// with each declaration gated on the X360 ledger: OnEnter (0x82332D70), Update
// (0x82316518) and OnLeave (0x823165F0) are all X360-attested.
//
// Behaviour: on entry it seeds mfCountdownSeconds from the mode's per-mode countdown
// time (ModeManager::GetCountdownTimeForMode) and resets the displayed value. Update
// counts the timer down by the frame time step each tick, pushes the ceil'd whole-second
// value to the GameMode for the HUD, and -- once the timer hits zero and the mode agrees
// the countdown should end -- fires the E_GME_NEXT event to advance the state machine.
//
// LAYOUT (X360-proven): the two data members sit at this+12 (mfCountdownSeconds) and
// this+16 (miCountdownDisplay), directly after the GameModeState base (vptr/mpModeManager/
// mpGameMode at +0/+4/+8). Accessed by name, never by raw offset.
class CountdownState : public GameModeState
{
public:
    virtual void OnEnter();
    virtual void Update();
    virtual void OnLeave();

private:
    f32 mfCountdownSeconds;   // seconds remaining on the countdown timer
    s32 miCountdownDisplay;   // last whole-second value pushed to the HUD (-1 == none yet)
};
}
