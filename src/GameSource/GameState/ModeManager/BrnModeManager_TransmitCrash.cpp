// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_TransmitCrash.cpp
// ============================================================================
// ModeManager keystone wave B -- AGENT 7b (the agent-7 split, conductor decision #8):
// the Transmit trio + the crash pair. Agent 7a owns PreWorldUpdate / PostWorldUpdate in
// BrnModeManager_WorldTick.cpp; this partfile owns nothing else and defines nothing else.
//
//   TransmitCheckPointDistancesToFinishLine  X360 0x82341FF8   (PreWorldUpdate callee)
//   TransmitAndIncrementCheckPointsReached   X360 0x82342098   (PreWorldUpdate callee)
//   TransmitAndIncrementFinishReached        X360 0x823424D0   (PreWorldUpdate callee)
//   ProcessPlayerCrashes                     X360 0x8231E638   (PostWorldUpdate callee)
//   CheckForOutOfRangeCarsReachingFinish     X360 0x82340800   (PostWorldUpdate callee)
//
// Owning header: GameSource/GameState/ModeManager/BrnModeManager.h (FROZEN this wave -- this
// partfile does not edit it; every declaration friction is filed as a header_request instead).
//
// ============================================================================
// WHAT THESE FIVE ACTUALLY DO (the per-frame publish half of the checkpoint system)
// ============================================================================
// ModeManager keeps two 35-bit sets -- mRaceCarReachedCheckpoint and mRaceCarReachedFinish --
// that the trigger/landmark consumers SET during the world update. The two
// TransmitAndIncrement* bodies below are the DRAIN: once per PreWorldUpdate they walk the set
// bits, post one game action per car, and clear the whole set with a single 64-bit store
// (the console's `std r20, 0(r14)`; on host CgsContainers::BitArray<>::UnSetAll()).
// TransmitCheckPointDistancesToFinishLine is the once-per-mode publish of the ScoringSystem's
// per-checkpoint distance-to-finish table. ProcessPlayerCrashes latches "the player crashed
// this frame" for UpdateCurrentMode's road-rage arm, and CheckForOutOfRangeCarsReachingFinish
// is the offline-race-only rescue path that credits a car that reached the finish line while
// it was OUT OF RANGE (i.e. no physics trigger ever fired for it).
// ============================================================================

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

// [!] SCOPE NOTE (the hazard BrnModeManager.h:53-61 documents): BrnGameStateModuleIO.h drags in
// a SECOND `enum EActiveRaceCarIndex : s32` inside `namespace BrnGameState`. It is included HERE
// (in the partfile, as the header's banner instructs) and AFTER BrnModeManager.h, so the class'
// member signatures are already bound to the global BurnoutConstants.h enums. Every race-car
// enum spelled in the bodies below is therefore GLOBAL-QUALIFIED (`::EGlobalRaceCarIndex`,
// `::EActiveRaceCarIndex`, `::E_..._COUNT`) so it cannot silently rebind.
#include "GameSource/GameState/BrnGameStateModuleIO.h"                                  // OutputBuffer / PostWorldInputBuffer / GameActionQueue
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"                        // CgsModule::VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Containers/CgsArray.h"                                 // CgsContainers::Array<f32,16> (the action-118 payload)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                              // BitArray<35>::GetFirst/GetNextNonZeroBit / UnSetAll
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // the global race-car output interface
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"                       // AICarOutputInterface::GetAISectionIndex
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"                // BrnPhysics::Vehicle::RaceCarCrashEvent
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h"       // VehicleManagerOutputInterface::RaceCarCrashEventQueue (the REAL queue type)
#include "GameSource/GameState/BrnCheckpointData.h"                                     // BrnGameState::CheckpointData (landmark + AI-section pair)

