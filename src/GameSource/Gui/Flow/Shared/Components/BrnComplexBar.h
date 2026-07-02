#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)

// BrnGui::ComplexBar - a multi-segment apt bar component (the red car-stats bar,
// boost bars, ...). FLAG: minimal slice -- only the surface PlayerStatsBar
// consumes is declared (the virtual Construct the X360 dispatches through the
// embedded member's vtable slot 0, and the transition-complete handler); the full
// DWARF member set + bodies land with the ComplexBar TU. DWARF home
// BrnComplexBar.h; base derivation CgsGui::GuiComponent (the PlayerStatsBar
// name-compare reads the embedded bar's GuiComponent::macName).
namespace BrnGui
{
    class ComplexBar : public CgsGui::GuiComponent
    {
    public:
        // Vtable slot 0 (PlayerStatsBar::Construct @0x8241AE80-family dispatches it
        // with (name, iface, parent)); declaration-only.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @ the PlayerStatsBar::HandleTransitionComplete forward @0x824E5BA8;
        // declaration-only (its own ledger function).
        void HandleTransitionComplete(s32 liArg);
    };
}
