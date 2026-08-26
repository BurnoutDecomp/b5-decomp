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
// X360 build inlined or COMDAT-folded onto a shared leaf. Construct is therefore modelled as an
// inline setter (matching the inlined GameModeState::Construct the GameMode::Construct body
// expands to). The three virtuals establish the load-bearing vtable slot order
// (OnEnter=0, OnLeave=1, Update=2) that GameMode dispatches through (SetCurrentState calls
// OnLeave on the outgoing state then OnEnter on the incoming; PreWorldUpdate calls Update on
// the current state) and that every concrete state's overrides bind to.
//
// ===================================================================================================
// THE EIGHT STATE VTABLES, DUMPED FROM image.bin (2026-08-26, states-blob unpark round). This is the
// ground truth for both the slot order above AND the base bodies below.
//
// GameMode embeds the eight concrete states BY VALUE, so their vtables sit consecutively in .rdata,
// three 4-byte slots each, in E_GMS_* order:
//
//   0x820CF3DC CountdownState      {82332D70 OnEnter, 823165F0 OnLeave, 82316518 Update}
//   0x820CF3E8 IntroState          {823163C8 OnEnter, 82316508 OnLeave, 823164A0 Update}
//   0x820CF3F4 InProgressState     {82316600 OnEnter, 8284CB38        , 8284CB38        }
//   0x820CF400 OutroState          {82316610 OnEnter, 8284CB38        , 82316650 Update}
//   0x820CF40C ResultsState        {82316690 OnEnter, 8284CB38        , 8284CB38        }
//   0x820CF418 QuitState           {823166A0 OnEnter, 8284CB38        , 8284CB38        }
//   0x820CF424 OnlineLoadingState  {823166B0 OnEnter, 8284CB38        , 8284CB38        }
//   0x820CF430 OnlineSplashState   {823166B0 OnEnter, 8284CB38        , 8284CB38        }
//
// 0x8284CB38 is the COMDAT-folded `blr` leaf (the same one that fills 12 of GameMode's own 26 base
// slots -- see the FOLDED-LEAF LEGEND in BrnGameMode.h).
//
// => OnLeave AND Update HAVE EMPTY BASE BODIES, and that is ATTESTED, not assumed: InProgressState
//    and QuitState each declare ONLY OnEnter (DWARF BrnInProgressState.h:45 / BrnQuitState.h:46),
//    so slots 1/2 of THEIR tables ARE GameModeState::OnLeave / ::Update -- and both read as the
//    empty leaf. ResultsState is NOT a witness for slot 2: its DWARF declares OnEnter AND Update
//    (BrnResultsState.cpp:51), so its slot-2 word is ResultsState::Update COMDAT-folded onto the
//    same blr leaf (corrected 2026-08-26 verify). OutroState corroborates slot 1 independently
//    (it overrides Update but not OnLeave).
//
// => OnEnter's base body is UNATTESTED and UNREACHABLE: all eight tables carry a real function in
//    slot 0, so no vtable in the image ever exposes GameModeState::OnEnter. It is given the same
//    inert empty body as its two siblings; nothing dispatches to it.
//
// WHY THESE ARE DEFINED INLINE HERE rather than in a BrnGameModeState.cpp: the console had such a TU
// (DWARF BrnGameModeState.cpp:47), but this tree has none and the shipping file list
// (tools/build/build_game_exe.bat) is off-limits to this round. Inline definitions keep the vtables
// of the eight embedded states resolvable at link time -- which they must be, now that
// GameMode::Construct really does contain all eight by value. [x] MOVE TO BrnGameModeState.cpp when
// the build file list is next touched.
// ===================================================================================================
//
// LAYOUT (DWARF BrnGameModeState.h:46): vptr, then two protected back-pointers --
// const ModeManager* mpModeManager and GameMode* mpGameMode. The X360 pseudocode confirms
// it (CountdownState/IntroState read mpGameMode at this+8, mpModeManager at this+4, vptr at
// +0 on the 32-bit X360 ABI). On the x64 PC gate pointer widths differ, so this preserves
// relative ordering / named access for semantic parity, not exact byte placement.
class GameModeState
{
public:
    // The X360 build emits NO GameModeState constructor -- the DecFIGS DWARF lists it alongside the
    // implicit copy constructor (BrnGameModeState.h:46), i.e. it is the compiler-generated trivial
    // one, and every field is seeded later by Construct (which GameMode::Construct 0x8232F9D8 calls
    // over all eight embedded states). Defined inline and EMPTY so that it stays exactly that while
    // still being a definition: GameMode now embeds the eight states BY VALUE, so GameMode::GameMode()
    // odr-uses this ctor and a declaration alone would be an unresolved external at link time.
    GameModeState() {}

    // Wires the state to its owning ModeManager / GameMode. Inlined in the X360 build;
    // GameMode::Construct expands this over each contained state.
    void Construct(const ModeManager* lpModeManager, GameMode* lpGameMode)
    {
        mpModeManager = lpModeManager;
        mpGameMode    = lpGameMode;
    }

    // Sub-state lifecycle hooks. vtable order is load-bearing -- see the dumped-vtable block in the
    // header note above, which is also where the (empty) base bodies come from.
    virtual void OnEnter() {}   // slot 0 -- base body unattested (all eight states override it)
    virtual void OnLeave() {}   // slot 1 -- base IS the folded `blr` leaf 0x8284CB38
    virtual void Update()  {}   // slot 2 -- base IS the folded `blr` leaf 0x8284CB38

protected:
    const ModeManager* mpModeManager; // owning ModeManager (read-only back-pointer)
    GameMode*          mpGameMode;     // owning GameMode whose flags this state drives
};
}
