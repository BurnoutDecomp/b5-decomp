#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                               // CGS_ASSERT (the expected-count tripwire)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/Gui/BrnGuiCache.h"                                          // BrnGui::GuiCache (expected-component list)
#include "GameSource/Gui/Flow/Screen/Components/BrnImageGallerySelectable.h"     // ImageGallerySelectable (the 4 category tabs)
#include "GameSource/Gui/Flow/Screen/Components/BrnImageGalleryCarouselItem.h"   // ImageGalleryCarouselItem (the 3 carousel slots)

// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnImageGallery.h
//
// BrnGui::ImageGalleryState - the mugshot/takedown image-gallery GUI screen
// state. MINIMAL SLICE: this header-TU carries the class shape around the four
// exported inline bodies (resource list / expected-component registration /
// GuiCache init poll / middle-image hide); the state's big interior (the
// carousel logic, category paging, image-info events) lands with the
// BrnImageGallery.cpp TU -- GROW in place. X360 offsets in comments (mpGuiCache
// +56, the hash list +64 (x7) with its count +92, the four carousel slots +132
// stride 760, the middle image +8516); access is BY NAME, the reserved spans
// keep the ORDER.
// ============================================================================

namespace BrnGui
{
    class ImageGalleryState : public CgsGui::State
    {
    public:
        // DWARF-free (asm): the expected-component list bound (the cpp:1057 assert).
        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 7;

        // @0x82500480 (this TU) -- hand out the gallery's one-APT resource list
        // (the XEX .rodata tuple @0x8205E608: id 0xA4, E_GUI_RESOURCETYPE_APT).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = KA_RESOURCES_TO_LOAD;
            *lpuNumberOfResources = 1;
        }

        // @0x82484720 (this TU; the assert cites BrnImageGallery.cpp:1057) -- rebuild
        // the expected-component list from the FOUR CATEGORY TABS (the asm's +132
        // stride-760 loop base is each maCategorySelectable element's embedded
        // GuiComponent subobject -- OnEnter @0x82495948 constructs the four
        // "categoryMenu%i_mc" selectables at +104 stride 760) and hand it to the
        // cache (flow layer 0).
        void SetExpectedAptComponentList()
        {
            ClearExpectedComponent();
            for (s32 liSlot = 0; liSlot < 4; ++liSlot)
            {
                SetExpectedComponent(maCategorySelectable[liSlot]);
            }
            CGS_ASSERT(muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM,
                       "muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM");   // cpp:1057 (non-gating)
            mpGuiCache->SetExpectedAptComponentList(E_GUIFLOW_SCREEN, maExpectedComponentHashes,
                                                    muNumExpectedComponents);
        }

        // @0x824846B8 (this TU) -- the per-frame init poll: once the cache reports
        // every expected component initialised (flow layer 0), clear the list and
        // report done.
        bool UpdateWFInit()
        {
            if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                return false;
            }
            ClearExpectedComponent();
            return true;
        }

        // @0x82484930 (this TU) -- drop the MIDDLE carousel item ("CarouselItemMid_mc",
        // the +8516 element == maCarouselItems[1]) to the current category's
        // invisible frame.
        void HideMiddleImage()
        {
            maCarouselItems[1].AddOutputAptViewState("apt_state",
                                                     KAPC_CATEGORY_INVISIBLE_FRAMES[GetCurrentCategory()],
                                                     false);
        }

        // Own ledger functions (the BrnImageGallery.cpp TU) -- declaration-only here.
        s32  GetCurrentCategory() const;
        void ClearExpectedComponent();
        void SetExpectedComponent(const CgsGui::GuiComponent& lrComponent);

    private:
        // The per-category invisible frame labels (XEX .data @0x82F25350:
        // takedown/mugshot/rulebreaker/finish; definition in the embed-check cpp).
        static const char* const KAPC_CATEGORY_INVISIBLE_FRAMES[4];
        // The one-entry APT resource list (XEX .rodata @0x8205E608).
        static const CgsGui::sResourceTuple KA_RESOURCES_TO_LOAD[1];

        // FLAG: reserved spans = state interior not yet recovered (the slice keeps
        // the X360 ORDER; PC offsets differ). The base State occupies the head.
        u8 maReservedToCache[8];                       // X360 up to +56 (post-base interior; span nominal)
        GuiCache* mpGuiCache;                          // X360 +56
        u32 maExpectedComponentHashes[KU_MAX_INIT_COMPONENTS_NUM];   // X360 +64 (x7)
        u32 muNumExpectedComponents;                   // X360 +92
        u8  maReserved96to104[104 - 96];               // X360 [+96, +104)
        // The four category tabs (X360 +104, stride 760 -- OnEnter @0x82495948
        // constructs "categoryMenu%i_mc" here; SetExpectedAptComponentList's +132
        // loop base is each element's embedded GuiComponent subobject).
        ImageGallerySelectable maCategorySelectable[4];
        u8  maReservedToCarousel[7780 - (104 + 4 * 760)];   // X360 [+3144, +7780)
        // The three carousel items (X360 +7780/+8516/+9252, stride 736 -- OnEnter
        // constructs "CarouselItemLeft/Mid/Right_mc"; DWARF h:117 maCarouselItems[3]).
        ImageGalleryCarouselItem maCarouselItems[3];
    };
}
