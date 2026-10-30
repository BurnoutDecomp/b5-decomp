#pragma once

// Native ARTIST GameStateModule: member identities/positions from827E44B8 and82380388.
// Exact sizeof47610 is independently reported at823BF02C.
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"
#include "GameSource/GameState/InviteManager/BrnGameStateInviteManager.h"
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h"
#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h"
#include "GameSource/GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h"

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "GameSource/BurnoutConstants.h"
#include "SharedClasses/BrnSharedConstants.h"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Containers/CgsStack.h"
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"
#include "GameSource/GameState/BrnResetPlayerDebugComponent.h"
#include "GameSource/GameState/BrnGameStateDebugComponent.h"
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"

#include "GameSource/GameState/Offences/BrnStuntManager.h"
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h"
#include "GameSource/GameState/FlybyManager/BrnGameStateOfflineFlybyManager.h"

#include "GameSource/GameState/AchievementManager/X360/BrnGameStateAchievementManagerX360.h"
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"
#include "GameSource/GameState/RumbleManager/BrnRumbleManager.h"

#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"

namespace BrnResource { struct VehicleList; struct VehicleListEntry; }

namespace BrnProgression { class Profile; }

namespace BrnGameState  { namespace GameStateModuleIO { struct OutputBuffer; } }

namespace BrnGameState  { namespace GameStateModuleIO { struct PreWorldInputBuffer; } }

namespace BrnGameState  { namespace GameStateModuleIO { struct PostWorldInputBuffer; } }

namespace BrnGameState  { namespace GameStateModuleIO { class GameEventQueue; struct StreamingCompleteEvent; } }

namespace BrnResource    { struct VehicleList; }

namespace BrnResource    { class  WheelList; }
namespace BrnTrigger     { struct TriggerData; }
namespace CgsWorld       { struct WorldMap2D; }

namespace CgsModule      { struct IOBufferStack; }

namespace CgsSystem      { struct TimerRequests; }
namespace BrnResource    { namespace GameDataIO { class AllocatorList; } }

namespace BrnGameState   { struct TakedownEvent; struct TakedownManager; struct TakedownPostWorldCache; }

namespace BrnGameState   { struct MugshotManager; struct PaybackManager; }

namespace BrnGameState   { struct GameStateInviteManager; struct GameStateImageManagerBase; class OnlineFlybyManager; class RichPresenceManagerX360; }
namespace BrnTraffic     { namespace BrnTrafficIO { struct TrafficTypeResponse; } }

namespace BrnPhysics     { namespace Vehicle { struct VehicleOutputInterface; } }

namespace BrnGameState
{
class GameStateModule;
namespace GameStateModuleIO
{

    CgsModule::VariableEventQueue<1536, 16>* PostWorldInput(GameStateModule* lpModule);
}

class GameStateModule : public CgsModule::ModuleSingleBuffered
{
public:
    GameStateModule();

    EActiveRaceCarIndex GetPlayerActiveRaceCarIndex()
    {
        CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
        return mePlayerActiveRaceCarIndex;
    }

    void ProcessContacts(const BrnPhysics::ContactSpy::ContactSpyInterface* lpContactSpyInterface);

    bool IsOnlineGameMode();

    void CheckIfPlayerIsAtJunctionWithAnEvent(
            const GameStateModuleIO::PreWorldInputBuffer* lpInput,
            GameStateModuleIO::OutputBuffer*              lpOutput);

    void DetectModeStarts(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                          GameStateModuleIO::OutputBuffer* lpOutput);

    bool ShouldStartSnapRaceMode(bool                     lbRaceModePressed,
                                 f32                      lfGameTimestep,
                                 EGameModeStartMechanism* lpOutStartMechanism);

    void StartModeAtLights(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                           GameStateModuleIO::OutputBuffer*              lpOutput,
                           EGameModeStartMechanism                       leStartMechanism);

    void Construct() ;
    void Destruct()  ;

    virtual bool Prepare(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                 CgsModule::IOBufferStack*        lpUpdateOutputBufferStack,
                 const BrnResource::GameDataIO::AllocatorList* lpAllocatorList);
    // ARTIST vtable +0x44, 823756A8; caller supplies the output buffer.
    virtual bool Release(GameStateModuleIO::OutputBuffer* lpOutputBuffer);

