#pragma once

// ===================================================================================
// BrnGui::MenuToggleGroupVarSize<TI_SIZE>  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h
//   class:BrnGui::MenuToggleGroupVarSize<3>  (template family; <2>/<4>/<5> instantiate too)
//
// A menu component holding a fixed array of TI_SIZE BrnGui::MenuToggle rows on top of a
// BrnGui::SelectableGroup base. DWARF home BrnMenuToggleGroup.h (the non-templated
// BrnGui::MenuToggleGroup is the <8> shape; the shipped screens use the VarSize<N>
// template with N in {2,3,4,5}). The var-size template shares the same field layout,
// only maItems' element count differing.
//
// Recovered X360 layout (base ptr is u8*, so displacement == byte offset; the gate
// compiles 64-bit so members are declared BY NAME):
//   +0x00  : Selectable vptr (off_820746F0 / 82074790 / 82074750 / 820730AC for <2>..<5>)
//   +0x0C  : Selectable::mxFlags -- dirty bit 0x10 (SetDirty) on any mutation
//   +0xA4  : s8  base selectable count (SelectableGroup::miSelectableCount)
//   +0xA6  : u8  mbWrapped      -- SetupGroup/Clear write it; dirty on change
//   +0x238 : MenuToggle maItems[TI_SIZE]   (element stride 0xE98 == sizeof(MenuToggle))
//   +0x2E00: s32 miMaxItems     -- item count Construct sets from the arg; loops read it
//   +0x2E04: s32 miLoadedItems  -- Unloaded/Construct reset it to 0
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (VarSize<3> instantiation):
//   MenuToggleGroupVarSize<3>::MenuToggleGroupVarSize<3> @ 0x82500DB8
//   MenuToggleGroupVarSize<3>::SetupGroup                @ 0x8241DA08
//   MenuToggleGroupVarSize<3>::SetupToggle               @ 0x82428288
//   MenuToggleGroupVarSize<3>::Unloaded                  @ 0x824BA9D0
//   MenuToggleGroupVarSize<3>::HighlightPreviousItem     @ 0x82500F18
// (Construct/Clear/GetSelectable/HighlightItem/HighlightNextItem/GetHighlightedItem*/
//  AppendExpectedAptComponent are separate ledger functions -- declare-only here; GROW
//  this home when they land, do NOT fork the type.)
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // BrnGui::SelectableGroup (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"       // BrnGui::MenuToggle (maItems element)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                // CgsCore::SPrintf (Construct)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint (AppendExpectedAptComponent)
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h" // CgsDev::Message::gxMessageFilterFlags / KX_FILTER_GLOBAL
#include <cstring>   // std::strlen (Construct name-length assert)

namespace BrnGui
{
    // The variable-size menu-toggle group. TI_SIZE == the compile-time item capacity
    // (the shipped screens use <2>/<3>/<4>/<5>); the on-disk field layout is identical
    // across sizes but for the maItems element count.
    template <s32 TI_SIZE>
    struct MenuToggleGroupVarSize : public SelectableGroup
    {
        // @ 0x82500DB8 (<3>) / 0x82500AC0 (<2>) -- install the group vtable and
        // default-construct the TI_SIZE MenuToggle rows (each brings up its embedded
        // TextSelection arrays). Every store in the X360 body is a sub-object vtable install
        // that C++ emits implicitly here, so the reconstruction is an empty body (2026-08-02;
        // it used to be declared-but-undefined and flagged BLOCKED).
        MenuToggleGroupVarSize();

        // Group Construct (DWARF): validate args, base-construct the SelectableGroup, construct
        // + clear the TI_SIZE toggle rows, record miMaxItems / reset miLoadedItems, then name and
        // (re)construct the miMaxItems live rows "<name>_<idx>".
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       s32 liItemCount, const char* lpacParentName, u64 luAptId);

        // Group virtual (DWARF `virtual void Clear()`), slot 6; resets the selectable set,
        // drops the wrap flag (dirtying), then clears each live row via its component clear
        // virtual.
        virtual void Clear();

