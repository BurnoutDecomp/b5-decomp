// ===================================================================================
// BrnNetwork::StateManager -- part 3: the network-event and game-action processors.
//   b5-decomp/src/GameSource/Network/Managers/BrnNetworkStateManager_wN1_03.cpp
//
// Both are called by UpdateEvents once per frame:
//   ProcessNetworkEvents     drains the network-IN event queue (the requests the game and
//                            the GUI post to the network module: invites, account settings,
//                            showtime / skillz / freeburn-challenge broadcasts, camera and
//                            gamer-card requests, ...) and routes each one to the manager
//                            that owns it.
//   ProcessGameStateActions  drains the game-action queue the game state hands the network
//                            (mode start / stop, results, car choices, invites) and moves the
//                            session state machine accordingly.
// Every event type the switch does not name is ignored by ProcessNetworkEvents; an action
// type ProcessGameStateActions does not name fires the "Unknown GameAction" assert.
// ===================================================================================

#include "GameSource/Network/Managers/BrnNetworkStateManager.h"

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                          // CGS_ASSERT, BeginAssert / FireAssert / EndAssert
#include "GameShared/GameClasses/Development/CgsStrStream.h"                                // CgsDev::StrStream (streamed assert messages)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                            // GetFirstEvent / GetNextEvent / AddEvent
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                                    // CgsSystem::Time(f32)
#include "GameShared/GameClasses/Network/Players/CgsPlayerManager.h"                        // CgsNetwork::PlayerManager
#include "GameShared/GameClasses/Network/StartTime/CgsStartTimeManager.h"                   // CgsNetwork::StartTimeManager
#include "GameShared/GameClasses/Network/VoIP/DirtySock/CgsVoIPManagerDirtySock.h"          // CgsNetwork::VoIPManager
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/CgsServerInterfaceDirtySock.h"             // EComponents / EStatus
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceGames.h"      // ServerInterfaceGames
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfaceConnection.h" // ServerInterfaceConnection
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/Components/CgsServerInterfacePlayerInfo.h" // ServerInterfacePlayerInfo
#include "GameSource/GameState/BrnGameActions.h"                                            // the game-action records
#include "GameSource/GameState/BrnGameStateSharedIO.h"                                      // EGameModeType, EPlayerTeam
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"                   // GameModeParams::GetGameModeType
#include "GameSource/Network/BrnNetworkModule.h"                                            // BrnNetworkModule, BrnNetworkManager
#include "GameSource/Network/BrnNetworkModuleIO.h"                                          // NetworkEventQueue
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"                                   // network IN records
#include "GameSource/Network/BrnNetworkOutEventTypeDefs.h"                                  // network OUT records
#include "GameSource/Network/BrnNetworkPlayer.h"                                            // BrnNetworkPlayer
#include "GameSource/Network/BrnNetworkPlayerMenuData.h"                                    // BrnNetwork::PlayerMenuData
#include "GameSource/Network/BrnNetworkGameParams.h"                                        // GameParams
#include "GameSource/Network/BrnServerInterface.h"                                          // BrnServerInterface
#include "GameSource/Network/BrnServerInterfaceBase.h"                                      // GetDownloadableConfigComponent
#include "GameSource/Network/Components/BrnServerInterfaceDownloadableConfig.h"             // the sync-time settings
#include "GameSource/Network/Parameters/BrnNetworkPlayerParams.h"                           // PlayerParamsBase
#include "GameSource/Network/Messages/BrnCollectableMessage.h"                              // CollectableMessage::ECollectableType
#include "GameSource/Network/Managers/BrnNetworkLoginManagerBase.h"                         // LoginManagerBase::CancelLogin
#include "GameSource/Network/Managers/BrnNetworkMatchMakingManager.h"                       // MatchMakingManager
#include "GameSource/Network/Managers/BrnNetworkPostRoundManager.h"                         // PostRoundManager
#include "GameSource/Network/Managers/BrnNetworkStandingsManager.h"                         // StandingsManager
#include "GameSource/Network/Managers/BrnNetworkTrafficManager.h"                           // TrafficManager
#include "GameSource/Network/Managers/BrnNetworkPlayerStatsManager.h"                       // NetworkPlayerStatsManager
#include "GameSource/Network/Managers/BrnNetworkScoreboardManager.h"                        // ScoreboardManager
#include "GameSource/Network/Managers/BrnNetworkLiveRevengeManager.h"                       // LiveRevengeManager
#include "GameSource/Network/Managers/BrnNetworkImageManager.h"                             // NetworkImageManager
#include "GameSource/Network/Managers/BrnNetworkInviteManager.h"                            // NetworkInviteManager

namespace BrnNetwork
{
    namespace GsmIO = BrnGameState::GameStateModuleIO;

    // ---------------------------------------------------------------------------------
    // ProcessNetworkEvents
    //
    // Walk the frame's network-IN queue. Every handled event type has its own case (the
    // console jump table gives each one a distinct target); the rest are ignored.
    // ---------------------------------------------------------------------------------
    void StateManager::ProcessNetworkEvents(const BrnNetworkModuleIO::NetworkEventQueue* lpNetworkEventQueue)
    {
        const CgsModule::Event* lpEvent = nullptr;
        s32 liEventSize = 0;
        s32 liEventType = lpNetworkEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

        while (lpEvent != nullptr)
        {
            switch (liEventType)
            {
            case BrnNetworkModuleIO::NetworkInFriendsUtilShut::KI_EVENT_TYPE:
                // The Guide's friends UI closed: re-read the buddy list.
                mpNetworkManager->GetBuddyManager()->RefreshBuddyList();
                break;

            case BrnNetworkModuleIO::NetworkInSendInvite::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInSendInvite* lpBuddyEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInSendInvite*>(lpEvent);

                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetServerInterface() != nullptr, "mpNetworkManager->GetServerInterface()");
                CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetGameComponent() != nullptr,
                           "mpNetworkManager->GetServerInterface()->GetGameComponent()");

                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
                {
                    // Already in a game: invite straight away, never into a ranked game.
                    GameParams lGameParams;
                    lGameParams.Prepare();
                    mpNetworkManager->GetServerInterface()->GetGameComponent()->GetGameParameters(&lGameParams);
                    CGS_ASSERT(!lGameParams.IsRankedGame(), "!lGameParams.IsRankedGame()");
                    if (!lGameParams.IsRankedGame())
                    {
                        mpNetworkManager->GetBuddyManager()->SendInvite(&lpBuddyEvent->mBuddyToSendTo);
                    }
                }
                else
                {
                    // Not online yet: sign in first, then create a game and invite from it.
                    BrnNetworkModuleIO::NetworkOutShowSignInGui lShowSignInGui;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lShowSignInGui),
                        lShowSignInGui.GetEventType(), static_cast<s32>(sizeof(lShowSignInGui)));

