// ============================================================================
// b5-decomp/src/GameSource/Game/GameBridgeGameStateToX_wN1_01.cpp
//
// BrnGame::BrnGameModule::BridgeGameStateToNetwork -- the game-state -> network bridge,
// partfile of GameBridgeGameStateToX.cpp (network wave N1).
//
// Called by DoUpdate_NetworkPostSim with the network module's post-sim INPUT buffer
// write-locked and the game-state OUTPUT buffer read-locked. It walks the game-state output's
// game-action queue once and, for every action:
//   1. forwards the actions the network module consumes verbatim (18 ids) into the post-sim
//      buffer's own game-action queue, unchanged (type, payload and size as queued);
//   2. translates the network-bound actions into network IN-events on the post-sim buffer's
//      network event queue (telemetry, freeburn challenge, showtime, payback, road rules,
//      rich presence, gamer card, image decode, offline progression, ...);
// then appends the game state's takedown output queue and its GameStateToNetworkInterface into
// the post-sim buffer.
//
// The action switch is three console jump tables (every target dumped from the image and
// checked): the forward table spans ids 0..238, the translate switch is split at 158 (ids
// 1..157 in one table, 158 tested on its own, 159..299 in the other). No two translated ids
// share a target, so no case bodies are collapsed. Game action 275 (road-rules batch query) is
// NOT handled here: it falls to the default of the 159..299 table.
//
// Every network IN-event is queued with sizeof(event) on the host. Three records carry a
// pointer and are wider on the host than on the console (RoadRulesChallengeScoresAction /
// NetworkInRoadRulesDataEvent: console 16 bytes; DXTDecodeImageAction /
// NetworkInDxtDecodeImageEvent: console 20 bytes), so they are copied by member, never by the
// console byte count.
//
// Five of the translated actions (18, 19, 20, 21, 299 -> network IN-events 56, 57, 45, 46, 55)
// are console-only records with no reference names; their records carry FLAG names in
// BrnGameActions.h / BrnNetworkInEventTypeDefs.h, the layouts are the console's.
// ============================================================================

#include "GameSource/Game/BrnGameModule.hpp"

#include "GameSource/GameState/BrnGameActions.h"                   // the GameStateModuleIO action records + EGameActionType
#include "GameSource/GameState/BrnGameStateModuleIO.h"             // GameStateModuleIO::OutputBuffer (action queue, takedown queue, network interface)
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"       // GameStateModuleIO::GameStats (action 180 payload)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // BrnGameState::GameModeParams (action 23)
#include "GameSource/Network/BrnNetworkModuleIO.h"                  // PostSimulationInputBuffer, NetworkEventQueue
#include "GameSource/Network/BrnNetworkInEventTypeDefs.h"           // the NetworkIn* event records
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"         // TelemetryData, ETelemetryHook
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // VariableEventQueue<N,16>
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                      // CgsIDConvertToString, KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Core/CgsStringUtils.h"             // CgsCore::StrCpy
#include "SharedClasses/StreetData/BrnChallengeData.h"              // BrnStreetData::ScoreType, ChallengeData

#include <cstring>                                                   // std::memcpy (the console's XMemCpy block copies)

