#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiComponent.h"     // CgsGui::GuiComponent (second base)
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"        // BrnGui::Selectable (first base)
#include "GameSource/Gui/BrnGuiTextField.h"                             // BrnGui::TextField (two by value)

// BrnGui::ImageGallerySelectable - one gallery-category slot of the image-gallery
// screen: a selectable with category/collected text children whose apt state
// mirrors highlighted/highlightable. DWARF home BrnImageGallerySelectable.h:42.
// The X360 layout pins the bases (the sibling ImageGalleryCarouselSelectable
// precedent): Selectable @+0x00 (flags @+0x0C) then the GuiComponent subobject
// @+0x18, with mCategoryText @+0xA4 and mCollectedText @+0x1CC.
namespace BrnGui
{
    struct ImageGallerySelectable : public Selectable, public CgsGui::GuiComponent
    {
        // @0x82419878 (this TU, DWARF cpp:46) -- both base Constructs, then the two
        // text-field children (parented under this component's own name).
        void Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                       const char* lpacParentName, u64 luId);

        // @0x82419910 (this TU, DWARF cpp:80) -- consume the dirty flag and push the
        // matching apt state.
        virtual void Update();

        // @0x82419998 (this TU, DWARF cpp:112) -- re-push a just-loaded child's data.
        bool HandleLoadNotifications(const char* lpacComponentName);

        // DWARF cpp:65 / h:99 / h:115 -- their own ledger functions / X360
        // header-inlines not exported (declaration-only here).
        virtual void Select();
        void SetCategory(const char* lpacCategory);
        void SetCollected(s32 liCollected);

    private:
        // DWARF cpp:23/:24 -- the child clip names.
        static const char KAC_CATEGORY_TEXT_NAME[12];    // "Category_mc"
        static const char KAC_COLLECTED_TEXT_NAME[13];   // "Collected_mc"

        TextField mCategoryText;    // DWARF h:82 (X360 +0x0A4)
        TextField mCollectedText;   // DWARF h:83 (X360 +0x1CC)
    };
}
