#pragma once

// ===================================================================================
// BrnGui::SelectableGroup  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h
//   class:BrnGui::SelectableGroup
//
// A group of up to 100 selectable items (BrnGui::Selectable*) with a current-highlight
// cursor and a wrap flag; the shared base of the menu component groups (BrnGui::
// MenuComponent / MenuToggleGroupVarSize / TextSelection / ColourSelection / Table /
// TableRow). Reconstructed from the X360 ARTIST build (the x64 gate widens pointers to
// 8 bytes, so byte offsets are documentary only).
//
// CLASS SHAPE. A group IS a Selectable (a group can itself be an item of another group:
// Table::SetupTable adds its TableRows) and a GuiComponent:
//   +0x00 : Selectable           -- vptr, selection text, mxFlags @+0x0C, mId @+0x10
//   +0x18 : CgsGui::GuiComponent -- its own vptr (one slot: Construct), name, hash, iface
//   +0xA4 : miSelectableCount   (s8, signed bounds checks)
//   +0xA5 : miHighlightedIndex  (s8, -1 == none)
//   +0xA6 : mbWrapped           (bool, HighlightNext/Previous wrap-around)
//   +0xA8 : maSelectables[100]  (Selectable* -- Add() appends, GetSelectable() returns)
// The DecFIGS dump prints only one base (`: public CgsGui::GuiComponent`), as it does for
// MenuToggle; the Selectable head at +0x00 is attested by every group vtable below, whose
// slots 0..3 are Selectable::SetActive/SetHighlightable/SetSelectable/SetHighlighted.
//
// VTABLE (off_82072F98, the plain SelectableGroup table built by the CarSelectVehicle and
// ImageGalleryState ctors; read from the ARTIST .rdata):
//    0..3  Selectable::SetActive / SetHighlightable / SetSelectable / SetHighlighted
//    4     SelectableGroup::Select            @0x824E3EC8
//    5     SelectableGroup::Update            @0x824E3FE0
//    6     SelectableGroup::Clear             @0x824E3100
//    7     SelectableGroup::Add               @0x824E32F8
//    8     SelectableGroup::Enable            @0x824E3490
//    9     SelectableGroup::Disable           @0x824E3620
//   10     SelectableGroup::HighlightNext     @0x824E3808
//   11     SelectableGroup::HighlightPrevious @0x824E3A78
//   12     SelectableGroup::HighlightIndex    @0x824E3CE0
// The DecFIGS DWARF marks exactly these virtual (BrnSelectableGroup.h); derived groups
// override 4..6 and append their own slots from 13 (see BrnTable.h / BrnTableRow.h /
// BrnMenuComponent.h / BrnMenuToggleGroup.h).
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"  // CgsGui::GuiComponent (second base)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"     // BrnGui::Selectable (primary base)

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    struct SelectableGroup : public Selectable, public CgsGui::GuiComponent
    {
        static const s32 KI_MAX_SELECTABLES = 100;   // GetHighlighted / Add bound (0x64)
        static const s32 KI_MAX_ADD         = 128;   // Add's hard index cap (0x80)
        // The Selectable dirty bit (`*(this+0xC) |= 0x10`) under the name the group bodies
        // have always used for it.
        static const u8  KU_FLAG_QUERIED    = E_FLAG_DIRTY;
        // The "no id" reset value Clear stores into the group's own id (X360 `li -1 ; clrldi 32`).
        static const u64 KU64_NO_ID         = 0xFFFFFFFFull;

        // @ 0x824E65B8 -- name the group, wire its state interface + apt id, null the
        // selectable slots, then Clear(). (a4 = parent name, a5 = apt id.)
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luAptId);

        // @ 0x824E3EC8 -- Select() the currently-highlighted item if it is selectable. Slot 4.
        virtual void Select();
        // @ 0x824E3FE0 -- when the group is dirty, Update() every item and clear the dirty bit.
        // Slot 5.
        virtual void Update();
        // @ 0x824E3100 -- clear every occupied slot (reset each item + drop the pointer),
        // then reset the count/highlight/id. Slot 6.
        virtual void Clear();
        // @ 0x824E32F8 -- append a selectable pointer to the next free slot, ++count, dirty.
        // Slot 7.
        virtual void Add(Selectable* lpSelectable);
        // @ 0x824E3490 -- (re)enable the item at index (Active/Highlightable/Selectable on).
        // Slot 8.
        virtual void Enable(s32 liIndex);
        // @ 0x824E3620 -- disable the item at index (Active off), asserting it is not the last.
        // Slot 9.
        virtual void Disable(s32 liIndex);
        // @ 0x824E3808 -- move the highlight to the next selectable/highlightable/non-hidden
        // item; lbQuiet suppresses the HighlightIndex call. Wraps when mbWrapped. Slot 10.
        virtual bool HighlightNext(bool lbQuiet);
        // @ 0x824E3A78 -- as HighlightNext, toward the previous item. Slot 11.
        virtual bool HighlightPrevious(bool lbQuiet);
        // @ 0x824E3CE0 -- highlight the item at index (un-highlight the previous; -1 clears the
        // highlight). Returns true when the highlight changed. Slot 12.
        virtual bool HighlightIndex(s32 liIndex);

        // @ 0x8240FC98 -- bounds-check index against the count, set the queried flag, return
        // maSelectables[index].
        Selectable* GetSelectable(s32 liIndex);
        // @ 0x8240FD60 -- bounds-check the highlight cursor, return maSelectables[cursor].
        Selectable* GetHighlighted();
        // @ 0x8241EA28 -- assert a row is highlighted, return its id.
        u64 GetHighlightedId();
        // The highlight cursor itself (-1 == none). DWARF BrnSelectableGroup.h:203 declares it;
        // callers that need the INDEX rather than the item read the member inline -- e.g.
        // CrashNavOptions::HandleOptionChanged @0x824D9408 switches on it.
        s32 GetHighlightedIndex() const { return static_cast<s32>(miHighlightedIndex); }
        // @ 0x824E4158 -- linear-search the items for one whose id matches, return its index
        // (-1 if none).
        s32 GetIndexFromId(u64 luId);
        // @ 0x824E6720 -- resolve the id to an index, then HighlightIndex it.
        bool HighlightId(u64 luId);

        // @ 0x824E31D8 -- set one item's id + dirty it.
        void SetSelectableId(s32 liIndex, u64 luId);
        // @ 0x824E3248 -- set every item's id (from the array, or its index when null) + dirty.
        void SetIds(const u64* lpaIds);

        // The flag surface (IsHighlighted, SetDirty, the Set* virtuals) is the Selectable
        // base's; the apt name (GetName) is the GuiComponent base's.

        s8  miSelectableCount;          // +0xA4
        s8  miHighlightedIndex;         // +0xA5 (-1 == none)
        bool mbWrapped;                 // +0xA6
        Selectable* maSelectables[KI_MAX_SELECTABLES]; // +0xA8 (this[42 + i] on the console)
    };
}
