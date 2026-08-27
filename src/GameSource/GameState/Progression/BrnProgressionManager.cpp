// ===================================================================================
// BrnProgression::ProgressionManager  -- the offline progression / unlock manager.
//   GameSource/Unity/../GameState/Progression/BrnProgressionManager.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the ASM is the spine: signatures, branches,
// constants and side-effects are taken from the disassembly, not the Hex-Rays pseudocode).
// The nine functions homed here are the boot-trace + caller-named slice of the manager; the
// full ProgressionManager TU is much larger. Every member is reached BY NAME through the
// minimal layout in BrnProgressionManager.h -- no raw-offset pointer arithmetic.
//
// NOTE on the X360 byte offsets quoted in BrnProgressionManager.h: they are the 32-bit-pointer
// ABI offsets proven from the XEX. The PC reconstruction compiles 64-bit, so the embedded
// Profile and pointer members are naturally wider; behaviour is identical because every access
// is by name.
// ===================================================================================

#include "BrnProgressionManager.h"
#include "BrnProfile.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                        // CgsIDCompress (AddCar's "CARBEAGT" test)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"            // CgsDev::Log::gpDebugPrint (the two FLAG lines)
#include "BrnProgressionCarData.h"                                    // BrnProgression::CarData (colour/palette/unlock type)
#include "SharedClasses/Progression/BrnProgressionData.h"             // BrnProgression::ProgressionData (rank count)
#include "SharedClasses/Progression/BrnRaceEventData.h"               // BrnProgression::EventJunction / RaceEventData (the event-list producer)
// [stuntrace waveB MOUNT-CLOSURE round, 2026-08-26] RE-POINTED, exactly as the note that stood
// here asked. GetRankThresholdForEvent dereferences the per-rank record, so it needs
// BrnProgression::ProgressionRankData COMPLETE -- BrnProgressionData.h only forward-declares it
// (:41). The canonical home now exists and carries the full 112-byte layout plus every accessor
// body, so the rank record comes from IT rather than from the retired BrnGameModeParams.h
// stand-in. BrnGameModeParams.h is still included below, but only for StartGameModeParams
// (GetStuntRunScoreTarget's `lpStartGameModeParams->GetEventData()`).
#include "SharedClasses/Progression/BrnProgressionRankData.h"             // BrnProgression::ProgressionRankData (real, single owner)
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // BrnGameState::StartGameModeParams (+ RaceEventData via its own include)
#include "SharedClasses/Trigger/BrnTriggerData.h"                         // BrnTrigger::TriggerData (FindLandmarkAISectionIndex's landmark count)
#include "GameSource/Math/BrnMathUtils.h"                             // BrnMath::RoundWithNumSignificantFigures (GetStuntRunScoreTarget)
#include "SharedClasses/DataLists/VehicleList.h"                      // BrnResource::VehicleList (GetVehicleIndex / GetVehicleData)
#include "SharedClasses/DataLists/VehicleListEntry.h"                 // BrnResource::VehicleListEntry (livery type / parent id)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                // OutputBuffer::GetResourceRequestInterface
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"     // RequestInterface<3072>::LoadBundle / AcquireResource
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"  // CgsModule::EventReceiverQueue<3072,16>
#include "GameShared/GameClasses/System/Resource/CgsResourceIOEvents.h" // CgsResource::Events::AcquireResourceResponse
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle

#include <string.h>   // memcpy (X360 XMemCpy)

namespace BrnProgression
{
// The verbatim X360-baked source path this TU's asserts reference.
static const char* const KAC_PROGMGR_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

// [stuntrace waveB CLOSURE round] GetStuntRunScoreTarget's rounding precision. X360 @0x8237BDF4
// `lfs f31, flt_82001D9C` feeds it as the second argument to
// BrnMath::RoundWithNumSignificantFigures. IMAGE-CITED: image.bin (VA - 0x82000000, big-endian)
// offset 0x1D9C reads 40 00 00 00 == 2.0f exactly -- i.e. round the interpolated stunt target to
// TWO significant figures, which is why shipped stunt-race targets are values like 34,000.
static const f32 KF_STUNT_TARGET_SIGNIFICANT_FIGURES = 2.0f;


// ------------------------------------------------------------------------------------
// ProgressionManager::ProgressionManager  @ 0x827DEA50  (EXECUTED in the boot trace)
//
// X360 ctor. It performs three groups of stores:
//   (1) resets the 18 manager-handle head slots to the -1 sentinel (the ctor loop:
//       18 stores of -1 at +0x10, stride 0x14);
//   (2) marks the embedded Profile's index->element containers "unconstructed" by poking
//       the -1 sentinel into their count words (the freeburn-challenge array + the five
//       mugshot arrays + the late trophy/achievement tail). FLAG: those count words are
//       PRIVATE Profile state; the X360 reaches them as raw interior offsets while inlining.
//       This reconstruction does NOT poke Profile internals by raw offset (forbidden) -- the
//       embedded Profile is default-constructed and its container-sentinel reset is owned by
//       the Profile TU's own Construct/Clear path. The observable manager-level state the
//       boot trace depends on (head slots, debug component, event lists) is reproduced below.
//   (3) installs the progression debug component's vtable and empties the two intrusive
//       event lists to their self-referential sentinel state.
// ------------------------------------------------------------------------------------
ProgressionManager::ProgressionManager()
{
    // (1) 18 head handle slots -> -1 "unset" sentinel.
    for (s32 liSlot = 0; liSlot < KI_HANDLE_SLOT_COUNT; ++liSlot)
    {
        maHandleSlots[liSlot].mi32Id = -1;
    }

    // (3a) install the debug component's vtable (X360: result[33250] = off_820CDE4C). The
    // real ProgressionDebugComponent vptr is supplied when Prepare2 constructs it; the ctor
    // only seeds the slot so an early teardown sees a valid component. FLAG: the X360 vtable
    // symbol off_820CDE4C is not yet homed; modelled as the seeded-null slot.
    mDebugComponent.mpVTable = nullptr;

    // ⚠️⚠️ [drive-thru wave 2026-08-27] THE TROPHY QUEUE MUST BE CLEARED HERE, and this is a
    // HOST-ONLY initialisation site, not an invented behaviour. The console clears it in
    // ProgressionManager::Construct @0x8237A5F8 (`*(a1 + 133320) = 0`, the count word) -- but
    // that Construct is NOT reconstructed, and Array<T,N> has no default constructor, so on the
    // host miCount would start as stack garbage. That is not inert: Append's own
    // "Array used before Construct/Clear was called" guard tests miCount != -1, which garbage
    // passes, and the next line writes maElements[garbage]. [[valid-pointer-invalid-object]] --
    // the guard is satisfied by a value that is not a count.
    // ⭐ MOVE-WHEN ProgressionManager::Construct lands; this Clear belongs there.
    mQueueOfTrophyCarUnLocks.Clear();

    // road-rules ruled tallies start clear (the ctor leaves the +133456..+133464 region 0).
    miNumberOfParCrashRoadRulesRuledByPlayer         = 0;
    miNumberOfParTimeRoadRulesRuledByPlayer          = 0;
    miNumberOfNumberOfCompleteRoadRulesRuledByPlayer = 0;

    // Prepare2 back-pointers start null.
    mpTriggerData        = nullptr;
    mpGameStateModule    = nullptr;
    mpAchievementManager = nullptr;

    // [stuntrace waveB MOUNT-CLOSURE round] The landmark -> AI-section cache starts blank.
    // ⚠️ FLAG (HOST-ONLY INITIALISATION, same precedent as the five zero-initialised pointer
    // members in the header): the X360 does NOT clear this table in the ctor, because the console
    // ProgressionManager is BSS-resident and therefore already zero, and because Prepare2's
    // ComputeLandmarkAISectionIndices overwrites every live entry before anything reads one. On
    // the host this class is a by-value sub-object of GameStateModule inside BrnGameModule, so the
    // 4 KB would otherwise start as stack/heap garbage -- and FindLandmarkAISectionIndex COMPARES
    // against mId, so garbage there is not inert: it could hand a caller a fabricated AI-section
    // index instead of the console's "not found" answer. Clearing reproduces the console's
    // starting state exactly; it is an initialisation-site difference, not a behavioural one.
    for (s32 liEntry = 0; liEntry < KI_LANDMARK_AI_SECTION_INDEX_COUNT; ++liEntry)
    {
        maLandmarkAISectionIndices[liEntry].mId              = 0;
        maLandmarkAISectionIndices[liEntry].muAISectionIndex = 0;
    }

    // (3b) the two resource pointers (mpProgressionData / mpAISectionData) reach the
    // X360 ctor's self-referential sentinel state (count-0/self-link/zero pattern)
    // through their BaseResourcePtr default ctors -- no explicit stores needed here.
}

// ------------------------------------------------------------------------------------
// ProgressionManager::LoadProgressionData  @ 0x82399ED0   ⭐ THE PROGRESSION.DAT LOADER
//
// The resumable five-stage machine Prepare2 gates on. Structurally the twin of
// TriggerQueryManager::Prepare @0x82398218 (LoadBundle -> acquire -> bind), and it is what
// makes ProgressionManager::GetProgressionData() answer non-null: nothing else in the whole
// image writes mpProgressionData (X360 `a1 + 133348`).
//
//   0 NOT_STARTED       : queue.Clear(); LoadBundle(&queue, 1, pool 5,
//                         byte_82FFA7F1 ? "BttProgression.dat" : "Progression.dat", useHDCache 0);
//                         stage = 1; FALL THROUGH into case 1 (the console `goto LABEL_6`)
//   1 BUNDLE_REQUESTED  : reply not in yet -> return false. Otherwise stage = 2 and fall through.
//   2 BUNDLE_LOADED     : queue.Clear(); AcquireResource(&queue, 1, pool 5, "ProgressionData");
//                         stage = 3; FALL THROUGH into case 3 (the console `goto LABEL_11`) --
//                         which then finds the queue empty on this tick and returns false.
//   3 ACQUIRE_REQUESTED : reply not in yet -> return false. Otherwise walk EVERY queued event and
//                         CreateFromHandle(&mpProgressionData, &response.mHandle); stage = 4;
//                         return true.
//   4 DONE              : return true.
//   default             : assert "ProgressionManager::meLoadStage in a weird state" (line 2799)
//                         then return TRUE (the console falls into LABEL_17, not into a false).
//
// ⚠️ The Hex-Rays `HashString("ProgressionData") | 0x500000000LL` is the same known store-fusion
// artifact TriggerQueryManager::Prepare's note documents: pool id 5 and the zero-extended 32-bit
// resource id are two INDEPENDENT stores into the acquire record. AcquireResource(...) builds it.
// ------------------------------------------------------------------------------------
namespace
{
    // The console's bundle/resource names, read off the X360 rodata the switch loads.
    const char* const KPC_PROGRESSION_FILE_NAME     = "Progression.dat";
    const char* const KPC_BTT_PROGRESSION_FILE_NAME = "BttProgression.dat";
    const char* const KPC_PROGRESSION_RESOURCE_NAME = "ProgressionData";

