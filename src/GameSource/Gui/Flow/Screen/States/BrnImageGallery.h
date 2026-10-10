// ============================================================================
// GameSource/Gui/Flow/Screen/States/BrnImageGallery.h
//
// BrnGui::ImageGalleryState - the mugshot/takedown image-gallery GUI screen state
// (base CgsGui::State). Four category tabs across the top, a 20-slot overview
// strip, a three-image carousel (left / middle / right) with arrow animators, the
// who / where / when text of the middle image and the image counters. The image
// data lives in the game state's image manager; the screen talks to it through the
// gallery request / collected-count / collected-data / image-info GUI events.
//
// Layout: member names and order from the class's member list, element counts and
// placement from the console asm (console offsets in the comments; the host widens
// every pointer, so access is by name only). The console carries 20 overview
// selectables: the ctor's vtable loop, OnEnter's construct loop, UpdateSetup's
// enable loop and the collected-data bit walk all stop at 20, and the collected-data
// bit array asserts against 20 bits.
// ============================================================================
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                               // CGS_ASSERT
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiState.h"                  // CgsGui::State (base)
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"   // CgsGui::sResourceTuple
#include "GameSource/GameState/BrnCgsPlayerName.h"                               // CgsNetwork::PlayerName (mSelectedPlayerName)
#include "GameSource/GameState/BrnGameStateSharedIO.h"                           // GameStateModuleIO::EImageGalleryType / EImageGalleryRequest
#include "GameSource/Gui/BrnGuiCache.h"                                          // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiShared.h"                                         // BrnGui::EGuiImageCategories
#include "GameSource/Gui/BrnGuiTextField.h"                                      // BrnGui::TextField
#include "GameSource/Input/GameInputActions.h"                                   // EGameInputActions (TriggerSound)
#include "GameSource/Gui/Flow/Shared/Components/BrnAnimationComponent.h"         // BrnGui::AnimationComponent
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"            // BrnGui::SelectableGroup
#include "GameSource/Gui/Flow/Screen/Components/BrnImageGallerySelectable.h"     // ImageGallerySelectable (the 4 category tabs)
#include "GameSource/Gui/Flow/Screen/Components/BrnImageGalleryCarouselSelectable.h" // ImageGalleryCarouselSelectable (the overview strip)
#include "GameSource/Gui/Flow/Screen/Components/BrnImageGalleryCarouselItem.h"   // ImageGalleryCarouselItem (the 3 carousel slots)

namespace CgsModule { struct Event; }

namespace BrnGui
{
    class ImageGalleryState : public CgsGui::State
    {
    public:
        // The screen's own sequencer (meInternalState).
        enum InternalState
        {
            E_INTERNALSTATE_LOADRESOURCES = 0,
            E_INTERNALSTATE_WFINIT        = 1,
            E_INTERNALSTATE_SETUP         = 2,
            E_INTERNALSTATE_RUNNING       = 3,
            E_INTERNALSTATE_LEFT          = 4,
            E_INTERNALSTATE_COUNT         = 5,
        };

        // The expected-component list bound.
        static const u32 KU_MAX_INIT_COMPONENTS_NUM = 7;
        // The overview strip / per-category image capacity (see the file banner).
        static const s32 KI_MAX_IMAGES = 20;

        // Compiler-emitted ctor: State base + the embedded GUI sub-objects.
        ImageGalleryState();

        virtual void OnEnter();
        virtual void OnLeave();
        virtual void Update();

        // Hand out the gallery's one-APT resource list.
        virtual void GetResourcesToLoad(const CgsGui::sResourceTuple** lppResourceTuples,
                                        u32* lpuNumberOfResources) const
        {
            *lppResourceTuples    = maResourcesToLoad;
            *lpuNumberOfResources = muNumResourcesToLoad;
        }

    private:
        // The per-state steps Update runs.
        bool UpdateLoadResources();
        bool UpdateWFInit();
        void UpdateSetup();
        void UpdateRunning();
        void UpdatePermanent();

        // The expected apt-component list handed to the cache.
        void SetExpectedComponent(const char* lpacComponentName);
        void SetExpectedAptComponentList();
        void ClearExpectedComponent();

