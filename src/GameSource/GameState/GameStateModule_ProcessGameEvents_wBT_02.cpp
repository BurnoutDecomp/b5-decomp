// GameStateModule::ProcessGameEvents_Group2 -- the online lobby, network game / round start and
// remote-player arms of the game-event dispatcher (GameStateModule_ProcessGameEvents.cpp routes
// these ids here, one event per call).
//
//   126 ONLINE_GAME_LAUNCHED       leave the junkyard, reset the street scores, enter online car select
//   17  START_NETWORK_GAME         latch the lobby roster in the NetworkRoundManager, seed the game
//   18  START_NETWORK_ROUND        start the latched mode for the next round
//   20  PLAYER_ACCEPTED_MODE       start an offline race over the event's checkpoints
//   127 ONLINE_PLAYER_ADDED        scoring record + action 219, challenge feed / join training tips
//   128 ONLINE_PLAYER_FINALISED    a remote player's join is complete
//   129 ONLINE_PLAYER_REMOVED      drop the player from the mode and the scoring system, action 220
//   121 REMOTE_PLAYER_DISCONNECTED action 11, then the same mode / skillz bookkeeping
//   122 LOCAL_PLAYER_CONNECTED     latch the local network id
//   123 LOCAL_PLAYER_DISCONNECTED  abort the online session, action 12, single-player frame rate
//   124 LOCAL_PLAYER_LEFT_LOBBY    cancel the lobby, restore the free-burn car, clear every skillz
//   125 ONLINE_GAME_PARAMS_CHANGED rich-presence lobby parameters
//   139 ONLINE_NEW_BURNOUT_SKILLZ  a remote player's skillz record
//   140 ONLINE_NEW_HOST            host migration
//
// The cases are in source order. lpPreWorldInput's player-status interface is read through its
// accessor; the dispatcher's caller holds the read lock.

#include "GameSource/GameState/BrnGameStateModule.h"

#include <cstring>                                                         // std::memcpy (case 123 frame-rate request)

#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                             // CgsIDCompress / CgsIDUnCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                 // gpDebugPrint, gxMessageFilterFlags
#include "GameShared/GameClasses/Development/CgsStrStream.h"               // StrStreamBase::operator<<
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // VariableEventQueue::AddEvent
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"            // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"              // CgsSystem::E_FRAMERATEMANAGER_MULTIPLE_CAPPED
#include "GameSource/GameState/BrnGameStateModuleIO.h"                     // PreWorldInputBuffer / OutputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                            // the event records
#include "GameSource/GameState/BrnGameActions.h"                           // actions 11 / 12 / 44 / 219 / 220 / 235
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"
#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h"  // OnlineFlybyManager::SetRandomNetworkGameSeed
#include "GameSource/GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.h"
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"  // StartGameModeParams
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/Progression/BrnProfile.h"
#include "SharedClasses/Progression/BrnTrainingTypes.h"                     // the three online join tips
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"  // GetLocalPlayerIsHost

