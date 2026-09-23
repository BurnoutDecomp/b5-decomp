// ===================================================================================
// BrnNetwork::StateManager -- part 2: log-in, launching, news/TOS download, the
// free-burn lobby helpers and the matchmaking / suspension completion callbacks.
//
// Every manager and component is reached through the owning BrnNetworkManager's
// accessors; every StateManager member by name.
//
// Network-out records come from BrnNetworkOutEventTypeDefs.h. The one game event posted
// here that BrnGameEvents.h does not carry yet (the online car-select signal) is spelled
// TU-local below with its console tag. FLAG: move it to BrnGameEvents.h.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Development/CgsStrStream.h"                              // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // CgsDev::DebugInterface
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                               // GuiEventNetworkConnected / InGame / InGameFailed / Launched
#include "GameShared/GameClasses/Gui/CgsGuiEventAddLocalisedTextPointer.h"                // CgsGui::GuiEventAddLocalisedTextPointer
#include "GameShared/GameClasses/Network/CgsNetworkVersionDisplay.h"                      // CgsNetwork::VersionDisplay
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                      // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                      // CgsNetwork::NetworkPlayer
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"               // CgsNetwork::HostMigrationManager
#include "GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h"                 // CgsNetwork::StartTimeManager
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"         // EComponents
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceServerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceHttp.h"
#include "GameShared/GameClasses/Network/Buddies/DirtySock/CgsBuddyManagerDirtySock.h"   // CgsNetwork::BuddyManagerBase::CancelInvites
#include "GameSource/GameState/BrnGameEvents.h"                                           // ChangePlayerCarEvent
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                    // EGameModeType / IsOnlineFreeBurnLobby
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                                     // GuiEventNetworkCustomMatchResults / NetworkConnect / ...
#include "GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h"                           // BrnGui::GuiEventNetworkCreateGame
#include "GameSource/Network/BrnNetworkModule.h"                                          // BrnNetworkModule
#include "GameSource/Network/BrnNetworkModuleIO.h"                                        // NetworkEventQueue
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                // NetworkOut* records
#include "GameSource/Network/BrnNetworkManager.h"                                         // accessors + the login / camera / buddy manager types
#include "GameSource/Network/BrnServerInterface.h"                                        // BrnServerInterface
#include "GameSource/Network/BrnNetworkGameParams.h"                                      // BrnNetwork::GameParams
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                                  // BrnNetwork::PlayerMenuData
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"           // GetTosDownloadBufferSize
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"
#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"
#include "GameSource/Network/Managers/BrnNetworkLaunchManager.h"
#include "GameSource/Network/Managers/BrnNetworkTeamSelectionManager.h"
#include "GameSource/Network/Managers/BrnNetworkInviteManager.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"          // GameStateToNetworkInterface
#include "pc/gcm/renderengine/pixelformat.h"                                              // renderengine::PIXELFORMAT_DXT1

#include <cstring>   // std::memcpy / std::strlen / std::strncpy

namespace BrnNetwork
{
    namespace
    {
        namespace GsmIO = BrnGameState::GameStateModuleIO;

        // CgsNetwork::K_INVALID_PLAYER_ID has no home in this tree yet (FLAG).
        const NetworkPlayerID KI_INVALID_PLAYER_ID = -1;

        // The compressed camera picture is a 160 x 120 DXT1 image: 160 * 120 / 2 bytes.
        const s32 KI_COMPRESSED_CAMERA_PIC_SIZE = 9600;

        // ---- game events (posted on the module's game event queue) ------------------------

        // OnlineCarSelectEvent: an empty signal (the console posts one byte).
        struct OnlineCarSelectEvent
        {
        };
        const s32 KI_GAME_EVENT_ONLINE_CAR_SELECT = 16;

        // ---- GUI event payload values ------------------------------------------------------
        // The committed GUI event types for these ids are opaque byte records; the values are
        // the reference enumerators of the payload word.

        // GuiEventNetworkLaunched::ELaunchedStatus.
        const s32 KI_LAUNCHED_STATUS_SUCCESS = 0;
        const s32 KI_LAUNCHED_STATUS_FAILED  = 6;

        // GuiEventNetworkInGameFailed::EInGameFailReason.
        const s32 KI_IN_GAME_FAIL_REASON_FULL                          = 0;
        const s32 KI_IN_GAME_FAIL_REASON_STARTED                       = 1;
        const s32 KI_IN_GAME_FAIL_REASON_UNKNOWN_GAME                  = 2;
        const s32 KI_IN_GAME_FAIL_REASON_CREATE_GAME_FOR_INVITE_FAILED = 3;
        const s32 KI_IN_GAME_FAIL_REASON_COUNT                         = 4;

