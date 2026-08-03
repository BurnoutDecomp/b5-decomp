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
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h" // BrnGui::SelectableGroup (picker nav base)
#include "GameSource/Gui/Flow/Shared/Components/BrnColourSelection.h" // BrnGui::ColourSelection (by value, x5)

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    class ColourMenuToggle
    {
    public:
        // The five colour pickers + the focused (central) one.
        static const s32 KI_NUM_ITEMS   = 5;
        static const s32 KI_CENTRE_ITEM = 2;      // element[2] -- the central/focused picker
        // Dirty / queried flag bit (methods OR *(this+0xC) |= 0x10).
        static const u8  KU_FLAG_DIRTY   = 0x10;
        static const u8  KU_FLAG_QUERIED = 0x10;
        // The picker's per-child "no id" sentinel (SelectableGroup::KU64_NO_ID).
        static const u64 KU64_NO_ID = 0xFFFFFFFFull;
        // One colour picker occupies 0x1338 (4920) bytes on the console.
        static const u32 KU_COLOUR_SELECTION_STRIDE = 0x1338;

        // muFlags state bits sampled by Update() to pick the apt view-state name (inferred
        // from the state ladder: bit0 active, bit2 enabled, bit3 highlighted).
        static const u8 KU_FLAG_ACTIVE      = 0x01;
        static const u8 KU_FLAG_ENABLED     = 0x04;
        static const u8 KU_FLAG_HIGHLIGHTED = 0x08;

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

        // @ 0x824E5670 -- clear every embedded colour selection.
        void Clear();
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
        // @ 0x824E56C8 -- tail-call the focused picker's Select().
        void Select();
        // @ 0x824EA110 -- set the title text, push a top/bottom colour gradient into all five
        // pickers, then dirty the component.
        void SetupMenuToggleGradient(s32 liActiveCount, bool lbActive, const char* lpacText,
                                     const rw::math::vpu::Vector4** lppTopColours,
                                     const rw::math::vpu::Vector4** lppBottomColours,
                                     u64* lpu64Ids);
        // @ 0x824E8D08 -- when dirtied, resolve the current apt view-state and re-push it,
        // then Update every embedded colour selection.
        void Update();

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

        // The component's flag byte, as the owning screen state samples it (+0xC).
        bool IsHighlighted() const { return (muFlags & KU_FLAG_HIGHLIGHTED) != 0; }

        // ADDITIVE GROW (BrnGui::CarSelectLivery, 2026-08-02). Component vtable slots 0..3,
        // which SetupPaintColourToggle and HandleControllerInput reach as
        // `(*(*(this + 0x2860) + 0/4/8/0xC))(this + 0x2860, x)`. The toggle's head carries the
        // same Selectable flag byte at +0x0C that SelectableGroup models, so these reproduce
        // Selectable::SetActive / SetHighlightable / SetSelectable / SetHighlighted on it.
        bool SetActive(bool lbActive)               { return SetStateFlag(KU_FLAG_ACTIVE, lbActive); }
        bool SetHighlightable(bool lbHighlightable) { return SetStateFlag(0x02, lbHighlightable); }
        bool SetSelectable(bool lbSelectable)       { return SetStateFlag(KU_FLAG_ENABLED, lbSelectable); }
        bool SetHighlighted(bool lbHighlighted)     { return SetStateFlag(KU_FLAG_HIGHLIGHTED, lbHighlighted); }

        // Mark the component dirty so the next Update() re-pushes its apt view state
        // (`*(this+0xC) |= 0x10`). CarSelectLivery's SetupPaintColourToggle does this
        // explicitly after re-seating the picker.
        void SetDirty() { muFlags = static_cast<u8>(muFlags | KU_FLAG_DIRTY); }

    private:
        // The shared body of the four flag setters (see SelectableGroup::SetStateFlag).
        bool SetStateFlag(u8 luBit, bool lbSet)
        {
            const u8 luFlags = muFlags;
            if (lbSet == ((luFlags & luBit) != 0))
                return false;
            muFlags = static_cast<u8>(lbSet ? (luFlags | luBit) : (luFlags ^ luBit));
            muFlags = static_cast<u8>(muFlags | KU_FLAG_DIRTY);
            return true;
        }

        // The focused picker (element[2]) -- the toggle's virtual delegation target
        // (Select / HighlightNext(false) / HighlightPrevious(false) / HighlightIndex).
        ColourSelection* GetFocusedSelectionNav() { return &maColourSelection[KI_CENTRE_ITEM]; }

        // ---- members ----------------------------------------------------------------
        void* mppVTable;                        // +0x00 (component vtable pointer)
        u8    maHeadReserved[0x08];             // +0x04..+0x0B (identity head)
        u8    muFlags;                          // +0x0C (methods OR in 0x10)
        u8    maPadD[3];                        // +0x0D..+0x0F
        u64   mu64Id;                           // +0x10
        CgsGui::GuiComponent mGuiComponentBase; // +0x18

        ColourSelection maColourSelection[KI_NUM_ITEMS];   // +0xA8 (stride 0x1338; [2] @ +0x2718)

        TextField mTitleText;                   // +0x60C0
        s32       miLoadedItems;                // +0x61E6

        // The five child colour-selection component names (off_82F27474). Defined in the .cpp.
        static const char* const KAC_ITEM_COLOUR_COMPONENT[KI_NUM_ITEMS];

        // The per-state apt view-state names (off_82F27488). Defined in the .cpp.
        static const char* const KAC_STATE_NAMES[E_MENUTOGGLESTATES_COUNT];
    };
}
