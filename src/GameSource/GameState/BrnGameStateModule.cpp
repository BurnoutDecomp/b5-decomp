#include "GameSource/GameState/BrnGameStateModule.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
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
#include "GameSource/GameState/BrnGameEvents.h"                         // [returning-player wave] E_GUI_HAS_STARTED_GAME (the case-78 trigger)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<1536,16> (the carry-queue walk)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"    // [gateui] the complete type mpTrainingManager is newed as (see its FLAG in the header)
#include "SharedClasses/DataLists/VehicleList.h"                        // BrnResource::VehicleList (GetVehicleIndex / GetVehicleData)
#include "SharedClasses/DataLists/WheelList.h"                          // BrnResource::WheelList (GetWheelCount -- Prepare's list diagnostic)
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // BrnResource::VehicleListEntry (parent id / livery + car type / stats)
#include "SharedClasses/Progression/BrnProgressionData.h"               // BrnProgression::ProgressionData::FindCarOpponentSet
#include "SharedClasses/Progression/BrnOpponentData.h"                  // BrnProgression::CarOpponentSet (opponent walk)
#include "GameShared/GameClasses/Containers/CgsArray.h"                 // CgsContainers::Array<s64,7> (opponent payload)
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"       // RequestInterface<3072>::GetVehicleList/GetWheelList
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"             // GameDataAssetEvent (the list replies)
#include "SharedClasses/Trigger/BrnTriggerData.h"                       // BrnTrigger::TriggerData (generic-region table)
#include "SharedClasses/Trigger/BrnGenericRegion.h"                     // BrnTrigger::GenericRegion (E_TYPE_JUNK_YARD)
#include "SharedClasses/Trigger/BrnRegion.h"                            // BrnTrigger::BoxRegion::GetPosition
#include "SharedClasses/Trigger/BrnSpawnLocation.h"                     // BrnTrigger::SpawnLocation (cross-table check)
#include <cmath>                                                        // std::sqrt (FindNearestJunkyardID)
#include <stdlib.h>                                                     // getenv (the [showtime-crash] witness)
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"                 // BrnWorld::PropEntityID + BrnWorld::E_ENTITYTYPE_* (ProcessContacts' prop leg)

