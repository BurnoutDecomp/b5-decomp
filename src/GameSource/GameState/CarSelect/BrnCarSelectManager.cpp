#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\CarSelect/BrnCarSelectManager.h"
#include <cstddef>   // offsetof (layout asserts)
#include <cstring>   // memset/memcpy (opaque action payloads)
#include <cstdlib>   // std::getenv ([car-audio] BRN_DEBUG_UNLOCK_CARS harness arm)

#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CgsDev::Assert Begin/Fire/EndAssert
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::VariableEventQueue<13312,16> (game-action queue)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"         // CgsDev::Log::gpDebugPrint / Message::gxMessageFilterFlags

#include "SharedClasses/Trigger/BrnSpawnLocation.h"                 // BrnTrigger::SpawnLocation (GetType / Type / E_TYPE_COUNT)
#include "SharedClasses/DataLists/VehicleList.h"                    // BrnResource::VehicleList (GetVehicleIndex / GetVehicleData)
#include "SharedClasses/DataLists/VehicleListEntry.h"               // BrnResource::VehicleListEntry (livery type / parent id)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h" // BrnProgression::CarData (Id / unlock-type / deform)
#include "GameSource/GameState/Progression/BrnProfile.h"           // BrnProgression::Profile (SetChosenLiveryIdForBaseCar / FindCar)
#include "GameSource/GameState/Progression/BrnProgressionManager.h"// BrnProgression::ProgressionManager (GetCarColourAndPalette)
#include "GameSource/GameState/Progression/BrnDerivedCars.h"       // BrnProgression::DerivedCarArray (StartCarModificationState's livery family)
#include "GameSource/GameState/BrnGameStateModule.h"               // BrnGameState::GameStateModule (streaming / car-change / pause hooks)
#include "GameSource/GameState/BrnGameActions.h"                   // GameStateModuleIO::ResetPlayerCarAction / CarSelectionChangedAction (typed payloads)
#include "SharedClasses/Trigger/BrnTriggerData.h"                  // BrnTrigger::TriggerData (GetSpawnLocation / count)
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h" // TriggerQueryManager::GetTriggerData

namespace BrnGameState
{
// The game-action queue the callers hand us is an GameStateModuleIO::GameActionQueue* (X360 == the
// OutputBuffer's embedded queue at this+4). Reinterpret it to the real backing type and post events
// via AddEvent (established precedent: BrnDriveThruManager.cpp / BrnTriggerQueryManager.cpp).
typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueueImpl;

static inline GameActionQueueImpl* AsActionQueue(GameStateModuleIO::GameActionQueue* lpQueue)
{
    // IDENTITY. GameStateModuleIO::GameActionQueue IS CgsModule::VariableEventQueue<13312,16>
    // (BrnGameStateSharedIO.h). The helper survives only as the call-site vocabulary.
    return lpQueue;
}

// The verbatim X360-baked source path the asserts reference (every assert in this TU shares it).
static const char* const KAC_CSM_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/CarSelect/BrnCarSelectManager.cpp";
static const char* const KAC_SPAWNLOC_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\trigger\\BrnSpawnLocation.h";
static const char* const KAC_BRNPROFILE_FILE =
    "..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h";

// flt_82029BB8 == 0.85: the deform DEBUG_UnlockCarsForTesting stamps onto a force-unlocked trophy car.
static const f32 KF_DEBUG_TROPHY_CAR_DEFORM = 0.85f;

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
    // ⚠️ RENAMED 2026-08-03. This is DWARF `CarSelectChangeColourAction`
    // (GameAction<E_ACTION_CAR_SELECT_CHANGE_COLOUR>, PS3 id 74 -> X360 79), whose payload is
    // {u32 muPaletteIndex, u32 muColourIndex} -- PALETTE FIRST. The old name `CarUnlockAction`
    // belongs to a DIFFERENT record: GameAction<E_ACTION_GUI_CAR_UNLOCK> (PS3 id 2), a single
    // `CgsID mCurrentCarToUnlock`, which is what KI_ACTION_CAR_CHANGED_SUFFIX below posts.
    KI_ACTION_CAR_SELECT_CHANGE_COLOUR  = 79,   // size 8          -- CarSelectChangeColourAction
    KI_ACTION_CAR_UNLOCK_END            = 63,   // size 1          -- CarUnlockEndAction
    KI_ACTION_CAR_SELECTION_CHANGED     = 64,   // size 64 (0x40)  -- CarSelectionChangedAction
    KI_ACTION_CAR_SELECTION_DROPIN      = 65,   // size 16         -- CarSelectionChangedDropInAction
    KI_ACTION_CAR_SELECTION_DROPIN_DONE = 66,   // size 16         -- CarSelectionDropInCompleteAction
    KI_ACTION_JUNKYARD_DRIVE_THRU       = 7,    // size 48 (0x30)  -- JunkYardDriveThruAction
    KI_ACTION_RESET_PLAYER_DRIVER       = 70,   // size 1          -- SetPlayerCarDriverAction (exit)
    KI_ACTION_SET_INPUT_ENABLED         = 71,   // size 1          -- (enter junkyard input enable)
    KI_ACTION_JUNKYARD_ENTERED          = 99,   // size 1          -- (junkyard entered flag)
    KI_ACTION_AUTOSAVE                  = 55,   // size 1          -- RequestAutoSaveAction
    KI_ACTION_REQUEST_GAME_TRAINING     = 149,  // size 4          -- RequestGameTrainingAction
    KI_ACTION_STARTUP_DEFORM            = 106,  // size 1          -- (ReallyEnter start-of-game deform)
};

// The pause-reason bit the junkyard exit clears (X360 UpdateExitState @0x82398C20 passes `li r4,1`
// to GameStateModule::RequestUnpause). The enum's real name is GameStateModuleIO::EPauseModule
// (the assert string "leUnpauseModule != E_PAUSE_NONE" attests it); only this one enumerator's
// VALUE is recovered, so it is named locally rather than fabricating the whole enum.
static const s32 KI_PAUSE_REASON_CAR_SELECT = 1;

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
    CGS_STATIC_ASSERT((offsetof(CarSelectManager, mDesiredCarId)
                      == offsetof(CarSelectManager, mStartCarId) + sizeof(CgsID)),
                  "mDesiredCarId is contiguous with mStartCarId (CgsID == 8 bytes)");
    CGS_STATIC_ASSERT((offsetof(CarSelectManager, mCacheDuringChangeCarId)
                      == offsetof(CarSelectManager, mDesiredCarId) + sizeof(CgsID)),
                  "mCacheDuringChangeCarId is contiguous with mDesiredCarId (CgsID == 8 bytes)");
    // The spawn-location array is exactly KU_CARSELECT_SPAWNLOCATION_COUNT handles wide.
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
    CGS_ASSERT(lpTriggerQueryManager, CGS_ASSERT_MESSAGE_AT("lpTriggerQueryManager", CGS_ARTIST_FILE, 319));
    CGS_ASSERT(lpGameStateModule, CGS_ASSERT_MESSAGE_AT("lpGameStateModule", CGS_ARTIST_FILE, 320));

    mpGameStateModule = lpGameStateModule;
    mpTriggerQueryManager = lpTriggerQueryManager;
    mpProgressionManager = lpProgressionManager;

    mfStateTimer = 0.0f;
    meState      = E_STATE_NONE;

    mpVehicleList = 0;
    mpWheelList = 0;

    mJunkyardId = 0;
    // One block clear (BrnCarSelectManager.h:334): ARTIST stores it as `std 0x28; std 0x30; stw 0x38`
    // (0x82356570), and the PS3 inline copy in GameStateModule::Construct (0x25A5A8) takes the
    // array's address before its five stores.
    std::memset(maSpawnLocations, 0, sizeof(maSpawnLocations));
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

    // [car-audio] HARNESS-ONLY, NOT an X360 store: `BRN_DEBUG_UNLOCK_CARS=1` arms the
    // console's OWN two development cheats above, which EnterJunkyard already reads to call
    // DEBUG_UnlockCarsForTesting() (ProgressionManager::AddCar on up to 3 trophy + 3 shutdown
    // cars).  WHY IT EXISTS: the harness creates a FRESH profile every run, a fresh profile
    // owns exactly ONE car (the Cavalry), so the junkyard carousel has a single entry and
    // -CarSelectTaps has nothing to move to -- measured 2026-09-16, three runs with Next:3 /
    // DPadRight:3 / no taps all left the player on VEH_PUSMC01.  The owner's bug is "every car
    // that is not the Cavalry has no sound", so a one-car profile cannot reproduce it at all.
    // Inert unless the variable is set; a default run is byte-identical.
}

// ============================================================================
// Prepare -- NO SYMBOL IN THE IMAGE: the console FULLY INLINES it.
//
// DWARF BrnCarSelectManager.h declares `Prepare(const VehicleList*, const WheelList*)`, but a
// name lookup over the ARTIST export set finds nothing (the export set's holes are real, but
// this one is an inline, not a hole). Recovered by CALLER instead -- its one call site is the
// terminal stage of GameStateModule::Prepare @0x8239E578, where it appears as two raw stores:
//
//     0x8239EC8C  add   r11, r31, r14      ; r14 = 0x456EC -> GameStateModule::mpWheelList
//     0x8239EC90  add   r10, r31, r17      ; r17 = 0x456E8 -> GameStateModule::mpVehicleList
//     0x8239EC98  addis r9,  r31, 3
//     0x8239ECA0  addi  r9,  r9,  -0x3260  ; r9 = this + 0x2CDA0 == &mCarSelectManager
//     0x8239ECA4  lwz   r7,  0(r11)
//     0x8239ECAC  lwz   r6,  0(r10)
//     0x8239ECB0  stw   r7,  0x18(r9)      ; mpWheelList   = lpWheelList
//     0x8239ECB4  stw   r6,  0x14(r9)      ; mpVehicleList = lpVehicleList
//
// That is the whole function -- two pointer stores, no asserts, no other side effect (the very
// next instructions are the identical pair for OnlineCarSelectManager at +0x0C/+0x10). Written
// in the DWARF's declaration order (vehicle then wheel); the X360's store order is the reverse
// only because of the scheduler.
// ============================================================================
void CarSelectManager::Prepare(const BrnResource::VehicleList* lpVehicleList,
                               const BrnResource::WheelList*   lpWheelList)
{
    mpVehicleList = lpVehicleList;
    mpWheelList = lpWheelList;

}