namespace BrnGameState
{
namespace
{
    // The module's file-scope invalid landmark index (rodata 0xFFFF), the id case 18 posts in action 44.
    const u16 KU_INVALID_LANDMARK_INDEX = 0xFFFFu;
}

void GameStateModule::ProcessGameEvents_Group2(s32                                           liEventType,
                                               const CgsModule::Event*                       lpEvent,
                                               GameStateModuleIO::GameActionQueue*           lpActionQueue,
                                               const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                               GameStateModuleIO::OutputBuffer*              lpOutput)
{
    switch (liEventType)
    {
    case GameStateModuleIO::E_EVENT_ONLINE_GAME_LAUNCHED:
    {
        const GameStateModuleIO::OnlineGameLaunchedEvent* lpLaunchedEvent =
            reinterpret_cast<const GameStateModuleIO::OnlineGameLaunchedEvent*>(lpEvent);
        CGS_ASSERT(lpLaunchedEvent, "lpLaunchedEvent");

        if (lpLaunchedEvent->mbSuccessfulLaunch)
        {
            if (mCarSelectManager.IsInJunkyard())
            {
                mCarSelectManager.ForceExitJunkyard(lpActionQueue, true);
            }
            mStreetManager.ProcessOnlineGameLaunchedEvent();

            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface =
                &mLastActiveRaceCarInterface;
            CGS_ASSERT(lpActiveRaceCarInterface->IsPlayerCarActive(),
                       "lpActiveRaceCarInterface->IsPlayerCarActive()");

            const Vector3 lPlayerDirection = lpActiveRaceCarInterface->GetPlayerDirection();
            const Vector3 lPlayerPosition  = lpActiveRaceCarInterface->GetPlayerPosition();
            mOnlineCarSelectManager.EnterOnlineCarSelect(lpActionQueue, lPlayerPosition, lPlayerDirection,
                                                         lpLaunchedEvent->miVehicleClassLimit,
                                                         lpLaunchedEvent->mbHostChoiceCarAndNotHost);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_START_NETWORK_GAME:
    {
        const GameStateModuleIO::StartNetworkGameEvent* lpStartNetworkGameEvent =
            reinterpret_cast<const GameStateModuleIO::StartNetworkGameEvent*>(lpEvent);
        CGS_ASSERT(lpStartNetworkGameEvent, "lpStartNetworkGameEvent");

        if (lpStartNetworkGameEvent->mbRefreshOnly)
        {
            CGS_ASSERT(GameStateModuleIO::IsOnlineFreeBurnLobby(lpStartNetworkGameEvent->meGameMode),
                       "GsmIO::IsOnlineFreeBurnLobby(lpStartNetworkGameEvent->meGameMode)");
            mNetworkRoundManager.NetworkGameStarted(lpStartNetworkGameEvent);
            mNetworkRoundManager.OnRoundStart();
            break;
        }

        if (GameStateModuleIO::IsOnlineFreeBurnLobby(lpStartNetworkGameEvent->meGameMode))
        {
            const BrnNetwork::NetworkPlayerID lLocalNetworkPlayerID = lpStartNetworkGameEvent->mLocalNetworkPlayerID;
            CGS_ASSERT(lpStartNetworkGameEvent->miNumRaceCars > 0, "lpStartNetworkGameEvent->miNumRaceCars > 0");

            for (s32 liRaceCar = 0; liRaceCar < lpStartNetworkGameEvent->miNumRaceCars; ++liRaceCar)
            {
                if (lLocalNetworkPlayerID == lpStartNetworkGameEvent->maNetworkPlayerID[liRaceCar])
                {
                    OnSpecialEventPlayerCarChange(lpStartNetworkGameEvent->maCarIds[liRaceCar], 0, lpActionQueue, true);
                    break;
                }
            }

            // A running online mode keeps going unless the start forces the lobby.
            if (mModeManager.IsOnlineGameMode() && !lpStartNetworkGameEvent->mbForceStartFreeburnLobby)
            {
                break;
            }

            mpTrainingManager->ForceUnpause(lpActionQueue);

            if (lpStartNetworkGameEvent->mbIsStartingGameAfterPlayerJoin)
            {
                if (mCarSelectManager.IsInJunkyard())
                {
                    mCarSelectManager.ForceExitJunkyard(lpActionQueue, false);
                }

                GameStateModuleIO::StartGameThroughPlayerJoinAction lStartGameThroughPlayerJoinAction;
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStartGameThroughPlayerJoinAction),
                                        GameStateModuleIO::E_ACTION_START_GAME_THROUGH_PLAYER_JOIN,
                                        sizeof(lStartGameThroughPlayerJoinAction));
            }
        }

        muNetworkGameRandomSeed = lpStartNetworkGameEvent->muRandomSeedForGame;
        mNetworkRoundManager.NetworkGameStarted(lpStartNetworkGameEvent);
        GetOnlineFlybyManager()->SetRandomNetworkGameSeed(lpStartNetworkGameEvent->muRandomSeedForGame);
        break;
    }

    case GameStateModuleIO::E_EVENT_START_NETWORK_ROUND:
    {
        StartGameModeParams lStartGameModeParams;

        // Every gate reads the game event the NetworkRoundManager latched, not this one.
        const GameStateModuleIO::StartNetworkGameEvent* lpNetworkGameEvent =
            mNetworkRoundManager.GetNetworkGameEvent();

        if (GameStateModuleIO::IsOnlineFreeBurnLobby(lpNetworkGameEvent->meGameMode) &&
            GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()) &&
            !lpNetworkGameEvent->mbForceStartFreeburnLobby)
        {
            break;
        }
        if (GameStateModuleIO::IsOnlineFreeBurnLobby(lpNetworkGameEvent->meGameMode) &&
            mNetworkRoundManager.GetTotalRounds() - 1 == mNetworkRoundManager.GetCurrentRound())
        {
            break;
        }

        mModeManager.CancelFreeburnChallenge(lpActionQueue);

        const GameStateModuleIO::StartNetworkRoundEvent* lpStartNetworkRoundEvent =
            reinterpret_cast<const GameStateModuleIO::StartNetworkRoundEvent*>(lpEvent);
        CGS_ASSERT(lpStartNetworkRoundEvent, "lpStartNetworkRoundEvent");
        mNetworkRoundManager.NetworkRoundStarted(lpStartNetworkRoundEvent);

        GameStateModuleIO::SetInModeStartRegionAction lStartRegionAction;
        lStartRegionAction.mbInStartRegion = 0;
        mModeManager.ClearModeStartRegion();
        lStartRegionAction.mu16StartLocationId = KU_INVALID_LANDMARK_INDEX;
        // The console leaves +0x03 unwritten; zeroed for a deterministic queue image, as
        // ModeManager::StartModeIntro does for the same record.
        lStartRegionAction.maPad03[0] = 0;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStartRegionAction),
                                GameStateModuleIO::E_ACTION_SET_IN_MODE_START_REGION,
                                sizeof(lStartRegionAction));