    bool Prepare2(GameStateModuleIO::OutputBuffer* lpOutputBuffer);

    TriggerQueryManager*       GetTriggerQueryManager()       { return &mTriggerQueryManager; }
    const TriggerQueryManager* GetTriggerQueryManager() const { return &mTriggerQueryManager; }

    NetworkRoundManager*       GetNetworkRoundManager()       { return &mNetworkRoundManager; }

    RoadRulesManager*          GetRoadRulesManager()          { return &mRoadRulesManager; }
    const NetworkRoundManager* GetNetworkRoundManager() const { return &mNetworkRoundManager; }

    BrnProgression::ProgressionManager*       GetProgressionManager()       { return &mProgressionManager; }
    const BrnProgression::ProgressionManager* GetProgressionManager() const { return &mProgressionManager; }

    CarSelectManager*       GetCarSelectManager()       { return &mCarSelectManager; }
    const CarSelectManager* GetCarSelectManager() const { return &mCarSelectManager; }

    StreetManager*          GetStreetManager()          { return &mStreetManager; }
    MugshotManager*         GetMugshotManager()   { return (&mMugshotManager); }

    StuntManager*       GetStuntManager()       { return &mStuntManager; }
    const StuntManager* GetStuntManager() const { return &mStuntManager; }

    DriveThruManager*       GetDriveThruManager()       { return &mDriveThruManager; }
    const DriveThruManager* GetDriveThruManager() const { return &mDriveThruManager; }

    void PreWorldUpdate(CgsModule::IOBufferStack* lpInputBufferStack,
                        CgsModule::IOBufferStack* lpOutputBufferStack,
                        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                        GameStateModuleIO::OutputBuffer* lpOutput,
                        BrnUpdateSet lUpdateSet);

    void PostWorldUpdate(CgsModule::IOBufferStack* lpUpdateInputBufferStack,
                         CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                         const GameStateModuleIO::PostWorldInputBuffer* lpPostWorldInputBuffer,
                         BrnUpdateSet lUpdateSet);

    void CopyScoringDataToOutput(GameStateModuleIO::OutputBuffer* lpOutput);

    void CacheTakedownManagerPostWorldInputData(const GameStateModuleIO::PostWorldInputBuffer* lpInput);
    void ClearTakedownRaceCarData();

    void ProcessTakedownEvents(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                               const CgsModule::EventQueue<TakedownEvent, 8>* lpTakedownEventQueue,
                               GameStateModuleIO::OutputBuffer* lpOutputBuffer);

    void SendRouteRequestAction(const GameStateModuleIO::LandmarkRouteRequestEvent* lpRouteRequestEvent,
                                GameStateModuleIO::GameActionQueue*                  lpOutputActionQueue,
                                BrnAI::RouteMapModuleIO::RequestOwner                leRequestOwner);

    void StartCrashMode(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                        GameStateModuleIO::OutputBuffer*              lpOutput);

    bool ShouldStartShowtimeMode(f32                       lfGameTimestep,
                                 bool                      lbCrashModePressed,
                                 CgsSystem::TimerRequests*  lpSimTimerRequests);

    bool IsInShowtimeIntro() const;

    f32 GetShowtimeIntroSteering() const;

    void UpdateShowtimeMode(const GameStateModuleIO::PreWorldInputBuffer*       lpInput,
                            GameStateModuleIO::OutputBuffer*                    lpOutput,
                            const BrnPhysics::ContactSpy::ContactSpyInterface*  lpContacts,
                            const CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse>*
                                                                                lpResponseQueue);

    void SendVehicleImpactMessages(
        const GameStateModuleIO::VehicleImpactEvent* lpImpactEvent,
        GameStateModuleIO::GameActionQueue* lpActionQueue);

    void CheckForTailingRivals(GameStateModuleIO::OutputBuffer* lpOutput,
                               const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutput,
                               f32 lfTimeStep);

    f32  GetRivalTailingTime(s32 liRaceCarIndex) const;
    void SetRivalTailingTime(s32 liRaceCarIndex, f32 lfTime);

    bool ShouldAllowTimedTutorialTips();

    TrainingManager*       GetTrainingManager()       { return &mTrainingManager; }

