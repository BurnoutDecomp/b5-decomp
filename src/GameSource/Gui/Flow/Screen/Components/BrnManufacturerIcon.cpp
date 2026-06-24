// ===================================================================================
// BrnGui::ManufacturersIcon  -- implementation
//   class:BrnGui::ManufacturersIcon
//
// Construct            @ 0x8241BD28
// Set(E_MANUFACTURER)  @ 0x8241BD30
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnManufacturerIcon.h"

namespace BrnGui
{
    // @ 0x8241BD28 -- `b CgsGui__GuiComponent__Construct`: a pure tail-call forward to the
    // base Construct, no added work.
    void ManufacturersIcon::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                      const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    }

    // @ 0x8241BD30 -- jump-table switch (9 explicit cases 0..8, default for everything
    // else) pushing the badge name to the "apt_manufacturer" apt view-state.
    void ManufacturersIcon::Set(E_MANUFACTURER leManufacturer)
    {
        const char* lpacBadge;
        switch (leManufacturer)
        {
            case E_MANUFACTURER_CARSON:     lpacBadge = "CARSON";     break;
            case E_MANUFACTURER_HUNTER:     lpacBadge = "HUNTER";     break;
            case E_MANUFACTURER_JANSEN:     lpacBadge = "JANSEN";     break;
            case E_MANUFACTURER_KERIGER:    lpacBadge = "KERIGER";    break;
            case E_MANUFACTURER_KITANO:     lpacBadge = "KITANO";     break;
            case E_MANUFACTURER_MONTGOMERY: lpacBadge = "MONTGOMERY"; break;
            case E_MANUFACTURER_NAKAMURA:   lpacBadge = "NAKAMURA";   break;
            case E_MANUFACTURER_ROSSOLINI:  lpacBadge = "ROSSOLINI";  break;
            case E_MANUFACTURER_WATSON:     lpacBadge = "WATSON";     break;
            default:                        lpacBadge = "invisible";  break;
        }
        AddOutputAptViewState("apt_manufacturer", lpacBadge, false);
    }
}
