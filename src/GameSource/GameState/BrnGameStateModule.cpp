#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // [diagnostic] Prepare's per-stage log line
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // BrnGameState::ModeManager::GetCurrentGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // BrnGameState::GameMode::IsOnline
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // [A9] ScoringSystem timer + medal accessors (CopyScoringDataToOutput)
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // [A9] CgsSystem::TimerStatusInterface / Time (the frame's "now")
#include "GameSource/GameState/Progression/BrnProgressionLiveryData.h"  // [A9] BrnProgression::LiveryData::mfDistanceDriven
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // GameStateModuleIO::EGameModeType (E_MODE_*_SHOWTIME)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // BrnProgression::ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                // BrnProgression::Profile::SetCarUnlockAlreadyShown
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // GameStateModuleIO::OutputBuffer (owned by pointer)
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"    // original mode-pause input: IsInTakedownCamera
#include "GameSource/GameState/BrnGameStateModule.h"             // [FX-GS2 G10-D11] mVehicleOutputInterface (CheckForTailingRivals' r5)
#include "GameSource/GameState/BrnGameEvents.h"                         // [returning-player wave] E_GUI_HAS_STARTED_GAME (the case-78 trigger)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16> (the carry-queue walk)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"
#include "SharedClasses/DataLists/VehicleList.h"                        // BrnResource::VehicleList (GetVehicleIndex / GetVehicleData)
#include "SharedClasses/DataLists/WheelList.h"                          // BrnResource::WheelList (GetWheelCount -- Prepare's list diagnostic)
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // BrnResource::VehicleListEntry (parent id / livery + car type / stats)
#include "SharedClasses/Progression/BrnProgressionData.h"               // BrnProgression::ProgressionData::FindCarOpponentSet
#include "SharedClasses/Progression/BrnOpponentData.h"                  // BrnProgression::CarOpponentSet (opponent walk)
#include "GameShared/GameClasses/Containers/CgsArray.h"                 // CgsContainers::Array<s64,7> (opponent payload)
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // RequestInterface<3072>::GetVehicleList/GetWheelList
#include "GameSource/Resource/SharedIO/BrnGameDataAllocatorList.h"     // AllocatorList::GetHeapAllocator (Prepare stage 13)
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"             // GameDataAssetEvent (the list replies)
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // [FX-FLOW] AcquireResourceResponse (Prepare stage 12's reply)
#include "SharedClasses/Graphics/BrnGlobalColourPalette.h"              // [FX-FLOW] BrnWorld::GlobalColourPalette (mpPlayerCarColours)
#include "SharedClasses/Trigger/BrnTriggerData.h"                       // BrnTrigger::TriggerData (generic-region table)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                     // BrnTrigger::GenericRegion (E_TYPE_JUNK_YARD)
#include "SharedClasses/Trigger/BrnRegion.h"                            // BrnTrigger::BoxRegion::GetPosition
#include "SharedClasses/Trigger/BrnSpawnLocation.h"                     // BrnTrigger::SpawnLocation (cross-table check)
#include <cmath>                                                        // std::sqrt (FindNearestJunkyardID)
#include <stdlib.h>                                                     // getenv (the [showtime-crash] witness)
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                 // BrnWorld::PropEntityID + BrnWorld::E_ENTITYTYPE_* (ProcessContacts' prop leg)
#include "GameSource/GameState/BrnGameActions.h"                         // [boost-wave2] PlayerHitRivalAction / RivalHitPlayerAction / ShowHudMessageAction
#include <cstring>   // [FX-BRIDGES CC-11] std::memset (SendRouteRequestAction)
#include "SharedClasses/AI/AISectionsResourceType.h"                    // [FX-BRIDGES CC-11] AISectionsData / AISection (SendRouteRequestAction's nearest section)
#include "GameShared/GameClasses/Core/CgsID.h"                            // [boost-wave2] CgsIDCompress (the two impact message-id tables)
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"        // [boost-wave2] BrnPhysics::Vehicle::EImpactType (E_IMPACT_COUNT)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"        // [FX-GS] PaybackManager::Destruct (the Destruct leg)
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"
#include "GameSource/GameState/InviteManager/BrnGameStateInviteManager.h"          // [PGEP] (&mGameStateInviteManager) (new / Construct / Prepare / Destruct)
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h"        // [PGEP] (&mImageManager)
#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h"      // [PGEP] (&mOnlineFlybyManager)
#include "GameSource/GameState/RichPresenceManager/X360/BrnGameStateRichPresenceManagerX360.h" // [PGEP] (&mRichPresenceManager)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // [PGEP] PerfMonCpu::AddMonitor ("XUserSetContext")

