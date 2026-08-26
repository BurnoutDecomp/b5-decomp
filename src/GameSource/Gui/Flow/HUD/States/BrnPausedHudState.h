#pragma once

#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"

// BrnGui::PausedHudState - the "PAUSED" HUD flow state (one of the 14 states the HUD flow
// pool owns; built by BrnHudFlow::Prepare @0x8251A620 as the size-56 / vtable off_820753DC
// slot). The class shape is from the DecFIGS DWARF (BrnPausedHudState.h); it derives from
// CgsGui::State.
//
// This TU reconstructs the state's OnEnter/OnLeave event wiring (X360 @0x8247CBE8 /
// @0x82475390) and, since 2026-08-26, its Update.
//
// ⚠️ RETRACTED 2026-08-26 (pause wave). This banner used to say: "The other state virtuals
// (Update, GetResourcesToLoad) are not in this TU's X360 function set, so they keep the base
// CgsGui::State defaults." That is FALSE for Update, and it was falsifiable from inside this
// very file: PausedHudState::Update EXISTS at 0x8247CC58, is exported under that exact name,
// and its own two asserts cite ".../BrnPausedHudState.cpp" lines 128 and 137. A claim that a
// function is "not in the X360 function set" is a claim about a NAME SEARCH, not about the
// image -- and this one was simply wrong. (GetResourcesToLoad is untouched; no claim is made
// about it either way here.)
//
// The class stays the size-56 shell the HUD-flow pool instantiates (the added overrides reuse
// existing base vtable slots and the static members do not change sizeof).
namespace BrnGui
{
    struct PausedHudState : public CgsGui::State
    {
        // X360 vtable overrides (CgsGui::State virtuals).
        virtual void OnEnter();   // @0x8247CBE8 - register for events + post the paused GUI event
        virtual void OnLeave();   // @0x82475390 - unregister from those events
        virtual void Update();    // @0x8247CC58 - drain the in-queue; 148 -> UNPAUSE, 377 -> crash

    private:
        // The GUI event ids this state observes (DecFIGS BrnPausedHudState.h:70-71;
        // miNumEventsObserved == 3). The table is dword_8205B060 in .rdata and the IDA export
        // carries no value for it -- but Update's own dispatch names all three ids outright
        // (14 / 148 / 377), which is how the {0,0,0} placeholder was retired.
        static const s32 maiEventToObserve[3];
        static const s32 miNumEventsObserved;
    };
}
