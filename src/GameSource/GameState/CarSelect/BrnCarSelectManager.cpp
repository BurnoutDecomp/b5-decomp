#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"

#include <cstddef>   // offsetof (layout asserts)
#include <cstring>   // memset/memcpy (opaque action payloads)

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CgsDev::Assert Begin/Fire/EndAssert
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<13312,16> (game-action queue)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags

#include "SharedClasses/Trigger/BrnSpawnLocation.h"                 // BrnTrigger::SpawnLocation (GetType / Type / E_TYPE_COUNT)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h" // BrnProgression::CarData (Id / unlock-type / deform)
#include "GameSource/GameState/Progression/BrnProfile.h"           // BrnProgression::Profile (SetChosenLiveryIdForBaseCar / FindCar)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"// BrnProgression::ProgressionManager (GetCarColourAndPalette)
#include "GameSource/GameState/BrnGameStateModule.h"               // BrnGameState::GameStateModule (streaming / car-change / pause hooks)

namespace BrnGameState
{
// The game-action queue the callers hand us is an ::InputBuffer::GameActionQueue* (X360 == the
// OutputBuffer's embedded queue at this+4). Reinterpret it to the real backing type and post events
// via AddEvent (established precedent: BrnDriveThruManager.cpp / BrnTriggerQueryManager.cpp).
typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;

static inline GameActionQueueImpl* AsActionQueue(::InputBuffer::GameActionQueue* lpQueue)
{
    return reinterpret_cast<GameActionQueueImpl*>(lpQueue);
}

// The verbatim X360-baked source path the asserts reference (every assert in this TU shares it).
static const char* const KAC_CSM_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/CarSelect/BrnCarSelectManager.cpp";
static const char* const KAC_SPAWNLOC_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\trigger\\BrnSpawnLocation.h";

// ----------------------------------------------------------------------------
// X360-baked tuning constants (the float rodata the bodies compare against). The DWARF declares the
// named class-scope KF_* statics at BrnCarSelectManager.cpp:30-41; only the immediates the
// reconstructed bodies use are recovered, named by their X360 rodata address + meaning.
//   flt_82001CC0 ==  0.0 : the timer-reset value.
//   flt_82001D9C ==  2.0 : KF_CARSELECT_TRANSITION_IN_MIN_TIME_NO_CARS      (Update case 1, no unlock cars).
//   flt_820095C0 ==  6.0 : KF_CARSELECT_TRANSITION_IN_MIN_TIME_WITH_CARS    (Update case 1, has unlock cars).
//   flt_82001DA0 ==  0.5 : KF_CARSELECT_TRANSITION_IN_REQUEST_STREAMING     (Update case 1 stream-kick).
//   flt_8202AE2C ==  7.0 : KF_CARSELECT_UNLOCK_MIN_TIME                     (unlock sequence min hold).
//   flt_82001C98 ==  1.0 : KF_CARSELECT_DELAY_BEFORE_CHANGE_CAR             (UpdateRequestCarChange).
//   flt_82029BB8 == 0.85 : KF_LIVERYSELECT_DELAY_BEFORE_CHANGE_CAR / start-of-game deform amount.
//   flt_82029F18 ==  1.5 : KF_CARSELECT_CHANGE_CAR_TELEPORT_DELAY           (UpdateRequestCarChange).
//   flt_820037C8 == -1.0 : the "no unlock target" deform sentinel (EnterJunkyardAtStartOfGame).
// ----------------------------------------------------------------------------
static const f32 KF_TIMER_RESET                        =  0.0f;  // flt_82001CC0
static const f32 KF_TRANSITION_IN_MIN_TIME_NO_CARS     =  2.0f;  // flt_82001D9C
static const f32 KF_TRANSITION_IN_MIN_TIME_WITH_CARS   =  6.0f;  // flt_820095C0
static const f32 KF_TRANSITION_IN_REQUEST_STREAMING    =  0.5f;  // flt_82001DA0
static const f32 KF_CARSELECT_UNLOCK_MIN_TIME          =  7.0f;  // flt_8202AE2C
static const f32 KF_CARSELECT_DELAY_BEFORE_CHANGE_CAR  =  1.0f;  // flt_82001C98
static const f32 KF_LIVERYSELECT_DELAY_BEFORE_CHANGE   =  0.85f; // flt_82029BB8
static const f32 KF_CARSELECT_CHANGE_CAR_TELEPORT_DELAY=  1.5f;  // flt_82029F18
static const f32 KF_STARTOFGAME_DEFORM_SENTINEL        = -1.0f;  // flt_820037C8

// ----------------------------------------------------------------------------
// X360-attested game-action event-type ids + payload sizes (the literal `li r5,<type>` /
// `li r6,<size>` immediates each AddEvent is given). Names mirror the GameStateModuleIO action
// structs the DWARF cpp lists; their field layouts are NOT in the exports, so each payload is
// posted as a sized opaque buffer (see `flagged`). The few payload bytes the X360 does write are
// reproduced at their attested in-payload offsets.
// ----------------------------------------------------------------------------
enum EGameAction
{
    KI_ACTION_RESET_PLAYER_CAR          = 0,    // size 80 (0x50)  -- ResetPlayerCarAction
    KI_ACTION_CAR_CHANGED_SUFFIX        = 2,    // size 8          -- CarSelectionChanged (post-set id)
    KI_ACTION_PAUSED_IN_CAR_SELECT      = 80,   // size 1          -- PausedInCarSelectAction
    KI_ACTION_TRANSITION_IN             = 73,   // size 2          -- CarSelectTransitionInAction
    KI_ACTION_CAR_SELECT_READY          = 75,   // size 4          -- CarSelectReadyAction
    KI_ACTION_CAR_MOD_SCREEN            = 76,   // size 8          -- CarSelectModificationScreen
    KI_ACTION_CAR_SELECT_EXIT           = 74,   // size 1          -- CarSelectExitAction
    KI_ACTION_FORCE_EXIT                = 78,   // size 1          -- CarSelectAbortAction
    KI_ACTION_NEW_CAR_UNLOCKED          = 62,   // size 16         -- NewCarUnlockedAction
    KI_ACTION_CAR_UNLOCK                = 79,   // size 8          -- CarUnlockAction (colour/palette)
    KI_ACTION_CAR_UNLOCK_END            = 63,   // size 1          -- CarUnlockEndAction
    KI_ACTION_CAR_SELECTION_CHANGED     = 64,   // size 64 (0x40)  -- CarSelectionChangedAction
    KI_ACTION_CAR_SELECTION_DROPIN      = 65,   // size 16         -- CarSelectionChangedDropInAction
    KI_ACTION_CAR_SELECTION_DROPIN_DONE = 66,   // size 16         -- CarSelectionDropInCompleteAction
    KI_ACTION_JUNKYARD_DRIVE_THRU       = 7,    // size 48 (0x30)  -- JunkYardDriveThruAction
    KI_ACTION_ALLOW_BOOST_EARNING       = 77,   // size 32 (0x20)  -- AllowBoostEarningAction
    KI_ACTION_RESET_PLAYER_DRIVER       = 70,   // size 1          -- SetPlayerCarDriverAction (exit)
    KI_ACTION_SET_INPUT_ENABLED         = 71,   // size 1          -- (enter junkyard input enable)
    KI_ACTION_JUNKYARD_ENTERED          = 99,   // size 1          -- (junkyard entered flag)
    KI_ACTION_AUTOSAVE                  = 55,   // size 1          -- RequestAutoSaveAction
    KI_ACTION_REQUEST_GAME_TRAINING     = 149,  // size 4          -- RequestGameTrainingAction
    KI_ACTION_STARTUP_DEFORM            = 106,  // size 1          -- (ReallyEnter start-of-game deform)
};

// X360 GameStateModule member byte the bodies poke directly through mpGameStateModule / the cached
// PreWorldInputBuffer ("a real call to that owner" -- de-inlined where a named accessor exists; the
// two raw output-buffer writes in UpdateExitState are flagged opaque pokes).
//   GameStateModule + 0x456D8 (284376/284380) : the active player car's CgsID (compared against the
//                                               desired/current car id to detect the swap completing).

// ============================================================================
// _AssertLayout (never called). PARITY-BY-NAMED-MEMBER: pins ABI-stable structural facts only.
// ============================================================================
void CarSelectManager::_AssertLayout()
{
    // The three car-id members are CgsID (8 bytes) and contiguous in the X360 (std @ +0x40/+0x48/+0x50).
    static_assert(offsetof(CarSelectManager, mDesiredCarId)
                      == offsetof(CarSelectManager, mStartCarId) + sizeof(CgsID),
                  "mDesiredCarId is contiguous with mStartCarId (CgsID == 8 bytes)");
    static_assert(offsetof(CarSelectManager, mCacheDuringChangeCarId)
                      == offsetof(CarSelectManager, mDesiredCarId) + sizeof(CgsID),
                  "mCacheDuringChangeCarId is contiguous with mDesiredCarId (CgsID == 8 bytes)");
    // The spawn-location array is exactly KU_CARSELECT_SPAWNLOCATION_COUNT handles wide.
    static_assert(sizeof(((CarSelectManager*)0)->maSpawnLocations)
                      == KU_CARSELECT_SPAWNLOCATION_COUNT * sizeof(Pointer32<const BrnTrigger::SpawnLocation>),
                  "maSpawnLocations holds KU_CARSELECT_SPAWNLOCATION_COUNT entries");
}

// ============================================================================
// Construct (X360 0x823564D0). [committed body -- preserved verbatim]
// Initialise the car-select manager to its idle defaults: store the three owning pointers, clear the
// state/timer, null the list + spawn-location pointers + car ids, and seed the unlock flags.
// mCurrentCarToUnlock / mfCarUnlockFadedOutTargetTime are deliberately left uninitialised (no store
// at those offsets in the X360 code).
// ============================================================================
void CarSelectManager::Construct(const TriggerQueryManager* lpTriggerQueryManager,
                                 GameStateModule* lpGameStateModule,
                                 BrnProgression::ProgressionManager* lpProgressionManager)
{
    CGS_ASSERT(lpTriggerQueryManager, "lpTriggerQueryManager");
    CGS_ASSERT(lpGameStateModule, "lpGameStateModule");

    mpGameStateModule.Set(lpGameStateModule);
    mpTriggerQueryManager.Set(lpTriggerQueryManager);
    mpProgressionManager.Set(lpProgressionManager);

    mfStateTimer = 0.0f;
    meState      = E_STATE_NONE;

    mpVehicleList.Set(nullptr);
    mpWheelList.Set(nullptr);

    mJunkyardId = 0;
    for (u32 lu = 0; lu < KU_CARSELECT_SPAWNLOCATION_COUNT; ++lu)
    {
        maSpawnLocations[lu].Set(nullptr);
    }
    meLastSpawnLocationType = KI_SPAWNLOCATIONTYPE_NONE;

    mStartCarId             = 0;
    mDesiredCarId           = 0;
    mCacheDuringChangeCarId = 0;

    mbWaitingForStreaming = false;
    muUnlockCount         = 0;

    mbNoNormalUnlockCars           = false;
    mbTransitionInRequestStreaming = false;
    mbNeedToTeleportTrick          = false;
    mbInCarModScreen               = false;
    mbShutdownUnlockSequence       = false;
    mbCarUnlockEnabled             = true;
    mbDEBUG_DisableUnlock                = false;
    mbDEBUG_UnlockTrophyCarsForTesting   = false;
    mbDEBUG_UnlockShutdownCarsForTesting = false;
}

// ============================================================================
// EnterModification (X360). [committed body -- preserved verbatim]
// ============================================================================
void CarSelectManager::EnterModification(::InputBuffer::GameActionQueue* lpActionQueue)
{
    CGS_ASSERT(meState == E_STATE_CAR_SELECT || meState == E_STATE_REQUEST_CAR_CHANGE, "CarSelectManager: Need to be in E_STATE_CAR_SELECT or E_STATE_REQUEST_CAR_CHANGE state.");

    StartCarModificationState(lpActionQueue);
}

// ============================================================================
// Update (X360 0x8239C218).
// Per-frame tick of the junkyard state machine. Asserts the owning module is set, then: if the game
// is paused (GameStateModule + 232288 nonzero) it just posts a PausedInCarSelect action; otherwise it
// advances mfStateTimer by the game timestep and dispatches on meState:
//   1 (TRANSITION_IN)   -> mid-transition: optionally kick streaming at t>=0.5 (the one-shot
//                          mbTransitionInRequestStreaming), else end the transition once t >= the
//                          min-time (6.0 when there are unlock cars, else 2.0) and streaming not pending.
//   2/3 (UNLOCK)        -> UpdateUnlockState.
//   6 (REQUEST_CHANGE)  -> UpdateRequestCarChangeState.
//   7/8 (CHANGING_CAR)  -> UpdateChangeCarState.
//   9 (EXITING)         -> UpdateExitState.
// ============================================================================
void CarSelectManager::Update(::InputBuffer::GameActionQueue* lpActionQueue,
                              const GameStateModuleIO::ControllerInput* lpControllerInput,
                              f32 lfGameTimestep)
{
    (void)lpControllerInput;   // passed through to the deeper state updaters; not read at this level

    GameStateModule* lpModule = mpGameStateModule.Get();
    if (lpModule == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGameStateModule", KAC_CSM_FILE, 67);
        CgsDev::Assert::EndAssert();
    }

    // X360: *(mpGameStateModule + 232288) -- "is the world currently paused / a pause is suppressing
    // the car-select tick". When set, just re-post the paused HUD action and bail.
    if (lpModule->IsTrainingPauseSuppressed())   // FLAG: GameStateModule + 232288 (training-pause/paused gate)
    {
        if (lpActionQueue == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpActionQueue", KAC_CSM_FILE, 70);
            CgsDev::Assert::EndAssert();
        }
        u8 lacPaused[1] = { 0 };
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacPaused), KI_ACTION_PAUSED_IN_CAR_SELECT, 1);
        return;
    }

    const State leState = meState;
    mfStateTimer += lfGameTimestep;
    const f32 lfTimer = mfStateTimer;

    switch (leState)
    {
        case E_STATE_TRANSITION_IN:   // 1
        {
            if (mbTransitionInRequestStreaming)
            {
                if (lfTimer >= KF_TRANSITION_IN_REQUEST_STREAMING)   // 0.5
                {
                    RequestStreamingForUnlock(lpActionQueue);
                    mbTransitionInRequestStreaming = false;
                }
            }
            else
            {
                const f32 lfMinTime = muUnlockCount ? KF_TRANSITION_IN_MIN_TIME_WITH_CARS  // 6.0 (has unlock cars)
                                                    : KF_TRANSITION_IN_MIN_TIME_NO_CARS;   // 2.0
                if (lfTimer >= lfMinTime && !mbWaitingForStreaming)
                    EndTransitionInState(lpActionQueue);
            }
            break;
        }
        case E_STATE_DISPLAY_UNLOCKED_CARS:           // 2
        case E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS:  // 3
            UpdateUnlockState(lpActionQueue);
            break;
        case E_STATE_REQUEST_CAR_CHANGE:              // 6
            UpdateRequestCarChangeState(lpActionQueue);
            break;
        case E_STATE_STARTING_CHANGING_CAR:           // 7
        case E_STATE_CHANGING_CAR:                    // 8
            UpdateChangeCarState(lpActionQueue);
            break;
        case E_STATE_EXITING:                         // 9
            UpdateExitState(lpActionQueue);
            break;
        default:
            break;
    }
}

