// ===================================================================================
// BrnNetwork::StateManager -- part 2: log-in, launching, news/TOS download, the
// free-burn lobby helpers and the matchmaking / suspension completion callbacks.
//
// Every manager and component is reached through the owning BrnNetworkManager's
// accessors; every StateManager member by name.
//
// Network-out records come from BrnNetworkOutEventTypeDefs.h, game events from
// BrnGameEvents.h.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                                        // CGS_ASSERT
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                                // CgsContainers::BitArray (scalp slots)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                // the memory checks' log line
#include "GameShared/GameClasses/Development/CgsStrStream.h"                              // CgsDev::StrStream (streamed asserts)
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // CgsDev::DebugInterface
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                                       // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                               // GuiEventNetworkConnected / InGame / InGameFailed / Launched
#include "GameShared/GameClasses/Gui/CgsGuiEventAddLocalisedTextPointer.h"                // CgsGui::GuiEventAddLocalisedTextPointer
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                          // CgsNetwork::K_INVALID_PLAYER_ID
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
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"   // CgsNetwork::EServerInterfaceError
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
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"
#include "GameSource/Network/Managers/BrnNetworkImageManager.h"
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                       // BrnNetwork::PlayerInfoData
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                    // BrnNetwork::PlayerParams
#include "GameSource/Resource/BrnResourceAllocator.h"                                     // GetAvailableMemory, BRN_RESOURCE_MEMORY_CHECK
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h"          // GameStateToNetworkInterface
#include "pc/gcm/renderengine/pixelformat.h"                                              // renderengine::PIXELFORMAT_DXT1

#include <cstring>   // std::memcpy / std::strlen / std::strncpy

// Platform sign-in state query (provided by the platform layer).
extern "C" u32 XUserGetSigninState(u32 luUserIndex);

namespace BrnNetwork
{
    namespace
    {
        namespace GsmIO = BrnGameState::GameStateModuleIO;

        // The compressed camera picture is a 160 x 120 DXT1 image: 160 * 120 / 2 bytes.
        const s32 KI_COMPRESSED_CAMERA_PIC_SIZE = 9600;

        // XUserGetSigninState: signed in to the online service.
        const u32 KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE = 2;

        // ConnApiControl selector for the downloaded connection mangle setting.
        const s32 KI_CONNAPI_CONTROL_MANGLE = 'mngl';

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
        const bool lbHaveLocalPlayer = (lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID);

        BrnNetworkModuleIO::NetworkOutHostChangedEvent lHostChangedEvent;
        lHostChangedEvent.mbIsLocalPlayerNowHost = lbHaveLocalPlayer && (lNewHostID == lLocalPlayerID);
        lHostChangedEvent.mbIsFirstHost          = (lOldHostID == CgsNetwork::K_INVALID_PLAYER_ID);

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
    // Create a game for an outgoing invite: the default create-game parameters in the
    // online free-burn lobby mode (15), security 2, unranked.
    // -----------------------------------------------------------------------------------
    void StateManager::CreateInviteGame()
    {
        BrnGui::GuiEventNetworkCreateGame lCreateGameEvent;
        lCreateGameEvent.Construct();
        lCreateGameEvent.meGameMode = 15;
        lCreateGameEvent.meSecurity = 2;
        lCreateGameEvent.mbRanked   = false;

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

        NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
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
            if (liErrorCode == CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_GAME_FULL)
            {
                liFailReason = KI_IN_GAME_FAIL_REASON_FULL;
            }
            else if (liErrorCode == CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED || liErrorCode == CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_GAME_LOCKED)
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
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_UNKOWN_GAME:
            liFailReason = KI_IN_GAME_FAIL_REASON_UNKNOWN_GAME;
            break;
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_GAME_FULL:
            liFailReason = KI_IN_GAME_FAIL_REASON_FULL;
            break;
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_ALREADY_STARTED:
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_ERROR_GAME_LOCKED:
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

        lpStateManager->mpNetworkManager->GetCamera()->RequestFeed(CgsNetwork::K_INVALID_PLAYER_ID);

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
            GsmIO::OnlineCarSelectEvent lCarSelectEvent;
            NetworkPlayerID         lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;

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
                AsEvent(&lCarSelectEvent), GsmIO::E_EVENT_ONLINE_CAR_SELECT, sizeof(lCarSelectEvent));

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

