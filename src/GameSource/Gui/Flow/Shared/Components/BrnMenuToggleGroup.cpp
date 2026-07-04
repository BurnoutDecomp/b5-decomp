// ===================================================================================
// BrnGui::MenuToggleGroupVarSize<TI_SIZE>  -- template method bodies
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.cpp
//
// Homes the shared template bodies for the variable-size menu-toggle group. The four shipped
// instantiations (<2>/<3>/<4>/<5>) share ONE template body defined here; the per-N explicit
// instantiations at the tail emit each screen's code. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (the <3> recon fixed the canonical member names/signatures in BrnMenuToggleGroup.h; the <2>/<4>
// /<5> recons match those bodies store-for-store, differing only in TI_SIZE).
//
// The group dispatches the still-unnamed GuiComponent virtuals through the base SelectableGroup's
// modelled component vtable (mppVTable) BY SLOT, following the project's flat-vtable convention.
// Per-row (MenuToggle) virtuals are dispatched via the row's own leading vtable pointer (the row
// has no exposed mppVTable member, so the X360 `(*(*item + 0x18))(item)` slot-6 idiom is spelled
// through a reinterpret_cast to a vtable-pointer array).
//
// The compiler-synthesised default ctor (MenuToggleGroupVarSize()) is BLOCKED: its X360 body
// installs the group's own vtable pair then, per row, brings up three polymorphic sub-object
// vtable chains + a 100-element BrnGui::TextSelectionItem `vector constructor iterator', writing
// off_820746F0/off_820746EC/off_82055B70/off_8207306C/off_82073068/off_82073020/off_8207301C/
// off_82072F8C -- none of those sub-object vtable identities are reconstructed in this wave.
// Modelling them as raw addresses would assert unattested identities and zeroing would be wrong,
// so the ctor is left declared-but-undefined (NOT explicitly instantiated below). GROW this home
// when the MenuToggle row layout + vtable symbols land.
// ===================================================================================

#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"       // BrnGui::MenuToggle
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // BrnGui::SelectableGroup (base)
#include "GameSource/Gui/BrnGuiCache.h"                                // BrnGui::GuiCache / GuiFlow (AppendExpectedAptComponent forwards)
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h" // CgsDev::Message::gxMessageFilterFlags / KX_FILTER_GLOBAL

#include <cstring>   // std::strlen (Construct name-length assert)

namespace BrnGui
{
    // The X360 "get the group's currently-highlighted MenuToggle row" accessor (sub_824E21B8),
    // shared identically across every VarSize<N> (HighlightNextItem/HighlightPreviousItem both
    // call it, then forward MenuToggle::HighlightNext/HighlightPrevious). Its own home is not
    // reconstructed in this wave, so it is declared here as a file-local forward reference; the
    // gate is compile-only, so the definition lands with its own recon pass. Modelled taking the
    // group (as a SelectableGroup*, the value `this` passes straight through in r3) and returning
    // the highlighted MenuToggle*.
    namespace
    {
        MenuToggle* GetHighlightedToggle(SelectableGroup* lpGroup);
    }

    // Row (MenuToggle) component-virtual dispatch: `(*(*item + byteOff))(item)` with no arg /
    // one s32 arg. MenuToggle exposes no vtable-pointer member, so the leading vptr is reached
    // via a reinterpret_cast (the flat-vtable convention). byteOff>>2 == the slot index.
    static inline void RowVCall(MenuToggle* lpItem, s32 liByteOff)
    {
        void** lppVTable = *reinterpret_cast<void***>(lpItem);
        reinterpret_cast<void (*)(MenuToggle*)>(lppVTable[liByteOff >> 2])(lpItem);
    }
    static inline void RowVCall(MenuToggle* lpItem, s32 liByteOff, s32 liArg)
    {
        void** lppVTable = *reinterpret_cast<void***>(lpItem);
        reinterpret_cast<void (*)(MenuToggle*, s32)>(lppVTable[liByteOff >> 2])(lpItem, liArg);
    }

    // Group component-virtual dispatch through the base SelectableGroup's modelled vtable
    // (mppVTable): parameterless / one-s32-arg / one-row-arg forms.
    static inline void GroupVCall(SelectableGroup* lpGroup, s32 liByteOff)
    {
        reinterpret_cast<void (*)(SelectableGroup*)>(lpGroup->mppVTable[liByteOff >> 2])(lpGroup);
    }
    static inline void GroupVCall(SelectableGroup* lpGroup, s32 liByteOff, s32 liArg)
    {
        reinterpret_cast<void (*)(SelectableGroup*, s32)>(lpGroup->mppVTable[liByteOff >> 2])(lpGroup, liArg);
    }
    static inline void GroupVCallRow(SelectableGroup* lpGroup, s32 liByteOff, MenuToggle* lpItem)
    {
        reinterpret_cast<void (*)(SelectableGroup*, MenuToggle*)>(lpGroup->mppVTable[liByteOff >> 2])(lpGroup, lpItem);
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
            muFlags |= KU_FLAG_DIRTY;     // +0xC |= 0x10
        }