// ============================================================================
// StartTransitionInState (X360 0x823929D0).
// Enter the junkyard transition-in: must be E_STATE_NONE. Resets the timer, sets state 1, builds the
// normal unlock list (and the shutdown list if shutdown-unlock is armed), seeds the unlock target
// (the first car if there are any to unlock, arming the one-shot streaming kick), posts the
// transition-in action (payload[0]=1 begin, payload[1]= "has cars to unlock"), then spawns the start car.
// ============================================================================
void CarSelectManager::StartTransitionInState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_NONE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State needs to be in E_STATE_NONE", KAC_CSM_FILE, 251);
        CgsDev::Assert::EndAssert();
    }

    mfStateTimer = 0.0f;
    meState      = E_STATE_TRANSITION_IN;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Transition In [Start]\n";

    SetupNormalUnlockList();
    if (mbNoNormalUnlockCars)   // X360 *(this+112) == byte 0x70 == mbNoNormalUnlockCars (SetupNormalUnlockList just set it)
        SetupShutdownUnlockList();

    mCurrentCarToUnlock = 0;
    if (muUnlockCount)
    {
        mbTransitionInRequestStreaming = true;   // X360 *(this+113) = 1
        mCurrentCarToUnlock = GetNextUnlockCarID(0);
    }

    u8 lacTransition[2];
    lacTransition[0] = 1;
    lacTransition[1] = (muUnlockCount != 0) ? 1 : 0;
    AsActionQueue(lpActionQueue)->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(lacTransition), KI_ACTION_TRANSITION_IN, 2);

    SpawnInStartCar(lpActionQueue);
}

