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
                CGS_ASSERT_LOCATION("d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 4589));
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

// FOLDED FROM GameStateModule_ProcessGameEvents_wBT_01.cpp
// GameStateModule::ProcessGameEvents_Group1 -- the dispatcher's player-car, car-select and
// profile-car arms (GameStateModule_ProcessGameEvents.cpp routes these sixteen ids here).
//
// Each arm is the console's: it sets the player car up or moves it, changes its colours, drives
// the junkyard (CarSelectManager) or the online car select (OnlineCarSelectManager) through the
// GUI's car-select steps, and answers the GUI's car and profile queries with game actions. The
// cases are in the console's source order.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Containers/CgsArray.h"                  // Array<CgsID,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"               // CarSelectionResponseAction bits
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsID.h"                           // CgsIDUnCompress / KI_CGSID_STRING_LEN
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // PreWorldInputBuffer, OutputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                          // the event payloads
#include "GameSource/GameState/BrnGameActions.h"                         // the posted action records
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"             // PlayerInfo
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // CarSelectManager
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"    // OnlineCarSelectManager
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // ModeManager::GetScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"   // ScoringSystem / CarData
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"      // BrnProgression::CarData
#include "GameSource/GameState/Progression/BrnProgressionLiveryData.h"   // BrnProgression::LiveryData
#include "GameSource/GameState/Progression/BrnDerivedCars.h"             // DerivedCarArray
#include "SharedClasses/DataLists/VehicleList.h"                         // VehicleList::GetVehicleFromId
#include "SharedClasses/DataLists/VehicleListEntry.h"                    // IsLiveryColour / GetParentId

namespace BrnGameState
{

void GameStateModule::ProcessGameEvents_Group1(s32 liEventType, const CgsModule::Event* lpEvent,
                                               GameStateModuleIO::GameActionQueue* lpActionQueue,
                                               const GameStateModuleIO::PreWorldInputBuffer* /*lpPreWorldInput*/,
                                               GameStateModuleIO::OutputBuffer* lpOutput)
{
    switch (liEventType)
    {
    case GameStateModuleIO::E_EVENT_SETUP_PLAYER_CAR:
    {
        const GameStateModuleIO::SetupPlayerCarEvent* lpSetupPlayerCarEvent =
            reinterpret_cast<const GameStateModuleIO::SetupPlayerCarEvent*>(lpEvent);
        CGS_ASSERT(!mCarSelectManager.IsInJunkyard(), "!mCarSelectManager.IsInJunkyard()");

        OnPlayerCarChange(lpSetupPlayerCarEvent->mCarModelId, lpSetupPlayerCarEvent->mWheelModelId,
                          lpActionQueue, true);

        GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
        lResetPlayerCarAction.mPosition            = lpSetupPlayerCarEvent->mPosition;
        lResetPlayerCarAction.mDirection           = lpSetupPlayerCarEvent->mDirection;
        lResetPlayerCarAction.mCarModelId          = lpSetupPlayerCarEvent->mCarModelId;
        lResetPlayerCarAction.mWheelModelId        = lpSetupPlayerCarEvent->mWheelModelId;
        lResetPlayerCarAction.mePlayerScoringIndex =
            FindPlayerScoringIndexForActiveRaceCar(GetPlayerActiveRaceCarIndex());
        lResetPlayerCarAction.mfDeformationAmount  =
            mProgressionManager.GetProfile()->GetPlayerBaseDeformAmount(lpSetupPlayerCarEvent->mCarModelId);
        lResetPlayerCarAction.miBaseDeformationType      = (lResetPlayerCarAction.mfDeformationAmount > 0.0f) ? 1 : -1;
        lResetPlayerCarAction.meCarSelectType            = GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP;
        lResetPlayerCarAction.mbInCarSelectScreen        = false;
        lResetPlayerCarAction.mbCarSelectDontStreamAudio = false;
        lResetPlayerCarAction.mbResetPlayerCamera        = false;
        lResetPlayerCarAction.mbKeepResetSection         = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                                GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                                static_cast<s32>(sizeof(lResetPlayerCarAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_REQUEST_CAR_UNLOCK_EVENT:
    {
        const GameStateModuleIO::RequestCarUnlockEvent* lpRequestCarUnlockEvent =
            reinterpret_cast<const GameStateModuleIO::RequestCarUnlockEvent*>(lpEvent);
        CGS_ASSERT(lpRequestCarUnlockEvent != NULL, "lpRequestCarUnlockEvent != NULL");
        mProgressionManager.AddCar(lpRequestCarUnlockEvent->mCarModelId,
                                   BrnProgression::CarData::E_UNLOCK_TYPE_SPONSOR);
        break;
    }

    case GameStateModuleIO::E_EVENT_TELEPORT_PLAYER_CAR:
    {
        const GameStateModuleIO::TeleportPlayerCarEvent* lpTeleportPlayerCarEvent =
            reinterpret_cast<const GameStateModuleIO::TeleportPlayerCarEvent*>(lpEvent);

        // Same car, new pose: no scoring slot and the no-deform sentinel. +0x38 is not written on
        // this path (as in CarSelectManager::EnterJunkyardAtStartOfGame).
        GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
        lResetPlayerCarAction.mPosition                  = lpTeleportPlayerCarEvent->mPosition;
        lResetPlayerCarAction.mDirection                 = lpTeleportPlayerCarEvent->mDirection;
        lResetPlayerCarAction.mCarModelId                = mActivePlayerCarId;
        lResetPlayerCarAction.mWheelModelId              = mActivePlayerWheelId;
        lResetPlayerCarAction.mePlayerScoringIndex       = GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
        lResetPlayerCarAction.mfDeformationAmount        = -1.0f;
        lResetPlayerCarAction.meCarSelectType            = GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP;
        lResetPlayerCarAction.mbInCarSelectScreen        = false;
        lResetPlayerCarAction.mbCarSelectDontStreamAudio = false;
        lResetPlayerCarAction.mbResetPlayerCamera        = false;
        lResetPlayerCarAction.mbKeepResetSection         = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                                GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                                static_cast<s32>(sizeof(lResetPlayerCarAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CHANGE_PLAYER_CAR:
        if (mCarSelectManager.IsInJunkyard())
        {
            mCarSelectManager.ForceExitJunkyard(lpActionQueue, false);
        }
        HandleChangePlayerCarEvent(reinterpret_cast<const GameStateModuleIO::ChangePlayerCarEvent*>(lpEvent),
                                   lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_SELECT_PLAYER_CAR:
    {
        const GameStateModuleIO::SelectPlayerCarEvent* lpSelectPlayerCarEvent =
            reinterpret_cast<const GameStateModuleIO::SelectPlayerCarEvent*>(lpEvent);
        if (mCarSelectManager.IsInJunkyard())
        {
            mCarSelectManager.RequestChangeCar(lpSelectPlayerCarEvent->mCarModelId);
        }
        else if (mOnlineCarSelectManager.IsInOnlineCarSelect())
        {
            mOnlineCarSelectManager.RequestChangeCar(lpActionQueue, lpSelectPlayerCarEvent->mCarModelId);
        }
        else
        {
            CGS_FAIL("Should be either in the junkyard or an online event car select\n");
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_CHANGE_PLAYER_CAR_COLOUR:
    {
        const GameStateModuleIO::ChangePlayerCarColourEvent* lpChangePlayerCarColourEvent =
            reinterpret_cast<const GameStateModuleIO::ChangePlayerCarColourEvent*>(lpEvent);
        CGS_ASSERT(( mCarSelectManager.IsInJunkyard() ) || ( mOnlineCarSelectManager.IsInOnlineCarSelect() ),
                   "( mCarSelectManager.IsInJunkyard() ) || ( mOnlineCarSelectManager.IsInOnlineCarSelect() )");

        BrnProgression::CarData* lpCarData = mProgressionManager.GetProfile()->FindCar(mActivePlayerCarId);

        GameStateModuleIO::CarSelectChangeColourAction lChangeColourAction;
        lChangeColourAction.muPaletteIndex = lpChangePlayerCarColourEvent->muPaletteIndex;
        lChangeColourAction.muColourIndex  = lpChangePlayerCarColourEvent->muColourIndex;
        if (lpCarData != 0)
        {
            lpCarData->SetColourIndex(static_cast<s32>(lpChangePlayerCarColourEvent->muColourIndex));
            lpCarData->SetPaletteIndex(static_cast<s32>(lpChangePlayerCarColourEvent->muPaletteIndex));
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChangeColourAction),
                                GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR,
                                static_cast<s32>(sizeof(lChangeColourAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_CAR_COLOUR_REQUEST:
    {
        const GameStateModuleIO::PlayerCarColourRequestEvent* lpPlayerCarColourRequestEvent =
            reinterpret_cast<const GameStateModuleIO::PlayerCarColourRequestEvent*>(lpEvent);

        u32 liColourIndex;
        u32 liPaletteIndex;
        mProgressionManager.GetCarColourAndPalette(lpPlayerCarColourRequestEvent->mCarId,
                                                   liColourIndex, liPaletteIndex);

        GameStateModuleIO::PlayerCarColourResponseAction lColourResponseAction;
        lColourResponseAction.muPaletteIndex = static_cast<u32>(liPaletteIndex);
        lColourResponseAction.muColourIndex  = static_cast<u32>(liColourIndex);
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lColourResponseAction),
                                GameStateModuleIO::E_ACTION_PLAYER_CAR_COLOUR_RESPONSE,
                                static_cast<s32>(sizeof(lColourResponseAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CHANGE_NETWORK_CAR:
    {
        const GameStateModuleIO::ChangeNetworkCarEvent* lpChangeNetworkCarEvent =
            reinterpret_cast<const GameStateModuleIO::ChangeNetworkCarEvent*>(lpEvent);
        ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();

        const CarData* lpScoringCarData = lpScoringSystem->GetCarData(lpChangeNetworkCarEvent->mNetworkPlayerID);
        if (lpScoringCarData == 0)
        {
            break;
        }
        const ::EActiveRaceCarIndex leActiveRaceCarIndex = lpScoringCarData->GetActiveRaceCarIndex();
        if (leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            break;
        }
        CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                   "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
        CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
                   "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface::RaceCarState* lpRaceCarState =
            mLastActiveRaceCarInterface.GetRaceCarState(leActiveRaceCarIndex);
        CGS_ASSERT(lpRaceCarState, "lpRaceCarState");

        // The car stays where the remote player's car is: the state's translation row is the
        // position, its at row the direction; the float is the payload's +0x18.
        const Vector3 lAt       = lpRaceCarState->mTransform.At();
        const Vector3 lPosition = lpRaceCarState->mTransform.Pos();

        GameStateModuleIO::SetupNetworkCarAction lSetupNetworkCarAction;
        lSetupNetworkCarAction.Construct(lpScoringSystem->GetPlayerScoringIndex(lpChangeNetworkCarEvent->mNetworkPlayerID),
                                         leActiveRaceCarIndex, lPosition, lAt,
                                         lpChangeNetworkCarEvent->mCarModelId,
                                         lpChangeNetworkCarEvent->mWheelModelId,
                                         lpChangeNetworkCarEvent->mf18);

        char lacCarName[KI_CGSID_STRING_LEN];
        CgsIDUnCompress(lpChangeNetworkCarEvent->mCarModelId, lacCarName);
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "NWCC: Network car in slot " << static_cast<s32>(leActiveRaceCarIndex)
                                       << " changed to " << static_cast<u64>(lpChangeNetworkCarEvent->mCarModelId)
                                       << "(" << lacCarName << ")\n";
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetupNetworkCarAction),
                                GameStateModuleIO::E_ACTION_SETUP_NETWORK_CAR,
                                static_cast<s32>(sizeof(lSetupNetworkCarAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_STREAMING_COMPLETE:
        ProcessStreamingCompleteEvent(reinterpret_cast<const GameStateModuleIO::StreamingCompleteEvent*>(lpEvent),
                                      lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_GAME_START:
        if (mbWaitForStreaming)
        {
            OnProfileLoaded(lpOutput, lpActionQueue);
            WaitForStreaming(lpActionQueue);
        }
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_INFO_REQUEST:
    {
        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();

        GameStateModuleIO::PlayerInfoResponseAction lPlayerInfoResponseAction;
        lPlayerInfoResponseAction.mPlayerInfo.Construct(lpProfile->GetName(), mActivePlayerCarId, 0, 0,
                                                        lpProfile->GetCarCount(), 0, 0);
        if (lpProfile->FindCar(mActivePlayerCarId) == 0)
        {
            lPlayerInfoResponseAction.mbIsCarDamaged = true;
        }
        else
        {
            lPlayerInfoResponseAction.mbIsCarDamaged =
                (lpProfile->GetPlayerBaseDeformAmount(mActivePlayerCarId) != 0.0f);
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPlayerInfoResponseAction),
                                GameStateModuleIO::E_ACTION_PLAYER_INFO_RESPONSE,
                                static_cast<s32>(sizeof(lPlayerInfoResponseAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_UNLOCKED_LIVERY_REQUEST:
    {
        const GameStateModuleIO::UnlockedLiveryRequest* lpUnlockedLiveryEvent =
            reinterpret_cast<const GameStateModuleIO::UnlockedLiveryRequest*>(lpEvent);
        CGS_ASSERT(lpUnlockedLiveryEvent, "lpUnlockedLiveryEvent");

        BrnProgression::DerivedCarArray lDerivedCars;
        lDerivedCars.ConstructColourLiveryList(mpVehicleList, lpUnlockedLiveryEvent->mCgsID);

        // Online, the family is only unlocked when the player owns the car asked about.
        if (!mOnlineCarSelectManager.IsInOnlineCarSelect() ||
            mProgressionManager.IsCarUnlocked(lpUnlockedLiveryEvent->mCgsID))
        {
            mProgressionManager.UnlockDerivedCarCollection(lDerivedCars);
        }

        // Every unlocked member, and online every base car of the family as well.
        GameStateModuleIO::UnlockedLiveryResponseAction lUnlockedLiveryResponseAction;
        lUnlockedLiveryResponseAction.maCars.Clear();
        for (u32 luCar = 0; luCar < lDerivedCars.GetLength(); ++luCar)
        {
            const CgsID lCarId = lDerivedCars.GetItem(luCar);
            if (mProgressionManager.IsCarUnlocked(lCarId) ||
                (mOnlineCarSelectManager.IsInOnlineCarSelect() &&
                 mpVehicleList->GetVehicleFromId(lCarId)->GetParentId() == 0))
            {
                lUnlockedLiveryResponseAction.maCars.Append(lCarId);
            }
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUnlockedLiveryResponseAction),
                                GameStateModuleIO::E_ACTION_UNLOCKED_LIVERY_RESPONSE,
                                static_cast<s32>(sizeof(lUnlockedLiveryResponseAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CAR_SELECTION_REQUEST:
    {
        const u32 KU_NUM_CARS = GameStateModuleIO::CarSelectionResponseAction::KU_MAX_CARS_IN_RESPONSE_ACTION;

        GameStateModuleIO::CarSelectionResponseAction lCarSelectionResponseAction;
        GetListOfPlayerSelectableVehicles(lCarSelectionResponseAction.maCars);
        lCarSelectionResponseAction.mHasBeenDrivenBitArray.UnSetAll();
        lCarSelectionResponseAction.mWreckedArray.UnSetAll();
        lCarSelectionResponseAction.miMaxAvailableCars = mProgressionManager.GetMaxCarCount();

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        const s32 liCarCount = static_cast<s32>(lCarSelectionResponseAction.maCars.GetLength());
        for (s32 liCar = 0; liCar < liCarCount; ++liCar)
        {
            const u32 luCar = static_cast<u32>(liCar);

            // Wrecked: a car the profile does not own yet, or one it owns still deformed.
            const BrnProgression::CarData* lpCarData =
                lpProfile->FindCar(lCarSelectionResponseAction.maCars.GetItem(luCar));
            if (lpCarData != 0)
            {
                if (lpCarData->GetUnlockDeformationAmount() > 0.0f)
                {
                    lCarSelectionResponseAction.mWreckedArray.SetBit(luCar);
                }
                else
                {
                    lCarSelectionResponseAction.mWreckedArray.UnSetBit(luCar);
                }
            }
            else
            {
                lCarSelectionResponseAction.mWreckedArray.SetBit(luCar);
            }

            // Driven: the livery choice of the car's base car has distance on it. A colour
            // livery is keyed on its parent.
            const BrnResource::VehicleListEntry* lpVehicleListEntry =
                mpVehicleList->GetVehicleFromId(lCarSelectionResponseAction.maCars.GetItem(luCar));
            CGS_ASSERT(lpVehicleListEntry, "lpVehicleListEntry");
            const CgsID lBaseCarId = lpVehicleListEntry->IsLiveryColour()
                                         ? lpVehicleListEntry->GetParentId()
                                         : lCarSelectionResponseAction.maCars.GetItem(luCar);
            const BrnProgression::LiveryData* lpLiveryData = lpProfile->GetChosenLiveryDataForBaseCar(lBaseCarId);
            if (lpLiveryData != 0)
            {
                if (lpLiveryData->mfDistanceDriven > 0.0f)
                {
                    lCarSelectionResponseAction.mHasBeenDrivenBitArray.SetBit(luCar);
                }
                else
                {
                    lCarSelectionResponseAction.mHasBeenDrivenBitArray.UnSetBit(luCar);
                }
            }
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectionResponseAction),
                                GameStateModuleIO::E_ACTION_CAR_SELECTION_RESPONSE,
                                static_cast<s32>(sizeof(lCarSelectionResponseAction)));

        RequestStreamingForVehicleSelection(
            lpActionQueue,
            mLastActiveRaceCarInterface.GetCarModelId(mLastActiveRaceCarInterface.GetPlayerActiveRaceCarIndex()));
        break;
    }

    case GameStateModuleIO::E_EVENT_RESET_PROFILE_REQUEST:
        mProgressionManager.GetProfile()->Construct();
        SendSetupPlayerCarEvent(lpActionQueue);
        SendSetUpAllEventStartsMessage(lpOutput);
        if (mCarSelectManager.IsInJunkyard())
        {
            mCarSelectManager.ForceExitJunkyard(lpActionQueue, false);
        }
        break;

    case GameStateModuleIO::E_EVENT_CARSELECT_STATE_CHANGED:
    {
        const GameStateModuleIO::CarSelectStateChangedEvent* lpCarSelectStateChangedEvent =
            reinterpret_cast<const GameStateModuleIO::CarSelectStateChangedEvent*>(lpEvent);

        if (lpCarSelectStateChangedEvent->meCarSelectType == GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD)
        {
            CGS_ASSERT(mCarSelectManager.IsInJunkyard(), "mCarSelectManager.IsInJunkyard()");
            switch (lpCarSelectStateChangedEvent->meState)
            {
            case GameStateModuleIO::E_CAR_SELECT_STATE_MODEL:
                mCarSelectManager.StartCarSelectState(lpActionQueue);
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_LIVERY:
                mCarSelectManager.EnterModification(lpActionQueue);
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_EXIT:
                mCarSelectManager.ExitJunkyard(lpActionQueue);
                break;
            default:
                CGS_FAIL("Unhandled enumeration");
                break;
            }
        }
        else if (lpCarSelectStateChangedEvent->meCarSelectType == GameStateModuleIO::E_CAR_SELECT_TYPE_ONLINE_EVENT_START)
        {
            CGS_ASSERT(mOnlineCarSelectManager.IsInOnlineCarSelect(), "mOnlineCarSelectManager.IsInOnlineCarSelect()");
            switch (lpCarSelectStateChangedEvent->meState)
            {
            case GameStateModuleIO::E_CAR_SELECT_STATE_MODEL:
                if (!mOnlineCarSelectManager.GetHostChoiceAndNotHost())
                {
                    mOnlineCarSelectManager.StartCarSelectState(lpActionQueue);
                }
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_LIVERY:
                CGS_ASSERT(mOnlineCarSelectManager.GetHostChoiceAndNotHost() == false,
                           "mOnlineCarSelectManager.GetHostChoiceAndNotHost() == false");
                mOnlineCarSelectManager.EnterModification(lpActionQueue);
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_WAIT_FOR_HOST:
                mOnlineCarSelectManager.EnterWaitForHost(lpActionQueue);
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_ACCEPT_CAR:
                if (!mOnlineCarSelectManager.GetHostChoiceAndNotHost())
                {
                    CGS_ASSERT(!mOnlineCarSelectManager.IsWaitingForStreaming(),
                               "!mOnlineCarSelectManager.IsWaitingForStreaming()");
                    mOnlineCarSelectManager.SubmitFinalCarSelection(lpActionQueue);
                }
                break;
            case GameStateModuleIO::E_CAR_SELECT_STATE_EXIT:
                mOnlineCarSelectManager.ExitOnlineCarSelect(lpActionQueue);
                break;
            default:
                CGS_FAIL("Unhandled enumeration");
                break;
            }
        }
        else
        {
            CGS_FAIL("Invalid Car Select Type\n");
        }
        break;
    }

    case GameStateModuleIO::E_GUI_HAS_STARTED_GAME:
        // [DIAG] harness witness, not console code: the returning-player case orders the junkyard
        // entry against it.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << "[jyentry] game event 78 (GUI has started game) received; waiting="
                                       << (mbWaitingToPutPlayerInJunkyard ? 1 : 0) << "\n";
        }
        if (mbWaitingToPutPlayerInJunkyard)
        {
            mCarSelectManager.ReallyEnterJunkyardAtStartOfGame(lpActionQueue);
            CGS_ASSERT(mCachedCarSelectChangedAction.mJunkyardId != 0,
                       "mCachedCarSelectChangedAction.mJunkyardId != kCGSID_NULL");
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&mCachedCarSelectChangedAction),
                                    GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED,
                                    static_cast<s32>(sizeof(mCachedCarSelectChangedAction)));
            mbWaitingToPutPlayerInJunkyard = false;
        }
        break;
    }
}

} // namespace BrnGameState

// FOLDED FROM GameStateModule_ProcessGameEvents_wBT_02.cpp
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

            mTrainingManager.ForceUnpause(lpActionQueue);

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
        lStartRegionAction.mbInModeStartRegion = false;
        mModeManager.ClearModeStartRegion();
        // ARTIST 0x823A225C reads this unit's K_INVALID_LANDMARK (.data 0x82CDB9B0); +0x03 is
        // padding the console never writes.
        lStartRegionAction.mCurrentLandmarkIndex = K_INVALID_LANDMARK;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStartRegionAction),
                                GameStateModuleIO::E_ACTION_SET_IN_MODE_START_REGION,
                                sizeof(lStartRegionAction));

        mMugshotManager.OnRoundStart();
        mPaybackManager.OnRoundStart();
        mNetworkRoundManager.OnRoundStart();
        mTakedownManager.ClearAllTakedowns(lpActionQueue);

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
                    mTrainingManager.RequestTraining(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE);
                }
                else if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE_2))
                {
                    mTrainingManager.RequestTraining(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE_2);
                }
                else if (!lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE_3))
                {
                    mTrainingManager.RequestTraining(BrnProgression::E_TRAINING_TYPE_JOIN_FREEBURN_ONLINE_3);
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
            lpOutput->GetFrameRateTypeRequestInterface()->RequestFrameRateTypeChange(
                CgsSystem::E_FRAMERATEMANAGER_MULTIPLE_CAPPED);
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

// FOLDED FROM GameStateModule_ProcessGameEvents_wBT_03.cpp
// BrnGameState::GameStateModule::ProcessGameEvents_Group3 -- the driving, crash and stunt arms of the
// game-event dispatcher (GameStateModule_ProcessGameEvents.cpp routes these ids here).
//
// Each case is one arm of the console's single switch, in the order the arms sit in the function:
// the world's driving events (near misses, drifts, spins, air, oncoming, tailgating, traffic checks)
// become the boost-ticker actions and feed ModeManager::ProcessEvent; the crash-mode events drive
// the crash scorer and post their crash actions; vehicle impacts reach the scoring system and the
// rumble; completed and in-progress stunts post actions 15 / 16, feed the developer challenges,
// achievements and training tips and keep the profile's best stunt stats; prop hits go to the stunt
// manager, prop-progression requests arm the dispatcher's tail, and training requests reach the
// training manager.
//
// lpActionQueue is the dispatcher's output action queue, lpOutput its output buffer. ProcessEvent's
// float is the module's cached sim time step. Payloads are read through their event structs
// (BrnGameEvents.h).

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                           // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"             // AddEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                       // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameStateSharedIO.h"                       // IsShowtimeGameMode
#include "GameSource/GameState/BrnGameEvents.h"                              // the event payloads
#include "GameSource/GameState/BrnGameActions.h"                             // the action records
#include "GameSource/GameState/ModeManager/BrnModeManager.h"                 // ModeManager::ProcessEvent
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"          // GameMode::OnPlayerInShortCut / HandleGameEvents
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"       // ScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h" // CrashModeScoring
#include "GameSource/GameState/RumbleManager/BrnRumbleManager.h"             // RumbleManager
#include "GameSource/GameState/Offences/BrnStuntManager.h"                   // StuntManager
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"         // TrainingManager::RequestTraining
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // DeveloperChallengeManager
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"              // CarSelectManager::IsInJunkyard
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"        // OnlineCarSelectManager::IsInOnlineCarSelect
#include "GameSource/GameState/Progression/BrnProgressionManager.h"          // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                     // Profile
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"           // BrnPhysics::Vehicle::EImpactType
#include "SharedClasses/Progression/BrnTrainingTypes.h"                      // BrnProgression::ETrainingType

namespace BrnGameState
{

namespace
{
    // Radians to degrees for the completed flat-spin angle (case 119).
    const f32 KF_RADIANS_TO_DEGREES = 57.29578f;
    // The flat spin, in degrees, a completed stunt must beat before the flat-spin tip is offered.
    const f32 KF_FLAT_SPIN_TRAINING_MIN_DEGREES = 90.0f;
}

void GameStateModule::ProcessGameEvents_Group3(s32                                           liEventType,
                                               const CgsModule::Event*                       lpEvent,
                                               GameStateModuleIO::GameActionQueue*           lpActionQueue,
                                               const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                               GameStateModuleIO::OutputBuffer*              lpOutput)
{
    (void)lpPreWorldInput;   // no group-3 arm reads the pre-world input

    switch (liEventType)
    {
    case GameStateModuleIO::E_EVENT_RACE_CAR_NEEDS_HIDING:
        mModeManager.GetCurrentGameMode()->HandleGameEvents(lpEvent, GameStateModuleIO::E_EVENT_RACE_CAR_NEEDS_HIDING);
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_IN_SHORT_CUT:
        mModeManager.GetCurrentGameMode()->OnPlayerInShortCut();
        break;

    case GameStateModuleIO::E_EVENT_RECORD_PROP_HIT:
    {
        const GameStateModuleIO::RecordPropHitEvent* lpPropHitEvent =
            reinterpret_cast<const GameStateModuleIO::RecordPropHitEvent*>(lpEvent);
        mStuntManager.OnPropHit(lpPropHitEvent->muZoneId, lpPropHitEvent->muPropId, lpPropHitEvent->mPosition);
        break;
    }

    case GameStateModuleIO::E_EVENT_OVERHEAD_SIGN_HIT:
        // Only showtime scores overhead signs.
        if (GameStateModuleIO::IsShowtimeGameMode(mModeManager.GetCurrentGameModeType()))
        {
            const GameStateModuleIO::HitOverheadSignEvent* lpSignEvent =
                reinterpret_cast<const GameStateModuleIO::HitOverheadSignEvent*>(lpEvent);
            mModeManager.GetScoringSystem()->GetCrashScorer()->DealWithHitOverheadSign();

            GameStateModuleIO::OverheadSignHitAction lSignAction = GameStateModuleIO::OverheadSignHitAction();
            lSignAction.miPointsAwarded = KI_SCORE_BONUS_PER_OVERHEAD_SIGN;
            lSignAction.mu8SignFlags    = lpSignEvent->muRaceCarId;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSignAction),
                                    GameStateModuleIO::E_ACTION_OVERHEAD_SIGN_HIT,
                                    static_cast<s32>(sizeof(lSignAction)));
        }
        break;

    case GameStateModuleIO::E_EVENT_REQUEST_PROP_PROGRESSION:
        // The dispatcher's tail posts the profile's hit props once the walk is done.
        mbPropSystemNeedsProgression = true;
        break;

    case GameStateModuleIO::E_EVENT_GAME_TRAINING_REQUEST:
    {
        const GameStateModuleIO::RequestGameTrainingEvent* lpTrainingEvent =
            reinterpret_cast<const GameStateModuleIO::RequestGameTrainingEvent*>(lpEvent);
        mTrainingManager.RequestTraining(static_cast<BrnProgression::ETrainingType>(lpTrainingEvent->meTrainingType));
        break;
    }

    case GameStateModuleIO::E_EVENT_COMPLETED_STUNT:
    {
        // Not while the junkyard or the online car select owns the player car.
        if (mCarSelectManager.IsInJunkyard() || mOnlineCarSelectManager.IsInOnlineCarSelect())
        {
            break;
        }

        const GameStateModuleIO::CompletedStuntEvent* lpCompletedStuntEvent =
            reinterpret_cast<const GameStateModuleIO::CompletedStuntEvent*>(lpEvent);
        CGS_ASSERT(lpCompletedStuntEvent, "lpCompletedStuntEvent");

        GameStateModuleIO::CompletedStuntAction lStuntAction = GameStateModuleIO::CompletedStuntAction();
        lStuntAction.muStuntActionComplete         = lpCompletedStuntEvent->muStuntActionComplete;
        lStuntAction.mfCompletedBarrelRollAngle    = lpCompletedStuntEvent->mfCompletedBarrelRollAngle;
        lStuntAction.mfCompletedAirSpinAngle       = lpCompletedStuntEvent->mfCompletedAirSpinAngle;
        lStuntAction.mfCompletedHandbreakTurnAngle = lpCompletedStuntEvent->mfCompletedHandbreakTurnAngle;
        lStuntAction.mfCompletedDriftTime          = lpCompletedStuntEvent->mfCompletedDriftTime;
        lStuntAction.mfCompletedDriftDistance      = lpCompletedStuntEvent->mfCompletedDriftDistance;
        lStuntAction.miCompletedBarrelRolls        = lpCompletedStuntEvent->miCompletedBarrelRolls;
        lStuntAction.mbSuccessfulLanding           = lpCompletedStuntEvent->mbSuccessfulLanding;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStuntAction),
                                GameStateModuleIO::E_ACTION_COMPLETED_STUNT,
                                static_cast<s32>(sizeof(lStuntAction)));

        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_COMPLETED_STUNT, lpEvent, mfSimTimeStep);
        mDeveloperChallengeManager.OnStuntOffenceComplete(reinterpret_cast<const u8*>(lpEvent));

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile != 0, "lpProfile != NULL");

        const u32 luStuntActionComplete = lpCompletedStuntEvent->muStuntActionComplete;
        s32       liCompletedBarrelRolls = 0;
        f32       lfFlatSpinDegrees      = 0.0f;
        f32       lfHandbreakTurnAngle   = 0.0f;
        f32       lfDriftDistance        = 0.0f;

        if ((luStuntActionComplete & 0x1u) == 0x1u)
        {
            liCompletedBarrelRolls = lpCompletedStuntEvent->miCompletedBarrelRolls;
            mTrainingManager.RequestTraining(BrnProgression::E_TRAINING_TYPE_BARREL_ROLL);
            mAchievementManager.OnBarrelRoll(liCompletedBarrelRolls);
            mModeManager.GetScoringSystem()->PlayerPerformedBarrelRolls(GetPlayerActiveRaceCarIndex(),
                                                                        liCompletedBarrelRolls);
        }
        if ((luStuntActionComplete & 0x2u) == 0x2u)
        {
            lfFlatSpinDegrees = lpCompletedStuntEvent->mfCompletedAirSpinAngle * KF_RADIANS_TO_DEGREES;
            mAchievementManager.OnFlatSpin(lfFlatSpinDegrees);
            mDeveloperChallengeManager.OnFlatSpin(lfFlatSpinDegrees);
            if (lfFlatSpinDegrees > KF_FLAT_SPIN_TRAINING_MIN_DEGREES)
            {
                mTrainingManager.RequestTraining(BrnProgression::E_TRAINING_TYPE_FLAT_SPIN);
            }
        }
        if ((luStuntActionComplete & 0x4u) == 0x4u)
        {
            mTrainingManager.RequestTraining(BrnProgression::E_TRAINING_TYPE_USES_E_BRAKE);
            lfHandbreakTurnAngle = lpCompletedStuntEvent->mfCompletedHandbreakTurnAngle;
        }
        if ((luStuntActionComplete & 0x40u) == 0x40u)
        {
            lfDriftDistance = lpCompletedStuntEvent->mfCompletedDriftDistance;
        }
        lpProfile->SetBestStuntStats(liCompletedBarrelRolls, lfFlatSpinDegrees, lfHandbreakTurnAngle, lfDriftDistance);
        break;
    }

    case GameStateModuleIO::E_EVENT_INPROGRESS_STUNT:
    {
        // Not while the junkyard or the online car select owns the player car.
        if (mCarSelectManager.IsInJunkyard() || mOnlineCarSelectManager.IsInOnlineCarSelect())
        {
            break;
        }

        const GameStateModuleIO::InProgressStuntEvent* lpInProgressStuntEvent =
            reinterpret_cast<const GameStateModuleIO::InProgressStuntEvent*>(lpEvent);
        CGS_ASSERT(lpInProgressStuntEvent, "lpInProgressStuntEvent");

        // The console adds the player's leg distance to the record's mfConvoyLegDistance without
        // writing that word first, so its sum starts from whatever the stack slot held. The record is
        // zero-initialised here, so the sum starts from 0.
        GameStateModuleIO::InProgressStuntAction lStuntAction = GameStateModuleIO::InProgressStuntAction();
        lStuntAction.muStuntActionInProgress        = lpInProgressStuntEvent->muStuntActionInProgress;
        lStuntAction.mfInProgressBarrelRollAngle    = lpInProgressStuntEvent->mfInProgressBarrelRollAngle;
        lStuntAction.mfInProgressAirSpinAngle       = lpInProgressStuntEvent->mfInProgressAirSpinAngle;
        lStuntAction.mfInProgressHandbreakTurnAngle = lpInProgressStuntEvent->mfInProgressHandbreakTurnAngle;
        lStuntAction.mfInProgressDriftTime          = lpInProgressStuntEvent->mfInProgressDriftTime;
        lStuntAction.mfInProgressDriftDistance      = lpInProgressStuntEvent->mfInProgressDriftDistance;

        // In a convoy of two or more: find the player, report its leg distance and the car ahead.
        if ((lpInProgressStuntEvent->muStuntActionInProgress & 0x80u) == 0x80u &&
            lpInProgressStuntEvent->miConvoyMemberCount > 1)
        {
            const EActiveRaceCarIndex lePlayerIndex = GetPlayerActiveRaceCarIndex();
            s32 liConvoyIndex = 0;
            for (; liConvoyIndex < lpInProgressStuntEvent->miConvoyMemberCount; ++liConvoyIndex)
            {
                if (lpInProgressStuntEvent->maConvoyMemberARCIs[liConvoyIndex] == lePlayerIndex)
                {
                    lStuntAction.mfConvoyLegDistance += lpInProgressStuntEvent->maConvoyLegDistances[liConvoyIndex];
                    lStuntAction.miCarInFrontIndex =
                        (liConvoyIndex > 0) ? lpInProgressStuntEvent->maConvoyMemberARCIs[liConvoyIndex - 1] : -1;
                    break;
                }
            }
            CGS_ASSERT(liConvoyIndex < lpInProgressStuntEvent->miConvoyMemberCount,
                       "Player supposed to be in convoy, but not found in CarIndices");
        }
        else
        {
            lStuntAction.mfConvoyLegDistance = 0.0f;
            lStuntAction.miCarInFrontIndex   = -1;
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStuntAction),
                                GameStateModuleIO::E_ACTION_INPROGRESS_STUNT,
                                static_cast<s32>(sizeof(lStuntAction)));
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_INPROGRESS_STUNT, lpEvent, mfSimTimeStep);
        break;
    }

    case GameStateModuleIO::E_EVENT_VEHICLE_IMPACT:
    {
        const GameStateModuleIO::VehicleImpactEvent* lpImpactEvent =
            reinterpret_cast<const GameStateModuleIO::VehicleImpactEvent*>(lpEvent);
        SendVehicleImpactMessages(lpImpactEvent, lpActionQueue);

        const BrnPhysics::Vehicle::EImpactType leImpactType =
            static_cast<BrnPhysics::Vehicle::EImpactType>(lpImpactEvent->meImpactType);
        if (lpImpactEvent->meAggressorActiveRaceCarIndex == GetPlayerActiveRaceCarIndex())
        {
            mModeManager.GetScoringSystem()->OnPlayerHitsRival(leImpactType);
            mRumbleManager.OnVehicleAggressorImpact(leImpactType);
        }
        if (lpImpactEvent->meVictimActiveRaceCarIndex == GetPlayerActiveRaceCarIndex())
        {
            mRumbleManager.OnVehicleVictimImpact(leImpactType);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_TRAFFIC_CHECKING:
    {
        const GameStateModuleIO::TrafficCheckingEvent* lpVcEvent =
            reinterpret_cast<const GameStateModuleIO::TrafficCheckingEvent*>(lpEvent);
        CGS_ASSERT(lpVcEvent != 0, "lpVcEvent != NULL");

        GameStateModuleIO::TrafficCheckingAction lCheckingAction;
        lCheckingAction.muVehicleIndex = lpVcEvent->muVehicleIndex;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCheckingAction),
                                GameStateModuleIO::E_ACTION_ON_TRAFFIC_CHECKING,
                                static_cast<s32>(sizeof(lCheckingAction)));
        mRumbleManager.OnTrafficCheck();
        break;
    }

    case GameStateModuleIO::E_EVENT_CRASH_COMBO_ITEM:
    {
        CrashModeScoring* lpCrashScorer = mModeManager.GetScoringSystem()->GetCrashScorer();
        CGS_ASSERT(lpCrashScorer != 0, "lpCrashScorer != NULL");

        const GameStateModuleIO::CrashComboItemEvent* lpComboItemEvent =
            reinterpret_cast<const GameStateModuleIO::CrashComboItemEvent*>(lpEvent);
        GameStateModuleIO::CrashComboAction lComboAction;
        lComboAction.meEntryType = lpComboItemEvent->meEntryType;
        lComboAction.mfValue     = lpComboItemEvent->mfValue;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lComboAction),
                                GameStateModuleIO::E_ACTION_CRASH_COMBO,
                                static_cast<s32>(sizeof(lComboAction)));
        lpCrashScorer->DealWithComboItem(lpComboItemEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_NEAR_MISS_SCORED:
    {
        const GameStateModuleIO::NearMissScoredEvent* lpNearMissEvent =
            reinterpret_cast<const GameStateModuleIO::NearMissScoredEvent*>(lpEvent);
        GameStateModuleIO::NearMissAction lNearMissAction;
        lNearMissAction.miCount        = lpNearMissEvent->miCount;
        lNearMissAction.meNearMissType = lpNearMissEvent->meNearMissType;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNearMissAction),
                                GameStateModuleIO::E_ACTION_NEAR_MISS,
                                static_cast<s32>(sizeof(lNearMissAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_NEAR_MISS:
    case GameStateModuleIO::E_EVENT_NEAR_MISS_CHAIN_COMPLETED:
    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_ACTION_SUCCESS:
    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET:
    case GameStateModuleIO::E_EVENT_FREEBURN_CHALLENGE_RESET_ALL_ACTIONS:
        mModeManager.ProcessEvent(static_cast<GameStateModuleIO::EGameEventType>(liEventType), lpEvent, mfSimTimeStep);
        break;

    case GameStateModuleIO::E_EVENT_DRIFTING:
    {
        const GameStateModuleIO::DriftingEvent* lpDriftingEvent =
            reinterpret_cast<const GameStateModuleIO::DriftingEvent*>(lpEvent);
        GameStateModuleIO::DriftingAction lDriftingAction;
        lDriftingAction.mfDistance = lpDriftingEvent->mfDistance;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDriftingAction),
                                GameStateModuleIO::E_ACTION_DRIFTING,
                                static_cast<s32>(sizeof(lDriftingAction)));
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_DRIFTING, lpEvent, mfSimTimeStep);
        break;
    }

    case GameStateModuleIO::E_EVENT_SPINNING:
    {
        const GameStateModuleIO::SpinningEvent* lpSpinningEvent =
            reinterpret_cast<const GameStateModuleIO::SpinningEvent*>(lpEvent);
        GameStateModuleIO::SpinningAction lSpinningAction;
        lSpinningAction.mfSpinAngle = lpSpinningEvent->mfSpinAngle;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSpinningAction),
                                GameStateModuleIO::E_ACTION_SPINNING,
                                static_cast<s32>(sizeof(lSpinningAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_IN_AIR:
    {
        const GameStateModuleIO::InAirEvent* lpInAirEvent =
            reinterpret_cast<const GameStateModuleIO::InAirEvent*>(lpEvent);
        CGS_ASSERT(lpInAirEvent, "lpInAirEvent");

        GameStateModuleIO::InAirAction lInAirAction;
        lInAirAction.mfCumulativeAirTime  = lpInAirEvent->mfCumulativeAirTime;
        lInAirAction.mfCurrentJumpAirTime = lpInAirEvent->mfCurrentJumpAirTime;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lInAirAction),
                                GameStateModuleIO::E_ACTION_IN_AIR,
                                static_cast<s32>(sizeof(lInAirAction)));

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile != 0, "lpProfile != NULL");
        lpProfile->SetNewAirMaximum(lpInAirEvent->mfCurrentJumpAirTime);

        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_IN_AIR, lpEvent, mfSimTimeStep);
        break;
    }

    case GameStateModuleIO::E_EVENT_ONCOMING:
    {
        const GameStateModuleIO::OncomingEvent* lpOncomingEvent =
            reinterpret_cast<const GameStateModuleIO::OncomingEvent*>(lpEvent);
        GameStateModuleIO::OncomingAction lOncomingAction;
        lOncomingAction.mfDistance = lpOncomingEvent->mfDistance;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lOncomingAction),
                                GameStateModuleIO::E_ACTION_ONCOMING,
                                static_cast<s32>(sizeof(lOncomingAction)));

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile != 0, "lpProfile != NULL");
        lpProfile->SetNewOncomingMaximum(lOncomingAction.mfDistance);

        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_ONCOMING, lpEvent, mfSimTimeStep);
        break;
    }

    case GameStateModuleIO::E_EVENT_ONCOMING_COMPLETED:
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_ONCOMING_COMPLETED, lpEvent, mfSimTimeStep);
        break;

    case GameStateModuleIO::E_EVENT_TAILGATING:
    {
        const GameStateModuleIO::TailgatingEvent* lpTailgatingEvent =
            reinterpret_cast<const GameStateModuleIO::TailgatingEvent*>(lpEvent);
        GameStateModuleIO::TailgatingAction lTailgatingAction;
        lTailgatingAction.mfDistance          = lpTailgatingEvent->mfDistance;
        lTailgatingAction.meTailgatedCarIndex = static_cast< ::EActiveRaceCarIndex>(lpTailgatingEvent->meTailgatedCarIndex);
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTailgatingAction),
                                GameStateModuleIO::E_ACTION_TAILGATING,
                                static_cast<s32>(sizeof(lTailgatingAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_RESET_ON_TRACK:
    {
        const GameStateModuleIO::PlayerResetOnTrackAction lResetAction = GameStateModuleIO::PlayerResetOnTrackAction();
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetAction),
                                GameStateModuleIO::E_ACTION_PLAYER_RESET_ON_TRACK,
                                static_cast<s32>(sizeof(lResetAction)));
        mStuntManager.ClearActiveJump();
        break;
    }

    case GameStateModuleIO::E_EVENT_TRIGGER_CRASH_BREAKER:
    {
        const GameStateModuleIO::TriggerCrashBreakerEvent* lpCrashBreakerEvent =
            reinterpret_cast<const GameStateModuleIO::TriggerCrashBreakerEvent*>(lpEvent);
        GameStateModuleIO::TriggerCrashBreakerAction lCrashBreakerAction;
        lCrashBreakerAction.mPosition        = lpCrashBreakerEvent->mPosition;
        lCrashBreakerAction.meRaceCarIndex   = lpCrashBreakerEvent->meRaceCarIndex;
        lCrashBreakerAction.mfNormMagnitude  = lpCrashBreakerEvent->mfNormMagnitude;
        lCrashBreakerAction.mfTimeUntilStart = lpCrashBreakerEvent->mfTimeUntilStart;
        lCrashBreakerAction.mfDurationTime   = lpCrashBreakerEvent->mfDurationTime;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCrashBreakerAction),
                                GameStateModuleIO::E_ACTION_TRIGGER_CRASH_BREAKER,
                                static_cast<s32>(sizeof(lCrashBreakerAction)));
        mModeManager.GetScoringSystem()->GetCrashScorer()->DealWithCrashbreakerRequest(lpCrashBreakerEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_CANCEL_CRASH_BREAKER:
    {
        const GameStateModuleIO::CancelCrashBreakerEvent* lpCancelEvent =
            reinterpret_cast<const GameStateModuleIO::CancelCrashBreakerEvent*>(lpEvent);
        GameStateModuleIO::CancelCrashBreakerAction lCancelAction;
        lCancelAction.meRaceCarIndex = lpCancelEvent->meRaceCarIndex;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCancelAction),
                                GameStateModuleIO::E_ACTION_CANCEL_CRASH_BREAKER,
                                static_cast<s32>(sizeof(lCancelAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PICKUP:
        mModeManager.GetScoringSystem()->GetCrashScorer()->DealWithPickup(
            reinterpret_cast<const GameStateModuleIO::PickupEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_VEHICLE_LEAPT:
    {
        mModeManager.GetScoringSystem()->GetCrashScorer()->DealWithVehicleLeaping(
            reinterpret_cast<const GameStateModuleIO::VehicleLeaptEvent*>(lpEvent));

        const GameStateModuleIO::VehicleLeaptAction lLeaptAction = GameStateModuleIO::VehicleLeaptAction();
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lLeaptAction),
                                GameStateModuleIO::E_ACTION_VEHICLE_LEAPT,
                                static_cast<s32>(sizeof(lLeaptAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_ENTER_NEW_ROAD:
    {
        const GameStateModuleIO::EnterNewRoadEvent* lpNewRoadEvent =
            reinterpret_cast<const GameStateModuleIO::EnterNewRoadEvent*>(lpEvent);
        GameStateModuleIO::EnterNewRoadAction lNewRoadAction;
        lNewRoadAction.mbIsJunction = lpNewRoadEvent->mbIsJunction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNewRoadAction),
                                GameStateModuleIO::E_ACTION_ENTER_NEW_ROAD,
                                static_cast<s32>(sizeof(lNewRoadAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_POWER_PARK_RESULT:
    {
        const GameStateModuleIO::PowerParkResultEvent* lpPowerParkEvent =
            reinterpret_cast<const GameStateModuleIO::PowerParkResultEvent*>(lpEvent);
        if (lpPowerParkEvent->meOutcome == BrnWorld::E_PPO_SUCCESS)
        {
            CGS_ASSERT(IsOnlineGameMode() || lpPowerParkEvent->miOtherPlayersInvolved == 0,
                       "IsOnlineGameMode() || lpPowerParkEvent->miOtherPlayersInvolved == 0");
            mProgressionManager.OnPowerParkResult(lpPowerParkEvent->miOverallRating,
                                                  lpPowerParkEvent->miOtherPlayersInvolved >= 2);
            mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_POWER_PARK_RESULT, lpEvent, mfSimTimeStep);
            mAchievementManager.OnPowerParking(lpPowerParkEvent->miOverallRating);
        }

        GameStateModuleIO::PowerParkResultAction lPowerParkAction;
        lPowerParkAction.meOutcome       = static_cast<BrnWorld::EPowerParkOutcome>(lpPowerParkEvent->meOutcome);
        lPowerParkAction.miOverallRating = lpPowerParkEvent->miOverallRating;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lPowerParkAction),
                                GameStateModuleIO::E_ACTION_POWER_PARK_RESULT,
                                static_cast<s32>(sizeof(lPowerParkAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_AFTERTOUCH:
    {
        const GameStateModuleIO::AftertouchEvent* lpAftertouchEvent =
            reinterpret_cast<const GameStateModuleIO::AftertouchEvent*>(lpEvent);
        GameStateModuleIO::AftertouchAction lAftertouchAction;
        lAftertouchAction.meRaceCarIndex       = lpAftertouchEvent->meRaceCarIndex;
        lAftertouchAction.mfForwardAftertouch  = lpAftertouchEvent->mfForwardAftertouch;
        lAftertouchAction.mfSidewaysAftertouch = lpAftertouchEvent->mfSidewaysAftertouch;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAftertouchAction),
                                GameStateModuleIO::E_ACTION_AFTERTOUCH,
                                static_cast<s32>(sizeof(lAftertouchAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_RACE_CAR_DRIVING_IN_CRASH:
    {
        const GameStateModuleIO::RaceCarDrivingInCrashEvent* lpDrivingInCrashEvent =
            reinterpret_cast<const GameStateModuleIO::RaceCarDrivingInCrashEvent*>(lpEvent);
        GameStateModuleIO::ResetRaceCarCrashingAction lResetCrashingAction;
        lResetCrashingAction.meActiveRaceCarIndex = lpDrivingInCrashEvent->meActiveRaceCarIndex;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetCrashingAction),
                                GameStateModuleIO::E_ACTION_RESET_RACE_CAR_CRASHING,
                                static_cast<s32>(sizeof(lResetCrashingAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_TRAFFIC_CHECKING_CHAIN:
    {
        const GameStateModuleIO::TrafficCheckingChainEvent* lpTrafficCheckingEvent =
            reinterpret_cast<const GameStateModuleIO::TrafficCheckingChainEvent*>(lpEvent);
        CGS_ASSERT(lpTrafficCheckingEvent, "lpTrafficCheckingEvent");

        // This one goes to the output buffer's own action queue.
        GameStateModuleIO::TrafficCheckingChainAction lChainAction;
        lChainAction.miChainSize = lpTrafficCheckingEvent->miChainSize;
        lpOutput->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChainAction),
                                                 GameStateModuleIO::E_ACTION_ON_TRAFFIC_CHECKING_CHAIN,
                                                 static_cast<s32>(sizeof(lChainAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE:
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_ACTIVE_FREEBURN_CHALLENGE, lpEvent, mfSimTimeStep);
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_CAN_SKIP_CRASH:
    {
        const GameStateModuleIO::HUDMessagePlayerCanSkipCrashAction lSkipCrashAction = GameStateModuleIO::HUDMessagePlayerCanSkipCrashAction();
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSkipCrashAction),
                                GameStateModuleIO::E_ACTION_HUD_MESSAGE_PLAYER_CAN_SKIP_CRASH,
                                static_cast<s32>(sizeof(lSkipCrashAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_CRASH_ENDING:
    {
        const GameStateModuleIO::PlayerCrashEndingSoonAction lCrashEndingAction = GameStateModuleIO::PlayerCrashEndingSoonAction();
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCrashEndingAction),
                                GameStateModuleIO::E_ACTION_PLAYER_CRASH_ENDING_SOON,
                                static_cast<s32>(sizeof(lCrashEndingAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_BOOST_TIME_COMPLETE:
        mModeManager.ProcessEvent(GameStateModuleIO::E_EVENT_BOOST_TIME_COMPLETE, lpEvent, mfSimTimeStep);
        break;
    }
}

}

// FOLDED FROM GameStateModule_ProcessGameEvents_wBT_04.cpp
// BrnGameState::GameStateModule::ProcessGameEvents_Group4 -- the GUI data-request arms of the
// game-event dispatcher (GameStateModule_ProcessGameEvents.cpp routes these ids here).
//
// Each case is one arm of the console's single switch, in source order: the GUI asks the game
// state for progression, rank, rival, landmark, region and road-rule data and gets an action
// back; the street manager, road-rules manager, mugshot manager and image manager take their
// online and gallery events; and the target-event-score family (ids 104, 152, 153, 175, which
// have no name in the image) keeps the profile's stored target scores in step with the server.
//
// lpActionQueue is the dispatcher's output action queue, lpOutput its output buffer. Payloads are
// read through their event structs (BrnGameEvents.h, and the image manager / mugshot manager
// headers for the events those managers own).

#include "GameSource/GameState/BrnGameStateModule.h"

#include <cstring>                                                        // memcpy (case 115)

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameShared/GameClasses/Containers/CgsArray.h"                  // Array<ProfileEvent,175>, Array<s64,15>
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                  // WorldMap2D::GetValue (case 95)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // OutputBuffer, PreWorldInputBuffer
#include "GameSource/GameState/BrnGameEvents.h"                          // the request payloads
#include "GameSource/GameState/BrnGameActions.h"                         // the answer records
#include "GameSource/GameState/BrnCgsPlayerName.h"                       // CgsNetwork::PlayerName
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"             // GameStats, LandmarkVariableInfo
#include "GameSource/GameState/SharedIO/BrnTargetEventScore.h"           // TargetEventScore
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"    // RivalData
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // ModeManager
#include "GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManager.h" // CountCompletedChallenges
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"          // RoadRulesManager
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"   // StreetManager
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"       // MugshotManager + its two events
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h" // GameStateImageManagerBase + its events
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // CarSelectManager
#include "GameSource/GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.h" // OnDistrictChange (case 115)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"               // RouteMapModuleIO::E_OWNER_GUI (case 84)
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData
#include "SharedClasses/Progression/BrnRival.h"                          // Rival
#include "SharedClasses/Progression/BrnRace.h"                           // Race (case 89)
#include "SharedClasses/World/BrnWorldRegion.h"                          // WorldRegion (cases 95, 102, 115)

namespace BrnGameState
{

void GameStateModule::ProcessGameEvents_Group4(s32                                           liEventType,
                                               const CgsModule::Event*                       lpEvent,
                                               GameStateModuleIO::GameActionQueue*           lpActionQueue,
                                               const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                               GameStateModuleIO::OutputBuffer*              lpOutput)
{
    (void)lpPreWorldInput;   // no group-4 arm reads the pre-world input

    switch (liEventType)
    {
    case GameStateModuleIO::E_EVENT_CHECK_FOR_COMPLETION:
        mProgressionManager.SendGameCompletionResults(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_ENTERS_RACE_MAP:
    case GameStateModuleIO::E_EVENT_LANDMARK_RACES_REQUEST:
        SendSetLandmarkRacesAction(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_LANDMARK_ROUTE_REQUEST:
    {
        SendRouteRequestAction(reinterpret_cast<const GameStateModuleIO::LandmarkRouteRequestEvent*>(lpEvent),
                               lpActionQueue,
                               BrnAI::RouteMapModuleIO::E_OWNER_GUI);

        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_PB_RECV:
        mStreetManager.ProcessNetworkHighScoreEvent(
            lpOutput, reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesPersonalBestRecvEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_UPLOADED:
        mStreetManager.ProcessUploadEvent(
            reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesUploadedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_DOWNLOADED:
        mStreetManager.ProcessDownloadEvent(
            reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesDownloadedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_EVENT_STATE_REQUEST:
    {
        // Every event the player has discovered, answered as action 179 (the whole 1404-byte
        // array, count word included).
        const BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();

        CgsContainers::Array<BrnProgression::ProfileEvent, 175> lDiscoveredEvents;
        lDiscoveredEvents.Construct();

        for (u32 luEvent = 0; luEvent < lpProfile->GetEventCount(); ++luEvent)
        {
            const BrnProgression::ProfileEvent* lpProfileEvent = lpProfile->GetEvent(luEvent);
            if (lpProfileEvent->IsFlagSet(BrnProgression::ProfileEvent::E_FLAG_DISCOVERED))
            {
                lDiscoveredEvents.Append(*lpProfileEvent);
            }
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lDiscoveredEvents),
                                GameStateModuleIO::E_ACTION_EVENT_STATE_RESPONSE,
                                static_cast<s32>(sizeof(lDiscoveredEvents)));
        break;
    }

    case GameStateModuleIO::E_EVENT_GAME_STATS_REQUEST:
    {
        const s32 liNumChallengesCompleted = mModeManager.GetChallengeManager()->CountCompletedChallenges();

        GameStateModuleIO::GameStats lGameStats;
        mProgressionManager.GetGameStats(&lGameStats, &mStuntManager, liNumChallengesCompleted);

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lGameStats),
                                GameStateModuleIO::E_ACTION_GAME_STATS_RESPONSE,
                                static_cast<s32>(sizeof(lGameStats)));
        break;
    }

    case GameStateModuleIO::E_EVENT_RANK_INFO_REQUEST:
    {
        const BrnProgression::ProgressionData* lpProgressionData = mProgressionManager.GetProgressionData();
        const s32 liRankCount = static_cast<s32>(lpProgressionData->GetProgressionRankCount());

        const s32 liMarkedMan = static_cast<s8>(
            mProgressionManager.GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_MARKED_MAN));
        const s32 liStuntAttack = static_cast<s8>(
            mProgressionManager.GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_STUNT_ATTACK));
        const s32 liRoadRage = static_cast<s8>(
            mProgressionManager.GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_ROAD_RAGE));
        const s32 liOfflineRace = static_cast<s8>(
            mProgressionManager.GetProgressionRankForGameMode(GameStateModuleIO::E_MODE_OFFLINE_RACE));
        const s32 liPlayerRank = static_cast<s8>(mProgressionManager.GetProgressionRank());

        GameStateModuleIO::RankInfoResponseAction lRankInfo;
        lRankInfo.SetProgressionRanks(liPlayerRank, liRankCount, liOfflineRace, liRoadRage, liStuntAttack, liMarkedMan);

        const BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        lRankInfo.SetProgressionRankEventWins(
            lpProfile->GetNumRankWinsForGameMode(GameStateModuleIO::E_MODE_OFFLINE_RACE),
            lpProfile->GetNumRankWinsForGameMode(GameStateModuleIO::E_MODE_ROAD_RAGE),
            lpProfile->GetNumRankWinsForGameMode(GameStateModuleIO::E_MODE_STUNT_ATTACK),
            lpProfile->GetNumRankWinsForGameMode(GameStateModuleIO::E_MODE_MARKED_MAN));

        // Stamped after SetProgressionRanks, so that setter's own last-rank assert never sees it.
        if (mProgressionManager.PlayerHasFinishedLastRank())
        {
            lRankInfo.miPlayerRank = GameStateModuleIO::RankInfoResponseAction::KI_PLAYER_HAS_FINISHED_LAST_RANK;
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRankInfo),
                                GameStateModuleIO::E_ACTION_RANK_INFO_RESPONSE,
                                static_cast<s32>(sizeof(lRankInfo)));
        break;
    }

    case GameStateModuleIO::E_EVENT_LANDMARK_VARIABLE_INFO_REQUEST:
    {
        const GameStateModuleIO::LandmarkInfoRequestEvent* lpLandmarkInfoRequestEvent =
            reinterpret_cast<const GameStateModuleIO::LandmarkInfoRequestEvent*>(lpEvent);

        BrnProgression::Race lRace;
        const u32 luNumRaces =
            mProgressionManager.GetRacesAtLandmark(&lRace, 1, lpLandmarkInfoRequestEvent->mLandmarkIndex, false);
        CGS_ASSERT(luNumRaces == 1,
                   "mProgressionManager.GetRacesAtLandmark( &lRace, 1, lpLandmarkInfoRequestEvent->mLandmarkIndex, false ) == 1");

        GameStateModuleIO::LandmarkVariableInfo lLandmarkVariableInfo;
        lLandmarkVariableInfo.Construct(&lRace, 0);

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lLandmarkVariableInfo),
                                GameStateModuleIO::E_ACTION_LANDMARK_VARIABLE_INFO_RESPONSE,
                                static_cast<s32>(sizeof(lLandmarkVariableInfo)));
        break;
    }

    case GameStateModuleIO::E_EVENT_PROGRESSION_PROFILE_LOADED:
        OnProfileLoaded(lpOutput, lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_CHANGE_WORLD_REGION:
    {
        const GameStateModuleIO::WorldRegionChangeEvent* lpWorldRegionChangeEvent =
            reinterpret_cast<const GameStateModuleIO::WorldRegionChangeEvent*>(lpEvent);

        GetImageManager()->HandleWorldRegionChangeEvent(lpWorldRegionChangeEvent);

        // The event's region goes out verbatim as action 112.
        GameStateModuleIO::WorldRegionChangeAction lWorldRegionChangeAction;
        lWorldRegionChangeAction.mNewWorldRegion = lpWorldRegionChangeEvent->mNewWorldRegion;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWorldRegionChangeAction),
                                GameStateModuleIO::E_ACTION_WORLD_REGION_CHANGE,
                                static_cast<s32>(sizeof(lWorldRegionChangeAction)));

        // The rich-presence manager publishes the new district on its next update.
        GetRichPresenceManager()->OnDistrictChange(lpWorldRegionChangeEvent->mNewWorldRegion.GetDistrict());
        break;
    }

    case GameStateModuleIO::E_EVENT_PLAYER_ROUTE_UPDATED:
    {
        const GameStateModuleIO::UpdateGuiRouteAction lUpdateGuiRouteAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lUpdateGuiRouteAction),
                                GameStateModuleIO::E_ACTION_UPDATE_GUI_ROUTE,
                                static_cast<s32>(sizeof(lUpdateGuiRouteAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_REGION_FROM_POSITION_REQUEST:
    {
        const GameStateModuleIO::RegionFromPositionRequestEvent* lpRegionRequestEvent =
            reinterpret_cast<const GameStateModuleIO::RegionFromPositionRequestEvent*>(lpEvent);

        // The district map is sampled at the position flattened to (x, z); the permute fills the
        // two unread lanes with x.
        const Vector3& lrPosition = lpRegionRequestEvent->mPosition;
        static const CGS_ALIGN(16) u32 lauFlattenPermute[4] = {
            0x00010203, 0x18191A1B, 0x00010203, 0x00010203 };
        Vector2 lFlatPosition;
        lFlatPosition.mV = __vperm(lrPosition.mV, lrPosition.mV, __lvx(lauFlattenPermute, 0));

        u8 luDistrict = GetDistrictMap()->GetValue(lFlatPosition);
        if (luDistrict == CgsWorld::KU_INVALID_WORLD_MAP_VALUE)
        {
            luDistrict = static_cast<u8>(BrnWorld::E_DISTRICT_INVALID);
        }

        GameStateModuleIO::RegionFromPositionResponseAction lRegionResponse;
        lRegionResponse.mWorldRegion.Construct(static_cast<BrnWorld::EDistrict>(luDistrict));
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRegionResponse),
                                GameStateModuleIO::E_ACTION_REGION_FROM_POSITION_RESPONSE,
                                static_cast<s32>(sizeof(lRegionResponse)));
        break;
    }

    case GameStateModuleIO::E_CAR_CONTROL_CHANGE_REQUEST:
    {
        const GameStateModuleIO::CarControlChangeRequestEvent* lpControlChangeEvent =
            reinterpret_cast<const GameStateModuleIO::CarControlChangeRequestEvent*>(lpEvent);
        CGS_ASSERT(lpControlChangeEvent, "lpControlChangeEvent");

        if (lpControlChangeEvent->mbPlayerShouldHaveControl)
        {
            SetControllerState(E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE);
        }
        else
        {
            SetControllerState(E_CONTROLLERSTATE_INACTIVE_GAME_MODE_STATE);
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_GUI_AWARD_SEQUENCE_START:
    {
        // The AI drives the player's car through the award sequence.
        GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction = GameStateModuleIO::SetPlayerCarDriverAction();
        lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_AI_MODULE;
        lSetPlayerCarDriverAction.mbIsDriveThru = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                                GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                                static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));

        const GameStateModuleIO::AwardSequenceStartAction lAwardSequenceStartAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAwardSequenceStartAction),
                                GameStateModuleIO::E_ACTION_AWARD_SEQUENCE_START,
                                static_cast<s32>(sizeof(lAwardSequenceStartAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_GUI_AWARD_SEQUENCE_END:
    {
        // The player gets the car back.
        GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction = GameStateModuleIO::SetPlayerCarDriverAction();
        lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_ENTITY_MODULE;
        lSetPlayerCarDriverAction.mbIsDriveThru = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                                GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                                static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));

        const GameStateModuleIO::AwardSequenceEndAction lAwardSequenceEndAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAwardSequenceEndAction),
                                GameStateModuleIO::E_ACTION_AWARD_SEQUENCE_END,
                                static_cast<s32>(sizeof(lAwardSequenceEndAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_CAR_UNLOCK_TICKER_CLOSED:
        mCarSelectManager.OnCarUnlockTickerComplete();
        break;

    case GameStateModuleIO::E_EVENT_ROAD_RULE_DATA_REQUEST:
        mRoadRulesManager.OnRoadRulesDataRequest(
            reinterpret_cast<const GameStateModuleIO::RoadRulesDataRequestEvent*>(lpEvent)->mRoadId, lpActionQueue);
        break;

    case 104:
    {
        const GameStateModuleIO::EventDataRequestEvent* lpEventDataRequest =
            reinterpret_cast<const GameStateModuleIO::EventDataRequestEvent*>(lpEvent);
        CGS_ASSERT(lpEventDataRequest, "lpEventDataRequest");

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "mProgressionManager.GetProfile()");

        const GameStateModuleIO::TargetEventScore* lpTargetEventScore =
            lpProfile->GetTargetEvent(lpEventDataRequest->mEventId);
        if (lpTargetEventScore != 0)
        {
            GameStateModuleIO::TargetEventDataResponse lResponse;
            lResponse.mEventId = lpTargetEventScore->mEventId;
            lResponse.miScore  = lpTargetEventScore->miScore;
            // The record head opens with the holder's name.
            lResponse.mPlayerName.Construct(reinterpret_cast<const char*>(&lpTargetEventScore->mHead));

            CGS_ASSERT(lpActionQueue, "lpOutputActionQueue");
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                    GameStateModuleIO::TargetEventDataResponse::KI_GAME_ACTION_TYPE,
                                    static_cast<s32>(sizeof(lResponse)));
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_ALL_RIVALRY_DATA_REQUEST:
        SendAllRivalryData(lpActionQueue);
        break;

    case GameStateModuleIO::E_EVENT_ONE_RIVALRY_DATA_REQUEST:
    {
        const GameStateModuleIO::RivalriesOneDataRequestEvent* lpRivalRequestEvent =
            reinterpret_cast<const GameStateModuleIO::RivalriesOneDataRequestEvent*>(lpEvent);

        CGS_ASSERT(mProgressionManager.GetProgressionData(), "mProgressionManager.GetProgressionData()");
        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "mProgressionManager.GetProfile()");

        const BrnProgression::Rival* lpRival =
            mProgressionManager.GetProgressionData()->FindRival(lpRivalRequestEvent->mRivalID);
        // The console streams the rival id between the two halves of this message.
        CGS_ASSERT(lpRival, "GS: Rival with ID  does not exist! \n");

        GameStateModuleIO::RivalryOneInDepthAction lRivalryAction;
        lRivalryAction.mId           = lpRival->GetId();
        lRivalryAction.mCarId        = lpRival->GetCarId();
        lRivalryAction.meCountyIndex = BrnWorld::WorldRegion::DistrictToCounty(lpRival->GetDistrict());

        const BrnProgression::RivalData* lpRivalData = lpProfile->FindRival(lpRivalRequestEvent->mRivalID);
        if (lpRivalData == 0)
        {
            lRivalryAction.miRaceWins       = 0;
            lRivalryAction.miProgressStatus = 0;
        }
        else
        {
            lRivalryAction.miRaceWins = lpRivalData->miEventCount;
            const f32 lfProgressStatus =
                (lpRivalData->meState == BrnProgression::RivalData::E_STATE_BEATEN) ? 100.0f : 0.0f;
            lRivalryAction.miProgressStatus = static_cast<s8>(lfProgressStatus);
        }

        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRivalryAction),
                                GameStateModuleIO::E_ACTION_ONE_RIVALRY_DATA_RESPONSE,
                                static_cast<s32>(sizeof(lRivalryAction)));
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_COLLECTABLE:
    {
        const GameStateModuleIO::OnlineNetworkPlayerCollectableEvent* lpCollectableEvent =
            reinterpret_cast<const GameStateModuleIO::OnlineNetworkPlayerCollectableEvent*>(lpEvent);
        CGS_ASSERT(lpCollectableEvent, "lpCollectableEvent");

        // Only a collectable the local player still lacks is passed on, on the output buffer's queue.
        if (!mProgressionManager.GetProfile()->IsStuntElementDone(lpCollectableEvent->meType, lpCollectableEvent->mID))
        {
            GameStateModuleIO::OnlineNetworkPlayerCollectableAction lCollectableAction;
            lCollectableAction.mID              = lpCollectableEvent->mID;
            lCollectableAction.mNetworkPlayerID = lpCollectableEvent->mNetworkPlayerID;
            lCollectableAction.meType           = lpCollectableEvent->meType;
            lpOutput->GetGameActionQueue()->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCollectableAction),
                                                     GameStateModuleIO::E_ACTION_NETWORK_COLLECTABLE,
                                                     static_cast<s32>(sizeof(lCollectableAction)));
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_ONLINE_IMAGE_RECEIVED:
        GetMugshotManager()->ProcessImageReceivedEvent(reinterpret_cast<const OnlineImageReceivedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_CAPTURE_WINNER_PHOTO_FINISH:
        GetMugshotManager()->ProcessOnlineWin(lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_TO_SAVE:
        GetImageManager()->HandleImageSaveEvent(reinterpret_cast<const ImageToSaveEvent*>(lpEvent), lpOutput,
                                                mModeManager.IsInPostEvent());
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_FILES_SAVED:
        GetImageManager()->HandleImageFilesSavedEvent(reinterpret_cast<const ImageFilesSavedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_REQUEST:
        GetImageManager()->HandleImageGalleryRequest(reinterpret_cast<const ImageGalleryRequestEvent*>(lpEvent),
                                                     lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_COUNT_REQUEST:
        GetImageManager()->HandleImageGalleryCountRequest(
            reinterpret_cast<const ImageGalleryCountRequestEvent*>(lpEvent), lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_GALLERY_DATA_REQUEST:
        GetImageManager()->HandleImageGalleryDataRequest(
            reinterpret_cast<const ImageGalleryDataRequestEvent*>(lpEvent), lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_ROAD_RULE_INTERACTION_CHANGE:
        mRoadRulesManager.SetSwitchingActive(
            reinterpret_cast<const GameStateModuleIO::RoadRuleInteractionChangeEvent*>(lpEvent)->mbSwitchRoadRulesEnabled);
        break;

    case GameStateModuleIO::E_EVENT_ROAD_RULE_MODE_SWITCH:
        mRoadRulesManager.SetRoadRulesMode(
            lpOutput, reinterpret_cast<const GameStateModuleIO::RoadRuleModeSwitchEvent*>(lpEvent)->mbIsOnline);
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_ROAD_RULES_CONNECT_INFO:
    {
        const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent* lpRRConnectedOnlineEvent =
            reinterpret_cast<const GameStateModuleIO::OnlineRoadRulesConnectInfoEvent*>(lpEvent);
        CGS_ASSERT(lpRRConnectedOnlineEvent, "lpRRConnectedOnlineEvent");
        mStreetManager.ProcessConnectedOnlineEvent(lpOutput, lpRRConnectedOnlineEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_ROAD_RULE_ROAD_SCORE_REQUEST:
        mStreetManager.ProcessScoreRequestEvent(
            lpOutput, reinterpret_cast<const GameStateModuleIO::RoadRulesScoreRequestEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_IMAGE_FILES_LOADED:
        GetImageManager()->HandleImageFilesLoadedEvent(reinterpret_cast<const ImageFilesLoadedEvent*>(lpEvent),
                                                       lpOutput);
        break;

    case GameStateModuleIO::E_EVENT_ONLINE_IMAGE_SEND_ABORTED:
        GetMugshotManager()->ProcessAbortCaptureEvent(reinterpret_cast<const OnlineImageCaptureAbortedEvent*>(lpEvent));
        break;

    case GameStateModuleIO::E_EVENT_ROAD_RULE_BATCH_DATA_REQUEST:
    {
        GameStateModuleIO::RoadRulesBatchQueryAction lRoadRulesQuery;
        mStreetManager.FillInRoadRulesQuery(&lRoadRulesQuery);
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRoadRulesQuery),
                                GameStateModuleIO::E_ACTION_ROAD_RULES_BATCH_QUERY,
                                static_cast<s32>(sizeof(lRoadRulesQuery)));
        break;
    }

    case GameStateModuleIO::E_EVENT_GUI_SWITCHES_ROAD_RULE_STATE:
    {
        const GameStateModuleIO::GUISwitchRoadRuleStateEvent* lpSwitchEvent =
            reinterpret_cast<const GameStateModuleIO::GUISwitchRoadRuleStateEvent*>(lpEvent);

        switch (lpSwitchEvent->meRoadRuleState)
        {
        case GameStateModuleIO::GUISwitchRoadRuleStateEvent::E_ROAD_RULE_STATE_OFF:
            mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_NONE);
            break;

        case GameStateModuleIO::GUISwitchRoadRuleStateEvent::E_ROAD_RULE_STATE_TIME:
            if (mModeManager.IsOnlineGameMode())
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_ONLINE_TIME);
            }
            else
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_OFFLINE_TIME);
            }
            break;

        case GameStateModuleIO::GUISwitchRoadRuleStateEvent::E_ROAD_RULE_STATE_CRASH:
            if (mModeManager.IsOnlineGameMode())
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_ONLINE_CRASH);
            }
            else
            {
                mRoadRulesManager.SetActiveRoadRule(lpActionQueue, E_ACTIVE_ROAD_RULE_OFFLINE_CRASH);
            }
            break;

        default:
            CGS_FAIL("Unknown road rule");
            break;
        }
        break;
    }

    case GameStateModuleIO::E_EVENT_BUDDY_REMOVED:
    {
        const GameStateModuleIO::BuddyRemovedEvent* lpBuddyRemovedEvent =
            reinterpret_cast<const GameStateModuleIO::BuddyRemovedEvent*>(lpEvent);
        CGS_ASSERT(lpBuddyRemovedEvent, "lpBuddyRemovedEvent");
        mStreetManager.ProcessBuddyRemoved(lpOutput, lpBuddyRemovedEvent);
        break;
    }

    case GameStateModuleIO::E_EVENT_INSTANT_FREEBURN:
    {
        const GameStateModuleIO::InstantFreeburnEvent* lpInstantFreeburnEvent =
            reinterpret_cast<const GameStateModuleIO::InstantFreeburnEvent*>(lpEvent);
        CGS_ASSERT(lpInstantFreeburnEvent, "lpInstantFreeburnEvent");

        // No car unlocks while an instant free-burn is under way.
        mCarSelectManager.SetCarUnlockEnabled(!lpInstantFreeburnEvent->mbIsDoingInstantFreeburn);
        break;
    }

    case 152:
    {
        const GameStateModuleIO::TargetEventScoreEvent* lpTargetEventScore =
            reinterpret_cast<const GameStateModuleIO::TargetEventScoreEvent*>(lpEvent);
        CGS_ASSERT(lpTargetEventScore, "lpTargetEventScore");

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        if (lpTargetEventScore->mbRemove)
        {
            lpProfile->RemoveTargetEventScore(lpTargetEventScore->mEventId);
        }
        else
        {
            lpProfile->SetTargetEventScore(lpTargetEventScore->mHead, lpTargetEventScore->mEventId,
                                           lpTargetEventScore->miScore);
        }

        GameStateModuleIO::TargetEventScoreChangedResponse lResponse;
        // The record head opens with the holder's name.
        lResponse.mPlayerName.Construct(reinterpret_cast<const char*>(&lpTargetEventScore->mHead));
        lResponse.mbRemoved = lpTargetEventScore->mbRemove;

        CGS_ASSERT(lpActionQueue, "lpOutputActionQueue");
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                GameStateModuleIO::TargetEventScoreChangedResponse::KI_GAME_ACTION_TYPE,
                                static_cast<s32>(sizeof(lResponse)));
        break;
    }

    case 153:
    {
        const GameStateModuleIO::DldScoreboardEvent* lpDldScoreboardEvent =
            reinterpret_cast<const GameStateModuleIO::DldScoreboardEvent*>(lpEvent);
        CGS_ASSERT(lpDldScoreboardEvent, "lpDldScoreboardEvent");

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        const GameStateModuleIO::TargetEventScore* lpTargetEventScore =
            lpProfile->GetTargetEvent(lpDldScoreboardEvent->mEventId);
        if (lpTargetEventScore != 0)
        {
            GameStateModuleIO::DldScoreboardResponse lResponse;
            // The record head opens with the holder's name.
            lResponse.mPlayerName.Construct(reinterpret_cast<const char*>(&lpTargetEventScore->mHead));

            CGS_ASSERT(lpActionQueue, "lpOutputActionQueue");
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResponse),
                                    GameStateModuleIO::DldScoreboardResponse::KI_GAME_ACTION_TYPE,
                                    static_cast<s32>(sizeof(lResponse)));
        }
        break;
    }

    case 175:
    {
        const GameStateModuleIO::UploadedModeScoresEvent* lpUploadedModeScoresEvent =
            reinterpret_cast<const GameStateModuleIO::UploadedModeScoresEvent*>(lpEvent);
        CGS_ASSERT(lpUploadedModeScoresEvent, "lpUploadedModeScoresEvent");

        BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile, "lpProfile");

        for (u32 luIndex = 0; luIndex < lpUploadedModeScoresEvent->maEventIds.GetLength(); ++luIndex)
        {
            lpProfile->RemoveEventScoreToUpload(lpUploadedModeScoresEvent->maEventIds.GetItem(luIndex));
        }
        break;
    }
    }
}

}

// FOLDED FROM GameStateModule_ProcessGameEvents_wBT_05.cpp
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
        // ARTIST 0x823A18D0..0x823A18E0 passes the empty one-byte action directly.
        GameStateModuleIO::StopModeSplashAction lStopModeSplashAction;
        lpActionQueue->AddEvent(&lStopModeSplashAction,
                                GameStateModuleIO::E_ACTION_STOP_MODE_SPLASH,
                                sizeof(lStopModeSplashAction));
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
        mTakedownManager.ClearRaceCarData();
        break;

    case GameStateModuleIO::E_EVENT_PLAYER_EXITED_MODE:
        mModeManager.UserCancelCurrentMode();
        mTakedownManager.ClearRaceCarData();
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
            const GameStateModuleIO::PromptDoInviteAction lPromptDoInviteAction = GameStateModuleIO::PromptDoInviteAction();
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
        const GameStateModuleIO::UpdatePrepareForInviteAction lUpdatePrepareForInviteAction = GameStateModuleIO::UpdatePrepareForInviteAction();
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
        mPaybackManager.ProcessPaybackTriggerableEvent();
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
        const GameStateModuleIO::JustAppliedExtraSpinAction lSpinAction = GameStateModuleIO::JustAppliedExtraSpinAction();
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
        BrnNetwork::StuntMultiplierMessage::MultiplierInfo lMultiplierInfo;
        std::memcpy(&lMultiplierInfo, &lpMultiplierEvent->mi64MultiplierData, sizeof(lMultiplierInfo));
        mModeManager.GetScoringSystem()->SetNetworkStuntMultiplier(lpMultiplierEvent->mNetworkPlayerID,
                                                                   lpMultiplierEvent->miStuntMultiplier,
                                                                   lMultiplierInfo);
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
