// ===================================================================================
// BrnGui::ColourSelection  -- implementation
//   class:BrnGui::ColourSelection
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct                    @ 0x824E85D8
//   SetupColourSelectionGradient @ 0x824E9E58
//   SetItemGradient              @ 0x824E8758
//   Update                       @ 0x824E8840
//   GetUint32ColourFromVector4   @ 0x824E8528
//
// The component-virtual dispatches the X360 makes on the picker itself (slot 7 == Add,
// slot 12 == HighlightIndex, slot 5 == Update) and on each swatch (slots 0..3 ==
// SetActive / SetHighlightable / SetSelectable / SetHighlighted) are all spelled by name --
// the slot map is the one resolved from .rdata for the sibling components (see the banner
// in BrnMenuToggleGroup.cpp).
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnColourSelection.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

namespace BrnGui
{
    // ---- Construct @ 0x824E85D8 -----------------------------------------------------
    void ColourSelection::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                    const char* lpacParentName, u64 lu64AptId)
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
        mBackgroundColourField.Construct(lacBackName, lpStateInterface, lpacParentName);
    }

    // ---- SetItemGradient @ 0x824E8758 -----------------------------------------------
    void ColourSelection::SetItemGradient(s32 liIndex, const rw::math::vpu::Vector4* lpv4Top,
                                          const rw::math::vpu::Vector4* lpv4Bottom)
    {
        // cpp:220 -- the console streams this text into the assert buffer. The compare is
        // UNSIGNED (`cmplwi r4, 0x64`), so a negative index trips it too.
        CGS_ASSERT(static_cast<u32>(liIndex) < static_cast<u32>(KI_MAX_ITEMS),
                   "Invalid index. Setup the ColourSelection first.");

        ColourSelectionItem& lrItem = maItems[liIndex];
        lrItem.mpcSelectionColour1 = lpv4Top;      // item +0x18
        lrItem.mpcSelectionColour2 = lpv4Bottom;   // item +0x1C
        lrItem.mbIsGradient        = true;         // item +0x20
        lrItem.SetDirty();                         // item +0x0C |= 0x10

        muFlags |= KU_FLAG_QUERIED;                // picker +0x0C |= 0x10
    }

    // ---- SetupColourSelectionGradient @ 0x824E9E58 ----------------------------------
    void ColourSelection::SetupColourSelectionGradient(
        s32 liActiveCount, bool lbActive,
        const rw::math::vpu::Vector4** lppTopColours,
        const rw::math::vpu::Vector4** lppBottomColours,
        u64* lpu64Ids)
    {
        // cpp:136 (streamed). SIGNED compare on the console (`cmpwi r4, 0x64 / ble`).
        CGS_ASSERT(liActiveCount <= KI_MAX_ITEMS, "Too many selection items");

        // The "active" argument is latched into the group's WRAP slot -- that is what the
        // console does (`stb a3, 0xA6(this)` guarded by a compare against the same byte).
        if (lbActive != mbWrapped)
        {
            mbWrapped = lbActive;
            muFlags  |= KU_FLAG_QUERIED;
        }

        s32 liCount = liActiveCount;
        if (liCount >= KI_MAX_ITEMS)
        {
            liCount = KI_MAX_ITEMS;
        }

        for (s32 liItem = 0; liItem < liCount; ++liItem)
        {
            ColourSelectionItem* lpItem = &maItems[liItem];

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

        muFlags |= KU_FLAG_QUERIED;
        Update();                                          // picker slot 5
    }

    // ---- Update @ 0x824E8840 --------------------------------------------------------
    void ColourSelection::Update()
    {
        if ((muFlags & KU_FLAG_QUERIED) == 0)
        {
            return;
        }

        if (miHighlightedIndex > -1)
        {
            ColourSelectionItem* lpSelected =
                static_cast<ColourSelectionItem*>(GetHighlighted());
            CGS_ASSERT(lpSelected != 0, "lpCurrentlySelected");   // cpp:247

            if (lpSelected->mbIsGradient)
            {
                const rw::math::vpu::Vector4* lpv4Colour1 = 0;
                const rw::math::vpu::Vector4* lpv4Colour2 = 0;
                lpSelected->GetGradient(&lpv4Colour1, &lpv4Colour2);

                // The X360 packs colour2 FIRST, then colour1, then calls
                // SetGradient(colour1, colour2).
                const s32 liColour2 =
                    GetUint32ColourFromVector4(reinterpret_cast<const f32*>(lpv4Colour2));
                const s32 liColour1 =
                    GetUint32ColourFromVector4(reinterpret_cast<const f32*>(lpv4Colour1));
                mColourField.SetGradient(static_cast<u32>(liColour1),
                                         static_cast<u32>(liColour2));
            }
            else
            {
                mColourField.SetColour(static_cast<u32>(GetUint32ColourFromVector4(
                    reinterpret_cast<const f32*>(lpSelected->mpcSelectionColour1))));
            }
        }

        SelectableGroup::Update();
    }

    // ---- GetUint32ColourFromVector4 @ 0x824E8528 ------------------------------------
    s32 ColourSelection::GetUint32ColourFromVector4(const f32* lpv4Colour)
    {
        CGS_ASSERT(lpv4Colour != 0, "lpv4Colour");                  // @0x824E8540 (cmplwi r31,0)

        // Per channel: scale by 255.0 (single-precision) then truncate toward zero.
        const s32 liRed   = static_cast<s32>(lpv4Colour[0] * 255.0f);   // fmuls/fctidz @0x824E857C
        const s32 liGreen = static_cast<s32>(lpv4Colour[1] * 255.0f);   // fmuls/fctidz @0x824E8580
        const s32 liBlue  = static_cast<s32>(lpv4Colour[2] * 255.0f);   // fmuls/fctidz @0x824E8584

        return ((liRed << 8) | liGreen) << 8 | liBlue;              // slwi/or chain @0x824E85A0..0x824E85C0
    }
}