// ============================================================================
// EndTransitionInState (X360 0x82392B30).
// Must be E_STATE_TRANSITION_IN. Posts the transition-in [End] action (payload[0]=0, payload[1]= "has
// unlocks"), then branches into the unlock sequence (if any cars remain to unlock) or straight to car
// select.
// ============================================================================
void CarSelectManager::EndTransitionInState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_TRANSITION_IN)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State needs to be in E_STATE_TRANSITION_IN.", KAC_CSM_FILE, 294);
        CgsDev::Assert::EndAssert();
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Transition In [End]\n";

    u8 lacTransition[2];
    lacTransition[0] = 0;
    lacTransition[1] = (muUnlockCount != 0) ? 1 : 0;
    AsActionQueue(lpActionQueue)->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(lacTransition), KI_ACTION_TRANSITION_IN, 2);

    if (muUnlockCount)
        StartUnlockState(lpActionQueue);
    else
        StartCarSelectState(lpActionQueue);
}

// ============================================================================
// StartCarSelectState (X360 0x823872D0).
// Valid from TRANSITION_IN / CAR_MODIFICATION / DISPLAY_UNLOCKED* / CAR_SELECT. If already in
// CAR_SELECT it is a no-op. Otherwise: when leaving CAR_MODIFICATION it requests streaming for the
// vehicle selection; then resets the timer, sets state 4, clears the in-car-mod flag, posts the
// "ready" (payload=1) + the car-mod-screen (payload[0]=1,[1]=0) actions, and -- when the desired car
// id's hi word is 0 -- copies mStartCarId into mDesiredCarId.
// ============================================================================
void CarSelectManager::StartCarSelectState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_TRANSITION_IN && meState != E_STATE_CAR_MODIFICATION &&
        meState != E_STATE_DISPLAY_UNLOCKED_CARS && meState != E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS &&
        meState != E_STATE_CAR_SELECT)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "CarSelectManager: State needs to be in E_STATE_TRANSITION_IN or E_STATE_CAR_MODIFICATION.",
            KAC_CSM_FILE, 331);
        CgsDev::Assert::EndAssert();
    }

    if (meState == E_STATE_CAR_SELECT)
        return;

    if (meState == E_STATE_CAR_MODIFICATION)
    {
        // X360: RequestStreamingForVehicleSelection(mpGameStateModule, mDesiredCarId, hi-word).
        mpGameStateModule.Get()->RequestStreamingForVehicleSelection(mDesiredCarId);
    }

    mfStateTimer = 0.0f;
    meState      = E_STATE_CAR_SELECT;
    mbInCarModScreen = false;   // X360 *(this+115) = 0

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Car Select\n";

    s32 liReady = 1;
    AsActionQueue(lpActionQueue)->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&liReady), KI_ACTION_CAR_SELECT_READY, 4);

    u8 lacModScreen[8];
    std::memset(lacModScreen, 0, sizeof(lacModScreen));
    lacModScreen[0] = 1;   // X360 v5 = 1 (begin), v6 = 0
    AsActionQueue(lpActionQueue)->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(lacModScreen), KI_ACTION_CAR_MOD_SCREEN, 8);

    // X360: if (mDesiredCarId hi-word == 0) mDesiredCarId = mStartCarId.
    if (mDesiredCarId == 0)
        mDesiredCarId = mStartCarId;
}