        for (s32 liIndex = 0; liIndex < miMaxItems; ++liIndex)
        {
            RowVCall(&maItems[liIndex], 0x18);   // row slot 6
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
            RowVCall(&maItems[liSlot], 0x18);
        }

        // Self-clear (this group's own component vtable slot 6).
        GroupVCall(this, 0x18);

        char lacName[0x80];
        for (s32 liIndex = 0; liIndex < miMaxItems; ++liIndex)
        {
            CGS_ASSERT(std::strlen(GetName()) + 3 < 0x80, "Name too long.");
            CgsCore::SPrintf(lacName, 0x80, "%s_%d", GetName(), liIndex);
            maItems[liIndex].Construct(lacName, lpStateInterface, lpacParentName,
                                       static_cast<u64>(0xFFFFFFFFu));
        }
    }

    // ---------------------------------------------------------------------------------
    // @ 0x8241DBB8 (<3>) / 0x82488C68 (<4>) / 0x82488D18 (<5>) / 0x824BA880 (<2>) --
    // bounds-check the index against [0, TI_SIZE) (the fixed template capacity, NOT the live
    // item count), then delegate to the base for the id and carry it back as the row pointer.
    template <s32 TI_SIZE>
    MenuToggle* MenuToggleGroupVarSize<TI_SIZE>::GetSelectable(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < TI_SIZE,
                   "MenuToggleGroup::GetSelectable() invalid index specified");
        return reinterpret_cast<MenuToggle*>(
            static_cast<uintptr_t>(SelectableGroup::GetSelectable(liIndex)));
    }

    // ---------------------------------------------------------------------------------
    // @ 0x824284A0 (<3>) / 0x824BD9C8 (<4>) / 0x8248E5F0 (<5>) / 0x824BD738 (<2>) --
    // highlight liItem within the row at liIndex: dispatch the row's embedded SelectableGroup
    // (+0xA8) vtable slot 12 (+0x30) with liItem; on success mark the row dirty and return true.
    template <s32 TI_SIZE>
    bool MenuToggleGroupVarSize<TI_SIZE>::HighlightItem(s32 liIndex, s32 liItem)
    {
        MenuToggle* lpToggle = GetSelectable(liIndex);

        SelectableGroup* lpInner = &lpToggle->mSelectableGroup;   // row +0xA8
        typedef bool (*HighlightFn)(SelectableGroup*, s32);
        HighlightFn lpfHighlight =
            reinterpret_cast<HighlightFn>(lpInner->mppVTable[12]);   // +0x30 == slot 12
        if (!lpfHighlight(lpInner, liItem))
        {
            return false;
        }
        lpToggle->muFlags |= MenuToggle::KU_FLAG_DIRTY;   // row +0xC |= 0x10
        return true;
    }

    // ---------------------------------------------------------------------------------
    // @ 0x82500EF0 (<3>) / 0x82500D68 (<4>) / 0x824F9BD8 (<5>) / 0x82500B38 (<2>) --
    // advance the highlight within the group's currently-highlighted row (X360 sub_824E21B8
    // resolves it), forwarding to MenuToggle::HighlightNext.
    template <s32 TI_SIZE>
    bool MenuToggleGroupVarSize<TI_SIZE>::HighlightNextItem()
    {
        return GetHighlightedToggle(this)->HighlightNext();
    }

    // ---------------------------------------------------------------------------------
    // @ 0x82500F18 (<3>) / 0x82500D90 (<4>) / 0x824F9C00 (<5>) / 0x82500B60 (<2>) --
    // retreat the highlight on the group's currently-highlighted row, forwarding to
    // MenuToggle::HighlightPrevious.
    template <s32 TI_SIZE>
    bool MenuToggleGroupVarSize<TI_SIZE>::HighlightPreviousItem()
    {
        return GetHighlightedToggle(this)->HighlightPrevious();
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

        GroupVCall(this, 0x18);

        for (s32 liIndex = 0; liIndex < miMaxItems; ++liIndex)
        {
            MenuToggle* lpItem = &maItems[liIndex];
            if (liIndex >= liItemCount)
            {
                RowVCall(lpItem, 0x00, 0);              // deactivate (row slot 0, arg 0)
            }
            else
            {
                RowVCall(lpItem, 0x00, 1);              // activate (row slot 0, arg 1)
                RowVCall(lpItem, 0x04, 1);              // row slot +0x04, arg 1
                RowVCall(lpItem, 0x08, 1);              // row slot +0x08, arg 1
                GroupVCallRow(this, 0x1C, lpItem);      // group slot +0x1C (register the row)
            }
        }

        if (liItemCount > 0)
        {
            GroupVCall(this, 0x30, 0);                  // group slot +0x30 (highlight first)
        }

        if ((lbWrapped != 0) != mbWrapped)
        {
            mbWrapped = (lbWrapped != 0);
            muFlags |= KU_FLAG_DIRTY;
        }
        muFlags |= KU_FLAG_DIRTY;                        // always dirty
    }

    // ---------------------------------------------------------------------------------
    // @ 0x82428288 (<3>) / 0x824BD7B0 (<4>) / 0x8248E3D8 (<5>) / 0x824BD520 (<2>) --
    // (re)configure the row at liIndex: validate index + selectable + payload, dirty the row
    // (row slot 6 == +0x18), flip its active state (row slot 0), then forward the text/id payload
    // to MenuToggle::SetupMenuToggle (active) or all-zero (inactive).
    template <s32 TI_SIZE>
    void MenuToggleGroupVarSize<TI_SIZE>::SetupToggle(
        s32 liIndex, s32 lbActive, const char* lpacText,
        const char** lppacOptions, u64* lpu64Ids, s32 liUnused)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex <= miMaxItems,
                   "MenuToggleGroupVarSize::SetupToggle() invalid index specified");
        CGS_ASSERT(GetSelectable(liIndex) != 0, "Invalid selectable specified");
        CGS_ASSERT(!(lpu64Ids == 0 && lbActive != 0), "Invalid text specified");

        MenuToggle* lpItem = &maItems[liIndex];
        RowVCall(lpItem, 0x18);                          // dirty the row (row slot 6)

        if (lbActive)
        {
            RowVCall(lpItem, 0x00, 1);                   // activate (row slot 0)
            lpItem->SetupMenuToggle(lbActive, lpacText, lppacOptions, lpu64Ids, liUnused);
        }
        else
        {
            RowVCall(lpItem, 0x00, 0);                   // deactivate (row slot 0)
            lpItem->SetupMenuToggle(0, 0, 0, 0, 0);
        }
    }

    // ---------------------------------------------------------------------------------
    // @ 0x824BA9D0 (<3>) / 0x82488300 (<4>) / 0x82488890 (<5>) -- tear the group down: run the
    // group's component unload virtual (group slot 6 == +0x18), reset miLoadedItems, then
    // Unloaded each of the miMaxItems rows.
    template <s32 TI_SIZE>
    void MenuToggleGroupVarSize<TI_SIZE>::Unloaded()
    {
        GroupVCall(this, 0x18);

        miLoadedItems = 0;
        for (s32 liIndex = 0; liIndex < miMaxItems; ++liIndex)
        {
            maItems[liIndex].Unloaded();
        }
    }

    // ---------------------------------------------------------------------------------
    // Per-N explicit instantiations of the shared template bodies (the four shipped screens use
    // <2>/<3>/<4>/<5>). Instantiated MEMBER-BY-MEMBER rather than whole-class, so the BLOCKED
    // compiler-synthesised ctor (declared-but-undefined above) is NOT forced -- a whole-class
    // `template class MenuToggleGroupVarSize<N>;` would require its (unreconstructed) body.
    // ---------------------------------------------------------------------------------