namespace BrnGameState
{
// The verbatim X360-baked source path every assert in this TU references.
static const char* const KAC_GSM_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/BrnGameStateModule.cpp";
static const char* const KAC_OPPONENTDATA_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\sharedclasses\\progression\\BrnOpponentData.h";

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
// The console's GameStateModule is a CgsModule::ModuleSingleBuffered, so its
// GameStateModuleIO::OutputBuffer is the DataStructure the base allocates from the module's
// own DataBuffer inside Prepare() -> CreateOutputDataStructure(). ⚠️ FLAG (PC bring-up seam):
// nothing on PC calls this module's Prepare(), so that allocation point never runs. The
// buffer is newed here instead and freed in Destruct(); the buffer TYPE, its Construct
// (X360 0x82382940) and its accessors are the real console ones -- only the allocation SITE
// moves. DELETE-WHEN the module's Prepare()/CreateOutputDataStructure() path is real.
//
// Constructing it is what makes the game-state -> director game-action route exist at all:
// BrnGameModule::BridgeGameStateToDirector @0x823CD170 Appends this buffer's +0x04 queue into
// the director input buffer's queue every frame, and MainDirector::ProcessInputQueue drains it.
// ----------------------------------------------------------------------------
void GameStateModule::Construct()
{
    CgsModule::ModuleSingleBuffered::Construct();

    if (mpOutputBuffer == 0)
    {
        mpOutputBuffer = new GameStateModuleIO::OutputBuffer();
        mpOutputBuffer->Construct();
    }

    // ⭐⭐ [D2 gesture-sink] THE PRE-WORLD CONTROLLER SINK. Allocated here for exactly the
    // reason mpOutputBuffer is, and with the same shape of FLAG -- the console stages this
    // buffer per frame out of the update IOBufferStack
    // (BrnGameModule::DoUpdate_GameStatePreWorld @0x823EE0E8:
    //  `CreateIOBuffer<GameStateModuleIO::PreWorldInputBuffer>(stack, &buf, "GameStatePreWorld")`
    //  ... `DestroyIOBuffer(stack, &buf)`), and nothing on PC runs that entry point. The full
    // attestation, the refuted "module + 0x2BE8" premise and the DELETE-WHEN live on
    // GameStateModule::GetPreWorldInputBuffer() in the header.
    //
    // ⚠️ `new T()` -- the parenthesised form is load-bearing, not decoration. PreWorldInputBuffer
    // has no user-provided constructor, so value-initialisation ZERO-INITIALISES the whole object
    // before the implicit default ctor runs. Without it ControllerInput::mbRaceModePressed (and
    // its twenty siblings) would be indeterminate for every read taken before the first
    // BridgeControllerToGameState of the session -- a "the gesture fired on boot" bug that would
    // reproduce about half the time.
    if (mpPreWorldInputBuffer == 0)
    {
        mpPreWorldInputBuffer = new GameStateModuleIO::PreWorldInputBuffer();
        mpPreWorldInputBuffer->Construct();   // CgsModule::IOBuffer::Construct -- raises eStatusConstructed

        // ⓘ THE never-Constructed-queue TRAP, PAID UP FRONT. The console's CreateIOBuffer path
        // Constructs the buffer's embedded VariableEventQueue<1536,16> at +0x4C; a queue that is
        // only zero-filled has miFirstEventOffset == 0 and mbIsConstructed == false, and the first
        // AddEvent on it asserts "Not Constructed" (or, worse for a queue that has been Clear()ed
        // instead, writes at an unaligned head). DoUpdate_GameStatePreWorld posts onto exactly
        // this queue -- events 33 (the stream-stall pair) and 8 -- so it WILL have a producer the
        // moment that entry point lands. Constructed through the committed write-locked accessor
        // rather than by poking the member, so the lock contract is honoured here too.
        mpPreWorldInputBuffer->LockForWrite();
        mpPreWorldInputBuffer->GetGameEventQueue()->Construct();
        mpPreWorldInputBuffer->UnlockForWrite();
    }

    // ⭐ X360 0x82380388 (this function) is the console's ONLY caller of
    // CarSelectManager::Construct @0x823564D0:
    //     BrnGameState::CarSelectManager::Construct(a1 + 183712, a1 + 42320, a1, a1 + 47920)
    // i.e. (&mCarSelectManager, &mTriggerQueryManager, this, &mProgressionManager) -- the three
    // owning pointers the junkyard FSM keeps for its whole life. Verbatim, same arguments.
    //
    // ⚠️ ORDER DEVIATION (harmless, and stated rather than hidden): the console runs this AFTER
    // TriggerQueryManager::Construct @0x82364BF0 and ProgressionManager::Construct, so the two
    // subobjects are already initialised when their addresses are taken. Neither of those has a
    // linked body on PC yet (BrnTriggerQueryManager.cpp is unmounted -- it costs 13 unresolved
    // externals, all from UpdateTriggers/ProcessPlayerTriggers; ProgressionManager::Construct has
    // no body in the tree at all), so they cannot be called here. CarSelectManager::Construct only
    // STORES the two pointers -- it never dereferences either -- so taking the address of a
    // not-yet-constructed subobject is well-defined and the stored value is already final.
    // DELETE-WHEN those two Constructs land: they must then run BEFORE this line.
    mCarSelectManager.Construct(&mTriggerQueryManager, this, &mProgressionManager);

    // ⭐⭐ [gateui] THE STUNT SUB-OBJECT, wired with the console's own six arguments. X360
    // 0x82380388, the line immediately after OnlineCarSelectManager::Construct:
    //     BrnGameState::StuntManager::Construct(a1 + 183952,   // &mStuntManager
    //                                           a1 + 47920,    // &mProgressionManager
    //                                           a1 + 42320,    // &mTriggerQueryManager
    //                                           a1 + 4128,     // &mModeManager
    //                                           a1 + 46640,    // the TrainingManager
    //                                           a1);           // this
    // Verbatim, same order. Construct is the manager's ONLY initialiser -- it seeds the "last
    // latched element" set to its empty state (muLastZoneId/muLastPropId = -1,
    // mpLastStuntOrSmashElement = NULL, meLastStuntElementType = COUNT), Constructs the district
    // map's receiver queue and registers the debug component -- so without it OnPropHit would
    // latch into, and Prepare would stream through, uninitialised state.
    //
    // ⚠️ THE TRAINING-MANAGER ARGUMENT IS A HEAP OBJECT HERE, NOT A SUB-OBJECT -- see
    // mpTrainingManager's FLAG in the header for the include cycle that forces it. It is
    // allocated FIRST so the pointer StuntManager stores is final and non-null: passing 0 would
    // fire the console's own `mpTrainingManager` assert (BrnStuntManager.cpp:71) EVERY BOOT.
    // (the old "TrainingManager::Construct is deliberately NOT called / nothing dereferences
    //  it" block is RETIRED -- its own DELETE-WHEN is paid below. The console's Construct is
    //  indeed absent from the 0x82380388 list; its PS3 twin 0x241DE0 shows what it seeds and
    //  the X360 inlines those stores at the manager's real initialisation site.)
    if (mpTrainingManager == 0)
    {
        mpTrainingManager = new TrainingManager();
    }
    // ⭐ [tut-ticker] 2026-08-24: TrainingManager::Construct HAS a caller now (the DELETE-WHEN
    // above is paid): the manager is Constructed here, before mStuntManager.Construct, so the
    // pointer StuntManager stores refers to a fully-seeded object. Body is the PS3-attested
    // TrainingManager::Construct (DecFIGS 0x241DE0; the X360 inlines it -- no export exists).
    mpTrainingManager->Construct(&mProgressionManager, this);

    // ⭐ [tut-ticker] the console's ModeManager::Construct runs from this Construct
    // (@0x82340008's sole caller); its inter-mode seed stores are extracted --
    // meCurrentGameModeType = E_MODE_NONE (-1) is load-bearing, see the ModeManager banner.
    // [takedown wave 2026-09-02] X360 Construct @0x82380388 inlines TakedownManager::Construct
    // (the two manager pointers into gsm+568, its back-pointer at gsm+1256) BEFORE the
    // TriggerQueryManager / RoadRules / DriveThru constructs; the queue constructs follow at
    // gsm+249936 / +250272 / +250800. Both halves in GameStateModule_gTD_00.cpp.
    ConstructTakedownBringUp();

    mModeManager.ConstructInterModeStateBringUp(this);

    mStuntManager.Construct(&mProgressionManager, &mTriggerQueryManager, &mModeManager,
                            mpTrainingManager, this);

    // ⭐ [drive-thru wave 2026-08-27] THE DRIVE-THRU SUB-OBJECT, wired with the console's own five
    // arguments. X360 0x82380388 line 116, immediately after RoadRulesManager::Construct:
    //     BrnGameState::DriveThruManager::Construct(a1 + 44240,   // &mDriveThruManager
    //                                               a1 + 183712,  // &mCarSelectManager
    //                                               a1 + 46640,   // the TrainingManager
    //                                               a1 + 4128,    // &mModeManager
    //                                               a1,           // this
    //                                               a1 + 47920);  // &mProgressionManager
    // Verbatim, same argument order. Construct is the manager's ONLY initialiser: it seeds all 46
    // DriveThruTriggerData timers to the -1.0 "inactive" sentinel, nulls their region pointers,
    // zeroes the six per-type totals and sets meDriveThruCache / meDiscoveredDriveThruType to the
    // E_TYPE_COUNT (32) "nothing cached" sentinel. Without it Update's per-frame sweep reads 46
    // uninitialised timers and its `meDriveThruCache != E_TYPE_COUNT` gate is a coin flip
    // [[valid-pointer-invalid-object]].
    // ⓘ The TrainingManager argument is the heap object, not a sub-object, for the same include
    // cycle documented at mStuntManager.Construct above; it is allocated further up, so the
    // pointer stored here is final and non-null.
    mDriveThruManager.Construct(&mCarSelectManager, mpTrainingManager, &mModeManager,
                                this, &mProgressionManager);

    // ⭐ [gateui] THE GAME-EVENT CARRY QUEUE (X360 this+248384). The console Constructs it right
    // here: `CgsModule::VariableEventQueue<1536,16>::Construct(a1 + 248384)` @0x82380388, in the
    // block of queue Constructs near the end of the body. This is the never-Constructed-queue
    // trap paid up front -- PostWorldUpdate Appends into it and PreWorldUpdate Clears it, and
    // Clear() does NOT bind a buffer.
    mGameEventCarryQueue.Construct();

    // ⭐ [showtime score wave 2026-08-29] THE SHOWTIME CRASH HAND-OFF STACK (X360 this+284488).
    // Same never-Constructed-container trap, and this container makes it LOUD rather than latent:
    // CgsContainers::Stack's uninitialised sentinel is miLength == 0x7FFFFFFF, and every Push /
    // Peek / Pop / IsFull opens with CGS_ASSERT(miLength != KI_STACK_UNCONSTRUCTED,
    // "Stack used before Construct/Clear was called"). ProcessContacts calls IsFull() on the very
    // first showtime frame, so without this the assert fires immediately -- and on a pool-carved
    // module the garbage length would then index maData out of range.
    // ⓘ The console does this in the same Construct @0x82380388 block; UpdateShowtimeMode's own
    // "Stack used before Construct/Clear was called" FireAssert (BrnGameStateModule.cpp:1611
    // region, CgsStack.h:177) is the X360 witness that the container is this type.
    mShowtimePendingTrafficIndexStack.Construct();

    // ⭐ [gateui r4] CONSTRUCT INSURANCE (verify_r3_fix3bridge NOTE-2). The round-3 carry-queue
    // gate narrowed the producer to E_MGS_IN_GAME, so the FIRST in-game pre-world leg now reads
    // an mLastActiveRaceCarInterface that has never been written (round 2 refreshed it on every
    // loading sub-step). Nothing else clears it either: the interface's default ctor is an
    // empty user-provided `{}`, so mePlayerActiveRaceCarIndex / mbIsPlayerCarActive have no
    // initialiser. It is safe TODAY only by accident -- BrnMain.cpp:45 is
    // `static BrnGame::BrnGameModule gGameModule;`, i.e. static storage, i.e. zero-init, i.e.
    // IsPlayerCarActive() false for exactly one sub-step. The day that allocation moves to the
    // boot allocator (which BrnMain.cpp:23 names as the console shape) a garbage-true
    // IsPlayerCarActive() sends GetPlayerRaceCarState() into maRaceCarStates[garbage].
    // Clear() is the interface's OWN X360 body (0x8227D550) and lands exactly the state the
    // readers expect: index -1, engine state COUNT, mbIsPlayerCarActive false.
    mLastActiveRaceCarInterface.Clear();

    // ⭐ [stuntrace waveB agent 9] SAME INSURANCE FOR THE GLOBAL SNAPSHOT, and for the same reason.
    // The console pairs the two Clears back to back in GameStateModule::ClearData @0x8236B3A8
    // (`RCEntityActiveRaceCarOutputInterface::Clear(a1 + 235488);
    //   RCEntityGlobalRaceCarOutputInterface::Clear(a1 + 245968);`), and ClearData is not
    // reconstructed here either. Without this, ModeManager::GlobalToActiveRaceCarIndex (which is
    // the interface's own maeActiveRaceCarIndices lookup) would read indeterminate slot indices and
    // hand a garbage active index to the checkpoint and results paths. Clear() lands the console's
    // own "no data" state: every active-index slot E_ACTIVE_RACE_CAR_INDEX_INVALID, so the readers
    // fire the console's own range asserts instead of indexing on garbage.
    // DELETE-WHEN PostWorldUpdate's snapshot leg lands (it XMemCpy's both interfaces).
    mLastGlobalRaceCarInterface.Clear();

    // (DeveloperChallengeManager::Construct @0x82380794 -- see the call after WireOwnerPointers below;
    //  the "BrnModeManager.cpp unmounted" reason recorded here was stale: ModeManager::GetScoringSystem
    //  is bodied in ModeManager_gUI_00.cpp, mounted.)

    // ⚠️⚠️ THE TWO PREPARE2 SUB-OBJECTS ARE NOT Construct()ed HERE (2026-08-11), and the reason in
    // BOTH cases is a MEASURED LINK COST -- not a missing body. The X360 Construct @0x82380388 runs
    //
    //   BrnGameState::AchievementManagerBase::Construct(a1 + 181680,   // &mAchievementManager
    //                                                   a1 + 47920,    // &mProgressionManager
    //                                                   a1 + 284520,   // &mStreetManager
    //                                                   a1 + 7632,     // mModeManager.GetScoringSystem()
    //                                                   a1);           // this
    //   BrnGameState::StreetManager::Construct(a1 + 284520,            // &mStreetManager
    //                                          a1,                     // this
    //                                          a1 + 47920,             // &mProgressionManager
    //                                          a1 + 183592);           // &mRoadRulesManager
    //
    // (`a1 + 7632` is the ScoringSystem EMBEDDED IN mModeManager, not a module member of its own:
    // mModeManager sits at a1 + 4128 and BrnModeManager.h:303 puts mScoringSystem at ModeManager
    // +0xDB0 == 3504; 4128 + 3504 == 7632 exactly. So the argument is mModeManager.GetScoringSystem().)
    //
    // What blocks each call, MEASURED with `cl /c` + dumpbin /SYMBOLS over the candidate mount set:
    //   * AchievementManagerBase::Construct lives in BrnGameStateAchievementManagerBase.cpp, and
    //     mounting that TU costs EIGHT unresolved externals with NO definition anywhere in the
    //     tree -- ScoringSystem::GetPlayerScore / GetPlayerModeCrashes / GetPlayerModeTakedowns /
    //     GetNewlyWreckedCarCount / GetNumberOfTakedownsAgainst and ProgressionManager::
    //     GetCarChallengeWinCount / GetCollectedStuntElementCount / GetProfileTotalTakedowns
    //     (pulled in by the base's gameplay-event hooks). ⚠️ VERIFIED, because the tree's folklore
    //     says otherwise: /Gy + /OPT:REF does NOT excuse those. A minimal repro (one COMDAT calling
    //     an undefined symbol, never referenced, linked with /OPT:REF) still fails LNK2019 -- the
    //     linker resolves symbols before it discards. So "nothing calls it" is NOT a link defence.
    //   * StreetManager::Construct's FIRST statement is mStreetManagerDebugComponent.Construct(this),
    //     which emits StreetManagerDebugComponent's vtable; that vtable hard-references the
    //     component's virtual Update/OnActivate/RenderHUD, and those pull in ~15 still-unhomed
    //     StreetManager / ScoringSystem / ProgressionManager / OutputBuffer symbols.
    //
    // What stands in, so neither subobject is INDETERMINATE (the thing that would actually bite):
    // the exact values each Construct writes are carried as in-class initialisers on the members
    // that the wired path reads -- the achievement manager's four back-pointers
    // (BrnGameStateAchievementManagerBase.h) and the street manager's three stage words
    // (BrnGameStateStreetManager.h). Both are FLAGGED at their declarations.
    //
    // ⛔⛔ AND THE SENTENCE THAT USED TO END THAT PARAGRAPH -- "Nothing on the wired path reads
    // any other member of either subobject" -- WAS TRUE WHEN WRITTEN AND WAS FALSIFIED BY THE
    // VERY NEXT WAVE. Un-parking Prepare2's SetupParRivals leg (2026-08-11) put
    // StreetManager::mpProgressionManager on the wired path, and StreetManager::Construct is its
    // ONLY writer. First boot after the un-park: EXCEPTION_ACCESS_VIOLATION reading 0x1D9E8 in
    // ProgressionManager::GetProgressionData <- StreetManager::SetupParRivals. 0x1D9E8 == 121320
    // is exactly the host offsetof(ProgressionManager, mpProgressionData) -- measured with a
    // compile-time probe against this build's headers -- i.e. a member read off a NULL base.
    // ⚠️ THE LESSON, for whoever un-parks the next call into a not-Construct()ed subobject:
    // "the stage words are seeded" is NOT the same claim as "every member this path reads has a
    // writer". Enumerate the `this->mp*` reads of the code you are un-parking and check each one.
    //
    // WIRED BELOW at the console's own call position (X360 0x82380768, immediately after
    // AchievementManagerBase::Construct), with the console's own two available values, through a
    // NAMED subset helper -- not a partial `Construct`. Its full contract, its console
    // attestation and its deliberate omission (the third owner pointer, whose RoadRulesManager
    // member does not exist on PC yet) are documented at its declaration in
    // BrnGameStateStreetManager.h. This is the same shape as mCarSelectManager.Construct above:
    // pointer stores only, into a subobject that is not otherwise constructed yet, which is
    // well-defined because nothing dereferences the stored values until Prepare2.
    mStreetManager.WireOwnerPointers(this, &mProgressionManager);

    // ⭐ 2026-09-03 (aiwave, lane P1): AchievementManagerBase.cpp is MOUNTED -- the eight externals the
    // bat named are bodied (BrnScoringSystem_Accessors2.cpp / _Queries.cpp, BrnProgressionManager_Rivals.cpp),
    // so the console's call goes back on its line (Construct @0x82380388: r4 = mProgressionManager,
    // r5 = mStreetManager, r6 = mModeManager's ScoringSystem, r7 = this).
    mAchievementManager.Construct(&mProgressionManager, &mStreetManager,
                                  mModeManager.GetScoringSystem(), this);

    // ⭐ 2026-09-03 (aiwave, lanes P1+P3): DeveloperChallengeManager::Construct @0x82380794 sits right after
    // GameStateImageManagerBase::Construct @0x82380778 and before ClearData @0x823807A8 (r4 = r28
    // mProgressionManager, r5 = r25 mStreetManager, r6 = r23 the ScoringSystem, r7 = r31 this). The TU is
    // mounted now (its five externals landed), so the console's call goes back on its line.
    mDeveloperChallengeManager.Construct(&mProgressionManager, &mStreetManager,
                                         mModeManager.GetScoringSystem(), this);

    // DELETE-WHEN those two closures land (StreetManager::Construct additionally needs a
    // RoadRulesManager member, DWARF :229 / X360 this+183592, for its third argument) -- and
    // then the WireOwnerPointers call above, the helper itself, and the `= 0` initialisers
    // backing it all go with them:
    //     (mAchievementManager.Construct -- DONE above, 2026-09-03)
    //     mStreetManager.Construct(this, &mProgressionManager, &mRoadRulesManager);
}

void GameStateModule::Destruct()
{
    if (mpOutputBuffer != 0)
    {
        delete mpOutputBuffer;
        mpOutputBuffer = 0;
    }

    // [D2 gesture-sink] the partner of Construct()'s allocation -- the console's partner is
    // DoUpdate_GameStatePreWorld's DestroyIOBuffer tail.
    if (mpPreWorldInputBuffer != 0)
    {
        mpPreWorldInputBuffer->Destruct();
        delete mpPreWorldInputBuffer;
        mpPreWorldInputBuffer = 0;
    }

    // [gateui] the partner of Construct()'s allocation -- see mpTrainingManager's FLAG.
    if (mpTrainingManager != 0)
    {
        delete mpTrainingManager;
        mpTrainingManager = 0;
    }

    CgsModule::ModuleSingleBuffered::Destruct();
}

// ----------------------------------------------------------------------------
// ⭐ X360 0x8239E578 -- GameStateModule::Prepare (vtable +64), the module's FIRST-pass prepare.
//
// THE SHAPE (console, statement for statement):
//     mbIsUpdating = true;                       // *(this + 292289) = 1
//     LockForWrite(lpOutputBuffer);
//     switch (mePrepareStage) { ...27 cases, each falling into the next on success... }
//     UnlockForWrite(lpOutputBuffer);
//     mbIsUpdating = false;
//     return <true only after stage 26>;
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
// ⚠️⚠️ RECONSTRUCTED SLICE -- say it plainly. The REAL stages are:
//   stage 3  E_PREPARESTAGE_LOAD_TRIGGER_DATA -> TriggerQueryManager::Prepare @0x82398218.
//   stage 4  E_PREPARESTAGE_STUNT_MANAGER     -> the LoadBundle("Districts.dat") half of
//            StuntManager::Prepare's LoadDistrictMap (the tally half is still deferred; see
//            the stage body and mbDistrictsBundleRequested).
//   stages 7/8 and 9/10 -> the vehicle / wheel list GETs.
//   stage 23 E_PREPARESTAGE_STREET_MANAGER    -> StreetManager::Prepare @0x82350900
//            (LoadAIData + LoadDistrictMap -> mDistrictMapResourceHandle), added 2026-08-11.
//   stage 26 -> the car-select / progression list publish.
// Every other stage logs once and advances, naming its X360 call. In console order they are:
//   0  START                    ClearData @(not exported by name) + DebugComponent::Register x2
//                               (this+208544 / this+208376)
//   1  MANAGER                  ModuleSingleBuffered::Prepare  -- see the mbIsNewModule note below
//   2  MODE_DATA_ACQUIRING      pass-through on the console too (it only sets the stage word)
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
//   26 DONE                     DriveThruManager::Prepare(this+44240, mTriggerQueryManager's
//                               TriggerData, the CarColours palette), publish the vehicle/wheel
//                               list pointers into two sub-managers, re-arm mePrepareStage to 1
//                               and clear the Prepare2 stage word.
// NONE of those is faked here: each is a log line, not a fabricated body. They land as their
// managers do. DO NOT read the log line as "done".
//
// ⚠️ FLAG (PC deviation, stage 1): `mbIsNewModule = true` before the base Prepare. The PC
// ModuleSingleBuffered::Construct leaves it FALSE, and with it false the base walks the
// old-style DataStructure path -- CreateInputDataStructure() fires its own
// "This is a new module type" assert and returns null, so Prepare would return false FOR EVER
// and GamePrepare would wedge. The GameState module is a new-style (IOBuffer) module on the
// console, and the committed GameDataModule::Prepare carries the identical line with the same
// reasoning (BrnGameDataModule.cpp:51, "[reliable] set before base Prepare"). The console sets
// it inside ClearData, which is stage 0's deferral.
// ----------------------------------------------------------------------------
// The GameData reply ids the two live list stages match (BrnGameDataModule's dispatch stages
// them at the slot: ProcessGetVehicleListRequest -> 52, ProcessGetWheelListRequest -> 59).
// ⚠️ The FreeburnChallengeList reply (53) is NOT here on purpose: its GameData handler is a
// DeferredGameDataRequest, so a stage waiting on it would never advance.
static const s32 KI_REPLY_VEHICLE_LIST = 52;
static const s32 KI_REPLY_WHEEL_LIST   = 59;

// ✅ [gateui] Stage 4's district-map bundle literals moved OUT of this file (2026-08-20): the
// LoadBundle they served now happens where the console has it, inside StuntManager::LoadDistrictMap
// @0x82399458, so KPC_DISTRICT_MAP_BUNDLE_NAME / _RESOURCE_NAME / KI_DISTRICT_MAP_EVENT_ID /
// KI_DISTRICT_MAP_POOL_ID live in BrnStuntManager.cpp alongside it.

namespace
{
    // One log line per stage, once. (Same shape as the loading flow's LogScriptedStageOnce.)
    void LogPrepareStageOnce(s32 liStage, const char* lpcWhat)
    {
        static bool sbLogged[32] = { false };
        if (liStage < 0 || liStage >= 32 || sbLogged[liStage])
            return;
        sbLogged[liStage] = true;
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[GameStateModule::Prepare] stage " << liStage << " -- " << lpcWhat << "\n";
        }
    }
}