    TakedownManager*       GetTakedownManager()       { return (&mTakedownManager); }
    const TakedownManager* GetTakedownManager() const { return (&mTakedownManager); }

    bool IsInTakedownCamera() const;
    const TrainingManager* GetTrainingManager() const { return &mTrainingManager; }

    CgsID FindNearestJunkyardID(Vector3 lPosition) const;

    void SendSetupPlayerCarEvent(GameStateModuleIO::GameActionQueue* lpActionQueue);

    void SendSetUpAllEventStartsMessage(GameStateModuleIO::OutputBuffer* lpOutput);

    void SendSetLandmarkRacesAction(GameStateModuleIO::GameActionQueue* lpActionQueue);

    void SendSpecificPreSetRacesModesAction(s32                              liEventType,
                                            GameStateModuleIO::OutputBuffer* lpOutput);

    void SendSetUpAllDriveThrusMessage(GameStateModuleIO::GameActionQueue* lpOutputActionQueue);

    GameStateModuleIO::EPlayerScoringIndex FindPlayerScoringIndexForActiveRaceCar(
            ::EActiveRaceCarIndex leActiveRaceCarIndex) const;

    void OnProfileLoaded(GameStateModuleIO::OutputBuffer* lpOutput,
                         GameStateModuleIO::GameActionQueue* lpOutputActionQueue);

    CgsID GetSpawnCar(const BrnProgression::Profile* lpProfile);

    s32  GetPlayerGlobalRaceCarIndex();

    bool IsRaceCarCrashing(::EActiveRaceCarIndex leRaceCarIndex);

    bool IsShowtimeGameMode();

    bool IsSimPaused(bool lbCheckGameMode, bool lbStrictMask) const;

    enum EControllerState
    {
        E_CONTROLLERSTATE_NOT_IN_GAME              = 0,
        E_CONTROLLERSTATE_CAR_SELECT               = 1,
        E_CONTROLLERSTATE_INACTIVE_GAME_MODE_STATE = 2,
        E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE   = 3
    };

    bool IsControllerActive() const;

    void SetControllerState(EControllerState leControllerState) { meControllerState = leControllerState; }
    void SetActiveGameModeState()   { meControllerState = E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE; }
    void SetInActiveGameModeState() { meControllerState = E_CONTROLLERSTATE_INACTIVE_GAME_MODE_STATE; }

    void OnModeFinish(GameStateModuleIO::OutputBuffer* lpOutputBuffer);
    void OnModeEnd(bool lbResetState);

    void CopyInputDataToPaybackManager(const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer);

    BrnNetwork::NetworkPlayerID GetLocalPlayerNetworkID() const { return mLocalPlayerNetworkID; }

    ::EActiveRaceCarIndex GetActiveRaceCarIndex(BrnNetwork::NetworkPlayerID lPlayerID);

    BrnNetwork::NetworkPlayerID GetNetworkPlayerID(::EActiveRaceCarIndex leActiveRaceCarIndex);

    BrnResource::VehicleList* GetVehicleList();

    CgsID GetOriginalCarId(CgsID lCarId);

    DeveloperChallengeManager* GetDeveloperChallengeManager();

    CgsModule::VariableEventQueue<18432, 16>* GetOutputGuiEventQueue();

    bool IsActiveRaceCarStillPresent(::EActiveRaceCarIndex leActiveRaceCarIndex) const;

    ModeManager* GetModeManager();

    GameStateModuleIO::EGameModeType GetCurrentGameModeType() const;

    void ToggleShowtimeBehaviour();

    AchievementManagerBase* GetAchievementManager();

    void RequestPause(s32 liPauseReasonFlags,
                      GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                      s32 liArg3, s32 liArg4);

    bool IsTrainingPauseSuppressed() const;

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
        GetLastActiveRaceCarInterface() const;

    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*
        GetLastGlobalRaceCarInterface() const;

    u32 GetNetworkRandomSeed() const;

    void CheckForAllEventsBeingFound(BrnProgression::Profile* lpProfile,
                                     GameStateModuleIO::GameActionQueue* lpQueue);

    void CheckForAllEventsOfATypeFound(const BrnProgression::Profile* lpProfile,
                                       GameStateModuleIO::GameActionQueue* lpQueue,
                                       BrnProgression::RaceEventData::EModeType leModeType);

