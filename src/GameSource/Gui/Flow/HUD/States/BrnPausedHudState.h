#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::PausedHudState - the "PAUSED" HUD flow state (one of the 14 states the HUD flow
// pool owns; built by BrnHudFlow::Prepare @0x8251A620 as the size-56 / vtable off_820753DC
// slot). The class shape is from the DecFIGS DWARF (BrnPausedHudState.h); it derives from
// CgsGui::State.
//
// This TU reconstructs the state's OnEnter/OnLeave event wiring (X360 @0x8247CBE8 /
// @0x82475390). The other state virtuals (Update, GetResourcesToLoad) are not in this TU's
// X360 function set, so they keep the base CgsGui::State defaults; the class stays the
// size-56 shell the HUD-flow pool instantiates (the added overrides reuse existing base
// vtable slots and the static members do not change sizeof).
namespace BrnGui
{
    struct PausedHudState : public CgsGui::State
    {
        // X360 vtable overrides (CgsGui::State virtuals).
        virtual void OnEnter();   // @0x8247CBE8 - register for events + post the paused GUI event
        virtual void OnLeave();   // @0x82475390 - unregister from those events

    private:
        // The GUI event ids this state observes (DecFIGS BrnPausedHudState.h:70-71;
        // miNumEventsObserved == 3). The id table lives in .rdata and carries no value in the
        // IDA export, so it is declared here and resolved at link time (as BrnGui::BootAttract).
        static const s32 maiEventToObserve[3];
        static const s32 miNumEventsObserved;
    };
}
