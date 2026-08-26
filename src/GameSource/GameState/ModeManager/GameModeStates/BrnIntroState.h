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
    // [!] CONSTRUCTOR DECLARATION REMOVED 2026-08-26 (states-blob unpark round). `IntroState();` was
    // declared here and defined NOWHERE in the tree -- harmless only for as long as nothing
    // instantiated an IntroState. GameMode now embeds one BY VALUE (console +0x40, GameMode::Construct
    // 0x8232F9D8), so GameMode::GameMode() odr-uses it and the declaration alone would be an
    // unresolved external at link time. The console has no IntroState constructor either: the DecFIGS
    // DWARF (BrnIntroState.h:45) lists it in the same implicit/compiler-generated form it lists
    // ResultsState / QuitState / InProgressState / OnlineLoadingState in -- and NONE of those four
    // committed headers declares one. mfCountdownSeconds / mbUseCountdown are seeded by OnEnter, which
    // is where the console seeds them too. The implicit default constructor is therefore both correct
    // and console-faithful; declaring it bought nothing.

    virtual void OnEnter();
    virtual void Update();
    virtual void OnLeave();

private:
    f32  mfCountdownSeconds; // remaining intro-countdown time, in seconds
    bool mbUseCountdown;     // true when this mode actually times its intro (vs. instant)
};
}
