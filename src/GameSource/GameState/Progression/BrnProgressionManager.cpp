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
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp"
#include "BrnProgressionManager.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "BrnProfile.h"
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"
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
#include "BrnDerivedCars.h"                                             // BrnProgression::DerivedCarArray (AddCar's silver-car fan-out)
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

// ARTIST827DEA50: all stores are the native C++ construction of the real members.
// Array default constructors mark their count words unconstructed; Profile recursively
// constructs its arrays; the debug member installs only its vtable; resource pointers
// self-link. The two-phase Construct method owns the later clears and scalar seeds.
ProgressionManager::ProgressionManager()
{
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
                                   CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 2799));
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
// ARTIST 0x8239DDA4..0x8239DDD4 initializes landmark and race data,
// constructs and registers the debug component, then sets up roaming sections.
// ------------------------------------------------------------------------------------
bool ProgressionManager::Prepare2(BrnGameState::GameStateModuleIO::OutputBuffer* lpOutput,
                                  BrnGameState::ModeManager* lpModeManager,
                                  CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue,
                                  const BrnTrigger::TriggerData* lpTriggerData,
                                  BrnGameState::AchievementManagerBase* lpAchievementManager)
{
    CGS_ASSERT_AT(lpOutput != 0, "lpOutput", CGS_ARTIST_FILE, 249);            // X360 BrnProgressionManager.cpp:249
    CGS_ASSERT_AT(lpReceiverQueue != 0, "lpReceiverQueue", CGS_ARTIST_FILE, 250);  // X360 :250

    // ✅ [progression wave: lifecycle, 2026-09-06] THE PROFILE BOOT SEAM IS RETIRED, ITS OWN
    // DELETE-WHEN PAID. The seam that stood here (landed 2026-08-24, deform-land wave) called
    // Profile::Construct @0x823708A8 from this body, latched by mbProfileConstructed, because
    // "the console constructs the profile on this manager's own construct/prepare path (idat
    // xrefs: ProgressionManager::Construct @0x8237A74C and the outer Prepare's call @0x8239DC78)
    // ... neither outer is reconstructed". BOTH OUTERS ARE RECONSTRUCTED NOW
    // (BrnProgressionManager_Lifecycle.cpp) and both are called from GameStateModule at the
    // console's own positions, so the profile is constructed twice on the way in, exactly as the
    // console constructs it -- and the LAST of those two calls now happens EARLIER than this seam
    // did (Prepare, stage 20, rather than Prepare2), which can only help anything that loads a
    // saved profile between the two. mbProfileConstructed is left declared but unused; it is the
    // rivals/medals lanes' neighbour in the header and removing a member mid-wave is not worth
    // the merge risk. FOLLOW-UP: delete mbProfileConstructed.

    // X360: if ( LoadProgressionData(this, lpOutput, lpReceiverQueue) ) { ... } else return false;
    if (!LoadProgressionData(lpOutput, lpReceiverQueue))
    {
        return false;
    }

    CGS_ASSERT_AT(lpTriggerData != 0, "lpTriggerData", CGS_ARTIST_FILE, 261);
    mpTriggerData     = lpTriggerData;           // X360 +0x20924 (a5)
    mpModeManager = lpModeManager;               // X360 +0x2093C (a3) -- the MODE manager; see the header

    CGS_ASSERT_AT(lpAchievementManager != 0, "lpAchievementManager", CGS_ARTIST_FILE, 265);
    mpAchievementManager = lpAchievementManager; // X360 +0x20938 (a6)

    ComputeLandmarkAISectionIndices();
    ProcessLoadedPresetRaces();
    mDebugComponent.Construct(this, lpModeManager);
    mDebugComponent.Register();
    SetupRoamingSections();
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
// ⭐⭐ [progression wave: medals, 2026-09-06] Q1, Q3 AND Q4 ARE PAID. Only Q2 is still parked,
// and its blocker is a MEASUREMENT, not a missing body -- read its note before touching it.
// The caller set changed with them: UpdatePlayerMedals @0x8239FE50 is bodied
// (BrnProgressionManager_Medals.cpp), so this function is once again reached from its console
// seat and CAN now be called with a NON-ZERO rank, which is what makes the Q3/Q4 arms live code
// rather than dead code.
//
// ⛔ HONEST PARTIAL -- WHAT IS LANDED AND WHAT IS PARKED.
// LANDED: the :965 assert; the whole rank-0 EVENT-LIST arm (the junction walk, the duplicate
//   test, AddEvent and AddEventTypeToEventTotals); UnlockDefaultPlayerCars (Q1); the RANK-N
//   licence-upgrade arm (Q3); and the shared rank tail (Q4).
// PARKED, with the console body written out for whoever lands it:
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
// ✅ Q3 PAID (progression wave: medals). The old text here said OnLicenseUpgrade @0x8235ADC8
//   "lives in a TU that is not mounted" and that the arm was "UNREACHABLE from this build's only
//   caller, which passes rank 0". BOTH HALVES WERE STALE: BrnGameStateAchievementManagerBase.cpp
//   is mounted (build_game_exe.bat, right after the X360 leaf -- the rem block above that echo
//   still argues for the old state), OnLicenseUpgrade is bodied there, and the caller is now
//   UpdatePlayerMedals, which passes whatever rank the medal total bought. Body:
//   UnlockToProgressionRankLicenceUpgrade in BrnProgressionManager_Medals.cpp.
// ✅ Q4 PAID (progression wave: medals). ClearMedalsOnRankUp @0x823705D8 is bodied, and the two
//   "un-homed words" are homed: the chosen-car-type slot is meLeastUsedCarType (+133480, DWARF
//   :167) and the `rank >= 5` store targets Profile::mbHasUnlockedCredits at profile+0x1CD14 ==
//   +118036 (the old note's "+118404" was a mis-read of `lis 1 / ori 0xCD14`). Body:
//   UnlockToProgressionRankTail in BrnProgressionManager_Medals.cpp. One store of the tail stays
//   parked -- the debug component's own display-ready flag; see that body's banner.
// The lpGameActionQueue parameter is consumed only by Q3.
// ------------------------------------------------------------------------------------
void ProgressionManager::UnlockToProgressionRank(s8 li8Rank,
                                                 BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    // @0x8239DE14 -- `lwz r11, 0x14(r3)` vs the cached rank byte, i.e. PlayerHasFinishedLastRank().
    if (PlayerHasFinishedLastRank())
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("!PlayerHasFinishedLastRank()", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 965));
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

        // ✅ [progression wave: medals, 2026-09-06] PARK Q1 IS PAID. UnlockDefaultPlayerCars
        // @0x8237BF98 (`bl` @0x8239DF44) is bodied in BrnProgressionManager_Medals.cpp: it walks
        // PROGRESSION.DAT's player-car id table, AddCar()s any the profile does not already own
        // (type 0 == E_UNLOCK_TYPE_UNLOCK) and marks each one's unlock sequence already-seen.
        // That is the rank-0 STARTING GARAGE -- until now the profile started with no cars at all
        // from this path.
        UnlockDefaultPlayerCars();

        // ⭐⭐ PARK Q2 IS NO LONGER BLOCKED ON WHAT THIS SITE SAID IT WAS (2026-08-27,
        // drive-thru link-closure wave). The old text named "OnTrophyUnlock @0x82389740 + the
        // unmounted AchievementManagerBase" as the blockers; OnTrophyUnlock is bodied now
        // (BrnProgressionManager_Unlocks.cpp) and the AchievementManagerBase drive-thru split IS
        // mounted. Leaving that text in place would send the next reader chasing two things that
        // are already there -- a diagnostic naming the wrong blocker costs exactly as much as one
        // naming the wrong object.
        //
        // WHAT Q2 ACTUALLY IS, from the asm @0x8239DF48..0x8239DFC4: THE STARTING BODY SHOP
        // REGISTERS ITSELF. `lis r11,6 / ori r31, r11, 0xC72D` builds the CgsID 0x6C72D, tests it
        // against mProfile.mBodyShopsDriveThruSet (manager+0xA7B8 == Profile+42568) with
        // Set<s64,11>::Contains, and on a MISS Inserts it -- then, if the set is now full
        // (GetLength() == 11), fires OnTrophyUnlock(20 == E_UNLOCKTYPE_FIND_ALL_BODYSHOPS) and,
        // when every drive-thru category is complete, OnTrophyUnlock(21) on top. It is
        // OnDriveThru's body-shop leg, inlined at boot for the one drive-thru the player starts
        // standing in.
        // ⚠️ DELIBERATELY NOT LANDED. It WRITES INTO THE SAVED PROFILE on the boot path, and it
        // should not land until a run has driven through the starting body shop behind it: landing
        // a profile-mutating boot arm that has never once run is how a save ends up with a wrong
        // set in it.
        //
        // ⚠️⚠️ THE REASON THIS NOTE GAVE FOR "could not exercise it" WAS WRONG, and re-measuring it
        // is what found the real bug (2026-08-28, gas-station wave). The text said "flow_run.ps1's
        // phase machine does not reach DRIVING on the returning-player path ... so -Drive silently
        // holds no throttle". Both halves fail on measurement:
        //   * the phase machine DOES reach DRIVING on that path now (flow_run.ps1's newprof+ingame
        //     promotion, run gas_r1_probe: phase=DRIVING at 32 s), and the throttle IS held --
        //   * but the car still did not move, because on the returning-player path the player's
        //     ENGINE IS NEVER STARTED (no GuiPlayerEngineEvent engineOn=1 in the whole run; the
        //     drive-thru gate reports gear=0 and never re-opens). The harness was innocent; the
        //     boot path is missing the engine start. THAT is the remaining blocker for this park
        //     on the returning path, and it is not a harness bug.
        // On the FRESH-profile path the car now does drive (the authored-extent fix in
        // BrnTriggerQueryManager.cpp), so the drive this park is waiting for is available there --
        // the starting body shop is drive-thru region id 444205 == 0x6C72D at (3345.78, 2.19,
        // -1712.68), which is the same CgsID the asm builds, confirmed against the live region
        // dump. Land it with that drive behind it. [[gates-are-stale-not-dead]]
        if (CgsDev::Log::gpDebugPrint != 0)
        {
        }
    }
    else
    {
        // ✅ [progression wave: medals, 2026-09-06] PARK Q3 IS PAID -- see the banner for why the
        // "unmounted TU" blocker was stale. @0x8239E034..0x8239E090.
        UnlockToProgressionRankLicenceUpgrade(li8Rank, lpGameActionQueue);
    }

    // ✅ [progression wave: medals, 2026-09-06] PARK Q4 IS PAID. The shared rank tail
    // (@0x8239E094 onward) runs for BOTH arms, exactly as the console falls into it: clamp and
    // write back the cached rank, pick the least-used car type, walk the rank up to li8Rank,
    // mirror it onto the profile (which is what finally moves mi8CurrentProgressionRank off the
    // -2 "rank not set" seed), unlock the credits at rank 5, reset the car-type distances, clear
    // the medals, and clear the 18 per-rank completed counts.
    UnlockToProgressionRankTail(li8Rank);
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
        CgsDev::Assert::FireAssert("lEGameModeType != GsmIO::E_MODE_NONE", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 797));
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
s32 ProgressionManager::GetEvent(s32 liGameType)
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
            CGS_FAIL_STREAM_AT("I dont know what this game type is! Maybe its new mode and needs adding this switch statement?\n", CGS_ARTIST_FILE, 906);

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

    CGS_FAIL(
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
    CGS_ASSERT_AT(lCarId != 0,
        "lCarId != kCGSID_NULL",
            "..\\..\\..\\GameSource\\GameState/Progression/BrnProgressionManager.h", 1415 /* ARTIST 0x8236365C */);

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
    CGS_ASSERT(lpaChallengeScores != 0, "lpaChallengeScores");
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
    CGS_ASSERT(lpaChallengeHighScores != 0, "lpaChallengeHighScores");
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
// (Bodied in the header, as the original: see BrnProgressionManager.h.)

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
// (GetAchievementManager is header-inline.)

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
// ✅ [progression wave: lifecycle, 2026-09-06] THE PRODUCER IS MOUNTED. The FLAG that stood here
// said "THE PRODUCER IS NOT MOUNTED, SO THE TABLE IS EMPTY TODAY ... a one-function frontier, not
// a research problem", and named exactly what was missing: a bound mpAISectionData and the
// LinearMalloc scratch arena. Both are supplied now --
// ProgressionManager::Prepare @0x8239DC38 -> LoadAIData @0x8239A0D0 binds mpAISectionData at
// GameStateModule::Prepare stage 20, and ComputeLandmarkAISectionIndices @0x82370008 is
// reconstructed in BrnProgressionManager_Lifecycle.cpp and dispatched by Prepare2's own tail. So
// this scan walks a REAL table (one entry per authored landmark, 105 in the shipped trigger data)
// and its "landmark not found " assert has gone back to meaning what it means on the console.
// ============================================================================================
u16 ProgressionManager::FindLandmarkAISectionIndex(CgsID lLandmarkId) const
{
    const BrnTrigger::TriggerData* lpTriggerData =
        mpTriggerData;

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
                               CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 3259));
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
// (GetProgressionData is header-inline.)

// The CarData record for the car the player is currently in (X360 this+133328).
// (GetCurrentCarData is header-inline.)

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
    CGS_ASSERT_AT(lpStuntRunEventData != NULL, "lpStuntRunEventData != NULL", CGS_ARTIST_FILE, 3891);

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
    CGS_ASSERT_AT(leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_OFFLINE_RACE ||
               leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_ROAD_RAGE    ||
               leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_STUNT_ATTACK ||
               leGameModeType == BrnGameState::GameStateModuleIO::E_MODE_MARKED_MAN,
               "This game mode doesn't have separate difficulty scaling", CGS_ARTIST_FILE, 3779);

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

    CGS_ASSERT_AT(liProgressionRank >= 0 && liProgressionRank < liNumRanks,
               "liProgressionRank >= 0 && liProgressionRank < liNumRanks", CGS_ARTIST_FILE, 3802);

    return static_cast<s8>(liProgressionRank);
}

// --------------------------------------------------------------------------------------------
// AddCar (X360 0x8237A970).
// Hand the car to the profile, then two tallies and the derived-("silver")-car fan-out.
// ARG SHAPE FROM ASM: r3=this, r4=carId, r5=unlockType. (ProgressionManager::OnPlayerCarChange
// calls it as `li r5,0` -> unlock type E_UNLOCK_TYPE_UNLOCK.)
//
// THE FAN-OUT. Once the profile's silver-cars flag (Profile+42516) is set, the console builds the
// added car's colour-livery family on the stack (both count words seeded -1, then
// ConstructColourLiveryList over mpVehicleList, which it dereferences with no test) and walks it:
// every member whose livery kind is E_LIVERY_TYPE_SILVER goes straight into the profile through
// Profile::AddCar with E_UNLOCK_TYPE_GOLD_SILVER -- NOT back through this function, so the two
// tallies above do not run for it -- and gets its unlock sequence marked already shown
// (`stb 1, +0x0A` of the new record). The loop bound is re-read every pass through the checked
// GetLength, and each kind is read through the capacity-checked GetLiveryType, as the console
// inlines both.
// --------------------------------------------------------------------------------------------
CarData* ProgressionManager::AddCar(CgsID lCarId, s32 leUnlockType)
{
    CarData* lpCarData = mProfile.AddCar(lCarId, static_cast<CarData::UnlockType>(leUnlockType));
    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData != NULL", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 536));
        CgsDev::Assert::EndAssert();
    }

    // X360: `if (a3 == 5) ++*(this + 133468);` -- E_UNLOCK_TYPE_SPONSOR.
    if (leUnlockType == CarData::E_UNLOCK_TYPE_SPONSOR)
    {
        ++miMaxCarCount;
    }
    // X360: `if (carId == CgsIDCompress("CARBEAGT")) ++*(this + 133468);` -- the same counter is
    // bumped a second time for that one car id. (CgsIDCompress is constant-folded by the console
    // compiler; the packed literal is reproduced by the shared helper.)
    if (lCarId == CgsIDCompress("CARBEAGT"))
    {
        ++miMaxCarCount;
    }

    if (mProfile.GetSilverCarsUnlocked())   // Profile+42516
    {
        DerivedCarArray lCarVariants;
        lCarVariants.ConstructColourLiveryList(mpVehicleList, lCarId);

        for (u32 luIndex = 0; luIndex < lCarVariants.GetLength(); ++luIndex)
        {
            if (lCarVariants.GetLiveryType(luIndex) != BrnResource::VehicleListEntry::E_LIVERY_TYPE_SILVER)
            {
                continue;
            }

            CarData* lpSilverCarData =
                mProfile.AddCar(lCarVariants.GetItem(luIndex), CarData::E_UNLOCK_TYPE_GOLD_SILVER);
            if (lpSilverCarData == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpSilverCarData != NULL", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 568));
                CgsDev::Assert::EndAssert();
            }
            lpSilverCarData->SetUnlockSequenceAlreadyShown();
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
        CgsDev::Assert::FireAssert("lpVehicleListEntry", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 1258));
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
// ⭐⭐ [L4 boot order 2026-09-28] ProgressionManager::OnLoadProfile  (X360 0x823893A8)
// DWARF BrnProgressionManager.h `void OnLoadProfile();` (PS3 0x27D63C). Sole caller:
// GameStateModule::OnProfileLoaded @0x82397310 (0x823973C4), right after the profile is loaded.
//
//   0x823893B8  UnlockDefaultPlayerCars()
//   0x823893C0  r11 = lbz this+0x1E0                      -- mProfile.mi8CurrentProgressionRank (Profile+0x70)
//   0x823893E0  stbx 0   -> +0x20974                      -- mbPlayerJustWonATrophyUpdateRequired
//   0x823893F0  stb  1   -> +0x20973                      -- mbPlayerMedalsUpdateRequired
//   0x823893F8  stbx 1   -> +0x20971                      -- mbUpdateRivals (this tree: mbUpdateRivalsRequested)
//   0x823893FC  stbx r11 -> +0x2096C                      -- the rank cache (this tree: mi8ProgressionRank)
//   0x82389400  if ((s8)r11 < 0) stb 1 -> +0x20973        -- the same flag again (`extsb ; cmpwi 0 ; bge`)
//   0x82389438  stbx 1   -> +0x20988                      -- mbDriveThruDataDirtyFlag (this tree: mbDriveThrusDirty)
//   0x82389440  stbx 0   -> +0x20970                      -- mbHasJustRankedUp
//   0x82389448  stdx 0   -> +0x20960                      -- mNewlyUnlockedCarID
//   0x82389458  +0x2095C = list+0x3408 - list+0x340C      -- miMaxCarCount = selectable - sponsor
//               (list = lwzx +0x20948, mpVehicleList -- ApplyVehicleList @0x82359A20's own formula)
//   0x823894A8  for (i = 0; i < mProfile.miCarCount /* lwz +0x3DC, re-read each pass */; ++i):
//                 the inlined GetCarData(i) bounds assert (BrnProfile.h:1923), before EACH field read;
//                 if (car.meUnlockType /* +0x10 */ == 5)  { ++miMaxCarCount; the "sponsor cars" line }
//                 if (car.mId == CgsIDCompress("CARBEAGT")) { ++miMaxCarCount; the "TROPY HACK" line }
//   0x82389674  if (lbzx this+0xA784 /* Profile+42516, mbSilverCarsUnlocked */)
//                   stbx 1 -> this+0x1CE86                -- Profile+118038, mbHaveSeenEliteCompletionSequence
//
// WHY IT MATTERS. Without it the loaded profile's sponsor cars (and CARBEAGT) never grew
// miMaxCarCount -- the "CARS OWNED x/Y" denominator -- after a reboot, and the rank cache kept
// Construct's -2 until a PC stand-in in PreWorldUpdate restored it (retired with this body).
// The two log lines are the console's, behind the console's own `gxMessageFilterFlags & 1` gate.
// --------------------------------------------------------------------------------------------
void ProgressionManager::OnLoadProfile()
{
    UnlockDefaultPlayerCars();

    const s8 li8SavedRank = mProfile.GetCurrentProgressionRank();
    mbPlayerJustWonATrophyUpdateRequired = false;
    mbPlayerMedalsUpdateRequired         = true;
    mbUpdateRivalsRequested              = true;
    mi8ProgressionRank                   = li8SavedRank;
    if (li8SavedRank < 0)
    {
        mbPlayerMedalsUpdateRequired = true;
    }

    mbDriveThrusDirty   = true;
    mbHasJustRankedUp   = false;
    mNewlyUnlockedCarID = 0;
    miMaxCarCount       = mpVehicleList->GetSelectableVehicleCount()      // +0x3408
                            - mpVehicleList->GetSponsorVehicleCount();    // +0x340C

    for (s32 liCarIndex = 0; liCarIndex < mProfile.GetCarCount(); ++liCarIndex)
    {
        if (mProfile.GetCarData(liCarIndex)->GetUnlockType() == CarData::E_UNLOCK_TYPE_SPONSOR)
        {
            ++miMaxCarCount;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Profile: increasing max car count cos of sponsor cars! ("
                                           << miMaxCarCount << ")\n";
            }
        }
        if (mProfile.GetCarData(liCarIndex)->GetId() == CgsIDCompress("CARBEAGT"))
        {
            ++miMaxCarCount;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "Profile: increasing max car count cos LAST MINUTE ONLINE CHALLENGE TROPY HACK!!!!! ("
                    << miMaxCarCount << ")\n";
            }
        }
    }

    if (mProfile.AreSilverCarsUnlocked())
    {
        mProfile.SetSeenEliteCompletionSequence();   // Profile::SetSeenEliteCompletionSequence @0x8235A150 is this store
    }
}

// --------------------------------------------------------------------------------------------
// GetCarColourAndPalette (X360 0x8237C0D8).
// Answer lCarId's colour + palette indices. The profile's own CarData record wins; the 0xFF
// "unset" sentinel in either field falls back to the car's authored defaults out of its
// VehicleListEntry (`BYTE2 / LOBYTE` of the word at entry+0xEC).
// --------------------------------------------------------------------------------------------
void ProgressionManager::GetCarColourAndPalette(CgsID lCarID, u32& luOutCarColourIndex, u32& luOutCarPaletteIndex) const
{
    // X360: a linear walk of mProfile.maCars[0 .. miCarCount) comparing the record's id.
    const CarData* lpCarData = 0;
    const s32 liCarCount = mProfile.GetCarCount();
    for (s32 liCar = 0; liCar < liCarCount; ++liCar)
    {
        const CarData* lpCandidate = mProfile.GetCarData(liCar);
        if (lpCandidate != 0 && lpCandidate->GetId() == lCarID)
        {
            lpCarData = lpCandidate;
            break;
        }
    }

    luOutCarColourIndex  = 255;
    luOutCarPaletteIndex = 255;
    if (lpCarData != 0)
    {
        luOutCarColourIndex  = lpCarData->mu8ColourIndex;
        luOutCarPaletteIndex = lpCarData->mu8PaletteIndex;
    }

    if (luOutCarColourIndex != 255 && luOutCarPaletteIndex != 255)
    {
        return;
    }

    const BrnResource::VehicleList* lpVehicleList = mpVehicleList;
    const s32 liVehicleIndex = (lpVehicleList != 0) ? lpVehicleList->GetVehicleIndex(lCarID) : -1;
    const BrnResource::VehicleListEntry* lpVehicleListEntry =
        (liVehicleIndex < 0) ? 0 : lpVehicleList->GetVehicleData(liVehicleIndex);

    if (lpVehicleListEntry == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpVehicleListEntry", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 4500));
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
    }

    // ⚠️ FLAG: the authored default colour/palette are the two bytes at entry+0xEE / +0xEF, inside
    // VehicleListEntry's trailing maPad224 span (X360 `lbz r11,0xEE` -> colour out,
    // `lbz r11,0xEF` -> palette out). Not named fields yet -- DELETE-WHEN the VehicleListEntry
    // gameplay tail is homed.
    const u8* lpcEntryBytes = reinterpret_cast<const u8*>(lpVehicleListEntry);
    luOutCarColourIndex  = lpcEntryBytes[0xEE];
    luOutCarPaletteIndex = lpcEntryBytes[0xEF];
}

// The two one-byte request flags CarSelectManager::UpdateExitState sets on junkyard exit
// (X360 `stbx r25(1), mpProgressionManager, 0x20971` and `... 0x20988`).

} // namespace BrnProgression

// FOLDED FROM BrnProgressionManager_Completion.cpp
// ===================================================================================
// BrnProgression::ProgressionManager -- THE GAME-COMPLETION PERCENTAGE CHAIN.
//   GameSource/Unity/../GameState/Progression/BrnProgressionManager.cpp  (per-function
//   partfile of that TU, the house BrnProgressionManager_EventFinish.cpp precedent.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   ProgressionManager::GetTrueNumberOfRivals          @0x8236FB10  (44 instructions)
//   ProgressionManager::GetNumberOfBeatenRivals        @0x8236FBC8  (113 instructions)
//   ProgressionManager::GetPercentageOfEventsCompleted @0x8237B390  (85 instructions)
//   ProgressionManager::ComputeCompletionPercentage    @0x8238A198  (321 instructions)
//   ProgressionManager::CheckForSpecialCarUnlocks      @0x82396058  (144 instructions)
//   ProgressionManager::SendGameCompletionResults      @0x82395C28  (44 instructions)
//
// ⚠️⚠️ HEX-RAYS IS UNUSABLE FOR THE BIG ONE. Its output for 0x8238A198 opens with
// "local variable allocation has failed, the output may be wrong!", loses the entire
// clamp-and-easter-egg tail (it renders the final CgsIDCompress("CARBEAGT") lookup as a
// dead expression whose result is discarded), and mangles the two `fsel`s. Everything
// below is read off the ASSEMBLY; the pseudocode was used only to cross-check the shape
// of the six weighted terms.
//
// ⭐ THE WEIGHTS CLOSE ON 100, WHICH IS HOW WE KNOW THEY WERE READ CORRECTLY. Six .rdata
// floats feed the sum here -- 9.0 + 9.0 + 11.0 + 11.0 + 2.5 + 2.5 == 45 -- and
// GetPercentageOfEventsCompleted's own tail multiplies its 0..100 rank sum by 0.01 and
// then by 55.0. 45 + 55 == 100 exactly. (Read with the verified .rdata reader over the
// unpacked ARTIST image; the same reader reproduces two constants this subsystem had
// already derived independently, 0.85f @flt_82029BB8 and 60.0f @flt_82004C6C.)
// ===================================================================================

#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/Progression/BrnProfile.h"             // Profile / ProfileEvent / RivalData
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"// RivalData::EState
#include "SharedClasses/Progression/BrnProgressionData.h"            // ProgressionData
#include "SharedClasses/Progression/BrnRival.h"                      // BrnProgression::Rival
#include "SharedClasses/Progression/BrnProgressionRankData.h"        // ProgressionRankData (medal threshold)
#include "SharedClasses/StreetData/BrnStreetData.h"                  // BrnStreetData::StreetData (GetRoadCount)
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h" // BrnGameState::StreetManager::GetStreetData
#include "GameSource/GameState/Offences/BrnStuntManager.h"           // BrnGameState::StuntManager::GetTotalStuntElementCount
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnGameCompletion
#include "GameSource/GameState/ModeManager/BrnModeManager.h"         // ModeManager (SendGameCompletionResults' mode read)
#include "GameSource/GameState/BrnGameActions.h"                     // GameStateModuleIO action ids
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"                       // CgsIDCompress ("CARBEAGT")
#include "GameSource/GameState/Progression/BrnDerivedCars.h"          // DerivedCarArray (UnlockDerivedCarCollection)
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"  // CarData::UnlockType (AddCar's E_UNLOCK_TYPE_GIFT)
#include "SharedClasses/Progression/BrnRaceEventData.h"              // RaceEventData::EModeType / GetMode, EventJunction
#include "SharedClasses/Progression/BrnTrophyUnlockData.h"           // TrophyUnlockData::UnlockType
#include "SharedClasses/Progression/BrnTrainingTypes.h"              // BrnProgression::E_TRAINING_TYPE_POWER_PARK
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h" // TrainingManager::RequestTraining
#include "GameSource/GameState/Offences/BrnStuntManager.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"           // CgsDev::Log::gpDebugPrint ([completion] witnesses)

#include <stdlib.h>                                                  // getenv (the [completion] witness gate)
#include <cstring>                                                   // strcmp (the trophy seed's mode)

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

// ---- the .rdata constants, all read from the unpacked ARTIST image -------------------
static const f32 KF_WEIGHT_TIME_ROAD_RULES  =   9.0f;   // flt_82CDB948
static const f32 KF_WEIGHT_CRASH_ROAD_RULES =   9.0f;   // flt_82CDB94C
static const f32 KF_WEIGHT_RIVALS_BEATEN    =  11.0f;   // flt_82CDB950
static const f32 KF_WEIGHT_STUNT_ELEMENTS   =  11.0f;   // flt_82CDB954
static const f32 KF_WEIGHT_EVENTS_FOUND     =   2.5f;   // flt_82CDB958
static const f32 KF_WEIGHT_DRIVE_THRUS      =   2.5f;   // flt_82CDB95C
static const f32 KF_EVENTS_COMPLETED_SCALE  =  55.0f;   // flt_82CDB960
static const f32 KF_PERCENT_SCALE           =   0.01f;  // flt_82029F24
static const f32 KF_ONE_DRIVE_THRU          =   0.02857142873108387f;  // flt_820317A0 == 1/35
static const f32 KF_COMPLETE                = 100.0f;   // flt_820049E0
static const f32 KF_COMPLETE_EPSILON        =   1.52587890625e-05f;    // flt_82029B74 == 2^-16
static const f32 KF_ONE_HUNDRED_AND_ONE     = 101.0f;   // flt_8203179C -- yes, really; see the tail
static const f32 KF_ZERO                    =   0.0f;   // flt_82001CC0

// The authored per-rank contribution to the completion percentage, flt_82CDB964[]. Six entries,
// one per progression rank, and they sum to exactly 100 -- which is the internal cross-check on
// the table's LENGTH: BrnProgressionRankData.h already records the six authored medal thresholds
// (2, 7, 15, 26, 40, 120) for the same six ranks, and the next .rdata word after this table is
// 540.0f, plainly a different constant.
static const f32 KAF_RANK_COMPLETION_CONTRIBUTION[] = { 2.5f, 7.5f, 10.0f, 17.5f, 27.5f, 35.0f };
static const s32 KI_RANK_COMPLETION_COUNT =
    static_cast<s32>(sizeof(KAF_RANK_COMPLETION_CONTRIBUTION) / sizeof(f32));

// ===================================================================================
// ProgressionManager::GetTrueNumberOfRivals  @ 0x8236FB10
//
// The rivals-beaten denominator: how many authored rivals are real opponents.
// `lbz r11, 0x17(r11)` on a 56-byte-strided Rival record is mbIsUsedForRankUpGiftCar --
// an entry that only exists to carry a rank-up gift car is not something you can beat,
// so it is excluded.
// ===================================================================================
s32 ProgressionManager::GetTrueNumberOfRivals()
{
    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        return 0;   // PC bring-up guard; the console reaches the table through operator->
    }

    s32 liTrueRivals = 0;
    for (s32 liIndex = 0; liIndex < lpProgressionData->GetRivalCount(); ++liIndex)
    {
        // GetRival owns the "liIndex < miRivalCount" assert the console bakes as
        // BrnProgressionData.h:460 and re-fires every iteration.
        const Rival* lpRival = lpProgressionData->GetRival(liIndex);
        if (!lpRival->GetIsUsedForRankUpGiftCar())
        {
            ++liTrueRivals;
        }
    }
    return liTrueRivals;
}

// ===================================================================================
// ProgressionManager::GetNumberOfBeatenRivals  @ 0x8236FBC8
//
// The numerator: of those same real rivals, how many the profile has BEATEN
// (RivalData::meState @+0x10 == E_STATE_BEATEN == 3).
//
// ⚠️ The console looks the profile record up TWICE with the same key (0x8236FCA0 and
// 0x8236FD20) -- once to test "is it there at all", once to read the state. Both scans
// are Profile::FindRival open-coded (Profile+0x6280, stride 0x38, `ld`/`cmpld` at +0).
// ⛔ AND THE SECOND LOOKUP'S MISS PATH DEREFERENCES NULL: at 0x8236FD40 the console does
// `li r11, 0` then `lwz r11, 0x10(r11)`, i.e. it reads absolute address 0x10. It cannot
// be reached -- the first lookup already proved the record exists and nothing between
// them mutates the list -- and on the console's low-memory map it would read a mapped
// word rather than fault. Reproducing an unreachable wild read would be the only way to
// turn a console non-event into a PC access violation, so the miss path is guarded here
// and the divergence is named rather than hidden.
// ===================================================================================
s32 ProgressionManager::GetNumberOfBeatenRivals()
{
    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        return 0;
    }

    s32 liBeatenRivals = 0;
    for (s32 liIndex = 0; liIndex < lpProgressionData->GetRivalCount(); ++liIndex)
    {
        const Rival* lpRival = lpProgressionData->GetRival(liIndex);
        if (lpRival->GetIsUsedForRankUpGiftCar())
        {
            continue;
        }

        // `ldx r8, r10, r29` -- the authored rival's own 64-bit id is the profile key.
        const RivalData* lpRivalData = mProfile.FindRival(lpRival->GetId());
        if (lpRivalData != 0 && lpRivalData->meState == RivalData::E_STATE_BEATEN)
        {
            ++liBeatenRivals;
        }
    }
    return liBeatenRivals;
}

// ===================================================================================
// ProgressionManager::GetPercentageOfEventsCompleted  @ 0x8237B390
//
//   sum   = SUM over the ranks already earned of KAF_RANK_COMPLETION_CONTRIBUTION[i]
//   frac  = totalWins / thisRank.mu16MedalThresholdToNextRank      (0 when no wins yet)
//   return (KAF_RANK_COMPLETION_CONTRIBUTION[rank] * frac + sum) * 0.01f * 55.0f
//
// ⚠️ THE WIN TOTAL IS THE FIRST AND THIRD OUT-PARAM ONLY. `lwz r10, var_60` +
// `lwz r11, var_58` + `add r28, r11, r10` -- rank wins plus special-event wins. The
// middle (non-rank) count is deliberately excluded, exactly as in OnTrophyUnlock.
// ⚠️ THE FRACTION'S GUARD IS ON THE NUMERATOR. `cmplwi cr6, r28, 0 / beq` -- zero wins
// short-circuits to 0.0f; the DENOMINATOR is never checked, so an authored threshold of
// 0 divides by zero on both console and host. Reproduced (the shipped table's six
// thresholds are 2/7/15/26/40/120, none of them zero).
// ===================================================================================
f32 ProgressionManager::GetPercentageOfEventsCompleted()
{
    f32 lfRankSum = KF_ZERO;

    // `extsb r31, r3` -- the rank is a signed byte.
    const s32 liRank = GetProgressionRank();
    s32       liNextRankIndex = 0;
    if (liRank > 0)
    {
        for (s32 liIndex = 0; liIndex < liRank; ++liIndex)
        {
            // FLAG (host guard, not the console's): the console indexes flt_82CDB964[]
            // unbounded. The table has six entries and the rank is clamped to the authored
            // rank count by GetProgressionRank, so this cannot trip -- but a table read
            // driven by save data is not something to leave unbounded on the host.
            if (liIndex < KI_RANK_COMPLETION_COUNT)
            {
                lfRankSum += KAF_RANK_COMPLETION_CONTRIBUTION[liIndex];
            }
        }
        liNextRankIndex = liRank;
    }

    u32 luRankWins         = 0;
    u32 luNonRankWins      = 0;
    u32 luSpecialEventWins = 0;
    mProfile.GetTotalWinCount(luRankWins, luNonRankWins, luSpecialEventWins);
    const u32 luTotalWins = luRankWins + luSpecialEventWins;

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        return KF_ZERO;
    }

    f32 lfFractionOfNextRank = KF_ZERO;
    if (luTotalWins != 0)
    {
        // GetProgressionRankData owns the "luIndex < muProgressionRankCount" assert the
        // console bakes as BrnProgressionData.h:330; the 112-byte stride is its `mulli 0x70`.
        const ProgressionRankData* lpRankData =
            lpProgressionData->GetProgressionRankData(static_cast<u32>(liRank));
        // `lhz r11, 0x4C(r11)` + `extsh` -- mu16MedalThresholdToNextRank.
        const s32 liMedalThreshold =
            static_cast<s32>(lpRankData->GetMedalThresholdToNextRank());
        lfFractionOfNextRank = static_cast<f32>(luTotalWins) / static_cast<f32>(liMedalThreshold);
    }

    f32 lfNextRankContribution = KF_ZERO;
    if (liNextRankIndex < KI_RANK_COMPLETION_COUNT)
    {
        lfNextRankContribution = KAF_RANK_COMPLETION_CONTRIBUTION[liNextRankIndex];
    }

    // `fmadds f13, f13, f0, f31` then the two `fmuls`.
    return (lfNextRankContribution * lfFractionOfNextRank + lfRankSum)
           * KF_PERCENT_SCALE * KF_EVENTS_COMPLETED_SCALE;
}