// ============================================================================
// IsInJunkyard -- NO SYMBOL IN THE IMAGE: the console INLINES it at every call site.
//
// Recovered from its call sites, which are all the same two instructions over mJunkyardId
// (this + 0x20, a 64-bit CgsID):
//     GameStateModule::PreWorldUpdate @0x823A5660 / @0x823A5914
//         ld     r11, 0(this + 0x2CDC0)      ; == &mCarSelectManager + 0x20 == mJunkyardId
//         cmpldi cr6, r11, 0
//         ... -> gate on CarSelectManager::Update and on the second car-select leg
// i.e. "in a junkyard" IS "mJunkyardId is not the null CgsID". Corroborated by the class's own
// invariant: EnterJunkyard*/ReallyEnterJunkyardAtStartOfGame are the only writers of a non-zero
// mJunkyardId, ExitJunkyard's tail clears it back to 0 (@0x82387880 -> this+0x20 = 0), and
// SetupSpawnLocations asserts `mJunkyardId != kCGSID_NULL` (BrnCarSelectManager.cpp:1063) as its
// own precondition.
// ============================================================================
// (IsInJunkyard is header-inline.)

// ============================================================================
// EnterModification (X360). [committed body -- preserved verbatim]
// ============================================================================
void CarSelectManager::EnterModification(GameStateModuleIO::GameActionQueue* lpActionQueue)
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
void CarSelectManager::Update(GameStateModuleIO::GameActionQueue* lpActionQueue,
                              const GameStateModuleIO::ControllerInput* lpControllerInput,
                              f32 lfGameTimestep)
{
    (void)lpControllerInput;   // passed through to the deeper state updaters; not read at this level

    CGS_ASSERT_AT(mpGameStateModule, "mpGameStateModule", KAC_CSM_FILE, 67);
    if (mpGameStateModule->IsAnythingPaused())
    {
        CGS_ASSERT_AT(lpActionQueue, "lpActionQueue", KAC_CSM_FILE, 70);
        // DWARF h:2720: an empty record, posted as its one uninitialised byte (no store, 0x8239C2A0).
        GameStateModuleIO::PausedInCarSelectAction lPausedAction;
        lpActionQueue->AddEvent(&lPausedAction, GameStateModuleIO::E_ACTION_CAR_SELECT_PAUSED_IN_CAR_SELECT,
                                static_cast<s32>(sizeof(lPausedAction)));
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
                if (lfTimer >= 0.5f)   // KF_CARSELECT_TRANSITION_IN_REQUEST_STREAMING: ARTIST loads the pooled 0.5 (0x82001DA0)
                {
                    RequestStreamingForUnlock(lpActionQueue);
                    mbTransitionInRequestStreaming = false;
                }
            }
            else
            {
                // DWARF cpp:97.
                bool lbMinTransitionTimeElapsed;
                if (muUnlockCount == 0)
                    lbMinTransitionTimeElapsed = lfTimer >= KF_TRANSITION_IN_MIN_TIME_NO_CARS;     // 2.0
                else
                    lbMinTransitionTimeElapsed = lfTimer >= KF_TRANSITION_IN_MIN_TIME_WITH_CARS;   // 6.0
                if (lbMinTransitionTimeElapsed && !mbWaitingForStreaming)
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
void CarSelectManager::StartTransitionInState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 251).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_NONE,
                         "CarSelectManager: State needs to be in E_STATE_NONE", KAC_CSM_FILE, 251);

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

    // DWARF cpp:275.
    GameStateModuleIO::CarSelectTransitionInAction lTransitionAction;
    lTransitionAction.mbStart         = true;
    lTransitionAction.mbUnlockingCars = muUnlockCount > 0;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTransitionAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_TRANSITION_IN,
                            static_cast<s32>(sizeof(lTransitionAction)));

    SpawnInStartCar(lpActionQueue);
}

// ============================================================================
// EndTransitionInState (X360 0x82392B30).
// Must be E_STATE_TRANSITION_IN. Posts the transition-in [End] action (payload[0]=0, payload[1]= "has
// unlocks"), then branches into the unlock sequence (if any cars remain to unlock) or straight to car
// select.
// ============================================================================
void CarSelectManager::EndTransitionInState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 294).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_TRANSITION_IN,
                         "CarSelectManager: State needs to be in E_STATE_TRANSITION_IN.", KAC_CSM_FILE, 294);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Transition In [End]\n";

    // DWARF cpp:299.
    GameStateModuleIO::CarSelectTransitionInAction lTransitionAction;
    lTransitionAction.mbStart         = false;
    lTransitionAction.mbUnlockingCars = muUnlockCount > 0;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTransitionAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_TRANSITION_IN,
                            static_cast<s32>(sizeof(lTransitionAction)));

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
// ============================================================================
// SaveChosenLiveryForCar (X360 0x82379EE8).
// Pin lCarId as the chosen livery of its BASE car on the profile. A livery variant (its
// VehicleListEntry livery-type byte at +0xE9 in {1,3,4}) is keyed under its parent car;
// anything else is its own base.
// ============================================================================
void CarSelectManager::SaveChosenLiveryForCar(CgsID lCarId)
{
    // DWARF cpp:1544..1546.
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    CGS_ASSERT_AT(lpProfile, "lpProfile", KAC_CSM_FILE, 1550);

    const BrnResource::VehicleListEntry* lpVehicleListEntry = mpVehicleList->GetVehicleData(lCarId);
    CGS_ASSERT_AT(lpVehicleListEntry, "lpVehicleListEntry", KAC_CSM_FILE, 1553);

    // A livery colour is keyed under its parent car, anything else under itself.
    if (lpVehicleListEntry->IsLiveryColour())
    {
        lpProfile->SetChosenLiveryIdForBaseCar(lpVehicleListEntry->GetParentId(), lCarId);
    }
    else
    {
        lpProfile->SetChosenLiveryIdForBaseCar(lCarId, lCarId);
    }
}

// ============================================================================
// StartCarModificationState (X360 0x82387410).
// Enter the paint/livery screen: state 5, reset timer, set mbInCarModScreen, post the
// car-mod-screen action (76, 8B), make sure a desired car is selected, and hand the car's
// derived colour-livery collection to the progression layer / the GUI.
//
// The derived-livery collection follows: the selected car's colour liveries are unlocked on the
// profile and, when the car has more than one livery version, they are posted as game action 69
// (CarSelectionRequestStreamingAction) so the world streams every version the paint screen can show.
// ============================================================================
void CarSelectManager::StartCarModificationState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (meState != E_STATE_CAR_SELECT && meState != E_STATE_REQUEST_CAR_CHANGE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "CarSelectManager: State needs to be in E_STATE_CAR_SELECT or E_STATE_REQUEST_CAR_CHANGE.",
            CGS_ASSERT_LOCATION(KAC_CSM_FILE, 385));
        CgsDev::Assert::EndAssert();
    }

    mfStateTimer     = KF_TIMER_RESET;              // X360 *(this+4) = 0.0
    meState          = E_STATE_CAR_MODIFICATION;    // 5
    mbInCarModScreen = true;                        // X360 *(this+115) = 1

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Car Modification\n";

    {
        GameStateModuleIO::CarSelectModificationScreen lModScreen;
        lModScreen.meCarSelectType = GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD;
        lModScreen.mbEntering = true;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lModScreen),
            KI_ACTION_CAR_MOD_SCREEN, sizeof(lModScreen));
    }

    // X360: if (mDesiredCarId == 0) mDesiredCarId = mStartCarId; then assert it is non-null.
    if (mDesiredCarId == 0)
    {
        mDesiredCarId = mStartCarId;
    }
    if (mDesiredCarId == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mDesiredCarId != kCGSID_NULL", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 402));
        CgsDev::Assert::EndAssert();
    }

    // ⭐ UN-PARKED [progression wave 2026-09-06, lane completion] -- THE DERIVED-LIVERY
    // COLLECTION, asm 0x82387560..0x82387728. Everything the old park was blocked on has a body:
    // DerivedCarArray::ConstructColourLiveryList @0x82374F60 (BrnDerivedCars.h, map arm
    // 2026-08-27), ProgressionManager::UnlockDerivedCarCollection @0x8237AD70 and
    // DerivedCarArray::DEBUG_PrintArray @0x8236ACE8 (this wave).
    //
    //     DerivedCarArray lDerivedCars;                                  // stw -1 into both counts
    //     lDerivedCars.ConstructColourLiveryList(mpVehicleList, mDesiredCarId);   // 0x8238756C
    //     CGS_ASSERT(lDerivedCars.GetLength() <= 8, "We weren't expecting <n> livery versions.");
    //     mpProgressionManager->UnlockDerivedCarCollection(lDerivedCars);         // 0x8238765C
    //     lDerivedCars.DEBUG_PrintArray();                                        // 0x82387664
    //     if (lDerivedCars.GetLength() > 1) { ...post the 88-byte action 69... }
    //
    // ⓘ THE ARGUMENTS ARE THIS CLASS'S OWN MEMBERS, not the progression manager's: the console
    // reads `lwz r4, 0x14(r29)` (mpVehicleList, +0x14) and `addi r31, r29, 0x48` (&mDesiredCarId,
    // +0x48) off the CarSelectManager, and only `lwz r3, 0x10(r29)` (mpProgressionManager) for
    // the unlock call itself.
    {
        BrnProgression::DerivedCarArray lDerivedCars;

        if (mpVehicleList != 0)
        {
            lDerivedCars.ConstructColourLiveryList(mpVehicleList, mDesiredCarId);
        }
        else
        {
        }

        // 0x823875A4..0x82387650. The console streams "We weren't expecting " << n <<
        // " livery versions." -- collapsed to the leading rodata fragment per the project
        // convention on the StrStream assert machinery. It cannot fire (Array<CgsID,8>::Append
        // asserts out-of-space first), and it is reproduced because it is in the binary.
        if (lDerivedCars.GetLength() > BrnProgression::KU_MAX_AMOUNT_OF_DERIVED_CARS)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("We weren't expecting ", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 407));
            CgsDev::Assert::EndAssert();
        }

        if (mpProgressionManager != 0)
        {
            mpProgressionManager->UnlockDerivedCarCollection(lDerivedCars);
        }

        // Debug dump, gated internally on CgsDev::Message::gxMessageFilterFlags bit 0.
        // Its own GetLength() != 0 assert (BrnDerivedCars.h:248) is the console's.
        lDerivedCars.DEBUG_PrintArray();

        // The livery-list publish: every version of the car, in the array's order, priority 1;
        // entry 0 (the base car) carries the wait-for-streaming flags (5), the rest carry none.
        if (lDerivedCars.GetLength() > 1)
        {
            GameStateModuleIO::CarSelectionRequestStreamingAction lRequestAction;
            lRequestAction.miCount = 0;
            for (u32 luIndex = 0; luIndex < lDerivedCars.GetLength(); ++luIndex)
            {
                lRequestAction.maCars[luIndex]            = lDerivedCars.GetItem(luIndex);
                lRequestAction.maiPriorities[luIndex]     = 1;
                lRequestAction.mauExtraInfoFlags[luIndex] = 0;
                ++lRequestAction.miCount;
            }
            lRequestAction.mauExtraInfoFlags[0] = 5;

            AsActionQueue(lpActionQueue)->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lRequestAction),
                GameStateModuleIO::E_ACTION_CAR_SELECTION_REQUEST_STREAMING, sizeof(lRequestAction));
        }
    }
}

