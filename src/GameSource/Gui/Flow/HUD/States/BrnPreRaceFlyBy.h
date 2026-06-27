#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::PreRaceFlyByState - the "PRE_FLY_BY" HUD flow state (one of the 14 states the HUD
// flow pool owns; built by BrnHudFlow::Prepare @0x8251A620 as the size-4160 slot, via the
// named ctor BrnGui::PreRaceFlyByState::PreRaceFlyByState @0x82514E58). Class shape from the
// DecFIGS DWARF (BrnPreRaceFlyBy.h); derives from CgsGui::State.
//
// PHASE NOTE (boot-path push): an in-game (pre-race fly-by camera) HUD state, off the boot
// path. This is a faithful class shell so the HUD flow's 14-state pool is structurally complete
// and instantiable; its ctor body + members + out-of-line bodies land in the later "full
// faithful all states" phase (class:BrnGui::PreRaceFlyByState TU). Base virtuals are non-pure,
// so the shell is constructible via CgsGui::State::Construct(id, fsm).
namespace BrnGui
{
    struct PreRaceFlyByState : public CgsGui::State
    {
    };
}