        // Forward each live row's expected apt component onto the GUI cache (warns via the
        // filter-gated debug print if the group was never SetupGroup'd).
        void AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache, bool lbArg);

        // Highlight liItem within the row at liIndex (via the row's inner SelectableGroup
        // vtable slot 12); on success dirty the row and return true, else false.
        bool HighlightItem(s32 liIndex, s32 liItem);

        // @ 0x8241DA08 (<3>) -- activate the first liItemCount rows and deactivate the
        // rest, set the wrap flag, mark dirty.
        void SetupGroup(s32 liItemCount, u8 lbWrapped);

        // @ 0x82428288 (<3>) / 0x824BD520 (<2>) -- (re)configure the toggle at liIndex:
        // enable/disable it and forward the option payload to MenuToggle::SetupMenuToggle.
        //
        // ⭐⭐ PARAMETER ROLES CORRECTED 2026-08-02 (the CarSelectLivery wave -- this class's
        // FIRST caller). The committed declaration read
        //   SetupToggle(s32 liIndex, s32 lbActive, const char* lpacText,
        //               const char** lppacOptions, u64* lpu64Ids, s32 liUnused)
        // which forwarded r5..r9 to MenuToggle::SetupMenuToggle positionally -- so the BEHAVIOUR
        // was right, but the LAST parameter was typed `s32` while the value the console puts in
        // r9 is the `u64*` id array. On x64 that TRUNCATES A POINTER TO 32 BITS. The two
        // call sites in BrnGui::CarSelectLivery::SetupComponents settle the real roles from the
        // asm (@0x824C8554 and @0x824C8608):
        //   r3 = this, r4 = index, r5 = numOptions, r6 = active, r7 = text,
        //   r8 = options[], r9 = ids[]
        // -- e.g. (0, muNumUnlockedLiveryCars, 1, "$CAR_MOD_LIVERY", $LIVERY_0..7, liveryIds)
        // and (1, 3, 1, "$CAR_MOD_PAINT_FINISH", $PAINT_GLOSS/METALLIC/PEARLESCENT, {0,1,2}).
        void SetupToggle(s32 liIndex, s32 liNumOptions, bool lbActive, const char* lpacText,
                         const char** lppacOptions, u64* lpu64Ids);

        // @ 0x824BA9D0 (<3>) -- tear the group down: run the base Unloaded, reset
        // miLoadedItems, then Unloaded each of the miMaxItems rows.
        void Unloaded();

        // The two virtuals the template appends to the SelectableGroup table, in its slot
        // order (13 then 14; e.g. <3>: 0x82500EF0 / 0x82500F18).
        // @ 0x82500EF0 (<3>) -- advance the highlight on the currently-highlighted row.
        virtual bool HighlightNextItem();
        // @ 0x82500F18 (<3>) -- retreat the highlight on the currently-highlighted row.
        virtual bool HighlightPreviousItem();

        // Fixed row indices fold the capacity assertion at ARTIST 0x82440440.
        MenuToggle* GetSelectable(s32 liIndex)
        {
            CGS_ASSERT(liIndex >= 0 && liIndex < TI_SIZE,
                CGS_ASSERT_STREAM_MESSAGE_AT("MenuToggleGroup::GetSelectable() invalid index specified",
                    "..\\..\\..\\GameSource\\Gui/Flow/Shared/Components/BrnMenuToggleGroup.h", 241));

            return static_cast<MenuToggle*>(SelectableGroup::GetSelectable(liIndex));
        }

        // ----- recovered layout -----
        static const s32 KI_ITEMS_OFFSET   = 0x238;   // maItems @ +0x238
        static const s32 KI_ITEM_STRIDE    = 0xE98;   // sizeof(MenuToggle) == 3736

        MenuToggle maItems[TI_SIZE];   // +0x238 (element stride 0xE98)
        s32        miMaxItems;         // +0x2E00 (item count; loops read it)
        s32        miLoadedItems;      // +0x2E04 (Unloaded/Construct reset to 0)
    };
}