void CarSelectManager::StartCarSelectState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST 0x82387310..0x82387350: the streamed assert (cpp:326 lStrStream, line 331).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_TRANSITION_IN || meState == E_STATE_CAR_MODIFICATION ||
                         meState == E_STATE_DISPLAY_UNLOCKED_CARS || meState == E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS ||
                         meState == E_STATE_CAR_SELECT,
                         "CarSelectManager: State needs to be in E_STATE_TRANSITION_IN or E_STATE_CAR_MODIFICATION.",
                         KAC_CSM_FILE, 331);

    if (meState == E_STATE_CAR_SELECT)
        return;

    if (meState == E_STATE_CAR_MODIFICATION)
    {
        mpGameStateModule->RequestStreamingForVehicleSelection(lpActionQueue, mDesiredCarId);
    }

    mfStateTimer = 0.0f;
    meState      = E_STATE_CAR_SELECT;
    mbInCarModScreen = false;   // X360 *(this+115) = 0

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Car Select\n";

    // DWARF cpp:354 / cpp:359.
    GameStateModuleIO::CarSelectReadyAction lReadyAction;
    lReadyAction.miCarSelectFlow = GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lReadyAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_READY,
                            static_cast<s32>(sizeof(lReadyAction)));

    GameStateModuleIO::CarSelectModificationScreen lModScreenAction;
    lModScreenAction.meCarSelectType = GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD;
    lModScreenAction.mbEntering      = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lModScreenAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_MODIFICATION_SCREEN,
                            static_cast<s32>(sizeof(lModScreenAction)));

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
void CarSelectManager::StartUnlockState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 449).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_TRANSITION_IN || meState == E_STATE_DISPLAY_UNLOCKED_CARS,
                         "CarSelectManager: State needs to be in E_STATE_TRANSITION_IN or E_STATE_DISPLAY_UNLOCKED_CARS.", KAC_CSM_FILE, 449);

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
void CarSelectManager::UpdateUnlockState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 758).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_DISPLAY_UNLOCKED_CARS || meState == E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS,
                         "CarSelectManager: State needs to be in Unlock sequence.", KAC_CSM_FILE, 758);

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
        CgsDev::Assert::FireAssert("lpUnlockCarData", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 777));
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

    if (mpGameStateModule == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpGameStateModule", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 816));
        CgsDev::Assert::EndAssert();
    }
    mpGameStateModule->SetCarUnlockAlreadyShown(lpUnlockCarData->GetId());
    mpGameStateModule->OnSpecialEventPlayerCarChange(mCurrentCarToUnlock, 0, lpActionQueue, true);

    const BrnTrigger::SpawnLocation* lpUnlockSpawnLocation = maSpawnLocations[3];
    if (lpUnlockSpawnLocation == 0)   // X360 *(v2+13) byte 0x34 == maSpawnLocations[3]
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 824));
        CgsDev::Assert::EndAssert();
    }

    // ResetPlayerCarAction (80B).
    // ⛔ CORRECTED 2026-08-01 -- the "FLAG: full ResetPlayerCarAction layout not in exports" was
    // wrong: the layout is fully attested at 0x82398B3C..0x82398B9C (payload base == var_B0,
    // size 0x50, r25 == 0, r28 == 1, r23 == 2), and it is the SAME record the other four builders
    // in this class emit. Every field of the old body was at the wrong offset and the spawn
    // transform was missing entirely. See BrnCarSelectManager_CarChange.cpp SpawnInStartCar for the
    // annotated master copy.
    //   +0x00 mPosition (16B)   +0x10 mDirection (16B)   +0x20 car id (8B)   +0x28 0 (8B)
    //   +0x30 8 (s32)           +0x34 deform (f32)       +0x38 1 (s32)       +0x3C 2 (s32)
    //   +0x40 1  +0x41 1  +0x42 0  +0x43 0
    // ⚠️ +0x3C is 2 HERE and 0 in SpawnInStartCar/EndUnlockState (`li r23, 2` @0x82398A74) -- the
    // unlock sequence is the one path that sets it; recorded as the console's literal.
    {
        u8 lacReset[80];
        std::memset(lacReset, 0, sizeof(lacReset));
        const CgsID lCarId        = lpUnlockCarData->GetId();
        const f32   lfDeform      = lpUnlockCarData->GetUnlockDeformationAmount();
        const s32   liCarSelType  = 8;
        const s32   liInCarSelect = 1;
        const s32   liUnlockMode  = 2;
        std::memcpy(lacReset + 0x00, &lpUnlockSpawnLocation->mPosition,  sizeof(Vector3));
        std::memcpy(lacReset + 0x10, &lpUnlockSpawnLocation->mDirection, sizeof(Vector3));
        std::memcpy(lacReset + 0x20, &lCarId,        sizeof(lCarId));
        std::memcpy(lacReset + 0x30, &liCarSelType,  sizeof(liCarSelType));
        std::memcpy(lacReset + 0x34, &lfDeform,      sizeof(lfDeform));
        std::memcpy(lacReset + 0x38, &liInCarSelect, sizeof(liInCarSelect));
        std::memcpy(lacReset + 0x3C, &liUnlockMode,  sizeof(liUnlockMode));
        lacReset[0x40] = 1;
        lacReset[0x41] = 1;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacReset), KI_ACTION_RESET_PLAYER_CAR, 80);
    }

    // CarSelectChangeColourAction (8B): the unlocked car's palette + colour indices.
    // X360 0x82398920: `GetCarColourAndPalette(.., &v17 + 4, &v17)` then `AddEvent(&v17, 79, 8)`
    // -- the COLOUR goes to payload+4 and the PALETTE to payload+0.
    {
        GameStateModuleIO::CarSelectChangeColourAction lColour;
        u32 liColour  = 0;
        u32 liPalette = 0;
        mpProgressionManager->GetCarColourAndPalette(mCurrentCarToUnlock, liColour, liPalette);
        lColour.muPaletteIndex = static_cast<u32>(liPalette);
        lColour.muColourIndex  = static_cast<u32>(liColour);
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lColour),
            KI_ACTION_CAR_SELECT_CHANGE_COLOUR, static_cast<s32>(sizeof(lColour)));
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
void CarSelectManager::EndUnlockState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 880).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_DISPLAY_UNLOCKED_CARS || meState == E_STATE_DISPLAY_UNLOCKED_SHUTDOWN_CARS,
                         "CarSelectManager: State needs to be in Unlock sequence.", KAC_CSM_FILE, 880);

    if (meState == E_STATE_DISPLAY_UNLOCKED_CARS)
    {
        SetupShutdownUnlockList();
        if (muUnlockCount)
        {
            StartUnlockState(lpActionQueue);
            return;
        }
    }

    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[1];   // X360 *(this+44)
    if (lpSpawnLocation == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 898));
        CgsDev::Assert::EndAssert();
    }

    mbCurrentCarTickerVisible = true;   // X360 *(this+60) = 1

    // X360: GetProfileCarData(this, &mCurrentCarToUnlock) -- the just-unlocked car being settled.
    CgsID lUnlockCarId = mCurrentCarToUnlock;
    const BrnProgression::CarData* lpCarData = GetProfileCarData(lUnlockCarId);

    // ResetPlayerCarAction (80B).
    // ⛔ CORRECTED 2026-08-01 -- the transform was NOT "modelled opaque", it was simply missing, and
    // every remaining field sat at the wrong offset. Attested at 0x82392D64..0x82392DC8 (payload
    // base == var_90, size 0x50, r30 == 0, r28 == 1):
    //   +0x00 mPosition (16B)   +0x10 mDirection (16B)   +0x20 mCurrentCarToUnlock (8B)  +0x28 0
    //   +0x30 8 (s32)           +0x34 deform (f32)       +0x38 1 (s32)                   +0x3C 0
    //   +0x40 1  +0x41 1  +0x42 0  +0x43 0
    {
        u8 lacReset[80];
        std::memset(lacReset, 0, sizeof(lacReset));
        const CgsID lCarId        = mCurrentCarToUnlock;
        const s32   liCarSelType  = 8;
        const s32   liInCarSelect = 1;
        std::memcpy(lacReset + 0x00, &lpSpawnLocation->mPosition,  sizeof(Vector3));
        std::memcpy(lacReset + 0x10, &lpSpawnLocation->mDirection, sizeof(Vector3));
        std::memcpy(lacReset + 0x20, &lCarId,        sizeof(lCarId));
        std::memcpy(lacReset + 0x30, &liCarSelType,  sizeof(liCarSelType));
        if (lpCarData != 0)
        {
            const f32 lfDeform = lpCarData->GetUnlockDeformationAmount();
            std::memcpy(lacReset + 0x34, &lfDeform, sizeof(lfDeform));
        }
        std::memcpy(lacReset + 0x38, &liInCarSelect, sizeof(liInCarSelect));
        lacReset[0x40] = 1;
        lacReset[0x41] = 1;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacReset), KI_ACTION_RESET_PLAYER_CAR, 80);
    }

    const u32 luSpawnType = static_cast<u32>(lpSpawnLocation->GetType());
    if (luSpawnType >= static_cast<u32>(BrnTrigger::SpawnLocation::E_TYPE_COUNT))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", CGS_ASSERT_LOCATION(KAC_SPAWNLOC_FILE, 152));
        CgsDev::Assert::EndAssert();
    }

    // CarSelectionChangedAction (64B): junkyard id + the spawn transform + the "pos is left" bit.
    // ⛔ CORRECTED 2026-08-01 -- the bit VALUE here was already right (type == 1) but it was stamped
    // at +0x10, on top of where the console puts the spawn POSITION, and the consumer
    // (MainDirector::ProcessInputQueue case 64, `lbz r11, 0x30(r30)`) reads +0x30. X360
    // 0x82392DCC..: +0x00 mJunkyardId, +0x10 mPosition, +0x20 mDirection, +0x30 the bit, +0x31 0.
    {
        u8 lacChanged[64];
        std::memset(lacChanged, 0, sizeof(lacChanged));
        std::memcpy(lacChanged + 0x00, &mJunkyardId,                 sizeof(mJunkyardId));
        std::memcpy(lacChanged + 0x10, &lpSpawnLocation->mPosition,  sizeof(Vector3));
        std::memcpy(lacChanged + 0x20, &lpSpawnLocation->mDirection, sizeof(Vector3));
        lacChanged[0x30] = (luSpawnType == 1) ? 1 : 0;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacChanged), KI_ACTION_CAR_SELECTION_CHANGED, 64);
    }

    if (static_cast<u32>(lpSpawnLocation->GetType()) >= static_cast<u32>(BrnTrigger::SpawnLocation::E_TYPE_COUNT))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", CGS_ASSERT_LOCATION(KAC_SPAWNLOC_FILE, 152));
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

    mpGameStateModule->OnSpecialEventPlayerCarChange(mCurrentCarToUnlock, 0, lpActionQueue, true);

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
void CarSelectManager::UpdateRequestCarChangeState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 611).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_REQUEST_CAR_CHANGE,
                         "CarSelectManager: State needs to be in E_STATE_REQUEST_CAR_CHANGE.", KAC_CSM_FILE, 611);

    // X360: if (active-player-car CgsID == mDesiredCarId) -> swap already complete.
    const CgsID lActiveCarId = mpGameStateModule->GetActivePlayerCarId();   // GameStateModule + 0x456D8
    if (lActiveCarId == mDesiredCarId)
    {
        mfStateTimer = KF_CARSELECT_DELAY_BEFORE_CHANGE_CAR;   // 1.0
        meState      = E_STATE_STARTING_CHANGING_CAR;          // 7
        return;
    }

    if (mbWaitingForStreaming)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("!mbWaitingForStreaming", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 622));
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

    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[meLastSpawnLocationType];
    if (lpSpawnLocation == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpSpawnLocation", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 643));
        CgsDev::Assert::EndAssert();
    }

    // ARTIST 0x82387C44-0x82387C94: full spawn pose and flags at +0x30/+0x31.
    {
        GameStateModuleIO::CarSelectionChangedAction lChanged;
        lChanged.mJunkyardId = mJunkyardId;
        lChanged.mPosition = lpSpawnLocation->mPosition;
        lChanged.mDirection = lpSpawnLocation->mDirection;
        lChanged.mbIsLeft = lpSpawnLocation->GetType() == BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT;
        lChanged.mbPlayCraneDropAudio = true;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lChanged), KI_ACTION_CAR_SELECTION_CHANGED, sizeof(lChanged));
    }

    if (!mbInCarModScreen)
        mpGameStateModule->RequestStreamingForVehicleSelection(lpActionQueue, mDesiredCarId);

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
void CarSelectManager::UpdateChangeCarState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 679).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_STARTING_CHANGING_CAR || meState == E_STATE_CHANGING_CAR,
                         "CarSelectManager: State needs to be in E_STATE_CHANGING_CAR.", KAC_CSM_FILE, 679);

    if (meState == E_STATE_STARTING_CHANGING_CAR)   // 7
    {
        if (mfStateTimer >= KF_CARSELECT_DELAY_BEFORE_CHANGE_CAR)   // >= 1.0
        {
            const CgsID lActiveCarId = mpGameStateModule->GetActivePlayerCarId();
            if (lActiveCarId == mDesiredCarId)
            {
                // DWARF cpp:699.
                GameStateModuleIO::CarSelectionChangedDropInAction lCarSelectDropInAction;
                lCarSelectDropInAction.mbIsLeft = meLastSpawnLocationType == BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT;
                lCarSelectDropInAction.mCarId   = mDesiredCarId;
                lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectDropInAction),
                                        GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED_DROPIN,
                                        static_cast<s32>(sizeof(lCarSelectDropInAction)));
                meState = E_STATE_CHANGING_CAR;   // 8
            }
            else
            {
                if (mpGameStateModule == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("mpGameStateModule", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 689));
                    CgsDev::Assert::EndAssert();
                }
                mpGameStateModule->OnSpecialEventPlayerCarChange(mDesiredCarId, 0, lpActionQueue, true);
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
        // DWARF cpp:736.
        GameStateModuleIO::CarSelectionDropInCompleteAction lCarSelectDropInCompleteAction;
        lCarSelectDropInCompleteAction.mbIsLeft = meLastSpawnLocationType == BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT;
        lCarSelectDropInCompleteAction.mCarId   = mDesiredCarId;
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectDropInCompleteAction),
                                GameStateModuleIO::E_ACTION_CAR_SELECTION_DROPIN_COMPLETE,
                                static_cast<s32>(sizeof(lCarSelectDropInCompleteAction)));
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
void CarSelectManager::UpdateExitState(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 958).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_EXITING,
                         "CarSelectManager: State needs to be in E_STATE_EXITING.", KAC_CSM_FILE, 958);

    if (mbWaitingForStreaming)
        return;

    meState          = E_STATE_NONE;   // 0
    mbInCarModScreen = false;          // X360 *(this+115) = 0
    mfStateTimer     = KF_TIMER_RESET; // 0.0

    // DWARF cpp:973: the end of the offline junkyard mod screen.
    GameStateModuleIO::CarSelectModificationScreen lModScreenAction;
    lModScreenAction.meCarSelectType = GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD;
    lModScreenAction.mbEntering      = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lModScreenAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_MODIFICATION_SCREEN,
                            static_cast<s32>(sizeof(lModScreenAction)));

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: Exit state is finished.\n";

    mJunkyardId = 0;   // X360 *(this+0x20) = 0

    // DWARF cpp:983.
    GameStateModuleIO::AllowBoostEarningAction lAllowBoostEarningAction;
    lAllowBoostEarningAction.mbAllowBoostEarning = true;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAllowBoostEarningAction),
                            GameStateModuleIO::E_ACTION_ALLOW_BOOST_EARNING,
                            static_cast<s32>(sizeof(lAllowBoostEarningAction)));

    // DWARF cpp:987: the junkyard's exit spawn location (maSpawnLocations[4], +0x38).
    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[4];
    CGS_ASSERT_AT(lpSpawnLocation, "lpSpawnLocation", KAC_CSM_FILE, 988);

    // DWARF cpp:991.
    GameStateModuleIO::CarSelectExitAction lCarSelectExitAction;
    lCarSelectExitAction.mExitSpawnLocation = lpSpawnLocation->mPosition;
    lCarSelectExitAction.mbOnlineCarSelect  = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectExitAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_FINISHED,
                            static_cast<s32>(sizeof(lCarSelectExitAction)));

    // DWARF cpp:997 JunkYardDriveThruAction (this tree's DriveThruJunkYardAction, id 99).
    GameStateModuleIO::DriveThruJunkYardAction lJunkYardAction;
    lJunkYardAction.mbIsInJunkYard = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lJunkYardAction),
                            GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD,
                            static_cast<s32>(sizeof(lJunkYardAction)));

    // DWARF cpp:1002 (ARTIST passes &mDesiredCarId itself, 0x82398DA4).
    const BrnProgression::CarData* lpCarData = GetProfileCarData(mDesiredCarId);
    CGS_ASSERT_AT(lpCarData, "lpCarData", KAC_CSM_FILE, 1004);

    // DWARF cpp:1007: the car goes back onto the exit spawn point, out of car-select.
    GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
    lResetPlayerCarAction.Construct();
    lResetPlayerCarAction.SetLocation(lpSpawnLocation->mPosition, lpSpawnLocation->mDirection);
    lResetPlayerCarAction.SetCarData(mDesiredCarId, 0);
    lResetPlayerCarAction.SetDeformAmount(lpCarData->GetUnlockDeformationAmount());
    lResetPlayerCarAction.SetInCarSelect(false);
    lResetPlayerCarAction.SetCarSelectDontStreamAudio(false);
    lResetPlayerCarAction.SetDeformationResetType(1);
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                            GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                            static_cast<s32>(sizeof(lResetPlayerCarAction)));

    // DWARF cpp:1019: control goes back to the player.
    GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction;
    lSetPlayerCarDriverAction.mbIsDriveThru = false;
    lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_ENTITY_MODULE;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                            GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                            static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));

    // DWARF cpp:1026.
    GameStateModuleIO::RequestGameTrainingAction lTrainingAction;
    lTrainingAction.meTrainingType = BrnProgression::E_TRAINING_TYPE_LEAVES_JUNKYARD;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTrainingAction),
                            GameStateModuleIO::E_ACTION_REQUEST_GAME_TRAINING,
                            static_cast<s32>(sizeof(lTrainingAction)));

    mpProgressionManager->SetDriveThrusDirtyFlag();

    // X360 UpdateExitState @0x82398C20: `li r4, 1` -- the pause-REASON bit this exit clears, not a
    // bool (see the RequestUnpause signature note in BrnGameStateModule.h).
    mpGameStateModule->RequestUnpause(KI_PAUSE_REASON_CAR_SELECT, lpActionQueue);

    if (!mpGameStateModule->IsOnlineGameMode())
        mpGameStateModule->OnPlayerCarChange(mDesiredCarId, 0, lpActionQueue, true);

    CGS_ASSERT_AT(mpProgressionManager, "mpProgressionManager", KAC_CSM_FILE, 1043);
    mpProgressionManager->RequestUpdateRivals();

    // DWARF cpp:1047 (RequestAutoSaveAction's constructor clears mbForceAutosave).
    GameStateModuleIO::RequestAutoSaveAction lAutosaveAction;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAutosaveAction),
                            GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE,
                            static_cast<s32>(sizeof(lAutosaveAction)));

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
void CarSelectManager::EnterJunkyard(GameStateModuleIO::GameActionQueue* lpActionQueue, CgsID lJunkyardId)
{
    // ARTIST streams this assert through a StrStream over gpcMessageBuffer (line 196).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_NONE,
                         "CarSelectManager: State is invalid", KAC_CSM_FILE, 196);

    if (mbDEBUG_UnlockTrophyCarsForTesting || mbDEBUG_UnlockShutdownCarsForTesting)   // X360 *(this+119)||*(this+120)
        DEBUG_UnlockCarsForTesting();

    mJunkyardId = lJunkyardId;   // X360 *(this+0x20) = lJunkyardId

    // ARTIST 0x823985DC..0x82398628: the id goes out in hex-once mode (`stw 2, 4(stream)`) through
    // the out-of-line operator<<(u64) 0x82203EE8.
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: EnterJunkyard: "
                                   << CgsDev::E_PRINTMODE_HEXONCE << mJunkyardId << "\n";
    }

    SetupSpawnLocations();

    // DWARF cpp:212 JunkYardDriveThruAction (this tree's DriveThruJunkYardAction, id 99).
    GameStateModuleIO::DriveThruJunkYardAction lJunkYardAction;
    lJunkYardAction.mbIsInJunkYard = true;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lJunkYardAction),
                            GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD,
                            static_cast<s32>(sizeof(lJunkYardAction)));

    // DWARF cpp:217.
    GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction;
    lSetPlayerCarDriverAction.mbIsDriveThru = false;
    lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_NONE;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                            GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                            static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));

    // DWARF cpp:223 / cpp:228 (StopBoostingAction has no members: its one byte is posted as is).
    GameStateModuleIO::AllowBoostEarningAction lAllowBoostEarningAction;
    lAllowBoostEarningAction.mbAllowBoostEarning = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAllowBoostEarningAction),
                            GameStateModuleIO::E_ACTION_ALLOW_BOOST_EARNING,
                            static_cast<s32>(sizeof(lAllowBoostEarningAction)));
    GameStateModuleIO::StopBoostingAction lStopBoostingAction;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lStopBoostingAction),
                            GameStateModuleIO::E_ACTION_STOP_BOOSTING,
                            static_cast<s32>(sizeof(lStopBoostingAction)));

    // The active player car becomes the start car and its own chosen livery (ARTIST passes the id
    // twice, r4/r5, 0x823986AC..B0; DWARF Profile::SetChosenLiveryIdForBaseCar(CgsID, CgsID)).
    mStartCarId = mpGameStateModule->GetActivePlayerCarId();
    mpProgressionManager->GetProfile()->SetChosenLiveryIdForBaseCar(mStartCarId, mStartCarId);

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
void CarSelectManager::EnterJunkyardAtStartOfGame(GameStateModuleIO::GameActionQueue* lpActionQueue,
                                                  CgsID lJunkyardId, CgsID lCarModelId, CgsID lWheelId,
                                                  GameStateModuleIO::EPlayerScoringIndex leScoringIndex,
                                                  GameStateModuleIO::CarSelectionChangedAction* lpCarSelectChangedAction)
{
    mJunkyardId = lJunkyardId;   // X360 *(this+0x20) = a2 (junkyard id)

    SetupSpawnLocations();

    // DWARF cpp:1588: the left car-select spawn point (maSpawnLocations[1], +0x2C).
    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT];
    CGS_ASSERT_AT(lpSpawnLocation, "lpSpawnLocation", KAC_CSM_FILE, 1589);
    meLastSpawnLocationType = lpSpawnLocation->GetType();   // X360 *(this+60)

    // DWARF cpp:1592: the start-of-game car drops into the junkyard's car-select slot with the
    // -1.0 no-deform sentinel Construct leaves (ARTIST 0x82393124); +0x38 is not written.
    GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
    lResetPlayerCarAction.Construct();
    lResetPlayerCarAction.SetLocation(lpSpawnLocation->GetPosition(), lpSpawnLocation->GetDirection());
    lResetPlayerCarAction.SetPlayerScoringIndex(leScoringIndex);
    lResetPlayerCarAction.SetCarData(lCarModelId, lWheelId);
    lResetPlayerCarAction.SetInCarSelect(true);
    lResetPlayerCarAction.SetCarSelectType(GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP);
    lResetPlayerCarAction.SetCarSelectDontStreamAudio(false);
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                            GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                            static_cast<s32>(sizeof(lResetPlayerCarAction)));

    // The caller's selection record (ARTIST 0x82393178..0x823931E0 stores through it unguarded).
    lpCarSelectChangedAction->mJunkyardId           = mJunkyardId;
    lpCarSelectChangedAction->mPosition             = lpSpawnLocation->GetPosition();
    lpCarSelectChangedAction->mDirection            = lpSpawnLocation->GetDirection();
    lpCarSelectChangedAction->mbIsLeft              = lpSpawnLocation->GetType() == BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT;
    lpCarSelectChangedAction->mbPlayCraneDropAudio  = false;

    mpGameStateModule->OnSpecialEventPlayerCarChange(lCarModelId, lWheelId, lpActionQueue, true);

    mStartCarId = lCarModelId;   // X360 *(this+64) = v7 (car model id)
}