namespace BrnGameState
{

// ============================================================================================
// X360-ATTESTED GAME-ACTION IDS + PAYLOAD SIZES POSTED BY THIS PARTFILE.
//
// Every id below is pinned at BOTH ends: the `li r5,<id>` / `li r6,<size>` immediates the
// producer hands VariableEventQueue<13312,16>::AddEvent, and the DecFIGS DWARF enumerator whose
// action record has EXACTLY that byte size and field order (see the record definitions below).
//
//   X360 113 (0x71) size 16  -- TransmitAndIncrementCheckPointsReached  @0x823422B0 / 0x823422AC
//                               DWARF E_ACTION_RACE_CAR_REACHED_CHECKPOINT == 108
//   X360 114 (0x72) size  8  -- TransmitAndIncrementFinishReached       @0x823426AC / 0x823426A4
//                               DWARF E_ACTION_RACE_CAR_REACHED_FINISH == 109
//   X360 115 (0x73) size  1  -- TransmitAndIncrementCheckPointsReached  @0x823422E4 / 0x823422E0
//                               DWARF E_ACTION_PLAYER_REACHED_PENULTIMATE_CHECKPOINT == 110
//   X360 118 (0x76) size 68  -- TransmitCheckPointDistancesToFinishLine @0x82342078 / 0x82342074
//                               DWARF E_ACTION_SET_WAYPOINT_DISTANCES_TO_FINISH == 113
//
// ALL FOUR ARE DWARF+5 -- the SAME +5 X360 shift BrnGameActions.h already records for the
// mode-lifecycle block (SHOW_MODE_RESULTS 32->37, FINISHED_MODE_RESULTS 33->38, STOP_MODE
// 34->39, SET_IN_MODE_START_REGION 39->44, SET_COUNTDOWN 42->47) and for the stunt block. The
// shift is NOT extrapolated here: each of the four is independently confirmed by its payload
// SIZE matching its DWARF record byte-for-byte (16 / 8 / 1 / 68) and by its producer's role.
//
// [x] header_request #1 CLOSED 2026-08-26 (stuntrace waveB CLOSURE round). All four now live in
// GameSource/GameState/BrnGameActions.h's EGameActionType -- E_ACTION_RACE_CAR_REACHED_CHECKPOINT
// (113), E_ACTION_RACE_CAR_REACHED_FINISH (114),
// E_ACTION_PLAYER_REACHED_PENULTIMATE_CHECKPOINT (115) and
// E_ACTION_SET_WAYPOINT_DISTANCES_TO_FINISH (118) -- and are used BY NAME below. The evidence
// above travelled into the header with them; the TU-local enum is deleted.
// ============================================================================================

namespace
{
// ============================================================================================
// THE THREE ACTION RECORDS THIS PARTFILE POSTS.
//
// [header_request #2] These are GameStateModuleIO action records and their real home is
// BrnGameActions.h (DWARF BrnGameActions.h:2275 / :2293 / :2308 / :3288, quoted verbatim in
// each banner). They are TU-LOCAL (unnamed namespace) here only because that header is out of
// this agent's lane; the layouts below ARE the wire format, and the static_asserts pin the
// exact console byte sizes the AddEvent immediates demand. Delete this block and switch to the
// header types the moment header_request #2 lands.
// ============================================================================================

// DWARF BrnGameActions.h:2275 -- RaceCarReachedCheckpointAction. Field order + offsets
// independently re-derived from the X360 stack frame at 0x823421D0..0x82342200:
//   sp+0x80  stw  activeRaceCarIndex              -> +0   meActiveRaceCarIndex
//   sp+0x84  stw  globalRaceCarIndex              -> +4   meGlobalRaceCarIndex
//   sp+0x88  stw  mauNextLandmark[global]         -> +8   miCheckPointIndex
//   sp+0x8C  sth  mauLandmarkSectionIndices[next] -> +12  muNextCheckpointAISectionIndex
//   sp+0x8E  stb  (global == mePlayerGlobalRaceCarIndex) -> +14 mbIsLocalPlayer
// posted as {ptr, type 113, size 16}.
struct RaceCarReachedCheckpointActionRecord
{
    ::EActiveRaceCarIndex meActiveRaceCarIndex;             // +0
    ::EGlobalRaceCarIndex meGlobalRaceCarIndex;             // +4
    s32                   miCheckPointIndex;                // +8
    u16                   muNextCheckpointAISectionIndex;   // +12
    bool                  mbIsLocalPlayer;                  // +14
};

// DWARF BrnGameActions.h:2293 -- RaceCarReachedFinishAction. X360 frame at 0x823425E4 / 0x82342628:
//   sp+0x60  stw  GetGlobalRaceCarIndex(active) -> +0  meGlobalRaceCarIndex
//   sp+0x64  stw  finish position               -> +4  miFinishPosition
// posted as {ptr, type 114, size 8}.
struct RaceCarReachedFinishActionRecord
{
    ::EGlobalRaceCarIndex meGlobalRaceCarIndex;   // +0
    s32                   miFinishPosition;       // +4
};

// DWARF BrnGameActions.h:3288 -- SetWayPointDistancesToFinishAction { Array<float32_t,16u> maDistances; }.
// 16 * 4 element bytes + the trailing Array count word == 68 == the X360 `li r6,0x44` immediate
// at 0x82342074, and the console builds it in exactly that shape (Array count zeroed at
// sp+0xA0 == payload+64 before the Append loop, whole 68-byte block handed to AddEvent).
struct SetWayPointDistancesToFinishActionRecord
{
    // (the console symbol is CgsContainers::Array<float,16>::Append @0x823182B8; this tree homes
    //  the Array template at GLOBAL scope -- CgsArray.h -- so it is spelled ::Array here)
    ::Array<f32, 16> maDistances;
};

// The three wire sizes, pinned against the `li r6,<size>` immediates the console hands AddEvent.
// These are the ONLY hard size facts in this partfile; everything else is by named member.
static_assert(sizeof(RaceCarReachedCheckpointActionRecord) == 16,
              "RaceCarReachedCheckpointAction wire size (X360 li r6,0x10 @0x823422AC)");
static_assert(sizeof(RaceCarReachedFinishActionRecord) == 8,
              "RaceCarReachedFinishAction wire size (X360 li r6,8 @0x823426A4)");
static_assert(sizeof(SetWayPointDistancesToFinishActionRecord) == 68,
              "SetWayPointDistancesToFinishAction wire size (X360 li r6,0x44 @0x82342074)");

// DWARF BrnGameActions.h:2308 -- PlayerReachedPenultimateCheckpointAction is an EMPTY struct
// (it carries no payload at all; the id IS the message). That is why the console posts it with
// size 1 from a stack slot it never writes: `addi r4, r1, 0x130+var_E0` @0x823422E8 with NO
// preceding store to sp+0x50. The single byte on the wire is empty-struct padding, not a value
// -- reproduced below as an explicitly-zeroed one-byte local, the same way
// GameStateModule::RequestUnpause reproduces its own uninitialised 1-byte post
// (BrnGameStateModule.cpp:1133). NOT a console bug, and NOT a placeholder zero.

} // anonymous namespace

// ============================================================================================
// TransmitCheckPointDistancesToFinishLine  --  X360 0x82341FF8  (DWARF BrnModeManager.h:874)
//
// Once per mode: publish the ScoringSystem's per-checkpoint distance-to-finish table to the
// world/GUI consumers as action 118, then latch so it is never sent twice.
//
// [!!] BOOL-BLOCK FINDING -- READ WITH hazards H4. The fire-once latch this body reads AND
// writes is X360 +38150 (0x9506): `addis r29,r3,1; addi r29,r29,-0x6AFA` (65536 - 27386) at
// 0x82342004 / 0x8234200C, `lbz r11,0(r29)` at 0x82342014, `li r11,1; stb r11,0(r29)` at
// 0x82342084 / 0x82342088. The frozen header names +38150 `mbFinishedOnlineLobbyMode` -- a
// DWARF-ORDER-ONLY PROVISIONAL name with no reader or writer behind it. THIS body is the byte's
// only known reader and its only known writer, and its semantics are statable in one line:
// "the waypoint distance-to-finish table has already been transmitted for this mode". That
// outranks DWARF order under the section-2 rule. The committed name is used below verbatim
// (the header is frozen); the rename is filed as header_request #3. Note the two zeroing sites
// the header already records for this byte -- StartGameMode and PrepareForMode -- are exactly
// the two places a per-mode "already sent" latch must be re-armed: corroboration, not coincidence.
// [OK] RESOLVED 2026-08-26: header_request #3 was APPLIED by the wave-B fix round after the
// export sweep re-confirmed the finding independently. The member is now
// mbWayPointDistancesToFinishSent and the call sites below use it.
// [!] It is NOT the same byte as +38148 mbDistanceToFinishLineTransmitted (0x9504): that one is
// PrepareForMode's "the distance data is READY" gate that StartModeIntro's fire-once test needs.
// Two adjacent but genuinely distinct flags -- do not collapse them.
// ============================================================================================
void ModeManager::TransmitCheckPointDistancesToFinishLine(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                                          ScoringSystem&                   lrScoringSystem)
{
    // X360: `if (!*(this+38150) && *(scoring+23776))`. The scoring byte at +23776 is
    // mbCheckPointDistancesToFinishReady (ScoringSystem::ProcessFinishDistances @0x823124F0 sets
    // it, and GetCheckpointDistanceToFinish's own "Distance to finish not ready" assert guards on
    // it) -- reached through its named accessor, never by the raw offset (hazards H9).
    if (mbWayPointDistancesToFinishSent)   // +38150 -- the fire-once "already sent" latch
    {
        return;
    }
    if (!lrScoringSystem.IsCheckPointDistanceToFinishReady())   // X360 scoring +23776
    {
        return;
    }

    SetWayPointDistancesToFinishActionRecord lAction;
    lAction.maDistances.Construct();   // X360 `stw r31(=0), 0xD0+var_30(r1)` @0x82342038 -- the Array count word

    // The X360 loop bound is re-read from scoring +0x4EE0 (20192) on EVERY iteration
    // (`lwz r11, 0x4EE0(r30)` @0x8234205C). That member is miTotalCheckpoints: muTotalLaps is the
    // adjacent +0x4ED8, pinned by ScoringSystem::ClearData's `stw r30,0x4ED8`, and
    // muNumCarsFinishedRace sits between them at +0x4EDC.
    for (s32 liCheckpoint = 0; liCheckpoint < lrScoringSystem.GetTotalCheckpoints(); ++liCheckpoint)
    {
        const f32 lfDistance =
            lrScoringSystem.GetCheckpointDistanceToFinish(static_cast<u32>(liCheckpoint));
        lAction.maDistances.Append(lfDistance);
    }

    // X360 `bl GameStateModuleIO::OutputBuffer::GetGameActionQueue` (0x8231D4B8 -- the
    // write-locked this+4 accessor) then AddEvent(queue, payload, 118, 68).
    lpOutputBuffer->GetGameActionQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lAction),
        GameStateModuleIO::E_ACTION_SET_WAYPOINT_DISTANCES_TO_FINISH,
        static_cast<s32>(sizeof(SetWayPointDistancesToFinishActionRecord)));

    mbWayPointDistancesToFinishSent = true;   // X360 +38150 = 1 (the fire-once latch)
}