// The template member bodies live in this header, as ARTIST shows: each instantiation's
// members are COMDATs spread over the TUs that use them (<3>::Clear @0x82500E78 in the flow
// TU's range, <5>::Clear @0x824F9B60 elsewhere), and callers inline them across TUs
// (OnlineScoreboards::SetupCategories @0x8248F838 inlines <3>::HighlightItem; its body
// is the reason UpdateSetupComponents @0x824918C8 keeps SetupCategories out of line).
// BrnMenuToggleGroup.cpp instantiates every member explicitly so each copy is still
// emitted and measured in one object.
namespace BrnGui
{
    // ---------------------------------------------------------------------------------
    // @ 0x82500AC0 (<2>) -- the compiler-synthesised default ctor. Its X360 body is a run of
    // sub-object vtable installs plus the rows' `vector constructor iterator's; C++ emits all
    // of them implicitly here. It stores no field values (Construct does that).
    template <s32 TI_SIZE>
    MenuToggleGroupVarSize<TI_SIZE>::MenuToggleGroupVarSize()
    {
    }

    // ---------------------------------------------------------------------------------
    // @ 0x8241DC68 (<3>) / 0x82488510 (<4>) / 0x82488AA0 (<5>) / 0x824BA930 (<2>) --
    // forward each live row's expected apt component to the GUI cache. If the group was never
    // sized via SetupGroup (base miSelectableCount @+0xA4 still non-zero) AND the global
    // message-filter bit is set, the non-gating warning fires. The loop count is the base count
    // when non-zero, else the configured miMaxItems.
    template <s32 TI_SIZE>
    void MenuToggleGroupVarSize<TI_SIZE>::AppendExpectedAptComponent(
        GuiFlow leFlow, GuiCache* lpGuiCache, bool lbArg)
    {
        if (miSelectableCount != 0
            && (CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "Do not know how many toggles in group - should call SetupGroup (and not then clear it!) prior to this!";
        }

        s32 liCount = miSelectableCount;
        if (liCount == 0)
        {
            liCount = miMaxItems;
        }

        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            maItems[liIndex].AppendExpectedAptComponent(leFlow, lpGuiCache, lbArg);
        }
    }

    // ---------------------------------------------------------------------------------
    // @ 0x82500E78 (<3>) / 0x82500CF0 (<4>) / 0x824F9B60 (<5>) / 0x82500AC0 (<2>) --
    // base Clear, drop the wrap flag (dirtying), then clear each live row via its component
    // clear virtual (row vtable slot 6 == byte +0x18).
    template <s32 TI_SIZE>
    void MenuToggleGroupVarSize<TI_SIZE>::Clear()
    {
        SelectableGroup::Clear();

        if (mbWrapped)                    // base +0xA6
        {
            mbWrapped = false;
            SetDirty();     // +0xC |= 0x10
        }

        for (s32 liIndex = 0; liIndex < miMaxItems; ++liIndex)
        {
            maItems[liIndex].Clear();            // row slot 6
        }
    }