// ----------------------------------------------------------------------------
// The shared body of Prepare's three "receive a resident data list" stages (vehicle @X360
// LABEL_17, wheel @LABEL_25, challenge @LABEL_9). The console writes all three out longhand;
// they are identical apart from the expected reply id and the two baked assert LINES, so they
// are folded here with those as parameters -- the same folding the committed
// GameDataModule::PrepareDataListResource already does for its two twins.
//
//   if (mReceiverQueue.GetLength() < 1) return false;            // still waiting
//   assert(event type == <replyId>)     "Invalid event id received\n"  <line A>
//   assert(reply->miEventId == 0)       "Invalid event id received\n"  <line B>
//   *lppOut = reply->mHandle.mpResourceMemory;                   // X360 `v13[8]`, i.e. +0x20
//
// ⚠️ The console's two asserts are built through the StrStream operator<< form
// (CgsDev::StrStream + StrStreamBase::operator<<), not FireAssert's literal; the message text
// is identical either way, so the plain Begin/Fire/End sequence is used with the X360 lines.
// ----------------------------------------------------------------------------
bool GameStateModule::ReceiveListResource(s32 liExpectedReplyId, s32 liAssertLineType,
                                          s32 liAssertLineEventId, void** lppOutResource)
{
    if (mReceiverQueue.GetLength() < 1)
        return false;

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    const s32 liType = mReceiverQueue.GetFirstEvent(&lpEvent, &liSize);

    if (liType != liExpectedReplyId)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid event id received\n", KAC_GSM_FILE, liAssertLineType);
        CgsDev::Assert::EndAssert();
    }

    const BrnResource::GameDataIO::GameDataAssetEvent* lpReply =
        static_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);

    if (lpReply == 0 || lpReply->miEventId != 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Invalid event id received\n", KAC_GSM_FILE, liAssertLineEventId);
        CgsDev::Assert::EndAssert();
        return false;
    }

    // X360 `*(this + 284392) = v13[8]` -- payload +0x20 is mHandle.mpResourceMemory. Read BY
    // MEMBER (the host ResourceHandle is 16 bytes where the console's is 8, so every literal
    // offset past it shifts).
    *lppOutResource = lpReply->mHandle.mpResourceMemory;
    return true;
}