    // Both legs use event id 1 (`li r28, 1`); pool 5 == the GameData pool.
    const s32 KI_PROGRESSION_EVENT_ID = 1;
    const s32 KI_PROGRESSION_POOL_ID  = 5;

    // X360 byte_82FFA7F1 -- the "Beat the team" mode flag. It is a PROGRAM-WIDE console global,
    // not a ProgressionManager member: its other three readers are DLCDebugComponent::RenderHUD
    // @0x826629C8 (which draws the literal "Beat the team mode") and the two
    // DeveloperChallengeManager entry points @0x8238D698 / @0x823674B8. None of those TUs is
    // reconstructed, so the flag has no shared home yet and is modelled here at its retail
    // default (clear -> the retail Progression.dat, not the BTT variant).
    // FLAG: promote this to the shared home when the DeveloperChallengeManager TU lands.
    bool gbBeatTheTeamMode = false;
}

bool ProgressionManager::LoadProgressionData(BrnGameState::GameStateModuleIO::OutputBuffer* lpOutput,
                                             CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue)
{
    switch (meLoadStage)
    {
    case E_LOADSTAGE_NOT_STARTED:
    {
        const char* const lpcFileName =
            gbBeatTheTeamMode ? KPC_BTT_PROGRESSION_FILE_NAME : KPC_PROGRESSION_FILE_NAME;

        lpReceiverQueue->Clear();
        lpOutput->GetResourceRequestInterface()->LoadBundle(
            lpReceiverQueue, KI_PROGRESSION_EVENT_ID, KI_PROGRESSION_POOL_ID,
            lpcFileName, /*lbUseHDCache*/ false);
        meLoadStage = E_LOADSTAGE_BUNDLE_REQUESTED;
    }
        // fall through -- the X360 `goto LABEL_6` drops into the poll on the same tick.

    case E_LOADSTAGE_BUNDLE_REQUESTED:
        if (lpReceiverQueue->GetCount() == 0)
            return false;
        meLoadStage = E_LOADSTAGE_BUNDLE_LOADED;
        // fall through.

    case E_LOADSTAGE_BUNDLE_LOADED:
        lpReceiverQueue->Clear();
        lpOutput->GetResourceRequestInterface()->AcquireResource(
            lpReceiverQueue, KI_PROGRESSION_EVENT_ID, KI_PROGRESSION_POOL_ID,
            KPC_PROGRESSION_RESOURCE_NAME);
        meLoadStage = E_LOADSTAGE_ACQUIRE_REQUESTED;
        // fall through -- the console does NOT return here (`goto LABEL_11`); the queue it just
        // cleared is empty, so the poll below returns false on this tick.

    case E_LOADSTAGE_ACQUIRE_REQUESTED:
    {
        if (lpReceiverQueue->GetCount() <= 0)
            return false;

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        lpReceiverQueue->GetFirstEvent(&lpEvent, &liSize);

        while (lpEvent != 0)
        {
            // reinterpret_cast, not static_cast: CgsResource::Events::Event and CgsModule::Event
            // are unrelated roots and the receiver queue hands out the module one (the same idiom
            // TriggerQueryManager::Prepare uses for its own acquire reply).
            const CgsResource::Events::AcquireResourceResponse* lpResponse =
                reinterpret_cast<const CgsResource::Events::AcquireResourceResponse*>(lpEvent);

            // X360 `CreateFromHandle(a1 + 133348, v13 + 24)` -- payload +0x18 is the response's
            // {mpResourceMemory, mpSourceEntry} pair, i.e. a ResourceHandle. Read BY MEMBER: the
            // host handle is 16 bytes where the console's is 8, so every literal offset shifts.
            CgsResource::ResourceHandle lHandle;
            lHandle.mpResourceMemory = lpResponse->mpResourceMemory;
            lHandle.mpSourceEntry    = lpResponse->mpSourceEntry;
            mpProgressionData = lHandle;   // ResourcePtr::operator=(handle) -> CreateFromHandle

            const CgsModule::Event* lpNext = 0;
            lpReceiverQueue->GetNextEvent(lpEvent, &lpNext, &liSize);
            lpEvent = lpNext;
        }

        meLoadStage = E_LOADSTAGE_DONE;

        // [diagnostic, one-shot] the same shape as TriggerQueryManager's LOADED line. RESIDENT IS
        // NOT USABLE: the counts are what prove the platform-4 port + FixUp landed (every table
        // base in the payload is a serialised 32-bit offset only FixUp rebases), and the rank
        // count in particular is what ProgressionManager::GetProgressionRank clamps against.
        // Delete with the rest of the bring-up diagnostics.
        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[ProgressionManager] LOADED -- progressionData="
                << (mpProgressionData.HasMemoryResource() ? 1 : 0) << "\n";
            if (mpProgressionData.HasMemoryResource())
            {
                const ProgressionData* lpData = mpProgressionData.operator->();
                *CgsDev::Log::gpDebugPrint
                    << "[ProgressionManager] ranks="    << lpData->GetProgressionRankCount()
                    << " junctions=" << lpData->GetEventJunctionCount()
                    << " rivals="    << lpData->GetRivalCount() << "\n";
            }
        }
        return true;
    }

    case E_LOADSTAGE_DONE:
        return true;

    default:
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("ProgressionManager::meLoadStage in a weird state",
                                   KAC_PROGMGR_FILE, 2799);
        CgsDev::Assert::EndAssert();
        // The console falls into LABEL_17 -- `result = 1` -- so a corrupt stage word reports DONE.
        return true;
    }
}

