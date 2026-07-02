#include "GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlineCountdown.h"

#include <cmath>   // std::ceil

#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SnPrintf

// BrnGui::CarSelectOnlineCountdown -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Screen/Components/BrnCarSelectOnlineCountdown.cpp):
//   CarSelectOnlineCountdown::Construct   @0x8241AE80
//   CarSelectOnlineCountdown::SetTimeLeft @0x8241AED8

namespace BrnGui
{
    const char CarSelectOnlineCountdown::KAC_NUMBER_TEXTFIELD_NAME[8] = "Time_mc";

    // @ 0x8241AE80
    void CarSelectOnlineCountdown::Construct(const char* lpacName,
                                             CgsGui::StateInterface* lpStateInterface,
                                             const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
        // Virtual dispatch through the embedded field's vtable, parented under this
        // component's own name.
        mNumberTextfield.Construct(KAC_NUMBER_TEXTFIELD_NAME, mpStateInterface, macName);
        miTimeLeft = -1;
    }

    // @ 0x8241AED8
    void CarSelectOnlineCountdown::SetTimeLeft(f32 lfTimeLeft)
    {
        const s32 liTimeLeft = static_cast<s32>(std::ceil(lfTimeLeft));
        if (liTimeLeft == miTimeLeft)
            return;
        miTimeLeft = liTimeLeft;

        // Past zero the clock hides; otherwise it ticks.
        AddOutputAptViewState("apt_state", (liTimeLeft < 0) ? "invisible" : "tick", false);

        char lacNumber[16];
        CgsCore::SnPrintf(lacNumber, 16, "%d", miTimeLeft);
        lacNumber[15] = 0;
        mNumberTextfield.SetText(lacNumber);
    }
}