bool GameStateModule::Prepare(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                              CgsModule::IOBufferStack*        lpUpdateOutputBufferStack,
                              const BrnResource::GameDataIO::AllocatorList* lpAllocatorList)
{
    // The console asserts nothing here; GamePrepare always hands it a live buffer. Guard anyway
    // -- a null would otherwise fault inside TriggerQueryManager::Prepare's own assert.
    if (lpOutputBuffer == 0)
    {
        CGS_ASSERT(false, "lpOutputBuffer");
        return false;
    }
    (void)lpUpdateOutputBufferStack;   // stages 13/24 (heap allocator) + the manager prepares
    (void)lpAllocatorList;             //   are the deferrals listed above

    // X360: `*(this + 292289) = 1` at entry, cleared at the single exit.
    mbIsUpdating = true;

    if (!mbReceiverQueueConstructed)
    {
        // The console's mReceiverQueue is Construct'd by ClearData (stage 0's deferral). Every
        // stage below names it as its reply target, and AddEvent on an unconstructed receiver
        // queue writes through a null buffer base. One-shot here until ClearData lands.
        mbReceiverQueueConstructed = true;
        mReceiverQueue.Construct();
    }

    lpOutputBuffer->LockForWrite();

    bool lbDone = false;

    switch (mePrepareStage)
    {
    case E_PREPARESTAGE_START:
        // X360: GameStateModule::ClearData(this); DebugComponent::Register(this+208544);
        // DebugComponent::Register(this+208376). [deferred -- ClearData is a large member-wipe
        // over members this slice does not model; the two components are debug-menu only.]
        LogPrepareStageOnce(0, "ClearData + 2 x DebugComponent::Register [deferred]");
        // fall through

    case E_PREPARESTAGE_MANAGER:
        mePrepareStage = E_PREPARESTAGE_MANAGER;
        // See the FLAG above: the console's GameState module is a new-style module, so the base
        // prepare is a no-op that reports done. Setting the flag is what makes that true here.
        mbIsNewModule = true;
        if (!CgsModule::ModuleSingleBuffered::Prepare())
            break;
        // fall through

    case E_PREPARESTAGE_MODE_DATA_ACQUIRING:
        // The console's case 2 only advances the stage word (LABEL_4 -> LABEL_5).
        mePrepareStage = E_PREPARESTAGE_LOAD_TRIGGER_DATA;
        // fall through

    case E_PREPARESTAGE_LOAD_TRIGGER_DATA:
        // ⭐ REAL. X360: `if (!TriggerQueryManager::Prepare(this + 42320, lpOutputBuffer,
        //                    this + 232384)) break;  *(this + 552) = 4;`
        // This is the LoadBundle("Triggers.dat", pool 5) -> acquire("TriggerData") ->
        // LoadTrafficLanes chain. It needs several pumps.
        mePrepareStage = E_PREPARESTAGE_LOAD_TRIGGER_DATA;
        if (!mTriggerQueryManager.Prepare(lpOutputBuffer, &mReceiverQueue))
            break;
        mePrepareStage = E_PREPARESTAGE_STUNT_MANAGER;
        // fall through

    case E_PREPARESTAGE_STUNT_MANAGER:
        // ⭐⭐ REAL, WHOLE (2026-08-20, [gateui]). X360:
        //     `if (!StuntManager::Prepare(this + 183952, out)) break;  *(this + 552) = 6;`
        // (@0x8239E578, LABEL_7/LABEL_8: the store is 6, not the 5 an earlier banner here quoted.
        //  Behaviourally identical in this tree -- E_PREPARESTAGE_REQUEST_CHALLENGE_LIST (5) is an
        //  immediate fall-through to 6 -- but the quoted asm has to be right.)
        // StuntManager::Prepare @0x8239C9C0 is TWO halves and BOTH are live now:
        //   * LoadDistrictMap @0x82399458 -- LoadBundle("Districts.dat", pool 5) -> wait ->
        //     acquire("Districts") -> bind mDistrictMapResourceHandle from the response. It needs
        //     several pumps, which is why this stage breaks and is re-entered.
        //   * the census -- bind mWorldMap2D over the streamed grid, then walk every generic
        //     region, county-classify it and tally the JUMP / SMASH / BILLBOARD totals (and the
        //     signature-takedown count). Those totals ARE the "12/45" denominator the HUD popup
        //     prints, so nothing downstream of a smashed billboard is meaningful without them.
        //
        // ⭐ WHY THE BUNDLE LOAD HAS TO HAPPEN AT THIS STAGE (unchanged, and still the reason):
        // stage 23's StreetManager::LoadDistrictMap @0x8234FB98 only ACQUIRES "Districts" -- it
        // never loads the bundle, because on the console THIS stage loaded it 19 stages earlier.
        // Without the bundle resident, PoolModule::DoAcquireResourceRequest still replies (it
        // always does) but with a NULL handle, and SetupParRivals then null-derefs. The request
        // that keeps that working is now the console's own, issued from the console's own
        // function, instead of from the meDistrictsBundleStage stand-in latch this replaces (the
        // latch, its enum and its member are deleted from the header this wave).
        //
        // ⓘ RE-PREPARE SAFETY (the reason the old latch had to be sticky): the terminal stage
        // re-arms mePrepareStage at MANAGER, so this stage is re-entered on a second pass.
        // StuntManager::meDistrictMapLoadStage's E_DISTRICT_MAP_DONE case returns true without
        // re-requesting, so the second pass walks through and only re-runs the census.
        mePrepareStage = E_PREPARESTAGE_STUNT_MANAGER;
        if (!mStuntManager.Prepare(lpOutputBuffer))
            break;
        mePrepareStage = E_PREPARESTAGE_REQUEST_CHALLENGE_LIST;
        // fall through
    case E_PREPARESTAGE_REQUEST_CHALLENGE_LIST:
    case E_PREPARESTAGE_RECEIVE_CHALLENGE_LIST:
        LogPrepareStageOnce(5, "GetFreeburnChallengeList + receive (reply 53) [deferred]");
        // fall through
    case E_PREPARESTAGE_REQUEST_VEHICLE_LIST:
        // ⭐ REAL. X360 LABEL_16: `stage = 8; GetVehicleList(requests, &mReceiverQueue, 0);
        //                          mReceiverQueue.Clear();` then fall into the receive.
        mePrepareStage = E_PREPARESTAGE_RECEIVE_VEHICLE_LIST;
        lpOutputBuffer->GetResourceRequestInterface()->GetVehicleList(&mReceiverQueue, 0);
        mReceiverQueue.Clear();
        // fall through

    case E_PREPARESTAGE_RECEIVE_VEHICLE_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_VEHICLE_LIST;
        if (!ReceiveListResource(KI_REPLY_VEHICLE_LIST, 556, 561,
                                 reinterpret_cast<void**>(&mpVehicleList)))
            break;
        // [deferred] ProgressionManager::ApplyVehicleList(this+47920) and
        // ModeManager::ApplyVehicleList(this+4128, mpVehicleList). Neither has a body in this
        // tree yet (only ChallengeManager::ApplyVehicleList is even declared). The pointer
        // itself IS installed, which is what every GameStateModule body that asserts on it
        // needs; the two republish hooks land with their managers.
        LogPrepareStageOnce(8, "vehicle list installed; 2 x ApplyVehicleList [deferred]");
        mePrepareStage = E_PREPARESTAGE_REQUEST_WHEEL_LIST;
        // fall through

    case E_PREPARESTAGE_REQUEST_WHEEL_LIST:
        // ⭐ REAL. X360 LABEL_24, the same shape with reply id 59.
        mePrepareStage = E_PREPARESTAGE_RECEIVE_WHEEL_LIST;
        lpOutputBuffer->GetResourceRequestInterface()->GetWheelList(&mReceiverQueue, 0);
        mReceiverQueue.Clear();
        // fall through

    case E_PREPARESTAGE_RECEIVE_WHEEL_LIST:
        mePrepareStage = E_PREPARESTAGE_RECEIVE_WHEEL_LIST;
        if (!ReceiveListResource(KI_REPLY_WHEEL_LIST, 599, 604,
                                 reinterpret_cast<void**>(&mpWheelList)))
            break;
        mePrepareStage = E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS;
        // fall through
    case E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS:
    case E_PREPARESTAGE_RECEIVE_PLAYERCARCOLOURS:
        LogPrepareStageOnce(11, "acquire \"CarColours\" (pool 5) + bind [deferred]");
        // fall through
    case E_PREPARESTAGE_MODEMANAGER:
    case E_PREPARESTAGE_TAKEDOWNMANAGER:
    case E_PREPARESTAGE_MUGSHOTMANAGER:
    case E_PREPARESTAGE_PAYBACKMANAGER:
    case E_PREPARESTAGE_INVITEMANAGER:
    case E_PREPARESTAGE_FLYBYMANAGER:
    case E_PREPARESTAGE_NETWORKROUNDMANAGER:
    case E_PREPARESTAGE_PROGRESSION:
    case E_PREPARESTAGE_RICH_PRESENCE:
    case E_PREPARESTAGE_ACHIEVEMENT_MANAGER:
        LogPrepareStageOnce(13, "the 10 manager prepares (Mode..Achievement) [deferred]");
        // [takedown wave 2026-09-02] stage 14 of the console ladder, `if (TakedownManager::Prepare(gsm+568))`
        // -- REAL now (the other nine stay deferred as the line above says).
        if (!PrepareTakedownBringUp())
        {
            break;
        }
        // fall through

    case E_PREPARESTAGE_STREET_MANAGER:
        // ⭐ REAL. X360 LABEL_51: `*(this+552) = 23;
        //     if (!StreetManager::Prepare(this + 284520, out, this + 232384)) break;`
        // i.e. mStreetManager.Prepare(out, &mReceiverQueue). StreetManager::Prepare @0x82350900 is
        //     LoadAIData(out, rq) && LoadDistrictMap(out, rq)
        //         -> debug component Construct + Register; return true
        // LoadDistrictMap @0x8234FB98 is the ONLY writer of mDistrictMapResourceHandle, which
        // SetupParRivals dereferences unconditionally -- so this stage is the precondition for
        // Prepare2 case 2's SetupParRivals leg. It acquires "Districts" out of pool 5; stage 4
        // above is what makes that bundle resident.
        mePrepareStage = E_PREPARESTAGE_STREET_MANAGER;
        if (!mStreetManager.Prepare(lpOutputBuffer, &mReceiverQueue))
            break;
        // fall through

    case E_PREPARESTAGE_IMAGE_MANAGER:
    case E_PREPARESTAGE_RUMBLE_MANAGER:
        LogPrepareStageOnce(24, "GameStateImageManagerBase / RumbleManager::Prepare [deferred]");
        // fall through
    case E_PREPARESTAGE_DONE:
        // [diagnostic, one-shot] print BOTH ENDS of the two list stages -- a non-null pointer
        // is not proof the list decoded. Delete with the rest of the bring-up diagnostics.
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[GameStateModule::Prepare] lists: vehicles="
                << (mpVehicleList != 0 ? mpVehicleList->GetVehicleCount() : -1)
                << " wheels="
                << (mpWheelList != 0 ? mpWheelList->GetWheelCount() : -1) << "\n";
        }
        // ⭐ REAL now (the "list publish" half of the terminal stage). X360 @0x8239EC8C, straight
        // after DriveThruManager::Prepare:
        //     r9 = this + 0x2CDA0 (mCarSelectManager);  r8 = this + 0x2CE20 (mOnlineCarSelectManager)
        //     stw *(this+0x456EC), 0x18(r9)   <- CarSelectManager::mpWheelList
        //     stw *(this+0x456E8), 0x14(r9)   <- CarSelectManager::mpVehicleList
        //     stw *(this+0x456EC), 0x10(r8)   <- OnlineCarSelectManager::mpWheelList
        //     stw *(this+0x456E8), 0x0C(r8)   <- OnlineCarSelectManager::mpVehicleList
        // Those four stores ARE CarSelectManager::Prepare / OnlineCarSelectManager::Prepare, both
        // fully inlined (neither has a symbol in the image; the DWARF declares both as
        // `Prepare(const VehicleList*, const WheelList*)`). Called through the named methods here.
        //
        // This is what the whole junkyard flow was missing: SetupSpawnLocations, SpawnInStartCar,
        // GetProfileCarData and StartCarSelectState all resolve their car records through
        // CarSelectManager::mpVehicleList, and nothing had ever written it.
        //
        // ⭐ [drive-thru wave 2026-08-27] REAL now -- the "[deferred]" half of this stage is paid.
        // X360 0x8239E578 @LABEL_55 (pseudocode lines 271-273), the two lines immediately BEFORE
        // the four car-select list stores below:
        //     Memor = BrnTrigger::TriggerData_::GetMemor(a1 + 43888);   // mTriggerQueryManager's
        //                                                              //   ResourcePtr<TriggerData>
        //     v25   = BrnWorld::GlobalColour(v33, a1 + 284400);         // the CarColours palette
        //     BrnGameState::DriveThruManager::Prepare(a1 + 44240, Memor, v25);
        // Prepare is what turns the loaded TriggerData into the manager's working set: it walks
        // every generic region, keeps the IsDriveThru() ones into maDriveThruTriggerData[46] with
        // their world position, and tallies miTotalJunkYards / miTotalGasStations / miTotalBodyShops
        // / miTotalPaintShops / miTotalCarParks. Without it every one of those totals stays 0 and
        // HandleDriveThru's "find this region's entry" scan can never match, so the whole drive-thru
        // chain is inert even with the call sites restored.
        // ⚠️ [FLAG PC bring-up] THE PALETTE IS NULL ON THIS BUILD. Stage 11/12
        // (E_PREPARESTAGE_REQUEST_PLAYERCARCOLOURS) still logs "acquire \"CarColours\" (pool 5) +
        // bind [deferred]" and never binds this+284400, so a DEFAULT-CONSTRUCTED (null) ResourcePtr
        // is passed -- which is the honest value, not a stand-in. Consequence, stated rather than
        // hidden: ProcessDriveThru's PAINT_SHOP arm is the ONE arm that dereferences it
        // (mpPlayerCarColours->maPalettes[2].miNumColours), so driving through a paint shop will
        // fire the ResourcePtr assert. Gas station and body shop do not touch it.
        // DELETE-WHEN stage 11/12 binds the CarColours resource for real.
        mDriveThruManager.Prepare(mTriggerQueryManager.GetTriggerData(),
                                  CgsResource::ResourcePtr<BrnWorld::GlobalColourPalette>());

        // [deferred] the OnlineCarSelectManager leg (its TU is unmounted).
        mCarSelectManager.Prepare(mpVehicleList, mpWheelList);

        // ⭐ AND THE PROGRESSION LAYER'S COPY (X360 ProgressionManager +133448). MEASURED:
        // the first live ResetPlayerCarAction chain fired the console's own "lpVehicleListEntry"
        // assert (BrnProgressionManager.cpp:1258) from OnPlayerCarChange, because the
        // ProgressionManager's vehicle-list pointer had NEVER been installed by anything --
        // its SetVehicleList had zero callers in the whole tree, and the header's own FLAG said
        // so ("nothing installs it yet -- Prepare2's caller does on the console"). Every
        // progression body that resolves a car record reads that pointer.
        // [FLAG PC bring-up] the console installs it from Prepare2's caller; this is the same
        // list, published from the stage that already publishes it to the two car-select
        // managers. DELETE-WHEN Prepare2's caller lands.
        mProgressionManager.SetVehicleList(mpVehicleList);

        // [diagnostic, one-shot] Prove the junkyard half of the trigger data end to end, WITHOUT
        // driving anything: count the E_TYPE_JUNK_YARD generic regions, run the console's own
        // FindNearestJunkyardID @0x8236BAC8 from the track's authored player-start position (the
        // exact vector SendSetupPlayerCarEvent @0x8239A918 feeds it), and check that the id it
        // picks also appears in the SPAWN table -- because that cross-table agreement is the
        // precondition for CarSelectManager::SetupSpawnLocations filling all five slots, and
        // therefore for the junkyard entry not null-dereferencing maSpawnLocations[1].
        // Delete with the rest of the bring-up diagnostics.
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
            if (lpTriggerData != 0)
            {
                s32 liJunkyardRegions = 0;
                const s32 liRegionCount = lpTriggerData->GetGenericRegionCount();
                for (s32 li = 0; li < liRegionCount; ++li)
                {
                    if (lpTriggerData->GetGenericRegion(li)->GetType()
                        == BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD)
                        ++liJunkyardRegions;
                }

                const Vector3 lStart      = lpTriggerData->GetPlayerStartPosition();
                const CgsID   lJunkyardId = FindNearestJunkyardID(lStart);

                s32 liMatchingSpawns = 0;
                const s32 liSpawnCount = lpTriggerData->GetSpawnLocationCount();
                for (s32 li = 0; li < liSpawnCount; ++li)
                {
                    if (lpTriggerData->GetSpawnLocation(li)->GetJunkyardId() == lJunkyardId)
                        ++liMatchingSpawns;
                }

                *CgsDev::Log::gpDebugPrint
                    << "[GameStateModule] junkyard regions=" << liJunkyardRegions
                    << "/" << liRegionCount
                    << " playerStart=(" << lStart.x << ", " << lStart.y << ", " << lStart.z << ")"
                    << " nearestJunkyardId=" << static_cast<u64>(lJunkyardId)
                    << " spawnsForThatJunkyard=" << liMatchingSpawns << "\n";
            }
        }
        // ⭐ ARM THE CONSOLE'S OWN START-OF-GAME LATCH (+0x32DC4). PreWorldUpdate tests it, runs
        // SendSetupPlayerCarEvent and clears it. The console arms it from an event handler this
        // slice does not reconstruct; this is the first moment all three of that function's data
        // preconditions hold (mpVehicleList, mpWheelList and the TriggerQueryManager's TriggerData
        // are all installed by the stages above), so it is armed here.
        // [FLAG PC bring-up] the ARMING SITE is the deviation -- the latch and everything it
        // drives are console code. DELETE-WHEN the arming event handler lands.
        mbSendSetupPlayerCarPending = true;

        // [drive-thru wave 2026-08-27] One-shot: prove Prepare actually classified regions. A
        // non-zero miTotalGasStations is the precondition for every later gas-station claim; a zero
        // here means the TriggerData had no drive-thru regions, NOT that the effect failed.
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[drivethru] Prepare: gas=" << mDriveThruManager.GetTotalDriveThrusOfType(
                       BrnTrigger::GenericRegion::E_TYPE_GAS_STATION)
                << " body=" << mDriveThruManager.GetTotalDriveThrusOfType(
                       BrnTrigger::GenericRegion::E_TYPE_BODY_SHOP)
                << " paint=" << mDriveThruManager.GetTotalDriveThrusOfType(
                       BrnTrigger::GenericRegion::E_TYPE_PAINT_SHOP)
                << " junk=" << mDriveThruManager.GetTotalDriveThrusOfType(
                       BrnTrigger::GenericRegion::E_TYPE_JUNK_YARD)
                << " carpark=" << mDriveThruManager.GetTotalDriveThrusOfType(
                       BrnTrigger::GenericRegion::E_TYPE_CAR_PARK) << "\n";
        }
        LogPrepareStageOnce(26, "car-select list publish REAL; DriveThruManager::Prepare REAL -- prepare DONE");
        // X360 tail: `*(this + 552) = 1; *(this + 560) = 0;` -- the machine re-arms at MANAGER
        // for a later re-prepare and clears the +560 flag. (CORRECTION 2026-08-11: +560 is NOT
        // Prepare2's stage word, as an earlier note here claimed -- Prepare2 @0x8239ED10 switches
        // on this+556 (`lwz r11, 0x22C(r31)`). +560 is a separate flag this slice does not model.)
        mePrepareStage = E_PREPARESTAGE_MANAGER;
        lbDone = true;
        break;

    default:
        CGS_ASSERT(false, "Invalid Stage\n");   // X360 BrnGameStateModule.cpp:825
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
        mbPrepare2Complete = true;
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
        const f32 lfDeltaX = lRegionPosition.x - lPosition.x;
        const f32 lfDeltaY = lRegionPosition.y - lPosition.y;
        const f32 lfDeltaZ = lRegionPosition.z - lPosition.z;
        const f32 lfSqDistance =
            lfDeltaX * lfDeltaX + lfDeltaY * lfDeltaY + lfDeltaZ * lfDeltaZ;
        const f32 lfDistance = (lfSqDistance != 0.0f) ? std::sqrt(lfSqDistance) : 0.0f;

        if (lfDistance < lfNearest)
        {
            lfNearest   = lfDistance;
            lJunkyardId = lpRegion->GetId();
        }
    }

    CGS_ASSERT(lJunkyardId != 0, "lJunkyardId != kCGSID_NULL");
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
    if (lpCurrentGameMode != nullptr)
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
    CGS_ASSERT(leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
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
        if (lpCurrentGameMode != nullptr && lpCurrentGameMode->IsOnline())
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
    CGS_ASSERT(lpProfile != nullptr, "mProgressionManager.GetProfile()");
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
CgsID GameStateModule::GetActivePlayerCarId() const
{
    return mActivePlayerCarId;
}

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

    if (mModeManager.GetCurrentGameMode() != nullptr)
    {
        return false;
    }

    return true;
}

