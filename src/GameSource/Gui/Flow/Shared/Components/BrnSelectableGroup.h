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

namespace BrnGui
{
    struct SelectableGroup
    {
        static const s32 KI_MAX_SELECTABLES = 100;   // GetHighlighted bound (0x64)
        static const u8  KU_FLAG_QUERIED    = 0x10;   // GetSelectable: *(this+0xC) |= 0x10

        // @ 0x8240FC98 -- bounds-check index, set the queried flag, return the id.
        u32 GetSelectable(s32 liIndex);
        // @ 0x8240FD60 -- bounds-check the highlight cursor, return the highlighted id.
        u32 GetHighlighted();

        // Members carrying the fields the two recovered functions touch. The leading
        // head (+0x00..+0xA3) is the unrecovered component base; modelled as a reserved
        // span so muFlags/muCount/... land at their recovered indices for clarity.
        u8  maHeadReserved[0x0C];   // +0x00..+0x0B (component base head; not recovered here)
        u8  muFlags;                // +0x0C (GetSelectable ORs 0x10)
        u8  maMidReserved[0xA4 - 0x0D]; // +0x0D..+0xA3
        s8  miSelectableCount;      // +0xA4
        s8  miHighlightedIndex;     // +0xA5
        u8  maPadA6[2];             // +0xA6..+0xA7 (alignment to the dword array)
        u32 maSelectables[KI_MAX_SELECTABLES]; // +0xA8 (this[42 + i])
    };
}