// ===================================================================================
// ProgressionManager::ComputeCompletionPercentage  @ 0x8238A198
//
// Seven weighted terms, clamped to [0, 100] -- and then ONE deliberate exception.
//
// ⭐⭐⭐ 101% IS REAL. The tail (0x8238A5CC..0x8238A688) compresses the literal "CARBEAGT",
// scans the profile's owned-car list for it, and -- when the clamped total is within
// 2^-16 of 100.0f AND that car is owned -- returns flt_8203179C, which reads 101.0f.
// Hex-Rays drops this entirely: it renders the CgsIDCompress call as a dead expression
// and returns the wrong register. The two `fsel`s that do the clamp are also lost in its
// output (they appear as inline `__asm` blocks with the wrong operands). This function
// is therefore reconstructed from the assembly alone.
// ⚠️ Do not "fix" the 101: CheckForSpecialCarUnlocks and SendGameCompletionResults both
// test `>= 100.0f`, so 101 passes them exactly like 100 does. It is a display value with
// no gate consequence, and clamping it to 100 would silently delete an authored reward.
//
// ⚠️ THE THREE STUNT-ELEMENT FRACTIONS ARE MULTIPLIED, NOT AVERAGED
// (`fmuls f13, f30, f29` / `fmuls f13, f13, f27` / `fmadds f29, f13, 11.0f, f28`), so the
// stunt term is 11.0 only when all three types are 100% complete and collapses toward 0
// if any one of them is low. That is unusual enough to be worth stating; it is what the
// binary does.
// ===================================================================================
f32 ProgressionManager::ComputeCompletionPercentage()
{
    // ---- terms 1 + 2: the two road-rule tallies over the world's road count ----------
    // Numerators are the manager's own +133460/+133456; the shared denominator is
    // `StreetData_::oper(mpStreetManager + 7368)` then `lwz r11, 0x20(r3)` -- the CONSOLE's
    // +0x20 is StreetData::miRoadCount (the host record widens its four pointers, so the
    // same member sits at +0x30 there; read by name, so the widening is irrelevant).
    // ⚠️ Each term is guarded on its own NUMERATOR being non-zero, and only then is the
    // StreetData fetched -- so a zero tally never touches the resource. Faithful.
    f32 lfTimeRoadRulesFraction  = KF_ZERO;
    f32 lfCrashRoadRulesFraction = KF_ZERO;

    const BrnStreetData::StreetData* lpStreetData =
        (mpStreetManager != 0) ? mpStreetManager->GetStreetData() : 0;
    if (lpStreetData != 0)
    {
        if (miNumberOfParTimeRoadRulesRuledByPlayer != 0)
        {
            lfTimeRoadRulesFraction =
                static_cast<f32>(miNumberOfParTimeRoadRulesRuledByPlayer) /
                static_cast<f32>(lpStreetData->GetRoadCount());
        }
        if (miNumberOfParCrashRoadRulesRuledByPlayer != 0)
        {
            lfCrashRoadRulesFraction =
                static_cast<f32>(miNumberOfParCrashRoadRulesRuledByPlayer) /
                static_cast<f32>(lpStreetData->GetRoadCount());
        }
    }

    f32 lfTotal = KF_WEIGHT_TIME_ROAD_RULES  * lfTimeRoadRulesFraction
                + KF_WEIGHT_CRASH_ROAD_RULES * lfCrashRoadRulesFraction;

    // ---- term 3: rivals beaten -------------------------------------------------------
    // ⚠️ The guard is `fcmpu f30, f31` on the BEATEN count (the numerator), not on the
    // rival total -- zero beaten short-circuits, and a zero denominator is not checked.
    f32       lfRivalsFraction = KF_ZERO;
    const f32 lfBeatenRivals   = static_cast<f32>(GetNumberOfBeatenRivals());
    const f32 lfTrueRivals     = static_cast<f32>(GetTrueNumberOfRivals());
    if (lfBeatenRivals != KF_ZERO)
    {
        lfRivalsFraction = lfBeatenRivals / lfTrueRivals;
    }
    lfTotal += KF_WEIGHT_RIVALS_BEATEN * lfRivalsFraction;

    // ---- term 4: the three stunt-element sets, MULTIPLIED -----------------------------
    // Numerators are the profile's per-type completed-element Set lengths (the console
    // reads the raw count word behind the CgsSet.h:227 "Set used before Construct/Clear"
    // assert, which is exactly Profile::GetStuntElementCount); denominators are the world
    // totals on the StuntManager (+0x5C4/+0x5C6/+0x5C8, s16). The console evaluates them
    // in the order JUMP, BILLBOARD, SMASH -- the same index feeds both sides of each
    // fraction, so the ordering is presentation only.
    f32 lfJumpFraction      = KF_ZERO;
    f32 lfSmashFraction     = KF_ZERO;
    f32 lfBillboardFraction = KF_ZERO;
    if (mpStuntManager != 0)
    {
        const s32 liJumpsDone = mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP);
        if (liJumpsDone != 0)
        {
            lfJumpFraction = static_cast<f32>(liJumpsDone) / static_cast<f32>(
                mpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP));
        }
        const s32 liBillboardsDone = mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD);
        if (liBillboardsDone != 0)
        {
            lfBillboardFraction = static_cast<f32>(liBillboardsDone) / static_cast<f32>(
                mpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD));
        }
        const s32 liSmashesDone = mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH);
        if (liSmashesDone != 0)
        {
            lfSmashFraction = static_cast<f32>(liSmashesDone) / static_cast<f32>(
                mpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH));
        }
    }
    lfTotal += KF_WEIGHT_STUNT_ELEMENTS
               * (lfSmashFraction * lfBillboardFraction * lfJumpFraction);

    // ---- term 5: drive-thrus found ---------------------------------------------------
    // `GetDriveThrusFound()` is called TWICE -- once as the guard, once for the value.
    // The scale is 1/35, i.e. the authored total number of drive-thrus.
    f32 lfDriveThruTerm = KF_ZERO;
    if (mProfile.GetDriveThrusFound() != 0)
    {
        lfDriveThruTerm = static_cast<f32>(mProfile.GetDriveThrusFound()) * KF_ONE_DRIVE_THRU;
    }
    lfTotal += KF_WEIGHT_DRIVE_THRUS * lfDriveThruTerm;

    // ---- term 6: events discovered ---------------------------------------------------
    // The console counts ProfileEvent flag bit 0 (E_FLAG_DISCOVERED) over miEventCount,
    // then -- if the count is non-zero -- COUNTS THE WHOLE LIST A SECOND TIME to produce
    // the numerator it actually divides. Reproduced as one count plus the same guard: the
    // second walk is byte-identical to the first and cannot produce a different answer.
    f32       lfEventsFoundFraction = KF_ZERO;
    const u32 luEventCount          = mProfile.GetEventCount();
    u32       luEventsFound         = 0;
    for (u32 luEvent = 0; luEvent < luEventCount; ++luEvent)
    {
        if (mProfile.GetEvent(luEvent)->IsFlagSet(ProfileEvent::E_FLAG_DISCOVERED))
        {
            ++luEventsFound;
        }
    }
    if (luEventsFound != 0)
    {
        lfEventsFoundFraction = static_cast<f32>(luEventsFound) / static_cast<f32>(luEventCount);
    }
    lfTotal += KF_WEIGHT_EVENTS_FOUND * lfEventsFoundFraction;

    // ---- term 7 + the clamp ----------------------------------------------------------
    lfTotal += GetPercentageOfEventsCompleted();

    // `fneg f13, f0 / fsel f0, f13, f31, f0` == "(-total >= 0) ? 0 : total", the low clamp;
    // `fsubs f13, f31, f0 / fsel f30, f13, f0, f31` == "(100 - total >= 0) ? total : 100".
    if (lfTotal <= KF_ZERO)
    {
        lfTotal = KF_ZERO;
    }
    if (lfTotal > KF_COMPLETE)
    {
        lfTotal = KF_COMPLETE;
    }

    // ⭐ The 101% award. `CgsIDCompress("CARBEAGT")` then the profile's owned-car scan; the
    // "is it 100" test is |total - 100| < 2^-16, not an equality.
    const bool lbIsComplete =
        (lfTotal - KF_COMPLETE < KF_COMPLETE_EPSILON) &&
        (KF_COMPLETE - lfTotal < KF_COMPLETE_EPSILON);
    if (lbIsComplete && mProfile.FindCar(CgsIDCompress("CARBEAGT")) != 0)
    {
        return KF_ONE_HUNDRED_AND_ONE;
    }
    return lfTotal;
}

// ===================================================================================
// ProgressionManager::CheckForSpecialCarUnlocks  @ 0x82396058
//
// The two derived-car tiers, each a one-shot latched in the profile:
//   SILVER (Profile+42516) unlocks at GetCurrentProgressionRank() >= ProgressionData+0x14
//   GOLD   (Profile+42517) unlocks at ComputeCompletionPercentage() >= 100.0f, and also
//          reports the game complete to the achievement manager.
// ⚠️ The two flags were committed under each other's names until this wave; the console's
// own debug strings print +42516 as AreSilverCarsUnlocked() and +42517 as
// AreGoldCarsUnlocked(), and the gates above corroborate. See BrnProfile.h.
//
// The three `gxMessageFilterFlags & 1` debug-print blocks the console carries are dropped
// per the project convention on the gpcMessageBuffer/StrStream machinery; they have no
// side effects (they read the same two flags the arms below test).
// ===================================================================================
void ProgressionManager::CheckForSpecialCarUnlocks()
{
    if (!mProfile.GetSilverCarsUnlocked())
    {
        // `lbz r19, 0x1E0(r31)` + `extsb` == Profile+112, mi8CurrentProgressionRank, compared
        // SIGNED against ProgressionData+0x14 (the authored rank count).
        // ⚠️ BOTH SIDES ARE SIGN-EXTENDED FROM A BYTE, and the right-hand one is the odd
        // part: the console loads the rank count with `lwz r10, 0x14(r3)` -- a full WORD -- and
        // then narrows it with `extsb r10, r10` before `cmpw` (0x8239615C..0x82396168). So the
        // comparand is the low byte of muProgressionRankCount sign-extended, not the word. With
        // the shipped table's six ranks the two are identical; reproduced as written because a
        // "tidy" widening would change the comparison for any authored count above 127.
        const ProgressionData* lpProgressionData = GetProgressionData();
        if (lpProgressionData != 0 &&
            static_cast<s32>(mProfile.GetCurrentProgressionRank())
                >= static_cast<s32>(static_cast<s8>(
                       static_cast<u8>(lpProgressionData->GetProgressionRankCount()))))
        {
            UnlockSpecialCars(4);
            mProfile.SetSilverCarsUnlocked(true);
        }
    }

    if (!mProfile.GetGoldCarsUnlocked())
    {
        if (ComputeCompletionPercentage() >= KF_COMPLETE)
        {
            UnlockSpecialCars(3);
            mProfile.SetGoldCarsUnlocked(true);

            // `OnGameCompletion(*(a1 + 133432))` -- the achievement manager, then the
            // manager's own one-byte "game complete" latch at +133487.
            BrnGameState::AchievementManagerBase* lpAchievementManager = GetAchievementManager();
            if (lpAchievementManager != 0)
            {
                lpAchievementManager->OnGameCompletion();
            }
            mbAutosaveRequested = true;   // `stbx r22, r31, 0x2096F` -- PreWorldUpdate drains it
        }
    }
}

// ===================================================================================
// ProgressionManager::SendGameCompletionResults  @ 0x82395C28
//
// Posts the 8-byte game-completion record (game action 208) onto the game-action queue,
// and -- the first time the game reads as complete -- stamps the completion date into the
// profile.
//
// THE RECORD, from the console's own frame (var_30 is the base handed to AddEvent with
// `li r6, 8`):
//     +0x00  s32  meGameMode          = mpModeManager->GetCurrentGameModeType()  (+3476)
//     +0x04  bool mbGameComplete
//     +0x05  bool mbCompletionAlreadyRecorded (the profile flag read at Profile+118032)
// ⚠️ Hex-Rays shows `v14`/`v15` as var_2C/var_2B, i.e. record+0x04 and +0x05, and it has
// them the right way round; the asm confirms (`stb` of 1 into var_2C on the complete arm).
// ===================================================================================
void ProgressionManager::SendGameCompletionResults(CgsModule::VariableEventQueue<13312, 16>* lpGameActionQueue)
{
    GsmIO::GameCompletionResultsAction lRecord;

    // `lwzx r11, r31, 0x2093C` then `lwz r11, 0xD94(r11)` -- the MODE manager's current mode.
    // (That +0xD94 read is the evidence that the +0x2093C back-pointer is a ModeManager; it was
    // committed as an untyped `void* mpGameStateModule` until this wave. See the header.)
    lRecord.meGameMode = mpModeManager->GetCurrentGameModeType();

    if (!(ComputeCompletionPercentage() < KF_COMPLETE))
    {
        lRecord.mbGameComplete = true;
        // `lbzx r10, r31, 0x1CE80` == manager+118400 == Profile+118032.
        lRecord.mbCompletionAlreadyRecorded = mProfile.GetSeen100PercentCompletionSequence();

        // `lbzx r11, r31, 0x1CE85` == manager+118405 == Profile+118037. First completion only:
        // stamp the date and latch the flag (the console does both, in that order, and nothing
        // else writes either -- which is why Profile exposes them as one method).
        if (!mProfile.GetHaveSet100PercentCompletedDate())
        {
            mProfile.Set100PercentCompletedDateAsNow();
        }
    }
    else
    {
        lRecord.mbGameComplete = false;
        lRecord.mbCompletionAlreadyRecorded = false;
    }

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lRecord),
                                GsmIO::E_ACTION_GAME_COMPLETION_RESULTS,
                                static_cast<s32>(sizeof(lRecord)));
}

// ############################################################################################
// ############################################################################################
// [progression wave 2026-09-06, lane "completion"] THE THREE COMPLETION-REWARD LEGS whose
// console CALLER was already live on PC while the body was missing -- the same species as the
// odometer (issue #10): a seat exists, nothing sits in it.
//
//   ProgressionManager::CheckForAllModeTypeCompletion        @0x82389698   (41 instructions)
//   ProgressionManager::GetEventCountForType                 @0x8236F9D8   (78 instructions)
//   ProgressionManager::GetEventTypeUniqueWinCount           @0x82370758   (83 instructions)
//   ProgressionManager::UnlockTrophyForEventTypeAllCompleted @0x82395FE8   (a 9-case jump table)
//   ProgressionManager::SendTrophyUnlockUpdate               @0x823892B8   (60 instructions)
//   ProgressionManager::UnlockDerivedCarCollection           @0x8237AD70   (113 instructions)
//   ProgressionManager::OnPowerParkResult                    @0x8238AF78   (76 instructions)
//
// (DerivedCarArray::DEBUG_PrintArray @0x8236ACE8 landed in BrnDerivedCars.h, its console home --
//  the CGS_ASSERT __FILE__ evidence in that header's banner.)
//
// ⭐ WHAT WAS ACTUALLY BROKEN, in one line each:
//   * SendTrophyUnlockUpdate is the ONLY drain of mQueueOfTrophyCarUnLocks. UnlockCarFromTrophy
//     (_Unlocks.cpp) Appends into it today, so the queue could only ever FILL: every trophy car
//     the player earned sat in a 12-slot Array that nothing emptied, the GUI never heard action
//     204, and the twelfth award would have hit "Array container out of space".
//   * UnlockDerivedCarCollection is what turns a car you own into its paint/livery family. Both
//     its console callers were live and both were parked (CarSelectManager::
//     StartCarModificationState @0x82387410 -- every junkyard/paint-shop visit -- and
//     OnEventFinishUpdateProfile's first-special-win arm).
//   * CheckForAllModeTypeCompletion is PreWorldUpdate's 2 s hold. The gate pair and the timer
//     were faithful; the callee was a park, so the "you have won every race/road rage/..." HUD
//     message could never be posted and the profile bit that suppresses it was never set.
//
// ⚠️ THE ASM IS THE SPINE. Two Hex-Rays renderings are wrong and each is called out at its site:
//   (a) UnlockDerivedCarCollection's `if (&v3[6 * v6 + 92] == -640)` is the compiler's always-true
//       null check on `&array[i]`, not a real comparison -- the branch it guards is the
//       ALREADY-OWNED skip.
//   (b) GetEventCountForType / GetEventTypeUniqueWinCount render the junction scan's "not found"
//       tail as `v14 = 0` followed by a use; on the console that use is a load off address 0.
//
// Every member is reached BY NAME through BrnProgressionManager.h / BrnProfile.h.
// ############################################################################################

// The console's baked assert path for BrnProgressionManager.cpp (this partfile's console home),
// so the file/line in the log stay the binary's rather than this TU's.
static const char* const KAC_COMPLETION_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

namespace
{
    s32       giWitnessLinesPrinted  = 0;

}

// ===================================================================================
// ProgressionManager::GetEventCountForType  @ 0x8236F9D8
//
// How many of the profile's event records are authored with mode leModeType.
//
// The loop bound is `lwz r21, 0x3E8(this)` == this+1000 == Profile+0x278 == miEventCount, read
// ONCE before the loop -- and then re-read every iteration for the BrnProfile.h:3004 range
// assert, which is Profile::GetEvent inlined (its body carries that assert, so this walk calls
// the accessor rather than duplicating it). The record base is `addi r22, r3, 0x170` +
// `li r28, 0x7080` == Profile+28800 == maEvents, stride 8.
//
// ⛔ THE :1627 ASSERT CANNOT FIRE. `add r31, r28, r22 / cmplwi r31, 0 / bne` is a null test on
// `&maEvents[i]`, i.e. on an interior address of a live object; the compiler emitted it because
// the source wrote `CGS_ASSERT(lpProfileEvent, ...)` after taking the address. Reproduced as the
// same test on the accessor's return so the assert exists where the console has it.
// ===================================================================================
u32 ProgressionManager::GetEventCountForType(RaceEventData::EModeType leModeType) const
{
    const u32 luEventCount = mProfile.GetEventCount();
    u32       luEventTypeCount = 0;

    for (u32 luIndex = 0; luIndex < luEventCount; ++luIndex)
    {
        const ProfileEvent* lpProfileEvent = mProfile.GetEvent(luIndex);
        if (lpProfileEvent == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpProfileEvent", CGS_ASSERT_LOCATION(KAC_COMPLETION_FILE, 1627));
            CgsDev::Assert::EndAssert();
        }

        const ProgressionData* lpProgressionData = GetProgressionData();
        const RaceEventData*   lpEventData =
            (lpProgressionData != 0) ? lpProgressionData->FindOfflineEvent(lpProfileEvent->GetID()) : 0;
        if (lpEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpEventData", CGS_ASSERT_LOCATION(KAC_COMPLETION_FILE, 1630));
            CgsDev::Assert::EndAssert();
            // [PC GUARD] the console then does `lbz r11, 0xEC(r31)` with r31 == 0 -- a read of
            // absolute address 0xEC, which its low-memory map tolerates and the host does not.
            // Skipping the entry is the only behaviour that is not an invented answer: on the
            // console the byte read there is not this event's mode either.
            continue;
        }

        // `lbz r11, 0xEC(r31) / cmpw r11, r19` -- RaceEventData::mu8Mode against the argument.
        if (static_cast<RaceEventData::EModeType>(lpEventData->GetMode()) == leModeType)
        {
            ++luEventTypeCount;
        }
    }

    return luEventTypeCount;
}

// ===================================================================================
// ProgressionManager::GetEventTypeUniqueWinCount  @ 0x82370758
//
// The same walk as GetEventCountForType, additionally requiring the record's FLAGS to carry the
// caller's win bit: `lhz r11, 4(r30) / and r11, r11, r17 / cmpwi 0 / beq`. The mask arrives as a
// ProfileEvent::Flags value in r5, used with the halfword loaded from the profile record.
//
// The console's own assert line numbers differ from the twin above (:4401 / :4404) because the
// two functions sit ~2800 lines apart in BrnProgressionManager.cpp; they are the same two texts.
// ===================================================================================
u32 ProgressionManager::GetEventTypeUniqueWinCount(RaceEventData::EModeType leModeType, ProfileEvent::Flags leWinType) const
{
    const u32 luEventCount = mProfile.GetEventCount();
    u32       luWinCount   = 0;

    for (u32 luIndex = 0; luIndex < luEventCount; ++luIndex)
    {
        const ProfileEvent* lpProfileEvent = mProfile.GetEvent(luIndex);
        if (lpProfileEvent == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpProfileEvent", CGS_ASSERT_LOCATION(KAC_COMPLETION_FILE, 4401));
            CgsDev::Assert::EndAssert();
        }

        const ProgressionData* lpProgressionData = GetProgressionData();
        const RaceEventData*   lpEventData =
            (lpProgressionData != 0) ? lpProgressionData->FindOfflineEvent(lpProfileEvent->GetID()) : 0;
        if (lpEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpEventData", CGS_ASSERT_LOCATION(KAC_COMPLETION_FILE, 4404));
            CgsDev::Assert::EndAssert();
            continue;   // [PC GUARD] -- see GetEventCountForType's note on the same tail
        }

        if (static_cast<RaceEventData::EModeType>(lpEventData->GetMode()) == leModeType &&
            (lpProfileEvent->GetFlags() & leWinType) != 0)
        {
            ++luWinCount;
        }
    }

    return luWinCount;
}

// ===================================================================================
// ProgressionManager::CheckForAllModeTypeCompletion  @ 0x82389698
//
// PreWorldUpdate's 2 s hold fires this once per armed mode: if the profile has not already been
// shown the "all <mode> events won" message, and the number of DISTINCT events of that mode the
// player has won has caught up with the number authored, post AllEventTypeWonAction (game action
// 206, size 4) and latch the profile bit so it is shown once ever.
//
// ⭐ THE WIN FLAG DEPENDS ON THE MODE, AND ONLY ON ONE MODE. `cmpwi cr6, r31, 4 / beq` at
// 0x823896DC: mode 4 == RaceEventData::E_MODE_BURNING_ROUTE asks for
// ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE (16); every other mode asks for
// E_FLAG_RANK_WIN (4). That is the binary distinguishing a burning route -- which is won by
// beating its target and is flagged as a "special event" win -- from a race you rank-win.
// (The `li r4, 4` on that same arm is the compiler re-materialising leModeType, which is
// already 4 there; the MASK is the only thing that changes.)
//
// ⚠️ THE COMPARISON IS `>=`, UNSIGNED (`cmplw r3, r28 / blt -> skip`). A player who has won more
// unique events of a mode than the authored count still passes; it is not an equality.
//
// ⓘ THE PAYLOAD IS AN EVENT TYPE, NOT A GAME MODE. `mr r3, r31 / bl GetEvent` -- the console
// calls ProgressionManager::GetEvent with the MODE in r3 and stores its returned word. GetEvent
// @0x82359850 ignores its `this` entirely (its switch is on r3), which is why the disassembler
// prints it as a one-argument function; the tree declares it as the non-static const member the
// DWARF gives, and the value it returns is identical either way. See BrnGameActions.h's note on
// AllEventTypeWonAction::leGameEventType.
// ===================================================================================
void ProgressionManager::CheckForAllModeTypeCompletion(
        GsmIO::GameActionQueue* lpGameActionQueue, RaceEventData::EModeType leModeType)
{
    // `clrlwi r11, r3, 24 / cmplwi 0 / bne` -- already shown for this mode, nothing to do.
    if (mProfile.GetSeenAllEventTypeWonMessage(leModeType))
    {
        return;
    }

    const u32 luEventCountForType = GetEventCountForType(leModeType);

    const u32 luEventTypeUniqueWinCount =
        (leModeType != RaceEventData::E_MODE_BURNING_ROUTE)
            ? GetEventTypeUniqueWinCount(leModeType, ProfileEvent::E_FLAG_RANK_WIN)
            : GetEventTypeUniqueWinCount(RaceEventData::E_MODE_BURNING_ROUTE, ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE);

    if (luEventTypeUniqueWinCount < luEventCountForType)
    {
        return;
    }

    GsmIO::AllEventTypeWonAction lAllEventTypeWonAction;
    lAllEventTypeWonAction.leGameEventType =
        static_cast<GsmIO::EGameModeType>(GetEvent(leModeType));
    lpGameActionQueue->AddEvent(
        &lAllEventTypeWonAction,
        GsmIO::E_ACTION_ALL_EVENT_TYPE_WON,                                     // li r5, 0xCE
        static_cast<s32>(sizeof(lAllEventTypeWonAction)));                      // li r6, 4

    mProfile.SetSeenAllEventTypeWonMessage(leModeType);

}

// ===================================================================================
// ProgressionManager::UnlockTrophyForEventTypeAllCompleted  @ 0x82395FE8
//
// OnEventFinishUpdateProfile's "every event of this game mode is now completed" seat: map the
// EGameModeType onto the TrophyUnlockData::UnlockType that rewards completing all of them, and
// tail-call OnTrophyUnlock. The console is a 9-entry jump table (cases 0..8; anything above 8
// returns immediately through `bgtlr`), and four of the nine cases deliberately do nothing.
//
// ⓘ CASE 8 IS NOT A TYPO. E_MODE_MARKED_MAN maps to E_UNLOCKTYPE_COMPLETE_ALL_SURVIVORS (15),
// not to an "all marked man" type -- Marked Man was authored as the Survivor mode and the trophy
// table kept the older name. Case 7 (STUNT_ATTACK) -> 16 and case 5 (BURNING_ROUTE) -> 13 are
// the neighbouring proofs that the mapping is by MODE and not by enumerator arithmetic.
// ⓘ The four silent cases are FACE_OFF (1), OFFLINE_SHOWTIME (2), PURSUIT (4) and ELIMINATOR (6)
// -- E_UNLOCKTYPE_COMPLETE_ALL_ELIMINATORS (14) exists in the table but nothing reaches it here.
// ===================================================================================
void ProgressionManager::UnlockTrophyForEventTypeAllCompleted(GsmIO::EGameModeType leGameModeType)
{
    switch (leGameModeType)
    {
        case GsmIO::E_MODE_OFFLINE_RACE:                                        // jumptable case 0
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_RACES);          // li r4, 0xB
            break;
        case GsmIO::E_MODE_ROAD_RAGE:                                           // case 3
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_ROADRAGES);      // li r4, 0xC
            break;
        case GsmIO::E_MODE_BURNING_ROUTE:                                       // case 5
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_BURNINGROUTES);  // li r4, 0xD
            break;
        case GsmIO::E_MODE_STUNT_ATTACK:                                        // case 7
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_STUNTATTACK);    // li r4, 0x10
            break;
        case GsmIO::E_MODE_MARKED_MAN:                                          // case 8
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_SURVIVORS);      // li r4, 0xF
            break;
        default:
            // cases 1, 2, 4, 6 (`blr`) and everything above 8 (`bgtlr`): no trophy.
            break;
    }
}

// ===================================================================================
// ProgressionManager::SendTrophyUnlockUpdate  @ 0x823892B8
//
// THE ONLY DRAIN OF mQueueOfTrophyCarUnLocks. PreWorldUpdate calls it every frame the queue is
// non-empty; it posts the TAIL element as game action 204 (E_ACTION_TROPHY_UNLOCK, 16 B) and
// Erases it, so one queued trophy car leaves per frame.
//
// ⭐ THE TAIL, NOT THE HEAD. `lwz r11, 0xC0(r31) / addi r30, r11, -1` -- lTrophyUnlockToSend is
// GetLength() - 1, and Erase() then shifts nothing (it is the last slot). Draining from the
// front would have re-ordered nothing visible but would have moved 15 bytes per element per
// frame; this is the binary's choice and it is also why the queue behaves as a stack.
//
// ⚠️ NO EMPTY-QUEUE GUARD, ON PURPOSE. With a length of 0 the index is (u32)-1 and the array's
// own checked operator[] fires "Array index out of bounds" -- exactly what the console does
// (`addi r30, r11, -1` with r11 == 0). The gate lives at the CALL SITE, in PreWorldUpdate's
// `if (GetLength() > 0)`, which is where the console puts it too.
//
// The two asserts are the console's, by name and by line; both read the element through a fresh
// operator[] call, as the three separate `bl sub_82360210` in the asm show.
// ===================================================================================
void ProgressionManager::SendTrophyUnlockUpdate(GsmIO::GameActionQueue* lpGameActionQueue)
{
    // GetLength() owns the CgsArray.h:336 "Array used before Construct/Clear was called" assert
    // the console fires here inline (`lwz r11, 0xC0(r31) / cmpwi -1`).
    const u32 luTrophyUnlockToSend = mQueueOfTrophyCarUnLocks.GetLength() - 1;

    if (mQueueOfTrophyCarUnLocks[luTrophyUnlockToSend].meUnlockType ==
            TrophyUnlockData::E_UNLOCKTYPE_NONE)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mQueueOfTrophyCarUnLocks[lTrophyUnlockToSend].meUnlockType != "
            "TrophyUnlockData::E_UNLOCKTYPE_NONE",
            CGS_ASSERT_LOCATION(KAC_COMPLETION_FILE, 639));
        CgsDev::Assert::EndAssert();
    }

    if (mQueueOfTrophyCarUnLocks[luTrophyUnlockToSend].mCarToUnlock == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(
            "mQueueOfTrophyCarUnLocks[lTrophyUnlockToSend].mCarToUnlock != kCGSID_NULL",
            CGS_ASSERT_LOCATION(KAC_COMPLETION_FILE, 640));
        CgsDev::Assert::EndAssert();
    }

    const GsmIO::TrophyUnlockAction& lrUnlock = mQueueOfTrophyCarUnLocks[luTrophyUnlockToSend];
    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lrUnlock),
                                GsmIO::E_ACTION_TROPHY_UNLOCK,                  // li r5, 0xCC
                                static_cast<s32>(sizeof(GsmIO::TrophyUnlockAction)));   // li r6, 0x10

    const CgsID lPostedCarId = lrUnlock.mCarToUnlock;
    const s32   liPostedType = static_cast<s32>(lrUnlock.meUnlockType);

    mQueueOfTrophyCarUnLocks.Erase(luTrophyUnlockToSend);

}

// ===================================================================================
// ProgressionManager::UnlockDerivedCarCollection  @ 0x8237AD70
//
// Award the derived cars of one car's livery family. Element 0 of the array is the PARENT (both
// DerivedCarArray builders Append it first), so the walk starts at index 1 -- you do not "unlock"
// the car you already picked.
//
// PER-ENTRY, in the console's order:
//   1. `bl __int64_8___GetItem` -- the entry's CgsID.
//   2. Profile::FindCar open-coded (`lwz r9, 0x26C(profile)` as the bound, `addi r10, profile,
//      0x280` as the base, 24-byte stride, 64-bit `cmpld` at offset 0). ALREADY OWNED -> skip the
//      whole entry. (Hex-Rays renders that skip as `if (&v3[6*v6 + 92] == -640)`, which is the
//      compiler's always-true null check on `&maCars[i]`, not a comparison anyone wrote.)
//   3. GetLiveryType(i) -- inlined, which is why the "luIndex < KU_MAX_AMOUNT_OF_DERIVED_CARS"
//      assert's baked location is BrnDerivedCars.h:232. The accessor owns it; this walk does not
//      duplicate it.
//   4. ⭐ THE PER-KIND GATE, and it is the whole point of the function:
//        kind == 3 (E_LIVERY_TYPE_COLOUR_ALT) -> gated on Profile+42517, GetGoldCarsUnlocked()
//        kind == 4 (E_LIVERY_TYPE_SILVER)     -> gated on Profile+42516, GetSilverCarsUnlocked()
//        anything else                        -> ungated (`li r30, 1` stands)
//      The two offsets are the console's `lbzx r30, r26, 0xA785` / `lbzx r30, r26, 0xA784`, i.e.
//      manager+42885 / manager+42884 == Profile+42517 / Profile+42516 -- the same pair
//      CheckForSpecialCarUnlocks above latches, and the gold/silver assignment agrees with it
//      (UnlockSpecialCars(3) is the 100%-gated set, UnlockSpecialCars(4) the rank-gated one).
//   5. AddCar(id, 1) == CarData::E_UNLOCK_TYPE_GIFT.
//   6. kind == 1 (E_LIVERY_TYPE_COLOUR) ALSO marks the unlock sequence already shown, so a plain
//      colour variant does not queue its own "new car" cinematic. Kinds 3 and 4 do queue one.
//
// ⚠️ THE LENGTH IS RE-READ EVERY ITERATION (`lwz r11, 0x40(r27)` at 0x8237ADE8 and 0x8237AE0C,
// inside the loop, with the sentinel assert re-fired each pass) rather than cached -- reproduced,
// because AddCar can in principle reach code that touches the array.
// ⚠️ THE INDEX IS A BYTE (`clrlwi r31, r11, 24`; DWARF `uint8_t lCurrentCar`), widened for the
// compare. Kept as the u8 it is; the capacity is 8, so nothing hangs on the wrap.
// ===================================================================================
void ProgressionManager::UnlockDerivedCarCollection(const DerivedCarArray& lrDerivedCarArray)
{
    // The leading `lwz r11, 0x40(r27) / cmpwi -1` sentinel assert, owned by GetLength().
    const u32 luInitialLength = lrDerivedCarArray.GetLength();

    u32 luUnlocked     = 0;
    u32 luAlreadyOwned = 0;

    if (luInitialLength > 1)                                       // `cmplwi r11, 1 / ble`
    {
        for (u8 lu8Index = 1;
             static_cast<u32>(lu8Index) < lrDerivedCarArray.GetLength();
             ++lu8Index)
        {
            const CgsID lDerivedCarId = lrDerivedCarArray.GetItem(lu8Index);

            if (mProfile.FindCar(lDerivedCarId) != 0)
            {
                ++luAlreadyOwned;
                continue;
            }

            const BrnResource::VehicleListEntry::ELiveryType leLiveryType =
                lrDerivedCarArray.GetLiveryType(lu8Index);

            bool lbUnlock = true;                                  // `li r30, 1`
            if (leLiveryType == BrnResource::VehicleListEntry::E_LIVERY_TYPE_COLOUR_ALT)
            {
                lbUnlock = mProfile.GetGoldCarsUnlocked();         // Profile+42517
            }
            else if (leLiveryType == BrnResource::VehicleListEntry::E_LIVERY_TYPE_SILVER)
            {
                lbUnlock = mProfile.GetSilverCarsUnlocked();       // Profile+42516
            }

            if (!lbUnlock)
            {
                continue;
            }

            AddCar(lDerivedCarId, CarData::E_UNLOCK_TYPE_GIFT);    // li r5, 1
            ++luUnlocked;

            if (leLiveryType == BrnResource::VehicleListEntry::E_LIVERY_TYPE_COLOUR)   // cmpwi r31, 1
            {
                mProfile.SetCarUnlockAlreadyShown(lDerivedCarId);
            }
        }
    }

}

// ===================================================================================
// ProgressionManager::OnPowerParkResult  @ 0x8238AF78
//
// The player just parallel-parked. Record the rating (best-of, per "was anyone else parked
// beside me"), award the online parallel-park trophy, and offer the POWER_PARK training tip.
//
//   1. Profile::RecordPowerParkingRating(liResult, lbBetweenOtherPlayers) -- inlined by the
//      console (0x8238AF94..0x8238AFCC, the two `lbz/extsb/cmpw/stb` arms on Profile+114 and
//      Profile+113); de-inlined into the DWARF-named accessor, which now lives in BrnProfile.h.
//   2. ⭐ ONLY THE "BETWEEN OTHER PLAYERS" RESULT AWARDS A TROPHY. `cmplwi r10, 0 / beq` at
//      0x8238AFCC skips OnTrophyUnlock entirely for a solo park, and the type it passes is 32 ==
//      E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE -- which is the ONLINE trophy, so the
//      flag's meaning is "there were other players' cars parked either side", not "online".
//   3. The training-tip request. The console open-codes TrainingManager::RequestTraining for
//      type 37 exactly as OnEventFinishUpdateProfile open-codes it for type 17: the
//      state==INACTIVE / !mbInPictureParadise / IsTipAllowedInGameMode / lpProfile assert
//      (BrnTrainingManager.cpp:382) / !HasPlayerSeenTrainingType / 5-second spacing
//      (`lfs 0x6C(profile) - lfs 0x18(tm) >= flt_8200426C == 5.0f`) gauntlet, then the tail store
//      pair `*(tm+4) = 37; *(tm+0) = 1`. Type 37 is not in the intro/licence family that skips
//      the spacing gate, which is why that arm is visible here and not at the type-17 site --
//      i.e. the inlining is RequestTraining specialised for 37, and it is reversed into the real
//      call, whose body (BrnTrainingManager.cpp:507) carries every one of those steps.
//
void ProgressionManager::OnPowerParkResult(s32 liResult, bool lbBetweenOtherPlayers)
{
    mProfile.RecordPowerParkingRating(liResult, lbBetweenOtherPlayers);

    if (lbBetweenOtherPlayers)
    {
        OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE);  // li r4, 0x20
    }

    if (mpTrainingManager != 0)
    {
        mpTrainingManager->RequestTraining(E_TRAINING_TYPE_POWER_PARK);   // li r4, 0x25 == 37
    }
    else if (CgsDev::Log::gpDebugPrint != 0)
    {
        // Same hole OnEventFinishUpdateProfile's tip leg already reports: nothing in the mounted
        // set calls ProgressionManager::SetTrainingManager, so this back-pointer reads NULL.
        // DELETE-WHEN the outer ProgressionManager Construct/Prepare pair installs it.
    }
}

}