// [tut-ticker] the declare-only ModeManager accessor, bodied (the X360 inlines the embedded
// member's address; MugshotManager/TrainingManager reach it by name here).
ModeManager* GameStateModule::GetModeManager()
{
    return &mModeManager;
}

// --------------------------------------------------------------------------------------------
// ⭐⭐ [tut-ticker] PreWorldUpdateTrainingBringUp -- the extracted TRAINING leg of PreWorldUpdate
// @0x823A5328 (see the header banner for the console asm). Runs the ModeManager clock leg first
// (the console ticks the clocks earlier in the same PreWorldUpdate body, @0x823537B8 via
// ModeManager::PreWorldUpdate), then the console's own pair:
//     r8 = ShouldAllowTimedTutorialTips();
//     TrainingManager::Update(mpTrainingManager, <preWorldInput>, actionQueue,
//                             &mLastActiveRaceCarInterface, lfGameTimestep, r8);
// The output buffer's action queue is taken under the module's own write bracket, exactly as
// the sibling PreWorldUpdate legs do.
// --------------------------------------------------------------------------------------------
void GameStateModule::PreWorldUpdateTrainingBringUp(f32 lfGameTimestep)
{
    if (mpOutputBuffer == 0 || mpTrainingManager == 0)
    {
        return;
    }

    // ⭐⭐ [D4 stuntrace WAVE D] THE CLOCK BRING-UP LEG IS RETIRED HERE -- DO NOT PUT IT BACK.
    // This call used to read:
    //     mModeManager.PreWorldUpdateClocksBringUp(lfGameTimestep);
    // and it was the ONLY thing accumulating mfTimeInFreeBurn / mfTimeInMode / mfTimeInOnline while
    // the full ModeManager::PreWorldUpdate had no caller. It has one now:
    // GameStateModule::PreWorldUpdateStuntBringUp stages ModeManager::PreWorldUpdate @0x823537B8
    // at the console's own position (#86, via the EmmPreWorldUpdate hop), and that function
    // CONTAINS the whole of PreWorldUpdateClocksBringUp -- the identical mode/online/free-burn
    // if/else, over the identical members.
    // ⛔ THE TWO MUST NEVER BE ARMED TOGETHER (the supersession rule stated in
    // ModeManager_gUI_00.cpp's own banner and in BrnModeManager_WorldTick.cpp:40): with both
    // running, every clock would advance at DOUBLE rate, which is silent -- no assert, no link
    // error, just a free-burn timer that runs 2x and a mode timer that expires at half the
    // authored limit.
    // The method itself is left in place (it is still the documented bring-up seam and it is
    // referenced by both banners); only this CALL is removed. Delete the method with its
    // declaration when the supersession is consolidated.

    mpOutputBuffer->LockForWrite();
    GameStateModuleIO::GameActionQueue* lpActionQueue = mpOutputBuffer->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue != NULL");

    mbIsUpdating = true;   // the module's own accessors assert this (IsOnlineGameMode et al.)
    const bool lbAllowTimedTips = ShouldAllowTimedTutorialTips();
    mpTrainingManager->Update(lpActionQueue, &mLastActiveRaceCarInterface,
                              lfGameTimestep, lbAllowTimedTips);
    mbIsUpdating = false;

    mpOutputBuffer->UnlockForWrite();
}