// ============================================================================
// StartUnlockState (X360 0x82387730).
// Valid from TRANSITION_IN / DISPLAY_UNLOCKED_CARS. If the X360 +0x76 "disable unlock" flag is set it
// force-clears the unlock count. If no cars remain -> StartCarSelectState.
// Otherwise it picks state 3 (shutdown rivals) or 2 (normal) per mbShutdownUnlockSequence, resets the
// unlock target / ticker, and arms the unlock hold timer (mfStateTimer = 7.0, faded-out target 0).
// ============================================================================
void CarSelectManager::StartUnlockState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_TRANSITION_IN && meState != E_STATE_DISPLAY_UNLOCKED_CARS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "CarSelectManager: State needs to be in E_STATE_TRANSITION_IN or E_STATE_DISPLAY_UNLOCKED_CARS.",
            KAC_CSM_FILE, 449);
        CgsDev::Assert::EndAssert();
    }

    if (mbDEBUG_DisableUnlock)   // X360 *(this+118): when set, force the unlock count to 0 (FLAG: flag<->slot)
        muUnlockCount = 0;

    if (muUnlockCount == 0)
    {
        StartCarSelectState(lpActionQueue);
        return;
    }

    if (mbNoNormalUnlockCars)   // X360 *(this+112) == byte 0x70 == mbNoNormalUnlockCars (the state 3-vs-2 select)
    {
        meState = E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS;   // 3
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Starting unlock for Shutdown rivals.\n";
    }
    else
    {
        meState = E_STATE_DISPLAY_UNLOCKED_CARS;            // 2
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Starting unlock for Normal.\n";
    }

    mCurrentCarToUnlock           = 0;       // X360 *(this+12) (8B) = 0
    mbCurrentCarTickerVisible     = false;   // X360 *(this+104) = 0
    mfStateTimer                  = KF_CARSELECT_UNLOCK_MIN_TIME;  // 7.0
    mfCarUnlockFadedOutTargetTime = KF_TIMER_RESET;               // 0.0
}

// ============================================================================
// UpdateUnlockState (X360 0x82398920).
// Must be in an unlock state (2 or 3). Once the unlock hold has elapsed (timer >= 7.0, not waiting on
// streaming, ticker not visible, timer past the faded-out target): if cars remain it picks the next
// unlock car, resolves its CarData, posts the new-car-unlocked action (payload[0..7]=car id, [16]=
// unlock variant 0/1/2/3), marks it already-shown on the module, fires OnSpecialEventPlayerCarChange,
// posts the reset-player-car (80B) + the unlock colour/palette (8B) + the suffix-id (8B) actions,
// flips the ticker visible, requests streaming, resets the timer, and decrements the remaining count.
// When no cars remain -> EndUnlockState.
// ============================================================================
void CarSelectManager::UpdateUnlockState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_DISPLAY_UNLOCKED_CARS && meState != E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State needs to be in Unlock sequence.", KAC_CSM_FILE, 758);
        CgsDev::Assert::EndAssert();
    }

    const f32 lfTimer = mfStateTimer;
    if (!(lfTimer >= KF_CARSELECT_UNLOCK_MIN_TIME && !mbWaitingForStreaming &&
          !mbCurrentCarTickerVisible && lfTimer > mfCarUnlockFadedOutTargetTime))
        return;

    if (muUnlockCount == 0)
    {
        EndUnlockState(lpActionQueue);
        return;
    }

    mCurrentCarToUnlock = GetNextUnlockCarID(mCurrentCarToUnlock);

    CgsID lUnlockCarId = mCurrentCarToUnlock;
    const BrnProgression::CarData* lpUnlockCarData = GetProfileCarData(lUnlockCarId);
    if (lpUnlockCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpUnlockCarData", KAC_CSM_FILE, 777);
        CgsDev::Assert::EndAssert();
    }

    // NewCarUnlockedAction (16B). X360 0x82398920 builds the payload as: carId(8B) @+0x00,
    // the unlock-variant s32 @+0x08 (switch on CarData word +0x10: 1->1, 2->2, 3->3, else 0), and
    // 7.0f @+0x0C. The variant selects the unlock fanfare/category on the HUD.
    {
        u8 lacUnlocked[16];
        std::memset(lacUnlocked, 0, sizeof(lacUnlocked));
        const CgsID lCarId = lpUnlockCarData->GetId();
        std::memcpy(lacUnlocked, &lCarId, sizeof(lCarId));                         // carId @+0x00
        const s32 liUnlockType = lpUnlockCarData->GetUnlockType();                 // CarData +0x10
        s32 liVariant;
        switch (liUnlockType)
        {
            case 1:  liVariant = 1; break;
            case 2:  liVariant = 2; break;
            case 3:  liVariant = 3; break;
            default: liVariant = 0; break;
        }
        std::memcpy(lacUnlocked + 0x08, &liVariant, sizeof(liVariant));            // unlock-variant @+0x08
        const f32 lfFanfare = KF_CARSELECT_UNLOCK_MIN_TIME;                        // 7.0 (X360 v19)
        std::memcpy(lacUnlocked + 0x0C, &lfFanfare, sizeof(lfFanfare));            // 7.0f @+0x0C
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacUnlocked), KI_ACTION_NEW_CAR_UNLOCKED, 16);
    }

    if (mpGameStateModule.Get() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGameStateModule", KAC_CSM_FILE, 816);
        CgsDev::Assert::EndAssert();
    }
    mpGameStateModule.Get()->SetCarUnlockAlreadyShown(lpUnlockCarData->GetId());
    mpGameStateModule.Get()->OnSpecialEventPlayerCarChange(mCurrentCarToUnlock, 0, lpActionQueue, true);

    if (maSpawnLocations[3].Get() == 0)   // X360 *(v2+13) byte 0x34 == maSpawnLocations[3]
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", KAC_CSM_FILE, 824);
        CgsDev::Assert::EndAssert();
    }

    // ResetPlayerCarAction (80B). Only the few fields the X360 seeds are reproduced: the unlocked car
    // id, the car's deform amount (CarData +12), and the in-car-select / car-select-type / dont-stream
    // flags. FLAG: full ResetPlayerCarAction layout not in exports.
    {
        u8 lacReset[80];
        std::memset(lacReset, 0, sizeof(lacReset));
        const CgsID lCarId = lpUnlockCarData->GetId();
        std::memcpy(lacReset, &lCarId, sizeof(lCarId));                       // car id
        const f32 lfDeform = lpUnlockCarData->GetUnlockDeformationAmount();
        std::memcpy(lacReset + 0x14, &lfDeform, sizeof(lfDeform));            // deform amount slot
        lacReset[0x28] = 8;   // car-select-type
        lacReset[0x30] = 1;   // in-car-select
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacReset), KI_ACTION_RESET_PLAYER_CAR, 80);
    }

    // CarUnlockAction (8B): the unlocked car's colour + palette indices.
    {
        s32 laiColourPalette[2] = { 0, 0 };
        mpProgressionManager.Get()->GetCarColourAndPalette(mCurrentCarToUnlock, &laiColourPalette[0], &laiColourPalette[1]);
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(laiColourPalette), KI_ACTION_CAR_UNLOCK, 8);
    }

    // CarSelectionChanged suffix (8B): the new unlock-target id.
    {
        CgsID lCurrent = mCurrentCarToUnlock;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lCurrent), KI_ACTION_CAR_CHANGED_SUFFIX, 8);
    }

    mbCurrentCarTickerVisible     = true;            // X360 *(this+104) = 1
    mfCarUnlockFadedOutTargetTime = KF_TIMER_RESET;  // 0.0
    RequestStreamingForUnlock(lpActionQueue);
    mfStateTimer = KF_TIMER_RESET;                   // 0.0
    --muUnlockCount;
}

