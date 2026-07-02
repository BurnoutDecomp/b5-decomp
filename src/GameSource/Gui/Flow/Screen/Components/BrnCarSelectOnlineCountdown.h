#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (base)
#include "GameSource/Gui/BrnGuiTextField.h"                           // BrnGui::TextField (embedded)

// BrnGui::CarSelectOnlineCountdown - the online car-select countdown clock: an apt
// state ("tick"/"invisible") plus the seconds-left number text. DWARF home
// BrnCarSelectOnlineCountdown.h:46. This TU bodies Construct + SetTimeLeft.
namespace BrnGui
{
    struct CarSelectOnlineCountdown : public CgsGui::GuiComponent
    {
        // @0x8241AE80 (this TU, DWARF cpp:44) -- base Construct + the number field,
        // countdown latch cleared to -1 (forces the first SetTimeLeft through).
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // @0x8241AED8 (this TU, DWARF cpp:64) -- push the ceiled seconds-left when it
        // changes: the apt state flips to "invisible" past zero, else "tick", and the
        // number text is re-rendered.
        void SetTimeLeft(f32 lfTimeLeft);

    private:
        // DWARF cpp:23 -- the number text field's component name ("Time_mc").
        static const char KAC_NUMBER_TEXTFIELD_NAME[8];

        TextField mNumberTextfield;   // DWARF h:64 (X360 +0x8C)
        s32       miTimeLeft;         // DWARF h:66 (X360 +0x1B4; -1 == unset)
    };
}
