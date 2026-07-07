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
// The picker type BrnGui::ColourSelection is not yet sized in its committed home, so the five
// pickers are modelled as strided opaque ColourSelectionSlot storage and driven through a
// SelectableGroup* view (for the virtual nav/select) + the picker's own vtable slots (reached
// by byte offset for Clear/HighlightIndex). When ColourSelection is sized, replace
// ColourSelectionSlot with the real type and call its methods directly.
// ===================================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"                                        // rw::math::vpu::Vector4 (gradient colours)
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsID (u64)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"   // CgsGui::GuiComponent (identity base)
#include "GameSource/Gui/BrnGuiTextField.h"                           // BrnGui::TextField (mTitleText)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h" // BrnGui::SelectableGroup (picker nav base)

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

    private:
        // Owned by a sibling recon pass (verdict=fail here); declared so the nav entry points
        // can call it (the per-TU gate does not link).
        void HighlightNeighbours();

        // One 0x1338-byte colour-picker slot (opaque; the real ColourSelection is not yet sized).
        struct ColourSelectionSlot { u8 maStorage[0x1338]; };

        // The focused picker (element[2]) as a SelectableGroup view for the toggle's virtual
        // delegation (Select / HighlightNext(false) / HighlightPrevious(false)).
        SelectableGroup* GetFocusedSelectionNav()
        {
            return reinterpret_cast<SelectableGroup*>(&maColourSelection[KI_CENTRE_ITEM]);
        }

        // ---- members ----------------------------------------------------------------
        void* mppVTable;                        // +0x00 (component vtable pointer)
        u8    maHeadReserved[0x08];             // +0x04..+0x0B (identity head)
        u8    muFlags;                          // +0x0C (methods OR in 0x10)
        u8    maPadD[3];                        // +0x0D..+0x0F
        u64   mu64Id;                           // +0x10
        CgsGui::GuiComponent mGuiComponentBase; // +0x18

        ColourSelectionSlot maColourSelection[KI_NUM_ITEMS];   // +0xA8 (stride 0x1338; [2] @ +0x2718)

        TextField mTitleText;                   // +0x60C0
        s32       miLoadedItems;                // +0x61E6

        // The five child colour-selection component names (off_82F27474). Defined in the .cpp.
        static const char* const KAC_ITEM_COLOUR_COMPONENT[KI_NUM_ITEMS];

        // The per-state apt view-state names (off_82F27488). Defined in the .cpp.
        static const char* const KAC_STATE_NAMES[E_MENUTOGGLESTATES_COUNT];
    };
}
