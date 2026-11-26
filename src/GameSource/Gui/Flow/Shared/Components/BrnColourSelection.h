#pragma once

// ===================================================================================
// BrnGui::ColourSelection  -- owning header
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnColourSelection.h
//
// One colour picker: a BrnGui::SelectableGroup of up to 100 BrnGui::ColourSelectionItem
// swatches, plus the two apt-driven BrnGui::ColourField panels that show the highlighted
// swatch's colour (or its two-stop gradient) and its outline plate. Five of these make a
// BrnGui::ColourMenuToggle. Recovered bodies:
//   Construct                    @ 0x824E85D8  (asserts BrnColourSelection.cpp:44/:45)
//   SetupColourSelectionGradient @ 0x824E9E58  (assert cpp:136)
//   SetItemGradient              @ 0x824E8758  (assert cpp:220)
//   Update                       @ 0x824E8840  (assert cpp:247)
//   GetUint32ColourFromVector4   @ 0x824E8528  (header-inline, h:129; assert h:131)
//
// LINE LAYOUT: the DecFIGS DWARF places the struct on h:46, the two constants on h:103/110,
// the members on h:112-114 and GetUint32ColourFromVector4 on h:129. ARTIST bakes that
// function's assert line (li r5,0x83 == 131 @0x824E854C, file string 0x8206CB80 = this
// header), so the declarations below sit on those lines; the blank lines keep them there.
//
// Layout (X360): +0x000 SelectableGroup base (maSelectables[100] ends at +0x238),
// +0x238 maSelectionItems[100] (stride 0x28), +0x11D8 mColourField, +0x1288
// mColourFieldOutline ("<name>_bk") -- sizeof == 0x1338, the stride
// ColourMenuToggle::HighlightNeighbours @0x824E8DE8 walks the five pickers by.
// ===================================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT (GetUint32ColourFromVector4)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"      // BrnGui::SelectableGroup (base)
#include "GameSource/Gui/Flow/Shared/Components/BrnColourSelectionItem.h"  // BrnGui::ColourSelectionItem (by value)
#include "GameSource/Gui/Flow/Shared/Components/BrnColourField.h"          // BrnGui::ColourField (by value)

namespace CgsGui { struct StateInterface; }

namespace BrnGui
{








    struct ColourSelection : public SelectableGroup
    {
        // @ 0x824E85D8 -- base Construct, then bring up the swatch panel under this picker's
        // own name and the outline plate under "<name>_bk".
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
        virtual void Update();

        // ---- layout (DWARF private; kept public: ColourMenuToggle reads the fields) -----
































        static const u32 KU_MAX_COLOUR_SELECTION_NAME_LEN = 128;   // DWARF h:103






        static const s32 KI_MAX_ITEMS_PER_SELECTION = 100;          // DWARF h:110 (SetItemGradient `cmplwi r4, 0x64`)

        ColourSelectionItem maSelectionItems[KI_MAX_ITEMS_PER_SELECTION];   // +0x238  (stride 0x28)
        ColourField         mColourField;                                  // +0x11D8 (4568)
        ColourField         mColourFieldOutline;                           // +0x1288 (4744) "<name>_bk"









    private:
        // @ 0x824E8528 -- pack a normalised colour (x, y, z = R, G, B in 0..1) into 0x00RRGGBB:
        // each channel scaled by 255.0 and truncated to uint32_t (fctidz); w is not used. The channels are
        // read with VecFloatRef::GetFloat (DWARF's inlined copies call it, BrnGuiSharedComponentUnity :2772;
        // ARTIST multiplies on the FPU, lfs/fmuls), not through the VecFloat operator set.
        uint32_t GetUint32ColourFromVector4(const rw::math::vpu::Vector4* lpv4Colour)
        {
            CGS_ASSERT(lpv4Colour != 0, "lpv4Colour");   // h:131 (ARTIST 0x824E854C)

            const uint32_t luRed   = static_cast<uint32_t>(lpv4Colour->GetX().GetFloat() * 255.0f);
            const uint32_t luGreen = static_cast<uint32_t>(lpv4Colour->GetY().GetFloat() * 255.0f);
            const uint32_t luBlue  = static_cast<uint32_t>(lpv4Colour->GetZ().GetFloat() * 255.0f);

            return ((luRed << 8) | luGreen) << 8 | luBlue;
        }
    };
}
