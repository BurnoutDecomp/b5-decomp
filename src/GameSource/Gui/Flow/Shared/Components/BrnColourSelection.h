#pragma once

// ===================================================================================
// BrnGui::ColourSelection  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnColourSelection.h
//   class:BrnGui::ColourSelection
//
// One colour picker: a BrnGui::SelectableGroup of up to 100 BrnGui::ColourSelectionItem
// swatches, plus the two apt-driven BrnGui::ColourField panels that show the highlighted
// swatch's colour (or its two-stop gradient) and its backing plate. Five of these make a
// BrnGui::ColourMenuToggle.
//
// 2026-08-02 -- GROWN from the single static helper it used to carry. The class had no
// member layout at all, so BrnGui::ColourMenuToggle modelled the five pickers as opaque
// `u8 maStorage[0x1338]` slots and reached their methods through `*(void***)slot` -- a raw
// vtable read on storage that nothing ever initialises. The real shape is settled by the
// four recovered bodies:
//   Construct                    @ 0x824E85D8  (asserts BrnColourSelection.cpp:44/:45)
//   SetupColourSelectionGradient @ 0x824E9E58  (assert cpp:136)
//   SetItemGradient              @ 0x824E8758  (assert cpp:220)
//   Update                       @ 0x824E8840  (assert cpp:247)
//   GetUint32ColourFromVector4   @ 0x824E8528  (static)
//
// Layout (X360 offsets documentary -- the x64 gate widens every pointer, so access is BY NAME):
//   +0x000 SelectableGroup base (its maSelectables[100] ends at +0x238)
//   +0x238 ColourSelectionItem maItems[100]   (stride 0x28 == 40; SetItemGradient's `&v5[10*i]`)
//   +0x11D8 ColourField mColourField           (named after the picker itself)
//   +0x1288 ColourField mBackgroundColourField ("<name>_bk")   -- sizeof == 0x1338 == 4920
// The 0x1338 stride is independently confirmed by ColourMenuToggle::HighlightNeighbours
// @0x824E8DE8, which walks the five pickers by exactly that step and reads each one's
// miHighlightedIndex at +0xA5 and each ColourField's mbHidden at +4740 / +4916.
// ===================================================================================

#include "types.hpp"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"      // BrnGui::SelectableGroup (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnColourSelectionItem.h"  // BrnGui::ColourSelectionItem (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnColourField.h"          // BrnGui::ColourField (by value)

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{
    struct ColourSelection : public SelectableGroup
    {
        // SetItemGradient's bound (X360 `cmplwi r4, 0x64`) and SetupColourSelectionGradient's
        // clamp -- the same 100 the SelectableGroup base can hold.
        static const s32 KI_MAX_ITEMS = 100;

        // @ 0x824E85D8 -- base Construct, then bring up the swatch panel under this picker's
        // own name and the backing plate under "<name>_bk".
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 lu64AptId);

        // @ 0x824E9E58 -- (re)populate the picker: latch the active flag into the wrap slot,
        // activate the first liActiveCount swatches (deactivating the rest), give each its
        // gradient pair, register every one with the group, apply the id array, highlight the
        // first and refresh.
        void SetupColourSelectionGradient(s32 liActiveCount, bool lbActive,
                                          const rw::math::vpu::Vector4** lppTopColours,
                                          const rw::math::vpu::Vector4** lppBottomColours,
                                          u64* lpu64Ids);

        // @ 0x824E8758 -- set one swatch's gradient endpoints (and dirty the swatch + picker).
        void SetItemGradient(s32 liIndex, const rw::math::vpu::Vector4* lpv4Top,
                             const rw::math::vpu::Vector4* lpv4Bottom);

        // @ 0x824E8840 -- component slot 5. When dirty, push the highlighted swatch's colour
        // (or gradient) into the swatch panel, then run the group update.
        void Update();

        // @ 0x824E8528 - pack a normalised colour vector (lpv4Colour[0..2] = R,G,B in 0..1)
        // into 0x00RRGGBB. Each channel is scaled by 255.0 and truncated toward zero; the
        // alpha lane (lpv4Colour[3]) is not used.
        static s32 GetUint32ColourFromVector4(const f32* lpv4Colour);

        // ---- layout ------------------------------------------------------------------
        ColourSelectionItem maItems[KI_MAX_ITEMS];   // +0x238  (stride 0x28)
        ColourField         mColourField;            // +0x11D8 (4568)
        ColourField         mBackgroundColourField;  // +0x1288 (4744)
    };
}
