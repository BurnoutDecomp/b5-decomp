// GameStateModule::ProcessGameEvents_Group5 -- the mode-flow, pause, showtime, invite and online
// challenge / score arms of the game-event dispatcher (GameStateModule_ProcessGameEvents.cpp routes
// these ids here, one event per call).
//
//   28  REQUEST_SPECIFIC_PRESET_RACES  the map menu's preset races for one mode
//   22  FINISHED_SYNCING_PLAYERS       action 32, stop the mode splash
//   21  MARKED_MAN_LOADED              ModeManager::MarkedManLoaded
//   23  FINISHED_SPLASH / 24 FINISHED_MAP_PAN   advance the mode
//   16  ONLINE_CAR_SELECT              the controller belongs to car select
//   149 RIVAL_SHUTDOWN_DISPLAY_FINSHED re-arm the "all rivals shut down" check
//   26  RESULTS_FINISHED               accept the results, re-check the event-type completion
//   32  PLAYER_FINISHED_MODE / 27 PLAYER_EXITED_MODE   finish / cancel the mode, drop takedown cars
//   57..62  the invite family          invite manager requests, actions 55 / 91 / 93 / 94 / 95 / 96
//   93, 33, 43, 10, 35, 36  the pause family   RequestPause / RequestUnpause with the reason bit
//   12  RIVAL_UPDATE_REQUESTED         ask the progression manager for a rival update
//   29  PREPARE_FOR_ONLINE             OnEnterOnline
//   34  PAYBACK_TRIGGERABLE            PaybackManager
//   52, 53  JUST_BOUNCED / JUST_APPLIED_EXTRA_SPIN   actions 144 / 145
//   155 BURNING_HOME_RUN_SWITCHED_RUNNER  ModeManager
//   49, 50, 51  the showtime family    actions 142 / 143 and GUI events 397 / 398
//   163, 164, 169, 168, 162, 170, 171, 172  the freeburn challenge family (ModeManager)
//   14  LOADING_SCREEN_LOADED / 141 REMOTE_PLAYER_TRIGGERED_CHECKPOINT / 25 GUI_FINISHED_OFFLINE_PRE_EVENT
//   144, 146, 147, 145  achievements (rival count, fever, mugshot sent) and leaving the online post event
//   174 MODE_MANAGER_ROUTE_INFO / 142, 143 the network stunt score and multiplier / 154 team selection
//
// The cases are in source order. lpPreWorldInput's player-status and timer interfaces are read
// through their accessors; the dispatcher's caller holds the read lock.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                         // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // VariableEventQueue::AddEvent
#include "GameShared/GameClasses/Network/CgsNetworkConstants.h"            // CgsNetwork::K_INVALID_PLAYER_ID
#include "GameSource/BurnoutConstants.h"                                   // operator++(EActiveRaceCarIndex&, int)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                     // PreWorldInputBuffer / OutputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                            // the event records
#include "GameSource/GameState/BrnGameActions.h"                           // the posted action records
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"                            // GuiShowtimeModeSwitch (397), GuiShowtimeBouncePrompt (398)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"     // GetCrashScorer / SetPlayerTeam / ...
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h"  // CrashModeScoring
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/InviteManager/BrnGameStateInviteManager.h"  // RequestInvite / StartPrepareForInvite
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"            // IsInJunkyard
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"       // ClearRaceCarData
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"         // ProcessPaybackTriggerableEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"  // GetLocalPlayerIsHost

