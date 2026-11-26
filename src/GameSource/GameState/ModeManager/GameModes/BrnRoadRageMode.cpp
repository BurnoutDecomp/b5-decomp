#include "GameSource/GameState/ModeManager/GameModes/BrnRoadRageMode.h"
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/GameModes/BrnRoadRageMode.cpp"
#include <cmath>                                                        // std::fabs (the `fabs` in UpdateMaxActiveCars)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "SDKs/EATech/include/rw/math/vpu/scalar.h"                     // rw::math::vpu::Lerp(VecFloat, VecFloat, VecFloat)
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // CgsDev::Log::gpDebugPrint / CgsDev::Message::gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // GameActionQueue::AddEvent<T>, CgsModule::Event
#include "GameSource/GameState/BrnGameActions.h"                        // GameStateModuleIO::FinishedModeAction (slot 15)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // OutputBuffer::GetGameActionQueue / PreWorldInputBuffer::GetTimerStatusInterface
#include "GameSource/GameState/BrnGameStateSharedIO.h"                  // GameStateModuleIO::E_MODE_ROAD_RAGE / E_GMS_*
#include "GameSource/GameState/ModeManager/BrnModeManager.h"            // ModeManager::SetStartingGrid / GetRoadRageTakedownTarget
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // GameModeParams / StartGameModeParams
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"  // ScoringSystem + the embedded RoadRageModeScoring
#include "SharedClasses/Progression/BrnProgressionRankData.h"           // ProgressionRankData accessors

namespace BrnGameState
{
// ============================================================================================
// TU-LOCAL CONSTANTS AND RECORDS. Every recovered value is image-cited
// (scratch/postfx_step9_final/envfix/work/image.bin, offset = VA - 0x82000000, big-endian);
// nothing here is invented. NAMES come from the DWARF's file-scope constant list for this TU
// (dwarfdump .../BrnRoadRageMode.cpp:28-47, which carries the names but not the float values);
// the float pool at 0x820211FC..0x82021218 holds the values IN THAT SAME DECLARATION ORDER
// (threshold, max/min hidden time, early/late max madness, post-target increase, early/late
// time-to-max), which is what pins each name to its value.
// ============================================================================================
namespace
{
    // ---- Start @0x82330678 ------------------------------------------------------------------
    // DWARF BrnRoadRageMode.cpp:43/:44 (values in the DWARF). The console's rival-count chain is
    //     n = (s32)(rankRatio * 3.0f + 5.0f);  n = min(n, 7);  n = min(n, rank->RoadRageRivalsNumber)
    // 0x82330910 `lfs f0, flt_82020F90` (image.bin @0x20F90 == 40 40 00 00 == 3.0f) and
    // 0x82330914 `lfs f13, flt_82020F98` (@0x20F98 == 40 A0 00 00 == 5.0f), `fmadds`, `fctiwz`;
    // 0x8233090C `li r28, 7` is both the clamp and the RIVALS_AT_START value stored later.
    const s32 KI_MIN_ROAD_RAGE_RIVAL_COUNT = 5;
    const s32 KI_MAX_ROAD_RAGE_RIVAL_COUNT = 7;
    // The 3.0f multiplier. NAME NOT ATTESTED (the DWARF has no constant for it -- it is either an
    // inline literal or (MAX - MIN + 1) folded by the compiler); the VALUE is image-cited above.
    const f32 KF_RIVAL_COUNT_RANK_SPAN = 3.0f;

    // DWARF :29/:30 (values in the DWARF, both 7). RIVALS_AT_START is the Start-time value of
    // miNumberOfAllowedRoadRageRivals (`stw r28(7), 0xC0(r30)` @0x823309DC); RIVALS_AT_START_LINE
    // is UpdateMaxActiveCars' "not in progress" value (`li r11,7 / stw r11, 0xC0(r3)` @0x82315FD0).
    // Same value, two DWARF names; the assignment of which name goes where is by role.
    const s32 KI_ROAD_RAGE_RIVALS_AT_START      = 7;
    const s32 KI_ROAD_RAGE_RIVALS_AT_START_LINE = 7;

    // 0x82330A20 `oris r11,r11,0xC431 / ori r11,r11,0x4823` on the 64-bit muFlags word at
    // params+0x860. Spelled out against BrnGameModeParams.h's KU_FLAG_* table:
    //   0x00000001 SET_CARS_TO_START_GRID        0x00000002 REMOVE_RIVALS_FROM_WORLD
    //   0x00000020 WRAP_AI_CARS_WHEN_OUT_OF_RANGE 0x00000800 CLEAR_NEARBY_TRAFFIC
    //   0x00004000 AI_DRIVE_BY_START             0x00010000 SET_ALL_CARS_TO_STARTING_AI_CONTROL
    //   0x00100000 DISABLE_UPCOMING_ROAD_SIGNS   0x00200000 DISABLE_AFTERTOUCH_TDS
    //   0x04000000 ROLLING_START                 0x40000000 AI_PERSISTENT_DAMAGE
    //   0x80000000 AI_RESET_ON_TRACK_BEHIND
    // (their sum is exactly 0xC4314823.)
    const u64 KU_ROAD_RAGE_MODE_FLAGS =
        GameModeParams::KU_FLAG_SET_CARS_TO_START_GRID              |
        GameModeParams::KU_FLAG_REMOVE_RIVALS_FROM_WORLD            |
        GameModeParams::KU_FLAG_WRAP_AI_CARS_WHEN_OUT_OF_RANGE      |
        GameModeParams::KU_FLAG_CLEAR_NEARBY_TRAFFIC                |
        GameModeParams::KU_FLAG_AI_DRIVE_BY_START                   |
        GameModeParams::KU_FLAG_SET_ALL_CARS_TO_STARTING_AI_CONTROL |
        GameModeParams::KU_FLAG_DISABLE_UPCOMING_ROAD_SIGNS         |
        GameModeParams::KU_FLAG_DISABLE_AFTERTOUCH_TDS              |
        GameModeParams::KU_FLAG_ROLLING_START                       |
        GameModeParams::KU_FLAG_AI_PERSISTENT_DAMAGE                |
        GameModeParams::KU_FLAG_AI_RESET_ON_TRACK_BEHIND;

