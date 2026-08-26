#pragma once

#include "GameSource/GameState/ModeManager/GameModeStates/BrnGameModeState.h"

namespace BrnGameState
{
// The online-splash sub-state of a game mode's state machine -- the EIGHTH and last state
// (E_GMS_ONLINE_SPLASH == 7, BrnGameStateSharedIO.h:161), and the one this tree was missing.
//
// WHY IT EXISTS NOW: GameMode::Construct (X360 0x8232F9D8) embeds eight state objects BY VALUE and
// publishes a pointer to each into maGameModeStates. Its eighth embed is at this+0x94 and its
// vtable is the eighth of the eight consecutive tables in .rdata (0x820CF430). Without a type for
// it, slot 7 of the array could not be published at all -- which is what left the whole array empty
// and made GameMode::Initialise -> SendEvent -> SetCurrentState index a zero-length array.
//
// SIZE (X360-proven): the embed runs from this+0x94 to mpModeManager at this+0xA0, i.e. TWELVE
// bytes -- exactly sizeof(GameModeState) on the 32-bit console ABI (vptr + mpModeManager +
// mpGameMode). So this class adds NO data members of its own, and the DWARF
// (BrnOnlineSplashState.h:45) agrees: it declares only the three lifecycle virtuals.
//
// BODY: its vtable @0x820CF430 reads {0x823166B0, 0x8284CB38, 0x8284CB38}. Slot 0 is the SAME word
// as OnlineLoadingState's slot 0 -- the two OnEnter bodies COMDAT-folded, so OnlineSplashState::
// OnEnter is byte-for-byte OnlineLoadingState::OnEnter:
//     0x823166B0  lwz r11, 8(r3)      ; mpGameMode
//     0x823166B4  li  r10, 0
//     0x823166B8  stb r10, 0xAD(r11)  ; GameMode +173 == mbFinished
//     0x823166BC  blr
// i.e. mpGameMode->SetFinished(false). Slots 1/2 are the folded `blr` leaf, so OnLeave / Update are
// left to the (empty) base -- the same call the committed ResultsState / QuitState /
// OnlineLoadingState headers make, where the DWARF also declares more overrides than the image can
// tell apart from the base. Declared here, DEFINED in BrnGameMode.cpp.
//
// [!] NO BrnOnlineSplashState.cpp: the shipping file list (tools/build/build_game_exe.bat) is
// off-limits to this round, so a new .cpp would compile in the gate and then silently not be built
// into the exe -- an unresolved external at link. OnEnter is therefore defined in BrnGameMode.cpp,
// which is already on that list. [x] MOVE IT to its own TU when the file list is next touched.
class OnlineSplashState : public GameModeState
{
public:
    virtual void OnEnter();
};
}
