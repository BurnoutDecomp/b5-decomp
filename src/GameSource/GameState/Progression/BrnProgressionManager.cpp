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

    // road-rules ruled tallies start clear (the ctor leaves the +133456..+133464 region 0).
    miNumberOfParCrashRoadRulesRuledByPlayer         = 0;
    miNumberOfParTimeRoadRulesRuledByPlayer          = 0;
    miNumberOfNumberOfCompleteRoadRulesRuledByPlayer = 0;

    // Prepare2 back-pointers start null.
    mpTriggerData        = nullptr;
    mpGameStateModule    = nullptr;
    mpAchievementManager = nullptr;

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
    return true;
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

// --------------------------------------------------------------------------------------------
// AddCar (X360 0x8237A970).
// Hand the car to the profile, then two tallies and the derived-("silver")-car fan-out.
// ARG SHAPE FROM ASM: r3=this, r4=carId, r5=unlockType. (ProgressionManager::OnPlayerCarChange
// @0x8237AC38 calls it as `li r5,0` -> unlock type E_UNLOCK_TYPE_UNLOCK.)
//
// ⛔ HONEST PARTIAL -- the derived-car leg. When the profile's mbGoldCarsUnlocked flag is set the
// console builds the car's colour-livery list (BrnProgression::DerivedCarArray::
// ConstructColourLiveryList @0x82374F60), walks it, and for every entry whose livery kind == 4 it
// adds that derived car to the profile too and marks its unlock sequence already-shown. That
// whole path needs BrnDerivedCars.h (DerivedCarArray + ConstructColourLiveryList +
// UnlockDerivedCarCollection + DEBUG_PrintArray, ~600 X360 instructions), which is NOT
// reconstructed. It is gated on mbGoldCarsUnlocked, which a fresh profile leaves FALSE, so it is
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

    if (mProfile.GetGoldCarsUnlocked())
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