// ============================================================================
// UpdateCarColour (DWARF cpp:1523, h:167; no ARTIST copy: every caller inlines it).
// The car's colour and palette, read straight into the action's fields.
// ============================================================================
void CarSelectManager::UpdateCarColour(CgsID lCarId, GameStateModuleIO::GameActionQueue* lpActionQueue) const
{
    GameStateModuleIO::CarSelectChangeColourAction lChangeCarColourAction;
    mpProgressionManager->GetCarColourAndPalette(lCarId, lChangeCarColourAction.muColourIndex,
                                                 lChangeCarColourAction.muPaletteIndex);
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lChangeCarColourAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_CHANGE_COLOUR,
                            static_cast<s32>(sizeof(lChangeCarColourAction)));
}

// ============================================================================
// ReallyEnterJunkyardAtStartOfGame (X360 0x823931F8).
// Completes the boot junkyard entry once setup is acked: posts the junkyard drive-thru (48B) action,
// posts the desired car's colour/palette (8B), and -- if the profile flags a startup deform -- finds
// the desired car's CarData and stamps the 0.85 startup deform, posting the startup-deform (1B) flag.
// Posts the junkyard-entered (1B) action, then starts the transition-in.
// ============================================================================
void CarSelectManager::ReallyEnterJunkyardAtStartOfGame(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // DWARF cpp:1628: the player keeps no driver while the junkyard takes over.
    GameStateModuleIO::SetPlayerCarDriverAction lSetPlayerCarDriverAction;
    lSetPlayerCarDriverAction.mbIsDriveThru = false;
    lSetPlayerCarDriverAction.meCarControl  = BrnWorld::E_CAR_CONTROL_NONE;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSetPlayerCarDriverAction),
                            GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER,
                            static_cast<s32>(sizeof(lSetPlayerCarDriverAction)));

    UpdateCarColour(mStartCarId, lpActionQueue);

    // DWARF cpp:1640: a brand-new profile's start car is handed out pre-deformed (0.85 read from
    // the same constant as the livery-select delay, 0x82029BB8).
    GameStateModuleIO::JunkYardDriveThruOnGameStartAction lGameAction;
    if (mpProgressionManager->GetProfile()->GetIsNewProfile())
    {
        BrnProgression::CarData* lpCarData = mpProgressionManager->GetProfile()->FindCar(mStartCarId);
        CGS_ASSERT_AT(lpCarData, "lpCarData", KAC_CSM_FILE, 1646);
        lGameAction.mbNewProfile = true;
        lpCarData->SetUnlockDeformationAmount(KF_LIVERYSELECT_DELAY_BEFORE_CHANGE);
    }
    else
    {
        lGameAction.mbNewProfile = false;
    }
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lGameAction),
                            GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD_ON_GAME_START,
                            static_cast<s32>(sizeof(lGameAction)));

    // DWARF cpp:1661 JunkYardDriveThruAction (this tree's DriveThruJunkYardAction, id 99).
    GameStateModuleIO::DriveThruJunkYardAction lJunkYardAction;
    lJunkYardAction.mbIsInJunkYard = true;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lJunkYardAction),
                            GameStateModuleIO::E_ACTION_DRIVE_THRU_JUNK_YARD,
                            static_cast<s32>(sizeof(lJunkYardAction)));

    StartTransitionInState(lpActionQueue);
}

