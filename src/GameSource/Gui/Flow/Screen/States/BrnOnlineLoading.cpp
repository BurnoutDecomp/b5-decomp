// GameSource/Gui/Flow/Screen/States/BrnOnlineLoading.cpp
//
// BrnGui::OnlineLoading -- the online loading screen (ON_LOAD). The thirteen bodies, read off
// the console asm:
//
//   OnEnter / OnLeave / Update          the lifecycle; entry starts the online bed music,
//                                       exit hides the map, files the received game options
//                                       and asks for a save
//   CheckForCompletedLoads              ON_LOAD + the route-info package, then the components,
//                                       then the route info
//   SetupGameModeInfo / SetupPlayerList / UpdatePlayerList
//   HandleGuiCacheEvent / HandleAptTriggers / HandlePlayerLobbyListEvent /
//   HandleGameParamsChangedEvent / HandleInviteFailed
//
// Once the screen has been up for KF_LOADING_SCREEN_UPDATE_TIMEOUT it tells the game it is
// done (GUI 135) and waits for the game mode to start. Out-queue records are the console's
// own wire records, posted on channel 40 through GetOutputEventQueue()->AddEvent with host
// sizeof sizes.
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineLoading.cpp"
#include "GameSource/Gui/Flow/Screen/States/BrnOnlineLoading.h"

#include <cstring>                                                        // std::memcpy / std::memset
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/CgsStrStream.h"              // CgsDev::StrStreamBase (unexpected-event log)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"               // CgsGui::GuiEventTimeInfo
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"  // StateInterface
#include "GameShared/GameClasses/Gui/View/AptInterface/CgsAptCommunicator.h" // GuiEventAptTriggerPayload
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // the state in-queue / AddEvent
#include "GameSource/Gui/BrnGuiCache.h"                                   // BrnGui::GuiCache
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                           // GuiAudioTriggerEvent / GuiOverlayRequest
#include "GameSource/Gui/BrnGuiOptionsDataProfile.h"                      // OptionsDataProfile::AddReceivedOnlineGameOptions
#include "GameSource/Gui/BrnGuiShared.h"                                  // gGuiResourceIdentifier
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"      // InGamePlayerStatusData
#include "GameSource/Network/SharedIO/BrnNetworkModuleOnlineLobbyPlayerStatusInterface.h" // LobbyPlayerStatusData

namespace BrnGui
{
    // ================================================================================
    //  Class statics (values read from the image)
    // ================================================================================

    // The events the screen observes: apt ONLOAD, the frame tick, disconnected, gui cache,
    // the game params, 93 (the round to show), 133 (invite failed), the lobby roster, 137
    // (advance), the satnav record and 91.
    const s32 OnlineLoading::maiEventToObserve[11] = { 21, 26, 44, 64, 257, 93, 133, 244, 137, 213, 91 };
    const s32 OnlineLoading::miNumEventsObserved   = 11;

    // ON_LOAD and the route-info package.
    const CgsGui::sResourceTuple OnlineLoading::maResourceTuplesToLoad[] =
    {
        { 168, CgsGui::E_GUI_RESOURCETYPE_APT },
        { 191, CgsGui::E_GUI_RESOURCETYPE_APT }
    };
    const s32 OnlineLoading::miNumResourcesToLoad = 2;

    const f32 OnlineLoading::KF_LOADING_SCREEN_UPDATE_TIMEOUT = 15.0f;

    const char OnlineLoading::KAC_PLAYER_TEMPLATE[13]          = "Player_%i_mc";
    const char OnlineLoading::KAC_ROUTE_INFO_NAME[10]          = "RouteInfo";
    const char OnlineLoading::KAC_ANIMATION_COMPONENT_NAME[15] = "PlayersHeading";

    const char* const OnlineLoading::KAPC_ANIMATION_STATES[2] = { "visible", "invisible" };

    namespace
    {
        typedef CgsModule::VariableEventQueue<18432, 16> StateInputQueue;   // mpInGuiEventQueue's real type

        const s32 KI_CHANNEL_GUI_OUT = 40;