    // 0x823309AC..0x823309BC: `li r11,2 / li r8,5` -> +0x840 = 5, +0x844 = 2, +0x848 = 2, +0x84C = 2,
    // i.e. meDefaultPlayerRouteFindingStyle / meDefaultAIRouteFindingStyle /
    // meAISpeedSelectionMethod / miAIAggressiveCarCount (the private run pinned by
    // GameModeParams::Construct's zeroing, BrnGameModeParams.h banner). The BrnAI enums have no
    // committed home, so the raw ordinals are carried as the *_Stub storage types, exactly as the
    // committed PursuitMode::Start note describes.
    const s32 KI_ROAD_RAGE_PLAYER_ROUTE_FINDING_STYLE = 5;
    const s32 KI_ROAD_RAGE_AI_ROUTE_FINDING_STYLE     = 2;
    const s32 KI_ROAD_RAGE_AI_SPEED_SELECTION_METHOD  = 2;
    const s32 KI_ROAD_RAGE_AI_AGGRESSIVE_CAR_COUNT    = 2;

    // ---- UpdateMaxActiveCars @0x82315E58 / UpdateHiddenRivals @0x82315DC0 ----------------------
    // In DWARF declaration order (cpp:31..41), the order ARTIST's .rdata holds them from the anchor
    // 0x820211FC (UpdateMaxActiveCars addresses +0x1C/+0x18/+0x14/+0x10/+0x0C off it).
    // DWARF :31. 0x82315F40 `lfs f13, flt_820211FC`; image.bin @0x211FC == 3B 23 D7 0A == 0.0025f.
    const f32 KF_BROADCAST_MADNESS_TRESHOLD = 0.0025f;              // (sic -- the DWARF's spelling)
    // DWARF :33/:34. unk_82021200 == 3F 00 00 00 == 0.5f, unk_82021204 == 00 00 00 00 == 0.0f.
    // The hidden-time cap lerps from MAX (madness 0) down to MIN (madness 1).
    const f32 KF_MAX_HIDDEN_TIME = 0.5f;
    const f32 KF_MIN_HIDDEN_TIME = 0.0f;
    // DWARF :36/:37. unk_82021208 == 3D CC CC CD == 0.1f, unk_8202120C == 3F 80 00 00 == 1.0f.
    // The madness ceiling lerps from EARLY (rank ratio 0) to LATE (rank ratio 1).
    const f32 KF_EARLY_RANK_MAX_MADNESS = 0.1f;
    const f32 KF_LATE_RANK_MAX_MADNESS  = 1.0f;
    // DWARF :38. 0x82315F08 `lfs f11, flt_82021210`; @0x21210 == 3E CC CC CD == 0.4f. Per takedown
    // beyond the target, the ceiling rises by this much.
    const f32 KF_POST_TARGET_TAKEDOWN_MADNESS_INCREASE = 0.4f;
    // DWARF :40/:41. unk_82021214 == 41 F0 00 00 == 30.0f, unk_82021218 == 41 20 00 00 == 10.0f.
    // Seconds for the madness ratio to reach 1.0, lerped EARLY -> LATE on the rank ratio.
    const f32 KF_EARLY_TIME_TO_REACH_MAX_MADNESS = 30.0f;
    const f32 KF_LATE_TIME_TO_REACH_MAX_MADNESS  = 10.0f;

    // ---- OnPlayerInShortCut @0x823160A0 ------------------------------------------------------
    // DWARF :28. 0x823160A4 `lis r9, 0x40A0` -> 0x40A00000 == 5.0f stored into all eight timers.
    const f32 KF_HIDE_TIME_IF_PLAYER_IN_SHORTCUT = 5.0f;

    // ---- the one inbound game event (HandleGameEvents @0x82315FF8) --------------------------
    // [!] HEADER REQUEST (interim TU-local mirror). X360 0x8231600C `cmpwi cr6, r5, 0x28` (40).
    // The PS3 DWARF numbers E_EVENT_RACE_CAR_NEEDS_HIDING = 41, and this region of the game-EVENT
    // enum carries the -1 drift the committed BrnGameEvents.h already pins for 32/33/35/36
    // (PS3 33/34/36/37): 41 - 1 == 40, and the payload the handler reads -- `lwz r10, 0(r11)`
    // (car index) / `lfs f0, 4(r11)` (hidden time) -- is exactly the DWARF record
    // RaceCarNeedsHidingEvent { int32_t miActiveRaceCarIndex; f32 mfHiddenTime; }
    // (dwarfdump BrnGameEvents.h:371), which the debug string "<AI> Race car N has been hidden
    // for T seconds" corroborates.
    const s32 KI_EVENT_RACE_CAR_NEEDS_HIDING = 40;

