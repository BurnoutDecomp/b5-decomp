#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include <cstring>                                                                         // memset / memcpy / strlen / strncpy
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                                 // the memory checks' log line
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                           // GetFirstEvent / GetNextEvent / AddEvent
#include "GameShared/GameClasses/Gui/CgsGuiEvent.h"                                        // CgsGui::GuiEvent<N>
#include "GameShared/GameClasses/Gui/CgsGuiEventTypeDefs.h"                                // login questions, loading screen, launched
#include "GameShared/GameClasses/Gui/CgsGuiEventLocalisedTextPointerRemoved.h"             // CgsGui::GuiEventLocalisedTextPointerRemoved
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h"                   // CgsGui::GuiEventNetworkSuspension
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"                            // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                       // CgsNetwork::KI_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                       // GetNextPlayerID / GetMenuDataByID
#include "GameShared/GameClasses/Network/Players/CgsPlayersConnectionManager.h"            // AreAllConnectionsSuccessful
#include "GameShared/GameClasses/Network/Players/CgsHostMigrationManager.h"                // HostMigrationManager::Disable
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"             // EComponents / EStatus / MemFree
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // IsLoggedIn
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // GetLocalPlayerInfo
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceUsersets.h"   // ServerInterfaceUsersets
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGamesX360.h"        // SetSessionFlags / GetPlayerXUIDByID
#include "GameSource/GameState/BrnGameEvents.h"                                            // the game-event records
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                     // EGameModeType / EPlayerTeam
#include "GameSource/Gui/BrnGuiDemangledEventTypes.h"                                      // the network GUI payload types
#include "GameSource/Gui/Events/BrnGuiEventNetworkCreateGame.h"                            // BrnGui::GuiEventNetworkCreateGame
#include "GameSource/Gui/Events/BrnGuiEventNetworkGameParams.h"                            // BrnGui::GuiEventNetworkGameParams
#include "GameSource/Gui/Events/BrnGuiEventNetworkPlayerStats.h"                           // BrnGui::GuiEventNetworkPlayerStats
#include "GameSource/Network/BrnNetworkGameParams.h"                                       // BrnNetwork::GameParams
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"
#include "GameSource/Network/BrnNetworkModuleIO.h"                                         // PostSimulationInputBuffer
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                 // NetworkOut* records
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"
#include "GameSource/Network/BrnServerInterface.h"                                         // BrnServerInterface
#include "GameSource/Network/Managers/BrnNetworkImageManager.h"                            // EnableMugshotOutput
#include "GameSource/Network/Managers/BrnNetworkLaunchManager.h"                           // LaunchManager::Start
#include "GameSource/Network/Managers/BrnNetworkMarkedManManager.h"                        // SendMarkedManDataToAll
#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"                      // create / join / search processes
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"                      // GetLocalPlayerStats
#include "GameSource/Network/Managers/BrnNetworkPostRoundManager.h"                        // StartEndOfGame
#include "GameSource/Network/Managers/BrnNetworkSelectedRoutesManager.h"                   // route data
#include "GameSource/Network/Managers/BrnNetworkSuspensionManager.h"                       // Suspend / Resume
#include "GameSource/Network/Parameters/BrnNetworkGameSearchParams.h"                      // BrnNetwork::GameSearchParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerInfoData.h"                        // BrnNetwork::PlayerInfoData
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                     // BrnNetwork::PlayerParams
#include "GameSource/Network/Managers/BrnNetworkPlayerStats.h"                             // BrnNetwork::NetworkPlayerStats
#include "GameSource/Resource/BrnResourceAllocator.h"                                      // GetAvailableMemory, BRN_RESOURCE_MEMORY_CHECK
#include "GameSource/GameState/BrnGameActions.h"

// ===================================================================================
// BrnNetwork::StateManager -- part 4: GUI-driven session changes, game-mode team
// checks and the round / game completion hand-offs.
//
//   PlayerAdded / PlayerRemoved        raise the per-frame freeburn-lobby refresh flags
//   UpdateMarkedMan                    cache the marked man until the game starts
//   GameModeHasTeams                   is the current game mode a team mode?
//   GameModeHasEnoughTeams             do the players in the game cover enough teams?
//   SuspendToLeaveGame                 record why we leave, then suspend the interface
//   PostRoundFinishedCallback /
//   PostGameFinishedCallback           post-round manager completion hand-offs
//   IntroStopped / MarkedManLoaded     start (or, offline, force) the network start time
//   PostRoundProcessingFinished        reset the marked men, stop host migration
//   SendStartNextRoundMessage          hand the next round's route to the game state
//   PostGameProcessingFinished         back to the lobby, or out of the game
//   PrepareForInvite                   settle the session before an invite is taken
//   JoinGameSession / CreateGame /
//   ModifyGame                         join an invited session, create or change a game
//   UpdateEvents                       drain the GUI, game-action and network queues
//   StartGameMode                      build and send the start-game event
//   ProcessGuiEvents                   act on the frame's network GUI events
// ===================================================================================

// Xbox Guide entry points, keyed by the signed-in user index, for a 64-bit XUID (0 == success).
// Declared as extern "C" free functions, as the other network TUs that open the Guide do.
extern "C"
{
    u32 XShowGamerCardUI(u32 luUserIndex, u64 luXuid);
    u32 XShowPlayerReviewUI(u32 luUserIndex, u64 luXuid);
}

namespace BrnNetwork
{
    namespace GsmIO = BrnGameState::GameStateModuleIO;

    namespace
    {
        // PreparedForInviteEvent names the module that is ready for the invite; the network
        // module is 1. FLAG: EModulePreparedForInvite has no home yet.
        const s32 KI_MODULE_PREPARED_FOR_INVITE_NETWORK = 1;

        // CreateGame: an eight-player game that needs two to launch.
        const s32 KI_CREATE_GAME_MAX_PLAYERS = 8;
        const s32 KI_CREATE_GAME_MIN_PLAYERS = 2;

        // ModifyGame: the session flags follow the game's security (EBrnGameSecurity 0 public,
        // 1 private, 2 closed; no home yet). FLAG: flag names not recovered.
        const s32 KI_GAME_SECURITY_CLOSED      = 2;
        const s32 KI_SESSION_FLAGS_OPEN_GAME   = 0x400;
        const s32 KI_SESSION_FLAGS_CLOSED_GAME = 0x600;

        // StartGameMode: the packed time limit counts in steps of ten; the event carries it
        // times sixty. FLAG: the two names are ours.
        const s32 KI_TIME_LIMIT_STEP    = 10;
        const f32 KF_SECONDS_PER_MINUTE = 60.0f;

        // The time limit the start-game event accepts (reference KF_TIME_MIN / KF_TIME_MAX).
        const f32 KF_TIME_MIN = 10.0f;
        const f32 KF_TIME_MAX = 30.0f;

        // ProcessGuiEvents: a launch needs at least two players; the values GuiEventNetworkLaunched
        // carries (CgsGui::GuiEventNetworkLaunched::ELaunchedStatus, reference values; the
        // committed event is a bare 4-byte payload).
        const s32 KI_MIN_PLAYERS_TO_LAUNCH               = 2;
        const s32 KI_LAUNCHED_STATUS_NOT_ENOUGH_PLAYERS  = 1;
        const s32 KI_LAUNCHED_STATUS_STILL_CONNECTING    = 2;
        const s32 KI_LAUNCHED_STATUS_WAITING_FOR_ROUTES  = 3;
        const s32 KI_LAUNCHED_STATUS_NOT_ENOUGH_TEAMS    = 4;
        const s32 KI_LAUNCHED_STATUS_STILL_PLAYING       = 5;

        // ProcessGuiEvents: the custom-match search asks for one free slot; the last Prepare
        // argument is 2. FLAG: that argument's name is not recovered.
        const u32 KU_SEARCH_REQUIRED_SLOTS = 1;
        const u32 KU_SEARCH_FIELD_29C      = 2;

