#pragma once

// ===================================================================================
// BrnGui::MenuComponent -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h
//   class:BrnGui::MenuComponent
//
// A menu component: a BrnGui::SelectableGroup over 16 embedded BrnGui::MenuItem rows. The
// console object is SelectableGroup (0x238) followed by MenuItem maItems[16] (@+0x238,
// 0xE8 stride) and the row count miNumMenuItems (@+0x10B8). Reconstructed from the X360
// ARTIST build (semantic parity by named members; the x64 gate widens pointers, so the
// guest offsets in the comments are documentary).
//
// The console dispatches the group/item methods through modelled component vtables; the
// reconstructed methods call each other + the MenuItem::Selectable virtuals directly by
// name (SetActive/SetHighlightable/SetSelectable/SetHighlighted/Select/Update), so the
// menu flow does not need a live vtable.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // base
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuItem.h"         // embedded row
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                        // BrnGui::GuiFlow

namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    class GuiCache;

    struct MenuComponent : public SelectableGroup
    {
        static const s32 KI_MAX_MENU_ITEMS = 16;   // GetSelectable's fixed index bound
        static const s32 KU_MAX_ITEM_NAME  = 64;   // "<name>_<i>" build buffer

        // @0x824FFEF8 -- vtable-init-only ctor (the group + 16 rows establish their vtables).
        MenuComponent();

        // @0x824E9320 -- name the group, record the row count, build + name each of liCount
        // MenuItem rows ("<name>_<i>"). liArgParent/luAptId feed SelectableGroup::Construct.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       s32 liCount, const char* lpacParentName, u64 luAptId);

        // @0x824E2C08 -- activate the first liNumActive rows (Active/Highlightable/Selectable +
        // register them), disable the rest, highlight row 0, set the wrap flag.
        void SetupMenu(s32 liNumActive, bool lbWrap);

        // @0x8241EA88 -- set the localisation-key text of the row at liIndex (-> MenuItem::SetText).
        void SetText(s32 liIndex, const char* lpacText);

        // @0x824E6428 -- clear the group + reset every row (component vtable slot 6).
        void Clear();

        // @0x8240FE20 -- bounds-check [0,16), return the row pointer (a MenuItem*).
        MenuItem* GetSelectable(s32 liIndex);

        // @0x82488FC0 -- set the row's id + dirty it; returns the row.
        MenuItem* SetId(s32 liIndex, u64 luId);

        // @0x824E2E38 -- make the row un-highlightable + un-selectable.
        void DisableSelectable(s32 liIndex);

        // @? -- move the highlight to the next row (SelectableGroup::HighlightNext, not quiet).
        bool HighlightNext();
        // @0x824E4DE8 -- move the highlight to the previous row (not quiet).
        bool HighlightPrevious();

        // @0x824E2DE0 -- register every row's apt component name with the loading-screen cache.
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpCache);

        MenuItem maItems[KI_MAX_MENU_ITEMS];   // +0x238 (16 rows, 0xE8 stride)
        s32      miNumMenuItems;               // +0x10B8 (rows created by Construct)
    };
}
