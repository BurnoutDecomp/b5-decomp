// BrnGuiDebugComponent.h
// Home of BrnGui::GuiDebugComponent -- the GUI debug-menu component singleton.
//
// This slice reconstructs ONLY the singleton accessor that the X360 ARTIST build
// emits out-of-line:
//
//   GetSingletonPtr @ 0x824ECC40 -> returns mspSingletonThis (a static GuiDebugComponent*)
//                                   after a non-fatal null guard (DWARF
//                                   gamesource/gui/BrnGuiDebugComponent.h:119).
//
// The component itself is a debug-only race-HUD toggler; the only out-of-line member
// in scope here is the singleton getter (its caller is
// BrnGui::GuiDebugComponent::ToggleRaceHudComponentsCallback). The rest of the class
// is declared-only and intentionally not modelled -- the singleton storage + accessor
// are all that the X360 ledger attributes to this TU.

#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // The GUI debug-menu component. Only the singleton storage + accessor are modelled
    // (X360-attested out-of-line). The static instance pointer is the X360
    // off_82FB5080 (mspSingletonThis).
    class GuiDebugComponent
    {
    public:
        // @ 0x824ECC40 -- return the singleton instance pointer, asserting it is non-null
        // (the X360 returns the pointer regardless; the assert is non-fatal).
        static GuiDebugComponent* GetSingletonPtr();

    private:
        // X360 off_82FB5080 -- the single live GuiDebugComponent instance, set when the
        // debug component is created. Modelled as the class's static singleton slot.
        static GuiDebugComponent* mspSingletonThis;
    };
}