namespace BrnGameState
{
// The verbatim X360-baked source path every assert in this TU references.
static const char* const KAC_GSM_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp";
static const char* const KAC_OPPONENTDATA_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\progression\\BrnOpponentData.h";
// [L4 boot order 2026-09-28] The path the inlined Profile accessor's "mSpawnCarId != 0" assert names
// (BrnProfile.h:1903) at both of its console sites here: GetSpawnCar @0x82376414 and OnProfileLoaded
// @0x8239743C (`lis r11, aGamesourceGame_13@ha` -- the literal as the ARTIST export prints it).
static const char* const KAC_PROFILE_H_FILE =
    "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h";
// OnProfileLoaded @0x823974C4..0x823974DC: `lfs f0, flt_82005450` -- the image word is 0x3F666666
// (tools/re/x360rd.py 82005450), i.e. 0.9f: the traffic scale posted (action 28) for a rank-0 player.
static const f32 KF_ONPROFILELOADED_RANK_ZERO_TRAFFIC_SCALE = 0.9f;   // flt_82005450 == 0x3F666666

// ----------------------------------------------------------------------------
// X360-attested game-action event-type ids + payload sizes (the `li r5,<type>` / `li r6,<size>`
// immediates each AddEvent is given). The payload structs are GameStateModuleIO action records
// whose field layouts are not in this TU's exports, so each is posted as a sized buffer with the
// bytes the X360 actually writes reproduced at their attested in-payload offsets.
// ----------------------------------------------------------------------------
enum EGsmGameAction
{
    KI_ACTION_PLAYER_CAR_CHANGED   = 1,     // size 8   -- OnSpecialEventPlayerCarChange (the new car id)
    KI_ACTION_CAR_OPPONENT_SET     = 4,     // size 64  -- OnPlayerCarChange (Array<CgsID,7> of opponents)
    KI_ACTION_UNPAUSE              = 87,    // size 1   -- RequestUnpause
    // [tut-ticker] RequestPause @0x82382010: `li r5,0x56` (86) on the strict IsSimPaused
    // transition, `li r5,0x58` (88) on the raw-flags transition. Both 1-byte, both posted
    // from an uninitialised stack byte, exactly like KI_ACTION_UNPAUSE.
    KI_ACTION_PAUSE_STRICT         = 86,    // size 1   -- RequestPause (checked-pause transition)
    KI_ACTION_PAUSE_RAW            = 88,    // size 1   -- RequestPause (raw-flags transition)
    KI_ACTION_APPLY_CAR_STATS      = 198,   // size 24  -- ApplyCarStats
    // ProcessGameEvents case 78 (`li r6,0x40; li r5,0x40` @0x823A45E0). Same id + size as the
    // CarSelectManager-side KI_ACTION_CAR_SELECTION_CHANGED; consumed by
    // MainDirector::ProcessInputQueue case 64.
    KI_ACTION_CAR_SELECTION_CHANGED = 64,   // size 64  -- CarSelectionChangedAction
};

// ----------------------------------------------------------------------------
// Construct / Destruct.
//
// ARTIST GamePrepare creates a fresh GameStateModuleIO::OutputBuffer on its update-output
// IOBufferStack for each Prepare pass. The PC instead allocates a stable typed OutputBuffer
// here for its persistent transport, and clears forwarded requests after each pass. The buffer
// type, Construct (X360 0x82382940) and accessors are recovered; the allocation site is host-only.
// This IOBuffer lifetime is separate from the unresolved legacy base DataStructure route below.
//
// Constructing it is what makes the game-state -> director game-action route exist at all:
// BrnGameModule::BridgeGameStateToDirector @0x823CD170 Appends this buffer's +0x04 queue into
// the director input buffer's queue every frame, and MainDirector::ProcessInputQueue drains it.
// ----------------------------------------------------------------------------
// ARTIST827E44B8. Real members perform their C++ construction in native declaration order.
GameStateModule::GameStateModule()
{
    mOutputGuiEventQueue.MarkUnconstructed();
    mGameEventCarryQueue.MarkUnconstructed();
    mShowtimePendingTrafficIndexStack.miLength = CgsContainers::KI_STACK_UNCONSTRUCTED;
}

void GameStateModule::Construct()
{
    mbIsUpdating = true;
    CgsModule::ModuleSingleBuffered::Construct();
    mePrepareStage = E_PREPARESTAGE_START;
    mePrepare2Stage = E_PREPARE2STAGE_START;
    meReleaseStage = E_RELEASESTAGE_DONE;
    mResetPlayerDebugComponent.Construct(this);
    mGameStateDebugComponent.Construct(this);
    mTakedownManager.Construct(&mModeManager, &mProgressionManager);
    mMugshotManager.Construct(this);
    mPaybackManager.Construct(this);
    mGameStateInviteManager.Construct();
    mModeManager.Construct(this, &mProgressionManager, &mTriggerQueryManager,
        &mNetworkRoundManager, &mStreetManager, &mMugshotManager,
        &mRoadRulesManager, &mAchievementManager);
    mProgressionManager.Construct(&mCarSelectManager, &mStreetManager,
        &mTrainingManager, &mStuntManager);
    mReceiverQueue.Construct();
    mTriggerQueryManager.Construct(&mProgressionManager, &mTakedownManager, &mRoadRulesManager);
    mRoadRulesManager.Construct(&mStreetManager, &mModeManager, &mTrainingManager);
    mDriveThruManager.Construct(&mCarSelectManager, &mTrainingManager, &mModeManager,
        this, &mProgressionManager);
    mTrainingManager.Construct(&mProgressionManager, this);
    mRumbleManager.Construct();
    mCarSelectManager.Construct(&mTriggerQueryManager, this, &mProgressionManager);
    mOnlineCarSelectManager.Construct(this, &mProgressionManager);
    mStuntManager.Construct(&mProgressionManager, &mTriggerQueryManager, &mModeManager,
        &mTrainingManager, this);
    mOfflineFlybyManager.Construct(this, mModeManager.GetScoringSystem());
    mOnlineFlybyManager.Construct(this, mModeManager.GetScoringSystem());
    mNetworkRoundManager.Construct();
    mContactSpyInterface.Construct();
    mRaceCarCrashEventQueue.Construct();
    mTakedownEventQueue.Construct();
    mVehicleOutputInterface.Construct();
    mGameEventCarryQueue.Construct();
    mLastTrafficTypeResponseQueue.Construct();
    mOutputGuiEventQueue.Construct();
    mRichPresenceManager.miSetContextPM =
        CgsDev::PerfMonCpu::AddMonitor("XUserSetContext", CgsDev::E_PMP_5, false, 1.0f, true);
    mRichPresenceManager.CreatePresenceThread();
    mRichPresenceManager.Construct(this, &mNetworkRoundManager, &mModeManager);
    mAchievementManager.Construct(&mProgressionManager, &mStreetManager,
        mModeManager.GetScoringSystem(), this);
    mStreetManager.Construct(this, &mProgressionManager, &mRoadRulesManager);
    mImageManager.Construct(&mProgressionManager);
    mDeveloperChallengeManager.Construct(&mProgressionManager, &mStreetManager,
        mModeManager.GetScoringSystem(), this);
    mbSendSetupPlayerCarPending = true;
    ClearData();
    mPlayerCarTransform.SetIdentity();
    mfShowtimeIntroTimeLeft = 0.0f;
    mbIsNewModule = true;
    mbWasInShowtimeGameMode = false;
    mfSnapRaceStartHoldSeconds = 0.35f;
    mbToggleShowtimeBehaviour = false;
    mfTimeSpentDoingCrashStartAction = 0.01f;
    mbIsUpdating = false;
    mfTimeSinceLastCrashMode = 2.0f;
    muDebugSigTDRegionIndex = 0;
    mfSimTimeStep = 0.0f;
    meShowtimeBehaviour = static_cast<EShowtimeBehaviour>(2);
    mbPropSystemNeedsProgression = false;
    mbDebugMakeAllTakedownsSignature = false;
    mbDrawCrashDebugText = false;
    mbAllowSnapRaceMode = true;
    mbDebugLimitSnapRaceStartConditions = true;
    mLocalPlayerNetworkID = -1;
    miPlayerMaxStatsDPadDownCount = 0;
    mbDrawSignatureTakedownRegionText = false;
    mbWaitForStreaming = true;
    mbFreeburnChallengeSelectorVisible = false;
    meFreeburnChallengeStyle = static_cast<BrnResource::ChallengeListEntry::EFreeburnChallengeStyle>(0);
    miTriggersPostPM = CgsDev::PerfMonCpu::AddMonitor("Trigger Post update", CgsDev::E_PMP_5, false, 1.0f, true);
    miTriggersPrePM = CgsDev::PerfMonCpu::AddMonitor("Trigger Pre update", CgsDev::E_PMP_5, false, 1.0f, true);
    miShowTimeUpdate = CgsDev::PerfMonCpu::AddMonitor("Show Time Update", CgsDev::E_PMP_5, false, 1.0f, true);
    miCarSelectManagerUpdate = CgsDev::PerfMonCpu::AddMonitor("Car Select Manager Update", CgsDev::E_PMP_5, false, 1.0f, true);
    miGameStatePrePM = CgsDev::PerfMonCpu::AddMonitor("Pre world update", CgsDev::E_PMP_5, false, 1.0f, true);
    miGameStatePostPM = CgsDev::PerfMonCpu::AddMonitor("Post world update", CgsDev::E_PMP_5, false, 1.0f, true);
    miDetectModeStartsPM = CgsDev::PerfMonCpu::AddMonitor("Detect mode starts", CgsDev::E_PMP_5, false, 1.0f, true);
    miRoadRulesPM = CgsDev::PerfMonCpu::AddMonitor("Road rules", CgsDev::E_PMP_5, false, 1.0f, true);
    miDriveThrusPM = CgsDev::PerfMonCpu::AddMonitor("Drive thrus", CgsDev::E_PMP_5, false, 1.0f, true);
    miProcessEventsPM = CgsDev::PerfMonCpu::AddMonitor("Process events", CgsDev::E_PMP_5, false, 1.0f, true);
    miTakedownPM = CgsDev::PerfMonCpu::AddMonitor("Takedown manager", CgsDev::E_PMP_5, false, 1.0f, true);
    miEmmUpdatePM = CgsDev::PerfMonCpu::AddMonitor("EMM update", CgsDev::E_PMP_5, false, 1.0f, true);
    miModeManagerPrePM = CgsDev::PerfMonCpu::AddMonitor("Mode manager PRE", CgsDev::E_PMP_5, false, 1.0f, true);
    miModeManagerPostPM = CgsDev::PerfMonCpu::AddMonitor("Mode manager POST", CgsDev::E_PMP_5, false, 1.0f, true);
    miCopyingAndResettingPM = CgsDev::PerfMonCpu::AddMonitor("Copying and resetting", CgsDev::E_PMP_5, false, 1.0f, true);
    miProcessContactsPM = CgsDev::PerfMonCpu::AddMonitor("Process contacts", CgsDev::E_PMP_5, false, 1.0f, true);
    miStreetManagerPM = CgsDev::PerfMonCpu::AddMonitor("Street manager", CgsDev::E_PMP_5, false, 1.0f, true);
    muCachedJunctionLightTriggerId = 0xFFFFFFFFu;
    muCachedJunctionLogicBoxId = 0xFFFFFFFFu;
    mbJunctionNewlyDiscovered = false;
    mbCanEnterEventAtJunction = false;
    mbAtJunctionWithEvent = false;
    if (miGameStatePrePM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miGameStatePrePM >= 0", KAC_GSM_FILE, 295);
        CgsDev::Assert::EndAssert();
    }
    if (miGameStatePostPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miGameStatePostPM >= 0", KAC_GSM_FILE, 296);
        CgsDev::Assert::EndAssert();
    }
    if (miDetectModeStartsPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miDetectModeStartsPM >= 0", KAC_GSM_FILE, 297);
        CgsDev::Assert::EndAssert();
    }
    if (miRoadRulesPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miRoadRulesPM >= 0", KAC_GSM_FILE, 298);
        CgsDev::Assert::EndAssert();
    }
    if (miDriveThrusPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miDriveThrusPM >= 0", KAC_GSM_FILE, 299);
        CgsDev::Assert::EndAssert();
    }
    if (miProcessEventsPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miProcessEventsPM >= 0", KAC_GSM_FILE, 300);
        CgsDev::Assert::EndAssert();
    }
    if (miTakedownPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miTakedownPM >= 0", KAC_GSM_FILE, 301);
        CgsDev::Assert::EndAssert();
    }
    if (miEmmUpdatePM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miEmmUpdatePM >= 0", KAC_GSM_FILE, 302);
        CgsDev::Assert::EndAssert();
    }
    if (miModeManagerPrePM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miModeManagerPrePM >= 0", KAC_GSM_FILE, 303);
        CgsDev::Assert::EndAssert();
    }
    if (miModeManagerPostPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miModeManagerPostPM >= 0", KAC_GSM_FILE, 304);
        CgsDev::Assert::EndAssert();
    }
    if (miCopyingAndResettingPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miCopyingAndResettingPM >= 0", KAC_GSM_FILE, 305);
        CgsDev::Assert::EndAssert();
    }
    if (miStreetManagerPM < 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("miStreetManagerPM >= 0", KAC_GSM_FILE, 306);
        CgsDev::Assert::EndAssert();
    }
    mActivePlayerCarId = 0;
    mActivePlayerWheelId = 0;
}

// ARTIST 82311E48..82311E70 and 82343220..82343248 inline this selector.
FlybyManager* GameStateModule::GetFlybyManager()
{
    if (IsOnlineGameMode())
        return (&mOnlineFlybyManager);
    return &mOfflineFlybyManager;
}

// ARTIST 8238EF50. Original whole owner body; the PC bring-up pump still has
// separate extracted legs until the full PreWorldUpdate caller is recovered.
// ARTIST 823A5328. The full original pre-world chain; PC bring-up entry points
// remain separate until this caller and its prerequisites have been reviewed.
void GameStateModule::PreWorldUpdate(
        CgsModule::IOBufferStack* lpInputBufferStack,
        CgsModule::IOBufferStack* lpOutputBufferStack,
        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
        GameStateModuleIO::OutputBuffer* lpOutput,
        BrnUpdateSet lUpdateSet)
{
    const bool lbSimPaused = IsSimPaused(true, false);
    CgsDev::PerfMonCpu::StartMonitor(miGameStatePrePM);
    mbIsUpdating = true;
    CGS_ASSERT(lpInputBufferStack != 0, CgsDev::Assert::MessageAt("lpInputBufferStack != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1128));
    CGS_ASSERT(lpOutputBufferStack != 0, CgsDev::Assert::MessageAt("lpOutputBufferStack != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1129));
    CGS_ASSERT(lpInput != 0, CgsDev::Assert::MessageAt("lpInput != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1130));
    CGS_ASSERT(lpOutput != 0, CgsDev::Assert::MessageAt("lpOutput != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1131));
    lpInput->LockForRead();
    lpOutput->LockForWrite();
    CGS_ASSERT(lpInput->GetTimerStatusInterface() != 0,
               CgsDev::Assert::MessageAt("lpInput->GetTimerStatusInterface() != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1138));
    CGS_ASSERT(lpInput->GetTimerStatusInterface()->GetSimTimerStatus() != 0,
               CgsDev::Assert::MessageAt("lpInput->GetTimerStatusInterface()->GetSimTimerStatus() != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1139));
    mfSimTimeStep = 0.0f;
    mfSimTimeStep = lpInput->GetTimerStatusInterface()->GetSimTimerStatus()->GetCurrentTimeStep();
    // ARTIST 823A54C4 repeats the read accessor before fetching the game status.
    (void)lpInput->GetTimerStatusInterface();
    const f32 lfGameTimeStep =
        lpInput->GetTimerStatusInterface()->GetGameTimerStatus()->GetCurrentTimeStep();
    GameStateModuleIO::GameActionQueue* const lpActionQueue = lpOutput->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, CgsDev::Assert::MessageAt("lpActionQueue != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1149));

    if (mbSendSetupPlayerCarPending)
    {
        SendSetupPlayerCarEvent(lpActionQueue);
        SendSetUpAllEventStartsMessage(lpOutput);
        mbSendSetupPlayerCarPending = false;
    }
    if (mModeManager.IsOnlineGameMode())
    {
        const EActiveRaceCarIndex lePlayerIndex = GetPlayerActiveRaceCarIndex();
        lpOutput->GetGameStateToGuiInterface()->SetPlayerRaceCarIndex(lePlayerIndex);
        lpOutput->GetGameStateToGuiInterface()->AppendRaceCarCrashes(
            &mRaceCarCrashEventQueue);
    }

    CgsDev::PerfMonCpu::StartMonitor(miDriveThrusPM);
    const GameStateModuleIO::EGameModeType leDriveMode = mModeManager.GetCurrentGameModeType();
    const bool lbIsShowtiming = leDriveMode == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
        leDriveMode == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME || mfShowtimeIntroTimeLeft > 0.0f;
    const bool lbIsFreeburn = mModeManager.GetCurrentGameMode() == 0 ||
        leDriveMode == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
        leDriveMode == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
    const bool lbInviteInProgress = mGameStateInviteManager.IsInviteInProgress();
    const bool lbIsInJunkyard = mCarSelectManager.IsInJunkyard();
    const bool lbIsAnythingPaused = miSimPauseFlags != 0;
    const bool lbIsOnline = mModeManager.IsOnlineGameMode();
    const f32 lfDriveTimeStep = mfSimTimeStep;
    const BrnResource::VehicleList* const lpVehicleList = mpVehicleList;
    CgsSystem::TimerRequestInterface* const lpTimerRequests = lpOutput->GetTimerRequestInterface();
    mDriveThruManager.Update(lpActionQueue, lpTimerRequests, lfDriveTimeStep,
        &mLastActiveRaceCarInterface, lbIsOnline, lbIsFreeburn, lbIsShowtiming,
        lbIsAnythingPaused, lbIsInJunkyard, lbInviteInProgress, lpVehicleList);
    CgsDev::PerfMonCpu::StopMonitor(miDriveThrusPM);

    CgsDev::PerfMonCpu::StartMonitor(miCopyingAndResettingPM);
    CopyInputDataToPaybackManager(lpInput);
    mOnlineFlybyManager.mPlayerInputInterface = *lpInput->GetPlayerStatusInterface();
    mGameStateInviteManager.AppendFromPreWorldInputData(lpInput);
    CgsDev::PerfMonCpu::StopMonitor(miCopyingAndResettingPM);
    CheckForTailingRivals(lpOutput, &mVehicleOutputInterface, lfGameTimeStep);
    const bool lbAllowTimedTips = ShouldAllowTimedTutorialTips();
    mTrainingManager.Update(lpInput, lpActionQueue, &mLastActiveRaceCarInterface,
                              lfGameTimeStep, lbAllowTimedTips);
    mRumbleManager.Update(&mLastActiveRaceCarInterface, &mRaceCarCrashEventQueue,
                          mePlayerActiveRaceCarIndex, &mContactSpyInterface, lfGameTimeStep);

    CgsDev::PerfMonCpu::StartMonitor(miCopyingAndResettingPM);
    GameStateModuleIO::GameEventQueue lGameEvents;
    lGameEvents.MarkUnconstructed();
    lGameEvents.Construct();
    lGameEvents.Append(mGameEventCarryQueue);
    lGameEvents.Append(*lpInput->GetGameEventQueue());
    lGameEvents.Append(*mGameStateInviteManager.GetGameEventQueue());
    mGameEventCarryQueue.Clear();
    CgsDev::PerfMonCpu::StopMonitor(miCopyingAndResettingPM);
    CgsDev::PerfMonCpu::StartMonitor(miShowTimeUpdate);
    UpdateShowtimeMode(lpInput, lpOutput, &mContactSpyInterface,
                       &mLastTrafficTypeResponseQueue);
    CgsDev::PerfMonCpu::StopMonitor(miShowTimeUpdate);
    mOutputGuiEventQueue.Clear();
    ProcessGameEvents(&lGameEvents, lpActionQueue, lpInput, lpOutput);

    if (mbWaitingForStreaming && miStreamingWaitCountdown > 0 && --miStreamingWaitCountdown == 0)
    {
        // The original empty action copies one unspecified representation byte.
        GameStateModuleIO::WaitForStreamingAction lAction;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
            GameStateModuleIO::E_ACTION_WAIT_FOR_STREAMING, sizeof(lAction));
    }
    CgsDev::PerfMonCpu::StartMonitor(miCarSelectManagerUpdate);
    if (mCarSelectManager.IsInJunkyard())
        mCarSelectManager.Update(lpActionQueue, lpInput->GetControllerInput(), lfGameTimeStep);
    CgsDev::PerfMonCpu::StopMonitor(miCarSelectManagerUpdate);
    mOnlineCarSelectManager.Update(lpActionQueue, lpInput->GetControllerInput(), lfGameTimeStep);
    mImageManager.PreWorldUpdate(mePlayerActiveRaceCarIndex,
                                  &mTakedownEventQueue, lpOutput);

    CgsDev::PerfMonCpu::StartMonitor(miTakedownPM);
    mTakedownEventQueue.Clear();
    if (!lbSimPaused)
    {
        BrnPhysics::Vehicle::CrashingRaceCarInterface lCrashingCars;
        lCrashingCars.SetFromVehicleOutputInterface(&mVehicleOutputInterface);
        mTakedownManager.Update(&mLastActiveRaceCarInterface, mfSimTimeStep,
            &mRaceCarCrashEventQueue, &lCrashingCars, lpInput, lpOutput,
            &mLastTrafficTypeResponseQueue);
        const GameStateModuleIO::TakedownEventOutputQueueType* const lpTakedownEvents =
            lpOutput->GetTakedownEventOutputQueue();
        mTakedownEventQueue.Clear();
        mTakedownEventQueue.Append(*lpTakedownEvents);
        mMugshotManager.Update(lpInput, lpOutput, &mVehicleOutputInterface,
            &mTakedownEventQueue, mModeManager.GetCurrentGameModeType(),
            miSimPauseFlags != 0);
        mPaybackManager.Update(lpInput, lpOutput, &mVehicleOutputInterface,
            &mTakedownEventQueue, mModeManager.GetCurrentGameModeType());
        ProcessTakedownEvents(lpActionQueue, &mTakedownEventQueue, lpOutput);
    }
    CgsDev::PerfMonCpu::StopMonitor(miTakedownPM);
    // ARTIST r7 is active-interface+0x2861, its named all-active-cars-ready byte.
    EmmPreWorldUpdate(lpInput, lpOutput, &lGameEvents, mLastActiveRaceCarInterface.GetAllActiveCarsRead());
    const bool lbRumblePaused = miSimPauseFlags != 0;
    mRumbleManager.UpdatePauseState(lbRumblePaused, lpOutput->GetGameActionQueue());
    CgsDev::PerfMonCpu::StartMonitor(miCopyingAndResettingPM);
    CopyScoringDataToOutput(lpOutput);
    CgsDev::PerfMonCpu::StopMonitor(miCopyingAndResettingPM);

    if ((lUpdateSet & 8) != 0)
    {
        CgsDev::PerfMonCpu::StartMonitor(miTriggersPrePM);
        mTriggerQueryManager.PreWorldUpdate(lpInput, lpOutput, &mStuntManager,
            &mDriveThruManager, &mLastActiveRaceCarInterface, mpVehicleList);
        CgsDev::PerfMonCpu::StopMonitor(miTriggersPrePM);
        const bool lbInCarSelect = mCarSelectManager.IsInJunkyard() ||
            mOnlineCarSelectManager.IsInOnlineCarSelect();
        mProgressionManager.PreWorldUpdate(mfSimTimeStep, lfGameTimeStep, lpOutput,
            &mLastActiveRaceCarInterface, lUpdateSet, lbInCarSelect);
        CheckIfPlayerIsAtJunctionWithAnEvent(lpInput, lpOutput);
    }
    const bool lbDriveThruDataDirty = mProgressionManager.IsDriveThruDataDirty();
    mProgressionManager.ClearDriveThruDataDirtyFlag();
    if (lbDriveThruDataDirty)
        SendSetUpAllDriveThrusMessage(lpActionQueue);
    DetectModeStarts(lpInput, lpOutput);
    lGameEvents.Destruct();
    mGameStateInviteManager.Update(lpOutput, lpActionQueue);
    mModeManager.HandleOnlineTeamModes(lpInput, lpOutput, &mTakedownEventQueue);
    const GameStateModuleIO::EGameModeType leStuntMode = mModeManager.GetCurrentGameModeType();
    const bool lbIsAGameModeActive = mModeManager.GetCurrentGameMode() != 0 &&
        leStuntMode != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
        leStuntMode != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
    mStuntManager.Update(lpOutput->GetGameActionQueue(), &mLastActiveRaceCarInterface,
                          mfSimTimeStep, lbIsAGameModeActive);
    mRichPresenceManager.Update(lpInput, lpOutput);
    mAchievementManager.Update(lpInput, lpOutput);
    mDeveloperChallengeManager.PreWorldUpdate(lpInput, lpOutput);

    CgsDev::PerfMonCpu::StartMonitor(miStreetManagerPM);
    if ((lUpdateSet & 8) != 0)
    {
        const f32 lfWrongWayTime = mModeManager.GetScoringSystem()->GetPlayerWrongWayTime();
        const bool lbDisablesUpcoming = mModeManager.GetCurrentGameMode() != 0 &&
            mModeManager.GetCurrentGameModeParams()->GetFlag(GameModeParams::KU_FLAG_DISABLE_UPCOMING_ROAD_SIGNS);
        const bool lbUsesNavigation = mModeManager.GetCurrentGameMode() != 0 &&
            mModeManager.GetCurrentGameModeParams()->GetFlag(GameModeParams::KU_FLAG_USES_NAVIGATION);
        mStreetManager.Update(miSimPauseFlags != 0, mfSimTimeStep, lpInput, lpOutput,
            &mLastActiveRaceCarInterface, &mLastAICarOutputInterface,
            lbUsesNavigation, lbDisablesUpcoming, lfWrongWayTime);
    }
    CgsDev::PerfMonCpu::StopMonitor(miStreetManagerPM);
    lpInput->UnlockForRead();

    const bool lbNetworkOnline = IsOnlineGameMode();
    lpOutput->GetGameStateToNetworkInterface()->SetIsInOnlineGameMode(lbNetworkOnline);
    const bool lbNetworkCarSelect = mCarSelectManager.IsInJunkyard() ||
        mOnlineCarSelectManager.IsInOnlineCarSelect();
    lpOutput->GetGameStateToNetworkInterface()->SetIsCarSelect(lbNetworkCarSelect);
    const GameStateModuleIO::EGameModeType leNetworkMode = mModeManager.GetCurrentGameModeType();
    lpOutput->GetGameStateToNetworkInterface()->SetCurrentGameMode(leNetworkMode);
    lpOutput->SetControllerActive(meControllerState == E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE ||
                                  meControllerState == E_CONTROLLERSTATE_NOT_IN_GAME);
    GameStateModuleIO::ProgressionProfileDataAction lProfileData;
    lProfileData.mpProgressionProfile = mProgressionManager.GetProfile();
    lProfileData.mpProgressionData = mProgressionManager.GetProgressionData();
    lProfileData.mbAreRoadRulesAvailable = mProgressionManager.AreRoadRulesAvailable();
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lProfileData),
        GameStateModuleIO::E_ACTION_PROGRESSION_PROFILE_DATA, sizeof(lProfileData));
    lpOutput->GetGuiEventQueue()->Append(mOutputGuiEventQueue);
    GameStateModuleIO::RaceCarRaceDistanceInterface* const lpRaceCarRaceDistInterface =
        lpOutput->GetRaceCarRaceDistanceInterface();
    CGS_ASSERT(lpRaceCarRaceDistInterface != 0, "lpRaceCarRaceDistInterface != NULL");
    mModeManager.FillInRaceDistanceInterface(lpRaceCarRaceDistInterface);
    mGameStateDebugComponent.RenderLandmarkDebug();
    lpOutput->UnlockForWrite();
    mbIsUpdating = false;
    CgsDev::PerfMonCpu::StopMonitor(miGameStatePrePM);
}

void GameStateModule::EmmPreWorldUpdate(
        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
        GameStateModuleIO::OutputBuffer* lpOutput,
        const GameStateModuleIO::GameEventQueue* lpGameEvents,
        bool lbIsOnline)
{
    (void)lpGameEvents;
    CgsDev::PerfMonCpu::StartMonitor(miEmmUpdatePM);
    const bool lbSimPaused = IsSimPaused(true, false);
    GameStateModuleIO::GameActionQueue* const lpActions = lpOutput->GetGameActionQueue();

    if (mLastActiveRaceCarInterface.IsPlayerCarActive())
    {
        mPlayerCarTransform = mLastActiveRaceCarInterface.GetRaceCarState(
            GetPlayerActiveRaceCarIndex())->mTransform;
    }

    mTimerStatusInterface = *lpInput->GetTimerStatusInterface();
    const CgsSystem::Time lSimTime = mTimerStatusInterface.GetSimTimerStatus()->GetTime();
    if (lbSimPaused)
    {
        mModeManager.PausedUpdate(lpOutput);
    }
    else
    {
        CgsDev::PerfMonCpu::StartMonitor(miModeManagerPrePM);
        // 8238F100..F154 compares takedown timer+640 with -1.0f and stacks
        // that result as the tenth argument. It is not the sim-pause flag.
        const bool lbTakedownPaused = mTakedownManager.IsInTakedownCamera();
        const EGlobalRaceCarIndex leGlobal =
            static_cast<EGlobalRaceCarIndex>(GetPlayerGlobalRaceCarIndex());
        const EActiveRaceCarIndex leActive = GetPlayerActiveRaceCarIndex();
        mModeManager.PreWorldUpdate(lpOutput, lpInput, mTimerStatusInterface,
                                   leActive, leGlobal, lbIsOnline, lpActions,
                                   &mLastGlobalRaceCarInterface, &mLastActiveRaceCarInterface,
                                   lbTakedownPaused);
        CgsDev::PerfMonCpu::StopMonitor(miModeManagerPrePM);
    }
    if (!lbSimPaused)
    {
        CgsDev::PerfMonCpu::StartMonitor(miRoadRulesPM);
        UpdateRoadRulesManager(lpOutput, lpInput->GetControllerInput());
        CgsDev::PerfMonCpu::StopMonitor(miRoadRulesPM);
    }

    ScoringSystem* const lpScoring = mModeManager.GetScoringSystem();
    CgsSystem::Time lElapsed(0.0f);
    if (GetPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID)
        lElapsed = lpScoring->GetRaceCarTotalTime(GetPlayerActiveRaceCarIndex(), lSimTime);
    lpOutput->SetGameModeElapsedTime(&lElapsed);

    for (EActiveRaceCarIndex leIndex = E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT; leIndex++)
    {
        const CarData* const lpCar = lpScoring->GetCarData(leIndex);
        if (lpCar != 0)
            lpOutput->GetGameStateToNetworkInterface()->SetActiveRaceCarIndex(
                lpCar->GetNetworkPlayerID(), lpCar->GetActiveRaceCarIndex());
    }
    if (GetPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID
        && lpScoring->GetOvertakenRival(GetPlayerActiveRaceCarIndex()))
    {
        const EActiveRaceCarIndex lePlayer = GetPlayerActiveRaceCarIndex();
        const u8 luPosition = static_cast<u8>(lpScoring->GetCarRacePosition(GetPlayerActiveRaceCarIndex()));
        lpOutput->GetGameStateToGuiInterface()->AddOvertakeEvent(luPosition, lePlayer);
    }
    CgsDev::PerfMonCpu::StopMonitor(miEmmUpdatePM);
}

// ----------------------------------------------------------------------------
// GameStateModule::ClearData  (X360 0x8236B3A8; DWARF BrnGameStateModule.h:688 private, .cpp:365)
// [FX-TAILS-A 2026-09-24] It had no body on PC; its values rode as member initialisers and two
// "construct insurance" Clears. Two callers, both unconditional: Construct @0x82380388 (0x823807A8,
// right after DeveloperChallengeManager::Construct) and Prepare @0x8239E578 case 0 (0x8239E6A8).
// Every store it makes, by address (the X360 order; the stores are independent of each other):
//   0x8236B3D4 / 0x8236B3E8  std 0    +0x456D8 / +0x456E0     mActivePlayerCarId / mActivePlayerWheelId
//   0x8236B400 / 0x8236B404  stw -1   +0x32DB0 / +0x32DB4     mePlayerActiveRaceCarIndex / miPlayerGlobalRaceCarIndex
//   0x8236B410               stw -1   +0x32DAC                muNetworkGameRandomSeed
//   0x8236B418 / 0x8236B41C  stw 0    +0x32DBC / +0x32DC0     maRaceCarCrashing[0..7]
//   0x8236B420..0x8236B42C   std 0 x3, stw 0  +0x32D90..      mafRivalTailingTimes[7]
//   0x8236B430 / 0x8236B43C  RCEntity{Active,Global}RaceCarOutputInterface::Clear(+0x397E0 / +0x3C0D0)
//   0x8236B444..0x8236B478   35 x { stfs FLT_MAX (flt_82CDB9AC = 0x7F7FFFFF) ; sth 0x7FFF } at
//                            +0x441E0 + 0x140C / + 0x1498 = AICarOutputInterface::Construct inlined on
//                            mLastAICarOutputInterface (the same loop OutputBuffer::Construct inlines)
//   0x8236B484               VariableEventQueue<1536,16>::Clear(+0x3CA40)  mGameEventCarryQueue
//   0x8236B4A4               stw 0    +0x3D058  the takedown event cache's miLength (EventQueue::Clear)
//   0x8236B4AC / 0x8236B4BC  stb 0    +0x38B71 / +0x38B72     mbWaitingForStreaming / mbWaitingToPutPlayerInJunkyard
//   0x8236B4CC               std 0    +0x38B80                mCachedCarSelectChangedAction.mJunkyardId
//   0x8236B4D4               stw 0    +0x38B6C                miStreamingWaitCountdown
//   0x8236B4E0 / 0x8236B4F0  stw 0    +0x38B60 / +0x38B64     miSimPauseFlags / meControllerState
//   0x8236B4FC..0x8236B514   stb 1 x4 +0x38B73..+0x38B76      mabModuleStreamingComplete[4]
//   0x8236B534               stb 0    +0x45761                mbWasInShowtimeGameMode
//   0x8236B53C               stw 0    +0x45758  mShowtimePendingTrafficIndexStack's miLength (Stack::Clear)
//   0x8236B548               sth -1   +0x4575C                muShowtimeRequestedTrafficIndex
//   0x8236B550               stw 1    +0x45744                miShowtimePendingFrameDelay
//   0x8236B554 / 0x8236B558  stw -1   +0x456C8 / +0x456CC     muCachedJunctionLightTriggerId / muCachedJunctionLogicBoxId
//   0x8236B55C..0x8236B564   stb 0 x3 +0x456D0..+0x456D2      mbJunctionNewlyDiscovered / mbCanEnterEventAtJunction /
//                                                             mbAtJunctionWithEvent
// ----------------------------------------------------------------------------
namespace
{
    // DWARF BrnGameStateModule.cpp:154 -- the console TU's file-scope constant (this file and
    // GameStateModule_Showtime.cpp are both splits of it): ClearData's `sth r6(-1)` @0x8236B548.
    const u16 K_INVALID_VEHICLE_INDEX = 65535;
}

void GameStateModule::ClearData()
{
    mActivePlayerCarId   = 0;
    mActivePlayerWheelId = 0;

    mePlayerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    miPlayerGlobalRaceCarIndex = ::E_GLOBAL_RACE_CAR_INDEX_INVALID;
    muNetworkGameRandomSeed    = 0xFFFFFFFFu;

    for (s32 liRaceCar = 0; liRaceCar < ::E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liRaceCar)
        maRaceCarCrashing[liRaceCar] = false;
    for (u32 luRival = 0; luRival < sizeof(mafRivalTailingTimes) / sizeof(mafRivalTailingTimes[0]); ++luRival)
        mafRivalTailingTimes[luRival] = 0.0f;

    mLastActiveRaceCarInterface.Clear();
    mLastGlobalRaceCarInterface.Clear();
    mLastAICarOutputInterface.Construct();

    mGameEventCarryQueue.Clear();
    mTakedownEventQueue.Clear();

    mbWaitingForStreaming                     = false;
    mbWaitingToPutPlayerInJunkyard            = false;
    mCachedCarSelectChangedAction.mJunkyardId = 0;
    miStreamingWaitCountdown                  = 0;
    miSimPauseFlags                           = 0;
    meControllerState                         = E_CONTROLLERSTATE_NOT_IN_GAME;
    for (u32 luModule = 0; luModule < sizeof(mabModuleStreamingComplete) / sizeof(mabModuleStreamingComplete[0]); ++luModule)
        mabModuleStreamingComplete[luModule] = true;

    mbWasInShowtimeGameMode = false;
    mShowtimePendingTrafficIndexStack.Clear();
    muShowtimeRequestedTrafficIndex = K_INVALID_VEHICLE_INDEX;
    miShowtimePendingFrameDelay     = 1;

    muCachedJunctionLightTriggerId = 0xFFFFFFFFu;
    muCachedJunctionLogicBoxId     = 0xFFFFFFFFu;
    mbJunctionNewlyDiscovered      = false;
    mbCanEnterEventAtJunction      = false;
    mbAtJunctionWithEvent          = false;
}

void GameStateModule::Destruct()
{
    mActivePlayerCarId = 0;
    mActivePlayerWheelId = 0;
    mbIsUpdating = true;
    mGameStateDebugComponent.Destruct();
    mResetPlayerDebugComponent.Destruct();
    for (s32 i = 0; i < 7; ++i) mafRivalTailingTimes[i] = 0.0f;
    meControllerState = E_CONTROLLERSTATE_NOT_IN_GAME;
    mDeveloperChallengeManager.maCollectedBillboards.Clear();
    mDeveloperChallengeManager.maRoadRageTakedownTimes.Clear();
    mDeveloperChallengeManager.mpProgressionManager = 0;
    mDeveloperChallengeManager.mpStreetManager = 0;
    mDeveloperChallengeManager.mpScoringSystem = 0;
    mDeveloperChallengeManager.mpGameStateModule = 0;
    mImageManager.Destruct();
    mStreetManager.Destruct();
    mRichPresenceManager.Destruct();
    mLastTrafficTypeResponseQueue.Clear();
    mGameEventCarryQueue.Destruct();
    mReceiverQueue.Clear();
    mProgressionManager.mpModeManager = 0;
    mNetworkRoundManager.Destruct();
    mOfflineFlybyManager.mStatShownMessages.UnSetAll();
    mModeManager.Destruct();
    mGameStateInviteManager.Destruct();
    mPaybackManager.Destruct();
    mMugshotManager.Destruct();
    mTriggerQueryManager.mTriggerQueryManagerDebugComponent.Destruct();
    mDriveThruManager.Destruct();
    mAchievementManager.mAchievementsEarnt.UnSetAll();
    mAchievementManager.mAchievementsToWrite.UnSetAll();
    mAchievementManager.miUserIndex = -1;
    mAchievementManager.miState = 0;
    mAchievementManager.mhEnumerator = 0;
    mAchievementManager.mOverLapped.Construct();
    CgsModule::ModuleSingleBuffered::Destruct();
    mbIsUpdating = false;
}

// ARTIST823756A8. Each asynchronous release resumes at the stage that suspended.
bool GameStateModule::Release(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    mbIsUpdating = true;
    lpOutputBuffer->LockForWrite();
    bool lbReleased = false;
    switch (meReleaseStage)
    {
    case E_RELEASESTAGE_START:
    case E_RELEASESTAGE_DEVELOPER_CHALLENGE_MANAGER:
        meReleaseStage = E_RELEASESTAGE_DEVELOPER_CHALLENGE_MANAGER;
        mDeveloperChallengeManager.maCollectedBillboards.Clear();
        mDeveloperChallengeManager.maRoadRageTakedownTimes.Clear();
        mDeveloperChallengeManager.maCompletedChallenges.UnSetAll();
        mDeveloperChallengeManager.maTakenDownRaceCars.UnSetAll();
        // fall through
    case E_RELEASESTAGE_IMAGE_MANAGER:
        meReleaseStage = E_RELEASESTAGE_IMAGE_MANAGER;
        if (!mImageManager.Release()) break;
        // fall through
    case E_RELEASESTAGE_STREET_MANAGER:
        meReleaseStage = E_RELEASESTAGE_STREET_MANAGER;
        // ARTIST calls the folded 82C296C8 leaf, which returns true without stores.
        // fall through
    case E_RELEASESTAGE_ACHIEVEMENT_MANAGER:
        meReleaseStage = E_RELEASESTAGE_ACHIEVEMENT_MANAGER;
        if (!mAchievementManager.Release()) break;
        // fall through
    case E_RELEASESTAGE_RICH_PRESENCE:
        meReleaseStage = E_RELEASESTAGE_RICH_PRESENCE;
        mRichPresenceManager.meOldRichPresenceStatus = static_cast<RichPresenceManagerBase::ERichPresenceStates>(0);
        mRichPresenceManager.miUserID = -1;
        mRichPresenceManager.meLobbyType = static_cast<GameStateModuleIO::EGameModeType>(18);
        mRichPresenceManager.meSetLobbyType = static_cast<GameStateModuleIO::EGameModeType>(18);
        mRichPresenceManager.miCurrentRound = -1;
        mRichPresenceManager.miTotalRounds = -1;
        mRichPresenceManager.mbPlayerIsInLobby = false;
        mRichPresenceManager.meIsRanked = RichPresenceManagerBase::E_GAME_COUNT;
        mRichPresenceManager.meOldRankedStatus = RichPresenceManagerBase::E_GAME_COUNT;
        // fall through
    case E_RELEASESTAGE_PROGRESSION:
        meReleaseStage = E_RELEASESTAGE_PROGRESSION;
        mProgressionManager.mpModeManager = 0;
        // fall through
    case E_RELEASESTAGE_NETWORKROUNDMANAGER:
        meReleaseStage = E_RELEASESTAGE_NETWORKROUNDMANAGER;
        // fall through
    case E_RELEASESTAGE_FLYBYMANAGER:
        meReleaseStage = E_RELEASESTAGE_FLYBYMANAGER;
        // fall through
    case E_RELEASESTAGE_INVITEMANAGER:
        meReleaseStage = E_RELEASESTAGE_INVITEMANAGER;
        mGameStateInviteManager.mGameEventQueue.Release();
        mGameStateInviteManager.mModulePreparedBitArray.UnSetAll();
        // fall through
    case E_RELEASESTAGE_PAYBACKMANAGER:
        mPaybackManager.mDirtyTrickOutputQueue.Clear();
        meReleaseStage = E_RELEASESTAGE_PAYBACKMANAGER;
        // fall through
    case E_RELEASESTAGE_MUGSHOTMANAGER:
        meReleaseStage = E_RELEASESTAGE_MUGSHOTMANAGER;
        // fall through
    case E_RELEASESTAGE_TAKEDOWNMANAGER:
        meReleaseStage = E_RELEASESTAGE_TAKEDOWNMANAGER;
        // fall through
    case E_RELEASESTAGE_MODEMANAGER:
        meReleaseStage = E_RELEASESTAGE_MODEMANAGER;
        if (!mModeManager.Release()) break;
        // fall through
    case E_RELEASESTAGE_MANAGER:
        meReleaseStage = E_RELEASESTAGE_MANAGER;
        if (!CgsModule::ModuleSingleBuffered::Release()) break;
        // fall through
    case E_RELEASESTAGE_DONE:
        lbReleased = true;
        mDriveThruManager.maDriveThroughClosed.UnSetAll();
        meReleaseStage = E_RELEASESTAGE_MANAGER;
        mePrepareStage = E_PREPARESTAGE_START;
        break;
    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid Stage\n", KAC_GSM_FILE, 1092);
        CgsDev::Assert::EndAssert();
        break;
    }
    lpOutputBuffer->UnlockForWrite();
    mbIsUpdating = false;
    return lbReleased;
}

// ----------------------------------------------------------------------------
// ⭐ X360 0x8239E578 -- GameStateModule::Prepare (vtable +64), the module's FIRST-pass prepare.
//
// THE SHAPE (console, statement for statement):
//     mbIsUpdating = true;                       // *(this + 292289) = 1
//     LockForWrite(lpOutputBuffer);
//     switch (mePrepareStage) { ...28 stage values, 0..27, falling into the next on success... }
//     UnlockForWrite(lpOutputBuffer);
//     mbIsUpdating = false;
//     return <true only after stage 27>;
// Every stage that is still waiting for a resource reply breaks straight to the unlock tail and
// returns false, so the caller pumps it once per frame.
//
// THE CALLER, and why this exists at all: BrnGameModule::GamePrepare @0x823EFBD0 stage 4 does
//     CreateIOBuffer<GameStateModuleIO::OutputBuffer>(updateOutStack, &out, "GameState");
//     prepared = mGameStateModule.Prepare(out, updateOutStack, gameDataOut->GetAllocatorList());
//     if (!prepared) { LockForRead(out);
//                      gameDataIn->AppendRequestInterface<3072>(*out->Get());
//                      UnlockForRead(out); }
// -- i.e. the requests the stages below stage onto the output buffer's +0x3414
// RequestInterface<3072> leave through that append and are serviced by the GameData pump.
// (LoadingScriptedState::LoadGameState2 @0x823EF4D8 is the same bracket for the SECOND pass,
// GameStateModule::Prepare2 @0x8239ED10 -- Progression + Street. Not this wave.)
//
// Complete 28-stage map (original order):
//   0  START                    ClearData @0x8236B3A8 + DebugComponent::Register x2
//                               (this+208544 / this+208376)
//   1  MANAGER                  host new-style flag, then ModuleSingleBuffered::Prepare
//   2  MODE_DATA_ACQUIRING      pass-through on the console too (it only sets the stage word)
//   3  LOAD_TRIGGER_DATA        TriggerQueryManager::Prepare(this+42320, out, &rq)
//   4  STUNT_MANAGER            StuntManager::Prepare(this+183952, out)
//   5/6 CHALLENGE_LIST          RequestInterface<3072>::GetFreeburnChallengeList(&rq, 0)
//                               -> reply type 53, mpChallengeList = reply.mHandle.mpResourceMemory
//   7/8 VEHICLE_LIST            GetVehicleList(&rq, 0) -> reply type 52, mpVehicleList = ...,
//                               then ProgressionManager::ApplyVehicleList + ModeManager::ApplyVehicleList
//   9/10 WHEEL_LIST             GetWheelList(&rq, 0) -> reply type 59, mpWheelList = ...
//   11/12 PLAYERCARCOLOURS      inline AcquireResourceRequest{&rq, 0, pool 5,
//                               HashString("CarColours")} -> CreateFromHandle(this+284400)
//   13 MODEMANAGER              ModeManager::Prepare(this+4128, mpChallengeList,
//                               allocatorList->GetHeapAllocator(0x1B))
//   14 TAKEDOWNMANAGER          TakedownManager::Prepare(this+568)
//   15 MUGSHOTMANAGER           (pass-through)
//   16 PAYBACKMANAGER           *(this+1448) = 0
//   17 INVITEMANAGER            VariableEventQueue<1536,16>::Prepare/Clear(this+2032)
//   18 FLYBYMANAGER             GameStateModuleIO::FlybyData::Prepare(this+186608)
//   19 NETWORKROUNDMANAGER      mReceiverQueue.Clear()
//   20 PROGRESSION              ProgressionManager::Prepare(this+47920)
//   21 RICH_PRESENCE            RichPresenceManagerBase::Prepare()
//   22 ACHIEVEMENT_MANAGER      AchievementManagerX360::Prepare(this+181680)
//   23 STREET_MANAGER           StreetManager::Prepare(this+284520, out, &rq)
//   24 IMAGE_MANAGER            GameStateImageManagerBase::Prepare(this+185520, heapAlloc 0x1B)
//   25 RUMBLE_MANAGER           RumbleManager::Prepare(this+46680)
//   26 DEVELOPER_CHALLENGE      clear completed-challenge, takedown-time, billboard and car sets
//   27 ORIGINAL_DONE            DriveThruManager::Prepare, publish vehicle/wheel lists, re-arm
//                               Prepare at stage 1 and reset the release stage.
// The full body below runs each owner operation and each resource wait. ARTIST
// Construct sets mbIsNewModule at 82380944 after base Construct clears it; stage 1
// only selects its stage and calls base Prepare (8239E6C4..CC), with no flag rewrite.
// ----------------------------------------------------------------------------
// The GameData reply ids the two live list stages match (BrnGameDataModule's dispatch stages
// them at the slot: ProcessGetVehicleListRequest -> 52, ProcessGetWheelListRequest -> 59).
// The FreeburnChallengeList reply (53) is live too now: GameDataModule answers it from
// ProcessGetFreeburnChallengeListRequest once its own Prepare stage 10 has built the list.
static const s32 KI_REPLY_VEHICLE_LIST = 52;
static const s32 KI_REPLY_WHEEL_LIST   = 59;
static const s32 KI_REPLY_FREEBURN_CHALLENGE_LIST = 53;
// Stage 13's heap: the allocator list's bank 0x1B, the network heap (BrnNetworkModule's
// KI_NETWORK_HEAP_BANK is the same bank).
static const s32 KI_MODE_MANAGER_HEAP_BANK = 0x1B;

// ✅ [gateui] Stage 4's district-map bundle literals moved OUT of this file (2026-08-20): the
// LoadBundle they served now happens where the console has it, inside StuntManager::LoadDistrictMap
// @0x82399458, so KPC_DISTRICT_MAP_BUNDLE_NAME / _RESOURCE_NAME / KI_DISTRICT_MAP_EVENT_ID /
// KI_DISTRICT_MAP_POOL_ID live in BrnStuntManager.cpp alongside it.

static const BrnResource::GameDataIO::GameDataAssetEvent* GetListReply(
    CgsModule::BaseEventReceiverQueue& lrQueue, s32 liExpectedType, s32 liTypeLine, s32 liEventLine)
{
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        const s32 liType = lrQueue.GetFirstEvent(&lpEvent, &liSize);
        if (liType != liExpectedType)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Invalid event id received\n", KAC_GSM_FILE, liTypeLine);
            CgsDev::Assert::EndAssert();
        }
        const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
            static_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);
        if (lpReply->miEventId != 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Invalid event id received\n", KAC_GSM_FILE, liEventLine);
            CgsDev::Assert::EndAssert();
        }
        return lpReply;
}


// ARTIST 0x8239E578..0x8239ED10. The original 28-case preparation ladder.
// BrnGameModule::GamePrepare calls this body; ResourceUpdateThread services the GameData requests
// and posts replies to the module-owned receiver queue between passes.
bool GameStateModule::Prepare(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                              CgsModule::IOBufferStack* lpUpdateOutputBufferStack,
                              const BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
{
    (void)lpUpdateOutputBufferStack; // ARTIST never reads r5.
    bool lbDone = false;
    mbIsUpdating = true;
    lpOutputBuffer->LockForWrite();

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        ClearData();
        mResetPlayerDebugComponent.Register();
        mGameStateDebugComponent.Register();
        // fall through
    case E_PREPARESTAGE_MANAGER:
        mePrepareStage = E_PREPARESTAGE_MANAGER;
        if (!CgsModule::ModuleSingleBuffered::Prepare())
            break;
        // fall through
    case E_PREPARESTAGE_MODE_DATA_ACQUIRING:
        mePrepareStage = E_PREPARESTAGE_LOAD_TRIGGER_DATA;
        // fall through
    case E_PREPARESTAGE_LOAD_TRIGGER_DATA:
        if (!mTriggerQueryManager.Prepare(lpOutputBuffer, &mReceiverQueue))
            break;
        mePrepareStage = E_PREPARESTAGE_STUNT_MANAGER;
        // fall through
    case E_PREPARESTAGE_STUNT_MANAGER:
        if (!mStuntManager.Prepare(lpOutputBuffer))
            break;
        // fall through
    case E_PREPARESTAGE_REQUEST_CHALLENGE_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_CHALLENGE_LIST;
        lpOutputBuffer->GetResourceRequestInterface()->GetFreeburnChallengeList(&mReceiverQueue, 0);
        mReceiverQueue.Clear();
        // fall through
    case E_PREPARESTAGE_RECEIVE_CHALLENGE_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_CHALLENGE_LIST;
        if (mReceiverQueue.GetLength() < 1)
            break;
        mpFreeburnChallengeList = static_cast<const BrnResource::GameDataIO::GetFreeburnChallengeListResponse*>(GetListReply(mReceiverQueue, KI_REPLY_FREEBURN_CHALLENGE_LIST, 516, 521))->GetFreeburnChallengeListPtr();
        // fall through
    case E_PREPARESTAGE_REQUEST_VEHICLE_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_VEHICLE_LIST;
        lpOutputBuffer->GetResourceRequestInterface()->GetVehicleList(&mReceiverQueue, 0);
        mReceiverQueue.Clear();
        // fall through
    case E_PREPARESTAGE_RECEIVE_VEHICLE_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_VEHICLE_LIST;
        if (mReceiverQueue.GetLength() < 1)
            break;
        mpVehicleList = const_cast<BrnResource::VehicleList*>(static_cast<const BrnResource::GameDataIO::GetVehicleListResponse*>(GetListReply(mReceiverQueue, KI_REPLY_VEHICLE_LIST, 556, 561))->GetVehicleListPtr());
        mProgressionManager.ApplyVehicleList(mpVehicleList);
        mModeManager.ApplyVehicleList(mpVehicleList);
        // fall through
    case E_PREPARESTAGE_REQUEST_WHEEL_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_WHEEL_LIST;
        lpOutputBuffer->GetResourceRequestInterface()->GetWheelList(&mReceiverQueue, 0);
        mReceiverQueue.Clear();
        // fall through
    case E_PREPARESTAGE_RECEIVE_WHEEL_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_WHEEL_LIST;
        if (mReceiverQueue.GetLength() < 1)
            break;
        mpWheelList = const_cast<BrnResource::WheelList*>(static_cast<const BrnResource::GameDataIO::GetWheelListResponse*>(GetListReply(mReceiverQueue, KI_REPLY_WHEEL_LIST, 599, 604))->GetWheelListPtr());
        // fall through
    case E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS:
        mePrepareStage = E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS;
        lpOutputBuffer->GetResourceRequestInterface()->AcquireResource(&mReceiverQueue, 0, 5, "CarColours");
        mReceiverQueue.Clear();
        // fall through
    case E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS:
    {
        mePrepareStage = E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS;
        if (mReceiverQueue.GetLength() < 1)
            break;
        const CgsModule::Event* lpEvent = 0;
        s32 liSize = 0;
        mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);
        const CgsResource::Events::AcquireResourceResponse* lpAcquire =
            reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);
        CgsResource::ResourceHandle lHandle;
        lHandle.mpResourceMemory = lpAcquire->mpResourceMemory;
        lHandle.mpSourceEntry = lpAcquire->mpSourceEntry;
        mpPlayerCarColours = lHandle;
    }
        // fall through
    case E_PREPARESTAGE_MODEMANAGER:
        mePrepareStage = E_PREPARESTAGE_MODEMANAGER;
        if (!mModeManager.Prepare(mpFreeburnChallengeList,
                                  lpAllocatorList->GetHeapAllocator(KI_MODE_MANAGER_HEAP_BANK)))
            break;
        // fall through
    case E_PREPARESTAGE_TAKEDOWNMANAGER:
        mePrepareStage = E_PREPARESTAGE_TAKEDOWNMANAGER;
        if (!mTakedownManager.Prepare())
            break;
        // fall through
    case E_PREPARESTAGE_MUGSHOTMANAGER:
        mePrepareStage = E_PREPARESTAGE_MUGSHOTMANAGER;
        // ARTIST has no provider work in this case.
        // fall through
    case E_PREPARESTAGE_PAYBACKMANAGER:
        mPaybackManager.Prepare();
        mePrepareStage = E_PREPARESTAGE_PAYBACKMANAGER;
        // fall through
    case E_PREPARESTAGE_INVITEMANAGER:
        mePrepareStage = E_PREPARESTAGE_INVITEMANAGER;
        mGameStateInviteManager.Prepare();
        // fall through
    case E_PREPARESTAGE_FLYBYMANAGER:
        mePrepareStage = E_PREPARESTAGE_FLYBYMANAGER;
        mOfflineFlybyManager.Prepare();
        mOnlineFlybyManager.Prepare();
        // fall through
    case E_PREPARESTAGE_NETWORKROUNDMANAGER:
        mePrepareStage = E_PREPARESTAGE_NETWORKROUNDMANAGER;
        mReceiverQueue.Clear();
        // fall through
    case E_PREPARESTAGE_PROGRESSION:
        mePrepareStage = E_PREPARESTAGE_PROGRESSION;
        if (!mProgressionManager.Prepare(lpOutputBuffer, &mReceiverQueue))
            break;
        // fall through
    case E_PREPARESTAGE_RICH_PRESENCE:
        mePrepareStage = E_PREPARESTAGE_RICH_PRESENCE;
        mRichPresenceManager.miNumberPlayers = 0;
        mRichPresenceManager.miLobbyContextEnabled = -1;
        if (!mRichPresenceManager.RichPresenceManagerBase::Prepare())
            break;
        // fall through
    case E_PREPARESTAGE_ACHIEVEMENT_MANAGER:
        mePrepareStage = E_PREPARESTAGE_ACHIEVEMENT_MANAGER;
        if (!mAchievementManager.Prepare())
            break;
        // fall through
    case E_PREPARESTAGE_STREET_MANAGER:
        mePrepareStage = E_PREPARESTAGE_STREET_MANAGER;
        if (!mStreetManager.Prepare(lpOutputBuffer, &mReceiverQueue))
            break;
        // fall through
    case E_PREPARESTAGE_IMAGE_MANAGER:
        mePrepareStage = E_PREPARESTAGE_IMAGE_MANAGER;
        if (!mImageManager.Prepare(lpAllocatorList->GetHeapAllocator(KI_MODE_MANAGER_HEAP_BANK)))
            break;
        // fall through
    case E_PREPARESTAGE_RUMBLE_MANAGER:
        mePrepareStage = E_PREPARESTAGE_RUMBLE_MANAGER;
        if (!mRumbleManager.Prepare())
            break;
        // fall through
    case E_PREPARESTAGE_DEVELOPER_CHALLENGE_MANAGER:
        mePrepareStage = E_PREPARESTAGE_DEVELOPER_CHALLENGE_MANAGER;
        mDeveloperChallengeManager.maCompletedChallenges.UnSetAll();
        mDeveloperChallengeManager.maRoadRageTakedownTimes.Clear();
        mDeveloperChallengeManager.maCollectedBillboards.Clear();
        mDeveloperChallengeManager.maTakenDownRaceCars.UnSetAll();
        // fall through
    case E_PREPARESTAGE_ORIGINAL_DONE:
        mDriveThruManager.Prepare(mTriggerQueryManager.GetTriggerData(),
                                  CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>(
                                      mpPlayerCarColours.GetResourceHandle()));
        lbDone = true;
        mCarSelectManager.Prepare(mpVehicleList, mpWheelList);
        mOnlineCarSelectManager.Prepare(mpVehicleList, mpWheelList);
        mePrepareStage = E_PREPARESTAGE_MANAGER;
        meReleaseStage = E_RELEASESTAGE_START;
        break;
    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid Stage\n", KAC_GSM_FILE, 825);
        CgsDev::Assert::EndAssert();
        break;
    }

    lpOutputBuffer->UnlockForWrite();
    mbIsUpdating = false;
    return lbDone;
}

// ----------------------------------------------------------------------------
// ⭐ X360 0x8239ED10 -- GameStateModule::Prepare2, the SECOND-pass prepare.
//
// THE SHAPE (console, instruction for instruction off 0x8239ED10):
//     LockForWrite(lpOutputBuffer);
//     switch (mePrepare2Stage /* this+0x22C */) {
//       case 0: case 1:
//           mePrepare2Stage = 1;
//           if (!mProgressionManager.Prepare2(lpOutputBuffer,          // r4
//                                             &mModeManager,           // r5 == this + 0x1020
//                                             &mReceiverQueue,         // r6 == this + 0x38BC0
//                                             mTriggerQueryManager.GetTriggerData(),  // r7
//                                             achievementManager))     // r8 == this + 181680
//               break;
//           // fall through
//       case 2:
//           mePrepare2Stage = 2;
//           if (!mStreetManager.Prepare2(lpOutputBuffer, &mReceiverQueue, &mTriggerQueryManager))
//               break;
//           // fall through
//       case 3: lbDone = true; break;
//     }
//     UnlockForWrite(lpOutputBuffer);
//
// ⭐ THE PROGRESSION LEG IS REAL. This is what the whole junkyard -> car-select handover was
// missing: ProgressionManager::LoadProgressionData @0x82399ED0 is the ONLY writer of
// mpProgressionData in the entire image, and nothing on PC had ever driven it, so
// GetProgressionData() answered NULL and OnPlayerCarChange fired the console's own
// "lpProgressionData != NULL" assert at BrnGameStateModule.cpp:4636.
//
// ⭐ BOTH LEGS ARE WIRED NOW (2026-08-11) -- the two "honest deviations" this banner used to
// list are paid off:
//   * the ACHIEVEMENT MANAGER (X360 this+181680) is a REAL embedded AchievementManagerX360
//     subobject (BrnGameStateModule.h, DWARF :226), constructed by this module's Construct with
//     the console's own four arguments. `&mAchievementManager` is passed as r8, so
//     ProgressionManager::Prepare2's `lpAchievementManager` assert
//     (BrnProgressionManager.cpp:265) no longer fires every boot.
//   * the STREET MANAGER (X360 this+284520) is a REAL embedded StreetManager subobject
//     (DWARF :425), and case 2 now drives the console's own STREETDATA.DAT load.
//
// ⭐ THE STREET PARK IS GONE (2026-08-11, SetupParRivals wave). Case 2 below is now the single
// console call `mStreetManager.Prepare2(out, &mReceiverQueue, &mTriggerQueryManager)`
// (X360 0x823509D8 == `if (LoadStreetData(out, rq)) { SetupParRivals(tqm); return 1; }`) --
// both halves, no stand-in. See the case body for what closed it.
// ----------------------------------------------------------------------------
bool GameStateModule::Prepare2(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    if (lpOutputBuffer == 0)
    {
        CGS_ASSERT(false, "lpOutputBuffer");
        return false;
    }

    lpOutputBuffer->LockForWrite();

    bool lbDone = false;

    switch (mePrepare2Stage)
    {
    case E_PREPARE2STAGE_START:
    case E_PREPARE2STAGE_PROGRESSION:
    {
        mePrepare2Stage = E_PREPARE2STAGE_PROGRESSION;

        // X360 `BrnTrigger::TriggerData_::GetMemor(this + 43888)` -- the trigger RESOURCE MEMORY
        // pointer, which is exactly what GetTriggerData() hands back. The manager stores it as an
        // opaque back-pointer (its own member is typed void*), hence the const strip.
        void* lpTriggerData =
            const_cast<void*>(static_cast<const void*>(mTriggerQueryManager.GetTriggerData()));

        // X360 r8 == `a1 + 181680` -- the embedded achievement manager, by address.
        if (!mProgressionManager.Prepare2(lpOutputBuffer,
                                          &mModeManager,
                                          &mReceiverQueue,
                                          lpTriggerData,
                                          &mAchievementManager))
        {
            break;
        }
    }
        // fall through

    case E_PREPARE2STAGE_STREET_MANAGER:
    {
        mePrepare2Stage = E_PREPARE2STAGE_STREET_MANAGER;

        // X360: `if (!StreetManager::Prepare2(this + 284520, out, this + 232384, this + 42320))
        //           break;` -- i.e. mStreetManager.Prepare2(out, &mReceiverQueue,
        // &mTriggerQueryManager). StreetManager::Prepare2 @0x823509D8 is exactly two things:
        //     if (LoadStreetData(this, out, rq)) { SetupParRivals(this, tqm); return 1; }
        //     return 0;
        //
        // ⭐ THE LOAD HALF IS REAL. LoadStreetData @0x8234F630 is the only writer of mpStreetData
        // in the whole image (LoadBundle "STREETDATA.DAT" -> acquire "StreetData" -> bind), so
        // this is what makes StreetManager::GetStreetData() answer non-null at all.
        //
        // ⭐ THE DATA GAP IS CLOSED (2026-08-11, district-map wave). What this park used to say
        // -- "the district map is NULL for two independent reasons" -- is no longer true:
        //   (a) Prepare stage 23 (E_PREPARESTAGE_STREET_MANAGER) now pumps
        //       StreetManager::Prepare @0x82350900, so LoadDistrictMap @0x8234FB98 runs, and
        //       stage 4 makes the DISTRICTS.DAT bundle resident first (its acquire needs that --
        //       the machine only ACQUIRES "Districts", it never loads the bundle); and
        //   (b) LoadDistrictMap's handle bind is REAL now -- it reads the acquire response's
        //       {mpResourceMemory, mpSourceEntry} pair BY MEMBER (X360 payload+0x18).
        // DISTRICTS.DAT ships in build/game and is a correct platform-4 bundle: bnd2 v2,
        // platform 4, ONE resource, id 0x68E318DC == HashString("Districts"), type 0x30 ==
        // WorldPainter2D (registered in CgsResourceTypeRegistration.cpp; its FixUp is
        // BinaryFileResourceType::FixUp, a genuine no-op -- a binary blob has nothing to
        // relocate), payload {mu32DataSize = 0x10010, mu32DataOffset = 0x10} over a 256x256
        // district grid.
        //
        // ⭐ THE BODY GAP IS CLOSED (2026-08-11, SetupParRivals wave). What this park used to
        // list -- four symbols SetupParRivals could not link against -- is now all homed, so the
        // console's own single call is made below:
        //   * BrnStreetData::Road::GetRoadLimitId0()          -> header inline, BrnStreetData.h
        //     (asm: the `ld r10, 0x18(r23)` inside SetupParRivals itself, 0x8233F758).
        //   * BrnProgression::ProgressionData::GetRival(s32)  -> BrnProgressionData.cpp
        //     (asm: `lwz 0x28 / add stride 0x38` + the BrnProgressionData.h:460 bounds assert,
        //     attested identically at 0x823363F8 and 0x8233F828).
        //   * CgsNumeric::Random::RandomInt(s32, s32)         -> CgsRandom.cpp
        //     (asm: the CgsRandom.h:320/:323 assert pair + `divwu` reduction of the PRE-step
        //     seed's high word, read in full out of Shuffle<u16> @0x8271B420, which is the one
        //     expansion with a runtime liMin).
        //   * StreetManager::FindRivalsByDistrict             -> its own split TU
        //     (BrnGameStateStreetManager_FindRivalsByDistrict.cpp; the _wC_04 partfile that used
        //     to be its only home costs four other symbols, so the one function was split out
        //     per the _Prepare.cpp precedent -- as were SetupParRivals and Prepare2 themselves).
        // Net measured cost of the three split TUs + the three accessor bodies: ZERO new
        // unresolved externals (cl /c with the build's flags + dumpbin /SYMBOLS against the
        // defined-symbol set of build\game\obj).
        //
        // ⚠️ AND THE DISTRICT MAP IS ACTUALLY BOUND NOW. The previous boot printed
        // `[StreetManager] district map: handle=0` with DISTRICTS.DAT demonstrably resident;
        // the cause was in StreetManager::LoadDistrictMap, not in this stage machine -- its
        // acquire tagged the resource id with the pool id (a Hex-Rays store-fusion artifact) and
        // posted the request at the console's 32-bit size. Both are fixed in
        // BrnGameStateStreetManager_wB_01.cpp, which carries the full evidence. SetupParRivals
        // additionally carries a documented PC-only guard so a future acquire failure names
        // itself instead of null-dereferencing here.
        if (!mStreetManager.Prepare2(lpOutputBuffer, &mReceiverQueue, &mTriggerQueryManager))
        {
            break;
        }
    }
        // fall through

    case E_PREPARE2STAGE_DONE:
        // The console does NOT write 3 into the stage word here (case 3 is only `li r28, 1`), so
        // a later re-entry re-runs the street leg -- which is idempotent once loaded. Reproduced:
        // no store.
        //
        // [PC bring-up observer, 2026-08-27 -- NOT an X360 store, and deliberately NOT the stage
        // word.] Latch that BOTH second-pass bundles ("Progression.dat" and "STREETDATA.DAT")
        // are now resident in pool 5. The GUI lane's own second-pass machine,
        // BrnGui::WorldDataController::Prepare2, ACQUIRES those two resources BY NAME out of the
        // same pool -- and an acquire for an absent resource is ANSWERED (with a null memory
        // pointer), not queued, so running it early does not retry, it binds nothing and latches
        // its terminal state for the rest of the session. On the console the module scheduler's
        // Prepare2 pass orders the two lanes; on PC the GUI lane is driven from
        // BrnGameModule::ResourceUpdateThread, which starts long before this flow state runs, so
        // it needs this signal to hold off. DELETE-WHEN the module scheduler's real Prepare2 pass
        // orders the two lanes.
        lbDone = true;
        break;

    default:
        // The console's jump table sends anything > 3 straight to the unlock tail with lbDone
        // still false -- there is no assert on this switch.
        break;
    }

    lpOutputBuffer->UnlockForWrite();
    return lbDone;
}

// ----------------------------------------------------------------------------
// ⭐ X360 0x8236BAC8 -- FindNearestJunkyardID.
//
// Linear scan of the track TriggerData's generic-region table for the E_TYPE_JUNK_YARD region
// whose box centre is nearest lPosition; returns that region's CgsID. The console:
//
//     td    = mTriggerQueryManager.GetTriggerData();        // this + 0xAB70 (43888)
//     count = td->miGenericRegionCount;                     // td + 0x48
//     best  = flt_82029B70;  id = kCGSID_NULL;
//     for (i = 0; i < count; ++i) {
//         assert(i < td->miGenericRegionCount);              // BrnTriggerData.h:495, inlined
//         r = td->mpGenericRegions + i * 0x38;               // td + 0x44, stride == sizeof
//         if (r->meType != 0) continue;                      // lbz +0x36; 0 == E_TYPE_JUNK_YARD
//         d = length(BoxRegion.position - lPosition);        // lfs +0x00/+0x04/+0x08
//         if (d < best) { best = d; id = (s64)(s32)r->mId; } // lwz +0x24, extsw
//     }
//     assert(id != kCGSID_NULL);                             // BrnGameStateModule.cpp:6723
//     return id;
//
// TWO MEASURED CONSTANTS, not guesses:
//   * flt_82029B70 == 0x7F7FFFFF == FLT_MAX (read out of .rdata with headless IDA, not inferred
//     from the idiom -- the brief's rule about guessed rodata literals).
//   * the 0x38 stride is exactly sizeof(GenericRegion) here too (36-byte BoxRegion + 8 + 12),
//     which is the check that our x64 GenericRegion did NOT drift from the console's.
//
// The X360 computes the TRUE distance, not the squared one: vmsum3fp128 gives the dot product and
// the two vnmsubfp/vmaddfp pairs are a Newton-refined rsqrt, with a vcmpeqfp/vsel guarding the
// zero-length case. Ordering is identical either way; the sqrt is kept so the value is the
// console's value.
// ----------------------------------------------------------------------------
CgsID GameStateModule::FindNearestJunkyardID(Vector3 lPosition) const
{
    const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
    const s32 liGenericRegionCount =
        (lpTriggerData != 0) ? lpTriggerData->GetGenericRegionCount() : 0;

    CgsID lJunkyardId = 0;
    f32   lfNearest   = 3.402823466e+38f;   // flt_82029B70 == FLT_MAX

    for (s32 li = 0; li < liGenericRegionCount; ++li)
    {
        const BrnTrigger::GenericRegion* lpRegion = lpTriggerData->GetGenericRegion(li);
        if (lpRegion->GetType() != BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD)
            continue;

        const Vector3 lRegionPosition = lpRegion->GetBoxRegion()->GetPosition();
        const f32 lfDeltaX = lRegionPosition.mV.x - lPosition.mV.x;
        const f32 lfDeltaY = lRegionPosition.mV.y - lPosition.mV.y;
        const f32 lfDeltaZ = lRegionPosition.mV.z - lPosition.mV.z;
        const f32 lfSqDistance =
            lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;
        const f32 lfDistance = (lfSqDistance != 0.0f) ? std::sqrt(lfSqDistance) : 0.0f;

        if (lfDistance < lfNearest)
        {
            lfNearest   = lfDistance;
            lJunkyardId = lpRegion->GetId();
        }
    }

    CGS_ASSERT(lJunkyardId != 0, CgsDev::Assert::MessageAt("lJunkyardId != kCGSID_NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6723));
    return lJunkyardId;
}

// X360 @ 0x823116D0. Returns whether the currently-running game mode is one of the online modes. May
// only be called while the module is updating (asserts mbIsUpdating). Fetches the current game mode
// from the embedded ModeManager and forwards to GameMode::IsOnline(); if there is no current mode,
// returns false. (X360 reads the current-mode pointer inline as *(this + 0x1DB8) inside mModeManager
// and its mbIsOnline at *(mode + 172); de-inlined to the two logical calls.)
bool GameStateModule::IsOnlineGameMode()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");

    const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
    if (lpCurrentGameMode != 0)
    {
        return lpCurrentGameMode->IsOnline();
    }
    return false;
}

// The cached current-game-mode type. The X360 reads it as the raw scalar just below the embedded
// mModeManager (GameStateModule+7604 == mModeManager+0xD94, meCurrentGameModeType); de-inlined to
// the ModeManager's own named accessor -- same read, no offset poke.
GameStateModuleIO::EGameModeType GameStateModule::GetCurrentGameModeType() const
{
    return mModeManager.GetCurrentGameModeType();
}

// ⭐ REAL (2026-08-11). The embedded achievement manager (X360 this+181680). Every console reader
// spells it as an inline `this + 181680` pointer adjust rather than a call -- BurnoutSkillzManager::
// Construct @0x82332688 (`*(a1 + 124) = *(a2 + 27992) + 181680`), Prepare2 @0x8239ED10 (r8),
// ProcessGameEvents @0x823A0A18, ProcessTakedownEvents @0x8238FC50, PostWorldUpdate @0x8238F358,
// UpdateShowtimeMode @0x82380EF8 and CheckForAllEventsBeingFound @0x82382460 -- so there is no
// out-of-line X360 symbol to cite for the accessor itself; it is the de-inlined form of that adjust.
AchievementManagerBase* GameStateModule::GetAchievementManager()
{
    return &mAchievementManager;
}

// X360 @ 0x82311620. Returns the player's GLOBAL race-car index (its slot in the full world race-car
// table). May only be called while the module is updating (asserts mbIsUpdating).
s32 GameStateModule::GetPlayerGlobalRaceCarIndex()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    return miPlayerGlobalRaceCarIndex;
}

// X360 @ 0x82356870. Returns whether the active race car in slot leRaceCarIndex is currently crashing.
// Asserts the module is updating and that the index is in [E_ACTIVE_RACE_CAR_INDEX_0,
// E_ACTIVE_RACE_CAR_INDEX_COUNT); reproduces the three X360 asserts verbatim (the module-updating one,
// then the two range guards) before returning the cached per-slot crash flag.
bool GameStateModule::IsRaceCarCrashing(::EActiveRaceCarIndex leRaceCarIndex)
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, CgsDev::Assert::MessageAt("leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\BrnGameStateModule.h", 1038));
    CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, CgsDev::Assert::MessageAt("leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\BrnGameStateModule.h", 1039));
    return maRaceCarCrashing[leRaceCarIndex];
}

// X360 @ 0x823567A8. True when the current game mode is a showtime mode (offline or online). May only
// be called while the module is updating (asserts mbIsUpdating). The X360 reads the current game-mode
// type inline (this+7604) and tests == E_MODE_OFFLINE_SHOWTIME (2) || == E_MODE_ONLINE_SHOWTIME (16);
// de-inlined to the GetCurrentGameModeType() accessor (same read) to avoid a raw offset access.
bool GameStateModule::IsShowtimeGameMode()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");

    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    return leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
        || leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME;
}

// X360 @ 0x82356978. True when the simulation is currently paused. miSimPauseFlags is a bitfield of
// active pause reasons; a nonzero value means paused. When lbCheckGameMode is set and the current mode
// is online, some pause-reason bits are ignored: the X360 masks off bits {2,3,5} (mask 0xFFFFFFD3, the
// strict path -- returns immediately) when lbStrictMask is set, otherwise bits {1,2,3,5}
// (mask 0xFFFFFFD1). (The two bool parameters are named from the asm's branch structure; their exact
// call-site meaning is confirmed when the pause callers -- RequestPause/RequestUnpause -- are homed.)
bool GameStateModule::IsSimPaused(bool lbCheckGameMode, bool lbStrictMask) const
{
    s32 liPauseFlags = miSimPauseFlags;
    if (lbCheckGameMode)
    {
        const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        if (lpCurrentGameMode != 0 && lpCurrentGameMode->IsOnline())
        {
            if (lbStrictMask)
            {
                return (liPauseFlags & ~0x2C) != 0;   // X360 mask 0xFFFFFFD3 -- clear bits {2,3,5}
            }
            liPauseFlags &= ~0x2E;                     // X360 mask 0xFFFFFFD1 -- clear bits {1,2,3,5}
        }
    }
    return liPauseFlags != 0;
}

// X360 @ 0x823566F8. Hands back the module's per-frame output GUI event queue (the
// CgsModule::VariableEventQueue<18432,16> that the PaybackManager and other managers publish their
// HUD/GUI events onto). May only be called while the module is updating (asserts mbIsUpdating).
CgsModule::VariableEventQueue<18432, 16>* GameStateModule::GetOutputGuiEventQueue()
{
    CGS_ASSERT(mbIsUpdating, "Can not use this function unless module is updating\n");
    return &mOutputGuiEventQueue;
}

// X360 @ 0x82363698. Mark car lCarId as already-shown in the unlock sequence. The X360 resolves the
// player Profile through the embedded progression manager (GetProfile inlines to the by-value Profile
// sub-object), asserts it is non-null with the verbatim message, then forwards to
// Profile::SetCarUnlockAlreadyShown.
void GameStateModule::SetCarUnlockAlreadyShown(CgsID lCarId)
{
    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile != 0, CgsDev::Assert::MessageAt("mProgressionManager.GetProfile()", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 4823));
    lpProfile->SetCarUnlockAlreadyShown(lCarId);
}

// ============================================================================================
// THE JUNKYARD / CAR-SELECT PRODUCER SURFACE (2026-08-01).
//
// Everything below is what CarSelectManager reaches for through mpGameStateModule. Each body is
// recovered from the X360 ASM, not the Hex-Rays prototype: on the X360 a CgsID is a full 64-bit
// value in ONE 64-bit GPR (`stdx r4, this, 0x456D8`), and Hex-Rays renders those register pairs
// as a single `__int64 a2`, dropping every argument after it without a trace. Three of the five
// signatures below would have been wrong if taken from the pseudocode.
// ============================================================================================

// The loaded vehicle list (X360 `lwzx rN, this, 0x456E8`).
BrnResource::VehicleList* GameStateModule::GetVehicleList()
{
    return mpVehicleList;
}

// X360 read at GameStateModule+0x456EC (284396) -- installed by Prepare's stage 9/10.
BrnResource::WheelList* GameStateModule::GetWheelList()
{
    return mpWheelList;
}

// The active player car / wheel ids the CarSelect FSM compares against its desired car
// (X360 raw reads at this+0x456D8 / +0x456E0; both are written by OnSpecialEventPlayerCarChange).
CgsID GameStateModule::GetActivePlayerWheelId() const
{
    return mActivePlayerWheelId;
}

// The module's cached active-race-car snapshot (X360 embedded interface at this+0x397E0).
const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
    GameStateModule::GetLastActiveRaceCarInterface() const
{
    return &mLastActiveRaceCarInterface;
}

// ⭐ [stuntrace waveB mount closure] GetDistrictMap -- X360 read at this+245904 (0x3C090).
// The world district map the checkpoint-district labeller and ProcessGameEvents' region lookup
// sample. NOT a member of its own: 245904 lands INSIDE mLastActiveRaceCarInterface (base +235488,
// span 10480) at interface+10416, which is that interface's embedded mWorldMap2D -- the member
// immediately before mbPlayerWrecked, whose own console store pins it at interface+0x28E0, 48
// bytes (sizeof(WorldMap2D) at Vector2 alignment) later. So the console's adjust is exactly
// &mLastActiveRaceCarInterface.mWorldMap2D, reached here through that interface's own accessor.
//
// Console attestations of the adjust (both dumped 2026-08-26):
//     ProcessGameEvents @0x823A3700       addis r3, r31, 4 / addi r3, r3, -0x3F70 -> WorldMap2D::GetValue
//     SetupCheckpointDistricts @0x82329740  the identical pair -> WorldMap2D::GetValue @0x82329830
//
// The declaration is non-const and hands back a mutable pointer (the ResetPlayerDebugComponent
// grow shape, BrnGameStateModule.h:773); the interface only publishes the const accessor, so the
// const is cast off here rather than adding a second accessor to that header. The DWARF's own
// name for this surface is GetWorldMap2D() const (BrnGameStateModule.h:1331) -- worth renaming to
// one day, but the tree's callers (BrnResetPlayerDebugComponent.cpp:365,
// BrnModeManager_CheckpointSetup.cpp:568) are all on GetDistrictMap, so the rename is a separate
// mechanical change, not this one.
CgsWorld::WorldMap2D* GameStateModule::GetDistrictMap()
{
    return const_cast<CgsWorld::WorldMap2D*>(mLastActiveRaceCarInterface.GetWorldMap2D());
}

// ⭐ [stuntrace waveB agent 9] The module's cached GLOBAL race-car snapshot (X360 embedded
// interface at this+0x3C0D0 == 245968, flush against the active one above). The console reaches it
// by that raw adjust at every site -- PostWorldUpdate's 2416-byte XMemCpy, ClearData's Clear, and
// the four ModeManager readers -- so this accessor is this repo's de-inlining of the adjust and is
// exactly the adjust and nothing more. See the header banner for the offset proof.
const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface*
    GameStateModule::GetLastGlobalRaceCarInterface() const
{
    return &mLastGlobalRaceCarInterface;
}

// ⭐ [stuntrace waveB agent 9] X360 this+208300 (0x32DAC). Written by ProcessGameEvents from the
// StartNetworkGameEvent, cleared to -1 by ClearData, read by
// ModeManager::TellGuiToShowOnlineFinalStandings @0x82329B68.
u32 GameStateModule::GetNetworkRandomSeed() const
{
    return muNetworkGameRandomSeed;
}

// ⭐ The raw `*(this + 232288)` nonzero test three console call sites open-code. That word IS
// miSimPauseFlags (see the header note), so this is exactly IsSimPaused(false, false).
bool GameStateModule::IsTrainingPauseSuppressed() const
{
    return miSimPauseFlags != 0;
}

// --------------------------------------------------------------------------------------------
// RequestUnpause (X360 0x82382138).
// Clear the leUnpauseModule pause-reason bits from miSimPauseFlags. The console samples
// IsSimPaused BEFORE and AFTER the clear and only broadcasts the unpause action (87, 1B) when the
// answer actually changed; the two asserts guard the "we asked to unpause and ended up paused"
// inversions. The X360 keeps `lbWasPausedBefore = (flags != 0)` from the PRE-clear word and
// `lbPausedAfterClear = (newFlags != 0)` from the post-clear word, which is why the second assert
// can fire even on the no-change path.
// --------------------------------------------------------------------------------------------
void GameStateModule::RequestUnpause(s32 leUnpauseModule, GameStateModuleIO::GameActionQueue* lpQueue)
{
    if (leUnpauseModule == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("leUnpauseModule != E_PAUSE_NONE", KAC_GSM_FILE, 6159);
        CgsDev::Assert::EndAssert();
    }

    const bool lbSimPausedBefore = IsSimPaused(false, false);

    const s32  liPreviousFlags = miSimPauseFlags;
    const s32  liNewFlags      = liPreviousFlags & ~leUnpauseModule;
    const bool lbWasPausedBefore = (liPreviousFlags != 0);
    miSimPauseFlags = liNewFlags;

    const bool lbSimPausedAfter  = IsSimPaused(false, false);
    const bool lbStillPaused     = (liNewFlags != 0);

    if (lbSimPausedAfter == lbSimPausedBefore)
    {
        if (lbStillPaused != lbWasPausedBefore && lbStillPaused)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Paused from RequestUnpause...?", KAC_GSM_FILE, 6187);
            CgsDev::Assert::EndAssert();
        }
        return;
    }

    if (lbSimPausedAfter)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Sim paused from RequestUnpause...?", KAC_GSM_FILE, 6182);
        CgsDev::Assert::EndAssert();
    }

