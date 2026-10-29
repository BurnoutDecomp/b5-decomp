// ===================================================================================
// BrnGui::OnlineSelectRoute -- the online route-selection screen state.
//   b5-decomp/src/GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.cpp
// ===================================================================================
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineSelectRoute.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"
#include "GameSource/Gui/BrnGuiShared.h"
#include <cstring>
#include <cmath>
#include "GameSource/Gui/BrnGuiWorldDataController.h"
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"
#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cmath>                                                          // route-time rounding / map-icon distance
#include <cstring>                                                        // tracker and event payloads
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // route invariants
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                  // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"              // raw controller payloads
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                      // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface (language manager, out queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // state input queue
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // per-round route events
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"           // GUI event id 257 params
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"          // LanguageManager::FormatAndAddText
#include "GameSource/Gui/BrnGuiCache.h"                                  // GuiCache::GetGuiTracker
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                    // custom route update event
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                          // BrnGui::GuiOverlayRequest
#include "GameSource/Gui/BrnGuiShared.h"                                 // gGuiResourceIdentifier
#include "GameSource/Gui/BrnGuiWorldDataController.h"                       // landmark inventory
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"                        // map-space transforms
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"                         // GuiTracker::GetRouteDistance
#include "GameSource/Input/GameInputActions.h"                           // GUI input ids

namespace BrnGui
{
namespace
{
    // The shortest route an online event accepts (rodata 1000.0; the main-option handler
    // tests the route against the same word).
    const f32 KF_MINIMUM_ROUTE_DISTANCE = 1000.0f;

    // The GUI-out channel the overlay request rides.
    const s32 KI_CHANNEL_GUI_OUT = 40;

    const u64 KU64_NO_APT_ID = 0xFFFFFFFFull; // X360 constructs MenuItem with 32-bit -1.

    const s32 KI_EVENT_CONTROLLER_PRESSED = 6;
    const s32 KI_EVENT_CONTROLLER_AXIS    = 8;
    const s32 KI_EVENT_NETWORK_DISCONNECT = 44;
    const s32 KI_EVENT_NETWORK_IN_GAME    = 50;
    const s32 KI_EVENT_NETWORK_IN_GAME_FAILED = 51;
    const s32 KI_EVENT_GUI_CACHE          = 64;

    const f32 KF_EVENT_ICON_FADE_TIME = 0.5f;
    const f32 KF_MAP_BASE_ASPECT_RATIO = 1.7777778f;   // flt_82F2693C, the 16:9 map base
    // ARTIST K_MAP_LONG_DISPLAY_RECT, initialized at 0x82C53A10 and consumed at
    // OnlineSelectRoute::CalculateZoomFactor @0x8248F4AC.
    const Vector2 K_MAP_LONG_DISPLAY_RECT = { 591.0f, 411.0f, 0.0f, 0.0f };
    const f32 KF_ROUTE_METRES_TO_SECONDS = 0.03f;      // 1 / average route speed
    const f32 KF_SECONDS_ROUND_VALUE = 5.0f;
    const char KAC_APT_TRANSITION[] = "apt_Transition";
    const char KAC_STATE_ADVANCE[] = "ADVANCE";
    const char KAC_STATE_GO_BACK[] = "GO_BACK";
    const char KAC_STATE_DISCONNECT[] = "DISCONNECT";
    const char KAC_OVERLAY_ENTER_GAME[] = "CNOnlEntGame";
    const char KAC_AUDIO_ACCEPT[] = "Accept";
    const char KAC_AUDIO_MOVE_LABEL[] = "MenuToggleDefault";
    const char KAC_AUDIO_PLACE_CHECKPOINT[] = "CodePlaceCheckpoint";
    const char KAC_AUDIO_MAP_SCROLL_END[] = "CodeMapScrollEnd";

    // ARTIST dword_8205F30C, read from the original IDB (10 big-endian words). The first
    // six are observed and forwarded to MainMapComponent; 213 is the wrapped SatNav view
    // state consumed by this state on teardown.
    const s32 maiEventToObserve[10] = { 14, 16, 21, 6, 50, 51, 44, 8, 64, 213 };

    struct MenuOption
    {
        OnlineSelectRoute::EMainMenuOptions meMenuOption;
        const char*                       mpcText;
    };

