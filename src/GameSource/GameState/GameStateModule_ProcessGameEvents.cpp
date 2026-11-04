// ARTIST 0x823A0A18. Each handler group owns the typed payloads for its cases.
#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"

namespace BrnGameState
{
void GameStateModule::ProcessGameEvents(const GameStateModuleIO::GameEventQueue* lpGameEventQueue,
                                        GameStateModuleIO::GameActionQueue* lpActionQueue,
                                        const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                        GameStateModuleIO::OutputBuffer* lpOutput)
{
    using namespace GameStateModuleIO;

    CgsDev::PerfMonCpu::StartMonitor(miProcessEventsPM);
    mTimerStatusInterface = *lpPreWorldInput->GetTimerStatusInterface();

    const CgsModule::Event* lpEvent;
    s32 liEventSize;
    s32 liEventType = lpGameEventQueue->GetFirstEvent(&lpEvent, &liEventSize);
    while (lpEvent)
    {
        switch (liEventType)
        {
        case E_EVENT_SETUP_PLAYER_CAR:
        case E_EVENT_TELEPORT_PLAYER_CAR:
        case E_EVENT_CHANGE_PLAYER_CAR:
        case E_EVENT_SELECT_PLAYER_CAR:
        case E_EVENT_CHANGE_PLAYER_CAR_COLOUR:
        case E_EVENT_PLAYER_CAR_COLOUR_REQUEST:
        case E_EVENT_CHANGE_NETWORK_CAR:
        case E_EVENT_GAME_START:
        case E_EVENT_STREAMING_COMPLETE:
        case E_GUI_HAS_STARTED_GAME:
        case E_EVENT_PLAYER_INFO_REQUEST:
        case E_EVENT_UNLOCKED_LIVERY_REQUEST:
        case E_EVENT_CAR_SELECTION_REQUEST:
        case E_EVENT_CARSELECT_STATE_CHANGED:
        case E_EVENT_REQUEST_CAR_UNLOCK_EVENT:
        case E_EVENT_RESET_PROFILE_REQUEST:
            ProcessGameEvents_Group1(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case E_EVENT_START_NETWORK_GAME:
        case E_EVENT_START_NETWORK_ROUND:
        case E_EVENT_PLAYER_ACCEPTED_MODE:
        case E_EVENT_REMOTE_PLAYER_DISCONNECTED:
        case E_EVENT_LOCAL_PLAYER_CONNECTED:
        case E_EVENT_LOCAL_PLAYER_DISCONNECTED:
        case E_EVENT_LOCAL_PLAYER_LEFT_LOBBY:
        case E_EVENT_ONLINE_GAME_PARAMS_CHANGED:
        case E_EVENT_ONLINE_GAME_LAUNCHED:
        case E_EVENT_ONLINE_PLAYER_ADDED:
        case E_EVENT_ONLINE_PLAYER_FINALISED:
        case E_EVENT_ONLINE_PLAYER_REMOVED:
        case E_EVENT_ONLINE_NEW_BURNOUT_SKILLZ:
        case E_EVENT_ONLINE_NEW_HOST:
            ProcessGameEvents_Group2(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case E_EVENT_VEHICLE_IMPACT:
        case E_EVENT_PLAYER_RESET_ON_TRACK:
        case E_EVENT_RACE_CAR_DRIVING_IN_CRASH:
        case E_EVENT_PLAYER_IN_SHORT_CUT:
        case E_EVENT_RACE_CAR_NEEDS_HIDING:
        case E_EVENT_PLAYER_CAN_SKIP_CRASH:
        case E_EVENT_PLAYER_CRASH_ENDING:
        case E_EVENT_TRIGGER_CRASH_BREAKER:
        case E_EVENT_CANCEL_CRASH_BREAKER:
        case E_EVENT_PICKUP:
        case E_EVENT_VEHICLE_LEAPT:
        case E_EVENT_ENTER_NEW_ROAD:
        case E_EVENT_POWER_PARK_RESULT:
        case E_EVENT_BOOST_TIME_COMPLETE:
        case E_EVENT_NEAR_MISS_SCORED:
        case E_EVENT_NEAR_MISS:
        case E_EVENT_NEAR_MISS_CHAIN_COMPLETED:
        case E_EVENT_DRIFTING:
        case E_EVENT_SPINNING:
        case E_EVENT_IN_AIR:
        case E_EVENT_ONCOMING:
        case E_EVENT_ONCOMING_COMPLETED:
        case E_EVENT_TAILGATING:
        case E_EVENT_TRAFFIC_CHECKING:
        case E_EVENT_TRAFFIC_CHECKING_CHAIN:
        case E_EVENT_CRASH_COMBO_ITEM:
        case E_EVENT_AFTERTOUCH:
        case E_EVENT_RECORD_PROP_HIT:
        case E_EVENT_REQUEST_PROP_PROGRESSION:
        case E_EVENT_GAME_TRAINING_REQUEST:
        case E_EVENT_OVERHEAD_SIGN_HIT:
        case E_EVENT_COMPLETED_STUNT:
        case E_EVENT_INPROGRESS_STUNT:
        case E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS:
        case E_EVENT_FREEBURN_CHALLENGE_RESET:
        case E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS:
        case E_EVENT_ACTIVE_FREEBURN_CHALLENGE:
            ProcessGameEvents_Group3(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case E_EVENT_PLAYER_ENTERS_RACE_MAP:
        case E_EVENT_EVENT_STATE_REQUEST:
        case E_EVENT_GAME_STATS_REQUEST:
        case E_EVENT_RANK_INFO_REQUEST:
        case E_EVENT_CAR_UNLOCK_TICKER_CLOSED:
        case E_EVENT_LANDMARK_ROUTE_REQUEST:
        case E_EVENT_LANDMARK_RACES_REQUEST:
        case E_EVENT_LANDMARK_VARIABLE_INFO_REQUEST:
        case E_EVENT_REGION_FROM_POSITION_REQUEST:
        case E_EVENT_ROAD_RULE_DATA_REQUEST:
        case E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST:
        case E_EVENT_ROAD_RULE_BATCH_DATA_REQUEST:
        case E_EVENT_ROAD_RULE_INTERACTION_CHANGE:
        case E_EVENT_ROAD_RULE_MODE_SWITCH:
        case E_EVENT_ALL_RIVALRY_DATA_REQUEST:
        case E_EVENT_ONE_RIVALRY_DATA_REQUEST:
        case E_EVENT_GUI_SWITCHES_ROAD_RULE_STATE:
        case 104:
        case E_CAR_CONTROL_CHANGE_REQUEST:
        case E_EVENT_GUI_AWARD_SEQUENCE_START:
        case E_EVENT_GUI_AWARD_SEQUENCE_END:
        case E_EVENT_PROGRESSION_PROFILE_LOADED:
        case E_EVENT_CHECK_FOR_COMPLETION:
        case E_EVENT_CHANGE_WORLD_REGION:
        case E_EVENT_PLAYER_ROUTE_UPDATED:
        case E_EVENT_ONLINE_ROAD_RULES_PB_RECV:
        case E_EVENT_ONLINE_ROAD_RULES_UPLOADED:
        case E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED:
        case E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO:
        case E_EVENT_ONLINE_COLLECTABLE:
        case E_EVENT_ONLINE_IMAGE_RECEIVED:
        case E_EVENT_ONLINE_CAPTURE_WINNER_PHOTO_FINISH:
        case E_EVENT_ONLINE_IMAGE_SEND_ABORTED:
        case E_EVENT_BUDDY_REMOVED:
        case E_EVENT_INSTANT_FREEBURN:
        case 152:
        case 153:
        case E_EVENT_IMAGE_TO_SAVE:
        case E_EVENT_IMAGE_FILES_SAVED:
        case E_EVENT_IMAGE_GALLERY_REQUEST:
        case E_EVENT_IMAGE_GALLERY_COUNT_REQUEST:
        case E_EVENT_IMAGE_GALLERY_DATA_REQUEST:
        case E_EVENT_IMAGE_FILES_LOADED:
        case 175:
            ProcessGameEvents_Group4(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        case E_EVENT_CONTROLLER_DISCONNECTED:
        case E_EVENT_RIVAL_UPDATE_REQUESTED:
        case E_EVENT_LOADING_SCREEN_LOADED:
        case E_EVENT_ONLINE_CAR_SELECT:
        case E_EVENT_MARKED_MAN_LOADED:
        case E_EVENT_FINISHED_SYNCING_PLAYERS:
        case E_EVENT_FINISHED_SPLASH:
        case E_EVENT_FINISHED_MAP_PAN:
        case E_EVENT_GUI_FINISHED_OFFLINE_PRE_EVENT:
        case E_EVENT_RESULTS_FINISHED:
        case E_EVENT_PLAYER_EXITED_MODE:
        case E_EVENT_REQUEST_SPECIFIC_PRESET_RACES:
        case E_EVENT_PREPARE_FOR_ONLINE:
        case E_EVENT_PLAYER_FINISHED_MODE:
        case E_EVENT_PLAYER_PAUSE_STATE_CHANGED:
        case E_EVENT_PAYBACK_TRIGGERABLE:
        case E_EVENT_ENTER_REPLAY:
        case E_EVENT_LEAVE_REPLAY:
        case E_EVENT_TRAINING_PAUSE_STATE_CHANGED:
        case E_EVENT_SHOWTIME_UPDATE:
        case E_EVENT_SHOWTIME_MODE_SWITCH:
        case E_EVENT_SHOWTIME_BOUNCE_PROMPT:
        case E_EVENT_JUST_BOUNCED:
        case E_EVENT_JUST_APPLIED_EXTRA_SPIN:
        case E_EVENT_REQUEST_INVITE:
        case E_EVENT_PREPARE_FOR_INVITE:
        case E_EVENT_UPDATE_PREPARE_FOR_INVITE:
        case E_EVENT_PREPARED_FOR_INVITE:
        case E_EVENT_PERFORM_INVITE:
        case E_EVENT_INVITE_COMPLETE:
        case E_EVENT_CRASHNAV_STATE_CHANGED:
        case E_EVENT_REMOTE_PLAYER_TRIGGERED_CHECKPOINT:
        case 142:
        case 143:
        case E_EVENT_ONLINE_RIVAL_COUNT:
        case E_EVENT_LEFT_ONLINE_POST_EVENT:
        case E_EVENT_ONLINE_CAUGHT_FEVER:
        case E_EVENT_ONLINE_MUGSHOT_SENT:
        case E_EVENT_RIVAL_SHUTDOWN_DISPLAY_FINSHED:
        case 154:
        case E_EVENT_BURNING_HOME_RUN_SWITCHED_RUNNER:
        case E_EVENT_FREEBURN_CHALLENGE_SELECTED:
        case E_EVENT_FREEBURN_CHALLENGE_SELECTED_REMOTELY:
        case E_EVENT_FREEBURN_CHALLENGE_TRIGGERED_REMOTELY:
        case E_EVENT_FREEBURN_CHALLENGE_ENDED:
        case E_EVENT_TRIGGER_FREEBURN_CHALLENGE:
        case E_EVENT_REQUEST_EVERY_PLAYER_COMPLETION_STATUS:
        case E_EVENT_FREEBURN_CHALLENGE_SUCCESS_UPDATE:
        case E_EVENT_FREEBURN_CHALLENGE_SUCCESS:
        case E_EVENT_MODE_MANAGER_ROUTE_INFO:
            ProcessGameEvents_Group5(liEventType, lpEvent, lpActionQueue, lpPreWorldInput, lpOutput);
            break;

        // These five table entries go directly to GetNextEvent.
        case 11:
        case 13:
        case 63:
        case 116:
        case 148:
            break;

        default:
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::StrStream lMessage(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
            lMessage << "Unexpected game event result id:" << liEventType << "\n";
            CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 4589);
            CgsDev::Assert::EndAssert();
            break;
        }
        }
        liEventType = lpGameEventQueue->GetNextEvent(lpEvent, &lpEvent, &liEventSize);
    }

    if (mbPropSystemNeedsProgression)
    {
        const PropSmashReportAction lAction(&mProgressionManager.GetProfile()->GetHitProps());
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                E_ACTION_PROP_SMASH_PROGRESSION, sizeof(lAction));
        mbPropSystemNeedsProgression = false;
    }

    CgsDev::PerfMonCpu::StopMonitor(miProcessEventsPM);
}
}
