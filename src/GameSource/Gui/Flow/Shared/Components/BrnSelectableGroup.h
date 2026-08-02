#pragma once

// ===================================================================================
// BrnGui::SelectableGroup  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h
//   class:BrnGui::SelectableGroup
//
// A group of up to 100 selectable items (BrnGui::Selectable*) with a current-highlight
// cursor and a wrap flag; the shared base of the menu component groups (BrnGui::
// MenuComponent / MenuToggleGroup / TableRow / the online screens). Reconstructed from
// the X360 ARTIST build (semantic parity by named members; the x64 gate widens the
// stored Selectable pointers to 8 bytes, so byte offsets are documentary only).
//
// Recovered layout (guest 32-bit offsets in the comments):
//   +0x00 : component vtable pointer (modelled -- see note below)
//   +0x0C : muFlags       -- the dirty/queried flag byte (methods OR in 0x10)
//   +0x10 : mu64SelectedId-- the group's own cached id (Clear stores KU64_NO_ID)
//   +0xA4 : miSelectableCount   (s8, signed bounds checks)
//   +0xA5 : miHighlightedIndex  (s8, -1 == none)
//   +0xA6 : mbWrapped           (bool, HighlightNext/Previous wrap-around)
//   +0xA8 : maSelectables[100]  (Selectable* -- Add() appends, GetSelectable() returns)
//
// The component vtable is modelled as the named `mppVTable` data pointer (the project's
// flat-vtable convention: derived screens dispatch the component virtuals through it BY
// SLOT -- see BrnTableRow/BrnMenuToggleGroup). The reconstructed methods below call each
// other and the item Selectable virtuals directly by name (semantic parity), so the menu
// flow does not depend on mppVTable being a live table.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"  // CgsGui::GuiComponent (the group's identity base)

