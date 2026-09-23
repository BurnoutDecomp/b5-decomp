#include "types.hpp"

#include <cstdint>                                                                         // intptr_t
#include <cstring>                                                                         // strlen / strncpy

#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"                                           // AddOutputGuiEvent / Get*EventQueue
#include "GameSource/Network/BrnNetworkModuleIO.h"                                         // OutputBuffer status / lobby / results interfaces
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                 // NetworkOut* records
#include "GameSource/Network/BrnNetworkPlayer.h"                                           // IsEliminated / IsFirstUpdateMessage
#include "GameSource/Network/BrnNetworkGameParams.h"                                       // GameParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                     // PlayerParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                        // PlayerInfoData
#include "GameSource/GameState/BrnGameEvents.h"                                            // LocalPlayerDisconnectedEvent
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                                      // GuiEventNetworkPlayerImage / CustomMatchResults / CamStatus
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"                           // GuiEventNetworkPlayerStats
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"                  // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                                // GuiEventShowLoginQuestion / GuiEventNetworkDisconnected
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"
#include "GameShared/GameClasses/Network/ServerInterface/CgsServerInterfaceErrors.h"       // EServerInterfaceError
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceEvents.h" // EServerInterfaceEvent
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceTelemetry.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h" // SetSessionFlags

// ============================================================================================
// BrnNetwork::BrnNetworkManager -- event handling and GUI / game output (partfile).
//
// The two raised-event entry points (the login state machine's and the server interface's),
// the player-manager event callback, and the per-frame writers of the player status, lobby
// and results interfaces, the game-parameter event and the GUI stats / image events.
// ============================================================================================