    u8 lacUnpause[1] = { 0 };   // X360 posts the uninitialised 1-byte local verbatim
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacUnpause), KI_ACTION_UNPAUSE, 1);
}

// --------------------------------------------------------------------------------------------
// X360 0x823900E8 -- WaitForStreaming. The boot-time pause (reason bit 1): from the MemoryCard
// flow state's exit (game event 8) until FinishStreaming, i.e. across the post-title world load
// and the intro video. Its audible consequence is the sound module's dispatch bit 0 (game
// actions 86/87 -> SoundLogicModule::ProcessGameActionQueue), which MusicEffect::UpdateParams
// @0x826FE5C8 reads as `lbStreamsPaused` in front of the EA Trax song selection -- with it set,
// no song can be picked while the loading / video window is open.
// --------------------------------------------------------------------------------------------
void GameStateModule::WaitForStreaming(GameStateModuleIO::GameActionQueue* lpQueue)
{
    CGS_ASSERT(lpQueue != 0, CgsDev::Assert::MessageAt("lpOutputActionQueue != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6283));   // BrnGameStateModule.cpp:6283
    if (lpQueue == 0)
        return;
    miStreamingWaitCountdown = 2;
    const GameMode* lpcMode = mModeManager.GetCurrentGameMode();
    const bool lbModeIsOnline = (lpcMode != 0) && lpcMode->IsOnline();   // mode +172
    if (mbWaitForStreaming || lbModeIsOnline)
    {
        if (lpcMode == 0 || lbModeIsOnline)
            RequestPause(1, lpQueue, 0, 0);
        mbWaitingForStreaming = true;
        mabModuleStreamingComplete[0] = false;
        mabModuleStreamingComplete[1] = false;
        mabModuleStreamingComplete[2] = false;
        mabModuleStreamingComplete[3] = false;
    }
    else
    {
        FinishStreaming(lpQueue);
    }
}

// X360 0x82382258 -- FinishStreaming: the wait is over; tell the GUI (action 191, one zero
// byte -- BridgeGameStateToGui turns it into the GUI's streaming-complete notice) and release
// the reason-1 pause. ARTIST823822A4..823822B8 then conditionally clears
// ModeManager's existing streaming latch: GSM+42275 == ModeManager+38147.
void GameStateModule::FinishStreaming(GameStateModuleIO::GameActionQueue* lpQueue)
{
    mbWaitingForStreaming = false;
    u8 lu8Payload = 0;
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Payload), 191, 1);
    RequestUnpause(1, lpQueue);
    if (mModeManager.mbIsModePrepared)
        mModeManager.mbIsModePrepared = false;
}

// ⭐ [FX-RUMBLE3 2026-09-24, crash-parity G10-D4] X360 0x8236B570 -- the three-instruction forward
// `addis r3,r3,1 ; addi r3,r3,-0x49A8 ; b RumbleManager::BridgeRumbleToInput` (this += 0xB658 ==
// &mRumbleManager, r4 / r5 passed through). Called by BrnGameModule::DoUpdate_InputPreWorld.
void GameStateModule::BridgeRumbleToInput(CgsInput::InputIO::PreWorldInputBuffer* lpInputInputBuffer,
                                          const CgsSystem::TimerStatusInterface*  lpTimerStatusInterface)
{
    mRumbleManager.BridgeRumbleToInput(lpInputInputBuffer, lpTimerStatusInterface);
}

// --------------------------------------------------------------------------------------------
// ⭐ [tut-ticker] RequestPause (X360 0x82382010) -- the pause twin of RequestUnpause above.
// Samples the CHECKED pause answer before and after ORing the reason bit in; a change there
// broadcasts action 86, else a change in the RAW flags-nonzero answer broadcasts action 88.
// The two asserts guard the "we asked to pause and ended up unpaused" inversions. The trailing
// two s32s are the second bools of the two IsSimPaused probes (both 0 at the TrainingManager
// call site).
// ⓘ NAMED PC DEVIATION, pre-existing: nothing in the PC world tick consumes miSimPauseFlags or
// the pause actions yet, so a training-driven pause latches the flag and broadcasts but freezes
// nothing. The console pauses the world while a training voiceover plays.
// --------------------------------------------------------------------------------------------
void GameStateModule::RequestPause(s32 liPauseReasonFlags,
                                   GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                   s32 liArg3, s32 liArg4)
{
    if (liPauseReasonFlags == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lePauseModule != E_PAUSE_NONE", KAC_GSM_FILE, 6115);
        CgsDev::Assert::EndAssert();
    }

    const bool lbCheckedPausedBefore = IsSimPaused(true, liArg4 != 0);

    const s32 liPreviousFlags = miSimPauseFlags;
    miSimPauseFlags = liPreviousFlags | liPauseReasonFlags;

    const bool lbRawPausedBefore  = (liPreviousFlags != 0);
    const bool lbCheckedPausedAfter = IsSimPaused(true, liArg3 != 0);
    const bool lbRawPausedAfter   = (miSimPauseFlags != 0);

    if (lbCheckedPausedAfter != lbCheckedPausedBefore)
    {
        if (!lbCheckedPausedAfter)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Sim unpaused from RequestPause...?", KAC_GSM_FILE, 6138);
            CgsDev::Assert::EndAssert();
        }
        u8 lacPause[1] = { 0 };   // X360 posts the uninitialised 1-byte local verbatim
        lpGameActionQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacPause), KI_ACTION_PAUSE_STRICT, 1);
        return;
    }

    if (lbRawPausedAfter != lbRawPausedBefore)
    {
        if (!lbRawPausedAfter)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Unpaused from RequestPause...?", KAC_GSM_FILE, 6143);
            CgsDev::Assert::EndAssert();
        }
        u8 lacPause[1] = { 0 };
        lpGameActionQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacPause), KI_ACTION_PAUSE_RAW, 1);
    }
}

// --------------------------------------------------------------------------------------------
// ⭐ [tut-ticker] ShouldAllowTimedTutorialTips (X360 0x82356DB0). True only when the ambient
// timed-tip machinery may run this frame. The console's five reads, in its own order (any
// failure returns false):
//   1. the embedded interface's player car is live -- `(iface.mePlayerActiveRaceCarIndex != -1)
//      ? iface.mbIsPlayerCarActive : 0`, with the interface's own bounds assert. That expression
//      IS RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive() (same assert file:line), so
//      it is reached by name.
//   2. the byte at this+245952 must be 0. ⚠️ FLAG: that byte has NO writer anywhere in the
//      30,084-function export set (measured 2026-08-24: the only pseudocode references are the
//      two readers, this function and RequestTraining's boost arm) and no DWARF name in the
//      committed slices; a zero-initialised member the console never sets would read 0 for the
//      whole session, which is what this build's absence of the member also yields. Named as a
//      gap, not modelled.
//   3. miSimPauseFlags == 0        (this+232288)
//   4. mCarSelectManager.mJunkyardId low word == 0 (the console `lwz`s this+183748, the LOW half
//      of the big-endian CgsID at +183744 -- reproduced as the low-32 test, not a full-ID test)
//   5. mModeManager.mpCurrentGameMode == 0 (this+7608) -- no game mode running.
// --------------------------------------------------------------------------------------------
bool GameStateModule::ShouldAllowTimedTutorialTips()
{
    if (!mLastActiveRaceCarInterface.IsPlayerCarActive())
    {
        return false;
    }

    // (read 2 -- the un-homed always-zero byte -- see the banner FLAG.)

    if (miSimPauseFlags != 0)
    {
        return false;
    }

    if ((static_cast<u64>(mCarSelectManager.GetJunkyardId()) & 0xFFFFFFFFull) != 0)
    {
        return false;
    }

    if (mModeManager.GetCurrentGameMode() != 0)
    {
        return false;
    }

    return true;
}

// [tut-ticker] the declare-only ModeManager accessor, bodied (the X360 inlines the embedded
// member's address; MugshotManager/TrainingManager reach it by name here).
// --------------------------------------------------------------------------------------------
// ApplyCarStats (X360 0x82381188).
// Publish the newly-selected car's gameplay stats out of its VehicleListEntry as the 24-byte
// action 198. The six payload words and their sources are read straight off the asm:
//   [+0]  = entry byte @0xE8 + 1  -> no; see the store map below (byte offsets, not indices).
// X360 store map (`li r6, 0x18` == 24 bytes, `li r5, 0xC6` == 198):
//   payload +0x00 <- lbz entry+0x99      payload +0x0C <- lbz entry+0x98
//   payload +0x04 <- lbz entry+0x9B      payload +0x10 <- lfs entry+0x90 (f32)
//   payload +0x08 <- lbz entry+0x9A      payload +0x14 <- lbz entry+0xE8
// ⚠️ FLAG: entry+0x90..+0x9B is inside VehicleListEntry's leading opaque header (maPad0), whose
// individual gameplay fields are not named yet -- CanAutoRepair()/IsTrophyCar()/GetUnlockRank()
// are the three bits of it that have been recovered so far. The five reads here are therefore
// taken as raw bytes/word from that region rather than through named accessors; entry+0xE8 IS
// named (GetCarType()). DELETE-WHEN the VehicleListEntry gameplay-data sub-object is homed.
// --------------------------------------------------------------------------------------------
void GameStateModule::ApplyCarStats(CgsID lCarId, GameStateModuleIO::GameActionQueue* lpQueue)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry", KAC_GSM_FILE, 4720);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    const u8* lpcEntryBytes = reinterpret_cast<const u8*>(lpVehicleListEntry);

    struct ApplyCarStatsAction
    {
        s32 miStatA;        // entry +0x99
        s32 miStatB;        // entry +0x9B
        s32 miStatC;        // entry +0x9A
        s32 miStatD;        // entry +0x98
        f32 mfStatE;        // entry +0x90
        s32 miCarType;      // entry +0xE8 == GetCarType()
    };
    ApplyCarStatsAction lAction;
    lAction.miStatA   = lpcEntryBytes[0x99];
    lAction.miStatB   = lpcEntryBytes[0x9B];
    lAction.miStatC   = lpcEntryBytes[0x9A];
    lAction.miStatD   = lpcEntryBytes[0x98];
    std::memcpy(&lAction.mfStatE, lpcEntryBytes + 0x90, sizeof(f32));
    lAction.miCarType = lpVehicleListEntry->GetCarType();

    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                      KI_ACTION_APPLY_CAR_STATS, sizeof(ApplyCarStatsAction));
}

// --------------------------------------------------------------------------------------------
// GetOriginalCarId (X360 0x823758E8).
// Walk lCarId up its VehicleListEntry parent chain to the base ("original") car a livery variant
// derives from. The console walks AT MOST TWO levels -- car -> parent -> grandparent -- and
// returns the deepest non-null id it reaches. The leading redundant lookup exists only to carry
// the console's own assert.
// --------------------------------------------------------------------------------------------
CgsID GameStateModule::GetOriginalCarId(CgsID lCarId)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;

    {
        const s32 liIndex = lpVehicleList->GetVehicleIndex(lCarId);
        if (liIndex < 0 || lpVehicleList->GetVehicleData(liIndex) == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("NULL != mpVehicleList->GetVehicleData(lCarId)", KAC_GSM_FILE, 5545);
            CgsDev::Assert::EndAssert();
            return lCarId;   // the X360 falls through into a null deref; bail instead of faulting
        }
    }

    const s32 liCarIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpCarEntry =
        (liCarIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liCarIndex);
    const CgsID lParentId = lpCarEntry->GetParentId();
    if (lParentId == 0)
    {
        return lCarId;
    }

    const s32 liParentIndex = lpVehicleList->GetVehicleIndex(lParentId);
    const BrnResource::VehicleListEntry* lpParentEntry =
        (liParentIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liParentIndex);
    const CgsID lGrandParentId = (lpParentEntry != 0) ? lpParentEntry->GetParentId() : 0;
    return (lGrandParentId != 0) ? lGrandParentId : lParentId;
}

// --------------------------------------------------------------------------------------------
// OnSpecialEventPlayerCarChange (X360 0x8238FB40).
// The single point every player-car swap funnels through: cache the new car + wheel ids, tell the
// progression layer, publish the car's stats, stamp the car's TYPE onto the profile, and broadcast
// the 8-byte "player car changed" action (1).
// ARG SHAPE FROM ASM: r3=this, r4=carId(std @0x456D8), r5=wheelId(std @0x456E0), r6=queue,
// r7=the bool forwarded to ProgressionManager::OnPlayerCarChange.
// --------------------------------------------------------------------------------------------
void GameStateModule::OnSpecialEventPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                                                    GameStateModuleIO::GameActionQueue* lpQueue,
                                                    bool lbUpdateProfile)
{
    mActivePlayerCarId   = lCarId;
    mActivePlayerWheelId = lWheelId;

    mProgressionManager.OnPlayerCarChange(lCarId, lWheelId, lbUpdateProfile);
    ApplyCarStats(lCarId, lpQueue);

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpPlayerCarVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpPlayerCarVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lPlayerCarVehicleListEntry != NULL", KAC_GSM_FILE, 4694);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    // X360 `lbz r29, 0xE8(entry)` -> the car TYPE byte, stored into the profile's cached
    // "current car type" word (Profile +117948, mProfile.meCurrentCarType).
    const u8 luCarType = lpPlayerCarVehicleListEntry->GetCarType();

    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile != NULL", KAC_GSM_FILE, 4698);
        CgsDev::Assert::EndAssert();
        return;
    }
    lpProfile->SetCurrentCarType(static_cast<s32>(luCarType));

    CgsID lNewCarId = lCarId;
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNewCarId),
                      KI_ACTION_PLAYER_CAR_CHANGED, sizeof(CgsID));
}

// --------------------------------------------------------------------------------------------
// OnPlayerCarChange (X360 0x82396B88).
// The offline junkyard-exit path. Does everything OnSpecialEventPlayerCarChange does, then looks
// up the AI opponent set for the car's ORIGINAL (base) id at the player's current progression rank
// and broadcasts up to seven opponent car ids as the 64-byte action 4.
// ARG SHAPE FROM ASM: identical to OnSpecialEventPlayerCarChange -- r3..r7 are forwarded to it
// unchanged (the X360 does not even reload them).
// --------------------------------------------------------------------------------------------
void GameStateModule::OnPlayerCarChange(CgsID lCarId, CgsID lWheelId,
                                        GameStateModuleIO::GameActionQueue* lpQueue,
                                        bool lbUpdateProfile)
{
    mActivePlayerCarId   = lCarId;
    mActivePlayerWheelId = lWheelId;

    // The X360 zeroes the opponent array's count word BEFORE the forwarded call (the local lives
    // across it), i.e. Array<CgsID,7>::Construct().
    CgsContainers::Array<s64, 7> laOpponentCarIds;
    laOpponentCarIds.Construct();

    OnSpecialEventPlayerCarChange(lCarId, lWheelId, lpQueue, lbUpdateProfile);

    const BrnProgression::ProgressionData* lpProgressionData = mProgressionManager.GetProgressionData();
    if (lpProgressionData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProgressionData != NULL", KAC_GSM_FILE, 4636);
        CgsDev::Assert::EndAssert();
    }
    else
    {
        const CgsID lOriginalCarId = GetOriginalCarId(lCarId);
        // X360 `extsb r5, r3` -- the rank is sign-extended from a BYTE before the lookup.
        const s32 liProgressionRank =
            static_cast<s32>(static_cast<s8>(mProgressionManager.GetProgressionRank()));

        const BrnProgression::CarOpponentSet* lpCarOpponentSet =
            lpProgressionData->FindCarOpponentSet(lOriginalCarId, liProgressionRank);
        if (lpCarOpponentSet != 0)
        {
            s32 liOpponents = lpCarOpponentSet->GetOpponentCount();
            if (liOpponents >= static_cast<s32>(CgsContainers::Array<s64, 7>::KU_SIZE))
            {
                liOpponents = static_cast<s32>(CgsContainers::Array<s64, 7>::KU_SIZE);
            }
            for (s32 liCarOpponentIndex = 0; liCarOpponentIndex < liOpponents; ++liCarOpponentIndex)
            {
                if (liCarOpponentIndex < 0 || liCarOpponentIndex >= lpCarOpponentSet->GetOpponentCount())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liCarOpponentIndex >= 0 && liCarOpponentIndex < miOpponentCount",
                        KAC_OPPONENTDATA_FILE, 224);
                    CgsDev::Assert::EndAssert();
                }
                laOpponentCarIds.Append(
                    static_cast<s64>(lpCarOpponentSet->GetCarOpponent(liCarOpponentIndex)->GetCarId()));
            }
        }
    }

    // X360 `li r6, 0x40` -- the whole Array<CgsID,7> (7*8 elements + count) goes on the wire.
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&laOpponentCarIds),
                      KI_ACTION_CAR_OPPONENT_SET, sizeof(laOpponentCarIds));
}

// ============================================================================
// FindPlayerScoringIndexForActiveRaceCar
//
// Linear scan of the mode manager's scoring system's eight per-player records for the player
// scoring slot whose active-race-car index is leActiveRaceCarIndex. The miss arm returns
// E_PLAYER_SCORING_INDEX_0, not an invalid sentinel, and without the not-found assert
// ScoringSystem::GetPlayerScoringIndex fires: start-of-game relies on it, nothing is mapped yet
// and the player's car takes scoring slot 0.
GameStateModuleIO::EPlayerScoringIndex
GameStateModule::FindPlayerScoringIndexForActiveRaceCar(::EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    const ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
    for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
    {
        const GameStateModuleIO::EPlayerScoringIndex leSlot =
            static_cast<GameStateModuleIO::EPlayerScoringIndex>(liSlot);
        if (lpScoringSystem->GetCarDataFromPlayerScoringIndex(leSlot)->GetActiveRaceCarIndex()
            == leActiveRaceCarIndex)
        {
            return leSlot;
        }
    }
    return GameStateModuleIO::E_PLAYER_SCORING_INDEX_0;
}

// ============================================================================
// SendSetupPlayerCarEvent  @ 0x8239A918   -- THE START-OF-GAME JUNKYARD ENTRY
//
// Console body, statement for statement (0x8239A918..0x8239AA30):
//   1. seed the profile's car pose with the track's authored player-start pose:
//        this+48336 (Profile+0x30) = TriggerData::GetPlayerStartPosition()   (lvx128 memory+0x10)
//        this+48352 (Profile+0x40) = TriggerData::GetPlayerStartDirection()  (lvx128 memory+0x20)
//   2. entry = VehicleList::GetVehicleData(mpVehicleList, 0);  carId = entry->GetId()
//   3. wheelId = WheelList::GetWheelData(mpWheelList,
//                    FindWheelIndexFromName(entry->GetDefaultWheelName()))->mID  (miss -> left 0)
//   4. junkyardId = FindNearestJunkyardID(playerStartPosition)
//   5. scoringIdx = FindPlayerScoringIndexForActiveRaceCar(GetPlayerActiveRaceCarIndex())
//   6. CarSelectManager::EnterJunkyardAtStartOfGame(queue, junkyardId, carId, wheelId,
//                                                  scoringIdx, &mCachedCarSelectChangedAction)
//   7. ProgressionManager::OnDriveThru(junkyardId, 0, 0)
//   8. this+232306 = 1     (the "waiting to REALLY enter the junkyard" flag ProcessGameEvents
//                           case 78 tests before ReallyEnterJunkyardAtStartOfGame)
//
// ⭐ STEP 8 IS NOW REAL (2026-08-01). It is the console's own member -- DWARF
// BrnGameStateModule.h:811 mbWaitingToPutPlayerInJunkyard -- and its reader, the extracted
// case-78 arm, lives in GameStateModule_ProcessGameEvents_wBT_01.cpp. Without this store the
// junkyard entry stops half-done: the player's car is placed at maSpawnLocations[1] and NOTHING
// ever posts the transition-in action, so the director's meJunkyardState stays E_JY_INACTIVE and
// ArbStateCarSelect is never reached. That was the whole gap.
//
// ⭐ [L4 boot order 2026-09-28] STEP 1 IS REAL: THE TWO POSE STORES ARE THE PROFILE'S CAR POSE. The console
// stores them at this+0xBCD0 / this+0xBCE0 (`stvx128 v127, r31, r10(0xBCD0)` @0x8239A97C, `stvx128 v0, r31,
// r9(0xBCE0)` @0x8239A980). this+0xBCA0 is mProgressionManager's embedded Profile (OnProfileLoaded's r30), so they
// are Profile+0x30 mCarPosition and Profile+0x40 mCarDirection -- the spawn-on-load pose OnProfileLoaded
// @0x82397310 hands FindNearestJunkyardID (`lvx128 v1, r30, 0x30` @0x823973D0). They were dropped here as "cached
// pose members with no reader"; since the MemoryCard exit runs OnProfileLoaded (f935feb8) the reader exists, and a
// profile that never had its pose seeded (every PC-written save, and a fresh profile) entered the junkyard nearest
// the ORIGIN -- 312262 at (-381.6, 13.1, 915.5) -- instead of the track's start junkyard 250700: the crash sweep's
// cell h225_s80 fired its shot from 4.8 km away (L2, scratch/flow_run/l2rdyoff_h225_s80_r1: `[sweep] shot 0/1 ...
// from (-299.567108, 8.158300, 923.594910)`, where l2bis74_h225_s80_r1 fired from (3007.971924, -2.540363,
// -1945.166992)).
// On the console the one-shot runs in the first loading-scripted frame, before the MemoryCard Deserialise, so a
// RETURNING profile's saved pose still wins; a new profile keeps the start pose (Profile::Construct @0x823708A8
// seeds (0,0,0,0) / (1,0,0,0), and ProcessGameEvents case 110 re-runs Construct then this function).
void GameStateModule::SendSetupPlayerCarEvent(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    const Vector3 lPlayerStart = mTriggerQueryManager.GetTriggerData()->GetPlayerStartPosition();
    const Vector3 lPlayerDirection = mTriggerQueryManager.GetTriggerData()->GetPlayerStartDirection();
    BrnProgression::Profile* const lpProfile = mProgressionManager.GetProfile();
    lpProfile->SetCarPosition(lPlayerStart);
    lpProfile->SetCarDirection(lPlayerDirection);

    const CgsID lCarModelId = mpVehicleList->GetVehicleData(0)->GetId();
    CgsID lWheelId = 0;
    const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lCarModelId);
    if (liVehicleIndex >= 0)
    {
        const BrnResource::VehicleListEntry* const lpEntry = mpVehicleList->GetVehicleData(liVehicleIndex);
        if (lpEntry != 0)
        {
            const s32 liWheelIndex = mpWheelList->FindWheelIndexFromName(lpEntry->GetDefaultWheelName());
            if (liWheelIndex != -1)
            {
                lWheelId = mpWheelList->GetWheelData(liWheelIndex)->mID;
            }
        }
    }

    const CgsID lJunkyardId = FindNearestJunkyardID(lPlayerStart);
    const GameStateModuleIO::EPlayerScoringIndex leScoringIndex =
        FindPlayerScoringIndexForActiveRaceCar(GetPlayerActiveRaceCarIndex());
    mCarSelectManager.EnterJunkyardAtStartOfGame(lpActionQueue, lJunkyardId, lCarModelId, lWheelId,
                                                leScoringIndex, &mCachedCarSelectChangedAction);
    mProgressionManager.OnDriveThru(lJunkyardId, BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD, 0);
    mbWaitingToPutPlayerInJunkyard = true;
}

// ============================================================================
// ⭐⭐⭐ [L4 boot order 2026-09-28] OnProfileLoaded -- X360 0x82397310. THE LOADED PROFILE REACHES THE
// GAME STATE. DWARF BrnGameStateModule.h:829 (BrnGameStateModule.cpp:6297 on the PS3, where the
// licence-picture fix-up is still the out-of-line Profile::FixUp; the X360 inlines it).
//
// THE CONSOLE BODY, statement for statement (r31 = this, r30 = the profile == this+0xBCA0 ==
// mProgressionManager's embedded Profile, r28 = lpOutput, r27 = lpOutputActionQueue):
//   0x8239733C  assert lpProfile != NULL                                          (cpp:6586)
//   0x82397368  NetworkTexture::Construct(profile+102620) ; NetworkTexture::Prepare(profile+102620,
//               profile+102648, 0x2580, 0xA0, 0x78, 0x1A200052)                -- Profile::FixUp, inlined
//   0x823973A4  ModeManager::ExitCurrentMode(this+0x1020, lpOutput, 1, 0x12)
//   0x823973B4  RoadRulesManager::QuitAnyActiveRules(this+0x2CD28, lpOutput)
//   0x823973C4  ProgressionManager::OnLoadProfile(this+0xBB30)
//   0x823973D4  jy  = FindNearestJunkyardID(v1 = lvx128 profile+0x30)             -- the SAVED car position
//   0x823973E4  car = GetSpawnCar(this, profile)
//   0x82397404  idx = FindPlayerScoringIndexForActiveRaceCar(GetPlayerActiveRaceCarIndex())
//   0x82397428  CarSelectManager::EnterJunkyardAtStartOfGame(this+0x2CDA0, queue, jy, car, 0, idx,
//               this+0x38B80)                                                     -- the wheel is `li r7, 0`
//   0x8239742C  ld profile.mSpawnCarId / profile.mSpawnWheelId; assert the car != 0 (BrnProfile.h:1903)
//   0x82397470  OnSpecialEventPlayerCarChange(this, profile.mSpawnCarId, profile.mSpawnWheelId, queue, 1)
//   0x82397480  StreetManager::OnProfileLoaded(this+0x45768, lpOutput)            -- the road-rule tables
//   0x82397488  ModeManager::OnProfileLoaded(this+0x1020)                         -- the challenge bits
//   0x8239749C  AddEvent(queue, <stack byte>, 194, 1)                             -- E_ACTION_LOAD_PROFILE
//   0x823974B0  stb 1 -> this+0x38B72                                            -- mbWaitingToPutPlayerInJunkyard
//   0x823974C0  if ((s8)ProgressionManager::GetProgressionRank() < 1)
//                   AddEvent(queue, &flt_82005450 (0.9f), 28, 4)                  -- E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK
//   0x823974E8  assert mProgressionManager.GetProfile()                          (cpp:6638)
//   0x82397510  payload = this+0x29188 == &profile.maEventScoresToUpload
//   0x82397518  assert lpOutputActionQueue                                        (cpp:6640)
//   0x82397548  AddEvent(queue, &payload, 19, 4)                                  -- E_ACTION_NON_UPLOADED_MODE_SCORES
//   0x82397558  RequestUnpause(this, 2, queue)
//
// WHY IT MATTERS. Every one of those puts the SAVE back into the running game: the road-rule records
// (StreetManager copies them out of the profile; StreetManager::Update copies its tables back INTO the
// profile every frame, so without this call a PC session wiped them and the next save wrote the wipe),
// the max-car count and the rank cache (OnLoadProfile), the junkyard car (the saved one, at the junkyard
// nearest where the player left it), and the smashed props (action 194 -> the prop world asks for the
// profile's hit-prop bits -> the ProcessGameEvents tail answers).
//
// Callers: ProcessGameEvents cases 8 and 109, inside PreWorldUpdate's output
// write lock with mbIsUpdating raised.
// ============================================================================
void GameStateModule::OnProfileLoaded(GameStateModuleIO::OutputBuffer* lpOutput,
                                      GameStateModuleIO::GameActionQueue* lpOutputActionQueue)
{
    BrnProgression::Profile* const lpProfile = mProgressionManager.GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile != NULL", KAC_GSM_FILE, 6586);
        CgsDev::Assert::EndAssert();
    }

    lpProfile->FixUp();

    // `li r6, 0x12` -- the mode-type argument is 18, one past E_MODE_ONLINE_MODE_END (17) on this build:
    // the ModeManager's slot count (see KI_GAME_MODE_SLOTS; the WorldTick caller spells it the same way).
    mModeManager.ExitCurrentMode(lpOutput, true,
                                 static_cast<GameStateModuleIO::EGameModeType>(ModeManager::KI_GAME_MODE_SLOTS));
    mRoadRulesManager.QuitAnyActiveRules(lpOutput);
    mProgressionManager.OnLoadProfile();

    const CgsID lJunkyardId = FindNearestJunkyardID(lpProfile->GetCarPosition());
    const CgsID lSpawnCarId = GetSpawnCar(lpProfile);
    const GameStateModuleIO::EPlayerScoringIndex leScoringIndex =
        FindPlayerScoringIndexForActiveRaceCar(GetPlayerActiveRaceCarIndex());
    mCarSelectManager.EnterJunkyardAtStartOfGame(lpOutputActionQueue, lJunkyardId, lSpawnCarId, 0,
                                                 leScoringIndex, &mCachedCarSelectChangedAction);

    // `ld r11, 0x50(r30) ; ld r25, 0x58(r30)` -- both ids are read before the inlined accessor's assert,
    // and the car id is re-read after it (`ld r4, 0x50(r30)`).
    const CgsID lSpawnWheelId = lpProfile->GetSpawnWheelId();
    if (lpProfile->GetSpawnCarId() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mSpawnCarId != 0", KAC_PROFILE_H_FILE, 1903);
        CgsDev::Assert::EndAssert();
    }
    OnSpecialEventPlayerCarChange(lpProfile->GetSpawnCarId(), lSpawnWheelId, lpOutputActionQueue, true);

    mStreetManager.OnProfileLoaded(lpOutput);
    mModeManager.OnProfileLoaded();

    // Action 194, one byte (`li r6, 1`; the payload is an untouched stack byte -- the record is empty).
    const GameStateModuleIO::LoadProfileAction lLoadProfile = GameStateModuleIO::LoadProfileAction();
    lpOutputActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lLoadProfile),
                                  GameStateModuleIO::E_ACTION_LOAD_PROFILE,
                                  static_cast<s32>(sizeof(lLoadProfile)));

    mbWaitingToPutPlayerInJunkyard = true;   // `stbx r10(1), r31, 0x38B72`

    // `extsb r11, r3 ; cmpwi cr6, r11, 1 ; bge` -- the SIGNED byte of the rank.
    if (static_cast<s8>(mProgressionManager.GetProgressionRank()) < 1)
    {
        f32 lfTrafficScale = KF_ONPROFILELOADED_RANK_ZERO_TRAFFIC_SCALE;
        lpOutputActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lfTrafficScale),
                                      GameStateModuleIO::E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK, 4);
    }

    if (mProgressionManager.GetProfile() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mProgressionManager.GetProfile()", KAC_GSM_FILE, 6638);
        CgsDev::Assert::EndAssert();
    }
    // Action 19: the profile's not-yet-uploaded event scores, BY POINTER (the console's payload is the
    // 4-byte address this+0x29188). The host record is one host pointer, read back at pointer width by
    // the bridge's E_ACTION_NON_UPLOADED_MODE_SCORES arm (GameBridgeGameStateToX_wN1_01.cpp).
    GameStateModuleIO::NonUploadedModeScoresAction lNonUploadedModeScores;
    lNonUploadedModeScores.mpNonUploadedScores = lpProfile->GetEventScoresToUpload();
    if (lpOutputActionQueue == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpOutputActionQueue", KAC_GSM_FILE, 6640);
        CgsDev::Assert::EndAssert();
    }
    lpOutputActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lNonUploadedModeScores),
                                  GameStateModuleIO::E_ACTION_NON_UPLOADED_MODE_SCORES,
                                  static_cast<s32>(sizeof(lNonUploadedModeScores)));

    RequestUnpause(2, lpOutputActionQueue);

}