                    if (lpBuddyEvent->mBuddyToSendTo.macName[0] != 0)
                    {
                        mpNetworkManager->GetBuddyManager()->SendInvite(&lpBuddyEvent->mBuddyToSendTo);
                        mbDoInviteAfterCreate = true;
                        mbInstantFreeburn     = false;
                        mbSuspendAfterSignIn  = false;
                    }
                    else
                    {
                        mbSuspendAfterSignIn  = true;
                        mbInstantFreeburn     = false;
                        mbDoInviteAfterCreate = false;
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInRevokeInvite::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInRevokeInvite* lpRevokeInviteEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInRevokeInvite*>(lpEvent);
                CGS_ASSERT(lpRevokeInviteEvent != nullptr, "lpRevokeInviteEvent");

                if (!lpRevokeInviteEvent->mbHasOnlineGameBeenStarted)
                {
                    CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                    CGS_ASSERT(mpNetworkManager->GetBuddyManager() != nullptr, "mpNetworkManager->GetBuddyManager()");

                    // The last open invite went away before the game started: leave the
                    // unlocked game we created for it.
                    if (!mpNetworkManager->GetBuddyManager()->AreAnyInvitesOpen())
                    {
                        CGS_ASSERT(mpNetworkManager->GetServerInterface() != nullptr, "mpNetworkManager->GetServerInterface()");
                        CGS_ASSERT(mpNetworkManager->GetServerInterface()->GetGameComponent() != nullptr,
                                   "mpNetworkManager->GetServerInterface()->GetGameComponent()");

                        if (mpNetworkManager->GetServerInterface()->CgsNetwork::ServerInterfaceDirtySock::GetStatus(
                                CgsNetwork::E_COMPONENTS_GAMES) == CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE &&
                            mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
                            !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameLocked())
                        {
                            mpNetworkManager->GetMatchMakingManager()->StartProcess(
                                MatchMakingManager::E_PROCESS_LEAVE_GAME, LeaveGameForOfflineGameFinishedCallback, this);
                            meState = E_STATE_WAIT_MATCHMAKING;
                        }
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInInstantFreeburn::KI_EVENT_TYPE:
            {
                // Instant freeburn from the menus: sign in, then go straight to a freeburn.
                CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkEventQueue() != nullptr, "mpNetworkModule->GetNetworkEventQueue()");

                BrnNetworkModuleIO::NetworkOutShowSignInGui lShowSignInGui;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lShowSignInGui),
                    lShowSignInGui.GetEventType(), static_cast<s32>(sizeof(lShowSignInGui)));

                BrnNetworkModuleIO::NetworkOutInstantFreeburnEvent lInstantFreeburnEvent;
                lInstantFreeburnEvent.mbIsDoingInstantFreeburn = true;
                mbInstantFreeburn     = true;
                mbSuspendAfterSignIn  = false;
                mbDoInviteAfterCreate = false;
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lInstantFreeburnEvent),
                    lInstantFreeburnEvent.GetEventType(), static_cast<s32>(sizeof(lInstantFreeburnEvent)));
                break;
            }

            case BrnNetworkModuleIO::NetworkInVoipEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInVoipEvent* lpVoipEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInVoipEvent*>(lpEvent);
                // The option screen's 0..10 volume step as a percentage.
                mpNetworkManager->GetVoIPManager()->SetVoipVolume(lpVoipEvent->miVoipVolume * 10);
                break;
            }

            case BrnNetworkModuleIO::NetworkInSettingsUpdateEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInSettingsUpdateEvent* lpSettingsUpdateEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInSettingsUpdateEvent*>(lpEvent);
                mpNetworkManager->GetCamera()->SetUserSetting(lpSettingsUpdateEvent->meCameraFeedSetting);
                break;
            }

            case BrnNetworkModuleIO::NetworkInLiveRevengeProfileLoaded::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInLiveRevengeProfileLoaded* lpLoadedEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInLiveRevengeProfileLoaded*>(lpEvent);
                CGS_ASSERT(lpLoadedEvent != nullptr, "lpLoadedEvent");
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != nullptr, "mpNetworkManager->GetLiveRevengeManager()");
                mpNetworkManager->GetLiveRevengeManager()->HandleLiveRevengeProfileLoadedEvent(lpLoadedEvent->mpLiveRevengeProfile);
                break;
            }

            case BrnNetworkModuleIO::NetworkInLoadingScreenShown::KI_EVENT_TYPE:
                // The loading screen is up: the mode can start now.
                if (meState == E_STATE_WAIT_FOR_LOADING_SCREEN)
                {
                    StartGameMode();
                    meState = E_STATE_WAIT_GAME_MODE_START;
                }
                break;

            case BrnNetworkModuleIO::NetworkInLeftPostEvent::KI_EVENT_TYPE:
                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame())
                {
                    mbSetNotPlaying = true;
                }
                else
                {
                    BrnNetworkModuleIO::NetworkOutPlayerLeftGame lLeftGameEvent;
                    lLeftGameEvent.meLeftGameReason = E_LEFT_GAME_REASON_GAME_DELETED;
                    lLeftGameEvent.meKickReason     = CgsNetwork::E_KICKREASON_COUNT;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lLeftGameEvent),
                        lLeftGameEvent.GetEventType(), static_cast<s32>(sizeof(lLeftGameEvent)));
                }
                break;

            case BrnNetworkModuleIO::NetworkInCancelLogin::KI_EVENT_TYPE:
                CGS_ASSERT(meState == E_STATE_LOGIN, "meState == E_STATE_LOGIN");
                mpNetworkManager->GetLoginManager()->CancelLogin();
                break;

            case BrnNetworkModuleIO::NetworkInLeavingJunkyard::KI_EVENT_TYPE:
                mpNetworkManager->OnFinishedBoot();
                break;

            case BrnNetworkModuleIO::NetworkInPaybackIntialised::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInPaybackIntialised* lpPaybackEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInPaybackIntialised*>(lpEvent);
                CGS_ASSERT(lpPaybackEvent != nullptr, "lpPaybackEvent");
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != nullptr, "mpNetworkManager->GetLiveRevengeManager()");
                mpNetworkManager->GetLiveRevengeManager()->HandlePaybackInitialisedEvent(lpPaybackEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInPaybackSucceeded::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInPaybackSucceeded* lpPaybackEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInPaybackSucceeded*>(lpEvent);
                CGS_ASSERT(lpPaybackEvent != nullptr, "lpPaybackEvent");
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != nullptr, "mpNetworkManager->GetLiveRevengeManager()");
                mpNetworkManager->GetLiveRevengeManager()->HandlePaybackSucceededEvent(lpPaybackEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInSwitchBurningHomeRunRunner::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInSwitchBurningHomeRunRunner* lpSwitchRunnerEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInSwitchBurningHomeRunRunner*>(lpEvent);
                CGS_ASSERT(lpSwitchRunnerEvent != nullptr, "lpSwitchRunnerEvent");
                CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
                CGS_ASSERT(mpNetworkModule->GetNetworkManager() != nullptr, "mpNetworkModule->GetNetworkManager()");
                CGS_ASSERT(mpNetworkModule->GetNetworkManager()->GetPlayerManager() != nullptr,
                           "mpNetworkModule->GetNetworkManager()->GetPlayerManager()");

                // Only the host picks the runner: tell every player, then the local game.
                if (mpNetworkModule->GetNetworkManager()->GetPlayerManager()->AmIHost())
                {
                    NetworkPlayerID lPlayerID = -1;
                    while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                               &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                    {
                        BrnNetworkPlayer* lpNetPlayer =
                            static_cast<BrnNetworkPlayer*>(mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID));
                        if (lpNetPlayer != nullptr)
                        {
                            lpNetPlayer->SendSwitchBurningHomeRunRunnerMessage(lpSwitchRunnerEvent->mNewRunnerID);
                        }
                    }

                    BrnNetworkModuleIO::NetworkOutSwitchBurningHomeRunRunner lSwitchRunnerEvent;
                    lSwitchRunnerEvent.mNewRunnerID = lpSwitchRunnerEvent->mNewRunnerID;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lSwitchRunnerEvent),
                        lSwitchRunnerEvent.GetEventType(), static_cast<s32>(sizeof(lSwitchRunnerEvent)));
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent* lpBurnoutSkillzEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent*>(lpEvent);
                CGS_ASSERT(lpBurnoutSkillzEvent != nullptr, "lpBurnoutSkillzEvent");
                CGS_ASSERT(lpBurnoutSkillzEvent->mPlayerID != -1,
                           "lpBurnoutSkillzEvent->mPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");

                switch (lpBurnoutSkillzEvent->meEventType)
                {
                case BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent::E_EVENT_TYPE_SEND_TO_ALL_PLAYERS:
                {
                    // The local player's skillz changed: send them to every remote player.
                    NetworkPlayerID lNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lNetworkPlayerID != -1 && lpBurnoutSkillzEvent->mPlayerID == lNetworkPlayerID)
                    {
                        lNetworkPlayerID = -1;
                        while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                        {
                            BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                                mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                            CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
                            lpNetworkPlayer->SendNewBurnoutSkillzData(&lpBurnoutSkillzEvent->mNewSkillzData, false);
                        }
                    }
                    break;
                }

                case BrnNetworkModuleIO::NetworkInBurnoutSkillzEvent::E_EVENT_TYPE_SEND_TO_A_SPECIFIC_PLAYER:
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lpBurnoutSkillzEvent->mPlayerID));
                    if (lpNetworkPlayer != nullptr)
                    {
                        lpNetworkPlayer->SendNewBurnoutSkillzData(&lpBurnoutSkillzEvent->mNewSkillzData, true);
                    }
                    break;
                }

                default:
                {
                    CgsDev::Assert::BeginAssert();
                    char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                    lStrStream << "Unhandled burnout skillz network event type: "
                               << static_cast<s32>(lpBurnoutSkillzEvent->meEventType);
                    CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                    CgsDev::Assert::EndAssert();
                    break;
                }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInShowtimeUpdateEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInShowtimeUpdateEvent* lpShowtimeUpdateEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInShowtimeUpdateEvent*>(lpEvent);
                CGS_ASSERT(lpShowtimeUpdateEvent != nullptr, "lpShowtimeUpdateEvent");

                if (lpShowtimeUpdateEvent->mPlayerID != -1)
                {
                    // The local score goes out round-robin, one remote player per turn.
                    NetworkPlayerID lNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lNetworkPlayerID != -1 && lpShowtimeUpdateEvent->mPlayerID == lNetworkPlayerID)
                    {
                        lNetworkPlayerID = -1;
                        while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                        {
                            if (mpNetworkManager->GetPlayerManager()->IsPlayerTurnToSendRoundRobinMessage(
                                    lNetworkPlayerID, true, 0))
                            {
                                BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                                    mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                                CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
                                lpNetworkPlayer->SendShowtimeUpdateData(lpShowtimeUpdateEvent->miShowtimeScore);
                            }
                        }
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInShowtimeSwitchEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInShowtimeSwitchEvent* lpShowtimeModeSwitchEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInShowtimeSwitchEvent*>(lpEvent);
                CGS_ASSERT(lpShowtimeModeSwitchEvent != nullptr, "lpShowtimeModeSwitchEvent");

                if (lpShowtimeModeSwitchEvent->mPlayerID != -1)
                {
                    NetworkPlayerID lNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lNetworkPlayerID != -1 && lpShowtimeModeSwitchEvent->mPlayerID == lNetworkPlayerID)
                    {
                        lNetworkPlayerID = -1;
                        while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                        {
                            BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                                mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                            CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
                            lpNetworkPlayer->SendShowtimeModeSwitchMessage(lpShowtimeModeSwitchEvent->miFinalShowtimeScore,
                                                                           lpShowtimeModeSwitchEvent->mbEnteringShowtime);
                        }
                    }

                    // Leaving showtime in an online game: the host restarts the network traffic.
                    if (!lpShowtimeModeSwitchEvent->mbEnteringShowtime &&
                        mpNetworkModule->GetNetworkManager()->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
                        mpNetworkModule->GetNetworkManager()->GetPlayerManager()->AmIHost())
                    {
                        mpNetworkModule->GetNetworkManager()->GetTrafficManager()->RestartNetworkTrafficForHost();
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent* lpFreeburnChallengeEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInFreeburnChallengeEvent*>(lpEvent);
                NetworkPlayerID lNetworkPlayerID = -1;
                CGS_ASSERT(lpFreeburnChallengeEvent != nullptr, "lpFreeburnChallengeEvent");

                // The host relays the challenge to every player.
                if (mpNetworkModule->GetNetworkManager()->GetPlayerManager()->AmIHost())
                {
                    while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                               &lNetworkPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                    {
                        BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                        if (lpNetworkPlayer != nullptr)
                        {
                            lpNetworkPlayer->SendFreeburnChallengeMessage(lpFreeburnChallengeEvent->mChallengeID,
                                                                          lpFreeburnChallengeEvent->meEventType,
                                                                          lpFreeburnChallengeEvent->meChallengeStatus,
                                                                          lpFreeburnChallengeEvent->miActionIndex);
                        }
                    }
                }

                mpNetworkManager->GetStatsManager()->HandleFreeburnChallengeEvent(lpFreeburnChallengeEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInFburnChallengeStatusEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInFburnChallengeStatusEvent* lpFreeburnChallengeStatusEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInFburnChallengeStatusEvent*>(lpEvent);
                CGS_ASSERT(lpFreeburnChallengeStatusEvent != nullptr, "lpFreeburnChallengeStatusEvent");
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != nullptr, "mpNetworkManager->GetPlayerManager()");

                BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                    mpNetworkManager->GetPlayerManager()->GetPlayerByID(lpFreeburnChallengeStatusEvent->mPlayerID));
                CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
                if (lpNetworkPlayer != nullptr)
                {
                    lpNetworkPlayer->SendFburnChallengeStatusMessage(&lpFreeburnChallengeStatusEvent->mCompletedChallenges);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInActiveFburnChallengeEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInActiveFburnChallengeEvent* lpActiveFreeburnChallengeEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInActiveFburnChallengeEvent*>(lpEvent);
                CGS_ASSERT(lpActiveFreeburnChallengeEvent != nullptr, "lpActiveFreeburnChallengeEvent");
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != nullptr, "mpNetworkManager->GetPlayerManager()");

                BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                    mpNetworkManager->GetPlayerManager()->GetPlayerByID(lpActiveFreeburnChallengeEvent->mPlayerToSendToID));
                CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
                if (lpNetworkPlayer != nullptr)
                {
                    // Translate the challengers' race-car slots into network player ids.
                    NetworkPlayerID laPlayerInChallengeIDs[7];
                    for (s32 liIndex = 0; liIndex < lpActiveFreeburnChallengeEvent->miNumPlayersInChallenge; ++liIndex)
                    {
                        laPlayerInChallengeIDs[liIndex] = mpNetworkModule->GetNetworkPlayerID(
                            lpActiveFreeburnChallengeEvent->maePlayersInChallengeARCI[liIndex]);
                    }
                    lpNetworkPlayer->SendActiveFburnChallengeMessage(lpActiveFreeburnChallengeEvent, laPlayerInChallengeIDs);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInChangeDistrictEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInChangeDistrictEvent* lpChangeDistrictEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInChangeDistrictEvent*>(lpEvent);
                CGS_ASSERT(lpChangeDistrictEvent != nullptr, "lpChangeDistrictEvent");

                mpNetworkManager->SetCurrentDistrict(lpChangeDistrictEvent->mNewWorldRegion.GetDistrict());

                // Waiting in a lobby: push the new district with the next player parameters.
                if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
                    !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                {
                    mDistrictData.mbValid    = true;
                    mDistrictData.meDistrict = lpChangeDistrictEvent->mNewWorldRegion.GetDistrict();
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInLocalPlayerReachesCheckpoint::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInLocalPlayerReachesCheckpoint* lpLandmarkEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInLocalPlayerReachesCheckpoint*>(lpEvent);
                CGS_ASSERT(lpLandmarkEvent != nullptr, "lpEvent");
                NetworkPlayerID lNetworkPlayerID = -1;
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != nullptr, "mpNetworkManager->GetPlayerManager()");

                while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
                    lpNetworkPlayer->SendLandmarkTriggeredMessage(lpLandmarkEvent->miCheckpointIndex);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInStuntScoreUpdatedEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInStuntScoreUpdatedEvent* lpStuntScoreUpdatedEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInStuntScoreUpdatedEvent*>(lpEvent);
                CGS_ASSERT(lpStuntScoreUpdatedEvent != nullptr, "lpStuntScoreUpdatedEvent");
                NetworkPlayerID lNetworkPlayerID = -1;
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != nullptr, "mpNetworkManager->GetPlayerManager()");

                while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
                    lpNetworkPlayer->SendStuntScoreUpdatedMessage(lpStuntScoreUpdatedEvent->miStuntScore);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInStuntMultiplierEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInStuntMultiplierEvent* lpStuntMultiplierEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInStuntMultiplierEvent*>(lpEvent);
                CGS_ASSERT(lpStuntMultiplierEvent != nullptr, "lpStuntMultiplierEvent");
                NetworkPlayerID lNetworkPlayerID = -1;
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetPlayerManager() != nullptr, "mpNetworkManager->GetPlayerManager()");

                while (mpNetworkManager->GetPlayerManager()->GetNextRemotePlayerID(&lNetworkPlayerID))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    CGS_ASSERT(lpNetworkPlayer != nullptr, "lpNetworkPlayer");
                    lpNetworkPlayer->SendStuntMultiplierMessage(*lpStuntMultiplierEvent);
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInShowGamerCard::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInShowGamerCard* lpShowGamerCard =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInShowGamerCard*>(lpEvent);
                CGS_ASSERT(lpShowGamerCard != nullptr, "lpShowGamerCard");
                mpNetworkManager->GetGamerCardManager()->ShowGamerCard(mpNetworkManager->GetLocalUserControllerPort(),
                                                                       &lpShowGamerCard->mPlayerName);
                break;
            }

            case BrnNetworkModuleIO::NetworkInDxtDecodeImageEvent::KI_EVENT_TYPE:
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                mpNetworkManager->ProcessNetworkTextureDecodeEvent(
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInDxtDecodeImageEvent*>(lpEvent));
                break;

            case BrnNetworkModuleIO::NetworkInSelectScoreboardEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInSelectScoreboardEvent* lpScoreboardEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInSelectScoreboardEvent*>(lpEvent);
                CGS_ASSERT(lpScoreboardEvent != nullptr, "lpScoreboardEvent");
                mpNetworkManager->GetScoreboardManager()->HandleScoreboardEvent(lpScoreboardEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInScoreTargetEvent::KI_EVENT_TYPE:
            {
                const EvScoreTargetEvent* lpScoreTargetEvent = reinterpret_cast<const EvScoreTargetEvent*>(lpEvent);
                CGS_ASSERT(lpScoreTargetEvent != nullptr, "lpScoreTargetEvent");
                mpNetworkManager->GetScoreboardManager()->HandleEvScoreTargetEvent(lpScoreTargetEvent);
                break;
            }

            case BrnNetworkModuleIO::NetworkInRequestAccountSettings::KI_EVENT_TYPE:
            {
                BrnNetworkModuleIO::NetworkOutAccountSettings lNetworkAccountSettings;
                mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetAccountSettings(
                    &lNetworkAccountSettings.mbAgreeToShareInfoEA,
                    &lNetworkAccountSettings.mbAgreeToShareInfoPartners,
                    &lNetworkAccountSettings.mbTelemetryEnable);
                mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lNetworkAccountSettings),
                    lNetworkAccountSettings.GetEventType(), static_cast<s32>(sizeof(lNetworkAccountSettings)));
                break;
            }

            case BrnNetworkModuleIO::NetworkInAccountUpdate::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInAccountUpdate* lpAccountUpdateEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInAccountUpdate*>(lpEvent);

                if (mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->GetStatus() ==
                    CgsNetwork::ServerInterfaceDirtySock::E_STATUS_IDLE)
                {
                    CGS_ASSERT(lpAccountUpdateEvent != nullptr, "lpAccountUpdateEvent");
                    mpNetworkManager->GetServerInterface()->GetPlayerInfoComponent()->EditAccount(
                        lpAccountUpdateEvent->mbAgreeToShareInfoEA,
                        lpAccountUpdateEvent->mbAgreeToShareInfoPartners,
                        lpAccountUpdateEvent->mbTelemetryEnable);
                    meState = E_STATE_WAIT_UPDATE_ACCOUNT;
                }
                else
                {
                    // The player-info component is busy: report the update as done (unchanged).
                    BrnNetworkModuleIO::NetworkOutAccountUpdateComplete lNetworkUpdateAccountCompleteEvent;
                    mpNetworkModule->GetNetworkEventQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lNetworkUpdateAccountCompleteEvent),
                        lNetworkUpdateAccountCompleteEvent.GetEventType(),
                        static_cast<s32>(sizeof(lNetworkUpdateAccountCompleteEvent)));
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInReqCamPicEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInReqCamPicEvent* lpReqCompPicEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInReqCamPicEvent*>(lpEvent);
                CGS_ASSERT(renderengine::PIXELFORMAT_DXT1 == lpReqCompPicEvent->meCompressedFormat,
                           "rw::graphics::core::PIXELFORMAT_LIN_DXT1 == lpReqCompPicEvent->meCompressedFormat");
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetCamera() != nullptr, "mpNetworkManager->GetCamera()");
                mpNetworkManager->GetCamera()->GetCompressedLocalCameraPicture(
                    lpReqCompPicEvent->mpTextureToCompressedInto, lpReqCompPicEvent->miQualitySetting,
                    GetCompressedCameraPicCallback, this);
                break;
            }

            case BrnNetworkModuleIO::NetworkInLocalPlayerCrashesEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInLocalPlayerCrashesEvent* lpPlayerCrashesEvent =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInLocalPlayerCrashesEvent*>(lpEvent);
                CGS_ASSERT(lpPlayerCrashesEvent != nullptr, "lpPlayerCrashesEvent");

                // Stamp who the local player crashed into on every player.
                NetworkPlayerID lNetworkPlayerID = -1;
                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lNetworkPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    if (lpNetworkPlayer != nullptr)
                    {
                        NetworkPlayerID lCrasherEntityNetworkID;
                        if (lpPlayerCrashesEvent->meCrasherRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0 &&
                            lpPlayerCrashesEvent->meCrasherRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT)
                        {
                            CGS_ASSERT(mpNetworkModule != nullptr, "mpNetworkModule");
                            lCrasherEntityNetworkID = mpNetworkModule->GetNetworkPlayerID(lpPlayerCrashesEvent->meCrasherRaceCarIndex);
                        }
                        else
                        {
                            lCrasherEntityNetworkID = -1;
                        }
                        lpNetworkPlayer->SetLocalPlayerCrasherID(lCrasherEntityNetworkID);
                    }
                }
                break;
            }

            case BrnNetworkModuleIO::NetworkInShowGamerCardForXuidEvent::KI_EVENT_TYPE:
            {
                const BrnNetworkModuleIO::NetworkInShowGamerCardForXuidEvent* lpShowGamerCard =
                    reinterpret_cast<const BrnNetworkModuleIO::NetworkInShowGamerCardForXuidEvent*>(lpEvent);
                CGS_ASSERT(lpShowGamerCard != nullptr, "lpShowGamerCard");
                mpNetworkManager->GetGamerCardManager()->ShowGamerCardForXuid(mpNetworkManager->GetLocalUserControllerPort(),
                                                                              lpShowGamerCard->mu64Xuid);
                break;
            }

            default:
                break;
            }

            liEventType = lpNetworkEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
        }
    }

    // ---------------------------------------------------------------------------------
    // ProcessGameStateActions
    //
    // Walk the frame's game-action queue. The game-state bridge forwards only the actions
    // the network acts on, so any other type is reported as unknown.
    // ---------------------------------------------------------------------------------
    void StateManager::ProcessGameStateActions(const CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue)
    {
        const CgsModule::Event* lpAction = nullptr;
        s32 liActionSize = 0;
        s32 liActionType = lpGameActionQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction != nullptr)
        {
            switch (liActionType)
            {
            case GsmIO::E_ACTION_RESET_PLAYER_CAR:
            {
                const GsmIO::ResetPlayerCarAction* lpResetPlayerCarAction =
                    reinterpret_cast<const GsmIO::ResetPlayerCarAction*>(lpAction);

                if (lpResetPlayerCarAction->mCarModelId != 0)
                {
                    CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
                    if (!lpGames->IsLocalPlayerInGame() || !lpGames->IsGameStarted())
                    {
                        mpNetworkManager->SetFreeBurnCar(lpResetPlayerCarAction->mCarModelId,
                                                         lpResetPlayerCarAction->mWheelModelId,
                                                         lpResetPlayerCarAction->mfDeformationAmount);
                    }

                    // In a lobby that has not started: push the car with the next player parameters.
                    if (mpNetworkManager->GetServerInterface()->GetGameComponent()->IsLocalPlayerInGame() &&
                        !mpNetworkManager->GetServerInterface()->GetGameComponent()->IsGameStarted())
                    {
                        CGS_ASSERT(lpResetPlayerCarAction->mCarModelId != 0,
                                   "lpResetPlayerCarAction->GetCarModelId() != kCGSID_NULL");
                        mNewCarData.mCarModelId     = lpResetPlayerCarAction->mCarModelId;
                        mNewCarData.mWheelModelId   = lpResetPlayerCarAction->mWheelModelId;
                        mNewCarData.mfDeformAmount  = lpResetPlayerCarAction->mfDeformationAmount;
                        mNewCarData.mbValid         = true;
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE:
            {
                const GsmIO::OnStuntElementCompleteAction* lpStuntElementAction =
                    reinterpret_cast<const GsmIO::OnStuntElementCompleteAction*>(lpAction);
                CGS_ASSERT(lpStuntElementAction != nullptr, "lpStuntElementAction");
                NetworkPlayerID lNetworkPlayerID = -1;

                CollectableMessage::ECollectableType leCollectableType;
                switch (lpStuntElementAction->meStuntElementType)
                {
                case BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP:
                    leCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_JUMP;
                    break;
                case BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH:
                    leCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_SMASH;
                    break;
                case BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD:
                    leCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_BILLBOARD;
                    break;
                default:
                    CGS_ASSERT(false, "Unknown stunt element type\n");
                    leCollectableType = CollectableMessage::E_COLLECTABLE_TYPE_JUMP;
                    break;
                }

                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lNetworkPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    BrnNetworkPlayer* lpNetworkPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lNetworkPlayerID));
                    if (lpNetworkPlayer != nullptr)
                    {
                        lpNetworkPlayer->SendCollectableDone(lpStuntElementAction->mID, leCollectableType);
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_PREPARE_FOR_MODE:
            {
                const GsmIO::PrepareForModeAction* lpPrepareForModeAction =
                    reinterpret_cast<const GsmIO::PrepareForModeAction*>(lpAction);
                CGS_ASSERT(lpPrepareForModeAction != nullptr, "lpPrepareForModeAction");

                // First prepare of an online mode: load the sync-time settings from the
                // downloaded config, unless the players are just moving between the two
                // lobby modes.
                if (lpPrepareForModeAction->IsFirstPrepareForMode() &&
                    lpPrepareForModeAction->GetGameModeParams()->GetGameModeType() >= GsmIO::E_MODE_ONLINE_MODE_START)
                {
                    const GsmIO::EGameModeType leGameMode = lpPrepareForModeAction->GetGameModeParams()->GetGameModeType();
                    const bool lbLobbyModeSwitch =
                        lpPrepareForModeAction->IsMovingBetweenOnlineLobbyModes() &&
                        (leGameMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY || leGameMode == GsmIO::E_MODE_ONLINE_SHOWTIME);

                    if (!lbLobbyModeSwitch)
                    {
                        mpNetworkManager->GetStartTimeManager()->PrepareStartTime();

                        BrnServerInterfaceDownloadableConfig* lpDownloadableConfig =
                            mpNetworkManager->GetServerInterface()->GetDownloadableConfigComponent();

                        mpNetworkManager->GetStartTimeManager()->SetSyncTimeTimeouts(
                            CgsSystem::Time(lpDownloadableConfig->MinSyncTime()),
                            CgsSystem::Time(lpDownloadableConfig->MaxSyncTime()));
                        mpNetworkManager->GetStartTimeManager()->SetWaitForClientReadyTimeouts(
                            CgsSystem::Time(lpDownloadableConfig->TimeToWaitForSilentClientReady()),
                            CgsSystem::Time(lpDownloadableConfig->TimeToWaitForCommunicatingClientReady()));
                        mpNetworkManager->GetStartTimeManager()->SetWaitForStartTimeTimeout(
                            CgsSystem::Time(lpDownloadableConfig->WaitForStartTime()));

                        // The lobby modes start at once; every other mode leaves the configured gap.
                        const GsmIO::EGameModeType leStartMode = lpPrepareForModeAction->GetGameModeParams()->GetGameModeType();
                        if (leStartMode == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY || leStartMode == GsmIO::E_MODE_ONLINE_SHOWTIME)
                        {
                            mpNetworkManager->GetStartTimeManager()->SetGapTillStartTime(CgsSystem::Time(0.0f));
                        }
                        else
                        {
                            mpNetworkManager->GetStartTimeManager()->SetGapTillStartTime(
                                CgsSystem::Time(lpDownloadableConfig->TimeGapBeforeStartTime()));
                        }
                    }
                }

                // An online race starts every player on the start line.
                if (lpPrepareForModeAction->GetGameModeParams()->GetGameModeType() == GsmIO::E_MODE_ONLINE_RACE)
                {
                    NetworkPlayerID lPlayerID = -1;
                    while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                               &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                    {
                        BrnNetworkPlayer* lpNetPlayer = static_cast<BrnNetworkPlayer*>(
                            mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID));
                        if (lpNetPlayer != nullptr)
                        {
                            lpNetPlayer->SetOnStartLine(true);
                        }
                    }
                }

                // Online showtime holds the traffic restart; every other mode releases it.
                const GsmIO::EGameModeType leModeType = lpPrepareForModeAction->GetGameModeParams()->GetGameModeType();
                if ((leModeType == GsmIO::E_MODE_OFFLINE_SHOWTIME || leModeType == GsmIO::E_MODE_ONLINE_SHOWTIME) &&
                    leModeType >= GsmIO::E_MODE_ONLINE_MODE_START && leModeType <= GsmIO::E_MODE_ONLINE_MODE_END)
                {
                    mpNetworkManager->GetTrafficManager()->SuppressTrafficRestart(true);
                }
                else
                {
                    mpNetworkManager->GetTrafficManager()->SuppressTrafficRestart(false);
                }
                break;
            }

            case GsmIO::E_ACTION_START_MODE_INTRO:
            {
                const GsmIO::StartModeIntroAction* lpStartModeIntro =
                    reinterpret_cast<const GsmIO::StartModeIntroAction*>(lpAction);
                CGS_ASSERT(lpStartModeIntro != nullptr, "lpStartModeIntro");

                // Online stunt run: players only talk to their own team.
                if (lpStartModeIntro->meGameMode == GsmIO::E_MODE_ONLINE_FUGITIVE)
                {
                    CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                    CGS_ASSERT(mpNetworkManager->GetVoIPManager() != nullptr, "mpNetworkManager->GetVoIPManager()");
                    CGS_ASSERT(mpNetworkManager->GetPlayerManager() != nullptr, "mpNetworkManager->GetPlayerManager()");

                    const NetworkPlayerID lLocalPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lLocalPlayerID != -1)
                    {
                        PlayerParamsBase lPlayerParams;
                        CGS_ASSERT(mpNetworkManager->GetServerInterface() != nullptr, "mpNetworkManager->GetServerInterface()");
                        CgsNetwork::ServerInterfaceGames* lpGames = mpNetworkManager->GetServerInterface()->GetGameComponent();
                        CGS_ASSERT(lpGames != nullptr, "lpGames");

                        lPlayerParams.PreparePattern();
                        lpGames->GetPlayerParametersByPlayerID(lLocalPlayerID, &lPlayerParams);
                        const GsmIO::EPlayerTeam leLocalTeam = lPlayerParams.GetPlayerTeam();
                        if (leLocalTeam == GsmIO::E_PLAYER_TEAM_NONE)
                        {
                            CgsDev::Assert::BeginAssert();
                            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                            CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                            lStrStream << "Starting online stunt run but the local player is not on a team\n";
                            CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                            CgsDev::Assert::EndAssert();
                        }

                        NetworkPlayerID lPlayerID = -1;
                        while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                                   &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_ALL_PLAYERS))
                        {
                            if (lpGames->IsPlayerInGameByID(lPlayerID) && lPlayerID != lLocalPlayerID)
                            {
                                lPlayerParams.PreparePattern();
                                lpGames->GetPlayerParametersByPlayerID(lPlayerID, &lPlayerParams);
                                if (lPlayerParams.GetPlayerTeam() != leLocalTeam)
                                {
                                    mpNetworkManager->GetVoIPManager()->DisableCommsWithPlayer(lPlayerID);
                                }
                            }
                        }
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_STOP_MODE_INTRO:
            {
                const GsmIO::StopModeIntroAction* lpStopIntroAction =
                    reinterpret_cast<const GsmIO::StopModeIntroAction*>(lpAction);
                if (lpStopIntroAction->meGameMode >= GsmIO::E_MODE_ONLINE_MODE_START &&
                    lpStopIntroAction->meGameMode <= GsmIO::E_MODE_ONLINE_MODE_END)
                {
                    IntroStopped(lpStopIntroAction);
                }
                break;
            }

            case GsmIO::E_ACTION_MARKED_MAN_LOADED:
            {
                const GsmIO::MarkedManLoadedAction* lpMarkedManLoadedAction =
                    reinterpret_cast<const GsmIO::MarkedManLoadedAction*>(lpAction);
                if (lpMarkedManLoadedAction->meGameMode >= GsmIO::E_MODE_ONLINE_MODE_START &&
                    lpMarkedManLoadedAction->meGameMode <= GsmIO::E_MODE_ONLINE_MODE_END)
                {
                    MarkedManLoaded(lpMarkedManLoadedAction);
                }
                break;
            }

            case GsmIO::E_ACTION_START_PLAYING_MODE:
            {
                // The mode is running: nobody is on the start line any more.
                NetworkPlayerID lPlayerID = -1;
                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    BrnNetworkPlayer* lpNetPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID));
                    if (lpNetPlayer != nullptr)
                    {
                        lpNetPlayer->SetOnStartLine(false);
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_FINISHED_MODE:
            {
                const GsmIO::FinishedModeAction* lpFinishedModeAction =
                    reinterpret_cast<const GsmIO::FinishedModeAction*>(lpAction);
                CGS_ASSERT(lpFinishedModeAction != nullptr, "lpModeResultsAction");

                // The console also writes two progress lines to the network debug log here;
                // that log stream has no home in the tree, so only the behaviour is kept.
                if (lpFinishedModeAction->mbIsOnlineGameMode &&
                    lpFinishedModeAction->meFinishedGameModeType != GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
                    lpFinishedModeAction->meFinishedGameModeType != GsmIO::E_MODE_ONLINE_SHOWTIME)
                {
                    const NetworkPlayerID lLocalNetworkPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                    if (lLocalNetworkPlayerID != -1)
                    {
                        mpNetworkManager->GetStandingsManager()->HandlePlayerFinishedMode(lLocalNetworkPlayerID,
                                                                                         lpFinishedModeAction);
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_SHOW_MODE_RESULTS:
            {
                const GsmIO::ShowModeResultsAction* lpShowModeResultsAction =
                    reinterpret_cast<const GsmIO::ShowModeResultsAction*>(lpAction);
                CGS_ASSERT(lpShowModeResultsAction != nullptr, "lpShowModeResultsAction");

                if (lpShowModeResultsAction->mbIsOnlinePostEvent &&
                    lpShowModeResultsAction->meGameModeType != GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
                    lpShowModeResultsAction->meGameModeType != GsmIO::E_MODE_ONLINE_SHOWTIME)
                {
                    meState = E_STATE_WAIT_POST_ROUND;
                    mpNetworkManager->GetPostRoundManager()->StartProcess(PostRoundManager::E_PROCESS_END_ROUND,
                                                                          PostRoundFinishedCallback, this);
                }

                mpNetworkManager->GetTrafficManager()->SuppressTrafficRestart(false);

                // Stunt run over: everybody may talk to everybody again.
                if (lpShowModeResultsAction->meGameModeType == GsmIO::E_MODE_ONLINE_FUGITIVE)
                {
                    CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                    CGS_ASSERT(mpNetworkManager->GetVoIPManager() != nullptr, "mpNetworkManager->GetVoIPManager()");
                    mpNetworkManager->GetVoIPManager()->EnableAllComms();
                }
                break;
            }

            case GsmIO::E_ACTION_STOP_MODE:
            {
                const GsmIO::StopModeAction* lpStopModeAction = reinterpret_cast<const GsmIO::StopModeAction*>(lpAction);
                CGS_ASSERT(lpStopModeAction != nullptr, "lpStopModeAction");

                if (lpStopModeAction->mu8Field10 &&
                    lpStopModeAction->meGameModeType != GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
                    lpStopModeAction->meGameModeType != GsmIO::E_MODE_ONLINE_SHOWTIME)
                {
                    if (lpStopModeAction->mu8Field12 || lpStopModeAction->mu8Field11 || lpStopModeAction->mu8Field13)
                    {
                        // Last round played (or the game is being left): end the game.
                        meState = E_STATE_WAIT_POST_ROUND;
                        mpNetworkManager->GetPostRoundManager()->StartEndOfGame(
                            lpStopModeAction->mu8Field11 != 0,
                            lpStopModeAction->mu8Field11 != 0 || lpStopModeAction->mu8Field13 != 0,
                            PostGameFinishedCallback, this);
                        mpNetworkModule->GetNetworkManager()->SetRound(-1);
                    }
                    else
                    {
                        // More rounds to play: start the next one.
                        SendStartNextRoundMessage(lpStopModeAction->miField08 + 1);
                        mpNetworkModule->GetNetworkManager()->SetRound(lpStopModeAction->miField08 + 1);
                        mpNetworkModule->GetNetworkManager()->GetStandingsManager()->ClearStandingsData();
                    }

                    mpNetworkManager->GetNetworkImageManager()->HandlePlayerStoppedMode();
                    mpNetworkManager->GetStartTimeManager()->StopSyncingTime();
                }

                mpNetworkManager->GetTrafficManager()->SuppressTrafficRestart(false);
                break;
            }

            case GsmIO::E_ACTION_CAR_SELECT_ONLINE_SELECT_CAR:
            {
                const GsmIO::CarSelectOnlineSelectCarAction* lpSelectCarAction =
                    reinterpret_cast<const GsmIO::CarSelectOnlineSelectCarAction*>(lpAction);

                NetworkPlayerID lPlayerID = -1;
                const NetworkPlayerID lLocalPlayerID = mpNetworkManager->GetPlayerManager()->GetLocalPlayerID();
                CGS_ASSERT(lLocalPlayerID != -1,
                           "mpNetworkManager->GetPlayerManager()->GetNextLocalPlayerID( &lLocalPlayerID )");

                PlayerMenuData* lpMenuData =
                    static_cast<PlayerMenuData*>(mpNetworkManager->GetPlayerManager()->GetMenuDataByID(lLocalPlayerID));
                CGS_ASSERT(lpMenuData != nullptr, "lpMenuData");

                // Record the choice on the local menu data, then tell every player.
                lpMenuData->mWheelId             = 0;
                lpMenuData->mCarId               = lpSelectCarAction->mCarID;
                lpMenuData->mu16CarColourIndex   = lpSelectCarAction->mu16ColourIndex;
                lpMenuData->mu16PaintFinishIndex = lpSelectCarAction->mu16PaintFinishIndex;
                lpMenuData->mfUnknown48          = lpSelectCarAction->mfDeformationAmount;
                lpMenuData->mbFinalCarSelection  = lpSelectCarAction->mbFinalSelection;

                while (mpNetworkManager->GetPlayerManager()->GetNextPlayerID(
                           &lPlayerID, CgsNetwork::PlayerManager::E_CONSIDER_PLAYERS_WHO_HAVE_FINALISED))
                {
                    BrnNetworkPlayer* lpNetPlayer = static_cast<BrnNetworkPlayer*>(
                        mpNetworkManager->GetPlayerManager()->GetPlayerByID(lPlayerID));
                    if (lpNetPlayer != nullptr)
                    {
                        lpNetPlayer->SendSelectedCar(lpMenuData->mCarId, lpMenuData->mWheelId,
                                                     lpMenuData->mu16CarColourIndex, lpMenuData->mu16PaintFinishIndex,
                                                     lpMenuData->mfUnknown48, lpSelectCarAction->mbFinalSelection);
                    }
                }
                break;
            }

            case GsmIO::E_ACTION_UPDATE_PREPARE_FOR_INVITE:
                PrepareForInvite();
                break;

            case GsmIO::E_ACTION_PERFORM_INVITE:
            {
                const GsmIO::PerformInviteAction* lpPerformInviteAction =
                    reinterpret_cast<const GsmIO::PerformInviteAction*>(lpAction);
                CGS_ASSERT(lpPerformInviteAction != nullptr, "lpPerformInviteAction");

                // A different local user is taking the invite: drop the current sign-in first.
                if (lpPerformInviteAction->mInviteParams.mbIsLocalUserChangeNeeded &&
                    mpNetworkManager->GetServerInterface()->GetConnectionComponent()->IsLoggedIn())
                {
                    mpNetworkManager->GetServerInterface()->GetConnectionComponent()->DisconnectFromServer();
                }

                mpNetworkManager->GetNetworkInviteManager()->StartInviteOrJoin(lpPerformInviteAction->mInviteParams);
                meState = E_STATE_COUNT;
                break;
            }

            case GsmIO::E_ACTION_PLAYER_ELIMINATED:
            {
                const GsmIO::PlayerEliminatedAction* lpPlayerEliminatedAction =
                    reinterpret_cast<const GsmIO::PlayerEliminatedAction*>(lpAction);
                const NetworkPlayerID lEliminatedPlayerID =
                    mpNetworkModule->GetNetworkPlayerID(lpPlayerEliminatedAction->meActiveRaceCarIndex);
                BrnNetworkPlayer* lpPlayer =
                    static_cast<BrnNetworkPlayer*>(mpNetworkManager->GetPlayerManager()->GetPlayerByID(lEliminatedPlayerID));
                if (lpPlayer != nullptr)
                {
                    lpPlayer->SetEliminated(true);
                }
                break;
            }

            case GsmIO::E_ACTION_ONLINE_GAME_RESULT:
            {
                const GsmIO::OnlineGameResults* lpResults = reinterpret_cast<const GsmIO::OnlineGameResults*>(lpAction);
                CGS_ASSERT(lpResults != nullptr, "lpResults");
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetStatsManager() != nullptr, "mpNetworkManager->GetStatsManager()");

                if (lpResults->miEventType == GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                    lpResults->miEventType == GsmIO::E_MODE_ONLINE_SHOWTIME)
                {
                    mpNetworkManager->GetStatsManager()->UploadFreeBurnLobbyStats(lpResults);
                }
                else
                {
                    CGS_ASSERT(mpNetworkManager->GetPostRoundManager() != nullptr, "mpNetworkManager->GetPostRoundManager()");
                    CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != nullptr, "mpNetworkManager->GetLiveRevengeManager()");
                    mpNetworkManager->GetPostRoundManager()->ProcessRaceResults(lpResults);
                    mpNetworkManager->GetStatsManager()->HandleGameResults(lpResults);
                }
                break;
            }

            case GsmIO::E_ACTION_ONLINE_ROUND_RESULT:
            {
                const GsmIO::OnlineRoundResults* lpRoundResults = reinterpret_cast<const GsmIO::OnlineRoundResults*>(lpAction);
                CGS_ASSERT(lpRoundResults != nullptr, "lpRoundResults");
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                CGS_ASSERT(mpNetworkManager->GetLiveRevengeManager() != nullptr, "mpNetworkManager->GetLiveRevengeManager()");
                mpNetworkManager->GetLiveRevengeManager()->HandleRoundResults(lpRoundResults);
                mpNetworkManager->GetNetworkImageManager()->HandleRoundResults(lpRoundResults);
                break;
            }

            case GsmIO::E_ACTION_NETWORK_CAUGHT_FEVER:
                CGS_ASSERT(mpNetworkManager != nullptr, "mpNetworkManager");
                mpNetworkManager->SetHasFever(true);
                mFeverData.mbValid    = true;
                mFeverData.mbHasFever = true;
                break;

            case GsmIO::E_ACTION_ACHIEVEMENTS_EARNED:
            {
                const GsmIO::AchievementsEarnedAction* lpAchievementAction =
                    reinterpret_cast<const GsmIO::AchievementsEarnedAction*>(lpAction);
                CGS_ASSERT(lpAchievementAction != nullptr, "lpAchievementAction");
                mpNetworkManager->GetStatsManager()->HandleNewNumberOfAchievements(lpAchievementAction->miAchivementCount);
                break;
            }

            default:
            {
                CgsDev::Assert::BeginAssert();
                char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream(lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
                lStrStream << "Unknown GameAction coming into NetworkManager: " << liActionType << "\n";
                CgsDev::Assert::FireAssert(lStrStream.GetBuffer(), __FILE__, __LINE__);
                CgsDev::Assert::EndAssert();
                break;
            }
            }

            liActionType = lpGameActionQueue->GetNextEvent(lpAction, &lpAction, &liActionSize);
        }
    }
} // namespace BrnNetwork