    // -----------------------------------------------------------------------------------
    // StartNewsDownload
    // Download the news from the server-configured URL. The built-in fallback is the same
    // literal the terms-of-service download uses.
    // -----------------------------------------------------------------------------------
    void StateManager::StartNewsDownload()
    {
        const char* lpcUrlToUse =
            "http://sdevbesl01.online.ea.com/easo/editorial/common/2006/tos/tos.jsp?style=accept&lang=%25s&platform=xbl2";

        char lacNewsURL[256] = {};

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        lpServerInterface->GetServerInfoComponent()->GetNewsUrl(lacNewsURL, sizeof(lacNewsURL));
        if (lacNewsURL[0] != '\0')
        {
            lpcUrlToUse = lacNewsURL;
        }

        lpServerInterface->GetHttpComponent()->StartHttpsDownload(
            lpcUrlToUse,
            lpServerInterface->GetDownloadableConfigComponent()->NewsBufferSize(),
            CgsNetwork::ServerInterfaceHttp::KI_SERVER_INTERFACE_HTTP_DOWNLOAD_TIMEOUT);

        meState = E_STATE_WAIT_DOWNLOAD_NEWS;
    }

    // -----------------------------------------------------------------------------------
    // UpdateNewsAndTOSDownload
    // Start whichever download is queued once the HTTP component is idle, or collect the
    // one in flight: a finished download is copied into mpNews / mpTOS and handed to the
    // GUI as localised text; a failure is reported as the matching FAILED event.
    // -----------------------------------------------------------------------------------
    void StateManager::UpdateNewsAndTOSDownload()
    {
        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();

        if (meState == E_STATE_WAIT_DOWNLOAD_NEWS_IDLE)
        {
            if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == BrnServerInterface::E_STATUS_IDLE)
            {
                StartNewsDownload();
            }
            return;
        }

        if (meState == E_STATE_WAIT_DOWNLOAD_TOS_IDLE)
        {
            if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == BrnServerInterface::E_STATUS_IDLE)
            {
                StartTOSDownload();
            }
            return;
        }

        typedef BrnGui::GuiEventNetworkNewsAndTOS NewsAndTOSEvent;
        NewsAndTOSEvent::EEventType leEventType = NewsAndTOSEvent::E_EVENT_TYPE_COUNT;
        bool                        lbFailed    = false;