// ============================================================================
// GetSpawnCar -- X360 0x823763C8 (DWARF BrnGameStateModule.h:787). The car the loaded profile spawns in.
//   0x823763E8  assert lpProfile != NULL                                          (cpp:6657)
//   0x82376408  ld profile+0x50; assert != 0                                      (BrnProfile.h:1903, inlined)
//   0x82376444  list = this+0x456E8 (mpVehicleList); idx = GetVehicleIndex(list, id)
//   0x82376458  entry = idx < 0 ? NULL : GetVehicleData(list, idx); assert entry  (cpp:6664)
//   0x8237648C  gameplay = entry + 0x90; assert gameplay                          (cpp:6668)
//   0x823764B8  rank byte = lbz 9(gameplay) (entry+0x99, VehicleListEntry::GetUnlockRank)
//   0x823764C8  `extsb r10, r3 ; cmpw r10, r11 ; blt` -- (s8)GetProgressionRank() < that byte
//                   -> CgsIDCompress("PUSMC01"), else the profile's id.
// ============================================================================
CgsID GameStateModule::GetSpawnCar(const BrnProgression::Profile* lpProfile)
{
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile != NULL", KAC_GSM_FILE, 6657);
        CgsDev::Assert::EndAssert();
    }
    if (lpProfile->GetSpawnCarId() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mSpawnCarId != 0", KAC_PROFILE_H_FILE, 1903);
        CgsDev::Assert::EndAssert();
    }

    const CgsID lSpawnCarId = lpProfile->GetSpawnCarId();
    const BrnResource::VehicleList* const lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = lpVehicleList->GetVehicleIndex(lSpawnCarId);
    const BrnResource::VehicleListEntry* const lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);
    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry != NULL", KAC_GSM_FILE, 6664);
        CgsDev::Assert::EndAssert();
    }
    // The console's second assert tests entry+0x90 (the embedded gameplay block), which cannot be NULL
    // once entry is not; GetUnlockRank() is that block's +0x09 byte.
    if (static_cast<s32>(static_cast<s8>(mProgressionManager.GetProgressionRank())) <
        static_cast<s32>(lpVehicleListEntry->GetUnlockRank()))
    {
        return CgsIDCompress("PUSMC01");
    }
    return lSpawnCarId;
}

// ============================================================================
// IsControllerActive -- DWARF BrnGameStateModule.h:1173, inlined by the console into
// PreWorldUpdate @0x823A5328. The two-value test is transcribed from that inline:
//     if ( v61 == 3 || (v63 = v61 != 0, v62 = 0, !v63) ) v62 = 1;
// which is `state == 3 || state == 0` written the way the compiler folded it.
// ============================================================================
bool GameStateModule::IsControllerActive() const
{
    return meControllerState == E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE
        || meControllerState == E_CONTROLLERSTATE_NOT_IN_GAME;
}

// ================================================================================================
// ⭐⭐⭐ [A9 scoring-feed wave 2026-08-27] GameStateModule::CopyScoringDataToOutput -- X360
// 0x8236CDC0, REAL and WHOLE (not an extracted leg). See the declaration in BrnGameStateModule.h
// for the event score/timer feed. The original single-output-argument shape reads
// the module's timer cache, refreshed by EmmPreWorldUpdate earlier in this caller.
//
// The console body, in order, with every base decoded:
//   r28 = a1 + 235488  == mLastActiveRaceCarInterface           (the cached active-car snapshot)
//   r26 = a2 + 173240  == lpOutput->GetScoringOutputInterface()
//   r29 = a2 + 175976  == lpOutput->GetOnlineScoringOutputInterface()
//   r23 = a1 + 4128    == mModeManager
//   r20 = a1 + 7632    == mModeManager.GetScoringSystem()        (ModeManager + 0xDB0)
// then
//   0x8236CDF4  IsPlayerCarActive() / GetPlayerActiveRaceCarIndex() on the INTERFACE (the two
//               asserts at BrnRaceCarEntityModuleOutputInterface.h:967 and :980 are theirs)
//   0x8236CE78  IsOnlineGameMode()
//   0x8236CE90  ModeManager::WriteDataToOutput(scoringOut, onlineOut, online, playerIndex)
//   0x8236CEBC  the mabValid[8] per-active-slot presence sweep
//   0x8236CF7C  mePlayerRaceCarIndex / meGameModeType / mbIsOnlineGameMode
//   0x8236CFA8  the timer block, behind ScoringSystem::IsTimeLimitActive()
//   0x8236D0D0  mfDistanceDrivenInCurrentCar
// ================================================================================================
void GameStateModule::CopyScoringDataToOutput(
        GameStateModuleIO::OutputBuffer* lpOutput)
{
    GameStateModuleIO::ScoringOutputInterface* const lpScoringOut =
        lpOutput->GetScoringOutputInterface();
    GameStateModuleIO::OnlineScoringOutputInterface* const lpOnlineScoringOut =
        lpOutput->GetOnlineScoringOutputInterface();

    // ---- the player's active slot, as the console derives it ----------------------------------
    // @0x8236CDF4..0x8236CE6C. IDA renders this as raw loads at interface+0x2858 / +0x2860 with
    // two baked asserts; both asserts belong to the interface accessors the build inlined, so the
    // calls are restored. IsPlayerCarActive() carries ":967 mePlayerActiveRaceCarIndex <
    // E_ACTIVE_RACE_CAR_INDEX_COUNT"; GetPlayerActiveRaceCarIndex() carries ":980 Player car index
    // hasn't been set".
    ::EActiveRaceCarIndex lePlayerRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
    if (mLastActiveRaceCarInterface.IsPlayerCarActive())
    {
        lePlayerRaceCarIndex = mLastActiveRaceCarInterface.GetPlayerActiveRaceCarIndex();
    }

    // ---- the delegation the whole scoring chain hangs off --------------------------------------
    // @0x8236CE78/0x8236CE90. THE ONLY CALLER of ModeManager::WriteDataToOutput anywhere in the
    // image -- and therefore the only caller of ScoringSystem::WriteDataToOutput behind it.
    const bool lbOnlineGameMode = IsOnlineGameMode();
    mModeManager.WriteDataToOutput(lpScoringOut, lpOnlineScoringOut,
                                   lbOnlineGameMode, lePlayerRaceCarIndex);

    // ---- mabValid[8]: "is there a live race car in this active slot?" --------------------------
    // @0x8236CEBC..0x8236CF74. For each active slot, linear-search the interface's live race list
    // for an entry whose meActiveRaceCarIndex matches; the slot is valid iff the search found one.
    // ⓘ The console fires CgsArray.h:336 ("Array used before Construct/Clear was called") on each
    // GetCount(); that assert is Array<T,N>::GetCount()'s own and the committed CgsArray.h
    // GetCount() does not carry it. Not re-added here -- it belongs to the container.
    // The `leEnumIndex <= E_ACTIVE_RACE_CAR_INDEX_COUNT` assert (BurnoutConstants.h:39) IS carried,
    // by the range-guarded post-increment on EActiveRaceCarIndex this loop uses, in the console's
    // own position (after the store, before the loop test).
    for (::EActiveRaceCarIndex leActive = ::E_ACTIVE_RACE_CAR_INDEX_0;
         leActive < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leActive++)
    {
        const s32 liCarsInRace = mLastActiveRaceCarInterface.maCarsInTheRace.GetCount();
        s32 liEntry = 0;
        while (liEntry < liCarsInRace &&
               mLastActiveRaceCarInterface.maCarsInTheRace[static_cast<u32>(liEntry)]
                   .meActiveRaceCarIndex != leActive)
        {
            ++liEntry;
        }
        lpScoringOut->mabValid[leActive] = (liEntry < liCarsInRace);   // stbx out+0xA08 + slot
    }

    // ---- the three identity scalars ------------------------------------------------------------
    lpScoringOut->mePlayerRaceCarIndex = GetPlayerActiveRaceCarIndex();         // stw out+0xA34
    // @0x8236CF84 `lwz r11, 0x1DB4(r21)` == gsm+7604 == mModeManager.meCurrentGameModeType.
    // ⭐ THIS is the word BrnGameModule::BridgeGameStateToGui gates its id-428 GuiAttackScoreUpdate
    // build on, and the one its id-424 GuiEventScoreUpdate record is built alongside.
    lpScoringOut->meGameModeType = mModeManager.GetCurrentGameModeType();       // stw out+0xA3C
    // @0x8236CF8C..0x8236CFA4: the console does NOT re-call IsOnlineGameMode here -- it inlines the
    // identical `mpCurrentGameMode ? mode->IsOnline() : false` pair a second time. De-inlined to
    // the same two named accessors (the mbIsUpdating assert already fired on the call above).
    {
        const GameMode* const lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        lpScoringOut->mbIsOnlineGameMode =
            (lpCurrentGameMode != 0) ? lpCurrentGameMode->IsOnline() : false;   // stb out+0xA40
    }

    // ---- THE HUD CLOCK -------------------------------------------------------------------------
    // @0x8236CFA8..0x8236D0CC. The gate is `scoring->mStartTime.miSeconds >= 0 &&
    // scoring->mEndTime.miSeconds >= 0` (asm `lwz 0(r20)` / `lwz 8(r20)`, both `cmpwi 0 ; blt`),
    // which is EXACTLY ScoringSystem::IsTimeLimitActive() -- de-inlined to it.
    ScoringSystem* const lpScoringSystem = mModeManager.GetScoringSystem();
    // The frame's "now". Console: gsm+208368 == the module's copy of the PreWorldInputBuffer timer
    // block (gsm+208328) at +40, i.e. TimerStatusInterface::mSimTimerStatus.mTime. See the FLAG at
    // the declaration for why it arrives as an argument on PC.
    const CgsSystem::Time lTimeNow = mTimerStatusInterface.GetSimTimerStatus()->GetTime();

    if (lpScoringSystem->IsTimeLimitActive())
    {
        // `Time::operator-(&tmp, &now, scoring+0)` then float(seconds)+fraction -> out+0xA90.
        // (The X360 also carries a dead alternative arm reading scoring+0x10 (mTotalTime) for the
        //  mStartTime.miSeconds < 0 case -- unreachable behind the gate above, so it is not
        //  reproduced; it is named here so the omission is on record.)
        lpScoringOut->mfModeTimeElapsed =
            lpScoringSystem->GetElapsedTime(lTimeNow).GetFloatVal();            // stfs out+0xA90
        // @0x8236D048 `ScoringSystem::GetModeTimeRemaining(&ret, scoring, &now)` -- an sret call,
        // and THE number the stunt-run HUD clock counts down.
        lpScoringOut->mfModeTimeRemaining =
            lpScoringSystem->GetModeTimeRemaining(lTimeNow).GetFloatVal();      // stfs out+0xA94
        // @0x8236D080 `Time::operator-(&tmp, scoring+8, scoring+0)` == mEndTime - mStartTime, the
        // mode's total authored duration. Spelled through the two public accessors that name those
        // same members: GetElapsedTime(x) IS `x - mStartTime` and GetTimeLimit() IS mEndTime, so
        // GetElapsedTime(GetTimeLimit()) is that difference with no offset poke (hazards H9).
        lpScoringOut->mfCurrentTargetModeTime =
            lpScoringSystem->GetElapsedTime(lpScoringSystem->GetTimeLimit()).GetFloatVal(); // stfs out+0xA98
        lpScoringOut->meCurrentMedalTarget   = lpScoringSystem->GetCurrentMedalTarget();    // stw  out+0xA9C
        lpScoringOut->meCurrentMedalAchieved = lpScoringSystem->GetCurrentMedalAchieved();  // stw  out+0xAA0
        lpScoringOut->mbTimerActive          = true;                                        // stb  out+0xAA8 (li r10,1)
    }
    else
    {
        // @0x8236D0C4..0x8236D0CC. flt_82001CC0, read from the image rodata at VA 0x82001CC0:
        // 00 00 00 00 == 0.0f. ⚠️ ONLY these two are written on this arm -- mfModeTimeElapsed and
        // mfCurrentTargetModeTime keep their previous values, which is what makes a stopped mode
        // clock FREEZE on the HUD rather than snap to zero (the same shape as the mbTimerActive
        // gate in GuiCache::RecEvent's case-424 arm, one hop later).
        lpScoringOut->mfModeTimeRemaining = 0.0f;                               // stfs out+0xA94
        lpScoringOut->mbTimerActive       = false;                              // stb  out+0xAA8
    }

    // ---- distance driven in the current car ----------------------------------------------------
    // @0x8236D0D0..0x8236D100: `lis r11,2 ; ori r10,r11,0x8D4 ; addis r11,r21,1 ; addi r11,r11,
    // -0x44D0 ; lwzx r11, r11, r10` == *(gsm + 47920 + 133332) -- mProgressionManager (gsm+47920,
    // pinned by GameStateModule::Construct's AchievementManagerBase::Construct call) at +0x208D4,
    // which BrnProgressionManager.h names mpCurrentLiveryData. When non-null publish its +0x10
    // float (LiveryData::mfDistanceDriven -- the very field ProgressionManager::AddDistanceDriven
    // @0x823668F0 accumulates into), else 0.0f (flt_82001CC0 again).
    {
        const BrnProgression::LiveryData* const lpLiveryData =
            mProgressionManager.GetCurrentLiveryData();
        lpScoringOut->mfDistanceDrivenInCurrentCar =
            (lpLiveryData != 0) ? lpLiveryData->mfDistanceDriven : 0.0f;        // stfs out+0xAA4
    }
}

// ==============================================================================================
// GameStateModule::ProcessContacts  --  X360 0x8236BC68  (DWARF BrnGameStateModule.h:853)
// ==============================================================================================
// THE MISSING PRODUCER of CrashModeScoring::DealWithHitProp / ::DealWithHitTrafficCar and of
// StuntModeScoring::DealWithHitProp. Its sole console caller is GameStateModule::PostWorldUpdate
// @0x8238F358 (`bl` #25, bracketed by PerfMonCpu Start/StopMonitor(miProcessContactsPM ==
// *(this+292352))), which calls it at the console's own position.
//
//
// THE TWO OWNER READS ARE NOT THE SAME INSTRUCTION AND MUST NOT BE THE SAME C++.
// The console reads the A-side owner as `lwz r11,0(c) ; srwi r10,r11,24` -- a word load and a
// shift -- but the B-side owner as `lbz r9,4(c)` / `lbz r10,4(c)`, a BYTE load at the word's
// base address. On the big-endian X360 that byte IS the word's most-significant byte, i.e. the
// same `>> 24` field. Transcribing it as a byte read of `&mEntityIdB` on this little-endian host
// would read the LEAST significant byte and the arm would never match. Both are written as
// `>> 24` here, which is the console's meaning on both hosts.
// [[invented-arms-and-the-c4715-ratchet]] -- ask which SIDE, and read vs write.
//
// THE PART-INDEX CALLS LOOK DEAD AND ARE KEPT. Each prop arm constructs a SECOND PropEntityID
// from the same word and calls GetPartIndex(), whose result is discarded (0x8236BDE8 /
// 0x8236BE30). They are kept because PropEntityID's constructor carries the console's own
// AssertIsProp() tripwire -- dropping them would drop an assert the console fires.
//
// GetPlayerActiveRaceCarIndex() IS RE-READ INSIDE BOTH LOOPS, per iteration, exactly as the
// console does (`mr r3,r25 ; bl ...` at 0x8236BE38 and 0x8236BEE8). Not hoisted: the accessor
// asserts mbIsUpdating and the console does not cache it.
// ==============================================================================================
void GameStateModule::ProcessContacts(
        const BrnPhysics::ContactSpy::ContactSpyInterface* lpContactSpyInterface)
{
    // 0x8236BC80..0x8236BC8C -- the inlined ContactSpyInterface::IsValid() (mpData != NULL).
    // NOT IsEmpty(): see the accessor's banner in BrnContactSpyInterface.h.
    CGS_ASSERT(lpContactSpyInterface != 0, "lpContactSpyInterface != NULL");   // [PC GUARD]

    // [DIAG] NOT IN THE X360 BINARY -- the ENTRY witness. It sits BEFORE every gate on purpose,
    // for two reasons:
    //   (a) a run that produces no [showtime-crash] line must still say WHICH precondition
    //       failed -- "the spy is unbound", "the mode is not showtime", "the mode is not
    //       IN_PROGRESS" and "there were no contacts" are four completely different defects;
    //   (b) ⚠️⚠️ the CONTACT COUNTERS MUST ACCUMULATE OVER EVERY FRAME, NOT SAMPLE ONE.
    //       A contact queue is a PER-FRAME queue -- the physics clears it at the top of each
    //       PhysicsModule::Update -- so it is non-empty only on the frames where a contact
    //       actually happened. A 1-in-60 sample of GetLength() reads 0 on ~99% of frames even
    //       while contacts stream through, and the first version of this probe did exactly that
    //       and printed "traffic=0" six times running. [[diagnostics-that-lie]]: sample period
    //       vs event lifetime. And they must accumulate OUTSIDE the mode gates, because on this
    //       build the showtime mode reaches E_GMS_IN_PROGRESS for only ~5 s of a 190 s run
    //       (measured: 152 samples in E_GMS_RESULTS, 5 in IN_PROGRESS) -- so counting only past
    //       the gates would measure the gate, not the physics.
    {
        static s32        siEntryFrame    = 0;
        static s32        siMaxProps      = 0;
        static s32        siMaxTraffic    = 0;
        static s32        siPropFrames    = 0;
        static s32        siTrafficFrames = 0;
        static s32        siTotalTraffic  = 0;

    }

    if (lpContactSpyInterface == 0 || !lpContactSpyInterface->IsValid())
    {
        return;
    }

    // 0x8236BC90..0x8236BCBC -- the mode gate: showtime (2 / 16) or stunt attack (7).
    const GameStateModuleIO::EGameModeType leGameModeType = mModeManager.GetCurrentGameModeType();
    const bool lbShowtime =
        (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME) ||
        (leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);
    if (!lbShowtime && leGameModeType != GameStateModuleIO::E_MODE_STUNT_ATTACK)
    {
        return;
    }

    // 0x8236BCC0..0x8236BCEC -- the mode must exist AND be IN_PROGRESS (the house
    // `lwz 0x28 ; addi -2 ; cntlzw ; extrwi` idiom, reached by name here).
    const GameMode* const lpCurrentGameMode = mModeManager.GetCurrentGameMode();
    if (lpCurrentGameMode == 0 ||
        lpCurrentGameMode->GetCurrentState() != GameStateModuleIO::E_GMS_IN_PROGRESS)
    {
        return;
    }

    // 0x8236BCF0..0x8236BD0C -- the two contact queues, each through its own re-fetch of the
    // interface (the console does not CSE those re-fetches either).
    const BrnPhysics::ContactSpy::ContactSpyData::PropContactQueue* const lpPropContacts =
        lpContactSpyInterface->GetPropContacts();
    const BrnPhysics::ContactSpy::ContactSpyData::TrafficContactQueue* const lpTrafficContacts =
        lpContactSpyInterface->GetTrafficContacts();

    // [DIAG] NOT IN THE X360 BINARY. The gate witness: which of this function's four
    // preconditions is the one that stops it, and how much traffic the spy is actually
    // carrying. Every gate above returns silently, so without this a run that produces no
    // [showtime-crash] line cannot distinguish "the spy is unbound" from "the mode is not
    // IN_PROGRESS" from "there were no traffic contacts" -- three completely different defects.
    // Once per second at 60 Hz, only past the gates, only under BRN_SHOWTIME_WATCH.
    // [[diagnostics-that-lie]] -- a probe that only fires on success proves nothing on failure.
    //
    // ⓘ THE ACCUMULATING HALF LIVES IN THE ENTRY PROBE, NOT HERE -- see its banner for why
    // (per-frame queues cannot be sampled, and the mode gates above are open for only ~5 s of a
    // 190 s run). This one only reports how many frames actually reached the console's own
    // scoring gates, which is the other half of the answer.
    {
        static s32        siFrame = 0;
    }

    // 0x8236BD10..0x8236BD30 -- the two scorers. r24 = ScoringSystem+0x20 (the crash scorer);
    // r26 = ScoringSystem + 0x2620 when IsOnlineGameMode(), else ScoringSystem + 0x350 -- i.e.
    // the ONLINE vs OFFLINE stunt scorer. Reached through the named accessors (hazards H9).
    ScoringSystem* const lpScoringSystem = mModeManager.GetScoringSystem();
    CrashModeScoring* const lpCrashScorer = lpScoringSystem->GetCrashScorer();
    StuntModeScoring* const lpStuntScorer = IsOnlineGameMode()
                                               ? lpScoringSystem->GetOnlineStuntScorer()
                                               : lpScoringSystem->GetStuntScorer();

    // The console's two tripwires, with their baked line numbers (li r5, 0x1A68 / 0x1A69).
    // Neither gates -- both are address-of-a-subobject tests that can only fail on a null this.
    CGS_ASSERT(lpCrashScorer != 0, CgsDev::Assert::MessageAt("lpCrashScorer != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6760));   // BrnGameStateModule.cpp:6760
    CGS_ASSERT(lpStuntScorer != 0, CgsDev::Assert::MessageAt("lpStuntScorer != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6761));   // BrnGameStateModule.cpp:6761

    // ------------------------------------------------------------------------------------
    // LEG 1 -- the PROP contacts (0x8236BD84..0x8236BE70).
    // Only a contact between a PROP and a RACE CAR counts, in either order, and only when that
    // race car is the local player's.
    // ------------------------------------------------------------------------------------
    for (s32 liIndex = 0; liIndex < lpPropContacts->GetLength(); ++liIndex)
    {
        const BrnPhysics::ContactSpy::PropContact& lrContact = lpPropContacts->GetEvent(liIndex);

        s32 liRaceCarIndex    = -1;   // r29 -- E_ACTIVE_RACE_CAR_INDEX_INVALID
        u32 luPropEntityIndex = 0;    // r30

        const u32 luOwnerA = lrContact.mEntityIdA.muValue >> BrnWorld::PropEntityID::KU_OWNER_BASE;
        const u32 luOwnerB = lrContact.mEntityIdB.muValue >> BrnWorld::PropEntityID::KU_OWNER_BASE;

        if (luOwnerA == static_cast<u32>(BrnWorld::E_ENTITYTYPE_PROP) &&
            luOwnerB == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR))
        {
            const BrnWorld::PropEntityID lPropId(lrContact.mEntityIdA.muValue);
            luPropEntityIndex = lPropId.GetEntityIndex();

            const BrnWorld::PropEntityID lPropIdForPart(lrContact.mEntityIdA.muValue);
            (void)lPropIdForPart.GetPartIndex();          // console 0x8236BDE8 -- result discarded

            liRaceCarIndex = static_cast<s32>(
                (lrContact.mEntityIdB.muValue >> BrnWorld::PropEntityID::KU_ENTITY_INDEX_BASE) &
                0x3FFFu);                                 // extrwi r29, r11, 14,8
        }
        else if (luOwnerA == static_cast<u32>(BrnWorld::E_ENTITYTYPE_RACECAR) &&
                 luOwnerB == static_cast<u32>(BrnWorld::E_ENTITYTYPE_PROP))
        {
            liRaceCarIndex = static_cast<s32>(
                (lrContact.mEntityIdA.muValue >> BrnWorld::PropEntityID::KU_ENTITY_INDEX_BASE) &
                0x3FFFu);

            const BrnWorld::PropEntityID lPropId(lrContact.mEntityIdB.muValue);
            luPropEntityIndex = lPropId.GetEntityIndex();

            const BrnWorld::PropEntityID lPropIdForPart(lrContact.mEntityIdB.muValue);
            (void)lPropIdForPart.GetPartIndex();          // console 0x8236BE30 -- result discarded
        }

        if (liRaceCarIndex == static_cast<s32>(GetPlayerActiveRaceCarIndex()))
        {
            lpCrashScorer->DealWithHitProp(static_cast<u16>(luPropEntityIndex), lrContact.muFlags);
            lpStuntScorer->DealWithHitProp(static_cast<u16>(luPropEntityIndex), lrContact.muFlags);
        }
    }

    // ------------------------------------------------------------------------------------
    // LEG 2 -- the TRAFFIC contacts (0x8236BE74..0x8236BF3C). SHOWTIME ONLY: the console
    // re-reads the mode type here and re-tests 2 / 16 WITHOUT the stunt-attack arm the outer
    // gate allowed, so a stunt-attack run reaches leg 1 and stops.
    //
    // THIS IS THE HALF THE SHOWTIME CRASH COUNT DEPENDS ON, AND IT ONLY PRIMES THE COUNT.
    // DealWithHitTrafficCar records a RecentCrash and bumps miCurrentComboCount; it does NOT
    // touch maiNumCarsCrashed. What it produces is the victim's traffic index, pushed here onto
    // mShowtimePendingTrafficIndexStack for the PRE-world half (UpdateShowtimeMode @0x82380EF8,
    // GameStateModule_Showtime.cpp, landed FX-SHOWTIME2 2026-09-24) to turn into a traffic-type
    // request and finally a score. The stack's banner in BrnGameStateModule.h carries the chain.
    // ------------------------------------------------------------------------------------
    if (!lbShowtime)
    {
        return;
    }

    // 0x8236BEA4 -- the console tests IsFull() ONCE before the loop and again after every
    // successful push, so a full stack drops further victims silently rather than tripping
    // Push's own "!IsFull()" assert.
    if (mShowtimePendingTrafficIndexStack.IsFull())
    {
        return;
    }

    for (s32 liIndex = 0; liIndex < lpTrafficContacts->GetLength(); ++liIndex)
    {
        const BrnPhysics::ContactSpy::TrafficContact& lrContact =
            lpTrafficContacts->GetEvent(liIndex);

        u16 luVictimTrafficIndex = 0;

        if (lpCrashScorer->DealWithHitTrafficCar(GetPlayerActiveRaceCarIndex(),
                                                 lrContact.mEntityIdA,
                                                 lrContact.mEntityIdB,
                                                 &luVictimTrafficIndex))
        {
            mShowtimePendingTrafficIndexStack.Push(luVictimTrafficIndex);

            // [DIAG] NOT IN THE X360 BINARY. The bounded witness for this leg: one line per
            // detected victim (the push). Its consumer, UpdateShowtimeMode, prints the matching
            // `[showtime-score] pop` / `answer` lines under the same BRN_SHOWTIME_WATCH opt-in, so
            // a run can count pushes against pops and scores. Capped, so a long showtime cannot
            // flood the log.
            {
                static s32        siPushLinesLeft = 160;
            }

            if (mShowtimePendingTrafficIndexStack.IsFull())
            {
                break;
            }
        }
    }
}

// =================================================================================================
// [boost-wave2 2026-09-14] THE TWO IMPACT MESSAGE-ID TABLES.
//
// The console keeps them as two 9-entry (== E_IMPACT_COUNT) u64 arrays in .bss, filled by a CRT
// dynamic initialiser -- sub_82C4D590 writes the AGGRESSOR table at 0x82FAE228 and sub_82C4D620
// the VICTIM table at 0x82FAE1E0 -- and SendVehicleImpactMessages indexes whichever table it
// picked with `slwi r9, r11, 3 ; ldx r11, r9, r10`, i.e. table[impactType].
//
// ⚠️ THE IMAGE BYTES READ ZERO BY DEFINITION (.bss), so the CONTENTS come from the two
// initialiser bodies, not from a data dump -- each is a straight-line run of
// `CgsIDCompress(<literal>)` calls and `std`s of a zeroed register, and the store offsets give
// the index exactly:
//     aggressor (player hit rival):  [1] "GameTPaint"  [3] "GameGdSlam"  [4] "GameGdShunt"
//     victim    (rival hit player):  [1] "GameTPaint"  [3] "GameBdSlam"  [4] "GameBdShunt"
// Every other slot -- [0] NONE, [2] NUDGE, [5] BOOST_SLAM, [6] BOOST_SHUNT, [7] GRINDING,
// [8] RUBBING -- is stored as literal ZERO (`li r30, 0` then five/four `std r30/r11`), and slot
// [0] of each table is never stored at all, so it keeps its .bss zero. That is why a nudge or a
// boost-slam posts action 48 with a ZERO message id: the console really does post it, and the
// HUD-message consumer really does ignore a zero id. Reproduced exactly; not "tidied" into a
// skip, because the action id and its count are observable.
//
// The tables are built on first use rather than at static-init time for the reason this project
// records for every recovered dyn-init constant: a namespace-scope initialiser here would run in
// an order the console never had. One-shot, single-threaded build.
// =================================================================================================
namespace
{
    struct VehicleImpactMessageTables
    {
        CgsID maAggressorMessages[BrnPhysics::Vehicle::E_IMPACT_COUNT];
        CgsID maVictimMessages[BrnPhysics::Vehicle::E_IMPACT_COUNT];

        VehicleImpactMessageTables()
        {
            for (s32 liImpact = 0; liImpact < BrnPhysics::Vehicle::E_IMPACT_COUNT; ++liImpact)
            {
                maAggressorMessages[liImpact] = 0;
                maVictimMessages[liImpact]    = 0;
            }
            // sub_82C4D590 -- the table SendVehicleImpactMessages picks when the aggressor is
            // the local player (and posts alongside action 53).
            maAggressorMessages[BrnPhysics::Vehicle::E_IMPACT_TRADING_PAINT] =
                CgsIDCompress("GameTPaint");                      // std -> 0x82FAE230
            maAggressorMessages[BrnPhysics::Vehicle::E_IMPACT_SLAM] =
                CgsIDCompress("GameGdSlam");                      // std -> 0x82FAE240
            maAggressorMessages[BrnPhysics::Vehicle::E_IMPACT_SHUNT] =
                CgsIDCompress("GameGdShunt");                     // std -> 0x82FAE248
            // sub_82C4D620 -- the table for every other aggressor (posted alongside action 54).
            maVictimMessages[BrnPhysics::Vehicle::E_IMPACT_TRADING_PAINT] =
                CgsIDCompress("GameTPaint");                      // std -> 0x82FAE1E8
            maVictimMessages[BrnPhysics::Vehicle::E_IMPACT_SLAM] =
                CgsIDCompress("GameBdSlam");                      // std -> 0x82FAE1F8
            maVictimMessages[BrnPhysics::Vehicle::E_IMPACT_SHUNT] =
                CgsIDCompress("GameBdShunt");                     // std -> 0x82FAE200
        }
    };

    const VehicleImpactMessageTables& GetVehicleImpactMessageTables()
    {
        static VehicleImpactMessageTables lTables;
        return lTables;
    }
}

// =================================================================================================
// SendVehicleImpactMessages  @0x82381A00  -- TRADING PAINT / NUDGE / SLAM / SHUNT, THE SOURCE.
//
// ⭐⭐⭐ THIS FUNCTION HAD NO BODY IN THE TREE, and it is the ONLY producer of game actions 53 and
// 54 in the whole image. So every rival impact the physics layer detected -- and the physics
// layer HAS been posting world event 31 for it all along (VehicleManager::
// HandleRaceCarRaceCarContact, reconstructed at BrnVehicleManager.cpp:378 / :438) -- died at the
// game-state boundary. Downstream that meant: no OnPlayerAttacksRival boost award for slamming a
// rival (RaceCarEntityModule::HandleGameActions case 53, landed this wave), and no HUD message
// for trading paint / a good or bad slam / a good or bad shunt.
//
// SIGNATURE from the prologue (`mr r31, r4` / `mr r29, r5`): (this, lpEvent, lpActionQueue).
// The action queue is the caller's -- ProcessGameEvents passes its own r22 -- and it is the
// 13312-byte GameActionQueue, not the 1536-byte game-event queue.
//
// THE WHOLE BODY, 34 instructions:
//     record = { event->meImpactType, event->meAggressor, event->meVictim }    // 3 x lwz/stw
//     if (event->meAggressorActiveRaceCarIndex == GetPlayerActiveRaceCarIndex())
//          id = 53 (PLAYER_HIT_RIVAL); table = aggressor table
//     else id = 54 (RIVAL_HIT_PLAYER); table = victim table
//     messageId = table[event->meImpactType]                                   // ldx, 8-byte stride
//     AddEvent(actionQueue, &record, id, 12)
//     AddEvent(actionQueue, &messageId, 48 /* SHOW_HUD_MESSAGE */, 8)
//
// ⚠️ THE MESSAGE-ID LOAD HAPPENS BEFORE THE FIRST AddEvent (`ldx r11, r9, r10 ; std r11, var_40`
// at 0x82381A64..0x82381A68) and is UNGUARDED: the impact type indexes a 9-entry table with no
// range check, exactly as written below. E_IMPACT_COUNT is 9 and the producer only ever writes
// 1..8, so the console's own bound holds.
// =================================================================================================
void GameStateModule::SendVehicleImpactMessages(
        const GameStateModuleIO::VehicleImpactEvent* lpImpactEvent,
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(lpImpactEvent != 0, "lpVehicleImpactEvent != NULL");
    CGS_ASSERT(lpActionQueue != 0, CgsDev::Assert::MessageAt("lpActionQueue != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1149));
    if (lpImpactEvent == 0 || lpActionQueue == 0)
    {
        return;
    }

    // The console builds the 12-byte record on the stack from three straight `lwz`/`stw` pairs.
    // PlayerHitRivalAction and RivalHitPlayerAction are the same three fields (DWARF :3733-:3735
    // and :3750-:3752), so one record serves both arms -- which is what the console does too:
    // it fills var_38 once, before it knows which id it will post.
    GameStateModuleIO::PlayerHitRivalAction lAction;
    lAction.meImpactType =
        static_cast<BrnPhysics::Vehicle::EImpactType>(lpImpactEvent->meImpactType);
    lAction.meAggressorActiveRaceCarIndex =
        static_cast< ::EActiveRaceCarIndex>(lpImpactEvent->meAggressorActiveRaceCarIndex);
    lAction.meVictimActiveRaceCarIndex =
        static_cast< ::EActiveRaceCarIndex>(lpImpactEvent->meVictimActiveRaceCarIndex);

    const bool lbPlayerIsAggressor =
        (lpImpactEvent->meAggressorActiveRaceCarIndex
            == static_cast<s32>(GetPlayerActiveRaceCarIndex()));

    const VehicleImpactMessageTables& lrTables = GetVehicleImpactMessageTables();

    const s32   liActionId = lbPlayerIsAggressor
                                 ? static_cast<s32>(GameStateModuleIO::E_ACTION_PLAYER_HIT_RIVAL)
                                 : static_cast<s32>(GameStateModuleIO::E_ACTION_RIVAL_HIT_PLAYER);
    const CgsID lMessageId = lbPlayerIsAggressor
                                 ? lrTables.maAggressorMessages[lpImpactEvent->meImpactType]
                                 : lrTables.maVictimMessages[lpImpactEvent->meImpactType];

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                            liActionId, sizeof(lAction));

    GameStateModuleIO::ShowHudMessageAction lMessageAction;
    lMessageAction.mMessageId = lMessageId;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lMessageAction),
                            static_cast<s32>(GameStateModuleIO::E_ACTION_SHOW_HUD_MESSAGE),
                            sizeof(lMessageAction));

    // [DIAG] BRN_BOOST_TICKER_DIAG -- NOT IN THE X360 BINARY. The rung that separates "the
    // physics never reported an impact" from "it reported one and nothing paid out". Budgeted.
    // DELETE-WHEN-STABLE.
    {
        static s32        siImpactLines = 0;
    }
}

// ============================================================================================
// [FX-BRIDGES CC-11, 2026-09-24] GameStateModule::SendRouteRequestAction -- X360 0x82381DC8
// (DWARF BrnGameStateModule.h:648; locals lRouteRequestAction :5622, luAISectionIndex :5626,
// liIndex :5628). THE ROUTE QUESTION. Every checkpoint-to-checkpoint distance a race learns starts
// here: ModeManager::UpdateCheckpointDistanceRequests hands it the pair it wants measured, this
// resolves both ends to AI sections and posts action 50 for the AI module's route planner, whose
// answer comes back through BridgeWorldToGameState as event 174.
//
// THE ASM, in order:
//   0x82381DF4..0x82381E44  the two non-gating asserts, "lpRouteRequestEvent" (line 0x170B) and
//                           "lpOutputActionQueue" (0x170C);
//   0x82381E60              `li r19, 2` -- the loop runs over the record's KI_MAX_POINTS ends;
//   0x82381EA4..0x82381EB8  per end, the point type `lwz 0(r26)` (r26 = event + 0x20):
//                             0 LANDMARK   -> ProgressionManager::FindLandmarkAISectionIndex(
//                                             `ld 0(r20)`, the landmark id at event + 0x30 + 8i)
//                                             @0x82381FAC (r3 = gsm + 0xBB30 == &mProgressionManager);
//                             1 JUNCTION, 2 PLAYERPOS
//                                          -> the AI-sections resource (gsm + 181300, the const
//                                             ResourcePtr::operator-> @0x82367718) and its nearest
//                                             section to the end's position (sub_8267A588) @0x82381F98;
//                             anything else -> the streamed "Unknown route node type: <n>" assert
//                                             (line 0x1731), then the SAME position lookup;
//   0x82381FB4..0x82381FCC  the end's position (`lvx128 v0, r0, r29`) and the u16 section go into
//                           the action record (+0x00/+0x10, +0x20/+0x22);
//   0x82381FDC..0x82381FFC  +0x24 = the event id (`lhz 0x44(r30)`), +0x28 = the owner (the 3rd
//                           argument), then AddEvent(record, 50, 0x30).
// The console never writes +0x26..+0x27 or +0x2C..+0x2F of its stack record (residue); zeroed here.
// ============================================================================================
namespace
{
// BrnAI::AISectionsData::FindNearestAISection(Vector3) const -- X360 0x8267A588, DWARF
// AISectionsData.h:447: the brute-force overload (no point map), whose only caller is
// SendRouteRequestAction above. Its home is SharedClasses/AI (AISectionsData), which carries only
// the point-map overload @0x82676CC0 on this build; until the owner lands this one there it is
// reproduced here, instruction for instruction, over that type's public API:
//   0x8267A5AC  best = 0x7FFF (`li r26, 0x7FFF`)       0x8267A5B8  bestDistSq = flt_820A366C
//                                                     (image-read 0x7F7FFFFF == FLT_MAX)
//   0x8267A5B4  for i < muNumSections (+0x30), the index kept to 16 bits (`clrlwi 16`);
//   0x8267A5D4  GetAISection's own "luSectionIndex < muNumSections" assert (line 0x4B1);
//   0x8267A60C  skip a section whose flag byte (+0x17) has bit 0x01 (IsShortcut) or 0x40
//               (IsAIShortcut);
//   0x8267A62C  d = GetMiddle() - position, `vmsum3fp128` (x*x + y*y + z*z);
//   0x8267A648  strictly smaller (`fcmpu ; bge`) -> keep this index.
// DELETE-WHEN AISectionsData::FindNearestAISection(Vector3) const exists in SharedClasses/AI.
u16 FindNearestAISectionWithoutPointMap(const BrnAI::AISectionsData* lpAISectionsData, const Vector3& lPosition)
{
    u16 luNearestSectionIndex = 0x7FFF;
    f32 lfNearestDistanceSquared = 3.4028234663852886e+38f;   // flt_820A366C
    for (u16 luSectionIndex = 0; luSectionIndex < lpAISectionsData->muNumSections; ++luSectionIndex)
    {
        const BrnAI::AISection* lpSection = lpAISectionsData->GetAISection(luSectionIndex);
        if (lpSection->IsShortcut() || lpSection->IsAIShortcut())
        {
            continue;
        }
        const Vector3 lMiddle = lpSection->GetMiddle();
        const f32 lfDx = lMiddle.mV.x - lPosition.mV.x;
        const f32 lfDy = lMiddle.mV.y - lPosition.mV.y;
        const f32 lfDz = lMiddle.mV.z - lPosition.mV.z;
        const f32 lfDistanceSquared = lfDx * lfDx + lfDy * lfDy + lfDz * lfDz;
        if (lfDistanceSquared < lfNearestDistanceSquared)
        {
            luNearestSectionIndex    = luSectionIndex;
            lfNearestDistanceSquared = lfDistanceSquared;
        }
    }
    return luNearestSectionIndex;
}
}

void GameStateModule::SendRouteRequestAction(const GameStateModuleIO::LandmarkRouteRequestEvent* lpRouteRequestEvent,
                                             GameStateModuleIO::GameActionQueue*                  lpOutputActionQueue,
                                             BrnAI::RouteMapModuleIO::RequestOwner                leRequestOwner)
{
    typedef GameStateModuleIO::LandmarkRouteRequestEvent RouteRequest;

    CGS_ASSERT(lpRouteRequestEvent != 0, CgsDev::Assert::MessageAt("lpRouteRequestEvent", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5899));   // BrnGameStateModule.cpp:5899 (0x170B)
    CGS_ASSERT(lpOutputActionQueue != 0, CgsDev::Assert::MessageAt("lpOutputActionQueue", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5797));   // BrnGameStateModule.cpp:5900 (0x170C)

    GameStateModuleIO::RequestRouteInfoAction lRouteRequestAction;
    std::memset(&lRouteRequestAction, 0, sizeof(lRouteRequestAction));

    for (s32 liIndex = 0; liIndex < RouteRequest::KI_MAX_POINTS; ++liIndex)
    {
        u16 luAISectionIndex = 0;
        switch (lpRouteRequestEvent->mePointTypes[liIndex])
        {
        case RouteRequest::E_ROUTE_END_POINT_TYPE_LANDMARK:
            luAISectionIndex = mProgressionManager.FindLandmarkAISectionIndex(lpRouteRequestEvent->maLandmarkIDs[liIndex]);
            break;

        case RouteRequest::E_ROUTE_END_POINT_TYPE_JUNCTION:
        case RouteRequest::E_ROUTE_END_POINT_TYPE_PLAYERPOS:
            luAISectionIndex = FindNearestAISectionWithoutPointMap(mProgressionManager.GetAISectionsData(),
                                                                   lpRouteRequestEvent->maPositions[liIndex]);
            break;

        default:
            // The console streams the type after the text ("Unknown route node type: " << type << "\n")
            // into Assert::gpcMessageBuffer; the static head is passed straight through (the project
            // convention for streamed asserts). Non-gating: it then takes the position lookup.
            CGS_ASSERT(false, "Unknown route node type: ");                     // BrnGameStateModule.cpp:5937 (0x1731)
            luAISectionIndex = FindNearestAISectionWithoutPointMap(mProgressionManager.GetAISectionsData(),
                                                                   lpRouteRequestEvent->maPositions[liIndex]);
            break;
        }

        if (liIndex == 0)
        {
            lRouteRequestAction.mStartPosition       = lpRouteRequestEvent->maPositions[0];
            lRouteRequestAction.muStartSectionIndex  = luAISectionIndex;
        }
        else
        {
            lRouteRequestAction.mEndPosition         = lpRouteRequestEvent->maPositions[1];
            lRouteRequestAction.muEndSectionIndex    = luAISectionIndex;
        }
    }

    lRouteRequestAction.muEventId = lpRouteRequestEvent->mu16EventID;           // lhz 0x44(r30) -> +0x24
    lRouteRequestAction.miOwnerId = static_cast<s32>(leRequestOwner);           // arg_2C -> +0x28
    lpOutputActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRouteRequestAction),
                                  static_cast<s32>(GameStateModuleIO::E_ACTION_REQUEST_ROUTE_INFO),
                                  static_cast<s32>(sizeof(lRouteRequestAction)));   // li r5, 0x32 ; li r6, 0x30

    // [DIAG] NOT IN THE X360 BINARY -- BRN_ROUTE_INFO_DIAG: the question as posted (first 40).
    static s32 siRouteLinesLeft = 40;
}

}

