#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include <cstring>                                                                         // memset
#include "GameShared/GameClasses/Core/CgsAssert.h"                                         // CGS_ASSERT
#include "GameShared/GameClasses/Network/Players/CgsNetworkPlayer.h"                       // CgsNetwork::KI_INVALID_PLAYER_ID
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                       // GetNextPlayerID / GetMenuDataByID
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                     // EGameModeType / EPlayerTeam
#include "GameSource/Network/BrnNetworkGameParams.h"                                       // BrnNetwork::GameParams
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"                     // BrnNetwork::PlayerParams
#include "GameSource/Network/BrnNetworkManager.h"
#include "GameSource/Network/BrnNetworkModule.h"
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"
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
// ===================================================================================

namespace BrnNetwork
{
    namespace GsmIO = BrnGameState::GameStateModuleIO;

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
            // FLAG: the console clears nine flags here (its GsmIO::E_PLAYER_TEAM_COUNT is 9:
            // the PlayerParams team asserts compare against 9). The committed EPlayerTeam
            // still carries the reference value 3; the array follows the enum by name.
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
                // FLAG: GsmIO::IsShowtimeGameMode (offline or online showtime) has no home yet; its
                // body is spelled out until it does.
                CGS_ASSERT(!(lpStopIntroAction->meGameMode == GsmIO::E_MODE_OFFLINE_SHOWTIME
                             || lpStopIntroAction->meGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME),
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
                // FLAG: GsmIO::IsShowtimeGameMode (offline or online showtime) has no home yet; its
                // body is spelled out until it does.
                CGS_ASSERT(!(lpMarkedManLoadedAction->meGameMode == GsmIO::E_MODE_OFFLINE_SHOWTIME
                             || lpMarkedManLoadedAction->meGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME),
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

} // namespace BrnNetwork