    struct RaceCarNeedsHidingEventRecord
    {
        s32 miActiveRaceCarIndex;   // +0x00
        f32 mfHiddenTime;           // +0x04
    };

}

// X360: BrnGameState::RoadRageMode::GetName (0x827E24D8). Trivial virtual override of
// GameMode::GetName; returns the mode's fixed name string. The virtual/trailing-const shape is
// from the DWARF declaration (the Hex-Rays pseudocode renders it as a plain function and drops const).
const char* RoadRageMode::GetName() const
{
    return "RoadRage";
}

// X360 vtable slot 13 (vtbl+52), folded leaf 0x827E2F38 == `li r3,0; blr` at slot 13 of
// RoadRageMode's vtable 0x820D05E8 (the offline base carries GameMode::ShouldExit 0x82315B80
// there instead). Road rage ends on its own takedown/timer conditions -- ShouldFinish, slot 14 --
// never on the shared idle-exit test.
bool RoadRageMode::ShouldExit(const ScoringSystem* lpScoringSystem) const
{
    (void)lpScoringSystem;
    return false;
}

// X360 vtable slot 23 (vtbl+92), folded leaf 0x827E2F38 == `li r3,0; blr` (the base is
// 0x82C296C8 == `li r3,1`). SetupGameMode @0x8234B158 gates the streaming wait on this.
bool RoadRageMode::RequiresStreaming() const
{
    return false;
}

// ============================================================================================
// RoadRageMode::Start -- X360 0x82330678 (vtable slot 5, vtbl+20)
// ============================================================================================
// Builds the mutable GameModeParams for a road rage out of the immutable StartGameModeParams plus
// the per-rank tuning record, seats the grid, installs the takedown target / time limit and resets
// the mode's own broadcast state. Reconstructed from the ASSEMBLY.
//
// REGISTER MAP (asm 0x82330688..0x82330690): r30 = this, r26 = lpStartGameModeParams,
// r31 = lpGameModeParams, r24 = lpProgressionRankData (loaded from start+0x334). r6 -- the
// ScoringSystem* the base slot passes -- is NEVER READ, which is why the parameter is unused below.
//
// CONSOLE OFFSET -> NAME, all re-derived this pass:
//   StartGameModeParams  +0x318 mfTrafficDensity      +0x328 muEventJunctionId   +0x330 muJunctionID
//                        +0x334 mpProgressionRankData +0x338 mfProgressionRankAsRatio
//   ProgressionRankData  +0x0C mfTrafficDensityRoadRage  +0x24 mfLargeVehicleProbability
//                        +0x2C maOvertakingDifficulty[8] +0x52 muRoadRageTime  +0x5E muRoadRageRivalsNumber
//   GameModeParams       +0x00 miNumRivals   +0x01 miNumNetworkPlayers   +0x04 mfProgressionRankAsRatio
//                        +0x30 mfTrafficDensityScale  +0x34 mfLargeVehicleProbability
//                        +0x40 mTrafficLightTriggerId +0x44 muEventJunctionID  +0x48 muJunctionID
//                        +0x4C miRoadRageThreshold    +0x60 mfNeedForBronze    +0x68 mfNeedForGold
//                        +0x6C mfModeTimeLimit        +0x74 mfOvertakingDifficulty[8]
//                        +0x840/+0x844/+0x848/+0x84C the private AI-style quartet
//                        +0x858 miPlayerWreckCount    +0x860 muFlags
// (The +0x60/+0x68/+0x6C run is the {bronze, silver, gold, time limit} quartet pinned in the
// committed StuntAttackMode::Start banner from GameModeParams::Construct + BurningRouteMode::Start.)
//
// The three `gxMessageFilterFlags & 1` debug prints (0x82330728..0x8233087C) are KEPT: the
// CgsDev::Log stream vocabulary is wired (BrnModeManager_CheckpointSetup.cpp uses the identical
// `*gpDebugPrint << "text" << f32 << "\n"` form), and each print's virtual operator<<(const char*)
// / operator<<(f32) pair is what the asm dispatches through vtbl+4 / sub_821F0F40.
// ============================================================================================
void RoadRageMode::Start(const StartGameModeParams* lpStartGameModeParams,
                         GameModeParams*            lpGameModeParams,
                         ScoringSystem*             /*lpScoringSystem*/)
{
    // 0x82330694..0x823306C4 -- the inlined StartGameModeParams::GetProgressionRankData guard
    // (BrnGameModeParams.h:945 on the console), restated because the console restates it.
    CGS_ASSERT(lpStartGameModeParams->GetProgressionRankData() != 0, "mpProgressionRankData != NULL");

    // 0x823306C8..0x823306F4 (BrnRoadRageMode.cpp:72 on the console).
    const BrnProgression::ProgressionRankData* lpProgressionRankData =
        lpStartGameModeParams->GetProgressionRankData();
    CGS_ASSERT_AT(lpProgressionRankData != 0, "lpProgressionRankData != NULL", CGS_ARTIST_FILE, 72);
    // 0x823306F8..0x82330718 (:73).
    CGS_ASSERT_AT(lpGameModeParams != 0, "lpGameModeParams != NULL", CGS_ARTIST_FILE, 73);

    // 0x8233071C..0x82330724 `li r4, 3` == E_MODE_ROAD_RAGE (a literal, not the start params' type).
    lpGameModeParams->Construct(GameStateModuleIO::E_MODE_ROAD_RAGE);

    // 0x82330728..0x823307DC -- the two density prints.
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "lpProgressionRankData->GetTrafficDensityRoadRage() :     "
                                   << lpProgressionRankData->GetTrafficDensityRoadRage() << "\n";
    }
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "lpStartGameModeParams->GetTrafficDensity() :             "
                                   << lpStartGameModeParams->GetTrafficDensity() << "\n";
    }

    // 0x823307E0..0x823307EC `lfs 0x318(r26) * lfs 0xC(r24) -> stfs 0x30(r31)`.
    lpGameModeParams->SetTrafficDensityScale(lpStartGameModeParams->GetTrafficDensity() *
                                             lpProgressionRankData->GetTrafficDensityRoadRage());

    // 0x823307F0..0x82330810 -- the inlined GetProgressionRankAsRatio guard (BrnGameModeParams.h:960).
    CGS_ASSERT(lpStartGameModeParams->GetProgressionRankData() != 0, "mpProgressionRankData != NULL");
    // 0x82330814..0x8233081C -- ONE load, TWO stores: the mode's own copy (+0xFC) and the params'.
    mfProgressionRankAsRatio = lpStartGameModeParams->GetProgressionRankAsRatio();
    lpGameModeParams->SetProgressionRankAsRatio(mfProgressionRankAsRatio);

    // 0x82330820..0x82330870 -- the third print.
    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "lpProgressionRankData->GetLargeVehicleProbability() :    "
                                   << lpProgressionRankData->GetLargeVehicleProbability() << "\n";
    }

    // 0x82330874..0x8233087C.
    lpGameModeParams->SetLargeVehicleProbability(lpProgressionRankData->GetLargeVehicleProbability());

    // 0x82330878..0x823308A0 -- the 8-float copy from rankData+0x2C into params+0x74, emitted as a
    // counted loop here (`li r11,8 / addi r11,r11,-1 / bne`). That IS
    // ProgressionRankData::GetOvertakingDifficulty (DWARF :192), same de-inlining as the siblings.
    lpProgressionRankData->GetOvertakingDifficulty(lpGameModeParams->mfOvertakingDifficulty);

    // 0x823308A4..0x823308B8.
    lpGameModeParams->muEventJunctionID = lpStartGameModeParams->GetEventJunctionId();
    lpGameModeParams->muJunctionID      = lpStartGameModeParams->GetJunctionID();

    // 0x823308BC..0x823308D4 -- the trigger-light id (an out-of-line call returning by pointer)
    // and the 64-bit read-modify-write of the flag word, interleaved by the scheduler.
    lpGameModeParams->SetTrafficLightTriggerId(lpStartGameModeParams->GetTrafficLightTriggerId());
    lpGameModeParams->SetFlag(KU_ROAD_RAGE_MODE_FLAGS);

    // 0x823308D8..0x823308F8 -- the GetProgressionRankAsRatio guard again (:960), restated.
    CGS_ASSERT(lpStartGameModeParams->GetProgressionRankData() != 0, "mpProgressionRankData != NULL");

    // 0x823308FC..0x82330950 -- the rival count: rank-scaled, clamped to the mode ceiling, then to
    // the rank record's own rivals number. Written as the three console stores (the middle one is
    // conditional) so each clamp is visible; `fctiwz` is the truncating cast.
    miNumberOfRivals = static_cast<s32>(lpStartGameModeParams->GetProgressionRankAsRatio() * KF_RIVAL_COUNT_RANK_SPAN +
                                        static_cast<f32>(KI_MIN_ROAD_RAGE_RIVAL_COUNT));
    if (miNumberOfRivals > KI_MAX_ROAD_RAGE_RIVAL_COUNT)
    {
        miNumberOfRivals = KI_MAX_ROAD_RAGE_RIVAL_COUNT;
    }
    // `lbz r11, 0x5E(r24)` (unsigned byte) vs `lwz 0xC4` with a SIGNED `cmpw`; the smaller wins.
    const s32 liRankRoadRageRivals = static_cast<s32>(lpProgressionRankData->GetRoadRageRivalsNumber());
    if (miNumberOfRivals >= liRankRoadRageRivals)
    {
        miNumberOfRivals = liRankRoadRageRivals;
    }

    // 0x82330954..0x82330974. `extsb r11` of the count -> `stb 0(r31)` (miNumRivals is an s8),
    // `stb r29(0), 1(r31)`, then the grid is seated for rivals + the player with
    // lbPushForwards == `li r6, 0`.
    lpGameModeParams->SetNumRivals(miNumberOfRivals);
    lpGameModeParams->miNumNetworkPlayers = 0;
    GetModeManager()->SetStartingGrid(lpGameModeParams, miNumberOfRivals + 1, false);

    // 0x82330978..0x8233097C -- the rank's takedown target (u32).
    const u32 luTakedownTarget = GetModeManager()->GetRoadRageTakedownTarget();

    // 0x82330984..0x823309A4 `lfs f0, 0x60(r31) ; fctiwz ; stfiwx -> 0x4C(r31)`: the road-rage
    // threshold is the TRUNCATED bronze threshold as it stands in the params at this point (i.e.
    // whatever Construct left there). Read BEFORE the +0x68 store below, exactly as scheduled.
    lpGameModeParams->miRoadRageThreshold = static_cast<s32>(lpGameModeParams->mfNeedForBronze);

    // 0x823309AC..0x823309BC -- the private AI-style quartet.
    lpGameModeParams->SetDefaultPlayerRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(KI_ROAD_RAGE_PLAYER_ROUTE_FINDING_STYLE));
    lpGameModeParams->SetDefaultAIRouteFindingStyle(static_cast<ERouteFindingStyle_Stub>(KI_ROAD_RAGE_AI_ROUTE_FINDING_STYLE));
    lpGameModeParams->SetAISpeedSelectionMethod(static_cast<EAISpeedSelMethod_Stub>(KI_ROAD_RAGE_AI_SPEED_SELECTION_METHOD));
    lpGameModeParams->SetAIAggresiveCarCount(KI_ROAD_RAGE_AI_AGGRESSIVE_CAR_COUNT);

    // 0x82330980 `clrldi r9, r3, 32` / 0x8233099C `std` / 0x823309A8 `lfd` / `fcfid` / `frsp` /
    // 0x823309C4 `stfs -> 0x68(r31)`: the unsigned target, widened to u64, converted to float.
    lpGameModeParams->mfNeedForGold = static_cast<f32>(static_cast<u64>(luTakedownTarget));

    // 0x823309C8..0x823309D0. The console passes r4 = lpStartGameModeParams; the committed base
    // keeps the no-arg shape (see the SHAPE NOTE on GameMode::CalculateMaxPlayerWrecks), so the
    // argument is not threaded here -- the store target is the same.
    lpGameModeParams->SetPlayerWreckCount(CalculateMaxPlayerWrecks(lpStartGameModeParams));   // r4 = start params (0x82330A0C)

    // 0x823309D8..0x823309DC.
    miNumberOfTransmittedRivals     = 0;
    miNumberOfAllowedRoadRageRivals = KI_ROAD_RAGE_RIVALS_AT_START;

    // 0x823309E0..0x823309F0 -- `lfs f0, flt_82001CC0` (== 0.0f) into the three madness floats.
    mfRoadRageMadnessRatio  = 0.0f;
    mfRoadRageMadness       = 0.0f;
    mfMadnessBroadcastLevel = 0.0f;

    // 0x823309F4..0x82330A10 `lhz r10, 0x52(r24) / extsw / std / lfd / fcfid / frsp / stfs 0x6C(r31)`:
    // the rank's road-rage time (u16 seconds) becomes the mode time limit.
    lpGameModeParams->mfModeTimeLimit = static_cast<f32>(lpProgressionRankData->GetRoadRageTime());

    // 0x82330A14..0x82330A18 -- both broadcast latches armed for the first frame.
    mbUpdateRivals       = true;
    mbUpdateMadnessLevel = true;

    // 0x82330A1C..0x82330A28 -- `mtctr 8` fill of the eight hidden timers with r29 == 0.
    for (s32 liRival = 0; liRival < 8; ++liRival)
    {
        mfHiddenTime[liRival] = 0.0f;
    }
}