// FOLDED FROM BrnProgressionManager_EventFinish.cpp
// ===================================================================================
// BrnProgression::ProgressionManager -- THE EVENT-FINISH PROGRESSION WRITERS.
//   GameSource/Unity/../GameState/Progression/BrnProgressionManager.cpp  (per-function
//   partfile of that TU, the house Scoring/BrnScoringSystem_*.cpp precedent; the console
//   homes all three functions in BrnProgressionManager.cpp, which is why every assert
//   below is fired with that file's baked path + the console's own line number rather
//   than through CGS_ASSERT's __FILE__/__LINE__.)
//
// [stuntrace waveB / agent 10] Conductor decision #5: "an event that finishes must update
// progression". These are the three bodies ModeManager::ShowModeResults @0x823436D0 calls:
//
//   OnEventFinishUpdateProfile  @0x823A0040   (the writer -- 380 instructions)
//   HasEventBeenWonPreviously   @0x82366B30   (the "was this a first win?" query)
//   PlayerHasFinishedLastRank   @0x82370180   (the rank-exhausted query)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. THE ASM IS THE SPINE -- the Hex-Rays
// pseudocode for 0x823A0040 mis-renders three things and each one is called out at its
// site: (a) `HIDWORD(v9) = 133484` is register-pair noise for the +0x2096C offset load,
// (b) `*(a1 + 133493) = 1` is wrong (the asm stores 0 there and 1 at +133494), and
// (c) `FindCar(*(rank+104), *(rank+108))` is one 8-byte `ld r4, 0x68(r11)`, not two words.
//
// Every member is reached BY NAME through BrnProgressionManager.h / BrnProfile.h; no raw
// offset arithmetic on `this`.
//
// -----------------------------------------------------------------------------------
// OnEventFinishUpdateProfile makes every call the console body makes, in the console's
// order: the won-event car family, the all-events trophy and the silver/gold car re-check,
// the per-mode trophy, the rank fix-up and medal refresh, the WON_EVENT training tip, the
// rival unlock, the next-rank car, the achievement hook, the autosave and traffic-scale
// posts, and the completion percentage stamped into the results record.
// ===================================================================================

#include "BrnProgressionManager.h"
#include "BrnProfile.h"
#include "SharedClasses/Progression/BrnProgressionRankData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint (the [eventfinish] witness)
#include "SharedClasses/Progression/BrnProgressionData.h"               // ProgressionData (rank count / junction table)
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // EventJunction / RaceEventData
#include "GameSource/GameState/BrnGameActions.h"                        // GameStateModuleIO::ShowModeResultsAction
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<13312,16>::AddEvent
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"    // TrainingManager::RequestTraining
#include "SharedClasses/Progression/BrnTrophyUnlockData.h"              // TrophyUnlockData::UnlockType (the all-events trophy)
#include "BrnDerivedCars.h"                                             // DerivedCarArray (the maBlock58 image)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase::OnEventWin

#include <stdlib.h>                                                     // getenv (the [eventfinish] witness gate)

namespace
{
// The verbatim X360-baked source path this TU's asserts reference (identical spelling to
// BrnProgressionManager.cpp's own KAC_PROGMGR_FILE -- the console homes these functions there).
const char* const KAC_PROGMGR_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

// ---------------------------------------------------------------------------------------
// Action ids posted by OnEventFinishUpdateProfile (X360 `li r5` immediates + the byte size
// in `li r6`). Named as file-local constants rather than grown into
// GameStateModuleIO::EGameActionType, which this agent does not own -- the two enumerator
// additions are filed as a header_request. BOTH values are attested at BOTH ends:
//
//   28 (size 4, one f32) == E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK
//        producer 0x823A0608 `li r5,0x1C` / `li r6,4`, payload flt_82001C98
//        consumer TrafficEntityModule::HandleExternalRequests @0x8274B660 `case 28`, whose
//                 assert string is literally "lpSetTrafficScaleBasedOnRankAction != NULL"
//                 (BrnTrafficEntityModule.cpp:5866) and which stores the f32 at +464912.
//        DWARF BrnGameActions.h:24 E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK == 24 (+4 on X360 --
//        the SAME +4 this enum already records for E_ACTION_PREPARE_FOR_MODE 19 -> 23).
//
//   55 (size 1, one bool) == E_ACTION_REQUEST_AUTOSAVE
//        producer 0x823A05DC `li r5,0x37` / `li r6,1`, payload the literal 1
//        consumer BrnGameModule::TranslateGameActionsToGuiEvents @0x823E9CE0 `case 55` ->
//                 AddGuiEvent<BrnGui::GuiAutosaveRequestEvent>
//        DWARF BrnGameActions.h:50 E_ACTION_REQUEST_AUTOSAVE == 50 (+5 on X360 -- the same +5
//        shift this enum already records for the mode-lifecycle block).
// ---------------------------------------------------------------------------------------
// [x] RETIRED 2026-08-26 (stuntrace waveB CLOSURE round): both ids now live in
// GameStateModuleIO::EGameActionType as E_ACTION_REQUEST_AUTOSAVE (55) and
// E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK (28), and are used BY NAME below. The 55 mapping
// gained three more producers in the closure sweep (DriveThruManager::ProcessDriveThru x2,
// DriveThruManager::UnlockCarChallengeForCar, StreetManager::ProcessNewRoadScore -- all id 55
// size 1), and 28 gained GameStateModule::OnProfileLoaded posting the same id/size.

// IMAGE-CITED CONSTANTS (big-endian dump of image.bin, offset == VA - 0x82000000):
//   flt_82029BB8 = 0x3F59999A == 0.85f  -- the unlock deformation amount stamped onto every
//                                          car this function adds (`stfs f0, 0xC(carData)`).
//   flt_82001C98 = 0x3F800000 == 1.0f   -- the action-28 traffic-scale payload.
const f32 KF_UNLOCK_DEFORM_AMOUNT   = 0.85f;   // flt_82029BB8
const f32 KF_TRAFFIC_SCALE_AT_RANK  = 1.0f;    // flt_82001C98

s32       giWitnessLines         = 0;

}

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

// ------------------------------------------------------------------------------------
// ProgressionManager::HasEventBeenWonPreviously  @ 0x82366B30
//
// Linear scan of the embedded Profile's discovered-event records for luEventId, then a
// single bit test. The X360 is entirely open-coded (the Profile sits at this+0x170, so its
// miEventCount reads as `lwz r9, 0x278(r8)` and its maEvents base as `addi r10, r8, 0x7080`
// with an 8-byte stride); the bit test is `lhz r11, 4(r11) / srwi 2 / clrlwi 31`, i.e.
// bit 2 of the u16 flag word == ProfileEvent::E_FLAG_RANK_WIN (4). Not "any win" -- a
// NON_RANK or SPECIAL_EVENT win answers false, which is what makes ModeManager::
// ShowModeResults' negated copy of this mean "first RANK win for this event".
//
// Sole console callers: ModeManager::ShowModeResults @0x823436D0.
// ------------------------------------------------------------------------------------
bool ProgressionManager::HasEventBeenWonPreviously(u32 luEventId)
{
    const s32 liEventCount = static_cast<s32>(mProfile.GetEventCount());
    if (liEventCount <= 0)
    {
        return false;                                  // X360 `ble cr6, loc_82366B80 / li r3,0`
    }

    for (s32 liIndex = 0; liIndex < liEventCount; ++liIndex)
    {
        const ProfileEvent* lpcEvent = mProfile.GetEvent(static_cast<u32>(liIndex));
        if (lpcEvent->GetID() == luEventId)
        {
            return lpcEvent->IsFlagSet(ProfileEvent::E_FLAG_RANK_WIN);
        }
    }

    return false;                                      // fell off the end -- `li r3,0 / blr`
}

// ------------------------------------------------------------------------------------
// ProgressionManager::PlayerHasFinishedLastRank  @ 0x82370180
//
//   `return ProgressionData->muProgressionRankCount == (s8)mi8ProgressionRank;`
//
// The whole body is four instructions plus the resource-pointer hop: `lwz r10, 0x14(r3)`
// (ProgressionData +0x14 == muProgressionRankCount), `lbzx r11, r31, 0x2096C` + `extsb`
// (the manager's own cached rank byte at +133484, SIGN-extended), `subf` + `cntlzw` +
// `extrwi r3, r11, 1, 26` == "the difference is zero".
//
// ⚠️ NOTE, deliberately faithful: this reads the RAW byte, NOT the clamped
// GetProgressionRank() (which answers 0 for the negative "rank not set" seed and clamps to
// count-1 at the top). So "finished the last rank" means the cached rank has run one PAST
// the authored table -- count, not count-1. The count-1 comparison OnEventFinishUpdateProfile
// makes is a different test on the same pair, and both are reproduced as written.
//
// Console callers: ModeManager::ShowModeResults, GameStateModule::StartModeAtLights
// @0x82396CF8, GameStateModule::ProcessGameEvents @0x823A0A18, and OnEventFinishUpdateProfile.
// ------------------------------------------------------------------------------------
bool ProgressionManager::PlayerHasFinishedLastRank() const
{
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());
    return static_cast<s32>(mi8ProgressionRank) == liRankCount;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::SetTrainingManager
// Installer for the X360 +133440 back-pointer (see the member's banner). No console symbol:
// the console's outer Construct/Prepare pair stores it directly.
// ------------------------------------------------------------------------------------
void ProgressionManager::SetTrainingManager(BrnGameState::TrainingManager* lpTrainingManager)
{
    mpTrainingManager = lpTrainingManager;
}

// ------------------------------------------------------------------------------------
// ProgressionManager::OnEventFinishUpdateProfile  @ 0x823A0040
//
// ARG SHAPE FROM ASM: r3=this, r4=lpGameActionQueue, r5=luEventId, r6=lpAction,
// r7=leGameModeType -- exactly the DWARF's four-parameter declaration (:330).
//
// CONSOLE FLOW (labels are the asm's):
//   1. `lwz r11, 4(r19); cmpwi 1; blt` -- nothing to do unless the player actually finished
//      (lpAction->miFinishPosition >= 1). NOTE +0x04 is the FINISH POSITION, not the mode;
//      see the offset-correction banner on ShowModeResultsAction in BrnGameActions.h.
//   2. find the profile's ProfileEvent for luEventId; assert it, assert it is DISCOVERED.
//   3. set E_FLAG_FINISHED, and remember which win flag (if any) the event ALREADY carried.
//   4. clear the results action's block-58 gate and the derived-car list length inside it.
//   5. if the finish position is exactly 1 (a win): resolve the event's RaceEventData through
//      the ProgressionData event-JUNCTION table, then either
//         (a) the event has no unlock car -> rewrite the win flags to RANK_WIN|WON_EVENT_BEFORE, or
//         (b) it has one and was already special-won -> just re-set WON_SPECIAL_EVENT_BEFORE, or
//         (c) it has one and is a first special win -> unlock the car family, publish it into
//             the results action, then set WON_SPECIAL_EVENT_BEFORE.
//      (a) and (c) additionally arm the deferred "all win types for this mode" check when the
//      player sits on the LAST authored rank.
//      Then: trophy check, per-mode completion tally, medal tally + win tally, medal GUI
//      refresh, the WON_EVENT training tip, rival unlocks, the next-rank car, the achievement
//      hook and the autosave request.
//   6. tail (reached on BOTH paths): post the traffic-scale action once the player has a rank,
//      and stamp the completion percentage into the results action.
// ------------------------------------------------------------------------------------
void ProgressionManager::OnEventFinishUpdateProfile(GsmIO::GameActionQueue* lpGameActionQueue,
                                                    u32 luEventId,
                                                    GsmIO::ShowModeResultsAction* lpAction,
                                                    GsmIO::EGameModeType leGameModeType)
{
    // ---- 1. the finish-position gate ---------------------------------------------------
    if (lpAction->miFinishPosition < 1)
    {
        return;
    }

    // ---- 2. the profile's record for this event ----------------------------------------
    // X360: the scan is open-coded over Profile+0x7080 with an 8-byte stride and Profile+0x278
    // as the bound. Routed through the named Profile accessors here; the const_cast is the
    // precedent Profile::GetPlayerBaseDeformAmount already sets in this class (the console's
    // inlined scan walks the mutable array, and only the const getter is bodied).
    ProfileEvent* lpEvent = 0;
    {
        const s32 liEventCount = static_cast<s32>(mProfile.GetEventCount());
        for (s32 liIndex = 0; liIndex < liEventCount; ++liIndex)
        {
            const ProfileEvent* lpcCandidate = mProfile.GetEvent(static_cast<u32>(liIndex));
            if (lpcCandidate->GetID() == luEventId)
            {
                lpEvent = const_cast<ProfileEvent*>(lpcCandidate);
                break;
            }
        }
    }

    if (lpEvent == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpEvent", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 1669));
        CgsDev::Assert::EndAssert();
        return;   // the X360 falls through into a null deref; bail instead of faulting
                  // (the OnPlayerCarChange / GetCarColourAndPalette precedent in this TU)
    }

    if (!lpEvent->IsFlagSet(ProfileEvent::E_FLAG_DISCOVERED))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpEvent->IsFlagSet( ProfileEvent::E_FLAG_DISCOVERED )",
                                   CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 1672));
        CgsDev::Assert::EndAssert();
    }

    // ---- 3. mark it finished, and classify the win it ALREADY had ----------------------
    // X360 0x8235010C: `lhz / ori 2 / sth`, then three `rlwinm` bit tests in this order.
    lpEvent->SetFlags(static_cast<u16>(lpEvent->GetFlags() | ProfileEvent::E_FLAG_FINISHED));

    s32 lePreviousWinFlag = 0;
    {
        const u16 lu16Flags = lpEvent->GetFlags();
        if ((lu16Flags & ProfileEvent::E_FLAG_RANK_WIN) != 0)
        {
            lePreviousWinFlag = ProfileEvent::E_FLAG_RANK_WIN;
        }
        else if ((lu16Flags & ProfileEvent::E_FLAG_NON_RANK_WIN) != 0)
        {
            lePreviousWinFlag = ProfileEvent::E_FLAG_NON_RANK_WIN;
        }
        else if ((lu16Flags & ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE) != 0)
        {
            lePreviousWinFlag = ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE;
        }
    }

    // ---- 4. reset the results action's derived-car block --------------------------------
    // X360 `stb r23, 0xDF(r19)` then `stw r23, 0x98(r19)`.
    // +0x98 is NOT a free field: it is +0x58 + 0x40, i.e. the COUNT WORD of the
    // Array<CgsID,8> base sub-object of the DerivedCarArray image that lives in maBlock58
    // (BrnDerivedCars.h pins the layout: CgsID elements @+0x00, count @+0x40, livery types
    // @+0x48, their count @+0x68 -- 0x70 bytes, exactly this block's size). So the console is
    // emptying the list it is about to (maybe) fill.
    lpAction->mbHasBlock58 = 0;
    reinterpret_cast<DerivedCarArray*>(lpAction->maBlock58)->Clear();

    // ---- 5. the WIN path ----------------------------------------------------------------
    if (lpAction->miFinishPosition == 1)
    {
        // Resolve the event's RaceEventData through the event-JUNCTION table (X360 reads
        // ProgressionData +0x1C count / +0x18 base with a 16-byte stride and takes the
        // junction's OFFLINE event slot at +0x04).
        const RaceEventData* lpcRaceEventData = 0;
        {
            const u32 luProfileEventId = lpEvent->GetID();
            const ProgressionData* lpcProgressionData = mpProgressionData.operator->();
            const u32 luJunctionCount = lpcProgressionData->GetEventJunctionCount();
            for (u32 luIndex = 0; luIndex < luJunctionCount; ++luIndex)
            {
                const EventJunction* lpcJunction = lpcProgressionData->GetEventJunction(luIndex);
                if (lpcJunction->GetID() == luProfileEventId)
                {
                    lpcRaceEventData = lpcJunction->GetOfflineEvent();
                    break;
                }
            }
        }

        if (lpcRaceEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpRaceEventData != NULL", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 1705));
            CgsDev::Assert::EndAssert();
            return;   // the X360 falls through into a null deref; bail instead of faulting
        }

        // The event's unlock car id: the 8-byte doubleword at RaceEventData +0x10 that the
        // console loads with a single `ld` and tests against 0.
        const CgsID lUnlockCarId = lpcRaceEventData->GetSpecialEventCarId();

        bool lbAlreadyWonSpecialEventBefore = false;

        if (lUnlockCarId == 0)
        {
            // ---- 5(a) no unlock car: rewrite the win flags -------------------------------
            // X360 `andi. r11, r11, 0xFFD3 / ori r11, r11, 0x24 / sth`:
            // clear {RANK_WIN, NON_RANK_WIN, WON_EVENT_BEFORE} then set
            // {RANK_WIN, WON_EVENT_BEFORE} -- i.e. promote any non-rank win to a rank win.
            const u16 lu16Mask = static_cast<u16>(~static_cast<u16>(ProfileEvent::E_FLAG_RANK_WIN |
                                                                    ProfileEvent::E_FLAG_NON_RANK_WIN |
                                                                    ProfileEvent::E_FLAG_WON_EVENT_BEFORE));
            lpEvent->SetFlags(static_cast<u16>((lpEvent->GetFlags() & lu16Mask) |
                                               static_cast<u16>(ProfileEvent::E_FLAG_RANK_WIN |
                                                                ProfileEvent::E_FLAG_WON_EVENT_BEFORE)));

            ArmAllWinTypesCheckIfAtLastRank(lpcRaceEventData);
        }
        else if (lpEvent->IsFlagSet(ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE))
        {
            // ---- 5(b) already special-won: nothing to unlock ----------------------------
            lbAlreadyWonSpecialEventBefore = true;
            lpEvent->SetFlags(static_cast<u16>(lpEvent->GetFlags() |
                                               ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE));
        }
        else
        {
            // ---- 5(c) FIRST special win: unlock the event's car family ------------------
            // X360 `ori r11, r11, 0x24 / sth` == set {RANK_WIN, WON_EVENT_BEFORE}.
            lpEvent->SetFlags(static_cast<u16>(lpEvent->GetFlags() |
                                               static_cast<u16>(ProfileEvent::E_FLAG_RANK_WIN |
                                                                ProfileEvent::E_FLAG_WON_EVENT_BEFORE)));

            // ⭐ UN-PARKED P1 [progression wave 2026-09-06, lane completion] -- THE WON-EVENT
            // CAR UNLOCK. Both halves of the old park have bodies now: DerivedCarArray::
            // ConstructPatternLiveryList @0x823751C0 landed with the map arm (2026-08-27) and
            // ProgressionManager::UnlockDerivedCarCollection @0x8237AD70 is bodied in
            // BrnProgressionManager_Completion.cpp. The console, asm 0x823A0234..0x823A02C0:
            //     DerivedCarArray lCarVariants;                       // stw -1 into BOTH counts
            //     lCarVariants.ConstructPatternLiveryList(mpVehicleList, lUnlockCarId);
            //     memcpy(&lpAction->maBlock58, &lCarVariants, 0x70);
            //     lpAction->mbHasBlock58 = true;                      // stb r27(1), 0xDF(r19)
            //     CarData* lpCarData = AddCar(lCarVariants.GetItem(1), 1);   // E_UNLOCK_TYPE_GIFT
            //     CGS_ASSERT(lpCarData != NULL, ...);                 // :1723
            //     lpCarData->SetUnlockDeformationAmount(0.85f);       // flt_82029BB8
            //     UnlockDerivedCarCollection(lCarVariants);
            //
            // ⚠️ GetItem(1) IS UNGUARDED ON THE CONSOLE and stays so here: index 1 is the first
            // SIBLING of the parent, and a family with no siblings makes the checked operator[]
            // fire "Array index out of bounds" -- an assert, not a guard, so the console reads
            // the slot anyway. Adding a length test would be inventing a decision the binary
            // does not make; the authored data is what guarantees a special event's car has a
            // pattern-livery sibling.
            // ⓘ The 0x70-byte memcpy is written as the whole-object assignment it is: the type
            // is pointer-free (CgsID elements + a count word + the livery kinds + their count),
            // so the copy is byte-for-byte the console's and needs no <cstring>.
            {
                DerivedCarArray lCarVariants;
                lCarVariants.ConstructPatternLiveryList(mpVehicleList, lUnlockCarId);

                *reinterpret_cast<DerivedCarArray*>(lpAction->maBlock58) = lCarVariants;
                lpAction->mbHasBlock58 = 1;

                CarData* lpCarData = AddCar(lCarVariants.GetItem(1), CarData::E_UNLOCK_TYPE_GIFT);
                if (lpCarData == 0)
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert("lpCarData != NULL", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 1723));
                    CgsDev::Assert::EndAssert();
                }
                else
                {
                    // [PC GUARD] the console stores the deform amount through the record whether
                    // or not the assert above fired. AddCar cannot return null in this tree --
                    // Profile::AddCar asserts and returns a real record -- so the `else` costs no
                    // behaviour and only removes a host access violation on a null record.
                    lpCarData->SetUnlockDeformationAmount(KF_UNLOCK_DEFORM_AMOUNT);
                }

                UnlockDerivedCarCollection(lCarVariants);
            }

            ArmAllWinTypesCheckIfAtLastRank(lpcRaceEventData);

            // X360 loc_823A0310 / loc_823A032C: BOTH sub-paths of 5(c) end by setting this.
            lpEvent->SetFlags(static_cast<u16>(lpEvent->GetFlags() |
                                               ProfileEvent::E_FLAG_WON_SPECIAL_EVENT_BEFORE));
        }

        // ---- the trophy gate ------------------------------------------------------------
        // X360: `r31 = *(this + 0x3E8)` == Profile+0x278 == miEventCount -- the console calls
        // that value "luMedalCount" in its own assert text.
        const u32 luMedalCount = mProfile.GetEventCount();
        u32 luRankWins = 0;
        u32 luNonRankWins = 0;
        u32 luSpecialEventWins = 0;
        const u32 luTotalWinCount =
            mProfile.GetTotalWinCount(luRankWins, luNonRankWins, luSpecialEventWins);

        if (luRankWins > luMedalCount)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("luRankWin <= luMedalCount", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 1771));
            CgsDev::Assert::EndAssert();
        }

        // `li r4, 9` -- the every-event-won trophy.
        if (luTotalWinCount >= luMedalCount)
        {
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_COMPLETE_ALL_JUNCTIONEVENTS);
        }
        CheckForSpecialCarUnlocks();

        // ---- the per-mode completion tally ----------------------------------------------
        mProfile.AddGameModeTypeCompleted(leGameModeType);

        // ---- the "every event of this mode is completed" check ---------------------------
        // Console (asm 0x823A0414..0x823A0438):
        //     if (GetGameModeTypeAmount(mode) == GetGameModeTypeCompletedAmountSinceTheStart(mode))
        //         UnlockTrophyForEventTypeAllCompleted(mode);
        // ⭐ [progression wave 2026-09-06, lane profile] the CONDITION is no longer parked:
        // Profile::GetGameModeTypeCompletedAmountSinceTheStart @0x82354B98 now has a body
        // (BrnProfile.cpp), so both sides of the comparison are the console's own reads.
        if (mProfile.GetGameModeTypeAmount(leGameModeType)
            == mProfile.GetGameModeTypeCompletedAmountSinceTheStart(leGameModeType))
        {
            // ⭐ UN-PARKED P4 [progression wave 2026-09-06, lane completion]: console
            // `UnlockTrophyForEventTypeAllCompleted(leGameModeType);` @0x82395FE8 (DWARF
            // BrnProgressionManager.cpp:351), bodied in BrnProgressionManager_Completion.cpp --
            // a 9-case map from the game mode onto the TrophyUnlockData::UnlockType that rewards
            // completing every event of it (RACE->11, ROAD_RAGE->12, BURNING_ROUTE->13,
            // STUNT_ATTACK->16, MARKED_MAN->15; the other four modes award nothing).
            UnlockTrophyForEventTypeAllCompleted(leGameModeType);
        }

        // ---- the medal + win tallies (THE payoff writes) --------------------------------
        // X360 `cmplwi r21, 4 / beq` -- skipped entirely when the event had ALREADY been
        // rank-won, so a re-win never double-counts.
        if (lePreviousWinFlag != ProfileEvent::E_FLAG_RANK_WIN)
        {
            if (!lbAlreadyWonSpecialEventBefore)
            {
                // X360 `addis r11, r20, 1 / addi r11, r11, -0x59F0` == Profile + 0xA610
                // == Profile+42512 == muMedalCountFromTheStart; `lwz / addi 1 / stw`.
                mProfile.SetMedalCountFromTheStart(mProfile.GetMedalCountFromTheStart() + 1);
            }

            mProfile.AddWinForGameMode(leGameModeType);

            // ✅ [progression wave: medals, 2026-09-06] PARK P5 IS PAID. FixGameModeRanks
            // @0x82395CD8 (DWARF :579) is bodied in BrnProgressionManager_Medals.cpp, so the win
            // this arm just tallied is followed by the console's own per-mode rank fix-up.
            FixGameModeRanks();
        }

        // ✅ [progression wave: medals, 2026-09-06] PARK P6 IS PAID. UpdatePlayerMedals
        // @0x8239FE50 -- the producer of game action 200 (E_ACTION_UPDATE_PLAYER_MEDALS ->
        // GuiEventMedalUpdate 307, the record BrnGameActions.h models as
        // UpdatePlayerMedalsAction) and the only console caller of UnlockToProgressionRank -- is
        // bodied in BrnProgressionManager_Medals.cpp. Winning an event now re-counts the medals,
        // ranks the licence up when the total crosses the next threshold, and refreshes the
        // medal HUD, exactly here.
        UpdatePlayerMedals(lpGameActionQueue);

        // ---- the WON_EVENT training tip -------------------------------------------------
        // X360 open-codes TrainingManager::RequestTraining(E_TRAINING_TYPE_WON_EVENT) here --
        // the state==INACTIVE / !mbInPictureParadise / IsTipAllowedInGameMode /
        // !HasPlayerSeenTrainingType gauntlet with the "lpProfile" assert
        // (BrnTrainingManager.cpp:382) in the middle -- and the tail store pair
        // `*(tm+0)=1; *(tm+4)=0x11`. That IS RequestTraining specialised for type 17 (the
        // type-8 free-burn-clock arm and the type-50 boost gauntlet both fold away), so the
        // inlining is reversed into the real call, which is bodied at
        // BrnTrainingManager.cpp:507.
        mpTrainingManager->RequestTraining(E_TRAINING_TYPE_WON_EVENT);

        // ⭐ UN-PARKED P7 [progression wave 2026-09-06, lane rivals]: console
        // `lpAction->mu64FieldC8 = UnlockRivals(lpGameActionQueue);` (DWARF :661,
        // `CgsID UnlockRivals(GameActionQueue*)`), bodied in BrnProgressionManager_Rivals.cpp
        // @0x8236F658. It answers 0 -- the "no rival unlocked" value the memset already left --
        // until the profile's medal count reaches the next rival's GetNumMedalsToUnlock(), and
        // the CgsID it answers is the unlocked rival's CAR id (`ld r3, 8(rival)`), even though
        // the DWARF names this field mNewlyUnlockedRivalID. See the body banner.
        lpAction->mu64FieldC8 = static_cast<u64>(UnlockRivals(lpGameActionQueue));

        // X360 `std r23, 0x40(r19)` / `stb r23, 0xDE(r19)` -- clear the next-rank car slot
        // and its gate before the next-rank leg below can set them.
        lpAction->mu64Field40 = 0;
        lpAction->mbHasField40 = 0;

        // ---- the next-rank car ----------------------------------------------------------
        // X360 reads the cached rank byte UNSIGNED here (`lbz / cmplwi 0`), so the -2
        // "not started" seed counts as non-zero.
        if (static_cast<u8>(mi8ProgressionRank) != 0 && !PlayerHasFinishedLastRank())
        {
            // ARTIST 0x823A0544..05A4: one 64-bit CgsID, not the decompiler's
            // spurious pair of arguments. The authored rank record owns this award.
            const ProgressionRankData* lpRank =
                GetProgressionData()->GetProgressionRankData(static_cast<s8>(mi8ProgressionRank));
            const CgsID lCarId = lpRank->GetFreeCarForRankUpID();
            if (!mProfile.FindCar(lCarId))
            {
                lpAction->mu64Field40 = lCarId;
                lpAction->mbHasField40 = true;
                AddCar(lCarId, 1)->SetUnlockDeformationAmount(KF_UNLOCK_DEFORM_AMOUNT);
            }
        }

        // ---- the achievement hook -------------------------------------------------------
        if (mpAchievementManager == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("mpAchievementManager", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 1831));
            CgsDev::Assert::EndAssert();
        }
        mpAchievementManager->OnEventWin(leGameModeType);

        // ---- the autosave request -------------------------------------------------------
        // X360 0x823A05D8: `li r6,1 / li r5,0x37`, payload one byte == 1.
        {
            u8 lu8Autosave = 1;
            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Autosave),
                                        GsmIO::E_ACTION_REQUEST_AUTOSAVE, 1);
        }
    }

    // ---- 6. the tail, reached from BOTH paths (X360 loc_823A05F0) ----------------------
    // `lbzx / extsb / cmpwi 1 / blt` -- SIGN-extended here (unlike the unsigned read above),
    // so the -2 "not started" seed does NOT post.
    if (static_cast<s32>(mi8ProgressionRank) >= 1)
    {
        f32 lfTrafficScale = KF_TRAFFIC_SCALE_AT_RANK;   // flt_82001C98 == 1.0f
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lfTrafficScale),
                                    GsmIO::E_ACTION_SET_TRAFFIC_SCALE_BASED_ON_RANK, 4);
    }

    // `stfs f1, 0xD0(r19)` -- the results record's f32 at +0xD0.
    const f32 lfCompletionPercentage = ComputeCompletionPercentage();
    lpAction->mfCurrentGameCompletePercentage = lfCompletionPercentage;

}

// ------------------------------------------------------------------------------------
// The "player is sitting on the LAST authored rank" arm, which the console emits TWICE
// (loc_823A02C4 for the special-win path and loc_823A034C for the no-unlock-car path) with
// byte-identical code. Both copies do:
//     if ((s8)(mpProgressionData->muProgressionRankCount - 1) == (s8)mi8ProgressionRank)
//     {
//         mbCheckAllWinTypesPending    = false;   // stbx r23(0), this, 0x20975
//         mbCheckAllWinTypesArmed      = true;    // stbx r27(1), this, 0x20976
//         meModeToCheckForAllWinTypes  = lpcRaceEventData->GetMode();   // lbz 0xEC / stwx
//     }
// ⚠️ FAITHFUL DETAIL: the X360 `extsb`s BOTH sides, so the comparison is between the LOW
// BYTES of (rankCount - 1) and the rank -- reproduced with the explicit s8 casts rather
// than widened to s32, because a rankCount of 0 makes the two differ (-1 vs 0xFF-as-s8).
// The reader is ProgressionManager::PreWorldUpdate @0x823A4F68, whose assert string
// ("meModeToCheckForAllWinTypes != RaceEventData::E_MODE_INVALID",
// BrnProgressionManager.cpp:382) is what PINS the third member's name.
// ------------------------------------------------------------------------------------
void ProgressionManager::ArmAllWinTypesCheckIfAtLastRank(const RaceEventData* lpcRaceEventData)
{
    const s32 liRankCount = static_cast<s32>(mpProgressionData->GetProgressionRankCount());
    if (static_cast<s8>(liRankCount - 1) != static_cast<s8>(mi8ProgressionRank))
    {
        return;
    }

    mbCheckAllWinTypesPending   = false;
    mbCheckAllWinTypesArmed     = true;
    meModeToCheckForAllWinTypes = static_cast<RaceEventData::EModeType>(lpcRaceEventData->GetMode());
}

} // namespace BrnProgression

// FOLDED FROM BrnProgressionManager_GameStats.cpp
// ===================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_GameStats.cpp
//
// ⭐⭐⭐ [pause-stats wave 2026-08-29] THE PAUSE SCREEN'S STAT PANEL, PRODUCER SIDE.
// Two BrnProgression::ProgressionManager members whose DWARF home is
// GameSource/GameState/Progression/BrnProgressionManager.cpp:
//
//   GetGameStats            @ 0x8238A6A0   (566 instructions)
//   GetTotalWinsForNextRank @ 0x82370510   (50 instructions -- GetGameStats' only caller)
//
// Split into a per-function TU rather than grown into BrnProgressionManager.cpp for the same
// reason BrnProgressionManager_Completion.cpp / _Unlocks.cpp / _EventFinish.cpp were: this pair
// pulls the StreetManager, StuntManager and VehicleList closures that the base TU deliberately
// keeps out.
//
// THE CHAIN THIS COMPLETES (the middle hop of three; the outer two were already live):
//   GUI 435 (GuiEventStatsRequest, posted by CrashNavDriverDetails::UpdateInitSetup)
//     -> game event 79   [BridgeGuiToGameState case 435, already live]
//     -> game action 180 [GameStateModule::ProcessGameEvents case 79]
//     -> GUI 436 (GuiEventStatsResponse) [TranslateGameActionsToGuiEvents case 180, this wave]
// Without it CrashNavDriverDetails::HandleStatData never runs and the Driver Details stat panel
// draws its labels with no numbers.
//
// ⛔ HEX-RAYS IS NOT USABLE ON 0x8238A6A0. Its output opens with "local variable allocation has
// failed, the output may be wrong!" and then:
//   * drops EVERY `fctiwz` -- it renders `stats.SetValue(DISTANCE_DRIVEN_ONLINE,
//     (s32)profile.mfDistanceDrivenOnline)` as a raw word copy `*(a2+24) = *(v8+100)`, i.e. it
//     turns a float->int CONVERT into a bit REINTERPRET. Five fields would have shipped as
//     nonsense bit patterns;
//   * invents eight parameters for ComputeCompletionPercentage (it takes none, and returns f32
//     in f1, which Hex-Rays also loses);
//   * renders GetTotalCarsToShutDown's plain r3 return as `HIDWORD(TotalCarsToShutDown)`;
//   * loses the `lhz`/`extsh` width on every stunt-grid read (they are s16 on both sides).
// Everything below is read off the ASSEMBLY.
// ===================================================================================

#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/SharedIO/BrnGameActionData.h"           // GameStateModuleIO::GameStats
#include "GameSource/GameState/Progression/BrnProfile.h"               // Profile / CarData / RivalData / ProfileEvent
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"  // RivalData
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"    // CarData::UnlockType
#include "GameSource/GameState/Offences/BrnStuntManager.h"             // StuntManager totals
#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h" // StreetManager roads-ruled + StreetData
#include "SharedClasses/StreetData/BrnStreetData.h"                    // StreetData::GetRoadCount
#include "SharedClasses/Progression/BrnProgressionData.h"              // ProgressionData rank table
#include "SharedClasses/Progression/BrnProgressionRankData.h"          // ProgressionRankData medal threshold
#include "SharedClasses/DataLists/VehicleList.h"                       // VehicleList::GetVehicleIndex / GetVehicleData
#include "SharedClasses/DataLists/VehicleListEntry.h"                  // IsTrophyCar / GetUnlockRank / GetLiveryType
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

namespace
{
    // The console's own baked assert locations (BeginAssert/FireAssert/EndAssert is called
    // directly rather than through CGS_ASSERT so the file/line stay the binary's, not this
    // TU's -- the same treatment BrnGameActionData.cpp's Construct uses).
    const char* const KAC_PROGRESSION_MANAGER_CPP =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";
    const char* const KAC_STUNT_MANAGER_H =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\gamestate\\offences\\BrnStuntManager.h";
}

// ===================================================================================
// ProgressionManager::GetTotalWinsForNextRank  @ 0x82370510
//
// The medal threshold of the player's CURRENT rank -- what the GUI calls "wins to next rank".
// Two asserts, then one unsigned halfword read:
//     lbz r11, 0x2096C(this) / cmplwi 0x80 / blt  -> "mi8CurrentProgressionRank >= 0" :4156
//     ResourcePtr<ProgressionData>::operator->    -> lwz r11, 0x14(pd) == the rank count
//     cmplw rank, count / blt                      -> "luIndex < muProgressionRankCount"
//                                                     (BrnProgressionData.h:330)
//     r31 = *(pd + 0x10) + 112 * rank              -> the rank record; assert non-null :4159
//     lhz r3, 0x4C(r31)                            -> mu16MedalThresholdToNextRank, ZERO-extended
// ⚠️ The rank index is the RAW sign-extended cached byte (`extsb`), not the clamped
// GetProgressionRank() -- exactly like PlayerHasFinishedLastRank.
// ===================================================================================
u32 ProgressionManager::GetTotalWinsForNextRank()
{
    // `cmplwi r11, 0x80 / blt` -- an UNSIGNED compare of the raw byte against 0x80, i.e. "the
    // signed byte is not negative".
    CGS_ASSERT_AT(mi8ProgressionRank >= 0, "mi8CurrentProgressionRank >= 0", CGS_ARTIST_FILE, 4156);   // cpp:4156

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
    }

    const s32 liRank = static_cast<s32>(mi8ProgressionRank);
    CGS_ASSERT_AT(static_cast<u32>(liRank) < lpProgressionData->GetProgressionRankCount(),
               "luIndex < muProgressionRankCount", "..\\..\\..\\SharedClasses\\Progression/BrnProgressionData.h", 330);                         // BrnProgressionData.h:330

    const ProgressionRankData* lpProgressionRank =
        lpProgressionData->GetProgressionRankData(static_cast<u32>(liRank));
    CGS_ASSERT_AT(lpProgressionRank != 0, "lpProgressionRank", CGS_ARTIST_FILE, 4159);                // cpp:4159
    if (lpProgressionRank == 0)
    {
        return 0;
    }

    // `lhz` with no `extsh` -- zero-extended, and the DWARF return type is uint32_t.
    // (GetPercentageOfEventsCompleted reads the SAME field with `extsh`; this caller does not.)
    return static_cast<u32>(lpProgressionRank->GetMedalThresholdToNextRank());
}

