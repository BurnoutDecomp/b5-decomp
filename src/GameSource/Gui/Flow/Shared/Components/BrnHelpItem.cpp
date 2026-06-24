// ===================================================================================
// BrnGui::HelpItem  -- implementation
//   class:BrnGui::HelpItem
//
// Construct @ 0x824E2848
// SetItem   @ 0x824E29B0
//   Control flow and stores reconstructed store-for-store from the X360 pseudocode/asm.
//
// FLAGGED SUBSTITUTION: the X360 builds each precondition's assert message through the
// CgsDev::StrStream machinery (off_82000D00 / off_82000D08 vtable + Clear + operator<<)
// then CgsDev::Assert::FireAssert. Per the project rule that substitutes that path, the
// guards are reconstructed with the committed CGS_ASSERT macro.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnHelpItem.h"

#include "GameShared/GameClasses/Core/CgsAssert.h" // committed CGS_ASSERT (flagged substitute)

namespace BrnGui
{
    // @ 0x824E2848
    void HelpItem::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                             const char* lpacParentName)
    {
        // asm: cmplwi r21(=a2=lpacName),0 -> assert "Invalid name"               (line 46)
        CGS_ASSERT(lpacName != nullptr, "Invalid name");
        // asm: cmplwi r24(=a3=lpStateInterface),0 -> assert "Invalid state interface" (line 47)
        CGS_ASSERT(lpStateInterface != nullptr, "Invalid state interface");

        // bl CgsGui__GuiComponent__Construct(this, name, stateInterface, parentName)
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        // (**(this+0x8C))(this+0x8C, "ButtonLeft", stateInterface, this->macName)
        //   -- Construct the left child button, parented to this item's own name.
        mIconLeft.Construct("ButtonLeft", lpStateInterface, macName);

        // (**(this+0x11C))(this+0x11C, "ButtonRight", stateInterface, this->macName)
        mIconRight.Construct("ButtonRight", lpStateInterface, macName);
    }

    // @ 0x824E29B0
    void HelpItem::SetItem(const char* lpacText, ButtonIconComponent::EPadButton leButtonLeft,
                           ButtonIconComponent::EPadButton leButtonRight)
    {
        // if (!lpacText) assert "Invalid text string"                       (line 68)
        CGS_ASSERT(lpacText != nullptr, "Invalid text string");

        // if (leButtonLeft < 0 || leButtonRight >= 16) assert "Invalid button state" (line 69)
        CGS_ASSERT(!(leButtonLeft < 0 || leButtonRight >= ButtonIconComponent::E_PADBUTTON_COUNT),
                   "Invalid button state");
        // if (leButtonRight < 0 || leButtonRight >= 16) assert "Invalid button state" (line 70)
        CGS_ASSERT(!(leButtonRight < 0 || leButtonRight >= ButtonIconComponent::E_PADBUTTON_COUNT),
                   "Invalid button state");

        // BrnGui__ButtonIconComponent__SetButton(this+0x8C, leButtonLeft, 0)
        mIconLeft.SetButton(leButtonLeft, ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);
        // BrnGui__ButtonIconComponent__SetButton(this+0x11C, leButtonRight, 0)
        mIconRight.SetButton(leButtonRight, ButtonIconComponent::E_PADBUTTON_STATE_ACTIVE);

        // AddOutputAptViewState(this, "apt_SetText", lpacText, 0)
        AddOutputAptViewState("apt_SetText", lpacText, false);
    }
}
