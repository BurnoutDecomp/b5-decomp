#pragma once

#include "types.hpp"

namespace BrnGameState
{
// Owning header for the ModeManager of the game-mode hierarchy. This is the real home
// for BrnGameState::ModeManager; BrnGameMode.h forward-declares it (GameMode methods
// take/return it only by pointer) and the game-mode .cpp files #include this header
// when they actually call into it.
//
// MINIMAL / BOUNDED reconstruction: only the single method this slice needs
// (TellGuiToShowOnlineFinalStandings, called by OnlineGameMode::SendEvent) is declared,
// gated on the X360 ledger. The full ModeManager is large (~190 methods + dozens of
// by-value contained mode/state objects in the DecFIGS DWARF, BrnModeManager.h) and is
// reconstructed by its own TU; this header is then extended toward that shape. No layout
// is reconstructed here because OnlineGameMode only ever holds a ModeManager* and calls a
// method on it -- a pointer-only use needs no member layout.
//
// TellGuiToShowOnlineFinalStandings is X360-attested (identity.json, 0x82329B68) but is
// PS3-only-absent from the DecFIGS DWARF (build drift), so its declaration shape is
// recovered from the X360 pseudocode: a non-static member (the pseudocode `a1` is `this`,
// accessing mpGameStateModule / mScoringSystem). The X360 body returns the pointer from
// ScoringSystem::UpdateCumulativeResults; the only caller (SendEvent) discards it, so it
// is declared `void` here to avoid forking the as-yet-unreconstructed return type.
class ModeManager
{
public:
    void TellGuiToShowOnlineFinalStandings();

    // Debug-tunable mode flags driven by the mode-manager debug menu (ModeManagerDebugComponent).
    // Declared as named members here (the real home) so the debug component accesses them by name
    // rather than by raw offset; the full ModeManager layout (the X360 places these deep in the
    // ~38KB object) is reconstructed by the ModeManager TU and these settle into their real slots.
    bool mbEndlessStuntRun;
    bool mbWinIfSecond;
    bool mbFinishCurrentEvent;
    s32  miFinishPosition;
};
}