// ============================================================================
// ExitJunkyard (X360 0x82387880).
// Must be E_STATE_CAR_MODIFICATION. Resets the timer, sets state 9 (EXITING), posts the car-select-exit
// (1B) action, sets waiting-for-streaming + the exit-pending latch (this+117), and logs the exit start.
// ============================================================================
void CarSelectManager::ExitJunkyard(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // ARTIST 0x823878A0..0x82387914: the streamed assert (StrStream over gpcMessageBuffer, cpp:503).
    CGS_ASSERT_STREAM_AT(meState == E_STATE_CAR_MODIFICATION,
                         "CarSelectManager: State needs to be in E_STATE_CAR_MODIFICATION.", KAC_CSM_FILE, 503);

    mfStateTimer = KF_TIMER_RESET;   // 0.0
    meState      = E_STATE_EXITING;  // 9

    // DWARF cpp:508.
    GameStateModuleIO::CarSelectWaitingForAudioAction lWaitingForAudioAction;
    lWaitingForAudioAction.mbWaiting = true;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWaitingForAudioAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECT_WAITING_FOR_AUDIO,
                            static_cast<s32>(sizeof(lWaitingForAudioAction)));

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
void CarSelectManager::ForceExitJunkyard(GameStateModuleIO::GameActionQueue* lpActionQueue, bool lbToOnlineEvent)
{
    // X360 asm: mfStateTimer = 0.0; meState = 9 (li r9,9); mbWaitingForStreaming = 0;
    // mDesiredCarId(+0x48) = mStartCarId(+0x40) (the abort carries the entry car id back through exit).
    mfStateTimer          = KF_TIMER_RESET;        // 0.0
    meState               = E_STATE_EXITING;       // 9
    mbWaitingForStreaming = false;                 // X360 *(this+88) = 0
    mDesiredCarId         = mStartCarId;           // X360 *(this+0x48) = *(this+0x40)

    UpdateExitState(lpActionQueue);

    {
        const bool lbAbortToOnlineEvent = lbToOnlineEvent;
        AsActionQueue(lpActionQueue)->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lbAbortToOnlineEvent),
            KI_ACTION_FORCE_EXIT, sizeof(lbAbortToOnlineEvent));
    }

    mbCarUnlockEnabled = true;   // X360 *(this+117) = 1 == byte 0x75 == mbCarUnlockEnabled (re-enable car unlock on exit)
}
// ---- formerly BrnCarSelectManager_CarChange.cpp (same DWARF home file) ----

// ============================================================================
// GetProfileCarData (X360 0x82365760).
// Resolve the player Profile's CarData record whose car id == lrCarID, or NULL on miss. Used by the
// unlock-display / change-car / exit / spawn paths to fetch the deform amount + id of the car being
// shown or spawned. (lrCarID is taken by reference to mirror the DWARF signature; only its value is
// read here.)
// ============================================================================
const BrnProgression::CarData* CarSelectManager::GetProfileCarData(CgsID& lrCarID) const
{
    if (mpProgressionManager == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1341));
        CgsDev::Assert::EndAssert();
    }

    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1347));
        CgsDev::Assert::EndAssert();
    }

    const BrnProgression::CarData* lpCarData = 0;
    const s32 liCarCount = lpProfile->GetCarCount();
    for (s32 li = 0; li < liCarCount; ++li)
    {
        const BrnProgression::CarData* lpCandidate = lpProfile->GetCarData(li);
        if (lpCandidate->GetId() == lrCarID)
        {
            lpCarData = lpCandidate;
            break;
        }
    }

    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1351));
        CgsDev::Assert::EndAssert();
    }
    return lpCarData;
}