// ============================================================================================
// RoadRageMode::PreWorldUpdate -- X360 0x823448C0 (vtable slot 2, vtbl+8)
// ============================================================================================
// REGISTER MAP (asm 0x823448CC..0x823448D8): r31 = this, r28 = lpOutput, r30 = lpInput,
// r29 = lpScoringSystem (r9). The base call at 0x823448DC receives r3..r9 untouched.
// ============================================================================================
void RoadRageMode::PreWorldUpdate(GameStateModuleIO::OutputBuffer* lpOutput,
                                  const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                  const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCars,
                                  const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCars,
                                  bool lbPaused,
                                  const ScoringSystem* lpScoringSystem)
{
    // 0x823448DC -- the base, all six arguments forwarded.
    GameMode::PreWorldUpdate(lpOutput, lpInput, lpGlobalRaceCars, lpActiveRaceCars, lbPaused, lpScoringSystem);

    // 0x823448E0..0x823448E4 PreWorldInputBuffer::GetTimerStatusInterface (0x8231CE28), then
    // 0x823448E8..0x823448F4 `lwz r11, 0x28(r31) ; cmpwi 2` -- GameMode::meCurrentState ==
    // E_GMS_IN_PROGRESS gates both updates.
    // DWARF local lpSimTimerStatus (cpp:459); ARTIST reads the step product afresh for each call
    // (0x823448F8..0x82344918), [+0x1C] * [+0x20] = base step * multiplier.
    const CgsSystem::TimerStatus* lpSimTimerStatus = lpInput->GetTimerStatusInterface()->GetSimTimerStatus();
    if (GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS)
    {
        // 0x823448FC `addi r5, r29, 0x4B40` == &lpScoringSystem->mRoadRageModeScoring.
        UpdateMaxActiveCars(lpSimTimerStatus->GetCurrentTimeStep(), lpScoringSystem->GetRoadRageScoring());
        UpdateHiddenRivals(lpSimTimerStatus->GetCurrentTimeStep());
    }

    // 0x82344920..0x82344928.
    BroadcastEventsToRivals(lpOutput);
}

