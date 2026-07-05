#ifndef BRN_MANUFACTURER_ICON_H
#define BRN_MANUFACTURER_ICON_H

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsID.h"                      // CgsID (used in Set signature)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h" // CgsGui::GuiComponent

// BrnGui::ManufacturersIcon - an apt-driven icon that shows a vehicle manufacturer's
// badge. The public Set(const BrnResource::VehicleList*, CgsID) resolves the selected
// car (following its parent id when set), converts that id to its printable name, and
// looks the name up in maCarNameToManufacturesMapping[88] to pick the manufacturer enum
// it hands to the private Set(E_MANUFACTURER); unmatched cars hide the icon ("invisible").
// The private Set(E_MANUFACTURER) maps the enum to the "apt_manufacturer" view-state
// string ("CARSON".."WATSON", or "invisible").
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::ManufacturersIcon::Construct                      @ 0x8241BD28  (tail-call to base)
//   BrnGui::ManufacturersIcon::Set(const VehicleList*, CgsID) @ 0x824350A0
//   BrnGui::ManufacturersIcon::Set(E_MANUFACTURER)            @ 0x8241BD30
//
// Shape confirmed by DecFIGS DWARF (BrnManufacturerIcon.h): nested ManufacturersStringEnumMap,
// the private static maCarNameToManufacturesMapping[88], the public Set(const VehicleList*,
// CgsID) [.cpp:165] and the private Set(E_MANUFACTURER) [.cpp:209].
//
// OMITTED (declared in DWARF, out of scope): Update() @ .cpp:150. The 88-entry CONTENTS of
// maCarNameToManufacturesMapping are pure rodata (car-name -> manufacturer-enum pairs) NOT
// recoverable from the in-scope asm; the table is DECLARED here, its definition left to a
// data-blob TU. GROW, do not fork.

namespace BrnResource { struct VehicleList; }
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

        // 0x824350A0 -- pick the manufacturer badge for the selected card (via its parent id
        // when set) and push it to "apt_manufacturer"; unmatched cars hide it ("invisible").
        void Set(const BrnResource::VehicleList* lpVehicleList, CgsID lSelectedCardId);

    private:
        // A single car-name -> manufacturer-enum mapping row (DWARF: BrnManufacturerIcon.h:66).
        struct ManufacturersStringEnumMap
        {
            const char*    mCarNameIdentifier;    // +0x00 -- the car's printable id (case-insensitive match key)
            E_MANUFACTURER meManufacturersIcon;   // +0x04 -- the manufacturer badge for that car
        };

        // 0x8241BD30 -- push the manufacturer badge string to "apt_manufacturer"; unknown
        // ids (>= E_MANUFACTURER_NONE) fall through to "invisible".
        void Set(E_MANUFACTURER leManufacturer);

        // The 88-row car-name -> manufacturer lookup table the public Set walks (DWARF:
        // BrnManufacturerIcon.cpp:23). X360 table stride is 8 bytes {const char*(+0),
        // E_MANUFACTURER(+4)}; on the 64-bit host the pointer widens (sizeof == 16) -- no
        // offsetof pin (pointer-widening, not assertable). CONTENTS are rodata not recoverable
        // from the in-scope asm; DECLARED here, defined by the data-blob TU.
        static const ManufacturersStringEnumMap maCarNameToManufacturesMapping[88];
    };
}

#endif