// ===================================================================================
// ProgressionManager::GetGameStats  @ 0x8238A6A0
//
// Fill one GameStats record. Statement order below is the console's own, so the asm reads
// top-to-bottom against it.
//
// ⚠️ FIVE FIELDS ARE FLOAT->INT CONVERTS, NOT COPIES (`lfs` + `fctiwz` + `stfiwx`):
// DISTANCE_DRIVEN_ONLINE/OFFLINE, TIME_PLAYED, BEST_DRIFT and BEST_ONCOMING. Two more stay
// float end to end (`stfs`): BEST_AIRTIME and BEST_SPIN. Hex-Rays shows all seven as plain
// word copies; that would have put raw IEEE bit patterns on screen as integers.
//
// ⚠️ FOUR INT SLOTS ARE NEVER WRITTEN BY THE CONSOLE and therefore keep Construct()'s zero:
// NUM_EVENT_MEDALS(9), TOTAL_EVENT_MEDALS(10), NUM_ROAD_RULE_MEDALS(11) and
// TOTAL_ROAD_RULE_MEDALS(12). The store list @0x8238A6A0 has no `stw` at +0x3C/+0x40/+0x44/
// +0x48. That is the binary's behaviour, not an omission here: the GUI's "all medals"/"road
// rule medals" readouts really do read zero on this build's console counterpart.
//
void ProgressionManager::GetGameStats(GsmIO::GameStats*                 lpGameStats,
                                      const BrnGameState::StuntManager* lpStuntManager,
                                      s32                               liNumChallengesCompleted)
{
    CGS_ASSERT(lpGameStats != 0, "lpGameStats");
    if (lpGameStats == 0)
    {
        return;
    }

    lpGameStats->Construct();

    // ---- 1) the profile scalars the console converts on the way in ----------------------
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_ONLINE,
                          static_cast<s32>(mProfile.GetDistanceDrivenOnline()));   // lfs 0x64 / fctiwz
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_DISTANCE_DRIVEN_OFFLINE,
                          static_cast<s32>(mProfile.GetDistanceDrivenOffline()));  // lfs 0x68 / fctiwz
    // `lfsx f0, r21, 0x1CD28` -- the REAL-time counter at Profile+118056, not mfInCarTimePlayed.
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TIME_PLAYED,
                          static_cast<s32>(mProfile.GetRealTimePlayed()));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_POWER_PARKING,
                          static_cast<s32>(mProfile.GetPowerParkingBestRating()));            // lbz/extsb 0x71
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_POWER_PARKING_BETWEEN_OTHER_PLAYERS,
                          static_cast<s32>(mProfile.GetPowerParkingBetweenOtherPlayersBestRating())); // 0x72

    // ---- 2) CARS COLLECTED -- one pass over the profile's car list ----------------------
    // A car counts when EITHER
    //   (a) its CarData unlock type is E_UNLOCK_TYPE_SPONSOR (5) and the player's progression
    //       rank has reached the vehicle entry's required rank (`lbz 0x99`), OR
    //   (b) the entry's flags word has bit 0 set (`lwz 0x94 & 1`) AND its livery-type tag is
    //       NOT one of 1 / 3 / 4 (`lbz 0xE9`) -- i.e. a base or pattern car, never one of its
    //       colour / silver variants.
    // ⛔ [owner list 2026-09-28, L6] (b) WAS INVERTED, and CARS OWNED read 171/86 on the owner's
    // profile (the 101 colour + 70 silver variants it holds) where the console shows 70/86.
    // Read the tail of the livery test with both arms named:
    //     0x8238A848  lbz    r11, 0xE9(r31)
    //     0x8238A84C  cmplwi r11, 1 / beq 0x8238A868     ; }
    //     0x8238A854  cmplwi r11, 3 / beq 0x8238A868     ; } r11 = (livery in {1,3,4})
    //     0x8238A85C  cmplwi r11, 4 / mr r11, 0 / bne 0x8238A86C
    //     0x8238A868  li     r11, 1                      ; }
    //     0x8238A870  cmplwi r11, 0
    //     0x8238A874  li     r11, 1
    //     0x8238A878  beq    0x8238A880                  ; NOT in the set -> r11 = 1 -> COUNT
    //     0x8238A87C  mr     r11, 0                      ; in the set (or bit 0 clear) -> no count
    //     0x8238A884  cmplwi r11, 0 / beq 0x8238A890 (skip) ; 0x8238A88C addi r26, r26, 1
    // The set is exactly VehicleListEntry::IsLiveryColour() -- the same {1, 3, 4} the colour-livery
    // builders test (ConstructColourLiveryList @0x82374F60) -- so it is spelled through it.
    // The two asserts are the console's: the index-range one is the inlined
    // Profile::GetCarData's (BrnProfile.h:1923) and the two null ones are this function's own
    // (BrnProgressionManager.cpp:3475 / :3478).
    s32 liCarsCollected = 0;
    if (mpVehicleList != 0)
    {
        const s32 liCarCount = mProfile.GetCarCount();
        for (s32 liCarIndex = 0; liCarIndex < liCarCount; ++liCarIndex)
        {
            if (liCarIndex < 0 || liCarIndex >= mProfile.GetCarCount())
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("liCarIndex >= 0 && liCarIndex < miCarCount",
                                           CGS_ASSERT_LOCATION("..\\..\\..\\GameSource\\GameState/Progression/BrnProfile.h", 1923));
                CgsDev::Assert::EndAssert();
            }

            const CarData* lpCarData = mProfile.GetCarData(liCarIndex);
            if (lpCarData == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpCarData", CGS_ASSERT_LOCATION(KAC_PROGRESSION_MANAGER_CPP, 3475));
                CgsDev::Assert::EndAssert();
                continue;   // [marked deviation] the console reads on through the null
            }

            const s32 liVehicleIndex = mpVehicleList->GetVehicleIndex(lpCarData->GetId());
            const BrnResource::VehicleListEntry* lpVehicleData =
                mpVehicleList->GetVehicleData(liVehicleIndex);
            if (lpVehicleData == 0)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("lpVehicleData", CGS_ASSERT_LOCATION(KAC_PROGRESSION_MANAGER_CPP, 3478));
                CgsDev::Assert::EndAssert();
                continue;   // [marked deviation] the console reads on through the null
            }

            if (lpCarData->GetUnlockType() == CarData::E_UNLOCK_TYPE_SPONSOR)
            {
                // `extsb r10, r3` on GetProgressionRank's return -- it is an s8 on the console.
                if (GetProgressionRank() >= static_cast<s32>(lpVehicleData->GetUnlockRank()))
                {
                    ++liCarsCollected;
                }
            }
            else if (lpVehicleData->IsTrophyCar() && !lpVehicleData->IsLiveryColour())
            {
                // IsTrophyCar() is the `lwz 0x94 & 1` bit (VehicleListEntry.h's name for it).
                ++liCarsCollected;
            }
        }
    }
    else if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[stats] mpVehicleList is NULL -- CARS_COLLECTED reported as 0 (PC bring-up)\n";
    }
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_CARS_COLLECTED, liCarsCollected);

    // ---- 3) the three medal tallies ------------------------------------------------------
    // ⚠️ NAME TENSION, NOT A BUG: Profile::GetTotalWinCount's three out-params are DWARF-named
    // (rankWins, nonRankWins, specialEventWins) and the console stores them, in that order,
    // into MEDALS_GOLD / MEDALS_SILVER / MEDALS_BRONZE (`stw` +0x30/+0x34/+0x38 @0x8238A8CC).
    // The GUI then shows them as the gold/silver/bronze medal counts. Transcribed as the asm
    // has it; the naming disagreement is the original's.
    u32 luGold = 0;
    u32 luSilver = 0;
    u32 luBronze = 0;
    mProfile.GetTotalWinCount(luGold, luSilver, luBronze);
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_MEDALS_GOLD,   static_cast<s32>(luGold));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_MEDALS_SILVER, static_cast<s32>(luSilver));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_MEDALS_BRONZE, static_cast<s32>(luBronze));

    // ---- 4) collected stunt elements (profile sets) and their world totals ---------------
    // The console inlines Set<CgsID,512>::GetLength three times, each behind its own
    // CgsSet.h:227 "Set used before Construct/Clear was called" sentinel check; that IS
    // Profile::GetStuntElementCount, which carries the same assert.
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_JUMPS,
                          mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_SMASHES,
                          mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH));
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_STUNTS,
                          mProfile.GetStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD));

    // `lhz 0x5C4 / 0x5C6 / 0x5C8` + `extsh` -- the world totals, SIGNED halfwords. Note the
    // GameStats enumerator named STUNTS_MAX is the BILLBOARD total (the DWARF's "stunts" is
    // this build's billboards; the district columns below agree, and so does the GUI).
    if (lpStuntManager != 0)
    {
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_JUMPS_MAX,
                              lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP));
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_SMASHES_MAX,
                              lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH));
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_STUNTS_MAX,
                              lpStuntManager->GetTotalStuntElementCount(BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD));
    }

    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TAKEDOWNS,
                          mProfile.GetTotalTakedownCount());                       // lwz 0x198

    // ---- 5) ACHIEVEMENTS -- the population count of the manager's 64-bit earned mask -----
    // ⛔ THE ONE FIELD THIS WAVE DOES NOT PRODUCE, AND IT IS SAID OUT LOUD RATHER THAN LEFT
    // TO LOOK FINISHED. X360 @0x8238A9D4: `lwzx r11, this, 0x20938` (mpAchievementManager),
    // `ld r11, 0x18(r11)`, then the open-coded 64-bit SWAR popcount
    //     x -= (x >> 1) & 0x5555555555555555
    //     x  = ((x >> 2) & 0x3333333333333333) + (x & 0x3333333333333333)
    //     x  = ((x >> 4) + x) & 0x0F0F0F0F0F0F0F0F
    //     result = (x * 0x0101010101010101) >> 56
    // -> maIntValues[ACHIEVEMENTS]. TWO reasons it cannot be written here yet, neither of them
    // "it was hard":
    //   (a) +0x18 is the first 8-byte-aligned slot PAST AchievementManagerBase's 0x14-byte head
    //       (vtable + the four DWARF-attested back-pointers, :228..:231 -- that DWARF member
    //       list ENDS there), i.e. it belongs to the CONCRETE manager. The console's concrete
    //       manager is BrnGameState::AchievementManagerX360 (its AchievementEarnt @0x82367000 /
    //       IsAchievementEarnt @0x82367240 both bound the index at 0x32 == 50 achievements, so
    //       the mask is one u64), and that class does not exist in this tree at all -- only a
    //       placeholder AchievementManagerPS3 with no members.
    //   (b) mpAchievementManager is NULL on this build regardless: nothing calls
    //       ProgressionManager::Prepare2 (the same hole this class's own header FLAGs).
    // Reaching +0x18 through a cast off the base pointer would be exactly the offset-hack this
    // project's faithfulness gate exists to stop. The field therefore keeps Construct()'s zero
    // and logs why -- a zero on screen that is attributable, not mysterious.
    if (CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint
            << "[stats] ACHIEVEMENTS reported as 0 -- mpAchievementManager "
            << ((mpAchievementManager != 0) ? "present but AchievementManagerX360 is unreconstructed"
                                            : "is NULL (PC bring-up)")
            << "\n";
    }

    // ---- 6) the two 3x5 stunt-element district grids -------------------------------------
    // MAX comes from the StuntManager's authored world grid (+0x5CA, s16); CURRENT comes from
    // the profile's own per-county tally (+96440, s16). Both are read `lhz`+`extsh`.
    // The two asserts inside the county loop are the inlined
    // StuntManager::GetMaxStuntElementCountByCounty's own pair (BrnStuntManager.h:284/285).
    for (s32 liStuntElementType = 0;
         liStuntElementType < GsmIO::GameStats::KI_STUNT_ELEMENT_TYPE_COUNT;
         ++liStuntElementType)
    {
        for (s32 liCounty = 0; liCounty < GsmIO::GameStats::KI_COUNTY_VALID_COUNT; ++liCounty)
        {
            if (liStuntElementType == BrnGameState::E_STUNT_ELEMENT_TYPE_COUNT)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("leType != E_STUNT_ELEMENT_TYPE_COUNT",
                                           CGS_ASSERT_LOCATION(KAC_STUNT_MANAGER_H, 284));
                CgsDev::Assert::EndAssert();
            }
            if (liCounty >= BrnWorld::E_COUNTY_VALID_COUNT)
            {
                CgsDev::Assert::BeginAssert();
                CgsDev::Assert::FireAssert("leCounty < BrnWorld::E_COUNTY_VALID_COUNT",
                                           CGS_ASSERT_LOCATION(KAC_STUNT_MANAGER_H, 285));
                CgsDev::Assert::EndAssert();
            }

            if (lpStuntManager != 0)
            {
                lpGameStats->SetMaxStuntElementPerCounty(
                    liStuntElementType, liCounty,
                    lpStuntManager->GetTotalStuntElementCountByCounty(
                        static_cast<BrnGameState::StuntElementType>(liStuntElementType), liCounty));
            }

            lpGameStats->SetCurrentStuntElementPerCounty(
                liStuntElementType, liCounty,
                mProfile.GetStuntElementCountByCounty(
                    static_cast<BrnGameState::StuntElementType>(liStuntElementType),
                    static_cast<BrnWorld::ECounty>(liCounty)));
        }
    }

    // ---- 7) the 13 takedown-type tallies -------------------------------------------------
    // One clean 13-word copy from Profile+416 into GameStats+0xA8. The two asserts are the
    // inlined Profile::GetTakedownTypeCount (BrnProfile.h:2167) and
    // GameStats::SetTakedownTypeCount (BrnGameActionData.h:454) range checks.
    for (s32 liTakedownType = 0;
         liTakedownType < GsmIO::GameStats::KI_TAKEDOWN_TYPE_COUNT;
         ++liTakedownType)
    {
        lpGameStats->SetTakedownTypeCount(liTakedownType,
                                          mProfile.GetTakedownTypeCount(liTakedownType));
    }

    // ---- 8) roads ruled + the world's road count ------------------------------------------
    if (mpStreetManager != 0)
    {
        lpGameStats->SetRoadsRuledCount(
            0, mpStreetManager->GetNumberOfParTimeTrialRoadsRuledByLocalPlayer());   // +0xDC
        lpGameStats->SetRoadsRuledCount(
            1, mpStreetManager->GetNumberOfParShowTimeRoadsRuledByLocalPlayer());    // +0xE0
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TOTALROADSRULED,
                              mpStreetManager->GetNumberOfCompleteRoadsRuledByLocalPlayer());  // +0x90

        // `lwz r11, 0x1CC8(streetMgr)` (the ResourcePtr's main-memory word, behind the
        // CgsResourcePtr.h:563 assert) then `lwz r11, 0x20(r11)` == StreetData::miRoadCount.
        const BrnStreetData::StreetData* lpStreetData = mpStreetManager->GetStreetData();
        CGS_ASSERT(lpStreetData != 0,
                   "Can not instance resource pointer - it has no main memory resource");
        if (lpStreetData != 0)
        {
            lpGameStats->SetTotalRoads(lpStreetData->GetRoadCount());
        }
    }
    else
    {

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[stats] mpStreetManager is NULL -- roads-ruled reported as 0 and total-roads"
                   " FORCED to 0 (Construct does not zero it; PC bring-up)\n";
        }
    }

    // The console re-zeroes the favourite/forgotten car ids here (`std r20, 0(r28)` /
    // `std r20, 8(r28)` with r20 == 0) even though Construct already did -- neither has a
    // producer in this function. Reproduced so the record is not left looking under-written.
    lpGameStats->SetValue(GsmIO::GameStats::E_ID_VALUE_TYPE_FAVOURITE_CAR, static_cast<CgsID>(0));
    lpGameStats->SetValue(GsmIO::GameStats::E_ID_VALUE_TYPE_FORGOTTEN_CAR, static_cast<CgsID>(0));

    // ---- 9) the NEMESIS -- the rival who has taken the player down most --------------------
    // `r7 = profile + 0x6280` walked with `addi r7, r7, 0x38`; the compared field is
    // RivalData+0x18 == miTakedownFromCount and the kept value is the record's leading CgsID.
    // Strictly greater-than, so the FIRST rival at the maximum wins.
    CgsID lNemesisId       = 0;
    s32   liNemesisTakedowns = 0;
    const s32 liRivalCount = mProfile.GetRivalCount();
    for (s32 liRivalIndex = 0; liRivalIndex < liRivalCount; ++liRivalIndex)
    {
        const RivalData* lpRival = mProfile.GetRivalData(liRivalIndex);
        if (lpRival->miTakedownFromCount > liNemesisTakedowns)
        {
            lNemesisId         = lpRival->mRivalId;
            liNemesisTakedowns = lpRival->miTakedownFromCount;
        }
    }
    lpGameStats->SetValue(GsmIO::GameStats::E_ID_VALUE_TYPE_NEMESIS, lNemesisId);

    // ---- 10) the personal-best block ------------------------------------------------------
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_NO_BARREL_ROLLS,
                          mProfile.GetCompletedBarrelRolls());                       // lwz 0x24C
    lpGameStats->SetValue(GsmIO::GameStats::E_FLOAT_VALUE_TYPE_BEST_SPIN,
                          mProfile.GetCompletedAirSpinAngle());                      // lfs 0x250 -> stfs
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_DRIFT,
                          static_cast<s32>(mProfile.GetCompletedDriftDistance()));   // lfs 0x258 / fctiwz
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_BOOST_CHAIN,
                          static_cast<s32>(mProfile.GetBestNewBurnoutChainScore())); // lwz 0x74
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_ONCOMING,
                          static_cast<s32>(mProfile.GetOncomingDistance()));         // lfs 0x25C / fctiwz
    lpGameStats->SetValue(GsmIO::GameStats::E_FLOAT_VALUE_TYPE_BEST_AIRTIME,
                          mProfile.GetAirMaximum());                                 // lfs 0x260 -> stfs
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_BEST_SHOWTIME,
                          mProfile.GetNewHighShowtimeScore());                       // lwz 0x264

    // ---- 11) wins to next rank -------------------------------------------------------------
    // `extsb(mi8ProgressionRank) == ProgressionData::GetProgressionRankCount()` is exactly
    // PlayerHasFinishedLastRank @0x82370180, inlined; -1 is the "no next rank" sentinel the
    // GUI's licence card already understands.
    if (PlayerHasFinishedLastRank())
    {
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK, -1);
    }
    else
    {
        lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TOTAL_WINS_FOR_NEXT_RANK,
                              static_cast<s32>(GetTotalWinsForNextRank()));
    }

    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_TOTAL_CARS_TO_SHUTDOWN,
                          mProfile.GetTotalCarsToShutDown());
    // `stfs f1, 0xA4(r28)` -- the f32 return goes into the record as a FLOAT, and the GUI
    // bridge is what converts it to an integer percentage (`fctiwz`, case 180).
    lpGameStats->SetValue(GsmIO::GameStats::E_FLOAT_VALUE_PERCENTAGE_COMPLETE,
                          ComputeCompletionPercentage());

    // ---- 12) events found / total, and the stunt-run high score -----------------------------
    // The console walks maEvents' flag halfword directly (`r10 = profile + 0x7084`, `lhz`,
    // `& 1`, stride 8) -- that is ProfileEvent::IsFound().
    s32       liEventsFound = 0;
    const s32 liEventCount  = static_cast<s32>(mProfile.GetEventCount());
    for (s32 liEventIndex = 0; liEventIndex < liEventCount; ++liEventIndex)
    {
        const ProfileEvent* lpEvent = mProfile.GetEvent(static_cast<u32>(liEventIndex));
        if (lpEvent != 0 && lpEvent->IsFound())
        {
            ++liEventsFound;
        }
    }
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_EVENTS_FOUND, liEventsFound);

    // The console emits each of these two stores TWICE (@0x8238AF50/+0x8238AF60 and
    // @0x8238AF58/0x8238AF6C) -- a repeated-expression artefact, not two different values.
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_EVENTS_TOTAL, liEventCount);
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_HIGHEST_STUNT_SCORE,
                          mProfile.GetBestStuntRunScore());

    // ---- 13) the X360-only third parameter --------------------------------------------------
    // `stw r10, 0x98(r28)` -- maIntValues[32], the count
    // ChallengeManager::CountCompletedChallenges handed the caller.
    lpGameStats->SetValue(GsmIO::GameStats::E_INT_VALUE_TYPE_FREEBURN_CHALLENGES_COMPLETE,
                          liNumChallengesCompleted);
}

} // namespace BrnProgression

// FOLDED FROM BrnProgressionManager_Lifecycle.cpp
// ============================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_Lifecycle.cpp
// ============================================================================================
// [progression wave: lane "lifecycle", 2026-09-06]
// THE OUTER LIFECYCLE PAIR OF BrnProgression::ProgressionManager, and the calls that hang off it.
// Every one of these had a LIVE PC caller and no body -- the species this wave closes.
//
//   Construct                       @0x8237A5F8  <- GameStateModule::Construct @0x82380388
//   Prepare                         @0x8239DC38  <- GameStateModule::Prepare   @0x8239E578 (stage 20)
//     LoadAIData                    @0x8239A0D0  <- Prepare
//   ApplyVehicleList                @0x82359A20  <- GameStateModule::Prepare   @0x8239E578 (stage 8)
//   ComputeLandmarkAISectionIndices @0x82370008  <- Prepare2                   @0x8239DC98
//   ProcessLoadedPresetRaces        @0x8236FDF8  <- Prepare2
//     HACK_SetupRaces               @0x82366968  <- ProcessLoadedPresetRaces
//     HACK_SetupRaceWithLandMarks   @0x82359B78  <- HACK_SetupRaces
//   SetupRoamingSections            @0x8236FE60  <- Prepare2
//   GetRacesAtLandmark              @0x8236F830  <- GameStateModule::SendSetLandmarkRacesAction
//                                                   @0x82381CD8 / ::ProcessGameEvents @0x823A0A18
//   LandmarkHasAvailableRaces       @0x8236F928  <- ModeManager::PlayerTriggersLandmark @0x82311A68
//
// WHAT WAS BROKEN. Without Construct, FOUR back-pointers the console installs at boot read NULL
// for the entire run -- mpStreetManager and mpStuntManager (ComputeCompletionPercentage guarded
// and logged instead of reading them), mpTrainingManager (the WON_EVENT training tip could never
// be queued; SetTrainingManager had NO caller anywhere in the tree) and mpCarSelectManager. A
// further two dozen scalars kept their host in-class initialisers instead of the console's seeds,
// two of which are NOT zero: mbPlayerMedalsUpdateRequired starts TRUE (the console asks for a
// medal pass on the very first frame) and mbDriveThruDataDirtyFlag starts TRUE. Without Prepare,
// mpAISectionData was never bound, so ComputeLandmarkAISectionIndices could not run and the
// landmark -> AI-section table FindLandmarkAISectionIndex reads stayed all-zero -- its own banner
// called that out as a one-function frontier. Without ProcessLoadedPresetRaces the preset-race
// list was empty, so LandmarkHasAvailableRaces answered "no races here" at every landmark.
//
// A partfile, not BrnProgressionManager.cpp, so this lane stays file-disjoint from the other four
// lanes of this wave (the _Completion / _EventFinish / _Unlocks / _Rivals / _PreWorldUpdate
// precedent). Every member is reached BY NAME; the X360 offsets are quoted only to show which
// member each store lands on. Console offsets are the 32-bit-pointer ABI's; the host is 64-bit.
//
// EVIDENCE. The ASSEMBLY (not the pseudocode) of each address above, plus the DecFIGS DWARF
// (references/DecFIGS/dwarfdump/GameSource/GameState/Progression/BrnProgressionManager.h) for
// declaration shape and member names. Where the two are quoted below, they agree.
// ============================================================================================
#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include <stdlib.h>                                                       // getenv (the [lifecycle] witness gate)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CgsDev::Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::AddMonitor
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"                // CgsMemory::LinearMalloc (the point-map arena)
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"      // CgsModule::EventReceiverQueue<3072,16>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"     // CgsResource::ResourceHandle (the AI-data bind)

#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile::Construct
#include "GameSource/GameState/BrnGameStateModuleIO.h"                    // OutputBuffer::GetResourceRequestInterface
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"         // RequestInterface<3072>::LoadAILanes / GetAILanes
#include "GameSource/Resource/SharedIO/BrnGameDataEvents.h"               // GameDataIO::GameDataAssetEvent (the reply record)

#include "SharedClasses/AI/AISectionsResourceType.h"                      // AISectionsData::BuildAISectionPointMap / FindNearestAISection
#include "SharedClasses/DataLists/VehicleList.h"                          // VehicleList::GetSelectableVehicleCount / GetSponsorVehicleCount
#include "SharedClasses/Trigger/BrnTriggerData.h"                         // TriggerData (landmarks + roaming locations)
#include "SharedClasses/Trigger/BrnLandmark.h"                            // complete Landmark (GetBoxRegion / GetId / GetRegionIndex)
#include "SharedClasses/Trigger/BrnRegion.h"                              // BoxRegion::GetPosition
#include "SharedClasses/Trigger/BrnRoamingLocation.h"                     // complete RoamingLocation (position + district)
#include "SharedClasses/World/BrnCollisionTag.h"                          // BrnWorld::KI_INVALID_SECTION_INDEX

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

namespace
{
    // The console's own baked assert location. BeginAssert/FireAssert/EndAssert is called
    // directly rather than through CGS_ASSERT so the file/line stay the BINARY's, exactly as the
    // sibling _EventFinish / _GameStats / _PreWorldUpdate partfiles do it.



    void FireConsoleAssert(const char* lpcExpression, s32 liLine)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lpcExpression, CGS_ASSERT_LOCATION(KAC_PROGRESSION_MANAGER_CPP, liLine));
        CgsDev::Assert::EndAssert();
    }

    // ---- Construct's two PerfMonCpu monitors ------------------------------------------------
    // 0x8237A8C8 / 0x8237A8F4: `r3 = name ; r4 = 5 ; r5 = 0 ; f1 = flt_82001C98 ; r7 = 1`.
    // r6 is NEVER written -- that is the PPC float-argument GPR skip this project's PerfMonCpu
    // header documents at length, and the reason the 5-parameter overload is the console's real
    // signature. IMAGE-CITED: image.bin (offset == VA - 0x82000000, big-endian) at 0x1C98 reads
    // 3F 80 00 00 == 1.0f, the CPU budget in milliseconds. Page 5 is `li r4, 5`.
    const char* const KPC_PRE_WORLD_MONITOR_NAME   = "Prog: Pre-World Up";
    const char* const KPC_POST_WORLD_MONITOR_NAME  = "Prog: Post-World Up";
    const f32         KF_PROGRESSION_CPU_BUDGET_MS = 1.0f;              // flt_82001C98

    // ---- the two zero-seeded float holds ----------------------------------------------------
    // IMAGE-CITED: 0x1CC0 reads 00 00 00 00 == 0.0f. Construct stfs's it into BOTH
    // mfTimeTillAllEventTypeCompleteHudMessage (+0x2097C) and
    // mfTimeTillShowAllRivalsBeatenMessage (+0x20984).
    const f32 KF_HUD_MESSAGE_HOLD_CLEARED = 0.0f;                       // flt_82001CC0

    // ---- Construct's meLastPlayerDistrict seed ----------------------------------------------
    // `li r7, 0x12 ; stwx r7, r31, 0x2094C` -- 18, i.e. one PAST the last real district, the
    // "no district yet" sentinel for the 0..17 district space SetupRoamingSections walks.
    const s32 KI_NO_LAST_PLAYER_DISTRICT = 18;

    // ---- Construct's progression-rank seed --------------------------------------------------
    // `li r10, -2 ; stbx r10, r31, 0x2096C` -- the "rank not set" seed GetProgressionRank reads
    // back as unsigned and answers 0 for.
    const s8 KI8_PROGRESSION_RANK_NOT_SET = -2;

    // ---- the AI-lanes stream ----------------------------------------------------------------
    // `li r5, 0 ; li r6, 5` on the LOAD leg and `li r5, 1 ; li r6, 5` on the GET leg.
    const s32 KI_AI_LANES_LOAD_EVENT_ID = 0;
    const s32 KI_AI_LANES_GET_EVENT_ID  = 1;
    const s32 KI_AI_LANES_POOL_ID       = 5;

    // ---- the AI-section point-map arena -----------------------------------------------------
    // Both ComputeLandmarkAISectionIndices (0x82370020 `lis r3, 4`) and SetupRoamingSections
    // (0x8236FE78 `lis r3, 4`) allocate 0x40000 bytes through operator new[] and adopt it with a
    // stack LinearMalloc, then `operator delete[]` it on the way out. The committed
    // GameStateModule_SendSetUpAllEventStarts.cpp does the identical dance for the identical
    // BuildAISectionPointMap call; same size, same idiom.
    const size_t KN_AI_POINT_MAP_ARENA_BYTES = 0x40000;

    // ---- HACK_SetupRaces' five authored races -----------------------------------------------
    // Read straight off 0x82366968's immediates. The ids are `std r10, 0x20(race)` (a full-width
    // 64-bit CgsID store); the landmark ids are the ten `std`s into the stack array at sp+0x50,
    // consumed two at a time (`li r6, 2` on every HACK_SetupRaceWithLandMarks call, cursor
    // sp+0x50 / +0x60 / +0x70 / +0x80 / +0x90).
    //
    // MEASURED, OFFLINE, AGAINST THE SHIPPED DATA (build/game/TRIGGERS.DAT, 105 landmarks):
    // NONE of the five landmark ids below exists in the retail landmark table. That is not a
    // transcription error -- it is what makes these "HACK_" races a shipped debug leftover.
    // TriggerData::FindLandmark @0x82675738 returns mpLandmarks (the FIRST landmark), not null,
    // on a miss -- the committed body and the console's both -- so the null test in
    // HACK_SetupRaceWithLandMarks never trips and each of the five races is built from landmark
    // [0] twice. Reproduced exactly rather than "fixed": the console does this too, on this data.
    const s32   KI_HACK_RACE_COUNT         = 5;
    const u32   KU_HACK_LANDMARKS_PER_RACE = 2;
    const CgsID KA_HACK_RACE_IDS[KI_HACK_RACE_COUNT] =
    {
        0x6E5D8, 0x6E5D9, 0x6E5DA, 0x6E5DB, 0x6E5DC
    };
    const char* const KAPC_HACK_RACE_NAMES[KI_HACK_RACE_COUNT] =
    {
        "Hack 01", "Hack 02", "Hack 03", "Hack 04", "Hack 05"
    };
    const CgsID KAA_HACK_RACE_LANDMARKS[KI_HACK_RACE_COUNT][KU_HACK_LANDMARKS_PER_RACE] =
    {
        { 0x751AD, 0x751B5 },   // sp+0x50 / +0x58
        { 0x7550F, 0x751AD },   // sp+0x60 / +0x68
        { 0x75517, 0x7550F },   // sp+0x70 / +0x78
        { 0x7550E, 0x75517 },   // sp+0x80 / +0x88
        { 0x751B5, 0x7550E }    // sp+0x90 / +0x98
    };

    // ---- one-shot park reporters (campaign house rule: a park is visible, never silent) ------
    void ParkOnce(bool& lrbAlreadySaid, const char* lpcMessage)
    {
        if (lrbAlreadySaid)
        {
            return;
        }
        lrbAlreadySaid = true;
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }

}

// --------------------------------------------------------------------------------------------
// ProgressionManager::Construct  (X360 0x8237A5F8)
//
// THE ASM, IN ITS OWN ORDER. Four non-null asserts (:124..:127, lines 0x7C..0x7F), the four
// pointer stores, the first block of zero seeds, Profile::Construct(this + 0x170), the second
// block of seeds, the two AddMonitor registrations with their >= 0 asserts (:197/:198), and the
// two _DEBUG members last. Every console store is accounted for below; each offset is named by
// the DecFIGS DWARF member list:
//   +0x20934 mpCarSelectManager   +0x20930 mpStreetManager    +0x20940 mpTrainingManager
//   +0x20944 mpStuntManager       +0x1F780 maPresetRaces.miCount (the Array's own count word --
//                                          maPresetRaces spans +0x1D980..+0x1F784)
//   +0x208D8 muNumPresetRaces     +0x20971 mbUpdateRivals      +0x20972 mbReturnRivals
//   +0x20928 miLastUpdatedRival   +0x2092C miLastReturnedRival
//   +0x208DC meLoadStage          +0x208E0 meAILoadStage
//   +0x20924 mpTriggerData        +0x208D0 mpCurrentCarData    +0x2093C mpModeManager
//   +0x20948 mpVehicleList        +0x2096C mi8CurrentProgressionRank = -2
//   +0x20973 mbPlayerMedalsUpdateRequired = 1  +0x20974 mbPlayerJustWonATrophyUpdateRequired
//   +0x20988 mbDriveThruDataDirtyFlag = 1      +0x2094C meLastPlayerDistrict = 18
//   +0x20950/54/58 the three road-rules tallies  +0x2096E mbHackEventNumberActive
//   +0x2096D mi8HackEventRankNumber             +0x208C8 mQueueOfTrophyCarUnLocks.miCount
//   +0x2097C mfTimeTillAllEventTypeCompleteHudMessage = 0.0f   +0x2095C miMaxCarCount
//   +0x20975 mbCheckForAllEventTypeComplete     +0x20970 mbHasJustRankedUp
//   +0x20976 mbNeedCheckForAllWinTypes          +0x20960 mNewlyUnlockedCarID (an 8-byte `stdx`
//                                                        of zero, i.e. the whole CgsID)
//   +0x20980 mbNeedToShowAllRivalsBeatenMessage +0x20981 mbShowShutDownAllIfNeeded
//   +0x20984 mfTimeTillShowAllRivalsBeatenMessage = 0.0f
//   +0x20978 meModeToCheckForAllWinTypes = RaceEventData::E_MODE_INVALID   +0x2096F mbForceAutoSaveForOneHundredPercent
//   +0x2098C miPreWorldUpdate     +0x20990 miPostWorldUpdate  (the two AddMonitor handles)
//   +0x20800 mbSendAchievementAwardedEvent_DEBUG (stb)  +0x20804 miAchievementToAward_DEBUG (stw)
//
// TWO OF THE SEEDS ARE NOT ZERO, and both are behaviour, not decoration:
//   mbPlayerMedalsUpdateRequired = TRUE  -- the console asks for a medal/rival pass on the FIRST
//     PreWorldUpdate of the session. On this build that arm is the medals lane's park, so landing
//     this seed makes that park line print ONCE (it did not print at all before). That is the
//     console's own state machine becoming visible, not a new defect.
//   mbDriveThruDataDirtyFlag     = TRUE  -- the drive-thru layer's first-frame refresh.
//
// THE 18-SLOT HEAD LOOP IS NOT HERE. The -1 written into each maRoamingSections count word is
// CgsArray's KI_UNCONSTRUCTED sentinel and belongs to the C++ constructor (ProgressionManager's
// own ctor @0x827DEA50, which this tree already has), not to Construct.
// --------------------------------------------------------------------------------------------
void ProgressionManager::Construct(BrnGameState::CarSelectManager* lpCarSelectManager,
                                   BrnGameState::StreetManager*    lpStreetManager,
                                   BrnGameState::TrainingManager*  lpTrainingManager,
                                   BrnGameState::StuntManager*     lpStuntManager)
{
    if (lpCarSelectManager == 0)
    {
        FireConsoleAssert("lpCarSelectManager != NULL", 124);
    }
    if (lpStreetManager == 0)
    {
        FireConsoleAssert("lpStreetManager != NULL", 125);
    }
    if (lpTrainingManager == 0)
    {
        FireConsoleAssert("lpTrainingManager != NULL", 126);
    }
    if (lpStuntManager == 0)
    {
        FireConsoleAssert("lpStuntManager != NULL", 127);
    }

    // The four owning back-pointers (0x8237A6D8..0x8237A6F0).
    mpCarSelectManager = lpCarSelectManager;      // +0x20934
    mpStreetManager    = lpStreetManager;         // +0x20930
    mpTrainingManager  = lpTrainingManager;       // +0x20940
    mpStuntManager     = lpStuntManager;          // +0x20944

    // First seed block (0x8237A70C..0x8237A748), before Profile::Construct.
    maPresetRaces.Construct();                                // +0x1F780 (the Array's count word)
    muNumPresetRaces        = 0;                          // +0x208D8
    mbUpdateRivalsRequested = false;                      // +0x20971 (DWARF mbUpdateRivals)
    mbReturnRivals          = false;                      // +0x20972
    miLastUpdatedRival      = 0;                          // +0x20928
    miLastReturnedRival     = 0;                          // +0x2092C
    meLoadStage             = E_LOADSTAGE_NOT_STARTED;    // +0x208DC
    meAILoadStage           = E_AI_DATA_LOAD_NOT_STARTED; // +0x208E0

    // 0x8237A74C -- `addi r3, r31, 0x170 ; bl Profile::Construct`.
    mProfile.Construct();

    // Second seed block (0x8237A750..0x8237A8C4).
    mpTriggerData                = 0;                     // +0x20924
    mpCurrentCarData             = 0;                     // +0x208D0
    mpModeManager                = 0;                     // +0x2093C
    mpVehicleList                = 0;                     // +0x20948
    mi8ProgressionRank           = KI8_PROGRESSION_RANK_NOT_SET;  // +0x2096C, `li -2` as a BYTE
    mbPlayerMedalsUpdateRequired = true;                  // +0x20973, `li r11, 1` -- NOT zero
    mbPlayerJustWonATrophyUpdateRequired = false;         // +0x20974
    mbDriveThrusDirty            = true;                  // +0x20988, `stbx r11(1)` -- NOT zero
    meLastPlayerDistrict = static_cast<BrnWorld::EDistrict>(KI_NO_LAST_PLAYER_DISTRICT); // +0x2094C
    miNumberOfParCrashRoadRulesRuledByPlayer         = 0;  // +0x20950
    miNumberOfParTimeRoadRulesRuledByPlayer          = 0;  // +0x20954
    miNumberOfNumberOfCompleteRoadRulesRuledByPlayer = 0;  // +0x20958
    mbHackEventNumberActive = false;                     // +0x2096E
    mi8HackEventRankNumber = 0;                          // +0x2096D
    mQueueOfTrophyCarUnLocks.Construct();                     // +0x208C8 (`stwx 0`, the count word)
    mfTimeTillAllEventTypeCompleteHudMessage = KF_HUD_MESSAGE_HOLD_CLEARED;   // +0x2097C
    miMaxCarCount            = 0;                     // +0x2095C (DWARF miMaxCarCount -- see below)
    mbCheckAllWinTypesPending    = false;                 // +0x20975
    ClearRankUpCache();                                  // +0x20970 and +0x20960
    mbCheckAllWinTypesArmed      = false;                 // +0x20976
    mbNeedToShowAllRivalsBeatenMessage   = false;         // +0x20980
    mbShowShutDownAllIfNeeded            = false;         // +0x20981
    mfTimeTillShowAllRivalsBeatenMessage = KF_HUD_MESSAGE_HOLD_CLEARED;       // +0x20984
    meModeToCheckForAllWinTypes  = RaceEventData::E_MODE_INVALID;                    // +0x20978, `li r8, -1` as a WORD
    mbAutosaveRequested          = false;                 // +0x2096F

    // 0x8237A8C8 / 0x8237A8F4 -- the two CPU monitors, then the console's own >= 0 asserts.
    miPreWorldUpdate  = CgsDev::PerfMonCpu::AddMonitor(
        KPC_PRE_WORLD_MONITOR_NAME,
        CgsDev::E_PMP_5,
        /*lbMinimum*/ false, KF_PROGRESSION_CPU_BUDGET_MS, /*lbLibPerfTagged*/ true);
    miPostWorldUpdate = CgsDev::PerfMonCpu::AddMonitor(
        KPC_POST_WORLD_MONITOR_NAME,
        CgsDev::E_PMP_5,
        /*lbMinimum*/ false, KF_PROGRESSION_CPU_BUDGET_MS, /*lbLibPerfTagged*/ true);

    if (miPreWorldUpdate < 0)
    {
        FireConsoleAssert("miPreWorldUpdate >= 0", 197);
    }
    if (miPostWorldUpdate < 0)
    {
        FireConsoleAssert("miPostWorldUpdate >= 0", 198);
    }

    mbSendAchievementAwardedEvent_DEBUG = false;          // +0x20800
    miAchievementToAward_DEBUG = 0;                       // +0x20804

    // NAME CORRECTION, RECORDED NOT APPLIED. The member stored above as `miMaxCarCount` is
    // spelled `int32_t miMaxCarCount` by the DWARF (BrnProgressionManager.h:859), sitting exactly
    // where +0x2095C is (right after the three road-rules tallies), with the getter
    // GetMaxCarCount() at DWARF :429. This tree's header already FLAGS the committed name as
    // "inferred from those two increments only" (AddCar's two bumps) -- and ApplyVehicleList
    // below SEEDS it from the vehicle list's selectable/sponsor pair, which is a maximum, not a
    // sponsor tally. The rename is NOT made in this wave because AddCar
    // (BrnProgressionManager_Unlocks.cpp) belongs to another lane running concurrently.
    // FOLLOW-UP: rename miMaxCarCount -> miMaxCarCount.

}