// ============================================================================================
// RoadRageMode::ShouldFinish -- X360 0x82315D60 (vtable slot 14, vtbl+56)
// ============================================================================================
//   lfs  f13, 0x5CF4(r4) ; scoring +23796 == mfPlayerTimeWithoutInput   (GetPlayerNoInputTime)
//   lfs  f0,  flt_820211D4 (4.0f) ; blt -> return 0
//   lfs  f13, 0x5CF0(r4) ; scoring +23792 == mfPlayerTimeStationary     (GetPlayerStationaryTime)
//   lfs  f0,  flt_82020F90 (3.0f) ; blt -> return 0
//   lfs  f0,  flt_82001CC0 (0.0f) ; stfs 0xD0 ; stb 0xF4 ; stfs 0xCC ; stb 0xF5 ; stfs 0xF8 ; return 1
// The `blt` form is kept (a NaN timer would fall through to "finish", as on the console). Note
// the ScoringSystem is NOT written here (unlike the stunt sibling) -- only the mode's own members.
// ============================================================================================
bool RoadRageMode::ShouldFinish(ScoringSystem* lpScoringSystem)
{
    // 0x82315D6C `lfs f0, flt_820211D4` (4.0f) and 0x82315D80 `lfs f0, flt_82020F90` (3.0f) -- the
    // same two idle thresholds GameMode::ShouldExit and StuntAttackMode::ShouldFinish declare locally.
    const f32 KF_MAX_NO_INPUT_TIME_FOR_MODE_FINISH   = 4.0f;
    const f32 KF_MAX_STATIONARY_TIME_FOR_MODE_FINISH = 3.0f;

    if (lpScoringSystem->GetPlayerNoInputTime()    < KF_MAX_NO_INPUT_TIME_FOR_MODE_FINISH ||
        lpScoringSystem->GetPlayerStationaryTime() < KF_MAX_STATIONARY_TIME_FOR_MODE_FINISH)
    {
        return false;
    }

    mfRoadRageMadnessRatio  = 0.0f;
    mbUpdateRivals          = true;
    mfRoadRageMadness       = 0.0f;
    mbUpdateMadnessLevel    = true;
    mfMadnessBroadcastLevel = 0.0f;
    return true;
}