// --------------------------------------------------------------------------------------------
// ⭐⭐ [tut-ticker] ProcessGameEventsTrainingRequestBringUp -- the extracted CASE-113 arm of
// ProcessGameEvents @0x823A0A18 (same extraction precedent as the case-111 PropHit arm):
//     case 113: BrnGameState::TrainingManager::RequestTraining(this + 46640, *payload);
// The payload's leading s32 is the BrnProgression::ETrainingType the world queued.
// --------------------------------------------------------------------------------------------
void GameStateModule::ProcessGameEventsTrainingRequestBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue)
{
    if (lpGameEventQueue == 0 || mpTrainingManager == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32 liSize = 0;
    s32 liType = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);
    while (lpEvent != 0)
    {
        if (liType == 113)   // E_EVENT_REQUEST_GAME_TRAINING
        {
            // (the caller -- PreWorldUpdateStuntBringUp's dispatcher walk -- already holds the
            //  module's mbIsUpdating bracket, exactly as the console's ProcessGameEvents does.)
            const s32 liTrainingType = *reinterpret_cast<const s32*>(lpEvent);
            mpTrainingManager->RequestTraining(
                static_cast<BrnProgression::ETrainingType>(liTrainingType));
        }

        const CgsModule::Event* lpNext = 0;
        liType = lpGameEventQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
        lpEvent = lpNext;
    }
}

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
    Array<s64, 7> laOpponentCarIds;
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
            if (liOpponents >= static_cast<s32>(Array<s64, 7>::KU_SIZE))
            {
                liOpponents = static_cast<s32>(Array<s64, 7>::KU_SIZE);
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

// --------------------------------------------------------------------------------------------
// RequestStreamingForVehicleSelection (X360 0x82382550).
//
// ⛔ HONEST PARTIAL -- READ THIS BEFORE TRUSTING THE CALL SITES.
//
// The console body builds the junkyard carousel's PRE-STREAM window: it asks
// GetListOfPlayerSelectableVehicles (X360 0x82376500) for the player's full selectable-car list
// into an Array<CgsID,128>, finds lCarId in it (falling back to the car's PARENT id when lCarId is
// a livery variant -- entry+0xE9 in {1,3,4}), then walks outward from that index collecting up to
// KI_MAX_ACTIVE_RACE_CARS(8) neighbours with a per-entry direction tag, and posts the resulting
// 88-byte action 69.
//
// WHAT IS REPRODUCED HERE: the lookup of the requested car, its livery->parent fallback, and both
// of the console's asserts. WHAT IS NOT: the selectable-list build and the neighbour window,
// because GetListOfPlayerSelectableVehicles is 183 instructions of its own and reaches four
// GameStateModule members that are not modelled on this slice (+183860 / +183937 / +183944 -- the
// online-event car-restriction state -- plus the profile car walk).
//
// WHY THIS IS NOT A SILENT DROP: (a) it logs, once per changed car, exactly what it did not send;
// (b) NOTHING IN THIS BUILD CONSUMES ACTION 69 -- the console consumer is
// RaceCarEntityModule::HandleSelectionRequestStreamingAction @0x822E9918, which is not
// reconstructed (grep for it: the only hit in the tree is a comment). So today the only observable
// difference between this and the full body is the log line.
// DELETE-WHEN GetListOfPlayerSelectableVehicles lands (then the window build comes back with it).
// --------------------------------------------------------------------------------------------
void GameStateModule::RequestStreamingForVehicleSelection(CgsID lCarId)
{
    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    if (lpVehicleList == 0)
    {
        return;
    }

    CgsID lStreamCarId = lCarId;

    const s32 liCurrentVehicleIndex = lpVehicleList->GetVehicleIndex(lCarId);
    const BrnResource::VehicleListEntry* lpCurrentVehicleData =
        (liCurrentVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liCurrentVehicleIndex);
    if (lpCurrentVehicleData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCurrentVehicleData != NULL", KAC_GSM_FILE, 7461);
        CgsDev::Assert::EndAssert();
    }
    else
    {
        // A livery variant is not itself in the selectable list -- the console re-looks-up its
        // parent (X360: entry+0xE9 in {1,3,4} -> use GetParentId()).
        const u8 luLiveryType = lpCurrentVehicleData->GetLiveryType();
        if (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
        {
            if (lpCurrentVehicleData->GetParentId() == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpCurrentVehicleData->GetParentId() != kCGSID_NULL",
                                           KAC_GSM_FILE, 7465);
                CgsDev::Assert::EndAssert();
            }
            lStreamCarId = lpCurrentVehicleData->GetParentId();
        }
    }

    static CgsID slLastLoggedCarId = 0;
    if (slLastLoggedCarId != lStreamCarId && CgsDev::Log::gpDebugPrint != 0)
    {
        slLastLoggedCarId = lStreamCarId;
        *CgsDev::Log::gpDebugPrint
            << "[FLAG PC bring-up] RequestStreamingForVehicleSelection(" << static_cast<u32>(lStreamCarId)
            << "): the 88-byte carousel pre-stream action (69) is NOT posted -- "
               "GetListOfPlayerSelectableVehicles is not reconstructed. Nothing in this build "
               "consumes action 69 either (HandleSelectionRequestStreamingAction is absent), so "
               "no consumer is being starved today.\n";
    }
}

// ============================================================================
// FindPlayerScoringIndexForActiveRaceCar  @ 0x82363450
//
// Linear scan of the scoring module's eight per-player records (the console walks
// `scoring + 20548 + 344*i`, comparing the leading word against the requested
// active-race-car index) for the player scoring slot that owns leActiveRaceCarIndex.
// ⚠️ The MISS arm returns E_PLAYER_SCORING_INDEX_0, not an invalid sentinel -- `result = 0`
// at the console's LABEL_7 -- and that is exactly what start-of-game relies on: nothing is
// mapped yet, so the player's car takes scoring slot 0.
//
// [FLAG PC bring-up] the scoring module's per-player record array is not homed on this slice
// (the console reaches it as `this + 7632`, deep inside the un-modelled mid-object span), so
// the scan itself has nothing to walk and the function returns the console's own miss value.
// It is written as the miss arm, NOT as a fabricated scan. DELETE-WHEN BrnScoringSystem's
// per-player record array is homed here.
GameStateModuleIO::EPlayerScoringIndex
GameStateModule::FindPlayerScoringIndexForActiveRaceCar(::EActiveRaceCarIndex leActiveRaceCarIndex) const
{
    (void)leActiveRaceCarIndex;
    return GameStateModuleIO::E_PLAYER_SCORING_INDEX_0;
}

// ============================================================================
// SendSetupPlayerCarEvent  @ 0x8239A918   -- THE START-OF-GAME JUNKYARD ENTRY
//
// Console body, statement for statement (0x8239A918..0x8239AA30):
//   1. cache the track's authored player-start pose:
//        this+48336 = TriggerData::GetPlayerStartPosition()   (lvx128 memory+0x10)
//        this+48352 = TriggerData::GetPlayerStartDirection()  (lvx128 memory+0x20)
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
// case-78 arm, is ProcessGameEventsReallyEnterJunkyardBringUp() below. Without this store the
// junkyard entry stops half-done: the player's car is placed at maSpawnLocations[1] and NOTHING
// ever posts the transition-in action, so the director's meJunkyardState stays E_JY_INACTIVE and
// ArbStateCarSelect is never reached. That was the whole gap.
//
// [FLAG PC bring-up] steps 1 (the two cached pose members) and 7 are DROPPED, not paraphrased:
// the two pose members sit inside this slice's un-modelled span and have no reconstructed reader,
// and ProgressionManager::OnDriveThru is not reconstructed. Step 4 reads the start position
// straight from the TriggerData, which is the same value step 1 would have cached.
void GameStateModule::SendSetupPlayerCarEvent(GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    const BrnTrigger::TriggerData* lpTriggerData = mTriggerQueryManager.GetTriggerData();
    if (lpTriggerData == 0 || mpVehicleList == 0 || mpWheelList == 0)
    {
        return;
    }

    const Vector3 lPlayerStart = lpTriggerData->GetPlayerStartPosition();

    const BrnResource::VehicleListEntry* lpEntry = mpVehicleList->GetVehicleData(0);
    if (lpEntry == 0)
    {
        return;
    }
    const CgsID lCarModelId = lpEntry->GetId();

    CgsID lWheelId = 0;
    const s32 liWheelIndex = mpWheelList->FindWheelIndexFromName(lpEntry->GetDefaultWheelName());
    if (liWheelIndex != -1)
    {
        const BrnResource::WheelListEntry* lpWheelEntry = mpWheelList->GetWheelData(liWheelIndex);
        if (lpWheelEntry != 0)
        {
            lWheelId = lpWheelEntry->mID;
        }
    }

    const CgsID lJunkyardId = FindNearestJunkyardID(lPlayerStart);
    const GameStateModuleIO::EPlayerScoringIndex leScoringIndex =
        FindPlayerScoringIndexForActiveRaceCar(mePlayerActiveRaceCarIndex);

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[GameStateModule::SendSetupPlayerCarEvent] junkyard=" << static_cast<u64>(lJunkyardId)
            << " car=" << static_cast<u64>(lCarModelId)
            << " wheel=" << static_cast<u64>(lWheelId)
            << " scoringIdx=" << static_cast<s32>(leScoringIndex)
            << " playerStart=(" << lPlayerStart.x << ", " << lPlayerStart.y << ", "
            << lPlayerStart.z << ")\n";
    }

    mCarSelectManager.EnterJunkyardAtStartOfGame(lpActionQueue, lJunkyardId, lCarModelId, lWheelId,
                                                leScoringIndex, &mCachedCarSelectChangedAction);

    // Step 8 -- X360 `li r11,1; stb r11, <this+0x38B72>`. Arm the "waiting to REALLY enter the
    // junkyard" latch that ProcessGameEvents case 78 tests.
    mbWaitingToPutPlayerInJunkyard = true;
}