// ============================================================================
// EndUnlockState (X360 0x82392C58).
// Must be an unlock state. If shutdown rivals remain after rebuilding the shutdown list, re-enter the
// unlock sequence. Otherwise it finalises: asserts a spawn location, marks the ticker visible, resolves
// the desired-car CarData, posts the reset-player-car (80B) + the two spawn-location car-selection
// actions (64B + 16B, each carrying the spawn-location type bit), fires OnSpecialEventPlayerCarChange,
// caches the current car into mCacheDuringChangeCarId, clears the unlock count, posts the unlock-end
// action, then drops to car select.
// ============================================================================
void CarSelectManager::EndUnlockState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_DISPLAY_UNLOCKED_CARS && meState != E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State needs to be in Unlock sequence.", KAC_CSM_FILE, 880);
        CgsDev::Assert::EndAssert();
    }

    if (meState == E_STATE_DISPLAY_UNLOCKED_CARS)
    {
        SetupShutdownUnlockList();
        if (muUnlockCount)
        {
            StartUnlockState(lpActionQueue);
            return;
        }
    }

    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[1].Get();   // X360 *(this+44)
    if (lpSpawnLocation == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", KAC_CSM_FILE, 898);
        CgsDev::Assert::EndAssert();
    }

    mbCurrentCarTickerVisible = true;   // X360 *(this+60) = 1

    // X360: GetProfileCarData(this, &mCurrentCarToUnlock) -- the just-unlocked car being settled.
    CgsID lUnlockCarId = mCurrentCarToUnlock;
    const BrnProgression::CarData* lpCarData = GetProfileCarData(lUnlockCarId);

    // ResetPlayerCarAction (80B): the spawn-location transform (the X360 vector copies) + the unlocked
    // car id + its deform amount + flags. FLAG: full layout not in exports; transform modelled opaque.
    {
        u8 lacReset[80];
        std::memset(lacReset, 0, sizeof(lacReset));
        std::memcpy(lacReset, &mCurrentCarToUnlock, sizeof(mCurrentCarToUnlock));
        if (lpCarData != 0)
        {
            const f32 lfDeform = lpCarData->GetUnlockDeformationAmount();
            std::memcpy(lacReset + 0x14, &lfDeform, sizeof(lfDeform));
        }
        lacReset[0x28] = 8;   // car-select-type
        lacReset[0x30] = 1;   // in-car-select
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacReset), KI_ACTION_RESET_PLAYER_CAR, 80);
    }

    const u32 luSpawnType = static_cast<u32>(lpSpawnLocation->GetType());
    if (luSpawnType >= static_cast<u32>(BrnTrigger::SpawnLocation::E_TYPE_COUNT))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", KAC_SPAWNLOC_FILE, 152);
        CgsDev::Assert::EndAssert();
    }

    // CarSelectionChangedAction (64B): the junkyard id + the spawn-location "is type 1" bit.
    {
        u8 lacChanged[64];
        std::memset(lacChanged, 0, sizeof(lacChanged));
        std::memcpy(lacChanged, &mJunkyardId, sizeof(mJunkyardId));
        lacChanged[0x10] = (luSpawnType == 1) ? 1 : 0;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacChanged), KI_ACTION_CAR_SELECTION_CHANGED, 64);
    }

    if (static_cast<u32>(lpSpawnLocation->GetType()) >= static_cast<u32>(BrnTrigger::SpawnLocation::E_TYPE_COUNT))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", KAC_SPAWNLOC_FILE, 152);
        CgsDev::Assert::EndAssert();
    }

    // CarSelectionChangedDropInAction (16B): the unlocked car id + the "is type 1" bit.
    {
        u8 lacDropIn[16];
        std::memset(lacDropIn, 0, sizeof(lacDropIn));
        std::memcpy(lacDropIn, &mCurrentCarToUnlock, sizeof(mCurrentCarToUnlock));
        lacDropIn[8] = (static_cast<u32>(lpSpawnLocation->GetType()) == 1) ? 1 : 0;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacDropIn), KI_ACTION_CAR_SELECTION_DROPIN, 16);
    }

    mpGameStateModule.Get()->OnSpecialEventPlayerCarChange(mCurrentCarToUnlock, 0, lpActionQueue, true);

    // X360 tail: mDesiredCarId(+0x48) = mCurrentCarToUnlock(+0x60); muUnlockCount(+0x5C) = 0. (The
    // X360 also scribbles a {0, spawnLoc} scratch value back into mCurrentCarToUnlock; inert.)
    mDesiredCarId = mCurrentCarToUnlock;
    muUnlockCount = 0;

    u8 lacUnlockEnd[1] = { 0 };
    AsActionQueue(lpActionQueue)->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(lacUnlockEnd), KI_ACTION_CAR_UNLOCK_END, 1);

    StartCarSelectState(lpActionQueue);
}

// ============================================================================
// UpdateRequestCarChangeState (X360 0x82387AB8).
// Must be E_STATE_REQUEST_CAR_CHANGE. If the active player car already matches the desired car id the
// swap is done (timer=1.0, state 7). Otherwise (asserts streaming is not pending) it waits out the
// change-car delay (1.0, or 0.85 in livery-select), then asserts the spawn location, posts the
// car-selection (64B, spawn-type bits), optionally re-requests vehicle-selection streaming, and arms
// state 7 (timer 0).
// ============================================================================
void CarSelectManager::UpdateRequestCarChangeState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_REQUEST_CAR_CHANGE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State needs to be in E_STATE_REQUEST_CAR_CHANGE.", KAC_CSM_FILE, 611);
        CgsDev::Assert::EndAssert();
    }

    // X360: if (active-player-car CgsID == mDesiredCarId) -> swap already complete.
    const CgsID lActiveCarId = mpGameStateModule.Get()->GetActivePlayerCarId();   // GameStateModule + 0x456D8
    if (lActiveCarId == mDesiredCarId)
    {
        mfStateTimer = KF_CARSELECT_DELAY_BEFORE_CHANGE_CAR;   // 1.0
        meState      = E_STATE_STARTING_CHANGING_CAR;          // 7
        return;
    }

    if (mbWaitingForStreaming)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("!mbWaitingForStreaming", KAC_CSM_FILE, 622);
        CgsDev::Assert::EndAssert();
    }

    // The wait gate: in livery-select (mbInCarModScreen) only the 0.85 floor applies; otherwise the
    // 1.5 / 0.85 two-stage gate from the X360 (KF_CARSELECT_CHANGE_CAR_TELEPORT_DELAY == 1.5).
    if (!mbInCarModScreen)
    {
        if (mfStateTimer < KF_CARSELECT_CHANGE_CAR_TELEPORT_DELAY)   // < 1.5
            return;
    }
    if (mfStateTimer < KF_LIVERYSELECT_DELAY_BEFORE_CHANGE)          // < 0.85
        return;

    if (!mbInCarModScreen)
        meLastSpawnLocationType = (meLastSpawnLocationType == 1) ? 2 : 1;   // X360 (==1)+1

    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[meLastSpawnLocationType].Get();
    if (lpSpawnLocation == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", KAC_CSM_FILE, 643);
        CgsDev::Assert::EndAssert();
    }

    // CarSelectionChangedAction (64B): the junkyard id + the spawn-location "type is type 0" bit.
    {
        u8 lacChanged[64];
        std::memset(lacChanged, 0, sizeof(lacChanged));
        std::memcpy(lacChanged, &mJunkyardId, sizeof(mJunkyardId));
        const u32 luType = static_cast<u32>(lpSpawnLocation->GetType());
        lacChanged[0x10] = (luType == 0) ? 1 : 0;
        lacChanged[0x11] = 1;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacChanged), KI_ACTION_CAR_SELECTION_CHANGED, 64);
    }

    if (!mbInCarModScreen)
        mpGameStateModule.Get()->RequestStreamingForVehicleSelection(mDesiredCarId);

    mfStateTimer = KF_TIMER_RESET;                  // 0.0
    meState      = E_STATE_STARTING_CHANGING_CAR;   // 7
}