// ------------------------------------------------------------------------------------
// ProgressionManager::Prepare2  @ 0x8239DC98
//
// Two-phase load. Asserts the output + receiver-queue pointers, loads the progression data;
// on success wires the trigger-data + game-state-module + achievement back-pointers, computes
// the landmark AI-section indices, processes the loaded preset races, constructs + registers
// the debug component, and sets up the roaming sections. Returns true iff the load succeeded.
//
// ⭐ THE LOAD IS REAL NOW (2026-08-11): LoadProgressionData above is reconstructed, so this
// function's `if (LoadProgressionData(...))` gate is the console's gate, not a pass-through.
// FLAG (still deferred): ComputeLandmarkAISectionIndices / ProcessLoadedPresetRaces /
// ProgressionDebugComponent::Construct / DebugComponent::Register / SetupRoamingSections are
// sibling functions in other (not-yet-reconstructed) ProgressionManager TUs. The ASM proves the
// call sequence + the three stores; the helper bodies are out of scope for this slice, so the
// orchestration is documented rather than dispatched (calling undeclared siblings would not
// compile). The three back-pointer stores -- the observable side effects -- are reproduced.
// ------------------------------------------------------------------------------------
bool ProgressionManager::Prepare2(BrnGameState::GameStateModuleIO::OutputBuffer* lpOutput,
                                  void* lpGameStateModule,
                                  CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue,
                                  void* lpTriggerData,
                                  BrnGameState::AchievementManagerBase* lpAchievementManager)
{
    CGS_ASSERT(lpOutput != nullptr, "lpOutput");            // X360 BrnProgressionManager.cpp:249
    CGS_ASSERT(lpReceiverQueue != nullptr, "lpReceiverQueue");  // X360 :250

    // ⭐ THE PROFILE BOOT SEAM (landed 2026-08-24, deform-land wave). Profile::Construct
    // @0x823708A8 was bodied but had NO caller anywhere in the tree, so the embedded
    // mProfile booted as raw storage -- BOOT-MEASURED: mbIsNewProfile read 0 at junkyard
    // entry ([deform-preset] probe newProfile=0), which silently disabled the start-of-game
    // 0.85 junkyard deform. The console constructs the profile on this manager's own
    // construct/prepare path (idat xrefs: ProgressionManager::Construct @0x8237A74C and the
    // outer Prepare's call @0x8239DC78, immediately before this Prepare2 body); neither outer
    // is reconstructed, so the call lands here at the same boot position, guarded to run once.
    if (!mbProfileConstructed)
    {
        mbProfileConstructed = true;
        mProfile.Construct();
    }

    // X360: if ( LoadProgressionData(this, lpOutput, lpReceiverQueue) ) { ... } else return false;
    if (!LoadProgressionData(lpOutput, lpReceiverQueue))
    {
        return false;
    }

    CGS_ASSERT(lpTriggerData != nullptr, "lpTriggerData");
    mpTriggerData     = lpTriggerData;           // X360 +0x20924 (a5)
    mpGameStateModule = lpGameStateModule;       // X360 +0x2093C (a3)

    CGS_ASSERT(lpAchievementManager != nullptr, "lpAchievementManager");
    mpAchievementManager = lpAchievementManager; // X360 +0x20938 (a6)

    // X360 then calls, in order:
    //   ComputeLandmarkAISectionIndices(this);
    //   ProcessLoadedPresetRaces(this);
    //   ProgressionDebugComponent::Construct(&mDebugComponent, this, lpGameStateModule);
    //   CgsDev::DebugComponent::Register(&mDebugComponent);
    //   SetupRoamingSections(this, ...);
    // (helper bodies land with their own TUs; see FLAG above.)

    // ⭐⭐ THE PROFILE EVENT-LIST SEAM (landed 2026-08-27, D1 wave) -- the twin of the
    // Profile::Construct seam above, and the reason it has to sit HERE rather than up there:
    // the population reads the loaded ProgressionData, so it cannot run until the acquire
    // above has bound mpProgressionData.
    //
    // WHAT WAS BROKEN: Profile::Construct zeroes miEventCount, and the ONLY console writer of
    // the ProfileEvent table -- UnlockToProgressionRank, the single xref to Profile::AddEvent
    // @0x82359EB8 in the whole XEX -- had no caller on this build. So mProfile.GetEventCount()
    // stayed 0 for the entire run, every id->record lookup answered NULL, and winning an
    // offline event fired `lpEvent` (BrnProgressionManager.cpp:1669) in
    // OnEventFinishUpdateProfile and then crashed on the null record.
    //
    // THE CONSOLE SEAT is PreWorldUpdate @0x823A4F68 -> `if (mbMedalsUpdateRequested)` ->
    // UpdatePlayerMedals @0x8239FE50, which computes CalculateRankFromMedalTotal(0 medals) == 0,
    // sees it above the profile's -2 "rank not set" seed, and calls UnlockToProgressionRank(0).
    // Neither PreWorldUpdate nor UpdatePlayerMedals is reconstructed, so the rank-0 call is made
    // from this seam at the same boot position, latched to run once. The population itself is
    // idempotent (it skips any junction the profile already holds a record for), which is the
    // console's own guarantee -- the latch only keeps the arms AROUND it single-shot.
    // DELETE-WHEN UpdatePlayerMedals + PreWorldUpdate land.
    //
    // The HasMemoryResource() test guards THIS SEAM, not console code: LoadProgressionData also
    // answers true from its corrupt-stage default arm (which fires its own :2799 assert and
    // reports DONE), and the rank-0 arm dereferences mpProgressionData unconditionally the way
    // the console can afford to. It is a [PC GUARD] on a PC-invented call site.
    // DELETE-WHEN this call moves to its console seat in UpdatePlayerMedals.
    if (!mbInitialRankUnlockDone && mpProgressionData.HasMemoryResource())
    {
        mbInitialRankUnlockDone = true;
        UnlockToProgressionRank(0, 0);
    }

    return true;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::UnlockToProgressionRank  @ 0x8239DDE8
// DWARF BrnProgressionManager.h:674 -- `void UnlockToProgressionRank(int8_t,
// InputBuffer::GameActionQueue*);`
//
// ⭐ THE PROFILE EVENT-LIST PRODUCER. Profile::AddEvent @0x82359EB8 has EXACTLY ONE xref in
// BURNOUT_X360_ARTIST.XEX and it is the loop below: this function IS how a Burnout Paradise
// profile comes to hold one ProfileEvent per authored event junction.
//
// THE ASM, ARM BY ARM (0x8239DDE8..0x8239E0F8+):
//   0x8239DE14  the `!PlayerHasFinishedLastRank()` assert (:965) -- fires BEFORE either arm.
//   0x8239DE4C  `extsb r21, r4 / cmpwi 0 / bne` -- the rank argument selects the two bodies.
//   0x8239DE60  RANK-0 ARM: walk the ProgressionData event-junction table (count +0x1C, base
//               +0x18, 16-byte stride). Per junction: skip it when its OFFLINE event slot
//               (+0x04) is null; otherwise open-code the profile's id->record scan and, on a
//               MISS, `Profile::AddEvent(junction id)` + `AddEventTypeToEventTotals(junction)`.
//               Then UnlockDefaultPlayerCars and the starting-drive-thru/trophy tail.
//   0x8239E034  RANK-N ARM: AchievementManagerBase::OnLicenseUpgrade + the licence-upgrade
//               telemetry event (id 228, size 20).
//   0x8239E094  the shared rank tail: clamp the cached rank byte, pick the lowest car-type
//               affinity, walk the cached rank up to the requested one, mirror it onto the
//               profile, zero the three car-type affinities, ClearMedalsOnRankUp, and clear
//               the 18 per-rank completed counts.
//
// ⛔ HONEST PARTIAL -- WHAT IS LANDED AND WHAT IS PARKED.
// LANDED: the :965 assert and the whole rank-0 EVENT-LIST arm (the junction walk, the
//   duplicate test, AddEvent and AddEventTypeToEventTotals). That is the arm this build's
//   single caller uses and the arm the whole `lpEvent`-assert class hangs on.
// PARKED, each on a sibling that has NO body anywhere in b5-decomp/src (landing the calls
//   would add unresolved externals to a MOUNTED TU -- the F2 failure mode this campaign keeps
//   re-learning), with the console body written out for whoever lands it:
//   Q1  UnlockDefaultPlayerCars @0x8237BF98 (absent) -- the rank-0 starting-garage fill.
//   Q2  the starting-drive-thru tail: `if (!mBodyShopsDriveThruSet.Contains(0x6C72D)) {
//         mBodyShopsDriveThruSet.Insert(0x6C72D);
//         if (GetLength() == 11) { OnTrophyUnlock(20);
//                                  if (mProfile.AreAllDriveThrusCompleted()) OnTrophyUnlock(21); }
//         if (mProfile.AreAllDriveThrusCompleted() && !mpAchievementManager->IsAwarded(32))
//             mpAchievementManager->Award(32);
//         mbDriveThrusDirty = true; }`
//       -- OnTrophyUnlock @0x82389740 is declared and parked tree-wide (the same P2 park
//       OnEventFinishUpdateProfile carries), and the two AchievementManagerBase vtable slots
//       route into a TU that is deliberately NOT MOUNTED.
//   Q3  the RANK-N arm: AchievementManagerBase::OnLicenseUpgrade @0x8235ADC8 (same unmounted
//       TU) + `TelemetryData::AddParameter(22, "%i" % rank)` and `queue->AddEvent(record, 228,
//       20)`. UNREACHABLE from this build's only caller, which passes rank 0.
//   Q4  the shared rank tail, which needs ClearMedalsOnRankUp @0x823705D8 (absent) plus two
//       un-homed words (the manager's chosen-car-type slot +133480 and the profile byte
//       +118404 the `rank >= 5` store targets). Leaving it parked means the profile's
//       mi8CurrentProgressionRank keeps the -2 "rank not set" seed Profile::Construct wrote,
//       which is the value this build already ships to the GUI -- landing HALF the tail would
//       change that reading without the medal/rank machinery that gives it meaning.
// The lpGameActionQueue parameter is consumed only by Q3, so this build's caller passes 0
// exactly as the console's UpdatePlayerMedals passes its own (unused-on-this-path) queue.
// ------------------------------------------------------------------------------------
void ProgressionManager::UnlockToProgressionRank(s8 li8Rank,
                                                 BrnGameState::GameStateModuleIO::GameActionQueue* /*lpGameActionQueue*/)
{
    // @0x8239DE14 -- `lwz r11, 0x14(r3)` vs the cached rank byte, i.e. PlayerHasFinishedLastRank().
    if (PlayerHasFinishedLastRank())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("!PlayerHasFinishedLastRank()", KAC_PROGMGR_FILE, 965);
        CgsDev::Assert::EndAssert();
    }

    if (li8Rank == 0)
    {
        // ---- @0x8239DE60..0x8239DF3C -- THE EVENT-LIST POPULATION -------------------------
        const ProgressionData* lpcProgressionData = mpProgressionData.operator->();
        const u32 luJunctionCount = lpcProgressionData->GetEventJunctionCount();

        for (u32 luEventJunctionIndex = 0; luEventJunctionIndex < luJunctionCount; ++luEventJunctionIndex)
        {
            // The console re-reads the resource pointer and re-checks the index against the
            // count every iteration (the bounds assert baked at BrnProgressionData.h:386 lives
            // inside GetEventJunction, which is where this routes it).
            const EventJunction* lpcJunction = lpcProgressionData->GetEventJunction(luEventJunctionIndex);

            // `lwz r11, 4(r31) / cmpwi 0 / beq` -- a junction with no OFFLINE event gets no
            // profile record. This is what makes the record set exactly "the offline events".
            if (lpcJunction->GetOfflineEvent() == 0)
            {
                continue;
            }

            // The open-coded id scan at 0x8239DEDC, routed through the named finder.
            if (mProfile.FindEvent(lpcJunction->GetID()) != 0)
            {
                continue;
            }

            mProfile.AddEvent(lpcJunction->GetID());
            AddEventTypeToEventTotals(lpcJunction);
        }

        // ⛔ PARK Q1 + Q2 -- UnlockDefaultPlayerCars and the starting-drive-thru/trophy tail.
        // See the banner; both need bodies that do not exist in this tree.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] ProgressionManager::UnlockToProgressionRank(0): profile "
                   "event list populated -- "
                << mProfile.GetEventCount()
                << " records from " << luJunctionCount
                << " authored junctions. PARKED on this path: UnlockDefaultPlayerCars "
                   "@0x8237BF98 and the starting-drive-thru/trophy tail (OnTrophyUnlock "
                   "@0x82389740 + the unmounted AchievementManagerBase).\n";
        }
    }
    else
    {
        // ⛔ PARK Q3 -- the RANK-N licence-upgrade arm (@0x8239E034). Unreachable from this
        // build's only caller (Prepare2 passes rank 0); see the banner for the console body.
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] ProgressionManager::UnlockToProgressionRank("
                << static_cast<s32>(li8Rank)
                << "): the licence-upgrade arm is PARKED (AchievementManagerBase::"
                   "OnLicenseUpgrade @0x8235ADC8 lives in a TU that is not mounted).\n";
        }
    }

    // ⛔ PARK Q4 -- the shared rank tail (@0x8239E094 onward). See the banner.
}

// ------------------------------------------------------------------------------------
// ProgressionManager::AddEventTypeToEventTotals  @ 0x82366628
// DWARF BrnProgressionManager.h:678 -- `void AddEventTypeToEventTotals(const EventJunction*);`
//
// Nine instructions plus two asserts, verbatim:
//     r3 = GetEvent( *(*(junction + 4) + 236) )    ; the OFFLINE event's data mode byte +0xEC,
//                                                  ; mapped through the mode table to a runtime
//                                                  ; GsmIO::EGameModeType
//     if (r3 == -1) assert "lEGameModeType != GsmIO::E_MODE_NONE"   (BrnProgressionManager.cpp:797)
//     if (r3 <= -1) assert "lEGameModeType > GsmIO::E_MODE_NONE"    (BrnProfile.h:2047)
//     ++*(4*(r3 + 30) + profile)                   ; == ++maGameModeTypeAmount[mode]
// The second assert is the INLINED Profile::AddGameModeTypeToTotals's own, which is why it
// carries a BrnProfile.h location; routing through the named Profile method keeps it there.
// ------------------------------------------------------------------------------------
void ProgressionManager::AddEventTypeToEventTotals(const EventJunction* lpEventJunction)
{
    const BrnGameState::GameStateModuleIO::EGameModeType lEGameModeType =
        static_cast<BrnGameState::GameStateModuleIO::EGameModeType>(
            GetEvent(static_cast<s32>(lpEventJunction->GetOfflineEvent()->GetMode())));

    if (lEGameModeType == BrnGameState::GameStateModuleIO::E_MODE_NONE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lEGameModeType != GsmIO::E_MODE_NONE", KAC_PROGMGR_FILE, 797);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into maGameModeTypeAmount[-1]; bail instead of
                  // corrupting the tally array (the OnPlayerCarChange precedent in this TU).
    }

    mProfile.AddGameModeTypeToTotals(lEGameModeType);
}