// FOLDED FROM BrnGameStateModule_wBT_01.cpp
// GameStateModule::ProcessStreamingCompleteEvent -- one module finished streaming.
//
// ProcessGameEvents' case 9 hands it the StreamingCompleteEvent. It records the module's answer;
// while the module waits for streaming it finishes the wait once all four modules have answered,
// otherwise a GUI-screen answer clears the loading state. A race-car answer then also completes
// the car change the junkyard or the online car select is waiting on.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameEvents.h"                          // StreamingCompleteEvent
#include "GameSource/GameState/BrnGameActions.h"                         // SetLoadingStateAction
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // CarSelectManager::StreamingFinished
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"    // OnlineCarSelectManager::StreamingFinished

namespace BrnGameState
{

void GameStateModule::ProcessStreamingCompleteEvent(const GameStateModuleIO::StreamingCompleteEvent* lpStreamingCompleteEvent,
                                                    GameStateModuleIO::GameActionQueue*              lpActionQueue)
{
    CGS_ASSERT(lpStreamingCompleteEvent->meModule >= 0, CgsDev::Assert::MessageAt("lpStreamingCompleteEvent->meModule >= 0", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6346));
    CGS_ASSERT(lpStreamingCompleteEvent->meModule < GameStateModuleIO::StreamingCompleteEvent::E_MODULE_COUNT,
               CgsDev::Assert::MessageAt("lpStreamingCompleteEvent->meModule < GsmIO::StreamingCompleteEvent::E_MODULE_COUNT", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6347));

    mabModuleStreamingComplete[lpStreamingCompleteEvent->meModule] = true;

    bool lbAllModulesComplete = true;
    for (s32 liModule = 0; liModule < GameStateModuleIO::StreamingCompleteEvent::E_MODULE_COUNT; ++liModule)
    {
        if (!mabModuleStreamingComplete[liModule])
        {
            lbAllModulesComplete = false;
            break;
        }
    }

    if (mbWaitingForStreaming && lbAllModulesComplete)
    {
        FinishStreaming(lpActionQueue);
    }
    else if (!mbWaitingForStreaming &&
             lpStreamingCompleteEvent->meModule == GameStateModuleIO::StreamingCompleteEvent::E_MODULE_GUI_SCREEN)
    {
        GameStateModuleIO::SetLoadingStateAction lSetLoadingStateAction;
        lSetLoadingStateAction.mbStartLoading = false;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetLoadingStateAction),
                                GameStateModuleIO::E_ACTION_SET_LOADING_STATE,
                                static_cast<s32>(sizeof(lSetLoadingStateAction)));
    }

    if (mabModuleStreamingComplete[GameStateModuleIO::StreamingCompleteEvent::E_MODULE_RACE_CAR_ENTITY])
    {
        if (mCarSelectManager.IsInJunkyard())
        {
            if (mCarSelectManager.IsWaitingForStreaming())
            {
                CGS_ASSERT(lpStreamingCompleteEvent->meModule ==
                               GameStateModuleIO::StreamingCompleteEvent::E_MODULE_RACE_CAR_ENTITY,
                           "Signalled streaming complete from wrong module, get Rob C");
                mCarSelectManager.StreamingFinished(lpStreamingCompleteEvent->mUserId, lpActionQueue);
            }
        }
        else if (mOnlineCarSelectManager.IsInOnlineCarSelect() && mOnlineCarSelectManager.IsWaitingForStreaming())
        {
            CGS_ASSERT(lpStreamingCompleteEvent->meModule ==
                           GameStateModuleIO::StreamingCompleteEvent::E_MODULE_RACE_CAR_ENTITY,
                       "Signalled streaming complete from wrong module, get Rob C");
            mOnlineCarSelectManager.StreamingFinished(lpStreamingCompleteEvent->mUserId, lpActionQueue);
        }
    }
}

} // namespace BrnGameState

// FOLDED FROM BrnGameStateModule_wBT_04.cpp
// BrnGameState::GameStateModule::SendAllRivalryData -- the answer to the GUI's "all rivals" request
// (ProcessGameEvents case 101): every rival of the progression data, its car, and whether the
// player has met it yet, posted as action 188.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameActions.h"                         // RivalryOverviewAction
#include "GameSource/GameState/Progression/BrnProgressionManager.h"      // ProgressionManager
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::FindRival, KI_MAX_RIVAL_COUNT
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData::GetRival / GetRivalCount
#include "SharedClasses/Progression/BrnRival.h"                          // Rival::GetId / GetCarId

namespace BrnGameState
{

void GameStateModule::SendAllRivalryData(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(mProgressionManager.GetProgressionData(), CgsDev::Assert::MessageAt("mProgressionManager.GetProgressionData()", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 4822));
    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile, CgsDev::Assert::MessageAt("mProgressionManager.GetProfile()", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 4823));
    CGS_ASSERT(BrnProgression::Profile::KI_MAX_RIVAL_COUNT > mProgressionManager.GetProgressionData()->GetRivalCount(),
               CgsDev::Assert::MessageAt("BrnProgression::Profile::KI_MAX_RIVAL_COUNT > mProgressionManager.GetProgressionData()->GetRivalCount()", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 4824));

    GameStateModuleIO::RivalryOverviewAction lRivalryOverview;
    lRivalryOverview.miRivalsReturned = mProgressionManager.GetProgressionData()->GetRivalCount();

    for (s32 liIndex = 0; liIndex < lRivalryOverview.miRivalsReturned; ++liIndex)
    {
        const BrnProgression::Rival* lpRival = mProgressionManager.GetProgressionData()->GetRival(liIndex);
        // The console streams the index between "Rival " and the rest of this message.
        CGS_ASSERT(lpRival, "Rival does not exist! \n");

        lRivalryOverview.mRivalIDs[liIndex]    = lpRival->GetId();
        lRivalryOverview.mRivalCarIDs[liIndex] = lpRival->GetCarId();

        // A rival the profile already holds has been met; any other is not yet in play.
        if (lpProfile->FindRival(lpRival->GetId()) != 0)
        {
            lRivalryOverview.mRivalryStatus[liIndex] = GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_DRIVER;
        }
        else
        {
            lRivalryOverview.mRivalryStatus[liIndex] = GameStateModuleIO::RivalryOverviewAction::E_RIVALRY_STAGE_INVALID;
        }
    }

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRivalryOverview),
                            GameStateModuleIO::E_ACTION_ALL_RIVALRY_DATA_RESPONSE,
                            static_cast<s32>(sizeof(lRivalryOverview)));
}

}

// FOLDED FROM BrnGameStateModule_wBT_05.cpp
// BrnGameState::GameStateModule::OnEnterOnline -- ProcessGameEvents case 29 (E_EVENT_PREPARE_FOR_ONLINE).
// Before the online session takes over: leave a running offline mode, leave the junkyard, lift a
// training pause, reset the player's crash state and turn the road rules to their online mode.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // AddEvent
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // OutputBuffer
#include "GameSource/GameState/BrnGameActions.h"                         // ResetCrashingAction (9)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // IsInGameMode / IsOnlineGameMode / ExitCurrentMode
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"          // IsInJunkyard / ForceExitJunkyard
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // ForceUnpause
#include "GameSource/GameState/RoadRules/BrnRoadRulesManager.h"          // SetRoadRulesMode

namespace BrnGameState
{

void GameStateModule::OnEnterOnline(GameStateModuleIO::OutputBuffer*    lpOutput,
                                    GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // An offline mode is quit outright; the next-mode argument is the 18 "no successor" value.
    if (mModeManager.IsInGameMode() && !mModeManager.IsOnlineGameMode())
    {
        mModeManager.ExitCurrentMode(lpOutput, true,
                                     static_cast<GameStateModuleIO::EGameModeType>(ModeManager::KI_GAME_MODE_SLOTS));
    }

    if (mCarSelectManager.IsInJunkyard())
    {
        mCarSelectManager.ForceExitJunkyard(lpActionQueue, false);
    }

    // Inlined here on the console: action 151 when the latched tip pauses the game.
    mTrainingManager.ForceUnpause(lpActionQueue);

    GameStateModuleIO::ResetCrashingAction lResetCrashingAction;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetCrashingAction),
                            GameStateModuleIO::E_ACTION_RESET_CRASHING,
                            static_cast<s32>(sizeof(lResetCrashingAction)));

    mRoadRulesManager.SetRoadRulesMode(lpOutput, true);
}

}

// FOLDED FROM BrnGameStateModule_wG4_00.cpp
// GameStateModule -- the junkyard car carousel's selectable-car list and its pre-stream request.
//
//   GetListOfPlayerSelectableVehicles    -- every car the player may pick right now (offline: the
//                                           owned race cars, sponsor cars once the progression rank
//                                           reaches their unlock rank; online: the cars inside the
//                                           event's vehicle-class limit).
//   RequestStreamingForVehicleSelection  -- finds the shown car in that list and posts game action
//                                           69 (CarSelectionRequestStreamingAction) carrying the car
//                                           plus up to seven neighbours on either side, so the world
//                                           streams the cars the carousel can reach next.
//
// Action 69's consumer is RaceCarEntityModule::HandleSelectionRequestStreamingAction: it keeps the
// slots that already hold a requested model, frees the rest, and hands each missing model to the
// race-car streamer with the request's per-entry priority.

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameSource/GameState/BrnGameActions.h"                       // CarSelectionRequestStreamingAction (action 69)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::IsCarUnlocked / GetProgressionRank
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::FindCar
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"     // CarData::GetUnlockType
#include "GameSource/GameState/CarSelect/BrnOnlineCarSelectManager.h"   // the online car-select limits
#include "GameSource/BurnoutConstants.h"                                // E_ACTIVE_RACE_CAR_INDEX_COUNT
#include "SharedClasses/DataLists/VehicleList.h"
#include "SharedClasses/DataLists/VehicleListEntry.h"
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsIDUnCompress
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <stdlib.h>   // getenv (BRN_CARSEL_DIAG)

namespace BrnGameState
{

// ============================================================================
// GetListOfPlayerSelectableVehicles
//
// Walks the whole vehicle list. A car is a candidate when its race-vehicle flag (gameplay data
// flags bit 0, read through IsTrophyCar()) is set, it is not a colour livery of another car, and
// -- online -- its vehicle class (+0x99) is inside the event's class limit (or the host chooses
// for everyone). Offline the candidate must be in the profile's garage; a sponsor car also needs
// the progression rank to have reached its unlock rank. Online it must be unlocked (or the host
// chooses). Online, the first race vehicle at exactly the class limit that is a base car (no
// parent) is appended last when it is not already in the list.
// ============================================================================
void GameStateModule::GetListOfPlayerSelectableVehicles(CgsContainers::Array<CgsID, 128>& lrCars)
{
    s32  liVehicleClassLimit        = 9;
    bool lbFoundVehicleAtClassLimit = false;

    lrCars.Clear();

    const bool lbIsOnlineCarSelect = mOnlineCarSelectManager.IsInOnlineCarSelect();
    if (lbIsOnlineCarSelect)
    {
        liVehicleClassLimit = mOnlineCarSelectManager.GetVehicleClassLimit();
    }

    for (s32 liCarIndex = 0; liCarIndex < mpVehicleList->GetVehicleCount(); ++liCarIndex)
    {
        const BrnResource::VehicleListEntry* lpVehicleData = mpVehicleList->GetVehicleData(liCarIndex);
        const bool lbInclude = !lpVehicleData->IsLiveryColour();

        s32  liVehicleClass;
        bool lbCorrectVehicleClass;
        if (lbIsOnlineCarSelect)
        {
            liVehicleClass        = lpVehicleData->GetUnlockRank();
            lbCorrectVehicleClass = liVehicleClass <= liVehicleClassLimit
                                 || mOnlineCarSelectManager.GetHostChoiceAndNotHost();
        }
        else
        {
            liVehicleClass        = 9;
            lbCorrectVehicleClass = true;
        }

        if (!lpVehicleData->IsTrophyCar() || !lbInclude || !lbCorrectVehicleClass)
        {
            continue;
        }

        const CgsID lCarId = lpVehicleData->GetId();
        if (lbIsOnlineCarSelect)
        {
            if (mProgressionManager.IsCarUnlocked(lCarId) || mOnlineCarSelectManager.GetHostChoiceAndNotHost())
            {
                lrCars.Append(lCarId);
            }
        }
        else
        {
            const BrnProgression::CarData* lpCarData = mProgressionManager.GetProfile()->FindCar(lCarId);
            if (lpCarData != 0)
            {
                if (lpCarData->GetUnlockType() != BrnProgression::CarData::E_UNLOCK_TYPE_SPONSOR)
                {
                    lrCars.Append(lCarId);
                }
                else if (mProgressionManager.GetProgressionRank() >= lpVehicleData->GetUnlockRank())
                {
                    lrCars.Append(lCarId);
                }
            }
        }

        // Computed on every included car exactly as the console does; nothing in this build
        // reads it afterwards.
        if (!lbFoundVehicleAtClassLimit && lbIsOnlineCarSelect
            && (mProgressionManager.IsCarUnlocked(lCarId) || mOnlineCarSelectManager.GetHostChoiceAndNotHost()))
        {
            lbFoundVehicleAtClassLimit = (liVehicleClassLimit == liVehicleClass);
        }
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "DONE\n";
    }

    if (lbIsOnlineCarSelect)
    {
        for (s32 liCarIndex = 0; liCarIndex < mpVehicleList->GetVehicleCount(); ++liCarIndex)
        {
            const BrnResource::VehicleListEntry* lpVehicleData = mpVehicleList->GetVehicleData(liCarIndex);
            if (lpVehicleData->IsTrophyCar()
                && lpVehicleData->GetUnlockRank() == liVehicleClassLimit
                && lpVehicleData->GetParentId() == 0)
            {
                if (!lrCars.Contains(lpVehicleData->GetId()))
                {
                    lrCars.Append(lpVehicleData->GetId());
                }
                break;
            }
        }
    }
}

// ============================================================================
// RequestStreamingForVehicleSelection
//
// Request layout (action 69): entry 0 is the shown car (flags 5, priority 0). The next car in
// the list goes to entry 1 (flags 2, priority 1); when there are cars behind the shown one,
// entry 2 is kept for the first of them and the cars ahead are capped at four. The cars ahead
// carry flags 2, the cars behind flags 0; every neighbour's priority is its distance from the
// shown car. At most KI_MAX_ACTIVE_RACE_CARS (8) entries.
// ============================================================================
void GameStateModule::RequestStreamingForVehicleSelection(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                                          CgsID lCurrentVehicle)
{
    CgsContainers::Array<CgsID, 128> lCars;
    lCars.Construct();
    GetListOfPlayerSelectableVehicles(lCars);

    if (lCars.GetLength() < 2)
    {
        return;
    }

    GameStateModuleIO::CarSelectionRequestStreamingAction lRequestAction;
    lRequestAction.maCars[0]            = lCurrentVehicle;
    s32 liCount                         = 1;
    lRequestAction.maiPriorities[0]     = 0;
    lRequestAction.mauExtraInfoFlags[0] = 5;

    s32 liCurrentVehicleIndex = lCars.FindFirstInstanceOf(lCurrentVehicle);
    if (liCurrentVehicleIndex == -1)
    {
        // A colour livery is not in the list itself: look up its parent car instead.
        const BrnResource::VehicleListEntry* lpCurrentVehicleData = mpVehicleList->GetVehicleData(lCurrentVehicle);
        CGS_ASSERT(lpCurrentVehicleData != 0, CgsDev::Assert::MessageAt("lpCurrentVehicleData != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 7461));
        if (lpCurrentVehicleData->IsLiveryColour())
        {
            CGS_ASSERT(lpCurrentVehicleData->GetParentId() != 0,
                       CgsDev::Assert::MessageAt("lpCurrentVehicleData->GetParentId() != kCGSID_NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 7465));
            liCurrentVehicleIndex = lCars.FindFirstInstanceOf(lpCurrentVehicleData->GetParentId());
        }
    }

    // The console streams "<id> was not found in the vehicle list" into the assert buffer.
    char lacUncompressedID[KI_CGSID_STRING_LEN];
    CgsIDUnCompress(lCurrentVehicle, lacUncompressedID);
    CGS_ASSERT(liCurrentVehicleIndex != -1, " was not found in the vehicle list");

    const u32 luCurrentVehicleIndex = static_cast<u32>(liCurrentVehicleIndex);
    const u32 luMaxNeighbours       = E_ACTIVE_RACE_CAR_INDEX_COUNT - 1;
    const u32 luMaxCountPerSide     = E_ACTIVE_RACE_CAR_INDEX_COUNT / 2;

    u32 luNextCount = 0;
    if (lCars.GetLength() != 0)
    {
        luNextCount = lCars.GetLength() - luCurrentVehicleIndex - 1;
        if (luNextCount >= luMaxNeighbours)
        {
            luNextCount = luMaxNeighbours;
        }
    }
    u32 luPrevCount = luCurrentVehicleIndex;
    if (luPrevCount >= luMaxNeighbours)
    {
        luPrevCount = luMaxNeighbours;
    }

    if (luNextCount != 0)
    {
        lRequestAction.maCars[1]            = lCars.GetItem(luCurrentVehicleIndex + 1);
        lRequestAction.maiPriorities[1]     = 1;
        lRequestAction.mauExtraInfoFlags[1] = 2;
        liCount = 2;

        u32 luSlot = 2;
        if (luPrevCount != 0)
        {
            if (luNextCount >= luMaxCountPerSide)
            {
                luNextCount = luMaxCountPerSide;
            }
            luSlot = 3;   // entry 2 belongs to the first car behind
        }

        for (u32 luIndex = 2; luIndex <= luNextCount; ++luIndex, ++luSlot)
        {
            ++liCount;
            lRequestAction.maCars[luSlot]            = lCars.GetItem(luCurrentVehicleIndex + luIndex);
            lRequestAction.mauExtraInfoFlags[luSlot] = 2;
            lRequestAction.maiPriorities[luSlot]     = static_cast<s8>(luIndex);
            CGS_ASSERT(liCount <= E_ACTIVE_RACE_CAR_INDEX_COUNT, "liCount <= BrnWorld::KI_MAX_ACTIVE_RACE_CARS");
        }
    }

    if (luPrevCount != 0)
    {
        const u32 luSlot = (luNextCount != 0) ? 2 : 1;
        lRequestAction.maCars[luSlot]            = lCars.GetItem(luCurrentVehicleIndex - 1);
        lRequestAction.mauExtraInfoFlags[luSlot] = 0;
        lRequestAction.maiPriorities[luSlot]     = 1;
        ++liCount;

        if (luPrevCount > 1)
        {
            u32 luIndex = 1;
            do
            {
                lRequestAction.maCars[liCount] = lCars.GetItem(luCurrentVehicleIndex - luIndex - 1);
                ++luIndex;
                lRequestAction.mauExtraInfoFlags[liCount] = 0;
                lRequestAction.maiPriorities[liCount]     = static_cast<s8>(luIndex);
                ++liCount;
            } while (liCount != E_ACTIVE_RACE_CAR_INDEX_COUNT && luIndex < luPrevCount);
        }
    }

    lRequestAction.miCount = liCount;
    CGS_ASSERT(liCount <= E_ACTIVE_RACE_CAR_INDEX_COUNT, "liCount <= BrnWorld::KI_MAX_ACTIVE_RACE_CARS");

    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequestAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECTION_REQUEST_STREAMING,
                            sizeof(lRequestAction));
}
}

// FOLDED FROM GameStateModule_gDC_00.cpp
// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_gDC_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the
// module's other committed bodies live in BrnGameStateModule.cpp, GameStateModule_gUI_00.cpp,
// GameStateModule_gSR_00.cpp, GameStateModule_gRR_00.cpp and GameStateModule_gTD_00.cpp).
//
// [link-closure lane P3 2026-09-03] ONE body: the module-level "is this active-race-car slot still
// a live car" predicate that BrnGameStateModule.h:1110 has declared -- and left unbodied -- since
// the PaybackManager wave. It is the last game-side undefined symbol standing between
// BrnPaybackManager.cpp and a link. BrnGameStateModule.cpp is conductor-owned, so the body lands
// here rather than there; the declaration is untouched.
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::GetScoringSystem
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem::GetCarData / CarData::GetNetworkPlayerID
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"             // BrnNetwork::NetworkPlayerID (== s32)

namespace BrnGameState
{

namespace
{
    // CgsNetwork::K_INVALID_PLAYER_ID stand-in -- the same -1 sentinel, spelled the same way, as
    // BrnScoringSystem_Lookup.cpp's local (no committed home for the constant).
    const BrnNetwork::NetworkPlayerID K_INVALID_PLAYER_ID = -1;
}

// ----------------------------------------------------------------------------------------------
// GameStateModule::IsActiveRaceCarStillPresent   (declared BrnGameStateModule.h:1110)
//
// NO out-of-line X360 symbol: this is a de-inlined name for the guard the console folds into its
// callers. The fold that pins it is BrnGameState::PaybackManager::HandleHavingPayback @0x82397B30,
// whose first four instructions are the whole predicate:
//
//     0x82397B4C  lwz  r11, 0x268(r31)     ; PaybackManager::mpGameStateModule
//     0x82397B50  lwz  r4,  0x244(r31)     ; mePaybackVictimRaceCarIndex
//     0x82397B54  addi r3,  r11, 0x1DD0    ; == the module's ScoringSystem
//     0x82397B58  bl   sub_8231DCD0        ; ScoringSystem::GetCarData(EActiveRaceCarIndex) (const twin)
//     0x82397B5C  cmplwi r3, 0   ; beq  -> "gone"
//     0x82397B64  lwz  r11, 0x148(r3)      ; CarData::mNetworkPlayerID
//     0x82397B68  cmpwi r11, -1  ; bne  -> "still present" (the function returns immediately)
//                                ; eq   -> falls into the "the car has left the game" teardown
//
// so: present == (GetCarData(idx) != NULL) && (that record's network player id != -1).
//
// gsm+0x1DD0 (7632) is NOT a module member of its own -- it is mModeManager.GetScoringSystem():
// mModeManager sits at gsm+4128 and ScoringSystem at ModeManager+0xDB0 (3504), and 4128 + 3504 ==
// 7632 exactly (the same identity BrnGameStateModule.cpp:274 records for AchievementManagerBase::
// Construct's `a1 + 7632` argument). Reached BY NAME here, never by that offset.
//
// CarData+0x148 is mNetworkPlayerID: sub_8231DCD0's own search matches the index against
// element+0x144 (its loop base is this+0x5044 against an element base of this+0x4F00), which is
// meRaceCarIndex, and mNetworkPlayerID is the member the DWARF declares immediately after it.
//
// THE OVERLOAD IS PINNED THROUGH A MEMBER POINTER: ScoringSystem::GetCarData is overloaded on
// EActiveRaceCarIndex and on BrnNetwork::NetworkPlayerID (== s32), so naming the by-active-index
// overload's exact type keeps a silent bind to the NetworkPlayerID overload a compile error.
// The CONST twin (0x8231DCD0) is the one the console calls and the one this const
// method can call.
// ----------------------------------------------------------------------------------------------
bool GameStateModule::IsActiveRaceCarStillPresent(::EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    typedef const CarData* (ScoringSystem::*ConstCarDataByActiveIndexFn)(::EActiveRaceCarIndex) const;
    const ConstCarDataByActiveIndexFn lpfnGetCarData = &ScoringSystem::GetCarData;

    const ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
    const CarData*       lpCarData       = (lpScoringSystem->*lpfnGetCarData)(leActiveRaceCarIndex);

    return (lpCarData != NULL) && (lpCarData->GetNetworkPlayerID() != K_INVALID_PLAYER_ID);
}

}

// FOLDED FROM GameStateModule_gRR_00.cpp
// b5-decomp/src/GameSource/GameState/GameStateModule_gRR_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the
// module's other committed bodies live in BrnGameStateModule.cpp, GameStateModule_gUI_00.cpp
// and GameStateModule_gSR_00.cpp).
//
// ============================================================================================
// ROAD RAGE WAVE, AGENT C -- THE TAKEDOWN FEED INTO GAME STATE.
//
//   GameStateModule::ProcessTakedownEvents  @0x8238FC50   (sole xref-to: PreWorldUpdate @0x823A5328)
//
// This is the per-frame drain of the takedown-event queue: for every takedown the PLAYER
// scored it feeds the road-rage scorer (ScoringSystem::OnPlayerDoesATakedown ->
// RoadRageModeScoring::IncrementPlayerNumTakedowns -- THE road-rage counter), the profile /
// progression tallies, the developer-challenge and achievement hooks, the network telemetry
// hook, and (for any takedown the player SUFFERED in Marked Man) the "you got taken down"
// training tip.
//
// SIGNATURE FROM THE ASM (0x8238FC58..0x8238FC70), the DWARF shape confirmed:
//     r3 this / r4 lpGameActionQueue / r5 lpTakedownEventQueue / r6 lpOutputBuffer
//   DWARF: void ProcessTakedownEvents(InputBuffer::GameActionQueue*,
//                                     const InputBuffer::TakedownEventQueue*,
//                                     GameStateModuleIO::OutputBuffer*);
// The pseudocode's `int result` first parameter is Hex-Rays reusing r3; there is no such argument.
//
// RECORD MAP (TakedownEvent, 40 bytes on the console, BrnTakedownManagerTypes.h):
//     +0x00 meAggressorIndex   +0x04 meVictimIndex   +0x18 meType
//     +0x20 miTakedownChainCount   +0x24 mbMarkedManTakeDown
//
// The original three-argument body reads the module's cached simulation Time at
// gsm+208368. PreWorldUpdate calls this before EmmPreWorldUpdate refreshes that
// cache; CopyScoringDataToOutput reads the refreshed cache later in the same frame.
//
// Every leg of the console body is live: the progression tally, the achievement hooks, the
// developer-challenge hooks and the telemetry posts all reach real bodies.
// ============================================================================================
#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Module/CgsEventQueue.h"                // CgsModule::EventQueue<TakedownEvent,8>
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface / Time

#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer / GameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                        // E_ACTION_NETWORK_CAUGHT_FEVER / E_ACTION_SEND_TELEMETRY
#include "GameSource/GameState/BrnTakedownType.h"                       // ETakedownType
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h" // TakedownEvent
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::GetScoringSystem / GetCurrentGameModeType
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem::GetCarData / OnPlayerDoesATakedown, CarData::HasFever
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"    // the Marked Man tip gauntlet
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::GetProfile
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::HasPlayerSeenTrainingType / the online vertical tally
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"           // TelemetryData / ETelemetryHook (the P4 post)
#include "SharedClasses/Progression/BrnTrainingTypes.h"                 // E_TRAINING_TYPE_TAKEN_DOWN_IN_MARKED_MAN
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // GetRivalId

namespace BrnGameState
{

namespace
{
    // flt_8200426C -- the console's 5.0 s "settle" gap between two training tips, the same literal
    // BrnDriveThruManager.cpp names KF_TRAINING_TIP_SETTLE_TIME.
    const f32 KF_TRAINING_TIP_SETTLE_TIME = 5.0f;

}

// ============================================================================
// ProcessTakedownEvents -- X360 0x8238FC50, whole. Console body in order, per event
// (`TakedownEvent_::GetEvent(queue, i)` @0x8238FCEC, 40-byte stride):
//
//   0x8238FCFC  if (event.meAggressorIndex == GetPlayerActiveRaceCarIndex())      -- the player's kill
//   0x8238FD4C    ldx r6 = gsm + 0x3BE10 + 8*victim   == mLastActiveRaceCarInterface.maRivalIds[victim]
//                 (interface base 235488 + 9776; the two :824/:825 range asserts are GetRivalId's)
//   0x8238FD58    if (!(mpCurrentGameMode && mpCurrentGameMode->IsOnline()))       -- gsm+0x1DB8 / mode+0xAC
//   0x8238FD98      ProgressionManager::OnTakedownTo(actionQ, event.meType, rivalId,
//                                                    meCurrentGameModeType == E_MODE_MARKED_MAN)
//                 else
//   0x8238FDA8      lpCarData = mScoringSystem.GetCarData(victim)   (0x8231DCD0, the const twin)
//   0x8238FDBC      CGS_ASSERT(lpCarData, "lpCarData")               (BrnGameStateModule.cpp:4940)
//   0x8238FDD0      if (lpCarData->HasFever())                       (CarData+0x155)
//   0x8238FDE0        if (!IsAchievementEarnt(49)) AchievementEarnt(49)  == OnCaughtFever @0x8235B590 [P2]
//   0x8238FE30        actionQ.AddEvent(<1 byte>, 238 /*E_ACTION_NETWORK_CAUGHT_FEVER*/, 1)
//   0x8238FE54    mScoringSystem.OnPlayerDoesATakedown(gsm+208368 /*Time*/, actionQ)
//   0x8238FE64    if (!IsAchievementEarnt(14) && event.miTakedownChainCount >= 10)
//                   AchievementEarnt(14)                              == AchievementManagerBase::OnTakedownChain
//   0x8238FEB0    mDeveloperChallengeManager.OnTakedownChain(event.miTakedownChainCount)      [P3]
//   0x8238FEC0    mDeveloperChallengeManager.OnTakedown(meCurrentGameModeType, victim)         [P3]
//   0x8238FEF0    if (!online) mAchievementManager.OnTakedown()                                [P2]
//   0x8238FEF4    telemetry hook: T_BONE(2) -> 13, VERTICAL(3) -> 11 (+ online: profile online
//                 vertical tally++ @gsm+48700 == Profile+412), else 10; AddParameter(player pos);
//                 actionQ.AddEvent(<20 bytes>, 228 /*E_ACTION_SEND_TELEMETRY*/, 20);
//                 if (event.mbMarkedManTakeDown) a second hook 14, same action
//   0x8238FFC4  else: the two :824/:825 range asserts on meAggressorIndex (an inlined interface
//               accessor whose value is dead; only its asserts survive)
//   0x82390004  BOTH ARMS: if (event.meVictimIndex == player && mode == E_MODE_MARKED_MAN &&
//               !tip pending && !picture paradise && IsTipAllowedInGameMode(56) &&
//               !profile->HasPlayerSeenTrainingType(56) && since-last-tip >= 5.0)  RequestTip(56)
//
// The loop re-reads the queue length every iteration (0x823900D0); nothing in the body appends
// to that queue, so a hoisted length would be equivalent -- kept as the console has it.
// ============================================================================
void GameStateModule::ProcessTakedownEvents(
        GameStateModuleIO::GameActionQueue*             lpGameActionQueue,
        const CgsModule::EventQueue<TakedownEvent, 8>*  lpTakedownEventQueue,
        GameStateModuleIO::OutputBuffer*                lpOutputBuffer)
{
    for (s32 liEvent = 0; liEvent < lpTakedownEventQueue->GetLength(); ++liEvent)
    {
        const TakedownEvent& lrEvent = lpTakedownEventQueue->GetEvent(liEvent);

        // ---- the player's own kill --------------------------------------------------------
        if (static_cast<s32>(lrEvent.meAggressorIndex) == static_cast<s32>(GetPlayerActiveRaceCarIndex()))
        {
            const ::EActiveRaceCarIndex leVictimIndex =
                static_cast<::EActiveRaceCarIndex>(lrEvent.meVictimIndex);

            // `ldx r6, (victim + 0x77C2) << 3, this` == the interface's maRivalIds[victim]; the
            // :824/:825 asserts baked here are GetRivalId's own and are reproduced by the call.
            const CgsID lVictimRivalId = mLastActiveRaceCarInterface.GetRivalId(leVictimIndex);

            // gsm+0x1DB8 (mModeManager.mpCurrentGameMode) then mode+0xAC (GameMode::IsOnline) --
            // exactly what the out-of-line IsOnlineGameMode @0x823116D0 evaluates.
            const bool lbOnlineMode = IsOnlineGameMode();

            if (!lbOnlineMode)
            {
                // The callee tallies the takedown into the profile and asks for the aggression /
                // generic takedown training tip. The action queue and the rival id ride the
                // console's argument registers and are DEAD in the callee body -- passed anyway
                // so the call keeps its declared shape.
                mProgressionManager.OnTakedownTo(
                    lpGameActionQueue,
                    lrEvent.meType,
                    lVictimRivalId,
                    GetCurrentGameModeType() == GameStateModuleIO::E_MODE_MARKED_MAN);
            }
            else
            {
                // The console calls the CONST GetCarData twin (0x8231DCD0) on the embedded
                // ScoringSystem (this+0x1DD0 == mModeManager + 0xDB0).
                // ⚠️ THE OVERLOAD IS PINNED THROUGH A MEMBER POINTER. ScoringSystem::GetCarData is
                // overloaded on EActiveRaceCarIndex and on BrnNetwork::NetworkPlayerID (== s32);
                // naming the by-active-index overload's exact type keeps a silent bind to the
                // NetworkPlayerID overload a compile error.
                // (The non-const twin @0x8231DC18 is used because the tree's CarData::HasFever is
                // declared non-const; the two twins are the same search.)
                typedef CarData* (ScoringSystem::*CarDataByActiveIndexFn)(::EActiveRaceCarIndex);
                const CarDataByActiveIndexFn lpfnGetCarData = &ScoringSystem::GetCarData;
                ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
                CarData* lpCarData = (lpScoringSystem->*lpfnGetCarData)(leVictimIndex);
                CGS_ASSERT(lpCarData != 0, CgsDev::Assert::MessageAt("lpCarData", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 4940));   // BrnGameStateModule.cpp:4940

                // [GUARD] the console asserts and then dereferences anyway; a null here is a crash
                // in a retail build. Early-out added behind the reproduced assert, as the sibling
                // partfiles do.
                //
                // `lbz r11, 0x155(r31)` == CarData+341. The tree's DWARF-ordered CarData puts
                // miCurrentCheckPoint (s8) at +341 and mbHasFever at +342; the action the console
                // [verify V2 2026-09-02] CORRECTION to the two sentences around this line: the tree
                // order is mbIsEliminated +339 / miCurrentCheckPoint +340 / mbHasFever +341, pinned by
                // BrnModeManager_Prepare.cpp:673-681 (PrepareForMode @0x82342930 stores mabPlayerHasFever
                // at car+341) and two Scoring bodies -- so +341 IS mbHasFever and there is no off-by-one.
                // Reads here are by name; nothing behavioural depends on the old claim.
                // posts on this byte -- E_ACTION_NETWORK_CAUGHT_FEVER -- pins its MEANING as "the
                // victim had fever", so the X360 record is one byte off the DecFIGS order here (a
                // merge-window delta of the CarData tail). Read BY NAME as HasFever; the layout
                // question is the ScoringSystem TU's, named here rather than papered over.
                if (lpCarData != 0 && lpCarData->HasFever())
                {
                    // `add r31, r29, r26`, r26 == +0x2C5B0, is the embedded achievement manager,
                    // and the vtable pair on id 49 is OnCaughtFever inlined. Called out of line
                    // here, as the sibling partfiles do.
                    mAchievementManager.OnCaughtFever();

                    // `li r6, 1 / li r5, 0xEE` -- one byte, never initialised by the console (an
                    // empty record); zeroed here so the payload is deterministic.
                    u8 lacCaughtFever[1] = { 0 };
                    lpOutputBuffer->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lacCaughtFever),
                        GameStateModuleIO::E_ACTION_NETWORK_CAUGHT_FEVER, 1);
                }
            }

            // ---- THE ROAD-RAGE COUNTER ------------------------------------------------------
            // 0x8238FE34..0x8238FE54: `lwz r10, 0(this+0x32DF0) / lfs f0, 4(...)` is the 8-byte
            // CgsSystem::Time at gsm+208368 (the sim-timer copy's mTime), passed BY VALUE.
            // See the signature FLAG in the banner for where the PC "now" comes from.
            mModeManager.GetScoringSystem()->OnPlayerDoesATakedown(
                mTimerStatusInterface.GetSimTimerStatus()->GetTime(), lpGameActionQueue);

            // ---- the chain / developer-challenge / achievement hooks ------------------------
            const s32 liTakedownChainCount = lrEvent.miTakedownChainCount;   // event+0x20

            // The next span is AchievementManagerBase::OnTakedownChain inlined --
            // `if (!IsAchievementEarnt(14) && chain >= 10) AchievementEarnt(14)`. The id and the
            // threshold live with the body, in the achievement-manager base home.
            mAchievementManager.OnTakedownChain(liTakedownChainCount);

            // The developer-challenge hooks (this+0x2D570 == mDeveloperChallengeManager).
            mDeveloperChallengeManager.OnTakedownChain(liTakedownChainCount);                       // r4 = event+0x20
            mDeveloperChallengeManager.OnTakedown(static_cast<s32>(GetCurrentGameModeType()),       // r4 = gsm+0x1DB4
                                                  static_cast<u32>(lrEvent.meVictimIndex));         // r5 = event+4

            // The console re-evaluates the online test inline off mModeManager.mpCurrentGameMode;
            // lbOnlineMode above is that same pair of reads.
            if (!lbOnlineMode)
            {
                mAchievementManager.OnTakedown();
            }

            // ---- the telemetry hook ----------------------------------------------------------
            // 0x8238FEF4..0x8238FF5C selects the hook id from the takedown type: 13 for
            // E_TAKEDOWN_T_BONE, 11 for E_TAKEDOWN_VERTICAL, 10 otherwise. The VERTICAL arm also
            // bumps the profile's online vertical-takedown tally when the mode is online
            // (`addis/addi -> gsm+0x10000-0x4360 == gsm+48288 (Profile) ; lwz/addi/stw 0x19C`).
            // The tally is a real profile store; the telemetry post below is live too.
            if (lrEvent.meType == E_TAKEDOWN_VERTICAL && lbOnlineMode)
            {
                mProgressionManager.GetProfile()->IncrementTotalOnlineVerticleTakedownCount();
            }

            // TelemetryData is the 20-byte record (ETelemetryHook + char[16]); hook 10 default,
            // 13 T-bone (@0x8238FF58), 11 vertical (@0x8238FF14); AddParameter(Vector3) = sub_8236A8B8.
            // r27 == the lpGameActionQueue parameter (`mr r27, r4` @0x8238FC6C), not the output buffer's.
            {
                BrnNetwork::ETelemetryHook leHook = BrnNetwork::E_TELEMETRY_TAKEDOWN;                        // li 0xA @0x8238FF0C
                if (lrEvent.meType == E_TAKEDOWN_T_BONE)        leHook = BrnNetwork::E_TELEMETRY_TAKEDOWN_TBONE;    // 0xD
                else if (lrEvent.meType == E_TAKEDOWN_VERTICAL) leHook = BrnNetwork::E_TELEMETRY_TAKEDOWN_VERTICAL; // 0xB
                BrnNetwork::BrnNetworkModuleIO::TelemetryData lTelemetry;
                lTelemetry.Construct(leHook);                                                      // stw var_D0 / stb 0 var_CC
                lTelemetry.AddParameter(mLastActiveRaceCarInterface.GetPlayerPosition());          // sub_823102F0 -> sub_8236A8B8
                lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTelemetry),
                                            GameStateModuleIO::E_ACTION_SEND_TELEMETRY, static_cast<s32>(sizeof(lTelemetry)));  // 0xE4, 0x14
                if (lrEvent.mbMarkedManTakeDown)                                                  // lbz 0x24(r28)
                {
                    BrnNetwork::BrnNetworkModuleIO::TelemetryData lMarkedMan;
                    lMarkedMan.Construct(BrnNetwork::E_TELEMETRY_TAKEDOWN_MARKED_MAN);             // li 0xE / stb 0, no parameter
                    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lMarkedMan),
                                                GameStateModuleIO::E_ACTION_SEND_TELEMETRY, static_cast<s32>(sizeof(lMarkedMan)));
                }
            }
        }
        else
        {
            // 0x8238FFC4..0x82390000: the console range-asserts meAggressorIndex against
            // BrnRaceCarEntityModuleOutputInterface.h:824/825 on this arm too -- the residue of an
            // inlined interface accessor whose value is dead. Only the asserts survive; reproduced.
            CGS_ASSERT(static_cast<s32>(lrEvent.meAggressorIndex) >= ::E_ACTIVE_RACE_CAR_INDEX_0,
                       "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(static_cast<s32>(lrEvent.meAggressorIndex) < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                       "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
        }

        // ---- BOTH ARMS: the "you were taken down" Marked Man tip -----------------------------
        // 0x82390004..0x823900C0. gsm+0xB630 (46640) is the training manager: +0 meTrainingState
        // (== INACTIVE), +0x14 mbInPictureParadise, +8 mpProgressionManager (+0x170 == Profile,
        // asserted "lpProfile" at BrnTrainingManager.cpp:382), +0x18 mfLastMessageFinishedTime;
        // Profile+0x6C is mfInCarTimePlayed. The whole gauntlet is the inlined tip-request guard
        // the tree already names on TrainingManager (IsTipPending / IsInPictureParadise /
        // GetProfile / GetTimeSinceLastTip / RequestTip -- the BrnDriveThruManager.cpp idiom).
        if (static_cast<s32>(lrEvent.meVictimIndex) == static_cast<s32>(GetPlayerActiveRaceCarIndex()) &&
            GetCurrentGameModeType() == GameStateModuleIO::E_MODE_MARKED_MAN &&
            !mTrainingManager.IsTipPending() &&
            !mTrainingManager.IsInPictureParadise() &&
            mTrainingManager.IsTipAllowedInGameMode(BrnProgression::E_TRAINING_TYPE_TAKEN_DOWN_IN_MARKED_MAN))
        {
            BrnProgression::Profile* lpProfile = mTrainingManager.GetProfile();
            CGS_ASSERT(lpProfile != 0, CgsDev::Assert::MessageAt("lpProfile", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/TrainingManager/BrnTrainingManager.cpp", 382));   // BrnTrainingManager.cpp:382
            if (lpProfile != 0 &&
                !lpProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_TAKEN_DOWN_IN_MARKED_MAN) &&
                mTrainingManager.GetTimeSinceLastTip() >= KF_TRAINING_TIP_SETTLE_TIME)   // fcmpu/blt @0x823900AC
            {
                mTrainingManager.RequestTip(BrnProgression::E_TRAINING_TYPE_TAKEN_DOWN_IN_MARKED_MAN);
            }
        }
    }
}

}