        // The GUI event ids ProcessGuiEvents acts on. Where the payload type has a home its
        // GetEventType() returns the same id. FLAG: the names of 52, 54, 56, 67, 68, 253, 259,
        // 260, 261 and 265 are ours (their GUI types have no home).
        const s32 KI_GUI_EVENT_LOCALISED_TEXT_POINTER_REMOVED  = 13;
        const s32 KI_GUI_EVENT_LOADING_SCREEN_STATE            = 33;
        const s32 KI_GUI_EVENT_NETWORK_SUSPENSION              = 45;
        const s32 KI_GUI_EVENT_ANSWER_LOGIN_QUESTION           = 48;
        const s32 KI_GUI_EVENT_NETWORK_LEAVE_GAME              = 52;
        const s32 KI_GUI_EVENT_NETWORK_LEAVING_GAME            = 53;
        const s32 KI_GUI_EVENT_NETWORK_LAUNCH                  = 54;
        const s32 KI_GUI_EVENT_NETWORK_FORCE_LAUNCH            = 56;
        const s32 KI_GUI_EVENT_AUTOSAVE_STARTED                = 67;
        const s32 KI_GUI_EVENT_AUTOSAVE_FINISHED               = 68;
        const s32 KI_GUI_EVENT_NETWORK_SELECTED_PLAYER_OPTION  = 246;
        const s32 KI_GUI_EVENT_NETWORK_HIGHLIGHTED_PLAYER      = 247;
        const s32 KI_GUI_EVENT_NETWORK_QUIT_PLAYING            = 250;
        const s32 KI_GUI_EVENT_NETWORK_QUICK_MATCH             = 251;
        const s32 KI_GUI_EVENT_NETWORK_CUSTOM_MATCH_SEARCH     = 252;
        const s32 KI_GUI_EVENT_NETWORK_CANCEL_SEARCH           = 253;
        const s32 KI_GUI_EVENT_NETWORK_CUSTOM_MATCH_JOIN       = 255;
        const s32 KI_GUI_EVENT_NETWORK_CREATE_GAME             = 256;
        const s32 KI_GUI_EVENT_NETWORK_GAME_PARAMS             = 257;
        const s32 KI_GUI_EVENT_NETWORK_REQUEST_LOCAL_STATS     = 259;
        const s32 KI_GUI_EVENT_NETWORK_REQUEST_GAME_PARAMS     = 260;
        const s32 KI_GUI_EVENT_NETWORK_READY_TO_JOIN_SESSION   = 261;
        const s32 KI_GUI_EVENT_NETWORK_OUTPUT_PLAYER_TEXTURE   = 264;
        const s32 KI_GUI_EVENT_NETWORK_SWITCH_TEAM             = 265;
        const s32 KI_GUI_EVENT_NETWORK_NEWS_AND_TOS            = 266;
        const s32 KI_GUI_EVENT_NETWORK_SPLASH                  = 269;
        const s32 KI_GUI_EVENT_NETWORK_CONNECT                 = 272;

        // The channel the PC GUI module leaves wrapped records on (see ProcessGuiEvents).
        const s32 KI_GUI_CHANNEL_OUT = 40;

        // The bare 4-byte GUI payloads carry one word.
        template <typename TGuiEvent>
        void SetPayloadWord(TGuiEvent* lpEvent, s32 liValue)
        {
            static_assert(sizeof(lpEvent->maData) == sizeof(s32), "4-byte GUI payload");
            std::memcpy(lpEvent->maData, &liValue, sizeof(s32));
        }

        template <typename TGuiEvent>
        s32 GetPayloadWord(const TGuiEvent* lpEvent)
        {
            static_assert(sizeof(lpEvent->maData) == sizeof(s32), "4-byte GUI payload");
            s32 liValue;
            std::memcpy(&liValue, lpEvent->maData, sizeof(s32));
            return liValue;
        }
    }

    // ------------------------------------------------------------------------------------
    // A player joined the session. The host of a live multi-player game that is not in
    // the middle of an invite restarts the freeburn lobby this frame (as a join); every
    // add refreshes the lobby.
    // ------------------------------------------------------------------------------------
    void StateManager::PlayerAdded()
    {
        if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
            && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost()
            && mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame() > 1
            && !mpNetworkManager->GetNetworkInviteManager()->IsInInvite())
        {
            mbStartFreeburnLobbyThisFrame       = true;
            mbStartingAfterJoinThisFrame        = true;
            mbStartingAfterOnlineEventThisFrame = false;
            mbForceStartFreeburnLobbyThisFrame  = false;
        }

