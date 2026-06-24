#pragma once

// ===================================================================================
// BrnGui::OnlineTimeoutComponent -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/HUD/Components/BrnOnlineTimeoutTimerComponent.h
//   class:BrnGui::OnlineTimeoutComponent
//
// The HUD "online timeout" timer component (an icon component wrapping a Flapt timer
// field). It drives a single apt view-state output through the component's virtual
// state-setter (vtable slot +0xC) and tracks whether it is currently shown via a flag.
//
// Derives from BrnGui::FlaptIconComponent (DecFIGS DWARF BrnOnlineTimeoutTimerComponent.h:58).
// That base hierarchy (FlaptIconComponent -> ... -> CgsGui::GuiComponent) is not recovered
// here; only the fields the three reconstructed functions touch are needed, so the leading
// base/member span is modelled as a reserved head and the recovered fields are placed at
// their X360 offsets for clarity. Member-by-name access (the gate compiles 64-bit, so the
// reserved-head offsets are illustrative, not byte-exact).
//
// Recovered functions (X360 ARTIST):
//   Show     @ 0x824735F0  -> SetAptViewState("visible");   mbActive = true
//   Transin  @ 0x82410A18  -> SetAptViewState("transin");   mbActive = true
//   Transout @ 0x82410A60  -> SetAptViewState("invisible");  mbActive = false
// Each calls the component's virtual apt-view-state setter (vtable byte offset +0xC ==
// slot index 3) with the state string, then sets/clears the +0xA5 "active/shown" byte.
// ===================================================================================

#include "types.hpp"

namespace BrnGui
{
    struct OnlineTimeoutComponent
    {
        // X360 +0xA5 -- the "currently shown" flag. Show/Transin set it; Transout clears it.
        // DWARF spells this member `bool mbActive` (BrnOnlineTimeoutTimerComponent.h:111).
        bool IsActive() const { return mbActive; }

        // @ 0x824735F0 -- show: publish the "visible" apt view state, then mark active.
        void Show();
        // @ 0x82410A18 -- transition in: publish "transin", then mark active.
        void Transin();
        // @ 0x82410A60 -- transition out: publish "invisible", then clear active.
        void Transout();

        // ----- recovered layout (member-by-name; reserved head carries the unrecovered base) -----
        // +0x00 : vtable pointer (FlaptIconComponent base). The virtual at byte offset +0xC
        //         (slot index 3) is the apt-view-state setter `void (*)(this, const char*)`
        //         the three functions invoke. Modelled as a typed function-pointer table so
        //         the dispatch can be reproduced (see BrnOnlineTimeoutTimerComponent.cpp).
        void**  mppVTable;          // +0x00
        u8      maHeadReserved[0xA5 - sizeof(void*)]; // +0x08..+0xA4 (base + mTimerField/mfNewTime/...)
        bool    mbActive;           // +0xA5 (the "shown" flag)
    };
}