// FOLDED FROM GameStateModule_gSR_00.cpp
// b5-decomp/src/GameSource/GameState/GameStateModule_gSR_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the
// module's other committed bodies live in BrnGameStateModule.cpp and GameStateModule_gUI_00.cpp).
//
// ============================================================================================
// STUNT-RACES WAVE D, AGENT D3 -- THE FOUR OFFLINE EVENT-START FUNCTIONS.
//
// The event CORE was already mounted and linking before this file existed (ModeManager spine,
// fifteen modes, the mode states, the scoring stack, the StuntAttackMode trio, the traffic
// junction leaves). What was missing was everything that DETECTS the junction and DRIVES a
// start. These four are that, in the console's own call order -- all four are reached from
// GameStateModule::PreWorldUpdate @0x823A5328, which is itself not reconstructed:
//
//   1. CheckIfPlayerIsAtJunctionWithAnEvent  @0x82390418
//        TriggerQueryManager::GetPlayerCurrentTrafficLightId
//          -> TrafficData::GetJunctionLogicBoxForTrafficLight
//          -> JunctionLogicBox::GetEventJunctionID
//          -> ProgressionData's EventJunction table -> EventJunction::GetOfflineEvent
//          -> POSTS GAME ACTION 201 (E_ACTION_EVENT_AT_JUNCTION_AVAILABLE, 40 bytes)
//      * ACTION 201 IS THE VISIBLE ORACLE FOR THE WHOLE WAVE. The consumer half is already
//      mounted and live: GameBridgeGameStateToX_EventFlowGuiEvents.cpp's case-201 arm repacks it
//      into BrnGui::GuiEventJunctionInfo (id 311), FBurnMainHudState dispatches 311, and
//      JunctionInfoComponent::HandleJunctionChange draws the "hold accelerator + brake" banner.
//      Every field written below is matched FIELD FOR FIELD against that arm.
//
//   2. DetectModeStarts                      @0x8239A428   -- the arm; calls 3 then 4.
//   3. ShouldStartSnapRaceMode                @0x82363700  -- the 0.35 s accel+brake hold gate.
//   4. StartModeAtLights                      @0x82396CF8  -- builds the StartGameModeParams and
//                                                             calls ModeManager::StartGameMode.
//
// THE PSEUDOCODE OF ALL FOUR IS UNSAFE AND IS NOT THE SOURCE FOR ANYTHING HERE. Measured this
// pass, each defect called out again at its site:
//   (a) 0x82390418 opens with "local variable allocation has failed"; its whole 40-byte action
//       frame renders as thirteen unrelated locals (v91..v103) whose ORDER in the record is only
//       recoverable from the stack offsets in the asm.
//   (b) 0x82363700's prototype carries a phantom `int a4` -- the PPC float argument (the game
//       timestep, f1) consumes the r5 GPR slot, so the out-parameter is r6.
//   (c) 0x8239A428 renders `ShouldStartSnapRaceMode(this, byte, dt)` with the out-parameter
//       dropped, and `StartModeAtLights(this, in, out)` with the START MECHANISM dropped -- the
//       one argument StartModeAtLights early-returns on.
//   (d) 0x82396CF8 renders `GetOriginalCarId(v8[71094], v8[71095])` for a single `ldx r4, r25,
//       0x456D8` (one CgsID split into two dwords), and `*(v21 + 20)` for the `ld r11, 0x10`
//       that loads RaceEventData::mSpecialEventCarId whole.
//
// TWO CLASSES OF NULL GUARD ARE ADDED, AND THEY ARE THE ONLY BEHAVIOURAL ADDITIONS IN THIS FILE.
// The console asserts `lpRaceEvent` / `lpEventData` on a failed EventJunction lookup and then
// FALLS THROUGH INTO THE DEREFERENCE ANYWAY (0x82390418's LABEL_24 -> `*(v9 + 16)` with v9 == 0,
// and 0x82396CF8's LABEL_25 -> LABEL_26 with v21 == 0); the same shape repeats on every
// "lp<thing>" assert in both bodies. In a retail build, where FireAssert is inert, that is a
// crash. The asserts are reproduced verbatim -- they are the diagnostic -- and an early return
// is added behind each, marked [GUARD] at the site.
// ============================================================================================
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/Offences/BrnDriveThruManager.h"   // DriveThroughsCloseOnceActivatedUntilFurtherNotice (StartGameMode leg b)

#include <stdlib.h>                                                     // getenv (the diag rungs)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h" // CgsSystem::TimerRequests (the showtime arm's third argument)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "rw/math/vpu/vector3_operation.h"                              // Dot (the showtime arm's two vmsum3fp128)

#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // PreWorldInputBuffer / OutputBuffer / ControllerInput
#include "GameSource/GameState/BrnGameActions.h"                        // JunctionInfoAction / WrongCarForChallengeAction
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"    // GetCurrentTrainingType
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // GameMode::IsOnline
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // StartGameModeParams

#include "SharedClasses/Progression/BrnProgressionData.h"               // ProgressionData / the junction table
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // RaceEventData / EventJunction
#include "SharedClasses/Progression/BrnProgressionRankData.h"           // ProgressionRankData (SetProgressionRankData)
#include "SharedClasses/Progression/BrnTrainingTypes.h"                 // E_TRAINING_TYPE_* (the seen-tip marks)
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"           // TrafficData::GetJunctionLogicBoxForTrafficLight
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"        // JunctionLogicBox::GetID / GetEventJunctionID
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"               // LightTriggerStartData::GetStartDirection
#include "SharedClasses/DataLists/VehicleList.h"                        // VehicleList::GetVehicleData(CgsID)
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // BrnResource::VehicleListEntry

#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // RaceCarState::mfSpeedMPH (@972)
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"                 // WorldMap2D::GetValue (the case-95 arm)
#include "SharedClasses/World/BrnWorldRegion.h"                         // WorldRegion (the case-95 arm's answer)
#include <cstring>                                                      // memcpy (the case-95 arm's position)

#include <xtl.h>
namespace vmx
{
    inline f32 Dot3(const Vector3& a, const Vector3& b) { return __vmsum3fp(__lvx(&a, 0), __lvx(&b, 0)).x; }
    inline f32 Flush(f32 x) { return __vmulfp(XMVectorReplicate(x), XMVectorReplicate(1.0f)).x; }
}


namespace BrnGameState
{
namespace
{

// --------------------------------------------------------------------------------------------
// The EventJunction lookup both CheckIfPlayerIsAtJunctionWithAnEvent and StartModeAtLights
// open-code, THREE times between them. The console form, verbatim (0x82390A20..0x82390A48 is
// the clearest of the three):
//     count = *(progressionData + 28);          // muEventJunctionCount
//     table = *(progressionData + 24);          // mpaEventJunctions, 16-byte stride
//     for (i = 0; i < count; ++i)
//         if (table[i].muID == luEventJunctionID) return &table[i];
//     return NULL;                              // then the caller's own assert fires
// Reproduced through ProgressionData::GetEventJunctionCount / GetEventJunction so no body here
// touches the +0x18 / +0x1C words directly.
// --------------------------------------------------------------------------------------------
const BrnProgression::EventJunction* FindEventJunctionById(
        const BrnProgression::ProgressionData* lpProgressionData, u32 luEventJunctionID)
{
    if (lpProgressionData == 0)
    {
        return 0;
    }
    const u32 luCount = lpProgressionData->GetEventJunctionCount();
    for (u32 lu = 0; lu < luCount; ++lu)
    {
        const BrnProgression::EventJunction* lpJunction = lpProgressionData->GetEventJunction(lu);
        if (lpJunction != 0 && lpJunction->GetID() == luEventJunctionID)
        {
            return lpJunction;
        }
    }
    return 0;
}

// --------------------------------------------------------------------------------------------
// "A BLOCKING training tip is on screen right now."
// The console open-codes this at THREE sites in this wave's chain, always identically
// (@0x8236382C-5C / @0x82390900-24 / @0x82390D68-8C, r11 = gsm+46640, the EMBEDDED
// TrainingManager):
//     lwz r10, 0(r11); cmpwi 0;    beq -> false     <- meTrainingState (+0x00) != INACTIVE
//     lwz r11, 4(r11); cmpwi 0x4D; blt -> true      <- meCurrentTrainingType (+0x04) < 77
// BOTH loads are TrainingManager MEMBERS -- `*(gsm+46640)` is meTrainingState, NOT a pointer
// null test (the 2026-08-26 verify caught that misread: the type idles at -1, signed, so the
// state term is load-bearing -- without it this returned TRUE on every idle frame and killed
// the gesture hold + forced canEnter=0 at every junction).
// 77 is BrnProgression::E_TRAINING_TYPE_NOT_TIMED_COUNT (below = MODAL tip; at/above = ambient
// timed). Sites:
//     ShouldStartSnapRaceMode              @0x82363700 (the pre-gate)
//     CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418 twice (mbCanEnterEventAtJunction, and the
//                                                     action's own mbCanEnterEvent at LABEL_118)
// De-inlined over the named accessors: IsTipPending() is bodied as exactly
// `meTrainingState != E_TRAINING_STATE_INACTIVE` (BrnTrainingManager.cpp:808). The manager is a
// POINTER on this build (documented include-cycle deviation) -- the null guard is a HOST guard,
// not the console's first term.
// --------------------------------------------------------------------------------------------
bool IsBlockingTrainingTipActive(const TrainingManager* lpTrainingManager)
{
    if (lpTrainingManager == 0)
    {
        return false;
    }
    return lpTrainingManager->IsTipPending() &&
           lpTrainingManager->GetCurrentTrainingType() <
               BrnProgression::E_TRAINING_TYPE_NOT_TIMED_COUNT;
}

// --------------------------------------------------------------------------------------------
// LightTriggerId::IsValid(), as CheckIfPlayerIsAtJunctionWithAnEvent's DEPARTURE arm inlines it
// on the CACHED handle (@0x82390DEC..0x82390E10):
//     rlwinm r9, r11, 0,8,23   ; id & 0x00FFFF00   -- the hull field, bits 8..23
//     cmplw  r9, 0xFFFF00      ; hull == 0xFFFF ?  -> invalid
//     clrlwi r11, r11, 24      ; id & 0xFF         -- the light-trigger index, bits 0..7
//     cmplwi r11, 0xFF         ; index == 0xFF ?   -> invalid
// Identical to the predicate TrafficData::GetJunctionLogicBoxForTrafficLight asserts and to
// BrnModeManager_IntroPlay.cpp's file-local IsLightTriggerIdValid. Note the owner TAG in bits
// 24..31 (0x39 == traffic-light trigger) is NOT masked off by either test -- the handle stays
// packed exactly as TrafficEntityModule::ManageTriggers built it.
// --------------------------------------------------------------------------------------------
bool IsLightTriggerIdValid(u32 luLightTriggerId)
{
    return ((luLightTriggerId & 0x00FFFF00u) != 0x00FFFF00u) &&
           ((luLightTriggerId & 0x000000FFu) != 0x000000FFu);
}

// ---- the showtime arm's three console literals, all image-read out of the ARTIST .rdata -----
// flt_82CDB8CC == 0x41200000 == 10.0f. The arm squares it (`fmuls f30, f0, f0` @0x8239A60C) and
// compares the squared speed against the square, so this is the minimum speed in the linear
// velocity's own units (m/s -- RaceCarState::mLinearVelocity is raw, see GetPlayerLinearVelocity's
// banner), i.e. about 22 mph.
const f32 KF_SHOWTIME_MIN_SPEED = 10.0f;
// flt_82CDB8D0 == 0x3F000000 == 0.5f. The showtime-intro window.
const f32 KF_SHOWTIME_INTRO_SECONDS = 0.5f;
// flt_82004884 == 0x3727C5AC == 9.99999975e-06f. Not a "nearly zero" placeholder: it is a
// deliberate hair above zero, so IsInShowtimeIntro() stays true and the next frame re-enters the
// decision arm. That is how the console waits for a car that is still airborne to land.
const f32 KF_SHOWTIME_INTRO_RETRY_SECONDS = 9.99999975e-06f;

}   // anonymous namespace

// ============================================================================================
// GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent -- X360 0x82390418
//   (BrnGameStateModule.cpp:6925..7135; every assert line quoted below is that file's)
// ============================================================================================
// THE ACTION-201 RECORD, and how its twelve fields were recovered. Hex-Rays gives the stores as
// thirteen unrelated locals; the frame is `var_C0` at sp+0x70 and AddEvent is called with
// `addi r4, r1, 0x130+var_C0` (@0x82390DD4 and @0x82390EB8), `li r5, 0xC9` (201) and
// `li r6, 0x28` (40). Subtracting the base from each store offset gives the layout:
//     var_C0  +0x00 muJunctionLogicBoxId        var_A4  +0x1C mi8Difficulty
//     var_BC  +0x04 muEventJunctionID           var_A3  +0x1D mi8MedalAchieved
//     var_B8  +0x08 muLightTriggerId            var_A2  +0x1E mbOnEntry
//     var_B0  +0x10 mSpecialEventCarId (std)    var_A1  +0x1F mbCanEnterEvent
//     var_A8  +0x18 meGameModeType              var_A0  +0x20 mbEventUnlocked
//                                               var_9F  +0x21 mbSpecificCarEventValid
//                                               var_9E  +0x22 mbIsNewlyDiscovered
//                                               var_9D  +0x23 mbIsAutoUnlockedChallenge
// which is exactly the committed GameStateModuleIO::JunctionInfoAction (BrnGameActions.h:1287)
// and exactly the eleven loads the mounted consumer arm emits at 0x823EA810.
//
// A REAL CONSOLE DEFECT, RECORDED RATHER THAN REPRODUCED: mi8Difficulty (+0x1C) IS NEVER STORED
// ON THE EVENT POST. A sweep of the whole export for `var_A4` finds exactly two stores, both
// `-1`, and both on paths that carry NO event (@0x82390DBC, the "this junction has no event"
// post, and @0x82390E94, the departure post). The event arm at 0x82390AD4..0x82390B18 stores
// meGameModeType, mi8MedalAchieved, mbEventUnlocked and mbSpecificCarEventValid and leaves
// +0x1C alone -- so the console ships whatever was on its stack there, and the GUI's
// GuiEventJunctionInfo::mi8Difficulty reads uninitialised memory on every junction arrival. The
// record here is VALUE-INITIALISED, so this build sends a stable 0 instead of stack garbage.
// That is a divergence and it is stated rather than hidden; it cannot be "fixed" into the right
// value because the console never computed one.
//
// TWO ARMS ARE PARKED, both marked [PARKED] at their sites with the console's own body:
//   * the DISCOVERY arm (0x823905C8..0x823907E8) -- ProfileEvent flag bit 0, action 55,
//     Profile::AddGameModeTypeToDiscovered, CheckForAllEventsOfATypeFound /
//     CheckForAllEventsBeingFound. Parked by the wave brief.
//   * the TRAINING-TIP arm (0x82390B18..0x82390D5C) -- the three-way DISCOVERS_EVENT /
//     CORRECT_CAR / WRONG_CAR tip choice.
// Neither parked arm posts an action the mounted GUI consumes; the action-201 post, which does,
// is complete.
// ============================================================================================
void GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent(
        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
        GameStateModuleIO::OutputBuffer*             lpOutput)
{
    if (lpInput == 0 || lpOutput == 0)
    {
        return;   // [GUARD] -- the console takes both non-null from PreWorldUpdate.
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrInterface =
        mLastActiveRaceCarInterface;

    // @0x82390440-ish: the interface's own bounds assert, fired on ITS index word (+10328) with
    // the interface header's file/line -- BrnRaceCarEntityModuleOutputInterface.h:967.
    CGS_ASSERT(lrInterface.GetPlayerActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CgsDev::Assert::MessageAt("mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", "..\\..\\..\\GameSource\\World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h", 967));

    // `v12 = (idx != -1) ? mbIsPlayerCarActive : 0; if (!v12) return;` -- the whole body is
    // behind a live player car.
    const bool lbPlayerCarActive =
        (lrInterface.GetPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
        lrInterface.IsPlayerCarActive();
    if (!lbPlayerCarActive)
    {
        return;
    }

    // `GetMemor(this + 43920, ...)` @0x8239055C -- 43920 == mTriggerQueryManager (this+42320)
    // + 0x640, i.e. the inlined TriggerQueryManager::GetTrafficData().
    const BrnTraffic::TrafficData* lpTrafficData = mTriggerQueryManager.GetTrafficData();
    CGS_ASSERT(lpTrafficData != 0, CgsDev::Assert::MessageAt("lpTrafficData", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6925));                              // :6925
    if (lpTrafficData == 0)
    {
        return;   // [GUARD] -- the console dereferences it below; see the file banner.
    }

    CGS_ASSERT(lrInterface.GetPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID,
               CgsDev::Assert::MessageAt("Player car index hasn't been set", "..\\..\\..\\GameSource\\World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h", 980));    // interface header :980
    const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState =
        lrInterface.GetRaceCarState(lrInterface.GetPlayerActiveRaceCarIndex());
    CGS_ASSERT(lpPlayerRaceCarState != 0, "lpPlayerRaceCarState");                // :6929
    if (lpPlayerRaceCarState == 0)
    {
        return;   // [GUARD]
    }

    // ---- 1) the "which event is this" pre-pass ---------------------------------------------
    // mbSpecificCarEventValid: the junction's event demands a specific car AND the player is in
    // it (`*&v22 = *(lpRaceEvent + 16)` == mSpecialEventCarId, compared against
    // GetOriginalCarId(mActivePlayerCarId) -- @0x82390618-ish).
    bool lbSpecificCarEventValid = false;

    if (mTriggerQueryManager.IsPlayerInTrafficLightRegion())
    {
        const LightTriggerId lLightTriggerId = mTriggerQueryManager.GetPlayerCurrentTrafficLightId();
        const BrnTraffic::JunctionLogicBox* lpJunctionLogicBox =
            lpTrafficData->GetJunctionLogicBoxForTrafficLight(static_cast<u32>(lLightTriggerId));
        CGS_ASSERT(lpJunctionLogicBox != 0, "lpJunctionLogicBox");                // :6941

        if (lpJunctionLogicBox != 0)   // [GUARD]
        {
            const u32 luEventJunctionID = lpJunctionLogicBox->GetEventJunctionID();
            const BrnProgression::ProgressionData* lpProgressionData =
                mProgressionManager.GetProgressionData();
            const BrnProgression::EventJunction* lpEventJunction =
                FindEventJunctionById(lpProgressionData, luEventJunctionID);
            const BrnProgression::RaceEventData* lpRaceEvent =
                (lpEventJunction != 0) ? lpEventJunction->GetOfflineEvent() : 0;
            CGS_ASSERT(lpRaceEvent != 0, "lpRaceEvent");                          // :6944

            if (lpRaceEvent != 0)   // [GUARD]
            {
                const CgsID lSpecialEventCarId = lpRaceEvent->GetSpecialEventCarId();
                lbSpecificCarEventValid =
                    (lSpecialEventCarId != 0) &&
                    (GetOriginalCarId(mActivePlayerCarId) == lSpecialEventCarId);
            }

            // ⭐ THE DISCOVERY ARM (0x823905C8..0x823907E8) -- LANDED 2026-08-27 (D1
            // profile-event-list wave). It was parked on two things, and both are now gone: the
            // ProfileEvent flag-bit mutator this tree did not model (ProfileEvent::EnableFlags,
            // DWARF BrnProfile.h:328, now bodied in BrnProfile.cpp) and the fact that the profile
            // held NO event records at all, so the ":6957 lpProfileEvent" assert would have fired
            // on every single junction arrival. ProgressionManager::UnlockToProgressionRank
            // @0x8239DDE8 now populates the table at boot, so the lookup below finds its record.
            //
            // WHY THIS ARM IS LOAD-BEARING AND NOT COSMETIC: E_FLAG_DISCOVERED (bit 0) is what
            // ProgressionManager::OnEventFinishUpdateProfile @0x823A0040 asserts on before it
            // writes the win flags ("lpEvent->IsFlagSet( ProfileEvent::E_FLAG_DISCOVERED )",
            // BrnProgressionManager.cpp:1672). Standing at the junction is what sets it, and
            // standing at the junction is exactly how the player starts the event they then win.
            //
            // The gate is the console's: `!mbJunctionNewlyDiscovered && !mpCurrentGameMode &&
            // !IsOnlineGameMode()` (@0x82390684..0x823906AC).
            // (`lpRaceEvent != 0` is this file's standing [GUARD] class, not a new one: the
            // console asserts it at :6944 above and then dereferences it here regardless --
            // see the file banner's "TWO CLASSES OF NULL GUARD" note.)
            if (lpRaceEvent != 0 &&
                !mbJunctionNewlyDiscovered &&
                mModeManager.GetCurrentGameMode() == 0 &&
                !IsOnlineGameMode())
            {
                BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();

                // @0x823906B0..0x8239071C: the open-coded scan of maEvents for the junction id,
                // then the ":6957 lpProfileEvent" assert on a miss. Routed through the named
                // finder (Profile::FindEvent, DWARF BrnProfile.h:713).
                BrnProgression::ProfileEvent* lpProfileEvent =
                    (lpProfile != 0) ? lpProfile->FindEvent(luEventJunctionID) : 0;
                CGS_ASSERT(lpProfileEvent != 0, "lpProfileEvent");                 // :6957

                // `lhz r11, 4(r30) / clrlwi 31 / bne` -- already discovered, nothing to do.
                if (lpProfileEvent != 0 && !lpProfileEvent->IsFlagSet(
                        BrnProgression::ProfileEvent::E_FLAG_DISCOVERED))
                {
                    CGS_ASSERT(lpProfile != 0, "lpProfile");                       // :6962

                    // @0x82390750..0x82390760: `lhz / ori 1 / sth`.
                    lpProfileEvent->EnableFlags(
                        static_cast<u16>(BrnProgression::ProfileEvent::E_FLAG_DISCOVERED));

                    // @0x82390764..0x82390780: the SAME data-mode -> runtime-mode hop the
                    // action-201 arm below makes, including the +180972 override word whose
                    // retail value is the 6 sentinel (see that arm's FLAG for the full cite).
                    lpProfile->AddGameModeTypeToDiscovered(
                        static_cast<GameStateModuleIO::EGameModeType>(
                            mProgressionManager.GetEvent(static_cast<s32>(lpRaceEvent->GetMode()))));

                    // @0x82390798: the latch the action-201 record reads as mbIsNewlyDiscovered
                    // (and which the post below clears again).
                    mbJunctionNewlyDiscovered = true;

                    // @0x8239079C..0x823907AC: `li r11,0 / stb / AddEvent(queue, &byte, 0x37, 1)`.
                    // NOTE the payload byte is ZERO here -- OnEventFinishUpdateProfile's copy of
                    // the same action posts 1. Reproduced as written.
                    const u8 lu8AutoSavePayload = 0;
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lu8AutoSavePayload),
                        GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE,
                        static_cast<s32>(sizeof(lu8AutoSavePayload)));

                    // Every event of this mode type found? Then every event at all? Each post its
                    // own reward action (203 / 202); the queue is re-fetched for each call.
                    CheckForAllEventsOfATypeFound(
                        lpProfile, lpOutput->GetGameActionQueue(),
                        static_cast<BrnProgression::RaceEventData::EModeType>(lpRaceEvent->GetMode()));
                    CheckForAllEventsBeingFound(lpProfile, lpOutput->GetGameActionQueue());
                }
            }
        }
    }
    else
    {
        // @0x823907EC: leaving every traffic-light region drops the "arrival posted" latch.
        mbAtJunctionWithEvent = false;
    }

    // ---- 2) the post gate -------------------------------------------------------------------
    // @0x823907F0..0x8239086C, verbatim:
    //     show = mbJunctionNewlyDiscovered
    //         || !lpInput->GetControllerInput()->mbAcceleratePressed   (lbz 0x12(ctrl))
    //         || playerState->mfSpeedMPH < 40.0f                       (flt_82004D0C, image-read)
    //         || lbSpecificCarEventValid;
    //     if (IsPlayerInTrafficLightRegion() && (show || mbAtJunctionWithEvent)) -> arrival post
    //     else                                                                   -> departure post
    // i.e. the banner is SUPPRESSED only while the player blasts through a junction with the
    // throttle down in a car the event does not care about -- and never once an arrival has
    // already been posted for the junction they are still standing in.
    const GameStateModuleIO::ControllerInput* lpControllerInput = lpInput->GetControllerInput();
    const bool lbShowJunctionInfo =
        mbJunctionNewlyDiscovered ||
        (lpControllerInput == 0) || !lpControllerInput->mbAcceleratePressed ||
        (lpPlayerRaceCarState->mfSpeedMPH < 40.0f) ||
        lbSpecificCarEventValid;

    if (mTriggerQueryManager.IsPlayerInTrafficLightRegion() &&
        (lbShowJunctionInfo || mbAtJunctionWithEvent))
    {
        // ---- THE ARRIVAL POST (0x82390870..0x82390DDC) --------------------------------------
        const LightTriggerId lLightTriggerId = mTriggerQueryManager.GetPlayerCurrentTrafficLightId();
        const BrnTraffic::JunctionLogicBox* lpJunctionLogicBox =
            lpTrafficData->GetJunctionLogicBoxForTrafficLight(static_cast<u32>(lLightTriggerId));
        CGS_ASSERT(lpJunctionLogicBox != 0, "lpJunctionLogicBox");                // :7002
        if (lpJunctionLogicBox == 0)
        {
            return;   // [GUARD]
        }

        const BrnPhysics::Vehicle::RaceCarState* lpState =
            lrInterface.GetRaceCarState(lrInterface.GetPlayerActiveRaceCarIndex());
        CGS_ASSERT(lpState != 0, "lpPlayerRaceCarState");                         // :7005
        if (lpState == 0)
        {
            return;   // [GUARD]
        }

        bool lbCanEnterEvent = false;
        if (mModeManager.GetCurrentGameMode() == 0)
        {
            lbCanEnterEvent = !IsBlockingTrainingTipActive(&mTrainingManager) &&
                              !(lpState->mfSpeedMPH > 30.0f);
        }

        GameStateModuleIO::JunctionInfoAction lAction = GameStateModuleIO::JunctionInfoAction();

        // The console's store ORDER matters in exactly two places: mbIsNewlyDiscovered is read
        // (`lbz r11, 0(r17)` @0x82390964) BEFORE the post clears it, and the cached box id is
        // read back out of the member AFTER being written (`stw` then `lwz`, @0x82390998/9C).
        lAction.mbIsNewlyDiscovered = mbJunctionNewlyDiscovered;
        mbAtJunctionWithEvent          = true;                                    // @0x8239096C
        muCachedJunctionLightTriggerId = static_cast<u32>(lLightTriggerId);        // @0x82390974
        mbCanEnterEventAtJunction      = lbCanEnterEvent;                          // @0x82390984
        lAction.mbOnEntry                 = true;                                  // @0x8239098C
        lAction.mbIsAutoUnlockedChallenge = false;                                 // @0x82390990

        const u32 luPreviousJunctionLogicBoxId = muCachedJunctionLogicBoxId;       // @0x82390994
        muCachedJunctionLogicBoxId            = lpJunctionLogicBox->GetID();       // @0x82390998
        lAction.muJunctionLogicBoxId          = muCachedJunctionLogicBoxId;        // @0x823909B0
        lAction.muLightTriggerId              = static_cast<u32>(lLightTriggerId);  // @0x82390954
        const u32 luEventJunctionID           = lpJunctionLogicBox->GetEventJunctionID();
        lAction.muEventJunctionID             = luEventJunctionID;                 // @0x823909B8

        // "The junction under the car CHANGED this frame" (@0x823909A0..0x823909BC). Consumed
        // only by the PARKED training-tip arm below; computed here because it is the console's,
        // and so that landing that arm needs no re-derivation.
        const bool lbJunctionChanged =
            (muCachedJunctionLogicBoxId != luPreviousJunctionLogicBoxId);
        (void)lbJunctionChanged;

        if (static_cast<s32>(luEventJunctionID) <= 0)                              // @0x823909C0
        {
            // "This traffic light is not an event junction" (@0x82390DA8..0x82390DC0).
            lAction.mbEventUnlocked  = false;
            lAction.mbCanEnterEvent  = false;
            lAction.meGameModeType   = GameStateModuleIO::E_MODE_OFFLINE_COUNT;    // li r11, 0xA
            lAction.mi8Difficulty    = -1;
            lAction.mi8MedalAchieved = -1;
        }
        else
        {
            const BrnProgression::ProgressionData* lpProgressionData =
                mProgressionManager.GetProgressionData();
            CGS_ASSERT(lpProgressionData != 0, "lpProgressionData");               // :7048

            const BrnProgression::EventJunction* lpEventJunction =
                FindEventJunctionById(lpProgressionData, luEventJunctionID);
            const BrnProgression::RaceEventData* lpRaceEvent =
                (lpEventJunction != 0) ? lpEventJunction->GetOfflineEvent() : 0;
            CGS_ASSERT(lpRaceEvent != 0, "lpRaceEvent");                           // :7051

            BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
            CGS_ASSERT(lpProfile != 0, "lpProfile");                               // :7054

            if (lpRaceEvent == 0 || lpProfile == 0)
            {
                return;   // [GUARD] -- the console falls straight into the deref here.
            }

            // THE DATA MODE -> RUNTIME MODE HOP. The console reads a game-type word first and
            // only falls back to the event's own mode when it holds 6:
            //     v59 = *(gsm + 180972); if (v59 == 6) v59 = lpRaceEvent->mu8Mode;
            //     meGameModeType = ProgressionManager::GetEvent(v59);
            // (@0x82390AB8..0x82390AC8, and StartModeAtLights repeats it at 0x82396F10.)
            //
            // FLAG -- THE OVERRIDE WORD HAS NO MEMBER AND NO WRITER. gsm+180972 is
            // mProgressionManager + 133052, which lands inside that manager's mDebugComponent
            // slot (the ProgressionDebugComponent reserved at the X360 +133000 region); that
            // sub-layout is not modelled in this tree, and a sweep of the export set finds NO
            // function that writes the word at all -- CheckIfPlayerIsAtJunctionWithAnEvent and
            // StartModeAtLights are its only two appearances anywhere. Its retail value is the 6
            // sentinel ("no override"): RaceEventData::EModeType tops out at 5, so 6 is outside
            // the data enum, and 6 is the ONLY value for which the console consults the event.
            // Reproduced as the sentinel path taken unconditionally, with the console expression
            // written out above so the override can be restored the moment
            // ProgressionDebugComponent is reconstructed. DELETE-WHEN that happens.
            const s32 liDataGameType = static_cast<s32>(lpRaceEvent->GetMode());
            lAction.meGameModeType   = static_cast<GameStateModuleIO::EGameModeType>(
                                           mProgressionManager.GetEvent(liDataGameType));

            const CgsID lSpecialEventCarId  = lpRaceEvent->GetSpecialEventCarId();
            lAction.mSpecialEventCarId      = lSpecialEventCarId;                  // std @0x82390AE4
            lAction.mbSpecificCarEventValid = false;                               // @0x82390ADC
            lAction.mbEventUnlocked         = true;                                // @0x82390AE8
            lAction.mi8MedalAchieved        = static_cast<s8>(
                lpProfile->GetMedalAchievedForEventWithID(static_cast<s32>(luEventJunctionID)));

            // r28 is the console's "unlocked" flag; LABEL_118 gates on it as well as on the
            // member, which is why it is a separate local from the record field.
            bool lbEventUnlocked = true;
            if (lSpecialEventCarId != 0)                                           // @0x82390AF0
            {
                if (lbSpecificCarEventValid)
                {
                    lAction.mbSpecificCarEventValid = true;                        // @0x82390B08
                }
                else
                {
                    lbEventUnlocked         = false;
                    lAction.mbEventUnlocked = false;                               // @0x82390B14
                }
            }

            bool lbActionCanEnterEvent = false;
            if (mbCanEnterEventAtJunction && lbEventUnlocked)
            {
                lbActionCanEnterEvent = !IsBlockingTrainingTipActive(&mTrainingManager);
            }
            lAction.mbCanEnterEvent = lbActionCanEnterEvent;
        }

        lpOutput->GetGameActionQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAction),
            GameStateModuleIO::E_ACTION_EVENT_AT_JUNCTION_AVAILABLE,
            static_cast<s32>(sizeof(GameStateModuleIO::JunctionInfoAction)));      // li r5,0xC9 / li r6,0x28