// --------------------------------------------------------------------------------------------
// ProgressionManager::LoadAIData  (X360 0x8239A0D0)
//
// The AI-lanes streaming machine Prepare gates on, and the ONLY writer of mpAISectionData
// (X360 `a1 + 133380`). Structurally the twin of LoadProgressionData (BrnProgressionManager.cpp)
// and of StreetManager::LoadAIData @0x8234FA70 -- with one difference that matters: this one
// LOADS the bundle first. StreetManager's copy only GETs.
//
//   0 NOT_STARTED       : queue.Clear(); LoadAILanes(&queue, 0, pool 5); stage = 1; FALL THROUGH
//   1 LOAD_REQUESTED    : reply not in yet -> the DEFAULT arm, i.e. return FALSE. Otherwise
//                         stage = 2 and fall through.
//   2 ACQUIRE_NOT_START : queue.Clear(); GetAILanes(&queue, 1, pool 5); stage = 3; FALL THROUGH
//                         (the console does not return here -- the queue it just cleared is
//                         empty, so the poll below answers false on this tick).
//   3 ACQUIRE_REQUESTED : reply not in yet -> return FALSE. Otherwise take the FIRST event,
//                         assert it is non-null (:2859) and that its event id is 1 (:2863), bind
//                         mpAISectionData from its ResourceHandle, stage = 4, return TRUE.
//   4 LOAD_COMPLETE     : return TRUE.
//   default             : return FALSE.  UNLIKE LoadProgressionData, whose default arm asserts
//                         and reports DONE, this one silently reports NOT DONE (`li r3, 0` at
//                         the shared def_8239A110 label). Reproduced as written.
//
// THE BIND. `CgsResource::BaseResourcePtr::CreateFromHandle(a1 + 133380, r31 + 0x20)` -- payload
// +0x20 is GameDataAssetEvent::mHandle (the member WorldGraphicsStreamer::OnLoadComplete
// @0x827BE5C8 reads at event+0x20). Read BY MEMBER: the host ResourceHandle is 16 bytes where the
// console's is 8, so the literal +0x20 would be wrong on this side of the port. Assignment
// through ResourcePtr::operator=(handle) IS CreateFromHandle -- the committed LoadProgressionData
// does the identical thing for mpProgressionData.
// --------------------------------------------------------------------------------------------
bool ProgressionManager::LoadAIData(GsmIO::OutputBuffer* lpOutput,
                                    CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue)
{
    switch (meAILoadStage)
    {
    case E_AI_DATA_LOAD_NOT_STARTED:
        lpReceiverQueue->Clear();
        lpOutput->GetResourceRequestInterface()->LoadAILanes(
            lpReceiverQueue, KI_AI_LANES_LOAD_EVENT_ID, KI_AI_LANES_POOL_ID);
        meAILoadStage = E_AI_DATA_LOAD_REQUESTED;
        // fall through -- the X360 drops straight into the poll on the same tick.

    case E_AI_DATA_LOAD_REQUESTED:
        if (lpReceiverQueue->GetCount() == 0)
        {
            return false;                       // `beq def_8239A110` -> `li r3, 0`
        }
        meAILoadStage = E_AI_DATA_ACQUIRE_NOT_STARTED;
        // fall through.

    case E_AI_DATA_ACQUIRE_NOT_STARTED:
        lpReceiverQueue->Clear();
        lpOutput->GetResourceRequestInterface()->GetAILanes(
            lpReceiverQueue, KI_AI_LANES_GET_EVENT_ID, KI_AI_LANES_POOL_ID);
        meAILoadStage = E_AI_DATA_ACQUIRE_REQUESTED;
        // fall through.

    case E_AI_DATA_ACQUIRE_REQUESTED:
    {
        if (lpReceiverQueue->GetCount() == 0)
        {
            return false;
        }

        const CgsModule::Event* lpEvent = 0;
        s32                     liSize  = 0;
        lpReceiverQueue->GetFirstEvent(&lpEvent, &liSize);

        if (lpEvent == 0)
        {
            FireConsoleAssert("lpEvent != NULL", 2859);
            return false;                       // the console falls into the same not-done arm
        }

        // reinterpret_cast, not static_cast: CgsModule::Event and the GameData reply record are
        // unrelated roots and the receiver queue hands out the module one (the idiom
        // LoadProgressionData and StreetManager::LoadAIData both use).
        const BrnResource::GameDataIO::GameDataAssetEvent* lpAIDataResponse =
            reinterpret_cast<const BrnResource::GameDataIO::GameDataAssetEvent*>(lpEvent);

        if (lpAIDataResponse->miEventId != KI_AI_LANES_GET_EVENT_ID)
        {
            FireConsoleAssert("lpAIDataResponse->GetEventId() == 1", 2863);
        }

        mpAISectionData = reinterpret_cast<const BrnResource::GameDataIO::GetAIDataResponse*>(lpAIDataResponse)->GetAIDataHandle();   // CreateFromHandle(&mpAISectionData, ...)
        meAILoadStage   = E_AI_DATA_LOAD_COMPLETE;
        return true;
    }

    case E_AI_DATA_LOAD_COMPLETE:
        return true;

    default:
        return false;
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::Prepare  (X360 0x8239DC38)
//
// Nine instructions of body:
//   0x8239DC4C  bl LoadAIData        -- r4/r5 are NEVER written, so the caller's output buffer
//                                       and receiver queue are forwarded verbatim. That is what
//                                       makes this a 2-parameter method and not the 0-parameter
//                                       one the Hex-Rays prototype shows.
//   0x8239DC50  clrlwi r11, r3, 24   -- the answer is a BOOL (byte), not an int.
//   0x8239DC5C  li r3, 0 ; blr       -- not done: return false and be re-entered next pass.
//   0x8239DC78  bl Profile::Construct(this + 0x170) ; li r3, 1
//
// THE SECOND Profile::Construct IS THE CONSOLE'S. Construct @0x8237A74C already called it once;
// this is the second call, at a later boot position, and it is deliberate. Landing it is what
// pays the DELETE-WHEN on Prepare2's mbProfileConstructed seam (BrnProgressionManager.cpp).
// --------------------------------------------------------------------------------------------
bool ProgressionManager::Prepare(GsmIO::OutputBuffer* lpOutput,
                                 CgsModule::EventReceiverQueue<3072, 16>* lpReceiverQueue)
{
    if (!LoadAIData(lpOutput, lpReceiverQueue))
    {
        return false;
    }

    mProfile.Construct();

    return true;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::ApplyVehicleList  (X360 0x82359A20)
//
//   0x82359A38  the `lpVehicleList == null\n` assert (:1553 == 0x611), streamed on the console
//               through gpcMessageBuffer; the text is static so it is passed straight through.
//   0x82359AC4  stwx r28, r27, 0x20948          -- mpVehicleList = lpVehicleList
//   0x82359AC8  lwz r11, 0x3408(list)           -- VehicleList::miSelectableVehicleCount
//   0x82359ACC  lwz r9,  0x340C(list)           -- VehicleList::miSponsorVehicleCount
//   0x82359AD4  stwx (r11 - r9), r27, 0x2095C   -- miMaxCarCount = selectable - sponsor
//
// The console does NOT return early on the null -- the assert fires and the stores happen anyway
// (an assert is not a guard) -- so the null test is reproduced without a return.
// --------------------------------------------------------------------------------------------
void ProgressionManager::ApplyVehicleList(const BrnResource::VehicleList* lpVehicleList)
{
    if (lpVehicleList == 0)
    {
        FireConsoleAssert("lpVehicleList == null\n", 1553);
    }

    mpVehicleList = lpVehicleList;                                  // +0x20948

    // `miMaxCarCount` is this tree's committed spelling of the DWARF's miMaxCarCount -- see
    // the NAME CORRECTION note in Construct above.
    miMaxCarCount = lpVehicleList->GetSelectableVehicleCount()  // +0x3408
                      - lpVehicleList->GetSponsorVehicleCount();    // +0x340C
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::ComputeLandmarkAISectionIndices  (X360 0x82370008)
//
// Prepare2's first call, and THE producer of maLandmarkAISectionIndices -- the table
// FindLandmarkAISectionIndex @0x82359AE0 scans, and whose banner in BrnProgressionManager.cpp
// recorded "THE PRODUCER IS NOT MOUNTED, SO THE TABLE IS EMPTY TODAY".
//
//   0x82370020  operator new[](0x40000) ; LinearMalloc::Construct/Create over it
//   0x82370044  pointMap = mpAISectionData->BuildAISectionPointMap(&arena)
//   per landmark i in [0, mpTriggerData->GetNumLandmarks()):
//     "liLandmarkIndex < miLandmarkCount"  (BrnTriggerData.h:390) -- the inlined GetLandmark guard
//     entry.mId = landmark[+0x24]                       (TriggerRegion::mId, as a WORD)
//     v20 = { landmark[+0x00], +0x04, +0x08, 0.0f }     (the BoxRegion position, w forced to 0)
//     entry.muAISectionIndex = mpAISectionData->FindNearestAISection(v20, pointMap)
//     "lpEntry->mId != BrnWorld::KI_INVALID_SECTION_INDEX" (:3227) -- yes, the console really
//       tests the LANDMARK ID against the section sentinel, not the section index. Reproduced.
//   operator delete[](arena)
//
// The landmark count comes from the TRIGGER DATA, exactly as the reader takes it, so producer and
// consumer walk the same range. mpTriggerData is installed by Prepare2 immediately before this
// call and the console does not null-test it; this body does not either.
// --------------------------------------------------------------------------------------------
void ProgressionManager::ComputeLandmarkAISectionIndices()
{
    u8* const lpArena = new u8[KN_AI_POINT_MAP_ARENA_BYTES];
    CgsMemory::LinearMalloc lArena;
    lArena.Construct();
    lArena.Create(lpArena, KN_AI_POINT_MAP_ARENA_BYTES);

    const BrnAI::AISectionsData* const lpAISectionsData = mpAISectionData.operator->();
    BrnAI::AISectionPointMap* const    lpAISectionPointMap =
        lpAISectionsData->BuildAISectionPointMap(&lArena);

    const BrnTrigger::TriggerData* const lpTriggerData =
        mpTriggerData;
    const s32 liLandmarkCount = lpTriggerData->GetNumLandmarks();

    for (s32 liLandmarkIndex = 0; liLandmarkIndex < liLandmarkCount; ++liLandmarkIndex)
    {
        const BrnTrigger::Landmark* const lpLandmark = lpTriggerData->GetLandmark(liLandmarkIndex);
        LandmarkAISectionIndexPair* const lpEntry    = &maLandmarkAISectionIndices[liLandmarkIndex];

        lpEntry->mId = static_cast<u32>(lpLandmark->GetId());

        const Vector3 lPosition = lpLandmark->GetBoxRegion()->GetPosition();   // w == 0.0f
        lpEntry->muAISectionIndex =
            lpAISectionsData->FindNearestAISection(lPosition, lpAISectionPointMap);

        if (lpEntry->mId == BrnWorld::KI_INVALID_SECTION_INDEX)
        {
            FireConsoleAssert("lpEntry->mId != BrnWorld::KI_INVALID_SECTION_INDEX", 3227);
        }
    }

    delete[] lpArena;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::HACK_SetupRaceWithLandMarks  (X360 0x82359B78)
//
//   per id in the caller's array (stride 8, i.e. a 64-bit CgsID each -- `ld r4, 0(r30)`):
//     landmark = mpTriggerData->FindLandmark(id)        -- `lwz r3, 0(mpTriggerData)`
//     if (landmark)                                     -- `cmplwi ; beq`
//       race->AddLandmark(landmark[+0x28] , FindLandmarkAISectionIndex(landmark[+0x24]))
//                         ^ GetRegionIndex()             ^ GetId()
// The two landmark reads are `lhz r31, 0x28(r11)` (the region index, a halfword) and
// `lwz r10, 0x24(r11)` + `extsw r4, r10` (the id, widened to the 64-bit CgsID parameter).
// --------------------------------------------------------------------------------------------
void ProgressionManager::HACK_SetupRaceWithLandMarks(Race* lpRace, const CgsID* lpaLandmarkIds,
                                                     u32 luCount) const
{
    const BrnTrigger::TriggerData* const lpTriggerData =
        mpTriggerData;

    for (u32 luIndex = 0; luIndex < luCount; ++luIndex)
    {
        const BrnTrigger::Landmark* const lpLandmark =
            lpTriggerData->FindLandmark(lpaLandmarkIds[luIndex]);
        if (lpLandmark == 0)
        {
            continue;
        }

        const BrnGameState::LandmarkIndex lLandmarkIndex(lpLandmark->GetRegionIndex());
        const u16 luAISectionIndex = FindLandmarkAISectionIndex(lpLandmark->GetId());
        lpRace->AddLandmark(lLandmarkIndex, luAISectionIndex);
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::HACK_SetupRaces  (X360 0x82366968)
//
// Five hard-coded races into the caller's scratch buffer, 120 bytes apart (`addi r29, r31, 0x78`
// / 0xF0 / 0x168 / 0x1E0), each built the same way: Race::Construct, SetId (a 64-bit `std` at
// +0x20), SetName (a 32-byte strncpy with `stb 0, 0x1F` after it) and two landmarks. Returns 5.
// See the KA_HACK_* tables above for the ids, the names and the measured fact that none of the
// ten landmark ids exists in the shipped trigger data.
// --------------------------------------------------------------------------------------------
s32 ProgressionManager::HACK_SetupRaces(Race* lpaRaceScratch)
{
    for (s32 liRace = 0; liRace < KI_HACK_RACE_COUNT; ++liRace)
    {
        Race* const lpRace = &lpaRaceScratch[liRace];

        lpRace->Construct();
        lpRace->SetId(KA_HACK_RACE_IDS[liRace]);
        lpRace->SetName(KAPC_HACK_RACE_NAMES[liRace]);
        HACK_SetupRaceWithLandMarks(lpRace, KAA_HACK_RACE_LANDMARKS[liRace],
                                    KU_HACK_LANDMARKS_PER_RACE);
    }
    return KI_HACK_RACE_COUNT;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::ProcessLoadedPresetRaces  (X360 0x8236FDF8)
//
//   0x8236FE04  the stack array at +0x50 is handed to HACK_SetupRaces
//   0x8236FE20  muNumPresetRaces = its answer  (`stw r3, 0(r28)`, r28 == this + 0x208D8)
//   0x8236FE3C  per race: Array<Race,64>::Append(&maPresetRaces, &scratch[i])   (stride 0x78)
// The loop bound is re-read from muNumPresetRaces every iteration, exactly as written.
//
// DecFIGS 0x287EE4 initializes 17 Race entries. ARTIST 0x8236FE00 reserves the
// corresponding stack frame, and 0x8236FE48 advances by the 120-byte Race stride.
// --------------------------------------------------------------------------------------------
void ProgressionManager::ProcessLoadedPresetRaces()
{
    const s32 KI_PRESET_RACE_SCRATCH_CAPACITY = 17;
    CGS_STATIC_ASSERT((KI_HACK_RACE_COUNT <= KI_PRESET_RACE_SCRATCH_CAPACITY),
        "preset race scratch capacity");
    Race laRaceScratch[KI_PRESET_RACE_SCRATCH_CAPACITY];

    muNumPresetRaces = static_cast<u32>(HACK_SetupRaces(laRaceScratch));

    for (u32 luRace = 0; luRace < muNumPresetRaces; ++luRace)
    {
        maPresetRaces.Append(laRaceScratch[luRace]);
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::SetupRoamingSections  (X360 0x8236FE60)
//
// Prepare2's fifth and last call, and THE producer of maRoamingSections.
//
//   0x8236FE78  operator new[](0x40000) ; LinearMalloc over it ; BuildAISectionPointMap
//   per district d in [0, 18):
//     maRoamingSections[d].Clear()                              -- `stw r17(0), 0x10(r28)`
//     per roaming location r in [0, mpTriggerData->GetRoamingLocationCount()):
//       "liRoamingLocationIndex < miRoamingLocationCount" (BrnTriggerData.h:569) -- inlined guard
//       if (location->GetDistrict() == d)                       -- `lbz r11, 0x10(r31)`
//         "Array used before Construct/Clear was called"        (CgsArray.h:336) -- count != -1
//         "Too many roaming locations in district" (:2985)      -- count < 8
//         maRoamingSections[d].Append(FindNearestAISection(location->GetPosition(), pointMap))
//   operator delete[](arena)
//
// The two container asserts are CgsArray's own and fire from inside Array<u16,8>::Clear/Append on
// the host, which is where the console's inlined copies live too -- so they are NOT re-spelled
// here. The "Too many roaming locations in district" one IS this file's (the console bakes
// BrnProgressionManager.cpp:2985 for it) and is checked before the Append, as the console does.
// --------------------------------------------------------------------------------------------
void ProgressionManager::SetupRoamingSections()
{
    u8* const lpArena = new u8[KN_AI_POINT_MAP_ARENA_BYTES];
    CgsMemory::LinearMalloc lArena;
    lArena.Construct();
    lArena.Create(lpArena, KN_AI_POINT_MAP_ARENA_BYTES);

    const BrnAI::AISectionsData* const lpAISectionsData = mpAISectionData.operator->();
    BrnAI::AISectionPointMap* const    lpAISectionPointMap =
        lpAISectionsData->BuildAISectionPointMap(&lArena);

    const BrnTrigger::TriggerData* const lpTriggerData =
        mpTriggerData;

    for (s32 liDistrict = 0; liDistrict < KI_DISTRICT_COUNT; ++liDistrict)
    {
        RoamingSections& lrDistrictSections = maRoamingSections[liDistrict];
        lrDistrictSections.Clear();

        const s32 liRoamingLocationCount = lpTriggerData->GetRoamingLocationCount();
        for (s32 liLocation = 0; liLocation < liRoamingLocationCount; ++liLocation)
        {
            const BrnTrigger::RoamingLocation* const lpLocation =
                lpTriggerData->GetRoamingLocation(liLocation);

            if (static_cast<s32>(lpLocation->GetDistrict()) != liDistrict)
            {
                continue;
            }

            if (lrDistrictSections.GetCount() >= KI_MAX_ROAMING_SECTIONS_PER_DISTRICT)
            {
                FireConsoleAssert("Too many roaming locations in district", 2985);
                continue;   // the console's Append would overrun; its assert halts the build
            }

            const u16 luAISectionIndex = lpAISectionsData->FindNearestAISection(
                lpLocation->GetPosition(), lpAISectionPointMap);
            lrDistrictSections.Append(luAISectionIndex);
        }
    }

    delete[] lpArena;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::GetRacesAtLandmark  (X360 0x8236F830)
//
//   luRaceCount = 0
//   per race i in [0, muNumPresetRaces):            -- the bound is +0x208D8, re-read each pass
//     race = maPresetRaces.GetItem(i)
//     "Need at least two landmarks for a race" (BrnRace.h:144 == 0x90) -- the inlined
//        Race::GetStartLandmarkIndex guard, `lbz r11, 0x70(race) ; cmplwi 2 ; bge`
//     if (race.maLandmarkIndices[0] == lLandmarkIndex)   -- `lhz r11, 0x30(race) ; extsh ; cmpw`
//       "luRaceCount < luMaxRaces" (:1173 == 0x495)
//       memcpy(out, race, 0x78) ; ++luRaceCount ; out += 0x78
//   return luRaceCount
//
// The fourth DWARF parameter (`GetRacesAtLandmark(Race *, uint32_t, LandmarkIndex, bool) const`,
// DWARF :297) is r7, never read by this optimized ARTIST body. Preserve its declaration
// shape and the callers' argument; it adds no filtering. memcpy retains every original120 byte.
// --------------------------------------------------------------------------------------------
u32 ProgressionManager::GetRacesAtLandmark(Race* lpaRacesOut, u32 luMaxRaces,
                                           BrnGameState::LandmarkIndex lLandmarkIndex,
                                           bool /*lbIncludeCustomRaces*/) const
{
    u32 luRaceCount = 0;

    for (u32 luRace = 0; luRace < muNumPresetRaces; ++luRace)
    {
        const Race& lrRace = maPresetRaces[luRace];

        // Race::GetStartLandmarkIndex carries the console's own ":144" assert and reads
        // maLandmarkIndices[0] -- exactly the `lbz +0x70 ; lhz +0x30` pair.
        if (static_cast<s32>(lrRace.GetStartLandmarkIndex()) != static_cast<s32>(lLandmarkIndex))
        {
            continue;
        }

        if (luRaceCount >= luMaxRaces)
        {
            FireConsoleAssert("luRaceCount < luMaxRaces", 1173);
        }

        std::memcpy(&lpaRacesOut[luRaceCount], &lrRace, sizeof(Race));
        ++luRaceCount;
    }

    return luRaceCount;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::LandmarkHasAvailableRaces  (X360 0x8236F928)
// The same walk, returning 1 at the first hit and 0 if the list runs out (an empty preset-race
// list answers 0 without touching the array -- `ble loc_8236F9BC`).
// --------------------------------------------------------------------------------------------
bool ProgressionManager::LandmarkHasAvailableRaces(BrnGameState::LandmarkIndex lLandmarkIndex) const
{
    for (u32 luRace = 0; luRace < muNumPresetRaces; ++luRace)
    {
        const Race& lrRace = maPresetRaces[luRace];
        if (static_cast<s32>(lrRace.GetStartLandmarkIndex()) == static_cast<s32>(lLandmarkIndex))
        {
            return true;
        }
    }
    return false;
}



}

// FOLDED FROM BrnProgressionManager_Medals.cpp
// ============================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_Medals.cpp
// ============================================================================================
// [progression wave 2026-09-06, lane `medals`] THE LICENCE / RANK-UP CHAIN.
//
//   ProgressionManager::UpdatePlayerMedals        @0x8239FE50  (DWARF BrnProgressionManager.cpp:630)
//   ProgressionManager::CalculateRankFromMedalTotal @0x8237AB38 (DWARF :735, `int8_t (uint32_t) const`)
//   ProgressionManager::ClearMedalsOnRankUp       @0x823705D8  (DWARF :4247)
//   ProgressionManager::UnlockDefaultPlayerCars   @0x8237BF98  (DWARF :4323)
//   ProgressionManager::FixGameModeRanks          @0x82395CD8  (DWARF :579)
//   ProgressionManager::DEBUG_ClearMedals         @0x82366BF8  (DWARF :3175)
//
// WHY THE LICENCE NEVER ADVANCED. UpdatePlayerMedals is the ONLY producer of game action 200
// (E_ACTION_UPDATE_PLAYER_MEDALS -> GuiEventMedalUpdate 307) and the ONLY console caller of
// UnlockToProgressionRank in the whole XEX. It had no body, so all three of its seats were
// parked: OnEventFinishUpdateProfile's P6, PreWorldUpdate's `mbPlayerMedalsUpdateRequired` arm,
// and -- transitively -- ProgressionManager::Construct @0x8237A5F8, whose two boot stores
// (`stbx -2 -> +0x2096C` @0x8237A7A0 and `stbx 1 -> +0x20973` @0x8237A7B0) are what ARM that arm
// in the first place. Construct is not reconstructed either, so on PC nothing set the flag and
// the arm could not have run even with a body behind it. The result on screen: the medal HUD
// never refreshed, the rank-up car unlocks never fired, and Profile::mi8CurrentProgressionRank
// kept the -2 "rank not set" seed Profile::Construct wrote.
//
// A partfile, not BrnProgressionManager.cpp, for the reason the _PreWorldUpdate / _EventFinish /
// _Completion / _Unlocks / _Rivals partfiles exist: the console homes all six functions in
// BrnProgressionManager.cpp, which is why every assert below is fired through
// CgsDev::Assert::{Begin,Fire,End}Assert with that file's baked path and the binary's own line
// number rather than through CGS_ASSERT's __FILE__/__LINE__.
//
// Every member is reached BY NAME. The console offsets are quoted only to say which member each
// load lands on.
// ============================================================================================
#include "BrnProgressionManager.h"
#include "BrnProfile.h"

#include <stdlib.h>                                                     // getenv (the [medals] witness gate)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CgsDev::Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                 // CgsCore::SPrintf (the Q3 telemetry parameter)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // gpDebugPrint + gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // GameActionQueue::AddEvent
#include "GameSource/GameState/BrnGameActions.h"                        // UpdatePlayerMedalsAction (200) / E_ACTION_SEND_TELEMETRY (228)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // GameStateModuleIO::GameActionQueue
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnLicenseUpgrade (Q3)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h"             // TelemetryData / E_TELEMETRY_EVENT_EARNED_LICENCE
#include "SharedClasses/Progression/BrnProgressionData.h"               // ProgressionData
#include "SharedClasses/Progression/BrnProgressionRankData.h"           // ProgressionRankData (the 112-byte per-rank record)
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // EventJunction / RaceEventData

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

namespace
{
    // The verbatim X360-baked source path this TU's asserts reference (identical spelling to
    // BrnProgressionManager.cpp's own KAC_PROGMGR_FILE -- the console homes these six functions
    // there, so their asserts must keep that file and the binary's line numbers).



    // IMAGE-CITED CONSTANT (big-endian dump of scratch/postfx_step9_final/envfix/work/image.bin,
    // file offset == VA - 0x82000000):
    //   flt_82029B70 = 0x7F7FFFFF == 3.40282347e+38f == FLT_MAX -- the seed
    //   Profile::GetCurrentCarTypeWithMinDistance's inlined min-finder starts from
    //   (UnlockToProgressionRank's shared rank tail, `lfs f13, flt_82029B70@l(r11)` @0x8239E0EC).
    // It is cited here rather than used here: the min-finder now lives in BrnProfile.h beside the
    // array it scans, and the constant is quoted on that body.

    // The licence rank at which the credits become selectable (`cmpwi cr6, r10, 5` @0x8239E170,
    // then `stbx 1 -> profile+0x1CD14` == Profile::mbHasUnlockedCredits @+118036).
    const s32 KI_RANK_THAT_UNLOCKS_CREDITS = 5;

    // The trophy type UpdatePlayerMedals evaluates on every call (`li r4, 0x16` @0x8239FF80).
    // 22 == BrnProgression::E_UNLOCKTYPE_* (the trophy table is data; OnTrophyUnlock @0x82389740
    // owns the "has this trophy's condition been met" walk, which is why the call is
    // unconditional here).
    const s32 KI_TROPHY_TYPE_MEDAL_TOTAL = 22;

    // The per-mode legs FixGameModeRanks fixes up, in the console's own order
    // (@0x82395CEC mode 0, @0x82395D6C mode 3, then the two INLINED legs for 7 and 8 --
    // see the banner on the body).
    const GsmIO::EGameModeType KAE_DIFFICULTY_SCALED_MODES[4] =
    {
        GsmIO::E_MODE_OFFLINE_RACE,   // 0 -> maiRankWinsPerOfflineGameMode[0], rank record +0x60
        GsmIO::E_MODE_ROAD_RAGE,      // 3 -> [3],                              rank record +0x62
        GsmIO::E_MODE_STUNT_ATTACK,   // 7 -> [7],                              rank record +0x61
        GsmIO::E_MODE_MARKED_MAN      // 8 -> [8],                              rank record +0x63
    };

    // One-shot park reporter (campaign house rule: a park must be visible, not silent).












    bool gbSaidNoAchievementManager   = false;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::CalculateRankFromMedalTotal  (X360 0x8237AB38)
// DWARF BrnProgressionManager.h:604 -- `int8_t CalculateRankFromMedalTotal(uint32_t) const;`
// DWARF locals (BrnProgressionManager.cpp:736/:737): `int8_t li8TestRank; int8_t li8RankCount;`
//
// "Which licence rank does this many medals buy?" Walk the per-rank medal thresholds up from the
// player's current rank until one is out of reach.
//   0x8237AB58..0x8237AB68  li8RankCount = (s8)(u8)muProgressionRankCount   (`clrlwi 24` + later `extsb`)
//   0x8237AB7C..0x8237ABAC  li8TestRank  = (mi8ProgressionRank == (s32)rankCount)
//                                          ? (s8)(rankCount - 1)            -- already off the end
//                                          : (s8)GetProgressionRank()       -- the clamped cache
//   0x8237ABD0..0x8237AC28  while (li8TestRank < li8RankCount):
//                             record = GetProgressionRankData(li8TestRank)  -- owns the :330 assert
//                             if (luMedalTotal < record->mu16MedalThresholdToNextRank) break;  (cmplw, UNSIGNED)
//                             li8TestRank = (s8)(li8TestRank + 1);          -- narrowed EVERY pass
// ⚠️ The `==` against the RAW rank count (not count-1) is the console's own, and it is NOT the
// same test as PlayerHasFinishedLastRank's -- GetProgressionRankNormalisedForCurrentRank carries
// the identical pair with the same note. Do not unify them.
// [!] The "luIndex < muProgressionRankCount" assert (BrnProgressionData.h:330) that the console
// fires at 0x8237ABE8 belongs to the INLINED GetProgressionRankData; routing through the accessor
// reproduces it exactly once, so it is not restated here.
// --------------------------------------------------------------------------------------------
s8 ProgressionManager::CalculateRankFromMedalTotal(u32 luMedalTotal) const
{
    const s8 li8RankCount =
        static_cast<s8>(static_cast<u8>(GetProgressionData()->GetProgressionRankCount()));

    s8 li8TestRank;
    if (static_cast<s32>(mi8ProgressionRank) ==
        static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
    {
        li8TestRank = static_cast<s8>(GetProgressionData()->GetProgressionRankCount() - 1u);
    }
    else
    {
        li8TestRank = static_cast<s8>(GetProgressionRank());
    }

    while (static_cast<s32>(li8TestRank) < static_cast<s32>(li8RankCount))
    {
        const ProgressionRankData* lpcRankData =
            GetProgressionData()->GetProgressionRankData(static_cast<u32>(static_cast<s32>(li8TestRank)));

        // `cmplw cr6, r24, r11` -- an UNSIGNED compare of the medal total against the u16 threshold.
        if (luMedalTotal < static_cast<u32>(lpcRankData->GetMedalThresholdToNextRank()))
        {
            break;
        }
        li8TestRank = static_cast<s8>(li8TestRank + 1);
    }

    return li8TestRank;
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::ClearMedalsOnRankUp  (X360 0x823705D8)
// DWARF BrnProgressionManager.h:619 -- `void ClearMedalsOnRankUp();`
// DWARF locals/hints (:4247): `uint32_t luIndex; uint32_t luEventCount;` +
//   `const ProfileEvent* lpProfileEvent; const RaceEventData* lpEventData;` and the callee list
//   {Profile::GetEventCount, Profile::GetEvent, ProgressionData::FindOfflineEvent,
//    ProfileEvent::GetID, Profile::ResetEventMedals} -- which is exactly the asm, with
//   FindOfflineEvent and ResetEventMedals inlined.
//
// A rank-up wipes the MEDAL bits of every profile event so the new rank is earned again, but it
// keeps the "you have already won this" memory:
//   0x823705F0..0x82370610  `if ((s8)mi8ProgressionRank == (s32)muProgressionRankCount) return;`
//                           -- the same off-the-end test CalculateRankFromMedalTotal uses.
//   0x82370614..0x82370620  luEventCount = mProfile.GetEventCount()  (`lwz r21, 0x3E8(this)` ==
//                           Profile+0x278 == miEventCount; read ONCE, the loop bound)
//   0x82370654..0x82370678  Profile::GetEvent(luIndex)  -- owns the BrnProfile.h:3004 index assert
//   0x82370680..0x8237069C  the ":4369 lpProfileEvent" assert
//   0x823706A8..0x823706F0  FindOfflineEvent(lpProfileEvent->GetID()) inlined: scan the junction
//                           table (count +0x1C, base +0x18, stride 16) for a matching muID and
//                           take that junction's OFFLINE event slot (+0x04)
//   0x823706F0..0x82370704  the ":4372 lpEventData" assert
//   0x82370708..0x82370740  `flags &= (lpEventData->GetSpecialEventCarId() != 0) ? 0x11 : 0x21`
//                           (`ld r11, 0x10(event)`; 0x11 == DISCOVERED|WON_SPECIAL_EVENT_BEFORE,
//                            0x21 == DISCOVERED|WON_EVENT_BEFORE) == Profile::ResetEventMedals
// So FINISHED / RANK_WIN / NON_RANK_WIN are cleared and DISCOVERED plus the matching
// "won before" bit survive.
// --------------------------------------------------------------------------------------------
void ProgressionManager::ClearMedalsOnRankUp()
{
    if (static_cast<s32>(mi8ProgressionRank) ==
        static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
    {
        return;
    }

    const u32 luEventCount = mProfile.GetEventCount();

    for (u32 luIndex = 0; luIndex < luEventCount; ++luIndex)
    {
        const ProfileEvent* lpcProfileEvent = mProfile.GetEvent(luIndex);
        if (lpcProfileEvent == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpProfileEvent", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 4369));
            CgsDev::Assert::EndAssert();
            continue;   // [PC GUARD] the console reads through the null; GetEvent cannot return
                        // one on the host, so the guard is unreachable rather than divergent.
        }

        const RaceEventData* lpcEventData =
            GetProgressionData()->FindOfflineEvent(lpcProfileEvent->GetID());
        if (lpcEventData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpEventData", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 4372));
            CgsDev::Assert::EndAssert();
        }

        // [PC GUARD] the console loads `0x10(lpEventData)` UNGUARDED right after that assert, so
        // a null there is an access violation on the console too. It cannot happen: the profile's
        // event records are minted by UnlockToProgressionRank from the junctions that HAVE an
        // offline event, so every id in the list resolves. A null is treated as "not a special
        // event" (mask 0x21) rather than dereferenced. DELETE-WHEN nothing else can add a
        // ProfileEvent whose id has no junction.
        const bool lbIsSpecialEvent = (lpcEventData != 0) && (lpcEventData->GetSpecialEventCarId() != 0);

        mProfile.ResetEventMedals(luIndex, lbIsSpecialEvent);
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::UnlockDefaultPlayerCars  (X360 0x8237BF98)
// DWARF BrnProgressionManager.h:679 -- `void UnlockDefaultPlayerCars();`
// DWARF local (:4323): `uint32_t luFreeCarIndex;`; hint callees {ProgressionData::GetPlayerCarId,
// ResourcePtr<ProgressionData>::operator->} -- Profile::FindCar is the inlined scan below.
//
// The rank-0 starting garage: every car id in PROGRESSION.DAT's player-car table becomes owned,
// and its unlock sequence is marked already-seen so the game does not play a reveal for a car the
// player has had since the first frame.
//   0x8237BFB8..0x8237BFC4  loop bound = muPlayerCarIdCount (+0x0C), re-read every pass (0x8237C0C0)
//   0x8237BFE4..0x8237C020  GetPlayerCarId(luFreeCarIndex) -- owns the :310 index assert; `ldx`,
//                           an 8-byte CgsID at the +0x08 table
//   0x8237C018..0x8237C064  Profile::FindCar inlined (count Profile+0x26C, base +0x280, stride
//                           0x18, `cmpld` on the record's 8-byte id)
//   0x8237C068..0x8237C070  on a MISS: AddCar(lCarId, 0 == E_UNLOCK_TYPE_UNLOCK)
//   0x8237C074..0x8237C0B0  a SECOND GetPlayerCarId (hence the second :310 assert in the asm) ->
//                           Profile::SetCarUnlockAlreadyShown
// ⚠️ The pseudocode's `if ( 24 * v10 + a1 + 368 == -640 )` is Hex-Rays rendering the console's
// `addi r11, r11, 0x280 / cmplwi r11, 0 / bne` -- a null test on the FOUND record's address, which
// can never be null. It is the else-arm of the found/not-found fork, nothing more.
// --------------------------------------------------------------------------------------------
void ProgressionManager::UnlockDefaultPlayerCars()
{
    for (u32 luFreeCarIndex = 0;
         luFreeCarIndex < GetProgressionData()->GetPlayerCarIdCount();
         ++luFreeCarIndex)
    {
        const CgsID lCarId = GetProgressionData()->GetPlayerCarId(luFreeCarIndex);

        if (mProfile.FindCar(lCarId) == 0)
        {
            AddCar(lCarId, CarData::E_UNLOCK_TYPE_UNLOCK);              // `li r5, 0`
        }

        mProfile.SetCarUnlockAlreadyShown(GetProgressionData()->GetPlayerCarId(luFreeCarIndex));
    }
}

// --------------------------------------------------------------------------------------------
// ProgressionManager::FixGameModeRanks  (X360 0x82395CD8)
// DWARF BrnProgressionManager.h:579 -- `void FixGameModeRanks();`
//
// The per-mode difficulty ladders are not allowed to lag more than TWO ranks behind the player's
// licence: for each of the four modes that have their own ladder, if the mode's derived rank is
// below (licence rank - 2), the mode's rank-win tally is raised to the threshold of that rank.
// It is what stops a player who ranked up on races from meeting rank-1 road rage at licence 5.
//
//   per leg: `lbz r11, 0x1E0(this)` == Profile+0x70 == mProfile.GetCurrentProgressionRank(),
//            `extsb` then `addi r11, r11, -2`                          -- the target rank
//            GetProgressionRankForGameMode(mode)                        -- the mode's own rank
//            `bge` -> skip when the mode is already at or above the target
//            else store GetRankThresholdForEvent(target, mode) into
//            Profile::maiRankWinsPerOfflineGameMode[mode] (this+0x36C + 4*mode).
//   leg 1 @0x82395CEC mode 0 -> +0x36C, rank record +0x60
//   leg 2 @0x82395D6C mode 3 -> +0x378, rank record +0x62
//   leg 3 @0x82395DD0 mode 7 -> +0x388, rank record +0x61
//   leg 4 @0x82395EE4 mode 8 -> +0x38C, rank record +0x63
//
// ⭐ INLINING REVERSAL. Legs 1 and 2 `bl GetProgressionRankForGameMode`; legs 3 and 4 carry that
// function OPEN-CODED (the same `liProgressionRank` climb from 1, the same `extsb`-narrowed rank
// count, and the same "liProgressionRank >= 0 && liProgressionRank < liNumRanks" assert with the
// same baked line 3802 -- which is that function's own assert, not this one's). The compiler
// folded the constant-mode switch away for the last two; the source called the same function four
// times, so it is written that way here. The threshold reads are likewise
// GetRankThresholdForEvent's own switch, folded to one byte load per leg.
//
// ⚠️ The target rank can be NEGATIVE (the profile's -2 seed gives -4). The console does not guard
// it and does not need to: the mode rank is never negative, so `modeRank < target` is false and
// the arm is skipped. Reproduced as shipped -- no invented clamp.
// --------------------------------------------------------------------------------------------
void ProgressionManager::FixGameModeRanks()
{
    s8 lai8ModeRanks[4] = { 0, 0, 0, 0 };

    for (s32 liLeg = 0; liLeg < 4; ++liLeg)
    {
        const GsmIO::EGameModeType leGameModeType = KAE_DIFFICULTY_SCALED_MODES[liLeg];

        const s8  li8ModeRank   = GetProgressionRankForGameMode(leGameModeType);
        const s32 liTargetRank  = static_cast<s32>(mProfile.GetCurrentProgressionRank()) - 2;

        lai8ModeRanks[liLeg] = li8ModeRank;

        if (static_cast<s32>(li8ModeRank) < liTargetRank)
        {
            mProfile.SetNumRankWinsForGameMode(GetRankThresholdForEvent(liTargetRank, leGameModeType),
                                               leGameModeType);
        }
    }

}

// --------------------------------------------------------------------------------------------
// ProgressionManager::UpdatePlayerMedals  (X360 0x8239FE50)
// DWARF BrnProgressionManager.h:658 -- `void UpdatePlayerMedals(GameActionQueue*);`
// DWARF locals (:631-:638), used verbatim below: `SendPlayerMedalAction lSendPlayerMedalAction;
//   uint32_t luTotalWinsForCurrentRank, luRankWin, luNonRankWin, luSpecialEventsWonBefore;
//   int32_t liWinsForNextRank; int8_t li8TestRank, liTopRankIndex;`
// ⓘ NAME NOTE: the DWARF calls the 8-byte record `SendPlayerMedalAction`; this tree already
// models it as GameStateModuleIO::UpdatePlayerMedalsAction (BrnGameActions.h:1930, named after
// the producer). Same record, same four halfwords, same size -- not renamed here because that
// header belongs to another lane this wave.
//
// Count the profile's medals, turn the total into a licence rank, unlock up to it if it grew, and
// publish the four numbers the medal HUD shows.
//
//   0x8239FE64..0x8239FE8C  liTopRankIndex = (s8)(muProgressionRankCount - 1)
//   0x8239FE90              Profile::GetTotalWinCount(luRankWin, luNonRankWin, luSpecialEventsWonBefore)
//                           (r4/r5/r6 == the three out-params, in that order)
//   0x8239FEA4..0x8239FEC4  luTotalWinsForCurrentRank = luRankWin, PLUS luSpecialEventsWonBefore
//                           when the player is already on the TOP rank -- the special events only
//                           count toward the total once there is no next rank to earn.
//   0x8239FEC8..0x8239FEEC  li8TestRank = (mi8ProgressionRank == (s32)rankCount)
//                                         ? mi8ProgressionRank
//                                         : CalculateRankFromMedalTotal(luTotalWinsForCurrentRank)
//   0x8239FEF0..0x8239FF54  if (li8TestRank > mi8ProgressionRank):
//                             if (mi8ProgressionRank <= liTopRankIndex):
//                                 UnlockToProgressionRank(li8TestRank, lpGameActionQueue)
//                                 re-run GetTotalWinCount + the top-rank sum (the unlock CLEARS
//                                 medals, so every number below is the post-unlock one)
//                             if (mi8ProgressionRank > liTopRankIndex): the debug complaint
//   0x8239FF80..0x8239FF88  OnTrophyUnlock(22) -- UNCONDITIONAL, on every path
//   0x8239FF90..0x8239FFC8  the ":723 mi8CurrentProgressionRank >= 0" assert (`cmplwi 0x80`)
//   0x8239FF94..0x823A0010  the record: four halfwords at +0/+2/+4/+6
//   0x8239FFCC..0x823A0008  +6 == -1 when the rank has run off the end, else
//                           max(0, GetTotalWinsForNextRank() - luTotalWinsForCurrentRank)
//   0x823A001C              AddEvent(queue, &record, 200, 8)
//   0x823A0024              CheckForSpecialCarUnlocks()
//   0x823A0028..0x823A0034  mbPlayerMedalsUpdateRequired = false
// --------------------------------------------------------------------------------------------
void ProgressionManager::UpdatePlayerMedals(GsmIO::GameActionQueue* lpGameActionQueue)
{
    const s8 li8TopRankIndex =
        static_cast<s8>(GetProgressionData()->GetProgressionRankCount() - 1u);

    u32 luRankWin                = 0;
    u32 luNonRankWin             = 0;
    u32 luSpecialEventsWonBefore = 0;
    mProfile.GetTotalWinCount(luRankWin, luNonRankWin, luSpecialEventsWonBefore);

    u32 luTotalWinsForCurrentRank =
        (static_cast<s32>(mi8ProgressionRank) == static_cast<s32>(li8TopRankIndex))
            ? (luRankWin + luSpecialEventsWonBefore)
            : luRankWin;

    // The default is the cached byte itself (`lbz r4, 0(r30)` before the branch): a player whose
    // rank has already run off the end of the table is not re-derived from medals.
    s8 li8TestRank = mi8ProgressionRank;
    if (static_cast<s32>(mi8ProgressionRank) !=
        static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
    {
        li8TestRank = CalculateRankFromMedalTotal(luTotalWinsForCurrentRank);
    }

    if (static_cast<s32>(li8TestRank) > static_cast<s32>(mi8ProgressionRank))
    {
        bool lbProgressionAlreadyComplete =
            (static_cast<s32>(mi8ProgressionRank) > static_cast<s32>(li8TopRankIndex));

        if (!lbProgressionAlreadyComplete)
        {
            UnlockToProgressionRank(li8TestRank, lpGameActionQueue);

            // The unlock ran ClearMedalsOnRankUp, so every tally below has to be re-counted.
            mProfile.GetTotalWinCount(luRankWin, luNonRankWin, luSpecialEventsWonBefore);
            luTotalWinsForCurrentRank =
                (static_cast<s32>(mi8ProgressionRank) == static_cast<s32>(li8TopRankIndex))
                    ? (luRankWin + luSpecialEventsWonBefore)
                    : luRankWin;

            lbProgressionAlreadyComplete =
                (static_cast<s32>(mi8ProgressionRank) > static_cast<s32>(li8TopRankIndex));
        }

        if (lbProgressionAlreadyComplete &&
            (CgsDev::Message::gxMessageFilterFlags & 1) != 0 &&
            CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "***********************************!\n"
                   "Junction event progression already complete!\n"
                   "***********************************!\n";
        }
    }

    OnTrophyUnlock(KI_TROPHY_TYPE_MEDAL_TOTAL);

    // `cmplwi cr6, r11, 0x80 / blt` -- an UNSIGNED compare of the raw byte, i.e. "the signed byte
    // is not negative". The console fires this AFTER the unlock above, which is exactly why it
    // does not trip on the -2 boot seed.
    if (static_cast<u8>(mi8ProgressionRank) >= 0x80u)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("mi8CurrentProgressionRank >= 0", CGS_ASSERT_LOCATION(KAC_PROGMGR_FILE, 723));
        CgsDev::Assert::EndAssert();
    }

    s16 li16WinsForNextRank;
    if (static_cast<s32>(mi8ProgressionRank) ==
        static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
    {
        li16WinsForNextRank = -1;                                       // `li r11, -1`
    }
    else
    {
        const s32 liWinsForNextRank =
            static_cast<s32>(GetTotalWinsForNextRank()) - static_cast<s32>(luTotalWinsForCurrentRank);
        li16WinsForNextRank = static_cast<s16>((liWinsForNextRank < 0) ? 0 : liWinsForNextRank);
    }

    GsmIO::UpdatePlayerMedalsAction lSendPlayerMedalAction;
    lSendPlayerMedalAction.mi16TotalWins      = static_cast<s16>(luRankWin);                // +0x00
    lSendPlayerMedalAction.mi16Field02        = static_cast<s16>(luNonRankWin);             // +0x02
    lSendPlayerMedalAction.mi16Field04        = static_cast<s16>(luSpecialEventsWonBefore); // +0x04
    lSendPlayerMedalAction.mi16WinsToNextRank = li16WinsForNextRank;                        // +0x06

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSendPlayerMedalAction),
                                GsmIO::E_ACTION_UPDATE_PLAYER_MEDALS,
                                static_cast<s32>(sizeof(lSendPlayerMedalAction)));   // li r5,0xC8 ; li r6,8

    CheckForSpecialCarUnlocks();

    mbPlayerMedalsUpdateRequired = false;                               // stbx 0 -> +0x20973

}

// --------------------------------------------------------------------------------------------
// ProgressionManager::DEBUG_ClearMedals  (X360 0x82366BF8)
// DWARF BrnProgressionManager.h:685 -- `void DEBUG_ClearMedals();`
// DWARF hints (:3175) name only the two request setters; the rest is inlined.
//
// The debug component's "start again from nothing" button (its four users are
// ProgressionDebugComponent::GetToOneWinBelowEliteLicenseBR / GetToOneWinBelowEliteLicense /
// GetToOneWinBelowBurnoutLicense / SkipToProgressionRank).
//   0x82366C08..0x82366C0C  Profile::DEBUG_ClearMedals()  (this+0x170)
//   0x82366C10..0x82366C3C  every rival record's state word (maRivals[i] +0x10, 0x38 stride,
//                           count Profile+0x274) back to 0 == RivalData::E_STATE_LOCKED
//   0x82366C60              mi8ProgressionRank = -2   -- the same "rank not set" seed
//                           Profile::Construct and ProgressionManager::Construct both write
//   0x82366C64/0x82366C68   RequestMedalUpdate() (+0x20973) / RequestUpdateRivals() (+0x20971)
// ⚠️ NO CALLER ON PC. ProgressionDebugComponent is not reconstructed, so this body exists to
// close the chain, not because anything reaches it. That is deliberate: it is four instructions
// of state and its absence is what would make the debug component's own reconstruction a partial.
// --------------------------------------------------------------------------------------------
void ProgressionManager::DEBUG_ClearMedals()
{
    mProfile.DEBUG_ClearMedals();

    for (s32 liRivalIndex = 0; liRivalIndex < mProfile.GetRivalCount(); ++liRivalIndex)
    {
        mProfile.GetRivalData(liRivalIndex)->meState = RivalData::E_STATE_LOCKED;
    }

    mi8ProgressionRank = -2;

    RequestMedalUpdate();
    RequestUpdateRivals();
}

// ============================================================================================
// ProgressionManager::UnlockToProgressionRankTail -- THE SHARED RANK TAIL (X360 0x8239E094..0x8239E1DC)
//
// [progression wave, lane `medals`] This is UnlockToProgressionRank's PARK Q4, landed. It lives
// here rather than in BrnProgressionManager.cpp because everything it needs
// (ClearMedalsOnRankUp) is in this partfile, and because the un-park at the call site is then a
// single line. It is NOT a console function -- the console falls into this code inline at the end
// of UnlockToProgressionRank -- so it is private and named for what it is.
//
//   0x8239E094..0x8239E0DC  write the CLAMPED rank back: the exact GetProgressionRank() ladder
//                           (raw byte >= 0x80 -> 0; else min(rank, rankCount - 1)), stored into
//                           mi8ProgressionRank.
//   0x8239E0E0..0x8239E12C  meLeastUsedCarType (+0x20968) = the index of the SMALLEST of the three
//                           Profile::mafCarTypes distances, seeded from FLT_MAX
//                           (flt_82029B70 == 0x7F7FFFFF) -- i.e. the car type the player has
//                           driven least. That is Profile::GetCurrentCarTypeWithMinDistance.
//   0x8239E130..0x8239E164  walk the cached rank up ONE AT A TIME to li8Rank, stopping early if it
//                           reaches the rank count. (Written as the console's loop, not as a
//                           min(): the loop re-reads the stored byte every pass.)
//   0x8239E168..0x8239E184  mirror it onto Profile::mi8CurrentProgressionRank, and at rank >= 5
//                           set Profile::mbHasUnlockedCredits (+0x1CD14).
//   0x8239E188..0x8239E19C  Profile::ResetCarTypeDistances()  (three `stw 0` over mafCarTypes)
//   0x8239E1A0              ClearMedalsOnRankUp()
//   0x8239E1A4..0x8239E1BC  Profile::ClearCurrentEventCompleteCounts() (18 words at Profile+0x108
//                           == maGameModeTypeAmountCompleted, `mtctr 0x12`)
//   0x8239E1C0..0x8239E1D0  mbHasJustRankedUp = true, but ONLY for a non-zero rank
//   0x8239E1D4..0x8239E1DC  a byte at this+0x207FF -- see the PARK below.
// ============================================================================================
void ProgressionManager::UnlockToProgressionRankTail(s8 li8Rank)
{
    // @0x8239E094..0x8239E0DC -- GetProgressionRank()'s ladder, open-coded by the console and
    // written back. Routed through the accessor: same value, same clamp, one owner.
    mi8ProgressionRank = static_cast<s8>(GetProgressionRank());

    // @0x8239E0E0..0x8239E12C
    meLeastUsedCarType = mProfile.GetCurrentCarTypeWithMinDistance();

    // @0x8239E130..0x8239E164
    while (static_cast<s32>(mi8ProgressionRank) < static_cast<s32>(li8Rank))
    {
        mi8ProgressionRank = static_cast<s8>(mi8ProgressionRank + 1);
        if (static_cast<s32>(mi8ProgressionRank) ==
            static_cast<s32>(GetProgressionData()->GetProgressionRankCount()))
        {
            break;
        }
    }

    // @0x8239E168..0x8239E184
    mProfile.SetCurrentProgressionRank(mi8ProgressionRank);
    if (static_cast<s32>(mi8ProgressionRank) >= KI_RANK_THAT_UNLOCKS_CREDITS)
    {
        mProfile.SetHasUnlockedCredits(true);
    }

    // @0x8239E188..0x8239E1BC
    mProfile.ResetCarTypeDistances();
    ClearMedalsOnRankUp();
    mProfile.ClearCurrentEventCompleteCounts();

    // @0x8239E1C0..0x8239E1D0 -- `cmpwi r21, 0 / beq` on the RANK ARGUMENT, so the rank-0
    // starting unlock is not a "you just ranked up".
    if (li8Rank != 0)
    {
        mbHasJustRankedUp = true;
    }

    // X360 0x8239E1DC writes one to mDebugComponent.mbProfileReadyForDisplay (+0x77).
    mDebugComponent.SetProfileReadyForDisplay();
}

// ============================================================================================
// ProgressionManager::UnlockToProgressionRankLicenceUpgrade -- PARK Q3, landed
// (X360 0x8239E034..0x8239E090, the RANK-N arm of UnlockToProgressionRank).
//
// [progression wave, lane `medals`] Q3's stated blocker was "AchievementManagerBase::
// OnLicenseUpgrade @0x8235ADC8 lives in a TU that is not mounted". THAT IS NO LONGER TRUE:
// BrnGameStateAchievementManagerBase.cpp is mounted (tools/build/build_game_exe.bat, next to the
// X360 leaf; the rem block above it still argues for the old state and is stale), OnLicenseUpgrade
// is bodied there, and BrnNetworkSharedIO_Telemetry.cpp -- which owns TelemetryData::Construct and
// AddParameter(const char*) -- is mounted too. So the arm is landed.
//
//   0x8239E034..0x8239E044  mpAchievementManager (+0x20938)->OnLicenseUpgrade(li8Rank)  -- the RAW
//                           byte argument (`mr r4, r31`), which the callee takes as a uint8_t.
//   0x8239E048..0x8239E04C  `cmplwi r30, 0 / beq` -- the whole telemetry post is skipped when the
//                           GameActionQueue argument is null. THIS build's other caller
//                           (UpdatePlayerMedals) always passes a real one.
//   0x8239E050..0x8239E070  TelemetryData::Construct(22 == E_TELEMETRY_EVENT_EARNED_LICENCE)
//                           inlined (`stw 0x16, var_80` + `stb 0, var_7C`), and
//                           CgsCore::SPrintf(buf, 16, "%i", (s32)li8Rank)
//   0x8239E07C              TelemetryData::AddParameter(buf)
//   0x8239E080..0x8239E090  AddEvent(queue, &record, 228, 20)
// ============================================================================================
void ProgressionManager::UnlockToProgressionRankLicenceUpgrade(
        s8 li8Rank, GsmIO::GameActionQueue* lpGameActionQueue)
{
    if (mpAchievementManager != 0)
    {
        mpAchievementManager->OnLicenseUpgrade(static_cast<u8>(li8Rank));
    }
    else
    {
    }

    if (lpGameActionQueue == 0)
    {
        return;                                                         // `cmplwi r30, 0 / beq`
    }

    char lacRankText[16];                                               // li r4, 0x10
    CgsCore::SPrintf(lacRankText, sizeof(lacRankText), "%i", static_cast<s32>(li8Rank));

    BrnNetwork::BrnNetworkModuleIO::TelemetryData lTelemetry;
    lTelemetry.Construct(BrnNetwork::E_TELEMETRY_EVENT_EARNED_LICENCE);  // li r11, 0x16
    lTelemetry.AddParameter(lacRankText);

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lTelemetry),
                                GsmIO::E_ACTION_SEND_TELEMETRY,
                                static_cast<s32>(sizeof(lTelemetry)));   // li r5,0xE4 ; li r6,0x14
}

}   // namespace BrnProgression

// FOLDED FROM BrnProgressionManager_PreWorldUpdate.cpp
// ============================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_PreWorldUpdate.cpp
// ============================================================================================
// [issue #10 "progression: miles driven not recorded when driving", 2026-09-06]
// The per-frame tick of BrnProgression::ProgressionManager and the distance integrator it drives.
//
//   PreWorldUpdate    @0x823A4F68  (DWARF BrnProgressionManager.cpp:304; asserts :343 / :382)
//   AddDistanceDriven @0x823668F0  (DWARF BrnProgressionManager.cpp:3523)
//
// WHY THE ODOMETER READ 0.0 km. AddDistanceDriven is the ONLY writer in the image of every
// "distance driven" number the game shows -- Profile::mfDistanceDrivenOnline / Offline, the
// per-car-type tally, and the current LiveryData::mfDistanceDriven that GameStateModule::
// CopyScoringDataToOutput @0x8236CDC0 publishes as ScoringOutputInterface::
// mfDistanceDrivenInCurrentCar -> GuiEventCurrentStatus (492) -> GuiCache::mfDistanceDriven ->
// OdometerComponent::Update. Its sole caller is PreWorldUpdate, which GameStateModule::
// PreWorldUpdate @0x823A5328 runs every in-game frame (the `(lUpdateSet & 8)` leg, right after
// TriggerQueryManager::PreWorldUpdate). Neither function existed in the tree and the pump's own
// banner recorded the call as "NOT STAGED ... nothing in the junction/start chain reads what it
// writes". The whole publish chain downstream was already live; the source was silent.
//
// A partfile, not BrnProgressionManager.cpp, so this lane stays file-disjoint from the other
// ProgressionManager partfiles (the _Completion / _EventFinish / _Unlocks / _Rivals precedent).
// Every member is reached BY NAME; the console offsets are quoted to show which member each
// load lands on. The embedded Profile is this+0x170 on the console (`addi r30, r31, 0x170`).
//
// WHAT IS PARKED (named, not faked -- each parks ONCE per run in the log):
//   UnlockRivals / CheckForAllModeTypeCompletion / UpdateRivals / SendTrophyUnlockUpdate
//   @0x823892B8. Their GATES (the request bytes and the two 2 s holds) run exactly as the
//   console runs them, so the state machine around them is real and the day a body lands it is a
//   one-line un-park.
//   ✅ [progression wave: medals, 2026-09-06] UpdatePlayerMedals @0x8239FE50 IS NO LONGER PARKED
//   -- it is bodied in BrnProgressionManager_Medals.cpp and called from the medals arm below,
//   which is what makes that arm self-limiting (the callee clears the request byte).
// ============================================================================================
#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include <stdlib.h>                                                       // getenv (the [odometer] witness gate)

#include "GameShared/GameClasses/Core/CgsAssert.h"                        // CgsDev::Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"          // GameActionQueue (VariableEventQueue<13312,16>) ::AddEvent
#include "GameSource/GameState/Progression/BrnProfile.h"                  // Profile::Add* / SetCar* (inline)
#include "GameSource/GameState/Progression/BrnProgressionLiveryData.h"    // LiveryData::mfDistanceDriven
#include "GameSource/GameState/BrnGameStateModuleIO.h"                    // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameActions.h"                          // E_ACTION_REQUEST_AUTOSAVE (55) / E_ACTION_ALL_RIVALS_SHUTDOWN (210)
#include "GameSource/GameState/ModeManager/BrnModeManager.h"              // ModeManager::IsOnlineGameMode
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface + RaceCarState

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

namespace
{
    // The console's own baked assert location (BeginAssert/FireAssert/EndAssert is called
    // directly rather than through CGS_ASSERT so the file/line stay the binary's, not this
    // TU's -- the _GameStats / _EventFinish partfiles' treatment).



    // IMAGE-CITED CONSTANTS (big-endian dump of image.bin, offset == VA - 0x82000000):
    //   flt_82F31928 = 0x3EE4E9C0 == 0.447039992f -- MPH -> m/s. The same rodata word the
    //                  TakedownManager / BystanderCam / SimpleVehiclePhysics reconstructions
    //                  cite; RaceCarState::mfSpeedMPH * this * simStep == metres this step.
    //   flt_82001D9C = 0x40000000 == 2.0f  -- the two HUD-message holds (seconds of sim time).
    //   flt_82001CC0 = 0x00000000 == 0.0f  -- the hold reset.
    const f32 KF_MPH_TO_METRES_PER_SECOND     = 0.447039992f;   // flt_82F31928
    const f32 KF_HUD_MESSAGE_HOLD_SECONDS     = 2.0f;           // flt_82001D9C

    // One-shot park reporters (campaign house rule: a park must be visible, not silent). The
    // same shape as _EventFinish.cpp's; that one lives in its own anonymous namespace.












    bool gbSaidAllModeTypes = false;
    bool gbSaidUpdateRivals = false;
    bool gbSaidTrophyUpdate = false;

}

// --------------------------------------------------------------------------------------------
// AddDistanceDriven (X360 0x823668F0).
//   fabs f0, f1                                  -- the metres, magnitude only
//   if (lbOnline)  Profile::AddDistanceDrivenOnline(f0)     `lfs/fadds/stfs 0x1D4(this)`
//   else           Profile::AddDistanceDrivenOffline(f0)    `0x68(profile)` + the per-car-type
//                                                             tally at profile+0x1CCB0+4*type
//   if (mpCurrentLiveryData)  mpCurrentLiveryData->mfDistanceDriven += f0
//                                                `lwz r10, 0x208D4(this) ; lfs/fadds/stfs 0x10`
// The DWARF hint list for :3523 is exactly {fpu::Abs, AddDistanceDrivenOffline,
// AddDistanceDrivenOnline}; the livery tail is the inlined `(*this+133332)->+0x10` the header
// already documents on GetCurrentLiveryData.
// --------------------------------------------------------------------------------------------
void ProgressionManager::AddDistanceDriven(f32 lfDistance, bool lbOnline)
{
    const f32 lfMetres = (lfDistance < 0.0f) ? -lfDistance : lfDistance;   // fabs

    if (lbOnline)
    {
        mProfile.AddDistanceDrivenOnline(lfMetres);
    }
    else
    {
        mProfile.AddDistanceDrivenOffline(lfMetres);
    }

    LiveryData* lpLiveryData = GetCurrentLiveryData();          // this+133332 (0x208D4)
    if (lpLiveryData != 0)
    {
        lpLiveryData->mfDistanceDriven += lfMetres;             // +0x10
    }
}

// --------------------------------------------------------------------------------------------
// PreWorldUpdate (X360 0x823A4F68). Register map from the prologue: r31 = this, f31 = f1 =
// lfSimTimeStep, f30 = f2 = lfGameTimeStep, r30 = r6 = lpOutputBuffer (then re-used as the
// Profile base `addi r30, r31, 0x170`), r29 = r7 = lpActiveRaceCarInterface, r28 = r8 =
// lUpdateSet, r27 = r9 = lbIsInJunkyard, r23 = &miPreWorldUpdate, r24 = the action queue.
// --------------------------------------------------------------------------------------------
void ProgressionManager::PreWorldUpdate(
    f32 lfSimTimeStep, f32 lfGameTimeStep,
    GsmIO::OutputBuffer* lpOutputBuffer,
    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    BrnUpdateSet lUpdateSet, bool lbIsInJunkyard)
{
    CgsDev::PerfMonCpu::StartMonitor(miPreWorldUpdate);                       // lwz r3, 0(r23)

    GsmIO::GameActionQueue* lpGameActionQueue = lpOutputBuffer->GetGameActionQueue();   // r24

    // `lfs/fadds/stfs 0x1CD28(profile)` on f30 -- the real-time counter ticks on the GAME step,
    // paused or not, in the junkyard or not.
    mProfile.AddRealTimePlayed(lfGameTimeStep);

    // ---- the player-car arm ---------------------------------------------------------------
    // IsPlayerCarActive() is inlined on the console (the :967 "mePlayerActiveRaceCarIndex <
    // E_ACTIVE_RACE_CAR_INDEX_COUNT" assert then `idx != -1 ? +0x2860 : 0`); the accessor body
    // carries that assert. `clrlwi r11, r28, 31` == lUpdateSet bit 0, the network catch-up step
    // Physics/AI test the same way: no time or distance is booked on a catch-up step.
    if (lpActiveRaceCarInterface->IsPlayerCarActive() && (lUpdateSet & 1) == 0)
    {
        const BrnPhysics::Vehicle::RaceCarState* lpRaceCarState =
            lpActiveRaceCarInterface->GetRaceCarState(
                lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex());
        if (lpRaceCarState == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpRaceCarState", CGS_ASSERT_LOCATION(KAC_PROGRESSION_MANAGER_CPP, 343));
            CgsDev::Assert::EndAssert();
        }

        // IsPlayerEngineOn() inlined: `idx != -1 && +0x285C == 2` (RUNNING). In-car time ticks on
        // the GAME step, `lfs/fadds/stfs 0x6C(profile)`.
        if (lpActiveRaceCarInterface->IsPlayerEngineOn())
        {
            mProfile.AddInCarTimePlayed(lfGameTimeStep);
        }

        // ModeManager::IsOnlineGameMode() inlined: `lwzx r11, this, 0x2093C ; lwz r11, 0xD98(r11)`
        // (mpModeManager->mpCurrentGameMode) then `lbz 0xAC` (GameMode::mbIsOnline) when non-null.
        // [PC GUARD] the console dereferences mpModeManager unconditionally; Prepare2 installs
        // it here, and a null one on a PC boot path is a silent "offline" rather than an AV.
        const bool lbOnlineGameMode = (mpModeManager != 0) && mpModeManager->IsOnlineGameMode();

        if (lbIsInJunkyard)
        {
            // The spawn pose: `lvx128 v0, rcs, 0x220 ; stvx128 v0, this, 0x1A0` (mTransform.wAxis
            // -> Profile::mCarPosition) and `0x210 -> 0x1B0` (mTransform.zAxis -> mCarDirection),
            // only when no online mode is running.
            if (!lbOnlineGameMode)
            {
                mProfile.SetCarPosition(lpRaceCarState->mTransform.wAxis);
                mProfile.SetCarDirection(lpRaceCarState->mTransform.zAxis);
            }
        }
        else
        {
            // `lfs f13, 0x3CC(rcs)` (RaceCarState::mfSpeedMPH) * flt_82F31928 * f31 (the SIM
            // step) -> metres this step; the online flag rides r5.
            AddDistanceDriven(lpRaceCarState->mfSpeedMPH * KF_MPH_TO_METRES_PER_SECOND * lfSimTimeStep,
                              lbOnlineGameMode);
        }
    }

    // ---- the deferred legs (each gated exactly as the console gates it) --------------------

    // `lbzx +0x20973` -- mbPlayerMedalsUpdateRequired -> UpdatePlayerMedals + UnlockRivals.
    if (mbPlayerMedalsUpdateRequired)
    {
        // ✅ [L4 boot order 2026-09-28] THE OnLoadProfile STAND-IN THAT STOOD HERE IS RETIRED.
        // It restored the rank cache from the loaded profile on this arm's first pass, because nothing
        // called ProgressionManager::OnLoadProfile @0x823893A8 on PC. It is bodied now
        // (BrnProgressionManager.cpp) and GameStateModule::OnProfileLoaded @0x82397310 calls it -- at the
        // boot through ProcessGameEvents case 8 (the MemoryCard exit), before the first in-game frame
        // reaches this arm -- and it is also what raises mbPlayerMedalsUpdateRequired for this arm.

        // ✅ [progression wave: medals, 2026-09-06] THE MEDALS ARM IS LIVE. UpdatePlayerMedals
        // @0x823A5154 is bodied in BrnProgressionManager_Medals.cpp, and it is the callee that
        // CLEARS mbPlayerMedalsUpdateRequired on its way out (`stbx 0 -> +0x20973` @0x823A0034),
        // so this arm is self-limiting exactly as the console's is -- the byte no longer stays set.
        // This is the seat that turns ProgressionManager::Construct's boot seed of that flag into
        // the rank-0 licence unlock; see the Prepare2 stand-in in BrnProgressionManager.cpp.
        UpdatePlayerMedals(lpGameActionQueue);

        // [conductor 2026-09-06] console: `UnlockRivals(lpGameActionQueue);` @0x823A5160 -- the
        // second seat of the CgsID-returning UnlockRivals (BrnProgressionManager_Rivals.cpp);
        // this seat discards the id (the `bl` result is never read). The medals lane held this
        // call back because it runs at BOOT (Construct seeds the request byte) and writes the
        // saved profile's rival table; that IS the console's own boot behaviour -- the first
        // rival whose medal threshold the profile already clears is unlocked here -- so it is
        // called as the console calls it. Its writes are what the rivals case witnesses.
        (void)UnlockRivals(lpGameActionQueue);
    }

    // `lbz +0x20975` then `lbz +0x20976` -- the all-win-types check runs a 2 s hold on the SIM step.
    if (mbCheckAllWinTypesPending)
    {
        if (mbCheckAllWinTypesArmed)
        {
            mfTimeTillAllEventTypeCompleteHudMessage += lfSimTimeStep;          // +0x2097C
            if (mfTimeTillAllEventTypeCompleteHudMessage > KF_HUD_MESSAGE_HOLD_SECONDS)
            {
                if (meModeToCheckForAllWinTypes == RaceEventData::E_MODE_INVALID)                          // E_MODE_INVALID
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "meModeToCheckForAllWinTypes != RaceEventData::E_MODE_INVALID",
                        CGS_ASSERT_LOCATION(KAC_PROGRESSION_MANAGER_CPP, 382));
                    CgsDev::Assert::EndAssert();
                }
                // ⭐ UN-PARKED [progression wave 2026-09-06, lane completion]: console
                // `CheckForAllModeTypeCompletion(lpGameActionQueue,
                // meModeToCheckForAllWinTypes);` @0x823A51EC, bodied in
                // BrnProgressionManager_Completion.cpp @0x82389698. It counts the authored
                // events of that mode against the ones the player has uniquely won and, on a
                // catch-up, posts AllEventTypeWonAction (game action 206, size 4) and latches
                // the profile's "seen this message" bit. The four clears below are the console's
                // and stay unconditional -- the callee's own profile bit is what makes the
                // message once-ever, not this arm.
                CheckForAllModeTypeCompletion(lpGameActionQueue, meModeToCheckForAllWinTypes);
                mbCheckAllWinTypesPending                = false;               // stb 0 -> +0x20975
                mbCheckAllWinTypesArmed                  = false;               // stb 0 -> +0x20976
                mfTimeTillAllEventTypeCompleteHudMessage = 0.0f;                // flt_82001CC0
                meModeToCheckForAllWinTypes              = RaceEventData::E_MODE_INVALID;                  // li r11, -1
            }
        }
    }

    // `lbz +0x20981` then `lbz +0x20980` -- the all-rivals-beaten message, 2 s hold on the SIM step,
    // then AllRivalsShutDownAction (id 210, size 1). The console posts var_70 WITHOUT storing to
    // it first on this arm (only the autosave arm below writes the byte); a zero is the one
    // value that cannot be "the wrong stack garbage".
    if (mbShowShutDownAllIfNeeded)
    {
        if (mbNeedToShowAllRivalsBeatenMessage)
        {
            mfTimeTillShowAllRivalsBeatenMessage += lfSimTimeStep;              // +0x20984
            if (mfTimeTillShowAllRivalsBeatenMessage > KF_HUD_MESSAGE_HOLD_SECONDS)
            {
                u8 lacAllRivalsShutDown[1] = { 0 };
                lpGameActionQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lacAllRivalsShutDown),
                    GsmIO::E_ACTION_ALL_RIVALS_SHUTDOWN, 1);                    // li r5,0xD2 ; li r6,1
                mbShowShutDownAllIfNeeded          = false;                     // stb 0 -> +0x20981
                mbNeedToShowAllRivalsBeatenMessage = false;                     // stb 0 -> +0x20980
            }
        }
    }

    // `lbz +0x2096F` -- the FORCED autosave: RequestAutoSaveAction (id 55, size 1) with the
    // payload byte set to 1 (`li r11,1 ; stb r11, var_70`), then the latch clears.
    if (mbAutosaveRequested)
    {
        u8 lacRequestAutosave[1] = { 1 };
        lpGameActionQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(lacRequestAutosave),
            GsmIO::E_ACTION_REQUEST_AUTOSAVE, 1);                               // li r5,0x37 ; li r6,1
        mbAutosaveRequested = false;                                            // stb 0 -> +0x2096F
    }

    // `lbzx +0x20971` -- mbUpdateRivals -> UpdateRivals(lpGameActionQueue).
    // ⭐ UN-PARKED [progression wave 2026-09-06, lane rivals]: console `UpdateRivals(
    // lpGameActionQueue);` @0x823A52BC, bodied in BrnProgressionManager_Rivals.cpp @0x82396298.
    // It is the callee that clears the byte -- it does so only when its resume cursor
    // (miLastUpdatedRival) reaches the end of the authored rival table, so this arm runs on as
    // many consecutive frames as there are rivals and then goes quiet, exactly as the console's.
    if (mbUpdateRivalsRequested)
    {
        UpdateRivals(lpGameActionQueue);
    }

    // `lwz r11, 0xC0(this+0x20808)` -- mQueueOfTrophyCarUnLocks' count word; the CgsArray.h:336
    // "Array used before Construct/Clear was called" assert is GetLength's own (the tree's
    // Array::GetLength fires the identical string on the -1 sentinel), then `> 0`.
    // ⭐ UN-PARKED [progression wave 2026-09-06, lane completion]: console
    // `SendTrophyUnlockUpdate(lpGameActionQueue);` @0x823892B8, bodied in
    // BrnProgressionManager_Completion.cpp. It posts the TAIL element as game action 204
    // (E_ACTION_TROPHY_UNLOCK, 16 B) and Erases it, so one queued trophy car leaves per frame --
    // and this `> 0` test is the console's own empty guard for a callee that has none.
    // Before this, UnlockCarFromTrophy (_Unlocks.cpp) could only ever FILL the 12-slot queue.
    if (mQueueOfTrophyCarUnLocks.GetLength() > 0)
    {
        SendTrophyUnlockUpdate(lpGameActionQueue);
    }

    CgsDev::PerfMonCpu::StopMonitor(miPreWorldUpdate);                        // lwz r3, 0(r23)
}

}   // namespace BrnProgression

// FOLDED FROM BrnProgressionManager_Rivals.cpp
// ============================================================================================
// b5-decomp/src/GameSource/GameState/Progression/BrnProgressionManager_Rivals.cpp
// ============================================================================================
// [takedown P1 wave 2026-09-03] The rival-shutdown leg of BrnProgression::ProgressionManager --
// what TakedownManager::ProcessTakedownEvent's free-burn arm calls when the player takes a rival
// down (the OnPursuitWon park in BrnTakedownManager_Detect.cpp), plus the two
// AchievementManagerBase accessors that were still declare-only in BrnProgressionManager.h.
//
//   OnPursuitWon              @0x82389F40  (asserts BrnProgressionManager.cpp:2400 / :2410 / :2413)
//   DefeatRivalAndUnlockCar   @0x8237B1D0  (assert  BrnProgressionManager.cpp:2465)
//   CheckForAllRivalsUnlocked @0x8236FD90
//   GetProfileTotalTakedowns  no X360 symbol -- `lwz r11, 0x198(profile)` inlined in
//                             AchievementManagerBase::OnTakedown @0x8235AB70
//   GetCarChallengeWinCount   no X360 symbol -- `lwz r11, 0x358(pm)` inlined in
//                             AchievementManagerBase::OnEventWin @0x82372B68 / @0x82372BB4
//
// A partfile, not BrnProgressionManager.cpp, so this lane stays file-disjoint from the other
// ProgressionManager partfiles (the _Completion / _EventFinish / _Unlocks precedent).
// Every member is reached BY NAME; the console offsets are quoted to show which member each
// load lands on. The embedded Profile is this+0x170 on the console (`addi r27, r25, 0x170`).
// ============================================================================================
#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/Progression/BrnProfile.h"                 // Profile::FindRival / AddRival / FindCar / tallies
#include "GameSource/GameState/Progression/BrnProgressionCarData.h"      // BrnProgression::CarData (AddCar's record)
#include "GameSource/GameState/Progression/BrnProgressionRivalData.h"    // BrnProgression::RivalData (+ EState)
#include "SharedClasses/Progression/BrnProgressionData.h"                // ProgressionData::FindRivalIndexFromId / GetRival
#include "SharedClasses/Progression/BrnRival.h"                          // BrnProgression::Rival
#include "GameSource/GameState/BrnGameActions.h"                         // RivalStateChangeAction / E_ACTION_*
#include "GameSource/GameState/BrnGameStateSharedIO.h"                   // GameStateModuleIO::E_MODE_BURNING_ROUTE / GameActionQueue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"         // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Core/CgsAssert.h"                       // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"               // gpDebugPrint / gxMessageFilterFlags
#include <cstring>                                                        // std::memset (the record's 3 pad bytes)
// ---- [progression wave 2026-09-06, lane rivals] ------------------------------------------
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"   // TrainingManager::RequestTraining (OnTakedownTo's inlined gauntlet)
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // AchievementManagerBase::OnOnlineRaceComplete
#include "SharedClasses/DataLists/VehicleListEntry.h"                  // BrnResource::E_CARTYPE_AGGRESSION (Profile::meCurrentCarType == 1)
#include "SharedClasses/Progression/BrnTrainingTypes.h"                // BrnProgression::ETrainingType
#include <stdlib.h>                                                    // getenv (the [rivals] witness gate)
// ---- [progression wave 2026-09-06, lane rivals2] ------------------------------------------
#include "SharedClasses/AI/AISectionsResourceType.h"                   // BrnAI::AISectionsData::GetAISection / AISection::GetMiddle
#include "SharedClasses/Progression/BrnRaceEventData.h"                // BrnProgression::EventRacerPersonality (the record's +0x58 field)
#include "BrnCommonTypes.h"                                            // Vector3 (the record's two 16-byte lanes)
#include <cstddef>                                                     // offsetof (the record's pinned offsets)

namespace BrnProgression
{

// --------------------------------------------------------------------------------------------
// GetProfileTotalTakedowns -- AchievementManagerBase::OnTakedown @0x8235AB20..0x8235AB74:
//   lwz  r11, 4(r31)        ; mpProgressionManager
//   addi r29, r11, 0x170    ; the embedded Profile (asserted "lpProfile", :188)
//   lwz  r11, 0x198(r29)    ; Profile+408 == miTotalTakedownCount ; cmpwi 0x1F4 (500)
// Profile+408 is the word Profile::AddTakedown @0x82354C00 increments (BrnProfile.h "+408").
// --------------------------------------------------------------------------------------------
s32 ProgressionManager::GetProfileTotalTakedowns() const
{
    return mProfile.GetTotalTakedownCount();
}

// --------------------------------------------------------------------------------------------
// GetCarChallengeWinCount -- AchievementManagerBase::OnEventWin @0x82372B64..0x82372B6C (and the
// same pair again @0x82372BB0..0x82372BB8):
//   lwz r11, 4(r31)         ; mpProgressionManager
//   lwz r11, 0x358(r11)     ; cmpwi 0x19 (25) / 0x23 (35)
// 0x358 == 856 == 0x170 (the embedded Profile) + 488, and Profile+488 is
// maiWinsPerOfflineGameMode[5] (array base +468, DWARF BrnProfile.h:1199; BrnGui's
// SetBurningRouteDescription reads the very same word as `lwz r11, 0x1E8(profile)`, see
// Profile::GetNumWinsForGameMode). 5 == GameStateModuleIO::E_MODE_BURNING_ROUTE -- the "car
// challenge" the two XS-car achievements count is the Burning Route win tally.
// --------------------------------------------------------------------------------------------
s32 ProgressionManager::GetCarChallengeWinCount() const
{
    return mProfile.GetNumWinsForGameMode(BrnGameState::GameStateModuleIO::E_MODE_BURNING_ROUTE);
}

// --------------------------------------------------------------------------------------------
// CheckForAllRivalsUnlocked @0x8236FD90 (caller: DefeatRivalAndUnlockCar, both arms).
//   0x8236FDA8  bl GetNumberOfBeatenRivals   -> r30
//   0x8236FDB4  bl GetTrueNumberOfRivals     -> r3
//   0x8236FDB8  cmpw r30, r3 ; blt -> done
//   0x8236FDD8  stbx 0, this+0x20981          ; mbShowShutDownAllIfNeeded          = false
//   0x8236FDDC  stbx 1, this+0x20980          ; mbNeedToShowAllRivalsBeatenMessage = true
// (Member identity: the two are consecutive bools, DWARF BrnProgressionManager.h:885/:886 in
// that order; ProgressionManager::PreWorldUpdate @0x823A4F68 is their reader.)
// --------------------------------------------------------------------------------------------
void ProgressionManager::CheckForAllRivalsUnlocked()
{
    const s32 liBeatenRivals = GetNumberOfBeatenRivals();
    const s32 liTrueRivals   = GetTrueNumberOfRivals();
    if (liBeatenRivals >= liTrueRivals)
    {
        mbShowShutDownAllIfNeeded          = false;
        mbNeedToShowAllRivalsBeatenMessage = true;
    }
}

// --------------------------------------------------------------------------------------------
// DefeatRivalAndUnlockCar @0x8237B1D0 (r3 this, r4 liRivalIndex, r5 leUnlockSequenceType).
//   0x8237B1F4..0x8237B230  GetRival(liRivalIndex): the :468 "liIndex < miRivalCount" assert is the
//                           accessor's own (BrnProgressionData.h), then `add r29 = rivals + 0x38*idx`
//   0x8237B238              ld r26, 8(r29)               ; Rival::mCarId
//   0x8237B23C..0x8237B248  the SAME bound re-tested silently: out of range == nothing below runs
//   0x8237B24C..0x8237B284  r27 = this+0x170 (Profile); the inlined Profile::FindRival(Rival::mId)
//                           walk over maRivals (+0x6280, 0x38 stride, count +0x274) -> r28
//   0x8237B288  cmpwi r24, 1  (E_UNLOCK_SEQUENCE_TYPE_NONE): only then, and only on a miss,
//   0x8237B2A0    bl Profile::AddRival(mId, mCarId)
//   0x8237B2B0  assert "lpSavedRival" (:2465)
//   0x8237B2D0..0x8237B310  if (gxMessageFilterFlags & 1) log "Moving rival to beaten state: " + id
//   0x8237B314/0x8237B320   li 3 ; stw 0x10(r28)         ; RivalData::meState = E_STATE_BEATEN
//   0x8237B324  bl Profile::FindCar(mCarId) ; bne -> done
//   0x8237B330  li r5, 3 ; bl AddCar(mCarId, 3)          ; CarData::E_UNLOCK_TYPE_SHUTDOWN_RIVAL
//   0x8237B33C  cmpwi r24, 0 (DEFAULT):  stfs flt_82029BB8 (0.85f, image bytes 3F59999A) -> CarData+0xC
//               else if r24 == 1 (NONE): stb 1 -> CarData+0xA
//   0x8237B354 / 0x8237B384  bl CheckForAllRivalsUnlocked   (both arms)
// --------------------------------------------------------------------------------------------
void ProgressionManager::DefeatRivalAndUnlockCar(s32 liRivalIndex, EUnlockSequenceType leUnlockSequenceType)
{
    const ProgressionData* lpProgressionData = GetProgressionData();

    // GetRival carries the console's :468 assert; callers do not duplicate it.
    const Rival* lpRival = lpProgressionData->GetRival(liRivalIndex);

    // 0x8237B23C..0x8237B248: the silent re-test -- the console reads the (out-of-range) record's
    // car id first and then skips the whole body; the read is moved under the test so a bad index
    // never dereferences. Same observable behaviour.
    if (liRivalIndex >= lpProgressionData->GetRivalCount())
    {
        return;
    }
    const CgsID lRivalId = lpRival->GetId();
    const CgsID lCarId   = lpRival->GetCarId();

    RivalData* lpSavedRival = mProfile.FindRival(lRivalId);
    if (leUnlockSequenceType == E_UNLOCK_SEQUENCE_TYPE_NONE)
    {
        if (lpSavedRival == 0)
        {
            lpSavedRival = mProfile.AddRival(lRivalId, lCarId);
        }
    }
    CGS_ASSERT_AT(lpSavedRival != 0, "lpSavedRival", CGS_ARTIST_FILE, 2465);   // BrnProgressionManager.cpp:2465

    if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
    {
        // The console streams the id through sub_82203EE8 (the StrStream CgsID formatter, not in
        // the tree); the raw 64-bit value is printed here.
        *CgsDev::Log::gpDebugPrint << "Moving rival to beaten state: " << static_cast<u64>(lRivalId) << "\n";
    }

    // [GUARD] the console stores through lpSavedRival unconditionally (`stw r11, 0x10(r28)`) and
    // would fault on null; the assert above is the console's, the null test is the host's.
    if (lpSavedRival != 0)
    {
        lpSavedRival->meState = RivalData::E_STATE_BEATEN;
    }

    if (mProfile.FindCar(lCarId) == 0)
    {
        CarData* lpCarData = AddCar(lCarId, CarData::E_UNLOCK_TYPE_SHUTDOWN_RIVAL);
        // [GUARD] the console writes CarData+0xC / +0xA through AddCar's result without a test.
        if (lpCarData != 0)
        {
            if (leUnlockSequenceType == E_UNLOCK_SEQUENCE_TYPE_DEFAULT)
            {
                lpCarData->SetUnlockDeformationAmount(0.85f);          // flt_82029BB8
            }
            else if (leUnlockSequenceType == E_UNLOCK_SEQUENCE_TYPE_NONE)
            {
                lpCarData->SetUnlockSequenceAlreadyShown();
            }
        }
        CheckForAllRivalsUnlocked();
    }
}

// --------------------------------------------------------------------------------------------
// OnPursuitWon @0x82389F40 (r3 this, r4 lRivalId (64-bit CgsID), r5 the game-action queue).
// Caller: TakedownManager::ProcessTakedownEvent @0x823940F4..0x823940F8
// (`lwz r3, 0x290(r30)` == mpProgressionManager ; r4 = GetRivalId(victim) ; r5 = lpOutput->
// GetGameActionQueue()).
//   0x82389F60..0x82389F6C  liIndex = GetProgressionData()->FindRivalIndexFromId(lRivalId)
//                           (@0x82676A90 answers miRivalCount on a miss)
//   0x82389F7C..0x8238A00C  assert liIndex < miRivalCount, "Could not find rival: " + id  (:2400)
//   0x8238A010..0x8238A054  GetRival(liIndex) (its own :468 assert) -> r30
//   0x8238A058..0x8238A064  the same bound tested silently; a miss skips everything below
//   0x8238A068..0x8238A088  assert "lpRival != NULL" (:2410)
//   0x8238A08C..0x8238A0D8  r8 = this+0x170 (Profile); the inlined Profile::FindRival(lRivalId)
//                           walk (count +0x274, table +0x6280, 0x38 stride) -> r31
//   0x8238A0DC..0x8238A0F4  assert "lpSavedRival" (:2413)
//   0x8238A0F8..0x8238A104  DefeatRivalAndUnlockCar(liIndex, 0 == E_UNLOCK_SEQUENCE_TYPE_DEFAULT)
//   0x8238A108..0x8238A128  7x ld/std: *lpRival      -> var_D0 (+0x00 of the record)
//   0x8238A12C..0x8238A14C  7x ld/std: *lpSavedRival -> var_98 (+0x38)   [taken AFTER the defeat]
//   0x8238A150/0x8238A168   li 2 ; stw var_60        (+0x70) mePreviousState = E_STATE_FLEEING
//   0x8238A154              stb r25, var_5C          (+0x74) miRivalIndex
//   0x8238A15C..0x8238A16C  AddEvent(queue, &record, 0xC5 == 197, 0x78 == 120)
//   0x8238A170..0x8238A188  li 1 ; stb var_F0 ; AddEvent(queue, &byte, 0x37 == 55, 1)
//                           == E_ACTION_REQUEST_AUTOSAVE with the FORCED payload (see the
//                           mbAutosaveRequested note in BrnProgressionManager.h)
// --------------------------------------------------------------------------------------------
void ProgressionManager::OnPursuitWon(CgsID lRivalId, BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    const ProgressionData* lpProgressionData = GetProgressionData();

    const s32 liRivalIndex = lpProgressionData->FindRivalIndexFromId(lRivalId);
    // The console streams the missing id into the assert text through sub_82203EE8; reduced to
    // the project CGS_ASSERT form (the BrnNetworkSharedIO.cpp precedent).
    CGS_ASSERT(liRivalIndex < lpProgressionData->GetRivalCount(), "Could not find rival: <lRivalId>");   // :2400

    const Rival* lpRival = lpProgressionData->GetRival(liRivalIndex);   // carries the :468 assert

    if (liRivalIndex >= lpProgressionData->GetRivalCount())
    {
        return;
    }
    CGS_ASSERT_AT(lpRival != 0, "lpRival != NULL", CGS_ARTIST_FILE, 2410);   // :2410

    RivalData* lpSavedRival = mProfile.FindRival(lRivalId);
    CGS_ASSERT_AT(lpSavedRival != 0, "lpSavedRival", CGS_ARTIST_FILE, 2413);   // :2413

    DefeatRivalAndUnlockCar(liRivalIndex, E_UNLOCK_SEQUENCE_TYPE_DEFAULT);

    // [GUARD] the console's two copy loops read through both pointers unconditionally; a null
    // here (the asserts above have already fired) would fault, so the post is skipped instead.
    if (lpRival != 0 && lpSavedRival != 0)
    {
        BrnGameState::GameStateModuleIO::RivalStateChangeAction lAction;
        // The console leaves the record's three tail pad bytes as stack residue; zeroed for a
        // reproducible payload (the ShutdownAction precedent in BrnTakedownManager_Detect.cpp).
        std::memset(&lAction, 0, sizeof(lAction));
        lAction.mRival          = *lpRival;
        lAction.mRivalSavedData = *lpSavedRival;                 // already E_STATE_BEATEN
        lAction.mePreviousState = RivalData::E_STATE_FLEEING;    // the literal 2
        lAction.miRivalIndex    = static_cast<s8>(liRivalIndex);
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                    BrnGameState::GameStateModuleIO::E_ACTION_RIVAL_STATE_CHANGED,
                                    static_cast<s32>(sizeof(lAction)));
    }

    const u8 luForcedAutosave = 1;
    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&luForcedAutosave),
                                BrnGameState::GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE, 1);
}

//
//   UnlockRivals         @0x8236F658  (asserts BrnProgressionManager.cpp:1096, ProgressionData.h:468)
//   UpdateRivals         @0x82396298  (assert  BrnProgressionData.h:468)
//   AddRivalToWorld      @0x8238B0A8  (asserts BrnProgressionManager.cpp:3673 / :3674 / :3686,
//                                      CgsArray.h:336, BrnProgressionData.h:497)   -- PARTIAL, see below
//   OnTakedownTo         @0x823666D0  (assert  BrnTrainingManager.cpp:382, via RequestTraining)
//   OnOnlineRaceComplete @0x82366B98
//
// THE TWO GAME ACTIONS, pinned at BOTH ends (see BrnGameActions.h for the enumerators):
//   195 == E_ACTION_REMOVE_ALL_RIVALS  (size 1) -> RaceCarEntityModule::RemoveAllRivalsFromWorld
//   196 == E_ACTION_ADD_RIVAL          (size 176) -> RaceCarEntityModule::AddRivalCar
// (X360 RaceCarEntityModule::HandleGameActions @0x8230BE08 `case 195:` / `case 196:`; the DWARF
// spells them 187 / 188, the same +8 the whole 187..202 band takes -- RIVAL_STATE_CHANGED
// 189 -> 197 is the neighbour this file's OnPursuitWon already posts.)
//
// Every member is reached BY NAME; the console offsets are quoted to show which member each load
// lands on. The embedded Profile is this+0x170 on the console.
// ==============================================================================================

namespace
{
    // The console's own baked assert location (BeginAssert/FireAssert/EndAssert called directly
    // so the file/line stay the binary's -- the _PreWorldUpdate / _GameStats treatment).
    const char* const KAC_PROGRESSION_MANAGER_CPP_R =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

    // One-shot park reporter (campaign house rule: a park must be visible, not silent).
    void ParkOnceRivals(bool& lrbAlreadySaid, const char* lpcMessage)
    {
        if (lrbAlreadySaid)
        {
            return;
        }
        lrbAlreadySaid = true;
        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint << lpcMessage;
        }
    }
    bool gbSaidNoTrainingManager = false;
    bool gbSaidNoProgressionData = false;
    bool gbSaidNoAchievementFwd  = false;
    // ---- [progression wave 2026-09-06, lane rivals2] ----
    bool gbSaidNoAISectionData   = false;

    // Drain-scoped counters for the witness (one UpdateRivals "drain" spans as many frames as
    // ProgressionData has rivals -- the console advances miLastUpdatedRival by at most one
    // world-add per call).
    s32 giDrainPosted    = 0;
    s32 giDrainLines     = 0;
    s32 giUnlockLines    = 0;
    s32 giTakedownLines  = 0;
    s32 giAddLines       = 0;   // [rivals2] AddRivalToWorld's own witness
}

// ----------------------------------------------------------------------------------------------
// UnlockRivals @0x8236F658 -- "has the player earned enough medals to release the next rival?"
// Callers: OnEventFinishUpdateProfile @0x823A0040 (its answer becomes ShowModeResultsAction's
// +0xC8) and PreWorldUpdate @0x823A5160 (the medals-dirty arm).
//
//   0x8236F680  lwzx r31, r26, 0xA780      ; Profile+42512 == muMedalCountFromTheStart ...
//   0x8236F6A4  clrlwi r20, r31, 24        ; ... TRUNCATED TO A BYTE for the compare below
//   loop idx = 0 .. GetRivalCount()-1 (the count is re-read every iteration):
//     0x8236F6C8  the :468 "liIndex < miRivalCount" assert is GetRival's own
//     0x8236F6EC  lpRival = rivals + 0x38*idx
//     0x8236F6F4  assert "lpRival" (BrnProgressionManager.cpp:1096)
//     0x8236F714  lbz  0x16(lpRival) ; cmplw r20 ; bgt -> next   == GetNumMedalsToUnlock()
//     0x8236F720  lbz  0x17(lpRival) ; bne -> next               == GetIsUsedForRankUpGiftCar()
//     0x8236F72C  the inlined Profile::FindRival(lpRival->mId) walk (count +0x274, table
//                 +0x6280, 0x38 stride); a miss falls into Profile::AddRival(mId, mCarId)
//     0x8236F784  lwz 0x10(saved) ; bne -> next                  == meState != E_STATE_LOCKED
//     0x8236F7B8  if (gxMessageFilterFlags & 1) "Unlocking Rival: " << mId << "\n"
//     0x8236F818  stw 1, 0x10(saved)                             == E_STATE_UNLOCKED
//     0x8236F820  ld  r3, 8(lpRival)                             == RETURN Rival::mCarId
//   0x8236F7AC  li r3, 0   -- nothing eligible
//
// ⚠️ THE RETURN VALUE IS THE RIVAL'S **CAR** ID (`ld 8(rival)`, the same word DefeatRivalAndUnlock-
// Car feeds Profile::AddRival as the car id), even though the DWARF names the results-action field
// it lands in `mNewlyUnlockedRivalID`. Pinned by the load offset, not by the field name.
// ----------------------------------------------------------------------------------------------
CgsID ProgressionManager::UnlockRivals(BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    // DWARF :661 gives the queue; the X360 body never touches r4 (it unlocks state only -- the
    // world side is UpdateRivals' job, reached through mbUpdateRivals).
    (void)lpGameActionQueue;

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // [PC GUARD] the console reaches the resource through ResourcePtr::operator-> (whose own
        // assert fires); GetProgressionData answers 0 before the load completes on PC.
        return 0;
    }

    const u8 lu8MedalCount = static_cast<u8>(mProfile.GetMedalCountFromTheStart());   // Profile+42512

    for (s32 liIndex = 0; liIndex < lpProgressionData->GetRivalCount(); ++liIndex)
    {
        const Rival* lpRival = lpProgressionData->GetRival(liIndex);   // carries the :468 assert
        CGS_ASSERT_AT(lpRival != 0, "lpRival", CGS_ARTIST_FILE, 1096);                          // :1096
        if (lpRival == 0)
        {
            continue;   // [GUARD] the console reads through it regardless
        }

        if (lpRival->GetNumMedalsToUnlock() > lu8MedalCount)          // Rival+0x16
        {
            continue;
        }
        if (lpRival->GetIsUsedForRankUpGiftCar())                     // Rival+0x17
        {
            continue;
        }

        RivalData* lpSavedRival = mProfile.FindRival(lpRival->GetId());
        if (lpSavedRival == 0)
        {
            lpSavedRival = mProfile.AddRival(lpRival->GetId(), lpRival->GetCarId());
        }
        if (lpSavedRival == 0)
        {
            continue;   // [GUARD] a full maRivals table; the console would fault on `lwz 0x10`
        }
        if (lpSavedRival->meState != RivalData::E_STATE_LOCKED)
        {
            continue;
        }

        if ((CgsDev::Message::gxMessageFilterFlags & 1) && CgsDev::Log::gpDebugPrint != 0)
        {
            // The console streams the id through sub_82203EE8 (the StrStream CgsID formatter,
            // not in the tree); the raw 64-bit value is printed here -- OnPursuitWon's precedent.
            *CgsDev::Log::gpDebugPrint << "Unlocking Rival: " << static_cast<u64>(lpRival->GetId()) << "\n";
        }

        lpSavedRival->meState = RivalData::E_STATE_UNLOCKED;          // stw 1, 0x10(saved)

        return lpRival->GetCarId();                                   // ld r3, 8(lpRival)
    }

    return 0;
}

// ----------------------------------------------------------------------------------------------
// UpdateRivals @0x82396298 -- the drain of mbUpdateRivals. Sole caller: PreWorldUpdate
// @0x823A52BC. The console advances ONE rival per call (miLastUpdatedRival, X360 +133416 ==
// 0x20928, DWARF BrnProgressionManager.h:841), so a full pass costs GetRivalCount() frames and the
// request byte only clears when the cursor reaches the end.
//
//   0x823962AC  r29 = this + 0x20928                     ; &miLastUpdatedRival
//   0x823962B4  if (miLastUpdatedRival == 0)
//   0x823962C0    AddEvent(queue, &var_60 /*1 untouched stack byte*/, 0xC3 == 195, 1)
//   loop while (miLastUpdatedRival < GetRivalCount()):
//     0x8239631C  the :468 assert is GetRival's own
//     0x82396340  r4 = rivals + 0x38*idx
//     0x8239634C  the inlined Profile::FindRival(lpRival->mId) walk (count +0x274, +0x6280, 0x38)
//     0x82396398  lwz 0x10(saved) ; == 0 -> next ; == 3 -> next   (LOCKED / BEATEN are skipped)
//     0x823963D4  AddRivalToWorld(lpRival, miLastUpdatedRival, queue) ; ++miLastUpdatedRival ; BREAK
//     0x823963AC  ++miLastUpdatedRival ; continue
//   0x823963F0  if (miLastUpdatedRival == GetRivalCount()) { miLastUpdatedRival = 0;
//   0x82396414                                              *(this + 0x20971) = 0; }
// ----------------------------------------------------------------------------------------------
void ProgressionManager::UpdateRivals(BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (miLastUpdatedRival == 0)
    {
        // `li r6,1 ; li r5,0xC3 ; addi r4, r1, var_60` -- ONE byte the console never stores to.
        // RemoveAllRivalsFromWorld ignores the payload; zeroed for a reproducible one.
        u8 lacRemoveAllRivals[1] = { 0 };
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacRemoveAllRivals),
                                    BrnGameState::GameStateModuleIO::E_ACTION_REMOVE_ALL_RIVALS, 1);
        giDrainPosted = 0;
    }

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // [PC GUARD] see UnlockRivals. The request byte stays set so the drain resumes when the
        // resource is bound, exactly as an unfinished console drain would.
        return;
    }

    while (miLastUpdatedRival < lpProgressionData->GetRivalCount())
    {
        const s32    liIndex = miLastUpdatedRival;
        const Rival* lpRival = lpProgressionData->GetRival(liIndex);   // carries the :468 assert

        const RivalData* lpSavedRival = (lpRival != 0) ? mProfile.FindRival(lpRival->GetId()) : 0;
        if (lpSavedRival != 0 &&
            lpSavedRival->meState != RivalData::E_STATE_LOCKED &&      // `cmpwi 0 ; beq -> next`
            lpSavedRival->meState != RivalData::E_STATE_BEATEN)        // `cmpwi 3 ; beq -> next`
        {
            AddRivalToWorld(lpRival, liIndex, lpGameActionQueue);
            ++miLastUpdatedRival;
            ++giDrainPosted;
            break;                                                     // one add per call
        }
        ++miLastUpdatedRival;
    }

    if (miLastUpdatedRival == lpProgressionData->GetRivalCount())
    {
        miLastUpdatedRival      = 0;
        mbUpdateRivalsRequested = false;                               // stbx 0 -> this+0x20971

        giDrainPosted = 0;
    }
}

