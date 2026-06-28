#pragma once

// ===================================================================================
// BrnGui::SelectableGroup  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h
//   class:BrnGui::SelectableGroup
//
// A group of up to 100 selectable component ids with a current-highlight cursor; the
// shared base of the menu component groups (BrnGui::MenuComponent / MenuToggleGroup /
// CrashNavPanel all call GetSelectable; the toggle/online screens call GetHighlighted).
// Reconstructed from the X360 ARTIST build:
//   SelectableGroup::GetSelectable(s32) @ 0x8240FC98
//   SelectableGroup::GetHighlighted()   @ 0x8240FD60
//
// Recovered layout (guest 32-bit offsets noted; the gate compiles 64-bit so members are
// declared by name, not raw offsets):
//   +0x0C : flag byte -- GetSelectable ORs in 0x10 ("a selectable was queried")
//   +0xA4 : s8  selectable count       (signed; extsb in GetSelectable bounds check)
//   +0xA5 : s8  highlighted index      (signed; extsb in GetHighlighted bounds check)
//   +0xA8 : u32 maSelectables[100]     (this[42 + index]; GetHighlighted caps at 100)
// ===================================================================================

#include "types.hpp"

// Forward-declared (pointer-only use in the Construct declaration) to avoid pulling the
// full component/state-interface header cascade into this base layout header.
namespace CgsGui { class StateInterface; }

namespace BrnGui
{
    struct SelectableGroup
    {
        static const s32 KI_MAX_SELECTABLES = 100;   // GetHighlighted bound (0x64)
        static const u8  KU_FLAG_QUERIED    = 0x10;   // GetSelectable: *(this+0xC) |= 0x10
        // The "no id" reset value TableRow::Clear stores into mu64SelectedId (X360
        // `li -1 ; clrldi 32` == 0x00000000FFFFFFFF).
        static const u64 KU64_NO_ID         = 0xFFFFFFFFull;

        // @ 0x8240FC98 -- bounds-check index, set the queried flag, return the id.
        u32 GetSelectable(s32 liIndex);
        // @ 0x8240FD60 -- bounds-check the highlight cursor, return the highlighted id.
        u32 GetHighlighted();

        // The component's apt name (inherited from CgsGui::GuiComponent). Declared here so
        // derived groups (BrnGui::TableRow) register the group's own name with the GUI cache
        // BY NAME rather than poking the raw name-field offset. Body links from the
        // SelectableGroup / GuiComponent TU.
        const char* GetName() const;

        // DWARF BrnSelectableGroup.cpp:286/352 (`virtual bool HighlightNext/Previous(bool)`):
        // advance/retreat the highlight cursor, lbWrap selecting wrap-around at the ends.
        // Declared here so derived groups (BrnGui::TableRow) forward to them by name; the
        // bodies are the SelectableGroup TU. (Declared non-virtual: the component vtable is
        // modelled explicitly via mppVTable in this flat layout, not as C++ virtuals.)
        bool HighlightNext(bool lbWrap);
        bool HighlightPrevious(bool lbWrap);

        // DWARF BrnSelectableGroup.cpp:42 -- build the group (name it, wire its state
        // interface, take its apt id). Called up to by BrnGui::TableRow::Construct. Body
        // links from the SelectableGroup TU.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luAptId);

        // DWARF BrnSelectableGroup.cpp:74 (`virtual void Clear()`) -- reset the group's
        // selectable set. Called up to by BrnGui::TableRow::Clear. Body links from the
        // SelectableGroup TU.
        void Clear();

        // Members carrying the fields the two recovered functions touch. The leading
        // head (+0x00..+0xA3) is the unrecovered component base; modelled as a reserved
        // span so muFlags/muCount/... land at their recovered indices for clarity.
        //
        // +0x00 is the component vtable pointer (the class is polymorphic in the game --
        // CgsGui::GuiComponent base). It is exposed as a named function-pointer table so the
        // derived groups can dispatch the still-unnamed component virtuals through it BY NAME
        // (e.g. BrnGui::TableRow::Clear/SetupRow call vtable slots), following the project's
        // modelled-vtable convention (see BrnOnlineTimeoutTimerComponent). The dispatched
        // slots' precise method identities are not reconstructed here; only the X360 byte
        // offset / slot index is documented at each call site.
        void** mppVTable;           // +0x00 (component vtable pointer)
        u8  maHeadReserved[0x08];   // +0x04..+0x0B (component base head; not recovered here)
        u8  muFlags;                // +0x0C (GetSelectable ORs 0x10)
        u8  maPadD[3];              // +0x0D..+0x0F (alignment before the id word)
        // +0x10: a 64-bit id word the group resets to all-ones (== "none") -- X360
        // TableRow::Clear does `std 0xFFFFFFFF, 0x10`. Named so derived groups reset it BY
        // NAME (its exact role -- a cached selected/highlighted id -- is not independently
        // recovered; only the reset is). KU64_NO_ID is the X360's reset value.
        u64 mu64SelectedId;         // +0x10
        u8  maMidReserved[0xA4 - 0x18]; // +0x18..+0xA3
        s8  miSelectableCount;      // +0xA4 (DWARF miItemCount)
        s8  miHighlightedIndex;     // +0xA5
        // +0xA6 (DWARF BrnSelectableGroup.h:178 `bool mbWrapped`): the wrap-around flag,
        // previously modelled as the first padding byte. Named so derived groups read/write
        // it BY NAME -- BrnGui::TableRow::SetupRow/Clear toggle it (X360 lbz/stb 0xA6) and
        // dirty the component when it changes.
        bool mbWrapped;             // +0xA6
        u8  maPadA7[1];             // +0xA7 (alignment to the dword array)
        u32 maSelectables[KI_MAX_SELECTABLES]; // +0xA8 (this[42 + i])
    };
}
