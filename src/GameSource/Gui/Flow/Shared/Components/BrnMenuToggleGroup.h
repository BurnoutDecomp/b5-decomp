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
// compiles 64-bit so members are declared BY NAME, the reserved head carrying the
// unrecovered SelectableGroup/GuiComponent base):
//   +0x00  : component vtable pointer (installed by the ctor)
//   +0x0C  : u8  muFlags        -- dirty flags (|= 0x10 on any mutation)
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
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // BrnGui::SelectableGroup (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"       // BrnGui::MenuToggle (maItems element)

namespace BrnGui
{
    // The variable-size menu-toggle group. TI_SIZE == the compile-time item capacity
    // (the shipped screens use <2>/<3>/<4>/<5>); the on-disk field layout is identical
    // across sizes but for the maItems element count.
    template <s32 TI_SIZE>
    struct MenuToggleGroupVarSize : public SelectableGroup
    {
        // Component dirty flag OR-ed into muFlags (+0xC) on any mutation.
        static const u8 KU_FLAG_DIRTY = 0x10;

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

        // Group virtual (DWARF `virtual void Clear()`); resets the selectable set, drops the
        // wrap flag (dirtying), then clears each live row via its component clear virtual.
        void Clear();

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

        // @ 0x82500F18 (<3>) -- retreat the highlight on the currently-highlighted row.
        bool HighlightPreviousItem();

        // Not in this batch (declared for the modelled group vtable / cross-refs); the
        // "get the highlighted MenuToggle row" helper + GetSelectable land with their TUs.
        bool HighlightNextItem();
        MenuToggle* GetSelectable(s32 liIndex);

        // ----- recovered layout -----
        static const s32 KI_ITEMS_OFFSET   = 0x238;   // maItems @ +0x238
        static const s32 KI_ITEM_STRIDE    = 0xE98;   // sizeof(MenuToggle) == 3736

        MenuToggle maItems[TI_SIZE];   // +0x238 (element stride 0xE98)
        s32        miMaxItems;         // +0x2E00 (item count; loops read it)
        s32        miLoadedItems;      // +0x2E04 (Unloaded/Construct reset to 0)
    };
}