// ============================================================================
// IsThisCarInCurrentUnlockSequence (X360 0x823799E8).
// Predicate used while building the unlock list (and walking GetNextUnlockCarID): does this profile
// car belong in the set of cars whose unlock is being shown right now?
//
// CarData fields the X360 reads:
//   +0x0A  bool  -- a per-car "hidden / already-shown" flag (true => excluded from BOTH paths).
//   +0x10  s32   -- the car's UNLOCK-TYPE enumerator (1/2/3 == various trophy/event unlocks, 5 == a
//                   rank-gated unlock, etc.).
// Two modes, selected by mbShutdownUnlockSequence (X360 *(this+0x74)):
//   * shutdown-unlock list (mbShutdownUnlockSequence != 0): in iff unlock-type == 3.
//   * normal list (mbShutdownUnlockSequence == 0):
//       - unlock-type == 3  -> NOT in this list (it belongs to the shutdown list).
//       - unlock-type == 5  -> in iff the player's progression rank >= the car's required rank
//                              (read from the VehicleListEntry gameplay data, +0x99).
//       - any other type    -> in.
// The hidden-flag at +0x0A short-circuits everything to "false".
// ============================================================================
bool CarSelectManager::IsThisCarInCurrentUnlockSequence(const BrnProgression::CarData* lpProfileCar) const
{
    if (lpProfileCar->WasUnlockSequenceAlreadyShown())   // X360 *(lpProfileCar + 0x0A) -- FLAG: CarData +0x0A bool
        return false;

    const s32 leUnlockType = lpProfileCar->GetUnlockType();   // X360 *(lpProfileCar + 0x10) -- FLAG: CarData +0x10 s32

    if (mbShutdownUnlockSequence)   // X360 *(this + 0x74) == mbShutdownUnlockSequence (the +0x70 select)
        return leUnlockType == 3;

    if (leUnlockType == 5)
    {
        // Rank-gated unlock: in iff the player's progression rank has reached the car's requirement.
        const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lpProfileCar->GetId());
        const BrnResource::VehicleListEntry* lpEntry =
            (liVehicleIndex < 0) ? 0 : mpVehicleList->GetVehicleData(liVehicleIndex);
        if (lpEntry == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpVehicleListEntry != NULL", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1169));
            CgsDev::Assert::EndAssert();
        }
        // X360 takes &entry->mGamePlayData (entry + 0x90) and asserts it non-NULL (it never is for a
        // by-value sub-object, but the baked assert is reproduced for fidelity), then reads the
        // required-rank byte at +0x09 of that sub-object.
        if (lpEntry == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpVehicleListEntryGamePlayData != NULL", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1172));
            CgsDev::Assert::EndAssert();
        }
        const s32 liRequiredRank = lpEntry->GetUnlockRank();   // X360 *(entry + 0x90 + 0x09) -- FLAG
        if (mpProgressionManager->GetProgressionRank() >= liRequiredRank)
            return true;
        return false;
    }
    else
    {
        // Every non-rank-gated unlock is in the normal list -- except the shutdown-only type 3.
        if (leUnlockType != 3)
            return true;
        return false;
    }
}

// ============================================================================
// SetupNormalUnlockList (X360 0x82379AD8).
// Recount how many profile cars belong in the NORMAL unlock-display sequence: clear the count + the
// shutdown-select flag, then (offline only, and only when unlock is enabled) walk every profile car
// and tally the ones IsThisCarInCurrentUnlockSequence accepts. Finally latch mbNoNormalUnlockCars =
// (count == 0). muUnlockCount drives the transition-in min-time + the unlock-state branch.
// ============================================================================
void CarSelectManager::SetupNormalUnlockList()
{
    if (mpProgressionManager == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1199));
        CgsDev::Assert::EndAssert();
    }
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1203));
        CgsDev::Assert::EndAssert();
    }

    muUnlockCount            = 0;   // X360 *(this + 0x5C) = 0
    mbShutdownUnlockSequence = false;   // X360 *(this + 0x74) = 0 (select the normal predicate)

    // Offline only, and only when car-unlock is enabled (X360 *(this + 0x75) == mbCarUnlockEnabled).
    if (!mpGameStateModule->IsOnlineGameMode() && mbCarUnlockEnabled)
    {
        // The count is re-read every pass (lwz 0x26c at 0x82379BD8).
        for (s32 li = 0; li < lpProfile->GetCarCount(); ++li)
        {
            const BrnProgression::CarData* lpProfileCar = lpProfile->GetCarData(li);
            if (lpProfileCar == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpProfileCar", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1217));
                CgsDev::Assert::EndAssert();
            }
            if (IsThisCarInCurrentUnlockSequence(lpProfileCar))
                ++muUnlockCount;
        }
    }

    mbNoNormalUnlockCars = (muUnlockCount < 1);    // X360 0x82379BEC: unsigned compare with 1 (subfc/subfe)
}

// ============================================================================
// SetupShutdownUnlockList (X360 0x82379C08).
// The "Burning Route / shutdown rival" variant: arm the shutdown-select flag, recount how many
// profile cars are in the shutdown sequence (offline + unlock-enabled), leaving muUnlockCount set.
// Unlike the normal list it does NOT touch mbNoNormalUnlockCars (it only sets the +0x70 select to 1
// up front). Used when entering the junkyard after taking down a rival who drops a car.
// ============================================================================
void CarSelectManager::SetupShutdownUnlockList()
{
    if (mpProgressionManager == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpProgressionManager", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1239));
        CgsDev::Assert::EndAssert();
    }
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1243));
        CgsDev::Assert::EndAssert();
    }

    // X360 0x82379C08 writes BOTH bytes: *(this + 0x70) = 1 (mbNoNormalUnlockCars -- the "this junkyard
    // entry should run a shutdown-rival unlock display" select StartTransitionInState/StartUnlockState
    // read) AND *(this + 0x74) = 1 (mbShutdownUnlockSequence -- the IsThisCarInCurrentUnlockSequence
    // shutdown-predicate select). They are set together here but read at different points, so they are
    // kept DISTINCT (folding them is unsafe: SetupNormalUnlockList clears only 0x74 while leaving 0x70 live).
    mbNoNormalUnlockCars     = true;    // X360 *(this + 0x70) = 1
    muUnlockCount            = 0;       // X360 *(this + 0x5C) = 0
    mbShutdownUnlockSequence = true;    // X360 *(this + 0x74) = 1

    if (!mpGameStateModule->IsOnlineGameMode() && mbCarUnlockEnabled)
    {
        // The count is re-read every pass (lwz 0x26c at 0x82379D10).
        for (s32 li = 0; li < lpProfile->GetCarCount(); ++li)
        {
            const BrnProgression::CarData* lpProfileCar = lpProfile->GetCarData(li);
            if (lpProfileCar == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpProfileCar", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1258));
                CgsDev::Assert::EndAssert();
            }
            if (IsThisCarInCurrentUnlockSequence(lpProfileCar))
                ++muUnlockCount;
        }
    }
}

// ============================================================================
// GetNextUnlockCarID (X360 0x82379D28).
// Given the car id currently being shown (lCurrentID, or kCGSID_NULL to start), return the id of the
// NEXT profile car in the unlock sequence. Finds lCurrentID's index in the profile car array (asserts
// it is present when non-null), then scans forward from index+1 for the first car
// IsThisCarInCurrentUnlockSequence accepts. Asserts "Can't find next unlock!" + returns kCGSID_NULL if
// none remains.
// ============================================================================
CgsID CarSelectManager::GetNextUnlockCarID(CgsID lCurrentID)
{
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1372));
        CgsDev::Assert::EndAssert();
    }

    s32 liCurrentIndex = -1;

    if (lCurrentID != 0)
    {
        s32 liCarCount = lpProfile->GetCarCount();
        s32 liCarIndex = 0;
        if (liCarCount > 0)
        {
            for (;;)
            {
                if (liCarIndex < 0 || liCarIndex >= liCarCount)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liCarIndex >= 0 && liCarIndex < miCarCount", CGS_ASSERT_LOCATION(KAC_BRNPROFILE_FILE, 1923));
                    CgsDev::Assert::EndAssert();
                }
                if (lpProfile->GetCarData(liCarIndex)->GetId() == lCurrentID)
                {
                    liCurrentIndex = liCarIndex;
                    break;
                }
                liCarCount = lpProfile->GetCarCount();
                ++liCarIndex;
                if (liCarIndex >= liCarCount)
                    break;
            }
        }
        if (liCurrentIndex < 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("liCurrentIndex >=0", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1386));
            CgsDev::Assert::EndAssert();
        }
    }

    s32 liCarCount = lpProfile->GetCarCount();
    s32 liNextIndex = liCurrentIndex + 1;
    if (liNextIndex >= liCarCount)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Can't find next unlock!\n", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1401));
        CgsDev::Assert::EndAssert();
        return 0;
    }

    for (;;)
    {
        if (liNextIndex < 0 || liNextIndex >= liCarCount)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "liCarIndex >= 0 && liCarIndex < miCarCount", CGS_ASSERT_LOCATION(KAC_BRNPROFILE_FILE, 1923));
            CgsDev::Assert::EndAssert();
        }
        const BrnProgression::CarData* lpProfileCar = lpProfile->GetCarData(liNextIndex);
        if (lpProfileCar == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpProfileCar", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1393));
            CgsDev::Assert::EndAssert();
        }
        if (IsThisCarInCurrentUnlockSequence(lpProfileCar))
            return lpProfileCar->GetId();

        liCarCount = lpProfile->GetCarCount();
        ++liNextIndex;
        if (liNextIndex >= liCarCount)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("Can't find next unlock!\n", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1401));
            CgsDev::Assert::EndAssert();
            return 0;
        }
    }
}