// ============================================================================================
// TransmitAndIncrementCheckPointsReached  --  X360 0x82342098  (DWARF BrnModeManager.h:878)
//
// Drain mRaceCarReachedCheckpoint: one action-113 per car that crossed a checkpoint since the
// last drain, plus the action-115 "the player just reached the penultimate checkpoint" ping,
// then clear the whole bit set.
//
// The console inlines CgsContainers::BitArray<35>'s iteration at both ends of the walk (the
// cntlzd lowest-set-bit idiom at 0x823420E0..0x82342100 for the first bit, the in-field
// `sld 1, bit & 63` scan from 0x82342418 for each next bit, carrying the CgsBitArray.h:203
// "invalid index : <i> < 35" stream-assert that is the container's own bounds guard). Expressed
// here through the committed named methods -- GetFirstNonZeroBit / GetNextNonZeroBit / UnSetAll
// -- which are value-identical for a single-field array.
//
// [!] THE CLEAR IS UNCONDITIONAL AND ON EVERY EXIT PATH (`std r20, 0(r14)` @0x823424C4 is the
// single join point, reached even when the set was empty). Losing it re-posts every checkpoint
// action every frame.
// ============================================================================================
void ModeManager::TransmitAndIncrementCheckPointsReached(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    typedef CgsContainers::BitArray< ::E_GLOBAL_RACE_CAR_INDEX_COUNT > KRaceCarBitSet;

    for (s32 liBit = mRaceCarReachedCheckpoint.GetFirstNonZeroBit();
         liBit != KRaceCarBitSet::KI_INVALID_BITINDEX;
         liBit = mRaceCarReachedCheckpoint.GetNextNonZeroBit(liBit))
    {
        const ::EGlobalRaceCarIndex leGlobalRaceCarIndex = static_cast< ::EGlobalRaceCarIndex>(liBit);

        // X360 `lwz r11, 0x6D58(r15)` (mpGameStateModule) + 0x3C0D0 -- accessor grow (1). The
        // console re-fetches it inside the loop; kept inside the loop for parity.
        const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCarOutput =
            GetGlobalRaceCarOutputInterface();

        CGS_ASSERT(leGlobalRaceCarIndex < ::E_GLOBAL_RACE_CAR_INDEX_COUNT,
                   "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");   // BrnRaceCarEntityModuleOutputInterface.h:1513

        RaceCarReachedCheckpointActionRecord lReachedCheckpointAction;

        // X360 `*(4 * (global + 525) + iface)` == the interface's maeActiveRaceCarIndices[global].
        // [!] HEADER FLAG REFUTED (BrnModeManager.h:374-377 says "the interface names only the
        // active->global direction ... DO NOT fabricate the walk"): the inverse accessor DOES
        // exist -- BrnRaceCarEntityModuleOutputInterface.h:327,
        // `EActiveRaceCarIndex GetActiveRaceCarIndex(EGlobalRaceCarIndex) const`. It is exactly the
        // array this expression indexes: the sibling GetGlobalRaceCarIndex @0x82310570 SCANS that
        // same table at iface+2100 (35 entries, 4-byte stride) looking for an active index. Called
        // by name here rather than through the still-unbodied ModeManager::GlobalToActiveRaceCarIndex.
        lReachedCheckpointAction.meActiveRaceCarIndex =
            lpGlobalRaceCarOutput->GetActiveRaceCarIndex(leGlobalRaceCarIndex);
        lReachedCheckpointAction.meGlobalRaceCarIndex = leGlobalRaceCarIndex;
        // X360 `lbzx r30, r11(== this+0x7FF8), r31` -- mauNextLandmark[global].
        // HasRaceCarHitValidCheckpoint parks the index of the checkpoint the car JUST hit there
        // (it writes mauNextLandmark BEFORE calling MarkCarHittingCheckpoint).
        lReachedCheckpointAction.miCheckPointIndex =
            static_cast<s32>(mauNextLandmark[leGlobalRaceCarIndex]);
        // X360 `extrwi r11, cntlzw(global - *(this+0x803C)), 1, 26` -- cntlzw == 32 iff the
        // difference is zero, i.e. (global == mePlayerGlobalRaceCarIndex). A BYTE store (hazards H9).
        lReachedCheckpointAction.mbIsLocalPlayer = (leGlobalRaceCarIndex == mePlayerGlobalRaceCarIndex);

        CGS_ASSERT(lReachedCheckpointAction.miCheckPointIndex >= 0,
                   "lReachedCheckpointAction.miCheckPointIndex >= 0");                                     // BrnModeManager.cpp:473
        CGS_ASSERT(static_cast<u32>(lReachedCheckpointAction.miCheckPointIndex) < muNumLandmarks,
                   "static_cast<uint32_t>( lReachedCheckpointAction.miCheckPointIndex ) < muNumLandmarks"); // BrnModeManager.cpp:474

        // The NEXT checkpoint's AI section, or 0 when the car has none left. CALLS the committed
        // CountCheckpointsRemaining body (hazards H2 -- never re-implemented here).
        if (CountCheckpointsRemaining(leGlobalRaceCarIndex) > 0)
        {
            CGS_ASSERT(leGlobalRaceCarIndex < ::E_GLOBAL_RACE_CAR_INDEX_COUNT,
                       "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT");   // BrnModeManager.cpp:5484
            const s32 liNextCheckpoint =
                maCarCheckpointData[leGlobalRaceCarIndex].GetNextCheckpointIndex();
            // X360 `lhzx r11, ((u8)next + 0x3F60) * 2, r15` -- mauLandmarkSectionIndices[next] (+32448).
            // The console narrows the returned index to its low byte first (`clrlwi r11,r3,24`).
            lReachedCheckpointAction.muNextCheckpointAISectionIndex =
                mauLandmarkSectionIndices[static_cast<u8>(liNextCheckpoint)];
        }
        else
        {
            lReachedCheckpointAction.muNextCheckpointAISectionIndex = 0;   // X360 `sth r20(=0), var_A4`
        }

        lpGameActionQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lReachedCheckpointAction),
            GameStateModuleIO::E_ACTION_RACE_CAR_REACHED_CHECKPOINT,
            static_cast<s32>(sizeof(RaceCarReachedCheckpointActionRecord)));

        // The player one checkpoint from the finish: the GUI's "last checkpoint" ping. The
        // console's payload is the EMPTY PlayerReachedPenultimateCheckpointAction -- see the
        // record banner above for why its single wire byte is never written.
        if (leGlobalRaceCarIndex == mePlayerGlobalRaceCarIndex)
        {
            if (CountCheckpointsRemaining(leGlobalRaceCarIndex) == 1)
            {
                u8 lacPenultimateCheckpoint[1] = { 0 };
                lpGameActionQueue->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(lacPenultimateCheckpoint),
                    GameStateModuleIO::E_ACTION_PLAYER_REACHED_PENULTIMATE_CHECKPOINT,
                    1);
            }
        }
    }

    mRaceCarReachedCheckpoint.UnSetAll();   // X360 `std r20, 0(r14)` @0x823424C4 -- every exit path
}