    // ---------------------------------------------------------------------------------
    // @ 0x8241D728 (<3>) / 0x82488020 (<4>) / 0x824885B0 (<5>) / 0x824BA3F0 (<2>) --
    // validate args, base-construct the SelectableGroup, construct + clear the TI_SIZE embedded
    // rows, self-clear (group slot 6), record miMaxItems / reset miLoadedItems, then name and
    // (re)construct the miMaxItems live rows "<name>_<idx>".
    template <s32 TI_SIZE>
    void MenuToggleGroupVarSize<TI_SIZE>::Construct(
        const char* lpacName, CgsGui::StateInterface* lpStateInterface,
        s32 liItemCount, const char* lpacParentName, u64 luAptId)
    {
        CGS_ASSERT(lpStateInterface != 0, "Invalid stat interface passed");
        CGS_ASSERT(lpacName != 0, "Invalid name passed in");
        CGS_ASSERT(liItemCount <= TI_SIZE,
                   "Too many items in menu for current setting in SelectableGroup");

        SelectableGroup::Construct(lpacName, lpStateInterface, lpacParentName, luAptId);

        miMaxItems    = liItemCount;   // +0x2E00
        miLoadedItems = 0;             // +0x2E04

        // Construct every embedded row and clear each (row component vtable slot 6).
        for (s32 liSlot = 0; liSlot < TI_SIZE; ++liSlot)
        {
            maItems[liSlot].Construct(lpacName, lpStateInterface, lpacParentName, luAptId);
            maItems[liSlot].Clear();
        }

        // Self-clear (this group's own component vtable slot 6 -- the DERIVED Clear).
        Clear();

        // X360 @0x8241D728 (the <3> instantiation; <2>/<4>/<5> are identical): the length
        // assert measures the COMPOSED name (this+0x1C, GetName()), but the SPrintf's %s is
        // `a2` -- the SHORT name the caller passed ("filterToggle") -- and the item is then
        // constructed with the same lpacParentName, so GuiComponent::SetName composes
        // "<parent>_<short>_<i>" exactly once. Printing GetName() here doubled the parent
        // ("CrashNavPanel_mc_CrashNavPanel_mc_filterToggle_0"), a name no apt clip carries, so
        // the crash-nav map's expected-component gate never passed (2026-09-10).
        char lacName[0x80];
        for (s32 liIndex = 0; liIndex < miMaxItems; ++liIndex)
        {
            CGS_ASSERT(std::strlen(GetName()) + 3 < 0x80, "Name too long.");
            CgsCore::SPrintf(lacName, 0x80, "%s_%d", lpacName, liIndex);
            maItems[liIndex].Construct(lacName, lpStateInterface, lpacParentName,
                                       static_cast<u64>(0xFFFFFFFFu));
        }
    }

    // ---------------------------------------------------------------------------------
    // @ 0x824284A0 (<3>) / 0x824BD9C8 (<4>) / 0x8248E5F0 (<5>) / 0x824BD738 (<2>) --
    // highlight liItem within the row at liIndex: dispatch the row's embedded SelectableGroup
    // (+0xA8) vtable slot 12 (+0x30) with liItem; on success mark the row dirty and return true.
    template <s32 TI_SIZE>
    bool MenuToggleGroupVarSize<TI_SIZE>::HighlightItem(s32 liIndex, s32 liItem)
    {
        MenuToggle* lpToggle = GetSelectable(liIndex);

        // The row's embedded option selection (+0xA8), vtable slot 12 == HighlightIndex.
        // Success arm first, `return false` last (ARTIST 0x824284E0 beq to the li r3,0 tail).
        if (lpToggle->mItemText.HighlightIndex(liItem))
        {
            lpToggle->SetDirty();                        // row +0xC |= 0x10
            return true;
        }
        return false;
    }

    // ---------------------------------------------------------------------------------
    // @ 0x82500EF0 (<3>) / 0x82500D68 (<4>) / 0x824F9BD8 (<5>) / 0x82500B38 (<2>) --
    // advance the highlight within the group's currently-highlighted row (X360 sub_824E21B8
    // resolves it), forwarding to MenuToggle::HighlightNext.
    template <s32 TI_SIZE>
    bool MenuToggleGroupVarSize<TI_SIZE>::HighlightNextItem()
    {
        return static_cast<MenuToggle*>(GetHighlighted())->HighlightNext();
    }

    // ---------------------------------------------------------------------------------
    // @ 0x82500F18 (<3>) / 0x82500D90 (<4>) / 0x824F9C00 (<5>) / 0x82500B60 (<2>) --
    // retreat the highlight on the group's currently-highlighted row, forwarding to
    // MenuToggle::HighlightPrevious.
    template <s32 TI_SIZE>
    bool MenuToggleGroupVarSize<TI_SIZE>::HighlightPreviousItem()
    {
        return static_cast<MenuToggle*>(GetHighlighted())->HighlightPrevious();
    }

