#ifndef BRN_HELP_ITEM_H
#define BRN_HELP_ITEM_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent
#include "GameSource/Gui/Flow/Shared/Components/BrnButtonIcon.h"     // BrnGui::ButtonIconComponent

// BrnGui::HelpItem - an apt-driven on-screen help line: a localisable text caption flanked
// by two control-pad button glyphs (a "left" and a "right" ButtonIconComponent). It pushes
// the caption to the bound apt movie via the "apt_SetText" view-state.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::HelpItem::Construct @ 0x824E2848
//   BrnGui::HelpItem::SetItem   @ 0x824E29B0
//
// Derives from CgsGui::GuiComponent (0x8C bytes) and embeds the two button-icon children:
//   +0x8C  mIconLeft  (BrnGui::ButtonIconComponent, sizeof 0x90)
//   +0x11C mIconRight (BrnGui::ButtonIconComponent)
//
// FLAGGED SUBSTITUTION: the X360 guards Construct/SetItem with the rich
// CgsDev::StrStream + CgsDev::Assert path; per the project rule those are reconstructed
// with the committed CGS_ASSERT macro.

namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    class HelpItem : public CgsGui::GuiComponent
    {
    public:
        // BrnHelpItem.h:48 (DWARF). Selects which of the two embedded icons SetIconState/
        // GetIconButton operate on.
        enum EIconPosition
        {
            E_ICONPOSITION_LEFT  = 0,
            E_ICONPOSITION_RIGHT = 1,
            E_ICONPOSITION_COUNT = 2,
        };

        // 0x824E2848 -- base Construct, then Construct the two child button icons named
        // "ButtonLeft" / "ButtonRight" parented to this item's name.
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

        // 0x824E29B0 -- set both button glyphs and push the caption text to "apt_SetText".
        void SetItem(const char* lpacText, ButtonIconComponent::EPadButton leButtonLeft,
                     ButtonIconComponent::EPadButton leButtonRight);

    private:
        // Guest +0x8C / +0x11C: the two flanking control-pad button glyphs.
        ButtonIconComponent mIconLeft;
        ButtonIconComponent mIconRight;
    };
}

#endif