// ----------------------------------------------------------------------------------------------
// AddRivalToWorld @0x8238B0A8 -- builds the 176-byte AddRivalCar record and posts it as game
// action 196. Sole caller: UpdateRivals @0x823963E0.
//
//   0x8238B0CC  the inlined Profile::FindRival(lpRival->mId) walk               -> lpRivalSavedData
//   0x8238B100  lbz 0x14(lpRival) ; extsb                                        == GetDistrict()
//   0x8238B114  assert "lpRivalSavedData"                        (BrnProgressionManager.cpp:3673)
//   0x8238B134  assert "leRivalDistrict < BrnWorld::E_DISTRICT_VALID_COUNT" (:3674, bound 0x12)
//   0x8238B158  7x ld/std   *lpRival          -> record + 0x20   (the whole 56-byte Rival)
//   0x8238B180  lhz 0x10(lpRival) ; extsh                         == GetPersonalityIndex()
//   0x8238B18C  GetProgressionData()->GetPersonality(index)       (its own :497 assert)
//   0x8238B1DC  4x lwz/stw  *personality      -> record + 0x58    (EventRacerPersonality, 16 B)
//   0x8238B200  7x ld/std   *lpRivalSavedData -> record + 0x68    (the whole 56-byte RivalData)
//   0x8238B218  stb  liRivalIndex             -> record + 0xA2
//   0x8238B224  r31 = this + 20 * leRivalDistrict                 == &maRoamingSections[district]
//   0x8238B228  lwz 0x10(r31)  -> the CgsArray.h:336 "Array used before Construct/Clear" assert
//   0x8238B25C  assert "maRoamingSections[leRivalDistrict].GetLength() > 0"                (:3686)
//   0x8238B2A8  GetItem(liRivalIndex % GetLength())               -> the AI section index (s16)
//   0x8238B2D4  mpAISectionData->GetAISection(section)->GetMiddle()
//   0x8238B2E8  sth  section                 -> record + 0xA0
//   0x8238B30C  lvx/stvx unk_82181520        -> record + 0x10     (image bytes 00000000 00000000
//                                                                  3F800000 00000000 == (0,0,1,0),
//                                                                  the spawn heading)
//   0x8238B318  lvx/stvx GetMiddle()         -> record + 0x00     (the spawn position)
//   0x8238B324  AddEvent(queue, record, 0xC4 == 196, 0xB0 == 176)
//
// ⭐ [progression wave 2026-09-06, lane rivals2] THE PARK IS GONE -- THIS IS NOW THE WHOLE
// CONSOLE BODY. Both holes the earlier park named were PRODUCER holes, and the lifecycle lane
// filled both in this same wave:
//   (1) `maRoamingSections` is now the DWARF's `RoamingSections maRoamingSections[18]`
//       (`typedef Array<u16,8>`, BrnGameStateTypes.h:202) -- the header defect this file found is
//       corrected -- and ProgressionManager::SetupRoamingSections @0x8236FE60 fills it from
//       Prepare2's tail (the live log prints `[lifecycle] prepare2: ... roamingSections=139`).
//   (2) `mpAISectionData` (X360 +133380) is bound by ProgressionManager::LoadAIData @0x8239A0D0
//       (`[lifecycle] prepare: aiSections=1 stage=4`).
// So the section pick, the AI-section spawn point and the action-196 post are all landed below.
//
// THE RECORD (176 bytes == 0xB0, the size the console hands AddEvent). The stack offsets fall out
// of the frame: AddEvent's r4 is `0x160+var_100`, so var_100 IS record+0x00 and every other var_
// lands at (0x100 - var) -- var_F0 -> +0x10, var_E0 -> +0x20, var_A8 -> +0x58, var_98 -> +0x68,
// var_60 -> +0xA0, var_5E -> +0xA2. Modelled below as `AddRivalCarAction`, offsets pinned by
// static_assert, every field reached BY NAME.
//
// THE THREE PC GUARDS (each marked [PC GUARD] in the body): a null Rival/RivalData, an
// out-of-range district, and an EMPTY/unconstructed district section array. All three are states
// the console's own asserts (:3673 / :3674 / :3686 + CgsArray.h:336) halt a dev build on, and the
// console's fall-through past them is an out-of-bounds `GetItem` or the `twllei r11, 0` divide
// trap -- so the post is skipped instead of faulting. The asserts still fire first, always.
// ----------------------------------------------------------------------------------------------