        mbJunctionNewlyDiscovered = false;                                         // @0x82390DDC

        return;
    }

    // ---- THE DEPARTURE POST (0x82390DE8..0x82390ED0) ----------------------------------------
    // Only when the CACHED handle still names a junction -- i.e. exactly once per exit.
    if (!IsLightTriggerIdValid(muCachedJunctionLightTriggerId))
    {
        return;
    }

    const BrnTraffic::JunctionLogicBox* lpCachedBox =
        lpTrafficData->GetJunctionLogicBoxForTrafficLight(muCachedJunctionLightTriggerId);
    CGS_ASSERT(lpCachedBox != 0, "lpJunctionLogicBox");                            // :7127
    if (lpCachedBox == 0)
    {
        return;   // [GUARD]
    }

    GameStateModuleIO::JunctionInfoAction lAction = GameStateModuleIO::JunctionInfoAction();
    lAction.mbOnEntry                 = false;                                     // @0x82390E6C
    lAction.mbIsNewlyDiscovered       = false;                                     // @0x82390E74
    lAction.mbIsAutoUnlockedChallenge = false;                                      // @0x82390E78
    lAction.muEventJunctionID         = lpCachedBox->GetEventJunctionID();          // @0x82390E7C
    lAction.muLightTriggerId          = muCachedJunctionLightTriggerId;             // @0x82390E84
    lAction.meGameModeType            = GameStateModuleIO::E_MODE_OFFLINE_COUNT;    // @0x82390E8C
    lAction.mi8Difficulty             = -1;                                         // @0x82390E94
    lAction.mi8MedalAchieved          = -1;                                         // @0x82390E98
    lAction.muJunctionLogicBoxId      = muCachedJunctionLogicBoxId;                 // @0x82390EA0
    lAction.mbCanEnterEvent           = mbCanEnterEventAtJunction;                  // @0x82390EA8
    // (mbEventUnlocked / mbSpecificCarEventValid are not stored on this path either -- the same
    //  uninitialised-stack family as mi8Difficulty; value-initialised false here.)

    lpOutput->GetGameActionQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lAction),
        GameStateModuleIO::E_ACTION_EVENT_AT_JUNCTION_AVAILABLE,
        static_cast<s32>(sizeof(GameStateModuleIO::JunctionInfoAction)));

    muCachedJunctionLightTriggerId = 0xFFFFFFFFu;                                  // @0x82390EC4
    muCachedJunctionLogicBoxId     = 0xFFFFFFFFu;                                  // @0x82390EC8
    mbCanEnterEventAtJunction      = false;                                        // @0x82390ECC
    mbJunctionNewlyDiscovered      = false;                                        // @0x82390ED0

}

