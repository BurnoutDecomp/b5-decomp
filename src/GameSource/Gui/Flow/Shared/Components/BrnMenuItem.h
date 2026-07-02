#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::GuiComponent (second base)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"        // BrnGui::Selectable (first base)

// BrnGui::MenuItem - one row of a menu component: a selectable with a label text
// and an apt state derived from its selectable flags. Class shape / member names /
// enum verbatim from the DecFIGS DWARF (BrnMenuItem.h:42/:84/:97-:104); gated on
// the X360 ledger. The X360 layout pins the usual MI bases (Selectable @+0x00,
// flags @+0x0C / id @+0x10; GuiComponent @+0x18) with macItemText @+0xA4 and
// mbUpdateText @+0xE4. This TU bodies Clear/Set/SetText/Update; Construct/
// GetItemText/Select/FindState are their own ledger functions / X360 inlines
// (declaration-only).
namespace BrnGui
{
    struct MenuItem : public Selectable, public CgsGui::GuiComponent
    {
        static const u32 KU_MAX_NAME_LENGTH = 64;   // DWARF h:45

        // DWARF BrnMenuItem.h:84.
        enum MenuItemStates
        {
            E_MENUITEMSTATES_UNUSED        = 0,
            E_MENUITEMSTATES_INVISIBLE     = 1,
            E_MENUITEMSTATES_DISABLED      = 2,
            E_MENUITEMSTATES_UNHIGHLIGHTED = 3,
            E_MENUITEMSTATES_HIGHLIGHTED   = 4,
            E_MENUITEMSTATES_COUNT         = 5,
        };

        // DWARF h:53/:71/:119/:95 -- declared-only (their own ledger functions /
        // X360 header-inlines not exported).
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luId);
        const char* GetItemText() const;
        virtual void Select();

        // @0x824E2E90 (this TU, DWARF h:57) -- wipe the row back to an unused slot.
        void Clear();

        // @0x824E2F58 (this TU, DWARF h:64, cpp:101) -- adopt a label + id.
        void Set(const char* lpacText, s32 liId);

        // @0x824E3030 (this TU, DWARF h:68, cpp:119) -- retext without re-tagging.
        void SetText(const char* lpacText);

        // @0x824E6490 (this TU, DWARF h:79) -- push the label/state/updatestate apt
        // outputs when dirtied or retexted.
        virtual void Update();

    private:
        MenuItemStates FindState() const;   // DWARF h:95 (X360-inlined into Update)

        // DWARF h:100 -- the per-state apt state names (X360 .data @0x82F27430;
        // strings recovered from the decrypted XEX; UNUSED shares "Invisible").
        static const char* const KAC_STATE_NAMES[E_MENUITEMSTATES_COUNT];

        char macItemText[KU_MAX_NAME_LENGTH];   // DWARF h:97 (X360 +0xA4)
        bool mbUpdateText;                      // DWARF h:98 (X360 +0xE4)
    };
}