// ============================================================================================
// RoadRageMode::SendEvent -- X360 0x82330A38 (vtable slot 12, vtbl+48)
// ============================================================================================
//   lwz r11, 0x28(r3) ; cmpwi 2 ; bne base       -- meCurrentState == E_GMS_IN_PROGRESS
//   cmpwi r4, 1 ; bne base                       -- leEvent == E_GME_NEXT
//   li r4, 4 ; b GameMode::SetCurrentState       -- E_GMS_RESULTS (a road rage skips the outro)
//   base: b GameMode::SendEvent
// ============================================================================================
void RoadRageMode::SendEvent(EGameModeEvent leEvent)
{
    if (GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS && leEvent == E_GME_NEXT)
    {
        SetCurrentState(GameStateModuleIO::E_GMS_RESULTS);
        return;
    }
    GameMode::SendEvent(leEvent);
}

// ============================================================================================
// RoadRageMode::HandleGameEvents -- X360 0x82315FF8 (vtable slot 22, vtbl+88)
// ============================================================================================
// Only event 40 (RACE_CAR_NEEDS_HIDING on this build) is handled:
//   lwz r10, 0(r11) ; lfs f0, 4(r11) ; addi r10, r10, 0x35 ; slwi 2 ; stfsx f0, r10, r3
//     -> mfHiddenTime[miActiveRaceCarIndex] = mfHiddenTime   ((idx + 53) * 4 == 212 + idx*4)
//   stb 1 -> 0xF4                                             -> mbUpdateRivals
//   then the gated "<AI> Race car N has been hidden for T seconds" print.
// [!] The console does NOT range-check the car index before the store (no compare between the
// `lwz` and the `stfsx`); reproduced as shipped, since a guard here would be invented behaviour.
// ============================================================================================
void RoadRageMode::HandleGameEvents(const CgsModule::Event* lpEvent, s32 liEventType)
{
    if (liEventType != KI_EVENT_RACE_CAR_NEEDS_HIDING)
    {
        return;
    }

    const RaceCarNeedsHidingEventRecord* lpHidingEvent =
        reinterpret_cast<const RaceCarNeedsHidingEventRecord*>(lpEvent);

    mfHiddenTime[lpHidingEvent->miActiveRaceCarIndex] = lpHidingEvent->mfHiddenTime;
    mbUpdateRivals = true;

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
    {
        *CgsDev::Log::gpDebugPrint << "<AI> Race car " << lpHidingEvent->miActiveRaceCarIndex
                                   << " has been hidden for " << lpHidingEvent->mfHiddenTime
                                   << " seconds\n";
    }
}

// ============================================================================================
// RoadRageMode::OnPlayerInShortCut -- X360 0x823160A0 (vtable slot 10, vtbl+40)
// ============================================================================================
//   addi r11, r3, 0xD4 ; lis r9, 0x40A0 ; mtctr 8 ; stw r9 ; addi 4 ; bdnz   -> all eight = 5.0f
//   li r11, 1 ; stb r11, 0xF4(r3)                                             -> mbUpdateRivals
// ============================================================================================
void RoadRageMode::OnPlayerInShortCut()
{
    for (s32 liRival = 0; liRival < 8; ++liRival)
    {
        mfHiddenTime[liRival] = KF_HIDE_TIME_IF_PLAYER_IN_SHORTCUT;
    }
    mbUpdateRivals = true;
}

// ============================================================================================
// RoadRageMode::FillInGameModeSpecificResults -- X360 0x82315D40 (vtable slot 15, vtbl+60)
// ============================================================================================
//   lwz r11, 0x4B40(r4) ; lwz r10, 0x4B4C(r4) ; cmpw ; li r11,1 ; bge skip ; li r11,2 ; stw r11, 0x24(r5)
// ss+0x4B40 is the embedded RoadRageModeScoring (BrnScoringSystem.h:466 pins it), whose +0x00 /
// +0x0C are miNumTakedownsAchieved / miTargetNumTakedowns (s32, s32, s16, u16, s32 -- the DWARF
// run in BrnRoadRageModeScoring.h). +0x24 on the action is FinishedModeAction::miFinishPosition.
// ============================================================================================
void RoadRageMode::FillInGameModeSpecificResults(const ScoringSystem* lpScoringSystem,
                                                 GameStateModuleIO::FinishedModeAction* lpAction)
{
    const RoadRageModeScoring* lpRoadRageScoring = lpScoringSystem->GetRoadRageScoring();

    // ARTIST 0x82315D4C..0x82315D54 (li 1 ; bge ; li 2): the reached-target arm first.
    if (lpRoadRageScoring->GetNumTakedownsAchieved() >= lpRoadRageScoring->GetTargetNumTakedowns())
    {
        lpAction->miFinishPosition = 1;
    }
    else
    {
        lpAction->miFinishPosition = 2;
    }
}