// ============================================================================
// ProcessGameEventsReallyEnterJunkyardBringUp
//   -- the extracted case-78 arm of ProcessGameEvents @0x823A0A18 (0x823A4590..0x823A45F8).
// See the header for the full FLAG (why the GUI-event trigger is not used on this build).
//
// The console arm, instruction for instruction:
//   0x823A4590  r29 = this + 0x38B72 (232306)
//   0x823A4598  lbz  r11, 0(r29);  if (!r11) break                  -- mbWaitingToPutPlayerInJunkyard
//   0x823A45A4  r3 = this + 0x2CDA0 (183712) == &mCarSelectManager
//   0x823A45A8  r4 = the game ACTION queue
//   0x823A45B0  bl  CarSelectManager::ReallyEnterJunkyardAtStartOfGame
//   0x823A45B4  r30 = this + 0x38B80 (232320) == &mCachedCarSelectChangedAction
//   0x823A45BC  ld   r11, 0(r30);  if (!r11) FireAssert(.., BrnGameStateModule.cpp, 0x1003=4099)
//   0x823A45E0  AddEvent(queue, r30, 0x40, 0x40)                    -- action 64, 64 bytes
//   0x823A45F4  stb  r18(==0), 0(r29)                               -- clear the latch
//
// ⚠️ THE ASSERT IS A 64-BIT TEST. Hex-Rays renders it `if (!*(v23 + 232324))` -- the classic
// big-endian misrender of a `ld` at +232320 as a word read of its low half. The asm computes
// r30 == this+232320 and does `ld r11, 0(r30)`, i.e. it tests the whole CgsID mJunkyardId. The
// same r30 is then handed to AddEvent as the record base, which only makes sense at +232320.
// ============================================================================
void GameStateModule::ProcessGameEventsReallyEnterJunkyardBringUp(
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (!mbWaitingToPutPlayerInJunkyard)
    {
        return;
    }

    mCarSelectManager.ReallyEnterJunkyardAtStartOfGame(lpActionQueue);

    // X360 assert literal, as IDA renders it: "mCachedCarSelectChangedAction.mJunkyard"... --
    // the string is truncated in the export at 39 characters; the tail below is this repo's
    // completion of it, in the file's own house style. The TEST is the console's.
    CGS_ASSERT(mCachedCarSelectChangedAction.mJunkyardId != 0,
               "mCachedCarSelectChangedAction.mJunkyardId != kCGSID_NULL");

    // The 64-byte CarSelectionChangedAction the entry filled in. MainDirector::ProcessInputQueue
    // case 64 is its consumer: it is the ONLY writer of the director GameState's mJunkyardId and
    // mbJunkyardPosIsLeft.
    lpActionQueue->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&mCachedCarSelectChangedAction),
        KI_ACTION_CAR_SELECTION_CHANGED,
        static_cast<s32>(sizeof(mCachedCarSelectChangedAction)));

    mbWaitingToPutPlayerInJunkyard = false;

    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[GameStateModule::ProcessGameEvents case 78] ReallyEnterJunkyardAtStartOfGame done;"
            << " junkyard=" << static_cast<u64>(mCachedCarSelectChangedAction.mJunkyardId)
            << " posIsLeft=" << (mCachedCarSelectChangedAction.mbJunkyardPosIsLeft ? 1 : 0) << "\n";
    }
}

// ============================================================================
// PreWorldUpdateSetupPlayerCarBringUp -- the extracted one-shot leg of
// PreWorldUpdate @0x823A5328 (0x823A5510..0x823A5540). See the header for the FLAG.
// ============================================================================
void GameStateModule::PreWorldUpdateSetupPlayerCarBringUp()
{
    // Two legs of PreWorldUpdate live here now, each behind its own console latch, in the
    // console's own body order: the one-shot setup leg @0x823A5510, then the case-78 arm of
    // ProcessGameEvents @0x823A58B8.
    //
    // ⭐⭐⭐ [returning-player wave 2026-08-28] THE ORDERING STAND-IN IS RETIRED.
    // This function used to take `bool lbMayCompleteJunkyardEntry` and the caller passed
    // MainDirector::IsNewProfileIntroActive(). That is a NEW-PROFILE-ONLY signal, so on any boot
    // that finds a Profile.sav the case-78 arm below never fired, the junkyard entry stayed HALF
    // DONE for the whole run, RaceCarEntityModule::mbInCarSelectScreen was never cleared (only
    // CarSelectManager::UpdateExitState posts the reset that clears it), and
    // ActiveRaceCar::UpdateEngineState's case-OFF arm -- `demand && !inCarSelect` -- refused to
    // crank the engine however long the throttle was held. A RETURNING PLAYER COULD NOT DRIVE.
    // The gate is now the console's own game event 78 (see the header banner for why the
    // "the event arrives before the latch" measurement that justified the stand-in was reading
    // the WRONG OnEnter).
    //
    // ⚠️ THE ARM-SELECTION HAZARD THE OLD NOTE RECORDED IS REAL AND IS *NOT* WHAT THE STAND-IN
    // WAS FOR. ArbStateCarSelect::Prepare picks its opening arm from mbNewProfileIntroActive, so
    // an entry that completes before the GUI has raised that flag puts the state in the junkyard
    // E_STATE_INTRO instead of E_STATE_GAME_INTRO_PART_ONE, and the intro's own fly-by request
    // then trips that state's `!mbGameIntroFlybyActive` tripwire (:381) once per frame (measured:
    // 163 asserts in a 98 s run). What protects against that is ORDER, not the new-profile flag:
    // the real event 78 is posted by BrnGui::InGame::OnEnter, i.e. only once the GUI screen flow
    // has reached the in-game screen -- which is the same screen state that raises the intro
    // flag, and ~200 log lines later than the point the arm used to be able to fire from.
    if (mpOutputBuffer == 0)
    {
        return;
    }
    if (!mbSendSetupPlayerCarPending && !mbWaitingToPutPlayerInJunkyard)
    {
        return;
    }

    // The console gets the queue from GameStateModuleIO::OutputBuffer (its `Ou` accessor at
    // 0x823A54E4) and asserts it non-null at BrnGameStateModule.cpp:1149.
    mpOutputBuffer->LockForWrite();
    GameStateModuleIO::GameActionQueue* lpActionQueue = mpOutputBuffer->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue != NULL");
    mbIsUpdating = true;                   // the module asserts this in its own accessors
    if (mbSendSetupPlayerCarPending)
    {
        mbSendSetupPlayerCarPending = false;   // the console's `stb r17, 0(r28)` -- one-shot

        // ⭐⭐ THE FLAG THAT USED TO STAND AT THE BOTTOM OF THIS FUNCTION IS RETIRED
        // (2026-08-27, event-starts producer wave). SendSetUpAllEventStartsMessage @0x823759D0 is
        // BODIED now (GameStateModule_SendSetUpAllEventStarts.cpp) and this is the console's own
        // partner call on this same one-shot latch -- PreWorldUpdate @0x823A5510..0x823A5540 runs
        // SendSetupPlayerCarEvent AND SendSetUpAllEventStartsMessage(lpOutput) under it, in this
        // order, inside the write lock this bracket already holds (which is required: the
        // publish's SetSetUpAllEventStartsInterfaceIsValid asserts the buffer is locked for
        // writing). It publishes the event-start table -- the ONLY path in the image to
        // SetUpAllEventStartsInterface::AddEventStart @0x82361398, and therefore the only thing
        // that ever puts a record in the GUI cache's maEventStarts. Until it ran,
        // GuiCache::GetProfileEventDisplayInfo walked a zero-length array on every sat-nav
        // refresh and fired the console's own "Unable to find event start with event id: ".
        // ⓘ ONE-SHOT: the latch fires at the end of Prepare's terminal stage, which is the first
        // moment the producer's three data preconditions (TrafficData, AI lanes, district map)
        // are all satisfied.
        SendSetupPlayerCarEvent(lpActionQueue);
        SendSetUpAllEventStartsMessage(mpOutputBuffer);
    }

    // The console's ProcessGameEvents pass, restricted to the one arm this tree has extracted.
    // Runs in the SAME sub-step as the arming leg above and BEFORE the CarSelectManager tick --
    // the console's own body order inside PreWorldUpdate (0x823A5510 / 0x823A58B8 / 0x823A5904).
    ProcessGameEventsGuiStartedGameBringUp(&mGameEventCarryQueue, lpActionQueue);

    mbIsUpdating = false;
    mpOutputBuffer->UnlockForWrite();
}

// ============================================================================
// ⭐⭐ ProcessGameEventsGuiStartedGameBringUp -- the QUEUE WALK that feeds the extracted
// case-78 arm below. Same shape as the case-111 / 113 / 115 / pause arms in
// GameStateModule_gUI_00.cpp: the console's dispatcher makes ONE pass over the merged event
// queue and switches; this tree extracts one arm per function and each does its own walk.
//
// ⚠️ IT DOES NOT Clear() THE QUEUE. PreWorldUpdateStuntBringUp owns the console's Clear (the
// one-frame-buffer invariant), later in the same sub-step, and the other arms must still see
// this frame's events. This walk is read-only over the same content they will read.
//
// Event 78 is a bare 1-byte SIGNAL -- BridgeGuiToGameState @0x823DDB78 case 145 emits
// `liType = 78; liSize = 1` with an uninitialised payload byte -- so there is nothing to
// decode; the arrival IS the message.
// ============================================================================
void GameStateModule::ProcessGameEventsGuiStartedGameBringUp(
        const CgsModule::VariableEventQueue<1536, 16>* lpGameEventQueue,
        GameStateModuleIO::GameActionQueue* lpActionQueue)
{
    if (lpGameEventQueue == 0 || lpActionQueue == 0)
    {
        return;
    }

    const CgsModule::Event* lpEvent = 0;
    s32                     liSize  = 0;
    s32                     liType  = lpGameEventQueue->GetFirstEvent(&lpEvent, &liSize);

    while (lpEvent != 0)
    {
        if (liType == GameStateModuleIO::E_GUI_HAS_STARTED_GAME)
        {
            // [DIAG] NOT IN THE X360 BINARY. The one rung that separates "the GUI never told us"
            // from "it told us and the latch was down" -- the exact ambiguity that hid this
            // defect behind a stand-in for a week [[diagnostics-that-lie]]. One line per event.
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[jyentry] game event 78 (GUI has started game) received; waiting="
                    << (mbWaitingToPutPlayerInJunkyard ? 1 : 0) << "\n";
            }

            // The console's case-78 arm. Its own gate (mbWaitingToPutPlayerInJunkyard) is inside.
            ProcessGameEventsReallyEnterJunkyardBringUp(lpActionQueue);
        }

        // GetNextEvent takes the CURRENT event and writes the next one through its second
        // parameter; sequenced through a local so the two uses of lpEvent do not alias.
        const CgsModule::Event* lpCurrent = lpEvent;
        liType = lpGameEventQueue->GetNextEvent(lpCurrent, &lpEvent, &liSize);
    }
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

// ============================================================================
// PreWorldUpdatePublishControllerActiveBringUp -- the extracted CONTROLLER-ACTIVE publish
// of PreWorldUpdate @0x823A5328. See the header for the FLAG and the measurement.
// ============================================================================
void GameStateModule::PreWorldUpdatePublishControllerActiveBringUp()
{
    if (mpOutputBuffer == 0)
    {
        return;
    }

    // The console holds the output buffer's write lock across this whole tail of PreWorldUpdate
    // (LockForWrite is taken near the top, at the same place the car-select leg takes it in its
    // own extraction), so the store is made under the same lock here.
    mpOutputBuffer->LockForWrite();
    mpOutputBuffer->SetControllerActive(IsControllerActive());
    mpOutputBuffer->UnlockForWrite();
}