    CgsID GetActivePlayerCarId() const;

    void RequestStreamingForVehicleSelection(GameStateModuleIO::GameActionQueue* lpActionQueue, CgsID lCarId);

    void GetListOfPlayerSelectableVehicles(Array<CgsID, 128>& lrCars);

    void OnSpecialEventPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                                       GameStateModuleIO::GameActionQueue* lpQueue, bool lbUpdateProfile);

    void OnPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                           GameStateModuleIO::GameActionQueue* lpQueue, bool lbUpdateProfile);

    void SetCarUnlockAlreadyShown(CgsID lCarId);

    void RequestUnpause(s32 leUnpauseModule, GameStateModuleIO::GameActionQueue* lpQueue);

    void WaitForStreaming(GameStateModuleIO::GameActionQueue* lpQueue);

    void FinishStreaming(GameStateModuleIO::GameActionQueue* lpQueue);

    void BridgeRumbleToInput(CgsInput::InputIO::PreWorldInputBuffer* lpInputInputBuffer,
                             const CgsSystem::TimerStatusInterface*  lpTimerStatusInterface);

    BrnResource::WheelList* GetWheelList();

    CgsModule::VariableEventQueue<1536, 16>* GetDebugGameEventQueue() { return &mGameEventCarryQueue; }
    void HandleChangePlayerCarEvent(const GameStateModuleIO::ChangePlayerCarEvent* lpEvent,
                                   GameStateModuleIO::GameActionQueue* lpActions);

    const BrnTrigger::TriggerData* GetTrackTriggerData() const
    {
        return mTriggerQueryManager.GetTriggerData();
    }

    CgsWorld::WorldMap2D* GetDistrictMap();

    CgsID GetActivePlayerWheelId() const;

    bool IsModeChangeInProgress() const;

