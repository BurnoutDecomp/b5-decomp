// ===================================================================================
// BrnGui::MenuComponent -- implementation
//   class:BrnGui::MenuComponent
//
// Reconstructed from the X360 ARTIST build. A SelectableGroup over 16 embedded MenuItem
// rows; the group registers the active rows and drives their apt state through
// MenuItem::Update -> GuiComponent::AddOutputAptViewState. Member-by-name access.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"  // CgsGui::GuiComponent (item base construct/name)
#include "GameSource/Gui/BrnGuiCache.h"                // BrnGui::GuiCache (AppendExpectedAptComponent)

#include <cstring>   // std::strlen (name-length assert)

namespace BrnGui
{
    using CgsGui::StateInterface;

    // @0x824FFEF8 -- the console ctor only seeds vtables: the group's primary + GuiComponent
    // branch, and each of the 16 MenuItem sub-objects' Selectable + GuiComponent vtables. Here
    // the SelectableGroup base (its GuiComponent identity) and the 16 MenuItem members establish
    // their own vtables via member construction; the modelled component-vtable pointer is not
    // materialised on PC (the reconstructed methods dispatch by name).
    MenuComponent::MenuComponent()
    {
        mppVTable      = nullptr;
        miNumMenuItems = 0;
    }

    // @0x824E9320 -- build the menu: name the group, record the row count, then create + name
    // each row.
    void MenuComponent::Construct(const char* lpacName, StateInterface* lpStateInterface,
                                  s32 liCount, const char* lpacParentName, u64 luAptId)
    {
        CGS_ASSERT(lpStateInterface != 0, "Invalid stat interface passed");
        CGS_ASSERT(lpacName != 0, "Invalid name passed in");
        CGS_ASSERT(liCount <= KI_MAX_MENU_ITEMS, "Too many items in menu for current setting in SelectableGroup");

        SelectableGroup::Construct(lpacName, lpStateInterface, lpacParentName, luAptId);
        miNumMenuItems = liCount;
        Clear();   // component vtable slot 6

        // The group name + "_NN" must fit a MenuItem's 64-byte name (invariant; the console
        // re-checks it per row).
        CGS_ASSERT(std::strlen(GetName()) + 3 < KU_MAX_ITEM_NAME, "Name too long.");

        for (s32 li = 0; li < miNumMenuItems; ++li)
        {
            char lacItemName[KU_MAX_ITEM_NAME];
            CgsCore::SPrintf(lacItemName, KU_MAX_ITEM_NAME, "%s_%d", lpacName, li);

            maItems[li].Set(lacItemName, li);
            maItems[li].CgsGui::GuiComponent::Construct(lacItemName, lpStateInterface, lpacParentName);
            // Reset the row to the "unused" baseline (the Set above seeded a transient id/state).
            maItems[li].SetId(Selectable::K_INVALID_ID);   // std -> mId (low 0xFFFFFFFF)
            maItems[li].ClearFlags();                       // stb 0 -> mxFlags
        }
    }

    // @0x824E2C08 -- activate the first liNumActive rows + register them, disable the rest,
    // highlight row 0, apply the wrap flag.
    void MenuComponent::SetupMenu(s32 liNumActive, bool lbWrap)
    {
        CGS_ASSERT(liNumActive >= 0 && liNumActive <= miNumMenuItems,
                   "MenuComponent::GetSelectable() invalid index specified");

        Clear();   // component vtable slot 6

        for (s32 li = 0; li < miNumMenuItems; ++li)
        {
            // (The console also SPrintf's "MenuItem_<i>" into a scratch buffer here; its result
            // is unused -- the rows were named in Construct -- so the dead build is omitted.)
            if (li >= liNumActive)
            {
                maItems[li].SetActive(false);   // item slot 0
            }
            else
            {
                maItems[li].SetActive(true);        // item slot 0
                maItems[li].SetHighlightable(true); // item slot 1
                maItems[li].SetSelectable(true);    // item slot 2
                Add(&maItems[li]);                  // group slot 7
            }
        }

        if (liNumActive > 0)
            HighlightIndex(0);   // group slot 12

        if (lbWrap != mbWrapped)
        {
            mbWrapped = lbWrap;
            muFlags |= KU_FLAG_QUERIED;
        }
        muFlags |= KU_FLAG_QUERIED;
    }

    // @0x8241EA88 -- retext the row at liIndex.
    void MenuComponent::SetText(s32 liIndex, const char* lpacText)
    {
        CGS_ASSERT(GetSelectable(liIndex) != 0, "Invalid selectable specified");
        CGS_ASSERT(lpacText != 0, "Invalid text specified");
        MenuItem* lpItem = GetSelectable(liIndex);
        lpItem->SetText(lpacText);
    }

    // @0x824E6428 -- clear the group + every row.
    void MenuComponent::Clear()
    {
        SelectableGroup::Clear();
        if (mbWrapped)
        {
            mbWrapped = false;
            muFlags |= KU_FLAG_QUERIED;
        }
        for (s32 li = 0; li < miNumMenuItems; ++li)
            maItems[li].Clear();
    }

    // @0x8240FE20 -- bounds-check [0,16), return the row pointer.
    MenuItem* MenuComponent::GetSelectable(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < KI_MAX_MENU_ITEMS,
                   "MenuComponent::GetSelectable() invalid index specified");
        return static_cast<MenuItem*>(SelectableGroup::GetSelectable(liIndex));
    }

    // @0x82488FC0 -- set the row's id + dirty it.
    MenuItem* MenuComponent::SetId(s32 liIndex, u64 luId)
    {
        CGS_ASSERT(GetSelectable(liIndex) != 0, "Invalid selectable specified");
        MenuItem* lpItem = GetSelectable(liIndex);
        lpItem->SetId(luId);
        lpItem->SetDirty();
        return lpItem;
    }

    // @0x824E2E38 -- make the row un-highlightable + un-selectable.
    void MenuComponent::DisableSelectable(s32 liIndex)
    {
        MenuItem* lpItem = GetSelectable(liIndex);
        lpItem->SetHighlightable(false);   // item slot 1
        lpItem->SetSelectable(false);      // item slot 2
    }

    // @? -- next-row highlight (SelectableGroup::HighlightNext, not quiet).
    bool MenuComponent::HighlightNext()
    {
        return SelectableGroup::HighlightNext(false);
    }

    // @0x824E4DE8 -- previous-row highlight (not quiet).
    bool MenuComponent::HighlightPrevious()
    {
        return SelectableGroup::HighlightPrevious(false);
    }

    // @0x824E2DE0 -- register every LIVE row's apt component name with the loading-screen
    // cache, so the owning screen's AreAllAptComponentsInitialised gate waits for the menu.
    // The X360 walks miNumMenuItems (`lwz +0x10B8`) rows from maItems[0]'s GuiComponent name
    // (`this + 596` == +0x238 + 0x1C) at the 0xE8 MenuItem stride, and hands each name to
    // the name-hashing GuiCache::AppendExpectedAptComponent overload (sub_824F87C0, which
    // measures the string and calls StateLoadingHelper::AppendExpectedAptComponent with its
    // CRC). Rows beyond miNumMenuItems are never registered -- the loop bound is the count
    // Construct wrote, not KI_MAX_MENU_ITEMS.
    void MenuComponent::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache)
    {
        for (s32 liRow = 0; liRow < miNumMenuItems; ++liRow)
        {
            lpGuiCache->AppendExpectedAptComponent(
                leFlow, maItems[liRow].CgsGui::GuiComponent::GetName());
        }
    }
}