        mpMugshotManager->OnRoundStart();
        mpPaybackManager->OnRoundStart();
        mNetworkRoundManager.OnRoundStart();
        mpTakedownManager->ClearAllTakedowns(lpActionQueue);

        lStartGameModeParams.Construct(lpNetworkGameEvent->meGameMode,
                                       mLastActiveRaceCarInterface.GetPlayerPosition(),
                                       E_GAMEMODESTARTMECHANISM_DEFAULT);
        mModeManager.StartGameMode(lpOutput, &lStartGameModeParams);
        mNetworkRoundManager.PreparedForMode();

        if (!GameStateModuleIO::IsOnlineFreeBurnLobby(lpNetworkGameEvent->meGameMode))
        {
            SetInActiveGameModeState();
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_ACCEPTED_MODE:
    {
        const GameStateModuleIO::PlayerAcceptedModeEvent* lpAcceptedModeEvent =
            reinterpret_cast<const GameStateModuleIO::PlayerAcceptedModeEvent*>(lpEvent);

        // Only when no mode is running. The start is always an offline race through the default
        // mechanism; the event supplies only the checkpoints.
        if (mModeManager.GetCurrentGameMode() == 0)
        {
            StartGameModeParams lStartGameModeParams;
            lStartGameModeParams.Construct(GameStateModuleIO::E_MODE_OFFLINE_RACE,
                                           mLastActiveRaceCarInterface.GetPlayerPosition(),
                                           E_GAMEMODESTARTMECHANISM_DEFAULT);

            for (s32 liLandmarkIndex = 0; liLandmarkIndex < lpAcceptedModeEvent->muNumLandmarks; ++liLandmarkIndex)
            {
                lStartGameModeParams.AddCheckpoint(lpAcceptedModeEvent->maLandmarkIndices[liLandmarkIndex],
                                                   lpAcceptedModeEvent->mauLandmarkSectionIds[liLandmarkIndex]);
            }

            mModeManager.StartGameMode(lpOutput, &lStartGameModeParams);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_PLAYER_ADDED:
    {
        const GameStateModuleIO::OnlinePlayerAddedEvent* lpPlayerAddedEvent =
            reinterpret_cast<const GameStateModuleIO::OnlinePlayerAddedEvent*>(lpEvent);
        CGS_ASSERT(lpPlayerAddedEvent, "lpPlayerAddedEvent");

        if (GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()))
        {
            ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
            if (lpScoringSystem->GetCarData(lpPlayerAddedEvent->mNetworkPlayerID) == 0)
            {
                GameStateModuleIO::OnlinePlayerAddedAction lPlayerAddedAction;
                lPlayerAddedAction.SetPlayerScoringIndex(
                    lpScoringSystem->AddPlayer(lpPlayerAddedEvent->mNetworkPlayerID, lpPlayerAddedEvent->meTeam));
                lPlayerAddedAction.mModelID                = lpPlayerAddedEvent->mModelID;
                lPlayerAddedAction.mfBaseDeformationAmount = lpPlayerAddedEvent->mf18;
                lPlayerAddedAction.mWheelID                = lpPlayerAddedEvent->mWheelID;
                lPlayerAddedAction.meTeam                  = lpPlayerAddedEvent->meTeam;
                lPlayerAddedAction.mAddedPlayerNetworkID   = lpPlayerAddedEvent->mNetworkPlayerID;
                lPlayerAddedAction.mu16CarColourIndex      = lpPlayerAddedEvent->mu16CarColourIndex;
                lPlayerAddedAction.mu16CarPaintFinishIndex = lpPlayerAddedEvent->mu16CarPaintFinishIndex;
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPlayerAddedAction),
                                        GameStateModuleIO::E_ACTION_ONLINE_PLAYER_ADDED,
                                        sizeof(lPlayerAddedAction));

                char lacCarName[KI_CGSID_STRING_LEN];
                CgsIDUnCompress(lpPlayerAddedEvent->mModelID, lacCarName);
                if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                {
                    *CgsDev::Log::gpDebugPrint << "NWCC: Added network car driving " << lpPlayerAddedEvent->mModelID
                                               << "(" << lacCarName << ")\n";
                }
            }

            if (!lpPlayerAddedEvent->mbIsLocalPlayer)
            {
                mModeManager.NetworkPlayerAdded(lpPlayerAddedEvent->mNetworkPlayerID, lpActionQueue,
                                                lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost());
            }
            else if (!lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost())
            {
                // The local player joined somebody else's lobby: the first join tip not seen yet.
                BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
                if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE))
                {
                    mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE);
                }
                else if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE_2))
                {
                    mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE_2);
                }
                else if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE_3))
                {
                    mpTrainingManager->RequestTraining(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE_3);
                }
            }
        }

        mModeManager.GetScoringSystem()->AddPlayerBurnoutSkillz(lpPlayerAddedEvent->mNetworkPlayerID,
                                                                mLocalPlayerNetworkID);
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_PLAYER_FINALISED:
    {
        const GameStateModuleIO::OnlinePlayerFinalisedEvent* lpPlayerFinalisedEvent =
            reinterpret_cast<const GameStateModuleIO::OnlinePlayerFinalisedEvent*>(lpEvent);
        CGS_ASSERT(lpPlayerFinalisedEvent, "lpPlayerFinalisedEvent");

        if (GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()) &&
            lpPlayerFinalisedEvent->mNetworkPlayerID != mLocalPlayerNetworkID)
        {
            mModeManager.NetworkPlayerFinalised(lpPlayerFinalisedEvent->mNetworkPlayerID, lpActionQueue,
                                                lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost());
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_PLAYER_REMOVED:
    {
        const GameStateModuleIO::OnlinePlayerRemovedEvent* lpPlayerRemovedEvent =
            reinterpret_cast<const GameStateModuleIO::OnlinePlayerRemovedEvent*>(lpEvent);
        CGS_ASSERT(lpPlayerRemovedEvent, "lpPlayerRemovedEvent");

        mModeManager.NetworkPlayerRemoved(lpPlayerRemovedEvent->mNetworkPlayerID, lpActionQueue,
                                          lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost());
        ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
        lpScoringSystem->ClearPlayersBurnoutSkillzData(lpPlayerRemovedEvent->mNetworkPlayerID);

        if (GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()))
        {
            const CarData* lpCarData = lpScoringSystem->GetCarData(lpPlayerRemovedEvent->mNetworkPlayerID);
            if (lpCarData != 0 && lpCarData->GetActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID)
            {
                GameStateModuleIO::OnlinePlayerRemovedAction lPlayerRemovedAction;
                lPlayerRemovedAction.SetActiveRaceCarIndex(lpCarData->GetActiveRaceCarIndex());
                lPlayerRemovedAction.mbIsLocalPlayerInGame = lpPlayerRemovedEvent->mbIsLocalPlayerInGame;
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPlayerRemovedAction),
                                        GameStateModuleIO::E_ACTION_ONLINE_PLAYER_REMOVED,
                                        sizeof(lPlayerRemovedAction));
            }
            lpScoringSystem->RemovePlayer(lpPlayerRemovedEvent->mNetworkPlayerID);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_REMOTE_PLAYER_DISCONNECTED:
    {
        const GameStateModuleIO::RemotePlayerDisconnectedEvent* lpPlayerDisconnectedEvent =
            reinterpret_cast<const GameStateModuleIO::RemotePlayerDisconnectedEvent*>(lpEvent);
        CGS_ASSERT(lpPlayerDisconnectedEvent, "lpEvent");
        CGS_ASSERT(lpPlayerDisconnectedEvent->mNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lpPlayerDisconnectedEvent->GetNetworkPlayerID() != CgsNetwork::K_INVALID_PLAYER_ID");

        const ::EActiveRaceCarIndex leActiveRaceCarIndex =
            GetActiveRaceCarIndex(lpPlayerDisconnectedEvent->mNetworkPlayerID);
        if (leActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            GameStateModuleIO::RemotePlayerDisconnectedAction lPlayerDisconnectedAction;
            lPlayerDisconnectedAction.SetNetworkPlayerID(lpPlayerDisconnectedEvent->mNetworkPlayerID);
            lPlayerDisconnectedAction.SetActiveRaceCarIndex(leActiveRaceCarIndex);
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPlayerDisconnectedAction),
                                    GameStateModuleIO::E_ACTION_REMOTE_PLAYER_DISCONNECTED,
                                    sizeof(lPlayerDisconnectedAction));
        }

        mModeManager.NetworkPlayerRemoved(lpPlayerDisconnectedEvent->mNetworkPlayerID, lpActionQueue,
                                          lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost());
        mModeManager.SetPlayerDisconnected(lpPlayerDisconnectedEvent->mNetworkPlayerID,
                                           &mLastActiveRaceCarInterface, lpActionQueue);
        mModeManager.GetScoringSystem()->ClearPlayersBurnoutSkillzData(lpPlayerDisconnectedEvent->mNetworkPlayerID);
        break;
    }

    case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_CONNECTED:
    {
        const GameStateModuleIO::LocalPlayerConnectedEvent* lpPlayerConnectedEvent =
            reinterpret_cast<const GameStateModuleIO::LocalPlayerConnectedEvent*>(lpEvent);
        CGS_ASSERT(lpPlayerConnectedEvent, "lpPlayerConnectedEvent");

        mLocalPlayerNetworkID = lpPlayerConnectedEvent->mNetworkPlayerID;
        CGS_ASSERT(mLocalPlayerNetworkID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "mLocalPlayerNetworkID != CgsNetwork::K_INVALID_PLAYER_ID");
        break;
    }

    case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_DISCONNECTED:
    {
        mModeManager.SetAbortedDueToDisconnect();

        if (GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()))
        {
            if (mOnlineCarSelectManager.IsInOnlineCarSelect())
            {
                mOnlineCarSelectManager.ExitOnlineCarSelect(lpActionQueue);

                // Back into the free-burn car (the default car when the manager holds none).
                GameStateModuleIO::ChangePlayerCarEvent lChangePlayerCarEvent;
                lChangePlayerCarEvent.mbResetPlayerCamera = false;
                CgsID lFreeburnCarId = mOnlineCarSelectManager.GetFreeburnCarId();
                if (lFreeburnCarId == 0)
                {
                    lFreeburnCarId = CgsIDCompress("PUSMC01");
                }
                lChangePlayerCarEvent.mCarModelId        = lFreeburnCarId;
                lChangePlayerCarEvent.mWheelModelId      = 0;
                lChangePlayerCarEvent.mbKeepResetSection = true;
                HandleChangePlayerCarEvent(&lChangePlayerCarEvent, lpActionQueue);
            }

            mModeManager.UserCancelCurrentMode();
            OnPlayerCarChange(mActivePlayerCarId, mActivePlayerWheelId, lpActionQueue, true);
        }

        if (mModeManager.IsOnlineGameMode() && mbWaitingForStreaming)
        {
            mModeManager.UserCancelCurrentMode();
        }

        GameStateModuleIO::NotifyDirectorLocalPlayerDisconnectedAction lNotifyDirectorLocalPlayerDisconnectedAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNotifyDirectorLocalPlayerDisconnectedAction),
                                GameStateModuleIO::E_ACTION_NOTIFY_DIRECTOR_LOCAL_PLAYER_DISCONNECTED,
                                sizeof(lNotifyDirectorLocalPlayerDisconnectedAction));

        mModeManager.LocalPlayerDisconnected(lpActionQueue);
        mLocalPlayerNetworkID = CgsNetwork::K_INVALID_PLAYER_ID;

        CGS_ASSERT(lpOutput, "lpOutput");
        CGS_ASSERT(lpOutput->GetFrameRateTypeRequestInterface(), "lpOutput->GetFrameRateTypeRequestInterface()");
        {
            // FrameRateTypeRequestInterface::RequestFrameRateTypeChange: the request byte (+4),
            // then the type word (+0). The output buffer still carries this interface as its
            // 12-byte storage, so the two fields are written through it as ModeManager does.
            GameStateModuleIO::OutputBufferFrameRateTypeReqInterface* lpFrameRateTypeRequest =
                lpOutput->GetFrameRateTypeRequestInterface();
            const s32 liRequestedFrameRateType = static_cast<s32>(CgsSystem::E_FRAMERATEMANAGER_MULTIPLE_CAPPED);
            lpFrameRateTypeRequest->maOpaque[4] = 1;
            std::memcpy(&lpFrameRateTypeRequest->maOpaque[0], &liRequestedFrameRateType, sizeof(s32));
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_LEFT_LOBBY:
    {
        GetRichPresenceManager()->LocalPlayerLeftLobby();

        if (GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()))
        {
            mModeManager.UserCancelCurrentMode();
        }

        mProgressionManager.RequestUpdateRivals();
        mRoadRulesManager.SetRoadRulesMode(lpOutput, false);
        mModeManager.CancelFreeburnChallenge(lpActionQueue);
        OnPlayerCarChange(mActivePlayerCarId, mActivePlayerWheelId, lpActionQueue, true);
        mModeManager.GetScoringSystem()->ClearAllBurnoutSkillzData();
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_GAME_PARAMS_CHANGED:
    {
        const GameStateModuleIO::OnlineGameParamsChanged* lpGameParamEvent =
            reinterpret_cast<const GameStateModuleIO::OnlineGameParamsChanged*>(lpEvent);
        CGS_ASSERT(lpGameParamEvent, "lpGameParamEvent");

        GetRichPresenceManager()->GameParametersChanged(lpGameParamEvent->meGameMode, lpGameParamEvent->mbIsRanked);
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_NEW_BURNOUT_SKILLZ:
    {
        const GameStateModuleIO::NewRemoteBurnoutSkillzEvent* lpBurnoutSkillzEvent =
            reinterpret_cast<const GameStateModuleIO::NewRemoteBurnoutSkillzEvent*>(lpEvent);
        CGS_ASSERT(lpBurnoutSkillzEvent, "lpBurnoutSkillzEvent");

        mModeManager.GetScoringSystem()->SetBurnoutSkillzData(lpBurnoutSkillzEvent->mPlayerID,
                                                              &lpBurnoutSkillzEvent->mNewSkillzData);
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_NEW_HOST:
    {
        const GameStateModuleIO::OnlineNewHostEvent* lpHostEvent =
            reinterpret_cast<const GameStateModuleIO::OnlineNewHostEvent*>(lpEvent);
        CGS_ASSERT(lpHostEvent, "lpHostEvent");

        mModeManager.HandleNewHostEvent(lpHostEvent, &mLastActiveRaceCarInterface, lpOutput);
        break;
    }
    }
}
}
