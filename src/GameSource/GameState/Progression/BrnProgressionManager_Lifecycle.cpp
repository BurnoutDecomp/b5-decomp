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
    const char* const KAC_PROGRESSION_MANAGER_CPP =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/Progression/BrnProgressionManager.cpp";

    void FireConsoleAssert(const char* lpcExpression, s32 liLine)
    {
        CgsDev::Assert::BeginAssert();
        CgsDev::Assert::FireAssert(lpcExpression, KAC_PROGRESSION_MANAGER_CPP, liLine);
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
    const s32         KI_PROGRESSION_PERFMON_PAGE  = 5;                 // `li r4, 5`

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
    bool gbSaidDebugComponent = false;

    // [FLAG PC witness] -- NOT IN THE X360 BINARY. Opt-in (BRN_PROGRESSION_LIFECYCLE=1), three
    // lines per boot at most (Construct / Prepare / the Prepare2 tail each print once).
    // tools/tests/cases/progression_lifecycle.ps1 is the reader.
    bool LifecycleDiagEnabled()
    {
        static const bool sbDiag = (getenv("BRN_PROGRESSION_LIFECYCLE") != 0);
        return sbDiag && CgsDev::Log::gpDebugPrint != 0;
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
//   +0x20978 meModeToCheckForAllWinTypes = -1   +0x2096F mbForceAutoSaveForOneHundredPercent
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
    maPresetRaces.Clear();                                // +0x1F780 (the Array's count word)
    muNumPresetRaces        = 0;                          // +0x208D8
    mbUpdateRivalsRequested = false;                      // +0x20971 (DWARF mbUpdateRivals)
    miLastUpdatedRival      = 0;                          // +0x20928
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
    mbDriveThrusDirty            = true;                  // +0x20988, `stbx r11(1)` -- NOT zero
    miNumberOfParCrashRoadRulesRuledByPlayer         = 0;  // +0x20950
    miNumberOfParTimeRoadRulesRuledByPlayer          = 0;  // +0x20954
    miNumberOfNumberOfCompleteRoadRulesRuledByPlayer = 0;  // +0x20958
    mQueueOfTrophyCarUnLocks.Clear();                     // +0x208C8 (`stwx 0`, the count word)
    mfTimeTillAllEventTypeCompleteHudMessage = KF_HUD_MESSAGE_HOLD_CLEARED;   // +0x2097C
    miMaxCarCount            = 0;                     // +0x2095C (DWARF miMaxCarCount -- see below)
    mbCheckAllWinTypesPending    = false;                 // +0x20975
    mbCheckAllWinTypesArmed      = false;                 // +0x20976
    mbNeedToShowAllRivalsBeatenMessage   = false;         // +0x20980
    mbShowShutDownAllIfNeeded            = false;         // +0x20981
    mfTimeTillShowAllRivalsBeatenMessage = KF_HUD_MESSAGE_HOLD_CLEARED;       // +0x20984
    meModeToCheckForAllWinTypes  = -1;                    // +0x20978, `li r8, -1` as a WORD
    mbAutosaveRequested          = false;                 // +0x2096F

    // 0x8237A8C8 / 0x8237A8F4 -- the two CPU monitors, then the console's own >= 0 asserts.
    miPreWorldUpdate  = CgsDev::PerfMonCpu::AddMonitor(
        KPC_PRE_WORLD_MONITOR_NAME,
        static_cast<CgsDev::PerfMonCpuPage>(KI_PROGRESSION_PERFMON_PAGE),
        /*lbMinimum*/ false, KF_PROGRESSION_CPU_BUDGET_MS, /*lbLibPerfTagged*/ true);
    miPostWorldUpdate = CgsDev::PerfMonCpu::AddMonitor(
        KPC_POST_WORLD_MONITOR_NAME,
        static_cast<CgsDev::PerfMonCpuPage>(KI_PROGRESSION_PERFMON_PAGE),
        /*lbMinimum*/ false, KF_PROGRESSION_CPU_BUDGET_MS, /*lbLibPerfTagged*/ true);

    if (miPreWorldUpdate < 0)
    {
        FireConsoleAssert("miPreWorldUpdate >= 0", 197);
    }
    if (miPostWorldUpdate < 0)
    {
        FireConsoleAssert("miPostWorldUpdate >= 0", 198);
    }

    mbHasJustRankedUp = false;
    mNewlyUnlockedCarID = 0;

    // [FLAG PC bring-up] Remaining console stores HAVE NO MODELLED MEMBER IN THIS TREE'S HEADER and
    // are therefore NOT made here, rather than made through an offset hack. They are named
    // by the DWARF; the remaining initialization gaps are listed below:
    //   +0x2092C miLastReturnedRival                  (DWARF :842 -- the rivals lane declared its
    //                                                  twin miLastUpdatedRival; add this with it)
    //   +0x20974 mbPlayerJustWonATrophyUpdateRequired (DWARF :194)
    //   +0x2096D mi8HackEventRankNumber               (DWARF :173)
    //   +0x2096E mbHackEventNumberActive              (DWARF :176)
    //   +0x20800 mbSendAchievementAwardedEvent_DEBUG /
    //   +0x20804 miAchievementToAward_DEBUG           (DWARF :828/:829)
    // The ONE that is NOT zero is +0x2094C meLastPlayerDistrict = 18 (DWARF :853); its value is
    // kept in KI_NO_LAST_PLAYER_DISTRICT above for whoever declares the member.
    // DELETE-WHEN those members are declared -- each is one line here.
    (void)KI_NO_LAST_PLAYER_DISTRICT;

    // NAME CORRECTION, RECORDED NOT APPLIED. The member stored above as `miMaxCarCount` is
    // spelled `int32_t miMaxCarCount` by the DWARF (BrnProgressionManager.h:859), sitting exactly
    // where +0x2095C is (right after the three road-rules tallies), with the getter
    // GetMaxCarCount() at DWARF :429. This tree's header already FLAGS the committed name as
    // "inferred from those two increments only" (AddCar's two bumps) -- and ApplyVehicleList
    // below SEEDS it from the vehicle list's selectable/sponsor pair, which is a maximum, not a
    // sponsor tally. The rename is NOT made in this wave because AddCar
    // (BrnProgressionManager_Unlocks.cpp) belongs to another lane running concurrently.
    // FOLLOW-UP: rename miMaxCarCount -> miMaxCarCount.

    if (LifecycleDiagEnabled())
    {
        *CgsDev::Log::gpDebugPrint
            << "[lifecycle] construct: street=" << (mpStreetManager    != 0 ? 1 : 0)
            << " stunt="                        << (mpStuntManager     != 0 ? 1 : 0)
            << " training="                     << (mpTrainingManager  != 0 ? 1 : 0)
            << " carselect="                    << (mpCarSelectManager != 0 ? 1 : 0)
            << " preMon="                       << miPreWorldUpdate
            << " postMon="                      << miPostWorldUpdate
            << " medalsRequested="              << (mbPlayerMedalsUpdateRequired ? 1 : 0)
            << "\n";
    }
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

        mpAISectionData = lpAIDataResponse->mHandle;   // CreateFromHandle(&mpAISectionData, ...)
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

    if (LifecycleDiagEnabled())
    {
        static bool sbSaid = false;
        if (!sbSaid)
        {
            sbSaid = true;
            *CgsDev::Log::gpDebugPrint
                << "[lifecycle] prepare: aiSections="
                << (mpAISectionData.HasMemoryResource() ? 1 : 0)
                << " stage=" << static_cast<s32>(meAILoadStage)
                << "\n";
        }
    }
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
        static_cast<const BrnTrigger::TriggerData*>(mpTriggerData);
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
        static_cast<const BrnTrigger::TriggerData*>(mpTriggerData);

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
//   0x8236FE04  a 2080-byte STACK scratch buffer (`var_820`) is handed to HACK_SetupRaces
//   0x8236FE20  muNumPresetRaces = its answer  (`stw r3, 0(r28)`, r28 == this + 0x208D8)
//   0x8236FE3C  per race: Array<Race,64>::Append(&maPresetRaces, &scratch[i])   (stride 0x78)
// The loop bound is re-read from muNumPresetRaces every iteration, exactly as written.
//
// The console's stack scratch is 2080 bytes -- room for 17 races, of which HACK_SetupRaces only
// ever writes 5. Sized here at the count the two share, so it cannot silently overrun if that
// count ever grows.
// --------------------------------------------------------------------------------------------
void ProgressionManager::ProcessLoadedPresetRaces()
{
    Race laRaceScratch[KI_HACK_RACE_COUNT];

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
        static_cast<const BrnTrigger::TriggerData*>(mpTriggerData);

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
// DWARF :297) is r7, and r7 is never read anywhere in the body -- it is dead in this build, so it
// is left off this declaration rather than carried as a lie. The copy is a memberwise assignment
// rather than the console's memcpy(120): Race's host size is the same 120 bytes, but assignment
// is what stays correct if it ever is not.
// --------------------------------------------------------------------------------------------
u32 ProgressionManager::GetRacesAtLandmark(Race* lpaRacesOut, u32 luMaxRaces,
                                           BrnGameState::LandmarkIndex lLandmarkIndex) const
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

        lpaRacesOut[luRaceCount] = lrRace;
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

// --------------------------------------------------------------------------------------------
// ProgressionManager::RunPrepare2Tail
//
// Prepare2's five trailing calls, factored here so BrnProgressionManager.cpp's Prepare2 keeps one
// line for them and this lane stays file-disjoint. The console issues them in this order at
// 0x8239DD94..:
//     ComputeLandmarkAISectionIndices(this);
//     ProcessLoadedPresetRaces(this);
//     ProgressionDebugComponent::Construct(&mDebugComponent, this, lpModeManager);
//     CgsDev::DebugComponent::Register(&mDebugComponent);
//     SetupRoamingSections(this);
// It is NOT a console function of its own -- it is this file's name for that sequence, which is
// why it carries no address.
//
// PARK -- ProgressionDebugComponent::Construct @0x82371A30 and its Register. The class has NO
// home in this tree (there is no BrnProgressionDebugComponent.{h,cpp}, and this header models the
// component as a reserved slot with only the vtable pointer named). Its body is a
// BaseCollisionGenerator::Destruct, twenty-odd debug-UI field seeds and a 6-entry game-mode name
// table built from ProgressionManager::GetEvent -- pure dev UI, and the lane brief scopes the 38
// render/callback functions out. Landing Construct alone would need the whole class layout, so it
// is PARKED BY NAME rather than half-built.
// DELETE-WHEN BrnProgressionDebugComponent.{h,cpp} exist.
// --------------------------------------------------------------------------------------------
void ProgressionManager::RunPrepare2Tail()
{
    // [PC GUARD -- not console code, and it guards a PC failure mode, not a console one.]
    // Three of the four legs below dereference mpAISectionData unconditionally, exactly as the
    // console does, because on the console Prepare (stage 20) cannot reach Prepare2 without
    // having bound it. On PC a GameData acquire ALWAYS replies -- if the resource is missing it
    // replies with a NULL handle -- so a data or pool problem would turn "the AI sections did not
    // load" into an access violation in the middle of the boot, on an exe shared with every other
    // lane. The guard converts that into the one thing that is actually useful: a named line.
    // It changes nothing on a healthy boot (the witness prints aiSections=1).
    // DELETE-WHEN the AI-lanes acquire is proven non-null at the source.
    if (!mpAISectionData.HasMemoryResource())
    {
        static bool sbSaidNoAISections = false;
        ParkOnce(sbSaidNoAISections,
                 "[FLAG PC bring-up] ProgressionManager::Prepare2: mpAISectionData is NOT bound "
                 "(LoadAIData @0x8239A0D0 reported DONE with a null handle), so "
                 "ComputeLandmarkAISectionIndices / ProcessLoadedPresetRaces / SetupRoamingSections "
                 "are SKIPPED this boot -- the landmark AI-section table and the preset-race list "
                 "stay empty. This is a data/pool problem, not a code one.\n");
        return;
    }

    ComputeLandmarkAISectionIndices();
    ProcessLoadedPresetRaces();

    ParkOnce(gbSaidDebugComponent,
             "[FLAG PC bring-up] ProgressionManager::Prepare2: ProgressionDebugComponent::Construct "
             "@0x82371A30 + CgsDev::DebugComponent::Register are NOT reconstructed (the class has no "
             "home in the tree); the debug component stays the reserved slot the ctor seeds.\n");

    SetupRoamingSections();

    if (LifecycleDiagEnabled())
    {
        static bool sbSaid = false;
        if (!sbSaid)
        {
            sbSaid = true;

            s32 liRoamingSections = 0;
            for (s32 liDistrict = 0; liDistrict < KI_DISTRICT_COUNT; ++liDistrict)
            {
                liRoamingSections += maRoamingSections[liDistrict].GetCount();
            }

            const BrnTrigger::TriggerData* const lpTriggerData =
                static_cast<const BrnTrigger::TriggerData*>(mpTriggerData);

            *CgsDev::Log::gpDebugPrint
                << "[lifecycle] prepare2: presetRaces=" << static_cast<s32>(muNumPresetRaces)
                << " roamingSections="                  << liRoamingSections
                << " landmarkAI="                       << lpTriggerData->GetNumLandmarks()
                << " vehicles="                         << (mpVehicleList        != 0 ? 1 : 0)
                << " ach="                              << (mpAchievementManager != 0 ? 1 : 0)
                << " street="                           << (mpStreetManager      != 0 ? 1 : 0)
                << " stunt="                            << (mpStuntManager       != 0 ? 1 : 0)
                << " training="                         << (mpTrainingManager    != 0 ? 1 : 0)
                << "\n";
        }
    }
}

}