namespace BrnNetwork
{
    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::TriggerEventFromLogin
    //
    // Every "show question" login event becomes a GuiEventShowLoginQuestion carrying the
    // matching question; PROCEED needs no GUI; anything else is a bad event.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::TriggerEventFromLogin(ELoginEvent leEvent, void* lpEventData)
    {
        (void)lpEventData;

        CgsGui::GuiEventShowLoginQuestion lShowQuestionEvent;

        switch (leEvent)
        {
        case E_LOGIN_EVENT_SHOW_TOS:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_TOS;
            break;
        case E_LOGIN_EVENT_SHOW_CREATE_ACCOUNT:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_CREATE_ACCOUNT;
            break;
        case E_LOGIN_EVENT_SHOW_SHARE:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_SHARE;
            break;
        case E_LOGIN_EVENT_SHOW_OPEN_US_ACCOUNT:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_OPEN_US_ACCOUNT;
            break;
        case E_LOGIN_EVENT_SHOW_NO_AGREEMENT:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT;
            break;
        case E_LOGIN_EVENT_SHOW_SIGN_IN:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN;
            break;
        case E_LOGIN_EVENT_SHOW_CHAT_RESTRICTED:
            lShowQuestionEvent.meLoginQuestion = CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION;
            break;
        case E_LOGIN_EVENT_PROCEED:
            return;
        default:
            CGS_ASSERT(false, "Invalid login event");
            return;
        }

        GetNetworkModule()->AddOutputGuiEvent(lShowQuestionEvent);
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::UpdateMenuDataFromPlayerParams
    //
    // A player's lobby parameters changed: refresh that player's menu data (marked man, free
    // burn car, paint) from them and, for a remote player, tell the game its car and paint.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::UpdateMenuDataFromPlayerParams(s32 liServerInterfacePlayerIndex)
    {
        PlayerParams lPlayerParams;
        lPlayerParams.Prepare();
        GetServerInterface()->GetGameComponent()->GetPlayerParametersByIndex(liServerInterfacePlayerIndex,
                                                                             &lPlayerParams);

        PlayerMenuData* lpPlayerMenuData =
            static_cast<PlayerMenuData*>(GetPlayerManager()->GetMenuDataByID(lPlayerParams.GetID()));
        CGS_ASSERT(lpPlayerMenuData != NULL, "Couldn't find menu data for playerID ");

        lpPlayerMenuData->mMarkedManID = lPlayerParams.GetMarkedPlayerID();
        lPlayerParams.GetFreeBurnCarID(&lpPlayerMenuData->mFreeBurnCarId);
        lpPlayerMenuData->mu16FreeburnCarColourIndex   = lPlayerParams.GetCarColourIndex();
        lpPlayerMenuData->mu16FreeburnPaintFinishIndex = lPlayerParams.GetPaintFinishIndex();
        lpPlayerMenuData->mfUnknown4C = lPlayerParams.IsCarDeformed() ? 0.85f : 0.0f;
        CGS_ASSERT(lpPlayerMenuData->mu16FreeburnPaintFinishIndex < 4, "Invalid Paint Finish: ");

        PlayerInfoData lPlayerInfo;
        lPlayerInfo.Prepare();
        GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);

        if (lPlayerParams.GetID() != lPlayerInfo.GetID())
        {
            BrnNetworkModuleIO::NetworkOutPlayerChangedCarEvent lChangedCarEvent;
            lChangedCarEvent.SetNetworkPlayerID(lPlayerParams.GetID());
            lChangedCarEvent.SetModelID(lpPlayerMenuData->mFreeBurnCarId);
            lChangedCarEvent.SetWheelID(lpPlayerMenuData->mFreeBurnWheelId);
            lChangedCarEvent.mf10 = lpPlayerMenuData->mfUnknown4C;
            GetNetworkModule()->GetNetworkEventQueue()->AddEvent(&lChangedCarEvent,
                                                                 lChangedCarEvent.GetEventType());

            BrnNetworkModuleIO::NetworkOutPlayerChangedCarColourEvent lCarColourEvent;
            lCarColourEvent.SetNetworkPlayerID(lPlayerParams.GetID());
            lCarColourEvent.SetActiveRaceCarIndex(
                GetNetworkModule()->GetActiveRaceCarIndex(lPlayerParams.GetID()));
            lCarColourEvent.SetCarColourIndex(lpPlayerMenuData->mu16FreeburnCarColourIndex);
            lCarColourEvent.SetPaintFinishIndex(lpPlayerMenuData->mu16FreeburnPaintFinishIndex);
            GetNetworkModule()->GetNetworkEventQueue()->AddEvent(&lCarColourEvent,
                                                                 lCarColourEvent.GetEventType());
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputPlayerStatsToGui
    //
    // Send one player's online stats to the GUI. lPlayerID == invalid means "retry the
    // pending request", which is only resent when the cached stats are newer than the last
    // ones sent (or have just been recalculated). The name and rank come from the lobby
    // parameters for a player in the game, from the local player info otherwise. When the
    // stats are not downloaded yet the request is parked in mPlayerIDStatsGet.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputPlayerStatsToGui(NetworkPlayerID lPlayerID)
    {
        if (!GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
        {
            return;
        }

        if (lPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
        {
            lPlayerID = GetCamera()->GetRequestedPlayerID();
            if (lPlayerID == CgsNetwork::K_INVALID_PLAYER_ID)
            {
                return;
            }

            const NetworkPlayerStats* lpPlayerStats = GetStatsManager()->GetPlayerStatsByID(lPlayerID);
            if (lpPlayerStats == NULL)
            {
                return;
            }

            if (!(lpPlayerStats->GetTimeStamp() > mLastStatsSentToOnlinePlayStamp) &&
                !lpPlayerStats->IsCalculated())
            {
                return;
            }
            mPlayerIDStatsGet = lPlayerID;
        }

        const char* lpcPlayerName;
        s32         liRank;

        // The local player's name is read out of lPlayerInfo after the branch, so the record
        // lives at function scope (the console frame keeps it; a block scope would not).
        PlayerInfoData lPlayerInfo;

        CgsNetwork::ServerInterfaceGames* lpGamesComponent = GetServerInterface()->GetGameComponent();
        if (lpGamesComponent->IsPlayerInGameByID(lPlayerID))
        {
            PlayerParams lPlayerParams;
            lPlayerParams.Prepare();
            lpGamesComponent->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);
            lpcPlayerName = lPlayerParams.GetName();
            liRank        = lPlayerParams.GetRank();
        }
        else
        {
            lPlayerInfo.Prepare();
            GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
            CGS_ASSERT(lPlayerInfo.GetID() == lPlayerID, "Can only get local player stats when not in a game");
            lpcPlayerName = lPlayerInfo.GetName();
            liRank        = lPlayerInfo.GetRank();
        }

        const NetworkPlayerStats* lpPlayerStats = GetStatsManager()->GetPlayerStatsByName(lpcPlayerName);
        if (lpPlayerStats != NULL && lpPlayerStats->GetStatus() == NetworkPlayerStats::E_STATS_AGE_CURRENT)
        {
            BrnGui::GuiEventNetworkPlayerStats lPlayerStatsEvent;
            mPlayerIDStatsGet = CgsNetwork::K_INVALID_PLAYER_ID;

            CGS_ASSERT(std::strlen(lpcPlayerName) < sizeof(lPlayerStatsEvent.macPlayerName), "String too long: ");
            std::strncpy(lPlayerStatsEvent.macPlayerName, lpcPlayerName, sizeof(lPlayerStatsEvent.macPlayerName));
            lPlayerStatsEvent.mPlayerID    = lPlayerID;
            static_cast<BrnNetwork::NetworkPlayerStats&>(lPlayerStatsEvent) = *lpPlayerStats;
            lPlayerStatsEvent.miWorldRank  = liRank;
            GetNetworkModule()->AddOutputGuiEvent(lPlayerStatsEvent);

            mLastStatsSentToOnlinePlayStamp = lpPlayerStats->GetTimeStamp();
        }
        else
        {
            mPlayerIDStatsGet = lPlayerID;
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::PackTextureAndSendDisplayEventToGui
    //
    // Hand a received player image to the GUI for display in the given slot.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::PackTextureAndSendDisplayEventToGui(const CgsNetwork::NetworkTexture* lpTexture,
                                                                s32 liTextureIndex)
    {
        CgsDev::PerfMonCpu::StartMonitor(miPackTextureToSendToGuiPM);

        BrnGui::GuiEventNetworkPlayerImage lEvent;
        lEvent.mpTexture      = lpTexture;
        lEvent.miTextureIndex = liTextureIndex;
        GetNetworkModule()->AddOutputGuiEvent(lEvent);

        CgsDev::PerfMonCpu::StopMonitor(miPackTextureToSendToGuiPM);
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputPlayerStatusInfo
    //
    // Rebuild the in-game player status interface and the lobby player status interface of
    // the output buffer from every player in the game: identity, race-car slot, marked man,
    // voice and camera state, stats, the live-revenge relationship with the local player,
    // host / local / in-world flags, the lobby car selection and ready state. Then publish
    // the player counts and the local camera status.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputPlayerStatusInfo(BrnNetworkModuleIO::OutputBuffer* lpOutput)
    {
        PlayerParams    lPlayerParams;
        GameParams      lGameParams;   // constructed and destroyed, never read
        PlayerMenuData* lpLocalPlayerMenuData = NULL;

        CGS_ASSERT(lpOutput != NULL, "lpOutput");

        CgsNetwork::PlayerManager*        lpPlayerManager  = GetPlayerManager();
        CgsNetwork::ServerInterfaceGames* lpGamesComponent = GetServerInterface()->GetGameComponent();

        const NetworkPlayerID lLocalPlayerID = lpPlayerManager->GetLocalPlayerID();

        if (lpGamesComponent->IsPlayerInGameByID(lLocalPlayerID))
        {
            lpOutput->GetInGamePlayerStatusInterface()->SetGameName(lpGamesComponent->GetGameName());
            lpOutput->GetInGamePlayerStatusInterface()->SetLocalPlayerIsHost(lpGamesComponent->IsLocalPlayerHost());

            lpLocalPlayerMenuData = static_cast<PlayerMenuData*>(lpPlayerManager->GetMenuDataByID(lLocalPlayerID));
            CGS_ASSERT(lpLocalPlayerMenuData != NULL, "lpLocalPlayerMenuData");
        }

        s32             liPlayerStatusDataIndex = 0;
        NetworkPlayerID lPlayerID               = CgsNetwork::K_INVALID_PLAYER_ID;

        while (lpPlayerManager->GetNextPlayerID(&lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
        {
            if (!lpGamesComponent->IsPlayerInGameByID(lPlayerID))
            {
                continue;
            }

            BrnNetworkModuleIO::InGamePlayerStatusData* lpPlayerStatusData =
                lpOutput->GetInGamePlayerStatusInterface()->GetPlayerStatusDataForWriting(liPlayerStatusDataIndex);
            PlayerMenuData* lpPlayerMenuData =
                static_cast<PlayerMenuData*>(lpPlayerManager->GetMenuDataByID(lPlayerID));

            lPlayerParams.PreparePattern();
            lpGamesComponent->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);

            lpPlayerStatusData->Clear();
            lpPlayerStatusData->mPlayerName.Construct(lpPlayerMenuData->macName);
            lpPlayerStatusData->mNetworkPlayerID      = lPlayerID;
            lpPlayerStatusData->meActiveRaceCarIndex = GetNetworkModule()->GetActiveRaceCarIndex(lPlayerID);

            if (lpPlayerMenuData->mMarkedManID != CgsNetwork::K_INVALID_PLAYER_ID &&
                lpPlayerManager->GetMenuDataByID(lpPlayerMenuData->mMarkedManID) != NULL)
            {
                lpPlayerStatusData->mMarkedManPlayerID = lpPlayerMenuData->mMarkedManID;
                lpPlayerStatusData->meMarkedManActiveRaceCarIndex =
                    GetNetworkModule()->GetActiveRaceCarIndex(lpPlayerMenuData->mMarkedManID);
            }

            CGS_ASSERT(lpPlayerMenuData->mu8HeadsetStatus < CgsNetwork::E_NETWORK_HEADSET_PLAYER_STATUS_COUNT,
                       "lpPlayerMenuData->mu8HeadsetStatus < CgsNetwork::E_NETWORK_HEADSET_PLAYER_STATUS_COUNT");
            lpPlayerStatusData->meVOIPStatus = lpPlayerMenuData->mu8HeadsetStatus;

            const NetworkPlayerStats* lpNetworkPlayerStats = GetStatsManager()->GetPlayerStatsByID(lPlayerID);
            if (lpNetworkPlayerStats != NULL)
            {
                lpPlayerStatusData->mPlayerStats = *lpNetworkPlayerStats;
            }

            if (lPlayerID != lLocalPlayerID)
            {
                const LiveRevengeRelationship* lpRelationship =
                    GetLiveRevengeManager()->GetNonConstRevengeRelation(lPlayerID);
                if (lpRelationship != NULL)
                {
                    lpPlayerStatusData->mLiveRevengeRelationship = *lpRelationship;
                }
            }

            lpPlayerStatusData->mbMarkedMan = (lpLocalPlayerMenuData->mMarkedManID == lPlayerParams.GetID());

            if (lLocalPlayerID == lPlayerID)
            {
                lpPlayerStatusData->mbIsEliminated       = false;
                lpPlayerStatusData->mbIsLocalPlayer      = true;
                lpPlayerStatusData->mbIsInLocalGameWorld = true;
            }
            else
            {
                lpPlayerStatusData->mbIsLocalPlayer = false;
                const BrnNetworkPlayer* lpNetworkPlayer =
                    static_cast<const BrnNetworkPlayer*>(lpPlayerManager->GetPlayerByID(lPlayerID));
                lpPlayerStatusData->mbIsInLocalGameWorld = !lpNetworkPlayer->IsFirstUpdateMessage();
                lpPlayerStatusData->mbIsEliminated       = lpNetworkPlayer->IsEliminated();
            }

            lpPlayerStatusData->mbIsHost = (lPlayerID == lpGamesComponent->GetHostPlayerID());

            CGS_ASSERT(lpPlayerMenuData->meCameraStatus >= BrnNetwork::E_CAMERA_STATUS_NONE,
                       "lpPlayerMenuData->meCameraStatus >= BrnNetwork::E_CAMERA_STATUS_NONE");
            CGS_ASSERT(lpPlayerMenuData->meCameraStatus < BrnNetwork::E_CAMERA_STATUS_COUNT,
                       "lpPlayerMenuData->meCameraStatus < BrnNetwork::E_CAMERA_STATUS_COUNT");
            lpPlayerStatusData->meCameraStatus = lpPlayerMenuData->meCameraStatus;

            BrnNetworkModuleIO::LobbyPlayerStatusData* lpPlayerLobbyData =
                lpOutput->GetOnlineLobbyPlayerStatusInterface()->GetPlayerLobbyData(liPlayerStatusDataIndex);
            lpPlayerLobbyData->Clear();
            lpPlayerLobbyData->mbLocalPlayer          = (lPlayerID == lLocalPlayerID);
            lpPlayerLobbyData->mbIsHost               = lpPlayerStatusData->mbIsHost;
            lpPlayerLobbyData->mPlayerID              = lPlayerParams.GetID();
            lpPlayerLobbyData->mbFinalSelection       = lpPlayerMenuData->mbFinalCarSelection;
            lpPlayerLobbyData->mSelectedCarID         = lpPlayerMenuData->mCarId;
            lpPlayerLobbyData->mSelectedWheelID       = lpPlayerMenuData->mWheelId;
            lpPlayerLobbyData->muCarColourIndex       = lPlayerParams.GetCarColourIndex();
            lpPlayerLobbyData->muCarPaintFinishIndex  = lPlayerParams.GetPaintFinishIndex();
            lpPlayerLobbyData->miPlayerColourIndex    = lPlayerParams.GetPlayerColourIndex();
            lpPlayerLobbyData->mbIsCriterion          = lPlayerParams.IsDeveloper();

            if (lpPlayerManager->IsLocalPlayer(lPlayerID))
            {
                lpPlayerLobbyData->meGameConnectionType = 0;
                lpPlayerLobbyData->meVoipConnectionType = 0;
            }
            else
            {
                const CgsNetwork::ConnectionData lConnectionData =
                    lpPlayerManager->GetPlayerByID(lPlayerID)->GetConnectionData();
                lpPlayerLobbyData->meGameConnectionType = lConnectionData.meGameConnectionType;
                lpPlayerLobbyData->meVoipConnectionType = lConnectionData.meVoipConnectionType;
            }

            if (lPlayerParams.IsPlaying())
            {
                lpPlayerLobbyData->meReadyStatus = BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_PLAYING;
            }
            else if (lpPlayerLobbyData->mbIsHost)
            {
                lpPlayerLobbyData->meReadyStatus = BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_HOST;
            }
            else
            {
                lpPlayerLobbyData->meReadyStatus = lPlayerParams.IsReady()
                    ? BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_READY
                    : BrnNetworkModuleIO::LobbyPlayerStatusData::E_READY_STATUS_NOT_READY;
            }

            lpPlayerLobbyData->mePlayerTeam = lPlayerParams.GetPlayerTeam();
            ++liPlayerStatusDataIndex;
        }

        lpOutput->GetInGamePlayerStatusInterface()->SetNumPlayers(liPlayerStatusDataIndex);
        lpOutput->GetInGamePlayerStatusInterface()->SetTotalNumberPlayers(
            lpPlayerManager->GetTotalNumberPlayers(CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED));

        BrnGui::GuiEventCamStatus leCamStatusEvent;
        leCamStatusEvent.meCamStatus = static_cast<ECameraStatus>(GetCamera()->GetLocalCameraStatus());
        GetNetworkModule()->AddOutputGuiEvent(leCamStatusEvent);
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputGameParameters
    //
    // Publish the current game's parameters (mode, security, boost, vehicle rules, rounds,
    // time limit, ranked / infinite boost / traffic flags and the selected routes) as a
    // NetworkOutGameParamsChanged event.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputGameParameters()
    {
        BrnNetworkModuleIO::NetworkOutGameParamsChanged lEvent;
        GameParams lGameParams;

        lEvent.Construct();
        lGameParams.Prepare();
        GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        lEvent.mbInfiniteBoost     = lGameParams.InfiniteBoost();
        lEvent.miVehicleClass      = lGameParams.VehicleLevelLimit();
        lEvent.mbRanked            = lGameParams.IsRankedGame();
        lEvent.meSecurity          = lGameParams.Security();
        lEvent.meGameMode          = lGameParams.GameMode();
        lEvent.mePreviousGameMode  = lGameParams.PreviousGameMode();
        GetSelectedRoutesManager()->GetRouteData(lEvent.maEvents);
        lEvent.miNumRounds         = lGameParams.NumberRounds();
        lEvent.mbTrafficOn         = lGameParams.IsTrafficOn();
        lEvent.meBoostType         = lGameParams.BoostType();
        lEvent.mbTrafficCheckingOn = lGameParams.IsTrafficCheckingOn();
        lEvent.miNumRunnerCrashes  = lGameParams.NumRunnerCrashes();
        lEvent.meVehicleChoice     = lGameParams.VehicleChoice();
        lEvent.miTimeLimit         = lGameParams.TimeLimit();

        GetNetworkModule()->GetNetworkEventQueue()->AddEvent(&lEvent, lEvent.GetEventType());
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::TriggerEventFromServerInterface
    //
    // The server interface's events, routed to the managers that care.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::TriggerEventFromServerInterface(CgsNetwork::EServerInterfaceEvent leEvent, void* lpData)
    {
        switch (leEvent)
        {
        case CgsNetwork::E_SERVER_INTERFACE_GENERAL_EVENT_LOBBY_API_CREATED:
            GetPlayerManager()->OnLobbyApiCreated();
            break;

        case CgsNetwork::E_SERVER_INTERFACE_CONNECTION_EVENT_CONNECTED:
            GetNetworkNotificationManager()->Connect();
            GetServerInterface()->GetTelemetryComponent()->CaptureEvent(E_TELEMETRY_NETWORK_CONNECT, NULL);
            break;

        case CgsNetwork::E_SERVER_INTERFACE_CONNECTION_EVENT_DISCONNECTED:
            if (!mbNotifyGameOfDisconnectAfterDiskError)
            {
                CgsGui::GuiEventNetworkDisconnected lDisconnectedEvent;
                lDisconnectedEvent.macReason[0] = 0;

                if (lpData != NULL)
                {
                    const char* lpcReason = static_cast<const char*>(lpData);
                    CGS_ASSERT(std::strlen(lpcReason) < CgsGui::GuiEventNetworkDisconnected::KI_MAX_REASON_LENGTH,
                               "String too long: ");
                    std::strncpy(lDisconnectedEvent.macReason, lpcReason,
                                 CgsGui::GuiEventNetworkDisconnected::KI_MAX_REASON_LENGTH);
                }

                if (GetLoginManager()->GetSignInType() == LoginManagerBase::E_SIGN_IN_TYPE_SILENT)
                {
                    lDisconnectedEvent.meLastError = CgsNetwork::E_SERVER_INTERFACE_ERROR_NONE;
                }
                else
                {
                    CGS_ASSERT(GetServerInterface() != NULL, "GetServerInterface()");
                    lDisconnectedEvent.meLastError =
                        GetServerInterface()->GetLastError(CgsNetwork::E_COMPONENTS_CONNECTION);

                    if (lDisconnectedEvent.meLastError == CgsNetwork::E_SERVER_INTERFACE_ERROR_NONE)
                    {
                        if (GetNetworkAdapter()->HadDuplicateLogin())
                        {
                            lDisconnectedEvent.meLastError =
                                CgsNetwork::E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT_DUPLICATE_LOGIN;
                        }
                        else if (GetServerInterface()->GetConnectionComponent()->IsConnectedToNetworkService())
                        {
                            lDisconnectedEvent.meLastError = CgsNetwork::E_SERVER_INTERFACE_CONNECTION_ERROR_DISCONNECT;
                        }
                        else
                        {
                            lDisconnectedEvent.meLastError = CgsNetwork::E_SERVER_INTERFACE_CONNECTION_NOT_SIGNED_IN;
                        }
                    }
                }

                GetNetworkModule()->AddOutputGuiEvent(lDisconnectedEvent);

                BrnGameState::GameStateModuleIO::LocalPlayerDisconnectedEvent lLocalPlayerDisconnectedEvent;
                GetNetworkModule()->GetGameEventQueue()->AddEvent(&lLocalPlayerDisconnectedEvent,
                                                                  BrnGameState::GameStateModuleIO::E_EVENT_LOCAL_PLAYER_DISCONNECTED);

                BrnNetworkModuleIO::NetworkOutLocalPlayerDisconnected lNwDisconnectedEvent;
                GetNetworkModule()->GetNetworkEventQueue()->AddEvent(&lNwDisconnectedEvent,
                                                                     lNwDisconnectedEvent.GetEventType());
            }

            GetStateManager()->Disconnected();
            GetHostMigrationManager()->Disconnected();
            GetStartTimeManager()->Disconnected();
            GetLoginManager()->Disconnected();
            GetLaunchManager()->Disconnected();
            GetMatchMakingManager()->Disconnected();
            GetSuspensionManager()->Disconnected();
            GetTrafficManager()->Disconnected();
            GetStandingsManager()->Disconnected();
            GetPostRoundManager()->Disconnected();
            GetCamera()->Disconnected();
            GetAggressiveDrivingManager()->Disconnected();
            GetLiveRevengeManager()->Disconnected();
            GetDirtyTrickManager()->Disconnected();
            GetNetworkImageManager()->Disconnected();
            GetGamerPictureManager()->Disconnected();
            GetSelectedRoutesManager()->Disconnected();
            GetNetworkNotificationManager()->Disconnect();
            GetRoadRulesManager()->Disconnected();
            GetChallengeSuccessManager()->Disconnected();
            GetStatsManager()->Disconnected();
            GetMarkedManManager()->Disconnected();
            GetScoreboardManager()->Disconnected();
            GetGamerCardManager()->Disconnected();
            GetAutoLoginManager()->Disconnected();
            GetTeamSelectionManager()->Disconnected();
            GetBuddyManager()->CancelInvites();
            GetPlayerManager()->Disconnected();
            GetVoIPManager()->Release();
            GetBuddyManager()->UpdateJoinableStatus();

            GetVersionDisplay()->SetGameServerGame(false);
            mPlayerIDStatsGet = CgsNetwork::K_INVALID_PLAYER_ID;
            break;

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_SEARCH_UPDATED:
        {
            BrnGui::GuiEventNetworkCustomMatchResults lSearchResults;
            GameParams lGameParams;

            CgsNetwork::ServerInterfaceGames* lpGamesComponent = GetServerInterface()->GetGameComponent();
            const s32 liNumGames = lpGamesComponent->GetNumberOfFoundGames();
            s32       liSearchGamesIndex = 0;

            lSearchResults.miNumGames = liNumGames;

            for (s32 liGamesIndex = 0; liGamesIndex < liNumGames; ++liGamesIndex)
            {
                if (liGamesIndex >= BrnGui::GuiEventNetworkCustomMatchResults::KI_MAX_NUM_GAMES)
                {
                    break;
                }

                lGameParams.Prepare();
                lpGamesComponent->GetFoundGame(liGamesIndex, &lGameParams);

                const char* lpcGameName = lGameParams.GetName();
                CGS_ASSERT(std::strlen(lpcGameName) < sizeof(lSearchResults.maacGameNames[liSearchGamesIndex]),
                           "String too long: ");
                std::strncpy(lSearchResults.maacGameNames[liSearchGamesIndex], lpcGameName,
                             sizeof(lSearchResults.maacGameNames[liSearchGamesIndex]));

                lSearchResults.maeGameMode[liSearchGamesIndex]         = lGameParams.GameMode();
                lSearchResults.maePreviousGameMode[liSearchGamesIndex] = lGameParams.PreviousGameMode();
                lSearchResults.maiMaxNumPlayers[liSearchGamesIndex]    = lGameParams.GetMaxPlayers();
                lSearchResults.maiNumPlayers[liSearchGamesIndex]       = lGameParams.GetNumberOfPlayers();

                // FLAG: a leading '@' in the game's host-name field takes one off both counts.
                if (lGameParams.GetHostName()[0] == '@')
                {
                    --lSearchResults.maiMaxNumPlayers[liSearchGamesIndex];
                    --lSearchResults.maiNumPlayers[liSearchGamesIndex];
                }

                lSearchResults.maiFoundGameIndex[liSearchGamesIndex] = liGamesIndex;
                lSearchResults.maiGameFlags[liSearchGamesIndex]      = 0;

                // The console tests the player count at the found-game index, not the search slot.
                if (lSearchResults.maiNumPlayers[liGamesIndex] != 0 && lGameParams.NetworkVersion() == 2)
                {
                    ++liSearchGamesIndex;
                }
                else
                {
                    --lSearchResults.miNumGames;
                }
            }

            GetNetworkModule()->AddOutputGuiEvent(lSearchResults);
            break;
        }

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_KICKED:
        {
            const CgsNetwork::EKickReason leKickReason = *static_cast<const CgsNetwork::EKickReason*>(lpData);
            GetStartTimeManager()->PrepareStartTime();
            GetStateManager()->SuspendToLeaveGame(E_LEFT_GAME_REASON_KICKED, leKickReason);
            GetBuddyManager()->UpdateJoinableStatus();
            break;
        }

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_ADDED:
        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_REMOVED:
            GetBuddyManager()->UpdateJoinableStatus();
            break;

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_PLAYER_PARAMETERS_CHANGED:
        {
            const s32 liServerInterfacePlayerIndex = static_cast<s32>(reinterpret_cast<intptr_t>(lpData));
            UpdateMenuDataFromPlayerParams(liServerInterfacePlayerIndex);
            break;
        }

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_GAME_PARAMETERS_CHANGED:
        {
            GameParams lGameParams;
            lGameParams.Prepare();

            CgsNetwork::ServerInterfaceGames* lpGamesComponent = GetServerInterface()->GetGameComponent();
            CGS_ASSERT(lpGamesComponent != NULL, "lpGamesComponent");
            lpGamesComponent->GetGameParameters(&lGameParams);

            if (!lpGamesComponent->IsGameStarted() &&
                lpGamesComponent->GetHostPlayerID() != GetPlayerManager()->GetHostPlayerID())
            {
                GetPlayerManager()->SetHostPlayerID(lpGamesComponent->GetHostPlayerID());
            }

            OutputGameParameters();
            GetBuddyManager()->UpdateJoinableStatus();

            // FLAG: the security-mode and session-flag names are unrecovered.
            static_cast<CgsNetwork::ServerInterfaceGamesX360*>(GetServerInterface()->GetGameComponent())
                ->SetSessionFlags(lGameParams.Security() == 2 ? 0x600 : 0x400);
            break;
        }

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_GAME_DELETED:
            if (GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
            {
                GetStateManager()->SuspendToLeaveGame(E_LEFT_GAME_REASON_GAME_DELETED, CgsNetwork::E_KICKREASON_COUNT);

                BrnGui::GuiEventNetworkPlayerImage lNetworkPlayerImageEvent;
                lNetworkPlayerImageEvent.mpTexture      = NULL;
                lNetworkPlayerImageEvent.miTextureIndex = -1;
                GetNetworkModule()->AddOutputGuiEvent(lNetworkPlayerImageEvent);

                PlayerInfoData lPlayerInfo;
                lPlayerInfo.Prepare();
                GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
                GetCamera()->RequestFeed(lPlayerInfo.GetID());
                GetStartTimeManager()->PrepareStartTime();
            }
            GetBuddyManager()->UpdateJoinableStatus();
            break;

        case CgsNetwork::E_SERVER_INTERFACE_GAMES_EVENT_GAME_ID_CHANGED:
            GetStateManager()->OnGameIDChanged();
            break;

        default:
            break;
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::PlayerManagerEventCallback
    //
    // The player manager's lifecycle events. The event data word is the player id; the user
    // data is the network manager.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::PlayerManagerEventCallback(CgsNetwork::PlayerManager::EEvent leEvent,
                                                       void* lpEventData, void* lpUserData)
    {
        BrnNetworkManager* lpNetworkManager = static_cast<BrnNetworkManager*>(lpUserData);
        const NetworkPlayerID lPlayerID = static_cast<NetworkPlayerID>(reinterpret_cast<intptr_t>(lpEventData));

        switch (leEvent)
        {
        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_ADDED:
        {
            PlayerParams lPlayerParams;
            const bool   lbIsLocalPlayer = lpNetworkManager->IsLocalPlayer(lPlayerID);

            lpNetworkManager->GetHostMigrationManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetStartTimeManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetStandingsManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetVoIPManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetTrafficManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetAggressiveDrivingManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetLiveRevengeManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetStatsManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetDirtyTrickManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetNetworkImageManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetSelectedRoutesManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetMarkedManManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetRoadRulesManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetChallengeSuccessManager()->AddPlayer(lPlayerID);
            lpNetworkManager->GetTeamSelectionManager()->AddPlayer(lPlayerID);

            PlayerMenuData* lpMenuData =
                static_cast<PlayerMenuData*>(lpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
            CGS_ASSERT(lpMenuData != NULL, "lpMenuData");

            lPlayerParams.Prepare();
            lpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID,
                                                                                                     &lPlayerParams);

            if (lbIsLocalPlayer)
            {
                lpNetworkManager->OutputGameParameters();
            }
            else
            {
                lpNetworkManager->GetCamera()->AddPlayer(lPlayerID);
                lpNetworkManager->GetGamerPictureManager()->AddPlayer(lPlayerID);
                lpNetworkManager->GetBuddyManager()->AddPlayerToHistory(&lPlayerParams);
            }

            lPlayerParams.GetFreeBurnCarID(&lpMenuData->mFreeBurnCarId);
            lpMenuData->meCameraStatus               = E_CAMERA_STATUS_NONE;
            lpMenuData->mCarId                       = 0;
            lpMenuData->mWheelId                     = 0;
            lpMenuData->mMarkedManID                 = CgsNetwork::K_INVALID_PLAYER_ID;
            lpMenuData->mu16FreeburnCarColourIndex   = lPlayerParams.GetCarColourIndex();
            lpMenuData->mu16FreeburnPaintFinishIndex = lPlayerParams.GetPaintFinishIndex();
            lpMenuData->mfUnknown4C                  = lPlayerParams.IsCarDeformed() ? 0.85f : 0.0f;
            lpMenuData->mu16CarColourIndex           = PlayerMenuData::KU_INVALID_COLOUR_INDEX;
            lpMenuData->mu16PaintFinishIndex         = PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX;
            CGS_ASSERT(lpMenuData->mu16FreeburnPaintFinishIndex < 4, "Invalid Paint Finish: ");

            BrnNetworkModuleIO::NetworkOutPlayerAddedEvent lPlayerAdded;
            lPlayerAdded.SetNetworkPlayerID(lPlayerID);
            lPlayerAdded.SetModelID(lpMenuData->mFreeBurnCarId);
            lPlayerAdded.SetWheelID(lpMenuData->mFreeBurnWheelId);
            lPlayerAdded.SetTeam(lPlayerParams.GetPlayerTeam());
            lPlayerAdded.mf18 = lpMenuData->mfUnknown4C;
            lPlayerAdded.SetLocalPlayer(lbIsLocalPlayer);
            lPlayerAdded.SetCarColourIndex(lpMenuData->mu16FreeburnCarColourIndex);
            lPlayerAdded.SetCarPaintFinishIndex(lpMenuData->mu16FreeburnPaintFinishIndex);
            lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(&lPlayerAdded,
                                                                                   lPlayerAdded.GetEventType());

            lpNetworkManager->GetStateManager()->PlayerAdded();
            break;
        }

        case CgsNetwork::PlayerManager::E_EVENT_START_PLAYER_REMOVAL:
            lpNetworkManager->GetHostMigrationManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetStartTimeManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetVoIPManager()->RemovePlayer(lpNetworkManager->GetServerInterface(), lPlayerID);
            lpNetworkManager->GetStandingsManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetTrafficManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetAggressiveDrivingManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetLiveRevengeManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetDirtyTrickManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetNetworkImageManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetSelectedRoutesManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetMarkedManManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetRoadRulesManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetChallengeSuccessManager()->RemovePlayer(lPlayerID);
            lpNetworkManager->GetTeamSelectionManager()->RemovePlayer(lPlayerID);

            if (!lpNetworkManager->IsLocalPlayer(lPlayerID))
            {
                CGS_ASSERT(lpNetworkManager->GetPlayerManager() != NULL, "lpNetworkManager->GetPlayerManager()");
                const PlayerMenuData* lpPlayerMenuData = static_cast<const PlayerMenuData*>(
                    lpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
                CGS_ASSERT(lpPlayerMenuData != NULL, "lpPlayerMenuData");

                CgsNetwork::PlayerName lPlayerName;
                lPlayerName.Construct(lpPlayerMenuData->macName);

                lpNetworkManager->GetCamera()->RemovePlayer(lPlayerID);
                lpNetworkManager->GetStatsManager()->RemovePlayer(lPlayerID);
                lpNetworkManager->GetGamerPictureManager()->RemovePlayer(lPlayerID);

                BrnNetworkModuleIO::NetworkOutPlayerRemovedEvent lPlayerRemoved;
                lPlayerRemoved.SetNetworkPlayerID(lPlayerID);
                CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                    lpNetworkManager->GetServerInterface()->GetGameComponent();
                lPlayerRemoved.SetIsLocalPlayerInGame(lpGamesComponent->IsLocalPlayerInGame());
                lPlayerRemoved.SetIsLocalPlayerLeavingGame(lpGamesComponent->IsLocalPlayerLeavingGame());
                lPlayerRemoved.SetPlayerName(&lPlayerName);
                lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(&lPlayerRemoved,
                                                                                       lPlayerRemoved.GetEventType());
            }

            lpNetworkManager->GetStateManager()->PlayerRemoved();
            break;

        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_FINALISED:
        {
            if (!lpNetworkManager->IsLocalPlayer(lPlayerID))
            {
                lpNetworkManager->GetCamera()->PlayerFinalised(lPlayerID);
            }
            lpNetworkManager->GetVoIPManager()->RemotePlayerFinalised(lpNetworkManager->GetServerInterface(), lPlayerID);
            lpNetworkManager->GetLiveRevengeManager()->RemotePlayerFinalised(lPlayerID);

            BrnNetworkModuleIO::NetworkOutPlayerFinalisedEvent lPlayerFinalised;
            lPlayerFinalised.SetNetworkPlayerID(lPlayerID);
            lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(&lPlayerFinalised,
                                                                                   lPlayerFinalised.GetEventType());
            break;
        }

        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_LOST_CONTACT:
        {
            CGS_ASSERT(lpNetworkManager != NULL, "lpNetworkManager");
            CGS_ASSERT(CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID, "CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID");

            CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                lpNetworkManager->GetServerInterface()->GetGameComponent();
            if (lpGamesComponent->IsLocalPlayerInGame() && lpGamesComponent->IsGameStarted())
            {
                const EActiveRaceCarIndex leActiveRaceCarIndex =
                    lpNetworkManager->GetNetworkModule()->GetActiveRaceCarIndex(lPlayerID);
                if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                {
                    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

                    BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent lPlayerDisconnectEvent;
                    lPlayerDisconnectEvent.Construct(
                        lPlayerID, leActiveRaceCarIndex,
                        BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_LOST_CONTACT);
                    lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(
                        &lPlayerDisconnectEvent, lPlayerDisconnectEvent.GetEventType());
                }
            }
            break;
        }

        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_REGAINED_CONTACT:
        {
            CGS_ASSERT(lpNetworkManager != NULL, "lpNetworkManager");
            CGS_ASSERT(CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID, "CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID");

            CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                lpNetworkManager->GetServerInterface()->GetGameComponent();
            if (lpGamesComponent->IsLocalPlayerInGame() && lpGamesComponent->IsGameStarted())
            {
                const EActiveRaceCarIndex leActiveRaceCarIndex =
                    lpNetworkManager->GetNetworkModule()->GetActiveRaceCarIndex(lPlayerID);
                if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
                {
                    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
                    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

                    BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent lPlayerDisconnectEvent;
                    lPlayerDisconnectEvent.Construct(
                        lPlayerID, leActiveRaceCarIndex,
                        BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_CONNECTED);
                    lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(
                        &lPlayerDisconnectEvent, lPlayerDisconnectEvent.GetEventType());
                }
            }
            break;
        }

        case CgsNetwork::PlayerManager::E_EVENT_PLAYER_DISCONNECTED:
        {
            CGS_ASSERT(lpNetworkManager != NULL, "lpNetworkManager");
            CGS_ASSERT(CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID, "CgsNetwork::K_INVALID_PLAYER_ID != lPlayerID");

            CgsNetwork::ServerInterfaceGames* lpGamesComponent =
                lpNetworkManager->GetServerInterface()->GetGameComponent();
            if (lpGamesComponent->IsLocalPlayerInGame() && lpGamesComponent->IsGameStarted())
            {
                BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent lPlayerDisconnectEvent;
                lPlayerDisconnectEvent.Construct(
                    lPlayerID, lpNetworkManager->GetNetworkModule()->GetActiveRaceCarIndex(lPlayerID),
                    BrnNetworkModuleIO::NetworkPlayerDisconnectedEvent::E_DISCONNECT_STATUS_DISCONNECTED);
                lpNetworkManager->GetNetworkModule()->GetNetworkEventQueue()->AddEvent(
                    &lPlayerDisconnectEvent, lPlayerDisconnectEvent.GetEventType());
            }
            break;
        }

        default:
            break;
        }
    }

    // ----------------------------------------------------------------------------------------
    // BrnNetworkManager::OutputPlayerResultsInfo
    //
    // Write the results record of every player in the game whose standings result has
    // arrived, packed from slot 0.
    // ----------------------------------------------------------------------------------------
    void BrnNetworkManager::OutputPlayerResultsInfo(BrnNetworkModuleIO::OutputBuffer* lpOutput)
    {
        CGS_ASSERT(lpOutput != NULL, "lpOutput");

        NetworkPlayerID lPlayerID            = CgsNetwork::K_INVALID_PLAYER_ID;
        s32             liPlayerResultsIndex = 0;

        while (GetPlayerManager()->GetNextPlayerID(&lPlayerID,
                                                   CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            if (GetServerInterface()->GetGameComponent()->IsPlayerInGameByID(lPlayerID) &&
                GetStandingsManager()->ArePlayersResultsValid(lPlayerID))
            {
                BrnNetworkModuleIO::PlayerResultsData* lpPlayerResultsData =
                    lpOutput->GetPlayerResultsInterface()->GetPlayerResultsDataForWriting(liPlayerResultsIndex);
                GetStandingsManager()->FillOutResultsData(lpPlayerResultsData, lPlayerID);
                ++liPlayerResultsIndex;
            }
        }
    }
} // namespace BrnNetwork