// ============================================================================================
// TransmitAndIncrementFinishReached  --  X360 0x823424D0  (DWARF BrnModeManager.h:882)
//
// Drain mRaceCarReachedFinish: one action-114 per car that crossed the finish line, then clear
// the set.
//
// [!] LAYOUT FINDING -- mRaceCarReachedFinish IS INDEXED BY ::EActiveRaceCarIndex, NOT BY
// ::EGlobalRaceCarIndex (its twin mRaceCarReachedCheckpoint IS global-indexed). Proof, from this
// body alone: the bit index r28 is (a) handed to
// RCEntityGlobalRaceCarOutputInterface::GetGlobalRaceCarIndex @0x82310570, whose own asserts
// bound its argument to [0,8) and name it leActiveRaceCarIndex; (b) range-asserted here against
// E_ACTIVE_RACE_CAR_INDEX_COUNT (`cmpwi r28,-1` / `cmpwi r28,8` @0x823425E0 / 0x823425EC); and
// (c) handed to ScoringSystem::GetCarData, which asserts < 8. Only bits 0..7 of the 35 are ever
// set. The declared BitArray<35> is console-faithful (both sets are the same type) -- this note
// exists so nobody "fixes" the index space in either direction.
// ============================================================================================
void ModeManager::TransmitAndIncrementFinishReached(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    typedef CgsContainers::BitArray< ::E_GLOBAL_RACE_CAR_INDEX_COUNT > KRaceCarBitSet;

    // X360 `lwz r11, 0xD98(r3); cmplwi r11,0; beq -> 0x8234288C`. NOTE the early-out branches
    // PAST the bit-set clear at 0x82342888: with no current mode the set is left INTACT for the
    // next frame. Faithful -- do not hoist the UnSetAll above this test.
    if (mpCurrentGameMode == nullptr)
    {
        return;
    }

    // X360 `mpGameStateModule + 0x3C0D0`, hoisted out of the loop by the console this time
    // (`stw r11, 0x130+var_E0(r1)` @0x82342508) -- accessor grow (1).
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCarOutput =
        GetGlobalRaceCarOutputInterface();

    for (s32 liBit = mRaceCarReachedFinish.GetFirstNonZeroBit();
         liBit != KRaceCarBitSet::KI_INVALID_BITINDEX;
         liBit = mRaceCarReachedFinish.GetNextNonZeroBit(liBit))
    {
        const ::EActiveRaceCarIndex leActiveRaceCarIndex = static_cast< ::EActiveRaceCarIndex>(liBit);

        RaceCarReachedFinishActionRecord lReachedFinishAction;

        // active -> global (the interface's own table scan). The console calls it BEFORE the
        // range assert below; order preserved.
        lReachedFinishAction.meGlobalRaceCarIndex =
            lpGlobalRaceCarOutput->GetGlobalRaceCarIndex(leActiveRaceCarIndex);

        // ScoringSystem::GetCarData's inlined precondition (its out-of-line copy asserts the same
        // thing at BrnScoringSystem.h:2793). Verbatim string is the strict-`<` variant at image
        // 0x82015F90 -- the one the `cmpwi r28,-1 / ble` + `cmpwi r28,8 / blt` pair matches.
        CGS_ASSERT((leActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID)
                       && (leActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                   "(leActiveRaceCarIndex>E_ACTIVE_RACE_CAR_INDEX_INVALID) && (leActiveRaceCarIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)");

        // X360: `r11 = 8; if (carData) r11 = *(carData + 0x14)`. CarData embeds CarScoreData at
        // +0, and CarScoreData +0x14 is miFinishPosition (BrnGameStateSharedIO.h) -- reached by
        // its named accessor, never by the raw offset (hazards H9). The console's fallback
        // immediate is a literal 8, i.e. last of the eight scoring slots, for a car the
        // ScoringSystem has no record of.
        CarData* lpCarData = mScoringSystem.GetCarData(leActiveRaceCarIndex);
        if (lpCarData != nullptr)
        {
            lReachedFinishAction.miFinishPosition = lpCarData->GetScoreData()->GetFinishPosition();
        }
        else
        {
            lReachedFinishAction.miFinishPosition = 8;   // X360 `li r11, 8`
        }

        // A car with no global slot is simply not reported (the console jumps straight on to the
        // next bit -- no action, no assert).
        if (lReachedFinishAction.meGlobalRaceCarIndex == ::E_GLOBAL_RACE_CAR_INDEX_INVALID)
        {
            continue;
        }

        // The X360 streams the offending index into the message ("Invalid AI car index: " then the
        // value, through CgsDev::StrStreamBase); CGS_ASSERT takes the static prefix, matching the
        // house treatment of the other streamed asserts in this class.
        CGS_ASSERT((lReachedFinishAction.meGlobalRaceCarIndex >= ::E_GLOBAL_RACE_CAR_INDEX_0)
                       && (lReachedFinishAction.meGlobalRaceCarIndex < ::E_GLOBAL_RACE_CAR_INDEX_COUNT),
                   "Invalid AI car index: ");   // BrnModeManager.cpp:538

        lpGameActionQueue->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lReachedFinishAction),
            GameStateModuleIO::E_ACTION_RACE_CAR_REACHED_FINISH,
            static_cast<s32>(sizeof(RaceCarReachedFinishActionRecord)));
    }

    mRaceCarReachedFinish.UnSetAll();   // X360 `std r22, 0(r15)` @0x82342888
}