// ============================================================================
// UpdateChangeCarState (X360 0x823986D0).
// Drives the actual car swap (states 7 -> 8). In state 7, once the 1.0 hold elapses: if the active
// player car already matches mDesiredCarId, post the drop-in (16B) action and advance to state 8;
// else fire OnSpecialEventPlayerCarChange, teleport the current vehicle, set waiting-for-streaming
// and advance to state 8. In state 8 (when streaming has finished), it returns either to the car-mod
// screen (state 5) or car select (state 4): if a queued car-change is pending it re-requests it, else
// posts the drop-in-complete (16B) action.
// ============================================================================
void CarSelectManager::UpdateChangeCarState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_STARTING_CHANGING_CAR && meState != E_STATE_CHANGING_CAR)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State needs to be in E_STATE_CHANGING_CAR.", KAC_CSM_FILE, 679);
        CgsDev::Assert::EndAssert();
    }

    if (meState == E_STATE_STARTING_CHANGING_CAR)   // 7
    {
        if (mfStateTimer >= KF_CARSELECT_DELAY_BEFORE_CHANGE_CAR)   // >= 1.0
        {
            const CgsID lActiveCarId = mpGameStateModule.Get()->GetActivePlayerCarId();
            if (lActiveCarId == mDesiredCarId)
            {
                // CarSelectionChangedDropInAction (16B): the desired car id + "type 1" bit.
                u8 lacDropIn[16];
                std::memset(lacDropIn, 0, sizeof(lacDropIn));
                std::memcpy(lacDropIn, &mDesiredCarId, sizeof(mDesiredCarId));
                lacDropIn[8] = (meLastSpawnLocationType == 1) ? 1 : 0;
                AsActionQueue(lpActionQueue)->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lacDropIn), KI_ACTION_CAR_SELECTION_DROPIN, 16);
                meState = E_STATE_CHANGING_CAR;   // 8
            }
            else
            {
                if (mpGameStateModule.Get() == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("mpGameStateModule", KAC_CSM_FILE, 689);
                    CgsDev::Assert::EndAssert();
                }
                mpGameStateModule.Get()->OnSpecialEventPlayerCarChange(mDesiredCarId, 0, lpActionQueue, true);
                TeleportCurrentVehicle(lpActionQueue);
                mbWaitingForStreaming = true;     // X360 *(this+88) = 1
                meState               = E_STATE_CHANGING_CAR;   // 8
            }
        }
        return;
    }

    // state 8: wait for streaming to finish, then return to the prior screen.
    if (mbWaitingForStreaming)
        return;

    mfStateTimer = KF_TIMER_RESET;   // 0.0
    if (mbInCarModScreen)
    {
        meState = E_STATE_CAR_MODIFICATION;   // 5
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Changing Car, then back to CarMod state.\n";
    }
    else
    {
        meState = E_STATE_CAR_SELECT;         // 4
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Changing Car, then back to CarSelect state.\n";
    }

    if (mCacheDuringChangeCarId)   // X360 v7[21] (hi-word of mCacheDuringChangeCarId)
    {
        RequestChangeCar(mCacheDuringChangeCarId);
        mCacheDuringChangeCarId = 0;   // X360 *(this+10) = 0
    }
    else
    {
        // CarSelectionDropInCompleteAction (16B): mDesiredCarId + the spawn-type bit.
        u8 lacDropInDone[16];
        std::memset(lacDropInDone, 0, sizeof(lacDropInDone));
        std::memcpy(lacDropInDone, &mDesiredCarId, sizeof(mDesiredCarId));
        lacDropInDone[8] = (meLastSpawnLocationType == 1) ? 1 : 0;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacDropInDone), KI_ACTION_CAR_SELECTION_DROPIN_DONE, 16);
    }
}