        const s32 KI_EVENT_APT_TRIGGER          = 21;
        const s32 KI_EVENT_FRAME_TICK           = 26;
        const s32 KI_EVENT_NETWORK_DISCONNECTED = 44;
        const s32 KI_EVENT_GUI_CACHE            = 64;
        const s32 KI_EVENT_91                   = 91;
        const s32 KI_EVENT_ROUND_TO_DISPLAY     = 93;
        const s32 KI_EVENT_INVITE_FAILED        = 133;
        const s32 KI_EVENT_ADVANCE              = 137;
        const s32 KI_EVENT_SHOW_HIDE_SATNAV     = 213;   // observed, no arm
        const s32 KI_EVENT_LOBBY_PLAYER_LIST    = 244;
        const s32 KI_EVENT_GAME_PARAMS          = 257;

        // The movie CheckForCompletedLoads plays: the screen's own package.
        const u32 KU_LOADING_MOVIE_RESOURCE = 168;
        const s32 KI_APT_MOVIE_LEVEL        = 3;
        const char* const KPC_EMPTY_STRING  = "";

        // The route info shows the big map.
        const s32 KI_ROUTE_INFO_MAP_TYPE = 2;

        // The player-name buffer OnEnter formats into (SPrintf is handed one byte less and the
        // last byte is cleared up front).
        const u32 KU_PLAYER_NAME_LENGTH = 32;

        const s32 KI_INVITE_FAIL_REASON_SAME_GAME = 1;   // EInviteFailReason: already in that game

        // The in-game record values the player rows show as connected camera / talking.
        const s32 KI_CAMERA_STATUS_SHOWN_CONNECTED = 3;
        const s32 KI_HEADSET_STATUS_TALKING        = 2;



        // ---- in-queue payload views (the queue hands out the header-stripped payload) ----
        struct GuiCachePayload : public CgsModule::Event
        {
            GuiCache* mpCache;   // +0x00
        };

        // Event 93: the round the route info should show (+0x08).
        struct RoundToDisplayPayload : public CgsModule::Event
        {
            s32 miPad00;          // +0x00
            s32 miPad04;          // +0x04
            s32 miRoundIndex;     // +0x08
        };

        struct InviteFailedPayload : public CgsModule::Event
        {
            s32 meInviteFailedReason;   // +0x00
        };

        // Event 244: the eight lobby rows then the player count (+0x1C0).


        // ---- out-queue wire records ------------------------------------------------------
        // { 1, N, 12, <one byte> }, channel 40, 16 bytes: the one-byte command records. The
        // console never writes the payload byte.
        template <s32 TI_EVENT_ID>
        struct GuiCommandWire16 : public CgsGui::GuiEvent<TI_EVENT_ID>
        {
            u8 mu8Payload;   // +0x0C

            GuiCommandWire16()
                : mu8Payload(0)
            {
            }
        };

        // { 2, 536, 12, <two bytes> }, channel 40, 16 bytes: the ticker-clear record.
        struct GuiTickerClearWire : public CgsGui::GuiEvent<536>
        {
            u8 mau8Payload[2];   // +0x0C

            GuiTickerClearWire(u8 lu8First, u8 lu8Second)
            {
                mau8Payload[0] = lu8First;
                mau8Payload[1] = lu8Second;
            }
            GuiTickerClearWire() {}
        };

        // { 1, 356, 12, <the byte> }, channel 40, 16 bytes: the autosave request.
        struct GuiAutosaveRequestWire : public CgsGui::GuiEvent<356>
        {
            u8 mu8Payload;   // +0x0C

            GuiAutosaveRequestWire() : mu8Payload(0) {}
        };

        // { 288, 184, 16, <pad>, the 288-byte request }, channel 40, 304 bytes.
        struct GuiOverlayRequestWire
        {
            GuiOverlayRequest mRequest;   // the OutputGuiEvent payload

            GuiOverlayRequestWire()
            {
            }
        };