// ============================================================================================
// ProcessPlayerCrashes  --  X360 0x8231E638  (DWARF BrnModeManager.h:160)
//
// Recompute, from scratch every PostWorldUpdate, the one-frame latch mbPlayerCrashedLastFrame
// (+38154 / 0x950A): true iff the player's own car is the subject of a PRIMARY crash event this
// frame. UpdateCurrentMode's road-rage arm is its consumer
// (`if (!TakedownManager::IsInTakedownCamera() && mbPlayerCrashedLastFrame)
//    ScoringSystem::OnRoadRagePlayerCrashed(...)`) -- which is what pins the byte's semantics
// (header +38154), so this body must clear it unconditionally BEFORE the scan or one crash would
// stick for the rest of the mode.
// ============================================================================================
void ModeManager::ProcessPlayerCrashes(const GameStateModuleIO::PostWorldInputBuffer* lpPostWorldInputBuffer)
{
    // [!] TYPE STAND-IN + [header_request #4]. PostWorldInputBuffer::GetRaceCarCrashEventQueue()
    // (X360 0x8231D170, read-locked this+0x10) still returns the NAMED-OPAQUE placeholder
    // `GameStateModuleIO::RaceCarCrashEventQueue { u8 maOpaque[0x210]; }`, whose own header
    // comment says: "Swap for the real EventQueue<RaceCarCrashEvent,8> when that physics type is
    // homed." IT IS HOMED -- BrnScoringSystemEventQueues.h completes
    // `VehicleManagerOutputInterface::RaceCarCrashEventQueue : public EventQueue<RaceCarCrashEvent,8>`,
    // and that is the exact type ScoringSystem::UpdateCrashes (@0x8231F9B8) and
    // ChallengeManager::PostWorldUpdate already take. The cast below is a TYPE re-home of the same
    // bytes at the same offset -- no fabricated offset, no invented member -- and it disappears
    // the moment the accessor is retyped. Agent 7a's PostWorldUpdate needs the identical fix.
    const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpRaceCarCrashEventQueue =
        reinterpret_cast<const VehicleManagerOutputInterface::RaceCarCrashEventQueue*>(
            lpPostWorldInputBuffer->GetRaceCarCrashEventQueue());

    ProcessPlayerCrashes(lpRaceCarCrashEventQueue);
}