// ============================================================================
// UpdateExitState (X360 0x82398C20).
// Must be E_STATE_EXITING. Once streaming is no longer pending it finalises the junkyard exit: resets
// to E_STATE_NONE / clears the in-car-mod flag / zeroes the timer, posts the close-car-mod-screen
// (8B), the reset-driver (1B), the allow-boost (32B), the junkyard-input (1B) actions, resolves the
// desired-car CarData, posts the reset-player-car (80B) + the car-select-exit (48B) actions, requests
// a training prompt (4B), pokes the two GameStateModule output-buffer flags, unpauses the world, fires
// OnPlayerCarChange when offline, and posts the auto-save (1B) action.
// ============================================================================
void CarSelectManager::UpdateExitState(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_EXITING)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State needs to be in E_STATE_EXITING.", KAC_CSM_FILE, 958);
        CgsDev::Assert::EndAssert();
    }

    if (mbWaitingForStreaming)
        return;

    meState          = E_STATE_NONE;   // 0
    mbInCarModScreen = false;          // X360 *(this+115) = 0
    mfStateTimer     = KF_TIMER_RESET; // 0.0

    GameActionQueueImpl* lpQueue = AsActionQueue(lpActionQueue);

    // close the car-mod screen (8B: [0]=1 begin -> end-screen).
    {
        u8 lacModScreen[8];
        std::memset(lacModScreen, 0, sizeof(lacModScreen));
        lacModScreen[0] = 1;
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacModScreen), KI_ACTION_CAR_MOD_SCREEN, 8);
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Exit state is finished.\n";

    mJunkyardId = 0;   // X360 *(this+0x20) = 0

    {
        u8 lacResetDriver[1] = { 1 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacResetDriver), KI_ACTION_RESET_PLAYER_DRIVER, 1);
    }

    if (maSpawnLocations[4].Get() == 0)   // X360 v2[14] byte 0x38 == maSpawnLocations[4]
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", KAC_CSM_FILE, 988);
        CgsDev::Assert::EndAssert();
    }

    // AllowBoostEarningAction (32B): seeded from the spawn-location transform (opaque). FLAG.
    {
        u8 lacAllowBoost[32];
        std::memset(lacAllowBoost, 0, sizeof(lacAllowBoost));
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacAllowBoost), KI_ACTION_ALLOW_BOOST_EARNING, 32);
    }

    {
        u8 lacJunkInput[1] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacJunkInput), KI_ACTION_JUNKYARD_ENTERED, 1);
    }

    CgsID lDesiredCarId = mDesiredCarId;
    const BrnProgression::CarData* lpCarData = GetProfileCarData(lDesiredCarId);
    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData", KAC_CSM_FILE, 1004);
        CgsDev::Assert::EndAssert();
    }

    // ResetPlayerCarAction (80B): desired car id + deform + flags (the X360 vector copies are opaque).
    {
        u8 lacReset[80];
        std::memset(lacReset, 0, sizeof(lacReset));
        std::memcpy(lacReset, &mDesiredCarId, sizeof(mDesiredCarId));
        const f32 lfDeform = lpCarData->GetUnlockDeformationAmount();
        std::memcpy(lacReset + 0x14, &lfDeform, sizeof(lfDeform));
        lacReset[0x28] = 8;   // car-select-type
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacReset), KI_ACTION_RESET_PLAYER_CAR, 80);
    }

    // CarSelectExitAction (48B): payload[0]=1 (X360 v21[0]=1). FLAG: full layout not in exports.
    {
        u8 lacExit[48];
        std::memset(lacExit, 0, sizeof(lacExit));
        lacExit[0] = 1;
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacExit), KI_ACTION_JUNKYARD_DRIVE_THRU, 48);
    }

    // RequestGameTrainingAction (4B: 0).
    {
        s32 liTraining = 0;
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liTraining), KI_ACTION_REQUEST_GAME_TRAINING, 4);
    }

    // X360 raw poke: *(mpProgressionManager + 133512) = 1 (a drive-thrus-dirty / rivals-dirty flag).
    mpProgressionManager.Get()->SetDriveThrusDirtyFlag();   // FLAG: ProgressionManager + 133512 (de-inlined byte poke)

    mpGameStateModule.Get()->RequestUnpause(true, lpActionQueue);

    if (!mpGameStateModule.Get()->IsOnlineGameMode())
        mpGameStateModule.Get()->OnPlayerCarChange(mDesiredCarId, 0, lpActionQueue, true);

    if (mpProgressionManager.Get() == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", KAC_CSM_FILE, 1043);
        CgsDev::Assert::EndAssert();
    }

    // X360 raw poke: *(mpProgressionManager + 133489) = 1 (rivals-update request flag).
    mpProgressionManager.Get()->RequestUpdateRivals();   // FLAG: ProgressionManager + 133489 (de-inlined byte poke)

    // RequestAutoSaveAction (1B: 0).
    {
        u8 lacAutosave[1] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacAutosave), KI_ACTION_AUTOSAVE, 1);
    }

    mDesiredCarId = 0;   // X360 *v10 = 0
}

// ============================================================================
// EnterJunkyard (X360 0x82398508).
// DriveThruManager::ProcessDriveThru case 0 hands the cached junkyard id here. Asserts E_STATE_NONE,
// optionally force-unlocks cars for testing (the two DEBUG flags), caches the junkyard id, builds the
// spawn locations, posts the junkyard-entered (1B) / drive-thru (48B) / reset-driver (1B) / input
// (1B) actions, snapshots the active player car id into mStartCarId, pins its chosen base-car livery
// on the profile, then starts the transition-in.
// ============================================================================
void CarSelectManager::EnterJunkyard(::InputBuffer::GameActionQueue* lpActionQueue, CgsID lJunkyardId)
{
    if (meState != E_STATE_NONE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State is invalid", KAC_CSM_FILE, 196);
        CgsDev::Assert::EndAssert();
    }

    if (mbDEBUG_UnlockTrophyCarsForTesting || mbDEBUG_UnlockShutdownCarsForTesting)   // X360 *(this+119)||*(this+120)
        DEBUG_UnlockCarsForTesting();

    mJunkyardId = lJunkyardId;   // X360 *(this+0x20) = lJunkyardId

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: EnterJunkyard: ";
        // X360 then streams the junkyard id then "\n" (pure logging; id stream omitted -- the
        // StrStream id-format helper sub_82203EE8 is out of scope for parity).
        *CgsDev::Log::gpDebugPrint << "\n";
    }

    SetupSpawnLocations();

    GameActionQueueImpl* lpQueue = AsActionQueue(lpActionQueue);

    {
        u8 lacEntered[1] = { 1 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacEntered), KI_ACTION_JUNKYARD_ENTERED, 1);
    }
    {
        u8 lacDriveThru[48];
        std::memset(lacDriveThru, 0, sizeof(lacDriveThru));   // X360 v21[0]=0, v22=0
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacDriveThru), KI_ACTION_JUNKYARD_DRIVE_THRU, 48);
    }
    {
        u8 lacResetDriver[1] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacResetDriver), KI_ACTION_RESET_PLAYER_DRIVER, 1);
    }
    {
        u8 lacInput[1] = { 0 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacInput), KI_ACTION_SET_INPUT_ENABLED, 1);
    }

    // Snapshot the active player car id into mStartCarId, then pin its chosen-livery base car on the
    // profile (X360 reads the active car CgsID at GameStateModule + 0x456D8).
    const CgsID lActiveCarId = mpGameStateModule.Get()->GetActivePlayerCarId();
    mStartCarId = lActiveCarId;
    mpProgressionManager.Get()->GetProfile()->SetChosenLiveryIdForBaseCar(lActiveCarId);

    StartTransitionInState(lpActionQueue);
}