// Forward-declared: the group stores Selectable* and calls their virtuals in the .cpp
// (which includes BrnSelectable.h). Pointer members need only the incomplete type here.
namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    struct Selectable;

    struct SelectableGroup
    {
        static const s32 KI_MAX_SELECTABLES = 100;   // GetHighlighted / Add bound (0x64)
        static const s32 KI_MAX_ADD         = 128;   // Add's hard index cap (0x80)
        static const u8  KU_FLAG_QUERIED    = 0x10;   // *(this+0xC) |= 0x10 (dirty/queried)
        // The "no id" reset value Clear stores into mu64SelectedId (X360 `li -1 ; clrldi 32`).
        static const u64 KU64_NO_ID         = 0xFFFFFFFFull;

        // @ 0x824E65B8 -- name the group, wire its state interface + apt id, null the
        // selectable slots, then Clear(). (a4 = parent name, a5 = apt id.)
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luAptId);
        // @ 0x824E3100 -- clear every occupied slot (reset each item + drop the pointer),
        // then reset the count/highlight/id. Component vtable slot 6.
        void Clear();
        // @ 0x824E32F8 -- append a selectable pointer to the next free slot, ++count, dirty.
        // Component vtable slot 7.
        void Add(Selectable* lpSelectable);

        // @ 0x8240FC98 -- bounds-check index against the count, set the queried flag, return
        // maSelectables[index].
        Selectable* GetSelectable(s32 liIndex);
        // @ 0x8240FD60 -- bounds-check the highlight cursor, return maSelectables[cursor].
        Selectable* GetHighlighted();
        // @ 0x8241EA28 -- assert a row is highlighted, return its id.
        u64 GetHighlightedId();
        // @ 0x824E4158 -- linear-search the items for one whose id matches, return its index
        // (-1 if none).
        s32 GetIndexFromId(u64 luId);

        // @ 0x824E3CE0 -- highlight the item at index (un-highlight the previous; -1 clears the
        // highlight). Returns true when the highlight changed. Component vtable slot 12.
        bool HighlightIndex(s32 liIndex);
        // @ 0x824E6720 -- resolve the id to an index, then HighlightIndex it.
        bool HighlightId(u64 luId);
        // @ 0x824E3808 -- move the highlight to the next selectable/highlightable/non-hidden
        // item; lbQuiet suppresses the HighlightIndex call. Wraps when mbWrapped.
        bool HighlightNext(bool lbQuiet);
        // @ 0x824E3A78 -- as HighlightNext, toward the previous item.
        bool HighlightPrevious(bool lbQuiet);

        // @ 0x824E3490 -- (re)enable the item at index (Active/Highlightable/Selectable on).
        void Enable(s32 liIndex);
        // @ 0x824E3620 -- disable the item at index (Active off), asserting it is not the last.
        void Disable(s32 liIndex);
        // @ 0x824E3EC8 -- Select() the currently-highlighted item if it is selectable.
        void Select();
        // @ 0x824E3FE0 -- when the group is dirty, Update() every item and clear the dirty bit.
        // Component vtable slot 5.
        void Update();

        // ADDITIVE GROW (BrnGui::CarSelectVehicle::OnEnter @0x824C9470). The group head IS a
        // BrnGui::Selectable on the console (this class models that head as reserved storage
        // plus the muFlags byte, so it cannot inherit the method), and OnEnter reaches
        // Selectable::SetHighlightable through the group's component vtable SLOT 1:
        // `(*(*(this + 0x2370) + 4))(this + 0x2370, 1)`. muFlags IS Selectable::mxFlags --
        // both live at +0x0C -- so this reproduces Selectable::SetHighlightable @0x824E5730
        // store for store on the same byte: set/clear bit 1, dirty and report a change.
        bool SetHighlightable(bool lbHighlightable)
        {
            const u8 luFlags = muFlags;
            if (lbHighlightable == (((luFlags >> 1) & 1) != 0))
                return false;
            muFlags = static_cast<u8>(lbHighlightable ? (luFlags | 0x02) : (luFlags ^ 0x02));
            muFlags = static_cast<u8>(muFlags | KU_FLAG_QUERIED);
            return true;
        }

        // @ 0x824E31D8 -- set one item's id + dirty it.
        void SetSelectableId(s32 liIndex, u64 luId);
        // @ 0x824E3248 -- set every item's id (from the array, or its index when null) + dirty.
        void SetIds(const u64* lpaIds);

        // The component's apt name (delegates to the group's GuiComponent identity).
        const char* GetName() const { return mGuiComponentBase.GetName(); }

        // Members carrying the recovered fields. The leading head (+0x00..+0xA3) is the
        // multiply-inherited Selectable + GuiComponent base pair, modelled as reserved spans
        // (the x64 gate accesses everything by name; the spans keep the recovered fields
        // labelled at their guest offsets for clarity).
        void** mppVTable;               // +0x00 (modelled component vtable pointer)
        u8  maHeadReserved[0x08];       // +0x04..+0x0B (Selectable text; not touched here)
        u8  muFlags;                    // +0x0C (methods OR in 0x10)
        u8  maPadD[3];                  // +0x0D..+0x0F (align to the id word)
        u64 mu64SelectedId;             // +0x10 (Clear stores KU64_NO_ID)
        // +0x18..+0xA3 -- the group's GuiComponent identity (name + hashed name + state
        // interface). The console multiply-inherits GuiComponent here; modelled by
        // composition with the real type (Construct forwards to mGuiComponentBase.Construct,
        // exactly the X360 `GuiComponent::Construct(this + 0x18, ...)`). Byte-sized to match.
        CgsGui::GuiComponent mGuiComponentBase; // +0x18 (0x8C bytes)
        s8  miSelectableCount;          // +0xA4
        s8  miHighlightedIndex;         // +0xA5 (-1 == none)
        bool mbWrapped;                 // +0xA6
        u8  maPadA7[1];                 // +0xA7 (align to the pointer array)
        Selectable* maSelectables[KI_MAX_SELECTABLES]; // +0xA8 (this[42 + i] on the console)
    };
}
