// ===================================================================================
// BrnGui::ColourSelection  -- implementation
//   class:BrnGui::ColourSelection
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct                    @ 0x824E85D8
//   SetupColourSelectionGradient @ 0x824E9E58
//   SetItemGradient              @ 0x824E8758
//   Update                       @ 0x824E8840
//   GetUint32ColourFromVector4   @ 0x824E8528  (header-inline, BrnColourSelection.h:129)
//
// The component-virtual dispatches the X360 makes on the picker itself (slot 7 == Add,
// slot 12 == HighlightIndex, slot 5 == Update) and on each swatch (slots 0..3 ==
// SetActive / SetHighlightable / SetSelectable / SetHighlighted) are all spelled by name --
// the slot map is the one resolved from .rdata for the sibling components (see the banner
// in BrnMenuToggleGroup.cpp).
//
// LINE LAYOUT: each definition sits on its DecFIGS DWARF line (BrnColourSelection.h dump:
// Construct cpp:42, SetupColourSelectionGradient cpp:134, SetItemGradient cpp:218, Update
// cpp:236), in that order; ARTIST bakes the asserts' __LINE__ (e.g. Update's li r5,0xF7 ==
// 247 @0x824E889C), so the blank lines keep each body on its original lines.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnColourSelection.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

namespace BrnGui
{











    // ---- Construct @ 0x824E85D8 -----------------------------------------------------
    void ColourSelection::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface, const char* lpacParentName, u64 lu64AptId)
    {
        CGS_ASSERT(lpacName != 0, "Invalid name");                      // cpp:44 (streamed)
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface");   // cpp:45 (streamed)

        SelectableGroup::Construct(lpacName, lpStateInterface, lpacParentName, lu64AptId);

        // The swatch panel carries the picker's own name; the backing plate is "<name>_bk".
        // Both are parented on the SAME parent the picker was given (X360 passes r24 through).
        mColourField.Construct(lpacName, lpStateInterface, lpacParentName);

        char lacBackName[128];
        CgsCore::SPrintf(lacBackName, 127, "%s_bk", lpacName);
        lacBackName[127] = 0;                                           // stb 0, 0x7F(sp)
        mColourFieldOutline.Construct(lacBackName, lpStateInterface, lpacParentName);
    }











































































    // ---- SetupColourSelectionGradient @ 0x824E9E58 ----------------------------------
    void ColourSelection::SetupColourSelectionGradient(s32 liActiveCount, bool lbActive, const rw::math::vpu::Vector4** lppTopColours, const rw::math::vpu::Vector4** lppBottomColours, u64* lpu64Ids)
    {
        CGS_ASSERT(liActiveCount <= KI_MAX_ITEMS_PER_SELECTION, "Too many selection items");   // cpp:136 (streamed; signed `cmpwi r4, 0x64 / ble`)

        // The "active" argument is latched into the group's WRAP slot -- that is what the
        // console does (`stb a3, 0xA6(this)` guarded by a compare against the same byte).
        if (lbActive != mbWrapped)
        {
            mbWrapped = lbActive;
            SetDirty();
        }

        s32 liCount = liActiveCount;
        if (liCount >= KI_MAX_ITEMS_PER_SELECTION)
        {
            liCount = KI_MAX_ITEMS_PER_SELECTION;
        }

        for (s32 liItem = 0; liItem < liCount; ++liItem)
        {
            ColourSelectionItem* lpItem = &maSelectionItems[liItem];

            lpItem->SetHighlighted(false);                 // swatch slot 3

            // ⓘ On the console this test can never take its `false` arm (liItem < liCount
            // <= liActiveCount), but it is what the body spells, so it is reproduced.
            const bool lbItemActive = (liItem < liActiveCount);
            lpItem->SetActive(lbItemActive);               // swatch slot 0
            lpItem->SetHighlightable(lbItemActive);        // swatch slot 1
            lpItem->SetSelectable(lbItemActive);           // swatch slot 2

            SetItemGradient(liItem, lppTopColours[liItem], lppBottomColours[liItem]);

            Add(lpItem);                                   // picker slot 7
        }

        SelectableGroup::SetIds(lpu64Ids);

        if (liCount > 0)
        {
            HighlightIndex(0);                             // picker slot 12
        }

        SetDirty();
        Update();                                          // picker slot 5
    }





































    // ---- SetItemGradient @ 0x824E8758 -----------------------------------------------
    void ColourSelection::SetItemGradient(s32 liIndex, const rw::math::vpu::Vector4* lpv4Top, const rw::math::vpu::Vector4* lpv4Bottom)
    {
        CGS_ASSERT(static_cast<u32>(liIndex) < static_cast<u32>(KI_MAX_ITEMS_PER_SELECTION), "Invalid index. Setup the ColourSelection first.");   // cpp:220 (streamed; unsigned `cmplwi r4, 0x64`)

        ColourSelectionItem& lrItem = maSelectionItems[liIndex];
        lrItem.mpcSelectionColour1 = lpv4Top;      // item +0x18
        lrItem.mpcSelectionColour2 = lpv4Bottom;   // item +0x1C
        lrItem.mbIsGradient        = true;         // item +0x20
        lrItem.SetDirty();                         // item +0x0C |= 0x10

        SetDirty();                // picker +0x0C |= 0x10
    }





    // ---- Update @ 0x824E8840 --------------------------------------------------------
    void ColourSelection::Update()
    {
        if (!IsDirty())
        {
            return;
        }
        const rw::math::vpu::Vector4* lpv4Colour1 = 0;   // both zeroed before the highlight test (ARTIST 0x824E8878/7C)
        const rw::math::vpu::Vector4* lpv4Colour2 = 0;
        if (miHighlightedIndex > -1)
        {
            ColourSelectionItem* lpCurrentlySelected = static_cast<ColourSelectionItem*>(GetHighlighted());
            CGS_ASSERT(lpCurrentlySelected != 0, "lpCurrentlySelected");   // cpp:247 (li r5,0xF7)

            if (lpCurrentlySelected->mbIsGradient)
            {
                lpCurrentlySelected->GetGradient(&lpv4Colour1, &lpv4Colour2);

                // Converted as call arguments (ARTIST packs colour2 first: right-to-left).
                mColourField.SetGradient(GetUint32ColourFromVector4(lpv4Colour1), GetUint32ColourFromVector4(lpv4Colour2));


            }
            else
            {
                mColourField.SetColour(GetUint32ColourFromVector4(lpCurrentlySelected->mpcSelectionColour1));
            }
        }

        SelectableGroup::Update();
    }
}
