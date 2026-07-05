// ===================================================================================
// BrnGui::ManufacturersIcon  -- implementation
//   class:BrnGui::ManufacturersIcon
//
// Construct                        @ 0x8241BD28
// Set(const VehicleList*, CgsID)   @ 0x824350A0
// Set(E_MANUFACTURER)              @ 0x8241BD30
//   Reconstructed store-for-store from the X360 pseudocode/asm.
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/Components/BrnManufacturerIcon.h"

#include "GameShared/GameClasses/Core/CgsID.h"            // CgsIDConvertToString
#include "SharedClasses/DataLists/VehicleList.h"          // BrnResource::VehicleList (GetVehicleIndex/GetVehicleData)
#include "SharedClasses/DataLists/VehicleListEntry.h"     // BrnResource::VehicleListEntry::GetParentId
#include <string.h>                                       // _stricmp

namespace BrnGui
{
    // @ 0x8241BD28 -- `b CgsGui__GuiComponent__Construct`: a pure tail-call forward to the
    // base Construct, no added work.
    void ManufacturersIcon::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                      const char* lpacParentName)
    {
        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);
    }

    // @ 0x824350A0 -- resolve the selected car's manufacturer badge. Look up the vehicle by
    // id (following its parent-car id when it has one), convert that id to its printable
    // 12-char name, then linear-scan maCarNameToManufacturesMapping[88] for a case-
    // insensitive name match. On a hit, push the mapped manufacturer enum through the
    // private Set(E_MANUFACTURER); otherwise hide the icon ("invisible").
    void ManufacturersIcon::Set(const BrnResource::VehicleList* lpVehicleList, CgsID lSelectedCardId)
    {
        bool  lbIconHasBeenSet = false;
        CgsID lConvertId       = lSelectedCardId;

        // Resolve the list entry for the selected card (null when not present).
        const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lSelectedCardId);
        const BrnResource::VehicleListEntry* lpVehicleData =
            (liVehicleIndex < 0) ? nullptr : lpVehicleList->GetVehicleData(liVehicleIndex);

        // Prefer the parent car's id for the badge lookup when one is set. The X360 reads
        // entry+0x08 UNCONDITIONALLY (`ld r11,8(r3)`, no null guard before it); mirror that.
        const CgsID lParentId = lpVehicleData->GetParentId();
        if (lParentId != 0)
            lConvertId = lParentId;

        // Printable id, capped at 12 chars (`buf[12] = 0`).
        char lacCarName[64];
        CgsIDConvertToString(lConvertId, lacCarName);
        lacCarName[12] = '\0';

        for (u32 lu = 0; lu < 0x58; ++lu)
        {
            if (_stricmp(maCarNameToManufacturesMapping[lu].mCarNameIdentifier, lacCarName) == 0)
            {
                Set(maCarNameToManufacturesMapping[lu].meManufacturersIcon);
                lbIconHasBeenSet = true;
                break;
            }
        }

        if (!lbIconHasBeenSet)
            AddOutputAptViewState("apt_manufacturer", "invisible", false);
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