namespace BrnGame
{
    void BrnGameModule::BridgeGameStateToNetwork(
        BrnNetwork::BrnNetworkModuleIO::PostSimulationInputBuffer* lpNetworkInput,
        const BrnGameState::GameStateModuleIO::OutputBuffer*       lpGameStateOutput)
    {
        namespace GsmIO = BrnGameState::GameStateModuleIO;
        namespace NetIO = BrnNetwork::BrnNetworkModuleIO;

        const GsmIO::GameActionQueue* lpInQueue  = lpGameStateOutput->GetGameActionQueue();
        GsmIO::GameActionQueue*       lpOutQueue = lpNetworkInput->GetGameActionQueue();
        CGS_ASSERT(lpInQueue, "lpInQueue");
        CGS_ASSERT(lpOutQueue, "lpOutQueue");

        const CgsModule::Event* lpAction = 0;
        s32 liActionSize = 0;
        s32 liActionType = lpInQueue->GetFirstEvent(&lpAction, &liActionSize);

        while (lpAction)
        {
            // ---- 1. actions the network module reads straight off its own game-action queue ----
            switch (liActionType)
            {
                case GsmIO::E_ACTION_RESET_PLAYER_CAR:
                case GsmIO::E_ACTION_PREPARE_FOR_MODE:
                case GsmIO::E_ACTION_START_MODE_INTRO:
                case GsmIO::E_ACTION_STOP_MODE_INTRO:
                case GsmIO::E_ACTION_MARKED_MAN_LOADED:
                case GsmIO::E_ACTION_START_PLAYING_MODE:
                case GsmIO::E_ACTION_FINISHED_MODE:
                case GsmIO::E_ACTION_SHOW_MODE_RESULTS:
                case GsmIO::E_ACTION_STOP_MODE:
                case GsmIO::E_ACTION_ON_STUNT_ELEMENT_COMPLETE:
                case GsmIO::E_ACTION_CAR_SELECT_ONLINE_SELECT_CAR:
                case GsmIO::E_ACTION_UPDATE_PREPARE_FOR_INVITE:
                case GsmIO::E_ACTION_PERFORM_INVITE:
                case GsmIO::E_ACTION_PLAYER_ELIMINATED:
                case GsmIO::E_ACTION_ACHIEVEMENTS_EARNED:
                case GsmIO::E_ACTION_ONLINE_GAME_RESULT:
                case GsmIO::E_ACTION_ONLINE_ROUND_RESULT:
                case GsmIO::E_ACTION_NETWORK_CAUGHT_FEVER:
                    lpOutQueue->AddEvent(lpAction, liActionType, liActionSize);
                    break;

                default:
                    break;
            }

            // ---- 2. actions translated into network IN-events ----------------------------------
            switch (liActionType)
            {
                case GsmIO::E_ACTION_GUI_UPDATE_PLAYER_CAR_ID:
                {
                    const GsmIO::GuiUpdatePlayerCarIDAction* lpCarUpdateAction =
                        reinterpret_cast<const GsmIO::GuiUpdatePlayerCarIDAction*>(lpAction);

                    NetIO::NetworkInTelemetryEvent lTeleEvent;
                    lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_EVENT_CAR_MODEL);
                    char lacCarID[KI_CGSID_STRING_LEN];
                    CgsIDConvertToString(lpCarUpdateAction->mPlayerCarID, lacCarID);
                    lTeleEvent.mEventData.AddParameter(lacCarID);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTeleEvent, lTeleEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PREPARE_FOR_MODE:
                {
                    const GsmIO::PrepareForModeAction* lpPrepAction =
                        reinterpret_cast<const GsmIO::PrepareForModeAction*>(lpAction);

                    if (lpPrepAction->IsFirstPrepareForMode())
                    {
                        const BrnGameState::GameModeParams* lpGameParams = lpPrepAction->GetGameModeParams();
                        const s32  liGameModeType = static_cast<s32>(lpGameParams->GetGameModeType());
                        const bool lbIsOnline     = lpGameParams->mbIsOnline;

                        NetIO::NetworkInTelemetryEvent lTeleEvent;
                        if (lbIsOnline)
                        {
                            lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_EVENT_STARTED_ONLINE);
                            lTeleEvent.mEventData.AddParameter(liGameModeType);
                            lTeleEvent.mEventData.AddParameter(static_cast<s32>(lpGameParams->miNumNetworkPlayers));
                        }
                        else
                        {
                            lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_EVENT_STARTED);
                            lTeleEvent.mEventData.AddParameter(liGameModeType);
                            lTeleEvent.mEventData.AddParameter(lpGameParams->mfProgressionRankAsRatio);
                        }
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTeleEvent, lTeleEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_FINISHED_MODE:
                {
                    const GsmIO::FinishedModeAction* lpModeResultsAction =
                        reinterpret_cast<const GsmIO::FinishedModeAction*>(lpAction);
                    CGS_ASSERT(lpModeResultsAction, "lpModeResultsAction");

                    NetIO::NetworkInTelemetryEvent lTeleEvent;
                    if (lpModeResultsAction->mbTimedOut)
                    {
                        lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_GAME_FINISH_TIMEOUT);
                        lTeleEvent.mEventData.AddParameter(static_cast<s32>(lpModeResultsAction->mfDistanceFromFinish));
                    }
                    else
                    {
                        lTeleEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_GAME_FINISH_TIME);
                        lTeleEvent.mEventData.AddParameter(lpModeResultsAction->mFinishTime);
                    }
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTeleEvent, lTeleEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_STOP_MODE:
                {
                    const GsmIO::StopModeAction* lpStopAction =
                        reinterpret_cast<const GsmIO::StopModeAction*>(lpAction);

                    // +0x13 is the abort-due-to-disconnects flag, +0x11 the has-aborted flag
                    // (reference member order of StopModeAction).
                    BrnNetwork::ETelemetryHook leHook;
                    if (lpStopAction->mu8Field13)
                    {
                        leHook = BrnNetwork::E_TELEMETRY_EVENT_DISCONNECT;
                    }
                    else if (lpStopAction->mu8Field11)
                    {
                        leHook = BrnNetwork::E_TELEMETRY_EVENT_QUIT;
                    }
                    else
                    {
                        leHook = BrnNetwork::E_TELEMETRY_EVENT_FINISHED;
                    }

                    NetIO::NetworkInTelemetryEvent lTeleEvent;
                    lTeleEvent.mEventData.Construct(leHook);
                    lTeleEvent.mEventData.AddParameter(static_cast<s32>(lpStopAction->meGameModeType));
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTeleEvent, lTeleEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_BODY_SHOP_DRIVE_THRU:
                {
                    NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                    lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_DRIVE_THRU_BODY_SHOP);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PAINT_SHOP_DRIVE_THRU:
                {
                    NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                    lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_DRIVE_THRU_PAINT_SHOP);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_DRIVE_THRU_JUNK_YARD:
                {
                    const GsmIO::DriveThruJunkYardAction* lpJunkYardAction =
                        reinterpret_cast<const GsmIO::DriveThruJunkYardAction*>(lpAction);
                    if (lpJunkYardAction->mbIsInJunkYard)
                    {
                        NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                        lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_DRIVE_THRU_JUNK_YARD);
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_GAS_STATION_DRIVE_THRU:
                {
                    NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                    lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_DRIVE_THRU_GAS_STATION);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_WORLD_REGION_CHANGE:
                {
                    const GsmIO::WorldRegionChangeAction* lpWorldRegionChangeAction =
                        reinterpret_cast<const GsmIO::WorldRegionChangeAction*>(lpAction);

                    NetIO::NetworkInChangeDistrictEvent lChangeDistrict;
                    lChangeDistrict.mNewWorldRegion = lpWorldRegionChangeAction->mNewWorldRegion;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lChangeDistrict, lChangeDistrict.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_RACE_CAR_REACHED_CHECKPOINT:
                {
                    const GsmIO::RaceCarReachedCheckpointAction* lpCheckpointAction =
                        reinterpret_cast<const GsmIO::RaceCarReachedCheckpointAction*>(lpAction);
                    CGS_ASSERT(lpCheckpointAction, "lpCheckpointAction");

                    if (lpCheckpointAction->mbIsLocalPlayer)
                    {
                        NetIO::NetworkInLocalPlayerReachesCheckpoint lCheckpointEvent;
                        lCheckpointEvent.miCheckpointIndex = lpCheckpointAction->miCheckPointIndex;
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lCheckpointEvent, lCheckpointEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_SHOWTIME_UPDATE:
                {
                    const GsmIO::ShowtimeUpdateAction* lpShowtimeUpdateAction =
                        reinterpret_cast<const GsmIO::ShowtimeUpdateAction*>(lpAction);
                    CGS_ASSERT(lpShowtimeUpdateAction, "lpShowtimeUpdateAction");

                    NetIO::NetworkInShowtimeUpdateEvent lNetworkShowtimeUpdateEvent;
                    lNetworkShowtimeUpdateEvent.mPlayerID       = lpShowtimeUpdateAction->mNetworkPlayerID;
                    lNetworkShowtimeUpdateEvent.miShowtimeScore = lpShowtimeUpdateAction->miShowtimeScore;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lNetworkShowtimeUpdateEvent,
                                                                     lNetworkShowtimeUpdateEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_SHOWTIME_MODE_SWITCH:
                {
                    const GsmIO::ShowtimeModeSwitchAction* lpShowtimeModeSwitchAction =
                        reinterpret_cast<const GsmIO::ShowtimeModeSwitchAction*>(lpAction);
                    CGS_ASSERT(lpShowtimeModeSwitchAction, "lpShowtimeModeSwitchAction");

                    NetIO::NetworkInShowtimeSwitchEvent lNetworkShowtimeModeSwitchEvent;
                    lNetworkShowtimeModeSwitchEvent.mPlayerID             = lpShowtimeModeSwitchAction->mNetworkPlayerID;
                    lNetworkShowtimeModeSwitchEvent.miFinalShowtimeScore = lpShowtimeModeSwitchAction->miFinalShowtimeScore;
                    lNetworkShowtimeModeSwitchEvent.mbEnteringShowtime   = lpShowtimeModeSwitchAction->mbEnteringShowtime;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lNetworkShowtimeModeSwitchEvent,
                                                                     lNetworkShowtimeModeSwitchEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_FREEBURN_CHALLENGE:
                {
                    const GsmIO::FreeburnChallengeAction* lpChallengeAction =
                        reinterpret_cast<const GsmIO::FreeburnChallengeAction*>(lpAction);
                    CGS_ASSERT(lpChallengeAction, "lpChallengeAction");

                    if (!lpChallengeAction->mbAbortingToStartNewChallenge &&
                        lpChallengeAction->meEventType != NetIO::E_CHALLENGE_EVENT_RESULTS_FINISHED)
                    {
                        // mPlayerID is left unset: the network side stamps it.
                        NetIO::NetworkInFreeburnChallengeEvent lChallengeEvent;
                        lChallengeEvent.mChallengeID                   = lpChallengeAction->mChallengeID;
                        lChallengeEvent.meEventType                    =
                            static_cast<NetIO::EChallengeEventType>(lpChallengeAction->meEventType);
                        lChallengeEvent.meChallengeStatus              = lpChallengeAction->meChallengeStatus;
                        lChallengeEvent.miActionIndex                  = lpChallengeAction->miActionIndex;
                        lChallengeEvent.miNumberOfCompletedChallenges  = lpChallengeAction->miNumChallengesComplete;
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lChallengeEvent, lChallengeEvent.GetEventType());
                    }

                    if (lpChallengeAction->meEventType == NetIO::E_CHALLENGE_EVENT_TRIGGERED)
                    {
                        NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                        lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_NETWORK_CHALLENGE_STARTED);
                        lTelemeteryEvent.mEventData.AddParameter(lpChallengeAction->mChallengeID);
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    }
                    else if (lpChallengeAction->meEventType == NetIO::E_CHALLENGE_EVENT_ENDED)
                    {
                        NetIO::NetworkInTelemetryEvent lTelemeteryEvent;
                        lTelemeteryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_NETWORK_CHALLENGE_FINISHED);
                        lTelemeteryEvent.mEventData.AddParameter(lpChallengeAction->mChallengeID);
                        lTelemeteryEvent.mEventData.AddParameter(static_cast<s32>(lpChallengeAction->meChallengeStatus));
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTelemeteryEvent, lTelemeteryEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_FREEBURN_CHALLENGE_COMPLETION_STATUS:
                {
                    const GsmIO::FburnChallengeStatusAction* lpFburnChallengeStatusAction =
                        reinterpret_cast<const GsmIO::FburnChallengeStatusAction*>(lpAction);
                    CGS_ASSERT(lpFburnChallengeStatusAction, "lpFburnChallengeStatusAction");

                    NetIO::NetworkInFburnChallengeStatusEvent lFburnChallengeStatusEvent;
                    lFburnChallengeStatusEvent.mCompletedChallenges = lpFburnChallengeStatusAction->mCompletedChallenges;
                    lFburnChallengeStatusEvent.mPlayerID            = lpFburnChallengeStatusAction->mPlayerID;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lFburnChallengeStatusEvent,
                                                                     lFburnChallengeStatusEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_FREEBURN_CHALLENGE_SUCCESS_UPDATE:
                {
                    const GsmIO::FburnChallengeSuccessUpdateAction* lpFburnChallengeSuccessUpdateAction =
                        reinterpret_cast<const GsmIO::FburnChallengeSuccessUpdateAction*>(lpAction);
                    CGS_ASSERT(lpFburnChallengeSuccessUpdateAction, "lpFburnChallengeSuccessUpdateAction");

                    NetIO::NetworkInFburnSuccessUpdateEvent lFburnChallengeSuccessUpdateEvent;
                    lFburnChallengeSuccessUpdateEvent.mChallengeSuccessUpdate =
                        lpFburnChallengeSuccessUpdateAction->mChallengeSuccessUpdate;
                    lFburnChallengeSuccessUpdateEvent.miActionIndex = lpFburnChallengeSuccessUpdateAction->miActionIndex;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lFburnChallengeSuccessUpdateEvent,
                                                                     lFburnChallengeSuccessUpdateEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_FREEBURN_CHALLENGE_SUCCESS:
                {
                    const GsmIO::FburnChallengeSuccessAction* lpFburnChallengeSuccessAction =
                        reinterpret_cast<const GsmIO::FburnChallengeSuccessAction*>(lpAction);
                    CGS_ASSERT(lpFburnChallengeSuccessAction, "lpFburnChallengeSuccessAction");

                    NetIO::NetworkInFburnChallengeSuccessEvent lFburnChallengeSuccessEvent;
                    std::memcpy(lFburnChallengeSuccessEvent.mafActionScores,
                                lpFburnChallengeSuccessAction->mafActionScores,
                                sizeof(lFburnChallengeSuccessEvent.mafActionScores));
                    std::memcpy(lFburnChallengeSuccessEvent.mabSuccessfulActions,
                                lpFburnChallengeSuccessAction->mabSuccessfulActions,
                                sizeof(lFburnChallengeSuccessEvent.mabSuccessfulActions));
                    std::memcpy(lFburnChallengeSuccessEvent.mabAccumulationThisFrame,
                                lpFburnChallengeSuccessAction->mabAccumulationThisFrame,
                                sizeof(lFburnChallengeSuccessEvent.mabAccumulationThisFrame));
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lFburnChallengeSuccessEvent,
                                                                     lFburnChallengeSuccessEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_ACTIVE_FREEBURN_CHALLENGE:
                {
                    const GsmIO::ActiveFburnChallengeAction* lpActiveFburnChallengeAction =
                        reinterpret_cast<const GsmIO::ActiveFburnChallengeAction*>(lpAction);
                    CGS_ASSERT(lpActiveFburnChallengeAction, "lpActiveFburnChallengeAction");

                    NetIO::NetworkInActiveFburnChallengeEvent lActiveFburnChallengeEvent;
                    lActiveFburnChallengeEvent.mChallengeID             = lpActiveFburnChallengeAction->mChallengeID;
                    lActiveFburnChallengeEvent.mPlayerToSendToID        = lpActiveFburnChallengeAction->mPlayerToSendToID;
                    lActiveFburnChallengeEvent.miNumPlayersInChallenge  = lpActiveFburnChallengeAction->miNumPlayersInChallenge;
                    std::memcpy(lActiveFburnChallengeEvent.maePlayersInChallengeARCI,
                                lpActiveFburnChallengeAction->maePlayersInChallengeARCI,
                                sizeof(lActiveFburnChallengeEvent.maePlayersInChallengeARCI));
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lActiveFburnChallengeEvent,
                                                                     lActiveFburnChallengeEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_SWITCH_BURNING_HOME_RUN_RUNNER:
                {
                    const GsmIO::SwitchBurningHomeRunRunnerAction* lpSwitchAction =
                        reinterpret_cast<const GsmIO::SwitchBurningHomeRunRunnerAction*>(lpAction);
                    CGS_ASSERT(lpSwitchAction, "lpSwitchAction");

                    NetIO::NetworkInSwitchBurningHomeRunRunner lNetworkSwitchEvent;
                    lNetworkSwitchEvent.mNewRunnerID = lpSwitchAction->mNewRunnerPlayerID;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lNetworkSwitchEvent, lNetworkSwitchEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_BURNOUT_SKILLZ:
                {
                    const GsmIO::BurnoutSkillzAction* lpGameStateSkillzAction =
                        reinterpret_cast<const GsmIO::BurnoutSkillzAction*>(lpAction);
                    CGS_ASSERT(lpGameStateSkillzAction, "lpGameStateSkillzAction");

                    NetIO::NetworkInBurnoutSkillzEvent lNetworkSkillzEvent;
                    lNetworkSkillzEvent.mPlayerID      = lpGameStateSkillzAction->mNetworkPlayerID;
                    lNetworkSkillzEvent.mNewSkillzData = lpGameStateSkillzAction->mBurnoutSkillzData;

                    const s32 liActionFlags = lpGameStateSkillzAction->miActionFlags;
                    if ((liActionFlags & GsmIO::BurnoutSkillzAction::KI_ACTION_TYPE_SEND_TO_SINGLE_PLAYER) ==
                        GsmIO::BurnoutSkillzAction::KI_ACTION_TYPE_SEND_TO_SINGLE_PLAYER)
                    {
                        lNetworkSkillzEvent.meEventType =
                            NetIO::NetworkInBurnoutSkillzEvent::E_EVENT_TYPE_SEND_TO_A_SPECIFIC_PLAYER;
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lNetworkSkillzEvent, lNetworkSkillzEvent.GetEventType());
                    }
                    else if ((liActionFlags & GsmIO::BurnoutSkillzAction::KI_ACTION_TYPE_SEND_TO_ALL_PLAYERS) ==
                             GsmIO::BurnoutSkillzAction::KI_ACTION_TYPE_SEND_TO_ALL_PLAYERS)
                    {
                        lNetworkSkillzEvent.meEventType =
                            NetIO::NetworkInBurnoutSkillzEvent::E_EVENT_TYPE_SEND_TO_ALL_PLAYERS;
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lNetworkSkillzEvent, lNetworkSkillzEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_GAME_STATS_RESPONSE:
                {
                    // The action's payload is the GameStats record itself (see the enumerator's
                    // note in BrnGameActions.h).
                    typedef GsmIO::GameStats GS;
                    const GS* lpGameStats = reinterpret_cast<const GS*>(lpAction);

                    NetIO::NetworkInOfflineProgression lNetworkProgressionEvent;
                    BrnNetwork::ServerGeneratedTypes::OfflineProgressionT& lrProgression =
                        lNetworkProgressionEvent.mOfflineProgression;
                    lrProgression.miTotalTime          = lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_TIME_PLAYED);
                    lrProgression.miTotalDistance      = lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_OFFLINE);
                    lrProgression.miTakedowns          = lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_TAKEDOWNS);
                    lrProgression.mi16Jumps            = static_cast<s16>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_JUMPS));
                    lrProgression.mi16Smashes          = static_cast<s16>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_SMASHES));
                    lrProgression.mi16Stunts           = static_cast<s16>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_STUNTS));
                    lrProgression.mi8CarsCollected     = static_cast<s8>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_CARS_COLLECTED));
                    lrProgression.mi8TimeRoadRulesWon  = static_cast<s8>(lpGameStats->GetRoadsRuledCount(BrnStreetData::E_SCORE_TYPE_TIME));
                    lrProgression.mi8CrashRoadRulesWon = static_cast<s8>(lpGameStats->GetRoadsRuledCount(BrnStreetData::E_SCORE_TYPE_CRASH));
                    lrProgression.mi8AchievementsEarnt = static_cast<s8>(lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_ACHIEVEMENTS));
                    CgsIDConvertToString(lpGameStats->GetValue(GS::E_ID_VALUE_TYPE_FAVOURITE_CAR), lrProgression.macFavouriteCar);
                    CgsIDConvertToString(lpGameStats->GetValue(GS::E_ID_VALUE_TYPE_FORGOTTEN_CAR), lrProgression.macForgottenCar);
                    CgsIDConvertToString(lpGameStats->GetValue(GS::E_ID_VALUE_TYPE_NEMESIS), lrProgression.macNemesis);
                    lNetworkProgressionEvent.miFreeburnChallengeSuccessCount =
                        lpGameStats->GetValue(GS::E_INT_VALUE_TYPE_FREEBURN_CHALLENGES_COMPLETE);

                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lNetworkProgressionEvent,
                                                                     lNetworkProgressionEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PAYBACK_MUGSHOT:
                {
                    const GsmIO::PaybackMugshotAction* lpPaybackMugshotAction =
                        reinterpret_cast<const GsmIO::PaybackMugshotAction*>(lpAction);
                    CGS_ASSERT(lpPaybackMugshotAction, "lpPaybackMugshotAction");

                    NetIO::NetworkInPaybackMugshotEvent lPaybackMugshotEvent;
                    lPaybackMugshotEvent.meTakedownAggressorIndex         = lpPaybackMugshotAction->meTakedownAggressorIndex;
                    lPaybackMugshotEvent.meTakedownVictimIndex            = lpPaybackMugshotAction->meTakedownVictimIndex;
                    lPaybackMugshotEvent.meMugshotResponse                = lpPaybackMugshotAction->meMugshotResponse;
                    lPaybackMugshotEvent.meMugshotType                    = lpPaybackMugshotAction->meImageType;
                    lPaybackMugshotEvent.mRoadRuleBeatenRoadID            = lpPaybackMugshotAction->mRoadRuleBeatenRoadID;
                    lPaybackMugshotEvent.mbIsTakedownAggressorLocalPlayer = lpPaybackMugshotAction->mbIsTakedownAggressorLocalPlayer;
                    lPaybackMugshotEvent.mbMugshotRequiresBroadcast       = lpPaybackMugshotAction->mbMugshotRequiresBroadcast;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lPaybackMugshotEvent, lPaybackMugshotEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_ABORT_MUGSHOT_CAPTURE:
                {
                    // An empty signal event; its single byte is never written.
                    NetIO::NetworkInAbortMugshotCaptureEvent lAbortCaptureEvent;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lAbortCaptureEvent, lAbortCaptureEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PAYBACK_ACTIVATED:
                {
                    const GsmIO::PaybackActivatedAction* lpPaybackAction =
                        reinterpret_cast<const GsmIO::PaybackActivatedAction*>(lpAction);
                    CGS_ASSERT(lpPaybackAction, "lpPaybackAction");

                    NetIO::NetworkInPaybackIntialised lNetworkPaybackEvent;
                    lNetworkPaybackEvent.mePaybackAggressorIndex = lpPaybackAction->mePaybackAggressorIndex;
                    lNetworkPaybackEvent.mePaybackVictimIndex    = lpPaybackAction->mePaybackVictimIndex;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lNetworkPaybackEvent, lNetworkPaybackEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_PAYBACK_SUCCEEDED:
                {
                    CGS_ASSERT(lpAction, "lpAction");
                    const GsmIO::PaybackSucceededAction* lpPaybackAction =
                        reinterpret_cast<const GsmIO::PaybackSucceededAction*>(lpAction);

                    NetIO::NetworkInPaybackSucceeded lPaybackSucceededEvent;
                    lPaybackSucceededEvent.mePaybackAggressorIndex = lpPaybackAction->mePaybackAggressorIndex;
                    lPaybackSucceededEvent.mePaybackVictimIndex    = lpPaybackAction->mePaybackVictimIndex;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lPaybackSucceededEvent, lPaybackSucceededEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_UPDATE_RICH_PRESENCE:
                {
                    const GsmIO::UpdateRichPresence* lpRichPresenceAction =
                        reinterpret_cast<const GsmIO::UpdateRichPresence*>(lpAction);

                    NetIO::NetworkInUpdateRichPresence lEvent;
                    CgsCore::StrCpy(lEvent.macRichPresenceString,
                                    static_cast<u32>(sizeof(lEvent.macRichPresenceString)),
                                    lpRichPresenceAction->macNewPresenceData);
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lEvent, lEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_SEND_TELEMETRY:
                {
                    const GsmIO::TelemetryAction* lpTeleAction =
                        reinterpret_cast<const GsmIO::TelemetryAction*>(lpAction);

                    NetIO::NetworkInTelemetryEvent lTeleEvent;
                    lTeleEvent.mEventData = lpTeleAction->mEventData;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTeleEvent, lTeleEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_ROAD_RULES_CHALLENGE_SCORES:
                {
                    const GsmIO::RoadRulesChallengeScoresAction* lpRoadRulesScoresAction =
                        reinterpret_cast<const GsmIO::RoadRulesChallengeScoresAction*>(lpAction);

                    const u32 luTimestamp = lpRoadRulesScoresAction->GetTimeStampOfLastRoadRulesDownload();
                    NetIO::NetworkInRoadRulesDataEvent lRoadRulesScoresEvent;
                    lRoadRulesScoresEvent.Construct(lpRoadRulesScoresAction->GetRoadRulesID(),
                                                    luTimestamp,
                                                    lpRoadRulesScoresAction->GetStreetManager());
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lRoadRulesScoresEvent, lRoadRulesScoresEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_ROAD_RULES_PERSONAL_BEST:
                {
                    const GsmIO::RoadRulesPersonalBestAction* lpRoadRulesPersonalBestAction =
                        reinterpret_cast<const GsmIO::RoadRulesPersonalBestAction*>(lpAction);

                    NetIO::NetworkInRoadRulesPBEvent lRoadRulesPersonalBestEvent;
                    lRoadRulesPersonalBestEvent.mPersonalBestScore.Copy(&lpRoadRulesPersonalBestAction->mPersonalBestScore);
                    lRoadRulesPersonalBestEvent.mChallengeIndex      = lpRoadRulesPersonalBestAction->mChallengeIndex;
                    lRoadRulesPersonalBestEvent.mbLobbyPersonalBest  = lpRoadRulesPersonalBestAction->mbLobbyPersonalBest;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lRoadRulesPersonalBestEvent,
                                                                     lRoadRulesPersonalBestEvent.GetEventType());

                    // One telemetry record per score type the new personal best carries.
                    for (BrnStreetData::ScoreType leScoreType = BrnStreetData::E_SCORE_TYPE_START;
                         leScoreType < BrnStreetData::E_SCORE_TYPE_COUNT;
                         leScoreType++)
                    {
                        if (!lRoadRulesPersonalBestEvent.mPersonalBestScore.ContainsData(leScoreType))
                        {
                            continue;
                        }

                        NetIO::NetworkInTelemetryEvent lTelemetryEvent;
                        switch (leScoreType)
                        {
                            case BrnStreetData::E_SCORE_TYPE_TIME:
                                lTelemetryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_ROAD_RULES_NEW_PB_TIME);
                                break;
                            case BrnStreetData::E_SCORE_TYPE_CRASH:
                                lTelemetryEvent.mEventData.Construct(BrnNetwork::E_TELEMETRY_ROAD_RULES_NEW_PB_CRASH);
                                break;
                            default:
                                CGS_ASSERT(false, "Telemetry can't report this unknown road rules type");
                                break;
                        }
                        lTelemetryEvent.mEventData.AddParameter(static_cast<s32>(lpRoadRulesPersonalBestAction->mChallengeIndex));
                        lTelemetryEvent.mEventData.AddParameter(
                            lRoadRulesPersonalBestEvent.mPersonalBestScore.GetScore(leScoreType));
                        lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lTelemetryEvent, lTelemetryEvent.GetEventType());
                    }
                    break;
                }

                case GsmIO::E_ACTION_ROAD_RULES_OVERWRITE_SERVER_RECORD:
                {
                    // An empty signal event; its single byte is never written.
                    NetIO::NetworkInRoadRulesOverwriteServerRecord lOverwriteServerRoadRulesEvent;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lOverwriteServerRoadRulesEvent,
                                                                     lOverwriteServerRoadRulesEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_REQUEST_GAMERCARD:
                {
                    const GsmIO::RequestGamerCardAction* lpGamerCardAction =
                        reinterpret_cast<const GsmIO::RequestGamerCardAction*>(lpAction);
                    CGS_ASSERT(lpGamerCardAction, "lpGamerCardAction");

                    NetIO::NetworkInShowGamerCard lShowGamerCardEvent;
                    lShowGamerCardEvent.mPlayerName = lpGamerCardAction->mPlayerName;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lShowGamerCardEvent, lShowGamerCardEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_DXT_DECODE_IMAGE:
                {
                    const GsmIO::DXTDecodeImageAction* lpDxtDecodeImageAction =
                        reinterpret_cast<const GsmIO::DXTDecodeImageAction*>(lpAction);
                    CGS_ASSERT(lpDxtDecodeImageAction, "lpDxtDecodeImageAction");

                    NetIO::NetworkInDxtDecodeImageEvent lDxtDecodeImageEvent;
                    lDxtDecodeImageEvent.mpTextureToDecode = lpDxtDecodeImageAction->mpTextureToDecode;
                    lDxtDecodeImageEvent.mPlayerName       = lpDxtDecodeImageAction->mPlayerName;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lDxtDecodeImageEvent, lDxtDecodeImageEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_EVENT_SCORE_TO_UPLOAD:
                {
                    const GsmIO::ModeScoreLeaderboardAction* lpModeScoreLeaderboardAction =
                        reinterpret_cast<const GsmIO::ModeScoreLeaderboardAction*>(lpAction);
                    CGS_ASSERT(lpModeScoreLeaderboardAction, "lpModeScoreLeaderboardAction");

                    NetIO::NetworkInScoreLeaderboardEvent lScoreLeaderboardEvent;
                    lScoreLeaderboardEvent.mEventID       = lpModeScoreLeaderboardAction->mEventID;
                    lScoreLeaderboardEvent.meGameModeType = static_cast<s32>(lpModeScoreLeaderboardAction->meGameModeType);
                    lScoreLeaderboardEvent.miScore        = lpModeScoreLeaderboardAction->miScore;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lScoreLeaderboardEvent,
                                                                     lScoreLeaderboardEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_NON_UPLOADED_MODE_SCORES:
                {
                    const GsmIO::NonUploadedModeScoresAction* lpNonUploadedModeScoresAction =
                        reinterpret_cast<const GsmIO::NonUploadedModeScoresAction*>(lpAction);
                    CGS_ASSERT(lpNonUploadedModeScoresAction, "lpNonUploadedModeScoresAction");

                    NetIO::NetworkInNonUploadedScoresEvent lNonUploadedScoresEvent;
                    lNonUploadedScoresEvent.mpNonUploadedScores = lpNonUploadedModeScoresAction->mpNonUploadedScores;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lNonUploadedScoresEvent,
                                                                     lNonUploadedScoresEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_STUNT_SCORE_UPDATED:
                {
                    // No assert on this arm.
                    const GsmIO::StuntScoreUpdatedAction* lpStuntScoreAction =
                        reinterpret_cast<const GsmIO::StuntScoreUpdatedAction*>(lpAction);

                    NetIO::NetworkInStuntScoreUpdatedEvent lStuntScoreUpdatedEvent;
                    lStuntScoreUpdatedEvent.miStuntScore = lpStuntScoreAction->miStuntScore;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lStuntScoreUpdatedEvent,
                                                                     lStuntScoreUpdatedEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_STUNT_MULTIPLIER:
                {
                    const GsmIO::StuntMultiplierAction* lpStuntMultiplierAction =
                        reinterpret_cast<const GsmIO::StuntMultiplierAction*>(lpAction);
                    CGS_ASSERT(lpStuntMultiplierAction, "lpStuntMultiplierAction");

                    // One 8-byte copy on the console.
                    NetIO::NetworkInStuntMultiplierEvent lStuntMultiplierEvent;
                    lStuntMultiplierEvent.muStuntTypes    = lpStuntMultiplierAction->muStuntTypes;
                    lStuntMultiplierEvent.mu16FlatSpins   = lpStuntMultiplierAction->mu16FlatSpins;
                    lStuntMultiplierEvent.mu16BarrelRolls = lpStuntMultiplierAction->mu16BarrelRolls;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lStuntMultiplierEvent,
                                                                     lStuntMultiplierEvent.GetEventType());
                    break;
                }

                case GsmIO::E_ACTION_SHOW_GAMERCARD_FOR_XUID:
                {
                    const GsmIO::ShowGamerCardForXuidAction* lpGamerCardAction =
                        reinterpret_cast<const GsmIO::ShowGamerCardForXuidAction*>(lpAction);
                    CGS_ASSERT(lpGamerCardAction, "lpGamerCardAction");

                    NetIO::NetworkInShowGamerCardForXuidEvent lShowGamerCardEvent;
                    lShowGamerCardEvent.mu64Xuid = lpGamerCardAction->mu64Xuid;
                    lpNetworkInput->GetNetworkEventQueue()->AddEvent(&lShowGamerCardEvent, lShowGamerCardEvent.GetEventType());
                    break;
                }

                default:
                    break;
            }

            liActionType = lpInQueue->GetNextEvent(lpAction, &lpAction, &liActionSize);
        }

        // The output accessor hands the takedown queue out as the forward-declared
        // TakedownEventOutputQueueType; the payload behind it is the committed
        // EventQueue<BrnGameState::TakedownEvent, 8> that AppendTakedownQueue takes (the same
        // cross-home cast BridgeGameStateToWorld carries).
        lpNetworkInput->AppendTakedownQueue(
            reinterpret_cast<const NetIO::PostSimulationInputBuffer::TakedownEventQueue*>(
                lpGameStateOutput->GetTakedownEventOutputQueue()));
        lpNetworkInput->AppendGameStateToNetworkInterface(lpGameStateOutput->GetGameStateToNetworkInterface());
    }
}