// ============================================================================
// RequestChangeCar (X360 0x82387990).
// Public entry: the player asked to switch to lCarId. Asserts a non-null id. If a car change is
// already in flight (state 7/8), it just stashes lCarId in mCacheDuringChangeCarId (it will be picked
// up when the in-flight swap finishes). Otherwise it records the chosen livery for the new car (from
// the live selection when in the car-mod screen, else from the saved per-car choice), stores lCarId
// as mDesiredCarId, resets the timer, and enters E_STATE_REQUEST_CAR_CHANGE so Update can drive the
// teleport/swap on the next ticks.
// ============================================================================
void CarSelectManager::RequestChangeCar(const CgsID& lCarId)
{
    if (lCarId == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lCardId != kCGSID_NULL", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 569));
        CgsDev::Assert::EndAssert();
    }

    if (meState == E_STATE_CHANGING_CAR || meState == E_STATE_STARTING_CHANGING_CAR)
    {
        // A swap is mid-flight; cache the request for when it completes.
        mCacheDuringChangeCarId = lCarId;
        return;
    }

    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 580));
        CgsDev::Assert::EndAssert();
    }

    if (mbInCarModScreen)   // X360 *(this + 0x73) == mbInCarModScreen
    {
        // In the car-mod screen the player picked a livery live -- persist that choice for lCarId.
        SaveChosenLiveryForCar(lCarId);
        mDesiredCarId = lCarId;
    }
    else
    {
        // Otherwise look up the livery already chosen for this base car.
        mDesiredCarId = lpProfile->GetChosenLiveryIdForBaseCar(lCarId);
    }

    mfStateTimer = 0.0f;
    meState      = E_STATE_REQUEST_CAR_CHANGE;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        // ARTIST 0x82387A7C..0x82387A9C: the id in hex-once mode through operator<<(u64) 0x82203EE8.
        *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: RequestChangeCar, CarId: "
                                   << CgsDev::E_PRINTMODE_HEXONCE << lCarId << "\n";
    }
}

// ============================================================================
// RequestStreamingForUnlock (X360 0x82387E78).
// Kick the world to start streaming the car(s) involved in the current unlock step, then latch
// mbWaitingForStreaming so the state machine stalls until StreamingFinished clears it.
//   * muUnlockCount == 0 -> nothing to stream (returns without arming the wait).
//   * muUnlockCount == 1 -> stream just the current unlock car (one CarUnlockEndAction entry).
//   * muUnlockCount >  1 -> stream the current unlock car AND prefetch the NEXT one (two entries; the
//                          second entry's "is-stream-target" flag layout differs for the prefetch).
// The action posted is CarUnlockEndAction (type 69 == 0x45, size 88 == 0x58).
// ============================================================================
void CarSelectManager::RequestStreamingForUnlock(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (muUnlockCount > 0)
    {
        // DWARF cpp:1424: the car being unlocked (extra-info 4, priority 0) and, when another car
        // follows, the next one (ARTIST 0x82387EA8..0x82387EF4; the flag/priority values are the
        // stores' immediates).
        GameStateModuleIO::CarSelectionRequestStreamingAction lRequestStreaming;
        lRequestStreaming.maCars[0]            = mCurrentCarToUnlock;
        lRequestStreaming.mauExtraInfoFlags[0] = 4;
        lRequestStreaming.maiPriorities[0]     = 0;
        lRequestStreaming.miCount              = 1;

        // DWARF cpp:1432.
        const u32 luCarsRemaining = muUnlockCount - 1;
        if (luCarsRemaining > 0)
        {
            lRequestStreaming.maCars[1] = GetNextUnlockCarID(mCurrentCarToUnlock);
            if (lRequestStreaming.maCars[1] != 0)
            {
                lRequestStreaming.miCount = 2;
                if (meState == E_STATE_TRANSITION_IN)
                {
                    lRequestStreaming.mauExtraInfoFlags[1] = 0;
                    lRequestStreaming.maiPriorities[1]     = 1;
                }
                else
                {
                    lRequestStreaming.mauExtraInfoFlags[1] = 4;
                    lRequestStreaming.maiPriorities[1]     = 0;
                }
            }
        }
        lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRequestStreaming),
                                GameStateModuleIO::E_ACTION_CAR_SELECTION_REQUEST_STREAMING,
                                static_cast<s32>(sizeof(lRequestStreaming)));
        mbWaitingForStreaming = true;
    }
}

// ============================================================================
// StreamingFinished (X360 0x82387210).
// Streaming-complete callback (the world finished streaming the car we asked for). Only acts when the
// completed car id (lActiveCarZeroId) matches the car we were waiting on (mDesiredCarId), or when no
// car was pending. Clears mbWaitingForStreaming. If we were EXITING and have held the exit long enough
// (timer >= 2.0), posts the CarSelectExitAction (type 74) to actually leave the junkyard.
// ============================================================================
void CarSelectManager::StreamingFinished(CgsID lActiveCarZeroId, GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (lActiveCarZeroId == mDesiredCarId || mDesiredCarId == 0)
    {
        if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
            *CgsDev::Log::gpDebugPrint << "=== CarSelectManager: StreamingFinished\n";

        mbWaitingForStreaming = false;   // X360 *(this + 0x58) = 0

        // The exit waits for the audio: 2.0 is the pooled literal ARTIST loads (0x82001D9C).
        if (meState == E_STATE_EXITING && mfStateTimer >= 2.0f)
        {
            // DWARF cpp:178.
            GameStateModuleIO::CarSelectWaitingForAudioAction lWaitingForAudioAction;
            lWaitingForAudioAction.mbWaiting = false;
            lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lWaitingForAudioAction),
                                    GameStateModuleIO::E_ACTION_CAR_SELECT_WAITING_FOR_AUDIO,
                                    static_cast<s32>(sizeof(lWaitingForAudioAction)));
        }
    }
}

// ============================================================================
// SpawnInStartCar (X360 0x82387CD8).
// Place the player's start car at the chosen "drive-in" spawn location when the junkyard transition
// begins. Asserts maSpawnLocations[0] is set (the X360 reads the +0x2C slot == maSpawnLocations[1] in
// the asm: this+0x2C; see note), marks streaming pending, resolves the start car's CarData (for the
// deform amount), then posts three game actions:
//   - ResetPlayerCarAction      (type 0,  80B): the start car id + deform + in-car-select flags.
//   - CarSelectionChangedAction (type 64, 64B): the start car id + spawn-location transform + type bit.
//   - CarSelectionDropInAction  (type 65, 16B): the start car id + spawn-location type bit.
// The spawn-location transform is read from the chosen SpawnLocation; its type bit (a single bool ==
// "is this a generic / drive-in spawn") is derived from SpawnLocation::GetType().
// ============================================================================
void CarSelectManager::SpawnInStartCar(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // DWARF cpp:1280: the start car always drops at the left car-select spawn point.
    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT];
    CGS_ASSERT_AT(lpSpawnLocation, "lpSpawnLocation", KAC_CSM_FILE, 1281);
    meLastSpawnLocationType = BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT;

    // DWARF cpp:1285 (no null test: ARTIST reads +0xC straight off the result, 0x82387D54).
    const BrnProgression::CarData* lpCarData = GetProfileCarData(mStartCarId);

    // DWARF cpp:1288.
    GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
    lResetPlayerCarAction.Construct();
    lResetPlayerCarAction.SetLocation(lpSpawnLocation->GetPosition(), lpSpawnLocation->GetDirection());
    lResetPlayerCarAction.SetDeformAmount(lpCarData->GetUnlockDeformationAmount());
    lResetPlayerCarAction.SetCarData(mStartCarId, 0);
    lResetPlayerCarAction.SetInCarSelect(true);
    lResetPlayerCarAction.SetCarSelectType(GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP);
    lResetPlayerCarAction.SetCarSelectDontStreamAudio(false);
    lResetPlayerCarAction.SetDeformationResetType(1);
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                            GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                            static_cast<s32>(sizeof(lResetPlayerCarAction)));

    // DWARF cpp:1301.
    GameStateModuleIO::CarSelectionChangedAction lCarSelectionChangedAction;
    lCarSelectionChangedAction.mJunkyardId          = mJunkyardId;
    lCarSelectionChangedAction.mPosition            = lpSpawnLocation->GetPosition();
    lCarSelectionChangedAction.mDirection           = lpSpawnLocation->GetDirection();
    lCarSelectionChangedAction.mbIsLeft             = lpSpawnLocation->GetType() == BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT;
    lCarSelectionChangedAction.mbPlayCraneDropAudio = false;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectionChangedAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED,
                            static_cast<s32>(sizeof(lCarSelectionChangedAction)));

    // DWARF cpp:1312.
    GameStateModuleIO::CarSelectionChangedDropInAction lCarSelectDropInAction;
    lCarSelectDropInAction.mCarId   = mStartCarId;
    lCarSelectDropInAction.mbIsLeft = lpSpawnLocation->GetType() == BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectDropInAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED_DROPIN,
                            static_cast<s32>(sizeof(lCarSelectDropInAction)));
}

// ============================================================================
// TeleportCurrentVehicle (X360 0x82392EF0).
// Move the player's car to the spawn location for the FINISHED swap. Indexes the spawn-location array
// by meLastSpawnLocationType (this+0x3C; maSpawnLocations[type]) -- the slot the previous flow recorded
// -- and asserts it is set. Resolves the desired car's CarData (deform). Posts three actions:
//   - ResetPlayerCarAction      (type 0,  80B): desired car id + deform + the in-car-mod flag.
//   - CarSelectionDropInAction  (type 65, 16B): desired car id + spawn-location type bit.
//   - CarUnlockAction           (type 79,  8B): the desired car's colour + palette indices.
// ============================================================================
void CarSelectManager::TeleportCurrentVehicle(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    // DWARF cpp:1472: the spawn point the last drop used.
    const BrnTrigger::SpawnLocation* lpSpawnLocation = maSpawnLocations[meLastSpawnLocationType];
    CGS_ASSERT_AT(lpSpawnLocation, "lpSpawnLocation", KAC_CSM_FILE, 1473);

    const BrnProgression::CarData* lpCarData = GetProfileCarData(mDesiredCarId);
    CGS_ASSERT_AT(lpCarData, "lpCarData", KAC_CSM_FILE, 1478);

    // DWARF cpp:1480: the car drops in unless the livery screen is up (ARTIST cntlzw of
    // mbInCarModScreen, 0x82392F80..9C).
    GameStateModuleIO::ResetPlayerCarAction::CarSelectType leCarSelectType;
    if (mbInCarModScreen)
        leCarSelectType = GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DONT_DROP;
    else
        leCarSelectType = GameStateModuleIO::ResetPlayerCarAction::E_CAR_SELECT_DROP_NORMAL;

    // DWARF cpp:1491.
    GameStateModuleIO::ResetPlayerCarAction lResetPlayerCarAction;
    lResetPlayerCarAction.Construct();
    lResetPlayerCarAction.SetCarSelectType(leCarSelectType);
    lResetPlayerCarAction.SetLocation(lpSpawnLocation->GetPosition(), lpSpawnLocation->GetDirection());
    lResetPlayerCarAction.SetCarData(mDesiredCarId, 0);
    lResetPlayerCarAction.SetDeformAmount(lpCarData->GetUnlockDeformationAmount());
    lResetPlayerCarAction.SetDeformationResetType(1);
    lResetPlayerCarAction.SetInCarSelect(true);
    lResetPlayerCarAction.SetCarSelectDontStreamAudio(true);
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lResetPlayerCarAction),
                            GameStateModuleIO::E_ACTION_RESET_PLAYER_CAR,
                            static_cast<s32>(sizeof(lResetPlayerCarAction)));

    // DWARF cpp:1504.
    GameStateModuleIO::CarSelectionChangedDropInAction lCarSelectDropInAction;
    lCarSelectDropInAction.mbIsLeft = lpSpawnLocation->GetType() == BrnTrigger::SpawnLocation::E_TYPE_CAR_SELECT_LEFT;
    lCarSelectDropInAction.mCarId   = mDesiredCarId;
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCarSelectDropInAction),
                            GameStateModuleIO::E_ACTION_CAR_SELECTION_CHANGED_DROPIN,
                            static_cast<s32>(sizeof(lCarSelectDropInAction)));

    UpdateCarColour(mDesiredCarId, lpActionQueue);
}