// The body proper, on the queue itself (see the overload note in BrnModeManager.h). The queue
// walk below is the console's, statement for statement; only the argument changed.
void ModeManager::ProcessPlayerCrashes(const CgsModule::BaseEventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent>* lpRaceCarCrashEventQueue)
{
    mbPlayerCrashedLastFrame = false;   // X360 `stb r31(=0), 0(this+0x950A)` -- BEFORE the scan

    // The X360 re-reads the queue length every iteration (`lwz r11, 8(r30)` @0x8231E6B0).
    for (s32 liCrashEvent = 0; liCrashEvent < lpRaceCarCrashEventQueue->GetLength(); ++liCrashEvent)
    {
        const BrnPhysics::Vehicle::RaceCarCrashEvent& lrCrashEvent =
            lpRaceCarCrashEventQueue->GetEvent(liCrashEvent);

        // X360 `lbz r11, 0x38(r3)` -- RaceCarCrashEvent +0x38 == mbIsPrimaryCrash
        // (VolumeInstanceId 8 + EntityId 8 + Vector3 16 + Vector3 16 + ETakedownType 4 + f32 4 == 56).
        if (!lrCrashEvent.mbIsPrimaryCrash)
        {
            continue;
        }

        // X360 `ld r11, 0(r3); srdi r11,r11,32; extrwi r11,r11,14,8` -- the 14-bit entity index
        // spliced into the crashing volume's embedded entity word, i.e.
        // VolumeInstanceId::GetEntityIDEntityIndex() (CgsVolumeInstanceId.h, whose own banner cites
        // this exact two-instruction fold). For a race-car entity that index IS the active
        // race-car slot, which is why the console compares it straight against
        // mePlayerActiveRaceCarIndex (`lwzx r10, r29, 0x8038`, a SIGNED `cmpw`).
        const s32 liCrashedRaceCarIndex =
            static_cast<s32>(lrCrashEvent.mRaceCarVolumeInstanceID.GetEntityIDEntityIndex());
        if (liCrashedRaceCarIndex == static_cast<s32>(mePlayerActiveRaceCarIndex))
        {
            mbPlayerCrashedLastFrame = true;   // X360 `stb r26(=1), 0(this+0x950A)`
        }
    }
}

