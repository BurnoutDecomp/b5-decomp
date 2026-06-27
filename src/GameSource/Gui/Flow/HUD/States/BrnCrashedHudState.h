#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::CrashedHudState - the "CRASHED" HUD flow state (one of the 14 states the HUD flow
// pool owns; built by BrnHudFlow::Prepare @0x8251A620 as the size-2336 / vtable off_82075400
// slot). Class shape from the DecFIGS DWARF (BrnCrashedHudState.h); derives from CgsGui::State.
// Distinct from BrnGui::CrashedStuntHudState (the "CRASHEDSTNT" slot).
//
// PHASE NOTE (boot-path push): an in-game gameplay HUD state, off the boot path. This is a
// faithful class shell so the HUD flow's 14-state pool is structurally complete and
// instantiable; members + out-of-line bodies land in the later "full faithful all states"
// phase (class:BrnGui::CrashedHudState TU). Base virtuals are non-pure, so the shell is
// constructible via CgsGui::State::Construct(id, fsm).
namespace BrnGui
{
    struct CrashedHudState : public CgsGui::State
    {
    };
}