        if (lpServerInterface->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == BrnServerInterface::E_STATUS_IDLE)
        {
            char* lpcBuffer = reinterpret_cast<char*>(lpServerInterface->GetHttpComponent()->GetHttpsDownloadBuffer());
            if (meState == E_STATE_WAIT_DOWNLOAD_NEWS)
            {
                lpcBuffer = StripWebOfferControlCodes(lpcBuffer);
                lbFailed  = (lpcBuffer == nullptr);
            }

            if (!lbFailed)
            {
                while (*lpcBuffer == '\n')
                {
                    ++lpcBuffer;
                }

                CgsUnicode::CgsUtf8** lppText;
                const char*           lpcTextID;
                if (meState == E_STATE_WAIT_DOWNLOAD_NEWS)
                {
                    lppText     = &mpNews;
                    lpcTextID   = "NEWS_TEXT";
                    leEventType = NewsAndTOSEvent::E_EVENT_TYPE_RETRIEVED_NEWS;
                }
                else
                {
                    lppText     = &mpTOS;
                    lpcTextID   = "TOS_TEXT";
                    leEventType = NewsAndTOSEvent::E_EVENT_TYPE_RETRIEVED_TOS;
                }

                if (*lppText != nullptr)
                {
                    CgsNetwork::ServerInterfaceDirtySock::MemFree(*lppText, 0, 0);
                    *lppText = nullptr;
                }
                *lppText = static_cast<CgsUnicode::CgsUtf8*>(CgsNetwork::ServerInterfaceDirtySock::MemAlloc(
                    mpNetworkManager->GetServerInterface()->GetHttpComponent()->GetHttpDownloadSize(), 0, 0));
                CgsUnicode::Copy(*lppText, reinterpret_cast<const CgsUnicode::CgsUtf8*>(lpcBuffer));

                CgsGui::GuiEventAddLocalisedTextPointer lTextEvent;
                lTextEvent.miMarker = 0;
                // The bounded copy's own "String too long" check (inlined from the string utilities).
                if (!(std::strlen(lpcTextID) < static_cast<u32>(CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH)))
                {
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "String too long: " << lpcTextID;
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                }
                std::strncpy(lTextEvent.macId, lpcTextID, CgsGui::GuiEventAddLocalisedTextPointer::KI_MAX_ADD_ID_STRING_LENGTH);
                lTextEvent.mpText = *lppText;
                mpNetworkModule->AddOutputGuiEvent(lTextEvent);
            }
        }
        else if (mpNetworkManager->GetServerInterface()->GetStatus(CgsNetwork::E_COMPONENTS_HTTP) == BrnServerInterface::E_STATUS_ERROR)
        {
            mpNetworkManager->GetServerInterface()->ClearLastError(CgsNetwork::E_COMPONENTS_HTTP);
            lbFailed = true;
        }

        if (lbFailed)
        {
            leEventType = (meState == E_STATE_WAIT_DOWNLOAD_NEWS) ? NewsAndTOSEvent::E_EVENT_TYPE_FAILED_NEWS
                                                                 : NewsAndTOSEvent::E_EVENT_TYPE_FAILED_TOS;
        }