private:
    friend class GameStateDebugComponent;

    friend CgsModule::VariableEventQueue<1536, 16>* GameStateModuleIO::PostWorldInput(GameStateModule*);

    void  ApplyCarStats(CgsID lCarId, GameStateModuleIO::GameActionQueue* lpQueue);

    void  ClearData();

    void ProcessGameEvents(const GameStateModuleIO::GameEventQueue*      lpGameEventQueue,
                           GameStateModuleIO::GameActionQueue*           lpActionQueue,
                           const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                           GameStateModuleIO::OutputBuffer*              lpOutput);
    void ProcessGameEvents_Group1(s32 liEventType, const CgsModule::Event* lpEvent,
                                  GameStateModuleIO::GameActionQueue* lpActionQueue,
                                  const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                  GameStateModuleIO::OutputBuffer* lpOutput);
    void ProcessGameEvents_Group2(s32 liEventType, const CgsModule::Event* lpEvent,
                                  GameStateModuleIO::GameActionQueue* lpActionQueue,
                                  const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                  GameStateModuleIO::OutputBuffer* lpOutput);
    void ProcessGameEvents_Group3(s32 liEventType, const CgsModule::Event* lpEvent,
                                  GameStateModuleIO::GameActionQueue* lpActionQueue,
                                  const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                  GameStateModuleIO::OutputBuffer* lpOutput);
    void ProcessGameEvents_Group4(s32 liEventType, const CgsModule::Event* lpEvent,
                                  GameStateModuleIO::GameActionQueue* lpActionQueue,
                                  const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                  GameStateModuleIO::OutputBuffer* lpOutput);
    void ProcessGameEvents_Group5(s32 liEventType, const CgsModule::Event* lpEvent,
                                  GameStateModuleIO::GameActionQueue* lpActionQueue,
                                  const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInput,
                                  GameStateModuleIO::OutputBuffer* lpOutput);

    void ProcessStreamingCompleteEvent(const GameStateModuleIO::StreamingCompleteEvent* lpEvent,
                                       GameStateModuleIO::GameActionQueue*              lpActionQueue);
    void OnEnterOnline(GameStateModuleIO::OutputBuffer* lpOutput, GameStateModuleIO::GameActionQueue* lpActionQueue);
    void SendAllRivalryData(GameStateModuleIO::GameActionQueue* lpActionQueue);

    enum EPauseFlags
    {
        E_PAUSE_NONE                    = 0,
        E_PAUSE_STREAMING               = 1,
        E_PAUSE_PLAYER                  = 2,
        E_PAUSE_GUI_CRASHNAV            = 4,
        E_PAUSE_CONTROLLER_DISCONNECTED = 8,
        E_PAUSE_PLAYING_REPLAY          = 16,
        E_PAUSE_GUI_CARSELECT           = 32,
        E_PAUSE_GAME_TRAINING           = 64
    };

    enum EPrepareStage
    {
        E_PREPARESTAGE_START                    = 0,
        E_PREPARESTAGE_MANAGER                  = 1,
        E_PREPARESTAGE_MODE_DATA_ACQUIRING      = 2,
        E_PREPARESTAGE_LOAD_TRIGGER_DATA        = 3,
        E_PREPARESTAGE_STUNT_MANAGER            = 4,
        E_PREPARESTAGE_REQUEST_CHALLENGE_LIST   = 5,
        E_PREPARESTAGE_RECEIVE_CHALLENGE_LIST   = 6,
        E_PREPARESTAGE_REQUEST_VEHICLE_LIST     = 7,
        E_PREPARESTAGE_RECEIVE_VEHICLE_LIST     = 8,
        E_PREPARESTAGE_REQUEST_WHEEL_LIST       = 9,
        E_PREPARESTAGE_RECEIVE_WHEEL_LIST       = 10,
        E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS = 11,
        E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS = 12,
        E_PREPARESTAGE_MODEMANAGER              = 13,
        E_PREPARESTAGE_TAKEDOWNMANAGER          = 14,
        E_PREPARESTAGE_MUGSHOTMANAGER           = 15,
        E_PREPARESTAGE_PAYBACKMANAGER           = 16,
        E_PREPARESTAGE_INVITEMANAGER            = 17,
        E_PREPARESTAGE_FLYBYMANAGER             = 18,
        E_PREPARESTAGE_NETWORKROUNDMANAGER      = 19,
        E_PREPARESTAGE_PROGRESSION              = 20,
        E_PREPARESTAGE_RICH_PRESENCE            = 21,
        E_PREPARESTAGE_ACHIEVEMENT_MANAGER      = 22,
        E_PREPARESTAGE_STREET_MANAGER           = 23,
        E_PREPARESTAGE_IMAGE_MANAGER            = 24,
        E_PREPARESTAGE_RUMBLE_MANAGER           = 25,
        E_PREPARESTAGE_DEVELOPER_CHALLENGE_MANAGER = 26,
        E_PREPARESTAGE_ORIGINAL_DONE             = 27,
    };

    enum EPrepare2Stage
    {
        E_PREPARE2STAGE_START          = 0,
        E_PREPARE2STAGE_PROGRESSION    = 1,
        E_PREPARE2STAGE_STREET_MANAGER = 2,
        E_PREPARE2STAGE_DONE           = 3,
    };

    enum EReleaseStage
    {
        E_RELEASESTAGE_START = 0,
        E_RELEASESTAGE_DEVELOPER_CHALLENGE_MANAGER = 1,
        E_RELEASESTAGE_IMAGE_MANAGER = 2,
        E_RELEASESTAGE_STREET_MANAGER = 3,
        E_RELEASESTAGE_ACHIEVEMENT_MANAGER = 4,
        E_RELEASESTAGE_RICH_PRESENCE = 5,
        E_RELEASESTAGE_PROGRESSION = 6,
        E_RELEASESTAGE_NETWORKROUNDMANAGER = 7,
        E_RELEASESTAGE_FLYBYMANAGER = 8,
        E_RELEASESTAGE_INVITEMANAGER = 9,
        E_RELEASESTAGE_PAYBACKMANAGER = 10,
        E_RELEASESTAGE_MUGSHOTMANAGER = 11,
        E_RELEASESTAGE_TAKEDOWNMANAGER = 12,
        E_RELEASESTAGE_MODEMANAGER = 13,
        E_RELEASESTAGE_MANAGER = 14,
        E_RELEASESTAGE_DONE = 15,
    };

public:
    GameStateInviteManager*    GetGameStateInviteManager() { return (&mGameStateInviteManager); }
    GameStateImageManagerBase* GetImageManager()           { return (&mImageManager); }

    FlybyManager*             GetFlybyManager();
    OnlineFlybyManager*        GetOnlineFlybyManager()     { return (&mOnlineFlybyManager); }
    RichPresenceManagerX360*   GetRichPresenceManager()    { return (&mRichPresenceManager); }