        mbRefreshingFreeburnLobbyThisFrame = true;
    }

    // ------------------------------------------------------------------------------------
    // A player left the session: refresh the freeburn lobby this frame.
    // ------------------------------------------------------------------------------------
    void StateManager::PlayerRemoved()
    {
        mbRefreshingFreeburnLobbyThisFrame = true;
    }

    // ------------------------------------------------------------------------------------
    // Remember the marked man chosen in the lobby. Once the game has started the choice
    // is owned by the marked-man manager, so the cache is left alone.
    // ------------------------------------------------------------------------------------
    void StateManager::UpdateMarkedMan(NetworkPlayerID lMarkedManID)
    {
        if (!mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
        {
            mMarkedManData.mPlayerID = lMarkedManID;
            mMarkedManData.mbValid   = true;
        }
    }

    // ------------------------------------------------------------------------------------
    // True when the game mode of the current game is played in teams.
    // ------------------------------------------------------------------------------------
    bool StateManager::GameModeHasTeams()
    {
        GameParams lGameParams;

        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        return lGameParams.GameMode() == GsmIO::E_MODE_ONLINE_ROAD_RAGE
            || lGameParams.GameMode() == GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN
            || lGameParams.GameMode() == GsmIO::E_MODE_ONLINE_FUGITIVE;
    }

    // ------------------------------------------------------------------------------------
    // Count the distinct teams among the players in the game and test the count against
    // what the team mode needs. Modes without teams always have enough.
    // ------------------------------------------------------------------------------------
    bool StateManager::GameModeHasEnoughTeams()
    {
        s32 liNumTeamsFound = 0;

        if (!GameModeHasTeams())
        {
            return true;
        }

        {
            // One flag per team (the console's GsmIO::E_PLAYER_TEAM_COUNT is 9).
            bool         labFoundTeams[GsmIO::E_PLAYER_TEAM_COUNT];
            PlayerParams lPlayerParams;

            memset(labFoundTeams, 0, sizeof(labFoundTeams));

            for (s32 liPlayerIndex = 0;
                 liPlayerIndex < mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame();
                 ++liPlayerIndex)
            {
                lPlayerParams.Prepare();
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByIndex(liPlayerIndex,
                                                                                                        &lPlayerParams);

                if (!labFoundTeams[lPlayerParams.GetPlayerTeam()])
                {
                    ++liNumTeamsFound;
                }
                labFoundTeams[lPlayerParams.GetPlayerTeam()] = true;
            }
        }

        {
            GameParams lGameParams;

            lGameParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

            switch (lGameParams.GameMode())
            {
            case GsmIO::E_MODE_ONLINE_ROAD_RAGE:
                return liNumTeamsFound == 2;

            case GsmIO::E_MODE_ONLINE_FUGITIVE:
                return liNumTeamsFound >= 2;

            case GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN:
                return true;

            default:
                CGS_ASSERT(false, "Selected game mode shouldn't have teams!\n");
                return false;
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // Leave the game by suspending the server interface. Ignored while already leaving or
    // in post-round processing. A suspension manager that is still busy defers the
    // suspend until it goes idle; an interface that is already suspended needs nothing.
    // ------------------------------------------------------------------------------------
    void StateManager::SuspendToLeaveGame(ELeftGameReason leLeftReason, CgsNetwork::EKickReason leKickReason)
    {
        if (meState == E_STATE_WAIT_SUSPENSION_LEAVING_GAME || meState == E_STATE_WAIT_POST_ROUND)
        {
            return;
        }

        meLeftReason = leLeftReason;
        meKickReason = leKickReason;

        if (mpNetworkManager->GetSuspensionManager()->IsSuspending())
        {
            meState = E_STATE_WAIT_SUSPENSION_IDLE_LEAVING_GAME;
        }
        else if (!mpNetworkManager->GetServerInterface()->IsSuspended())
        {
            meState = E_STATE_WAIT_SUSPENSION_LEAVING_GAME;
            // FLAG: Suspend is expanded inline here and both suspension types yield the same
            // interface flags, so the type argument cannot be read back; OFFLINE is assumed.
            mpNetworkManager->GetSuspensionManager()->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                              SuspensionFinishedCallback, this);
        }
    }

    // ------------------------------------------------------------------------------------
    // Post-round manager completion: the round's results are through.
    // ------------------------------------------------------------------------------------
    void StateManager::PostRoundFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT(lbSuccess, "lbSuccess");

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        lpStateManager->PostRoundProcessingFinished();
    }

    // ------------------------------------------------------------------------------------
    // Post-round manager completion at the end of the game.
    // ------------------------------------------------------------------------------------
    void StateManager::PostGameFinishedCallback(bool lbSuccess, void* lpUserData)
    {
        CGS_ASSERT(lbSuccess, "lbSuccess");

        StateManager* lpStateManager = static_cast<StateManager*>(lpUserData);
        lpStateManager->PostGameProcessingFinished();
    }
    // ------------------------------------------------------------------------------------
    // The intro of a game mode finished. Logged in, a freeburn-lobby (or showtime) intro
    // that is not a move between lobby modes starts the network time sync. Offline, the
    // online modes other than those two force the start time from the game timer.
    // ------------------------------------------------------------------------------------
    void StateManager::IntroStopped(const BrnGameState::GameStateModuleIO::StopModeIntroAction* lpStopIntroAction)
    {
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetServerInterface(), "mpNetworkManager->GetServerInterface()");
        CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetConnectionComponent(),
                   "mpNetworkManager->GetServerInterface()->GetConnectionComponent()");

        if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
        {
            if (!lpStopIntroAction->mbMovingBetweenLobbyModes
                && (lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
                    || lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME)
                && mpNetworkManager->IsDoingFreeBurnLobby())
            {
                CGS_ASSERT(!GsmIO::IsShowtimeGameMode(lpStopIntroAction->meGameMode),
                           "!GsmIO::IsShowtimeGameMode(lpStopIntroAction->meGameMode)");

                mpNetworkManager->GetStartTimeManager()->StartSyncingTime(
                    lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY);
                meState = E_STATE_SYNC_TIME;
            }
        }
        else if (!lpStopIntroAction->mbMovingBetweenLobbyModes
                 && !(lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY
                      || lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME)
                 && lpStopIntroAction->meGameMode >= GsmIO::E_MODE_ONLINE_MODE_START
                 && lpStopIntroAction->meGameMode <= GsmIO::E_MODE_ONLINE_MODE_END)
        {
            mpNetworkManager->GetStartTimeManager()->ForceStartTime(mpNetworkManager->GetTimerStatus());
            meState = E_STATE_SYNC_TIME;
        }
    }

    // ------------------------------------------------------------------------------------
    // The marked man's assets are loaded. Logged in, a load that is not a move between
    // lobby modes starts the network time sync; offline, the start time is forced from
    // the game timer.
    // ------------------------------------------------------------------------------------
    void StateManager::MarkedManLoaded(const BrnGameState::GameStateModuleIO::MarkedManLoadedAction* lpMarkedManLoadedAction)
    {
        CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
        CGS_ASSERT(mpNetworkManager->GetServerInterface(), "mpNetworkManager->GetServerInterface()");
        CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetConnectionComponent(),
                   "mpNetworkManager->GetServerInterface()->GetConnectionComponent()");

        if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
        {
            if (!lpMarkedManLoadedAction->mbMovingBetweenLobbyModes)
            {
                CGS_ASSERT(!GsmIO::IsShowtimeGameMode(lpMarkedManLoadedAction->meGameMode),
                           "!GsmIO::IsShowtimeGameMode(lpMarkedManLoadedAction->meGameMode)");

                mpNetworkManager->GetStartTimeManager()->StartSyncingTime(
                    lpMarkedManLoadedAction->meGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY);
                meState = E_STATE_SYNC_TIME;
            }
        }
        else
        {
            mpNetworkManager->GetStartTimeManager()->ForceStartTime(mpNetworkManager->GetTimerStatus());
            meState = E_STATE_SYNC_TIME;
        }
    }

    // ------------------------------------------------------------------------------------
    // The round's post-processing is done: tell the manager the round is over, wait for
    // the next game-mode start, forget every player's marked man and stop host migration
    // for the rest of the game.
    // ------------------------------------------------------------------------------------
    void StateManager::PostRoundProcessingFinished()
    {
        mpNetworkManager->OnRoundFinish();
        meState = E_STATE_WAIT_GAME_MODE_START;

        NetworkPlayerID lPlayerID = CgsNetwork::KI_INVALID_PLAYER_ID;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            PlayerMenuData* lpMenuData =
                static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
            CGS_ASSERT(lpMenuData, "lpMenuData");

            lpMenuData->mMarkedManID = CgsNetwork::KI_INVALID_PLAYER_ID;
        }

        if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
        {
            mpNetworkManager->GetHostMigrationManager()->Disable(
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetHostPlayerID(),
                mpNetworkManager->GetTimerStatus(),
                static_cast<u16>(mpNetworkManager->GetCurrentFrame() % 0xFFFFu));
        }
    }

    // ------------------------------------------------------------------------------------
    // Tell the game state to start round liRoundIndex: copy that round's route (landmarks
    // and traffic-light trigger) into a start-round event. The host also records the
    // round's event id in telemetry.
    // ------------------------------------------------------------------------------------
    void StateManager::SendStartNextRoundMessage(s32 liRoundIndex)
    {
        GsmIO::StartNetworkRoundEvent lEvent;
        GsmIO::SpecificGameModeEventInterface::Event laEvents[GsmIO::KU_MAX_ONLINE_ROUNDS_IN_MODE];
        NetworkPlayerID lPlayerID;

        mpNetworkManager->GetSelectedRoutesManager()->GetRouteData(laEvents);

        lEvent.miNumLandmarksInRound = laEvents[liRoundIndex].GetNumLandmarks();
        lEvent.mLightTriggerID       = laEvents[liRoundIndex].GetTrafficLightTriggerId();
        for (s32 liLandmarkIndex = 0; liLandmarkIndex < lEvent.miNumLandmarksInRound; ++liLandmarkIndex)
        {
            lEvent.maLandmarks[liLandmarkIndex] = laEvents[liRoundIndex].GetLandmark(liLandmarkIndex);
        }

        mpNetworkModule->GetGameEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                                                       GsmIO::E_EVENT_START_NETWORK_ROUND,
                                                       static_cast<s32>(sizeof(lEvent)));

        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lPlayerID);
        CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        if (mpNetworkManager->GetPlayerManager()->GetHostPlayerID() == lPlayerID)
        {
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_HOST_TRACKID,
                                                    laEvents[liRoundIndex].GetEventID());
        }
    }

    // ------------------------------------------------------------------------------------
    // The game's post-processing is done. Still in the game: stop host migration, clear
    // every player's car choice and camera state and fall back into the freeburn lobby.
    // Out of the game: leave (suspending if logged in) and, outside the lobby modes, put
    // the local player back in the freeburn car. Either way tell the game side and drop
    // the cached network player ids.
    // ------------------------------------------------------------------------------------
    void StateManager::PostGameProcessingFinished()
    {
        BrnNetworkModuleIO::NetworkOutPostGameProcessingFinished lPostGameProcessingFinished;

        mpNetworkManager->OnGameFinish();
        meState = E_STATE_COUNT;

        if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
        {
            mpNetworkManager->GetHostMigrationManager()->Disable(
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetHostPlayerID(),
                mpNetworkManager->GetTimerStatus(),
                static_cast<u16>(mpNetworkManager->GetCurrentFrame() % 0xFFFFu));

            NetworkPlayerID lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
            while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                       &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
            {
                PlayerMenuData* lpMenuData =
                    static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
                CGS_ASSERT(lpMenuData, "lpMenuData");

                lpMenuData->mCarId               = 0;
                lpMenuData->mWheelId             = 0;
                lpMenuData->mu16CarColourIndex   = PlayerMenuData::KU_INVALID_COLOUR_INDEX;
                lpMenuData->mu16PaintFinishIndex = PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX;
                lpMenuData->meCameraStatus       = E_CAMERA_STATUS_NONE;
            }

            StartFreeBurnLobbyGameMode(false, true, false, false);
            lPostGameProcessingFinished.mbIsStillInGame = true;
        }
        else
        {
            if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
            {
                SuspendToLeaveGame(E_LEFT_GAME_REASON_LEFT, CgsNetwork::E_KICKREASON_COUNT);
            }

            CGS_ASSERT(mpNetworkModule, "mpNetworkModule");
            CGS_ASSERT(mpNetworkModule->GetGameStateToNetworkInterface(),
                       "mpNetworkModule->GetGameStateToNetworkInterface()");

            if (!GsmIO::IsOnlineFreeBurnLobby(mpNetworkModule->GetGameStateToNetworkInterface()->GetCurrentGameMode()))
            {
                GsmIO::ChangePlayerCarEvent lChangeCarEvent;
                lChangeCarEvent.mCarModelId         = mpNetworkManager->GetFreeBurnCarID();
                lChangeCarEvent.mWheelModelId       = mpNetworkManager->GetFreeBurnWheelID();
                lChangeCarEvent.mbResetPlayerCamera = false;
                lChangeCarEvent.mbKeepResetSection  = true;
                mpNetworkModule->GetGameEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChangeCarEvent),
                                                               GsmIO::E_EVENT_CHANGE_PLAYER_CAR,
                                                               static_cast<s32>(sizeof(lChangeCarEvent)));
            }

            lPostGameProcessingFinished.mbIsStillInGame = false;
        }

        mpNetworkModule->GetNetworkEventQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lPostGameProcessingFinished),
            lPostGameProcessingFinished.GetEventType(), static_cast<s32>(sizeof(lPostGameProcessingFinished)));
        mpNetworkModule->ClearCachedNetworkPlayerIDsAtGameEnd();
    }

    // ------------------------------------------------------------------------------------
    // An invite arrived. In car or team selection the game is ended (and left) first. Idle,
    // any running game search is cancelled once the games component is free, then the
    // manager waits for the component to settle; once it has, the game state is told the
    // network side is ready for the invite.
    // ------------------------------------------------------------------------------------
    void StateManager::PrepareForInvite()
    {
        if (meState == E_STATE_WAIT_CAR_SELECT || meState == E_STATE_WAIT_TEAM_SELECTION)
        {
            meState = E_STATE_WAIT_POST_ROUND;
            mpNetworkManager->GetPostRoundManager()->StartEndOfGame(true, true, PostGameFinishedCallback, this);
            mpNetworkModule->GetNetworkManager()->SetRound(-1);
        }
        else if (meState == E_STATE_COUNT)
        {
            if (!mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::IsSuspended()
                && mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
            {
                if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(
                        CgsNetwork::E_COMPONENTS_GAMES) != CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                {
                    return;
                }
                mpNetworkManager->GetServerInterface()->GetGameComponent()->CancelSearchForGames();
            }
            meState = E_STATE_PREPARING_FOR_INVITE;
        }
        else if (meState == E_STATE_PREPARING_FOR_INVITE)
        {
            if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(
                    CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
            {
                GsmIO::PreparedForInviteEvent lPreparedForInviteEvent;
                lPreparedForInviteEvent.meModulePreparedForInvite = KI_MODULE_PREPARED_FOR_INVITE_NETWORK;
                mpNetworkModule->GetGameEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lPreparedForInviteEvent),
                    GsmIO::E_EVENT_PREPARED_FOR_INVITE, static_cast<s32>(sizeof(lPreparedForInviteEvent)));
                meState = E_STATE_PREPARED_FOR_INVITE;
            }
        }
    }

    // ------------------------------------------------------------------------------------
    // Join the game session lpcSessionID (an accepted invite): start the join through the
    // matchmaking manager and tell the GUI a join is under way.
    // ------------------------------------------------------------------------------------
    void StateManager::JoinGameSession(char* lpcSessionID)
    {
        BrnNetworkModuleIO::NetworkOutJoiningGameEvent lJoiningGameEvent;
        GameParams                                     lGameParams;

        mbForceStartFreeburnLobby = false;

        CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn(),
                   "mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()");

        lGameParams.Prepare();
        lGameParams.SetSession(lpcSessionID);
        mpNetworkManager->GetMatchMakingManager()->JoinGame(&lGameParams, true, JoinGameFinishedCallback, this);

        mpNetworkModule->GetNetworkEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lJoiningGameEvent),
                                                          lJoiningGameEvent.GetEventType(),
                                                          static_cast<s32>(sizeof(lJoiningGameEvent)));
        meState = E_STATE_WAIT_MATCHMAKING;
    }

    // ------------------------------------------------------------------------------------
    // Create a game from the GUI's options: an 8-player fixed game named after the local
    // player, at the local player's rank and locality, then hand it to matchmaking (which
    // leaves the current game first when there is one) and publish the chosen routes.
    // ------------------------------------------------------------------------------------
    void StateManager::CreateGame(const BrnGui::GuiEventNetworkCreateGame* lpGameParamsEvent, bool lbCreatedFromMenus)
    {
        GameParams     lGameParams;
        PlayerInfoData lPlayerInfo;

        mbForceStartFreeburnLobby = false;

        lGameParams.Prepare();
        lPlayerInfo.Prepare();
        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);

        lGameParams.SetName(lPlayerInfo.GetName());
        lGameParams.SetMaxPlayers(KI_CREATE_GAME_MAX_PLAYERS);
        lGameParams.SetMinPlayers(KI_CREATE_GAME_MIN_PLAYERS);
        lGameParams.SetTotalSlots(KI_CREATE_GAME_MAX_PLAYERS, 0);
        lGameParams.SetTimeLimit(lpGameParamsEvent->miTimeLimit);
        lGameParams.SetRagerVehicleLevelLimit(0);
        lGameParams.SetFixedGame(true);
        lGameParams.SetSkillLevel(static_cast<u32>(lPlayerInfo.GetRank()));
        lGameParams.SetInfiniteBoost(lpGameParamsEvent->mbInfiniteBoost);
        lGameParams.SetNumberRounds(lpGameParamsEvent->miNumRounds);
        lGameParams.SetVehicleLevelLimit(lpGameParamsEvent->miVehicleClass);
        lGameParams.SetRankedGame(lpGameParamsEvent->mbRanked);
        lGameParams.SetSecurity(lpGameParamsEvent->meSecurity);
        lGameParams.SetGameMode(lpGameParamsEvent->meGameMode);
        lGameParams.SetTrafficOn(lpGameParamsEvent->mbTrafficOn);
        lGameParams.SetTrafficCheckingOn(lpGameParamsEvent->mbTrafficCheckingOn);
        lGameParams.SetBoostType(lpGameParamsEvent->meBoostType);
        lGameParams.SetNumRunnerCrashes(lpGameParamsEvent->miNumRunnerCrashes);
        lGameParams.SetVehicleChoice(lpGameParamsEvent->meVehicleChoice);
        lGameParams.SetLocality(lPlayerInfo.GetLocality());

        mpNetworkManager->GetMatchMakingManager()->CreateGame(&lGameParams, CreateGameFinishedCallback, this);

        mpNetworkManager->GetSelectedRoutesManager()->SetRouteData(
            lpGameParamsEvent->miNumRounds, lpGameParamsEvent->maEvents,
            static_cast<GsmIO::EGameModeType>(lpGameParamsEvent->meGameMode));

        mbCreatedFromMenus = lbCreatedFromMenus;
        meState            = E_STATE_WAIT_MATCHMAKING;
    }

    // ------------------------------------------------------------------------------------
    // The host changed the game's options: re-apply them over the current game parameters
    // (with the local player's rank and locality), set the session flags a closed game
    // needs, push the parameters to the server and publish the chosen routes.
    // ------------------------------------------------------------------------------------
    void StateManager::ModifyGame(const BrnGui::GuiEventNetworkGameParams* lpGameParamsEvent)
    {
        GameParams     lGameParams;
        PlayerInfoData lPlayerInfo;

        lGameParams.Prepare();
        lPlayerInfo.Prepare();
        mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        lGameParams.SetSkillLevel(static_cast<u32>(lPlayerInfo.GetRank()));
        lGameParams.SetInfiniteBoost(lpGameParamsEvent->mbInfiniteBoost);
        lGameParams.SetNumberRounds(lpGameParamsEvent->miNumRounds);
        lGameParams.SetVehicleLevelLimit(lpGameParamsEvent->miVehicleClass);
        lGameParams.SetSecurity(lpGameParamsEvent->meSecurity);
        lGameParams.SetGameMode(lpGameParamsEvent->meGameMode);
        lGameParams.SetTrafficOn(lpGameParamsEvent->mbTrafficOn);
        lGameParams.SetTrafficCheckingOn(lpGameParamsEvent->mbTrafficCheckingOn);
        lGameParams.SetBoostType(lpGameParamsEvent->meBoostType);
        lGameParams.SetNumRunnerCrashes(lpGameParamsEvent->miNumRunnerCrashes);
        lGameParams.SetVehicleChoice(lpGameParamsEvent->meVehicleChoice);
        lGameParams.SetTimeLimit(lpGameParamsEvent->miTimeLimit);
        lGameParams.SetLocality(lPlayerInfo.GetLocality());

        static_cast<CgsNetwork::ServerInterfaceGamesX360*>(mpNetworkManager->GetServerInterface()->GetGameComponent())
            ->SetSessionFlags(lGameParams.Security() == KI_GAME_SECURITY_CLOSED ? KI_SESSION_FLAGS_CLOSED_GAME
                                                                                 : KI_SESSION_FLAGS_OPEN_GAME);
        mpNetworkManager->GetServerInterface()->GetGameComponent()->UpdateGameParameters(&lGameParams);

        mpNetworkManager->GetSelectedRoutesManager()->SetRouteData(
            lpGameParamsEvent->miNumRounds, lpGameParamsEvent->maEvents,
            static_cast<GsmIO::EGameModeType>(lpGameParamsEvent->meGameMode));

        meState = E_STATE_WAIT_MODIFY_GAME;
    }

    // ------------------------------------------------------------------------------------
    // The launch went through: build the start-game event from the game parameters and
    // every finalised player's menu choices (a player with no car, wheel, colour or paint
    // takes the first player's), remember each player's XUID, send it with the first
    // round's route, set the round counter and let the host record the game in telemetry.
    // ------------------------------------------------------------------------------------
    void StateManager::StartGameMode()
    {
        PlayerParams                 lPlayerParams;
        GameParams                   lGameParams;
        PlayerMenuData*              lpMenuData;
        PlayerMenuData*              lpHostMenuData;
        CgsID                        lFirstCarID                  = 0;
        CgsID                        lFirstWheelID                = 0;
        GsmIO::StartNetworkGameEvent lEvent;
        s32                          liGridPosition;
        f32                          lfTimeLimit;
        u16                          lu16FirstCarColourIndex      = PlayerMenuData::KU_INVALID_COLOUR_INDEX;
        u16                          lu16FirstCarPaintFinishIndex = PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX;
        NetworkPlayerID              lLocalPlayerID;
        NetworkPlayerID              lPlayerID                    = CgsNetwork::K_INVALID_PLAYER_ID;

        for (s32 liSlot = 0; liSlot < CurrentPlayerXUIDs::KI_NUM_SLOTS; ++liSlot)
        {
            mCurrentPlayerXUIDs.maSlots[liSlot].miPlayerId = CurrentPlayerXUIDs::KI_FREE_SLOT;
            mCurrentPlayerXUIDs.maSlots[liSlot].mu64XUID   = 0;
        }

        lGameParams.Prepare();
        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);

        lEvent.Clear();
        lEvent.meGameMode          = static_cast<GsmIO::EGameModeType>(lGameParams.GameMode());
        lEvent.muRandomSeedForGame = lGameParams.GetRandomSeed();
        lEvent.miNumRaceCars       = mpNetworkManager->GetPlayerManager()->GetTotalNumberPlayers(
            CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED);
        lEvent.miNumRounds         = lGameParams.NumberRounds();

        lfTimeLimit = static_cast<f32>((lGameParams.TimeLimit() + 1) * KI_TIME_LIMIT_STEP);
        CGS_ASSERT(lfTimeLimit >= KF_TIME_MIN, "lfTimeLimit >= KF_TIME_MIN");
        CGS_ASSERT(lfTimeLimit <= KF_TIME_MAX, "lfTimeLimit <= KF_TIME_MAX");

        // FLAG: the reference fills these through the event's inline SetGameData / SetTimeLimit /
        // SetLocalNetworkPlayerID, which have no home yet; their stores are written by name.
        lEvent.miNumRunnerCrashes         = lGameParams.NumRunnerCrashes();
        lEvent.meBoostType                = lGameParams.BoostType();
        lEvent.mbIsTrafficCheckingOn      = lGameParams.IsTrafficCheckingOn();
        lEvent.mbRedTeamHaveInfiniteBoost = lGameParams.InfiniteBoost();
        lEvent.mbIsTrafficOn              = lGameParams.IsTrafficOn();
        lEvent.mbIsRanked                 = lGameParams.IsRankedGame();
        lEvent.mfTimeLimit                = lfTimeLimit * KF_SECONDS_PER_MINUTE;

        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
        CGS_ASSERT(lLocalPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lLocalNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        lEvent.mLocalNetworkPlayerID = lLocalPlayerID;

        lPlayerID      = mpNetworkManager->GetPlayerManager()->GetHostPlayerID();
        lpHostMenuData = static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));
        CGS_ASSERT(lpHostMenuData, "lpHostMenuData");

        // Pick the first choice made for each of car, wheel, colour and paint, and note every
        // player's XUID on the way.
        lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            u64 lu64XUID;

            lPlayerParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID,
                                                                                                    &lPlayerParams);
            lpMenuData = static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));

            static_cast<CgsNetwork::ServerInterfaceGamesX360*>(mpNetworkManager->GetServerInterface()->GetGameComponent())
                ->GetPlayerXUIDByID(lPlayerID, &lu64XUID);
            mCurrentPlayerXUIDs.SetXUID(lPlayerID, lu64XUID);

            if (lFirstCarID == 0 && lpMenuData->mCarId != 0)
            {
                lFirstCarID = lpMenuData->mCarId;
            }
            if (lFirstWheelID == 0 && lpMenuData->mWheelId != 0)
            {
                lFirstWheelID = lpMenuData->mWheelId;
            }
            if (lu16FirstCarColourIndex == PlayerMenuData::KU_INVALID_COLOUR_INDEX
                && lpMenuData->mu16CarColourIndex != PlayerMenuData::KU_INVALID_COLOUR_INDEX)
            {
                lu16FirstCarColourIndex = lpMenuData->mu16CarColourIndex;
            }
            if (lu16FirstCarPaintFinishIndex == PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX
                && lpMenuData->mu16PaintFinishIndex != PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX)
            {
                lu16FirstCarPaintFinishIndex = lpMenuData->mu16PaintFinishIndex;
            }

            // The console stops once car, wheel and paint are known (the colour is not tested).
            if (lFirstCarID != 0 && lFirstWheelID != 0
                && lu16FirstCarPaintFinishIndex != PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX)
            {
                break;
            }
        }

        CGS_ASSERT((lFirstCarID != 0), "(lFirstCarID != 0)");

        // Fill the gaps from the first choices and put every player on the grid in turn.
        lPlayerID      = CgsNetwork::K_INVALID_PLAYER_ID;
        liGridPosition = 0;
        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
        {
            lPlayerParams.Prepare();
            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(lPlayerID,
                                                                                                    &lPlayerParams);
            lpMenuData = static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lPlayerID));

            if (lpMenuData->mCarId == 0)
            {
                lpMenuData->mCarId = lFirstCarID;
            }
            if (lpMenuData->mWheelId == 0)
            {
                lpMenuData->mWheelId = lFirstWheelID;
            }
            if (lpMenuData->mu16CarColourIndex == PlayerMenuData::KU_INVALID_COLOUR_INDEX)
            {
                lpMenuData->mu16CarColourIndex = lu16FirstCarColourIndex;
            }
            if (lpMenuData->mu16PaintFinishIndex == PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX)
            {
                lpMenuData->mu16PaintFinishIndex = lu16FirstCarPaintFinishIndex;
            }

            CGS_ASSERT(lpMenuData->mu16CarColourIndex != PlayerMenuData::KU_INVALID_COLOUR_INDEX,
                       "Invalid car colour index\n");
            CGS_ASSERT(lpMenuData->mu16PaintFinishIndex != PlayerMenuData::KU_INVALID_PAINT_FINISH_INDEX,
                       "Invalid paint palette index\n");

            lEvent.SetPlayerData(liGridPosition, lPlayerID, lpMenuData->mCarId, lpMenuData->mWheelId,
                                 lpMenuData->mu16CarColourIndex, lpMenuData->mu16PaintFinishIndex,
                                 lpMenuData->mfUnknown48, lPlayerParams.GetPlayerTeam(),
                                 mpNetworkManager->GetPlayerManager()->GetHostPlayerID() == lPlayerID,
                                 lPlayerParams.HasFever());
            ++liGridPosition;
        }

        mpNetworkModule->GetGameEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lEvent),
                                                       GsmIO::E_EVENT_START_NETWORK_GAME,
                                                       static_cast<s32>(sizeof(lEvent)));

        SendStartNextRoundMessage(0);

        if (GsmIO::IsOnlineFreeBurnLobby(lEvent.meGameMode))
        {
            mpNetworkModule->GetNetworkManager()->SetRound(-1);
        }
        else
        {
            mpNetworkModule->GetNetworkManager()->SetRound(0);
        }

        lPlayerID = CgsNetwork::K_INVALID_PLAYER_ID;
        mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lPlayerID);
        CGS_ASSERT(lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID, "lPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

        if (mpNetworkManager->GetPlayerManager()->GetHostPlayerID() == lPlayerID)
        {
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_HOST_PLAYERS_IN_GAME, lEvent.miNumRaceCars);
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_HOST_NUMBER_OF_ROUNDS, lEvent.miNumRounds);
            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_HOST_IS_RANKED,
                                                    lEvent.mbIsRanked ? "true" : "false");
        }
    }

    // ------------------------------------------------------------------------------------
    // Drain the frame's event queues: GUI events, then game-state actions, then network
    // events, checking between each that no memory was allocated.
    // ------------------------------------------------------------------------------------
    void StateManager::UpdateEvents(const BrnNetworkModuleIO::PostSimulationInputBuffer* lpInput)
    {
        u32 luMemory = BrnResource::GetAvailableMemory();
        BRN_RESOURCE_MEMORY_CHECK(luMemory);

        ProcessGuiEvents(lpInput->GetGuiEventQueue());
        BRN_RESOURCE_MEMORY_CHECK(luMemory);

        ProcessGameStateActions(lpInput->GetGameActionQueue());
        BRN_RESOURCE_MEMORY_CHECK(luMemory);

        ProcessNetworkEvents(lpInput->GetNetworkEventQueue());
        BRN_RESOURCE_MEMORY_CHECK(luMemory);
    }

    // ------------------------------------------------------------------------------------
    // ProcessGuiEvents
    //
    // Walk the frame's GUI out-events and act on the network ones: suspension, the login
    // questions, leaving / launching / joining / creating / modifying games, the game-room
    // player options, stats and picture requests, news and terms-of-service, the splash
    // screen and the connect request.
    //
    // FLAG PC-ABI adapter (the GameBridgeGUIToX one): the PC GUI module leaves some records
    // on channel 40 with the event id in the record's second word and the payload offset in
    // its third; a record posted directly carries its own id. Types that carry the GUI event
    // header (GuiEvent<N>-derived) are read from the record, the bare payload types from the
    // payload.
    // ------------------------------------------------------------------------------------
    void StateManager::ProcessGuiEvents(const CgsModule::VariableEventQueue<18432, 16>* lpGuiEventQueue)
    {
        const CgsModule::Event* lpEvent     = nullptr;
        s32                     liEventSize = 0;
        s32                     liEventId   = lpGuiEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent != nullptr)
        {
            const CgsModule::Event* lpRecord  = lpEvent;
            const void*             lpPayload = lpEvent;
            s32                     liCommand = liEventId;
            const s32               liHeaderSize = static_cast<s32>(sizeof(CgsGui::GuiEvent<0>));
            if (liEventId == KI_GUI_CHANNEL_OUT && liEventSize >= liHeaderSize)
            {
                const u32* lpuRecord = reinterpret_cast<const u32*>(lpEvent);
                liCommand = static_cast<s32>(lpuRecord[1]);
                const u32 luOffset = lpuRecord[2];
                if (luOffset >= static_cast<u32>(liHeaderSize) && static_cast<s32>(luOffset) < liEventSize)
                {
                    lpPayload = reinterpret_cast<const u8*>(lpEvent) + luOffset;
                }
            }

            switch (liCommand)
            {
            case KI_GUI_EVENT_LOCALISED_TEXT_POINTER_REMOVED:
            {
                // The GUI dropped a downloaded text: free our copy of it.
                const CgsGui::GuiEventLocalisedTextPointerRemoved* lpTextRemovedEvent =
                    static_cast<const CgsGui::GuiEventLocalisedTextPointerRemoved*>(lpPayload);
                CGS_ASSERT(lpTextRemovedEvent, "lpTextRemovedEvent");

                if (lpTextRemovedEvent->IsThisTheRemovedString("TOS_TEXT"))
                {
                    CgsNetwork::ServerInterfaceDirtySock::MemFree(mpTOS, 0, 0);
                    mpTOS = nullptr;
                }
                else if (lpTextRemovedEvent->IsThisTheRemovedString("NEWS_TEXT"))
                {
                    CgsNetwork::ServerInterfaceDirtySock::MemFree(mpNews, 0, 0);
                    mpNews = nullptr;
                }
                break;
            }

            case KI_GUI_EVENT_LOADING_SCREEN_STATE:
            {
                const CgsGui::GuiEventLoadingScreenState* lpLoadingScreenStateEvent =
                    static_cast<const CgsGui::GuiEventLoadingScreenState*>(lpPayload);
                CGS_ASSERT(lpLoadingScreenStateEvent, "lpLoadingScreenStateEvent");

                mbLoadingScreenVisible = lpLoadingScreenStateEvent->mbIsVisible;
                break;
            }

            case KI_GUI_EVENT_NETWORK_SUSPENSION:
            {
                const CgsGui::GuiEventNetworkSuspension* lpSuspensionEvent =
                    static_cast<const CgsGui::GuiEventNetworkSuspension*>(lpRecord);

                switch (lpSuspensionEvent->meSuspensionType)
                {
                case CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_SUSPEND:
                    if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()
                        && !mpNetworkManager->GetLoginManager()->IsSigningIn()
                        && !mpNetworkManager->GetSuspensionManager()->IsSuspending()
                        && !mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::IsSuspended()
                        && !mpNetworkManager->IsDoingFreeBurnLobby())
                    {
                        meState = E_STATE_WAIT_SUSPENSION;
                        // FLAG: Suspend is expanded inline here and both suspension types yield the
                        // same interface flags, so the type argument cannot be read back; OFFLINE is
                        // assumed (as in SuspendToLeaveGame).
                        mpNetworkManager->GetSuspensionManager()->Suspend(SuspensionManager::E_SUSPENSION_TYPE_OFFLINE,
                                                                          SuspensionFinishedCallback, this);
                    }
                    else if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()
                             && !mpNetworkManager->GetLoginManager()->IsSigningIn()
                             && !mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::IsSuspended()
                             && !mpNetworkManager->IsDoingFreeBurnLobby())
                    {
                        // The suspension manager is busy: suspend once it goes idle.
                        meState = E_STATE_WAIT_SUSPENSION_IDLE_TO_SUSPEND;
                    }
                    // FLAG: otherwise the console logs "SUSPENDING: faied as either logging in,
                    // signing in or already suspended\n" to a network debug stream that has no
                    // home in this tree; the log line is not reproduced.
                    break;

                case CgsGui::GuiEventNetworkSuspension::E_SUSPENSION_TYPE_RESUME:
                    if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn()
                        && !mpNetworkManager->GetSuspensionManager()->IsSuspending()
                        && mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::IsSuspended())
                    {
                        meState = E_STATE_WAIT_SUSPENSION;
                        mpNetworkManager->GetSuspensionManager()->Resume(ResumeFinishedCallback, this);
                    }
                    break;

                default:
                    CGS_ASSERT(false, "Invalid suspension type");
                    break;
                }
                break;
            }

            case KI_GUI_EVENT_ANSWER_LOGIN_QUESTION:
            {
                const CgsGui::GuiEventAnswerLoginQuestion* lpAnswerQuestionEvent =
                    static_cast<const CgsGui::GuiEventAnswerLoginQuestion*>(lpPayload);
                LoginManagerBase* lpLoginManager = mpNetworkManager->GetLoginManager();

                switch (lpAnswerQuestionEvent->meLoginQuestion)
                {
                case CgsGui::E_LOGIN_QUESTION_TOS:
                    lpLoginManager->AnswerAgreeTOS(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_CREATE_ACCOUNT:
                    lpLoginManager->AnswerCreateAccount(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_SHARE:
                    lpLoginManager->AnswerShareInfo(lpAnswerQuestionEvent->mbAccept, lpAnswerQuestionEvent->mbAccept2);
                    break;
                case CgsGui::E_LOGIN_QUESTION_OPEN_US_ACCOUNT:
                    lpLoginManager->AnswerOpenUsAccount(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_NO_AGREEMENT:
                    lpLoginManager->AnswerNoAgreement(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_SHOW_SIGN_IN:
                    lpLoginManager->AnswerSignIn(lpAnswerQuestionEvent->mbAccept);
                    break;
                case CgsGui::E_LOGIN_QUESTION_CHAT_RESTRICTION:
                    lpLoginManager->AnswerChatRestricted(lpAnswerQuestionEvent->mbAccept);
                    break;
                default:
                    CGS_ASSERT(false, "Invalid login question");
                    break;
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_LEAVE_GAME:
            {
                // Leave the online game for an offline one, unless the game is locked or the
                // games component is busy.
                if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(
                        CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE
                    && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                    && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameLocked())
                {
                    CgsGui::GuiEvent<KI_GUI_EVENT_NETWORK_LEAVING_GAME> lLeavingGameEvent;

                    mpNetworkManager->GetMatchMakingManager()->StartProcess(
                        MatchMakingManager::E_PROCESS_LEAVE_GAME, LeaveGameForOfflineGameFinishedCallback, this);
                    meState = E_STATE_WAIT_MATCHMAKING;
                    mpNetworkModule->AddOutputGuiEvent(lLeavingGameEvent);
                }
                else
                {
                    BrnGui::GuiEventNetworkLeavingGameFailed lLeavingGameFailedEvent;
                    mpNetworkModule->AddOutputGuiEvent(lLeavingGameFailedEvent);
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_LAUNCH:
            {
                if (meState != E_STATE_COUNT)
                {
                    break;
                }

                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerHost())
                {
                    // The host launches once there are enough players, all connected, all with
                    // the routes, in enough teams and none still playing.
                    GameParams                               lGameParams;
                    BrnNetworkModuleIO::NetworkOutLaunchEvent lNetworkLaunchEvent;
                    CgsGui::GuiEventNetworkLaunched          lLaunchedEvent;
                    bool                                     lbPlayerStillPlaying = false;
                    PlayerParams                             lPlayerParams;

                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame()
                        < KI_MIN_PLAYERS_TO_LAUNCH)
                    {
                        SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_NOT_ENOUGH_PLAYERS);
                        mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                    }
                    else if (!mpNetworkManager->GetPlayersConnectionManager()->AreAllConnectionsSuccessful())
                    {
                        SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_STILL_CONNECTING);
                        mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                    }
                    else if (!mpNetworkManager->GetSelectedRoutesManager()->HaveAllPlayersReceivedRoutes())
                    {
                        SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_WAITING_FOR_ROUTES);
                        mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                    }
                    else if (!GameModeHasEnoughTeams())
                    {
                        SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_NOT_ENOUGH_TEAMS);
                        mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                    }
                    else
                    {
                        for (s32 liPlayerIndex = 0;
                             liPlayerIndex < mpNetworkManager->GetServerInterface()->GetGameComponent()->GetNumberPlayersInGame();
                             ++liPlayerIndex)
                        {
                            lPlayerParams.Prepare();
                            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByIndex(
                                liPlayerIndex, &lPlayerParams);
                            if (lPlayerParams.IsPlaying())
                            {
                                lbPlayerStillPlaying = true;
                                break;
                            }
                        }

                        if (lbPlayerStillPlaying)
                        {
                            SetPayloadWord(&lLaunchedEvent, KI_LAUNCHED_STATUS_STILL_PLAYING);
                            mpNetworkModule->AddOutputGuiEvent(lLaunchedEvent);
                        }
                        else
                        {
                            lGameParams.Prepare();
                            mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);
                            lNetworkLaunchEvent.meGameModeType = static_cast<GsmIO::EGameModeType>(lGameParams.GameMode());
                            lNetworkLaunchEvent.mHostPlayerID  = mpNetworkManager->GetPlayerManager()->GetHostPlayerID();
                            mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                                reinterpret_cast<const CgsModule::Event*>(&lNetworkLaunchEvent),
                                lNetworkLaunchEvent.GetEventType(), static_cast<s32>(sizeof(lNetworkLaunchEvent)));

                            mpNetworkManager->GetLaunchManager()->Start();
                            mpNetworkManager->OnGameLaunching();
                            meState = E_STATE_LAUNCHING;
                            mpNetworkManager->CaptureTelemetryEvent(E_TELEMETRY_NETWORK_GAME_STARTED, nullptr);
                        }
                    }
                }
                else if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(
                             CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                {
                    // A client toggles its ready flag.
                    PlayerParams    lPlayerParams;
                    NetworkPlayerID lLocalNetworkPlayerID;

                    const bool lbHaveLocalPlayer = mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
                    CGS_ASSERT(lbHaveLocalPlayer, "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");
                    lPlayerParams.Prepare();
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                        lLocalNetworkPlayerID, &lPlayerParams);
                    lPlayerParams.SetReady(!lPlayerParams.IsReady());
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->UpdatePlayerParameters(
                        lLocalNetworkPlayerID, &lPlayerParams);
                    meState = E_STATE_WAIT_SERVER_INTERFACE_ACTION;
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_FORCE_LAUNCH:
            {
                GameParams                                lGameParams;
                BrnNetworkModuleIO::NetworkOutLaunchEvent lNetworkLaunchEvent;

                lGameParams.Prepare();
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);
                lNetworkLaunchEvent.meGameModeType = static_cast<GsmIO::EGameModeType>(lGameParams.GameMode());
                lNetworkLaunchEvent.mHostPlayerID  = mpNetworkManager->GetPlayerManager()->GetHostPlayerID();
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkLaunchEvent),
                    lNetworkLaunchEvent.GetEventType(), static_cast<s32>(sizeof(lNetworkLaunchEvent)));

                mpNetworkManager->GetLaunchManager()->Start();
                mpNetworkManager->OnGameLaunching();
                meState = E_STATE_LAUNCHING;
                break;
            }

            case KI_GUI_EVENT_AUTOSAVE_STARTED:
                mbAreWeAutosaving = true;
                break;

            case KI_GUI_EVENT_AUTOSAVE_FINISHED:
                mbAreWeAutosaving = false;
                break;

            case KI_GUI_EVENT_NETWORK_SELECTED_PLAYER_OPTION:
            {
                const BrnGui::GuiEventNetworkSelectedPlayerOption* lpSelectedPlayerOptionEvent =
                    static_cast<const BrnGui::GuiEventNetworkSelectedPlayerOption*>(lpPayload);

                if (!(meState == E_STATE_COUNT || meState == E_STATE_SYNC_TIME || meState == E_STATE_RACING
                      || meState == E_STATE_WAIT_GAME_MODE_START))
                {
                    break;
                }
                // Once the round is syncing or racing nobody can be kicked.
                if ((meState == E_STATE_SYNC_TIME || meState == E_STATE_RACING)
                    && lpSelectedPlayerOptionEvent->meOptionSelected
                           == BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_KICK_PLAYER)
                {
                    break;
                }

                switch (lpSelectedPlayerOptionEvent->meOptionSelected)
                {
                case BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_MARK_PLAYER:
                    if (!mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                    {
                        // In the lobby the mark lives in the player parameters: marking the
                        // player already marked clears it.
                        PlayerParams    lLocalPlayerParams;
                        PlayerParams    lPlayerParams;
                        NetworkPlayerID lLocalNetworkPlayerID;

                        const bool lbHaveLocalPlayer = mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
                        CGS_ASSERT(lbHaveLocalPlayer, "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");
                        lLocalPlayerParams.Prepare();
                        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                            lLocalNetworkPlayerID, &lLocalPlayerParams);
                        lPlayerParams.Prepare();
                        mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                            lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lPlayerParams);

                        if (lLocalPlayerParams.GetMarkedPlayerID() == lPlayerParams.GetID())
                        {
                            UpdateMarkedMan(CgsNetwork::K_INVALID_PLAYER_ID);
                        }
                        else
                        {
                            UpdateMarkedMan(lPlayerParams.GetID());
                        }
                    }
                    else if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                    {
                        // In a game the mark lives in the local menu data and goes to everyone.
                        NetworkPlayerID lLocalNetworkPlayerID;
                        PlayerMenuData* lpMenuData;

                        const bool lbHaveLocalPlayer = mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalNetworkPlayerID);
                        CGS_ASSERT(lbHaveLocalPlayer, "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalNetworkPlayerID )");
                        lpMenuData = static_cast<PlayerMenuData*>(
                            mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lLocalNetworkPlayerID));
                        CGS_ASSERT(lpMenuData, "lpMenuData");

                        if (lpMenuData->mMarkedManID == lpSelectedPlayerOptionEvent->mSelectedPlayerID)
                        {
                            lpMenuData->mMarkedManID = CgsNetwork::K_INVALID_PLAYER_ID;
                        }
                        else
                        {
                            lpMenuData->mMarkedManID = lpSelectedPlayerOptionEvent->mSelectedPlayerID;
                        }
                        mpNetworkManager->GetMarkedManManager()->SendMarkedManDataToAll(false);
                    }
                    break;

                case BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_VIEW_GAMERCARD:
                {
                    u64 lu64XUID;
                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                        && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsPlayerInGame(
                               lpSelectedPlayerOptionEvent->mSelectedPlayerID))
                    {
                        static_cast<CgsNetwork::ServerInterfaceGamesX360*>(
                            mpNetworkManager->GetServerInterface()->GetGameComponent())
                            ->GetPlayerXUIDByID(lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lu64XUID);
                    }
                    else
                    {
                        mCurrentPlayerXUIDs.GetXUID(lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lu64XUID);
                    }
                    XShowGamerCardUI(static_cast<u32>(mpNetworkManager->GetActiveControllerPort()), lu64XUID);
                    break;
                }

                case BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_SUBMIT_REVIEW:
                {
                    u64 lu64XUID;
                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                        && mpNetworkManager->GetServerInterface()->GetGameComponent()->IsPlayerInGame(
                               lpSelectedPlayerOptionEvent->mSelectedPlayerID))
                    {
                        static_cast<CgsNetwork::ServerInterfaceGamesX360*>(
                            mpNetworkManager->GetServerInterface()->GetGameComponent())
                            ->GetPlayerXUIDByID(lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lu64XUID);
                    }
                    else
                    {
                        mCurrentPlayerXUIDs.GetXUID(lpSelectedPlayerOptionEvent->mSelectedPlayerID, &lu64XUID);
                    }
                    XShowPlayerReviewUI(static_cast<u32>(mpNetworkManager->GetActiveControllerPort()), lu64XUID);
                    break;
                }

                case BrnGui::GuiEventNetworkSelectedPlayerOption::E_OPTION_SELECTED_KICK_PLAYER:
                {
                    // A player in our userset is kicked from the userset, anyone else from the game.
                    CgsNetwork::ServerInterfaceGames* lpGames = static_cast<CgsNetwork::ServerInterfaceGames*>(
                        mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetGameComponent());
                    CgsNetwork::ServerInterfaceUsersets* lpUsersets = static_cast<CgsNetwork::ServerInterfaceUsersets*>(
                        mpNetworkManager->GetServerInterface()->GetUsersetsComponent());

                    CGS_ASSERT(lpGames, "lpGames");
                    CGS_ASSERT(lpUsersets, "lpUsersets");

                    if (lpUsersets->IsPlayerInOurUserset(lpSelectedPlayerOptionEvent->mSelectedPlayerID))
                    {
                        if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(
                                CgsNetwork::E_COMPONENTS_USERSETS) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                        {
                            lpUsersets->KickPlayerByID(lpSelectedPlayerOptionEvent->mSelectedPlayerID,
                                                       CgsNetwork::E_KICKREASON_HOSTKICK);
                        }
                    }
                    else if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(
                                 CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                    {
                        lpGames->KickPlayerByID(lpSelectedPlayerOptionEvent->mSelectedPlayerID,
                                                CgsNetwork::E_KICKREASON_HOSTKICK, false);
                    }
                    break;
                }

                default:
                    CGS_ASSERT(false, "Invalid player option");
                    break;
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_HIGHLIGHTED_PLAYER:
            {
                // Watch the highlighted player's camera and show their stats.
                const BrnGui::GuiEventNetworkHighlightedPlayer* lpHighlightedPlayerEvent =
                    static_cast<const BrnGui::GuiEventNetworkHighlightedPlayer*>(lpPayload);

                mpNetworkManager->GetCamera()->RequestFeed(lpHighlightedPlayerEvent->mPlayerID);
                mpNetworkManager->OutputPlayerStatsToGui(lpHighlightedPlayerEvent->mPlayerID);
                break;
            }

            case KI_GUI_EVENT_NETWORK_QUIT_PLAYING:
            {
                // FLAG: the console first logs "***** NetworkManager got a
                // E_GUI_NETWORK_QUIT_PLAYING_EVENT - SO SEND PlayerExitedModeEvent\n" to the
                // unhomed network debug stream; the log line is not reproduced.
                GsmIO::PlayerExitedModeEvent lPlayerExitedModeEvent;
                mpNetworkModule->GetGameEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lPlayerExitedModeEvent),
                    GsmIO::E_EVENT_PLAYER_EXITED_MODE, static_cast<s32>(sizeof(lPlayerExitedModeEvent)));
                break;
            }

            case KI_GUI_EVENT_NETWORK_QUICK_MATCH:
            {
                const BrnGui::GuiEventNetworkQuickMatch* lpQuickMatchEvent =
                    static_cast<const BrnGui::GuiEventNetworkQuickMatch*>(lpPayload);

                mbForceStartFreeburnLobby = false;
                mpNetworkManager->GetMatchMakingManager()->QuickJoinGame(
                    lpQuickMatchEvent->mbRanked, lpQuickMatchEvent->mbFreeburn, QuickJoinFinishedCallback, this);
                meState = E_STATE_WAIT_MATCHMAKING;
                break;
            }

            case KI_GUI_EVENT_NETWORK_CUSTOM_MATCH_SEARCH:
            {
                // Search for games matching the GUI's filters at the local player's rank.
                const BrnGui::GuiEventNetworkCustomMatchSearch* lpCustomMatchSearchEvent =
                    static_cast<const BrnGui::GuiEventNetworkCustomMatchSearch*>(lpRecord);
                GameSearchParams lSearchParams;
                PlayerParams     lPlayerParams;
                PlayerInfoData   lPlayerInfo;

                lPlayerParams.Prepare();
                lPlayerInfo.Prepare();
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);

                lSearchParams.Prepare(lpCustomMatchSearchEvent->meGameMode, E_GAMESTATE_WAITING_FOR_PLAYERS,
                                      static_cast<u32>(lPlayerInfo.GetRank()),
                                      lpCustomMatchSearchEvent->meSearchOpponentTypes,
                                      lpCustomMatchSearchEvent->mbRanked, lpCustomMatchSearchEvent->mbFreeburn,
                                      KU_SEARCH_REQUIRED_SLOTS, lPlayerParams.GetFirewallSettings(),
                                      mpNetworkManager, KU_SEARCH_FIELD_29C);
                lSearchParams.SetReturnPlayers(true);

                mpNetworkManager->GetMatchMakingManager()->SearchForGames(&lSearchParams, SearchFinishedCallback, this);
                meState = E_STATE_WAIT_MATCHMAKING;
                break;
            }

            case KI_GUI_EVENT_NETWORK_CANCEL_SEARCH:
                if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
                {
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->CancelSearchForGames();
                }
                break;

            case KI_GUI_EVENT_NETWORK_CUSTOM_MATCH_JOIN:
            {
                // Join one of the games the last search found.
                const BrnGui::GuiEventNetworkCustomMatchJoin* lpCustomMatchJoinEvent =
                    static_cast<const BrnGui::GuiEventNetworkCustomMatchJoin*>(lpPayload);
                GameParams lGameParams;

                lGameParams.Prepare();
                mpNetworkManager->GetServerInterface()->GetGameComponent()->GetFoundGame(
                    GetPayloadWord(lpCustomMatchJoinEvent), &lGameParams);
                mpNetworkManager->GetServerInterface()->GetGameComponent()->CancelSearchForGames();

                mbForceStartFreeburnLobby = false;
                mpNetworkManager->GetMatchMakingManager()->JoinGame(&lGameParams, false, JoinGameFinishedCallback, this);
                meState = E_STATE_WAIT_MATCHMAKING;
                break;
            }

            case KI_GUI_EVENT_NETWORK_CREATE_GAME:
                CreateGame(static_cast<const BrnGui::GuiEventNetworkCreateGame*>(lpPayload), true);
                break;

            case KI_GUI_EVENT_NETWORK_GAME_PARAMS:
                CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame(),
                           "mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()");
                ModifyGame(static_cast<const BrnGui::GuiEventNetworkGameParams*>(lpPayload));
                break;

            case KI_GUI_EVENT_NETWORK_REQUEST_LOCAL_STATS:
            {
                if (mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
                {
                    PlayerInfoData lPlayerInfo;

                    lPlayerInfo.Prepare();
                    mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetLocalPlayerInfo(&lPlayerInfo);
                    mpNetworkManager->OutputPlayerStatsToGui(lPlayerInfo.GetID());
                }
                else
                {
                    // Offline: show the stats cached for the local player, if they are current.
                    const NetworkPlayerStats* lpPlayerStats = mpNetworkManager->GetStatsManager()->GetLocalPlayerStats();
                    if (lpPlayerStats != nullptr && lpPlayerStats->GetStatus() == NetworkPlayerStats::E_STATS_AGE_CURRENT)
                    {
                        BrnGui::GuiEventNetworkPlayerStats lPlayerStatsEvent;
                        const char* lpcPlayerName = lpPlayerStats->GetName();

                        CGS_ASSERT(std::strlen(lpcPlayerName) < sizeof(lPlayerStatsEvent.macPlayerName),
                                   "String too long: ");
                        std::strncpy(lPlayerStatsEvent.macPlayerName, lpcPlayerName,
                                     sizeof(lPlayerStatsEvent.macPlayerName));
                        lPlayerStatsEvent.mPlayerID = 0;
                        static_cast<NetworkPlayerStats&>(lPlayerStatsEvent) = *lpPlayerStats;
                        lPlayerStatsEvent.miWorldRank = 0;
                        mpNetworkModule->AddOutputGuiEvent(lPlayerStatsEvent);
                    }
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_REQUEST_GAME_PARAMS:
                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
                {
                    mpNetworkManager->OutputGameParameters();
                }
                break;

            case KI_GUI_EVENT_NETWORK_READY_TO_JOIN_SESSION:
                mbReadyToJoinGameSession = true;
                break;

            case KI_GUI_EVENT_NETWORK_OUTPUT_PLAYER_TEXTURE:
            {
                // Which picture to output (and for whom). The game camera and the mugshots
                // take turns: outputting a picture turns the mugshots off.
                const BrnGui::GuiEventNetworkOutputPlayerTexture* lpOutputPlayerTextureEvent =
                    static_cast<const BrnGui::GuiEventNetworkOutputPlayerTexture*>(lpPayload);

                meOutputPlayerTexture    = static_cast<s32>(lpOutputPlayerTextureEvent->meOutput);
                mNetworkPlayerIDToOutput = lpOutputPlayerTextureEvent->mPlayerIDToOutput;

                mpNetworkManager->GetCamera()->SetGameEnabled(
                    meOutputPlayerTexture != BrnGui::GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF);
                mpNetworkManager->GetNetworkImageManager()->EnableMugshotOutput(
                    meOutputPlayerTexture == BrnGui::GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF);

                if (meOutputPlayerTexture == BrnGui::GuiEventNetworkOutputPlayerTexture::E_OUTPUT_OFF)
                {
                    mpNetworkManager->PackTextureAndSendDisplayEventToGui(nullptr, -1);
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_SWITCH_TEAM:
            {
                // In the lobby of a team mode, move the local player to the other team.
                if (meState == E_STATE_COUNT
                    && GameModeHasTeams()
                    && mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(
                           CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE
                    && !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                {
                    NetworkPlayerID lLocalPlayerID;
                    PlayerParams    lPlayerParams;

                    lPlayerParams.Prepare();
                    const bool lbHaveLocalPlayer = mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID);
                    CGS_ASSERT(lbHaveLocalPlayer, "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalPlayerID )");
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->GetPlayerParametersByPlayerID(
                        lLocalPlayerID, &lPlayerParams);

                    if (lPlayerParams.GetPlayerTeam() == GsmIO::E_PLAYER_TEAM_RED_TEAM)
                    {
                        lPlayerParams.SetPlayerTeam(GsmIO::E_PLAYER_TEAM_BLUE_TEAM);
                    }
                    else if (lPlayerParams.GetPlayerTeam() == GsmIO::E_PLAYER_TEAM_BLUE_TEAM)
                    {
                        lPlayerParams.SetPlayerTeam(GsmIO::E_PLAYER_TEAM_RED_TEAM);
                    }

                    mpNetworkManager->GetServerInterface()->GetGameComponent()->UpdatePlayerParameters(lLocalPlayerID,
                                                                                                      &lPlayerParams);
                    meState = E_STATE_WAIT_SERVER_INTERFACE_ACTION;
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_NEWS_AND_TOS:
            {
                const BrnGui::GuiEventNetworkNewsAndTOS* lpNewsAndTOSEvent =
                    static_cast<const BrnGui::GuiEventNetworkNewsAndTOS*>(lpPayload);
                CGS_ASSERT(lpNewsAndTOSEvent, "lpNewsAndTOSEvent");

                if (lpNewsAndTOSEvent->meEventType == BrnGui::GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_REQUEST_NEWS)
                {
                    meState = E_STATE_WAIT_DOWNLOAD_NEWS_IDLE;
                }
                else if (lpNewsAndTOSEvent->meEventType == BrnGui::GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_REQUEST_TOS)
                {
                    meState = E_STATE_WAIT_DOWNLOAD_TOS_IDLE;
                }
                else if (lpNewsAndTOSEvent->meEventType == BrnGui::GuiEventNetworkNewsAndTOS::E_EVENT_TYPE_RELEASE)
                {
                    ReleaseNewsAndTOSDownload();
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_SPLASH:
            {
                // The splash screen has finished: in a game the local player's marking is final.
                const BrnGui::GuiEventNetworkSplashEvent* lpSplashEvent =
                    static_cast<const BrnGui::GuiEventNetworkSplashEvent*>(lpPayload);
                CGS_ASSERT(lpSplashEvent, "lpSplashEvent");

                if (lpSplashEvent->meSplashState == BrnGui::GuiEventNetworkSplashEvent::E_SPLASH_STATE_FINISHED)
                {
                    NetworkPlayerID lLocalPlayerID;

                    CGS_ASSERT(mpNetworkManager, "mpNetworkManager");
                    CGS_ASSERT(mpNetworkManager->GetServerInterface(), "mpNetworkManager->GetServerInterface()");
                    CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetGameComponent(),
                               "mpNetworkManager->GetServerInterface()->GetGameComponent()");
                    CGS_ASSERT(mpNetworkManager->GetPlayerManager(), "mpNetworkManager->GetPlayerManager()");

                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame()
                        && mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID(&lLocalPlayerID))
                    {
                        mpNetworkManager->GetMarkedManManager()->SendMarkedManDataToAll(true);
                        mpNetworkManager->GetMarkedManManager()->MarkingFinished();
                    }
                }
                break;
            }

            case KI_GUI_EVENT_NETWORK_CONNECT:
                HandleConnectEvent(static_cast<const BrnGui::GuiEventNetworkConnect*>(lpRecord));
                break;

            default:
                break;
            }

            liEventId = lpGuiEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
    }

} // namespace BrnNetwork