// ============================================================================================
// RoadRageMode::UpdateMaxActiveCars -- X360 0x82315E58 (non-virtual, DWARF BrnRoadRageMode.h:139)
// ============================================================================================
// r3 = this, f1 = lfDeltaTime, r5 = lpRoadRageScoring. Three de-SIMD'd lerps -- the console
// splats a scalar pair into VMX, `vsubfp` / `vmaddfp`, and reads one lane back through the stack
// (`stvx128 -> lfs back_chain`). The export lists `vmaddfp vD, vA, vB, vC` (encoding order) == vA*vC + vB
// with vA = (to - from) from the preceding `vsubfp`, vB = from (3rd operand), vC = t (4th), i.e. Lerp(from, to, t);
// all four lerps in this TU decode consistently under that one rule:
//   1. 0x82315E78..0x82315EA0  Lerp(30.0, 10.0, mfProgressionRankAsRatio)  -> seconds to max madness
//   2. 0x82315EC4..0x82315EF8  Lerp(0.1,  1.0,  mfProgressionRankAsRatio)  -> max madness
//   3. 0x82315F6C..0x82315FB8  Lerp(7.0,  n+1,  mfRoadRageMadnessRatio)    -> allowed rivals
// The two `fsel f, (1.0 - x), x, 1.0` sequences are min(x, 1.0).
// ============================================================================================
// DWARF locals lfTimeToReachMaxMadness / liExcessTakedowns / lfMaxMadness (cpp:393/394/400); the
// call list is the SDK's VecFloat Lerp (x3, each read back through VecFloat::operator float),
// Min<float> (x2, `fsel` on 1.0 - x: Min(1.0f, x)) and Abs<float>.
void RoadRageMode::UpdateMaxActiveCars(f32 lfTimeStep, const RoadRageModeScoring* lpRoadRageScoring)
{
    f32 lfTimeToReachMaxMadness = rw::math::vpu::Lerp(KF_EARLY_TIME_TO_REACH_MAX_MADNESS,
                                                            KF_LATE_TIME_TO_REACH_MAX_MADNESS,
                                                            mfProgressionRankAsRatio);
    mfRoadRageMadnessRatio = rw::math::vpu::Min(1.0f, lfTimeStep / lfTimeToReachMaxMadness + mfRoadRageMadnessRatio);

    // 0x82315EC8 `lwz r9, 0(r5)` / 0x82315EE0 `lwz r10, 0xC(r5)` / `subf` -- achieved - target, SIGNED.
    int32_t liExcessTakedowns =
        lpRoadRageScoring->GetNumTakedownsAchieved() - lpRoadRageScoring->GetTargetNumTakedowns();

    f32 lfMaxMadness = rw::math::vpu::Lerp(KF_EARLY_RANK_MAX_MADNESS, KF_LATE_RANK_MAX_MADNESS,
                                                 mfProgressionRankAsRatio);
    if (liExcessTakedowns > 0)
    {
        lfMaxMadness = rw::math::vpu::Min(1.0f, static_cast<f32>(liExcessTakedowns) * KF_POST_TARGET_TAKEDOWN_MADNESS_INCREASE + lfMaxMadness);
    }

    mfRoadRageMadness = mfRoadRageMadnessRatio * lfMaxMadness;
    if (rw::math::vpu::Abs(mfRoadRageMadness - mfMadnessBroadcastLevel) > KF_BROADCAST_MADNESS_TRESHOLD)
    {
        mfMadnessBroadcastLevel = mfRoadRageMadness;
        mbUpdateMadnessLevel    = true;
    }

    // In progress: lerp from the rival-count ceiling (DWARF KI_MAX_ROAD_RAGE_RIVAL_COUNT, cpp:44, as a
    // float temporary: ARTIST 0x82315F7C..0x82315F88 spills the pooled 7.0 for the VecFloat) to the
    // rivals + 1 on the madness ratio, truncated (fctiwz/stfiwx); otherwise the start-line count.
    if (GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS)
    {
        miNumberOfAllowedRoadRageRivals = static_cast<int32_t>(static_cast<float>(
            rw::math::vpu::Lerp(static_cast<f32>(KI_MAX_ROAD_RAGE_RIVAL_COUNT),
                                static_cast<f32>(miNumberOfRivals + 1), mfRoadRageMadnessRatio)));
    }
    else
    {
        miNumberOfAllowedRoadRageRivals = KI_ROAD_RAGE_RIVALS_AT_START_LINE;
    }

    // 0x82315FD8..0x82315FEC -- a changed allowance is what the rivals need to hear about.
    if (miNumberOfAllowedRoadRageRivals != miNumberOfTransmittedRivals)
    {
        mbUpdateRivals              = true;
        miNumberOfTransmittedRivals = miNumberOfAllowedRoadRageRivals;
    }
}

