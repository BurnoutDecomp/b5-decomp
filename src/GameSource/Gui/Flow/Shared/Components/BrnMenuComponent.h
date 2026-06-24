#pragma once

// ===================================================================================
// BrnGui::MenuComponent -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h
//   class:BrnGui::MenuComponent
//
// A menu component built on a BrnGui::SelectableGroup of up to 16 selectable items. The
// X360 ARTIST build emits these out-of-line members (file/line are the X360-baked assert
// references in BrnMenuComponent.h):
//   GetSelectable @0x8240FE20 -> assert 0 <= index < 16 ("MenuComponent::GetSelectable()
//                                invalid index specified", h:171); then delegate to
//                                SelectableGroup::GetSelectable(this, index) and return its
//                                result (the guest Selectable*, returned as a u32).
//   SetId         @0x82488FC0 -> sel = GetSelectable(index); assert sel != 0 ("Invalid
//                                selectable specified", h:156); sel = GetSelectable(index);
//                                sel->flags(+0x0C) |= 0x10; sel->id(+0x10) = id (64-bit store).
//   MenuComponent (ctor) @0x824FFEF8 -> NOT reconstructed (vtable-init-only ctor over an
//                                un-homed multiply-inheriting polymorphic hierarchy: it sets
//                                the primary vptr @+0x00 and a secondary vptr @+0x18 on the
//                                outer object plus on an embedded array of 16 polymorphic
//                                sub-objects @+0x238 with a 0xE8 stride; the vtable symbols
//                                off_82074068/off_82074064/off_82072FD0/off_82072FCC and the
//                                sub-object class are un-homed and cannot be reproduced
//                                store-for-store without fabrication. BLOCKED -- see the TU
//                                report). The two data accessors above are bodied in the .cpp.
//
// MenuComponent's SelectableGroup base sits at offset 0: GetSelectable passes `this`
// directly as the SelectableGroup pointer to SelectableGroup::GetSelectable. The X360
// stride bound here is the fixed constant 16 (the menu's selectable capacity), distinct
// from SelectableGroup::GetSelectable's own `< count` bound.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // BrnGui::SelectableGroup

namespace BrnGui
{
    // The selectable item that MenuComponent::SetId mutates. SelectableGroup::GetSelectable
    // returns the item as a guest 32-bit pointer (the X360 Selectable*); SetId loads the
    // item's flag byte at +0x0C, ORs in 0x10, and writes the 64-bit id at +0x10 (`std`).
    // Only those two fields are touched here; the full Selectable class is owned elsewhere
    // (DWARF BrnSelectable.h: vptr@+0x00, mpcSelectionText@+0x04, mxFlags@+0x08, mId@+0x10).
    struct MenuSelectableView
    {
        u8  maHead[0x0C];   // +0x00..+0x0B (vptr / text / leading flags -- not touched here)
        u8  mu8Flags;       // +0x0C (SetId: |= 0x10)
        u8  maPad[0x03];    // +0x0D..+0x0F (to the 8-byte-aligned id slot)
        u64 mId;            // +0x10 (SetId: 64-bit store of the new id)
    };

    // MenuComponent derives from SelectableGroup (base subobject at offset 0): the X360
    // GetSelectable passes `this` straight through as the SelectableGroup pointer.
    struct MenuComponent : public SelectableGroup
    {
        // GetSelectable's fixed index bound (the menu's selectable capacity). The X360
        // asserts 0 <= index < 16 before delegating to the base group.
        static const s32 KI_MAX_MENU_SELECTABLES = 16;   // cmpwi index, 0x10

        // SetId marks the chosen selectable's flag byte (|= 0x10) the same way
        // SelectableGroup::KU_FLAG_QUERIED is set: an "id was assigned" bit.
        static const u8 KU_FLAG_ID_SET = 0x10;           // ori r11, r11, 0x10

        // @0x8240FE20 -- bounds-check index against [0,16), then return the base group's
        // selectable for that index (a guest Selectable*, carried as a u32).
        u32 GetSelectable(u32 luIndex);

        // @0x82488FC0 -- look up the selectable for luIndex, mark it id-set, and store luId
        // (64-bit) into the selectable's id field. Returns the selectable (guest pointer).
        u32 SetId(u32 luIndex, u64 luId);
    };
}
