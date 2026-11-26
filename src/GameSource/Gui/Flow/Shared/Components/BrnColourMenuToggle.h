#pragma once

// ===================================================================================
// GameSource/Gui/Flow/Shared/Components/BrnColourMenuToggle.h
//
// BrnGui::ColourMenuToggle -- a five-picker colour-toggle GUI component. It embeds five
// BrnGui::ColourSelection pickers (maColourSelection[5]; element [2] is the focused/central
// picker) plus a title text field. Select / HighlightNext / HighlightPrevious / HighlightIndex
// all delegate to the central picker's SelectableGroup base and re-fan the highlight to the
// neighbours (HighlightNeighbours).
//
// DWARF (BrnColourMenuToggle.h) is authoritative for the member set and method shape:
//   ColourMenuToggle : public CgsGui::GuiComponent
//   BrnGui::ColourSelection maColourSelection[5]  (stride 0x1338 == 4920)
//   BrnGui::TextField       mTitleText
//   int32_t                 miLoadedItems
//
// 2026-08-02 -- THE OPAQUE PICKER SLOTS ARE RETIRED. The five pickers used to be strided
// `u8 maStorage[0x1338]` blobs driven through `*(void***)slot` raw vtable reads. Nothing in
// this tree ever writes a vtable word into modelled component storage, so the very first
// Clear()/Update()/HighlightIndex() would have jumped through an uninitialised pointer --
// this TU was simply never mounted, which is the only reason it never fired. BrnGui::
// ColourSelection is now a real, sized type (see BrnColourSelection.h) and every call below
// is by name.
// ===================================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                                        // rw::math::vpu::Vector4 (gradient colours)
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsID (u64)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (identity base)
#include "GameSource/Gui/BrnGuiTextField.h"                           // BrnGui::TextField (mTitleText)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"      // BrnGui::Selectable (primary base)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h" // BrnGui::SelectableGroup (picker nav base)
#include "GameSource/Gui/Flow/Shared/Components/BrnColourSelection.h" // BrnGui::ColourSelection (by value, x5)

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    // DWARF BrnColourMenuToggle.h:48 `ColourMenuToggle : public CgsGui::GuiComponent`; the dumper
    // prints one base, as for MenuToggle and SelectableGroup. ARTIST's implicit ctor (0x82500B88)
    // stores the Selectable-headed vtable 0x82074730 at +0 (Selectable::SetActive/SetHighlightable/
    // SetSelectable/SetHighlighted, then this class's Select, Update and Clear in slots 4..6) and
    // the GuiComponent subobject's 0x8207472C at +0x18: Selectable first, GuiComponent second.
    struct ColourMenuToggle : public Selectable, public CgsGui::GuiComponent
    {
        // The five colour pickers + the focused (central) one.
        static const s32 KI_NUM_ITEMS   = 5;
        static const s32 KI_CENTRE_ITEM = 2;      // element[2] -- the central/focused picker
        // The picker's per-child "no id" sentinel (SelectableGroup::KU64_NO_ID).
        static const u64 KU64_NO_ID = 0xFFFFFFFFull;
        // One colour picker occupies 0x1338 (4920) bytes on the console.
        static const u32 KU_COLOUR_SELECTION_STRIDE = 0x1338;

        // DWARF BrnColourMenuToggle.h:164 -- the apt view-state ladder Update() maps to.
        enum MenuToggleStates
        {
            E_MENUTOGGLESTATES_UNUSED        = 0,
            E_MENUTOGGLESTATES_INVISIBLE     = 1,
            E_MENUTOGGLESTATES_DISABLED      = 2,
            E_MENUTOGGLESTATES_UNHIGHLIGHTED = 3,
            E_MENUTOGGLESTATES_HIGHLIGHTED   = 4,
            E_MENUTOGGLESTATES_COUNT         = 5,
        };

        // @ 0x824E5670 (cpp:288) -- clear every embedded colour selection. Virtual (DWARF): slot 6,
        // which CarSelectLivery::OnEnter/OnLeave dispatch (0x824D6AE4 / 0x824D6CC8).
        virtual void Clear();
        // @ 0x824E8B70 -- construct the five pickers + title text; mark loaded.
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 lu64AptId);
        // @ 0x824EA2A0 -- highlight the requested index on the centre picker; on change,
        // refresh neighbours + dirty the component.
        //
        // NOTE (measured 2026-08-02, corrected 2026-08-03): this body genuinely never touches
        // r4 -- it sets only r3 before dispatching the centre picker's slot 12
        // (`addi r3,r31,0x2718 / lwz r11,0x2718(r31) / lwz r11,0x30(r11) / bctrl`). But the
        // behaviour is FULLY DETERMINED, not undefined: both BrnGui::CarSelectLivery call
        // sites DO set r4 (0x824C0E88 `lwz r4,4(r30)`; 0x824D73B0 `lbz r4,0xEE(r28)`), and
        // because this body leaves r4 alone the caller's value is ABI-forwarded straight
        // through to the picker's slot 12. An earlier version of this comment claimed the
        // callers passed only `this` and that the result was unreproducible -- both wrong.
        // The committed explicit-parameter form below is therefore exactly faithful
        // of "refresh the colour highlight".
        bool HighlightIndex(s32 liIndex);
        // @ 0x824EA1A0 / 0x824EA220 -- move the focused picker's highlight next/prev.
        bool HighlightNext();
        bool HighlightPrevious();
        // @ 0x824E56C8 (cpp:305, virtual) -- tail-call the focused picker's Select().
        virtual void Select();
        // @ 0x824EA110 -- set the title text, push a top/bottom colour gradient into all five
        // pickers, then dirty the component.
        void SetupMenuToggleGradient(s32 liActiveCount, bool lbActive, const char* lpacText,
                                     const rw::math::vpu::Vector4** lppTopColours,
                                     const rw::math::vpu::Vector4** lppBottomColours,
                                     u64* lpu64Ids);
        // @ 0x824E8D08 (cpp:228, virtual) -- when dirtied, resolve the current apt view-state and
        // re-push it, then Update every embedded colour selection.
        virtual void Update();

        // @ 0x824E8DE8 -- after the centre picker's highlight has moved, re-seat the four
        // flanking pickers' highlights and hide the swatch/plate pair of any picker that has
        // run into its inner neighbour.
        void HighlightNeighbours();

        // The currently selected colour index -- the centre picker's own highlighted-item
        // index. BrnGui::CarSelectLivery reads it as the byte at +20509
        // (== 0xA8 + 2*0x1338 + 0xA5) at three sites.
        s32 GetHighlightedColourIndex() const
        {
            return maColourSelection[KI_CENTRE_ITEM].miHighlightedIndex;
        }

        // The flag surface (IsHighlighted, SetDirty, the SetActive/SetHighlightable/
        // SetSelectable/SetHighlighted virtuals CarSelectLivery dispatches through slots 0..3)
        // is the Selectable base's; the apt name and view-state output are GuiComponent's.

    private:
        // The focused picker (element[2]) -- the toggle's virtual delegation target
        // (Select / HighlightNext(false) / HighlightPrevious(false) / HighlightIndex).
        ColourSelection* GetFocusedSelectionNav() { return &maColourSelection[KI_CENTRE_ITEM]; }

        // ---- members (after the Selectable head +0x00..+0x17 and GuiComponent at +0x18) ----
        ColourSelection maColourSelection[KI_NUM_ITEMS];   // +0xA8 (stride 0x1338; [2] @ +0x2718)

        TextField mTitleText;                   // +0x60C0
        s32       miLoadedItems;                // +0x61E6

        // The five child colour-selection component names (off_82F27474). Defined in the .cpp.
        static const char* KAC_ITEM_COLOUR_COMPONENT[KI_NUM_ITEMS];

        // The per-state apt view-state names (off_82F27488). Defined in the .cpp.
        static const char* KAC_STATE_NAMES[E_MENUTOGGLESTATES_COUNT];
    };
}