namespace
{
    // The 176-byte E_ACTION_ADD_RIVAL (196) payload -- RaceCarEntityModule::AddRivalCar's record.
    // REHOMED 2026-09-10: it lives next to RivalStateChangeAction in BrnGameActions.h now that the
    // PC consumer (AddRivalCar / RemoveAllRivalsFromWorld, BrnRaceCarEntityModule_Rivals.cpp) exists;
    // producer and consumer share that one definition and its offset proofs.
    using BrnGameState::GameStateModuleIO::AddRivalCarAction;
}

// ----------------------------------------------------------------------------------------------
// BrnProgression::Rival::GetPersonalityIndex -- BODIED HERE. It was in BrnRival.h's declared-only
// list and defined nowhere in the tree (the same latent unresolved-external state GetId /
// GetCarId / GetNumMedalsToUnlock were in), and it surfaces the moment its first caller is
// mounted. No standalone X360 symbol: the console folds it into AddRivalToWorld as a bare
// `lhz r11, 0x10(r31) ; extsh r31, r11` on the 56-byte-strided record -- a SIGN-extended halfword
// load, which is where the declaration's s16 return comes from. Out-of-line here (rather than a
// BrnRival.h inline like its siblings) because this wave's lane ownership puts that header
// outside this lane; REHOME it there when a header-owning lane or the Rival TU lands. Grepped:
// no other definition exists in the tree.
// ----------------------------------------------------------------------------------------------
s16 Rival::GetPersonalityIndex() const
{
    return miPersonalityIndex;   // Rival+0x10
}

