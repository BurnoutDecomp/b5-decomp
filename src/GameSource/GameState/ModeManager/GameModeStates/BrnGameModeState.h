#pragma once

#include "types.hpp"

namespace BrnGameState
{
// Forward declarations: the base holds a ModeManager* / GameMode* only by pointer, so
// the full layouts are not needed here. Both have their own owning headers
// (GameSource/GameState/ModeManager/BrnModeManager.h, GameModes/BrnGameMode.h)
// #included by the concrete state .cpp files when they actually call into them.
class ModeManager;
class GameMode;

// MERGED OWNING HEADER for the game-mode state-machine state base (consolidated from the
// GameMode / CountdownState / IntroState / OutroState worker contributions; all four wrote
// a minimal-but-compatible version of this file).
//
// Abstract base for the per-mode sub-states driven by the GameMode state machine
// (Countdown / Intro / InProgress / Outro / Results / Quit / OnlineLoading / OnlineSplash).
// Reconstructed from the DecFIGS DWARF (GameSource/GameState/ModeManager/GameModeStates/
// BrnGameModeState.h:46): a polymorphic base with a vtable and two back-pointers, wired up
// by the non-virtual Construct.
//
// X360-LEDGER NOTE: none of GameModeState's own methods (Construct / the three virtuals)
// appear in the X360 ledger as separately-emitted functions -- they are pure base hooks the
// X360 build inlined (or never emitted standalone). Construct is therefore modelled as an
// inline setter (matching the inlined GameModeState::Construct the GameMode::Construct body
// expands to). The three virtuals establish the load-bearing vtable slot order
// (OnEnter=0, OnLeave=1, Update=2) that GameMode dispatches through (SetCurrentState calls
// OnLeave on the outgoing state then OnEnter on the incoming; PreWorldUpdate calls Update on
// the current state) and that every concrete state's overrides bind to. Their out-of-line
// bodies belong to the real GameModeState TU; the compile gate (cl /c) needs only the
// declarations because each instantiated concrete state fully (or partially, see OutroState)
// overrides them.
//
// LAYOUT (DWARF BrnGameModeState.h:46): vptr, then two protected back-pointers --
// const ModeManager* mpModeManager and GameMode* mpGameMode. The X360 pseudocode confirms
// it (CountdownState/IntroState read mpGameMode at this+8, mpModeManager at this+4, vptr at
// +0 on the 32-bit X360 ABI). On the x64 PC gate pointer widths differ, so this preserves
// relative ordering / named access for semantic parity, not exact byte placement.
class GameModeState
{
public:
    GameModeState();

    // Wires the state to its owning ModeManager / GameMode. Inlined in the X360 build;
    // GameMode::Construct expands this over each contained state.
    void Construct(const ModeManager* lpModeManager, GameMode* lpGameMode)
    {
        mpModeManager = lpModeManager;
        mpGameMode    = lpGameMode;
    }

    // Sub-state lifecycle hooks. vtable order is load-bearing -- see header note.
    virtual void OnEnter();
    virtual void OnLeave();
    virtual void Update();

protected:
    const ModeManager* mpModeManager; // owning ModeManager (read-only back-pointer)
    GameMode*          mpGameMode;     // owning GameMode whose flags this state drives
};
}
