// GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptionsSummary.cpp
//
// BrnGui::OnlineGameOptionsSummary -- the online game-options summary screen (ON_CRSUM). The
// seventeen bodies, read off the console asm:
//
//   OnEnter / OnLeave / Update          the lifecycle
//   CheckForCompletedLoads              ON_CRSUM + the route-info package, then the components
//   HandleGuiCacheEvent                 adopt the cache, register the components
//   HandleControllerInput(+CreateGame/+SaveOptions)
//                                       the summary page (rounds, done, save) and the save page
//                                       (slots, overwrite question)
//   ShowGameOptionsScreen / ShowSaveScreen / SetupSaveMenuText / SetupHelpBar
//   FinishedCreatingGame                create the game (or set the free-burn lobby's params)
//                                       behind the "entering game" overlay
//   HandleInGameEvent / HandleInGameFailedEvent / HandleOverlayComplete
//
// Out-queue records are the console's own wire records, posted on channel 40 through
// GetOutputEventQueue()->AddEvent with host sizeof sizes.

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineGameOptionsSummary.h"

#include <cstring>                                                        // std::memcpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N> / GuiEventWrapper
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"           // CgsLanguage::LanguageManager
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                     // GuiEventShowHideSatNav / GuiChallengeSelectedEvent
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiEventActivateCrashNav / overlay requests / GuiAudioTriggerEvent
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"                // FreeburnChallengeManager
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile (the saved routes)
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h"           // GuiEventNetworkCreateGame
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // GuiEventNetworkGameParams
#include "GameSource/Input/GameInputActions.h"                            // EGameInputActions
#include "SharedClasses/DataLists/ChallengeListEntry.h"                   // BrnResource::ChallengeListEntry

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: 14, apt ONLOAD, controller press, gui cache,
    // disconnected, in-game, in-game failed, the satnav record and overlay complete.
    const s32 OnlineGameOptionsSummary::maiEventToObserve[9] = { 14, 21, 6, 64, 44, 50, 51, 213, 189 };
    const s32 OnlineGameOptionsSummary::miNumEventsObserved  = 9;

    // ON_CRSUM and the route-info package.
    const CgsGui::sResourceTuple OnlineGameOptionsSummary::maResourceTuplesToLoad[] =
    {
        { 177, CgsGui::E_GUI_RESOURCETYPE_APT },
        { 191, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const s32 OnlineGameOptionsSummary::miNumResourcesToLoad = 2;

    const char OnlineGameOptionsSummary::KAC_TITLE_TEXT_COMPONENT[11]   = "Title_text";
    const char OnlineGameOptionsSummary::KAC_MENU_OPTIONS_COMPONENT[9]  = "MenuItem";
    const char OnlineGameOptionsSummary::KAC_ROUTE_INFO_NAME[10]        = "RouteInfo";
    const char OnlineGameOptionsSummary::KAC_UP_ARROW_COMPONENT[13]     = "ArrowUp_anim";
    const char OnlineGameOptionsSummary::KAC_DOWN_ARROW_COMPONENT[15]   = "ArrowDown_anim";
    const char OnlineGameOptionsSummary::KAC_HELP_BAR_COMPONENT[7]      = "Button";

    const char* const OnlineGameOptionsSummary::KPC_ARROW_ANIMATION_STATES[3] =
    {
        "invisible", "visible", "animate"
    };

    const char OnlineGameOptionsSummary::KAC_SUMMARY_TITLE_STRING_ID[34]      = "$PAGE_HEADING_GAME_OPTION_SUMMARY";
    const char OnlineGameOptionsSummary::KAC_SAVE_OPTIONS_TITLE_STRING_ID[32] = "$PAGE_HEADING_SAVE_GAME_OPTIONS";

    const char* const OnlineGameOptionsSummary::KPC_MAIN_MENU_OPTION_STRING_IDS[KI_NUM_MAIN_MENU_OPTIONS] =
    {
        "$ONLINE_GAME_OPTION_DONE", "$ONLINE_GAME_OPTION_SAVE_DONE"
    };

    const char OnlineGameOptionsSummary::KPC_EMPTY_SLOT_STRING_IDS[30] = "ONLINE_GAME_OPTION_EMPTY_SLOT";
    const char OnlineGameOptionsSummary::KPC_SLOT_STRING_FORMAT_ID[24] = "ONLINE_GAME_OPTION_SLOT";
    const char OnlineGameOptionsSummary::KPC_SLOT_STRING_ID[28]        = "$ONLINE_GAME_OPTION_SLOT_%d";

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const s32 KI_EVENT_CONTROLLER_INPUT     = 6;
        const s32 KI_EVENT_OBSERVED_NO_ARM_14   = 14;
        const s32 KI_EVENT_APT_ONLOAD           = 21;    // observed, no arm
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_IN_GAME              = 50;
        const s32 KI_EVENT_IN_GAME_FAILED       = 51;
        const s32 KI_EVENT_GUI_CACHE            = 64;
        const s32 KI_EVENT_OVERLAY_COMPLETE     = 189;
        const s32 KI_EVENT_SHOW_HIDE_SATNAV     = 213;   // observed, no arm

        // The movie CheckForCompletedLoads plays: the screen's own package.
        const u32 KU_SUMMARY_MOVIE_RESOURCE = 177;
        const s32 KI_APT_MOVIE_LEVEL        = 3;
        const char* const KPC_EMPTY_STRING  = "";

        // The route info here shows the round's map.
        const s32 KI_ROUTE_INFO_MAP_TYPE = 0;

        const u64 KU64_NO_APT_ID = 0xFFFFFFFFull;   // the menu's "no apt id" (32-bit -1, zero-extended)

        // The arrow states (KPC_ARROW_ANIMATION_STATES).
        const s32 KI_ARROW_INVISIBLE = 0;
        const s32 KI_ARROW_VISIBLE   = 1;
        const s32 KI_ARROW_ANIMATE   = 2;

        // The save page has one slot more than the saved routes, up to the table size.
        const s32 KI_MAX_SAVED_ROUTES = OptionsDataProfile::KI_MAX_CREATED_ONLINE_GAME_OPTIONS;

        // The menu-text buffers.
        const u32 KU_SLOT_TEXT_LENGTH = 128;

        // The "save over this slot?" overlay, and the "entering game" overlay.
        const char KAC_OVERWRITE_QUESTION_OVERLAY_ID[] = "CNOnlOvGOQn";
        const char KAC_ENTER_GAME_OVERLAY_ID[]         = "CNOnlEntGame";

        // The menu click cues.
        const s32  KI_AUDIO_ACTION_MENU_CUE   = 7;
        const char KAC_AUDIO_MOVE_LABEL[]     = "MenuToggleDefault";
        const char KAC_AUDIO_ACCEPT_LABEL[]   = "Accept";

        // The challenge-selected record's action word when a game is created mid-challenge.
        const s32 KI_CHALLENGE_SELECTOR_ACTION_CREATE = 1;

        const char KAC_APT_TRANSITION_NAME[] = "apt_Transition";
        const char KAC_ADVANCE_EVENT[]       = "ADVANCE";
        const char KAC_DISCONNECT_EVENT[]    = "DISCONNECT";
        const char KAC_GO_BACK_EVENT[]       = "GO_BACK";

        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct ControllerButtonPayload : public CgsModule::Event
        {
            s32 miPadId;      // +0x00
            s32 miButtonId;   // +0x04 (the input action id)
        };

        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpCache;   // +0x00
        };

        struct OverlayCompletePayload : public CgsModule::Event
        {
            CgsID mOverlayId;                                   // +0x00
            GuiOverlayCompleteEvent::LeaveMethod meLeaveMethod; // +0x08
        };

        // ---- out-queue wire records ------------------------------------------------------
        // { 1, N, 12, <one byte> }, channel 40, 16 bytes: the one-byte command records. The
        // console never writes the payload byte.
        template <s32 TI_EVENT_ID>
        struct GuiCommandWire16 : public CgsGui::GuiEvent<TI_EVENT_ID>
        {
            u8 mu8Payload;   // +0x0C

            GuiCommandWire16()
                : CgsGui::GuiEvent<TI_EVENT_ID>(
                      static_cast<u32>(sizeof(u8)),
                      static_cast<u32>(sizeof(CgsGui::GuiEvent<TI_EVENT_ID>)))
                , mu8Payload(0)
            {
            }
        };

        // { 1, 148, 12, <show byte> }, channel 40, 16 bytes: show / hide the HUD.
        struct GuiEventShowHideHudWire : public CgsGui::GuiEvent<148>
        {
            bool mbShowHud;   // +0x0C

            explicit GuiEventShowHideHudWire(bool lbShowHud)
                : CgsGui::GuiEvent<148>(1, 12)
                , mbShowHud(lbShowHud)
            {
            }
        };

        // { 1, 356, 12, <the byte> }, channel 40, 16 bytes: the autosave request.
        struct GuiAutosaveRequestWire : public CgsGui::GuiEvent<356>
        {
            u8 mu8Payload;   // +0x0C

            GuiAutosaveRequestWire() : CgsGui::GuiEvent<356>(1, 12), mu8Payload(0) {}
        };

        // { 100, 457, 12, the audio trigger }, channel 40, 112 bytes.
        struct GuiAudioTriggerWire
        {
            s32 miSize;
            s32 miType;
            s32 miOffset;
            GuiAudioTriggerWirePayload457 mPayload;
        };

        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes.
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

        // { 8, 188, 16, <pad>, the compressed overlay id }, channel 40, 24 bytes.
        struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
        {
            GuiOverlayWaitFinishRequest mRequest;   // +0x10

            explicit GuiOverlayWaitFinishWire(const char* lpcOverlayName)
                : CgsGui::GuiEvent<188>(static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)), 16)
            {
                mRequest.Construct(lpcOverlayName);
            }
        };

        typedef CgsGui::GuiEventWrapper<GuiEventNetworkCreateGame, 40> GuiEventNetworkCreateGameWire;
        typedef CgsGui::GuiEventWrapper<GuiEventNetworkGameParams, 40> GuiEventNetworkGameParamsWire;
        typedef CgsGui::GuiEventWrapper<GuiChallengeSelectedEvent, 40> GuiChallengeSelectedWire;

        static_assert(sizeof(GuiCommandWire16<175>) == 16, "command record is 16 bytes");
        static_assert(sizeof(GuiEventShowHideHudWire) == 16, "show/hide-hud record is 16 bytes");
        static_assert(sizeof(GuiAutosaveRequestWire) == 16, "autosave record is 16 bytes");
        static_assert(sizeof(GuiAudioTriggerWire) == 112, "audio trigger record is 112 bytes");
        static_assert(sizeof(GuiOverlayRequestWire) == 304, "overlay request record is 304 bytes");
        static_assert(sizeof(GuiOverlayWaitFinishWire) == 24, "wait-finish record is 24 bytes");
        static_assert(sizeof(GuiEventNetworkCreateGameWire) == 492, "create-game record is 492 bytes");
        static_assert(sizeof(GuiEventNetworkGameParamsWire) == 492, "game-params record is 492 bytes");
        static_assert(sizeof(GuiChallengeSelectedWire) == 32, "challenge-selected record is 32 bytes");
        static_assert(sizeof(GuiEventActivateCrashNav) == 20, "activate-crashnav record is 20 bytes");

        template <typename TWire>
        void PostWire(CgsGui::StateInterface* lpStateInterface, const TWire& lrWire)
        {
            lpStateInterface->GetOutputEventQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lrWire), KI_CHANNEL_GUI_OUT,
                static_cast<s32>(sizeof(lrWire)));
        }

        // The menu click: the audio trigger record with the cue's label.
        void PostMenuAudio(CgsGui::StateInterface* lpStateInterface, const char* lpacLabel)
        {
            GuiAudioTriggerEvent lAudio;
            lAudio.Construct(KI_AUDIO_ACTION_MENU_CUE, KPC_EMPTY_STRING, lpacLabel, KPC_EMPTY_STRING);
            GuiAudioTriggerWire lAudioWire = { 100, 457, 12, {} };
            std::memcpy(&lAudioWire.mPayload, lAudio.macComponent, sizeof(lAudioWire.mPayload));
            PostWire(lpStateInterface, lAudioWire);
        }

        // Show / hide the main map on both the view and the internal channels.
        void ShowHideMainMap(CgsGui::StateInterface* lpStateInterface, bool lbShow)
        {
            GuiEventShowHideSatNav lSatNav;
            lSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, lbShow, 0.0f);
            lpStateInterface->OutputViewState(lSatNav);
            lpStateInterface->OutputInternalState(lSatNav);
        }
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlineGameOptionsSummary::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        mMenuOptions.Construct(KAC_MENU_OPTIONS_COMPONENT, mpStateInterface, KI_MAX_CREATE_GAME_OPTIONS,
                               0, KU64_NO_APT_ID);
        mRouteInfoDisplay.Construct(KAC_ROUTE_INFO_NAME, KI_ROUTE_INFO_MAP_TYPE, mpStateInterface, 0);
        mUpArrowAnimator.Construct(KAC_UP_ARROW_COMPONENT, mpStateInterface, 0);
        mDownArrowAnimator.Construct(KAC_DOWN_ARROW_COMPONENT, mpStateInterface, 0);
        mTitleText.Construct(KAC_TITLE_TEXT_COMPONENT, mpStateInterface, 0);
        mHelpBar.Construct(KAC_HELP_BAR_COMPONENT, KI_MAX_HELP_BAR_ITEMS, mpStateInterface, 0);

        meSubState     = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache     = 0;
        miCurrentRound = 0;

        // The summary owns the screen: CrashNav down, HUD hidden.
        PostWire(mpStateInterface, GuiEventActivateCrashNav(false));
        PostWire(mpStateInterface, GuiEventShowHideHudWire(false));

        miStartSaveItem = 0;
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineGameOptionsSummary::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        mMenuOptions.Clear();
        mRouteInfoDisplay.Destruct();
        ShowHideMainMap(mpStateInterface, false);
        mHelpBar.Destruct();
    }

    // ================================================================================
    //  Update
    // ================================================================================
    void OnlineGameOptionsSummary::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);

        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_INPUT:
                HandleControllerInput(lpEvent);
                break;

            case KI_EVENT_OBSERVED_NO_ARM_14:
            case KI_EVENT_APT_ONLOAD:
            case KI_EVENT_SHOW_HIDE_SATNAV:
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                mpGuiCache->SetDoDisconnectPopup(lpEvent);
                SendStateEvent(KAC_DISCONNECT_EVENT);
                break;

            case KI_EVENT_IN_GAME:
                HandleInGameEvent(lpEvent);
                break;

            case KI_EVENT_IN_GAME_FAILED:
                HandleInGameFailedEvent(lpEvent);
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;

            case KI_EVENT_OVERLAY_COMPLETE:
                HandleOverlayComplete(lpEvent);
                break;

            default:
                // The console's streamed "Unexpected event received : <id> in <file> at
                // line 215" assert.
                CGS_ASSERT(false, "Unexpected event received : ");
                break;
            }
        }

        mRouteInfoDisplay.Update(mpInGuiEventQueue);

        lpInQueue->Clear();

        CheckForCompletedLoads();

        mMenuOptions.Update();

        if (meSubState > E_SUBSTATE_LOADING_COMPONENTS)
        {
            mHelpBar.Update(mpGuiCache->GetTime());
        }
    }

    // ================================================================================
    //  CheckForCompletedLoads -- the packages, then the components; then the summary page.
    // ================================================================================
    void OnlineGameOptionsSummary::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_SUMMARY_MOVIE_RESOURCE],
                                               KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
        }
        else if (meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            // The console re-tests the cache after the assert (the assert does not stop).
            if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
            {
                mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
                mRouteInfoDisplay.miNumComponentsLoaded = GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD;
                ShowGameOptionsScreen();
                mHelpBar.SetupComponent();
            }
        }
    }

    // ================================================================================
    //  HandleGuiCacheEvent -- adopt the first cache offered and register the components.
    // ================================================================================
    void OnlineGameOptionsSummary::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpPayload = static_cast<const GuiCachePayload*>(lpEvent);

        CGS_ASSERT(lpPayload->mpCache != 0, "Invalid cache in HandleGuiCacheEvent::Update");

        if (mpGuiCache == 0)
        {
            mpGuiCache = lpPayload->mpCache;
            mMenuOptions.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mRouteInfoDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mHelpBar.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mUpArrowAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mDownArrowAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleText.GetName());
        }
    }

    // ================================================================================
    //  SetupHelpBar -- select (or save, on the save page) and back.
    // ================================================================================
    void OnlineGameOptionsSummary::SetupHelpBar(bool /*lbUnused*/)
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        const ButtonIconComponent::EPadButton laeButtons[KI_MAX_HELP_BAR_ITEMS] =
        {
            ButtonIconComponent::E_PADBUTTON_SELECT, ButtonIconComponent::E_PADBUTTON_BACK
        };
        const char* lapacText[KI_MAX_HELP_BAR_ITEMS] =
        {
            (meSubState != E_SUBSTATE_SELECTING_PARAMS && meSubState == E_SUBSTATE_SAVE_OPTIONS)
                ? "$CAPS_BUTTON_SAVE" : "$CAPS_BUTTON_SELECT",
            "$CAPS_BUTTON_BACK_UP"
        };

        mHelpBar.Clear();
        for (s32 liItem = 0; liItem < KI_MAX_HELP_BAR_ITEMS; ++liItem)
        {
            mHelpBar.AppendHelpBarItem(lapacText[liItem], laeButtons[liItem],
                                       ButtonIconComponent::E_PADBUTTON_INVISIBLE);
        }
    }

    // ================================================================================
    //  ShowGameOptionsScreen -- the summary page: done / save, round 0 of the options.
    // ================================================================================
    void OnlineGameOptionsSummary::ShowGameOptionsScreen()
    {
        meSubState = E_SUBSTATE_SELECTING_PARAMS;

        mMenuOptions.SetupMenu(KI_NUM_MAIN_MENU_OPTIONS, true);
        for (s32 liOption = 0; liOption < KI_NUM_MAIN_MENU_OPTIONS; ++liOption)
        {
            mMenuOptions.SetText(liOption, KPC_MAIN_MENU_OPTION_STRING_IDS[liOption]);
        }

        SetupHelpBar(false);

        miCurrentRound = 0;
        mTitleText.SetText(KAC_SUMMARY_TITLE_STRING_ID);

        mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);
        mRouteInfoDisplay.SetInfo(miCurrentRound,
                                  reinterpret_cast<const GuiEventNetworkGameParams*>(
                                      mpGuiCache->maOnlineGameModeOptionsStorage));

        ShowHideMainMap(mpStateInterface, true);

        mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                 KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
        mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                               KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
    }

    // ================================================================================
    //  ShowSaveScreen -- the save page: the saved slots plus the next empty one, the
    //  highlight on the first free slot (or on slot 0 with its route when the table is full).
    // ================================================================================
    void OnlineGameOptionsSummary::ShowSaveScreen()
    {
        meSubState = E_SUBSTATE_SAVE_OPTIONS;

        mMenuOptions.SetupMenu(KI_MAX_CREATE_GAME_OPTIONS, false);
        miCurrentRound  = 0;
        miStartSaveItem = 0;
        SetupSaveMenuText();

        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();
        const s32 liNumSaved = lpProfile->GetNumCreatedOnlineGameOptions();

        if (liNumSaved < KI_MAX_SAVED_ROUTES)
        {
            if (!(liNumSaved < KI_MAX_CREATE_GAME_OPTIONS))
            {
                miStartSaveItem = liNumSaved - (KI_MAX_CREATE_GAME_OPTIONS - 1);
                SetupSaveMenuText();
            }

            mMenuOptions.HighlightIndex(lpProfile->GetNumCreatedOnlineGameOptions() - miStartSaveItem);
            mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_INVISIBLE);

            const s32 liArrowState = (lpProfile->GetNumCreatedOnlineGameOptions() == 0)
                                         ? KI_ARROW_INVISIBLE : KI_ARROW_VISIBLE;
            mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KPC_ARROW_ANIMATION_STATES[liArrowState], false);
            mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                   KPC_ARROW_ANIMATION_STATES[liArrowState], false);
        }
        else
        {
            mMenuOptions.HighlightIndex(0);
            mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);

            GuiEventNetworkGameParams lSavedParams;
            lpProfile->GetCreatedOnlineGameOptions(0, mpGuiCache, &lSavedParams);
            mRouteInfoDisplay.SetInfo(miCurrentRound, &lSavedParams);

            mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                     KPC_ARROW_ANIMATION_STATES[KI_ARROW_VISIBLE], false);
            mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                   KPC_ARROW_ANIMATION_STATES[KI_ARROW_VISIBLE], false);
        }

        SetupHelpBar(false);
        mTitleText.SetText(KAC_SAVE_OPTIONS_TITLE_STRING_ID);
        ShowHideMainMap(mpStateInterface, true);
    }

    // ================================================================================
    //  SetupSaveMenuText -- name the seven rows from miStartSaveItem: "slot N" for a saved
    //  route, "empty slot" for the next one, blank past it; a row is selectable up to the
    //  empty slot (all ten when the table is full).
    // ================================================================================
    void OnlineGameOptionsSummary::SetupSaveMenuText()
    {
        const s32 liNumSaved = mpGuiCache->GetOptionsDataProfile()->GetNumCreatedOnlineGameOptions();

        for (s32 liRow = 0; liRow < KI_MAX_CREATE_GAME_OPTIONS; ++liRow)
        {
            const s32 liSlot = miStartSaveItem + liRow;

            // The row's text id is "$ONLINE_GAME_OPTION_SLOT_<row>"; its localised text is
            // added under the same id without the '$'.
            char lacRowText[KU_SLOT_TEXT_LENGTH];
            CgsCore::SPrintf(lacRowText, KU_SLOT_TEXT_LENGTH, KPC_SLOT_STRING_ID, liRow);

            const s32 liNumSavedNow = mpGuiCache->GetOptionsDataProfile()->GetNumCreatedOnlineGameOptions();
            if (liSlot < liNumSavedNow)
            {
                char lacSlotNumber[KU_SLOT_TEXT_LENGTH];
                CgsCore::SPrintf(lacSlotNumber, KU_SLOT_TEXT_LENGTH, "%d", liSlot + 1);
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacRowText + 1, KPC_SLOT_STRING_FORMAT_ID,
                    CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 1,
                    lacSlotNumber, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
            }
            else if (liSlot == liNumSavedNow)
            {
                mpStateInterface->GetLanguageManager()->FormatAndAddText(lacRowText + 1,
                                                                         KPC_EMPTY_SLOT_STRING_IDS);
            }
            else
            {
                mpStateInterface->GetLanguageManager()->FormatAndAddText(
                    lacRowText + 1, " ", CgsLanguage::LanguageManager::E_FORMAT_TEXT);
            }

            mMenuOptions.SetText(liRow, lacRowText);

            if ((liNumSaved < KI_MAX_SAVED_ROUTES && liSlot <= liNumSaved)
                || (liNumSaved == KI_MAX_SAVED_ROUTES && liSlot < KI_MAX_SAVED_ROUTES))
            {
                mMenuOptions.Enable(liRow);
            }
            else
            {
                mMenuOptions.Disable(liRow);
            }
        }
    }

    // ================================================================================
    //  FinishedCreatingGame -- create the game from the options (a free-burn lobby or
    //  showtime only sends its new params), behind the "entering game" overlay; hand the
    //  front end back; and re-announce a running free-burn challenge.
    // ================================================================================
    void OnlineGameOptionsSummary::FinishedCreatingGame()
    {
        const s32 leGameMode = mpGuiCache->GetCurrentGameModeType();
        const bool lbInLobby = leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY
                            || leGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;

        const GuiEventNetworkGameParams* lpParams =
            reinterpret_cast<const GuiEventNetworkGameParams*>(mpGuiCache->maOnlineGameModeOptionsStorage);

        if (!lbInLobby)
        {
            GuiEventNetworkCreateGame lCreateGame;
            lCreateGame.SetFromGameParams(lpParams);
            PostWire(mpStateInterface, GuiEventNetworkCreateGameWire(lCreateGame));

            GuiOverlayRequestWire lEnterGameOverlay;
            lEnterGameOverlay.mRequest.Construct(KAC_ENTER_GAME_OVERLAY_ID);
            PostWire(mpStateInterface, lEnterGameOverlay);
        }
        else
        {
            GuiEventNetworkGameParams lGameParams = *lpParams;
            PostWire(mpStateInterface, GuiEventNetworkGameParamsWire(lGameParams));
        }

        meSubState = E_SUBSTATE_WAIT_IN_GAME;

        // Clear apt level 3, hide the main map, raise the CrashNav flow again and hand the
        // front end back.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);
        ShowHideMainMap(mpStateInterface, false);
        PostWire(mpStateInterface, GuiEventActivateCrashNav(true));
        PostWire(mpStateInterface, GuiCommandWire16<533>());

        const FreeburnChallengeManager* lpChallengeManager = mpGuiCache->GetFreeburnChallengeManager();
        if (lpChallengeManager->IsRunning())
        {
            const BrnResource::ChallengeListEntry* lpChallenge =
                mpGuiCache->GetFreeburnChallengeManager()->GetCurrentChallenge();

            GuiChallengeSelectedEvent lChallengeSelected;
            lChallengeSelected.miSelectorAction = KI_CHALLENGE_SELECTOR_ACTION_CREATE;
            lChallengeSelected.mChallengeID     = lpChallenge->GetChallengeID();
            lChallengeSelected.miChall          = static_cast<s32>(lpChallenge->GetChallengeStyle());
            PostWire(mpStateInterface, GuiChallengeSelectedWire(lChallengeSelected));
        }
    }

    // ================================================================================
    //  HandleInGameEvent -- GUI 50: the game exists; drop the overlay and advance.
    // ================================================================================
    void OnlineGameOptionsSummary::HandleInGameEvent(const CgsModule::Event* lpInGameEvent)
    {
        // The console's text names OnlineSelectRoute (the same pooled string).
        CGS_ASSERT(lpInGameEvent != 0, "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");
        CGS_ASSERT(lpInGameEvent != 0, "lpInGameEvent");

        PostWire(mpStateInterface, GuiOverlayWaitFinishWire(KAC_ENTER_GAME_OVERLAY_ID));
        SendStateEvent(KAC_ADVANCE_EVENT);
        PostWire(mpStateInterface, GuiCommandWire16<175>());
    }

    // ================================================================================
    //  HandleInGameFailedEvent -- GUI 51: start the screen over.
    // ================================================================================
    void OnlineGameOptionsSummary::HandleInGameFailedEvent(const CgsModule::Event* lpEvent)
    {
        // The console's text names OnlineSelectRoute::HandleInGameEvent (the same pooled string).
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");

        meSubState     = E_SUBSTATE_LOADING_SCREEN;
        miCurrentRound = 0;

        mMenuOptions.Clear();
        mRouteInfoDisplay.Destruct();
        mHelpBar.Destruct();

        mMenuOptions.Construct(KAC_MENU_OPTIONS_COMPONENT, mpStateInterface, KI_MAX_CREATE_GAME_OPTIONS,
                               0, KU64_NO_APT_ID);
        mRouteInfoDisplay.Construct(KAC_ROUTE_INFO_NAME, KI_ROUTE_INFO_MAP_TYPE, mpStateInterface, 0);
        mUpArrowAnimator.Construct(KAC_UP_ARROW_COMPONENT, mpStateInterface, 0);
        mDownArrowAnimator.Construct(KAC_DOWN_ARROW_COMPONENT, mpStateInterface, 0);
        mTitleText.Construct(KAC_TITLE_TEXT_COMPONENT, mpStateInterface, 0);
        mHelpBar.Construct(KAC_HELP_BAR_COMPONENT, KI_MAX_HELP_BAR_ITEMS, mpStateInterface, 0);

        mMenuOptions.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
        mRouteInfoDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
        mHelpBar.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mUpArrowAnimator.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mDownArrowAnimator.GetName());
        mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleText.GetName());

        PostWire(mpStateInterface, GuiEventActivateCrashNav(false));
        PostWire(mpStateInterface, GuiEventShowHideHudWire(false));

        miStartSaveItem = 0;
    }

    // ================================================================================
    //  HandleOverlayComplete -- "yes" to the overwrite question saves over the highlighted
    //  row's slot and returns to the summary page.
    // ================================================================================
    void OnlineGameOptionsSummary::HandleOverlayComplete(const CgsModule::Event* lpOverlayCompleteEvent)
    {
        CGS_ASSERT(lpOverlayCompleteEvent != 0, "lpOverlayCompleteEvent");

        const OverlayCompletePayload* lpOverlayComplete =
            static_cast<const OverlayCompletePayload*>(lpOverlayCompleteEvent);
        if (lpOverlayComplete->mOverlayId == CgsIDCompress(KAC_OVERWRITE_QUESTION_OVERLAY_ID)
            && lpOverlayComplete->meLeaveMethod == GuiOverlayCompleteEvent::E_LEAVEMETHOD_OK
            && meSubState == E_SUBSTATE_SAVE_OPTIONS)
        {
            mpGuiCache->GetOptionsDataProfile()->SetCreatedOnlineGameOptions(
                mMenuOptions.GetHighlightedIndex(), mpGuiCache,
                reinterpret_cast<const GuiEventNetworkGameParams*>(mpGuiCache->maOnlineGameModeOptionsStorage));
            PostWire(mpStateInterface, GuiAutosaveRequestWire());
            ShowGameOptionsScreen();
        }
    }

    // ================================================================================
    //  HandleControllerInput -- the summary page or the save page.
    // ================================================================================
    void OnlineGameOptionsSummary::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineGameOptionsSummary::HandleControllerInput");

        if (meSubState == E_SUBSTATE_SELECTING_PARAMS)
        {
            HandleControllerInputCreateGame(lpEvent);
        }
        else if (meSubState == E_SUBSTATE_SAVE_OPTIONS)
        {
            HandleControllerInputSaveOptions(lpEvent);
        }
    }

    // ================================================================================
    //  HandleControllerInputCreateGame -- up / down between done and save, left / right
    //  through the rounds (two at a time in road rage), select, back.
    // ================================================================================
    void OnlineGameOptionsSummary::HandleControllerInputCreateGame(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameOptionsSummary::HandleControllerInputCreateGame");

        const GuiEventNetworkGameParams* lpParams =
            reinterpret_cast<const GuiEventNetworkGameParams*>(mpGuiCache->maOnlineGameModeOptionsStorage);

        const ControllerButtonPayload* lpInput = static_cast<const ControllerButtonPayload*>(lpEvent);
        switch (lpInput->miButtonId)
        {
        case E_GAMEINPUTACTIONS_GUI_UP:
            if (mMenuOptions.HighlightPrevious())
            {
                PostMenuAudio(mpStateInterface, KAC_AUDIO_MOVE_LABEL);
            }
            break;

        case E_GAMEINPUTACTIONS_GUI_DOWN:
            if (mMenuOptions.HighlightNext())
            {
                PostMenuAudio(mpStateInterface, KAC_AUDIO_MOVE_LABEL);
            }
            break;

        case E_GAMEINPUTACTIONS_GUI_LEFT:
            if (miCurrentRound > 0)
            {
                if (lpParams->meGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
                {
                    miCurrentRound = miCurrentRound - 2;
                }
                else
                {
                    miCurrentRound = miCurrentRound - 1;
                }
                mRouteInfoDisplay.SetInfo(miCurrentRound, lpParams);
            }
            break;

        case E_GAMEINPUTACTIONS_GUI_RIGHT:
        {
            const s32 liStep =
                (lpParams->meGameMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE) ? 2 : 1;
            if (miCurrentRound < lpParams->miNumRounds - liStep)
            {
                miCurrentRound = miCurrentRound + liStep;
                mRouteInfoDisplay.SetInfo(miCurrentRound, lpParams);
            }
            break;
        }

        case E_GAMEINPUTACTIONS_GUI_SELECT:
            if (mMenuOptions.GetHighlightedIndex() == 0)
            {
                FinishedCreatingGame();
            }
            else
            {
                ShowSaveScreen();
            }
            PostMenuAudio(mpStateInterface, KAC_AUDIO_ACCEPT_LABEL);
            break;

        case E_GAMEINPUTACTIONS_GUI_CANCEL:
            SendStateEvent(KAC_GO_BACK_EVENT);
            PostMenuAudio(mpStateInterface, KAC_GO_BACK_EVENT);
            break;

        default:
            break;
        }
    }

    // ================================================================================
    //  HandleControllerInputSaveOptions -- up / down through the slots (scrolling the
    //  seven-row window, wrapping at the ends), select a slot (asking first when it is
    //  taken), back to the summary page.
    // ================================================================================
    void OnlineGameOptionsSummary::HandleControllerInputSaveOptions(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineGameOptionsSummary::HandleControllerInputSaveOptions");

        OptionsDataProfile* lpProfile = mpGuiCache->GetOptionsDataProfile();
        const s32 liHighlightedSlot = mMenuOptions.GetHighlightedIndex() + miStartSaveItem;

        const ControllerButtonPayload* lpInput = static_cast<const ControllerButtonPayload*>(lpEvent);
        switch (lpInput->miButtonId)
        {
        case E_GAMEINPUTACTIONS_GUI_SELECT:
            if (liHighlightedSlot < lpProfile->GetNumCreatedOnlineGameOptions())
            {
                GuiOverlayRequestWire lOverwriteQuestion;
                lOverwriteQuestion.mRequest.Construct(KAC_OVERWRITE_QUESTION_OVERLAY_ID);
                PostWire(mpStateInterface, lOverwriteQuestion);
            }
            else
            {
                lpProfile->SetCreatedOnlineGameOptions(
                    liHighlightedSlot, mpGuiCache,
                    reinterpret_cast<const GuiEventNetworkGameParams*>(mpGuiCache->maOnlineGameModeOptionsStorage));
                PostWire(mpStateInterface, GuiAutosaveRequestWire());
                ShowGameOptionsScreen();
            }
            PostMenuAudio(mpStateInterface, KAC_AUDIO_ACCEPT_LABEL);
            break;

        case E_GAMEINPUTACTIONS_GUI_CANCEL:
            ShowGameOptionsScreen();
            PostMenuAudio(mpStateInterface, KAC_GO_BACK_EVENT);
            break;

        case E_GAMEINPUTACTIONS_GUI_UP:
        {
            bool lbChanged = false;
            const s32 liNumSaved = lpProfile->GetNumCreatedOnlineGameOptions();
            const s32 liNumRows  = (liNumSaved < KI_MAX_SAVED_ROUTES) ? liNumSaved + 1 : liNumSaved;

            if (mMenuOptions.HighlightPrevious())
            {
                const s32 liSlot = mMenuOptions.GetHighlightedIndex() + miStartSaveItem;
                miCurrentRound = 0;
                lbChanged = true;

                mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);
                GuiEventNetworkGameParams lSavedParams;
                lpProfile->GetCreatedOnlineGameOptions(liSlot, mpGuiCache, &lSavedParams);
                mRouteInfoDisplay.SetInfo(miCurrentRound, &lSavedParams);

                PostMenuAudio(mpStateInterface, KAC_AUDIO_MOVE_LABEL);
            }
            else if (miStartSaveItem > 0)
            {
                miStartSaveItem = miStartSaveItem - 1;
                lbChanged = true;
                SetupSaveMenuText();
            }
            else if (liNumRows > 1)
            {
                // Wrap to the last row.
                s32 liStart = liNumRows - KI_MAX_CREATE_GAME_OPTIONS;
                if (liStart < 0)
                {
                    liStart = 0;
                }
                miStartSaveItem = liStart;
                lbChanged = true;
                SetupSaveMenuText();
                mMenuOptions.HighlightIndex(liNumRows - miStartSaveItem - 1);
            }

            if (lbChanged)
            {
                if (liNumRows > 1)
                {
                    mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                           KPC_ARROW_ANIMATION_STATES[KI_ARROW_ANIMATE], false);
                    mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                             KPC_ARROW_ANIMATION_STATES[KI_ARROW_VISIBLE], false);
                }
                else
                {
                    mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                           KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
                    mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                             KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
                }
            }
            break;
        }

        case E_GAMEINPUTACTIONS_GUI_DOWN:
        {
            bool lbChanged = false;
            const s32 liNumSaved = lpProfile->GetNumCreatedOnlineGameOptions();
            const s32 liNumRows  = (liNumSaved < KI_MAX_SAVED_ROUTES) ? liNumSaved + 1 : liNumSaved;

            if (mMenuOptions.HighlightNext())
            {
                const s32 liSlot = mMenuOptions.GetHighlightedIndex() + miStartSaveItem;
                miCurrentRound = 0;
                lbChanged = true;

                if (!(liSlot < lpProfile->GetNumCreatedOnlineGameOptions()))
                {
                    mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_INVISIBLE);
                }
                else
                {
                    mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);
                    GuiEventNetworkGameParams lSavedParams;
                    lpProfile->GetCreatedOnlineGameOptions(liSlot, mpGuiCache, &lSavedParams);
                    mRouteInfoDisplay.SetInfo(miCurrentRound, &lSavedParams);
                }

                PostMenuAudio(mpStateInterface, KAC_AUDIO_MOVE_LABEL);
            }
            else
            {
                const s32 liNumSavedNow = lpProfile->GetNumCreatedOnlineGameOptions();
                if ((liNumSavedNow < KI_MAX_SAVED_ROUTES
                     && miStartSaveItem + KI_MAX_CREATE_GAME_OPTIONS < liNumSavedNow + 1)
                    || miStartSaveItem + KI_MAX_CREATE_GAME_OPTIONS < liNumSavedNow)
                {
                    miStartSaveItem = miStartSaveItem + 1;
                    lbChanged = true;
                    SetupSaveMenuText();
                }
                else if (liNumRows > 1)
                {
                    // Wrap to the first row.
                    miStartSaveItem = 0;
                    lbChanged = true;
                    SetupSaveMenuText();
                    mMenuOptions.HighlightIndex(0);
                }
            }

            if (lbChanged)
            {
                if (liNumRows > 1)
                {
                    mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                           KPC_ARROW_ANIMATION_STATES[KI_ARROW_VISIBLE], false);
                    mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                             KPC_ARROW_ANIMATION_STATES[KI_ARROW_ANIMATE], false);
                }
                else
                {
                    mUpArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                           KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
                    mDownArrowAnimator.AddOutputAptViewState(KAC_APT_TRANSITION_NAME,
                                                             KPC_ARROW_ANIMATION_STATES[KI_ARROW_INVISIBLE], false);
                }
            }
            break;
        }

        default:
            break;
        }
    }
}