// ----------------------------------------------------------------------------------------------
void ProgressionManager::AddRivalToWorld(const Rival* lpRival, s32 liRivalIndex,
                                         BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    const RivalData* lpRivalSavedData = (lpRival != 0) ? mProfile.FindRival(lpRival->GetId()) : 0;
    const s32 leRivalDistrict = (lpRival != 0) ? static_cast<s32>(lpRival->GetDistrict()) : 0;

    if (lpRivalSavedData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpRivalSavedData", CGS_ASSERT_LOCATION(KAC_PROGRESSION_MANAGER_CPP_R, 3673));
        CgsDev::Assert::EndAssert();
    }
    if (leRivalDistrict >= 18)   // BrnWorld::E_DISTRICT_VALID_COUNT (`cmpwi r28, 0x12`)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("leRivalDistrict < BrnWorld::E_DISTRICT_VALID_COUNT",
                                   CGS_ASSERT_LOCATION(KAC_PROGRESSION_MANAGER_CPP_R, 3674));
        CgsDev::Assert::EndAssert();
    }

    // [PC GUARD] the console reads through both pointers and indexes maRoamingSections with the
    // district unconditionally; the two asserts above have already fired, so the post is skipped
    // rather than faulting (the OnPursuitWon precedent above).
    if (lpRival == 0 || lpRivalSavedData == 0 ||
        leRivalDistrict < 0 || leRivalDistrict >= KI_DISTRICT_COUNT)
    {
        return;
    }

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // [PC GUARD] see UnlockRivals: the console reaches the resource through
        // ResourcePtr::operator-> (whose own assert fires); GetProgressionData answers 0 before
        // the load completes on PC. UpdateRivals cannot reach here with a null today.
        return;
    }

    AddRivalCarAction lAction;
    // The console leaves the record's 13 tail pad bytes as stack residue; zeroed here for a
    // reproducible payload (the RivalStateChangeAction precedent above).
    std::memset(&lAction, 0, sizeof(lAction));

    lAction.mRival = *lpRival;                                     // 7x ld/std  -> record +0x20

    // `lhz 0x10(lpRival) ; extsh` then GetPersonality, whose OWN :497 assert
    // ("luIndex < muPersonalityCount", BrnProgressionData.h) fires inside it -- not restated here.
    lAction.mPersonality = *lpProgressionData->GetPersonality(
        static_cast<u32>(lpRival->GetPersonalityIndex()));          // 4x lwz/stw -> record +0x58

    lAction.mRivalSavedData = *lpRivalSavedData;                    // 7x ld/std  -> record +0x68
    lAction.mu8RivalIndex   = static_cast<u8>(liRivalIndex);        // stb r26    -> record +0xA2

    // `slwi r11, r28, 2 ; add r11, r28, r11 ; slwi r11, r11, 2 ; add r31, r11, r27`
    // == this + 20 * district == &maRoamingSections[district] (the 20-byte Array<u16,8> stride).
    const RoamingSections& lrDistrictSections = maRoamingSections[leRivalDistrict];

    // GetLength() carries CgsArray's own "Array used before Construct/Clear was called" assert
    // (CgsArray.h:336) -- exactly the console's inlined `lwz 0x10(r31) ; cmpwi -1` pair at
    // 0x8238B228. The console calls GetLength() TWICE (again at 0x8238B284, for the modulo), so
    // both reads are spelled out here rather than cached.
    if (!(lrDistrictSections.GetLength() > 0))
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("maRoamingSections[leRivalDistrict].GetLength() > 0",
                                   CGS_ASSERT_LOCATION(KAC_PROGRESSION_MANAGER_CPP_R, 3686));
        CgsDev::Assert::EndAssert();
    }

    // [PC GUARD] an empty (or never-Construct'ed) district array: the console guards its `divwu`
    // by that length only with `twllei r11, 0`, i.e. it TRAPS -- there is no console behaviour to
    // reproduce past the assert above. Districts with no authored RoamingLocation are real
    // (SetupRoamingSections fills 139 sections across the 18 districts, not 18 x 8).
    if (lrDistrictSections.GetCount() <= 0)
    {
        return;
    }

    // `divwu / mullw / subf` == the UNSIGNED modulo of the rival index by the district's section
    // count; GetItem returns the u16 the console loads with `lhz 0(r11)`.
    const u32 luSectionSlot = static_cast<u32>(liRivalIndex) % lrDistrictSections.GetLength();
    const u16 lu16AISection = lrDistrictSections.GetItem(luSectionSlot);
    lAction.mi16AISectionIndex = static_cast<s16>(lu16AISection);   // sth r31 -> record +0xA0

    // [PC GUARD] LoadAIData @0x8239A0D0 binds mpAISectionData before Prepare2's tail runs
    // SetupRoamingSections, so a FILLED roaming array implies a bound resource; the console
    // dereferences it unconditionally (`addis r3, r27, 2 ; addi r3, r3, 0x904`).
    if (!mpAISectionData.HasMemoryResource())
    {
        return;
    }

    const BrnAI::AISectionsData* const lpAISectionsData = mpAISectionData.operator->();
    const BrnAI::AISection* const      lpAISection      =
        lpAISectionsData->GetAISection(static_cast<u32>(lu16AISection));

    // [PC GUARD] GetAISection carries its own bounds assert; a null here would fault in GetMiddle.
    if (lpAISection == 0)
    {
        return;
    }

    lAction.mSpawnPosition = lpAISection->GetMiddle();              // stvx -> record +0x00
    // unk_82181520, read out of the image at file offset 0x181520:
    //   00000000 00000000 3F800000 00000000 == (0.0f, 0.0f, 1.0f, 0.0f)
    // i.e. +Z -- the spawn heading every rival is dropped into the world with.
    // The native three-float constructor sets all four lanes, including w = 0.
    {
        const Vector3 lAggregateValue(0.0f, 0.0f, 1.0f);
        lAction.mSpawnHeading = lAggregateValue;
    }      // stvx -> record +0x10

    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                BrnGameState::GameStateModuleIO::E_ACTION_ADD_RIVAL,
                                static_cast<s32>(sizeof(lAction)));  // li r5, 0xC4 ; li r6, 0xB0

}

// ----------------------------------------------------------------------------------------------
// OnTakedownTo @0x823666D0 -- the PROGRESSION side of "the player took somebody down", the offline
// arm of GameStateModule::ProcessTakedownEvents @0x8238FD98.
//
//   0x823666EC  lwzx 0x208D0        ; mpCurrentCarData == 0        -> straight to the tally
//   0x823666F8  clrlwi r7           ; lbMarkedManTakeDown != 0     -> straight to the tally
//   0x8236670C  lwzx 0x1CE2C        ; Profile+117948 == meCurrentCarType ; == 1 ? arm A : arm B
//   arm A (E_CARTYPE_AGGRESSION):
//     0x82366720  ldx 0x1CE30       ; Profile+117952 == maHasPlayerSeenTraining word 0 (the
//                                     BitArray is 64-BIT worded: HasPlayerSeenTrainingType
//                                     @0x8231C878 does `word = type >> 6`, `bit = 1 << (type & 63)`)
//     0x82366724  rlwinm ...,0,9,9  ; mask 0x00400000 == bit 22    == seen(AGGRESSION_TAKEDOWN)
//     0x82366744  rlwinm ...,0,8,8  ; mask 0x00800000 == bit 23    == seen(AGGRESSION_LOST_BOOST_CHUNK)
//     0x82366764..0x82366818  the open-coded TrainingManager::RequestTraining(22) gauntlet:
//                 state == INACTIVE, !mbInPictureParadise, IsTipAllowedInGameMode(22),
//                 the "lpProfile" assert (BrnTrainingManager.cpp:382),
//                 !HasPlayerSeenTrainingType(22), (Profile+0x6C - tm+0x18) >= flt_8200426C (5.0f),
//                 then `*(tm+4) = 22 ; *(tm+0) = 1`.
//   arm B (any other car type): the identical gauntlet for type 0x28 == 40.
//   0x823668D8  BOTH ARMS AND EVERY EARLY-OUT: Profile::AddTakedown(leType).
//
// The gauntlet IS RequestTraining specialised (the tree's body at BrnTrainingManager.cpp:507 runs
// the same tests in the same order -- neither 22 nor 40 is in its 5-second-bypass set
// {0,1,2,7,17,33,39}), so the inlining is reversed into the real call, exactly as
// OnEventFinishUpdateProfile's WON_EVENT tip already is.
//
// ⚠️ r4 (the game-action queue) and r6 (the rival CgsID) are DEAD in this body -- nothing reads
// either register. The DWARF signature keeps them; the rival's own tally lives in the takedown
// manager's OnPursuitWon path, not here.
// ----------------------------------------------------------------------------------------------
void ProgressionManager::OnTakedownTo(BrnGameState::GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                      BrnGameState::ETakedownType leType,
                                      CgsID lRivalId, bool lbMarkedManTakeDown)
{
    (void)lpGameActionQueue;
    (void)lRivalId;

    if (mpCurrentCarData != 0 && !lbMarkedManTakeDown)
    {
        ETrainingType leTip = E_TRAINING_TYPE_TAKEDOWN;               // arm B, `li r4, 0x28`
        bool          lbRequest = true;

        if (mProfile.GetCurrentCarType() == BrnResource::E_CARTYPE_AGGRESSION)   // Profile+117948 == 1
        {
            leTip = E_TRAINING_TYPE_AGGRESSION_TAKEDOWN;              // arm A, `li r4, 0x16`
            lbRequest =
                !mProfile.HasPlayerSeenTrainingType(E_TRAINING_TYPE_AGGRESSION_TAKEDOWN) &&
                !mProfile.HasPlayerSeenTrainingType(E_TRAINING_TYPE_AGGRESSION_LOST_BOOST_CHUNK);
        }

        if (lbRequest)
        {
            if (mpTrainingManager != 0)
            {
                mpTrainingManager->RequestTraining(leTip);
            }
            else
            {
                // The same hole OnEventFinishUpdateProfile's WON_EVENT tip sits in: nothing calls
                // SetTrainingManager because the outer ProgressionManager::Construct/Prepare pair
                // is not reconstructed. DELETE-WHEN that installer lands.
            }
        }

    }

    mProfile.AddTakedown(leType);                                     // 0x823668E0
}

// ----------------------------------------------------------------------------------------------
// OnOnlineRaceComplete @0x82366B98 -- ONLINE. Caller: ModeManager::SendModeStopMessages
// @0x8234BEC0 (the `!timedOut && meCurrentGameModeType == E_MODE_ONLINE_RACE` arm, which is inside
// that function's still-deferred online block -- see BrnModeManager_Start.cpp).
//
//   0x82366B9C  r10 = this + 0x170                                   ; the embedded Profile
//   0x82366BAC  lwz/addi/stw  profile + 0x1CCE0                      ; ++miNumOnlineRacesDone
//   0x82366BB8  if (lbWonRace) lwz/addi/stw profile + 0x1CCE4        ; ++miNumOnlineRacesWon
//               (that pair IS the inlined DWARF `Profile::OnOnlineRaceComplete(int32_t, bool)`;
//                the player-count argument is not stored anywhere)
//   0x82366BE8  r7 = *(this + 0x1CE50) == profile + 0x1CCE0          ; the updated "done" tally
//   0x82366BEC  r6 = *(this + 0x1CE54) == profile + 0x1CCE4          ; the updated "won" tally
//   0x82366BF0  r3 = *(this + 0x20938) == mpAchievementManager
//   0x82366BF4  b  AchievementManagerBase::OnOnlineRaceComplete      ; r4/r5 still hold our args
// ----------------------------------------------------------------------------------------------
void ProgressionManager::OnOnlineRaceComplete(s32 liNumberOfPlayers, bool lbWonRace)
{
    mProfile.OnOnlineRaceComplete(liNumberOfPlayers, lbWonRace);

    // ⛔ PARK -- console: `AchievementManagerBase::OnOnlineRaceComplete(mpAchievementManager,
    //   liNumberOfPlayers, lbWonRace, mProfile.GetNumOnlineRacesDone(),
    //   mProfile.GetNumOnlineRacesWon());`  (the tail branch at 0x82366BF4, whose r6/r7 are the
    // two counters just bumped). The body EXISTS in the tree -- BrnGameStateAchievementManager-
    // PS3.cpp:193, X360 0x8235B6A8, achievements 35/36/37/38 -- but that TU is NOT MOUNTED in
    // tools/build/build_game_exe.bat, so calling it is an LNK2019. Every other consumer of that
    // class parks around the same hole (see the P2 parks in GameStateModule_gRR_00.cpp).
    // The profile tallies above ARE written, which is the part this function owns.
    // DELETE-WHEN BrnGameStateAchievementManagerPS3.cpp is mounted.
    (void)mpAchievementManager;
}

}

// FOLDED FROM BrnProgressionManager_Unlocks.cpp
// ===================================================================================
// BrnProgression::ProgressionManager -- THE TROPHY / SPECIAL-CAR UNLOCK WRITERS.
//   GameSource/Unity/../GameState/Progression/BrnProgressionManager.cpp  (per-function
//   partfile of that TU, the house BrnProgressionManager_EventFinish.cpp precedent; the
//   console homes all three functions in BrnProgressionManager.cpp, which is why every
//   assert below is fired with that file's baked path + the console's own line number
//   rather than through CGS_ASSERT's __FILE__/__LINE__.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   ProgressionManager::UnlockSpecialCars    @0x8237AF38  (107 instructions)
//   ProgressionManager::UnlockCarFromTrophy  @0x8237B0E8  (57 instructions)
//   ProgressionManager::OnTrophyUnlock       @0x82389740  (500 instructions)
//
// [drive-thru link-closure wave, 2026-08-27] These three were PARKED in the header on
// 2026-08-20 as "not small" and blocked on a missing ProgressionData trophy-table header.
// Two thirds of that park had gone stale: the table ROOT was already modelled
// (BrnProgressionData.h, GetTrophyUnlock @0x823569F0 bodied) and the "four unmodelled
// manager/Profile fields" were all modelled Profile members reached through the manager
// (+776/+780/+482 == Profile+408/+412/+114). What was genuinely missing was the table's
// ELEMENT TYPE -- see the ⚠️⚠️ note in SharedClasses/Progression/BrnTrophyUnlockData.h,
// which this wave homed.
//
// ⚠️ THE ASM IS THE SPINE. Hex-Rays mis-renders three things here and each is called out
// at its site:
//   (a) `UnlockCarFromTrophy(__SPAIR64__(a1, v12), a2)` -- it fused r3:r4 into one 64-bit
//       argument. The call is (this, CgsID, unlockType) in r3/r4/r5.
//   (b) the profile car scans render as `*(v6 + 4) != a1`; the asm is `ld r7, 0(r10)`, a
//       full 64-bit compare at offset ZERO (the same register-pair confusion).
//   (c) `if (&v8[6 * v9] != -640)` / `if (24 * v4 + ... == -640)` are the compiler's
//       always-true null checks on `&array[i]`, not real comparisons.
//
// Every member is reached BY NAME through BrnProgressionManager.h / BrnProfile.h; no raw
// offset arithmetic on `this`.
// ===================================================================================

#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/Progression/BrnProfile.h"            // BrnProgression::Profile / CarData
#include "GameSource/GameState/Progression/BrnProgressionCarData.h" // BrnProgression::CarData (UnlockType, SetUnlockDeformationAmount)
#include "SharedClasses/Progression/BrnProgressionData.h"           // BrnProgression::ProgressionData (trophy table root)
#include "SharedClasses/Progression/BrnTrophyUnlockData.h"          // BrnProgression::TrophyUnlockData (the 16-byte element)
#include "SharedClasses/DataLists/VehicleList.h"                    // BrnResource::VehicleList
#include "SharedClasses/DataLists/VehicleListEntry.h"               // BrnResource::VehicleListEntry (GetId/GetParentId/GetLiveryType)
#include "GameSource/GameState/BrnGameActions.h"                    // GameStateModuleIO::TrophyUnlockAction
#include "GameShared/GameClasses/Core/CgsAssert.h"                  // CgsDev::Assert::Begin/Fire/End
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h" // OnFindAllDriveThrus
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // VariableEventQueue<13312,16>::AddEvent

namespace BrnProgression
{

namespace GsmIO = BrnGameState::GameStateModuleIO;

// The console's baked assert path for BrnProgressionManager.cpp (this partfile's console home).
static const char* const KAC_UNLOCKS_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

// flt_82001CC0 == 0.0f -- the "this car has never been deformed" comparand in UnlockSpecialCars.
static const f32 KF_NO_UNLOCK_DEFORMATION = 0.0f;
// flt_82029BB8 == 0.85f -- the deform a trophy car is presented with in the unlock sequence.
// (The same literal ReallyEnterJunkyardAtStartOfGame @0x823931F8 writes as a bare
// `*(carData + 12) = 0.85f`; both read the same .rdata word.)
static const f32 KF_TROPHY_CAR_UNLOCK_DEFORMATION = 0.85f;

// ===================================================================================
// ProgressionManager::UnlockCarFromTrophy  @ 0x8237B0E8
//
// Award the car a trophy unlocks. Returns false -- doing NOTHING -- when the profile
// already owns it, which is what stops OnTrophyUnlock's table walk from re-awarding the
// same car every time a tally is re-evaluated, and is why OnTrophyUnlock stops its walk
// on a true.
//
// ⚠️ ARG SHAPE FROM ASM (0x8237B0F4..0x8237B100): `mr r29, r3` / `mr r30, r4` /
// `mr r28, r5` -- this, the 64-bit CgsID, the unlock type. Hex-Rays' `(__int64 a1, int a2)`
// is r3:r4 fused into one value; there is no doubleword first parameter.
// ===================================================================================
bool ProgressionManager::UnlockCarFromTrophy(CgsID lCarId, s32 liTrophyType)
{
    // `lwz r9, 0x26C(r8)` + the `addi r10, r8, 0x280` / `ld r7, 0(r10)` / `cmpld` / stride
    // 0x18 walk is Profile::FindCar open-coded. Already owning the car ends the call.
    if (mProfile.FindCar(lCarId) != 0)
    {
        return false;
    }

    // `li r5, 2` == CarData::E_UNLOCK_TYPE_TROPHY.
    CarData* lpCarData = AddCar(lCarId, CarData::E_UNLOCK_TYPE_TROPHY);
    if (lpCarData == 0)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert("lpCarData != NULL", CGS_ASSERT_LOCATION(KAC_UNLOCKS_FILE, 2322));
        CgsDev::Assert::EndAssert();
    }

    // `stfs f0, 0xC(r31)` with f0 == flt_82029BB8.
    lpCarData->SetUnlockDeformationAmount(KF_TROPHY_CAR_UNLOCK_DEFORMATION);

    // The 16-byte record the console builds at var_40 and hands to Append:
    //   std r30, 0x90+var_40(r1)   -> rec+0x00, the CgsID
    //   stw r28, 0x90+var_38(r1)   -> rec+0x08, the unlock type
    // (That store pair is half the proof that TrophyUnlockAction's members are CgsID-first;
    //  the other half is SendTrophyUnlockUpdate's two named asserts. See BrnGameActions.h.)
    BrnGameState::GameStateModuleIO::TrophyUnlockAction lUnlock;
    lUnlock.mCarToUnlock = lCarId;
    lUnlock.meUnlockType = static_cast<TrophyUnlockData::UnlockType>(liTrophyType);
    mQueueOfTrophyCarUnLocks.Append(lUnlock);

    return true;
}

// ===================================================================================
// ProgressionManager::UnlockSpecialCars  @ 0x8237AF38
//
// Walk the loaded vehicle list; for every entry whose LIVERY TYPE (VehicleListEntry+0xE9,
// `lbz r11, 0xE9(r29)`) equals lu8LiveryType, award that entry's car IF the profile
// already owns its PARENT car -- the special/derived cars are variants of cars you own.
// CheckForSpecialCarUnlocks calls it with 4 (rank-gated set) and 3 (100%-gated set).
//
// ⚠️ The parent-owned test is not decoration: the `beq`/`b loc_8237B0C4` pair at
// 0x8237AFEC/0x8237B000 means an entry whose parent is NOT in the profile is skipped
// entirely -- no AddCar, no livery choice.
// ===================================================================================
void ProgressionManager::UnlockSpecialCars(u8 lu8LiveryType)
{
    // `lwz r3, 0(r22)` / `lwz r11, 0x3400(r3)` -- the bound is re-read from the list every
    // iteration (0x8237B0C4), not cached.
    for (s32 liIndex = 0; liIndex < mpVehicleList->GetVehicleCount(); ++liIndex)
    {
        const BrnResource::VehicleListEntry* lpVehicleListEntry =
            mpVehicleList->GetVehicleData(liIndex);
        if (lpVehicleListEntry == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpVehicleListEntry != NULL", CGS_ASSERT_LOCATION(KAC_UNLOCKS_FILE, 2117));
            CgsDev::Assert::EndAssert();
        }

        if (lpVehicleListEntry->GetLiveryType() != lu8LiveryType)
        {
            continue;
        }

        // `ld r8, 8(r29)` -- the PARENT car's id, scanned for in the profile.
        const CgsID lParentCarId  = lpVehicleListEntry->GetParentId();
        const CgsID lDerivedCarId = lpVehicleListEntry->GetId();

        if (mProfile.FindCar(lParentCarId) == 0)
        {
            continue;   // parent not owned -> this variant is not awarded
        }

        // `li r5, 4` == CarData::E_UNLOCK_TYPE_GOLD_SILVER, for BOTH livery types --
        // the console passes the unlock KIND, not the livery type, to AddCar.
        CarData* lpCarData = AddCar(lDerivedCarId, CarData::E_UNLOCK_TYPE_GOLD_SILVER);
        if (lpCarData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpCarData != NULL", CGS_ASSERT_LOCATION(KAC_UNLOCKS_FILE, 2124));
            CgsDev::Assert::EndAssert();
        }

        // `stb r26, 0xA(r31)` with r26 == 1 -- special cars skip the unlock cinematic.
        lpCarData->SetUnlockSequenceAlreadyShown();

        // The console re-runs the parent scan (0x8237B058..0x8237B0A8) rather than reusing
        // the first result, then makes the new variant the chosen livery for the base car
        // UNLESS the parent still carries an unlock deformation. `lfs f0, 0xC(r11)` /
        // `fcmpu f0, f31` with f31 == flt_82001CC0 == 0.0f: a parent that is still sitting
        // in its own unlock sequence keeps its livery until that finishes.
        const CarData* lpParentCarData = mProfile.FindCar(lParentCarId);
        if (lpParentCarData == 0 ||
            lpParentCarData->GetUnlockDeformationAmount() == KF_NO_UNLOCK_DEFORMATION)
        {
            mProfile.SetChosenLiveryIdForBaseCar(lParentCarId, lDerivedCarId);
        }
    }
}

// ===================================================================================
// ProgressionManager::OnTrophyUnlock  @ 0x82389740
//
// THE TROPHY-CONDITION MACHINE. Walk PROGRESSION.DAT's trophy-unlock table; for every
// record whose type equals liTrophyType, test that type's own tally against the record's
// threshold, and on the first record that both matches and passes, award its car and stop.
//
// SHAPE OF THE WALK (asm 0x8238982C..0x82389F34):
//   * the bound is muTrophyUnlockCount, cached once at 0x823897A0 into var_188 and reloaded
//     at the tail (0x82389F14); the record pointer is `table_base + 16 * index` with the
//     byte offset carried in var_1A0 and advanced by 16;
//   * every iteration re-fetches the ProgressionData through the resource pointer and
//     re-fires the bound assert -- that is GetTrophyUnlock @0x823569F0 inlined, which is
//     why the assert's baked location is BrnProgressionData.h:302 and not this file;
//   * a record whose type does not match is skipped WITHOUT touching the unlocked flag
//     (`bne cr6, loc_82389F18` at 0x8238989C jumps past the test).
//
// ⭐ A ZERO THRESHOLD MEANS "ALREADY EARNED", NOT "NEVER". `cmplwi cr6, r25, 0` /
// `bne loc_82389CD8` at 0x823898A8: only a NON-zero muNumberTrophyUnlock reaches the
// tally switch. A zero one falls through the per-type "we need a number" assert chain to
// LABEL_37, which sets the flag TRUE. That is how types 1..21 (the "complete/find all X"
// trophies, whose condition the CALLER has already decided -- e.g. OnDriveThru's
// AreAllDriveThrusCompleted) get awarded: they carry no number, and the asserts exist only
// to catch a data author leaving a 0 in one of the 22..34 "reach N of these" rows.
// ⚠️ This is the [[placeholder-identity-element]] shape inverted -- here 0 is not inert and
// not "immediately never"; it is "immediately YES". Do not "guard" it.
// ===================================================================================
void ProgressionManager::OnTrophyUnlock(s32 liTrophyType)
{
    // `GetTotalWinCount(&var_1A0, &var_198, &var_19C)` then `add r11, r11, r10` over the
    // FIRST and THIRD out-params only -- rank wins + special-event wins. The middle
    // (non-rank) count is deliberately not in the medal total.
    u32 luRankWins         = 0;
    u32 luNonRankWins      = 0;
    u32 luSpecialEventWins = 0;
    mProfile.GetTotalWinCount(luRankWins, luNonRankWins, luSpecialEventWins);
    const u32 luTotalMedals = luRankWins + luSpecialEventWins;

    const ProgressionData* lpProgressionData = GetProgressionData();
    if (lpProgressionData == 0)
    {
        // The console reaches the table through ResourcePtr<ProgressionData>::operator->,
        // which asserts internally; there is no null test in this body. Guarding here rather
        // than dereferencing null is a PC bring-up difference and nothing else -- everything
        // below is a pure read of a resource that Prepare2's loader binds.
        return;
    }

    const u32 luTrophyUnlockCount = lpProgressionData->GetTrophyUnlockCount();
    bool      lbConditionMet      = false;

    for (u32 luIndex = 0; luIndex < luTrophyUnlockCount; ++luIndex)
    {
        // GetTrophyUnlock inlined: the bound assert is BrnProgressionData.h:302, the
        // console's own baked location for it.
        const TrophyUnlockData* lpCurrentTrophyUnlockData = lpProgressionData->GetTrophyUnlock(luIndex);
        if (lpCurrentTrophyUnlockData == 0)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert("lpCurrentTrophyUnlockData != NULL", CGS_ASSERT_LOCATION(KAC_UNLOCKS_FILE, 2162));
            CgsDev::Assert::EndAssert();
        }

        // `lhz r23, 4(r24)` then `cmpw` against the argument -- a halfword field widened for
        // the compare, which is why the record's type is a u16 and not the enum's int.
        const s32 liRecordType = static_cast<s32>(lpCurrentTrophyUnlockData->GetUnlockType());
        if (liRecordType != liTrophyType)
        {
            continue;
        }

        const u32   luNumberRequired = lpCurrentTrophyUnlockData->GetNumberForTrophyUnlock();
        const CgsID lCarUnlockId     = lpCurrentTrophyUnlockData->GetCarUnlockID();

        if (luNumberRequired == 0)
        {
            // The data-authoring guard chain. Each arm is the console's own line number; all
            // of them fall through to "condition met", asserts or not.
            switch (liTrophyType)
            {
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_MEDELS:
                    CGS_FAIL("We need a number of medals for this\n");                  // :2174
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_CRASH_ROADRULES:
                    CGS_FAIL("We need a number of Road rules for this\n");              // :2175
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_ROADRULES:
                    CGS_FAIL("We need a number of Road rules for this\n");              // :2176
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_TIME_ROADRULES:
                    CGS_FAIL("We need a number of Road rules for this\n");              // :2177
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_NORMALTAKEDOWNS:
                    CGS_FAIL("We need a number that is not 0\n");                       // :2178
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_SIGNATURETAKEDOWNS:
                    CGS_FAIL("We need a number that is not 0\n");                       // :2179
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_JUMPS:
                    CGS_FAIL("We need a number that is not 0\n");                       // :2180
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_SMASHES:
                    CGS_FAIL("We need a number that is not 0\n");                       // :2181
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_STUNTS:
                    CGS_FAIL("We need a number that is not 0\n");                       // :2184
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_ONLINE_VERTICLE_TAKEDOWNS:
                    CGS_FAIL("We need a number that is not 0\n");                       // :2185
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE:
                    CGS_FAIL("We need a number that is not 0\n");                       // :2186
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_OF_EACH_ONLINE_EVENT_COMPLETE:
                    CGS_FAIL("We need a number that is not 0\n");                       // :2187
                    break;
                default:
                    // Types 1..21 (and 34) reach here with no assert: they carry no number by
                    // design. The console's switch has no default arm and simply falls through.
                    break;
            }
            lbConditionMet = true;
        }
        else
        {
            // The `addi r11, r23, -0x16 / cmplwi r11, 0xC` jump table @0x82389CD8 -- exactly
            // types 22..34, with 27 falling to the default (unknown) arm. Every comparison is
            // the compiler's unsigned `>=` idiom (`subfc/subfe/addi r22, r11, 1`) except where
            // noted.
            switch (liTrophyType)
            {
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_MEDELS:
                    lbConditionMet = (luTotalMedals >= luNumberRequired);
                    break;

                // The three road-rule tallies are the MANAGER's, not the profile's
                // (manager+133464/+133460/+133456).
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_ROADRULES:
                    lbConditionMet = (static_cast<u32>(miNumberOfNumberOfCompleteRoadRulesRuledByPlayer)
                                          >= luNumberRequired);
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_TIME_ROADRULES:
                    lbConditionMet = (static_cast<u32>(miNumberOfParTimeRoadRulesRuledByPlayer)
                                          >= luNumberRequired);
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_CRASH_ROADRULES:
                    lbConditionMet = (static_cast<u32>(miNumberOfParCrashRoadRulesRuledByPlayer)
                                          >= luNumberRequired);
                    break;

                case TrophyUnlockData::E_UNLOCKTYPE_NUM_NORMALTAKEDOWNS:
                    lbConditionMet = (static_cast<u32>(mProfile.GetTotalTakedownCount())
                                          >= luNumberRequired);
                    break;

                // ⛔⛔ THE COMPARISON IS THE OTHER WAY ROUND, AND THAT IS THE BINARY, NOT A
                // TYPO. Case 31 is the ONLY arm whose `subfc` operands are swapped:
                //     0x82389E4C  lwz   r11, 0x30C(r19)      ; miTotalOnlineVerticleTakedownCount
                //     0x82389E50  subfc r11, r11, r25        ; r25 - r11   <-- reversed
                //     0x82389E54  subfe r11, r11, r11
                //     0x82389E58  addi  r22, r11, 1          ; r22 = (threshold >= tally)
                // Every other arm is `subfc r11, r25, r11` == (tally >= threshold). So this
                // trophy is awarded while the player has FEWER vertical takedowns than the
                // requirement and stops being awarded once they pass it -- i.e. it is
                // effectively always true at 0 takedowns. Reproduced verbatim; it looks like an
                // original-source slip and is exactly the kind of thing a "tidy-up" would
                // silently correct. Do not flip it without new evidence.
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_ONLINE_VERTICLE_TAKEDOWNS:
                    lbConditionMet = (luNumberRequired
                                          >= static_cast<u32>(mProfile.GetTotalOnlineVerticleTakedownCount()));
                    break;

                case TrophyUnlockData::E_UNLOCKTYPE_NUM_JUMPS:
                    lbConditionMet = (static_cast<u32>(mProfile.GetStuntElementCount(
                                          BrnGameState::E_STUNT_ELEMENT_TYPE_JUMP)) >= luNumberRequired);
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_SMASHES:
                    lbConditionMet = (static_cast<u32>(mProfile.GetStuntElementCount(
                                          BrnGameState::E_STUNT_ELEMENT_TYPE_SMASH)) >= luNumberRequired);
                    break;
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_STUNTS:
                    lbConditionMet = (static_cast<u32>(mProfile.GetStuntElementCount(
                                          BrnGameState::E_STUNT_ELEMENT_TYPE_BILLBOARD)) >= luNumberRequired);
                    break;

                // `lbz r11, 0x1E2(r19)` + `extsb` -- a SIGNED byte, compared through the same
                // unsigned idiom, so a negative rating wraps huge and passes. Reproduced as the
                // sign-extended value re-read as unsigned rather than "cleaned up".
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_PERCENTAGE_PARALLELPARK_ONLINE:
                    lbConditionMet = (static_cast<u32>(static_cast<s32>(
                                          mProfile.GetPowerParkingBetweenOtherPlayersBestRating()))
                                      >= luNumberRequired);
                    break;

                // ⭐ THIS ARM IGNORES THE THRESHOLD ENTIRELY. `lwz 0x130/0x134/0x13C(r16)`
                // against the literal 5 (`cmpwi cr6, r11, 5`), on the PROFILE not the manager:
                // Profile+304/+308/+316 == maGameModeTypeAmountCompleted[10]/[11]/[13], the
                // three online modes E_MODE_ONLINE_RACE, E_MODE_ONLINE_ROAD_RAGE and
                // E_MODE_ONLINE_BURNING_HOME_RUN. Index 12 (E_MODE_ONLINE_FUGITIVE) is NOT
                // tested -- the console reads +0x134 then jumps to +0x13C.
                case TrophyUnlockData::E_UNLOCKTYPE_NUM_OF_EACH_ONLINE_EVENT_COMPLETE:
                    lbConditionMet =
                        mProfile.GetGameModeTypeCompleted(GsmIO::E_MODE_ONLINE_RACE)             >= 5 &&
                        mProfile.GetGameModeTypeCompleted(GsmIO::E_MODE_ONLINE_ROAD_RAGE)        >= 5 &&
                        mProfile.GetGameModeTypeCompleted(GsmIO::E_MODE_ONLINE_BURNING_HOME_RUN) >= 5;
                    break;

                case TrophyUnlockData::E_UNLOCKTYPE_NUM_MUG_SHOTS_COLLECTED:
                    lbConditionMet = (static_cast<u32>(mProfile.GetNumAllMugshots()) >= luNumberRequired);
                    break;

                default:
                    // Includes E_UNLOCKTYPE_NUM_SIGNATURETAKEDOWNS (27), which the jump table
                    // routes to the default arm -- it has no tally in this build.
                    CGS_FAIL("Unknown trophy unlock");                                  // :2270
                    break;
            }
        }

        if (lbConditionMet)
        {
            if (UnlockCarFromTrophy(lCarUnlockId, liTrophyType))
            {
                return;   // `bne cr6, loc_82389F38` -- the walk stops at the first award
            }
        }
    }
}


// ===================================================================================
// ProgressionManager::OnDriveThru  @ 0x82399DD0   ⭐⭐⭐ THE LAST UNRESOLVED EXTERNAL
//
// Record the discovery of a drive-thru and fan out everything that follows from it.
// DriveThruManager::HandleDriveThru is its only caller, and that path is HOT: a
// __debugbreak() left in this symbol's place broke into the debugger ~9.5 s into an
// ordinary junkyard -> car-select -> free-burn drive, via ProcessPlayerTriggers.
//
// ⚠️ ARG SHAPE FROM ASM (0x82399DDC..0x82399DF0): r3=this, r4=the drive-thru id ->r30,
// r5=the region type ->r29, r6=the game-action queue ->r27, and r31 = this+0x170, the
// embedded Profile. The Profile calls that "take no arguments" in the pseudocode
// (IsDriveThruDiscoverd) are the compiler leaving r4/r5 untouched from the prologue --
// nothing clobbers them between `mr r30, r4` and the call, which is why only `mr r3, r31`
// is emitted.
//
// ⭐ THE WHOLE CASCADE IS GATED, AND ONLY THE FIRST GATE IS COMMON. A re-entered
// drive-thru returns immediately (IsDriveThruDiscoverd). A newly discovered one runs
// AddDriveThru; only when THAT awards a trophy does OnTrophyUnlock fire; and the
// completion pair (CheckForSpecialCarUnlocks + SendGameCompletionResults) needs every
// drive-thru in the world found. The unconditional part is the action-55 autosave post
// and the dirty flag.
//
// ⚠️ THE AUTOSAVE POST CARRIES A ZERO PAYLOAD (`li r11, 0` / `stb r11, var_40` before
// `li r5, 0x37`), i.e. an UNFORCED autosave: GuiModule ORs that byte into
// mbForceProfileAutosave, so a zero leaves the 60-second throttle in charge. The
// completion path's own request (mbAutosaveRequested, drained by PreWorldUpdate) posts
// the same action with a payload of ONE and does bypass it. Same id, different urgency.
// ===================================================================================
void ProgressionManager::OnDriveThru(CgsID lId, BrnTrigger::GenericRegion::Type leType,
                                     BrnGameState::GameStateModuleIO::GameActionQueue* lpQueue)
{
    if (mProfile.IsDriveThruDiscoverd(lId, leType))
    {
        return;
    }

    // AddDriveThru returns the TrophyUnlockData::UnlockType this discovery completed, or
    // E_UNLOCKTYPE_NONE (0) when the category is not finished yet.
    const s32 liTrophyType = mProfile.AddDriveThru(lId, leType);
    if (liTrophyType != TrophyUnlockData::E_UNLOCKTYPE_NONE)
    {
        OnTrophyUnlock(liTrophyType);

        // `li r4, 0x15` == 21 == E_UNLOCKTYPE_FIND_ALL_DRIVE_THRUS -- the umbrella trophy on
        // top of the per-category one, awarded only when this was the last category to close.
        if (mProfile.AreAllDriveThrusCompleted())
        {
            OnTrophyUnlock(TrophyUnlockData::E_UNLOCKTYPE_FIND_ALL_DRIVE_THRUS);
        }
    }

    if (mProfile.AreAllDriveThrusCompleted())
    {
        GetAchievementManager()->OnFindAllDriveThrus();
    }

    if (lpQueue != 0)
    {
        if (mProfile.AreAllDriveThrusCompleted())
        {
            CheckForSpecialCarUnlocks();
            SendGameCompletionResults(lpQueue);
        }

        GsmIO::RequestAutoSaveAction lRequestAutoSave;
        lRequestAutoSave.mbForceAutosave = false;
        lpQueue->AddEvent(&lRequestAutoSave, GsmIO::E_ACTION_REQUEST_AUTOSAVE, sizeof(lRequestAutoSave));
    }

    // `stbx r10(1), r28, 0x20988` == manager+133512, unconditional on the not-yet-discovered
    // path (it sits AFTER the `if (lpQueue)` block, not inside it).
    mbDriveThrusDirty = true;
}

}

// FOLDED FROM BrnProgressionManager_wBT_05.cpp
// BrnProgression::ProgressionManager::OnMugshotSent (debug info BrnProgressionManager.h).
// No out-of-line copy exists on the console: it is inlined into GameStateModule::ProcessGameEvents'
// case 147 (E_EVENT_ONLINE_MUGSHOT_SENT), which is where this body is read from. The arm bumps the
// profile's sent-mugshot count (+0x170 profile, +117992 count) and only then reads the count back
// for the achievement manager the progression manager holds (+0x20938).

#include "GameSource/GameState/Progression/BrnProgressionManager.h"

#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/AchievementManager/BrnGameStateAchievementManagerBase.h"   // OnMugshotSent(s32)

namespace BrnProgression
{

void ProgressionManager::OnMugshotSent()
{
    mProfile.OnMugshotSent();
    mpAchievementManager->OnMugshotSent(mProfile.GetNumMugshotsSent());
}

}

#undef CGS_ARTIST_FILE
