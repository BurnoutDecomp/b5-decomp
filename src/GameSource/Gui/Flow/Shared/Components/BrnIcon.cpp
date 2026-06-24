// ===================================================================================
// BrnGui::IconComponent  -- implementation
//   class:BrnGui::IconComponent
//
// Construct      @ 0x824E63C0
// SetState(u32)  @ 0x824E2B60
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnIcon.h"

namespace BrnGui
{
    // @ 0x824E63C0
    void IconComponent::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                  const char* const* lppacStateIdentifiers, const char* lpacParentName)
    {
        // bl CgsGui__GuiComponent__Construct -- base gets (this, name, stateInterface, parentName).
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        mpStateIdentifiers = lppacStateIdentifiers;  // stw r30, 0x8C(r31)
        muStateIndex = 0;                            // stw r11(0), 0x90(r31)

        // if ( a4 ) AddOutputAptViewState(this, "apt_state", *a4, 0)
        if (lppacStateIdentifiers)
            AddOutputAptViewState("apt_state", lppacStateIdentifiers[0], false);
    }

    // @ 0x824E2B60
    void IconComponent::SetState(u32 luStateIndex)
    {
        const char* const* lppacTable = mpStateIdentifiers;  // lwz r11, 0x8C(r3)
        muStateIndex = luStateIndex;                         // stw r4, 0x90(r3)

        // if ( table ) AddOutputAptViewState(this, "apt_state", table[index], 0)
        if (lppacTable)
            AddOutputAptViewState("apt_state", lppacTable[luStateIndex], false);
    }
}