#define BRN_INSTANTIATE_MENUTOGGLEGROUP(N)                                                        \
    template void        MenuToggleGroupVarSize<N>::AppendExpectedAptComponent(GuiFlow, GuiCache*, bool); \
    template void        MenuToggleGroupVarSize<N>::Clear();                                       \
    template void        MenuToggleGroupVarSize<N>::Construct(const char*, CgsGui::StateInterface*,\
                                                              s32, const char*, u64);              \
    template MenuToggle* MenuToggleGroupVarSize<N>::GetSelectable(s32);                            \
    template bool        MenuToggleGroupVarSize<N>::HighlightItem(s32, s32);                       \
    template bool        MenuToggleGroupVarSize<N>::HighlightNextItem();                           \
    template bool        MenuToggleGroupVarSize<N>::HighlightPreviousItem();                       \
    template void        MenuToggleGroupVarSize<N>::SetupGroup(s32, u8);                           \
    template void        MenuToggleGroupVarSize<N>::SetupToggle(s32, s32, const char*,             \
                                                               const char**, u64*, s32);           \
    template void        MenuToggleGroupVarSize<N>::Unloaded();

    BRN_INSTANTIATE_MENUTOGGLEGROUP(2)
    BRN_INSTANTIATE_MENUTOGGLEGROUP(3)
    BRN_INSTANTIATE_MENUTOGGLEGROUP(4)
    BRN_INSTANTIATE_MENUTOGGLEGROUP(5)

#undef BRN_INSTANTIATE_MENUTOGGLEGROUP
}
