#include "GameSource/Gui/Flow/Screen/States/BrnImageGallery.h"

#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"            // CgsContainers::FastBitArray (collected-data bits)
#include "GameShared/GameClasses/Containers/CgsHash.h"                    // CgsContainers::CgsHash::CalculateHash
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                            // CgsIDCompress
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf / SnPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventWrapper
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                      // CgsGui::GuiAccessPointers (the cache pointer)
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // CgsGui::GuiEventAptTriggerPayload (event 21)
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // FormatDateString / ParameterFormatType
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameShared/GameClasses/System/Timer/PS3/CgsDateAndTimePS3.h"    // CgsSystem::DateAndTime (image capture date)
#include "GameSource/Input/GameInputActions.h"                            // EGameInputActions (controller actions)
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiOverlayRequest / GuiOverlayCompleteEvent / GuiAudioTriggerEvent / GuiEventActivateCrashNav / GuiFlow
#include "SharedClasses/World/BrnWorldRegion.h"                           // BrnWorld::WorldRegion (image capture district)

#include <cstring>   // std::strstr / std::strcmp

// BrnGui::ImageGalleryState -- the image-gallery screen state.
//
//   OnEnter / OnLeave / Update and the whole Update closure: the five-step sequencer
//   (UpdateLoadResources / UpdateWFInit / UpdateSetup / UpdateRunning, plus
//   UpdatePermanent every live frame), the in-queue handlers (controller, load
//   notification, overlay complete, image info, collected count / data), the carousel
//   refresh and the small view helpers. Out-queue records are posted through
//   GetOutputEventQueue()->AddEvent with their console wire sizes, channel 40 (GUI out)
//   unless noted.

namespace BrnGui
{
    namespace GameStateModuleIO = BrnGameState::GameStateModuleIO;

    // ---- statics ---------------------------------------------------------------------------
    // The events the screen subscribes to, in the console's order: controller action, apt
    // trigger, gui cache, image info, collected count, collected data, image load complete,
    // overlay complete.
    const s32 ImageGalleryState::maiEventToObserve[8] = { 6, 21, 64, 518, 520, 522, 523, 189 };
    const s32 ImageGalleryState::miNumEventsObserved  = 8;

