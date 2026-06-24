// ===================================================================================
// BrnGui::MenuComponent -- implementation (data accessors)
//   class:BrnGui::MenuComponent
//
// Reconstructed store-for-store from the X360 ARTIST build. Member-by-name access; the
// guest Selectable* that the base group returns is reinterpreted to a MenuSelectableView
// to reach the flag byte (+0x0C) and the 64-bit id (+0x10), matching the X360
// `lbz 0xC(r3) / std 0x10(r3) / stb 0xC(r3)` store sequence.
//
// NOTE: MenuComponent::MenuComponent (ctor @0x824FFEF8) is NOT reconstructed here. It is a
// vtable-init-only constructor over an un-homed multiply-inheriting polymorphic hierarchy
// (primary vptr @+0x00 and secondary vptr @+0x18 on the object, plus the same pair on each
// of 16 embedded sub-objects @+0x238 stride 0xE8); the vtable symbols and the sub-object
// class are un-homed and cannot be reproduced store-for-store without fabrication. The TU
// is therefore reported BLOCKED with the two genuine accessors below committed.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

#include <cstdint>   // uintptr_t -- guest 32-bit Selectable* carried as a u32

namespace BrnGui
{
    // @0x8240FE20 -- assert the index is in [0, 16) ("MenuComponent::GetSelectable()
    // invalid index specified", BrnMenuComponent.h:171), then delegate to the base group
    // and return its result (the guest Selectable*, as a u32). The signed comparisons in
    // the asm (cmpwi index,0 / cmpwi index,0x10) check both ends.
    u32 MenuComponent::GetSelectable(u32 luIndex)
    {
        const s32 liIndex = static_cast<s32>(luIndex);
        CGS_ASSERT(liIndex >= 0 && liIndex < KI_MAX_MENU_SELECTABLES,
                   "MenuComponent::GetSelectable() invalid index specified");
        return SelectableGroup::GetSelectable(liIndex);
    }

    // @0x82488FC0 -- look up the selectable, assert it is non-null ("Invalid selectable
    // specified", BrnMenuComponent.h:156), re-look it up, then OR 0x10 into its flag byte
    // (+0x0C) and store the 64-bit id (+0x10). The X360 emits two GetSelectable calls: the
    // first for the null check, the second for the field writes; both are reproduced.
    u32 MenuComponent::SetId(u32 luIndex, u64 luId)
    {
        const u32 luFirst = GetSelectable(luIndex);
        CGS_ASSERT(luFirst != 0, "Invalid selectable specified");

        const u32 luSelectable = GetSelectable(luIndex);
        MenuSelectableView* lpSelectable =
            reinterpret_cast<MenuSelectableView*>(static_cast<uintptr_t>(luSelectable));

        lpSelectable->mId = luId;                          // std r26, 0x10(r3)
        lpSelectable->mu8Flags |= KU_FLAG_ID_SET;          // lbz/ori 0x10 / stb 0xC(r3)
        return luSelectable;
    }
}