// ============================================================================================
// RoadRageMode::UpdateHiddenRivals -- X360 0x82315DC0 (non-virtual, DWARF BrnRoadRageMode.h:143)
// ============================================================================================
// r3 = this, f1 = lfDeltaTime. One de-SIMD'd lerp (0x82315DD8..0x82315E00, the same vsubfp /
// vmaddfp shape as UpdateMaxActiveCars): Lerp(0.5, 0.0, mfRoadRageMadness) -- the cap on how long
// a rival may stay hidden shrinks to nothing as the madness rises. Then the eight-timer loop
// (`li r9, 8` counted down):
//   lfs f0, 0(r11) ; fcmpu 0.0 ; ble next                -- only armed timers tick
//   fcmpu f0, cap ; ble ; stfs cap                       -- clamp DOWN to the cap first
//   lfs ; fsubs f1 ; stfs                                -- then subtract the frame delta
//   fcmpu 0.0 ; bgt next ; stb 1 -> 0xF4                 -- expired -> mbUpdateRivals
// ============================================================================================
// DWARF locals liIndex (cpp:288) and float lfMaxHiddenTime (cpp:290); its call list is the SDK's
// rw::math::vpu::Lerp over VecFloat with VecFloat::operator float (ARTIST lvlx/vspltw, vsubfp,
// vmaddfp, stvx and lfs back, 0x82315DD8..0x82315E08).
void RoadRageMode::UpdateHiddenRivals(f32 lfTimeStep)
{
    int32_t liIndex;
    float   lfMaxHiddenTime = rw::math::vpu::Lerp(KF_MAX_HIDDEN_TIME, KF_MIN_HIDDEN_TIME, mfRoadRageMadness);

    for (liIndex = 0; liIndex < 8; liIndex++)
    {
        if (mfHiddenTime[liIndex] > 0.0f)
        {
            if (mfHiddenTime[liIndex] > lfMaxHiddenTime)
            {
                mfHiddenTime[liIndex] = lfMaxHiddenTime;
            }
            mfHiddenTime[liIndex] -= lfTimeStep;
            if (mfHiddenTime[liIndex] <= 0.0f)
            {
                mbUpdateRivals = true;
            }
        }
    }
}

// ============================================================================================
// RoadRageMode::BroadcastEventsToRivals -- X360 0x82344798 (non-virtual, DWARF BrnRoadRageMode.h:141)
// ============================================================================================
// r30 = this, r28 = lpOutput. Two mutually-exclusive arms (`lbz 0xF4 ; beq -> lbz 0xF5`), each a
// loop over rivals 0 .. miNumberOfRivals INCLUSIVE (`lwz 0xC4 ; addic. r11, r11, 1 ; cmpw r31, r11
// ; blt`), each posting one 8-byte action per rival through OutputBuffer::GetGameActionQueue
// (0x8231D4B8, the `Ou` the export truncates) and VariableEventQueue<13312,16>::AddEvent:
//   mbUpdateRivals arm (0x823447BC..0x8234483C): action 129 {rival, allowed}, where allowed ==
//     (mfHiddenTime[rival] <= 0.0f) && (rival < miNumberOfAllowedRoadRageRivals)  -- the console's
//     `fcmpu ; ble -> (cmpw ; blt -> li 1 / else 0) ; else stb 0`; then the latch is cleared.
//   mbUpdateMadnessLevel arm (0x8234484C..0x823448AC): action 131 {rival, mfRoadRageMadness};
//     then mfMadnessBroadcastLevel = mfRoadRageMadness and the latch is cleared.
// The loop bound is re-read from the member each iteration on the console; miNumberOfRivals is not
// written inside either loop, so a hoisted bound is the same program.
// ============================================================================================
// DWARF (BrnGameStateScoringAndModesUnity): liIndex (cpp:332) and lAllowToJoin (cpp:336, inside the
// loop) in the first arm; lUpdateRoadRageMadnessAction (cpp:363) declared before its loop's liIndex
// (cpp:365) in the second; both post through AddGameAction<T> (ids 129 / 131, 8 bytes each).
void RoadRageMode::BroadcastEventsToRivals(GameStateModuleIO::OutputBuffer* lpOutput)
{
    if (mbUpdateRivals)
    {
        for (int32_t liIndex = 0; liIndex < miNumberOfRivals + 1; liIndex++)
        {
            GameStateModuleIO::AllowCarToJoinRoadRageAction lAllowToJoin;
            lAllowToJoin.mActiveRaceCarIndex = static_cast<::EActiveRaceCarIndex>(liIndex);
            // A still-hidden rival is not allowed (ARTIST 0x823447E8 `ble`, not-greater, to the count
            // test; the false store falls through).
            if (mfHiddenTime[liIndex] > 0.0f)
            {
                lAllowToJoin.mbAllowedInRoadRage = false;
            }
            else
            {
                lAllowToJoin.mbAllowedInRoadRage = liIndex < miNumberOfAllowedRoadRageRivals;
            }

            lpOutput->GetGameActionQueue()->AddGameAction(lAllowToJoin);
        }
        mbUpdateRivals = false;
    }
    else if (mbUpdateMadnessLevel)
    {
        GameStateModuleIO::UpdateRoadRageMadnessAction lUpdateRoadRageMadnessAction;
        for (int32_t liIndex = 0; liIndex < miNumberOfRivals + 1; liIndex++)
        {
            lUpdateRoadRageMadnessAction.mActiveRaceCarID  = static_cast<::EActiveRaceCarIndex>(liIndex);
            lUpdateRoadRageMadnessAction.mRoadRageMadnesss = mfRoadRageMadness;

            lpOutput->GetGameActionQueue()->AddGameAction(lUpdateRoadRageMadnessAction);
        }
        mfMadnessBroadcastLevel = mfRoadRageMadness;
        mbUpdateMadnessLevel    = false;
    }
}
}

#undef CGS_ARTIST_FILE