private:

    static const s32 KI_MAX_SIMULTANEOUS_SHOWTIME_CRASHES = 8;

public:

    void EmmPreWorldUpdate(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                          GameStateModuleIO::OutputBuffer* lpOutput,
                          const GameStateModuleIO::GameEventQueue* lpGameEvents,
                          bool lbIsOnline);

    void UpdateRoadRulesManager(GameStateModuleIO::OutputBuffer*          lpOutputBuffer,
                                const GameStateModuleIO::ControllerInput* lpControllerInput);

private:
    // Native declaration order. No PC transport/lifetime fields are stored in this object.
    EPrepareStage mePrepareStage;                                      //00228
    EPrepare2Stage mePrepare2Stage;                                    //0022C
    EReleaseStage meReleaseStage;                                      //00230
    TakedownManager mTakedownManager;                                   //00238
    MugshotManager mMugshotManager;                                     //00500
    PaybackManager mPaybackManager;                                     //00570
    GameStateInviteManager mGameStateInviteManager;                     //007F0
    ModeManager mModeManager;                                           //01020
    TriggerQueryManager mTriggerQueryManager;                           //0A550
    DriveThruManager mDriveThruManager;                                 //0ACD0
    TrainingManager mTrainingManager;                                   //0B630
    RumbleManager mRumbleManager;                                       //0B658
    NetworkRoundManager mNetworkRoundManager;                           //0B9F8
    BrnProgression::ProgressionManager mProgressionManager;              //0BB30
    RichPresenceManagerX360 mRichPresenceManager;                        //2C4D0
    AchievementManagerX360 mAchievementManager;                         //2C5B0
    RoadRulesManager mRoadRulesManager;                                 //2CD28
    CarSelectManager mCarSelectManager;                                 //2CDA0
    OnlineCarSelectManager mOnlineCarSelectManager;                     //2CE20
    StuntManager mStuntManager;                                         //2CE90
    GameStateImageManagerBase mImageManager;                            //2D4B0
    DeveloperChallengeManager mDeveloperChallengeManager;               //2D570
    OfflineFlybyManager mOfflineFlybyManager;                           //2D630
    OnlineFlybyManager mOnlineFlybyManager;                             //2D8E0
    CgsModule::VariableEventQueue<18432, 16> mOutputGuiEventQueue;       //2E580
    f32 mafRivalTailingTimes[7];                                        //32D90
    u32 muNetworkGameRandomSeed;                                        //32DAC
    EActiveRaceCarIndex mePlayerActiveRaceCarIndex;                     //32DB0
    s32 miPlayerGlobalRaceCarIndex;                                     //32DB4
    BrnResource::ChallengeListEntry::EFreeburnChallengeStyle meFreeburnChallengeStyle; //32DB8
    bool maRaceCarCrashing[E_ACTIVE_RACE_CAR_INDEX_COUNT];               //32DBC
    bool mbSendSetupPlayerCarPending;                                   //32DC4, original mbIsFirstUpdate
    bool mbFreeburnChallengeSelectorVisible;                           //32DC5
    CgsSystem::TimerStatusInterface mTimerStatusInterface;              //32DC8
    GameStateDebugComponent mGameStateDebugComponent;                   //32DF8
    ResetPlayerDebugComponent mResetPlayerDebugComponent;               //32EA0
    s32 miSimPauseFlags;                                                //38B60
    EControllerState meControllerState;                                 //38B64
    BrnNetwork::NetworkPlayerID mLocalPlayerNetworkID;                   //38B68
    s32 miStreamingWaitCountdown;                                       //38B6C
    bool mbWaitForStreaming;                                            //38B70
    bool mbWaitingForStreaming;                                        //38B71
    bool mbWaitingToPutPlayerInJunkyard;                               //38B72
    bool mabModuleStreamingComplete[4];                               //38B73
    GameStateModuleIO::CarSelectionChangedAction mCachedCarSelectChangedAction; //38B80
    CgsModule::EventReceiverQueue<3072, 16> mReceiverQueue;             //38BC0
    BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface mLastActiveRaceCarInterface; //397E0
    BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface mLastGlobalRaceCarInterface; //3C0D0
    CgsModule::VariableEventQueue<1536, 16> mGameEventCarryQueue;        //3CA40
    CgsModule::EventQueue<TakedownEvent, 8> mTakedownEventQueue;         //3D050
    CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent, 8> mRaceCarCrashEventQueue; //3D1A0
    BrnPhysics::ContactSpy::ContactSpyInterface mContactSpyInterface;   //3D3B0
    BrnPhysics::Vehicle::VehicleOutputInterface mVehicleOutputInterface; //3D3C0
    CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse, 32> mLastTrafficTypeResponseQueue; //43FD0
    BrnAI::AIModuleIO::AICarOutputInterface mLastAICarOutputInterface;  //441E0
    u32 muCachedJunctionLightTriggerId;                                 //456C8
    u32 muCachedJunctionLogicBoxId;                                     //456CC
    bool mbJunctionNewlyDiscovered;                                    //456D0
    bool mbCanEnterEventAtJunction;                                    //456D1
    bool mbAtJunctionWithEvent;                                         //456D2
    CgsID mActivePlayerCarId;                                           //456D8
    CgsID mActivePlayerWheelId;                                         //456E0
    BrnResource::VehicleList* mpVehicleList;                             //456E8
    BrnResource::WheelList* mpWheelList;                                 //456EC
    CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette> mpPlayerCarColours; //456F0
    const BrnResource::ChallengeList* mpFreeburnChallengeList;           //45710
    f32 mfSnapRaceStartHoldSeconds;                                     //45714
    f32 mfTimeSpentDoingCrashStartAction;                              //45718
    f32 mfTimeSinceLastCrashMode;                                       //4571C
    f32 mfShowtimeIntroTimeLeft;                                        //45720
    f32 mfShowtimeIntroSteering;                                        //45724
    Vector3 mShowtimeIntroOriginalDirection;                            //45730
    EShowtimeBehaviour meShowtimeBehaviour;                             //45740
    s32 miShowtimePendingFrameDelay;                                   //45744
    CgsContainers::Stack<u16, KI_MAX_SIMULTANEOUS_SHOWTIME_CRASHES> mShowtimePendingTrafficIndexStack; //45748
    u16 muShowtimeRequestedTrafficIndex;                               //4575C
    bool mbShowtimeIntroHasTouchedGround;                             //4575E
    bool mbDebugLimitSnapRaceStartConditions;                          //4575F
    bool mbToggleShowtimeBehaviour;                                   //45760
    bool mbWasInShowtimeGameMode;                                      //45761
    bool mbAllowSnapRaceMode;                                          //45762
    StreetManager mStreetManager;                                      //45768
    Matrix44Affine mPlayerCarTransform;                                //47570
    u32 muDebugSigTDRegionIndex;                                       //475B0
    bool mbDrawCrashDebugText;                                         //475B4
    bool mbDrawSignatureTakedownRegionText;                            //475B5
    bool mbDebugMakeAllTakedownsSignature;                            //475B6
    u32 miPlayerMaxStatsDPadDownCount;                                //475B8
    f32 mfSimTimeStep;                                                  //475BC
    bool mbPropSystemNeedsProgression;                                //475C0
    bool mbIsUpdating;                                                  //475C1
    s32 miTriggersPostPM;                                               //475C4
    s32 miTriggersPrePM;                                                //475C8
    s32 miShowTimeUpdate;                                               //475CC
    s32 miCarSelectManagerUpdate;                                      //475D0
    s32 miGameStatePrePM;                                               //475D4
    s32 miGameStatePostPM;                                              //475D8
    s32 miTakedownPM;                                                   //475DC
    s32 miDetectModeStartsPM;                                           //475E0
    s32 miRoadRulesPM;                                                  //475E4
    s32 miDriveThrusPM;                                                 //475E8
    s32 miProcessEventsPM;                                              //475EC
    s32 miEmmUpdatePM;                                                  //475F0
    s32 miModeManagerPrePM;                                             //475F4
    s32 miModeManagerPostPM;                                            //475F8
    s32 miCopyingAndResettingPM;                                        //475FC
    s32 miProcessContactsPM;                                            //47600
    s32 miStreetManagerPM;                                             //47604

};
}