// ============================================================================
// SetupSpawnLocations (X360 0x8236EA98).
// Build the junkyard's spawn-location array from the loaded track TriggerData: clear all five slots,
// then for every SpawnLocation whose junkyard id matches mJunkyardId, file it by type:
//   * type 1..3 -> maSpawnLocations[type]  (asserts the slot is empty; one of each).
//   * type 0    -> the two "generic drive-in" slots: the first found goes to maSpawnLocations[0], the
//                  second to maSpawnLocations[4].
// Then, when the player has a valid last-active race car, it measures which of the two type-0 slots is
// nearer the car's last position and swaps them so maSpawnLocations[0] is the nearest (the player
// drives back in next to where they were). Finally asserts all five slots are filled.
// ============================================================================
void CarSelectManager::SetupSpawnLocations()
{
    if (mJunkyardId == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mJunkyardId != kCGSID_NULL", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1063));
        CgsDev::Assert::EndAssert();
    }
    if (mpTriggerQueryManager == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mpTriggerQueryManager", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1064));
        CgsDev::Assert::EndAssert();
    }

    for (u32 lu = 0; lu < KU_CARSELECT_SPAWNLOCATION_COUNT; ++lu)
        maSpawnLocations[lu] = 0;

    const BrnTrigger::TriggerData* lpTriggerData = mpTriggerQueryManager->GetTriggerData();
    if (lpTriggerData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpTriggerData", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1082));
        CgsDev::Assert::EndAssert();
    }

    const s32 liSpawnCount = lpTriggerData->GetSpawnLocationCount();
    for (s32 li = 0; li < liSpawnCount; ++li)
    {
        const BrnTrigger::SpawnLocation* lpSpawnLocation = lpTriggerData->GetSpawnLocation(li);
        if (lpSpawnLocation == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpSpawnLocation", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1088));
            CgsDev::Assert::EndAssert();
        }

        if (lpSpawnLocation->GetJunkyardId() == mJunkyardId)
        {
            const u32 luType = static_cast<u32>(lpSpawnLocation->GetType());
            if (luType >= 4u)   // E_TYPE_COUNT == 4
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("muType < E_TYPE_COUNT", CGS_ASSERT_LOCATION(KAC_SPAWNLOC_FILE, 152));
                CgsDev::Assert::EndAssert();
            }

            if (luType != 0)
            {
                // type 1..3 -> the matching dedicated slot (exactly one of each).
                const u32 luSlot = luType;
                if (maSpawnLocations[luSlot] != 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "maSpawnLocations[lSpawnLocationType] == NULL", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1110));
                    CgsDev::Assert::EndAssert();
                }
                maSpawnLocations[luSlot] = lpSpawnLocation;
            }
            else if (maSpawnLocations[0] != 0)
            {
                // second generic drive-in slot -> index 4.
                maSpawnLocations[4] = lpSpawnLocation;
            }
            else
            {
                // first generic drive-in slot -> index 0.
                maSpawnLocations[0] = lpSpawnLocation;
            }
        }
    }

    // ARTIST 0x8236ECB4..0x8236ED6C. Keep the authored slot order unless
    // the active player's heading opposes the second generic spawn.
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpLastActiveCar =
        mpGameStateModule->GetLastActiveRaceCarInterface();
    if (lpLastActiveCar == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpLastActiveRaceCarInterface", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1119));
        CgsDev::Assert::EndAssert();
    }

    if (lpLastActiveCar->IsPlayerCarActive())
    {
        const Vector3 lvSpawnDirection = maSpawnLocations[4]->mDirection;
        const Vector3 lvPlayerDirection = lpLastActiveCar->GetPlayerDirection();
        if (rw::math::vpu::Dot(lvSpawnDirection, lvPlayerDirection) < 0.0f)
        {
            const BrnTrigger::SpawnLocation* lpFirst = maSpawnLocations[0];
            maSpawnLocations[0] = maSpawnLocations[4];
            maSpawnLocations[4] = lpFirst;
        }
    }

    // --- All five slots must now be filled --------------------------------------------------------
    for (u32 lu = 0; lu < KU_CARSELECT_SPAWNLOCATION_COUNT; ++lu)
    {
        if (maSpawnLocations[lu] == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(
                "The junkyard does not have all of the spawn locations setup.", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1140));
            CgsDev::Assert::EndAssert();
        }
    }
}

// ============================================================================
// DEBUG_UnlockCarsForTesting (X360 0x82387F18).
// Cheat: force-add cars to the player profile for testing. Walks the whole VehicleList; for each
// vehicle that is a trophy/special car (VehicleListEntry +0x94 bit0) and that the profile does NOT yet
// own, it calls ProgressionManager::AddCar. Two passes, each capped at 3 newly-counted cars:
//   * Pass 1 == trophy pass (entered when mbDEBUG_UnlockTrophyCarsForTesting != 0): AddCar(id, 0), no
//     deform stamp.
//   * Pass 2 == shutdown pass (entered after the cap iff mbDEBUG_UnlockShutdownCarsForTesting != 0):
//     AddCar(id, 3) AND stamp a 0.85 deform onto the new CarData (CarData +0x0C) so it reads as "won".
// The X360 carries a single byte `v4` that is BOTH the pass selector and the AddCar-type selector:
//   v4 == 0  -> trophy pass     -> AddCar(id, 0), no deform.
//   v4 != 0  -> shutdown pass   -> AddCar(id, 3), 0.85 deform.
// v4 starts as (mbDEBUG_UnlockTrophyCarsForTesting == 0); when the cap is hit in the trophy pass it is
// reloaded from mbDEBUG_UnlockShutdownCarsForTesting (which, if armed, is non-zero -> the shutdown
// pass). An already-owned trophy car that is NOT hidden (CarData +0x0A == 0) still counts toward the
// cap without being re-added.
// ============================================================================
void CarSelectManager::DEBUG_UnlockCarsForTesting()
{
    BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();
    if (lpProfile == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpProfile", CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1681));
        CgsDev::Assert::EndAssert();
    }
    if (!mbDEBUG_UnlockTrophyCarsForTesting && !mbDEBUG_UnlockShutdownCarsForTesting)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mbDEBUG_UnlockTrophyCarsForTesting || mbDEBUG_UnlockShutdownCarsForTesting",
            CGS_ASSERT_LOCATION(KAC_CSM_FILE, 1682));
        CgsDev::Assert::EndAssert();
    }

    // X360 v4: 0 == trophy pass (AddCar type 0), non-zero == shutdown pass (AddCar type 3 + deform).
    u8  lucPass = (mbDEBUG_UnlockTrophyCarsForTesting == 0) ? 1 : 0;
    u32 luAddedThisPass = 0;

    const s32 liVehicleCount = mpVehicleList->GetVehicleCount();
    for (s32 liVehicle = 0; liVehicle < liVehicleCount; ++liVehicle)
    {
        const BrnResource::VehicleListEntry* lpVehicleData = mpVehicleList->GetVehicleData(liVehicle);
        const CgsID lCarId = lpVehicleData->GetId();   // X360 *(VehicleData) (the leading car id)

        // Does the profile already own this car?
        const BrnProgression::CarData* lpOwned = 0;
        const s32 liCarCount = lpProfile->GetCarCount();
        for (s32 li = 0; li < liCarCount; ++li)
        {
            const BrnProgression::CarData* lpCandidate = lpProfile->GetCarData(li);
            if (lpCandidate->GetId() == lCarId)
            {
                lpOwned = lpCandidate;
                break;
            }
        }

        // Only trophy/special cars are eligible (the +0x94 bit0 flag).
        if (lpVehicleData->IsTrophyCar())
        {
            if (lpOwned == 0)
            {
                // Not owned -> add it. Pass selector chooses the unlock type + deform stamp.
                const s32 leUnlockType = (lucPass != 0) ? 3 : 0;
                BrnProgression::CarData* lpNewCar = mpProgressionManager->AddCar(lCarId, leUnlockType);
                if (lucPass != 0)
                    lpNewCar->SetUnlockDeformationAmount(KF_DEBUG_TROPHY_CAR_DEFORM);   // CarData +0x0C = 0.85
                ++luAddedThisPass;
            }
            else if (!lpOwned->WasUnlockSequenceAlreadyShown())   // X360 *(lpOwned + 0x0A) == 0 (not hidden)
            {
                // Already owned and visible -> still counts toward the cap, no re-add.
                ++luAddedThisPass;
            }
        }

        // Cap each pass at 3. When the trophy pass tops out, switch to the shutdown pass iff armed.
        if (luAddedThisPass >= 3)
        {
            if (lucPass != 0)
                return;   // shutdown pass done
            lucPass         = (mbDEBUG_UnlockShutdownCarsForTesting != 0) ? 1 : 0;
            luAddedThisPass = 0;
            if (lucPass == 0)
                return;   // no shutdown pass requested
        }
    }
}

}

// ARTIST ProcessGameEvents case 83 inlines CarSelectManager::OnCarUnlockTickerComplete.
// The fade-out must finish before UpdateUnlockState advances to the next car.
void BrnGameState::CarSelectManager::OnCarUnlockTickerComplete()
{
    mbCurrentCarTickerVisible = false;
    mfCarUnlockFadedOutTargetTime = mfStateTimer + 2.0f;
}

#undef CGS_ARTIST_FILE
