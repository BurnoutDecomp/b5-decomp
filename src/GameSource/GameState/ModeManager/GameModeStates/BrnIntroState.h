#pragma once

#include "GameSource/GameState/ModeManager/GameModeStates/BrnGameModeState.h"

namespace BrnGameState
{
// The "intro" state of a game mode's state machine: it runs a short countdown timer
// (the pre-event flyby / "3-2-1" lead-in) and, when the timer elapses, advances the mode
// to its next state. Reconstructed from the DecFIGS DWARF (BrnIntroState.h: derives from
// GameModeState; private mfCountdownSeconds / mbUseCountdown) with the bodies recovered
// from the X360 pseudocode (OnEnter 0x823163C8, Update 0x823164A0, OnLeave 0x82316508).
//
// OnEnter / Update / OnLeave override the GameModeState virtuals (same vtable slots, in
// DWARF source order). The X360 ledger attests exactly these three for IntroState.
class IntroState : public GameModeState
{
public:
    IntroState();

    virtual void OnEnter();
    virtual void Update();
    virtual void OnLeave();

private:
    f32  mfCountdownSeconds; // remaining intro-countdown time, in seconds
    bool mbUseCountdown;     // true when this mode actually times its intro (vs. instant)
};
}