// ============================================================================
// EnterJunkyardAtStartOfGame (X360 0x82393080).
// The boot-time path (OnProfileLoaded / SendSetupPlayerCarEvent): cache the junkyard id, build the
// spawn locations, assert the spawn location + its type, post the reset-player-car (80B: car model +
// wheel ids, -1 deform sentinel, flags) action, copy the spawn-location transform into the caller's
// CarSelectionChangedAction, set the drop-in "type 1" flag, and fire OnSpecialEventPlayerCarChange.
// The desired car id is then cached into mStartCarId.
// ============================================================================
void CarSelectManager::EnterJunkyardAtStartOfGame(::InputBuffer::GameActionQueue* lpActionQueue,
                                                  CgsID lJunkyardId, CgsID lCarModelId, CgsID lWheelId,
                                                  GameStateModuleIO::EPlayerScoringIndex leScoringIndex,
                                                  GameStateModuleIO::CarSelectionChangedAction* lpCarSelectChangedAction)
{
    mJunkyardId = lJunkyardId;   // X360 *(this+0x20) = a2 (junkyard id)

    SetupSpawnLocations();

    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[1].Get();   // X360 *(this+44)
    if (lpSpawnLocation == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", KAC_CSM_FILE, 1589);
        CgsDev::Assert::EndAssert();
    }
    if (static_cast<u32>(lpSpawnLocation->GetType()) >= static_cast<u32>(BrnTrigger::SpawnLocation::E_TYPE_COUNT))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", KAC_SPAWNLOC_FILE, 152);
        CgsDev::Assert::EndAssert();
    }

    meLastSpawnLocationType = static_cast<s32>(lpSpawnLocation->GetType());   // X360 *(this+60)

    // ResetPlayerCarAction (80B): car model id + wheel id + the -1.0 deform sentinel + the spawn-location
    // transform (opaque) + flags. leScoringIndex is recorded via SetPlayerScoringIndex (opaque slot).
    {
        u8 lacReset[80];
        std::memset(lacReset, 0, sizeof(lacReset));
        std::memcpy(lacReset,        &lCarModelId, sizeof(lCarModelId));
        std::memcpy(lacReset + 0x08, &lWheelId,    sizeof(lWheelId));
        const f32 lfDeform = KF_STARTOFGAME_DEFORM_SENTINEL;   // -1.0
        std::memcpy(lacReset + 0x34, &lfDeform, sizeof(lfDeform));
        const s32 liScoring = static_cast<s32>(leScoringIndex);
        std::memcpy(lacReset + 0x10, &liScoring, sizeof(liScoring));   // FLAG: scoring-index slot inferred
        lacReset[0x40] = 1;   // in-car-select (X360 v27 = 1)
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacReset), KI_ACTION_RESET_PLAYER_CAR, 80);
    }

    // Copy the spawn-location transform + junkyard id into the caller's CarSelectionChangedAction and
    // set its "spawn type == 1" drop-in flag. FLAG: CarSelectionChangedAction layout opaque -- the
    // X360 writes the junkyard id at +0x00, the transform at +0x10/+0x20, the type-1 bit at +0x30.
    if (lpCarSelectChangedAction != 0)
    {
        u8* lpcAction = reinterpret_cast<u8*>(lpCarSelectChangedAction);
        std::memcpy(lpcAction, &mJunkyardId, sizeof(mJunkyardId));
        lpcAction[0x30] = (static_cast<u32>(lpSpawnLocation->GetType()) == 1) ? 1 : 0;
        lpcAction[0x31] = 0;
    }

    mpGameStateModule.Get()->OnSpecialEventPlayerCarChange(lCarModelId, lWheelId, lpActionQueue, true);

    mStartCarId = lCarModelId;   // X360 *(this+64) = v7 (car model id)
}

// ============================================================================
// ReallyEnterJunkyardAtStartOfGame (X360 0x823931F8).
// Completes the boot junkyard entry once setup is acked: posts the junkyard drive-thru (48B) action,
// posts the desired car's colour/palette (8B), and -- if the profile flags a startup deform -- finds
// the desired car's CarData and stamps the 0.85 startup deform, posting the startup-deform (1B) flag.
// Posts the junkyard-entered (1B) action, then starts the transition-in.
// ============================================================================
void CarSelectManager::ReallyEnterJunkyardAtStartOfGame(::InputBuffer::GameActionQueue* lpActionQueue)
{
    GameActionQueueImpl* lpQueue = AsActionQueue(lpActionQueue);

    {
        u8 lacDriveThru[48];
        std::memset(lacDriveThru, 0, sizeof(lacDriveThru));
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacDriveThru), KI_ACTION_JUNKYARD_DRIVE_THRU, 48);
    }

    // CarUnlockAction (8B): the desired car's colour + palette.
    {
        s32 laiColourPalette[2] = { 0, 0 };
        mpProgressionManager.Get()->GetCarColourAndPalette(mStartCarId, &laiColourPalette[0], &laiColourPalette[1]);
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(laiColourPalette), KI_ACTION_CAR_UNLOCK, 8);
    }

    u8 lacStartupDeform[1] = { 0 };
    BrnProgression::Profile* lpProfile = mpProgressionManager.Get()->GetProfile();
    if (lpProfile->IsStartOfGameDeformActive())   // FLAG: Profile + 118401 (de-inlined byte read)
    {
        BrnProgression::CarData* lpCarData = lpProfile->FindCar(mStartCarId);
        if (lpCarData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpCarData", KAC_CSM_FILE, 1646);
            CgsDev::Assert::EndAssert();
        }
        lacStartupDeform[0] = 1;
        lpCarData->SetUnlockDeformationAmount(KF_LIVERYSELECT_DELAY_BEFORE_CHANGE);   // 0.85 (X360 *(car+12) = 0.85)
    }
    lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacStartupDeform), KI_ACTION_STARTUP_DEFORM, 1);

    {
        u8 lacEntered[1] = { 1 };
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacEntered), KI_ACTION_JUNKYARD_ENTERED, 1);
    }

    StartTransitionInState(lpActionQueue);
}

// ============================================================================
// ExitJunkyard (X360 0x82387880).
// Must be E_STATE_CAR_MODIFICATION. Resets the timer, sets state 9 (EXITING), posts the car-select-exit
// (1B) action, sets waiting-for-streaming + the exit-pending latch (this+117), and logs the exit start.
// ============================================================================
void CarSelectManager::ExitJunkyard(::InputBuffer::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_CAR_MODIFICATION)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("CarSelectManager: State needs to be in E_STATE_CAR_MODIFICATION.", KAC_CSM_FILE, 503);
        CgsDev::Assert::EndAssert();
    }

    mfStateTimer = KF_TIMER_RESET;   // 0.0
    meState      = E_STATE_EXITING;  // 9

    {
        u8 lacExit[1] = { 1 };
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacExit), KI_ACTION_CAR_SELECT_EXIT, 1);
    }

    mbWaitingForStreaming = true;   // X360 *(this+88) = 1
    mbCarUnlockEnabled    = true;   // X360 *(this+117) = 1 == byte 0x75 == mbCarUnlockEnabled (re-enable car unlock on exit)

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Start Exit state.\n";
}

// ============================================================================
// ForceExitJunkyard (X360 0x8239C418).
// Abort path (OnEnterOnline / ProcessGameEvents). Forces the manager straight into the EXITING state:
// resets the timer, sets state 9, clears waiting-for-streaming, copies mStartCarId into mDesiredCarId,
// runs UpdateExitState to completion, posts the abort (1B: lbToOnlineEvent) action, and sets the
// exit-pending latch (this+117).
// ============================================================================
void CarSelectManager::ForceExitJunkyard(::InputBuffer::GameActionQueue* lpActionQueue, bool lbToOnlineEvent)
{
    // X360 asm: mfStateTimer = 0.0; meState = 9 (li r9,9); mbWaitingForStreaming = 0;
    // mDesiredCarId(+0x48) = mStartCarId(+0x40) (the abort carries the entry car id back through exit).
    mfStateTimer          = KF_TIMER_RESET;        // 0.0
    meState               = E_STATE_EXITING;       // 9
    mbWaitingForStreaming = false;                 // X360 *(this+88) = 0
    mDesiredCarId         = mStartCarId;           // X360 *(this+0x48) = *(this+0x40)

    UpdateExitState(lpActionQueue);

    {
        u8 lacAbort[1];
        lacAbort[0] = lbToOnlineEvent ? 1 : 0;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacAbort), KI_ACTION_FORCE_EXIT, 1);
    }

    mbCarUnlockEnabled = true;   // X360 *(this+117) = 1 == byte 0x75 == mbCarUnlockEnabled (re-enable car unlock on exit)
}
}
