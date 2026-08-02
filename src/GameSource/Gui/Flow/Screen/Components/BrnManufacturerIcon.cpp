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
    // @ 0x8204CEC0 (.rdata, DWARF BrnManufacturerIcon.cpp:23). The 88-row car-name ->
    // manufacturer lookup table Set(const VehicleList*, CgsID) linear-scans. DUMPED from
    // BURNOUT_X360_ARTIST.XEX, not reconstructed by inference: 88 rows x 8 bytes
    // {const char*(+0), E_MANUFACTURER(+4)}, big-endian, read through the calibrated .id1
    // reader. Bound proof: the loop compares the index against 0x58 == 88, and the 8 bytes at
    // 0x8204CEC0 + 88*8 are 0x44697374 ("Dist...", the next rodata string) -- not a row. All
    // 88 rows decoded to a printable id and an in-range enum with zero suspects. Row order is
    // the console's, since Set takes the FIRST case-insensitive match.
    const ManufacturersIcon::ManufacturersStringEnumMap
    ManufacturersIcon::maCarNameToManufacturesMapping[88] =
    {
        { "PUSCT01",  E_MANUFACTURER_HUNTER },
        { "XUSCT01B", E_MANUFACTURER_HUNTER },
        { "PUSCL01",  E_MANUFACTURER_CARSON },
        { "XUSCL1B1", E_MANUFACTURER_CARSON },
        { "PUSM03",   E_MANUFACTURER_CARSON },
        { "XUSMU3B",  E_MANUFACTURER_CARSON },
        { "PASC01",   E_MANUFACTURER_KITANO },
        { "XASC1B1",  E_MANUFACTURER_KITANO },
        { "PASBS01",  E_MANUFACTURER_NAKAMURA },
        { "XASBSB1",  E_MANUFACTURER_NAKAMURA },
        { "PASBSC01", E_MANUFACTURER_NAKAMURA },
        { "XASBSCB1", E_MANUFACTURER_NAKAMURA },
        { "PUSBC01",  E_MANUFACTURER_HUNTER },
        { "XUSBCB1",  E_MANUFACTURER_HUNTER },
        { "PASBCC01", E_MANUFACTURER_KITANO },
        { "XASBCB1",  E_MANUFACTURER_KITANO },
        { "PUSBH01",  E_MANUFACTURER_CARSON },
        { "XUSBHB1",  E_MANUFACTURER_CARSON },
        { "PEUBR01",  E_MANUFACTURER_WATSON },
        { "XEUBRB",   E_MANUFACTURER_WATSON },
        { "PUSCV01",  E_MANUFACTURER_CARSON },
        { "XUSCV1B1", E_MANUFACTURER_CARSON },
        { "PUSMC01",  E_MANUFACTURER_HUNTER },
        { "XUSM1B1",  E_MANUFACTURER_HUNTER },
        { "PUSCCO01", E_MANUFACTURER_HUNTER },
        { "XUSCCOB1", E_MANUFACTURER_HUNTER },
        { "PUSRC01",  E_MANUFACTURER_CARSON },
        { "XUSRCB1",  E_MANUFACTURER_CARSON },
        { "PEULM01",  E_MANUFACTURER_ROSSOLINI },
        { "XEULM1B1", E_MANUFACTURER_ROSSOLINI },
        { "PEUSC02",  E_MANUFACTURER_ROSSOLINI },
        { "XEUSC2B",  E_MANUFACTURER_ROSSOLINI },
        { "PEURC03",  E_MANUFACTURER_MONTGOMERY },
        { "XEURC3B1", E_MANUFACTURER_MONTGOMERY },
        { "PEUSR01",  E_MANUFACTURER_MONTGOMERY },
        { "XEUSRB1",  E_MANUFACTURER_MONTGOMERY },
        { "PEUSC03",  E_MANUFACTURER_KERIGER },
        { "XEUSC3B1", E_MANUFACTURER_KERIGER },
        { "PEUSC04",  E_MANUFACTURER_JANSEN },
        { "PEUS4B1",  E_MANUFACTURER_JANSEN },
        { "PEUSV01",  E_MANUFACTURER_KERIGER },
        { "XEUSVB1",  E_MANUFACTURER_KERIGER },
        { "PUSGA01",  E_MANUFACTURER_CARSON },
        { "PUSGAB1",  E_MANUFACTURER_CARSON },
        { "PEURG01",  E_MANUFACTURER_KERIGER },
        { "XEURG1BG", E_MANUFACTURER_KERIGER },
        { "PUSLR01",  E_MANUFACTURER_CARSON },
        { "XUSLRB1",  E_MANUFACTURER_CARSON },
        { "PSPMICR",  E_MANUFACTURER_KITANO },
        { "PUSME01",  E_MANUFACTURER_HUNTER },
        { "XUSMEB1",  E_MANUFACTURER_HUNTER },
        { "PUSMC02",  E_MANUFACTURER_CARSON },
        { "XUSM2B1",  E_MANUFACTURER_CARSON },
        { "PUSRN01",  E_MANUFACTURER_HUNTER },
        { "XUSRNB1",  E_MANUFACTURER_HUNTER },
        { "PUSPK01",  E_MANUFACTURER_HUNTER },
        { "XUSPKB1",  E_MANUFACTURER_HUNTER },
        { "PEURR01",  E_MANUFACTURER_WATSON },
        { "XEURRB1",  E_MANUFACTURER_WATSON },
        { "PEUS01",   E_MANUFACTURER_MONTGOMERY },
        { "XEUSB1",   E_MANUFACTURER_MONTGOMERY },
        { "PSPBEST",  E_MANUFACTURER_HUNTER },
        { "PSPBZ",    E_MANUFACTURER_NAKAMURA },
        { "PSPCHRO",  E_MANUFACTURER_NONE },
        { "PSPCIR",   E_MANUFACTURER_CARSON },
        { "PSPPS3",   E_MANUFACTURER_MONTGOMERY },
        { "PSPGAS",   E_MANUFACTURER_KITANO },
        { "PSPMETL",  E_MANUFACTURER_CARSON },
        { "PSPT",     E_MANUFACTURER_KERIGER },
        { "PSPTS",    E_MANUFACTURER_HUNTER },
        { "PSPWAL",   E_MANUFACTURER_KERIGER },
        { "PUSSC01",  E_MANUFACTURER_JANSEN },
        { "XUSSCB1",  E_MANUFACTURER_JANSEN },
        { "PUSCC01",  E_MANUFACTURER_HUNTER },
        { "XUSCCB1",  E_MANUFACTURER_HUNTER },
        { "PUSCLT02", E_MANUFACTURER_HUNTER },
        { "XUSLT2B1", E_MANUFACTURER_HUNTER },
        { "PUSMC04",  E_MANUFACTURER_CARSON },
        { "XUSMC4B1", E_MANUFACTURER_CARSON },
        { "PUSRI01",  E_MANUFACTURER_KERIGER },
        { "PUSCPI5",  E_MANUFACTURER_KERIGER },
        { "PSPYODO",  E_MANUFACTURER_KITANO },
        { "CARBB1GT", E_MANUFACTURER_NAKAMURA },
        { "CARBB2CC", E_MANUFACTURER_KITANO },
        { "CARBEAGT", E_MANUFACTURER_KERIGER },
        { "CARBMC04", E_MANUFACTURER_CARSON },
        { "CARBRWDS", E_MANUFACTURER_MONTGOMERY },
        { "CARBSC04", E_MANUFACTURER_JANSEN },
    };

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

        // ⚠️ MARKED DEVIATION (2026-08-02, and it is not theoretical): the X360 reads
        // entry+0x08 UNCONDITIONALLY on the next line (`ld r11,8(r3)`, no null guard before
        // it), which it can afford because every caller hands it a live VehicleList id. On
        // this build the id comes from CarSelectMain::mCurrentSetupInfo.mCarId, which
        // CarSelectMain::Construct seeds to (CgsID)-1 and which only becomes real when
        // BrnGameModule::PublishCarSelectionToGui -- a flagged STAND-IN for the console's
        // action-182/406 producer -- has published. The moment the car-select screen stopped
        // bailing on a null VehicleList this AV'd for real: WER fault offset 0x59A70 on the
        // 0x6a6eab6e build, which resolves through Burnout_PC.map to the ICF fold of
        // `mov rax,[rcx+8] / ret` shared by VehicleListEntry::GetParentId, and a byte-scan of
        // .text for `E8 rel32 -> 0x59A70` puts ManufacturersIcon::Set+0x46 in the caller set.
        // Bailing (no badge) is what the console shows for an unmapped car anyway -- the
        // `lbIconHasBeenSet == false` arm below hides the badge. DELETE-WHEN the real
        // event-406/565 producer lands and mCurrentSetupInfo.mCarId is always a live id.
        if (lpVehicleData == 0)
        {
            AddOutputAptViewState("apt_manufacturer", "invisible", false);
            return;
        }

        // Prefer the parent car's id for the badge lookup when one is set.
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