        // In-queue handlers. The queue hands out the header-stripped payload, so each
        // takes the raw event pointer.
        void HandleControllerInputPressed(const CgsModule::Event* lpEvent);
        void HandleLoadNotification(const char* lpacComponentName);
        void HandleOverlayComplete(const CgsModule::Event* lpOverlayCompleteEvent);
        void HandleImageInfoEvent(const CgsModule::Event* lpEvent);
        void HandleCollectedDataEvent(const CgsModule::Event* lpEvent);

        EGuiImageCategories GetCurrentCategory();
        void RefreshCarousel(BrnGameState::GameStateModuleIO::EImageGalleryRequest leRequest);
        BrnGameState::GameStateModuleIO::EImageGalleryType
            GetGsmIOCategoryFromGuiEnum(EGuiImageCategories leImageCategory);
        EGuiImageCategories
            GetGuiCategoryFromGsmIOEnum(BrnGameState::GameStateModuleIO::EImageGalleryType leImageGalleryType);
        void SetupCountForCategory(EGuiImageCategories leCategory, s32 liCount);
        void SetupButtons();
        void TriggerSound(EGameInputActions leAction);
        void HideMiddleImage();
        void HideRightImage();

        // Pointer-free layout facts (never called).
        static void _AssertLayout();

        // ---- statics --------------------------------------------------------------------
        static const s32                    maiEventToObserve[8];
        static const s32                    miNumEventsObserved;
        static const CgsGui::sResourceTuple maResourcesToLoad[1];
        static const u32                    muNumResourcesToLoad;
        static const char* const            KAPC_MENU_TITLES[4];
        static const char                   KAC_CAROUSEL_ANIMATOR_NAME[23];
        static const char                   KAC_CAROUSEL_ITEM_LEFT_NAME[20];
        static const char                   KAC_CAROUSEL_ITEM_MIDDLE_NAME[19];
        static const char                   KAC_CAROUSEL_ITEM_RIGHT_NAME[21];
        static const char                   KAC_CAROUSEL_LEFT_ARROW_ANIMATOR_NAME[15];
        static const char                   KAC_CAROUSEL_RIGHT_ARROW_ANIMATOR_NAME[16];
        static const char                   KAC_WHO_TEXT[11];
        static const char                   KAC_WHERE_TEXT[13];
        static const char                   KAC_WHEN_TEXT[12];
        static const char                   KAC_IMAGE_INFO_ANIMATOR_NAME[15];
        static const char                   KAC_CURRENT_IMAGE_TEXT[11];
        static const char                   KAC_TOTAL_IMAGE_TEXT[11];
        static const char                   KAC_BUTTON_ANIMATOR_NAME[13];

        // ---- members (console offsets) ----------------------------------------------------
        GuiCache*                      mpGuiCache;                                    // +56
        InternalState                  meInternalState;                               // +60
        u32                            mauExpectedComponentIds[KU_MAX_INIT_COMPONENTS_NUM]; // +64
        u32                            muNumExpectedComponents;                       // +92
        s32                            miCurrentlySelectedCarouselItem;               // +96
        ImageGallerySelectable         maCategorySelectable[4];                       // +104   (stride 760)
        SelectableGroup                mCategorySelectableGroup;                      // +3144
        ImageGalleryCarouselSelectable maCarouselOverviewSelectable[KI_MAX_IMAGES];   // +3712  (stride 168)
        SelectableGroup                mCarouselOverviewSelectableGroup;              // +7072
        AnimationComponent             mCarouselAnimator;                             // +7640
        ImageGalleryCarouselItem       maCarouselItems[3];                            // +7780  (stride 736)
        AnimationComponent             mCarouselLeftArrowAnimator;                    // +9988
        AnimationComponent             mCarouselRightArrowAnimator;                   // +10128
        s32                            miRequestsLeftPending;                         // +10268
        TextField                      mWhoText;                                      // +10272
        TextField                      mWhereText;                                    // +10568
        TextField                      mWhenText;                                     // +10864
        bool                           mbIsCurrentLocked;                             // +11160
        s32                            maiPhotoCountPerCategory[4];                   // +11164
        AnimationComponent             mImageInfoAnimator;                            // +11180
        TextField                      mCurrentImageText;                             // +11320
        TextField                      mTotalImageText;                               // +11616
        AnimationComponent             mButtonAnimator;                               // +11912
        CgsNetwork::PlayerName         mSelectedPlayerName;                           // +12052
        bool                           mbSelectedImageValid;                          // +12068
        bool                           mbPendingSnapShotRequest;                      // +12069
        s8                             miSnapShotDelayCounter;                        // +12070
    };
}
