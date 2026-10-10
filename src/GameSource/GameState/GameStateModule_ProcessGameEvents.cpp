// BrnGameState::GameStateModule::ProcessGameEvents -- the module's game-event dispatcher.
//
// PreWorldUpdate hands it the frame's merged game-event queue, the output action queue, the
// pre-world input buffer and the output buffer. It walks the queue once, front to back, and answers
// every event by its id through one switch; after the walk it posts the prop-progression report the
// prop world asked for (case 112 arms it) and clears the request. The whole walk is bracketed by the
// module's "Process events" PerfMonCpu monitor.
//
// The jump table has 176 slots (ids 0..175). Ids 3, 19, 30, 56, 86, 87, 90, 91, 92, 134 and anything
// above 175 reach the default arm, which asserts with the offending id. Ids 11, 13, 63, 116 and 148
// jump straight to the next event. Every other id has its own arm; the arms are grouped by the
// managers they drive into five private helpers, ProcessGameEvents_Group1..5, one partfile each
// (GameStateModule_ProcessGameEvents_wBT_01..05.cpp). The case lists below are the groups.
//
// Right after starting the monitor the console copies the input buffer's 48-byte timer block into
// mTimerStatusInterface; the mode and scoring updates that follow the walk read the module's copy.

#include "GameSource/GameState/BrnGameStateModule.h"

#include <stdlib.h>                                                     // getenv (the action-199 witness)

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // PerfMonCpu::StartMonitor / StopMonitor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint (the action-199 witness)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // GetFirstEvent / GetNextEvent / AddEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // GameEventQueue, PreWorldInputBuffer, OutputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                          // EGameEventType
#include "GameSource/GameState/BrnGameActions.h"                         // PropSmashReportAction (action 199)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::GetHitProps

