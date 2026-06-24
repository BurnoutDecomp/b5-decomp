#ifndef BRN_MANUFACTURER_ICON_H
#define BRN_MANUFACTURER_ICON_H

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent

// BrnGui::ManufacturersIcon - an apt-driven icon that shows a vehicle manufacturer's
// badge. The private Set(E_MANUFACTURER) maps the manufacturer enum to the apt
// "apt_manufacturer" view-state string ("CARSON".."WATSON", or "invisible").
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::ManufacturersIcon::Construct           @ 0x8241BD28  (tail-call to base)
//   BrnGui::ManufacturersIcon::Set(E_MANUFACTURER) @ 0x8241BD30
//
// MINIMAL-SLICE: only the two in-scope functions are committed. Update(),
// Set(const BrnResource::VehicleList*, CgsID), and the maCarNameToManufacturesMapping[88]
// car-name lookup table are declared in the DWARF but NOT in this slice and are OMITTED
// (the public Set walks that table to pick the enum it hands to the private Set below;
// neither is recoverable from the two in-scope functions' asm).

namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    class ManufacturersIcon : public CgsGui::GuiComponent
    {
    public:
        enum E_MANUFACTURER
        {
            E_MANUFACTURER_CARSON = 0,
            E_MANUFACTURER_HUNTER = 1,
            E_MANUFACTURER_JANSEN = 2,
            E_MANUFACTURER_KERIGER = 3,
            E_MANUFACTURER_KITANO = 4,
            E_MANUFACTURER_MONTGOMERY = 5,
            E_MANUFACTURER_NAKAMURA = 6,
            E_MANUFACTURER_ROSSOLINI = 7,
            E_MANUFACTURER_WATSON = 8,
            E_MANUFACTURER_NONE = 9,
            E_MANUFACTURER_COUNT = 10,
        };

        // 0x8241BD28 -- forwards straight to the base Construct (no added body).
        virtual void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName);

    private:
        // 0x8241BD30 -- push the manufacturer badge string to "apt_manufacturer"; unknown
        // ids (>= E_MANUFACTURER_NONE) fall through to "invisible".
        void Set(E_MANUFACTURER leManufacturer);
    };
}

#endif