    // ARTIST `unk_8205F5D0`, `unk_8205F5F0`, `unk_8205F608` and `unk_8205F620`, with
    // exact ids and localized-string pointers verified by the bounded IDB read. The text
    // order differs from enum order: row 0 is Next Round (id 0), then Edit / Preset / Done.
    const MenuOption KA_MENU_OPTIONS[4] =
    {
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_EDIT,     "$ONLINE_SELECT_ROUTE_EDIT" },
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_PRESET,   "$ONLINE_SELECT_ROUTE_PRESET" },
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_NEXT,     "$CAPS_BUTTON_NEXT_ROUND" },
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_FINISHED, "$CAPS_BUTTON_FINISHED" },
    };
    const MenuOption KA_FINAL_MENU_OPTIONS[3] =
    {
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_EDIT,     "$ONLINE_SELECT_ROUTE_EDIT" },
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_PRESET,   "$ONLINE_SELECT_ROUTE_PRESET" },
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_FINISHED, "$CAPS_BUTTON_FINISHED" },
    };
    const MenuOption KA_SPECIAL_MENU_OPTIONS[3] =
    {
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_PRESET,   "$ONLINE_SELECT_ROUTE_PRESET" },
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_NEXT,     "$CAPS_BUTTON_NEXT_ROUND" },
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_FINISHED, "$CAPS_BUTTON_FINISHED" },
    };
    const MenuOption KA_SPECIAL_FINAL_MENU_OPTIONS[2] =
    {
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_PRESET,   "$ONLINE_SELECT_ROUTE_PRESET" },
        { OnlineSelectRoute::E_MAIN_MENU_OPTION_FINISHED, "$CAPS_BUTTON_FINISHED" },
    };

    const char* const KAPC_ANIMATION_STATES[3] =
    {
        "active", "inactive", "active_eventOnly"
    };

    typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;

    // StateInputQueue returns the payload after its wire header. The controller producer
    // posts exactly {padId, buttonId}; the 12-byte CgsGui::GuiEvent header is not present.
    struct ControllerButtonPayload : public CgsModule::Event
    {
        s32 miPadId;
        s32 miButtonId;
    };

    struct ControllerAxisPayload : public CgsModule::Event
    {
        s32 miAxis;
        f32 mfXAxis;
        f32 mfYAxis;
    };

    struct GuiCachePayload : public CgsModule::Event
    {
        GuiCache* mpCache;
    };

    // The game-state cache mirrors the pointer-free 10 * 44 byte event array at +0xA800.
    // This is a typed view of that named storage member, matching OnlineGameOptionsSummary.
    typedef BrnGameState::GameStateModuleIO::SpecificGameModeEventInterface::Event OnlineModeEvent;

    const PresetEvent* FindPresetEventById(const GuiCache* lpGuiCache, u32 luEventId)
    {
        for (s32 liIndex = 0; liIndex < static_cast<s32>(lpGuiCache->GetNumPresetEvents()); ++liIndex)
        {
            const PresetEvent* lpPreset = lpGuiCache->GetPresetEvent(liIndex);
            if (lpPreset->GetEventId() == luEventId)
                return lpPreset;
        }
        CGS_ASSERT(false, "Unable to find preset event with event id");
        return 0;
    }

    void PostAudio(CgsGui::StateInterface* lpStateInterface, const char* lpacLabel)
    {
        GuiAudioTriggerEvent lEvent;
        // ARTIST OutputGuiEvent<GuiAudioTriggerEvent> @0x82436890 posts this exact
        // payload wrapper on channel 40. The PC event has a different 12-byte header;
        // copy only its native 100-byte body into the console record.
        lEvent.Construct(7, "", lpacLabel, "");
        struct GuiAudioTriggerWire
        {
            s32 miSize;
            s32 miType;
            s32 miOffset;
            GuiAudioTriggerWirePayload457 mPayload;
        } lWire = { 100, 457, 12, {} };
        CGS_STATIC_ASSERT((sizeof(GuiAudioTriggerWire) == 112),
                      "ARTIST GUI audio trigger wrapper size");
        std::memcpy(&lWire.mPayload, lEvent.macComponent, sizeof(lWire.mPayload));
        lpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lWire)));
    }
    // The overlay request as the out-queue carries it: the GuiEvent<184> header sized to the
    // request, a pad word, then the request itself (304 bytes on the console).
    struct GuiOverlayRequestWire : public CgsGui::GuiEvent<184>
    {
        u32               muPad0C;    // +0x0C
        GuiOverlayRequest mRequest;   // +0x10
        GuiOverlayRequestWire()
            : CgsGui::GuiEvent<184>(static_cast<u32>(sizeof(GuiOverlayRequest)), 16)
            , muPad0C(0) {}
    };

    struct GuiOverlayWaitFinishWire : public CgsGui::GuiEvent<188>
    {
        GuiOverlayWaitFinishRequest mRequest;

        explicit GuiOverlayWaitFinishWire(const char* lpacOverlayName)
            : CgsGui::GuiEvent<188>(static_cast<u32>(sizeof(GuiOverlayWaitFinishRequest)), 16)
        {
            mRequest.Construct(lpacOverlayName);
        }
    };
}

    // the route screen and the shared online frame.
    const CgsGui::sResourceTuple OnlineSelectRoute::maResourceTuplesToLoad[] =
        { { 178, CgsGui::E_GUI_RESOURCETYPE_APT }, { 191, CgsGui::E_GUI_RESOURCETYPE_APT } };
    const s32 OnlineSelectRoute::miNumResourcesToLoad = 2;

    // member construction only: the console body installs the state and
    // route-component vtables, runs the two MenuComponent constructors, the HelpBar
    // constructor and the map component's MapManager constructor, and installs the vtables
    // of the text fields, the eight animators, the borough outline and the cursor. No
    // scalar member is stored until OnEnter.
    OnlineSelectRoute::OnlineSelectRoute()
    {
    }

    // ARTIST @0x82491210. The screen owns the top-level map and every movie clip named
    // here; the state switches from resource loading to component loading in Update.
    void OnlineSelectRoute::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, 10);

        MainMapComponent::MainMapParameterBundle lMapParameters;
        const Vector4 lViewRect = { 0.0f, 0.175f, 1.0f, 0.84027779f };
        lMapParameters.mv4ViewRect = lViewRect;
        const Vector4 lPaddingRect = { 0.4f, 0.18055555f, 0.95f, 0.83333331f };
        lMapParameters.mv4PaddingRect = lPaddingRect;
        lMapParameters.meMapType      = GuiEventRenderMainMap::E_MAPTYPE_MAINMAP;
        mMainMapComponent.Construct(mpStateInterface, &lMapParameters);
        mMainMapComponent.Prepare();
        mMainMapComponent.SetStickMapToScreenEdges(false, false, false, false);

        mCrashNavBorough.Construct("Borough_mc", mpStateInterface, 0);
        mCursor.Construct("cursor_mc", mpStateInterface, 6.0f, 0.0f, 0.0f, 0);
        mCursor.SetBounds(mMainMapComponent.GetDisplayRect());
        mCursor.SetAlwaysSnap(true);

        mSelectRoutes.Construct("SelectRoutes", mpStateInterface, 0);
        mMenuOptions.Construct("MenuItem", mpStateInterface, 4, 0, KU64_NO_APT_ID);
        mCurrentRoundDisplay.Construct("CurrentRoundTxt", mpStateInterface, 0);
        mEventNameValue.Construct("EventValue", mpStateInterface, 0);
        mEventNameValue.SetAutoSize(true);
        mTimeValue.Construct("TimeValue", mpStateInterface, 0);
        mDistanceValue.Construct("DistanceValue", mpStateInterface, 0);
        mCheckpointMenuAnimator.Construct("CheckpointMenu_anim", mpStateInterface, 0);
        mCursorAnimator.Construct("cursor_anim", mpStateInterface, 0);
        mInfoBackgroundAnimator.Construct("infobackground_anim", mpStateInterface, 0);
        mDistanceAnimator.Construct("distance_anim", mpStateInterface, 0);
        mTimeAnimator.Construct("time_anim", mpStateInterface, 0);
        mEventNameAnimator.Construct("event_name_anim", mpStateInterface, 0);
        mHelpBar.Construct("Button", 5, mpStateInterface, 0);
        mTitleText.Construct("TitleTxt", mpStateInterface, 0);

        meSubState                = E_SUBSTATE_LOADING_SCREEN;
        mpGuiCache                = 0;
        mpIconManager             = 0;
        mIconManagerOwnerId       = MapIconManager::E_CRASHNAV_MAP_ONLINE_SELECT_ROUTE;
        muSavedCheckpointId       = 0;
        mbWaitingForRoute         = false;
        mbEditingNewCheckpoint    = false;
        mbRouteEdited             = false;
        const rw::math::vpu::Vector2 lWorldCenter = { 0.0f, 0.0f, 0.0f, 0.0f };
        mv2WorldCenterPoint = lWorldCenter;
        mRandom.Construct();
        // ARTIST repeats this active latch after Prepare; it is a real store on the map
        // component and remains visible in the recovered source.
        mMainMapComponent.mbIsActive = true;
    }

    // ARTIST @0x8249DC40. Remove the subscriptions, clear level 3, hide the main map on
    // both GUI routes, return icon ownership and destroy the live help bar.
    void OnlineSelectRoute::OnLeave()
    {
        mpStateInterface->UnRegisterForEvents(maiEventToObserve, 10);
        mpStateInterface->PlayAptMovie("", 3);

        GuiEventShowHideSatNav lHideMainMap;
        lHideMainMap.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, false, 0.0f);
        mpStateInterface->OutputViewState(lHideMainMap);
        mpStateInterface->OutputInternalState(lHideMainMap);

        meSubState = E_SUBSTATE_LEAVING_STATE;
        if (mpIconManager != 0)
        {
            mpIconManager->ReleaseResources(mpStateInterface, mIconManagerOwnerId);
            mpIconManager->mbIsActive = true;
        }
        mMainMapComponent.mbIsActive = true;
        mHelpBar.Destruct();
    }

    // ARTIST @0x824B0148. The queue gives the event payload after its wire header. Every
    // event is also passed through the embedded MainMapComponent before the frame update.
    void OnlineSelectRoute::Update()
    {
        StateInputQueue* lpInQueue = reinterpret_cast<StateInputQueue*>(mpInGuiEventQueue);
        const CgsModule::Event* lpEvent = 0;
        s32 liEventSize = 0;

        for (s32 liEventId = lpInQueue->GetFirstEvent(&lpEvent, &liEventSize);
             lpEvent != 0;
             liEventId = lpInQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_CONTROLLER_PRESSED:
                HandleControllerInput(lpEvent);
                break;
            case KI_EVENT_CONTROLLER_AXIS:
                HandleControllerAxis(lpEvent);
                break;
            case KI_EVENT_NETWORK_DISCONNECT:
                CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
                mpGuiCache->SetDoDisconnectPopup(lpEvent);
                SendStateEvent(KAC_STATE_DISCONNECT);
                break;
            case KI_EVENT_NETWORK_IN_GAME:
                HandleInGameEvent(lpEvent);
                break;
            case KI_EVENT_NETWORK_IN_GAME_FAILED:
                HandleInGameFailedEvent(lpEvent);
                break;
            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                break;
            default:
                break;
            }

            mMainMapComponent.RecvEvent(lpEvent, liEventId);
        }

        mv2WorldCenterPoint = mMainMapComponent.Update(mv2WorldCenterPoint);
        lpInQueue->Clear();
        CheckForCompletedLoads();
        mSelectRoutes.Update();
        mMenuOptions.Update();

        if (meSubState == E_SUBSTATE_SELECTING_MAIN_OPTION ||
            meSubState == E_SUBSTATE_SELECTING_CHECKPOINT ||
            meSubState == E_SUBSTATE_SELECTING_POINT ||
            meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT)
        {
            UpdateIconManager();
        }

        if (meSubState > E_SUBSTATE_LOADING_COMPONENTS)
        {
            mHelpBar.Update(mpGuiCache->GetTime());
        }

        if (mMainMapComponent.IsZooming())
        {
            PlaceCursorOnLandmark();
        }

        GuiTracker* lpTracker = mpGuiCache->GetGuiTracker();
        if (lpTracker->IsRouteInfoAvailable() && mbWaitingForRoute)
        {
            const double ldDistance = lpTracker->GetRouteDistance();
            char lacFormattedValue[64];
            CgsCore::SPrintf(lacFormattedValue, sizeof(lacFormattedValue), "%f", ldDistance);
            mpStateInterface->GetLanguageManager()->FormatAndAddText(
                "ONLINE_SELECT_ROUTE_DISTANCE_VALUE", "%1",
                CgsLanguage::LanguageManager::E_FORMAT_TEXT, 1,
                lacFormattedValue, CgsLanguage::LanguageManager::E_FORMAT_LARGE_DISTANCE);

            // ARTIST @0x824B04EC..0x824B053C uses two single-precision fused
            // multiply-adds, then rounds the resulting five-second unit down with the
            // signed-magic fsel sequence, and multiplies that integer by 5.0f. The
            // image operands are -/+2^52, 0/1, 0.03f, 0.2f, 2.5f, 0.5f and 5.0f;
            // nearest-integer plus the residual correction is exactly floor here.
            const f32 lfDistance = static_cast<f32>(ldDistance);
            const f32 lfBiasedSeconds = lfDistance * KF_ROUTE_METRES_TO_SECONDS + 2.5f;
            const f32 lfFiveSecondUnits = lfBiasedSeconds * 0.2f + 0.5f;
            const f32 lfRoundedSeconds = static_cast<f32>(
                std::floor(static_cast<double>(lfFiveSecondUnits))) * KF_SECONDS_ROUND_VALUE;
            CgsCore::SPrintf(lacFormattedValue, sizeof(lacFormattedValue), "%f",
                             static_cast<double>(lfRoundedSeconds));
            mpStateInterface->GetLanguageManager()->FormatAndAddText(
                "ONLINE_SELECT_ROUTE_TIME_VALUE", "%1",
                CgsLanguage::LanguageManager::E_FORMAT_TEXT, 1,
                lacFormattedValue, CgsLanguage::LanguageManager::E_FORMAT_MINUTES_SECONDS);
            mTimeValue.SetText("$ONLINE_SELECT_ROUTE_TIME_VALUE");
            mDistanceValue.SetText("$ONLINE_SELECT_ROUTE_DISTANCE_VALUE");
            mbWaitingForRoute = false;
        }

        mCursor.Update();
    }

    // ARTIST @0x8249DDA8. Resources first, then wait for all registered components; only
    // after the cache is complete does the route UI install its screen and highlight row.
    void OnlineSelectRoute::CheckForCompletedLoads()
    {
        CGS_ASSERT(mpGuiCache, "mpGuiCache");
        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(
                    maResourceTuplesToLoad, static_cast<u32>(miNumResourcesToLoad)))
            {
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[178], 3);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
            return;
        }

        if (meSubState == E_SUBSTATE_LOADING_COMPONENTS && mpGuiCache &&
            mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN))
        {
            // ARTIST 8249DE80/84 stores one into the payload's show byte.
            GuiEventShowHideSatNav lShowMainMap;
            lShowMainMap.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, true, 0.0f);
            mpStateInterface->OutputViewState(lShowMainMap);
            mpStateInterface->OutputInternalState(lShowMainMap);
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mCursor.SetActive();
            mHelpBar.SetupComponent();

            const GuiEventNetworkGameParams* lpParams =
                reinterpret_cast<const GuiEventNetworkGameParams*>(
                    mpGuiCache->maOnlineGameModeOptionsStorage);
            mSelectRoutes.Setup(lpParams, mpGuiCache);
            ShowMainMenu();

            const s32 liRound = mSelectRoutes.GetCurrentRound();
            if (mpGuiCache->GetOnlineGameMode() ==
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
            {
                mMenuOptions.HighlightId(liRound >= mpGuiCache->miOnlineNumRounds - 2
                    ? E_MAIN_MENU_OPTION_FINISHED : E_MAIN_MENU_OPTION_NEXT);
            }
            else
            {
                mMenuOptions.HighlightId(liRound >= mpGuiCache->miOnlineNumRounds - 1
                    ? E_MAIN_MENU_OPTION_FINISHED : E_MAIN_MENU_OPTION_NEXT);
            }
        }
    }

    void OnlineSelectRoute::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpCacheEvent = static_cast<const GuiCachePayload*>(lpEvent);
        CGS_ASSERT(lpCacheEvent->mpCache, "Invalid cache in HandleGuiCacheEvent::Update");
        if (mpGuiCache == 0)
        {
            mpGuiCache = lpCacheEvent->mpCache;
            CGS_ASSERT(mpGuiCache, "mpGuiCache");
            mpIconManager = mpGuiCache->GetMapIconManager();
            CGS_ASSERT(mpIconManager, "mpIconManager");

            if ((CgsDev::Message::gxMessageFilterFlags & CgsDev::Message::KX_FILTER_GLOBAL) != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "MAPICONMANAGER: OnlineSelectRoute is calling SetOwnerParameters with OwnerID "
                    << static_cast<s32>(mIconManagerOwnerId) << ".\n";
            }
            mIconManagerOwnerId = mpIconManager->SetOwnerParameters(
                mpStateInterface, "SatNavIcon", MapIconManager::KI_MAX_CRASHNAV_MAP_ICONS,
                mIconManagerOwnerId, false, false, false,
                GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT, 0);
            mpIconManager->mbRotateSatNav = false;
            mpIconManager->meIconSizeMode = MapIconManager::E_ICONSIZE_LARGE;
            mpIconManager->mbShowingOnlineRoute = true;
            mpIconManager->SetIconsVisible(true);

            mSelectRoutes.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mMenuOptions.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mHelpBar.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCurrentRoundDisplay.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mEventNameValue.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTimeValue.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mDistanceValue.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleText.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCheckpointMenuAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCursorAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mInfoBackgroundAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mDistanceAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTimeAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mEventNameAnimator.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mCursor.GetName());

            // ARTIST fmuls/fctidz/stfiwx/lwz zero-extends the low clock word;
            // SetSeed reproduces its complete eight-slot refill and seed advance.
            const f32 lfClockTicks = mpGuiCache->GetTime() * 100.0f;
            const s64 liClockTicks = static_cast<s64>(lfClockTicks);
            mRandom.SetSeed(static_cast<u64>(static_cast<u32>(liClockTicks)));
        }
    }

    void OnlineSelectRoute::HandleInGameEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");
        CGS_ASSERT(lpEvent != 0, "lpInGameEvent");

        const GuiOverlayWaitFinishWire lRequest(KAC_OVERLAY_ENTER_GAME);
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lRequest), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(lRequest)));
        SendStateEvent(KAC_STATE_ADVANCE);
    }

    void OnlineSelectRoute::HandleInGameFailedEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleInGameEvent");
        SendStateEvent(KAC_STATE_GO_BACK);
    }

    void OnlineSelectRoute::SetupNormalRoute()
    {
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        GuiEventNetworkGameParams* lpParams =
            reinterpret_cast<GuiEventNetworkGameParams*>(mpGuiCache->maOnlineGameModeOptionsStorage);
        for (s32 liRound = 0; liRound < mpGuiCache->miOnlineNumRounds; ++liRound)
        {
            const u32 luStartPresetEventIndex = mSelectRoutes.GetStartPointEventIndex(liRound);
            const PresetEvent* lpStartPresetEvent =
                mpGuiCache->GetPresetEvent(static_cast<s32>(luStartPresetEventIndex));

            const s32 liNumCheckpoints = mSelectRoutes.GetNumberCheckpoints(liRound);
            CGS_ASSERT(liNumCheckpoints <= 17,
                       "mSelectRoutes.GetNumberCheckpoints( liRoundNumber ) <= "
                       "(KI_MAX_LANDMARKS_IN_MODE+1)");

            BrnGameState::LandmarkIndex laLandmarks[17];
            for (s32 liCheckpoint = 1; liCheckpoint < liNumCheckpoints; ++liCheckpoint)
            {
                laLandmarks[liCheckpoint - 1] =
                    mSelectRoutes.GetCheckpointLandmark(liRound, liCheckpoint);
            }

            lpParams->maEvents[liRound].Construct(
                static_cast<s32>(lpStartPresetEvent->GetEventId()),
                mSelectRoutes.GetStartLightTriggerID(liRound),
                laLandmarks, liNumCheckpoints - 1);
        }
    }

    void OnlineSelectRoute::HandleControllerInput(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleControllerInput");

        switch (meSubState)
        {
        case E_SUBSTATE_SELECTING_MAIN_OPTION:
            HandleControllerInputSelectMainOption(lpEvent);
            break;
        case E_SUBSTATE_SELECTING_CHECKPOINT:
            HandleControllerInputSelectCheckpoint(lpEvent);
            break;
        case E_SUBSTATE_SELECTING_POINT:
            HandleControllerInputSelectPoint(lpEvent);
            break;
        case E_SUBSTATE_SELECTING_PRESET_EVENT:
            HandleControllerInputSelectPresetEvent(lpEvent);
            break;
        default:
            break;
        }
    }

    void OnlineSelectRoute::HandleControllerInputSelectMainOption(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleControllerInputSelectMainOption");
        const ControllerButtonPayload* lpInput = static_cast<const ControllerButtonPayload*>(lpEvent);

        switch (lpInput->miButtonId)
        {
        case E_GAMEINPUTACTIONS_GUI_UP:
            if (mMenuOptions.HighlightPrevious())
                PostAudio(mpStateInterface, KAC_AUDIO_MOVE_LABEL);
            break;

        case E_GAMEINPUTACTIONS_GUI_DOWN:
            if (mMenuOptions.HighlightNext())
                PostAudio(mpStateInterface, KAC_AUDIO_MOVE_LABEL);
            break;

        case E_GAMEINPUTACTIONS_GUI_SELECT:
        {
            const u64 luHighlightedId = mMenuOptions.GetHighlightedId();
            if (luHighlightedId >= E_MAIN_MENU_OPTION_COUNT)
            {
                CGS_ASSERT(false, "Unknown menu option\n");
                PostAudio(mpStateInterface, KAC_AUDIO_ACCEPT);
                break;
            }

            const EMainMenuOptions leOption = static_cast<EMainMenuOptions>(luHighlightedId);
            if (leOption == E_MAIN_MENU_OPTION_NEXT || leOption == E_MAIN_MENU_OPTION_FINISHED)
            {
                GuiTracker* lpTracker = mpGuiCache->GetGuiTracker();
                if (lpTracker->IsRouteInfoAvailable() &&
                    lpTracker->GetRouteDistance() < KF_MINIMUM_ROUTE_DISTANCE)
                {
                    DisplayTooShortRouteMessage();
                }
                else if (leOption == E_MAIN_MENU_OPTION_NEXT &&
                         mSelectRoutes.GetCurrentRound() < mpGuiCache->miOnlineNumRounds - 1)
                {
                    SelectRound(E_SELECT_ROUND_FORWARDS);
                }
                else
                {
                    SetupNormalRoute();
                    SendStateEvent(KAC_STATE_ADVANCE);
                }
            }
            else if (leOption == E_MAIN_MENU_OPTION_EDIT)
            {
                ShowSelectCheckpointMenu();
            }
            else if (leOption == E_MAIN_MENU_OPTION_PRESET)
            {
                ShowPresetEvents();
            }

            PostAudio(mpStateInterface, KAC_AUDIO_ACCEPT);
            break;
        }

        case E_GAMEINPUTACTIONS_GUI_CANCEL:
            if (mSelectRoutes.GetCurrentRound() > 0)
                SelectRound(E_SELECT_ROUND_BACKWARDS);
            else
                SendStateEvent(KAC_STATE_GO_BACK);
            PostAudio(mpStateInterface, KAC_STATE_GO_BACK);
            break;

        default:
            break;
        }
    }

    void OnlineSelectRoute::HandleControllerInputSelectCheckpoint(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleControllerInputSelectCheckpoint");
        const ControllerButtonPayload* lpInput = static_cast<const ControllerButtonPayload*>(lpEvent);
        if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_CANCEL)
        {
            ShowMainMenu();
            return;
        }

        const SelectRoutes::EChangedItem leChangedItem = mSelectRoutes.HandleControllerInput(lpEvent);
        UpdateAfterToggleChange(leChangedItem);
    }

    void OnlineSelectRoute::HandleControllerInputSelectPoint(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleControllerInputSelectPoint");
        const ControllerButtonPayload* lpInput = static_cast<const ControllerButtonPayload*>(lpEvent);
        if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_DPAD_UP && !mMainMapComponent.IsZooming())
        {
            mCursor.SetDelta(0.0f, 1.0f, mpGuiCache->GetTime());
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_DPAD_DOWN && !mMainMapComponent.IsZooming())
        {
            mCursor.SetDelta(0.0f, -1.0f, mpGuiCache->GetTime());
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_DPAD_LEFT && !mMainMapComponent.IsZooming())
        {
            mCursor.SetDelta(-1.0f, 0.0f, mpGuiCache->GetTime());
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_DPAD_RIGHT && !mMainMapComponent.IsZooming())
        {
            mCursor.SetDelta(1.0f, 0.0f, mpGuiCache->GetTime());
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_SELECT)
        {
            mbRouteEdited = true;
            mpIconManager->SetUseEventIcons(GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT,
                                            mpStateInterface, KF_EVENT_ICON_FADE_TIME, 0, 0);

            const s32 liNumCheckpoints = mSelectRoutes.GetNumberCheckpoints(mSelectRoutes.GetCurrentRound());
            const s32 liSelectedIndex = mSelectRoutes.GetCurrentlySelectedCheckpointIndex();
            if (mbEditingNewCheckpoint)
            {
                const bool lbLastSlot =
                    (mpGuiCache->GetOnlineGameMode() ==
                         BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN &&
                     liSelectedIndex == liNumCheckpoints - 1) ||
                    (mpGuiCache->GetOnlineGameMode() !=
                         BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN &&
                     ((liSelectedIndex == liNumCheckpoints - 1 && liNumCheckpoints == 17) ||
                      (liSelectedIndex == liNumCheckpoints - 2 && liNumCheckpoints < 17)));
                if (lbLastSlot)
                    mSelectRoutes.HighlightAddCheckpoint();
            }

            PostAudio(mpStateInterface, KAC_AUDIO_PLACE_CHECKPOINT);
            ShowSelectCheckpointMenu();
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_CANCEL)
        {
            if (mbEditingNewCheckpoint)
            {
                mSelectRoutes.RemoveLastCheckpoint();
            }
            else if (mSelectRoutes.GetCurrentlySelectedCheckpointIndex() != 0)
            {
                mSelectRoutes.SetCurrentlySelectedCheckpointLandmark(
                    mSelectRoutes.GetCurrentRound(),
                    static_cast<BrnGameState::LandmarkIndex>(muSavedCheckpointId));
            }
            else
            {
                mSelectRoutes.SetStartLightTriggerID(mSelectRoutes.GetCurrentRound(),
                                                     muSavedCheckpointId);
            }

            PostAudio(mpStateInterface, KAC_STATE_GO_BACK);
            mpIconManager->SetUseEventIcons(GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT,
                                            mpStateInterface, KF_EVENT_ICON_FADE_TIME, 0, 0);
            ShowSelectCheckpointMenu();
        }
    }

    void OnlineSelectRoute::HandleControllerInputSelectPresetEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0,
                   "Invalid event sent to OnlineSelectRoute::HandleControllerInputSelectPresetEvent");
        const ControllerButtonPayload* lpInput = static_cast<const ControllerButtonPayload*>(lpEvent);
        if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_DPAD_UP && !mMainMapComponent.IsZooming())
        {
            mCursor.SetDelta(0.0f, 1.0f, mpGuiCache->GetTime());
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_DPAD_DOWN && !mMainMapComponent.IsZooming())
        {
            mCursor.SetDelta(0.0f, -1.0f, mpGuiCache->GetTime());
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_DPAD_LEFT && !mMainMapComponent.IsZooming())
        {
            mCursor.SetDelta(-1.0f, 0.0f, mpGuiCache->GetTime());
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_DPAD_RIGHT && !mMainMapComponent.IsZooming())
        {
            mCursor.SetDelta(1.0f, 0.0f, mpGuiCache->GetTime());
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_SELECT)
        {
            mpIconManager->SetUseEventIcons(GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT,
                                            mpStateInterface, KF_EVENT_ICON_FADE_TIME, 0, 0);
            mSelectRoutes.SetRouteFromPresetEvent(static_cast<s32>(muCurrentPresetEventID));
            PostAudio(mpStateInterface, KAC_AUDIO_PLACE_CHECKPOINT);
            ShowMainMenu();
        }
        else if (lpInput->miButtonId == E_GAMEINPUTACTIONS_GUI_CANCEL)
        {
            mpIconManager->SetUseEventIcons(GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_COUNT,
                                            mpStateInterface, KF_EVENT_ICON_FADE_TIME, 0, 0);
            ShowMainMenu();
            PostAudio(mpStateInterface, KAC_STATE_GO_BACK);
        }
    }

    void OnlineSelectRoute::ShowMainMenu()
    {
        const ESubState leOldSubState = meSubState;
        meSubState = E_SUBSTATE_SELECTING_MAIN_OPTION;
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        const s32 liMode = mpGuiCache->GetOnlineGameMode();
        const s32 liRound = mSelectRoutes.GetCurrentRound();
        const bool lbSpecialMode =
            liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN ||
            liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE ||
            liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END;
        const bool lbLastRound =
            (liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE &&
             liRound >= mpGuiCache->miOnlineNumRounds - 2) ||
            (liMode != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE &&
             liRound >= mpGuiCache->miOnlineNumRounds - 1);

        const MenuOption* lpMenuOptions = 0;
        s32 liMenuOptionCount = 0;
        if (lbLastRound)
        {
            if (lbSpecialMode)
            {
                lpMenuOptions = KA_SPECIAL_FINAL_MENU_OPTIONS;
                liMenuOptionCount = 2;
            }
            else
            {
                lpMenuOptions = KA_FINAL_MENU_OPTIONS;
                liMenuOptionCount = 3;
            }
        }
        else if (lbSpecialMode)
        {
            lpMenuOptions = KA_SPECIAL_MENU_OPTIONS;
            liMenuOptionCount = 3;
        }
        else
        {
            lpMenuOptions = KA_MENU_OPTIONS;
            liMenuOptionCount = 4;
        }

        mMenuOptions.SetupMenu(liMenuOptionCount, false);
        for (s32 liIndex = 0; liIndex < liMenuOptionCount; ++liIndex)
        {
            mMenuOptions.SetText(liIndex, lpMenuOptions[liIndex].mpcText);
            mMenuOptions.SetId(liIndex, static_cast<u64>(lpMenuOptions[liIndex].meMenuOption));
        }

        mSelectRoutes.Hide();
        mCursor.SetInactive();
        mCheckpointMenuAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[0], false);
        mCursorAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[1], false);
        mEventNameAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[1], false);

        const char* lpacInfoState = lbSpecialMode ? KAPC_ANIMATION_STATES[1]
                                                  : KAPC_ANIMATION_STATES[0];
        mInfoBackgroundAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpacInfoState, false);
        mDistanceAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpacInfoState, false);
        mTimeAnimator.AddOutputAptViewState(KAC_APT_TRANSITION, lpacInfoState, false);

        mMainMapComponent.SetActive(false);
        mTitleText.SetText("$CN_TITLE_SELECT_ROUTE");
        if (mpIconManager != 0)
            mpIconManager->mbIsActive = false;

        if (mpGuiCache != 0 && meSubState > E_SUBSTATE_LOADING_COMPONENTS)
        {
            const f32 lfZoomFactor = CalculateZoomFactor();
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                      lfZoomFactor, false);
        }
        else
        {
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_LOW, 0.0f, false);
        }

        PlaceCursorOnLandmark();
        SetActiveLandmarks();
        DisplayCurrentRound();
        SetupHelpBar(leOldSubState != E_SUBSTATE_LOADING_COMPONENTS);
    }

    void OnlineSelectRoute::ShowSelectCheckpointMenu()
    {
        if (meSubState == E_SUBSTATE_SELECTING_MAIN_OPTION)
            mSelectRoutes.Show();

        meSubState = E_SUBSTATE_SELECTING_CHECKPOINT;
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");
        mMenuOptions.Clear();
        mCursor.SetActive();
        mbEditingNewCheckpoint = false;

        mCheckpointMenuAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[0], false);
        mCursorAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[1], false);
        mEventNameAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[1], false);

        mMainMapComponent.SetActive(false);
        mTitleText.SetText("$CN_TITLE_EDIT_ROUTE");
        if (mpIconManager != 0)
            mpIconManager->mbIsActive = false;

        if (mpGuiCache != 0 && meSubState > E_SUBSTATE_LOADING_COMPONENTS)
        {
            const f32 lfZoomFactor = CalculateZoomFactor();
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                      lfZoomFactor, false);
        }
        else
        {
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_LOW, 0.0f, false);
        }

        PlaceCursorOnLandmark();
        SetActiveLandmarks();
        DisplayCurrentRound();
        SetupHelpBar(true);
    }

    void OnlineSelectRoute::ShowPresetEvents()
    {
        meSubState = E_SUBSTATE_SELECTING_PRESET_EVENT;
        CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

        mpIconManager->SetUseEventIcons(
            GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_EVENT_PRESETS,
            mpStateInterface, KF_EVENT_ICON_FADE_TIME, 0, 0);

        const u32 luPresetEventIndex =
            mSelectRoutes.GetStartPointEventIndex(mSelectRoutes.GetCurrentRound());
        const PresetEvent* lpPresetEvent = mpGuiCache->GetPresetEvent(static_cast<s32>(luPresetEventIndex));
        muCurrentPresetEventID = lpPresetEvent->GetEventId();

        const f32 lfInitialZoomFactor = CalculateZoomFactor();
        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                  lfInitialZoomFactor, false);
        mCursor.SetActive();
        mbEditingNewCheckpoint = false;
        mCheckpointMenuAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[1], false);
        mCursorAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[0], false);
        mEventNameAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[0], false);

        const s32 liMode = mpGuiCache->GetOnlineGameMode();
        const bool lbSpecialMode =
            liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN ||
            liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE ||
            liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END;
        mInfoBackgroundAnimator.AddOutputAptViewState(
            KAC_APT_TRANSITION, KAPC_ANIMATION_STATES[lbSpecialMode ? 2 : 0], false);
        mMainMapComponent.SetActive(true);
        if (mpIconManager != 0)
            mpIconManager->mbIsActive = true;

        if (mpGuiCache != 0 && meSubState > E_SUBSTATE_LOADING_COMPONENTS)
        {
            const f32 lfZoomFactor = CalculateZoomFactor();
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                      lfZoomFactor, false);
        }
        else
        {
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_LOW, 0.0f, false);
        }
        PlaceCursorOnLandmark();
        SetActiveLandmarks();
        SetupHelpBar(true);
        DisplayEventName();
    }

    void OnlineSelectRoute::SetupPreSetRaceToggle()
    {
        if (mpGuiCache != 0 && meSubState > E_SUBSTATE_LOADING_COMPONENTS)
        {
            const f32 lfZoomFactor = CalculateZoomFactor();
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                      lfZoomFactor, false);
        }
        else
        {
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_LOW, 0.0f, false);
        }
        PlaceCursorOnLandmark();
    }

    void OnlineSelectRoute::SelectRound(ESelectRound leDirection)
    {
        const s32 liRoundDelta =
            mpGuiCache->GetOnlineGameMode() ==
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE ? 2 : 1;
        s32 liNextRound = mSelectRoutes.GetCurrentRound();
        if (leDirection == E_SELECT_ROUND_FORWARDS)
        {
            liNextRound += liRoundDelta;
        }
        else if (leDirection == E_SELECT_ROUND_BACKWARDS)
        {
            liNextRound -= liRoundDelta;
        }
        else
        {
            CGS_ASSERT(false, "Invalid select round value");
        }

        mSelectRoutes.SetCurrentRound(liNextRound);
        SetupHelpBar(true);

        if (mpGuiCache != 0 && meSubState > E_SUBSTATE_LOADING_COMPONENTS)
        {
            const f32 lfZoomFactor = CalculateZoomFactor();
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM,
                                      lfZoomFactor, false);
        }
        else
        {
            mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_LOW, 0.0f, false);
        }

        PlaceCursorOnLandmark();
        SetActiveLandmarks();
        DisplayCurrentRound();
        ShowMainMenu();

        const s32 liMode = mpGuiCache->GetOnlineGameMode();
        const bool lbLastRound =
            (liMode == BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE &&
             liNextRound >= mpGuiCache->miOnlineNumRounds - 2) ||
            (liMode != BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE &&
             liNextRound >= mpGuiCache->miOnlineNumRounds - 1);
        mMenuOptions.HighlightId(lbLastRound ? E_MAIN_MENU_OPTION_FINISHED
                                              : E_MAIN_MENU_OPTION_NEXT);
    }

    void OnlineSelectRoute::SetupHelpBar(bool lbEditingRoute)
    {
        (void)lbEditingRoute;
        const char* lapcHelpText[5];
        ButtonIconComponent::EPadButton laButtons[5];
        s32 liNumHelpItems = 0;

        if (meSubState == E_SUBSTATE_SELECTING_MAIN_OPTION)
        {
            lapcHelpText[0] = "$CAPS_BUTTON_SELECT";
            laButtons[0] = static_cast<ButtonIconComponent::EPadButton>(4);
            lapcHelpText[1] = "$CAPS_BUTTON_BACK_UP";
            laButtons[1] = static_cast<ButtonIconComponent::EPadButton>(5);
            liNumHelpItems = 2;
        }
        else if (meSubState == E_SUBSTATE_SELECTING_CHECKPOINT)
        {
            const SelectRoutes::EMenuItemType leType = mSelectRoutes.GetCurrentMenuItemType();
            const s32 liNumCheckpoints =
                mSelectRoutes.GetNumberCheckpoints(mSelectRoutes.GetCurrentRound());

            switch (leType)
            {
            case SelectRoutes::E_MENU_ITEM_TYPE_START_POINT:
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_EDIT";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(4);
                if (liNumCheckpoints < 17)
                {
                    lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_ADD_CHECKPOINT";
                    laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(11);
                }
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_RANDOMIZE";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(6);
                break;

            case SelectRoutes::E_MENU_ITEM_TYPE_CHECKPOINT:
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_EDIT";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(4);
                if (liNumCheckpoints < 17)
                {
                    lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_ADD_CHECKPOINT";
                    laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(11);
                }
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_DELETE_CHECKPOINT";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(7);
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_RANDOMIZE";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(6);
                break;

            case SelectRoutes::E_MENU_ITEM_TYPE_ADD_CHECKPOINT:
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_ADD";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(4);
                if (liNumCheckpoints < 17)
                {
                    lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_ADD_CHECKPOINT";
                    laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(11);
                }
                break;

            case SelectRoutes::E_MENU_ITEM_TYPE_FINISH_POINT:
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_EDIT";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(4);
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_RANDOMIZE";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(6);
                break;

            case SelectRoutes::E_MENU_ITEM_TYPE_DONE:
                lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_SELECT";
                laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(4);
                break;

            default:
                CGS_ASSERT(false, "Unhandled menu item type");
                break;
            }

            lapcHelpText[liNumHelpItems] = "$CAPS_BUTTON_BACK_UP";
            laButtons[liNumHelpItems++] = static_cast<ButtonIconComponent::EPadButton>(5);
        }
        else if (meSubState == E_SUBSTATE_SELECTING_POINT ||
                 meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT)
        {
            lapcHelpText[0] = "$CAPS_BUTTON_PLACE";
            laButtons[0] = static_cast<ButtonIconComponent::EPadButton>(4);
            lapcHelpText[1] = "$CAPS_BUTTON_SCROLL";
            laButtons[1] = static_cast<ButtonIconComponent::EPadButton>(13);
            lapcHelpText[2] = "$CAPS_BUTTON_BACK_UP";
            laButtons[2] = static_cast<ButtonIconComponent::EPadButton>(5);
            liNumHelpItems = 3;
        }
        else
        {
            CGS_ASSERT(false, "Setting helpbar when not in a valid state");
        }

        mHelpBar.Clear();
        for (s32 liIndex = 0; liIndex < liNumHelpItems; ++liIndex)
        {
            mHelpBar.AppendHelpBarItem(
                lapcHelpText[liIndex], laButtons[liIndex],
                static_cast<ButtonIconComponent::EPadButton>(15));
        }
    }

    void OnlineSelectRoute::DisplayCurrentRound()
    {
        char lacCurrentRound[64];
        char lacCurrentRoundEnd[64];
        char lacTotalRounds[64];
        const s32 liRound = mSelectRoutes.GetCurrentRound();

        if (mpGuiCache->GetOnlineGameMode() ==
            BrnGameState::GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)
        {
            CgsCore::SPrintf(lacCurrentRound, sizeof(lacCurrentRound), "%d", liRound + 1);
            CgsCore::SPrintf(lacCurrentRoundEnd, sizeof(lacCurrentRoundEnd), "%d", liRound + 2);
            CgsCore::SPrintf(lacTotalRounds, sizeof(lacTotalRounds), "%d", mpGuiCache->miOnlineNumRounds);
            mpStateInterface->GetLanguageManager()->FormatAndAddText(
                "ONLINE_CURRENT_ROUND", "ONLINE_ROUTE_NUM_ROUNDS_RR",
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 3,
                lacCurrentRound, CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                lacCurrentRoundEnd, CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                lacTotalRounds, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }
        else
        {
            CgsCore::SPrintf(lacCurrentRound, sizeof(lacCurrentRound), "%d", liRound + 1);
            CgsCore::SPrintf(lacTotalRounds, sizeof(lacTotalRounds), "%d", mpGuiCache->miOnlineNumRounds);
            mpStateInterface->GetLanguageManager()->FormatAndAddText(
                "ONLINE_CURRENT_ROUND", "ONLINE_ROUTE_NUM_ROUNDS",
                CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP, 2,
                lacCurrentRound, CgsLanguage::LanguageManager::E_FORMAT_INTEGER,
                lacTotalRounds, CgsLanguage::LanguageManager::E_FORMAT_INTEGER);
        }
        mCurrentRoundDisplay.SetText("$ONLINE_CURRENT_ROUND");
    }

    void OnlineSelectRoute::DisplayEventName()
    {
        char lacEventName[64];
        CgsCore::SPrintf(lacEventName, sizeof(lacEventName), "EV_%d_O", muCurrentPresetEventID);
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "ONLINE_SELECT_ROUTE_EVENT_NAME_VALUE", "%1",
            CgsLanguage::LanguageManager::E_FORMAT_TEXT, 1,
            lacEventName, CgsLanguage::LanguageManager::E_FORMAT_ID_LOOKUP);
        mEventNameValue.SetText("$ONLINE_SELECT_ROUTE_EVENT_NAME_VALUE");
    }

    s32 OnlineSelectRoute::GetNumBoundingBoxCoords() const
    {
        if (meSubState == E_SUBSTATE_SELECTING_POINT)
        {
            const s32 liSelectedIndex = mSelectRoutes.GetCurrentlySelectedCheckpointIndex();
            if (liSelectedIndex == -1 || liSelectedIndex == 0)
                return static_cast<s32>(mpGuiCache->GetNumPresetEvents());

            return mpGuiCache->GetWorldDataController()->GetTotalNumberOfLandmarks();
        }

        if (meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT)
            return static_cast<s32>(mpGuiCache->GetNumPresetEvents());

        return mSelectRoutes.GetNumberCheckpoints(mSelectRoutes.GetCurrentRound());
    }

    Vector3 OnlineSelectRoute::GetZoomBoundingBoxCoord(s32 liIndex) const
    {
        Vector3 lv3Position;

        if (meSubState == E_SUBSTATE_SELECTING_POINT)
        {
            const s32 liSelectedIndex = mSelectRoutes.GetCurrentlySelectedCheckpointIndex();
            if (liSelectedIndex == -1 || liSelectedIndex == 0)
            {
                const PresetEvent* lpPreset = mpGuiCache->GetPresetEvent(liIndex);
                const SatNavEventDisplayInfo* lpEventStart =
                    mpGuiCache->GetPresetEventDisplayInfo(lpPreset->GetPositionLookupId());
                CGS_ASSERT(lpEventStart != 0, "lpEventStart");
                lv3Position = lpEventStart->mv3Position;
            }
            else
            {
                GuiEventUpdateSatNav::SatNavIconInfo lIconInfo;
                mpGuiCache->GetLandmarkInfoAtPositionInList(liIndex, &lIconInfo);
                const Vector4& lrv4Position = lIconInfo.GetPositionLane();
                lv3Position.x = lrv4Position.x;
                lv3Position.y = lrv4Position.y;
                lv3Position.z = lrv4Position.z;
                lv3Position.w = lrv4Position.w;
            }
            return lv3Position;
        }

        if (meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT)
        {
            const PresetEvent* lpPreset = mpGuiCache->GetPresetEvent(liIndex);
            const SatNavEventDisplayInfo* lpEventStart =
                mpGuiCache->GetPresetEventDisplayInfo(lpPreset->GetPositionLookupId());
            CGS_ASSERT(lpEventStart != 0, "lpEventStart");
            return lpEventStart->mv3Position;
        }

        if (liIndex == 0)
        {
            const SatNavEventDisplayInfo* lpEventStart = mpGuiCache->GetPresetEventDisplayInfo(
                mSelectRoutes.GetStartLightTriggerID(mSelectRoutes.GetCurrentRound()));
            CGS_ASSERT(lpEventStart != 0, "lpEventStart");
            return lpEventStart->mv3Position;
        }

        GuiEventUpdateSatNav::SatNavIconInfo lIconInfo;
        const BrnGameState::LandmarkIndex lLandmark =
            mSelectRoutes.GetCheckpointLandmark(mSelectRoutes.GetCurrentRound(), liIndex);
        mpGuiCache->GetLandmarkInfoFromIndex(lLandmark, &lIconInfo);
        const Vector4& lrv4Position = lIconInfo.GetPositionLane();
        lv3Position.x = lrv4Position.x;
        lv3Position.y = lrv4Position.y;
        lv3Position.z = lrv4Position.z;
        lv3Position.w = lrv4Position.w;
        return lv3Position;
    }

    f32 OnlineSelectRoute::CalculateZoomFactor()
    {
        const Vector2 lv2First = MapTransform::Flatten(GetZoomBoundingBoxCoord(0));
        Vector2 lv2Min = lv2First;
        Vector2 lv2Max = lv2First;
        const s32 liNumCoords = GetNumBoundingBoxCoords();
        for (s32 liIndex = 1; liIndex < liNumCoords; ++liIndex)
        {
            const Vector2 lv2Point = MapTransform::Flatten(GetZoomBoundingBoxCoord(liIndex));
            if (lv2Point.x < lv2Min.x) lv2Min.x = lv2Point.x;
            if (lv2Point.y < lv2Min.y) lv2Min.y = lv2Point.y;
            if (lv2Point.x > lv2Max.x) lv2Max.x = lv2Point.x;
            if (lv2Point.y > lv2Max.y) lv2Max.y = lv2Point.y;
        }

        Vector2 lv2Centre;
        lv2Centre.x = (lv2Min.x + lv2Max.x) * 0.5f;
        lv2Centre.y = (lv2Min.y + lv2Max.y) * 0.5f;
        lv2Centre.z = lv2Centre.x;
        lv2Centre.w = lv2Centre.x;
        mMainMapComponent.SetDesiredWorldCentre(lv2Centre);

        return MapTransform::CalculateZoomFactor(lv2Min, lv2Max,
                                                 K_MAP_LONG_DISPLAY_RECT,
                                                 KF_MAP_BASE_ASPECT_RATIO);
    }

    void OnlineSelectRoute::PlaceCursorOnLandmark()
    {
        if (mpGuiCache == 0)
            return;

        Vector3 lv3WorldPosition;
        bool lbPlaceCursor = false;

        if (meSubState == E_SUBSTATE_SELECTING_CHECKPOINT ||
            meSubState == E_SUBSTATE_SELECTING_POINT)
        {
            const SelectRoutes::EMenuItemType leMenuType = mSelectRoutes.GetCurrentMenuItemType();
            if (leMenuType == SelectRoutes::E_MENU_ITEM_TYPE_START_POINT)
            {
                const SatNavEventDisplayInfo* lpEventStart = mpGuiCache->GetPresetEventDisplayInfo(
                    mSelectRoutes.GetStartLightTriggerID(mSelectRoutes.GetCurrentRound()));
                CGS_ASSERT(lpEventStart != 0, "lpEventStart");
                lv3WorldPosition = lpEventStart->mv3Position;
                mCursor.SetActive();
                lbPlaceCursor = true;
            }
            else if (leMenuType == SelectRoutes::E_MENU_ITEM_TYPE_CHECKPOINT ||
                     leMenuType == SelectRoutes::E_MENU_ITEM_TYPE_FINISH_POINT)
            {
                GuiEventUpdateSatNav::SatNavIconInfo lIconInfo;
                const BrnGameState::LandmarkIndex lLandmark =
                    mSelectRoutes.GetCurrentlySelectedCheckpointLandmark(mSelectRoutes.GetCurrentRound());
                mpGuiCache->GetLandmarkInfoFromIndex(lLandmark, &lIconInfo);
                const Vector4& lrv4Position = lIconInfo.GetPositionLane();
                lv3WorldPosition.x = lrv4Position.x;
                lv3WorldPosition.y = lrv4Position.y;
                lv3WorldPosition.z = lrv4Position.z;
                lv3WorldPosition.w = lrv4Position.w;
                mCursor.SetActive();
                lbPlaceCursor = true;
            }
            else
            {
                mCursor.SetInactive();
            }
        }
        else if (meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT)
        {
            const PresetEvent* lpPreset = FindPresetEventById(mpGuiCache, muCurrentPresetEventID);
            CGS_ASSERT(lpPreset != 0, "lpPresetEvent");
            const SatNavEventDisplayInfo* lpEventStart =
                mpGuiCache->GetPresetEventDisplayInfo(lpPreset->GetPositionLookupId());
            CGS_ASSERT(lpEventStart != 0, "lpEventStart");
            lv3WorldPosition = lpEventStart->mv3Position;
            lbPlaceCursor = true;
        }

        if (lbPlaceCursor)
        {
            // ARTIST @0x8248F4D0 builds the current world-rect and view-rect coordinate
            // spaces, composes the view mapping with the device space, then transforms the
            // world XZ point into screen pixels. Keep those three named transforms in that
            // same coordinate order: current map world -> normalized -> viewport -> device.
            const Vector2 lv2WorldPosition = MapTransform::Flatten(lv3WorldPosition);
            const Vector2 lv2NormalizedPosition = MapTransform::Transform(
                lv2WorldPosition, mMainMapComponent.GetWorldRect(), MapTransform::GetNormalisedRect());
            const Vector2 lv2ViewPosition = MapTransform::Transform(
                lv2NormalizedPosition, MapTransform::GetNormalisedRect(),
                mMainMapComponent.GetViewRect());
            const Vector2 lv2DevicePosition = MapTransform::Transform(
                lv2ViewPosition, MapTransform::GetDeviceSpace());
            mCursor.SetPosition(lv2DevicePosition, false);
        }
    }
    void OnlineSelectRoute::SetActiveLandmarks()
    {
        s32 liNumTrackedItems = 0;
        u32 luStartTriggerId = 0;
        const PresetEvent* lpPresetEvent = 0;

        if (meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT)
        {
            lpPresetEvent = FindPresetEventById(mpGuiCache, muCurrentPresetEventID);
            CGS_ASSERT(lpPresetEvent != 0, "lpPresetEvent");
            liNumTrackedItems = lpPresetEvent->GetNumLandmarks() + 1;
            luStartTriggerId = lpPresetEvent->GetPositionLookupId();
        }
        else
        {
            liNumTrackedItems =
                mSelectRoutes.GetNumberCheckpoints(mSelectRoutes.GetCurrentRound());
            luStartTriggerId =
                mSelectRoutes.GetStartLightTriggerID(mSelectRoutes.GetCurrentRound());
        }

        CGS_ASSERT(liNumTrackedItems <= 64,
                   "lSetTrackerEvent.miNumTrackedItems <= GuiTracker::KI_TRACKER_STACK_SIZE");
        const SatNavEventDisplayInfo* lpEventStart =
            mpGuiCache->GetPresetEventDisplayInfo(luStartTriggerId);
        CGS_ASSERT(lpEventStart != 0, "lpEventStart");

        GuiEventSetActiveLandmarks lActiveLandmarks;
        lActiveLandmarks.muNumLandmarks = static_cast<u32>(liNumTrackedItems - 1);
        GuiEventSetTracker lSetTrackerEvent;
        lSetTrackerEvent.mbIsEntireRoute = true;
        lSetTrackerEvent.miCurrentlyTrackedIndex = 0;

        GuiTracker::TrackerInformation& lrStart = lSetTrackerEvent.mTrackedDataInfo[0];
        lrStart.meIconType = GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_JUNCTION;
        lrStart.mv3Position = lpEventStart->mv3Position;
        lrStart.muTargetSectionId = static_cast<u16>(lpEventStart->mi16AISectionIndex);
        lrStart.mTargetJunctionId = lpEventStart->muJunctionId;
        lrStart.mTargetLandmarkIndex = 0;

        for (s32 liCheckpoint = 1; liCheckpoint < liNumTrackedItems; ++liCheckpoint)
        {
            BrnGameState::LandmarkIndex lLandmark;
            if (lpPresetEvent != 0)
            {
                lLandmark = lpPresetEvent->GetLandmark(liCheckpoint - 1);
            }
            else
            {
                lLandmark = mSelectRoutes.GetCheckpointLandmark(
                    mSelectRoutes.GetCurrentRound(), liCheckpoint);
            }

            GuiEventUpdateSatNav::SatNavIconInfo lIconInfo;
            mpGuiCache->GetLandmarkInfoFromIndex(lLandmark, &lIconInfo);
            GuiTracker::TrackerInformation& lrItem =
                lSetTrackerEvent.mTrackedDataInfo[liCheckpoint];
            lrItem.meIconType = GuiEventUpdateSatNav::SatNavIconInfo::E_SATNAVICON_LANDMARK;
            const Vector4& lv4Position = lIconInfo.GetPositionLane();
            lrItem.mv3Position.x = lv4Position.x;
            lrItem.mv3Position.y = lv4Position.y;
            lrItem.mv3Position.z = lv4Position.z;
            lrItem.mv3Position.w = lv4Position.w;
            lrItem.mTargetJunctionId = 0;
            const s16 li16LandmarkIndex = lIconInfo.GetLandmarkIndexHalf();
            lrItem.mTargetLandmarkIndex = static_cast<u16>(li16LandmarkIndex);
            lActiveLandmarks.maLandmarkIndices[liCheckpoint - 1] =
                BrnGameState::LandmarkIndex(li16LandmarkIndex);
        }

        mpGuiCache->HandleSetActiveLandmarksEvent(&lActiveLandmarks);
        lSetTrackerEvent.miNumTrackedItems = liNumTrackedItems;
        GuiTracker* lpTracker = mpGuiCache->GetGuiTracker();
        CGS_ASSERT(lpTracker != 0, "Invalid tracker pointer");
        lpTracker->RecEvent(reinterpret_cast<const CgsModule::Event*>(&lSetTrackerEvent),
                            lSetTrackerEvent.GetEventType(),
                            static_cast<s32>(sizeof(lSetTrackerEvent)));
        mbWaitingForRoute = true;
    }

    void OnlineSelectRoute::UpdateCursor()
    {
        // ARTIST reserves a 0x1330-byte frame; DecFIGS UpdateCursor constructs
        // 0x121 Vector2 entries here, not the 576-byte slice inferred by Hex-Rays.
        Vector2 lav2IconPositions[289];
        s32 liNumIcons = 0;
        mpIconManager->GetSatNavIconPositions(lav2IconPositions, &liNumIcons);
        const GuiCursor::SnapResults lSnap =
            mCursor.UpdateToSnapLocations(lav2IconPositions, static_cast<u32>(liNumIcons), false);

        if (lSnap.luLockedIndex == GuiCursor::SnapResults::KI_INVALID_LOCK_INDEX || liNumIcons <= 0)
            return;

        const u32 luEventId = mpIconManager->GetEventIDAtIndex(
            static_cast<s32>(lSnap.luLockedIndex));
        if (meSubState == E_SUBSTATE_SELECTING_POINT)
        {
            if (mSelectRoutes.GetCurrentMenuItemType() != SelectRoutes::E_MENU_ITEM_TYPE_START_POINT)
            {
                const BrnGameState::LandmarkIndex lCurrent =
                    mSelectRoutes.GetCurrentlySelectedCheckpointLandmark(
                        mSelectRoutes.GetCurrentRound());
                if (static_cast<u32>(lCurrent) != luEventId)
                {
                    mSelectRoutes.SetCurrentlySelectedCheckpointLandmark(
                        mSelectRoutes.GetCurrentRound(),
                        static_cast<BrnGameState::LandmarkIndex>(luEventId));
                    SetActiveLandmarks();
                    PostAudio(mpStateInterface, KAC_AUDIO_MAP_SCROLL_END);
                }
            }
            else
            {
                const SatNavEventDisplayInfo* lpEventStart =
                    mpGuiCache->GetProfileEventDisplayInfo(luEventId);
                if (lpEventStart->muLightTriggerId !=
                    mSelectRoutes.GetStartLightTriggerID(mSelectRoutes.GetCurrentRound()))
                {
                    mSelectRoutes.SetStartLightTriggerID(mSelectRoutes.GetCurrentRound(),
                                                         lpEventStart->muLightTriggerId);
                    SetActiveLandmarks();
                    PostAudio(mpStateInterface, KAC_AUDIO_MAP_SCROLL_END);
                }
            }
        }
        else if (meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT &&
                 muCurrentPresetEventID != luEventId)
        {
            muCurrentPresetEventID = luEventId;
            DisplayEventName();
            SetActiveLandmarks();
            PostAudio(mpStateInterface, KAC_AUDIO_MAP_SCROLL_END);
        }
    }

    void OnlineSelectRoute::UpdateIconManager()
    {
        if (mpGuiCache == 0 || mpIconManager == 0)
            return;

        mpIconManager->meIconFilterMode = MapIconManager::E_ICONFILTER_ALL;
        mpIconManager->mbIsDisplayingEventInfo = false;
        if (meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT)
        {
            const PresetEvent* lpPreset = FindPresetEventById(mpGuiCache, muCurrentPresetEventID);
            mpIconManager->miSelectedCheckpoint = 0;
            mpIconManager->mSelectedLightTriggerID = lpPreset->GetPositionLookupId();
        }
        else
        {
            const s32 liSelectedCheckpoint = mSelectRoutes.GetCurrentlySelectedCheckpointIndex();
            mpIconManager->miSelectedCheckpoint = liSelectedCheckpoint < 0 ? 0 : liSelectedCheckpoint;
            mpIconManager->mSelectedLightTriggerID =
                mSelectRoutes.GetStartLightTriggerID(mSelectRoutes.GetCurrentRound());
        }
        mpIconManager->miNumUsedIcons = 0;
        mpIconManager->Update();

        if ((meSubState == E_SUBSTATE_SELECTING_POINT ||
             meSubState == E_SUBSTATE_SELECTING_PRESET_EVENT) &&
            !mMainMapComponent.IsZooming())
        {
            UpdateCursor();
        }
    }

    void OnlineSelectRoute::HandleControllerAxis(const CgsModule::Event* lpEvent)
    {
        if ((meSubState != E_SUBSTATE_SELECTING_POINT &&
             meSubState != E_SUBSTATE_SELECTING_PRESET_EVENT) ||
            mMainMapComponent.IsZooming())
        {
            return;
        }

        CGS_ASSERT(lpEvent != 0,
                   "Invalid event in SelectRoutes::HandleControllerAxis");
        const ControllerAxisPayload* lpAxis = static_cast<const ControllerAxisPayload*>(lpEvent);
        if (lpAxis->miAxis == 0 || lpAxis->miAxis == 2)
        {
            mCursor.SetDelta(lpAxis->mfXAxis, lpAxis->mfYAxis, mpGuiCache->GetTime());
        }
    }

    void OnlineSelectRoute::UpdateAfterToggleChange(SelectRoutes::EChangedItem leChangedItem)
    {
        switch (leChangedItem)
        {
        case SelectRoutes::E_CHANGED_ITEM_LANDMARK:
            SetupPreSetRaceToggle();
            SetActiveLandmarks();
            return;

        case SelectRoutes::E_CHANGED_ITEM_CHECKPOINT:
            SetupHelpBar(true);
            SetupPreSetRaceToggle();
            SetActiveLandmarks();
            return;

        case SelectRoutes::E_CHANGED_ITEM_EDIT_NEW_CHECKPOINT:
            mbEditingNewCheckpoint = true;
            break;

        case SelectRoutes::E_CHANGED_ITEM_EDIT_CHECKPOINT:
            break;

        case SelectRoutes::E_CHANGED_ITEM_FINISHED_EDIT:
            if (mbRouteEdited)
            {
                // ARTIST OutputGuiEvent<GuiNetworkCustomRouteCreated> @82493F38:
                // {8,286,12,<checkpoint count,round>} on channel 40, twenty bytes.
                GuiNetworkCustomRouteCreated lCreatedRoute;
                const s32 laiRouteParams[2] =
                {
                    mSelectRoutes.GetNumberCheckpoints(mSelectRoutes.GetCurrentRound()),
                    mSelectRoutes.GetCurrentRound()
                };
                CGS_STATIC_ASSERT((sizeof(lCreatedRoute.maData) == sizeof(laiRouteParams)),
                              "custom-route-created payload");
                std::memcpy(lCreatedRoute.maData, laiRouteParams, sizeof(laiRouteParams));
                CgsGui::GuiEventWrapper<GuiNetworkCustomRouteCreated, 40> lWrapper(lCreatedRoute);
                CGS_STATIC_ASSERT((sizeof(lWrapper) == 20), "custom-route-created wrapper");
                mpStateInterface->GetOutputEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lWrapper), 40,
                    static_cast<s32>(sizeof(lWrapper)));
            }
            ShowMainMenu();
            return;

        default:
            return;
        }

        u32 lauIconsToIgnore[GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE];
        s32 liNumIconsToIgnore = 0;
        if (mSelectRoutes.GetCurrentlySelectedCheckpointIndex() == 0)
        {
            // Start-point editing hides starts too close to the first checkpoint.
            GuiEventUpdateSatNav::SatNavIconInfo lFirstCheckpointInfo;
            mpGuiCache->GetLandmarkInfoFromIndex(
                mSelectRoutes.GetCheckpointLandmark(mSelectRoutes.GetCurrentRound(), 1),
                &lFirstCheckpointInfo);

            if (mpGuiCache->GetNumEventStarts() != 0)
            {
                u32 luEventStart = 0;
                do
                {
                    const SatNavEventDisplayInfo* lpEventStart = mpGuiCache->GetEventStart(luEventStart);
                    CGS_ASSERT(lpEventStart, "lpEventStart");
                    const f32 lfExclusionX = lpEventStart->mv3Position.x - lFirstCheckpointInfo.GetPositionLane().x;
                    const f32 lfExclusionY = lpEventStart->mv3Position.y - lFirstCheckpointInfo.GetPositionLane().y;
                    const f32 lfExclusionZ = lpEventStart->mv3Position.z - lFirstCheckpointInfo.GetPositionLane().z;
                    if (std::sqrt(lfExclusionX * lfExclusionX + lfExclusionY * lfExclusionY +
                        lfExclusionZ * lfExclusionZ) < 200.0f)
                    {
                        lauIconsToIgnore[liNumIconsToIgnore++] = lpEventStart->muEventInstanceId;
                        CGS_ASSERT(liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE,
                            "liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE");
                    }
                    if (liNumIconsToIgnore == GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE)
                        break;
                    ++luEventStart;
                }
                while (luEventStart < mpGuiCache->GetNumEventStarts());
            }

            mpGuiCache->GetMapIconManager()->SetUseEventIcons(
                GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_EVENT_STARTS,
                mpStateInterface, 0.5f, lauIconsToIgnore, liNumIconsToIgnore);
            muSavedCheckpointId = mSelectRoutes.GetStartLightTriggerID(mSelectRoutes.GetCurrentRound());
        }
        else
        {
            const SelectRoutes::EMenuItemType leMenuType = mSelectRoutes.GetCurrentMenuItemType();
            s32 liPreviousCheckpointOffset = 1;
            if (leMenuType == SelectRoutes::E_MENU_ITEM_TYPE_FINISH_POINT &&
                mSelectRoutes.GetNumberCheckpoints(mSelectRoutes.GetCurrentRound()) < SelectRoutes::KI_MAX_CHECKPOINTS)
            {
                liPreviousCheckpointOffset = 2;
            }

            if (mSelectRoutes.GetCurrentlySelectedCheckpointIndex() > liPreviousCheckpointOffset)
            {
                lauIconsToIgnore[liNumIconsToIgnore++] = static_cast<u32>(static_cast<s32>(
                    mSelectRoutes.GetCheckpointLandmark(
                        mSelectRoutes.GetCurrentRound(),
                        mSelectRoutes.GetCurrentlySelectedCheckpointIndex() - liPreviousCheckpointOffset)));
                CGS_ASSERT(liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE,
                    "liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE");
            }

            bool lbIgnoreNextCheckpoint = true;
            if (mpGuiCache->GetOnlineGameMode() ==
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN)
            {
                if (mSelectRoutes.GetCurrentlySelectedCheckpointIndex() >=
                    mSelectRoutes.GetNumberCheckpoints(mSelectRoutes.GetCurrentRound()) - 1)
                {
                    lbIgnoreNextCheckpoint = false;
                }
            }
            else if (leMenuType == SelectRoutes::E_MENU_ITEM_TYPE_FINISH_POINT)
            {
                lbIgnoreNextCheckpoint = false;
            }
            if (lbIgnoreNextCheckpoint)
            {
                lauIconsToIgnore[liNumIconsToIgnore++] = static_cast<u32>(static_cast<s32>(
                    mSelectRoutes.GetCheckpointLandmark(
                        mSelectRoutes.GetCurrentRound(),
                        mSelectRoutes.GetCurrentlySelectedCheckpointIndex() + 1)));
                CGS_ASSERT(liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE,
                    "liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE");
            }

            if (leMenuType == SelectRoutes::E_MENU_ITEM_TYPE_FINISH_POINT)
            {
                // Finish-point editing additionally hides finish points near the start.
                const SatNavEventDisplayInfo* lpEventStart = mpGuiCache->GetPresetEventDisplayInfo(
                    mSelectRoutes.GetStartLightTriggerID(mSelectRoutes.GetCurrentRound()));
                CGS_ASSERT(lpEventStart, "lpEventStart");
                if (static_cast<s32>(mpGuiCache->GetNumOnlineFinishPoints()) > 0)
                {
                    s32 liFinishPoint = 0;
                    do
                    {
                        GuiEventUpdateSatNav::SatNavIconInfo lFinishInfo;
                        mpGuiCache->GetOnlineFinishPoint(liFinishPoint, &lFinishInfo);
                        const f32 lfExclusionX = (lFinishInfo.GetPositionLane()).x - (lpEventStart->mv3Position).x;
                        const f32 lfExclusionY = (lFinishInfo.GetPositionLane()).y - (lpEventStart->mv3Position).y;
                        const f32 lfExclusionZ = (lFinishInfo.GetPositionLane()).z - (lpEventStart->mv3Position).z;
                        if (std::sqrt(lfExclusionX * lfExclusionX + lfExclusionY * lfExclusionY +
                        lfExclusionZ * lfExclusionZ) < 200.0f)
                        {
                            lauIconsToIgnore[liNumIconsToIgnore++] = static_cast<u32>(static_cast<s32>(lFinishInfo.GetLandmarkIndexHalf()));
                            CGS_ASSERT(liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE,
                                "liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE");
                        }
                        if (liNumIconsToIgnore == GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE)
                            break;
                        ++liFinishPoint;
                    }
                    while (liFinishPoint < static_cast<s32>(mpGuiCache->GetNumOnlineFinishPoints()));
                }
            }
            else if (mSelectRoutes.GetCurrentlySelectedCheckpointIndex() == liPreviousCheckpointOffset)
            {
                // The first checkpoint additionally excludes every landmark near the start.
                const SatNavEventDisplayInfo* lpEventStart = mpGuiCache->GetPresetEventDisplayInfo(
                    mSelectRoutes.GetStartLightTriggerID(mSelectRoutes.GetCurrentRound()));
                CGS_ASSERT(lpEventStart, "lpEventStart");
                if (mpGuiCache->GetWorldDataController()->GetTotalNumberOfLandmarks() > 0)
                {
                    s32 liLandmark = 0;
                    do
                    {
                        GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
                        mpGuiCache->GetLandmarkInfoAtPositionInList(liLandmark, &lLandmarkInfo);
                        const f32 lfExclusionX = (lLandmarkInfo.GetPositionLane()).x - (lpEventStart->mv3Position).x;
                        const f32 lfExclusionY = (lLandmarkInfo.GetPositionLane()).y - (lpEventStart->mv3Position).y;
                        const f32 lfExclusionZ = (lLandmarkInfo.GetPositionLane()).z - (lpEventStart->mv3Position).z;
                        if (std::sqrt(lfExclusionX * lfExclusionX + lfExclusionY * lfExclusionY +
                        lfExclusionZ * lfExclusionZ) < 200.0f)
                        {
                            lauIconsToIgnore[liNumIconsToIgnore++] = static_cast<u32>(static_cast<s32>(lLandmarkInfo.GetLandmarkIndexHalf()));
                            CGS_ASSERT(liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE,
                                "liNumIconsToIgnore <= GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE");
                        }
                        if (liNumIconsToIgnore == GuiEventDrawEventIcons::KI_MAX_ICONS_TO_IGNORE)
                            break;
                        ++liLandmark;
                    }
                    while (liLandmark < mpGuiCache->GetWorldDataController()->GetTotalNumberOfLandmarks());
                }
            }

            mpGuiCache->GetMapIconManager()->SetUseEventIcons(
                leMenuType == SelectRoutes::E_MENU_ITEM_TYPE_FINISH_POINT
                    ? GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_FINISH_POINTS
                    : GuiEventDrawEventIcons::E_ICON_DISPLAY_TYPE_ONLINE_CHECKPOINTS,
                mpStateInterface, 0.5f, lauIconsToIgnore, liNumIconsToIgnore);
            muSavedCheckpointId = static_cast<u32>(static_cast<s32>(
                mSelectRoutes.GetCurrentlySelectedCheckpointLandmark(mSelectRoutes.GetCurrentRound())));
        }

        meSubState = E_SUBSTATE_SELECTING_POINT;
        const f32 lfZoomFactor = CalculateZoomFactor();
        mMainMapComponent.SetZoom(MainMapComponent::E_ZOOMFACTOR_CUSTOM, lfZoomFactor, false);
        SetupHelpBar(true);
        SetActiveLandmarks();
        mCheckpointMenuAnimator.AddOutputAptViewState("apt_Transition", "inactive", false);
        mCursorAnimator.AddOutputAptViewState("apt_Transition", "active", false);
        mEventNameAnimator.AddOutputAptViewState("apt_Transition", "inactive", false);
        mMainMapComponent.mbIsActive = true;
        if (mpIconManager)
            mpIconManager->mbIsActive = true;
    }

    // Publish the current route length and the minimum as the two small-distance strings
    // "ONLINE_ROUTE_DISTANCE" / "ONLINE_ROUTE_MINIMUM_DISTANCE", then post the "OnRouteShort"
    // overlay request that shows them.
    void OnlineSelectRoute::DisplayTooShortRouteMessage()
    {
        char lacDistance[64];

        CgsCore::SPrintf(lacDistance, sizeof(lacDistance), "%f", mpGuiCache->GetGuiTracker()->GetRouteDistance());
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "ONLINE_ROUTE_DISTANCE", lacDistance, CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);

        CgsCore::SPrintf(lacDistance, sizeof(lacDistance), "%f", static_cast<double>(KF_MINIMUM_ROUTE_DISTANCE));
        mpStateInterface->GetLanguageManager()->FormatAndAddText(
            "ONLINE_ROUTE_MINIMUM_DISTANCE", lacDistance, CgsLanguage::LanguageManager::E_FORMAT_SMALL_DISTANCE);

        GuiOverlayRequestWire lWire;
        lWire.mRequest.Construct("OnRouteShort");
        lWire.mRequest.AddMessageParam(2, "ONLINE_ROUTE_DISTANCE");
        lWire.mRequest.AddMessageParam(2, "ONLINE_ROUTE_MINIMUM_DISTANCE");
        mpStateInterface->GetOutputEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lWire), KI_CHANNEL_GUI_OUT,
            static_cast<s32>(sizeof(GuiOverlayRequestWire)));
    }
}
