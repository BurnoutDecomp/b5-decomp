#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::GenericForwardState (DecFIGS DWARF BrnGenericForwardState.h:42) : CgsGui::State.
//
// A screen-flow FSM state whose Update simply advances the flow -- it raises the "ADVANCE"
// state event so the owning controller sequences to the next phase. Only Update @0x82500950
// is bodied by this TU (sibling BrnGenericForwardState.cpp); the other DWARF overrides
// (OnEnter / OnLeave / GetResourcesToLoad) are reconstructed by their own TUs and will extend
// this header. Update is declared virtual (it overrides the FSM state's Update slot) without
// `override` so the header stays compilable while the CgsFsm::ScriptedState base virtual set
// is still being filled in.

namespace BrnGui
{
    struct GenericForwardState : public CgsGui::State
    {
        // @0x82500950 -- advance the flow by sending the "ADVANCE" state event.
        virtual void Update();
    };
}