// ============================================================================
// PreWorldUpdateCarSelectBringUp -- the extracted CAR-SELECT leg of PreWorldUpdate
// @0x823A5328 (0x823A5904..0x823A5958). See the header for the FLAG and the asm.
// ============================================================================
void GameStateModule::PreWorldUpdateCarSelectBringUp(f32 lfGameTimestep)
{
    if (mpOutputBuffer == 0)
    {
        return;
    }

    // The console's gate: a 64-bit load of CarSelectManager::mJunkyardId (this + 0x2CDC0 ==
    // mCarSelectManager + 0x20), non-zero == "the player is in a junkyard".
    if (!mCarSelectManager.IsInJunkyard())
    {
        return;
    }

    mpOutputBuffer->LockForWrite();
    GameStateModuleIO::GameActionQueue* lpActionQueue = mpOutputBuffer->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue != NULL");   // BrnGameStateModule.cpp:1149
    mbIsUpdating = true;
    // [FLAG PC bring-up] stand-in for the world's StreamingCompleteEvent -- see
    // CarSelectManager::UpdateExitStreamingBringUp. Runs immediately before Update so the
    // console's own case-9 arm (UpdateExitState) sees the cleared latch in the SAME sub-step,
    // which is what happens on the console when the exit needs no new streaming.
    mCarSelectManager.UpdateExitStreamingBringUp(lpActionQueue);
    mCarSelectManager.Update(lpActionQueue, 0, lfGameTimestep);
    mbIsUpdating = false;
    mpOutputBuffer->UnlockForWrite();
}

// ============================================================================
// ProcessGameEventsActivateCarSelectBringUp -- the extracted case-94 JUNKYARD arm of
// ProcessGameEvents @0x823A0A18. See the header for the FLAG and the (action, type) proof.
// ============================================================================
void GameStateModule::ProcessGameEventsActivateCarSelectBringUp(s32 liAction, s32 liCarSelectType)
{
    // The three ACTION values the console's inner switch recognises. Left as TU-local constants
    // rather than promoted into an enum: the X360 switch is over bare integers and no DWARF
    // enum for them has been found, so naming them publicly would be a fabricated type.
    const s32 KI_CAR_SELECT_ACTION_START  = 0;   // -> StartCarSelectState
    const s32 KI_CAR_SELECT_ACTION_MODIFY = 1;   // -> EnterModification
    const s32 KI_CAR_SELECT_ACTION_EXIT   = 4;   // -> ExitJunkyard

    // The console's outer pivot: word1 selects the manager. 1 == the junkyard
    // (CarSelectManager), 2 == the online event (OnlineCarSelectManager, not wired here), and
    // anything else is the console's own "Unknown car select type" assert @cpp:3302.
    if (liCarSelectType != GameStateModuleIO::E_CAR_SELECT_TYPE_JUNKYARD)
    {
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[GameStateModule::ProcessGameEvents case 94] car-select type " << liCarSelectType
                << " is not the junkyard -- the OnlineCarSelectManager arm is not plumbed on this"
                   " build (FLAG).\n";
        }
        return;
    }

    if (mpOutputBuffer == 0)
    {
        return;
    }

    // The console's own gate for this arm (BrnGameStateModule.cpp:3222).
    CGS_ASSERT(mCarSelectManager.IsInJunkyard(), "mCarSelectManager.IsInJunkyard()");
    if (!mCarSelectManager.IsInJunkyard())
    {
        return;
    }

    mpOutputBuffer->LockForWrite();
    GameStateModuleIO::GameActionQueue* lpActionQueue = mpOutputBuffer->GetGameActionQueue();
    CGS_ASSERT(lpActionQueue != 0, "lpActionQueue != NULL");
    mbIsUpdating = true;

    switch (liAction)
    {
    case KI_CAR_SELECT_ACTION_START:          // 0
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint
                << "[GameStateModule::ProcessGameEvents case 94] action 0 -> StartCarSelectState\n";
        mCarSelectManager.StartCarSelectState(lpActionQueue);
        break;

    case KI_CAR_SELECT_ACTION_MODIFY:         // 1
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint
                << "[GameStateModule::ProcessGameEvents case 94] action 1 -> EnterModification\n";
        mCarSelectManager.EnterModification(lpActionQueue);
        break;

    case KI_CAR_SELECT_ACTION_EXIT:           // 4
        if (CgsDev::Log::gpDebugPrint != 0)
            *CgsDev::Log::gpDebugPrint
                << "[GameStateModule::ProcessGameEvents case 94] action 4 -> ExitJunkyard\n";
        mCarSelectManager.ExitJunkyard(lpActionQueue);
        break;

    default:
        // The console's formatted default assert (BrnGameStateModule.cpp:3245).
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("Unknown car select action",
                                   "GameSource/GameState/BrnGameStateModule.cpp", 3245);
        CgsDev::Assert::EndAssert();
        break;
    }

    mbIsUpdating = false;
    mpOutputBuffer->UnlockForWrite();
}

// ================================================================================================
// ⭐⭐⭐ [A9 scoring-feed wave 2026-08-27] GameStateModule::CopyScoringDataToOutput -- X360
// 0x8236CDC0, REAL and WHOLE (not an extracted leg). See the declaration in BrnGameStateModule.h
// for why this one function unblocks the entire event score/timer feed, and for the ONE named PC
// deviation (the frame's Time arrives through a TimerStatusInterface argument instead of through
// the module's copy of the PreWorldInputBuffer timer block at gsm+208328).
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
        GameStateModuleIO::OutputBuffer* lpOutput,
        const CgsSystem::TimerStatusInterface& lrTimerStatusInterface)
{
    if (lpOutput == 0)
    {
        return;
    }

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
    // ⚠️ EXPLICITLY GLOBAL-QUALIFIED, type AND enumerators. Two distinct
    // `enum EActiveRaceCarIndex : s32` live in this tree -- the global one (BurnoutConstants.h,
    // which the race-car output interface and ModeManager::WriteDataToOutput both take) and
    // BrnGameState::EActiveRaceCarIndex (BrnTakedownManagerTypes.h). Unqualified inside
    // `namespace BrnGameState` the enumerators bind to the WRONG one. Same pin
    // BrnGameStateModuleIO.h's GetActivePaybackAggressor already carries, and for the same reason.
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
    const CgsSystem::Time lTimeNow = lrTimerStatusInterface.GetSimTimerStatus()->GetTime();

    if (lpScoringSystem->IsTimeLimitActive())
    {
        // `Time::operator-(&tmp, &now, scoring+0)` then float(seconds)+fraction -> out+0xA90.
        // (The X360 also carries a dead alternative arm reading scoring+0x10 (mTotalTime) for the
        //  mStartTime.miSeconds < 0 case -- unreachable behind the gate above, so it is not
        //  reproduced. Named, not silently dropped.)
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
// *(this+292352))); that function has no call site on this build, so on the X360 this runs on
// every showtime / stunt-attack frame and here it ran never. Reached now from
// PostWorldUpdateStuntBringUp's LEG 5, at the console's own position.
//
// [FLAG PC bring-up] ARGUMENT REDUCED, BODY COMPLETE. The console parameter is
// `const PostWorldInputBuffer*` and the ONLY thing the body ever reads from it is
// GetContactSpyInterface() (sub_82362988 -> +0x6E30, BrnGameStateModuleIO.h:204), called three
// times; everything else comes from `this`. See the declaration's banner in the header.
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
        static const bool sbWatch = (getenv("BRN_SHOWTIME_WATCH") != 0);
        static s32        siEntryFrame    = 0;
        static s32        siMaxProps      = 0;
        static s32        siMaxTraffic    = 0;
        static s32        siPropFrames    = 0;
        static s32        siTrafficFrames = 0;
        static s32        siTotalTraffic  = 0;

        if (sbWatch && CgsDev::Log::gpDebugPrint != 0 &&
            lpContactSpyInterface != 0 && lpContactSpyInterface->IsValid())
        {
            const s32 liProps   = lpContactSpyInterface->GetPropContacts()->GetLength();
            const s32 liTraffic = lpContactSpyInterface->GetTrafficContacts()->GetLength();
            if (liProps   > siMaxProps)   { siMaxProps   = liProps; }
            if (liTraffic > siMaxTraffic) { siMaxTraffic = liTraffic; }
            if (liProps   > 0) { ++siPropFrames; }
            if (liTraffic > 0) { ++siTrafficFrames; siTotalTraffic += liTraffic; }
        }

        if (sbWatch && CgsDev::Log::gpDebugPrint != 0 && (siEntryFrame++ % 120) == 0)
        {
            const GameMode* const lpMode = mModeManager.GetCurrentGameMode();
            *CgsDev::Log::gpDebugPrint
                << "[contact-entry] frames=" << siEntryFrame
                << " spy=" << ((lpContactSpyInterface != 0) ? 1 : 0)
                << " valid=" << ((lpContactSpyInterface != 0 && lpContactSpyInterface->IsValid()) ? 1 : 0)
                << " mode=" << static_cast<s32>(mModeManager.GetCurrentGameModeType())
                << " state=" << ((lpMode != 0) ? static_cast<s32>(lpMode->GetCurrentState()) : -99)
                << " | props: max=" << siMaxProps << " frames=" << siPropFrames
                << " | traffic: max=" << siMaxTraffic << " frames=" << siTrafficFrames
                << " total=" << siTotalTraffic
                << "\n";
        }
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
        static const bool sbWatch = (getenv("BRN_SHOWTIME_WATCH") != 0);
        static s32        siFrame = 0;
        if (sbWatch && CgsDev::Log::gpDebugPrint != 0 && (siFrame++ % 120) == 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[contact-pass] gatedFrames=" << siFrame
                << " mode=" << static_cast<s32>(leGameModeType)
                << " state=" << static_cast<s32>(lpCurrentGameMode->GetCurrentState())
                << " playerIdx=" << static_cast<s32>(GetPlayerActiveRaceCarIndex())
                << " props=" << lpPropContacts->GetLength()
                << " traffic=" << lpTrafficContacts->GetLength()
                << "\n";
        }
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
    CGS_ASSERT(lpCrashScorer != 0, "lpCrashScorer != NULL");   // BrnGameStateModule.cpp:6760
    CGS_ASSERT(lpStuntScorer != 0, "lpStuntScorer != NULL");   // BrnGameStateModule.cpp:6761

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
    // not reconstructed) to turn into a traffic-type request and finally a score. The stack's
    // banner in BrnGameStateModule.h carries the whole seven-hop chain.
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

            // [DIAG] NOT IN THE X360 BINARY. The bounded witness for this leg, and the only
            // evidence available for it: the crash COUNT cannot move until four more hops land,
            // so there is no pixel to film here. One line per detected victim; the stack fills
            // to 8 and stops, so it cannot flood. BRN_SHOWTIME_WATCH is the same opt-in the
            // showtime physics witness in RaceCarPhysics::Update uses.
            {
                static const bool sbWatch = (getenv("BRN_SHOWTIME_WATCH") != 0);
                if (sbWatch && CgsDev::Log::gpDebugPrint != 0)
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[showtime-crash] traffic car " << static_cast<s32>(luVictimTrafficIndex)
                        << " crashed; combo=" << lpCrashScorer->GetCurrentComboCount()
                        << " pending=" << mShowtimePendingTrafficIndexStack.GetLength()
                        << " (no consumer yet -- UpdateShowtimeMode @0x82380EF8 unreconstructed)\n";
                }
            }

            if (mShowtimePendingTrafficIndexStack.IsFull())
            {
                break;
            }
        }
    }
}

}