        // ---- games-component error codes ---------------------------------------------------
        // FLAG: console values. The committed CgsNetwork::EServerInterfaceError numbers these
        // four lower (the console carries four more enumerators ahead of the games block), so
        // the enumerator names cannot be used until that enum is re-numbered.
        const s32 KI_GAMES_ERROR_UNKNOWN_GAME    = 28;
        const s32 KI_GAMES_ERROR_GAME_FULL       = 30;
        const s32 KI_GAMES_ERROR_ALREADY_STARTED = 32;
        const s32 KI_GAMES_ERROR_GAME_LOCKED     = 33;

        template <typename TRecord>
        const CgsModule::Event* AsEvent(const TRecord* lpRecord)
        {
            return reinterpret_cast<const CgsModule::Event*>(lpRecord);
        }

        template <typename TGuiEvent>
        void SetPayloadWord(TGuiEvent* lpEvent, s32 liValue)
        {
            static_assert(sizeof(lpEvent->maData) == sizeof(s32), "4-byte GUI payload");
            std::memcpy(lpEvent->maData, &liValue, sizeof(s32));
        }
    }

    // -----------------------------------------------------------------------------------
    // SearchFinishedCallback
    // A failed custom-match search clears the games component's error and publishes an
    // empty result list.
    // -----------------------------------------------------------------------------------
    void StateManager::SearchFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);

        if (!lbSuccess)
        {
            lpStateManager->mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);

            BrnGui::GuiEventNetworkCustomMatchResults lResultsEvent;
            lResultsEvent.miNumGames = 0;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lResultsEvent);
        }
    }

    // -----------------------------------------------------------------------------------
    // ResumeFinishedCallback
    // -----------------------------------------------------------------------------------
    void StateManager::ResumeFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT(lbSuccess, "lbSuccess");

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        lpStateManager->meState = E_STATE_COUNT;
        lpStateManager->OnFullConnection();
    }

    // -----------------------------------------------------------------------------------
    // SuspensionFinishedCallback
    // Tell the GUI the suspension is done; when it was a suspend to leave the game, also
    // publish why the player left.
    // -----------------------------------------------------------------------------------
    void StateManager::SuspensionFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT(lbSuccess, "lbSuccess");

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);

        CgsGui::GuiEvent<46> lSuspendedEvent;
        lpStateManager->mpNetworkModule->AddOutputGuiEvent(lSuspendedEvent);

        if (lpStateManager->meState == E_STATE_WAIT_SUSPENSION_LEAVING_GAME)
        {
            BrnNetworkModuleIO::NetworkOutPlayerLeftGame lLeftGameEvent;
            lLeftGameEvent.meLeftGameReason = lpStateManager->meLeftReason;
            lLeftGameEvent.meKickReason     = lpStateManager->meKickReason;
            lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                AsEvent(&lLeftGameEvent), lLeftGameEvent.GetEventType(), sizeof(lLeftGameEvent));
        }

        lpStateManager->meState = E_STATE_COUNT;
    }

    // -----------------------------------------------------------------------------------
    // HostChangedCallback
    // Registered with the host-migration flow: publish whether the local player is the new
    // host and whether this is the first host of the session (no previous host).
    // -----------------------------------------------------------------------------------
    void StateManager::HostChangedCallback(const CgsSystem::TimerStatus* lpTimerStatus,
                                           NetworkPlayerID lOldHostID,
                                           NetworkPlayerID lNewHostID,
                                           void* lpUserData)
    {
        (void)lpTimerStatus;

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != nullptr, "lpStateManager");
        CGS_ASSERT(lpStateManager->mpNetworkManager != nullptr, "lpStateManager->mpNetworkManager");
        CGS_ASSERT(lpStateManager->mpNetworkManager->GetPlayerManager() != nullptr,
                   "lpStateManager->mpNetworkManager->GetPlayerManager()");

        const NetworkPlayerID lLocalPlayerID =
            lpStateManager->mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
        const bool lbHaveLocalPlayer = (lLocalPlayerID != KI_INVALID_PLAYER_ID);

        BrnNetworkModuleIO::NetworkOutHostChangedEvent lHostChangedEvent;
        lHostChangedEvent.mbIsLocalPlayerNowHost = lbHaveLocalPlayer && (lNewHostID == lLocalPlayerID);
        lHostChangedEvent.mbIsFirstHost          = (lOldHostID == KI_INVALID_PLAYER_ID);

        CGS_ASSERT(lpStateManager->mpNetworkModule != nullptr, "lpStateManager->mpNetworkModule");
        CGS_ASSERT(lpStateManager->mpNetworkModule->GetNetworkEventQueue() != nullptr,
                   "lpStateManager->mpNetworkModule->GetNetworkEventQueue()");
        lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            AsEvent(&lHostChangedEvent), lHostChangedEvent.GetEventType(), sizeof(lHostChangedEvent));
    }

    // -----------------------------------------------------------------------------------
    // GetCompressedCameraPicCallback
    // The compressor finished the local camera picture: hand the DXT1 pixels on.
    // -----------------------------------------------------------------------------------
    void StateManager::GetCompressedCameraPicCallback(void* lpPixels, void* lpUserData)
    {
        CGS_ASSERT(lpPixels != nullptr, "lpPixels");
        CGS_ASSERT(lpUserData != nullptr, "lpUserData");

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != nullptr, "lpStateManager");

        BrnNetworkModuleIO::NetworkOutCamPicCompressedEvent lCompressedEvent;
        lCompressedEvent.miCompressedPixelSize = KI_COMPRESSED_CAMERA_PIC_SIZE;
        lCompressedEvent.meCompressedFormat    = renderengine::PIXELFORMAT_DXT1;
        lCompressedEvent.mpcCompressedPixels   = static_cast<char*>(lpPixels);
        lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            AsEvent(&lCompressedEvent), lCompressedEvent.GetEventType(), sizeof(lCompressedEvent));
    }

    // -----------------------------------------------------------------------------------
    // CreateInviteGame
    // Create a game for an outgoing invite: the default create-game parameters with 15
    // player slots.
    // -----------------------------------------------------------------------------------
    void StateManager::CreateInviteGame()
    {
        BrnGui::GuiEventNetworkCreateGame lCreateGameEvent;
        lCreateGameEvent.Construct();
        lCreateGameEvent.miMaxPlayers = 15;
        lCreateGameEvent.miCountA     = 2;
        lCreateGameEvent.mbFlagK      = 0;

        CreateGame(&lCreateGameEvent, false);
    }

    // -----------------------------------------------------------------------------------
    // DoInstantFreeburn
    // Quick-join an unranked free-burn game (leaving the current one first if in one).
    // -----------------------------------------------------------------------------------
    void StateManager::DoInstantFreeburn()
    {
        mbForceStartFreeburnLobby = false;
        mpNetworkManager->GetMatchMakingManager()->QuickJoinGame(false, true, QuickJoinFinishedCallback, this);
        meState = E_STATE_WAIT_MATCHMAKING;
    }

    // -----------------------------------------------------------------------------------
    // RefreshFreeBurnLobbyGameMode
    // -----------------------------------------------------------------------------------
    void StateManager::RefreshFreeBurnLobbyGameMode()
    {
        CGS_ASSERT(mbRefreshingFreeburnLobbyThisFrame, "mbRefreshingFreeburnLobbyThisFrame");

        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
        const bool lbInLobby = lpGames->IsLocalPlayerInGame() && !lpGames->IsGameStarted();
        if (lbInLobby && mpNetworkManager->GetPlayerManager()->GetNumberLocalPlayers() > 0)
        {
            StartFreeBurnLobbyGameMode(false, false, false, true);
        }
    }

    // -----------------------------------------------------------------------------------
    // StartTOSDownload
    // Download the terms of service from the server-configured URL (falling back to the
    // built-in one).
    // -----------------------------------------------------------------------------------
    void StateManager::StartTOSDownload()
    {
        const char* lpcUrlToUse =
            "http://sdevbesl01.online.ea.com/easo/editorial/common/2006/tos/tos.jsp?style=accept&lang=%25s&platform=xbl2";

        char lacTosURL[256] = {};

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        lpServerInterface->GetServerInfoComponent()->GetTosUrl(lacTosURL, sizeof(lacTosURL));
        if (lacTosURL[0] != '\0')
        {
            lpcUrlToUse = lacTosURL;
        }

        lpServerInterface->GetHttpComponent()->StartHttpsDownload(
            lpcUrlToUse,
            lpServerInterface->GetDownloadableConfigComponent()->GetTosDownloadBufferSize(),
            CgsNetwork::ServerInterfaceHttp::KI_SERVER_INTERFACE_HTTP_DOWNLOAD_TIMEOUT);

        meState = E_STATE_WAIT_DOWNLOAD_TOS;
    }

    // -----------------------------------------------------------------------------------
    // ReleaseNewsAndTOSDownload
    // Abort any download in flight and re-publish whichever texts are still held.
    // -----------------------------------------------------------------------------------
    void StateManager::ReleaseNewsAndTOSDownload()
    {
        if (meState == E_STATE_WAIT_DOWNLOAD_NEWS || meState == E_STATE_WAIT_DOWNLOAD_TOS)
        {
            mpNetworkManager->GetServerInterface()->GetHttpComponent()->StopHttpDownload();
            mpNetworkManager->GetServerInterface()->GetHttpComponent()->DestroyHttpsDownload();
            meState = E_STATE_COUNT;
        }

        CgsGui::GuiEventAddLocalisedTextPointer lTextEvent;

        if (mpTOS != nullptr)
        {
            lTextEvent.miMarker = 1;
            std::strncpy(lTextEvent.macId, "TOS_TEXT", CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH);
            lTextEvent.mpText = mpTOS;
            mpNetworkModule->AddOutputGuiEvent(lTextEvent);
        }

        if (mpNews != nullptr)
        {
            lTextEvent.miMarker = 1;
            std::strncpy(lTextEvent.macId, "NEWS_TEXT", CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH);
            lTextEvent.mpText = mpNews;
            mpNetworkModule->AddOutputGuiEvent(lTextEvent);
        }
    }

    // -----------------------------------------------------------------------------------
    // HaveAllSelectedACar
    // True once every player has either made a final car selection or is paused.
    // -----------------------------------------------------------------------------------
    bool StateManager::HaveAllSelectedACar()
    {
        GameParams lGameParams;
        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();

        NetworkPlayerID lPlayerID = KI_INVALID_PLAYER_ID;
        while (lpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            PlayerMenuData* lpMenuData = static_cast<PlayerMenuData*>(lpPlayerManager->GetMenuDataByID(lPlayerID));
            CGS_ASSERT(lpMenuData != nullptr, "lpMenuData");

            CgsNetwork::NetworkPlayer* lpPlayer = lpPlayerManager->GetPlayerByID(lPlayerID);
            if (!lpMenuData->mbFinalCarSelection
                && (lpPlayer == nullptr || !lpPlayer->mbNetworkPlayerPaused))
            {
                return false;
            }
        }

        return true;
    }

    // -----------------------------------------------------------------------------------
    // StartLogIn
    // Kick the login manager off with the requested sign-in flavour.
    // -----------------------------------------------------------------------------------
    void StateManager::StartLogIn(LoginManagerBase::ESignInType leSignInType)
    {
        CGS_ASSERT(!mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()
                       || mpNetworkManager->GetLoginManager()->IsSigningIn()
                       || mpNetworkManager->GetServerInterface()->IsSuspended(),
                   "!mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn() || mpNetworkManager->GetLoginManager()->IsSigningIn() || mpNetworkManager->GetServerInterface()->IsSuspended()");

        if (meState != E_STATE_COUNT && meState != E_STATE_LOGIN)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << "meState=" << static_cast<s32>(meState);
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }

        CGS_ASSERT(( !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() )
                       || ( !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted() ),
                   "( !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() ) || ( !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted() )");

        {
            CgsDev::DebugInterface lDebugInterface;
            lDebugInterface.GetDebugManager().ActivateComponent(mpNetworkManager->GetVersionDisplay());

            mpNetworkManager->GetLoginManager()->Start(leSignInType);
            meState = E_STATE_LOGIN;
        }
    }

    // -----------------------------------------------------------------------------------
    // HandleConnectEvent
    // The GUI asked to go online. Already logged in (and not mid sign-in or suspended):
    // report the connection straight back and carry on with any pending invite or
    // instant free-burn. Otherwise start the log-in.
    // -----------------------------------------------------------------------------------
    void StateManager::HandleConnectEvent(const BrnGui::GuiEventNetworkConnect* lpConnectEvent)
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();

        if (lpServerInterface->GetConnectionComponent()->IsLoggedIn()
            && !mpNetworkManager->GetLoginManager()->IsSigningIn()
            && !lpServerInterface->IsSuspended())
        {
            // FLAG: the console also logs "HandleConnectEvent() - Already connected.
            // SignInType=" <type> " Still Signing In=" <bool> "\n" to a network debug stream
            // that has no home in this tree; the log line is not reproduced.

            CgsGui::GuiEventNetworkConnected lConnectedEvent;
            lConnectedEvent.maData[0] = mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsFirstLogin();
            lConnectedEvent.maData[1] = mpNetworkManager->GetServerInterface()->GetServerInfoComponent()->IsNewsUpdated();
            mpNetworkModule->AddOutputGuiEvent(lConnectedEvent);

            mpNetworkManager->GetNetworkInviteManager()->LogInComplete(true);
            mpNetworkManager->OnLogIn();

            if (mbDoInviteAfterCreate)
            {
                CreateInviteGame();
            }
            else if (mbInstantFreeburn)
            {
                DoInstantFreeburn();
            }
            return;
        }

        CGS_ASSERT(lpConnectEvent != nullptr, "lpConnectEvent");

        // FLAG: the console logs "HandleConnectEvent() - NOT connected. SignInType=" <type>
        // " Still Signing In=" "true"/"false" "\n" to the same unhomed debug stream here.

        StartLogIn(static_cast<LoginManagerBase::ESignInType>(lpConnectEvent->miConnectType));
    }

    // -----------------------------------------------------------------------------------
    // CreateGameFinishedCallback
    // -----------------------------------------------------------------------------------
    void StateManager::CreateGameFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        GameParams lGameParams;

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != nullptr, "lpStateManager");

        lpStateManager->meState = E_STATE_COUNT;

        CGS_ASSERT(lpStateManager->mpNetworkManager != nullptr, "lpStateManager->mpNetworkManager");
        CGS_ASSERT(lpStateManager->mpNetworkManager->GetServerInterface() != nullptr,
                   "lpStateManager->mpNetworkManager->GetServerInterface()");

        CgsNetwork::ServerInterfaceGames* lpGames =
            lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent();
        const bool lbInGame = lbSuccess && lpGames->IsLocalPlayerInGame();

        if (lbInGame)
        {
            lpStateManager->mpNetworkManager->OnEnterGame();

            CgsGui::GuiEventNetworkInGame lInGameEvent;
            lInGameEvent.maData[0] = false;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameEvent);

            lGameParams.Prepare();
            lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

            if (!GsmIO::IsOnlineFreeBurnLobby(static_cast<GsmIO::EGameModeType>(lGameParams.GameMode()))
                || lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame() > 1
                || lpStateManager->mbCreatedFromMenus)
            {
                lpStateManager->StartFreeBurnLobbyGameMode(false, false, false, false);
            }

            lpStateManager->mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_GAME_CREATED, nullptr);

            BrnNetworkManager* lpNetworkManager = lpStateManager->mpNetworkManager;
            lpNetworkManager->GetVersionDisplay()->SetGameServerGame(
                lpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameServerGame());
        }
        else
        {
            s32 liFailReason;
            const s32 liErrorCode =
                lpStateManager->mpNetworkManager->GetServerInterface()->GetAndClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
            if (liErrorCode == KI_GAMES_ERROR_GAME_FULL)
            {
                liFailReason = KI_IN_GAME_FAIL_REASON_FULL;
            }
            else if (liErrorCode == KI_GAMES_ERROR_ALREADY_STARTED || liErrorCode == KI_GAMES_ERROR_GAME_LOCKED)
            {
                liFailReason = KI_IN_GAME_FAIL_REASON_STARTED;
            }
            else
            {
                liFailReason = KI_IN_GAME_FAIL_REASON_COUNT;
            }

            if (lpStateManager->mbDoInviteAfterCreate)
            {
                lpStateManager->mbDoInviteAfterCreate = false;

                CGS_ASSERT(lpStateManager->mpNetworkManager != nullptr, "lpStateManager->mpNetworkManager");
                CGS_ASSERT(lpStateManager->mpNetworkManager->GetBuddyManager() != nullptr,
                           "lpStateManager->mpNetworkManager->GetBuddyManager()");
                lpStateManager->mpNetworkManager->GetBuddyManager()->CancelInvites();

                CGS_ASSERT(lpStateManager->mpNetworkManager != nullptr, "lpStateManager->mpNetworkManager");
                CGS_ASSERT(lpStateManager->mpNetworkManager->GetSuspensionManager() != nullptr,
                           "lpStateManager->mpNetworkManager->GetSuspensionManager()");
                SuspensionManager* lpSuspensionManager = lpStateManager->mpNetworkManager->GetSuspensionManager();
                if (!lpSuspensionManager->IsSuspending())
                {
                    lpStateManager->meState = E_STATE_WAIT_SUSPENSION;
                    // FLAG: the suspension type is folded away on the console (both types
                    // request the same server-interface suspend flags).
                    lpSuspensionManager->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                 SuspensionFinishedCallback, lpStateManager);
                }
                else
                {
                    lpStateManager->meState = E_STATE_WAIT_SUSPENSION_IDLE_TO_SUSPEND;
                }

                liFailReason = KI_IN_GAME_FAIL_REASON_CREATE_GAME_FOR_INVITE_FAILED;
            }

            CgsGui::GuiEventNetworkInGameFailed lInGameFailedEvent;
            SetPayloadWord(&lInGameFailedEvent, liFailReason);
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameFailedEvent);

            lpStateManager->mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
            lpStateManager->mpNetworkManager->GetBuddyManager()->CancelInvites();

            // FLAG: the console logs "Invite sending might have failed as we failed to create
            // a game\n" to the unhomed network debug stream here.
        }

        lpStateManager->mbDoInviteAfterCreate = false;
    }

    // -----------------------------------------------------------------------------------
    // JoinGameFinishedCallback
    // -----------------------------------------------------------------------------------
    void StateManager::JoinGameFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != nullptr, "lpStateManager");

        lpStateManager->meState = E_STATE_COUNT;

        CGS_ASSERT(lpStateManager->mpNetworkManager != nullptr, "lpStateManager->mpNetworkManager");
        CGS_ASSERT(lpStateManager->mpNetworkManager->GetServerInterface() != nullptr,
                   "lpStateManager->mpNetworkManager->GetServerInterface()");

        const bool lbInGame =
            lbSuccess && lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();

        lpStateManager->mpNetworkManager->GetNetworkInviteManager()->JoinGameComplete(lbInGame);

        if (lbInGame)
        {
            lpStateManager->mpNetworkManager->OnEnterGame();

            CgsGui::GuiEventNetworkInGame lInGameEvent;
            lInGameEvent.maData[0] = false;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameEvent);

            lpStateManager->StartFreeBurnLobbyGameMode(false, false, lpStateManager->mbForceStartFreeburnLobby, false);
            lpStateManager->mbForceStartFreeburnLobby = false;

            lpStateManager->mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_GAME_JOINED, nullptr);

            BrnNetworkManager* lpNetworkManager = lpStateManager->mpNetworkManager;
            lpNetworkManager->GetVersionDisplay()->SetGameServerGame(
                lpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameServerGame());
            return;
        }

        s32 liFailReason;
        switch (lpStateManager->mpNetworkManager->GetServerInterface()->GetAndClearLastError(CgsNetwork::E_COMPONENTS_GAMES))
        {
        case KI_GAMES_ERROR_UNKNOWN_GAME:
            liFailReason = KI_IN_GAME_FAIL_REASON_UNKNOWN_GAME;
            break;
        case KI_GAMES_ERROR_GAME_FULL:
            liFailReason = KI_IN_GAME_FAIL_REASON_FULL;
            break;
        case KI_GAMES_ERROR_ALREADY_STARTED:
        case KI_GAMES_ERROR_GAME_LOCKED:
            liFailReason = KI_IN_GAME_FAIL_REASON_STARTED;
            break;
        default:
            liFailReason = KI_IN_GAME_FAIL_REASON_COUNT;
            break;
        }

        CgsGui::GuiEventNetworkInGameFailed lInGameFailedEvent;
        SetPayloadWord(&lInGameFailedEvent, liFailReason);
        lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameFailedEvent);
    }

    // -----------------------------------------------------------------------------------
    // LeaveGameForOfflineGameFinishedCallback
    // Left the online game to go offline: clear the player images, drop the requested
    // camera feed, put the free-burn car back when leaving an online event, and suspend
    // the server interface. On failure tell the GUI and go idle.
    // -----------------------------------------------------------------------------------
    void StateManager::LeaveGameForOfflineGameFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != nullptr, "lpStateManager");
        CGS_ASSERT(lpStateManager->mpNetworkManager != nullptr, "lpStateManager->mpNetworkManager");

        if (!lbSuccess)
        {
            lpStateManager->mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);

            BrnGui::GuiEventNetworkLeavingGameFailed lLeavingFailedEvent;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lLeavingFailedEvent);

            lpStateManager->meState = E_STATE_COUNT;
            return;
        }

        lpStateManager->mpNetworkManager->OnLeaveGame();

        BrnGui::GuiEventNetworkPlayerImage lNetworkPlayerImageEvent;
        lNetworkPlayerImageEvent.mpTexture      = nullptr;
        lNetworkPlayerImageEvent.miTextureIndex = -1;
        lpStateManager->mpNetworkModule->AddOutputGuiEvent(lNetworkPlayerImageEvent);

        lpStateManager->mpNetworkManager->GetCamera()->RequestFeed(KI_INVALID_PLAYER_ID);

        CGS_ASSERT(lpStateManager->mpNetworkModule != nullptr, "lpStateManager->mpNetworkModule");
        CGS_ASSERT(lpStateManager->mpNetworkModule->GetGameStateToNetworkInterface() != nullptr,
                   "lpStateManager->mpNetworkModule->GetGameStateToNetworkInterface()");

        if (!GsmIO::IsOnlineFreeBurnLobby(lpStateManager->mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode()))
        {
            const GsmIO::EGameModeType leGameMode =
                lpStateManager->mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode();
            if (leGameMode >= GsmIO::E_MODE_ONLINE_MODE_START && leGameMode <= GsmIO::E_MODE_ONLINE_MODE_END)
            {
                GsmIO::ChangePlayerCarEvent lChangeCarEvent;
                lChangeCarEvent.mCarModelId         = lpStateManager->mpNetworkManager->GetFreeBurnCarID();
                lChangeCarEvent.mWheelModelId       = lpStateManager->mpNetworkManager->GetFreeBurnWheelID();
                lChangeCarEvent.mbResetPlayerCamera = false;
                lChangeCarEvent.mbKeepResetSection  = true;
                lpStateManager->mpNetworkModule->GetGameEventQueue()->AddEvent(
                    AsEvent(&lChangeCarEvent), GsmIO::E_EVENT_CHANGE_PLAYER_CAR, sizeof(lChangeCarEvent));
            }
        }

        lpStateManager->mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_GAME_LEFT, nullptr);
        lpStateManager->mpNetworkManager->GetVersionDisplay()->SetGameServerGame(false);
        lpStateManager->mpNetworkManager->GetStartTimeManager()->PrepareStartTime();

        lpStateManager->SuspendToLeaveGame(E_LEFT_GAME_REASON_LEFT, CgsNetwork::E_KICKREASON_COUNT);
    }

    // -----------------------------------------------------------------------------------
    // QuickJoinFinishedCallback
    // -----------------------------------------------------------------------------------
    void StateManager::QuickJoinFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        CGS_ASSERT(lpStateManager != nullptr, "lpStateManager");

        lpStateManager->meState = E_STATE_COUNT;

        CGS_ASSERT(lpStateManager->mpNetworkManager != nullptr, "lpStateManager->mpNetworkManager");
        CGS_ASSERT(lpStateManager->mpNetworkManager->GetServerInterface() != nullptr,
                   "lpStateManager->mpNetworkManager->GetServerInterface()");

        const bool lbInGame =
            lbSuccess && lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();

        if (lbInGame)
        {
            lpStateManager->mpNetworkManager->OnEnterGame();

            CgsGui::GuiEventNetworkInGame lInGameEvent;
            lInGameEvent.maData[0] = false;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lInGameEvent);

            BrnNetworkManager* lpNetworkManager = lpStateManager->mpNetworkManager;
            CGS_ASSERT(lpNetworkManager != nullptr, "lpNetworkManager");

            lpStateManager->StartFreeBurnLobbyGameMode(false, false, lpStateManager->mbForceStartFreeburnLobby, false);
            lpStateManager->mbForceStartFreeburnLobby = false;

            lpStateManager->mpNetworkManager->GetVersionDisplay()->SetGameServerGame(
                lpStateManager->mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameServerGame());
        }
        else
        {
            lpStateManager->mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);

            BrnGui::GuiEventNetworkCustomMatchResults lResultsEvent;
            lResultsEvent.miNumGames = 0;
            lpStateManager->mpNetworkModule->AddOutputGuiEvent(lResultsEvent);
        }

        if (lpStateManager->mbInstantFreeburn)
        {
            lpStateManager->mbInstantFreeburn = false;

            BrnNetworkModuleIO::NetworkOutInstantFreeburnComplete lCompleteEvent;
            lCompleteEvent.mbSuccess = lbInGame;
            lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                AsEvent(&lCompleteEvent), lCompleteEvent.GetEventType(), sizeof(lCompleteEvent));

            if (!lbInGame)
            {
                lpStateManager->meState = E_STATE_WAIT_SUSPENSION;
                // FLAG: the suspension type is folded away on the console (both types
                // request the same server-interface suspend flags).
                lpStateManager->mpNetworkManager->GetSuspensionManager()->Suspend(
                    SuspensionManager::E_SUSPENSION_TYPE_OFFLINE, SuspensionFinishedCallback, lpStateManager);
            }
        }

        BrnNetworkModuleIO::NetworkOutInstantFreeburnEvent lInstantFreeburnEvent;
        lInstantFreeburnEvent.mbIsDoingInstantFreeburn = false;
        lpStateManager->mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            AsEvent(&lInstantFreeburnEvent), lInstantFreeburnEvent.GetEventType(), sizeof(lInstantFreeburnEvent));
    }

    // -----------------------------------------------------------------------------------
    // UpdateLaunching
    // Tick the launch manager. Once launched: stop the time sync, start the game, arm host
    // migration, start team selection, tell the GUI / game state / network side, cache the
    // player ids, drop the per-lobby cached data and fix each player's car choice. A failed
    // launch goes idle and reports the failure.
    // -----------------------------------------------------------------------------------
    void StateManager::UpdateLaunching()
    {
        switch (mpNetworkManager->GetLaunchManager()->Update())
        {
        case LaunchManager::E_STATUS_IDLE:
        case LaunchManager::E_STATUS_BUSY:
        case LaunchManager::E_STATUS_WAITING_FOR_LAUNCH:
        case LaunchManager::E_STATUS_LAUNCHING:
            break;

        case LaunchManager::E_STATUS_LAUNCHED:
        {
            GameParams              lGameParams;
            BrnNetworkModuleIO::NetworkOutLaunchedEvent lNetworkLaunchedEvent;
            OnlineCarSelectEvent    lCarSelectEvent;
            NetworkPlayerID         lPlayerID = KI_INVALID_PLAYER_ID;

            CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");

            lGameParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

            mpNetworkManager->GetStartTimeManager()->StopSyncingTime();
            mpNetworkManager->OnGameStart();
            mpNetworkManager->GetStartTimeManager()->PrepareStartTime();

            mpNetworkManager->GetHostMigrationManager()->Enable(
                mpNetworkManager->GetPlayerManager()->GetHostPlayerID(),
                mpNetworkManager->GetTimerStatus(),
                mpNetworkManager->GetTimeManager()->GetU16FrameCount());

            CGS_ASSERT(mpNetworkManager->GetTeamSelectionManager() != nullptr,
                       "mpNetworkManager->GetTeamSelectionManager()");
            meState = E_STATE_WAIT_TEAM_SELECTION;
            mpNetworkManager->GetTeamSelectionManager()->StartTeamSelection(
                lGameParams.GameMode(), TeamSelectionFinishedCallback, this);

            CgsGui::GuiEventNetworkLaunched lLaunchedEvent;
            SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_SUCCESS);
            mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);

            mpNetworkModule->GetGameEventQueue()->AddEvent(
                AsEvent(&lCarSelectEvent), KI_GAME_EVENT_ONLINE_CAR_SELECT, sizeof(lCarSelectEvent));

            lNetworkLaunchedEvent.mbSuccessfulLaunch  = true;
            lNetworkLaunchedEvent.miVehicleClassLimit = lGameParams.VehicleLevelLimit();
            lNetworkLaunchedEvent.mbHostChoiceCarAndNotHost =
                lGameParams.VehicleChoice() == 1
                && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost();
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                AsEvent(&lNetworkLaunchedEvent), lNetworkLaunchedEvent.GetEventType(), sizeof(lNetworkLaunchedEvent));

            mpNetworkManager->GetLaunchManager()->Stop();
            mpNetworkModule->CacheNetworkPlayerIDsAtGameStart();

            mMarkedManData.mbValid  = false;
            mNewCarData.mbValid     = false;
            mDistrictData.mbValid   = false;
            mCarColourData.mbValid  = false;
            mFeverData.mbValid      = false;
            mbSetNotPlaying         = false;

            while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
            {
                PlayerMenuData* lpMenuData =
                    static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
                CGS_ASSERT((lpMenuData->mFreeBurnCarId != 0), "(lpMenuData->mFreeBurnCarId != 0)");

                if (lpMenuData->mCarId == 0)
                {
                    lpMenuData->mCarId   = lpMenuData->mFreeBurnCarId;
                    lpMenuData->mWheelId = lpMenuData->mFreeBurnWheelId;
                }
            }
            break;
        }

        case LaunchManager::E_STATUS_ERROR:
        {
            BrnNetworkModuleIO::NetworkOutLaunchedEvent lNetworkLaunchedEvent;

            meState = E_STATE_COUNT;

            CgsGui::GuiEventNetworkLaunched lLaunchedEvent;
            SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_FAILED);
            mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);

            // mbHostChoiceCarAndNotHost is not written on this path.
            lNetworkLaunchedEvent.mbSuccessfulLaunch  = false;
            lNetworkLaunchedEvent.miVehicleClassLimit = 9;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                AsEvent(&lNetworkLaunchedEvent), lNetworkLaunchedEvent.GetEventType(), sizeof(lNetworkLaunchedEvent));

            mpNetworkManager->GetLaunchManager()->Stop();
            mpNetworkModule->ClearCachedNetworkPlayerIDsAtGameEnd();
            mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_GAMES);
            break;
        }

        default:
            CGS_ASSERT(false, "Invalid launch state");
            break;
        }
    }
} // namespace BrnNetwork
