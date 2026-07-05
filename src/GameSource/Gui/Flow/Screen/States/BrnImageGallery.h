// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnImageGallery.h
//
// BrnGui::ImageGalleryState - the mugshot/takedown image-gallery GUI screen
// state (DWARF home BrnImageGallery.h:53, base CgsGui::State). MINIMAL SLICE:
// this header carries the class shape around the four exported inline bodies
// (resource list / expected-component registration / GuiCache init poll /
// middle-image hide) PLUS the out-of-line ledger surface bodied in
// BrnImageGallery.cpp (ctor / GetCurrentCategory / ClearExpectedComponent /
// SetExpectedComponent / GetGsmIOCategoryFromGuiEnum). The state's huge interior
// (30-slot carousel-overview array, text fields, animators, PlayerName) is not
// yet recovered; reserved spans keep the X360 ORDER, access is BY NAME.
// X360 offsets in comments (mpGuiCache +56, mauExpectedComponentIds +64 (x7)
// with its count +92, the four category tabs +104 stride 760, the
// mCategorySelectableGroup highlighted-index byte +3309, the three carousel
// items +7780 stride 736).
//
// DWARF-authoritative (references/DecFIGS/dwarfdump/.../BrnImageGallery.h):
//   member  mauExpectedComponentIds (NOT ...Hashes) : h:99 uint32_t[7]
//   SetExpectedComponent(const char*) -> void       : h:282 (r3 hash is dead)
//   GetCurrentCategory() -> EGuiImageCategories      : h:324 (non-const)
//   GetGsmIOCategoryFromGuiEnum(EGuiImageCategories)
//       -> GameStateModuleIO::EImageGalleryType      : cpp:1176 (dossier name truncated)
// ============================================================================
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                               // CGS_ASSERT (the expected-count tripwire)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/GameState/BrnGameStateSharedIO.h"                           // BrnGameState::GameStateModuleIO::EImageGalleryType
#include "GameSource/Gui/BrnGuiCache.h"                                          // BrnGui::GuiCache (expected-component list)
#include "GameSource/Gui/Flow/Screen/Components/BrnImageGallerySelectable.h"     // ImageGallerySelectable (the 4 category tabs) + GetName (GuiComponent base)
#include "GameSource/Gui/Flow/Screen/Components/BrnImageGalleryCarouselItem.h"   // ImageGalleryCarouselItem (the 3 carousel slots) + EGuiImageCategories fwd

namespace BrnGui
{
    class ImageGalleryState : public CgsGui::State
    {
    public:
        // DWARF h:98 -- the expected-component list bound (the h:284 assert).
        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 7;

        // @0x82500328 (BrnImageGallery.cpp) -- compiler-emitted ctor: State base +
        // the embedded GUI sub-objects (per-slot vtable stores handled by their ctors).
        ImageGalleryState();

        // @0x82500480 (this TU) -- hand out the gallery's one-APT resource list
        // (the XEX .rodata tuple @0x8205E608: id 0xA4, E_GUI_RESOURCETYPE_APT).
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = KA_RESOURCES_TO_LOAD;
            *lpuNumberOfResources = 1;
        }

        // @0x82484720 (this TU; assert cites BrnImageGallery.cpp:1057) -- rebuild the
        // expected-component list from the FOUR CATEGORY TABS and hand it to the cache
        // (flow layer 0). SetExpectedComponent @0x82482BD8 takes a component NAME string,
        // so each tab is registered by its GuiComponent name.
        void SetExpectedAptComponentList()
        {
            ClearExpectedComponent();
            for (s32 liSlot = 0; liSlot < 4; ++liSlot)
            {
                SetExpectedComponent(maCategorySelectable[liSlot].GetName());
            }
            CGS_ASSERT(muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM,
                       "muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM");   // cpp:1057 (non-gating)
            mpGuiCache->SetExpectedAptComponentList(E_GUIFLOW_SCREEN, mauExpectedComponentIds,
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
        // maCarouselItems[1]) to the current category's invisible frame.
        void HideMiddleImage()
        {
            maCarouselItems[1].AddOutputAptViewState("apt_state",
                                                     KAPC_CATEGORY_INVISIBLE_FRAMES[GetCurrentCategory()],
                                                     false);
        }

        // Own ledger functions (BrnImageGallery.cpp TU) -- declaration-only here.
        // Signatures/return-types are DWARF-authoritative (see file banner).
        EGuiImageCategories GetCurrentCategory();                    // @0x82482D40 (h:324, non-const)
        void                ClearExpectedComponent();               // @0x82482CC0 (h:299)
        void                SetExpectedComponent(const char* lpacComponentName);   // @0x82482BD8 (h:282, -> void)
        BrnGameState::GameStateModuleIO::EImageGalleryType
            GetGsmIOCategoryFromGuiEnum(EGuiImageCategories leImageCategory);      // @0x824847A0 (cpp:1176)

    private:
        // The per-category invisible frame labels (XEX .data @0x82F25350:
        // takedown/mugshot/rulebreaker/finish; definition in the embed-check cpp).
        static const char* const KAPC_CATEGORY_INVISIBLE_FRAMES[4];
        // The one-entry APT resource list (XEX .rodata @0x8205E608).
        static const CgsGui::sResourceTuple KA_RESOURCES_TO_LOAD[1];

        // FLAG: reserved spans = state interior not yet recovered (the slice keeps
        // the X360 ORDER; PC offsets differ). The base CgsGui::State occupies the head.
        u8 maReservedToCache[8];                       // X360 up to +56 (post-base interior; span nominal)
        GuiCache* mpGuiCache;                          // X360 +56  (DWARF h:91)
        u32 mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM];   // X360 +64 (x7) (DWARF h:99)
        u32 muNumExpectedComponents;                   // X360 +92  (DWARF h:100)
        u8  maReserved96to104[104 - 96];               // X360 [+96, +104) (miCurrentlySelectedCarouselItem @+96, DWARF h:102)
        // The four category tabs (X360 +104, stride 760 -- DWARF h:105).
        ImageGallerySelectable maCategorySelectable[4];
        // FLAG: mCategorySelectableGroup (DWARF h:106, BrnGui::SelectableGroup) interior
        // not recovered; only its highlighted-index byte @+3309 is attested
        // (GetCurrentCategory reads mCategorySelectableGroup.GetHighlightedIndex() as a
        // signed byte here).
        u8  maReservedToGroupIndex[3309 - (104 + 4 * 760)];  // X360 [+3144, +3309)
        u8  muHighlightedCategoryIndex;                 // X360 +3309 (mCategorySelectableGroup.GetHighlightedIndex())
        u8  maReservedGroupToCarousel[7780 - 3310];     // X360 [+3310, +7780)
        // The three carousel items (X360 +7780, stride 736 -- DWARF h:117 maCarouselItems[3]).
        ImageGalleryCarouselItem maCarouselItems[3];
    };
}
