#ifndef BRN_ICON_H
#define BRN_ICON_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent

// BrnGui::IconComponent - a generic apt-driven icon screen component. It is configured
// with a table of state-identifier strings; SetState selects which identifier to push to
// the bound apt movie's "apt_state" view-state, so the artwork swaps between named frames.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::IconComponent::Construct       @ 0x824E63C0
//   BrnGui::IconComponent::SetState(u32)   @ 0x824E2B60
//
// Derives from CgsGui::GuiComponent and adds the two instance members the asm touches:
//   +0x8C mpStateIdentifiers (the const char* table), +0x90 muStateIndex (selected index).
// The const char*-keyed SetState overload, GetState, and the file-scope macState string
// table are declared/used only by the not-yet-bodied SetState(const char*) overload and
// are OMITTED from this minimal slice (their contents are not recoverable from the two
// in-scope functions' asm).

namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    class IconComponent : public CgsGui::GuiComponent
    {
    public:
        // 0x824E63C0 -- run the base Construct, store the state-identifier table, zero the
        // selected index, and (if a table was supplied) push its first entry as the initial
        // "apt_state". `lpacParentName` is forwarded to the base; the table is the extra arg.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* const* lppacStateIdentifiers, const char* lpacParentName);

        // 0x824E2B60 -- record the selected state index and, if a table is bound, push the
        // indexed identifier onto the apt "apt_state" view-state.
        void SetState(u32 luStateIndex);

        u32 GetState() const { return muStateIndex; }

    private:
        // Guest +0x8C: the const char* table of state-identifier strings (or null).
        const char* const* mpStateIdentifiers;
        // Guest +0x90: the currently-selected index into mpStateIdentifiers.
        u32                muStateIndex;
    };
}

#endif