namespace BrnGameState
{

void GameStateModule::ProcessGameEvents_Group5(s32                                           liEventType,
                                               const CgsModule::Event*                       lpEvent,
                                               GameStateModuleIO::GameActionQueue*           lpActionQueue,
                                               const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                               GameStateModuleIO::OutputBuffer*              lpOutput)
{
    switch (liEventType)
    {
    case GameStateModuleIO::E_EVENT_REQUEST_SPECIFIC_PRESET_RACES:
    {
        const GameStateModuleIO::RequestSpecificPreSetRaces* lpRequest =
            reinterpret_cast<const GameStateModuleIO::RequestSpecificPreSetRaces*>(lpEvent);
        SendSpecificPreSetRacesModesAction(lpRequest->meRequiredGameMode, lpOutput);
        break;
    }

    case GameStateModuleIO::E_EVENT_FINISHED_SYNCING_PLAYERS:
    {
        const GameStateModuleIO::StopModeSplashAction lStopModeSplashAction = {};
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStopModeSplashAction),
                                GameStateModuleIO::E_ACTION_STOP_MODE_SPLASH,
                                static_cast<s32>(sizeof(lStopModeSplashAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_MARKED_MAN_LOADED:
        mModeManager.MarkedManLoaded(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_FINISHED_SPLASH:
        mModeManager.FinishedSplashScreen();
        break;

    case GameStateModuleIO::E_EVENT_FINISHED_MAP_PAN:
        mModeManager.FinishedMapPan();
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_CAR_SELECT:
        meControllerState = E_CONTROLLERSTATE_CAR_SELECT;
        break;

    case GameStateModuleIO::E_EVENT_RIVAL_SHUTDOWN_DISPLAY_FINSHED:
        mProgressionManager.ShowShutDownAllIfNeeded();
        break;

    case GameStateModuleIO::E_EVENT_RESULTS_FINISHED:
        mModeManager.ResultsAccept();
        mProgressionManager.SetCheckForAllEventTypeComplete();
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_FINISHED_MODE:
        mModeManager.PlayerFinishedMode(reinterpret_cast<const GameStateModuleIO::PlayerFinishedModeEvent*>(lpEvent));
        mpTakedownManager->ClearRaceCarData();
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_EXITED_MODE:
        mModeManager.UserCancelCurrentMode();
        mpTakedownManager->ClearRaceCarData();
        break;

    case GameStateModuleIO::E_EVENT_REQUEST_INVITE:
    {
        const GameStateModuleIO::RequestInviteEvent* lpRequestInviteEvent =
            reinterpret_cast<const GameStateModuleIO::RequestInviteEvent*>(lpEvent);
        CGS_ASSERT(lpRequestInviteEvent, "lpRequestInviteEvent");

        GetGameStateInviteManager()->RequestInvite(lpRequestInviteEvent->mInviteParams);

        // With no mode running and no user change needed the invite starts straight away (after an
        // unforced autosave); otherwise the player is asked first.
        if (!mModeManager.IsInGameMode() && !lpRequestInviteEvent->mInviteParams.mbIsLocalUserChangeNeeded)
        {
            GameStateModuleIO::RequestAutoSaveAction lRequestAutosaveAction;
            lRequestAutosaveAction.mbForceAutosave = false;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequestAutosaveAction),
                                    GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE,
                                    static_cast<s32>(sizeof(lRequestAutosaveAction)));
            GetGameStateInviteManager()->StartPrepareForInvite();
        }
        else
        {
            const GameStateModuleIO::PromptDoInviteAction lPromptDoInviteAction = {};
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPromptDoInviteAction),
                                    GameStateModuleIO::E_ACTION_PROMPT_DO_INVITE,
                                    static_cast<s32>(sizeof(lPromptDoInviteAction)));
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_PREPARE_FOR_INVITE:
        GetGameStateInviteManager()->StartPrepareForInvite();
        if (mModeManager.IsInGameMode())
        {
            mModeManager.UserCancelCurrentMode();
        }
        break;

    case GameStateModuleIO::E_EVENT_UPDATE_PREPARE_FOR_INVITE:
    {
        const GameStateModuleIO::UpdatePrepareForInviteAction lUpdatePrepareForInviteAction = {};
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUpdatePrepareForInviteAction),
                                GameStateModuleIO::E_ACTION_UPDATE_PREPARE_FOR_INVITE,
                                static_cast<s32>(sizeof(lUpdatePrepareForInviteAction)));

        // The game state is ready once no mode runs and the player is out of the junkyard.
        if (!mModeManager.IsInGameMode() && !mCarSelectManager.IsInJunkyard())
        {
            GameStateModuleIO::PreparedForInviteAction lPreparedForInviteAction;
            lPreparedForInviteAction.meModulePreparedForInvite = 0;   // E_MODULE_PREPARED_FOR_INVITE_GAME_STATE
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPreparedForInviteAction),
                                    GameStateModuleIO::E_ACTION_PREPARED_FOR_INVITE,
                                    static_cast<s32>(sizeof(lPreparedForInviteAction)));
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_PREPARED_FOR_INVITE:
    {
        const GameStateModuleIO::PreparedForInviteEvent* lpPreparedForInviteEvent =
            reinterpret_cast<const GameStateModuleIO::PreparedForInviteEvent*>(lpEvent);
        CGS_ASSERT(lpPreparedForInviteEvent, "lpPreparedForInviteEvent");

        GameStateModuleIO::PreparedForInviteAction lPreparedForInviteAction;
        lPreparedForInviteAction.meModulePreparedForInvite = lpPreparedForInviteEvent->meModulePreparedForInvite;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPreparedForInviteAction),
                                GameStateModuleIO::E_ACTION_PREPARED_FOR_INVITE,
                                static_cast<s32>(sizeof(lPreparedForInviteAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PERFORM_INVITE:
    {
        const GameStateModuleIO::PerformInviteEvent* lpPerformInviteEvent =
            reinterpret_cast<const GameStateModuleIO::PerformInviteEvent*>(lpEvent);
        CGS_ASSERT(lpPerformInviteEvent, "lpPerformInviteEvent");

        GameStateModuleIO::PerformInviteAction lPerformInviteAction;
        lPerformInviteAction.mInviteParams = lpPerformInviteEvent->mInviteParams;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPerformInviteAction),
                                GameStateModuleIO::E_ACTION_PERFORM_INVITE,
                                static_cast<s32>(sizeof(lPerformInviteAction)));
        mModeManager.GetScoringSystem()->ClearDisconnectedPlayers();
        break;
    }

    case GameStateModuleIO::E_EVENT_INVITE_COMPLETE:
    {
        const GameStateModuleIO::InviteCompleteEvent* lpInviteCompleteEvent =
            reinterpret_cast<const GameStateModuleIO::InviteCompleteEvent*>(lpEvent);
        CGS_ASSERT(lpInviteCompleteEvent, "lpInviteCompleteEvent");

        GameStateModuleIO::InviteCompleteAction lInviteCompleteAction;
        lInviteCompleteAction.mbSuccess = lpInviteCompleteEvent->mbSuccess;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInviteCompleteAction),
                                GameStateModuleIO::E_ACTION_INVITE_COMPLETE,
                                static_cast<s32>(sizeof(lInviteCompleteAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CRASHNAV_STATE_CHANGED:
    {
        const GameStateModuleIO::CrashNavStateChangedEvent* lpCrashNavEvent =
            reinterpret_cast<const GameStateModuleIO::CrashNavStateChangedEvent*>(lpEvent);
        if (lpCrashNavEvent->mbActivated)
        {
            RequestPause(E_PAUSE_GUI_CRASHNAV, lpActionQueue, 0, 0);
        }
        else
        {
            RequestUnpause(E_PAUSE_GUI_CRASHNAV, lpActionQueue);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_PAUSE_STATE_CHANGED:
    {
        const GameStateModuleIO::PlayerPauseStateChangedEvent* lpPlayerPauseEvent =
            reinterpret_cast<const GameStateModuleIO::PlayerPauseStateChangedEvent*>(lpEvent);
        if (lpPlayerPauseEvent->mbActivated)
        {
            RequestPause(E_PAUSE_PLAYER, lpActionQueue, lpPlayerPauseEvent->mbStalled, lpPlayerPauseEvent->mbWasStalled);
        }
        else
        {
            RequestUnpause(E_PAUSE_PLAYER, lpActionQueue);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_TRAINING_PAUSE_STATE_CHANGED:
    {
        const GameStateModuleIO::TrainingPauseStateChangedEvent* lpTrainingPauseEvent =
            reinterpret_cast<const GameStateModuleIO::TrainingPauseStateChangedEvent*>(lpEvent);
        if (lpTrainingPauseEvent->mbActivated)
        {
            RequestPause(E_PAUSE_GAME_TRAINING, lpActionQueue, 0, 0);
        }
        else
        {
            RequestUnpause(E_PAUSE_GAME_TRAINING, lpActionQueue);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_CONTROLLER_DISCONNECTED:
    {
        const GameStateModuleIO::ControllerDisconnectedEvent* lpControllerDisconnectedEvent =
            reinterpret_cast<const GameStateModuleIO::ControllerDisconnectedEvent*>(lpEvent);
        if (lpControllerDisconnectedEvent->mbDisconnected)
        {
            RequestPause(E_PAUSE_CONTROLLER_DISCONNECTED, lpActionQueue, 0, 0);
        }
        else
        {
            RequestUnpause(E_PAUSE_CONTROLLER_DISCONNECTED, lpActionQueue);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_ENTER_REPLAY:
        RequestPause(E_PAUSE_PLAYING_REPLAY, lpActionQueue, 0, 0);
        break;

    case GameStateModuleIO::E_EVENT_LEAVE_REPLAY:
        RequestUnpause(E_PAUSE_PLAYING_REPLAY, lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_RIVAL_UPDATE_REQUESTED:
        mProgressionManager.RequestUpdateRivals();
        break;

    case GameStateModuleIO::E_EVENT_PREPARE_FOR_ONLINE:
        OnEnterOnline(lpOutput, lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_PAYBACK_TRIGGERABLE:
        mpPaybackManager->ProcessPaybackTriggerableEvent();
        break;

    case GameStateModuleIO::E_EVENT_JUST_BOUNCED:
    {
        const GameStateModuleIO::JustBouncedEvent* lpBounceEvent =
            reinterpret_cast<const GameStateModuleIO::JustBouncedEvent*>(lpEvent);
        CrashModeScoring* const lpCrashScorer = mModeManager.GetScoringSystem()->GetCrashScorer();

        GameStateModuleIO::JustBouncedAction lBouncyBouncy;   // +0x24..+0x2F are never written
        lBouncyBouncy.miEventWord0           = lpBounceEvent->miBounceChain;
        lBouncyBouncy.mbFromStationary       = lpBounceEvent->mbFromStationary;
        lBouncyBouncy.mbOnCar                = lpBounceEvent->mbOnCar;
        lBouncyBouncy.mbBoostedBounce        = lpBounceEvent->mbBoostedBounce;
        lBouncyBouncy.mu8EventByte7          = lpBounceEvent->mbGoodImpact ? 1 : 0;
        lBouncyBouncy.miCurrentComboCount    = lpCrashScorer->GetCurrentComboCount();
        lBouncyBouncy.miTotalVehiclesCrashed = lpCrashScorer->GetNumCarsCrashed();
        lBouncyBouncy.miEventWord2           = static_cast<s32>(lpBounceEvent->midImpactEntityId.muValue);
        lBouncyBouncy.mContactPoint          = lpBounceEvent->mContactPoint;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lBouncyBouncy),
                                GameStateModuleIO::E_ACTION_JUST_BOUNCED,
                                static_cast<s32>(sizeof(lBouncyBouncy)));

        // The three arguments are read back from the posted record.
        EntityId lImpactEntityId;
        lImpactEntityId.muValue = static_cast<u32>(lBouncyBouncy.miEventWord2);
        lpCrashScorer->DealWithPlayerBounced(lBouncyBouncy.mbOnCar, lBouncyBouncy.mu8EventByte7 != 0, lImpactEntityId);
        break;
    }

    case GameStateModuleIO::E_EVENT_JUST_APPLIED_EXTRA_SPIN:
    {
        const GameStateModuleIO::JustAppliedExtraSpinAction lSpinAction = {};
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSpinAction),
                                GameStateModuleIO::E_ACTION_JUST_APPLIED_EXTRA_SPIN,
                                static_cast<s32>(sizeof(lSpinAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_BURNING_HOME_RUN_SWITCHED_RUNNER:
    {
        const GameStateModuleIO::BurningHomeRunSwitchRunnerEvent* lpSwitchedRunnerEvent =
            reinterpret_cast<const GameStateModuleIO::BurningHomeRunSwitchRunnerEvent*>(lpEvent);
        CGS_ASSERT(lpSwitchedRunnerEvent, "lpSwitchedRunnerEvent");
        mModeManager.HandleBurningHomeRunRunnerSwitch(lpSwitchedRunnerEvent, lpOutput);
        break;
    }

    case GameStateModuleIO::E_EVENT_SHOWTIME_UPDATE:
    {
        const GameStateModuleIO::ShowtimeUpdateEvent* lpShowtimeUpdateEvent =
            reinterpret_cast<const GameStateModuleIO::ShowtimeUpdateEvent*>(lpEvent);
        CGS_ASSERT(lpShowtimeUpdateEvent, "lpShowtimeUpdateEvent");

        // A player with no active race car is skipped.
        if (GetActiveRaceCarIndex(lpShowtimeUpdateEvent->mPlayerID) == ::E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            break;
        }
        CGS_ASSERT(GetActiveRaceCarIndex(lpShowtimeUpdateEvent->mPlayerID) != ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "GetActiveRaceCarIndex(lpShowtimeUpdateEvent->mPlayerID) != E_ACTIVE_RACE_CAR_INDEX_COUNT");

        GameStateModuleIO::ShowtimeUpdateAction lShowtimeUpdateAction;
        lShowtimeUpdateAction.mNetworkPlayerID      = lpShowtimeUpdateEvent->mPlayerID;
        lShowtimeUpdateAction.meActiveRaceCarIndex  = GetActiveRaceCarIndex(lpShowtimeUpdateEvent->mPlayerID);
        lShowtimeUpdateAction.miShowtimeScore       = lpShowtimeUpdateEvent->miShowtimeScore;
        lpOutput->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowtimeUpdateAction),
                                                 GameStateModuleIO::E_ACTION_SHOWTIME_UPDATE,
                                                 static_cast<s32>(sizeof(lShowtimeUpdateAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_SHOWTIME_MODE_SWITCH:
    {
        const GameStateModuleIO::ShowtimeModeSwitchEvent* lpShowtimeModeSwitchEvent =
            reinterpret_cast<const GameStateModuleIO::ShowtimeModeSwitchEvent*>(lpEvent);
        CGS_ASSERT(lpShowtimeModeSwitchEvent, "lpShowtimeModeSwitchEvent");

        if (GetActiveRaceCarIndex(lpShowtimeModeSwitchEvent->mPlayerID) == ::E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            break;
        }
        CGS_ASSERT(GetActiveRaceCarIndex(lpShowtimeModeSwitchEvent->mPlayerID) != ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "GetActiveRaceCarIndex(lpShowtimeModeSwitchEvent->mPlayerID) != E_ACTIVE_RACE_CAR_INDEX_COUNT");

        GameStateModuleIO::ShowtimeModeSwitchAction lShowtimeModeSwitchAction;   // +0x0D..+0x0F are never written
        lShowtimeModeSwitchAction.mNetworkPlayerID      = lpShowtimeModeSwitchEvent->mPlayerID;
        lShowtimeModeSwitchAction.meActiveRaceCarIndex  = GetActiveRaceCarIndex(lpShowtimeModeSwitchEvent->mPlayerID);
        lShowtimeModeSwitchAction.miFinalShowtimeScore  = lpShowtimeModeSwitchEvent->miFinalScore;
        lShowtimeModeSwitchAction.mbEnteringShowtime    = lpShowtimeModeSwitchEvent->mbEnteringShowtime;
        lpOutput->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowtimeModeSwitchAction),
                                                 GameStateModuleIO::E_ACTION_SHOWTIME_MODE_SWITCH,
                                                 static_cast<s32>(sizeof(lShowtimeModeSwitchAction)));

        // The GUI twin carries the same four fields, copied from the action.
        BrnGui::GuiShowtimeModeSwitch lShowtimeModeSwitch;
        lShowtimeModeSwitch.mNetworkPlayerID      = lShowtimeModeSwitchAction.mNetworkPlayerID;
        lShowtimeModeSwitch.meActiveRaceCarIndex  = lShowtimeModeSwitchAction.meActiveRaceCarIndex;
        lShowtimeModeSwitch.miFinalShowtimeScore  = lShowtimeModeSwitchAction.miFinalShowtimeScore;
        lShowtimeModeSwitch.mbEnteringShowtime    = lShowtimeModeSwitchAction.mbEnteringShowtime;
        lpOutput->GetGuiEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lShowtimeModeSwitch),
                                               lShowtimeModeSwitch.GetEventType(),
                                               static_cast<s32>(sizeof(lShowtimeModeSwitch)));
        break;
    }

    case GameStateModuleIO::E_EVENT_SHOWTIME_BOUNCE_PROMPT:
    {
        const GameStateModuleIO::ShowtimeBouncePromptEvent* lpPromptEvent =
            reinterpret_cast<const GameStateModuleIO::ShowtimeBouncePromptEvent*>(lpEvent);

        BrnGui::GuiShowtimeBouncePrompt lGuiPrompt;
        lGuiPrompt.mbPromptNeeded = lpPromptEvent->mbPromptNeeded;
        lpOutput->GetGuiEventQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lGuiPrompt),
                                               lGuiPrompt.GetEventType(),
                                               static_cast<s32>(sizeof(lGuiPrompt)));
        break;
    }

    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SELECTED_REMOTELY:
    {
        const GameStateModuleIO::FreeburnChallengeSelectedRemotelyEvent* lpSelectedEvent =
            reinterpret_cast<const GameStateModuleIO::FreeburnChallengeSelectedRemotelyEvent*>(lpEvent);
        CGS_ASSERT(lpSelectedEvent, "lpSelectedEvent");
        mModeManager.HandleRemoteStartFreeburnChallengeMessage(
            lpSelectedEvent->mChallengeID, lpActionQueue,
            lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost());
        break;
    }

    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_TRIGGERED_REMOTELY:
    {
        const GameStateModuleIO::FreeburnChallengeTriggeredRemotelyEvent* lpTriggeredEvent =
            reinterpret_cast<const GameStateModuleIO::FreeburnChallengeTriggeredRemotelyEvent*>(lpEvent);
        CGS_ASSERT(lpTriggeredEvent, "lpTriggeredEvent");
        mModeManager.HandleRemoteTriggeredFreeburnChallengeMessage(
            lpTriggeredEvent->mChallengeID, lpActionQueue,
            lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost());
        break;
    }

    case GameStateModuleIO::E_EVENT_TRIGGER_FREEBURN_CHALLENGE:
    {
        const bool lbIsHost = lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost();
        mModeManager.TriggerFreeburnChallenge(mModeManager.GetCurrentFreeburnChallengeID(), lpActionQueue, lbIsHost);
        break;
    }

    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ENDED:
    {
        const GameStateModuleIO::FreeburnChallengeEndedEvent* lpEndEvent =
            reinterpret_cast<const GameStateModuleIO::FreeburnChallengeEndedEvent*>(lpEvent);
        CGS_ASSERT(lpEndEvent, "lpEndEvent");
        mModeManager.HandleOnlineEndFreeburnChallengeMessage(
            lpActionQueue, lpEndEvent->meChallengeStatus,
            lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost());
        break;
    }

    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SELECTED:
    {
        const GameStateModuleIO::FreeburnChallengeSelectedEvent* lpChallengeSelectedEvent =
            reinterpret_cast<const GameStateModuleIO::FreeburnChallengeSelectedEvent*>(lpEvent);
        CGS_ASSERT(lpChallengeSelectedEvent, "lpChallengeSelectedEvent");

        switch (lpChallengeSelectedEvent->meAction)
        {
        case GameStateModuleIO::FreeburnChallengeSelectedEvent::E_ACTION_CHOSEN:
            mModeManager.HandleLocalStartFreeburnChallengeMessage(
                lpChallengeSelectedEvent->mChallengeID, lpActionQueue, &mLastActiveRaceCarInterface,
                lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost(), false);
            mModeManager.TriggerFreeburnChallenge(
                lpChallengeSelectedEvent->mChallengeID, lpActionQueue,
                lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost());
            break;

        case GameStateModuleIO::FreeburnChallengeSelectedEvent::E_ACTION_CANCELED:
            mModeManager.CancelFreeburnChallenge(lpActionQueue);
            break;

        case GameStateModuleIO::FreeburnChallengeSelectedEvent::E_ACTION_SHOWN:
            mbFreeburnChallengeSelectorVisible = true;
            mModeManager.HandleLocalStartFreeburnChallengeMessage(
                lpChallengeSelectedEvent->mChallengeID, lpActionQueue, &mLastActiveRaceCarInterface,
                lpPreWorldInput->GetPlayerStatusInterface()->GetLocalPlayerIsHost(), true);
            break;

        case GameStateModuleIO::FreeburnChallengeSelectedEvent::E_ACTION_HIDDEN:
            mbFreeburnChallengeSelectorVisible = false;
            break;
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_REQUEST_EVERY_PLAYER_COMPLETION_STATUS:
        mModeManager.OutputFreeburnChallengeEveryPlayerStatusEvent(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE:
    {
        const GameStateModuleIO::FburnChallengeSuccessUpdateEvent* lpSuccessUpdateEvent =
            reinterpret_cast<const GameStateModuleIO::FburnChallengeSuccessUpdateEvent*>(lpEvent);
        CGS_ASSERT(lpSuccessUpdateEvent, "lpSuccessUpdateEvent");
        mModeManager.HandleSuccessUpdateEvent(lpPreWorldInput->GetTimerStatusInterface(), lpSuccessUpdateEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SUCCESS:
    {
        const GameStateModuleIO::FburnChallengeSuccessEvent* lpChallengeSuccessEvent =
            reinterpret_cast<const GameStateModuleIO::FburnChallengeSuccessEvent*>(lpEvent);
        CGS_ASSERT(lpChallengeSuccessEvent, "lpChallengeSuccessEvent");
        mModeManager.HandleChallengeSuccessEvent(lpChallengeSuccessEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_LOADING_SCREEN_LOADED:
        mModeManager.HandleLoadingScreenLoaded(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_REMOTE_PLAYER_TRIGGERED_CHECKPOINT:
    {
        const GameStateModuleIO::RemotePlayerTriggeredCheckpoint* lpCheckpointTriggerEvent =
            reinterpret_cast<const GameStateModuleIO::RemotePlayerTriggeredCheckpoint*>(lpEvent);
        CGS_ASSERT(lpCheckpointTriggerEvent, "lpCheckpointTriggerEvent");
        CGS_ASSERT(lpCheckpointTriggerEvent->miCheckpointIndex != -1, "lpCheckpointTriggerEvent->miCheckpointIndex != -1");
        CGS_ASSERT(lpCheckpointTriggerEvent->miCheckpointIndex < GameStateModuleIO::KI_MAX_LANDMARKS_IN_MODE,
                   "lpCheckpointTriggerEvent->miCheckpointIndex < KI_MAX_LANDMARKS_IN_MODE");
        CGS_ASSERT(lpCheckpointTriggerEvent->mNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID,
                   "lpCheckpointTriggerEvent->mNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
        mModeManager.RemoteRaceCarHitsCheckpoint(lpCheckpointTriggerEvent->mNetworkPlayerID,
                                                 lpCheckpointTriggerEvent->miCheckpointIndex);
        break;
    }

    case GameStateModuleIO::E_EVENT_GUI_FINISHED_OFFLINE_PRE_EVENT:
        mModeManager.FinishOfflineModeIntro();
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_RIVAL_COUNT:
    {
        const GameStateModuleIO::OnlineRivalCount* lpRivalCountEvent =
            reinterpret_cast<const GameStateModuleIO::OnlineRivalCount*>(lpEvent);
        mAchievementManager.OnRivalAdded(lpRivalCountEvent->miRivalCount);
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_CAUGHT_FEVER:
        mAchievementManager.OnCaughtFever();
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_MUGSHOT_SENT:
        mProgressionManager.OnMugshotSent();
        break;

    case GameStateModuleIO::E_EVENT_LEFT_ONLINE_POST_EVENT:
        mModeManager.LeftOnlinePostEvent();
        break;

    case GameStateModuleIO::E_EVENT_MODE_MANAGER_ROUTE_INFO:
    {
        const GameStateModuleIO::ModeManagerRouteInfoEvent* lpRouteInfoEvent =
            reinterpret_cast<const GameStateModuleIO::ModeManagerRouteInfoEvent*>(lpEvent);
        CGS_ASSERT(lpRouteInfoEvent, "lpRouteInfoEvent");
        mModeManager.HandleCheckpointDistanceResponse(lpRouteInfoEvent);
        break;
    }

    case 142:   // a remote player's online stunt score
    {
        const GameStateModuleIO::StuntScoreUpdatedEvent* lpStuntScoreEvent =
            reinterpret_cast<const GameStateModuleIO::StuntScoreUpdatedEvent*>(lpEvent);
        mModeManager.SetNetworkStuntScore(lpStuntScoreEvent->mNetworkPlayerID, lpStuntScoreEvent->miStuntScore);
        break;
    }

    case 143:   // a remote player's stunt multiplier
    {
        const GameStateModuleIO::StuntMultiplierEvent* lpMultiplierEvent =
            reinterpret_cast<const GameStateModuleIO::StuntMultiplierEvent*>(lpEvent);
        CGS_ASSERT(lpMultiplierEvent, "lpMultiplierEvent");
        // The console hands the whole 64-bit word over; ScoringSystem::SetNetworkStuntMultiplier
        // declares that parameter as an s32.
        mModeManager.GetScoringSystem()->SetNetworkStuntMultiplier(lpMultiplierEvent->mNetworkPlayerID,
                                                                   lpMultiplierEvent->miStuntMultiplier,
                                                                   static_cast<s32>(lpMultiplierEvent->mi64MultiplierData));
        break;
    }

    case 154:   // the host's team selection
    {
        const GameStateModuleIO::TeamSelectionEvent* lpTeamSelectionEvent =
            reinterpret_cast<const GameStateModuleIO::TeamSelectionEvent*>(lpEvent);
        CGS_ASSERT(lpTeamSelectionEvent, "lpTeamSelectionEvent");

        for (::EActiveRaceCarIndex leRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
             leRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; leRaceCarIndex++)
        {
            if (lpTeamSelectionEvent->maePlayerTeam[leRaceCarIndex] != GameStateModuleIO::E_PLAYER_TEAM_NONE)
            {
                CGS_ASSERT(mModeManager.GetScoringSystem(), "mModeManager.GetScoringSystem()");
                mModeManager.GetScoringSystem()->SetPlayerTeam(leRaceCarIndex, lpTeamSelectionEvent->maePlayerTeam[leRaceCarIndex]);
            }
        }
        break;
    }
    }
}

}
