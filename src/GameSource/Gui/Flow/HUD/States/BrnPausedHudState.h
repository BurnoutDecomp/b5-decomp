#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::PausedHudState - the "PAUSED" HUD flow state (one of the 14 states the HUD flow
// pool owns; built by BrnHudFlow::Prepare @0x8251A620 as the size-56 / vtable off_820753DC
// slot). The class shape is from the DecFIGS DWARF (BrnPausedHudState.h); it derives from
// CgsGui::State.
//
// PHASE NOTE (boot-path push): this push reconstructs the HUD-flow boot path (BrnHudFlow +
// the controller sequencing BF_PRELOAD->BF_LOADING->BF_VIDEOS...). The in-game gameplay HUD
// states (PAUSED/CRASHED/...) are off the boot path; this is a faithful class shell so the
// flow's 14-state pool is structurally complete and instantiable. Its members and out-of-line
// OnEnter/OnLeave/Update bodies are reconstructed in the later "full faithful all states"
// phase (class:BrnGui::PausedHudState TU). All base virtuals are non-pure, so the bare shell
// inherits working defaults and is constructible via CgsGui::State::Construct(id, fsm).
namespace BrnGui
{
    struct PausedHudState : public CgsGui::State
    {
    };
}