namespace BrnGameState
{

void GameStateModule::ProcessGameEvents(const GameStateModuleIO::GameEventQueue*      lpGameEventQueue,
                                        GameStateModuleIO::GameActionQueue*           lpActionQueue,
                                        const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                        GameStateModuleIO::OutputBuffer*              lpOutput)
{
    CgsDev::PerfMonCpu::StartMonitor(miProcessEventsPM);

    mTimerStatusInterface = *lpPreWorldInput->GetTimerStatusInterface();

    const CgsModule::Event* lpEvent     = 0;
    s32                     liEventSize = 0;
    s32                     liEventType = lpGameEventQueue->GetFirstEvent(&lpEvent, &liEventSize);

    while (lpEvent != 0)
    {
        switch (liEventType)
        {
        case GameStateModuleIO::E_EVENT_SETUP_PLAYER_CAR:
        case GameStateModuleIO::E_EVENT_TELEPORT_PLAYER_CAR:
        case GameStateModuleIO::E_EVENT_CHANGE_PLAYER_CAR:
        case GameStateModuleIO::E_EVENT_SELECT_PLAYER_CAR:
        case GameStateModuleIO::E_EVENT_CHANGE_PLAYER_CAR_COLOUR:
        case GameStateModuleIO::E_EVENT_PLAYER_CAR_COLOUR_REQUEST:
        case GameStateModuleIO::E_EVENT_CHANGE_NETWORK_CAR:
        case GameStateModuleIO::E_EVENT_GAME_START:
        case GameStateModuleIO::E_EVENT_STREAMING_COMPLETE:
        case GameStateModuleIO::E_GUI_HAS_STARTED_GAME:
        case GameStateModuleIO::E_EVENT_PLAYER_INFO_REQUEST:
        case GameStateModuleIO::E_EVENT_UNLOCKED_LIVERY_REQUEST:
        case GameStateModuleIO::E_EVENT_CAR_SELECTION_REQUEST:
        case GameStateModuleIO::E_EVENT_CARSELECT_STATE_CHANGED:
        case GameStateModuleIO::E_EVENT_REQUEST_CAR_UNLOCK_EVENT:
        case GameStateModuleIO::E_EVENT_RESET_PROFILE_REQUEST:
            ProcessGameEvents_Group1(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case GameStateModuleIO::E_EVENT_START_NETWORK_GAME:
        case GameStateModuleIO::E_EVENT_START_NETWORK_ROUND:
        case GameStateModuleIO::E_EVENT_PLAYER_ACCEPTED_MODE:
        case GameStateModuleIO::E_EVENT_REMOTE_PLAYER_DISCONNECTED:
        case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_CONNECTED:
        case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_DISCONNECTED:
        case GameStateModuleIO::E_EVENT_LOCAL_PLAYER_LEFT_LOBBY:
        case GameStateModuleIO::E_EVENT_ONLINE_GAME_PARAMS_CHANGED:
        case GameStateModuleIO::E_EVENT_ONLINE_GAME_LAUNCHED:
        case GameStateModuleIO::E_EVENT_ONLINE_PLAYER_ADDED:
        case GameStateModuleIO::E_EVENT_ONLINE_PLAYER_FINALISED:
        case GameStateModuleIO::E_EVENT_ONLINE_PLAYER_REMOVED:
        case GameStateModuleIO::E_EVENT_ONLINE_NEW_BURNOUT_SKILLZ:
        case GameStateModuleIO::E_EVENT_ONLINE_NEW_HOST:
            ProcessGameEvents_Group2(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case GameStateModuleIO::E_EVENT_VEHICLE_IMPACT:
        case GameStateModuleIO::E_EVENT_PLAYER_RESET_ON_TRACK:
        case GameStateModuleIO::E_EVENT_RACE_CAR_DRIVING_IN_CRASH:
        case GameStateModuleIO::E_EVENT_PLAYER_IN_SHORT_CUT:
        case GameStateModuleIO::E_EVENT_RACE_CAR_NEEDS_HIDING:
        case GameStateModuleIO::E_EVENT_PLAYER_CAN_SKIP_CRASH:
        case GameStateModuleIO::E_EVENT_PLAYER_CRASH_ENDING:
        case GameStateModuleIO::E_EVENT_TRIGGER_CRASH_BREAKER:
        case GameStateModuleIO::E_EVENT_CANCEL_CRASH_BREAKER:
        case GameStateModuleIO::E_EVENT_PICKUP:
        case GameStateModuleIO::E_EVENT_VEHICLE_LEAPT:
        case GameStateModuleIO::E_EVENT_ENTER_NEW_ROAD:
        case GameStateModuleIO::E_EVENT_POWER_PARK_RESULT:
        case GameStateModuleIO::E_EVENT_BOOST_TIME_COMPLETE:
        case GameStateModuleIO::E_EVENT_NEAR_MISS_SCORED:
        case GameStateModuleIO::E_EVENT_NEAR_MISS:
        case GameStateModuleIO::E_EVENT_NEAR_MISS_CHAIN_COMPLETED:
        case GameStateModuleIO::E_EVENT_DRIFTING:
        case GameStateModuleIO::E_EVENT_SPINNING:
        case GameStateModuleIO::E_EVENT_IN_AIR:
        case GameStateModuleIO::E_EVENT_ONCOMING:
        case GameStateModuleIO::E_EVENT_ONCOMING_COMPLETED:
        case GameStateModuleIO::E_EVENT_TAILGATING:
        case GameStateModuleIO::E_EVENT_TRAFFIC_CHECKING:
        case GameStateModuleIO::E_EVENT_TRAFFIC_CHECKING_CHAIN:
        case GameStateModuleIO::E_EVENT_CRASH_COMBO_ITEM:
        case GameStateModuleIO::E_EVENT_AFTERTOUCH:
        case GameStateModuleIO::E_EVENT_RECORD_PROP_HIT:
        case GameStateModuleIO::E_EVENT_REQUEST_PROP_PROGRESSION:
        case GameStateModuleIO::E_EVENT_GAME_TRAINING_REQUEST:
        case GameStateModuleIO::E_EVENT_OVERHEAD_SIGN_HIT:
        case GameStateModuleIO::E_EVENT_COMPLETED_STUNT:
        case GameStateModuleIO::E_EVENT_INPROGRESS_STUNT:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS:
        case GameStateModuleIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE:
            ProcessGameEvents_Group3(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case GameStateModuleIO::E_EVENT_PLAYER_ENTERS_RACE_MAP:
        case GameStateModuleIO::E_EVENT_EVENT_STATE_REQUEST:
        case GameStateModuleIO::E_EVENT_GAME_STATS_REQUEST:
        case GameStateModuleIO::E_EVENT_RANK_INFO_REQUEST:
        case GameStateModuleIO::E_EVENT_CAR_UNLOCK_TICKER_CLOSED:
        case GameStateModuleIO::E_EVENT_LANDMARK_ROUTE_REQUEST:
        case GameStateModuleIO::E_EVENT_LANDMARK_RACES_REQUEST:
        case GameStateModuleIO::E_EVENT_LANDMARK_VARIABLE_INFO_REQUEST:
        case GameStateModuleIO::E_EVENT_REGION_FROM_POSITION_REQUEST:
        case GameStateModuleIO::E_EVENT_ROAD_RULE_DATA_REQUEST:
        case GameStateModuleIO::E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST:
        case GameStateModuleIO::E_EVENT_ROAD_RULE_BATCH_DATA_REQUEST:
        case GameStateModuleIO::E_EVENT_ROAD_RULE_INTERACTION_CHANGE:
        case GameStateModuleIO::E_EVENT_ROAD_RULE_MODE_SWITCH:
        case GameStateModuleIO::E_EVENT_ALL_RIVALRY_DATA_REQUEST:
        case GameStateModuleIO::E_EVENT_ONE_RIVALRY_DATA_REQUEST:
        case GameStateModuleIO::E_EVENT_GUI_SWITCHES_ROAD_RULE_STATE:
        case 104:
        case GameStateModuleIO::E_CAR_CONTROL_CHANGE_REQUEST:
        case GameStateModuleIO::E_EVENT_GUI_AWARD_SEQUENCE_START:
        case GameStateModuleIO::E_EVENT_GUI_AWARD_SEQUENCE_END:
        case GameStateModuleIO::E_EVENT_PROGRESSION_PROFILE_LOADED:
        case GameStateModuleIO::E_EVENT_CHECK_FOR_COMPLETION:
        case GameStateModuleIO::E_EVENT_CHANGE_WORLD_REGION:
        case GameStateModuleIO::E_EVENT_PLAYER_ROUTE_UPDATED:
        case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_PB_RECV:
        case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_UPLOADED:
        case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED:
        case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO:
        case GameStateModuleIO::E_EVENT_ONLINE_COLLECTABLE:
        case GameStateModuleIO::E_EVENT_ONLINE_IMAGE_RECEIVED:
        case GameStateModuleIO::E_EVENT_ONLINE_CAPTURE_WINNER_PHOTO_FINISH:
        case GameStateModuleIO::E_EVENT_ONLINE_IMAGE_SEND_ABORTED:
        case GameStateModuleIO::E_EVENT_BUDDY_REMOVED:
        case GameStateModuleIO::E_EVENT_INSTANT_FREEBURN:
        case 152:
        case 153:
        case GameStateModuleIO::E_EVENT_IMAGE_TO_SAVE:
        case GameStateModuleIO::E_EVENT_IMAGE_FILES_SAVED:
        case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_REQUEST:
        case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_COUNT_REQUEST:
        case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_DATA_REQUEST:
        case GameStateModuleIO::E_EVENT_IMAGE_FILES_LOADED:
        case 175:
            ProcessGameEvents_Group4(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case GameStateModuleIO::E_EVENT_CONTROLLER_DISCONNECTED:
        case GameStateModuleIO::E_EVENT_RIVAL_UPDATE_REQUESTED:
        case GameStateModuleIO::E_EVENT_LOADING_SCREEN_LOADED:
        case GameStateModuleIO::E_EVENT_ONLINE_CAR_SELECT:
        case GameStateModuleIO::E_EVENT_MARKED_MAN_LOADED:
        case GameStateModuleIO::E_EVENT_FINISHED_SYNCING_PLAYERS:
        case GameStateModuleIO::E_EVENT_FINISHED_SPLASH:
        case GameStateModuleIO::E_EVENT_FINISHED_MAP_PAN:
        case GameStateModuleIO::E_EVENT_GUI_FINISHED_OFFLINE_PRE_EVENT:
        case GameStateModuleIO::E_EVENT_RESULTS_FINISHED:
        case GameStateModuleIO::E_EVENT_PLAYER_EXITED_MODE:
        case GameStateModuleIO::E_EVENT_REQUEST_SPECIFIC_PRESET_RACES:
        case GameStateModuleIO::E_EVENT_PREPARE_FOR_ONLINE:
        case GameStateModuleIO::E_EVENT_PLAYER_FINISHED_MODE:
        case GameStateModuleIO::E_EVENT_PLAYER_PAUSE_STATE_CHANGED:
        case GameStateModuleIO::E_EVENT_PAYBACK_TRIGGERABLE:
        case GameStateModuleIO::E_EVENT_ENTER_REPLAY:
        case GameStateModuleIO::E_EVENT_LEAVE_REPLAY:
        case GameStateModuleIO::E_EVENT_TRAINING_PAUSE_STATE_CHANGED:
        case GameStateModuleIO::E_EVENT_SHOWTIME_UPDATE:
        case GameStateModuleIO::E_EVENT_SHOWTIME_MODE_SWITCH:
        case GameStateModuleIO::E_EVENT_SHOWTIME_BOUNCE_PROMPT:
        case GameStateModuleIO::E_EVENT_JUST_BOUNCED:
        case GameStateModuleIO::E_EVENT_JUST_APPLIED_EXTRA_SPIN:
        case GameStateModuleIO::E_EVENT_REQUEST_INVITE:
        case GameStateModuleIO::E_EVENT_PREPARE_FOR_INVITE:
        case GameStateModuleIO::E_EVENT_UPDATE_PREPARE_FOR_INVITE:
        case GameStateModuleIO::E_EVENT_PREPARED_FOR_INVITE:
        case GameStateModuleIO::E_EVENT_PERFORM_INVITE:
        case GameStateModuleIO::E_EVENT_INVITE_COMPLETE:
        case GameStateModuleIO::E_EVENT_CRASHNAV_STATE_CHANGED:
        case GameStateModuleIO::E_EVENT_REMOTE_PLAYER_TRIGGERED_CHECKPOINT:
        case 142:
        case 143:
        case GameStateModuleIO::E_EVENT_ONLINE_RIVAL_COUNT:
        case GameStateModuleIO::E_EVENT_LEFT_ONLINE_POST_EVENT:
        case GameStateModuleIO::E_EVENT_ONLINE_CAUGHT_FEVER:
        case GameStateModuleIO::E_EVENT_ONLINE_MUGSHOT_SENT:
        case GameStateModuleIO::E_EVENT_RIVAL_SHUTDOWN_DISPLAY_FINSHED:
        case 154:
        case GameStateModuleIO::E_EVENT_BURNING_HOME_RUN_SWITCHED_RUNNER:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SELECTED:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SELECTED_REMOTELY:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_TRIGGERED_REMOTELY:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ENDED:
        case GameStateModuleIO::E_EVENT_TRIGGER_FREEBURN_CHALLENGE:
        case GameStateModuleIO::E_EVENT_REQUEST_EVERY_PLAYER_COMPLETION_STATUS:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE:
        case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_SUCCESS:
        case GameStateModuleIO::E_EVENT_MODE_MANAGER_ROUTE_INFO:
            ProcessGameEvents_Group5(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case GameStateModuleIO::E_EVENT_CONFIG_CONTROLLER:
        case GameStateModuleIO::E_EVENT_PLAYER_CAR_PLACED_ON_TRACK:
        case GameStateModuleIO::E_EVENT_VEHICLE_CRASHED:
        case GameStateModuleIO::E_EVENT_REQUEST_FREE_ROAM_TRACKER:
        case GameStateModuleIO::E_EVENT_ONLINE_CREATED_CUSTOM_ROUTE:
            break;

        default:
            // The console streams the id after the message into the assert buffer; the project
            // assert takes the fixed text.
            CGS_ASSERT(false, "Unexpected game event result id:");
            break;
        }

        const CgsModule::Event* lpCurrent = lpEvent;
        liEventType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liEventSize);
    }

    // The prop world asked for the profile's broken props (case 112): post action 199 with the
    // profile's hit-prop bits and clear the request.
    if (mbPropSystemNeedsProgression == true)
    {
        const GameStateModuleIO::PropSmashReportAction lReport(&mProgressionManager.GetProfile()->GetHitProps());
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReport),
                                GameStateModuleIO::E_ACTION_PROP_SMASH_PROGRESSION,
                                static_cast<s32>(sizeof(lReport)));
        mbPropSystemNeedsProgression = false;

        // [DIAG] harness witness, not console code: BRN_PROPPROG_DIAG=1 only, default off, capped
        // (the handshake runs once per profile load).
        static const bool sbDiag = []() {
            const char* const lpcValue = getenv("BRN_PROPPROG_DIAG");
            return lpcValue != 0 && lpcValue[0] != 0 && lpcValue[0] != '0';
        }();
        static s32 siDiagLines = 0;
        if (sbDiag && siDiagLines < 24 && CgsDev::Log::gpDebugPrint != 0)
        {
            ++siDiagLines;
            const BrnProgression::Profile::HitPropsBitArray& lrHitProps = mProgressionManager.GetProfile()->GetHitProps();
            u32 luHitProps = 0;
            for (u32 luBit = 0; luBit < 300000u; ++luBit)
            {
                if (lrHitProps.IsBitSet(luBit))
                {
                    ++luHitProps;
                }
            }
            *CgsDev::Log::gpDebugPrint << "[propprog] action 199 (prop smash progression) posted: the profile holds "
                                       << luHitProps << " hit props\n";
        }
    }

    CgsDev::PerfMonCpu::StopMonitor(miProcessEventsPM);
}

}