        // The channel-40 records OutputGuiEvent's GuiEventOut<T> builds around these payloads.
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiCommandWire16<92> >) == 16), "command record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiTickerClearWire>) == 16), "ticker-clear record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiAutosaveRequestWire>) == 16), "autosave record is 16 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiAudioTriggerEvent>) == 112), "audio trigger record is 112 bytes");
        CGS_STATIC_ASSERT((sizeof(CgsGui::GuiEventOut<GuiOverlayRequest>) == 304), "overlay request record is 304 bytes");
        CGS_STATIC_ASSERT((sizeof(GuiEventNetworkLobbyPlayerList) == 0x1C8), "the roster is 456 bytes");

        // Publish one payload through OutputGuiEvent (channel 40), which takes the event by
        // non-const reference: the boxed copy is the record ARTIST builds inline.
        template <typename TEvent>
        void PostWire(CgsGui::StateInterface* lpStateInterface, const TEvent& lrEvent)
        {
            TEvent lEvent = lrEvent;
            lpStateInterface->OutputGuiEvent(lEvent);
        }
    }

    // ================================================================================
    //  OnEnter
    // ================================================================================
    void OnlineLoading::OnEnter()
    {
        mpStateInterface->RegisterForEvents(maiEventToObserve, miNumEventsObserved);

        miNumFilesToLoad = 0;
        mRouteInfoDisplay.Construct(KAC_ROUTE_INFO_NAME, KI_ROUTE_INFO_MAP_TYPE, mpStateInterface, 0);
        mPlayerListHeadingComponent.Construct(KAC_ANIMATION_COMPONENT_NAME, mpStateInterface, 0);
        mTitleText.Construct("Title_mc", mpStateInterface, 0);
        mChevronsColourComponent.Construct("ChevronsColour", mpStateInterface, 0);

        char lacPlayerName[KU_PLAYER_NAME_LENGTH];
        lacPlayerName[KU_PLAYER_NAME_LENGTH - 1] = 0;
        for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
        {
            CgsCore::SPrintf(lacPlayerName, KU_PLAYER_NAME_LENGTH - 1, KAC_PLAYER_TEMPLATE, liPlayer);
            maPlayer[liPlayer].Construct(lacPlayerName, mpStateInterface, 0);
        }

        meSubState = E_SUBSTATE_LOADING_SCREEN;
        mCurrentGameParams.Construct();
        miCurrentRoundDisplayed = 0;
        mpGuiCache              = 0;
        miNumLoadedComponents   = 0;
        mbDisconnected          = false;
        mfUpdateTimer           = 0.0f;
        std::memset(&mLobbyPlayerInfoList, 0, sizeof(mLobbyPlayerInfoList));

        // Start the online bed music.
        GuiAudioTriggerEvent lBedMusic;
        lBedMusic.Construct(0, KPC_EMPTY_STRING, "OnlineBedLoop", KPC_EMPTY_STRING);
        mpStateInterface->OutputGuiEvent(lBedMusic);   // {100, 457, 12} + 100 on channel 40
    }

    // ================================================================================
    //  OnLeave
    // ================================================================================
    void OnlineLoading::OnLeave()
    {
        // The console inlines StateInterface::PlayAptMovie here: the empty name at level 3
        // clears the level.
        mpStateInterface->PlayAptMovie(KPC_EMPTY_STRING, KI_APT_MOVIE_LEVEL);

        mpStateInterface->UnRegisterForEvents(maiEventToObserve, miNumEventsObserved);

        PostWire(mpStateInterface, GuiTickerClearWire(1, 0));

        miNumLoadedComponents = 0;
        mRouteInfoDisplay.Destruct();

        // Hide the main map on both the view and the internal channels.
        GuiEventShowHideSatNav lHideSatNav;
        lHideSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, false, 0.0f);
        mpStateInterface->OutputViewState(lHideSatNav);
        mpStateInterface->OutputInternalState(lHideSatNav);

        PostWire(mpStateInterface, GuiCommandWire16<480>());

        // Leaving from the first round files the game's options among the received ones,
        // and asks for a save.
        if (miCurrentRoundDisplayed == 0)
        {
            CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 354);
            mpGuiCache->GetMenuOptionsData()->AddReceivedOnlineGameOptions(mpGuiCache,
                                                                             &mCurrentGameParams);
            PostWire(mpStateInterface, GuiAutosaveRequestWire());
        }
    }

    // ================================================================================
    //  Update
    // ================================================================================
    void OnlineLoading::Update()
    {


        const CgsModule::Event* lpEvent;
        s32 liSize;
        for (s32 liEventId = mpInGuiEventQueue->GetFirstEvent(&lpEvent, &liSize);
             lpEvent != 0;
             liEventId = mpInGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liSize))
        {
            switch (liEventId)
            {
            case KI_EVENT_APT_TRIGGER:
                HandleAptTriggers(lpEvent);
                break;

            case KI_EVENT_FRAME_TICK:
                // The loading screen's dwell only counts once the players are up.
                if (meSubState == E_SUBSTATE_MAIN)
                {
                    mfUpdateTimer = mfUpdateTimer
                        + reinterpret_cast<const CgsGui::GuiEventTimeInfo*>(lpEvent)->GetTimeStep();
                }
                break;

            case KI_EVENT_NETWORK_DISCONNECTED:
                PostWire(mpStateInterface, GuiCommandWire16<135>());
                mbDisconnected = true;
                break;

            case KI_EVENT_GUI_CACHE:
                HandleGuiCacheEvent(lpEvent);
                CheckForCompletedLoads();
                break;

            case KI_EVENT_91:
                if (meSubState > E_SUBSTATE_LOADING_ROUTE_INFO)
                {
                    PostWire(mpStateInterface, GuiCommandWire16<92>());
                }
                break;

            case KI_EVENT_ROUND_TO_DISPLAY:
            {
                const s32 liRound = static_cast<const RoundToDisplayPayload*>(lpEvent)->miRoundIndex;
                miCurrentRoundDisplayed = liRound;
                if (mRouteInfoDisplay.IsComponentLoaded())
                {
                    s32 liShownRound = liRound;
                    if (!(liShownRound < mCurrentGameParams.miNumRounds - 1))
                    {
                        liShownRound = mCurrentGameParams.miNumRounds - 1;
                    }
                    if (!(liShownRound > 0))
                    {
                        liShownRound = 0;
                    }
                    mRouteInfoDisplay.SetInfo(liShownRound, &mCurrentGameParams);
                }
                break;
            }

            case KI_EVENT_ADVANCE:
                if (mbDisconnected)
                {
                    CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 230);
                    mpGuiCache->SetDoDisconnectPopup(lpEvent);
                    SendStateEvent("DISCONNECT");
                }
                else
                {
                    SendStateEvent("ADVANCE");
                }
                break;

            case KI_EVENT_INVITE_FAILED:
                HandleInviteFailed(lpEvent);
                break;

            case KI_EVENT_SHOW_HIDE_SATNAV:
                break;

            case KI_EVENT_LOBBY_PLAYER_LIST:
                HandlePlayerLobbyListEvent(lpEvent);
                break;

            case KI_EVENT_GAME_PARAMS:
                HandleGameParamsChangedEvent(lpEvent);
                break;

            default:
                if (CgsDev::Message::gxMessageFilterFlags & 1)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "Unexpected event received : " << liEventId
                        << " in "
                        << "..\\..\\..\\GameSource\\Gui/Flow/Screen/States/BrnOnlineLoading.cpp"
                        << " at line " << 284 << "\n";
                }
                break;
            }
        }

        mRouteInfoDisplay.Update(mpInGuiEventQueue);

        mpInGuiEventQueue->Clear();

        // The players have been up long enough: tell the game, then wait for the mode.
        if (mfUpdateTimer > KF_LOADING_SCREEN_UPDATE_TIMEOUT && meSubState == E_SUBSTATE_MAIN)
        {
            PostWire(mpStateInterface, GuiCommandWire16<135>());
            meSubState = E_SUBSTATE_WAIT_GAME_MODE_START;
        }
    }

    // ================================================================================
    //  CheckForCompletedLoads -- the packages, then the screen's components (players, map),
    //  then the route info.
    // ================================================================================
    void OnlineLoading::CheckForCompletedLoads()
    {
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 374);

        if (meSubState == E_SUBSTATE_LOADING_SCREEN)
        {
            if (mpGuiCache->EnsureResourcesAreLoaded(maResourceTuplesToLoad,
                                                     static_cast<u32>(miNumResourcesToLoad)))
            {
                PostWire(mpStateInterface, GuiCommandWire16<479>());
                mpStateInterface->PlayAptMovie(gGuiResourceIdentifier[KU_LOADING_MOVIE_RESOURCE],
                                               KI_APT_MOVIE_LEVEL);
                meSubState = E_SUBSTATE_LOADING_COMPONENTS;
            }
            return;
        }

        if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN)
            && meSubState == E_SUBSTATE_LOADING_COMPONENTS)
        {
            meSubState = E_SUBSTATE_MAIN;
            SetupPlayerList();
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mPlayerListHeadingComponent.AddOutputAptViewState("apt_Transition",
                                                              KAPC_ANIMATION_STATES[1], false);
            mRouteInfoDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            meSubState = E_SUBSTATE_LOADING_ROUTE_INFO;

            PostWire(mpStateInterface, GuiCommandWire16<260>());

            // Bring the main map up on both the view and the internal channels.
            GuiEventShowHideSatNav lShowSatNav;
            lShowSatNav.Construct(GuiEventShowHideSatNav::E_MAPTYPE_MAIN, true, 0.0f);
            mpStateInterface->OutputViewState(lShowSatNav);
            mpStateInterface->OutputInternalState(lShowSatNav);

            SetupGameModeInfo();
            return;
        }

        if (mpGuiCache != 0 && mpGuiCache->AreAllAptComponentsInitialised(E_GUIFLOW_SCREEN)
            && meSubState == E_SUBSTATE_LOADING_ROUTE_INFO)
        {
            mRouteInfoDisplay.miNumComponentsLoaded = GuiNetworkRouteInfo::KI_NUM_COMPONENTS_TO_LOAD;
            mRouteInfoDisplay.SetState(GuiNetworkRouteInfo::E_STATE_VISIBLE);
            meSubState = E_SUBSTATE_MAIN;

            PostWire(mpStateInterface, GuiCommandWire16<281>());
            PostWire(mpStateInterface, GuiCommandWire16<92>());
        }
    }

    // ================================================================================
    //  SetupGameModeInfo -- the title and chevron colour for the game mode.
    // ================================================================================
    void OnlineLoading::SetupGameModeInfo()
    {
        const GuiEventNetworkGameParams* lpParams =
            mpGuiCache->GetOnlineGameModeOptions();
        CGS_ASSERT(lpParams != 0, CGS_ASSERT_MESSAGE_AT("lpParams", CGS_ARTIST_FILE, 680 /* ARTIST 0x82485634 */));

        const char* lpacChevronsState = 0;

        switch (lpParams->meGameMode)
        {
        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
            mTitleText.SetText("$STANDBY_STUNTRUN_TEAM");
            lpacChevronsState = "StuntRun";
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
            mTitleText.SetText("$STANDBY_STUNTRUN_INDIVIDUAL");
            lpacChevronsState = "StuntRun";
            break;

        case BrnGameState::GameStateModuleIO::E_MODE_ONLINE_MODE_END:
            mTitleText.SetText("$STANDBY_STUNTRUN_COOP");
            lpacChevronsState = "StuntRun";
            break;

        default:
            mTitleText.SetText("$STANDBY");
            lpacChevronsState = "Default";
            break;
        }

        mChevronsColourComponent.AddOutputAptViewState("apt_Transition", lpacChevronsState,
                                                       false);
    }

    // ================================================================================
    //  SetupPlayerList -- every row hidden and cleared.
    // ================================================================================
    void OnlineLoading::SetupPlayerList()
    {
        CGS_ASSERT(mpGuiCache != 0, CGS_ASSERT_MESSAGE_AT("mpGuiCache", CGS_ARTIST_FILE, 737 /* ARTIST 0x82485754 */));

        for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
        {
            OnlineLoadingPlayer& lrPlayer = maPlayer[liPlayer];
            lrPlayer.Hide();
            lrPlayer.SetCameraConnected(false);
            lrPlayer.SetCrown(false);
            lrPlayer.SetGamertag(KPC_EMPTY_STRING);
            lrPlayer.SetLiveRevenge(0);
            lrPlayer.SetTeam(BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_NONE);
            lrPlayer.SetVOIPActive(false);
        }
    }

    // ================================================================================
    //  UpdatePlayerList -- refresh each occupied row's camera, team and VOIP icons from the
    //  in-game records (the free-burn lobby has no teams).
    // ================================================================================
    void OnlineLoading::UpdatePlayerList()
    {
        CGS_ASSERT_AT(mpGuiCache != 0, "mpGuiCache", CGS_ARTIST_FILE, 629);


        for (s32 liPlayerInfoIndex = 0; liPlayerInfoIndex < KI_MAX_PLAYERS; ++liPlayerInfoIndex)
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerInfo =
                mpGuiCache->GetOnlinePlayerInfoByIndex(liPlayerInfoIndex);
            CGS_ASSERT(lpPlayerInfo != 0, "lpPlayerInfo");

            if (lpPlayerInfo->mNetworkPlayerID == -1)
            {
                continue;
            }

            CGS_ASSERT(lpPlayerInfo->mNetworkPlayerID ==
                           mLobbyPlayerInfoList.maPlayerLobbyInfo[liPlayerInfoIndex].mPlayerID,
                       "lpPlayerInfo->mNetworkPlayerID == mLobbyPlayerInfoList.maPlayerLobbyInfo"
                       "[ liPlayerInfoIndex ].mPlayerID");
            CGS_ASSERT(mpGuiCache != 0, "mpGuiCache");

            BrnGameState::GameStateModuleIO::EPlayerTeam leTeam =
                BrnGameState::GameStateModuleIO::E_PLAYER_TEAM_NONE;
            if (mpGuiCache->GetGameMode() !=
                BrnGameState::GameStateModuleIO::E_MODE_ONLINE_FREE_BURN)
            {
                leTeam = static_cast<BrnGameState::GameStateModuleIO::EPlayerTeam>(
                    mLobbyPlayerInfoList.maPlayerLobbyInfo[liPlayerInfoIndex].mePlayerTeam);
            }

            OnlineLoadingPlayer& lrPlayer = maPlayer[liPlayerInfoIndex];
            lrPlayer.SetCameraConnected(lpPlayerInfo->meCameraStatus == KI_CAMERA_STATUS_SHOWN_CONNECTED);
            lrPlayer.SetTeam(leTeam);
            lrPlayer.SetVOIPActive(lpPlayerInfo->meVOIPStatus == KI_HEADSET_STATUS_TALKING);
        }
    }

    // ================================================================================
    //  HandleGuiCacheEvent -- adopt the first cache offered and register every component;
    //  each cache event also clears the ticker.
    // ================================================================================
    void OnlineLoading::HandleGuiCacheEvent(const CgsModule::Event* lpEvent)
    {
        const GuiCachePayload* lpPayload = static_cast<const GuiCachePayload*>(lpEvent);

        CGS_ASSERT(lpPayload->mpCache != 0, "Invalid cache in HandleGuiCacheEvent::Update");

        if (mpGuiCache == 0)
        {
            mpGuiCache = lpPayload->mpCache;
            mpGuiCache->ClearExpectedAptComponentList(E_GUIFLOW_SCREEN);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mTitleText.GetName());
            mRouteInfoDisplay.AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mPlayerListHeadingComponent.GetName());
            mpGuiCache->AppendExpectedAptComponent(E_GUIFLOW_SCREEN,
                                                   mChevronsColourComponent.GetName());
            for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
            {
                maPlayer[liPlayer].AppendExpectedAptComponent(E_GUIFLOW_SCREEN, mpGuiCache);
            }
        }

        PostWire(mpStateInterface, GuiTickerClearWire(0, 0));
    }

    // ================================================================================
    //  HandleAptTriggers -- hand an apt ONLOAD to the player rows until one claims it.
    // ================================================================================
    void OnlineLoading::HandleAptTriggers(const CgsModule::Event* lpEvent)
    {
        const CgsGui::GuiEventAptTriggerPayload* lpTrigger =
            reinterpret_cast<const CgsGui::GuiEventAptTriggerPayload*>(lpEvent);
        if (lpTrigger->meEventType != CgsGui::GuiEventAptTrigger::E_APT_EVENT_ONLOAD)
        {
            return;
        }

        for (s32 liPlayer = 0; liPlayer < KI_MAX_PLAYERS; ++liPlayer)
        {
            if (maPlayer[liPlayer].HandleLoadNotification(lpTrigger->mpacComponentName) == true)
            {
                break;
            }
        }
    }

    // ================================================================================
    //  HandlePlayerLobbyListEvent -- keep the roster; once the players are up, refresh them.
    // ================================================================================
    void OnlineLoading::HandlePlayerLobbyListEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, CGS_ASSERT_STREAM_MESSAGE_AT("Invalid event sent to OnlineLoading::HandlePlayerLobbyListEvent", CGS_ARTIST_FILE, 543));

        std::memcpy(&mLobbyPlayerInfoList, lpEvent, sizeof(mLobbyPlayerInfoList));

        if (meSubState != E_SUBSTATE_LOADING_SCREEN
            && meSubState != E_SUBSTATE_LOADING_COMPONENTS
            && meSubState != E_SUBSTATE_LOADING_ROUTE_INFO
            && mLobbyPlayerInfoList.miNumberOfPlayers > 0)
        {
            UpdatePlayerList();
        }
    }

    // ================================================================================
    //  HandleGameParamsChangedEvent -- keep the params; once the route info is up, show the
    //  current round (clamped to the game's rounds).
    // ================================================================================
    void OnlineLoading::HandleGameParamsChangedEvent(const CgsModule::Event* lpEvent)
    {
        CGS_ASSERT(lpEvent != 0, "Invalid event sent to OnlineLoading::HandleGameParamsChangedEvent");

        std::memcpy(&mCurrentGameParams, lpEvent, sizeof(mCurrentGameParams));

        if (mRouteInfoDisplay.IsComponentLoaded())
        {
            if (!(miCurrentRoundDisplayed < mCurrentGameParams.miNumRounds - 1))
            {
                miCurrentRoundDisplayed = mCurrentGameParams.miNumRounds - 1;
            }
            if (!(miCurrentRoundDisplayed > 0))
            {
                miCurrentRoundDisplayed = 0;
            }
            mRouteInfoDisplay.SetInfo(miCurrentRoundDisplayed, &mCurrentGameParams);
        }
    }

    // ================================================================================
    //  HandleInviteFailed -- an invite into the game the player is already in raises its overlay.
    // ================================================================================
    void OnlineLoading::HandleInviteFailed(const CgsModule::Event* lpInviteFailedEvent)
    {
        CGS_ASSERT(lpInviteFailedEvent != 0, CGS_ASSERT_MESSAGE_AT("lpInviteFailedEvent", __FILE__, 604 /* ARTIST 0x8249B644 */));

        const InviteFailedPayload* lpInviteFailed =
            static_cast<const InviteFailedPayload*>(lpInviteFailedEvent);
        if (lpInviteFailed->meInviteFailedReason == KI_INVITE_FAIL_REASON_SAME_GAME)
        {
            GuiOverlayRequestWire lInviteFailedOverlay;
            lInviteFailedOverlay.mRequest.Construct("CNInvSmGame");
            mpStateInterface->OutputGuiEvent(lInviteFailedOverlay.mRequest);
        }
    }
}

#undef CGS_ARTIST_FILE
