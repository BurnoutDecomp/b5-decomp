#ifndef BRN_BUTTON_ICON_H
#define BRN_BUTTON_ICON_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent

// BrnGui::ButtonIconComponent - an apt-driven control-pad button glyph. It carries the
// pad-button id it currently shows; SetButton swaps both the glyph (apt_button) and the
// visual state (apt_state) by pushing the matching identifier strings to the bound apt
// movie. Construct seeds the button to E_PADBUTTON_INVISIBLE.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::ButtonIconComponent::Construct  @ 0x824E20E8
//   BrnGui::ButtonIconComponent::SetButton  @ 0x824E2138
//
// Derives from CgsGui::GuiComponent and adds the one instance member the asm touches:
//   +0x8C meButton (the selected pad-button id; stored as a 32-bit word).

namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    class ButtonIconComponent : public CgsGui::GuiComponent
    {
    public:
        // Pad-button id space (drives the apt_button glyph). 0x824E2138 indexes the
        // identifier table by this value.
        enum EPadButton
        {
            E_PADBUTTON_UP = 0,
            E_PADBUTTON_DOWN = 1,
            E_PADBUTTON_LEFT = 2,
            E_PADBUTTON_RIGHT = 3,
            E_PADBUTTON_SELECT = 4,
            E_PADBUTTON_BACK = 5,
            E_PADBUTTON_OPTION0 = 6,
            E_PADBUTTON_OPTION1 = 7,
            E_PADBUTTON_LSHOULDER = 8,
            E_PADBUTTON_RSHOULDER = 9,
            E_PADBUTTON_LTRIGGER = 10,
            E_PADBUTTON_RTRIGGER = 11,
            E_PADBUTTON_START = 12,
            E_PADBUTTON_LTHUMB = 13,
            E_PADBUTTON_RTHUMB = 14,
            E_PADBUTTON_INVISIBLE = 15,
            E_PADBUTTON_COUNT = 16,
        };

        // Visual-state space (drives the apt_state). 0x824E2138 indexes the state table
        // by this value.
        enum EPadButtonState
        {
            E_PADBUTTON_STATE_ACTIVE = 0,
            E_PADBUTTON_STATE_HIGLIGHTED = 1,
            E_PADBUTTON_STATE_PRESSED = 2,
            E_PADBUTTON_STATE_UNSELECTABLE = 3,
            E_PADBUTTON_STATE_COUNT = 4,
        };

        // 0x824E20E8 -- base Construct, then seed meButton = E_PADBUTTON_INVISIBLE and push
        // the invisible glyph to "apt_button".
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName);

        // 0x824E2138 -- store the button id, push its glyph identifier to "apt_button", then
        // push the state identifier to "apt_state".
        void SetButton(EPadButton leButton, EPadButtonState leState);

        EPadButton GetButton() const { return meButton; }

    private:
        // Guest +0x8C: the selected pad-button id (stored as a 32-bit word).
        EPadButton meButton;
    };
}

#endif