// ============================================================================================
// GameStateModule::ShouldStartSnapRaceMode -- X360 0x82363700
//   (BrnGameStateModule.cpp:5161..5210)
// ============================================================================================
// THE GESTURE IS ANALOGUE AND THE TREE USED TO MISNAME IT. lbRaceModePressed is
// PreWorldInputBuffer +0x45 == ControllerInput::mbRaceModePressed, which SetButtonPressed
// @0x823BA240 computes as `padAction[+0x00].value > 0.25f && padAction[+0x08].value > 0.25f` --
// the ACCELERATOR and BRAKE analogue axes, not the sticks and not the shoulder buttons.
//
// The gate, in the console's own order. Every bail arm RE-ARMS the timer to 0.35; the two arms
// at the bottom do NOT -- that asymmetry is the whole hold mechanic:
//     interface bounds assert
//     player car live?                     no  -> re-arm, false
//     crashing / no index / engine not RUNNING (mePlayerEngineState != 2)
//                                          yes -> re-arm, false
//     a game mode is running AND it is not E_MODE_ONLINE_FREE_BURN_LOBBY(15) /
//         E_MODE_ONLINE_SHOWTIME(16)           -> re-arm, false
//     not in a traffic-light region            -> re-arm, false
//     a blocking training tip is up            -> re-arm, false
//     gesture not held OR speed > 30 mph       -> re-arm, false
//     timer -= dt; still > 0, or the sim is paused -> false   (NO re-arm: the hold runs)
//     else                                     -> re-arm, write the mechanism, TRUE
//
// The final `*out = <in a light region> ? 2 : 1` is the console's, kept verbatim even though the
// region test five lines above already forces the 2 -- StartModeAtLights early-returns on
// anything but 2, so the branch is load-bearing documentation of the discriminant. The Hex-Rays
// form of that store is `*a5 = ((_cntlzw(v19) & 0x20) == 0) + 1`, i.e. (v19 != 0) + 1.
// ============================================================================================
bool GameStateModule::ShouldStartSnapRaceMode(bool                     lbRaceModePressed,
                                              f32                      lfGameTimestep,
                                              EGameModeStartMechanism* lpOutStartMechanism)
{
    CGS_ASSERT(lpOutStartMechanism != 0, CgsDev::Assert::MessageAt("lpOutStartMechanism", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5161));                   // :5161
    if (lpOutStartMechanism == 0)
    {
        return false;   // [GUARD]
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrInterface =
        mLastActiveRaceCarInterface;

    CGS_ASSERT(lrInterface.GetPlayerActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               CgsDev::Assert::MessageAt("mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT", "..\\..\\..\\GameSource\\World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h", 967));      // interface :967

    const ::EActiveRaceCarIndex lePlayerIndex = lrInterface.GetPlayerActiveRaceCarIndex();

    const bool lbPlayerCarActive = (lePlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                                   lrInterface.IsPlayerCarActive();
    if (!lbPlayerCarActive)
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    if (lrInterface.IsPlayerCarCrashing() ||
        lePlayerIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID ||
        lrInterface.GetPlayerEngineState() !=
            BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING)
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    if (mModeManager.GetCurrentGameMode() != 0)
    {
        const GameStateModuleIO::EGameModeType leType = mModeManager.GetCurrentGameModeType();
        if (leType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
            leType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            mfSnapRaceStartHoldSeconds = 0.35f;
            return false;
        }
    }

    if (!mTriggerQueryManager.IsPlayerInTrafficLightRegion())
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    if (IsBlockingTrainingTipActive(&mTrainingManager))
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState =
        lrInterface.GetRaceCarState(lrInterface.GetPlayerActiveRaceCarIndex());
    CGS_ASSERT(lpPlayerRaceCarState != 0, CgsDev::Assert::MessageAt("lpPlayerRaceCarState", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5196));                 // :5196
    if (lpPlayerRaceCarState == 0)
    {
        return false;   // [GUARD]
    }

    if (!lbRaceModePressed || lpPlayerRaceCarState->mfSpeedMPH > 30.0f)
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    // The hold. `v18 = timer - dt; timer = v18; if (v18 > 0.0 || IsSimPaused(1,0)) return 0;`
    mfSnapRaceStartHoldSeconds -= lfGameTimestep;
    if (mfSnapRaceStartHoldSeconds > 0.0f || IsSimPaused(true, false))
    {
        return false;
    }

    const bool lbAtLights = mTriggerQueryManager.IsPlayerInTrafficLightRegion();
    mfSnapRaceStartHoldSeconds = 0.35f;
    *lpOutStartMechanism = lbAtLights ? E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS
                                      : E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_ANYWHERE;

    return true;
}

// ============================================================================================
// GameStateModule::DetectModeStarts -- X360 0x8239A428
//   (BrnGameStateModule.cpp:5240..5241 for the two asserts)
// ============================================================================================
// The arm. Two gates and a fork:
//   * `lwz r11, 0x1DB8(this)` == mModeManager.mpCurrentGameMode; if non-null,
//     `lbz r11, 0xAC(mode)` == GameMode::mbIsOnline (+172 -- the byte
//     OfflineGameMode::Construct clears and OnlineGameMode::Construct sets). Detection runs when
//     NO online mode is running -- or when the running mode is E_MODE_ONLINE_FREE_BURN_LOBBY
//     (15) or E_MODE_ONLINE_SHOWTIME (16), the two online modes you can still gesture in.
//   * on a successful hold, the SAME 15/16 test again -- but INVERTED: StartModeAtLights is
//     called only when the mode type is NOT one of those two (@0x8239A52C..0x8239A54C). That
//     asymmetry is the console's: the online lobby modes let the gesture be DETECTED (the online
//     path consumes it elsewhere) but never let it start an OFFLINE event.
//
// ✅ THE SHOWTIME (CRASH-MODE) ARM IS NO LONGER PARKED -- it is landed below, in full
// (0x8239A568..0x8239A8EC). See its own banner at the `else` for what it does, for the three
// errors the old park note carried, and for the two timesteps it needs. [showtime S7b-b]
// ============================================================================================
void GameStateModule::DetectModeStarts(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                       GameStateModuleIO::OutputBuffer* lpOutput)
{
    // ARTIST 8239A428: the monitor encloses both assertions and every mode arm.
    CgsDev::PerfMonCpu::StartMonitor(miDetectModeStartsPM);
    CGS_ASSERT(lpInput != 0, CgsDev::Assert::MessageAt("lpInput != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5240));
    CGS_ASSERT(lpOutput != 0, CgsDev::Assert::MessageAt("lpOutput != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5241));

    const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
    const GameStateModuleIO::EGameModeType leType = mModeManager.GetCurrentGameModeType();
    if (lpCurrentGameMode == 0 || !lpCurrentGameMode->IsOnline() ||
        leType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
        leType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        EGameModeStartMechanism leStartMechanism;
        if (ShouldStartSnapRaceMode(lpInput->GetControllerInput()->mbRaceModePressed,
                                    mfSimTimeStep, &leStartMechanism))
        {
            const GameStateModuleIO::EGameModeType leStartType = mModeManager.GetCurrentGameModeType();
            if (leStartType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
                leStartType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
            {
                StartModeAtLights(lpInput, lpOutput, leStartMechanism);
            }
        }
        else
        {
            const f32 lfGameTimeStep = lpInput->GetTimerStatusInterface()
                ->GetGameTimerStatus()->GetCurrentTimeStep();
            GameStateModuleIO::ShowtimeIntroAction lShowtimeIntro;
            bool lbStartedIntro = false;

            // The original bgt skips this attempt only for an ordered positive timer.
            if (!(mfShowtimeIntroTimeLeft > 0.0f))
            {
                CgsSystem::TimerRequests* lpSimRequests =
                    lpOutput->GetTimerRequestInterface()->GetSimTimerRequests();
                if (ShouldStartShowtimeMode(lfGameTimeStep,
                        lpInput->GetControllerInput()->mbCrashModePressed, lpSimRequests))
                {
                    bool lbMayStartIntro = mLastActiveRaceCarInterface.IsPlayerCarCrashing();
                    if (!lbMayStartIntro)
                    {
                        const Vector3& lrVelocity =
                            mLastActiveRaceCarInterface.GetPlayerRaceCarState()->mLinearVelocity;
                        lbMayStartIntro = vmx::Dot3(lrVelocity, lrVelocity) >=
                            vmx::Flush(KF_SHOWTIME_MIN_SPEED * KF_SHOWTIME_MIN_SPEED);
                    }
                    if (lbMayStartIntro)
                    {
                        lShowtimeIntro.mbStart = true;
                        lShowtimeIntro.mAimDirection = mLastActiveRaceCarInterface.GetPlayerDirection();
                        lpOutput->GetGameActionQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lShowtimeIntro),
                            GameStateModuleIO::E_ACTION_SHOWTIME_INTRO_START, sizeof(lShowtimeIntro));
                        mfShowtimeIntroTimeLeft = KF_SHOWTIME_INTRO_SECONDS;
                        mShowtimeIntroOriginalDirection = mLastActiveRaceCarInterface.GetPlayerDirection();
                        mbShowtimeIntroHasTouchedGround = false;
                        const f32 lfAngularVelocityY =
                            mLastActiveRaceCarInterface.GetPlayerRaceCarState()->mAngularVelocity.mV.y;
                        mfShowtimeIntroSteering = vmx::Flush(lfAngularVelocityY) >= 0.0f ? -1.0f : 1.0f;
                        lbStartedIntro = true;
                    }
                }
            }

            if (!lbStartedIntro && mfShowtimeIntroTimeLeft > 0.0f)
            {
                mfShowtimeIntroTimeLeft -= mfSimTimeStep;
                mbShowtimeIntroHasTouchedGround |= !mLastActiveRaceCarInterface.IsPlayerInAir();
                const f32 lfHeadingDot = vmx::Dot3(mLastActiveRaceCarInterface.GetPlayerDirection(),
                                                  mShowtimeIntroOriginalDirection);
                if (!(mfShowtimeIntroTimeLeft > 0.0f) ||
                    mLastActiveRaceCarInterface.IsPlayerInAir() ||
                    mLastActiveRaceCarInterface.IsPlayerCarCrashing() ||
                    mLastActiveRaceCarInterface.IsPlayerWrecked() || !(lfHeadingDot > 0.0f))
                {
                    mfShowtimeIntroTimeLeft = 0.0f;
                    CgsSystem::TimerRequests* lpSimRequests =
                        lpOutput->GetTimerRequestInterface()->GetSimTimerRequests();
                    if (ShouldStartShowtimeMode(lfGameTimeStep, true, lpSimRequests))
                    {
                        if (mbShowtimeIntroHasTouchedGround)
                            StartCrashMode(lpInput, lpOutput);
                        else
                            mfShowtimeIntroTimeLeft = KF_SHOWTIME_INTRO_RETRY_SECONDS;
                    }
                    else
                    {
                        // ARTIST writes only the cancel byte; the aim bytes remain indeterminate.
                        lShowtimeIntro.mbStart = false;
                        lpOutput->GetGameActionQueue()->AddEvent(
                            reinterpret_cast<const CgsModule::Event*>(&lShowtimeIntro),
                            GameStateModuleIO::E_ACTION_SHOWTIME_INTRO_START, sizeof(lShowtimeIntro));
                    }
                }
            }
        }
    }
    CgsDev::PerfMonCpu::StopMonitor(miDetectModeStartsPM);
}

// ============================================================================================
// GameStateModule::StartModeAtLights -- X360 0x82396CF8
//   (BrnGameStateModule.cpp:5365..5512; every assert line quoted below is that file's)
// ============================================================================================
// THE START. Resolve the junction, refuse if the event demands a car the player is not in, build
// a StartGameModeParams and hand it to ModeManager::StartGameMode.
//
// FOUR TRAPS, ALL OF THEM LIVE ON THE TEST TARGET (junction 480897 / event 558269):
//  1. THE MODE ENUM CROSSING. RaceEventData::mu8Mode is the DATA enum (STUNT_ATTACK == 2);
//     ModeManager indexes the RUNTIME enum (E_MODE_STUNT_ATTACK == 7).
//     ProgressionManager::GetEvent @0x82359850 is the ONLY bridge, and it is already bodied in
//     the mounted BrnProgressionManager.cpp -- verified against the export this wave: cases
//     0->0, 1->3, 2->7, 3->8, 4->5, 5->4, default asserts ("I dont know what this game type
//     is! ...", BrnProgressionManager.cpp:906) and returns -1. Using the data value as a mode
//     index would silently select E_MODE_OFFLINE_SHOWTIME.
//  2. THE START MECHANISM IS A DISCRIMINANT, NOT A FLAG. `cmpwi r31, 2 / bne` @0x82396D64.
//  3. THE SPECIAL-EVENT CAR GATE ABORTS THE START. On a mismatch the console posts action 272
//     (8 bytes, the demanded CgsID) and RETURNS -- it does not start anything. On a boot-drive
//     in an arbitrary car this WILL fire on Burning Route junctions; the one-shot log below is
//     what makes that visible instead of looking like a dead chain.
//  4. THE RANK FORK. Modes {0, 3, 7, 8} (stunt IS 7) take the PER-MODE rank pair; everything
//     else takes the global one, with a further PlayerHasFinishedLastRank branch. Getting it
//     wrong changes the target score and the time limit rather than crashing.
//
// THREE TAIL LEGS ARE PARKED, each with its console stores written out at the site.
// ============================================================================================
void GameStateModule::StartModeAtLights(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                        GameStateModuleIO::OutputBuffer*             lpOutput,
                                        EGameModeStartMechanism                      leStartMechanism)
{
    CGS_ASSERT(lpInput != 0,  CgsDev::Assert::MessageAt("lpInput != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5365));                                  // :5365
    CGS_ASSERT(lpOutput != 0, CgsDev::Assert::MessageAt("lpOutput != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5366));                                 // :5366
    if (lpOutput == 0)
    {
        return;   // [GUARD]
    }
    (void)lpInput;   // the console asserts it and never dereferences it in this body.

    // @0x82396D64. Trap 2.
    if (leStartMechanism != E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS)
    {
        return;
    }

    // @0x82396D6C: `addis r3, r25, 1 / addi r3, r3, -0x5470` == this + 43920 == the inlined
    // TriggerQueryManager::GetTrafficData().
    const BrnTraffic::TrafficData* lpTrafficData = mTriggerQueryManager.GetTrafficData();
    CGS_ASSERT(lpTrafficData != 0, CgsDev::Assert::MessageAt("lpTrafficData", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5384));                               // :5384
    if (lpTrafficData == 0)
    {
        return;   // [GUARD]
    }

    const BrnProgression::ProgressionData* lpProgressionData =
        mProgressionManager.GetProgressionData();
    CGS_ASSERT(lpProgressionData != 0, CgsDev::Assert::MessageAt("lpProgressionData", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5387));                       // :5387
    if (lpProgressionData == 0)
    {
        return;   // [GUARD]
    }

    const LightTriggerId lLightTriggerId = mTriggerQueryManager.GetPlayerCurrentTrafficLightId();
    const BrnTraffic::JunctionLogicBox* lpJunctionLogicBox =
        lpTrafficData->GetJunctionLogicBoxForTrafficLight(static_cast<u32>(lLightTriggerId));
    CGS_ASSERT(lpJunctionLogicBox != 0, CgsDev::Assert::MessageAt("lpJunctionLogicBox", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5391));                     // :5391
    if (lpJunctionLogicBox == 0)
    {
        return;   // [GUARD]
    }

    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile != 0, CgsDev::Assert::MessageAt("lpProfile", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5394));                                       // :5394
    if (lpProfile == 0)
    {
        return;   // [GUARD]
    }

    // The console's `v15 = 15; do { *v16 = -1; v16 += 44; } while (v15-- >= 0); v58 = -1;`
    // (@0x82396E30-ish) is Array<CheckpointData,16>'s own default construction, inlined -- the
    // 16 per-element -1 heads plus the -1 live-count word at +704. On the host that IS the
    // default constructor of this local, so it is not restated.
    StartGameModeParams lStartGameModeParams;

    // `lwz r23, 0x38(r27); cmpwi r23, -1; beq -> return` @0x82396E9C.
    const u32 luEventJunctionID = lpJunctionLogicBox->GetEventJunctionID();
    if (luEventJunctionID == 0xFFFFFFFFu)
    {
        return;
    }

    const BrnProgression::EventJunction* lpEventJunctionForEvent =
        FindEventJunctionById(lpProgressionData, luEventJunctionID);
    const BrnProgression::RaceEventData* lpEventData =
        (lpEventJunctionForEvent != 0) ? lpEventJunctionForEvent->GetOfflineEvent() : 0;
    CGS_ASSERT(lpEventData != 0, CgsDev::Assert::MessageAt("lpEventData", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5405));                                   // :5405
    if (lpEventData == 0)
    {
        return;   // [GUARD] -- the console falls into the deref here; see the file banner.
    }

    // Trap 1. The same +180972 override the junction check reads -- see the FLAG on it in
    // CheckIfPlayerIsAtJunctionWithAnEvent above; the sentinel path is taken here for the same
    // reason and carries the same DELETE-WHEN. (@0x82396F10..0x82396F20.)
    const s32 liDataGameType = static_cast<s32>(lpEventData->GetMode());
    const GameStateModuleIO::EGameModeType leGameModeType =
        static_cast<GameStateModuleIO::EGameModeType>(mProgressionManager.GetEvent(liDataGameType));

    // ---- Trap 3: the special-event car gate (@0x82396F28..0x82396FAC) -----------------------
    const CgsID lSpecialEventCarId = lpEventData->GetSpecialEventCarId();
    if (lSpecialEventCarId != 0)
    {
        const CgsID lOriginalCarId = GetOriginalCarId(mActivePlayerCarId);
        if (lOriginalCarId != lSpecialEventCarId)
        {
            GameStateModuleIO::WrongCarForChallengeAction lWrongCar;
            lWrongCar.mSpecialEventCarId = lSpecialEventCarId;
            lpOutput->GetGameActionQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWrongCar),
                GameStateModuleIO::E_ACTION_WRONG_CAR_FOR_CHALLENGE,
                static_cast<s32>(sizeof(GameStateModuleIO::WrongCarForChallengeAction)));

            // [DIAG] NOT IN THE X360 BINARY. One-shot, and deliberately NOT behind the
            // BRN_PROP_DIAG env guard the other rungs use: a wrong-car abort looks exactly like
            // a broken chain from the outside (the gesture completes, nothing happens), and this
            // is the one line that tells the two apart. Emitted at most once per process.
            static bool sbWrongCarLogged = false;
            if (!sbWrongCarLogged && CgsDev::Log::gpDebugPrint != 0)
            {
                sbWrongCarLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "[snap] START ABORTED -- this event needs a specific car (event junction "
                    << luEventJunctionID << "); posted action 272.\n";
            }
            return;
        }

        // The two 8-byte OR-stores into Profile+117952 (@0x82396F98..0x82396FAC): that word is
        // the head field of Profile::maHasPlayerSeenTraining, a CgsContainers::BitArray<256>
        // whose fields are u64 with `1 << (index % 64)` -- so `oris 0x100` sets bit 24 and
        // `oris 0x200` sets bit 25, i.e. training types 24 and 25 exactly. Their names confirm
        // the reading: E_TRAINING_TYPE_CORRECT_CAR_FOR_CHALLENGE / _WRONG_CAR_FOR_CHALLENGE --
        // once you have started a special-event race in the right car, neither car tip is ever
        // shown again. Written through Profile::SetTrainingAlreadySeen, not by offset.
        lpProfile->SetTrainingAlreadySeen(BrnProgression::E_TRAINING_TYPE_CORRECT_CAR_FOR_CHALLENGE);
        lpProfile->SetTrainingAlreadySeen(BrnProgression::E_TRAINING_TYPE_WRONG_CAR_FOR_CHALLENGE);
    }

    // The unconditional `ori 0x100` on the same word (@0x82396FB0..0x82396FC0) -- bit 8 ==
    // E_TRAINING_TYPE_DISCOVERS_EVENT.
    lpProfile->SetTrainingAlreadySeen(BrnProgression::E_TRAINING_TYPE_DISCOVERS_EVENT);

    // ✅ [progression wave: medals, 2026-09-06] THE PARK IS PAID. ProgressionManager::
    // FixGameModeRanks @0x82395CD8 is bodied in BrnProgressionManager_Medals.cpp and called here,
    // at the console's own seat (@0x82396FC4).
    // The old note said "none of those four manager members is modelled on this tree's slice" --
    // that reading was wrong, and correcting it is what made the body writable: the four offsets
    // are not manager members at all. ProgressionManager+480 is Profile+0x70 ==
    // mi8CurrentProgressionRank, and +876 / +888 / +904 / +908 are Profile+0x1FC + 4*mode ==
    // maiRankWinsPerOfflineGameMode[0] / [3] / [7] / [8] -- the SAME array
    // Profile::GetNumRankWinsForGameMode reads (BrnProgressionManager.cpp's
    // GetProgressionRankForGameMode banner already pinned that anchor). All four have been
    // modelled at those offsets all along; the function just needed the DWARF-named setter.
    // What it does: no mode's difficulty ladder is allowed to lag more than TWO licence ranks
    // behind, so a player who ranked up on races does not meet rank-1 road rage at licence 5.
    mProgressionManager.FixGameModeRanks();

    // ---- build the params (@0x82396FC8..0x8239729C) -----------------------------------------
    // `sub_823102F0(&frame, this + 235488)` is the inlined
    // RCEntityActiveRaceCarOutputInterface::GetPlayerPosition(); it rides v1 and consumes no GPR
    // slot, which is why Hex-Rays renders Construct with only two arguments.
    lStartGameModeParams.Construct(leGameModeType,
                                   mLastActiveRaceCarInterface.GetPlayerPosition(),
                                   E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS);

    lStartGameModeParams.SetTrafficDensity(lpEventData->GetTrafficDensity());      // lfs 8(event)
    lStartGameModeParams.SetBoostEarning(lpEventData->GetBoostEarning());          // lfs 0xC(event)
    lStartGameModeParams.SetTrafficLightTriggerId(
        mTriggerQueryManager.GetPlayerCurrentTrafficLightId());                    // @0x82397018
    lStartGameModeParams.SetEventJunctionId(luEventJunctionID);                    // @0x82397024
    lStartGameModeParams.SetEventData(lpEventData);                                // @0x82397020
    lStartGameModeParams.SetJunctionID(lpJunctionLogicBox->GetID());               // @0x82397038

    // `ldx r29, r25, 0x456D8` == mActivePlayerCarId (@0x82397034).
    const CgsID lPlayerCarId = mActivePlayerCarId;
    CGS_ASSERT(lPlayerCarId != 0, "That Player Car CGSID is just wrong\n");         // :5471

    // `lwzx r3, r25, 0x456E8` == mpVehicleList, then sub_82233A28 == VehicleList::GetVehicleData
    // (CgsID) -- the committed body at SharedClasses/DataLists/VehicleList.cpp:309.
    const BrnResource::VehicleListEntry* lpPlayerVehicleEntry =
        (mpVehicleList != 0) ? mpVehicleList->GetVehicleData(lPlayerCarId) : 0;
    CGS_ASSERT(lpPlayerVehicleEntry != 0, "Couldnt get the Player Car Vehicle List Entry\n");  // :5475
    lStartGameModeParams.SetPlayerVehicleGamePlayData(lpPlayerVehicleEntry);

    // ---- Trap 4: the rank fork (@0x823970E0..0x82397174) ------------------------------------
    f32 lfProgressionRankAsRatio;
    s8  li8ProgressionRank;
    if (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_RACE ||
        leGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE    ||
        leGameModeType == GameStateModuleIO::E_MODE_STUNT_ATTACK ||
        leGameModeType == GameStateModuleIO::E_MODE_MARKED_MAN)
    {
        lfProgressionRankAsRatio =
            mProgressionManager.GetProgressionRankForGameModeNormalised(leGameModeType);
        li8ProgressionRank = mProgressionManager.GetProgressionRankForGameMode(leGameModeType);
    }
    else
    {
        lfProgressionRankAsRatio = mProgressionManager.GetProgressionRankNormalisedForCurrentRank();
        if (mProgressionManager.PlayerHasFinishedLastRank())
        {
            // `sub_82369020(this + 181268)` == GetProgressionData(); `lwz r11, 0x14(r3)` ==
            // muProgressionRankCount; `addi -1 / extsb` (@0x82397150..0x82397164).
            li8ProgressionRank = static_cast<s8>(
                static_cast<s32>(lpProgressionData->GetProgressionRankCount()) - 1);
        }
        else
        {
            li8ProgressionRank = static_cast<s8>(mProgressionManager.GetProgressionRank());
        }
    }
    lStartGameModeParams.SetProgressionRankAsRatio(lfProgressionRankAsRatio);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "liProgressionRank: "
                                   << static_cast<s32>(li8ProgressionRank) << "\n";
    }

    const BrnProgression::ProgressionRankData* lpProgressionRankData =
        lpProgressionData->GetProgressionRankData(
            static_cast<u32>(static_cast<s32>(li8ProgressionRank)));
    CGS_ASSERT(lpProgressionRankData != 0, CgsDev::Assert::MessageAt("lpProgressionRankData != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5506));       // :5506
    lStartGameModeParams.SetProgressionRankData(lpProgressionRankData);

    // The SECOND EventJunction scan (@0x82397208..0x82397258) -- same key, different assert
    // string, and a different field taken off the record.
    const BrnProgression::EventJunction* lpEventJunction =
        FindEventJunctionById(lpProgressionData, luEventJunctionID);
    CGS_ASSERT(lpEventJunction != 0, CgsDev::Assert::MessageAt("lpEventJunction != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 5512));                   // :5512
    lStartGameModeParams.SetShotGroup(
        (lpEventJunction != 0) ? lpEventJunction->GetShotGroup() : 0);             // junction +0xC

    // `addi r31, r25, 0x1020` == &mModeManager (this + 4128) -- the offset this campaign had to
    // arbitrate: it is 0x1020, NOT the old note's +46640 (which is the training manager).
    const BrnTraffic::LightTriggerStartData* lpStartData =
        mModeManager.GetStartDataForTrafficLight(static_cast<u32>(lLightTriggerId));
    if (lpStartData != 0)   // [GUARD] -- the console tail-calls straight into GetStartDirection
    {
        lStartGameModeParams.SetStartDirection(lpStartData->GetStartDirection(0));
    }

    lStartGameModeParams.SetPlayerBaseDeformation(
        lpProfile->GetPlayerBaseDeformAmount(lPlayerCarId));                       // @0x82397298

    // THE HOP. ModeManager::StartGameMode @0x8234FCE8 is COMMITTED (BrnModeManager_Start.cpp)
    // and mounted; it is what runs SetupGameMode -> PrepareForMode (the 2272-byte action 23) and
    // seats the concrete GameMode.
    mModeManager.StartGameMode(lpOutput, &lStartGameModeParams);

    // [PARKED] THE POST-START TAIL, both halves (@0x823972B0..0x823972FC):
    //   (a) five stores into the embedded StreetManager (base this + 284520):
    //          +0x1D3C, +0x1D80, +0x1D84  <- dword_820A766C, rodata holding -1 (image-read at
    //                                        0x820A766C: FF FF FF FF). Note it is a LOAD from
    //                                        rodata, not an immediate -- the same shape
    //                                        BrnModeManager_Start.cpp already records for
    //                                        SendModeStopMessages.
    //          +0x1DC7, +0x1DC8           <- 0
    //       i.e. GameStateModule +292004 / +292072 / +292076 / +292143 / +292144. A road-rule
    //       "current run" reset; none of the five is a modelled StreetManager member.
    //   (b) for E_MODE_ROAD_RAGE (3) and E_MODE_MARKED_MAN (8) only:
    //          *(this + 46448) = 0   (std, 8 bytes -- r19 is 0 from the prologue @0x82396DA4,
    //                                 so this is a CLEAR, not the trigger id the pseudocode's
    //                                 `*(v8 + 5806) = v12` suggests)
    //          *(this + 46620) = 1
    //       both inside the embedded DriveThruManager (base this + 44240).
    // Leg (a) still has no modelled StreetManager member to bind to. Named in full so landing it is
    // a lookup, not a re-derivation.
    // ✅ Leg (b) LANDED 2026-09-10: the two stores are DriveThruManager::maDriveThroughClosed (+0x8A0,
    // the std of zero) and mbDriveThroughsCloseWhenUsed (+0x94C, the stb of one) --
    // DriveThroughsCloseOnceActivatedUntilFurtherNotice(), whose inverse OnModeFinish / OnModeEnd run.
    if (leGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE ||
        leGameModeType == GameStateModuleIO::E_MODE_MARKED_MAN)
    {
        mDriveThruManager.DriveThroughsCloseOnceActivatedUntilFurtherNotice();
    }
}

// ==============================================================================================
// GameStateModule::CheckForAllEventsOfATypeFound
//
// "Has the player now discovered every event of THIS mode type?" Walk ProgressionData's event
// junction table; for each junction whose offline event has the searched mode (RaceEventData
// +0xEC), count it and, if the profile's event record at the SAME index has its DISCOVERED bit
// set, count it found. The profile's event table is indexed by junction index here, through
// Profile::GetEvent (whose bounds assert the console inlines at every matching junction).
// When none is missing, post action 203 (E_ACTION_ALL_EVENTS_OF_TYPE_DISCOVERED) with one
// word: ProgressionManager::GetEvent(mode), the runtime mode type the HUD names. The bridge turns
// it into GUI 313, which HudMessageAnalyzer::HandleAllJunctionsOfTypeFound shows.
// An empty junction table skips the loop and the assert and posts (0 found of 0).
// ==============================================================================================
void GameStateModule::CheckForAllEventsOfATypeFound(const BrnProgression::Profile* lpProfile,
                                                    GameStateModuleIO::GameActionQueue* lpQueue,
                                                    BrnProgression::RaceEventData::EModeType leModeType)
{
    u32 luEventCountForSearchingType      = 0;
    u32 luEventFoundCountForSearchingType = 0;

    const BrnProgression::ProgressionData* lpProgressionData =
        mProgressionManager.GetProgressionData();
    CGS_ASSERT(lpProgressionData != NULL, CgsDev::Assert::MessageAt("lpProgressionData != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 7175));
    // [GUARD] the console dereferences the null table after the assert (this file's banner).
    if (lpProgressionData == 0)
    {
        return;
    }

    const u32 luJunctionCount = lpProgressionData->GetEventJunctionCount();
    if (luJunctionCount != 0)
    {
        for (u32 luIndex = 0; luIndex < lpProgressionData->GetEventJunctionCount(); ++luIndex)
        {
            const BrnProgression::EventJunction* lpJunction =
                lpProgressionData->GetEventJunction(luIndex);
            if (static_cast<s32>(lpJunction->GetOfflineEvent()->GetMode()) ==
                static_cast<s32>(leModeType))
            {
                ++luEventCountForSearchingType;
                if (lpProfile->GetEvent(luIndex)->IsFlagSet(
                        BrnProgression::ProfileEvent::E_FLAG_DISCOVERED))
                {
                    ++luEventFoundCountForSearchingType;
                }
            }
        }

        CGS_ASSERT(luEventFoundCountForSearchingType <= luEventCountForSearchingType,
                   CgsDev::Assert::MessageAt("luEventFoundCountForSearchingType <= luEventCountForSearchingType", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 7191));
        if (luEventFoundCountForSearchingType < luEventCountForSearchingType)
        {
            return;
        }
    }

    // Action 203, size 4 (E_ACTION_ALL_EVENTS_OF_TYPE_DISCOVERED); BrnGameActions.h does
    // not enumerate it yet.
    const s32 KI_ACTION_ALL_EVENTS_OF_TYPE_DISCOVERED = 203;
    const s32 liGameModeType = mProgressionManager.GetEvent(static_cast<s32>(leModeType));
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liGameModeType),
                      KI_ACTION_ALL_EVENTS_OF_TYPE_DISCOVERED,
                      static_cast<s32>(sizeof(liGameModeType)));
}

// ==============================================================================================
// GameStateModule::CheckForAllEventsBeingFound  (X360 0x82382460)
//
// ⭐ THE [PARKED] NOTE IN OnEventFinishUpdateProfile ABOVE IS PAID -- and this is not a
// nice-to-have: with BrnDriveThruManager.cpp mounted it is ON THE LIVE DRIVING PATH. Measured
// 2026-08-27 with a trap stub in its place: a plain junkyard -> car-select -> free-burn run
// broke into the debugger 9.5 s after the first throttle input, at
//   CheckForAllEventsBeingFound  <-  DriveThruManager::HandleDriveThru
//                                <-  TriggerQueryManager::ProcessPlayerTriggers
//                                <-  ...PreWorldUpdatePlayerTriggersBringUp
// so it runs the moment the player is near any drive-thru region.
//
// "Has the player now discovered EVERY event in the profile?" -- walk the profile's event
// records, stop at the first one whose DISCOVERED bit is clear, and if the walk ran to the end,
// fire the find-all-events achievement (once) and post the 1-byte all-events-discovered action.
//
// ASM, @0x8238247C..0x82382540, store for store:
//     lwz    r11, 0x278(r29)                   ; Profile::miEventCount
//     cmplwi r11, 0 / beq -> tail              ; count == 0 skips the loop ENTIRELY, and the
//                                              ; tail's `0 >= 0` then reports ALL FOUND
//     addi   r30, r29, 0x7084                  ; &maEvents[0].muFlags   (0x7080 + 4)
//   loop:
//     cmplw  r31, r11 / blt ok                 ; UNSIGNED index-vs-count check ...
//     FireAssert("luIndex < static_cast<uint32_t>(miEventCount)", ..., 3004)
//   ok:                                        ; ... i.e. this loop body IS Profile::GetEvent
//     lhz    r11, 0(r30) / clrlwi r11, r11, 31 ; muFlags & 1 == E_FLAG_DISCOVERED
//     cmplwi r11, 0 / beq -> tail              ; first UNdiscovered record ends the walk
//     lwz    r11, 0x278(r29) / addi r31,r31,1 / addi r30,r30,8 / cmplw / blt loop
//   tail:
//     lwz    r11, 0x278(r29) / cmplw r31, r11 / blt -> return
//     addis  r31, r26, 3 / addi r31, r31, -0x3A50   ; this + 181680 == mAchievementManager
//     li     r4, 0x19 / slot 1 -> IsAchievementEarnt(25); if clear, slot 0 -> AchievementEarnt(25)
//     li     r6, 1 / li r5, 0xCA / AddEvent(queue, &stackbyte, 202, 1)
// The achievement pair is spelled through AchievementManagerBase::OnFindAllEvents(), which is
// exactly those four instructions and is where they have to live -- both virtuals are protected
// in the DWARF, so this class cannot name them. See that function's banner for the inference.
//
// ⚠️ The console's 1-byte payload for action 202 is an UNINITIALISED stack slot -- var_50 is
// handed to AddEvent with no preceding store (unlike the sibling id-55 post in
// UnlockCarChallengeForCar, which does `v21[0] = 0` first). A zero is written here instead of
// reproducing an indeterminate read: no consumer of 202 exists in this tree to read it, and
// posting stack garbage would be a real defect, not fidelity. FLAG, deliberate.
// ==============================================================================================
void GameStateModule::CheckForAllEventsBeingFound(BrnProgression::Profile* lpProfile,
                                                  GameStateModuleIO::GameActionQueue* lpQueue)
{
    u32 luIndex = 0;
    if (lpProfile->GetEventCount() != 0)
    {
        // The console's do/while: the first record is read before any continue test, and the
        // walk stops at the first record whose DISCOVERED bit is clear.
        do
        {
            if (!lpProfile->GetEvent(luIndex)->IsFlagSet(
                    BrnProgression::ProfileEvent::E_FLAG_DISCOVERED))
            {
                break;
            }
            ++luIndex;
        }
        while (luIndex < lpProfile->GetEventCount());
    }

    if (luIndex >= lpProfile->GetEventCount())
    {
        mAchievementManager.OnFindAllEvents();

        const u8 lu8Payload = 0;   // FLAG: the console hands AddEvent an uninitialised stack byte
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Payload),
                          GameStateModuleIO::E_ACTION_ALL_EVENTS_DISCOVERED,
                          static_cast<s32>(sizeof(lu8Payload)));
    }
}

}   // namespace BrnGameState

// FOLDED FROM GameStateModule_gTD_00.cpp
// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_gTD_00.cpp
//
// [takedown wave 2026-09-02, conductor] THE TAKEDOWN MANAGER'S PLUMBING INTO GameStateModule.
//
//
// Console positions reproduced here (GameStateModule::PreWorldUpdate @0x823A5328, !IsSimPaused arm,
// ): SetFromVehicleOutputInterface(stack, cachedVehicleOutput == r24 ==
// gsm+250816) ->
// TakedownManager::Update(this+0x238, activeIf, dt, crashQ, &crashingIf, preIn, out, trafficTypeQ)
// -> `*(gsm+249944) = 0` (the module copy's miLength) + TakedownEvent_::Append(gsm+249936, out's
// takedown queue) -> MugshotManager::Update(gsm +0x500, ...) -> PaybackManager::Update(gsm +0x570,
// ...) ->
// ProcessTakedownEvents(actionQ, gsm+249936, out).
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"     // MugshotManager (Construct / Update)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"     // PaybackManager (Construct / Update)
#include "GameSource/GameState/Offences/BrnDriveThruManager.h"    // DriveThroughsCanNowOpenAgain (OnModeFinish / OnModeEnd)
#include "GameSource/GameState/BrnGameActions.h"                   // SetTakedownCameraAction (OnModeFinish)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"   // GameMode::GetCurrentState / GetTimeInMode (harness hook)
#include <cstdlib>   // getenv / atof (harness hook)

namespace BrnGameState
{

// ==============================================================================================
// GameStateModule::CacheTakedownManagerPostWorldInputData  (, post-world `bl` #18)
//
// The console body, statement for statement (r29 == gsm, r26 == lpInput, r31 == gsm+250816,
// r27 == gsm+250800, r30 == lpInput's VehicleOutputInterface):
//   CGS_ASSERT(lpInput, "lpInput")
//   *(gsm+250280) = 0                     the crash queue's miLength                 (Clear)
//   *(gsm+250800) = 0                     the contact-spy word
//   *(+250816 +0x2628) = 0                the cached traffic-state queue's miLength   (Clear)
//   *(+250816 +0x2318) = 0                the cached impact queue's miLength          (Clear)
//   VariableEventQueue<1536,16>::Clear(+250816 +0x65F0)   the cached game-event queue (Clear)
//   std 0, +250816                        the used-cars head, zeroed
//   stb 0, +250816 +0x6C00 .. +0x6C04     the five AggressiveDrivingFlags bytes
//  *(gsm+250800) = *<contact-spy accessor>(lpInput)  [NAME NOT RECOVERED: that
//                                        symbol is unnamed in the image and its body is only the
//                                        "Not locked for reading" assert around a member fetch]
//   RaceCarCrashEvent_::Append(gsm+250272, GetRaceCarCrashEventQueue(lpInput))
//   r30 = GetVehicleOutputInterface(lpInput)
//   PhysicalTrafficState_::Append(+250816 +0x2620, r30 +0x2620)
//   ImpactEvent_::Append          (+250816 +0x2310, r30 +0x2310)
//   VariableEventQueue::Append    (+250816 +0x65F0, r30 +0x65F0)
//   std *(r30 +0), +250816                the used-cars head
//   memcpy(+250816 +0x10, r30 +0x10, 0x2300)   the eight RaceCarStates (CONSOLE span)
// That Clear-then-Append/copy of every field of the cached interface IS
// VehicleOutputInterface::operator= (, BrnVehicleOutputInterface.cpp), so the copy runs
// through that committed symbol -- by sizeof on the host, never at the console's 8960.
//   ONE RECORDED DIFFERENCE: the console leaves the cached AggressiveDrivingFlags ZEROED (the five
//   byte stores above are never followed by a copy) while operator= copies them. Nothing reads that
//   field out of the cache -- SetFromVehicleOutputInterface touches only mUsedRaceCars and the
//   RaceCarStates -- so the copy is inert; recorded rather than special-cased.
//
// The contact-spy handle (gsm+250800, mContactSpyInterface) is cleared and then copied out of the
// buffer here as well; its reader is RumbleManager::Update's UpdateImpacts on the next pre-world.
// ==============================================================================================
void GameStateModule::CacheTakedownManagerPostWorldInputData(
        const GameStateModuleIO::PostWorldInputBuffer* lpInput)
{
    CGS_ASSERT(lpInput, CgsDev::Assert::MessageAt("lpInput", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6258));

    mRaceCarCrashEventQueue.Clear();
    mContactSpyInterface.Construct();
    mContactSpyInterface = *lpInput->GetContactSpyInterface();
    mRaceCarCrashEventQueue.Append(*lpInput->GetRaceCarCrashEventQueue());
    mVehicleOutputInterface = *lpInput->GetVehicleOutputInterface();
}

// ProcessGameEvents @0x823A0A18 case 27 (POST_EVENT_LEAVE) ends with `TakedownManager::
// ClearRaceCarData(gsm+568)` -- wired. The console's other callers are ProcessGameEvents case 32
// (no such arm on this build yet) and GameStateModule::OnModeEnd @0x823767E0 (below, LIVE
// 2026-09-10). ClearAllTakedowns' host callers: OnModeFinish @0x82390EE0 (below) and the online
// case 18 (no such arm yet).
// ⓘ [FX-TAILS-A 2026-09-24] Neither wrapper tests the manager pointer: the console has no pointer to
// test -- the manager is embedded at gsm+0x238 and every caller calls it straight
// (UpdateCurrentMode @0x823513DC `addi r3, r11, 0x238 ; bl IsInTakedownCamera`; OnModeEnd @0x82376804
// `addi r3, r31, 0x238 ; bl ClearRaceCarData`). On PC the pointer is set by Construct
// (ConstructTakedownBringUp) before any caller can run, so the `!= 0` tests these carried were
// invented arms that could only ever pass.
bool GameStateModule::IsInTakedownCamera() const
{
    return mTakedownManager.IsInTakedownCamera();
}

void GameStateModule::ClearTakedownRaceCarData()
{
    mTakedownManager.ClearRaceCarData();
}

// ==============================================================================================
// GameStateModule::OnModeFinish  (X360 0x82390EE0) -- FinishCurrentMode @0x8234B978's last call.
//
//   0x82390EFC  var_30 = -1 ; var_2C = 0 ; var_2B = 0        (an 8-byte SetTakedownCameraAction:
//                                                             focus -1, active 0, signature 0)
//   0x82390F1C  AddEvent(lpOutputBuffer->GetGameActionQueue(), &var_30, 6, 8)
//   0x82390F30  TakedownManager::ClearAllTakedowns(this + 568, lpOutputBuffer->GetGameActionQueue())
//   0x82390F38  stb 0, this+46620 ; std 0, this+46448       == DriveThruManager (this+44240)
//               +0x94C mbDriveThroughsCloseWhenUsed / +0x8A0 maDriveThroughClosed:
//               DriveThroughsCanNowOpenAgain().
// ==============================================================================================
void GameStateModule::OnModeFinish(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    GameStateModuleIO::GameActionQueue* lpActionQueue = lpOutputBuffer->GetGameActionQueue();

    GameStateModuleIO::SetTakedownCameraAction lCameraOff;
    lCameraOff.meFocusOnRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID; // li r11, -1 -> var_30 (the GLOBAL enum -- two of this name are visible here)
    lCameraOff.mbActive              = false;                             // stb 0, var_2C
    lCameraOff.mbIsSignature         = false;                             // stb 0, var_2B
    lCameraOff.mbIsRevengeTakedown   = false;                             // +6 / +7: stack residue on
    lCameraOff.muPad07               = 0;                                 //  the console; zeroed here
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCameraOff),
                            GameStateModuleIO::E_ACTION_SET_TAKEDOWN_CAMERA_STATE,
                            static_cast<s32>(sizeof(lCameraOff)));

    // Embedded by value on the console (`addi r3, r30, 0x238 ; bl ClearAllTakedowns` @0x82390F2C,
    // r4 = the action queue re-fetched @0x82390F24) -- no test. A pointer on this build (see
    // ConstructTakedownBringUp), always set by Construct; the `!= 0` guard it used to carry is gone
    // [FX-TAILS-A 2026-09-24, REVIEW-E on a5619c29].
    mTakedownManager.ClearAllTakedowns(lpActionQueue);

    mDriveThruManager.DriveThroughsCanNowOpenAgain();
}

// ==============================================================================================
// GameStateModule::OnModeEnd  (X360 0x823767E0) -- SendModeStopMessages @0x8234BEC0's tail.
//
//   0x823767F4  MugshotManager::OnRoundEnd(this + 0x500, r4)
//   0x823767FC  PaybackManager::OnRoundEnd(this + 0x570, r4)
//     ✅ [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] WIRED -- the "argument unrecovered" FLAG that
//     stood here is resolved. The console sets only r3 at these two calls because r4 is ALREADY the
//     argument: SendModeStopMessages loads `mr r4, r29` (== !lbOnlineLobbyHandover) @0x8234C6DC
//     before `bl OnModeEnd`, OnModeEnd never writes r4, and MugshotManager::OnRoundEnd @0x82357AF8
//     only reads it (`clrlwi r11, r4, 24`, no r4 store), so the same bool reaches both callees.
//     DWARF: OnModeEnd(bool) :609, both OnRoundEnd(bool lbResetState).
//   0x82376804  TakedownManager::ClearRaceCarData(this + 568)
//   0x82376808  lwz this+7604 == meCurrentGameModeType ; == 2 || == 16 (the two SHOWTIME modes) ->
//   0x82376838      stw 0, +284504   == mShowtimePendingTrafficIndexStack's count (Clear)
//   0x8237683C      stfs 2.0, +284444 == mfTimeSinceLastCrashMode (the post-mode lockout, re-armed)
//   0x82376840      sth -1, +284508  == muShowtimeRequestedTrafficIndex (K_INVALID_VEHICLE_INDEX)
//   0x82376848  stb 0, this+46620 ; std 0, this+46448   == DriveThroughsCanNowOpenAgain()
// ==============================================================================================
void GameStateModule::OnModeEnd(bool lbResetState)
{
    // Embedded by value on the console (+0x500 / +0x570: no test before either `bl`, 0x823767F4 /
    // 0x823767FC); held by pointer on this build and never null here, so no test here either
    // [FX-FLOW 2026-09-24, review D]: ConstructTakedownBringUp -- called from GameStateModule::
    // Construct -- assigns both from `new` and dereferences them on the spot (->Construct(this)),
    // nothing else ever writes (&mMugshotManager), and (&mPaybackManager) is cleared only by
    // GameStateModule::Destruct, which has no ModeManager leg left to end a mode. OnModeEnd's one
    // caller is ModeManager::SendModeStopMessages, i.e. a running game.
    mMugshotManager.OnRoundEnd(lbResetState);
    mPaybackManager.OnRoundEnd(lbResetState);

    // [diag] BRN_MODEMGR_DIAG -- NOT IN THE X360 BINARY: the round-end hand-off.

    ClearTakedownRaceCarData();

    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    if (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
        leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        mShowtimePendingTrafficIndexStack.Clear();
        mfTimeSinceLastCrashMode        = 2.0f;
        muShowtimeRequestedTrafficIndex = 0xFFFFu;
    }

    mDriveThruManager.DriveThroughsCanNowOpenAgain();
}

// ==============================================================================================
// GameStateModule::CopyInputDataToPaybackManager  (X360 0x8239AA78; DWARF BrnGameStateModule.h:835)
// [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] Called by PreWorldUpdate @0x823A5328 at 0x823A572C, once
// per pre-world tick, unconditionally, right after DriveThruManager::Update. Both callees are
// inlined on the console:
//   0x8239AA98  PreWorldInputBuffer::GetTimerStatusInterface (0x8231CE28)
//   0x8239AAA0..0x8239AB0C  the 48-byte copy into PaybackManager +0x00  == SetTimerInterface
//   0x8239AB10  PreWorldInputBuffer::GetControllerInput (0x823632F8)
//   0x8239AB14..0x8239AB58  `lbz 0xC` (mbDirtyTrickPressed) -> +0x264/+0x265 and the press-edge
//                           ChangeState(5)                                 == SetDirtyTrickButtonState
// Without it the payback manager's timer copy never left Construct's Clear(): its aggressor timer
// never advanced and the OnRoundStart / OnRoundEnd reseed read frame count 0; and the dirty-trick
// button never reached the aggressor FSM.
// ==============================================================================================
void GameStateModule::CopyInputDataToPaybackManager(
        const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer)
{
    // Embedded by value on the console (this + 0x570, used with no test); a pointer on this build,
    // never null here (see OnModeEnd above: set from `new` in GameStateModule::Construct's
    // ConstructTakedownBringUp, cleared only by Destruct) -- the one caller is the pre-world tick.

    mPaybackManager.SetTimerInterface(lpPreWorldInputBuffer->GetTimerStatusInterface());
    mPaybackManager.SetDirtyTrickButtonState(
        lpPreWorldInputBuffer->GetControllerInput()->mbDirtyTrickPressed);

    // [diag] BRN_MODEMGR_DIAG -- NOT IN THE X360 BINARY. One line, the first time a running game
    // timer arrives: the witness that the copy is dispatched and carries a live frame count.
    static bool       sbReported    = false;
    const CgsSystem::TimerStatus* const lpGameTimer =
        lpPreWorldInputBuffer->GetTimerStatusInterface()->GetGameTimerStatus();
}

}

// FOLDED FROM GameStateModule_gUI_00.cpp
// b5-decomp/src/GameSource/GameState/GameStateModule_gUI_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the rest
// of the module's committed bodies are in BrnGameStateModule.cpp).
//
// THE gateui WAVE'S GameState-SIDE PLUMBING -- the bodies that carry a broken prop from the
// world module all the way to a game action the GUI bridge can translate:
//
//     world OutputBuffer::GetGameEventQueue()            (produced by PropEntityModule::
//                                                         ProcessContacts -> the bridge legs)
//        -> GameStateModule::PostWorldUpdate             (GameStateModule_wW_01.cpp: refreshes
//                                                         mLastActiveRaceCarInterface, appends
//                                                         into mGameEventCarryQueue, runs the
//                                                         mode manager's scorers and HUD pump)
//        -> PreWorldUpdateStuntBringUp                   (X360 PreWorldUpdate @0x823A5328)
//             * ProcessGameEvents                        (GameStateModule_ProcessGameEvents.cpp,
//                                                         case 111 -> StuntManager::OnPropHit)
//             * TriggerQueryManager::UpdateTriggers      (arms maActiveTriggers for NEXT frame)
//             * StuntManager::Update                     (consumes the latch ->
//                                                         ProcessStuntElement -> action 58)
//
// Each function's console attestation, and each deliberate deviation, is written out at its
// declaration in BrnGameStateModule.h and again at its body below. Nothing here is fabricated:
// every reduction is named as a reduction.
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/GameState/BrnGameStateModule.h"

#include <stdlib.h>                                                     // getenv ([UI-gate] diag)
#include <string.h>                                                     // memcpy / memset (the case-20 payload view)
#include <stdio.h>                                                      // optional harness ready-file gate

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16>

#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer (lock + GetGameActionQueue)
#include "GameSource/GameState/FlybyManager/BrnGameStateOnlineFlybyManager.h" // current player-status snapshot
#include "GameSource/GameState/BrnGameStateModule.h"             // mTakedownEventQueue (the post-world scoring leg)
#include "GameSource/GameState/BrnGameEvents.h"                         // RecordPropHitEvent / E_EVENT_RECORD_PROP_HIT / E_EVENT_CHANGE_WORLD_REGION
#include "GameSource/GameState/ImageManager/BrnGameStateImageManagerBase.h" // WorldRegionChangeEvent (the case-115 payload)
#include "GameSource/GameState/Offences/BrnStuntManager.h"              // StuntManager::OnPropHit / Update
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // UpdateTriggers / GetActiveTrigger*
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // the accessor's return type
#include "GameSource/GameState/BrnGameActions.h"                        // RankInfoResponseAction (the case-80 record)
#include "GameSource/GameState/SharedIO/BrnGameActionData.h"            // GameStats (the case-79 record)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"     // ProgressionManager::GetGameStats
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData::GetProgressionRankCount
#include "GameSource/GameState/Progression/BrnProfile.h"                // Profile::GetNumRankWinsForGameMode
#include "GameSource/GameState/Progression/BrnDerivedCars.h"

#include "SharedClasses/Trigger/BrnTriggerData.h"                       // TriggerData::GetRegion
#include "SharedClasses/Trigger/BrnTriggerBase.h"                       // TriggerRegion::GetType
#include "SharedClasses/Trigger/BrnGenericRegion.h"                     // GenericRegion::Type (SMASH / OVERDRIVE_BOOST)

// ---- [D4 stuntrace WAVE D -- THE PUMP] ------------------------------------------------------
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::Pre/PostWorldUpdate, StartGameMode
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // GameMode::GetCurrentState / GetIntroDurationSeconds
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // StartGameModeParams (the case-20 local)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h"  // CgsSystem::TimerStatusInterface (the pump's new argument)
#include "GameShared/GameClasses/Core/CgsID.h"                          // CgsIDCompress ([car] BRN_DEBUG_PLAYER_CAR)
#include "SharedClasses/DataLists/VehicleList.h"                        // VehicleList::GetVehicleCount/GetVehicleData ([car])
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // VehicleListEntry::GetId/GetName/GetDefaultWheelName ([car])
#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"          // CgsResource::ID::HashString ([car-audio] audit)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"   // CarData::GetId ([car-audio] junkyard pick)
#include "SharedClasses/DataLists/WheelList.h"                          // WheelList::FindWheelIndexFromName/GetWheelData ([car])
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h" // [FX-GS2 G10-D11] GameStateToGuiInterface::AddOnTailEvent
#include "GameSource/Network/SharedIO/BrnNetworkModuleGameStateIOInterfaces.h" // [FX-FLOW NEW-EMMTAIL] GameStateToNetworkInterface::SetActiveRaceCarIndex
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // [FX-GS2 G10-D11] GetUsedCarsBitArray / GetRaceCar
#include "rw/math/vpu/vector3_operation.h"                              // [FX-GS2 G10-D11] Magnitude (the player's speed)
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h" // the case-17/18 arms
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // TrainingManager::ForceUnpause (case 17)
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"     // TakedownManager::ClearAllTakedowns (case 18)
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"       // MugshotManager::OnRoundStart (case 18)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"       // PaybackManager::OnRoundStart (case 18)
namespace BrnGameState
{

// ARTIST 0x82397568: update progression, reset the model at the current location,
// then apply the selected car's saved palette and colour.
void GameStateModule::HandleChangePlayerCarEvent(
    const GameStateModuleIO::ChangePlayerCarEvent* lpEvent,
    GameStateModuleIO::GameActionQueue* lpActions)
{
    OnPlayerCarChange(lpEvent->mCarModelId, lpEvent->mWheelModelId, lpActions, true);
    GameStateModuleIO::ResetPlayerCarAction lReset = GameStateModuleIO::ResetPlayerCarAction();
    lReset.mCarModelId = lpEvent->mCarModelId;
    lReset.mWheelModelId = lpEvent->mWheelModelId;
    lReset.mePlayerScoringIndex = GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT;
    lReset.muReserved0x42 = lpEvent->mbResetPlayerCamera;
    lReset.mbKeepResetSection = lpEvent->mbKeepResetSection;
    lReset.mfDeformationAmount = mProgressionManager.GetProfile()->GetPlayerBaseDeformAmount(lpEvent->mCarModelId);
    lReset.miBaseDeformationType = lReset.mfDeformationAmount > 0.0f ? 1 : -1;
    lpActions->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReset),
                       GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR, sizeof(lReset));
    GameStateModuleIO::CarSelectChangeColourAction lColour;
    s32 liColour, liPalette;
    mProgressionManager.GetCarColourAndPalette(lpEvent->mCarModelId, &liColour, &liPalette);
    lColour.muPaletteIndex = liPalette;
    lColour.muColourIndex = liColour;
    lpActions->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lColour),
                       GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR, sizeof(lColour));
}

// ============================================================================
// â­ [gateui] GetDeveloperChallengeManager -- the body behind the declaration the StreetManager
// wave added with no member behind it.
//
// The console never emits an accessor for this subobject: every call site reaches it through the
// inlined `mpGameStateModule + 185712` pointer adjust, and both of them assert it non-null with
// the accessor spelled out --
//   StreetManager::ProcessNewRoadScore      @0x823496C8 ("mpGameStateModule->GetDeveloperChallengeManager()")
//   StuntManager::ProcessStuntElement       @0x8239CDB0 (same string, BrnStuntManager.cpp)
// De-inlined here over the real embedded member (BrnGameStateModule.h,
// mDeveloperChallengeManager), so no reconstructed body has to poke a byte offset.
// ============================================================================
DeveloperChallengeManager* GameStateModule::GetDeveloperChallengeManager()
{
    return &mDeveloperChallengeManager;
}

// ============================================================================
// â­ [P1 sim-pause] PostWorldInput -- the free-function accessor BridgeGuiToGameState posts
// through (console: returns the module's post-world input GameEventQueue). PC body: the CARRY
// QUEUE, the named reduction spelled out at the declaration (BrnGameStateModule.h).
// ============================================================================
namespace GameStateModuleIO
{
    CgsModule::VariableEventQueue<1536, 16>* PostWorldInput(GameStateModule* lpModule)
    {
        return &lpModule->mGameEventCarryQueue;
    }
}

// ==============================================================================================
// [FX-GS2 2026-09-23, crash-parity G10-D11] The rival-tailing clock and its per-frame check.
//
// GetRivalTailingTime / SetRivalTailingTime: CheckForTailingRivals keeps one f32 per race-car
// slot at gsm+0x32D90 + 4 * slot (r27, stepped by `addi r27, r27, 4` @0x82376384 for EVERY slot,
// the player's included). The DWARF array is f32[7] (BrnGameStateModule.h:792), so slot 7 lands on
// the next word, muNetworkGameRandomSeed (gsm+0x32DAC). That aliasing is the console's; it is
// routed here through the seed word's bits rather than by indexing past the array.
// ==============================================================================================
f32 GameStateModule::GetRivalTailingTime(s32 liRaceCarIndex) const
{
    if (liRaceCarIndex < 7)
    {
        return mafRivalTailingTimes[liRaceCarIndex];
    }
    f32 lfTime;
    memcpy(&lfTime, &muNetworkGameRandomSeed, sizeof(lfTime));   // slot 7 == gsm+0x32DAC
    return lfTime;
}

void GameStateModule::SetRivalTailingTime(s32 liRaceCarIndex, f32 lfTime)
{
    if (liRaceCarIndex < 7)
    {
        mafRivalTailingTimes[liRaceCarIndex] = lfTime;
        return;
    }
    memcpy(&muNetworkGameRandomSeed, &lfTime, sizeof(lfTime));   // slot 7 == gsm+0x32DAC
}

// ----------------------------------------------------------------------------------------------
// CheckForTailingRivals @0x82375F90 (DWARF BrnGameStateModule.h:808; sole caller PreWorldUpdate
// @0x823A5328, `bl` @0x823A57A0). For each rival sitting just behind the player at speed, run a
// clock; at 3 s, post the GUI "on your tail" record and restart it.
//     0x82375FBC..0x82375FEC  mode type (ModeManager +0xD94) == 0 (offline race), or the current
//                             mode (+0xD98) is online (+0xAC) -- else return
//     0x82375FF0..0x82376000  meControllerState (gsm+0x38B64) == 3 -- else return
//     0x82376004..0x823760F8  the player's used-car bit (the bit array's "invalid index" assert)
//     0x823760FC..0x8237619C  |RaceCarState.mLinearVelocity (+0x330)| > 50.0 (flt_820138DC):
//                             vmsum3fp128 + vrsqrtefp + two Newton steps (0 when the square is 0);
//                             the exact root here, this tree's convention (BrnMathUtils.h)
//     0x823761A0..0x823761DC  assert "lpScoringSystem != NULL" (line 0x1949); the player's
//                             GetRaceCarDistanceToFinish
//     loop slot 0..7 (the enum ++ carries BurnoutConstants.h:39's range assert):
//       0x82376220  the player's slot: skipped (its clock untouched)
//       0x82376234  player < rival (fcmpu/bge) and !((player + 20.0 (flt_820054CC)) < rival)
//                   (fcmpu/blt) -- the rival is BEHIND the player by at most 20 m
//       0x823762DC  the rival's used-car bit ; 0x82376318 RaceCarState.mfSpeedMPH (+0x3CC) > 50.0
//       0x82376328  all true: clock += the time step
//       0x82376338  otherwise `fsel f0, f0, 0.0, f0`: a clock >= 0 resets to 0, a negative or NaN
//                   one is kept
//       0x82376344  !(clock < 3.0 (flt_8202AC20)) (a NaN passes too): AddOnTailEvent(GetRivalId(slot)
//                   from the module's active-car snapshot (gsm+0x397E0), slot) through the
//                   write-locked GetGameStateToGuiInterface, then the clock = 0.0 (0x8237637C)
// Because of the slot-7 aliasing, ClearData's 0xFFFFFFFF seed reads as a NaN clock: on the first
// gated frame in which the player is not in slot 7 the console posts one record for slot 7 and the
// seed word becomes 0.0f. That is the console's arithmetic and it is kept.
// ----------------------------------------------------------------------------------------------
void GameStateModule::CheckForTailingRivals(GameStateModuleIO::OutputBuffer* lpOutput,
                                            const BrnPhysics::Vehicle::VehicleOutputInterface* lpVehicleOutput,
                                            f32 lfTimeStep)
{
    if (mModeManager.GetCurrentGameModeType() != GameStateModuleIO::E_MODE_OFFLINE_RACE)
    {
        const GameMode* lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        if (lpCurrentGameMode == 0 || !lpCurrentGameMode->IsOnline())
        {
            return;
        }
    }

    if (meControllerState != E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE)
    {
        return;
    }

    if (!lpVehicleOutput->GetUsedCarsBitArray().IsBitSet(static_cast<u32>(GetPlayerActiveRaceCarIndex())))
    {
        return;
    }

    const f32 lfPlayerSpeed = rw::math::vpu::Magnitude(
        lpVehicleOutput->GetRaceCar(static_cast<u32>(GetPlayerActiveRaceCarIndex()))->mLinearVelocity);
    if (!(lfPlayerSpeed > 50.0f))
    {
        return;
    }

    const ScoringSystem* lpScoringSystem = mModeManager.GetScoringSystem();
    CGS_ASSERT(lpScoringSystem != NULL, CgsDev::Assert::MessageAt("lpScoringSystem != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 6473));

    const f32 lfPlayerDistanceToFinish =
        lpScoringSystem->GetRaceCarDistanceToFinish(GetPlayerActiveRaceCarIndex());

    for (::EActiveRaceCarIndex leIndex = ::E_ACTIVE_RACE_CAR_INDEX_0;
         leIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT;
         leIndex++)
    {
        if (GetPlayerActiveRaceCarIndex() == leIndex)
        {
            continue;
        }

        const f32 lfDistanceToFinish = lpScoringSystem->GetRaceCarDistanceToFinish(leIndex);
        f32 lfTime = GetRivalTailingTime(static_cast<s32>(leIndex));

        if (lfPlayerDistanceToFinish < lfDistanceToFinish
            && !((lfPlayerDistanceToFinish + 20.0f) < lfDistanceToFinish)
            && lpVehicleOutput->GetUsedCarsBitArray().IsBitSet(static_cast<u32>(leIndex))
            && lpVehicleOutput->GetRaceCar(static_cast<u32>(leIndex))->mfSpeedMPH > 50.0f)
        {
            lfTime = lfTime + lfTimeStep;
        }
        else
        {
            lfTime = (lfTime >= 0.0f) ? 0.0f : lfTime;   // fsel
        }
        SetRivalTailingTime(static_cast<s32>(leIndex), lfTime);

        if (!(lfTime < 3.0f))
        {
            lpOutput->GetGameStateToGuiInterface()->AddOnTailEvent(
                mLastActiveRaceCarInterface.GetRivalId(leIndex), leIndex);
            SetRivalTailingTime(static_cast<s32>(leIndex), 0.0f);

        }
    }
}

}

// FOLDED FROM GameStateModule_wW_01.cpp
// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_wW_01.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h).
//
// GameStateModule::PostWorldUpdate: the module's post-world pass, called once per sub-step by
// BrnGameModule::DoUpdate_GameStatePostWorld with the PostWorldInputBuffer that
// BridgeWorldToGameState has just filled. In order:
//   1. mbIsUpdating on; the sim-pause test; the buffer read-locked.
//   2. the module's snapshots of the buffer: the active and global race-car interfaces, the AI car
//      interface, the traffic-type responses; the game events folded into the carry queue that the
//      next pre-world pass drains.
//   3. unless the sim is paused: the takedown caches, then ModeManager::PostWorldUpdate with the
//      module's takedown-event queue and its cached sim timestep.
//   4. with update-set bit 0x8 (in game): the trigger queries.
//   5. ProcessContacts; the per-car crashing flags; the player index publish; the boost tips.
//   6. the buffer unlocked; mbIsUpdating off.
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"                   // PostWorldInputBuffer
#include "GameSource/GameState/BrnGameStateModule.h"              // mpTakedownCache's queues
#include "GameSource/GameState/ModeManager/BrnModeManager.h"             // ModeManager::PostWorldUpdate
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // the landmark pass
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"     // the boost tips
#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::HasPlayerSeenTrainingType
#include "SharedClasses/Progression/BrnTrainingTypes.h"                  // E_TRAINING_TYPE_DANGER_BOOST_FULL
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // BoostOutputInfo

namespace BrnGameState
{

namespace
{
    // Rodata 5.0f: the settle gap between two training tips (the same literal the other tip
    // requesters name KF_TRAINING_TIP_SETTLE_TIME).


    // Update-set bit 0x8: raised by BrnGameModule::ConstructUpdateSetFromFsm while in game.
    const u32 KU_UPDATE_SET_IN_GAME = 0x8u;
}

void GameStateModule::PostWorldUpdate(CgsModule::IOBufferStack*                      /*lpUpdateInputBufferStack*/,
                                      CgsModule::IOBufferStack*                      /*lpUpdateOutputBufferStack*/,
                                      const GameStateModuleIO::PostWorldInputBuffer* lpPostWorldInputBuffer,
                                      BrnUpdateSet                                   lUpdateSet)
{
    // The console brackets the body and each leg below with PerfMonCpu monitors whose handles this
    // slice of the module does not model; they only time the frame.
    mbIsUpdating = true;
    const bool lbSimPaused = IsSimPaused(true, false);
    lpPostWorldInputBuffer->LockForRead();

    // ---- the snapshots. Copied by assignment: every one of these interfaces is a different size
    // on the host, so the console's byte counts do not apply.
    mLastActiveRaceCarInterface = *lpPostWorldInputBuffer->GetActiveRaceCarOutputInterface();
    mLastGlobalRaceCarInterface = *lpPostWorldInputBuffer->GetGlobalRaceCarOutputInterface();
    mLastAICarOutputInterface   = *lpPostWorldInputBuffer->GetAICarOutputInterface();

    // The traffic-type responses TakedownManager::Update reads next pre-world (gsm+278480).
    mLastTrafficTypeResponseQueue.Clear();
    mLastTrafficTypeResponseQueue.Append(*lpPostWorldInputBuffer->GetTrafficTypeResponseQueue());

    // The frame's game events join the carry queue; PreWorldUpdate's ProcessGameEvents drains it.
    mGameEventCarryQueue.Append(*lpPostWorldInputBuffer->GetGameEventQueue());

    if (!lbSimPaused)
    {
        CacheTakedownManagerPostWorldInputData(lpPostWorldInputBuffer);
        mModeManager.PostWorldUpdate(lpPostWorldInputBuffer,
                                     &mTakedownEventQueue,
                                     mfSimTimeStep);
    }

    if ((static_cast<u32>(lUpdateSet) & KU_UPDATE_SET_IN_GAME) != 0)
    {
        mTriggerQueryManager.PostWorldUpdate(lpPostWorldInputBuffer, &mModeManager,
                                            GetPlayerActiveRaceCarIndex());
    }

    ProcessContacts(lpPostWorldInputBuffer->GetContactSpyInterface());

    // ---- the per-slot "this car is crashing" cache IsRaceCarCrashing answers from.
    for (s32 liSlot = 0; liSlot < E_ACTIVE_RACE_CAR_INDEX_COUNT; ++liSlot)
    {
        const ::EActiveRaceCarIndex leSlot = static_cast< ::EActiveRaceCarIndex>(liSlot);
        maRaceCarCrashing[liSlot] =
            mLastActiveRaceCarInterface.IsRaceCarActive(leSlot) &&
            mLastActiveRaceCarInterface.GetRaceCarState(leSlot)->mbCrashing;
    }

    // ---- the player's indices. ProcessContacts above ran on the previous frame's values; that
    // one-frame lag is the console's.
    if (mLastActiveRaceCarInterface.IsPlayerCarActive())
    {
        mePlayerActiveRaceCarIndex = mLastActiveRaceCarInterface.GetPlayerActiveRaceCarIndex();
        miPlayerGlobalRaceCarIndex =
            lpPostWorldInputBuffer->GetGlobalRaceCarOutputInterface()->GetPlayerGlobalRaceCarIndex();

        const BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
        CGS_ASSERT(lpProfile != NULL, CgsDev::Assert::MessageAt("lpProfile != NULL", "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp", 1736));
        (void)lpProfile;

        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput =
            lpPostWorldInputBuffer->GetActiveRaceCarOutputInterface();
        const BrnWorld::RaceCarEntityModuleIO::BoostOutputInfo* lpBoostInfo =
            lpActiveRaceCarOutput->GetBoostOutputInfoN(lpActiveRaceCarOutput->GetPlayerActiveRaceCarIndex());

        // [X] NOT REPRODUCED: the best-burnout-chain block. When the player's chain count beats
        // the profile's muBestNewBurnoutChainScore the console stores the new best, asserts
        // "muNumBoostChains > 0", requests tip E_TRAINING_TYPE_BURNOUT and raises achievement 18
        // at a chain of 20 (AchievementManagerBase::OnBoostChain). Blocked: Profile has no
        // SetBestBurnoutChain on this tree and OnBoostChain has no body.

        // The danger-boost-full tip: a danger boost bar that is full. The console skips on `blt`,
        // so an unordered compare still asks.
        if (lpBoostInfo->meBoostType == BrnWorld::E_BOOST_TYPE_DANGER &&
            !(lpBoostInfo->mfBoostAmount < lpBoostInfo->mfMaxBoost) &&
            !mTrainingManager.IsTipPending() &&
            !mTrainingManager.IsInPictureParadise() &&
            mTrainingManager.IsTipAllowedInGameMode(BrnProgression::E_TRAINING_TYPE_DANGER_BOOST_FULL))
        {
            BrnProgression::Profile* lpTipProfile = mTrainingManager.GetProfile();
            CGS_ASSERT(lpTipProfile != 0, "lpProfile");
            if (!lpTipProfile->HasPlayerSeenTrainingType(BrnProgression::E_TRAINING_TYPE_DANGER_BOOST_FULL) &&
                !(mTrainingManager.GetTimeSinceLastTip() < KF_TRAINING_TIP_SETTLE_TIME))
            {
                mTrainingManager.RequestTip(BrnProgression::E_TRAINING_TYPE_DANGER_BOOST_FULL);
            }
        }

        // [X] NOT REPRODUCED: the lost-boost-chunk tip (E_TRAINING_TYPE_AGGRESSION_LOST_BOOST_CHUNK).
        // Its gate reads two flag bits (0x400000, 0x800000) of a 64-bit word at gsm+0x28960 that
        // this slice of the module does not model.
    }
    else
    {
        mePlayerActiveRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID;
        miPlayerGlobalRaceCarIndex = ::E_GLOBAL_RACE_CAR_INDEX_INVALID;
    }

    lpPostWorldInputBuffer->UnlockForRead();
    mbIsUpdating = false;

    // [X] NOT REPRODUCED: the tail store of lpPostWorldInputBuffer->GetInPictureParadise() into
    // the rumble manager's mbInPictureParadise (its UpdatePictureParadiseState is not declared on
    // this tree).
}

}