// ------------------------------------------------------------------------------------
// ProgressionManager::AreRoadRulesAvailable  @ 0x82311520
//
// X360: returns 1 if the player's medal-progress count (mProfile.muMedalCountFromTheStart,
// the X360 a1[10720] read) is >= 4, OR if either tail availability flag is non-zero.
// ------------------------------------------------------------------------------------
bool ProgressionManager::AreRoadRulesAvailable() const
{
    if (mProfile.GetMedalCountFromTheStart() >= 4u)
    {
        return true;
    }
    if (miNumberOfParCrashRoadRulesRuledByPlayer != 0)
    {
        return true;
    }
    if (miNumberOfParTimeRoadRulesRuledByPlayer != 0)
    {
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::GetEvent  @ 0x82359850
//
// Map an offline game-mode index (0..5) to its event-data id. Pure index->constant table
// (the X360 jump table). Unknown index asserts and returns -1.
// Constants are the X360 li immediates: 0->0, 1->3, 2->7, 3->8, 4->5, 5->4.
// ------------------------------------------------------------------------------------
s32 ProgressionManager::GetEvent(s32 liGameType) const
{
    switch (liGameType)
    {
        case 0: return 0;
        case 1: return 3;
        case 2: return 7;
        case 3: return 8;
        case 4: return 5;
        case 5: return 4;
        default:
            CGS_ASSERT(false,
                "I dont know what this game type is! Maybe its new mode and needs adding this switch statement?\n");
            return -1;
    }
}

// ------------------------------------------------------------------------------------
// ProgressionManager::GetOnlin  @ 0x82359960
//
// Map an online game-mode index (0..2) to its event-data id. The X360 immediates:
// 0->10, 1->11, 2->13. Unknown index asserts and returns -1.
// ------------------------------------------------------------------------------------
s32 ProgressionManager::GetOnlin(u32 luGameType) const
{
    if (luGameType == 0)
    {
        return 10;
    }
    if (luGameType == 1)
    {
        return 11;
    }
    if (luGameType < 3)   // i.e. == 2
    {
        return 13;
    }

    CGS_ASSERT(false,
        "I dont know what this game type is! Maybe its new mode and needs adding this switch statement?\n");
    return -1;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::IsCarUnlocked  @ 0x823635C0
//
// True when the player already owns lCarId. The X360 scans the embedded Profile's owned-car
// list (Profile +0x26C count word, Profile +0x280 maCars[] with a 0x18 stride, comparing the
// 8-byte id at each record's +0). Reconstructed through the named Profile car accessors.
// ------------------------------------------------------------------------------------
bool ProgressionManager::IsCarUnlocked(CgsID lCarId) const
{
    const s32 liCarCount = mProfile.GetCarCount();
    if (liCarCount <= 0)
    {
        return false;
    }

    for (s32 liIndex = 0; liIndex < liCarCount; ++liIndex)
    {
        if (mProfile.GetCarData(liIndex)->GetId() == lCarId)
        {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::RepairUnlockedVehicle  @ 0x82363630
//
// Clear the just-repaired car's stored deform/damage. Asserts lCarId is non-null then delegates
// to the embedded Profile (X360: Profile::RepairUnlockedVehicle(this+0x170, lCarId)).
// ------------------------------------------------------------------------------------
void ProgressionManager::RepairUnlockedVehicle(CgsID lCarId)
{
    CGS_ASSERT(lCarId != 0, "lCarId != kCGSID_NULL");
    mProfile.RepairUnlockedVehicle(lCarId);
}

// ------------------------------------------------------------------------------------
// ProgressionManager::SetRoadRuleChallengeData  @ 0x823114A8
//
// Replace the whole 64-entry road-rules challenge-score table (X360: XMemCpy into
// Profile +100056 = mProfile.maChallengeData, 2560 bytes). Asserts the source non-null
// (the X360 fires the assert twice -- once at the wrapper, once inside the inlined Profile
// body -- which is reproduced as the wrapper assert here + the Profile-body assert in the
// delegated Profile::SetRoadRuleChallengeData).
// ------------------------------------------------------------------------------------
void ProgressionManager::SetRoadRuleChallengeData(const BrnStreetData::ChallengePlayerScoreEntry* lpaChallengeScores)
{
    CGS_ASSERT(lpaChallengeScores != nullptr, "lpaChallengeScores");
    mProfile.SetRoadRuleChallengeData(lpaChallengeScores);
}

// ------------------------------------------------------------------------------------
// ProgressionManager::SetRoadRuleNetworkHighScores  @ 0x82311430
//
// Replace the whole 64-entry road-rules network high-score table (X360: XMemCpy into
// Profile +96472 = mProfile.maNetworkChallengeData, 3584 bytes). Asserts the source non-null.
// ------------------------------------------------------------------------------------
void ProgressionManager::SetRoadRuleNetworkHighScores(const BrnStreetData::ChallengeHighScoreEntry* lpaChallengeHighScores)
{
    CGS_ASSERT(lpaChallengeHighScores != nullptr, "lpaChallengeHighScores");
    mProfile.SetRoadRuleNetworkHighScores(lpaChallengeHighScores);
}

// ------------------------------------------------------------------------------------
// ProgressionManager::GetProfile
//
// The player Profile is EMBEDDED BY VALUE at ProgressionManager+0x170 (mProfile), so the X360
// renders every GetProfile() call as a `this + 0x170` pointer adjust with no call at all --
// which is why the exports carry no symbol for it. Its callers ALWAYS null-check the result
// (`CGS_ASSERT(mProgressionManager.GetProfile())`), so the console's own contract allows a
// null answer; an embedded sub-object simply never is one.
// ------------------------------------------------------------------------------------
Profile* ProgressionManager::GetProfile()
{
    return &mProfile;
}

// ============================================================================================
// [gateui] 2026-08-20 (owner `deps`) -- THE THREE STUNT-ELEMENT / ACHIEVEMENT ACCESSORS the
// smash-gate + billboard UI chain links against. All three were declaration-only and are
// measured UNDEF externals in StuntManager_gUI_00.obj and BrnStuntManager.obj (dumpbin
// /SYMBOLS), which is what keeps GameSource/GameState/Offences/BrnStuntManager.cpp from
// mounting. None of them owns a standalone X360 symbol -- the console header-inlines each one,
// so the only evidence is the inlined form at the call sites, quoted per body below. They are
// placed here rather than as header inlines to match the placement GetProfile /
// GetProgressionData / GetCurrentCarData already use in this TU.
//
// THE ADDRESS IDENTITY THAT MAKES ALL OF THIS A DELEGATION, ONCE:
//   * the player Profile is embedded at ProgressionManager+368 (0x170) -- see GetProfile above,
//     and the console's own `Profile::RecordPropHit(mpProgressionManager + 368, ...)` in
//     StuntManager::ProcessStuntElement @0x8239CDB0;
//   * Profile's per-type completed-element sets start at Profile+30200 with a 4104-byte stride
//     (Profile::IsStuntElementDone @0x823619B0 = `Find(4104*type + this + 30200, &id) != -1`;
//     Profile::GetStuntElementCount @0x82361950 = `*(4104*type + this + 30200 + 4096)`);
//   * 368 + 30200 == 30568, which is EXACTLY the base ProcessStuntElement uses when it inlines
//     the manager-side query: `Set<s64,512>::Find(mpProgressionManager + 4104*type + 30568)`.
// Same address, same stride, same comparison -- so the manager methods are the profile methods,
// and nothing is dropped by routing through the bodied Profile pair.
// ============================================================================================

// Is this stunt element already in the player's completed set for its type?
//
// X360 (inlined, ProcessStuntElement @0x8239CDB0): `v29 = 4104 * HIDWORD(v8)` then
// `Set<__int64,512>::Find(mpProgressionManager + v29 + 30568, &key)`, with the
// `_cntlzw(-1 - Find(...)) & 0x20) == 0` dance being nothing but the compiler's "!= -1" test.
// Neither this method nor Profile::IsStuntElementDone asserts on the console, so none is added.
bool ProgressionManager::IsStuntElementDone(BrnGameState::StuntElementType leStuntElementType,
                                            CgsID                          lStuntElementKey) const
{
    return mProfile.IsStuntElementDone(leStuntElementType, lStuntElementKey);
}

// How many elements of this type the player has collected -- the `miCurrentCount` of game
// action 58, i.e. the "12" in the HUD's "Billboards Smashed 12/45".
//
// X360 (inlined, ProcessStuntElement): `*(mpProgressionManager + 4104*type + 30568 + 4096)`.
// The +4096 lands on the set's length word, so the console's own
// "Set used before Construct/Clear was called" sentinel (CgsSet.h:227, the assert
// Profile::GetStuntElementCount @0x82361950 carries) is on this path via Set<>::GetLength.
//
// FLAG (NAME): the DWARF spells the method `GetStuntElementCount` (BrnProgressionManager.h:438)
// with this exact shape. The repo name is kept -- see the header comment for the rename plan.
s32 ProgressionManager::GetCollectedStuntElementCount(BrnGameState::StuntElementType leStuntType) const
{
    return mProfile.GetStuntElementCount(leStuntType);
}

// The achievement manager Prepare2 installed (X360 *(progmgr + 133432) == +0x20938 == the
// `mpAchievementManager` member). Read as a POINTER by every console consumer, e.g.
// CheckForSpecialCarUnlocks @0x82396058 `OnGameCompletion(*(a1 + 133432))` and
// StuntManager::ProcessStuntElement's `OnCollectStunt` / `OnCollectAllStunts` calls.
//
// ⚠️ FLAG (PC bring-up, pre-existing): nothing in the mounted set calls Prepare2 yet, so this
// answers NULL today. That is the console's own contract -- every caller asserts non-null
// first -- so it surfaces as the game's own assert rather than a silent dereference.
BrnGameState::AchievementManagerBase* ProgressionManager::GetAchievementManager()
{
    return mpAchievementManager;
}

// ============================================================================================
// FindLandmarkAISectionIndex (X360 0x82359AE0) -- DWARF BrnProgressionManager.h:385
// ============================================================================================
// [stuntrace waveB MOUNT-CLOSURE round, 2026-08-26] Bodied. It was declare-only since the header
// was first carved ("the definition lives with the ProgressionManager TU"), and it is one of the
// 63 unresolved externals the wave-B event-core mount measured -- with five console callers:
// OfflineGameMode::SelectRandomDestinations @0x82321E38, ModeManager::SetOnlineLandmarks
// @0x82328800, ModeManager::SetUpCheckPointsForGameMode @0x82328BC8, HACK_SetupRaceWithLandMarks
// @0x82359B78 and GameStateModule::SendRouteRequestAction @0x82381DC8.
//
// WHAT IT IS: a linear scan of the landmark -> AI-section cache the progression layer builds at
// Prepare2 time. Given a landmark's id it answers which AI section that landmark sits in, so a
// route/checkpoint consumer can hand the AI a section index instead of a world position.
//
// THE ASM, INSTRUCTION BY INSTRUCTION (nothing below is inferred):
//   0x82359AEC  lis r11,2 / ori r11,r11,0x924 / lwzx r11,r3,r11
//                                 -- r11 = *(this + 0x20924) == mpTriggerData.
//   0x82359AFC  lwz r9, 0x34(r11) -- the LIVE bound is TriggerData::miLandmarkCount (+0x34), NOT
//                                    a count member of this manager. The committed
//                                    SharedClasses/Trigger/BrnTriggerData.h names that word and
//                                    exposes it as GetNumLandmarks(), which is what is called
//                                    here -- no raw-offset reach-around.
//   0x82359B00  cmpwi r9,0 / ble  -- count <= 0 goes STRAIGHT to the not-found arm. An empty
//                                    table is a failure, not a quiet zero.
//   0x82359B08  addis r11,r3,2 / addi r11,r11,-0x878
//                                 -- 0x20000 - 0x878 == 0x1F788 == &maLandmarkAISectionIndices[0].
//   0x82359B10  lwz r8, 0(r11)    -- entry.mId, loaded as a WORD (so it zero-extends)...
//   0x82359B14  cmpld cr6, r8, r4 -- ...and compared 64-BIT against the CgsID argument. That is
//                                    the compiler's own widening of a u32 member against a u64
//                                    parameter, which is why `lpEntry->mId == lLandmarkId` below
//                                    is the faithful spelling: C++ promotes mId identically.
//   0x82359B1C  addi r10,r10,1 / addi r11,r11,8 / cmpw r10,r9 / blt   -- the 8-byte stride loop.
//   0x82359B60  lhz r3, 4(r11)    -- the hit returns entry.muAISectionIndex (halfword at +4).
//   0x82359B2C  the miss fires the assert below and `li r3, 0x7FFF` -- KI_INVALID_SECTION_INDEX,
//                                    the same sentinel BrnGameStateStreetManager_wB_09.cpp:202
//                                    and GameBridgeGUIToX_GameState.cpp:226 already spell.
// Assert message VERBATIM from the export, INCLUDING ITS TRAILING SPACE; file/line are the
// console's own (BrnProgressionManager.cpp:3259) and are kept here because this TU already bakes
// KAC_PROGMGR_FILE for its other explicit asserts.
//
// [!] The console does NOT null-test mpTriggerData before dereferencing it, and neither does this
// body -- GameStateModule::Prepare2 installs it (BrnGameStateModule.cpp, the
// mTriggerQueryManager.GetTriggerData() argument) on the same boot path that reaches every caller.
// mpTriggerData is typed void* by the header (it is a Prepare2 back-pointer the manager treats as
// opaque, and retyping it would ripple through Prepare2's signature and its call site in another
// agent's file this round), so the cast to the real type is made here, in the only body that
// dereferences it. Re-point the member's type when that signature is next touched.
//
// ⚠️⚠️ FLAG -- THE PRODUCER IS NOT MOUNTED, SO THE TABLE IS EMPTY TODAY. The cache is filled by
// ProgressionManager::ComputeLandmarkAISectionIndices (X360 0x82370008), which the console's
// Prepare2 calls immediately after installing the back-pointers -- and which this tree's Prepare2
// only lists in its "X360 then calls, in order:" comment. Until that lands, every lookup walks a
// zeroed table, misses, fires the console's own "landmark not found " assert and returns
// KI_INVALID_SECTION_INDEX. That is the console's genuine miss behaviour rather than an invented
// one, and the callers all carry the answer as a plain u16, so nothing dereferences it -- but no
// caller gets a REAL section index yet. This is a one-function frontier, not a research problem:
// ComputeLandmarkAISectionIndices' two heavy legs, AISectionsData::BuildAISectionPointMap
// @0x8267A688 and AISectionsData::FindNearestAISection @0x82676CC0, are BOTH already bodied and
// mounted (SharedClasses/AI/AISectionsData.cpp:76 / :277); what it still needs is a bound
// mpAISectionData and the CgsMemory::LinearMalloc scratch arena. Reported as this slice's
// follow-up -- do not quote this function as "working" until that call is wired.
// ============================================================================================
u16 ProgressionManager::FindLandmarkAISectionIndex(CgsID lLandmarkId) const
{
    const BrnTrigger::TriggerData* lpTriggerData =
        static_cast<const BrnTrigger::TriggerData*>(mpTriggerData);

    // `lwz r9, 0x34(mpTriggerData)` -- the live landmark count is the trigger data's, and the
    // table is indexed in step with it.
    const s32 liLandmarkCount = lpTriggerData->GetNumLandmarks();

    for (s32 liLandmarkIndex = 0; liLandmarkIndex < liLandmarkCount; ++liLandmarkIndex)
    {
        const LandmarkAISectionIndexPair* lpEntry = &maLandmarkAISectionIndices[liLandmarkIndex];

        // `cmpld` -- the u32 member widens to the u64 id, exactly as the console compares them.
        if (lpEntry->mId == lLandmarkId)
        {
            return lpEntry->muAISectionIndex;   // `lhz r3, 4(r11)`
        }
    }

    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert("ProgressionManager::FindLandmarkAISectionIndex, landmark not found ",
                               KAC_PROGMGR_FILE, 3259);
    CgsDev::Assert::EndAssert();
    return 0x7FFF;   // BrnWorld::KI_INVALID_SECTION_INDEX
}

// ============================================================================================
// THE JUNKYARD / CAR-SELECT PRODUCER SURFACE (2026-08-01).
// Everything below is what CarSelectManager and GameStateModule reach for through the
// progression layer. Bodies recovered from the X360 ASM (the Hex-Rays prototypes for AddCar /
// OnPlayerCarChange render the 64-bit CgsID register arguments as one `__int64` and drop the
// rest -- see the per-body notes).
// ============================================================================================

// The loaded ProgressionData resource (X360: the ResourcePtr at this+133348). Every caller
// null-tests the answer, so the raw HasMemoryResource() test is used rather than operator->
// (whose own assert would pre-empt the caller's).
const ProgressionData* ProgressionManager::GetProgressionData() const
{
    if (!mpProgressionData.HasMemoryResource())
    {
        return 0;
    }
    return mpProgressionData.operator->();
}

// The CarData record for the car the player is currently in (X360 this+133328).
CarData* ProgressionManager::GetCurrentCarData()
{
    return mpCurrentCarData;
}

// The chosen-livery record for that car (X360 this+133332). See the header for the two
// console readers that inline this adjust (AddDistanceDriven @0x823668F0 writes
// `(*this+133332)->mfDistanceDriven`; GameStateModule::CopyScoringDataToOutput @0x8236CDC0
// publishes it).
LiveryData* ProgressionManager::GetCurrentLiveryData()
{
    return mpCurrentLiveryData;
}

// X360 this+133448 -- the loaded vehicle list. See the header FLAG on the install site.
void ProgressionManager::SetVehicleList(const BrnResource::VehicleList* lpVehicleList)
{
    mpVehicleList = lpVehicleList;
}

// --------------------------------------------------------------------------------------------
// GetProgressionRank (X360 0x823701D8).
// The cached rank byte at this+133484, CLAMPED into the loaded rank table:
//   * read UNSIGNED and compared `>= 0x80` -- i.e. any signed-negative cache (the Profile seeds
//     -2 for "not started") answers rank 0 without touching the resource;
//   * otherwise `min(rank, rankCount - 1)` against ProgressionData::muProgressionRankCount (+0x14).
// --------------------------------------------------------------------------------------------
s32 ProgressionManager::GetProgressionRank() const
{
    if (static_cast<u8>(mi8ProgressionRank) >= 0x80u)
    {
        return 0;
    }

    const s32 liRank      = static_cast<s32>(static_cast<u8>(mi8ProgressionRank));
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());
    if (liRank < liRankCount)
    {
        return liRank;
    }
    return liRankCount - 1;
}

// ============================================================================================
// GetRankThresholdForEvent (X360 0x82370260) -- DWARF BrnProgressionManager.h:285
// ============================================================================================
// [stuntrace waveB CLOSURE round, 2026-08-26] Bodied. It was declare-only, and it is the sole
// blocker under GetProgressionRankForGameMode below -- which StuntAttackMode::Start @0x82332088
// calls directly, and which ModeManager::GetRoadRageTakedownTarget is parked on.
//
// "How many wins at this mode does the player need in order to have reached progression rank
// liProgressionRank?" Four of the ten offline modes carry their own per-rank threshold byte; the
// other six have no separate difficulty ladder and answer 0.
//
//   0x8237026C..0x8237027C  addis r3,r3,2 / addi r3,r3,0x8E4 / bl sub_82369020
//                           -- ResourcePtr<ProgressionData>::GetMemory() on this+0x208E4, i.e.
//                              GetProgressionData(). sub_82369020 IS that template body: it fires
//                              "Can not instance resource pointer - it ..." (GameShared/
//                              GameClasses/Sy..., line 0x233) and returns *(ptr+0).
//   0x82370284..0x8237028C  lwz r11,0x14(data) / cmplw r31,r11 / blt
//                           -- luIndex < muProgressionRankCount (assert line 0x14A == 330, file
//                              "..\..\..\SharedClasses\Progression/...")
//   0x823702B0..0x823702B8  lwz r10,0x10(data) / mulli r11,r31,0x70 / add
//                           -- the 112-byte per-rank record, i.e. GetProgressionRankData(luIndex)
//   0x823702BC..0x82370338  a 9-case switch on the mode -> one of four bytes, else 0
//
// [!] THE INDEX ASSERT IS NOT RESTATED HERE. Its baked file/line is BrnProgressionData.h:330 --
// it belongs to ProgressionData::GetProgressionRankData, which this tree already bodies WITH that
// assert. The console carries it at this site only because it inlined that accessor; calling the
// accessor reproduces it exactly once. Restating it would double the assert and poison the H10
// assert storm, whose whole value is one line per missing wire.
//
// [!] The console does NOT null-test GetProgressionData() here -- it goes through the ResourcePtr,
// whose own "Can not instance resource pointer" assert is the guard. Reproduced: no invented
// early return.
// ============================================================================================
s32 ProgressionManager::GetRankThresholdForEvent(s32 liProgressionRank,
                                                 BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const
{
    const ProgressionRankData* lpRankData =
        mpProgressionData->GetProgressionRankData(static_cast<u32>(liProgressionRank));

    switch (leGameModeType)
    {
        case BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE:   // case 0 -> rank+0x60
            return lpRankData->GetNumWinsToRankUpRace();
        case BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK:   // case 7 -> rank+0x61
            return lpRankData->GetNumWinsToRankUpStunt();
        case BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE:      // case 3 -> rank+0x62
            return lpRankData->GetNumWinsToRankUpRoadRage();
        case BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN:     // case 8 -> rank+0x63
            return lpRankData->GetNumWinsToRankUpMarkedMan();
        default:                                                     // cases 1,2,4,5,6 + out of range
            return 0;
    }
}

// ============================================================================================
// ⭐⭐ [stuntrace wave D, D3] THE THREE RANK-AS-RATIO QUERIES.
//
// GameStateModule::StartModeAtLights @0x82396CF8 forks between them on the runtime mode
// (@0x823970E0..0x82397134) and publishes the answer through
// StartGameModeParams::SetProgressionRankAsRatio -- the number StuntAttackMode::Start and
// RaceMode::Start scale event difficulty by. Every one of the three is walked from its own
// EXPORT ASSEMBLY: Hex-Rays renders all three as int/float-union noise and drops the f1 return
// in each, so the pseudocode is unusable and is not the source for anything below.
// ============================================================================================

// --------------------------------------------------------------------------------------------
// ProgressionManager::GetProgressionRankNormalised  @ 0x82370340
// --------------------------------------------------------------------------------------------
// `clamp(lfRank / (rankCount - 1), 0, 1)`, instruction for instruction:
//   0x82370358..0x82370364  addis r3,r3,2 / addi r3,r3,0x8E4 / bl sub_82369020
//                           -- ResourcePtr<ProgressionData>::GetMemory() on this+0x208E4
//   0x82370368..0x82370388  lwz r11,0x14(data) / addi r11,r11,-1 / extsb / fcfid / frsp
//                           -- f31 = (f32)(s8)(muProgressionRankCount - 1)
//   0x8237037C              lfs f30, flt_82001CC0   ; == 0.0f (image-read)
//   0x8237038C/0x82370390   fcmpu f31, f30 / bgt    -- SKIP the assert when maxRank > 0
//   0x82370394..0x8237042C  the "Max Rank set to <n>\n" assert, BrnProgressionManager.cpp:3995
//   0x82370430              fdivs f0, f29(lfRank), f31
//   0x82370444/0x8237044C   fneg f13, f0 / fsel f0, f13, f30(0.0f), f0   -- lower clamp at 0
//   0x82370450/54/58        lfs f13, flt_82001C98 (== 1.0f) / fsubs f12, f13, f0 /
//                           fsel f30, f12, f0, f13                       -- upper clamp at 1
//   tail                    the `(gxMessageFilterFlags & 1)` "Normalised rank is X (a of b)" line
// --------------------------------------------------------------------------------------------
f32 ProgressionManager::GetProgressionRankNormalised(f32 lfRank) const
{
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());
    const f32 lfMaxRank   = static_cast<f32>(static_cast<s8>(liRankCount - 1));

    // The console's own assert text streams the value; CGS_ASSERT takes a literal, so the number
    // is dropped and the condition + the message stem are kept verbatim.
    CGS_ASSERT(lfMaxRank > 0.0f, "Max Rank set to ");                 // :3995

    f32 lfNormalised = lfRank / lfMaxRank;
    if (lfNormalised < 0.0f)                                          // fsel @0x8237044C
    {
        lfNormalised = 0.0f;
    }
    if (lfNormalised > 1.0f)                                          // fsel @0x82370458
    {
        lfNormalised = 1.0f;
    }

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "Normalised rank is " << lfNormalised
                                   << " (" << lfRank << " of " << lfMaxRank << ")\n";
    }
    return lfNormalised;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::GetProgressionRankNormalisedForCurrentRank  @ 0x8237B610
// --------------------------------------------------------------------------------------------
// Exported unnamed; the GLOBAL arm of StartModeAtLights' rank fork (`bl sub_8237B610`
// @0x82397134). The name is descriptive -- no symbol survives. Body, from the asm:
//   0x8237B634..0x8237B64C  lwz r10,0x14(data) / lbzx r11,this,0x2096C / extsb / cmpw
//                           -- (s8)mi8ProgressionRank == (s32)muProgressionRankCount ?
//   0x8237B654..0x8237B670  yes: f0 = (f32)(s8)(muProgressionRankCount - 1)
//   0x8237B674..0x8237B684  no : f0 = (f32)(s8)GetProgressionRank()
//   0x8237B694              bl GetProgressionRankNormalised(f1 == that value)
// ⓘ The `==` is the console's, and it is NOT the same test as PlayerHasFinishedLastRank()
// (which this tree spells `>=`). Reproduced as shipped; do not "unify" them.
// --------------------------------------------------------------------------------------------
f32 ProgressionManager::GetProgressionRankNormalisedForCurrentRank() const
{
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());

    f32 lfRank;
    if (static_cast<s32>(mi8ProgressionRank) == liRankCount)
    {
        lfRank = static_cast<f32>(static_cast<s8>(liRankCount - 1));
    }
    else
    {
        lfRank = static_cast<f32>(static_cast<s8>(GetProgressionRank()));
    }
    return GetProgressionRankNormalised(lfRank);
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::GetProgressionRankForGameModeNormalised  @ 0x8237BE10
// --------------------------------------------------------------------------------------------
// The PER-MODE arm of the same fork (modes 0/3/7/8). Body, from the asm:
//   0x8237BE38..0x8237BE4C  f30 = (f32)(s8)(muProgressionRankCount - 1)          [maxRank]
//   0x8237BE50..0x8237BE78  f28 = (f32)(s8)GetProgressionRankForGameMode(mode)   [modeRank]
//   0x8237BE7C/0x8237BE80   fcmpu f28, f30 / bge -> 0x8237BF80: return flt_82001C98 (== 1.0f)
//   0x8237BE88..0x8237BEA4  lwz r10,0x14(data) / lbzx r11,this,0x2096C / extsb / cmpw / beq
//                           -- also return 1.0f when the cached rank byte has reached the count
//   0x8237BEB0..0x8237BEE0  f31 = (f32)GetRankThresholdForEvent(modeRank,     mode)   [low]
//   0x8237BF08/0x8237BF10   lfs f0, flt_820049E0 (== 100.0f) / fdivs f30, f0, f30
//                           -- f30 is REUSED: it becomes 100.0f / maxRank, a PERCENT PER RANK
//   0x8237BEFC..0x8237BF24  f29 = (f32)GetRankThresholdForEvent(modeRank + 1, mode)   [high]
//   0x8237BF28..0x8237BF4C  f12 = (f32)Profile::GetNumRankWinsForGameMode(mode)       [wins]
//                           (`addi r3, r31, 0x170` == the embedded mProfile)
//   0x8237BF30/34/40/50/54  f0  = f29 - f31              ; (high - low)
//                           f13 = f30 * f28              ; percentPerRank * modeRank
//                           f0  = f30 / f0               ; percentPerRank / (high - low)
//                           f12 = f12 - f31              ; (wins - low)
//                           f1  = f12 * f0 + f13         ; fmadds -- the result, in PERCENT
//   0x8237BF58..0x8237BF6C  lfs f0, flt_82001CC0 (== 0.0f) / fcmpu / ble ->
//                           lfs f0, flt_82029F24 (== 0.01f) / fmuls f1, f1, f0
//                           -- percent -> 0..1, ONLY when the sum is strictly positive
// The four image constants above were read out of the ARTIST image, not guessed
// (flt_82001C98 = 1.0f, flt_82001CC0 = 0.0f, flt_820049E0 = 100.0f, flt_82029F24 = 0.01f).
// ⓘ The console calls GetProgressionRankForGameMode FOUR times (once for the compare and once
// per threshold lookup); kept, because the calls are the console's and one of them could in
// principle observe a changed rank.
// --------------------------------------------------------------------------------------------
f32 ProgressionManager::GetProgressionRankForGameModeNormalised(
        BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const
{
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());
    const f32 lfMaxRank   = static_cast<f32>(static_cast<s8>(liRankCount - 1));
    const f32 lfModeRank  = static_cast<f32>(GetProgressionRankForGameMode(leGameModeType));

    if (lfModeRank >= lfMaxRank)                                       // @0x8237BE80
    {
        return 1.0f;                                                   // flt_82001C98
    }
    if (static_cast<s32>(mi8ProgressionRank) == liRankCount)            // @0x8237BEA4
    {
        return 1.0f;
    }

    const f32 lfLowThreshold = static_cast<f32>(
        GetRankThresholdForEvent(GetProgressionRankForGameMode(leGameModeType), leGameModeType));
    const f32 lfPercentPerRank = 100.0f / lfMaxRank;                   // @0x8237BF10
    const f32 lfHighThreshold = static_cast<f32>(
        GetRankThresholdForEvent(GetProgressionRankForGameMode(leGameModeType) + 1, leGameModeType));
    const f32 lfWins = static_cast<f32>(mProfile.GetNumRankWinsForGameMode(leGameModeType));

    f32 lfPercent = (lfWins - lfLowThreshold) *
                        (lfPercentPerRank / (lfHighThreshold - lfLowThreshold)) +
                    (lfPercentPerRank * lfModeRank);
    if (lfPercent > 0.0f)                                              // @0x8237BF60
    {
        lfPercent *= 0.01f;                                            // flt_82029F24
    }
    return lfPercent;
}

// ============================================================================================
// GetStuntRunScoreTarget (X360 0x8237B6B0) -- DWARF BrnProgressionManager.h:270
// ============================================================================================
// [stuntrace waveB CLOSURE round, 2026-08-26] Bodied. THIS IS THE STUNT RACE'S SCORE TARGET.
// StuntAttackMode::Start @0x82332150 calls it on the path the campaign actually takes -- when the
// profile carries no per-event TargetEventScore record -- and writes the answer into
// GameModeParams::mfNeedForGold, which ScoringSystem::OnModeStart @0x823382A8 then hands to
// StuntModeScoring::Activate as the mode's target. With no body, a stunt race has no target.
//
// The header's declare-only banner listed FOUR blockers. Three were already false and the fourth
// is closed by this same round, which is why it can land now:
//   (a) "a real ProgressionRankData LAYOUT"  -- narrowed to ONE byte, rank+0x61, which is now the
//       DWARF-named ProgressionRankData::GetNumWinsToRankUpStunt and is reached here through
//       GetRankThresholdForEvent, not by offset. (2026-08-26 MOUNT-CLOSURE round: the blocker is
//       gone outright rather than narrowed -- the real LAYOUT landed as
//       SharedClasses/Progression/BrnProgressionRankData.h, and the BrnGameModeParams.h stand-in
//       this line used to name is retired.)
//   (b) RaceEventData::GetRankScore @0x823543D0 -- bodied, SharedClasses/Progression/BrnRaceEventData.cpp:42;
//   (c) BrnMath::RoundWithNumSignificantFigures -- bodied, GameSource/Math/BrnMathUtils.cpp:123;
//   (d) GetRankThresholdForEvent's own body -- landed above this pass.
//
// WHAT IT COMPUTES: the player sits somewhere BETWEEN two progression ranks, measured in stunt-mode
// wins; the target score is the event's per-rank score linearly interpolated by that same fraction,
// rounded to 2 significant figures. At the last rank there is nothing to interpolate towards, so
// the rank's own score is returned outright.
//
// FIVE INLINE COPIES, ONE VALUE. The console inlines GetProgressionRankForGameMode
// (E_MODE_STUNT_ATTACK) FIVE times -- at 0x8237B744, 0x8237B818, 0x8237B910, 0x8237BA2C and
// 0x8237BAF4 -- each an identical rank-count / threshold-ladder loop over the 112-byte per-rank
// stride reading `lbz r11, 0x61(r11)`, each followed by the same
// "liProgressionRank >= 0 && liProgressionRank < liNumRanks" assert (line 0xEDA). Nothing between
// them mutates the profile or the resource, so all five yield the same rank; called ONCE here.
// That is rematerialisation, not five different quantities -- the same compiler behaviour
// StuntAttackMode::Start's double GetRankTime call already documents.
//
// REGISTER / OFFSET MAP, re-derived this pass:
//   0x8237B6C4  lwz r19, 0x32C(r5)        -- a3 == lpStartGameModeParams; +0x32C == mpEventData.
//                                            a2 (lpGameModeParams) is NEVER dereferenced -- it is
//                                            carried for the console's signature and nothing else.
//   0x8237B6E4  li r5, 0xF33              -- assert 3891 "lpStuntRunEventData != NULL"
//   0x8237B710  lwz r27, 0x388(r24)       -- this+0x388. mProfile is at this+0x170 and
//                                            maiRankWinsPerOfflineGameMode at Profile+0x1FC, so
//                                            0x388 == +0x1FC + 7*4 == the STUNT slot, i.e. the
//                                            inlined Profile::GetNumRankWinsForGameMode
//                                            (E_MODE_STUNT_ATTACK). Re-read at 0x8237B804 /
//                                            0x8237B8E4 / 0x8237B9E0 / 0x8237BACC -- same word.
//   0x8237B714  lwz r11,0x14 / addi -1 / extsb r25
//                                         -- liLastRankForGameMode == (s8)(rankCount - 1)
//   0x8237B7DC  cmpw r20, r25 ; blt       -- liCurrentRank >= liLastRankForGameMode -> early return
//   0x8237B7EC  bl GetRankScore(lpEventData, liCurrentRank)   (the early-return value)
//   0x8237B8EC  lbz 0x61 -> fcfid/frsp    -- f29 lfTotalNumberOfWinsForThisRank
//   0x8237B9E8  lbz 0x61 (rank+1) -> f27  -- lfTotalNumberOfWinsForNextRank
//   0x8237BA0C  f25                       -- lfCurrentEventWins (the +0x388 word as f32)
//   0x8237BA18  fdivs f30, (f25-f29), (f27-f29)   -- lfCurrentRelativeEventRatio
//   0x8237BAC0  GetRankScore(rank)   -> f31 lfCurrentRankStuntRunScore
//   0x8237BB8C  GetRankScore(rank+1) -> f28 lfNextRankStuntRunScore
//   0x8237BBC4  fmadds f26, (f28-f31), f30, f31   -- lfCurrentStuntRunTarget
//   0x8237BDFC  BrnMath::RoundWithNumSignificantFigures(f26, flt_82001D9C)
//               IMAGE-CITED: image.bin @0x1D9C == 40 00 00 00 == 2.0f exactly.
//
// The local names above are the CONSOLE'S OWN -- they are the literal strings its debug-print
// block streams (aLilastrankforg / aLicurrentrank / aLftotalnumbero / aLfcurrentevent_0 /
// aLfcurrentrelat_0 / aLfcurrentranks / aLfnextrankstun / aLfcurrentstunt), so the variable
// naming below is recovered, not invented.
//
// DROPPED, deliberately (the same ruling StuntAttackMode::Start's banner records for its own
// `gxMessageFilterFlags & 1` print): the eight-line debug dump at 0x8237BB94..0x8237BDF0, plus its
// EXTRA RoundWithNumSignificantFigures call at 0x8237BDB0 -- which exists only to print the result
// and is discarded. That duplicate call is why a naive transcription would round twice. The
// surviving call is the tail one at 0x8237BDFC. No state depends on any of it.
// ============================================================================================
s32 ProgressionManager::GetStuntRunScoreTarget(const BrnGameState::GameModeParams* /*lpGameModeParams*/,
                                               const BrnGameState::StartGameModeParams* lpStartGameModeParams) const
{
    const RaceEventData* lpStuntRunEventData = lpStartGameModeParams->GetEventData();
    CGS_ASSERT(lpStuntRunEventData != NULL, "lpStuntRunEventData != NULL");

    // `lwz r11,0x14 / addi r11,r11,-1 / extsb r25` -- narrowed to a signed byte, like every other
    // rank count in this file.
    const s32 liLastRankForGameMode =
        static_cast<s8>(static_cast<u8>(mpProgressionData->GetProgressionRankCount() - 1u));

    const s32 liCurrentRank =
        GetProgressionRankForGameMode(BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK);

    // Top rank: nothing above to interpolate towards, so the rank's own score IS the target.
    if (liCurrentRank >= liLastRankForGameMode)
    {
        return lpStuntRunEventData->GetRankScore(static_cast<u32>(liCurrentRank));
    }

    // How far the player is between this rank's win requirement and the next one's.
    const f32 lfTotalNumberOfWinsForThisRank = static_cast<f32>(
        GetRankThresholdForEvent(liCurrentRank,
                                 BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK));
    const f32 lfTotalNumberOfWinsForNextRank = static_cast<f32>(
        GetRankThresholdForEvent(liCurrentRank + 1,
                                 BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK));
    const f32 lfCurrentEventWins = static_cast<f32>(
        mProfile.GetNumRankWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK));

    const f32 lfCurrentRelativeEventRatio =
        (lfCurrentEventWins - lfTotalNumberOfWinsForThisRank) /
        (lfTotalNumberOfWinsForNextRank - lfTotalNumberOfWinsForThisRank);

    // The same fraction applied to the event's two neighbouring rank scores.
    const f32 lfCurrentRankStuntRunScore =
        static_cast<f32>(lpStuntRunEventData->GetRankScore(static_cast<u32>(liCurrentRank)));
    const f32 lfNextRankStuntRunScore =
        static_cast<f32>(lpStuntRunEventData->GetRankScore(static_cast<u32>(liCurrentRank + 1)));

    // `fmadds f26, (f28 - f31), f30, f31` -- one fused multiply-add, written in that order.
    const f32 lfCurrentStuntRunTarget =
        (lfNextRankStuntRunScore - lfCurrentRankStuntRunScore) * lfCurrentRelativeEventRatio +
        lfCurrentRankStuntRunScore;

    return BrnMath::RoundWithNumSignificantFigures(lfCurrentStuntRunTarget,
                                                   KF_STUNT_TARGET_SIGNIFICANT_FIGURES);
}

// ============================================================================================
// GetProgressionRankForGameMode (X360 0x8237B4E8) -- DWARF BrnProgressionManager.h:282
// ============================================================================================
// [stuntrace waveB CLOSURE round, 2026-08-26] Bodied. This is StuntAttackMode::Start's rank
// source (`li r4,7 / lwz r3,0x6D5C(modeMgr) / bl` @0x82332084) -- the value that then indexes
// RaceEventData::GetRankTime for the stunt race's time limit -- and one of the two bodies
// ModeManager::GetRoadRageTakedownTarget is parked on.
//
// Walks the player's WINS AT THIS MODE up the per-rank threshold ladder and answers the highest
// rank whose threshold the player has met. Instruction map:
//
//   0x8237B4FC..0x8237B518  mode == 0 || 3 || 7 || 8, else assert
//                           "This game mode doesn't have separate difficulty scaling"
//                           (line 0xEC3 == 3779). It is an ASSERT, not a guard: the console falls
//                           straight through and reads the array anyway.
//   0x8237B554..0x8237B580  the SECOND assert, "( leGameModeType < GsmIO::E_MODE_OFFLINE_COUNT )
//                           && ( leGameModeType > GsmIO::E_MODE_NONE )" -- baked against a
//                           DIFFERENT file ("..\..\..\GameSource\GameState/Progr...", line 0x893)
//                           because it belongs to the INLINED Profile::GetNumRankWinsForGameMode.
//                           NOT restated here: that accessor is bodied with it (BrnProfile.cpp:853).
//   0x8237B584..0x8237B594  addi r11,r28,0xDB / slwi 2 / lwzx r30,r11,r27
//                           -- this + (mode + 0xDB)*4 == this + 0x36C + mode*4. mProfile sits at
//                              this+0x170 (the same anchor StuntAttackMode::Start's
//                              `addi r3, r11, 0x170` uses), so +0x36C == Profile+0x1FC ==
//                              maiRankWinsPerOfflineGameMode[mode] (BrnProfile.h:617, +508) --
//                              exactly what Profile::GetNumRankWinsForGameMode @0x8230FA40 reads.
//                              Reached through that accessor, so no raw offset survives here.
//   0x8237B598..0x8237B5A4  bl <ResourcePtr GetMemory> / lwz r11,0x14 / extsb r29
//                           -- the rank count, SIGN-EXTENDED FROM A BYTE (`extsb`), not a word.
//   0x8237B5A8..0x8237B5D0  liProgressionRank starts at 1 and climbs while
//                           rankWins >= GetRankThresholdForEvent(liProgressionRank, mode) and
//                           liProgressionRank < liNumRanks; the loop is skipped when
//                           liNumRanks <= 1 (`cmpwi r29,1 / ble`).
//   0x8237B5D4..0x82370604  liProgressionRank -= 1; assert
//                           "liProgressionRank >= 0 && liProgressionRank < liNumRanks"
//                           (line 0xEDA == 3802); return extsb(liProgressionRank).
//
// Return type is s8 per the DWARF, which is why every caller `extsb`s r3 -- the final
// `extsb r3, r31` IS that narrowing, and the s8 return preserves it.
// ============================================================================================
s8 ProgressionManager::GetProgressionRankForGameMode(BrnGameState::GameStateModuleIO::EGameModeType leGameModeType) const
{
    CGS_ASSERT(leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE ||
               leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE    ||
               leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK ||
               leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN,
               "This game mode doesn't have separate difficulty scaling");

    // Profile::GetNumRankWinsForGameMode owns the range assert the console fires here.
    const s32 liRankWins = mProfile.GetNumRankWinsForGameMode(leGameModeType);

    // `extsb r29, r11` -- the rank count is loaded as a word and then narrowed to a signed byte.
    const s32 liNumRanks =
        static_cast<s8>(static_cast<u8>(mpProgressionData->GetProgressionRankCount()));

    s32 liProgressionRank = 1;
    while (liProgressionRank < liNumRanks)
    {
        if (liRankWins < GetRankThresholdForEvent(liProgressionRank, leGameModeType))
        {
            break;
        }
        ++liProgressionRank;
    }
    --liProgressionRank;

    CGS_ASSERT(liProgressionRank >= 0 && liProgressionRank < liNumRanks,
               "liProgressionRank >= 0 && liProgressionRank < liNumRanks");

    return static_cast<s8>(liProgressionRank);
}

// --------------------------------------------------------------------------------------------
// AddCar (X360 0x8237A970).
// Hand the car to the profile, then two tallies and the derived-("silver")-car fan-out.
// ARG SHAPE FROM ASM: r3=this, r4=carId, r5=unlockType. (ProgressionManager::OnPlayerCarChange
// @0x8237AC38 calls it as `li r5,0` -> unlock type E_UNLOCK_TYPE_UNLOCK.)
//
// ⛔ HONEST PARTIAL -- the derived-car leg. When the profile's mbSilverCarsUnlocked flag is set the
// console builds the car's colour-livery list (BrnProgression::DerivedCarArray::
// ConstructColourLiveryList @0x82374F60), walks it, and for every entry whose livery kind == 4 it
// adds that derived car to the profile too and marks its unlock sequence already-shown. That
// whole path needs BrnDerivedCars.h (DerivedCarArray + ConstructColourLiveryList +
// UnlockDerivedCarCollection + DEBUG_PrintArray, ~600 X360 instructions), which is NOT
// reconstructed. It is gated on mbSilverCarsUnlocked (Profile+42516 -- the pair was renamed
// 2026-08-27 to match the X360's own debug strings; the byte and the test are unchanged), which
// a fresh profile leaves FALSE, so it is
// off the start-of-game path entirely -- and it announces itself in the log when it is hit.
// DELETE-WHEN BrnDerivedCars.h lands.
// --------------------------------------------------------------------------------------------
CarData* ProgressionManager::AddCar(CgsID lCarId, s32 leUnlockType)
{
    CarData* lpCarData = mProfile.AddCar(lCarId, static_cast<CarData::UnlockType>(leUnlockType));
    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData != NULL", KAC_PROGMGR_FILE, 536);
        CgsDev::Assert::EndAssert();
    }

    // X360: `if (a3 == 5) ++*(this + 133468);` -- E_UNLOCK_TYPE_SPONSOR.
    if (leUnlockType == CarData::E_UNLOCK_TYPE_SPONSOR)
    {
        ++miSponsorCarCount;
    }
    // X360: `if (carId == CgsIDCompress("CARBEAGT")) ++*(this + 133468);` -- the same counter is
    // bumped a second time for that one car id. (CgsIDCompress is constant-folded by the console
    // compiler; the packed literal is reproduced by the shared helper.)
    if (lCarId == CgsIDCompress("CARBEAGT"))
    {
        ++miSponsorCarCount;
    }

    if (mProfile.GetSilverCarsUnlocked())   // Profile+42516, the console's `lwz *(progMgr+42884)`
    {
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[FLAG PC bring-up] ProgressionManager::AddCar: the derived-("
                   "silver)-car fan-out is NOT reconstructed (needs BrnDerivedCars.h -- "
                   "DerivedCarArray::ConstructColourLiveryList @0x82374F60). Car "
                << static_cast<u32>(lCarId) << " was added, its derived variants were not.\n";
        }
    }

    return lpCarData;
}

// --------------------------------------------------------------------------------------------
// OnPlayerCarChange (X360 0x8237AC38).
// ARG SHAPE FROM ASM: r3=this, r4=carId, r5=wheelId, r6=the bool.
//   lbUpdateProfile == false -> just clear the cached current-car record and return.
//   lbUpdateProfile == true  -> persist the chosen car+wheel onto the profile's spawn slots,
//     make sure the car is owned (adding it -- and marking its unlock sequence already-shown --
//     when it is not), then cache the chosen-livery record for the car's BASE id (a livery
//     variant, entry+0xE9 in {1,3,4}, resolves through GetParentId()).
// --------------------------------------------------------------------------------------------
void ProgressionManager::OnPlayerCarChange(CgsID lCarId, CgsID lWheelId, bool lbUpdateProfile)
{
    if (!lbUpdateProfile)
    {
        mpCurrentCarData = 0;   // X360 `stwx r10(0), this, 0x208D0`
        return;
    }

    // X360 `std r29, 0x1C0(this)` / `std r5, 0x1C8(this)` == mProfile.mSpawnCarId / mSpawnWheelId
    // (mProfile is embedded at this+0x170, so +0x1C0/+0x1C8 are Profile+80/+88).
    mProfile.SetSpawnCarId(lCarId);
    mProfile.SetSpawnWheelId(lWheelId);

    mpCurrentCarData = mProfile.FindCar(lCarId);
    if (mpCurrentCarData == 0)
    {
        mpCurrentCarData = AddCar(lCarId, CarData::E_UNLOCK_TYPE_UNLOCK);
        mProfile.SetCarUnlockAlreadyShown(lCarId);
    }

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = (lpVehicleList != 0) ? lpVehicleList->GetVehicleIndex(lCarId) : -1;
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry", KAC_PROGMGR_FILE, 1258);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    const u8 luLiveryType = lpVehicleListEntry->GetLiveryType();
    const CgsID lBaseCarId =
        (luLiveryType == 1 || luLiveryType == 3 || luLiveryType == 4)
            ? lpVehicleListEntry->GetParentId()
            : lCarId;

    mpCurrentLiveryData = mProfile.GetChosenLiveryDataForBaseCar(lBaseCarId);
}

// --------------------------------------------------------------------------------------------
// GetCarColourAndPalette (X360 0x8237C0D8).
// Answer lCarId's colour + palette indices. The profile's own CarData record wins; the 0xFF
// "unset" sentinel in either field falls back to the car's authored defaults out of its
// VehicleListEntry (`BYTE2 / LOBYTE` of the word at entry+0xEC).
// --------------------------------------------------------------------------------------------
void ProgressionManager::GetCarColourAndPalette(CgsID lCarId, s32* lpiColour, s32* lpiPalette)
{
    // X360: a linear walk of mProfile.maCars[0 .. miCarCount) comparing the record's id.
    const CarData* lpCarData = 0;
    const s32 liCarCount = mProfile.GetCarCount();
    for (s32 liCar = 0; liCar < liCarCount; ++liCar)
    {
        const CarData* lpCandidate = mProfile.GetCarData(liCar);
        if (lpCandidate != 0 && lpCandidate->GetId() == lCarId)
        {
            lpCarData = lpCandidate;
            break;
        }
    }

    *lpiColour  = 255;
    *lpiPalette = 255;
    if (lpCarData != 0)
    {
        *lpiColour  = lpCarData->mu8ColourIndex;
        *lpiPalette = lpCarData->mu8PaletteIndex;
    }

    if (*lpiColour != 255 && *lpiPalette != 255)
    {
        return;
    }

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = (lpVehicleList != 0) ? lpVehicleList->GetVehicleIndex(lCarId) : -1;
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry", KAC_PROGMGR_FILE, 4500);
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    // ⚠️ FLAG: the authored default colour/palette are the two bytes at entry+0xEE / +0xEF, inside
    // VehicleListEntry's trailing maPad224 span (X360 `lbz r11,0xEE` -> colour out,
    // `lbz r11,0xEF` -> palette out). Not named fields yet -- DELETE-WHEN the VehicleListEntry
    // gameplay tail is homed.
    const u8* lpcEntryBytes = reinterpret_cast<const u8*>(lpVehicleListEntry);
    *lpiColour  = lpcEntryBytes[0xEE];
    *lpiPalette = lpcEntryBytes[0xEF];
}

// The two one-byte request flags CarSelectManager::UpdateExitState sets on junkyard exit
// (X360 `stbx r25(1), mpProgressionManager, 0x20971` and `... 0x20988`).
void ProgressionManager::RequestUpdateRivals()
{
    mbUpdateRivalsRequested = true;
}

void ProgressionManager::SetDriveThrusDirtyFlag()
{
    mbDriveThrusDirty = true;
}

} // namespace BrnProgression