    // The gallery's apt package (resource id 164, "ON_IMG_GAL").
    const CgsGui::sResourceTuple ImageGalleryState::maResourcesToLoad[1] =
        { { 164, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const u32 ImageGalleryState::muNumResourcesToLoad = 1;

    // The category tab titles (indexed by EGuiImageCategories).
    const char* const ImageGalleryState::KAPC_MENU_TITLES[4] =
    {
        "$IMAGE_GALLERY_FREEBURN",
        "$IMAGE_GALLERY_EVENT_MUGSHOTS",
        "$IMAGE_GALLERY_RULEBREAKER",
        "$IMAGE_GALLERY_PHOTOFINISH",
    };

    const char ImageGalleryState::KAC_CAROUSEL_ANIMATOR_NAME[23]             = "CarouselAnimation_anim";
    const char ImageGalleryState::KAC_CAROUSEL_ITEM_LEFT_NAME[20]            = "CarouselItemLeft_mc";
    const char ImageGalleryState::KAC_CAROUSEL_ITEM_MIDDLE_NAME[19]          = "CarouselItemMid_mc";
    const char ImageGalleryState::KAC_CAROUSEL_ITEM_RIGHT_NAME[21]           = "CarouselItemRight_mc";
    const char ImageGalleryState::KAC_CAROUSEL_LEFT_ARROW_ANIMATOR_NAME[15]  = "ArrowLeft_anim";
    const char ImageGalleryState::KAC_CAROUSEL_RIGHT_ARROW_ANIMATOR_NAME[16] = "ArrowRight_anim";
    const char ImageGalleryState::KAC_WHO_TEXT[11]                           = "WhoText_mc";
    const char ImageGalleryState::KAC_WHERE_TEXT[13]                         = "WhereText_mc";
    const char ImageGalleryState::KAC_WHEN_TEXT[12]                          = "WhenText_mc";
    const char ImageGalleryState::KAC_IMAGE_INFO_ANIMATOR_NAME[15]           = "ImageInfo_anim";
    const char ImageGalleryState::KAC_CURRENT_IMAGE_TEXT[11]                 = "ImageNo_mc";
    const char ImageGalleryState::KAC_TOTAL_IMAGE_TEXT[11]                   = "TotalNo_mc";
    const char ImageGalleryState::KAC_BUTTON_ANIMATOR_NAME[13]               = "Buttons_anim";

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type
        typedef CgsLanguage::LanguageManager             LanguageManager;

        const s32 KI_CHANNEL_GUI_OUT = 40;

        // The in-queue events Update's steps dispatch on (the maiEventToObserve set).
        const s32 KI_EVENT_CONTROLLER_INPUT  = 6;
        const s32 KI_EVENT_APT_TRIGGER       = 21;
        const s32 KI_EVENT_OVERLAY_COMPLETE  = 189;
        const s32 KI_EVENT_IMAGE_INFO        = 518;
        const s32 KI_EVENT_COLLECTED_COUNT   = 520;
        const s32 KI_EVENT_COLLECTED_DATA    = 522;
        const s32 KI_EVENT_IMAGE_LOAD_DONE   = 523;

        // The out-queue ids this screen posts.
        const s32 KI_GUI_EVENT_SHOW_HIDE_HUD               = 148;
        const s32 KI_GUI_EVENT_IMAGE_EXPORT_REQUESTED      = 361;
        const s32 KI_GUI_EVENT_AUDIO_TRIGGER               = 457;
        const s32 KI_GUI_EVENT_IMAGE_GALLERY_REQUEST       = 517;
        const s32 KI_GUI_EVENT_REQUEST_COLLECTED_COUNT     = 519;
        const s32 KI_GUI_EVENT_REQUEST_COLLECTED_DATA      = 521;

        // The apt movie the screen plays (gGuiResourceIdentifier[164] == "ON_IMG_GAL", the
        // resource list's id) on display level 3; OnLeave unbinds the level with "".
        const s32         KI_RESOURCE_ID_IMAGE_GALLERY = 164;
        const s32         KI_APT_MOVIE_LEVEL           = 3;
        const char* const KPC_EMPTY                    = "";

        // The component apt id argument (no parent, unset id).
        const u64 KU_NO_APT_ID = 0xFFFFFFFFull;

        // GuiAudioTriggerEvent::meAction for every menu cue.
        const s32 KI_AUDIO_ACTION_MENU_CUE = 7;

        // The overlay ids the screen raises / reacts to.
        const char KAC_DELETE_OVERLAY[] = "LineUpDelete";
        const char KAC_EXPORT_OVERLAY[] = "LineUpExport";

        // The district localisation ids the "where" text looks up (an image table of
        // EDistrict-indexed ids).
        const char* const KAPC_DISTRICT_TEXT_IDS[18] =
        {
            "NHD_OV",  "NHD_WA", "NHD_TB", "NHD_BS", "NHD_ES", "NHD_HP",
            "NHD_HH",  "NHD_RRC","NHD_SB", "NHD_PV", "NHD_PW", "NHD_CS",
            "NHD_LP",  "NHD_SV", "NHD_DT", "NHD_RC", "NHD_MC", "NHD_WF",
        };

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04
        };

        struct OverlayCompletePayload : public CgsModule::Event
        {
            CgsID                                mOverlayId;     // +0x00
            GuiOverlayCompleteEvent::LeaveMethod meLeaveMethod;  // +0x08
        };

        // Event 518: one gallery slot's image info.
        struct ImageInfoPayload : public CgsModule::Event
        {
            CgsSystem::DateAndTime mCaptureDate;   // +0x00
            BrnWorld::WorldRegion  mWorldRegion;   // +0x0C (district +0x10)
            s32                    miNumCaptures;  // +0x14
            s32                    miSlotIndex;    // +0x18
            CgsNetwork::PlayerName mPlayerName;    // +0x1C
            bool                   mbLocked;       // +0x2C
            bool                   mbHasDeleted;   // +0x2D
        };

        // Event 520: how many images a gallery type holds.
        struct CollectedCountPayload : public CgsModule::Event
        {
            GameStateModuleIO::EImageGalleryType meImageGalleryImageType;   // +0x00
            s32                                  miCollectedCount;          // +0x04
        };

        // Event 522: which of a gallery type's image slots are occupied.
        struct CollectedDataPayload : public CgsModule::Event
        {
            GameStateModuleIO::EImageGalleryType                          meImageGalleryImageType; // +0x00
            CgsContainers::FastBitArray<ImageGalleryState::KI_MAX_IMAGES> mActiveBitArray;         // +0x08
        };

        // ---- out-queue payloads (boxed by GuiEventWrapper<T, 40>) ---------------------------
        // Id 517: { image index, on-screen slot, request kind, gallery type } (28-byte record).
        struct ImageGalleryRequestPayload
        {
            s32                                     miImageIndex;
            s32                                     miSlotIndex;
            GameStateModuleIO::EImageGalleryRequest meImageGalleryRequest;
            GameStateModuleIO::EImageGalleryType    meImageGalleryImageType;

            s32 GetEventType() const { return KI_GUI_EVENT_IMAGE_GALLERY_REQUEST; }
        };

        // Ids 519 / 521: one gallery type (16-byte records).
        template <s32 N>
        struct ImageGalleryTypePayload
        {
            GameStateModuleIO::EImageGalleryType meImageGalleryImageType;

            s32 GetEventType() const { return N; }
        };

        // Id 361: { vignette flag, player name } (17 payload bytes, 32-byte record).
        struct ImageExportRequestedPayload
        {
            bool                   mbVignette;
            CgsNetwork::PlayerName mPlayerName;

            s32 GetEventType() const { return KI_GUI_EVENT_IMAGE_EXPORT_REQUESTED; }
        };

        static_assert(sizeof(ImageGalleryRequestPayload) == 16, "request payload is 16 bytes");
        static_assert(sizeof(ImageExportRequestedPayload) == 17, "export payload is 17 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<ImageGalleryRequestPayload, 40>) == 28,
                      "request record is 28 bytes");
        static_assert(sizeof(CgsGui::GuiEventWrapper<ImageExportRequestedPayload, 40>) == 32,
                      "export record is 32 bytes");

        // { 1, 148, 12, <show byte> }: 16 bytes.
        struct GuiShowHideHudWire : public CgsGui::GuiEvent<KI_GUI_EVENT_SHOW_HIDE_HUD>
        {
            u8 mu8Show;   // +0x0C
            u8 maPad[3];

            explicit GuiShowHideHudWire(bool lbShow)
                : CgsGui::GuiEvent<KI_GUI_EVENT_SHOW_HIDE_HUD>(static_cast<u32>(sizeof(u8)), 12)
                , mu8Show(lbShow ? 1 : 0)
            {
                maPad[0] = maPad[1] = maPad[2] = 0;
            }
        };

        // { 288, 184, 16, <pad>, the 288-byte request }: 304 bytes.
        struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
        {
            u32               muPad0C;    // +0x0C
            GuiOverlayRequest mRequest;   // +0x10

            GuiOverlayRequestWire()
                : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
                , muPad0C(0)
            {
            }
        };

        static_assert(sizeof(GuiShowHideHudWire) == 16, "show/hide hud record is 16 bytes");
        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate-crashnav record is 20 bytes");
        static_assert(sizeof(GuiAudioTriggerEvent) == 112, "audio trigger record is 112 bytes");

        template <class TPayload>
        void PostWrapped(CgsGui::StateInterface* lpStateInterface, TPayload& lrPayload)
        {
            CgsGui::GuiEventWrapper<TPayload, KI_CHANNEL_GUI_OUT> lWrapper(lrPayload);
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWrapper), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lWrapper)));
        }

        template <class TRecord>
        void PostRecord(CgsGui::StateInterface* lpStateInterface, const TRecord& lrRecord)
        {
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lrRecord), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lrRecord)));
        }

        void PostImageGalleryRequest(CgsGui::StateInterface* lpStateInterface, s32 liImageIndex,
                                     s32 liSlotIndex, GameStateModuleIO::EImageGalleryRequest leRequest,
                                     GameStateModuleIO::EImageGalleryType leType)
        {
            ImageGalleryRequestPayload lRequest;
            lRequest.miImageIndex            = liImageIndex;
            lRequest.miSlotIndex             = liSlotIndex;
            lRequest.meImageGalleryRequest   = leRequest;
            lRequest.meImageGalleryImageType = leType;
            PostWrapped(lpStateInterface, lRequest);
        }

        template <s32 N>
        void PostImageGalleryTypeRequest(CgsGui::StateInterface* lpStateInterface,
                                         GameStateModuleIO::EImageGalleryType leType)
        {
            ImageGalleryTypePayload<N> lRequest;
            lRequest.meImageGalleryImageType = leType;
            PostWrapped(lpStateInterface, lRequest);
        }

        void PostOverlayRequest(CgsGui::StateInterface* lpStateInterface, const char* lpcOverlayId)
        {
            GuiOverlayRequestWire lWire;
            lWire.mRequest.Construct(lpcOverlayId);
            PostRecord(lpStateInterface, lWire);
        }

        // "%d" of liValue through the integer formatter into lrField.
        void SetIntegerText(TextField& lrField, s32 liValue)
        {
            char lacText[64];
            CgsCore::SnPrintf(lacText, sizeof(lacText), "%d", liValue);
            lacText[sizeof(lacText) - 1] = 0;
            lrField.SetLocalisedText(lacText, LanguageManager::E_FORMAT_INTEGER);
        }
    }

    // Compiler-emitted construction of the image-gallery flow state: the CgsGui::State base
    // plus the embedded GUI sub-objects (each lays down its own vtables). No member payload
    // beyond the vtables is set here; OnEnter initialises the state.
    ImageGalleryState::ImageGalleryState()
        : CgsGui::State()
    {
    }

    // Subscribe, take the cache, build every component (the four category tabs, the 20
    // overview slots, the carousel and its animators / text fields), reset the counters
    // and post "crash nav inactive" + "hide the HUD".
    void ImageGalleryState::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();
        mbPendingSnapShotRequest = false;

        // The console formats into 31 characters and terminates the 32nd byte.
        char lacName[32];

        mCategorySelectableGroup.Construct(KPC_EMPTY, mpStateInterface, 0, KU_NO_APT_ID);
        for (s32 liCategory = 0; liCategory < E_GUI_IMAGE_CATEGORIES_COUNT; ++liCategory)
        {
            CgsCore::SPrintf(lacName, sizeof(lacName) - 1, "categoryMenu%i_mc", liCategory);
            lacName[sizeof(lacName) - 1] = 0;

            ImageGallerySelectable& lrTab = maCategorySelectable[liCategory];
            lrTab.Construct(lacName, mpStateInterface, 0, KU_NO_APT_ID);
            mCategorySelectableGroup.Add(&lrTab);
            lrTab.SetCategory(KAPC_MENU_TITLES[liCategory]);
            lrTab.SetCollected(0);
        }

        mCarouselOverviewSelectableGroup.Construct(KPC_EMPTY, mpStateInterface, 0, KU_NO_APT_ID);
        for (s32 liSlot = 0; liSlot < KI_MAX_IMAGES; ++liSlot)
        {
            CgsCore::SPrintf(lacName, sizeof(lacName) - 1, "Overview_%i_mc", liSlot);
            lacName[sizeof(lacName) - 1] = 0;

            ImageGalleryCarouselSelectable& lrSlot = maCarouselOverviewSelectable[liSlot];
            lrSlot.Construct(lacName, mpStateInterface, 0, KU_NO_APT_ID);
            mCarouselOverviewSelectableGroup.Add(&lrSlot);
        }

        mCarouselAnimator.Construct(KAC_CAROUSEL_ANIMATOR_NAME, mpStateInterface, 0);
        maCarouselItems[0].Construct(KAC_CAROUSEL_ITEM_LEFT_NAME, mpStateInterface, 0);
        maCarouselItems[1].Construct(KAC_CAROUSEL_ITEM_MIDDLE_NAME, mpStateInterface, 0);
        maCarouselItems[2].Construct(KAC_CAROUSEL_ITEM_RIGHT_NAME, mpStateInterface, 0);
        mCarouselLeftArrowAnimator.Construct(KAC_CAROUSEL_LEFT_ARROW_ANIMATOR_NAME, mpStateInterface, 0);
        mCarouselRightArrowAnimator.Construct(KAC_CAROUSEL_RIGHT_ARROW_ANIMATOR_NAME, mpStateInterface, 0);

        miCurrentlySelectedCarouselItem = 0;
        mbIsCurrentLocked               = false;
        miRequestsLeftPending           = 1;

        mWhoText.Construct(KAC_WHO_TEXT, mpStateInterface, 0);
        mWhereText.Construct(KAC_WHERE_TEXT, mpStateInterface, 0);
        mWhenText.Construct(KAC_WHEN_TEXT, mpStateInterface, 0);
        mImageInfoAnimator.Construct(KAC_IMAGE_INFO_ANIMATOR_NAME, mpStateInterface, 0);
        mCurrentImageText.Construct(KAC_CURRENT_IMAGE_TEXT, mpStateInterface, 0);
        mTotalImageText.Construct(KAC_TOTAL_IMAGE_TEXT, mpStateInterface, 0);
        mButtonAnimator.Construct(KAC_BUTTON_ANIMATOR_NAME, mpStateInterface, 0);

        std::memset(maiPhotoCountPerCategory, 0, sizeof(maiPhotoCountPerCategory));

        meInternalState = E_INTERNALSTATE_LOADRESOURCES;

        PostRecord(mpStateInterface, GuiEventActivateCrashNav(false));
        PostRecord(mpStateInterface, GuiShowHideHudWire(false));

        mbSelectedImageValid = false;
    }

    // Stop listening, unbind the movie level and mark the state left.
    void ImageGalleryState::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);
        mpStateInterface->PlayAptMovie(KPC_EMPTY, KI_APT_MOVIE_LEVEL);
        meInternalState = E_INTERNALSTATE_LEFT;
    }

    // Run the sequencer: each completed step falls through into the next within the same
    // frame (load -> wait for components -> set up -> running). Every live frame then runs
    // the permanent step, and the in-queue is cleared.
    void ImageGalleryState::Update()
    {
        switch (meInternalState)
        {
            case E_INTERNALSTATE_LOADRESOURCES:
                meInternalState = E_INTERNALSTATE_LOADRESOURCES;
                if (!UpdateLoadResources())
                {
                    break;
                }
                // fall through
            case E_INTERNALSTATE_WFINIT:
                meInternalState = E_INTERNALSTATE_WFINIT;
                if (!UpdateWFInit())
                {
                    break;
                }
                // fall through
            case E_INTERNALSTATE_SETUP:
                meInternalState = E_INTERNALSTATE_SETUP;
                UpdateSetup();
                // fall through
            case E_INTERNALSTATE_RUNNING:
                meInternalState = E_INTERNALSTATE_RUNNING;
                UpdateRunning();
                break;

            case E_INTERNALSTATE_LEFT:
                break;

            default:
                // The console streams "Invalid internal state : " << meInternalState.
                CGS_ASSERT(false, "Invalid internal state : ");
                break;
        }

        if (meInternalState != E_INTERNALSTATE_LEFT)
        {
            UpdatePermanent();
        }

        reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue)->Clear();
    }

    // Once the gallery package is resident: register the four tabs as the expected apt
    // components and play the movie.
    bool ImageGalleryState::UpdateLoadResources()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        if (!mpGuiCache->EnsureResourcesAreLoaded(maResourcesToLoad, muNumResourcesToLoad))
        {
            return false;
        }

        SetExpectedAptComponentList();
        mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KI_RESOURCE_ID_IMAGE_GALLERY],
                                       KI_APT_MOVIE_LEVEL);
        return true;
    }

    // The per-frame init poll: once the cache reports every expected component initialised
    // (screen flow layer), clear the list and report done.
    bool ImageGalleryState::UpdateWFInit()
    {
        if (!mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            return false;
        }
        ClearExpectedComponent();
        return true;
    }

    // Enable the four tabs (asking the game for each tab's image count), highlight the first,
    // enable the overview strip and ask for the first gallery's occupancy, then hide both
    // arrows and print the current image number.
    void ImageGalleryState::UpdateSetup()
    {
        for (EGuiImageCategories leCategory = E_GUI_IMAGE_CATEGORIES_FIRST;
             leCategory < E_GUI_IMAGE_CATEGORIES_COUNT; leCategory++)
        {
            mCategorySelectableGroup.Enable(leCategory);
            PostImageGalleryTypeRequest<KI_GUI_EVENT_REQUEST_COLLECTED_COUNT>(
                mpStateInterface, GetGsmIOCategoryFromGuiEnum(leCategory));
        }
        mCategorySelectableGroup.HighlightIndex(0);

        for (s32 liSlot = 0; liSlot < KI_MAX_IMAGES; ++liSlot)
        {
            mCarouselOverviewSelectableGroup.Enable(liSlot);
        }
        PostImageGalleryTypeRequest<KI_GUI_EVENT_REQUEST_COLLECTED_DATA>(
            mpStateInterface, GameStateModuleIO::E_IMAGE_GALLERY_TYPE_FREEBURN_MUGSHOT);
        mCarouselOverviewSelectableGroup.HighlightIndex(0);

        mCarouselLeftArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
        mCarouselRightArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);

        SetIntegerText(mCurrentImageText, miCurrentlySelectedCarouselItem + 1);
    }

    // Drain the in-queue (stopping once the state has been left), update both selectable
    // groups, then run the delayed snapshot export when one is pending.
    void ImageGalleryState::UpdateRunning()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0 && meInternalState != E_INTERNALSTATE_LEFT;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            switch (liEventId)
            {
                case KI_EVENT_CONTROLLER_INPUT:
                    HandleControllerInputPressed(lpEvent);
                    break;

                case KI_EVENT_OVERLAY_COMPLETE:
                    HandleOverlayComplete(lpEvent);
                    break;

                case KI_EVENT_IMAGE_INFO:
                    HandleImageInfoEvent(lpEvent);
                    break;

                case KI_EVENT_COLLECTED_COUNT:
                {
                    const CollectedCountPayload* lpCount =
                        reinterpret_cast<const CollectedCountPayload*>(lpEvent);
                    SetupCountForCategory(GetGuiCategoryFromGsmIOEnum(lpCount->meImageGalleryImageType),
                                          lpCount->miCollectedCount);
                    break;
                }

                case KI_EVENT_COLLECTED_DATA:
                    HandleCollectedDataEvent(lpEvent);
                    break;

                case KI_EVENT_IMAGE_LOAD_DONE:
                    miRequestsLeftPending = 0;
                    for (s32 liItem = 0; liItem < 3; ++liItem)
                    {
                        maCarouselItems[liItem].HideLoading();
                    }
                    break;

                default:
                    break;
            }
        }

        mCategorySelectableGroup.Update();
        mCarouselOverviewSelectableGroup.Update();

        if (mbPendingSnapShotRequest)
        {
            --miSnapShotDelayCounter;
            if (miSnapShotDelayCounter < 1)
            {
                mbPendingSnapShotRequest = false;

                ImageExportRequestedPayload lExport;
                lExport.mbVignette  = (GetCurrentCategory() == E_GUI_IMAGE_CATEGORIES_PHOTO_FINISH);
                lExport.mPlayerName = mSelectedPlayerName;
                PostWrapped(mpStateInterface, lExport);

                PostOverlayRequest(mpStateInterface, KAC_EXPORT_OVERLAY);
            }
        }
    }

    // Every live frame: forward apt "loaded" triggers to the components.
    void ImageGalleryState::UpdatePermanent()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            if (liEventId == KI_EVENT_APT_TRIGGER)
            {
                const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
                    reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
                if (lpTrigger->meEventType == CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
                {
                    HandleLoadNotification(lpTrigger->mpacComponentName);
                }
            }
        }
    }

    // Register one expected apt component by name: bounds-check the 7-slot list, hash the
    // NUL-terminated name (CgsHash over strlen bytes), append it and bump the count. The
    // console's overflow text is streamed; collapsed to the static message.
    void ImageGalleryState::SetExpectedComponent(const char* lpacComponentName)
    {
        CGS_ASSERT(muNumExpectedComponents < KU_MAX_INIT_COMPONENTS_NUM,
                   "No space for new expected component");

        // strlen the name (the console's while(*p++) walk) then hash the byte span.
        const char* lpcEnd = lpacComponentName;
        while (*lpcEnd++)
        {
        }
        const s32 liLength = static_cast<s32>(lpcEnd - lpacComponentName - 1);

        // CalculateHash takes a mutable char*; the name is read-only here.
        const u32 luHash =
            CgsContainers::CgsHash::CalculateHash(const_cast<char*>(lpacComponentName), liLength);

        mauExpectedComponentIds[muNumExpectedComponents] = luHash;
        ++muNumExpectedComponents;
    }

    // Rebuild the expected-component list from the four category tabs and hand it to the
    // cache (screen flow layer).
    void ImageGalleryState::SetExpectedAptComponentList()
    {
        ClearExpectedComponent();
        for (s32 liCategory = 0; liCategory < E_GUI_IMAGE_CATEGORIES_COUNT; ++liCategory)
        {
            SetExpectedComponent(maCategorySelectable[liCategory].GetName());
        }
        CGS_ASSERT(muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM,
                   "muNumExpectedComponents <= KU_MAX_INIT_COMPONENTS_NUM");
        mpGuiCache->SetExpectedAptComponentList(E_GUIFLOW_SCREEN, mauExpectedComponentIds,
                                                muNumExpectedComponents);
    }

    // Clear the pending expected-component list: zero the seven id slots and the count,
    // assert the cache pointer, then drop the screen layer's watch list.
    void ImageGalleryState::ClearExpectedComponent()
    {
        for (u32 luSlot = 0; luSlot < KU_MAX_INIT_COMPONENTS_NUM; ++luSlot)
        {
            mauExpectedComponentIds[luSlot] = 0;
        }
        muNumExpectedComponents = 0;

        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
    }

    // Running only. Up / down change the category tab (resetting the carousel to the first
    // image and asking for the new gallery's occupancy); left / right step the carousel;
    // select asks to lock the middle image, X to delete it (unless locked), Y to show the
    // owner's gamer card; cancel leaves (through the easy-drive exit when the screen was
    // entered that way). Every image request is refused while earlier ones are in flight.
    void ImageGalleryState::HandleControllerInputPressed(const CgsModule::Event* lpEvent)
    {
        if (meInternalState != E_INTERNALSTATE_RUNNING)
        {
            return;
        }

        const EGameInputActions leAction =
            static_cast<EGameInputActions>(reinterpret_cast<const ControllerButtonPayload*>(lpEvent)->miButtonId);
        switch (leAction)
        {
            case E_GAMEINPUTACTIONS_GUI_UP:
            case E_GAMEINPUTACTIONS_GUI_DOWN:
            {
                if (miRequestsLeftPending > 0)
                {
                    break;
                }

                const bool lbMoved = (leAction == E_GAMEINPUTACTIONS_GUI_UP)
                                         ? mCategorySelectableGroup.HighlightPrevious(false)
                                         : mCategorySelectableGroup.HighlightNext(false);
                if (!lbMoved)
                {
                    break;
                }

                mCarouselAnimator.AddOutputAptViewState("apt_Transition", "transin", false);
                miCurrentlySelectedCarouselItem = 0;
                mCarouselOverviewSelectableGroup.HighlightIndex(0);
                RefreshCarousel(GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_NEW_IMAGES);

                PostImageGalleryTypeRequest<KI_GUI_EVENT_REQUEST_COLLECTED_DATA>(
                    mpStateInterface, GetGsmIOCategoryFromGuiEnum(GetCurrentCategory()));

                if (maiPhotoCountPerCategory[GetCurrentCategory()] > 0)
                {
                    mImageInfoAnimator.AddOutputAptViewState("apt_Transition", "info", false);
                    SetIntegerText(mTotalImageText, maiPhotoCountPerCategory[GetCurrentCategory()]);
                }
                else
                {
                    mImageInfoAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
                }

                SetupButtons();
                TriggerSound(leAction);
                break;
            }

            case E_GAMEINPUTACTIONS_GUI_LEFT:
                if (miRequestsLeftPending > 0 || miCurrentlySelectedCarouselItem <= 0)
                {
                    break;
                }
                --miCurrentlySelectedCarouselItem;
                mCarouselOverviewSelectableGroup.HighlightIndex(miCurrentlySelectedCarouselItem);
                RefreshCarousel(GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_LEFT);
                TriggerSound(leAction);
                break;

            case E_GAMEINPUTACTIONS_GUI_RIGHT:
            {
                if (miRequestsLeftPending > 0)
                {
                    break;
                }
                const s32 liCurrent = miCurrentlySelectedCarouselItem;
                if (liCurrent >= maiPhotoCountPerCategory[GetCurrentCategory()] - 1)
                {
                    break;
                }
                miCurrentlySelectedCarouselItem = liCurrent + 1;
                mCarouselOverviewSelectableGroup.HighlightIndex(miCurrentlySelectedCarouselItem);
                RefreshCarousel(GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_RIGHT);
                TriggerSound(leAction);
                break;
            }

            case E_GAMEINPUTACTIONS_GUI_SELECT:
                if (miRequestsLeftPending > 0)
                {
                    break;
                }
                PostImageGalleryRequest(mpStateInterface, miCurrentlySelectedCarouselItem, 1,
                                        GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_LOCK,
                                        GetGsmIOCategoryFromGuiEnum(GetCurrentCategory()));
                ++miRequestsLeftPending;
                break;

            case E_GAMEINPUTACTIONS_GUI_CANCEL:
                if (mpGuiCache->IsOnlineStartPending())
                {
                    CgsGui::GuiEventNetworkSuspension lSuspension(false);
                    mpStateInterface->OutputGuiEvent(lSuspension);
                    mpGuiCache->SetOnlineStartPending(false);
                    SendStateEvent("GO_BACK_EASY");
                }
                else
                {
                    SendStateEvent("GO_BACK");
                }
                break;

            case E_GAMEINPUTACTIONS_GUI_OPTION0:
                if (miRequestsLeftPending > 0 || mbIsCurrentLocked == true)
                {
                    break;
                }
                PostImageGalleryRequest(mpStateInterface, miCurrentlySelectedCarouselItem, 1,
                                        GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_ASK_DELETE,
                                        GetGsmIOCategoryFromGuiEnum(GetCurrentCategory()));
                ++miRequestsLeftPending;
                break;

            case E_GAMEINPUTACTIONS_GUI_OPTION1:
                if (miRequestsLeftPending > 0)
                {
                    break;
                }
                PostImageGalleryRequest(mpStateInterface, miCurrentlySelectedCarouselItem, 1,
                                        GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SHOW_GAMERCARD,
                                        GetGsmIOCategoryFromGuiEnum(GetCurrentCategory()));
                break;

            default:
                break;
        }
    }

    // A component finished loading: a category tab, the carousel animator (which starts the
    // carousel off) or one of the carousel items.
    void ImageGalleryState::HandleLoadNotification(const char* lpacComponentName)
    {
        for (s32 liCategory = 0; liCategory < E_GUI_IMAGE_CATEGORIES_COUNT; ++liCategory)
        {
            if (std::strstr(lpacComponentName, maCategorySelectable[liCategory].GetName()) != 0)
            {
                maCategorySelectable[liCategory].HandleLoadNotifications(lpacComponentName);
                return;
            }
        }

        if (std::strcmp(lpacComponentName, mCarouselAnimator.GetName()) == 0)
        {
            mCarouselAnimator.AddOutputAptViewState("apt_Transition", "transin", false);
            RefreshCarousel(GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_NEW_IMAGES);
            return;
        }

        for (s32 liItem = 0; liItem < 3; ++liItem)
        {
            if (std::strstr(lpacComponentName, maCarouselItems[liItem].GetName()) != 0)
            {
                maCarouselItems[liItem].HandleLoadNotifications(lpacComponentName);
                return;
            }
        }
    }

    // The delete question closed: OK asks the game to delete the middle image, anything
    // else just releases the request lock. The export overlay closed: release the lock and
    // clear the cache's export-in-progress flag.
    void ImageGalleryState::HandleOverlayComplete(const CgsModule::Event* lpOverlayCompleteEvent)
    {
        CGS_ASSERT(lpOverlayCompleteEvent != 0, "lpOverlayCompleteEvent");

        const OverlayCompletePayload* lpPayload =
            reinterpret_cast<const OverlayCompletePayload*>(lpOverlayCompleteEvent);

        if (lpPayload->mOverlayId == CgsIDCompress(KAC_DELETE_OVERLAY))
        {
            if (lpPayload->meLeaveMethod == GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK)
            {
                PostImageGalleryRequest(mpStateInterface, miCurrentlySelectedCarouselItem, 1,
                                        GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_DELETE,
                                        GetGsmIOCategoryFromGuiEnum(GetCurrentCategory()));
            }
            else
            {
                miRequestsLeftPending = 0;
            }
        }
        else if (lpPayload->mOverlayId == CgsIDCompress(KAC_EXPORT_OVERLAY))
        {
            miRequestsLeftPending = 0;
            CGS_ASSERT(mpGuiCache, "mpGuiCache");
            mpGuiCache->SetImageExportInProgress(false);
        }
    }

    // One carousel slot's image info arrived. A populated slot shows its gamertag and lock;
    // for the middle slot it also fills the who / where / when text, the lock state behind
    // the buttons and the selected player. An empty slot is blanked; when the middle image
    // was just deleted the carousel steps back (or hides the middle image when it was the
    // last one).
    void ImageGalleryState::HandleImageInfoEvent(const CgsModule::Event* lpEvent)
    {
        const ImageInfoPayload* lpInfo = reinterpret_cast<const ImageInfoPayload*>(lpEvent);

        // The console streams "Too many responses received... Pending : " << pending.
        CGS_ASSERT(miRequestsLeftPending >= 0, "Too many responses received... Pending : ");

        const s32 liSlot = lpInfo->miSlotIndex;
        if (liSlot < 0 || liSlot >= 3)
        {
            // The console streams "Invalid slot index passed through from online : " << slot.
            CGS_ASSERT(false, "Invalid slot index passed through from online : ");
            return;
        }

        ImageGalleryCarouselItem& lrItem = maCarouselItems[liSlot];

        if (lpInfo->mPlayerName.GetPlayerName()[0] != 0)
        {
            lrItem.SetGamertag(lpInfo->mPlayerName.GetPlayerName());
            lrItem.SetLocked(lpInfo->mbLocked);

            if (liSlot != 1)
            {
                return;
            }

            if (lpInfo->mbHasDeleted &&
                miCurrentlySelectedCarouselItem == maiPhotoCountPerCategory[GetCurrentCategory()] - 2)
            {
                HideRightImage();
            }

            mbIsCurrentLocked = lpInfo->mbLocked;
            SetupButtons();

            mWhoText.SetLocalisedText(lpInfo->mPlayerName.GetPlayerName(), LanguageManager::E_FORMAT_TEXT);
            mWhereText.SetLocalisedText(KAPC_DISTRICT_TEXT_IDS[lpInfo->mWorldRegion.GetDistrict()],
                                        LanguageManager::E_FORMAT_ID_LOOKUP);

            const s32 liYear  = lpInfo->mCaptureDate.GetYear();
            const s32 liMonth = lpInfo->mCaptureDate.GetMonth();
            const s32 liDay   = lpInfo->mCaptureDate.GetDay();
            char lacDate[64];
            mpStateInterface->GetLanguageManager()->FormatDateString(lacDate, liDay, liMonth, liYear,
                                                                    sizeof(lacDate));
            lacDate[sizeof(lacDate) - 1] = 0;
            mWhenText.SetLocalisedText(lacDate, LanguageManager::E_FORMAT_TEXT);

            mSelectedPlayerName  = lpInfo->mPlayerName;
            mbSelectedImageValid = true;
        }
        else
        {
            lrItem.SetGamertag(KPC_EMPTY);
            lrItem.SetLocked(false);
            lrItem.HideLoading();

            if (liSlot != 1 || !lpInfo->mbHasDeleted)
            {
                return;
            }

            if (maiPhotoCountPerCategory[GetCurrentCategory()] == 1)
            {
                HideMiddleImage();
            }
            else if (miCurrentlySelectedCarouselItem == maiPhotoCountPerCategory[GetCurrentCategory()] - 1)
            {
                // The last image went: step the carousel left as if the player had pressed
                // left (the pad id word is left unset by the console).
                miRequestsLeftPending = 0;
                ControllerButtonPayload lPress;
                lPress.miPadId    = 0;
                lPress.miButtonId = E_GAMEINPUTACTIONS_GUI_LEFT;
                HandleControllerInputPressed(&lPress);
                HideRightImage();
            }
        }
    }

    // Mirror the gallery's occupancy bits onto the overview strip (dirtying every slot whose
    // used flag changes), dirty the strip, then count the images for that category.
    void ImageGalleryState::HandleCollectedDataEvent(const CgsModule::Event* lpEvent)
    {
        const CollectedDataPayload* lpData = reinterpret_cast<const CollectedDataPayload*>(lpEvent);

        s32 liCollected = 0;
        for (s32 liSlot = 0; liSlot < KI_MAX_IMAGES; ++liSlot)
        {
            // The console's inlined range check streams "Index " << i << " is out of range
            // (max bits: " << 20; the loop bound keeps it unreachable.
            if (lpData->mActiveBitArray.IsBitSet(static_cast<u32>(liSlot)))
            {
                maCarouselOverviewSelectable[liSlot].SetUsed(true);
                ++liCollected;
            }
            else
            {
                maCarouselOverviewSelectable[liSlot].SetUsed(false);
            }
        }
        mCarouselOverviewSelectableGroup.SetDirty();

        SetupCountForCategory(GetGuiCategoryFromGsmIOEnum(lpData->meImageGalleryImageType), liCollected);
    }

    // The currently highlighted image category (0..3): the category group's highlight
    // cursor, range-checked.
    EGuiImageCategories ImageGalleryState::GetCurrentCategory()
    {
        const s32 liCategory = mCategorySelectableGroup.GetHighlightedIndex();

        CGS_ASSERT(liCategory >= E_GUI_IMAGE_CATEGORIES_FIRST,
                   "mCategorySelectableGroup.GetHighlightedIndex() >= E_GUI_IMAGE_CATEGORIES_FIRST");
        CGS_ASSERT(liCategory < E_GUI_IMAGE_CATEGORIES_COUNT,
                   "mCategorySelectableGroup.GetHighlightedIndex() < E_GUI_IMAGE_CATEGORIES_COUNT");
        return static_cast<EGuiImageCategories>(liCategory);
    }

    // Redraw the carousel around the current image: the left / right arrows and images
    // (animated when the move came from that side), the middle image, the image number;
    // then request the images (all three slots for a fresh set, else the middle one) and
    // blank the gamertags and the who / where / when text until the answers arrive.
    void ImageGalleryState::RefreshCarousel(GameStateModuleIO::EImageGalleryRequest leRequest)
    {
        const EGuiImageCategories leCategory = GetCurrentCategory();

        if (miCurrentlySelectedCarouselItem > 0)
        {
            mCarouselLeftArrowAnimator.AddOutputAptViewState(
                "apt_Transition",
                (leRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_LEFT) ? "animate" : "visible",
                false);
            maCarouselItems[0].SetImageType(leCategory, false);
        }
        else
        {
            mCarouselLeftArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
            maCarouselItems[0].InvalidateImageType();
        }

        const s32 liCount = maiPhotoCountPerCategory[leCategory];
        maCarouselItems[1].SetImageType(leCategory, liCount <= 0);

        if (miCurrentlySelectedCarouselItem < liCount - 1)
        {
            mCarouselRightArrowAnimator.AddOutputAptViewState(
                "apt_Transition",
                (leRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_SCROLL_RIGHT) ? "animate" : "visible",
                false);
            maCarouselItems[2].SetImageType(leCategory, false);
        }
        else
        {
            mCarouselRightArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
            maCarouselItems[2].InvalidateImageType();
        }

        SetIntegerText(mCurrentImageText, miCurrentlySelectedCarouselItem + 1);

        const GameStateModuleIO::EImageGalleryType leType = GetGsmIOCategoryFromGuiEnum(GetCurrentCategory());
        if (leRequest == GameStateModuleIO::E_IMAGE_GALLERY_REQUEST_NEW_IMAGES)
        {
            PostImageGalleryRequest(mpStateInterface, miCurrentlySelectedCarouselItem - 1, 0, leRequest, leType);
            PostImageGalleryRequest(mpStateInterface, miCurrentlySelectedCarouselItem, 1, leRequest, leType);
            const s32 liNext = (miCurrentlySelectedCarouselItem < KI_MAX_IMAGES - 1)
                                   ? miCurrentlySelectedCarouselItem + 1
                                   : -1;
            PostImageGalleryRequest(mpStateInterface, liNext, 2, leRequest, leType);
        }
        else
        {
            PostImageGalleryRequest(mpStateInterface, miCurrentlySelectedCarouselItem, 1, leRequest, leType);
        }

        for (s32 liItem = 0; liItem < 3; ++liItem)
        {
            maCarouselItems[liItem].SetGamertag(KPC_EMPTY);
        }

        mWhoText.ClearText();
        mWhoText.OutputAptData();
        mWhereText.ClearText();
        mWhereText.OutputAptData();
        mWhenText.ClearText();
        mWhenText.OutputAptData();

        mbSelectedImageValid = false;
    }

    // Map an image-gallery GUI category (takedown/mugshot/rulebreaker/finish) onto its
    // GameStateModuleIO EImageGalleryType. The console's default text is streamed
    // ("Invalid image category in Image Gallery : " << category); collapsed to the static
    // message.
    GameStateModuleIO::EImageGalleryType
    ImageGalleryState::GetGsmIOCategoryFromGuiEnum(EGuiImageCategories leImageCategory)
    {
        switch (leImageCategory)
        {
            case E_GUI_IMAGE_CATEGORIES_TAKEDOWNS:    return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_FREEBURN_MUGSHOT;
            case E_CUI_IMAGE_CATEGORIES_MUGSHOTS:     return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_MUGSHOT;
            case E_GUI_IMAGE_CATEGORIES_RULEBREAKER:  return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_ROAD_RULE_MUGSHOT;
            case E_GUI_IMAGE_CATEGORIES_PHOTO_FINISH: return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_VICTORY_MUGSHOT;
            default:
                CGS_ASSERT(false, "Invalid image category in Image Gallery : ");
                return GameStateModuleIO::E_IMAGE_GALLERY_TYPE_FREEBURN_MUGSHOT;
        }
    }

    // The reverse map (inlined at both of its console call sites, UpdateRunning's
    // collected-count arm and HandleCollectedDataEvent): road-rule, victory and mugshot
    // galleries map to their tabs, everything else to the takedown tab.
    EGuiImageCategories
    ImageGalleryState::GetGuiCategoryFromGsmIOEnum(GameStateModuleIO::EImageGalleryType leImageGalleryType)
    {
        switch (leImageGalleryType)
        {
            case GameStateModuleIO::E_IMAGE_GALLERY_TYPE_ROAD_RULE_MUGSHOT: return E_GUI_IMAGE_CATEGORIES_RULEBREAKER;
            case GameStateModuleIO::E_IMAGE_GALLERY_TYPE_VICTORY_MUGSHOT:   return E_GUI_IMAGE_CATEGORIES_PHOTO_FINISH;
            case GameStateModuleIO::E_IMAGE_GALLERY_TYPE_MUGSHOT:           return E_CUI_IMAGE_CATEGORIES_MUGSHOTS;
            default:                                                        return E_GUI_IMAGE_CATEGORIES_TAKEDOWNS;
        }
    }

    // Store a category's image count and print it on its tab; for the current tab also
    // show / hide the image info panel, print the total and refresh the buttons.
    void ImageGalleryState::SetupCountForCategory(EGuiImageCategories leCategory, s32 liCount)
    {
        maiPhotoCountPerCategory[leCategory] = liCount;
        maCategorySelectable[leCategory].SetCollected(liCount);

        if (GetCurrentCategory() != leCategory)
        {
            return;
        }

        mImageInfoAnimator.AddOutputAptViewState("apt_Transition",
                                                 (maiPhotoCountPerCategory[leCategory] > 0) ? "info" : "invisible",
                                                 false);
        SetIntegerText(mTotalImageText, maiPhotoCountPerCategory[GetCurrentCategory()]);
        SetupButtons();
    }

    // The help-bar buttons: none without images, else the lock or unlock set.
    void ImageGalleryState::SetupButtons()
    {
        const char* lpacState;
        if (maiPhotoCountPerCategory[GetCurrentCategory()] > 0)
        {
            lpacState = mbIsCurrentLocked ? "imageX360Unlock" : "imageX360Lock";
        }
        else
        {
            lpacState = "noImage";
        }
        mButtonAnimator.AddOutputAptViewState("apt_Transition", lpacState, false);
    }

    // The menu cue for a navigation action: the toggle cue for the vertical moves, the
    // item-toggle cue for the horizontal ones.
    void ImageGalleryState::TriggerSound(EGameInputActions leAction)
    {
        const char* lpcLabel = 0;
        switch (leAction)
        {
            case E_GAMEINPUTACTIONS_GUI_DPAD_UP:
            case E_GAMEINPUTACTIONS_GUI_DPAD_DOWN:
            case E_GAMEINPUTACTIONS_GUI_UP:
            case E_GAMEINPUTACTIONS_GUI_DOWN:
                lpcLabel = "MenuToggleDefault";
                break;

            case E_GAMEINPUTACTIONS_GUI_DPAD_LEFT:
            case E_GAMEINPUTACTIONS_GUI_DPAD_RIGHT:
            case E_GAMEINPUTACTIONS_GUI_LEFT:
            case E_GAMEINPUTACTIONS_GUI_RIGHT:
                lpcLabel = "MenuItemToggleDefault";
                break;

            default:
                CGS_ASSERT(false, "lpcLabel");
                break;
        }

        // The wire record is { 100, 457, 12 } + the 100-byte payload.
        GuiAudioTriggerEvent lAudio;
        lAudio.Construct(KI_AUDIO_ACTION_MENU_CUE, KPC_EMPTY, lpcLabel, KPC_EMPTY);
        lAudio.muHeader0   = static_cast<u32>(sizeof(GuiAudioTriggerEvent) - sizeof(CgsGui::GuiEvent<201>));
        lAudio.muEventType = static_cast<u32>(KI_GUI_EVENT_AUDIO_TRIGGER);
        lAudio.muHeader2   = 12u;
        PostRecord(mpStateInterface, lAudio);
    }

    // Drop the middle carousel item to the current category's invisible frame.
    void ImageGalleryState::HideMiddleImage()
    {
        maCarouselItems[1].SetImageType(GetCurrentCategory(), true);
    }

    // Hide the right carousel item and its arrow.
    void ImageGalleryState::HideRightImage()
    {
        maCarouselItems[2].AddOutputAptViewState("apt_state", "invisible", false);
        mCarouselRightArrowAnimator.AddOutputAptViewState("apt_Transition", "invisible", false);
    }

    // Counts and widths that do not depend on the host pointer size. The console offsets in
    // the header do not survive the 64-bit host, so nothing here pins a byte offset.
    void ImageGalleryState::_AssertLayout()
    {
        static_assert(sizeof(((ImageGalleryState*)0)->maCategorySelectable) / sizeof(ImageGallerySelectable)
                          == E_GUI_IMAGE_CATEGORIES_COUNT,
                      "one category tab per image category");
        static_assert(sizeof(((ImageGalleryState*)0)->maiPhotoCountPerCategory) / sizeof(s32)
                          == E_GUI_IMAGE_CATEGORIES_COUNT,
                      "one image count per image category");
        static_assert(sizeof(((ImageGalleryState*)0)->maCarouselOverviewSelectable)
                              / sizeof(ImageGalleryCarouselSelectable) == 20,
                      "the console's overview strip holds 20 slots");
        static_assert(sizeof(((ImageGalleryState*)0)->maCarouselItems) / sizeof(ImageGalleryCarouselItem) == 3,
                      "left / middle / right carousel items");
        static_assert(sizeof(((ImageGalleryState*)0)->mauExpectedComponentIds) / sizeof(u32)
                          == KU_MAX_INIT_COMPONENTS_NUM,
                      "seven expected-component slots");
        static_assert(sizeof(((ImageGalleryState*)0)->miSnapShotDelayCounter) == 1,
                      "the snapshot delay is a signed byte (lbz/extsb)");
        static_assert(sizeof(((ImageGalleryState*)0)->mSelectedPlayerName) == 16,
                      "the selected player name is the 16-byte copy UpdateRunning exports");
        static_assert(sizeof(CollectedDataPayload) == 16, "collected-data payload: type + one bit field");
    }
}   // namespace BrnGui