    // ---------------------------------------------------------------------------------
    // @ 0x8241DA08 (<3>) / 0x82488360 (<4>) / 0x824888F0 (<5>) / 0x824BA6D0 (<2>) --
    // activate the first liItemCount rows (activate + two follow-on row virtuals + a group
    // callback), deactivate the rest, highlight the first, then latch the wrap flag; always
    // mark dirty. Group virtual +0x18 begins the setup; +0x1C is the per-activated-row callback;
    // +0x30 selects the first row when any were activated.
    template <s32 TI_SIZE>
    void MenuToggleGroupVarSize<TI_SIZE>::SetupGroup(s32 liItemCount, u8 lbWrapped)
    {
        CGS_ASSERT(liItemCount >= 0 && liItemCount <= miMaxItems,
                   "MenuToggleGroupVarSize::SetupGroup() item countspecified");

        Clear();

        for (s32 liIndex = 0; liIndex < miMaxItems; ++liIndex)
        {
            MenuToggle* lpItem = &maItems[liIndex];
            if (liIndex >= liItemCount)
            {
                lpItem->SetActive(false);               // row slot 0
            }
            else
            {
                lpItem->SetActive(true);                // row slot 0
                lpItem->SetHighlightable(true);         // row slot 1 (+0x04)
                lpItem->SetSelectable(true);            // row slot 2 (+0x08)
                Add(lpItem);                            // group slot 7 (+0x1C)
            }
        }

        if (liItemCount > 0)
        {
            HighlightIndex(0);                          // group slot 12 (+0x30)
        }

        if ((lbWrapped != 0) != mbWrapped)
        {
            mbWrapped = (lbWrapped != 0);
            SetDirty();
        }
        SetDirty();                        // always dirty
    }

    // ---------------------------------------------------------------------------------
    // @ 0x82428288 (<3>) / 0x824BD7B0 (<4>) / 0x8248E3D8 (<5>) / 0x824BD520 (<2>) --
    // (re)configure the row at liIndex: validate index + selectable + payload, dirty the row
    // (row slot 6 == +0x18), flip its active state (row slot 0), then forward the text/id payload
    // to MenuToggle::SetupMenuToggle (active) or all-zero (inactive).
    template <s32 TI_SIZE>
    void MenuToggleGroupVarSize<TI_SIZE>::SetupToggle(
        s32 liIndex, s32 liNumOptions, bool lbActive, const char* lpacText,
        const char** lppacOptions, u64* lpu64Ids)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex <= miMaxItems,
                   "MenuToggleGroupVarSize::SetupToggle() invalid index specified");
        CGS_ASSERT(GetSelectable(liIndex) != 0, "Invalid selectable specified");
        // X360 @0x82428288 (<3>): `if (!a6 && a3)` -- the OPTION-TEXT array (r8) is null while
        // liNumOptions (r5) is non-zero. The row text may legitimately be null: CrashNavPanel's
        // `SetupToggle(KI_TOGGLE_IN_EVENT, 0, true, 0, 0, 0)` is the console's own call.
        CGS_ASSERT(!(lppacOptions == 0 && liNumOptions != 0), "Invalid text specified");   // h:397

        MenuToggle* lpItem = &maItems[liIndex];
        lpItem->Clear();                                 // row slot 6

        // The console branches on liNumOptions (`if (a3)`), not on the wrap flag: a row with
        // no options is set INACTIVE and cleared whatever lbActive says.
        if (liNumOptions != 0)
        {
            lpItem->SetActive(true);                     // row slot 0
            lpItem->SetupMenuToggle(liNumOptions, lbActive, lpacText, lppacOptions, lpu64Ids);
        }
        else
        {
            lpItem->SetActive(false);                    // row slot 0
            lpItem->SetupMenuToggle(0, false, 0, 0, 0);
        }
    }

    // ---------------------------------------------------------------------------------
    // @ 0x824BA9D0 (<3>) / 0x82488300 (<4>) / 0x82488890 (<5>) -- tear the group down: run the
    // group's component unload virtual (group slot 6 == +0x18), reset miLoadedItems, then
    // Unloaded each of the miMaxItems rows.
    template <s32 TI_SIZE>
    void MenuToggleGroupVarSize<TI_SIZE>::Unloaded()
    {
        Clear();

        miLoadedItems = 0;
        for (s32 liIndex = 0; liIndex < miMaxItems; ++liIndex)
        {
            maItems[liIndex].Unloaded();
        }
    }
}
