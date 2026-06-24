// ===================================================================================
// BrnGui::ColourField  -- implementation
//   class:BrnGui::ColourField
//
// Construct      @ 0x824E53E0
// OutputAptData  @ 0x824E5440
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnColourField.h"

#include "GameShared/GameClasses/Core/CgsStringUtils.h" // CgsCore::SPrintf

namespace BrnGui
{
    // @ 0x824E53E0
    void ColourField::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                const char* lpacParentName)
    {
        // bl CgsGui__GuiComponent__Construct
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        // SPrintf(a1+0x8C, 15, "%u", 0) -- format primary colour buffer to "0".
        CgsCore::SPrintf(macColour1, KU_MAX_COLOUR_LEN - 1, "%u", 0);
        macColour1[KU_MAX_COLOUR_LEN - 1] = 0;   // stb r29(0), 0x9B(r31)

        // SPrintf(a1+0x9C, 15, "%u", 0) -- format secondary colour buffer to "0".
        CgsCore::SPrintf(macColour2, KU_MAX_COLOUR_LEN - 1, "%u", 0);
        macColour2[KU_MAX_COLOUR_LEN - 1] = 0;   // stb r29(0), 0xAB(r31)

        mbHidden = false;                        // stb r29(0), 0xAC(r31)
        // mbIsGradient (0xAD) is intentionally not initialised here -- the X360 Construct
        // emits no store for it (only the three stb stores above).
    }

    // @ 0x824E5440
    void ColourField::OutputAptData()
    {
        // Always advertise colour1 and emit its value.
        AddOutputAptViewState("apt_useColour1", "1", false);
        AddOutputAptViewState("apt_colour1", macColour1, false);

        // colour2 only when the gradient flag (+0xAD) is set; otherwise advertise "0".
        if (mbIsGradient)
        {
            AddOutputAptViewState("apt_useColour2", "1", false);
            AddOutputAptViewState("apt_colour2", macColour2, false);
        }
        else
        {
            AddOutputAptViewState("apt_useColour2", "0", false);
        }

        // hidden flag (+0xAC) -> "1" / "0".
        AddOutputAptViewState("apt_hideColour", mbHidden ? "1" : "0", false);
    }
}
