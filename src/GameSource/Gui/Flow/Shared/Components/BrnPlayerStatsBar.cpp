#include "GameSource/Gui/Flow/Shared/Components/BrnPlayerStatsBar.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstring>   // std::strcmp (the inlined name compare)

// BrnGui::PlayerStatsBar -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (2 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Shared/Components/BrnPlayerStatsBar.cpp):
//   PlayerStatsBar::Construct                @0x824E5B10
//   PlayerStatsBar::HandleTransitionComplete @0x824E5B60

namespace BrnGui
{
    const char PlayerStatsBar::KAC_CAR_BAR_NAME[10] = "RedBar_mc";

    // @ 0x824E5B10 -- base Construct, then the bar (virtual dispatch through the
    // embedded member's vtable, name parented under this component's own name).
    void PlayerStatsBar::Construct(const char* lpacName,
                                   CgsGui::StateInterface* lpStateInterface,
                                   const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
        mCarBar.Construct(KAC_CAR_BAR_NAME, mpStateInterface, macName);
    }

    // @ 0x824E5B60 -- the X360 inlines the strcmp against the embedded bar's
    // component name; on a match the transition is the bar's.
    bool PlayerStatsBar::HandleTransitionComplete(const char* lpcComponentName, s32 liArg)
    {
        if (std::strcmp(lpcComponentName, mCarBar.GetName()) == 0)
        {
            mCarBar.HandleTransitionComplete(liArg);
            return true;
        }
        return false;
    }

    // @ 0x824BAD68 -- set the car-stats value: assert non-negative, clamp to the
    // bar's max, latch it, recolour the bar and run it to the new segment. The X360
    // de-inlines the failure path into BeginAssert/StrStream("Invalid car stat : " <<
    // liValue << "\n")/FireAssert/EndAssert; it folds to one CGS_ASSERT (streamed
    // integer and trailing \n dropped, matching the sibling ComplexBar::RunTo idiom).
    void PlayerStatsBar::SetCar(s32 liValue, u32 luMax)
    {
        CGS_ASSERT(liValue >= 0, "Invalid car stat : ");

        if (liValue > KI_MAX_BAR_LENGTH)
            liValue = KI_MAX_BAR_LENGTH;

        miCarValue = liValue;
        mCarBar.SetColour(luMax);
        mCarBar.RunTo(miCarValue);
    }
}