        if (leEventType != NewsAndTOSEvent::E_EVENT_TYPE_COUNT)
        {
            mpNetworkManager->GetServerInterface()->GetHttpComponent()->DestroyHttpsDownload();

            NewsAndTOSEvent lNewsAndTOSEvent;
            lNewsAndTOSEvent.meEventType = leEventType;
            mpNetworkModule->AddOutputGuiEvent(lNewsAndTOSEvent);

            meState = E_STATE_COUNT;
        }
    }

    // -----------------------------------------------------------------------------------
    // OnFullConnection
    // Fully online: request the local player's stats, pass the downloaded mangle setting to
    // the connection API and drop any requested camera feed.
    // -----------------------------------------------------------------------------------
    void StateManager::OnFullConnection()
    {
        // The console's PlayerInfoData constructor runs Prepare itself; the tree's does not yet.
        PlayerInfoData lPlayerInfo;
        lPlayerInfo.Prepare();
        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        CGS_ASSERT(lPlayerInfo.GetID() > 0, "lPlayerInfo.GetID() > 0");

        mpNetworkManager->GetStatsManager()->RequestPlayerStats(lPlayerInfo.GetName());

        BrnServerInterface* lpServerInterface = mpNetworkManager->GetServerInterface();
        ConnApiControl(lpServerInterface->GetConnAPIRef(), KI_CONNAPI_CONTROL_MANGLE,
                       (lpServerInterface->GetDownloadableConfigComponent()->MangleConfigValue() != 0) ? 1 : 0,
                       0, nullptr);

        mpNetworkManager->GetCamera()->RequestFeed(CgsNetwork::K_INVALID_PLAYER_ID);
    }

    // -----------------------------------------------------------------------------------
    // StartFreeBurnLobbyGameMode
    // Post the start-network-game event for the free-burn lobby (every player's free-burn
    // car, colours, team, host slot and fever flag), then the round start unless this is a
    // refresh. A host that starts the lobby because a player joined, while not already in
    // the lobby, also tells the network side.
    // FLAG: when the local player id is invalid the console dumps the game component and
    // player manager state to the unhomed network debug stream just before the assert; the
    // dump is not reproduced.
    // -----------------------------------------------------------------------------------
    void StateManager::StartFreeBurnLobbyGameMode(bool lbStartingAfterJoin, bool lbStartingAfterOnlineEvent,
                                                  bool lbForceStartFreeburnLobby, bool lbRefreshOnly)
    {
        CGS_ASSERT(!lbRefreshOnly || (lbStartingAfterJoin && lbStartingAfterOnlineEvent && lbForceStartFreeburnLobby)==false,
                   "!lbRefreshOnly || (lbStartingAfterJoin && lbStartingAfterOnlineEvent && lbForceStartFreeburnLobby)==false");

        PlayerParams                 lPlayerParams;
        GameParams                   lGameParams;
        GsmIO::StartNetworkGameEvent lEvent;

        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        lEvent.Clear();
        lEvent.muRandomSeedForGame        = 0;
        lEvent.meGameMode                 = GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY;
        lEvent.miNumRaceCars              = mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
                                                CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS);
        lEvent.mbRefreshOnly              = lbRefreshOnly;
        lEvent.mbForceStartFreeburnLobby  = lbForceStartFreeburnLobby;
        lEvent.mbIsRanked                 = false;
        lEvent.mfTimeLimit                = 0.0f;
        lEvent.miNumRounds                = 1;
        lEvent.mbIsTrafficOn              = true;
        lEvent.mbIsTrafficCheckingOn      = true;
        lEvent.mbRedTeamHaveInfiniteBoost = false;
        lEvent.meBoostType                = 0;
        lEvent.miNumRunnerCrashes         = 3;

        // The reference's inline StartNetworkGameEvent::SetLocalNetworkPlayerID (assert, then
        // store); BrnGameEvents.h does not declare it yet.
        const NetworkPlayerID lLocalNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
        CGS_ASSERT(lLocalNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lLocalNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        lEvent.mLocalNetworkPlayerID = lLocalNetworkPlayerID;

        s32             liGridPosition = 0;
        NetworkPlayerID lPlayerID      = CgsNetwork::K_INVALID_PLAYER_ID;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
        {
            lPlayerParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);

            PlayerMenuData* lpMenuData =
                static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
            CGS_ASSERT((lpMenuData->mFreeBurnCarId != 0), "(lpMenuData->mFreeBurnCarId != 0)");

            lEvent.SetPlayerData(liGridPosition, lPlayerID,
                                 lpMenuData->mFreeBurnCarId, lpMenuData->mFreeBurnWheelId,
                                 lpMenuData->mu16FreeburnCarColourIndex, lpMenuData->mu16FreeburnPaintFinishIndex,
                                 lpMenuData->mfUnknown4C,
                                 lPlayerParams.GetPlayerTeam(),
                                 lPlayerID == mpNetworkManager->GetPlayerManager()->GetHostPlayerID(),
                                 lPlayerParams.HasFever());
            ++liGridPosition;
        }

        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
        lEvent.mbIsStartingGameAfterPlayerJoin =
            lpGames->IsLocalPlayerInGame() && lpGames->IsLocalPlayerHost() && lbStartingAfterJoin;
        lEvent.mbIsStartingFreeburnLobbyAfterOnlineEvent = lbStartingAfterOnlineEvent;
        mpNetworkModule->GetGameEventQueue()->AddEvent(AsEvent(&lEvent), GsmIO::E_EVENT_START_NETWORK_GAME, sizeof(lEvent));

        if (!lbRefreshOnly)
        {
            GsmIO::StartNetworkRoundEvent lRoundEvent;
            lRoundEvent.miNumLandmarksInRound = 0;
            mpNetworkModule->GetGameEventQueue()->AddEvent(AsEvent(&lRoundEvent), GsmIO::E_EVENT_START_NETWORK_ROUND, sizeof(lRoundEvent));
        }

        CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
        CGS_ASSERT(mpNetworkModule->GetGameStateToNetworkInterface() != nullptr,
                   "mpNetworkModule->GetGameStateToNetworkInterface()");

        if (lEvent.mbIsStartingGameAfterPlayerJoin
            && !GsmIO::IsOnlineFreeBurnLobby(mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode()))
        {
            BrnNetworkModuleIO::NetworkOutStartingGameDueToPlayerJoin lStartingGameDueToPlayerJoin;
            mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                AsEvent(&lStartingGameDueToPlayerJoin), lStartingGameDueToPlayerJoin.GetEventType(),
                sizeof(lStartingGameDueToPlayerJoin));
        }
    }

    // -----------------------------------------------------------------------------------
    // OutputPlayerTexture
    // Push the pictures the GUI asked for (meOutputPlayerTexture) through the network
    // manager's texture slots: the requested camera picture or gamer picture, the round
    // winner's photo finish, the per-car takedown scalps, or a gamer picture for the
    // photo booth / licence. Every mode except the player image turns the game camera off
    // afterwards.
    // -----------------------------------------------------------------------------------
    void StateManager::OutputPlayerTexture()
    {
        typedef BrnGui::GuiEventNetworkOutputPlayerTexture OutputPlayerTextureEvent;

        if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_PLAYER_IMAGE)
        {
            // The console evaluates IsLocalPlayerInGame here and enables the camera either way.
            mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame();
            mpNetworkManager->GetCamera()->SetGameEnabled(true);

            const CgsNetwork::NetworkTexture* lpTextureToDisplay = mpNetworkManager->GetCamera()->GetRequestedCameraPicture();
            if (lpTextureToDisplay == nullptr)
            {
                lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(
                    mpNetworkManager->GetCamera()->GetRequestedPlayerID());
            }
            mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 0);
            for (s32 liPlayerIndex = 1; liPlayerIndex < 3; ++liPlayerIndex)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(nullptr, liPlayerIndex);
            }
        }
        else if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_WINNER_IMAGE)
        {
            CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
            CGS_ASSERT(mpNetworkManager->GetServerInterface() != nullptr, "mpNetworkManager->GetServerInterface()");
            CgsNetwork::ServerInterfaceGames* lpGamesComponent = mpNetworkManager->GetServerInterface()->GetGameComponent();
            CGS_ASSERT(lpGamesComponent != nullptr, "lpGamesComponent");

            s32 liDisplayedImageIndex = 0;
            if (lpGamesComponent->IsLocalPlayerInGame())
            {
                bool                              lbAlreadyShowingPhotoFinish = false;
                const CgsNetwork::NetworkTexture* lpTextureToDisplay          = nullptr;
                if (mNetworkPlayerIDToOutput != CgsNetwork::K_INVALID_PLAYER_ID)
                {
                    CgsNetwork::PlayerManager* lpPlayerManager = mpNetworkManager->GetPlayerManager();
                    if (lpPlayerManager->GetPlayerByID(mNetworkPlayerIDToOutput) != nullptr
                        || lpPlayerManager->IsLocalPlayer(mNetworkPlayerIDToOutput))
                    {
                        lpTextureToDisplay = mpNetworkManager->GetNetworkImageManager()->GetPhotoFinishImageByRoundWinner(
                            mNetworkPlayerIDToOutput, &lbAlreadyShowingPhotoFinish);
                    }
                    if (lpTextureToDisplay == nullptr)
                    {
                        lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(mNetworkPlayerIDToOutput);
                    }
                }
                if (!lbAlreadyShowingPhotoFinish)
                {
                    mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 0);
                }
                liDisplayedImageIndex = 1;
            }
            else
            {
                const CgsNetwork::NetworkTexture* lpTextureToDisplay =
                    mpNetworkManager->GetGamerPictureManager()->GetLocalPlayerGamerPictureTexture();
                if (lpTextureToDisplay != nullptr)
                {
                    mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 0);
                    liDisplayedImageIndex = 1;
                }
            }

            for (s32 liPlayerIndex = liDisplayedImageIndex; liPlayerIndex < 3; ++liPlayerIndex)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(nullptr, liPlayerIndex);
            }
        }
        else if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_SCALP_IMAGE)
        {
            // BitArray's own index tripwires (inlined on the console) cannot fire here: the set
            // index is asserted below E_ACTIVE_RACE_CAR_INDEX_COUNT and the clear loop stops at 8.
            CgsContainers::BitArray<8u> lRaceCarsWithScalps;
            lRaceCarsWithScalps.UnSetAll();

            CgsNetwork::ServerInterfaceGames* lpGamesComponent = mpNetworkManager->GetServerInterface()->GetGameComponent();
            CGS_ASSERT(lpGamesComponent != nullptr, "lpGamesComponent");

            if (lpGamesComponent->IsLocalPlayerInGame())
            {
                NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
                {
                    if (!lpGamesComponent->IsPlayerInGameByID(lPlayerID))
                    {
                        continue;
                    }

                    const CgsNetwork::NetworkTexture* lpTextureToDisplay = nullptr;
                    const NetworkPlayerID lVictimID = mpNetworkManager->GetNetworkImageManager()->GetMugshotVictimID(lPlayerID);
                    if (lVictimID != CgsNetwork::K_INVALID_PLAYER_ID)
                    {
                        lpTextureToDisplay = mpNetworkManager->GetNetworkImageManager()->GetMugshotImageByAggressor(lPlayerID);
                        if (lpTextureToDisplay == nullptr)
                        {
                            lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(lVictimID);
                        }
                    }

                    const EActiveRaceCarIndex leActiveRaceCarIndex = mpNetworkModule->GetActiveRaceCarIndex(lPlayerID);
                    if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                    {
                        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
                        lRaceCarsWithScalps.SetBit(static_cast<u32>(leActiveRaceCarIndex));
                        mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, leActiveRaceCarIndex);
                    }
                }
            }

            for (s32 liPlayerIndex = 0; liPlayerIndex < 8; ++liPlayerIndex)
            {
                if (!lRaceCarsWithScalps.IsBitSet(static_cast<u32>(liPlayerIndex)))
                {
                    mpNetworkManager->PackTextureAndSendDisplayEventToGui(nullptr, liPlayerIndex);
                }
            }
        }
        else if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_GAMERPIC)
        {
            const CgsNetwork::NetworkTexture* lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(
                mpNetworkManager->GetCamera()->GetRequestedPlayerID());
            if (lpTextureToDisplay != nullptr)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 0);
            }
            else
            {
                CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != nullptr, "mpNetworkModule->GetNetworkEventQueue()");
                BrnNetworkModuleIO::NetworkOutNoPhotoBoothGamerPic lNoPhotoBoothGamerPicEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    AsEvent(&lNoPhotoBoothGamerPicEvent), lNoPhotoBoothGamerPicEvent.GetEventType(),
                    sizeof(lNoPhotoBoothGamerPicEvent));
            }
            for (s32 liPlayerIndex = 1; liPlayerIndex < 3; ++liPlayerIndex)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(nullptr, liPlayerIndex);
            }
        }
        else if (meOutputPlayerTexture == OutputPlayerTextureEvent::E_OUTPUT_GAMERPIC_LICENCE)
        {
            for (s32 liPlayerIndex = 0; liPlayerIndex < 3; ++liPlayerIndex)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(nullptr, liPlayerIndex);
            }

            const CgsNetwork::NetworkTexture* lpTextureToDisplay = mpNetworkManager->GetGamerPictureManager()->GetGamerPictureTexture(
                mpNetworkManager->GetCamera()->GetRequestedPlayerID());
            if (lpTextureToDisplay != nullptr)
            {
                mpNetworkManager->PackTextureAndSendDisplayEventToGui(lpTextureToDisplay, 1);
            }
            else
            {
                CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != nullptr, "mpNetworkModule->GetNetworkEventQueue()");
                BrnNetworkModuleIO::NetworkOutNoPhotoBoothGamerPic lNoPhotoBoothGamerPicEvent;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    AsEvent(&lNoPhotoBoothGamerPicEvent), lNoPhotoBoothGamerPicEvent.GetEventType(),
                    sizeof(lNoPhotoBoothGamerPicEvent));
            }
        }

        if (meOutputPlayerTexture != OutputPlayerTextureEvent::E_OUTPUT_PLAYER_IMAGE)
        {
            mpNetworkManager->GetCamera()->SetGameEnabled(false);
        }
    }

    // -----------------------------------------------------------------------------------
    // UpdateLogin
    // Tick the login manager. When it finishes, go idle and tell the invite manager.
    // Signed in: prepare VoIP (the game sends the headset status), tell the GUI and the
    // network side, apply the account's telemetry choice, register the local gamer picture,
    // check the downloaded fever list, then carry on with the stats request, the pending
    // invite game or the instant free-burn, and suspend again when the sign-in asked for it.
    // Not signed in: report the disconnection (a silent sign-in stays quiet), cancel the
    // pending invites and tell the network side instant free-burn is off. Memory checks
    // bracket every step.
    // -----------------------------------------------------------------------------------
    void StateManager::UpdateLogin(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        u32 luAvailableMemory = BrnResource::GetAvailableMemory();
        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

        if (mpNetworkManager->GetLoginManager()->Update(lpInput))
        {
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

            const bool lbLoggedIn = mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn();
            const LoginManagerBase::ESignInType leSignInType = mpNetworkManager->GetLoginManager()->GetCompletedSignInType();

            meState = E_STATE_COUNT;
            mpNetworkManager->GetNetworkInviteManager()->LogInComplete(lbLoggedIn);

            if (lbLoggedIn)
            {
                // FLAG: the console logs "Finished Signing In. SignInType=" <type> "\n" to the
                // unhomed network debug stream here; the log line is not reproduced.
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                CGS_ASSERT(mpNetworkManager->GetVoIPManager()->Prepare(mpNetworkManager->GetPlayerManager(), mpNetworkManager->GetTimeManager(), mpNetworkManager->GetServerInterface(), mpNetworkManager->GetBuddyManager(), true ),
                           "mpNetworkManager->GetVoIPManager()->Prepare(mpNetworkManager->GetPlayerManager(), mpNetworkManager->GetTimeManager(), mpNetworkManager->GetServerInterface(), mpNetworkManager->GetBuddyManager(), true )");
                mpNetworkManager->GetVoIPManager()->SetGameSendingHeadsetStatus(true);
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                CgsGui::GuiEventNetworkConnected lConnectedEvent;
                lConnectedEvent.maData[0] = mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsFirstLogin();
                lConnectedEvent.maData[1] = mpNetworkManager->GetServerInterface()->GetServerInfoComponent()->IsNewsUpdated();
                mpNetworkModule->AddOutputGuiEvent(lConnectedEvent);
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                // The console's PlayerInfoData constructor runs Prepare itself; the tree's does not yet.
                PlayerInfoData lPlayerInfo;
                lPlayerInfo.Prepare();
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                CGS_ASSERT(lPlayerInfo.Prepare(), "lPlayerInfo.Prepare()");
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                BrnNetworkModuleIO::NetworkOutLocalPlayerConnected lLocalPlayerConnectedEvent;
                lLocalPlayerConnectedEvent.mPlayerID = lPlayerInfo.GetID();
                CGS_ASSERT(lPlayerInfo.GetID() != CgsNetwork::K_INVALID_PLAYER_ID,
                           "lPlayerInfo.GetID() != CgsNetwork::K_INVALID_PLAYER_ID");
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    AsEvent(&lLocalPlayerConnectedEvent), lLocalPlayerConnectedEvent.GetEventType(),
                    sizeof(lLocalPlayerConnectedEvent));
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                bool lbAgreeToShareInfoEA;
                bool lbAgreeToShareInfoPartners;
                bool lbTelemetryEnable;
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetAccountSettings(
                    &lbAgreeToShareInfoEA, &lbAgreeToShareInfoPartners, &lbTelemetryEnable);
                mpNetworkManager->GetServerInterface()->GetTelemetryComponent()->EnableTemetry(lbTelemetryEnable);

                lPlayerInfo.Prepare();
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
                CGS_ASSERT(lPlayerInfo.GetID() > 0, "lPlayerInfo.GetID() > 0");
                mpNetworkManager->GetGamerPictureManager()->AddPlayer(lPlayerInfo.GetID());

                if (mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent()->IsGamertagInFeverList(lPlayerInfo.GetName()))
                {
                    BrnNetworkModuleIO::NetworkOutCaughtFever lCaughtFeverEvent;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                        AsEvent(&lCaughtFeverEvent), lCaughtFeverEvent.GetEventType(), sizeof(lCaughtFeverEvent));
                    mpNetworkManager->SetIsDeveloper(true);
                }

                mpNetworkManager->OnLogIn();

                if (leSignInType == LoginManagerBase::E_SIGN_IN_TYPE_SILENT)
                {
                    mpNetworkManager->GetStatsManager()->RequestPlayerStats(lPlayerInfo.GetName());
                }
                else if (leSignInType == LoginManagerBase::E_SIGN_IN_TYPE_FREE_BURN_LOBBY && mbDoInviteAfterCreate)
                {
                    CreateInviteGame();
                    OnFullConnection();
                }
                else
                {
                    if (mbInstantFreeburn)
                    {
                        DoInstantFreeburn();
                    }
                    OnFullConnection();
                }
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

                if (mbSuspendAfterSignIn)
                {
                    // FLAG: the suspension type is folded away on the console (both types
                    // request the same server-interface suspend flags).
                    mpNetworkManager->GetSuspensionManager()->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                                      SuspensionFinishedCallback, this);
                    mbSuspendAfterSignIn = false;
                    meState              = E_STATE_WAIT_SUSPENSION;
                }
            }
            else if (leSignInType != LoginManagerBase::E_SIGN_IN_TYPE_SILENT)
            {
                CgsGui::GuiEventNetworkDisconnected lDisconnectedEvent;
                lDisconnectedEvent.macReason[0] = '\0';
                lDisconnectedEvent.meLastError  = mpNetworkManager->GetServerInterface()->GetLastError(CgsNetwork::E_COMPONENTS_CONNECTION);
                if (lDisconnectedEvent.meLastError == CgsNetwork::E_SERVER_INTERFACE_ERROR_NONE
                    && XUserGetSigninState(static_cast<u32>(mpNetworkManager->GetActiveControllerPort())) != KU_SIGNIN_STATE_SIGNED_IN_TO_LIVE)
                {
                    lDisconnectedEvent.meLastError = CgsNetwork::E_SERVER_INTERFACE_CONNECTION_NOT_SIGNED_IN;
                }
                mpNetworkModule->AddOutputGuiEvent(lDisconnectedEvent);

                mpNetworkManager->GetBuddyManager()->CancelInvites();
                BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
            }

            mbSuspendAfterSignIn = false;
            BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);

            if (!lbLoggedIn)
            {
                BrnNetworkModuleIO::NetworkOutInstantFreeburnEvent lInstantFreeburnEvent;
                lInstantFreeburnEvent.mbIsDoingInstantFreeburn = false;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    AsEvent(&lInstantFreeburnEvent), lInstantFreeburnEvent.GetEventType(), sizeof(lInstantFreeburnEvent));
            }
        }

        BRN_RESOURCE_MEMORY_CHECK(luAvailableMemory);
    }
} // namespace BrnNetwork