// ============================================================================================
// CheckForOutOfRangeCarsReachingFinish  --  X360 0x82340800  (DWARF BrnModeManager.h:164)
//
// OFFLINE-RACE ONLY, IN-PROGRESS ONLY. A rival that is streamed out (out of range) fires no
// physics trigger, so it can never "cross" the finish line the normal way. This body is the
// rescue: any out-of-range car whose AI section matches the finish checkpoint's AI section is
// credited with the finish landmark exactly as if it had triggered it, through
// ModeManager::RaceCarTriggersLandmark.
//
// (i) NOT A STUNT-RACE PATH: the gate is `meCurrentGameModeType == E_MODE_OFFLINE_RACE` (0) and a
// stunt race is mode 7. Reconstructed for completeness of the PostWorldUpdate spine.
// ============================================================================================
void ModeManager::CheckForOutOfRangeCarsReachingFinish(const GameStateModuleIO::PostWorldInputBuffer* lpPostWorldInputBuffer)
{
    // [!] FETCH-PATH STAND-IN + [header_request #5]. The console reads the GLOBAL race-car output
    // interface OUT OF THE POST-WORLD INPUT BUFFER: sub_8231D368 is a read-locked accessor
    // returning `buffer + 0x9B40` (39744) whose assert cites BrnGameStateModuleIO.h:213 -- the
    // sibling of the :210 GetActiveRaceCarOutputInterface (+0x7250) and the :216
    // GetAICarOutputInterface (+0xAAC0) the tree already declares. PostWorldInputBuffer has NO such
    // accessor yet (+0x9B40 falls inside its mPostActiveCarOutputStorage filler), so the
    // manager-side embed is used instead -- the SAME interface type, fetched from
    // mpGameStateModule+0x3C0D0 rather than from this frame's snapshot buffer. Re-point at the
    // buffer accessor the moment header_request #5 lands.
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCarOutput =
        GetGlobalRaceCarOutputInterface();

    // X360 0x8231D410 (read-locked buffer + 0xAAC0, BrnGameStateModuleIO.h:216).
    const GameStateModuleIO::AICarOutputInterface* lpAICarOutput =
        lpPostWorldInputBuffer->GetAICarOutputInterface();

    // X360 `lwz r11, 0xD94(r27); cmpwi r11,0; bne -> exit`.
    if (meCurrentGameModeType != GameStateModuleIO::E_MODE_OFFLINE_RACE)
    {
        return;
    }
    // X360 `lwz r11, 0xD98(r27); lwz r11, 0x28(r11); cmpwi r11,2; bne -> exit`. The console does
    // NOT null-check mpCurrentGameMode here -- meCurrentGameModeType != E_MODE_NONE already
    // implies a live mode (hazards H3). Kept faithful.
    if (mpCurrentGameMode->GetCurrentState() != GameStateModuleIO::E_GMS_IN_PROGRESS)
    {
        return;
    }

    // ============================================================================
    // PARKED LEG + [header_request #6 and #7] -- the two declarations this body needs and the
    // frozen tree does not have. NOTHING here is guessed; both console reads are pinned:
    //
    //   (a) THE FINISH CHECKPOINT.  X360 `BrnGameState::CheckpointData,16>::GetItem(this+34272, 0)`
    //       @0x82340858 and again @0x823408CC. this+34272 == mCurrentGameModeParams (+33664) + 608,
    //       and the callee's own bounds assert reads the Array count at base+704 == 16 * 44 ==
    //       sizeof(CheckpointData) * 16 -- i.e. it is GameModeParams::maCheckpointDataArray,
    //       element 0. The two fields taken off it are:
    //           `lhz r24, 2(r3)`  -> CheckpointData +0x02 muAISectionIndex  (GetAISectionIndex())
    //           `lhz r29, 0(r11)` -> CheckpointData +0x00 muLandmarkIndex   (GetLandmarkIndex())
    //       GameModeParams declares maCheckpointDataArray PRIVATE and publishes only
    //       GetCheckpointCount(); its StartGameModeParams twin already publishes
    //       `const CheckpointData* GetCheckpointData(s32) const` (BrnGameModeParams.h:330).
    //       => header_request #6 adds the same accessor to GameModeParams.
    //
    //   (b) THE CREDIT CALL.  X360 `bl BrnGameState::ModeManager::RaceCarTriggersLandmark`
    //       @0x823408F8 with (this, buffer+0x7250 == GetActiveRaceCarOutputInterface(),
    //       leGlobalRaceCarIndex, leActiveRaceCarIndex, finishLandmarkIndex, false).
    //       ModeManager::RaceCarTriggersLandmark @0x82337258 is real (its own asserts name both
    //       index parameters, BrnModeManager.cpp:2573 / :2574) and has three console callers, but
    //       it is NOT declared in the frozen BrnModeManager.h and belongs to no agent this wave --
    //       it is also the only writer of the UNPLACED mauLastLandmarkHit[35] the header FLAGs at
    //       :562-565.  => header_request #7 declares it.
    //
    // Until both land, the scan below runs and names every console call it CAN make; the two
    // parked lines are the only thing missing. A one-for-one revive, not a rewrite:
    //
    //   const CheckpointData* lpFinishCheckpoint     = mCurrentGameModeParams.GetCheckpointData(0);
    //   const u16             luFinishAISectionIndex = lpFinishCheckpoint->GetAISectionIndex();
    //   ...
    //   if (luAISectionIndex == luFinishAISectionIndex)
    //   {
    //       RaceCarTriggersLandmark(lpPostWorldInputBuffer->GetActiveRaceCarOutputInterface(),
    //                               leGlobalRaceCarIndex, leActiveRaceCarIndex,
    //                               mCurrentGameModeParams.GetCheckpointData(0)->GetLandmarkIndex(),
    //                               false);
    //   }
    // ============================================================================

    for (::EGlobalRaceCarIndex leGlobalRaceCarIndex = ::E_GLOBAL_RACE_CAR_INDEX_0;
         leGlobalRaceCarIndex < ::E_GLOBAL_RACE_CAR_INDEX_COUNT;
         leGlobalRaceCarIndex++)
    {
        // X360 0x8231CAF8 -- only STREAMED-OUT cars are candidates.
        if (lpGlobalRaceCarOutput->IsInRange(leGlobalRaceCarIndex))
        {
            continue;
        }

        // X360 0x8230F888 -- the car's current AI section (u16), compared against the finish
        // checkpoint's AI section. (The comparison itself is parked -- see (a) above.)
        const u16 luAISectionIndex =
            lpAICarOutput->GetAISectionIndex(static_cast<s32>(leGlobalRaceCarIndex));
        (void)luAISectionIndex;

        // X360 0x821F46C8 -- the car's active slot; -1 means it is not in the event at all.
        const ::EActiveRaceCarIndex leActiveRaceCarIndex =
            lpGlobalRaceCarOutput->GetActiveRaceCarIndex(leGlobalRaceCarIndex);
        if (leActiveRaceCarIndex == ::E_ACTIVE_RACE_CAR_INDEX_INVALID)
        {
            continue;
        }
        (void)leActiveRaceCarIndex;

        // (parked: the finish-section match + the RaceCarTriggersLandmark credit -- banner above)
    }
    // [!] The console's loop-tail `leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT` assert
    // (BurnoutConstants.h:84, fired at 0x82340908) rides inside the inlined
    // `operator++(EGlobalRaceCarIndex&, int)` used above. The tree's global post-increment
    // (BurnoutConstants.h:67-72) is MISSING that CGS_ASSERT even though its EActiveRaceCarIndex
    // twin four lines below carries the matching one -- filed as header_request #8; once it is
    // added, this loop picks the assert up for free.
}

} // namespace BrnGameState
