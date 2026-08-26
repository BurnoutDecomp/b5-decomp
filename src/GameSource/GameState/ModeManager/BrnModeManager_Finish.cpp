// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_Finish.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// Wave-B keystone, AGENT 8 -- the finish / results half of the mode spine:
//
//   ModeManager::GetCountdownTimeForMode   X360 0x82327D10
//   ModeManager::GetPlayersFinishPosition  X360 0x823281E8
//   ModeManager::SendFinishedModeAction    X360 0x82343628
//   ModeManager::FinishCurrentMode         X360 0x8234B978
//   ModeManager::ShowModeResults           X360 0x823436D0   (THE progression payoff)
//
// Every field offset, action id, payload size and rodata constant below is quoted from the
// ASSEMBLY of those five exports, not from the Hex-Rays pseudocode. The pseudocode for
// ShowModeResults in particular renders the 232-byte stack record as a `_QWORD v42[30]` and
// then names its bytes with a MIXTURE of little- and big-endian sub-field conventions
// (BYTE4(v42[28]) lands on +0xE4 while LOBYTE(v42[27]) lands on +0xDF); every offset here was
// re-derived from the `stw/stb/stfs ... 0x1E0+var_NNN(r1)` stack displacements against the
// record base (var_180 == the memset destination == the AddEvent payload pointer).
//
// [X] hazards H2: the 16 committed BrnModeManager.cpp bodies are CALLED, never re-implemented.
//     This file calls SendModeResults (:347), HasPlayerWon (:187) and GetPlayersFinishPosition
//     (bodied HERE -- it is agent 8's, listed in the H2 sheet only because HasPlayerWon calls it).
//
// [X][X] LINK FRONTIER THIS FILE INTRODUCES (added 2026-08-26, fix round -- it was previously
//        unreported, which is why it is spelled out here rather than only in a report). Two callees
//        below are DECLARE-ONLY tree-wide; each is an LNK2019 the moment this partfile mounts:
//   1. ScoringSystem::HasBeatenRoadRageTarget()   -- declared in BrnScoringSystem.h (near
//      HasTargetScoreBeenExceeded / GetOnlineFinishPosition; cited by neighbour rather than by line
//      because that header is being edited concurrently this wave), NO definition
//      anywhere in src/ and no link stub. Do not be fooled by
//      BrnRoadRageModeScoringLinkStubs.cpp:175: that defines the DIFFERENT
//      RoadRageModeScoring::HasBeatenRoadRageTarget (declared BrnRoadRageModeScoring.h:73).
//      Call sites here: :299 (GetPlayersFinishPosition's road-rage arm) and :487
//      (FinishCurrentMode's road-rage arm). Also called from BrnModeManager_UpdateMode.cpp:543/:584.
//   2. CgsSystem::TimerRequestInterface::GetSimTimerRequests() -- declared
//      CgsTimerRequestInterface.h:49-50 ("declaration-only here"), no definition. Call site :464
//      (FinishCurrentMode's mode-2 SetTimestepMultiplier leg).
//      A THIRD, ScoringSystem::GetPlayerModeTakedowns() (BrnScoringSystem.h:467, no definition),
//      was called at :473 for a value that was immediately discarded; that call is now inside the
//      parked block it belongs to, so it is no longer a live external from this file.

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include <cstring>   // memset / memcpy (the raw action payloads whose records are not homed yet)

// The spine buffers. BrnModeManager.h deliberately does NOT include BrnGameStateModuleIO.h (its own
// banner explains why); a partfile includes it locally, where the scope question is local.
//
// [!] INCLUDE ORDER IS LOAD-BEARING. BrnGameStateModuleIO.h reaches BrnTakedownManagerTypes.h and
// that header defines a SECOND `enum EActiveRaceCarIndex : s32` inside `namespace BrnGameState`
// (the dual-scope hazard BrnGameStateModuleIO.h:630 documents and BrnModeManager.h's own include
// ban exists for). GameStateModule::GetPlayerActiveRaceCarIndex spells its return type
// UNQUALIFIED, so whichever of the two enums is visible first WINS -- and every consumer this file
// hands the index to (ScoringSystem::GetPlayerTeam / GetCarRaceFinishPosition / GetFinishTime /
// StopModeTimer and GameStateToGuiInterface::AddFinishedRaceEvent) takes the GLOBAL
// BurnoutConstants.h one. So BrnGameStateModule.h is included FIRST, before anything that can drag
// the BrnGameState-scoped duplicate in, and every local is spelled `::EActiveRaceCarIndex`.
#include "GameSource/GameState/BrnGameStateModule.h"                       // GameStateModule (player index / car ids / dev challenges)
#include "GameSource/GameState/BrnGameStateModuleIO.h"                     // GameStateModuleIO::OutputBuffer
#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"   // GameStateToGuiInterface::AddFinishedRaceEvent
#include "GameSource/GameState/DeveloperChallengeManager/BrnDeveloperChallengeManager.h" // DeveloperChallengeManager::OnEventEnd
#include "GameSource/GameState/Progression/BrnProgressionManager.h"        // ProgressionManager (the payoff callees)
#include "GameSource/GameState/Progression/BrnProfile.h"                   // Profile::AddLossForGameMode / GetNumLossesForGameMode / SetEventScoreToUpload
#include "SharedClasses/Progression/BrnProgressionData.h"                  // ProgressionData::GetEventJunction*
#include "SharedClasses/Progression/BrnRaceEventData.h"                    // EventJunction::GetID / GetOfflineEvent
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"  // StuntModeScoring::GetCurrentScore / GetTargetScore
#include "GameShared/GameClasses/System/Timer/CgsTime.h"                   // CgsSystem::Time::GetFloatVal
#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h"  // CgsSystem::TimerRequestInterface / TimerRequests
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"           // VariableEventQueue<13312,16>::AddEvent

namespace BrnGameState
{

// ============================================================================
// TU-LOCAL CONSTANTS -- every one image-cited or asm-cited. No invented values.
// ============================================================================

// The verbatim X360-baked source path every assert in this class shares (the string the exports
// show as aDP4B5MainBurno_70). Kept for the record; CGS_ASSERT supplies __FILE__/__LINE__ itself,
// and the console's own line number is quoted at each site.
static const char* const KAC_MODEMANAGER_FILE =
    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/ModeManager/BrnModeManager.cpp";

// ---------------------------------------------------------------------------
// The per-mode pre-race countdown table. GetCountdownTimeForMode @0x82327D10 indexes
// flt_82020E98 by EGameModeType (`slwi r9, r11, 2; lfsx f31, r9, r10`).
//
// DUMPED BIG-ENDIAN FROM THE IMAGE THIS SESSION
// (scratch/postfx_step9_final/envfix/work/image.bin, offset 0x20E98 == VA - 0x82000000):
//   0x40200000 0x40C00000 0x00000000 0x40533333 0x40400000 0x40A00000 0x40A00000 0x40200000
//   0x40533333 0x40A00000 0x40400000 0x40400000 0x40400000 0x40400000 0x40400000 0x00000000
//   0x00000000 0x40400000
//
// [!] AUTHORED ZEROES, NOT PLACEHOLDERS: entries 2 (OFFLINE_SHOWTIME), 15 (ONLINE_FREE_BURN_LOBBY)
//     and 16 (ONLINE_SHOWTIME) are 0.0f IN THE SHIPPED ROM -- those three modes have no countdown.
//     Any placeholder-zero sweep must leave them alone.
// [!] Entries 3 and 8 are 0x40533333 == 3.2999999523f; they are authored 3.3f.
// STUNT_ATTACK (7) == 2.5 s -- the value the stunt-race campaign's boot oracle expects.
// ---------------------------------------------------------------------------
static const f32 KAF_COUNTDOWN_TIME_FOR_MODE[ModeManager::KI_GAME_MODE_SLOTS] =
{
    2.5f,  // 0  E_MODE_OFFLINE_RACE
    6.0f,  // 1  E_MODE_FACE_OFF
    0.0f,  // 2  E_MODE_OFFLINE_SHOWTIME        [!] AUTHORED ZERO
    3.3f,  // 3  E_MODE_ROAD_RAGE               (0x40533333)
    3.0f,  // 4  E_MODE_PURSUIT
    5.0f,  // 5  E_MODE_BURNING_ROUTE
    5.0f,  // 6  E_MODE_ELIMINATOR              (authored NULL mode slot; the table entry is real)
    2.5f,  // 7  E_MODE_STUNT_ATTACK
    3.3f,  // 8  E_MODE_MARKED_MAN              (0x40533333)
    5.0f,  // 9  E_MODE_TRAFFIC_ATTACK          (authored NULL mode slot)
    3.0f,  // 10 E_MODE_ONLINE_RACE
    3.0f,  // 11 E_MODE_ONLINE_ROAD_RAGE
    3.0f,  // 12 E_MODE_ONLINE_FUGITIVE
    3.0f,  // 13 E_MODE_ONLINE_BURNING_HOME_RUN
    3.0f,  // 14 E_MODE_ONLINE_FREE_BURN
    0.0f,  // 15 E_MODE_ONLINE_FREE_BURN_LOBBY  [!] AUTHORED ZERO
    0.0f,  // 16 E_MODE_ONLINE_SHOWTIME         [!] AUTHORED ZERO
    3.0f,  // 17 E_MODE_ONLINE_MODE_END
};

// The two team-1 (RED) countdown bonuses GetCountdownTimeForMode adds. Image-dumped this session:
//   flt_82020F98 == 0x40A00000 == 5.0f  -- mode 11 (E_MODE_ONLINE_ROAD_RAGE)
//   flt_82020F90 == 0x40400000 == 3.0f  -- mode 13 (E_MODE_ONLINE_BURNING_HOME_RUN)
static const f32 KF_TEAM_RED_COUNTDOWN_BONUS_ONLINE_ROAD_RAGE = 5.0f;   // flt_82020F98
static const f32 KF_TEAM_RED_COUNTDOWN_BONUS_ONLINE_HOME_RUN  = 3.0f;   // flt_82020F90

// ShowModeResults' showtime score conversion: metres -> yards.
// Image-dumped: flt_820DB5A8 == 0x3F8BFB85 == 1.0936132669448853f.
static const f32 KF_METRES_TO_YARDS = 1.0936132669448853f;
// The burning-route upload score is the finish time in MILLISECONDS.
// Image-dumped: flt_820DB5C8 == 0x447A0000 == 1000.0f.
static const f32 KF_SECONDS_TO_MILLISECONDS = 1000.0f;
// The "player aborted / did not really finish" sentinel ShowModeResults writes into the results
// record's time field. Image-dumped: flt_820282B4 == 0x4CBEBC20 == 100000000.0f (1.0e8 s).
static const f32 KF_ABORTED_FINISH_TIME = 100000000.0f;
// flt_82001CC0 == 0x00000000 == 0.0f (the shared zero constant the whole image uses).
static const f32 KF_ZERO = 0.0f;
// flt_82001C98 == 0x3F800000 == 1.0f -- the timestep multiplier FinishCurrentMode restores for
// OFFLINE_SHOWTIME (i.e. the end of crash-mode impact time).
static const f32 KF_NORMAL_TIMESTEP_MULTIPLIER = 1.0f;

// ---------------------------------------------------------------------------
// BrnGui::EFinishType values, recovered from this TU's own asserts + arms.
//
// PINNED BY THE ASSERT STRINGS (FinishCurrentMode :1439 / :1440): the race bucket computes
// `leFinishType = luFinishPosition - 1` and asserts `>= E_FINISH_TYPE_1ST` / `<= E_FINISH_TYPE_8TH`
// against 0 and 7, so E_FINISH_TYPE_1ST == 0 .. E_FINISH_TYPE_8TH == 7.
//
// [!] FLAG -- 8..11 ARE NAMED FROM THEIR WRITERS, NOT FROM A STRING. 8 is written whenever the
// +0x94FD latch is set. [!] NAME CORRECTED 2026-08-26 (CLOSURE round, H4 conductor ruling): that
// byte is mbPlayerFinishedTimedOut, NOT the old "results-team-won" -- its writer is
// ModeManager::PlayerFinishedMode @0x823280D8 storing PlayerFinishedModeEvent BYTE 0, which says
// nothing about teams (full refutation at BrnModeManager.h:758-783). So arm 8 is "the run ended on
// the mode clock", which is what this TU's own reads at :402/:435/:507 treat it as; 9/10 are the pass/fail pair
// of every target-based mode (road rage, marked man, stunt run, showtime, the team modes); 11 is
// only reachable from the "Unknown or unsupported game mode type" default. The tree's BrnGui
// EFinishType enum currently carries only E_FINISH_TYPE_NONE == 0, so the values live here as
// file-local constants and are cast at the single AddFinishedRaceEvent call site. header_request
// filed to grow the real enum; do NOT invent GUI-side meanings for 8..11 beyond this.
// ---------------------------------------------------------------------------
static const s32 KI_FINISH_TYPE_1ST      = 0;
static const s32 KI_FINISH_TYPE_8TH      = 7;
static const s32 KI_FINISH_TYPE_ABORTED  = 8;    // the mbPlayerFinishedTimedOut (+0x94FD) arm of every mode
static const s32 KI_FINISH_TYPE_PASSED   = 9;    // target met  (road rage / marked man / stunt run / showtime)
static const s32 KI_FINISH_TYPE_FAILED   = 10;   // target missed
static const s32 KI_FINISH_TYPE_UNKNOWN  = 11;   // the default arm's value

// The finish positions the non-race modes answer through GetPlayersFinishPosition. The console
// open-codes them as `(passed ? 1 : 4)` via a cntlzw/subfe/rlwinm sequence; the two literals are
// the only values that sequence can produce (`addi r3, r11, 4` over a 0 / -3 mask).
static const s32 KI_FINISH_POSITION_PASSED = 1;
static const s32 KI_FINISH_POSITION_FAILED = 4;

// ---------------------------------------------------------------------------
// [x] TU-LOCAL ACTION IDS RETIRED 2026-08-26 (stuntrace waveB CLOSURE round).
// All four ids this file posts now have enumerators in GameStateModuleIO::EGameActionType and
// are used BY NAME below; the mirrors are deleted (two of them -- 43 and 7 -- were also being
// carried by BrnModeManager_Start.cpp, which is exactly the maintenance hazard the header move
// closes). Each value's evidence travelled into BrnGameActions.h with it:
//   35  -> E_ACTION_FINISHED_MODE_NOTIFY  (SendFinishedModeAction @0x82343628, `li r5,0x23`,
//          size 1, payload posted UNINITIALISED). NAME STILL FLAGGED in the header.
//   43  -> E_ACTION_IMPACT_TIME_END       (FinishCurrentMode @0x8234BB5C, `li r5,0x2B`, size 1)
//   18  -> E_ACTION_EVENT_SCORE_TO_UPLOAD (ShowModeResults @0x82343BBC, `li r5,0x12`, size 16;
//          payload {+0 CgsID event id, +8 EGameModeType, +0xC score}, and the next console call
//          is Profile::SetEventScoreToUpload). NAME STILL FLAGGED in the header -- no DWARF
//          enumerator fits this payload.
//   7   -> E_ACTION_SET_PLAYER_CAR_DRIVER (ShowModeResults @0x82343E3C, `li r5,7`, size 48).
//          NOW PRODUCER-PINNED, not inferred: DriveThruManager::SetPlayerCarDriver posts the
//          same id/size @0x82386830 and its symbol name IS the DWARF name (:7).
// [x] The 31-vs-35/36 split this banner used to raise is SETTLED in the header:
// E_ACTION_FINISHED_MODE == 36 (the 48-byte SendModeResults record that struct FinishedModeAction
// models), E_ACTION_FINISHED_MODE_NOTIFY == 35, and 31 is E_ACTION_MARKED_MAN_LOADED -- pinned by
// ModeManager::MarkedManLoaded, which posts id 31 size 8.
// ---------------------------------------------------------------------------

// The ETrainingType ShowModeResults requests after an ONLINE_RACE (X360 `li r11, 0x3E` == 62,
// posted as the 4-byte payload of the already-enumerated E_ACTION_REQUEST_GAME_TRAINING (149)).
// FLAG: the training-type enum is not homed; 62 is the asm-pinned literal.
static const s32 KI_TRAINING_TYPE_AFTER_ONLINE_RACE = 62;

// ============================================================================
// ModeManager::GetCountdownTimeForMode -- X360 0x82327D10
// ============================================================================
// The pre-race countdown length for the CURRENT mode, plus the red-team bonus the two online team
// modes grant. WHOLE (hazards H7 lists it as no-slicing: the table is one load).
f32 ModeManager::GetCountdownTimeForMode() const
{
    // Console: `if (meCurrentGameModeType <= -1 || >= 0x12) { assert }`.
    CGS_ASSERT((meCurrentGameModeType > GameStateModuleIO::E_MODE_NONE) &&
               (static_cast<s32>(meCurrentGameModeType) < KI_GAME_MODE_SLOTS),
               "(meCurrentGameModeType > GameStateModuleIO::E_MODE_NONE) && (meCurrentGameModeType < GameStateModuleIO::E_MODE_COUNT)");
    // (BrnModeManager.cpp:2401 -- the assert's own line; note it names E_MODE_COUNT while the asm
    //  compares against 18, which is exactly the 17-vs-18 split the header's KI_GAME_MODE_SLOTS
    //  banner resolves.)

    // [!] NAMED HOST GUARD (one instruction of divergence, deliberate): the console keeps going
    // after a fired assert and indexes flt_82020E98[meCurrentGameModeType] anyway -- with
    // E_MODE_NONE (-1) that is a read one float BEFORE the table. On the host that is UB, so the
    // out-of-range answer is pinned at 0.0f instead of reading off the end of a real array.
    // Behaviour inside the attested range is identical.
    if ((meCurrentGameModeType <= GameStateModuleIO::E_MODE_NONE) ||
        (static_cast<s32>(meCurrentGameModeType) >= KI_GAME_MODE_SLOTS))
    {
        return KF_ZERO;
    }

    const f32 lfCountdownTime = KAF_COUNTDOWN_TIME_FOR_MODE[meCurrentGameModeType];

    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE)          // console `cmpwi r11, 0xB`
    {
        const ::EActiveRaceCarIndex lePlayerRaceCarIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();
        if (mScoringSystem.GetPlayerTeam(lePlayerRaceCarIndex) == GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM)
        {
            return lfCountdownTime + KF_TEAM_RED_COUNTDOWN_BONUS_ONLINE_ROAD_RAGE;    // flt_82020F98 == 5.0f
        }
    }
    else if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN)  // console `cmpwi r11, 0xD`
    {
        const ::EActiveRaceCarIndex lePlayerRaceCarIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();
        if (mScoringSystem.GetPlayerTeam(lePlayerRaceCarIndex) == GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM)
        {
            return lfCountdownTime + KF_TEAM_RED_COUNTDOWN_BONUS_ONLINE_HOME_RUN;     // flt_82020F90 == 3.0f
        }
    }

    return lfCountdownTime;
}

// ============================================================================
// ModeManager::GetPlayersFinishPosition -- X360 0x823281E8
// ============================================================================
// The player's 1-based finish position for the current mode. WHOLE (hazards H7).
//
// [!] THE DEBUG OVERRIDE IS CONSUME-ONCE. The console reads miDebugFinishPosition (+0x9518,
// `addis r11,r31,1; addi r11,r11,-0x6AE8` == this+0x9518) and, when it is POSITIVE, ZEROES IT and
// returns the value it read. Losing the zeroing pins every subsequent finish at the debug value.
s32 ModeManager::GetPlayersFinishPosition()
{
    if (miDebugFinishPosition > 0)
    {
        const s32 liOverriddenPosition = miDebugFinishPosition;
        miDebugFinishPosition = 0;                       // console `li r10,0; stw r10, 0(r11)`
        return liOverriddenPosition;
    }

    const ::EActiveRaceCarIndex lePlayerRaceCarIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();

    switch (meCurrentGameModeType)
    {
        // jumptable cases 0,1,11-16,18 == mode -1,0,10,11,12,13,14,15,17: the live scoring position.
        case GameStateModuleIO::E_MODE_NONE:
        case GameStateModuleIO::E_MODE_OFFLINE_RACE:
        case GameStateModuleIO::E_MODE_ONLINE_RACE:
        case GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:
        case GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
        case GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY:
        case GameStateModuleIO::E_MODE_ONLINE_MODE_END:
            return mScoringSystem.GetCarRaceFinishPosition(lePlayerRaceCarIndex);

        // jumptable cases 3,17 == mode 2,16: showtime always "finishes first".
        case GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME:
        case GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:
            return KI_FINISH_POSITION_PASSED;

        // jumptable case 4 == mode 3. Console: `lwz 0x58F0` (ScoringSystem+0x4B40, the player's
        // road-rage takedowns) vs `lwz 0x58FC` (ScoringSystem+0x4B4C, the target), both inlined.
        // FLAG: ScoringSystem names the takedown side (GetPlayerModeTakedowns) but not the target,
        // so the comparison is reached through the named predicate that IS declared for it.
        case GameStateModuleIO::E_MODE_ROAD_RAGE:
            return mScoringSystem.HasBeatenRoadRageTarget() ? KI_FINISH_POSITION_PASSED
                                                            : KI_FINISH_POSITION_FAILED;

        // jumptable case 6 == mode 5. Console: `lwz r11, 0x6ABC(r31)` == ScoringSystem+0x5D0C + 1.
        // ScoringSystem+0x5D0C is meCurrentMedalTarget (anchored off the Lookup.cpp-proven
        // mbEliminationRequired @ss+0x5D20 / muCarsThatHaventBeenEliminated @ss+0x5D24 pair, back
        // through mfEliminatorTimeStep/mLastCarRaceCarIndex/mbPlayerElimated/mbEliminationMode/
        // meCurrentMedalAchieved). Gold(0) -> 1st, silver(1) -> 2nd, ...
        case GameStateModuleIO::E_MODE_BURNING_ROUTE:
            return static_cast<s32>(mScoringSystem.GetCurrentMedalTarget()) + 1;

        // jumptable case 8 == mode 7. Console: `lwz 0x1110` vs `lwz 0x1114` == the OFFLINE stunt
        // scorer (ScoringSystem+0x350) +0x10 (miCurrentScore) vs +0x14 (miTargetScore).
        case GameStateModuleIO::E_MODE_STUNT_ATTACK:
        {
            const StuntModeScoring* lpStuntScorer = mScoringSystem.GetStuntScorer();
            return (lpStuntScorer->GetCurrentScore() >= lpStuntScorer->GetTargetScore())
                       ? KI_FINISH_POSITION_PASSED
                       : KI_FINISH_POSITION_FAILED;
        }

        // jumptable case 9 == mode 8. Console: `lbzx r11, r31, 0x94FE` then `(byte != 0) + 1`.
        // [x] BOOL-BLOCK FLAG CLEARED (hazards H4, closure round 2026-08-26). This reader was the
        // evidence that RENAMED the byte: its semantics are "the player did not survive the Marked
        // Man run" (set -> 2nd == lost, clear -> 1st == won), which fits +38142's writer -- the
        // PlayerFinishedModeEvent's mbCarDestroyed byte -- and does not fit the old
        // mbHasAbortedDueToDisconnect (a disconnect has no business deciding an OFFLINE mode's
        // finish position). Byte and name now agree; see the H4 block in BrnModeManager.h.
        case GameStateModuleIO::E_MODE_MARKED_MAN:
            return (mbPlayerFinishedCarDestroyed ? 2 : 1);

        // jumptable default, cases 2,5,7,10 == mode 1,4,6,9 (FACE_OFF, PURSUIT and the two
        // authored-NULL slots ELIMINATOR / TRAFFIC_ATTACK).
        default:
            CGS_ASSERT(false, "Unknown or unsupported game mode type");   // BrnModeManager.cpp:3645
            return 0;
    }
}

// ============================================================================
// ModeManager::SendFinishedModeAction -- X360 0x82343628
// ============================================================================
// Broadcast the payload-free "the mode has finished" action -- UNLESS the debug finish-position
// override is armed (in which case the console swallows the broadcast entirely).
void ModeManager::SendFinishedModeAction(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");   // BrnModeManager.cpp:3258

    // The console calls GetPlayerActiveRaceCarIndex here and leaves the answer in r3 (it becomes
    // the suppressed path's return value; this method is void in the DWARF signature). The call
    // itself is kept so the sequence of side-effect-free reads matches.
    (void)mpGameStateModule->GetPlayerActiveRaceCarIndex();

    // Console: `lbzx 0x94F7` (mbFinishCurrentModeNextUpdate); if clear -> post. If set, `lwzx
    // 0x9518` (miDebugFinishPosition); if > 0 -> return without posting, else post.
    if (mbFinishCurrentModeNextUpdate && (miDebugFinishPosition > 0))
    {
        return;
    }

    // One byte, posted UNINITIALISED by the console (the stack slot is never written) -- the same
    // shape GameStateModule::RequestUnpause uses; zeroed here so the queue image is deterministic.
    u8 lacFinishedMode[1] = { 0 };
    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(lacFinishedMode),
                                GameStateModuleIO::E_ACTION_FINISHED_MODE_NOTIFY, 1);
}

// ============================================================================
// ModeManager::FinishCurrentMode -- X360 0x8234B978
// ============================================================================
// The end-of-mode funnel: classify the outcome into a BrnGui::EFinishType, keep the
// consecutive-failed-attempts tally, tell the GUI, advance the mode's own state machine, stop the
// mode timer, ship the results, and hand off to the GameStateModule.
//
// [!] [stuntrace] ONLINE ARM DEFERRED -- none. Every arm below is written whole (hazards H7 lists
// FinishCurrentMode as may-slice for the team/winner legs, but ScoringSystem::GetPlayerTeam /
// IsBlueTeamEliminated / OnlineStuntRunModeScoring::GetWinnerTeam are all DECLARED, so slicing
// would have cost more than it saved).
void ModeManager::FinishCurrentMode(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                    const CgsSystem::Time&           lrTime)
{
    const ::EActiveRaceCarIndex lePlayerRaceCarIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();

    // r29 == "this attempt counts as a success" (it drives the unsuccessful-attempt tally below);
    // r30 == the BrnGui::EFinishType. Both seeded before the switch: r28(=1) into r29, and r30
    // written by every arm.
    bool lbAttemptSucceeded = true;
    s32  liFinishType       = KI_FINISH_TYPE_UNKNOWN;

    // Two byte latches the console raises here, BEFORE the switch (`stbx r28, r31, 0x9501` and
    // `stbx r28, r31, 0x950B`).
    // [!] BOOL-BLOCK FLAG (hazards H4), both reported for the verifier's cross-agent pass:
    //   +38145 (0x9501) carries the DWARF-order name mbModeDataIsLoading and IsWaitingForModeDataToLoad()
    //          returns it -- but its only proven WRITER (this one) raises it when a mode FINISHES,
    //          which "mode data is loading" does not describe. Candidate semantics: "the mode has
    //          finished / results are pending".
    //   +38155 (0x950B) is the header's muUnkByte_0x950B ("no reader identified anywhere"). This is
    //          its first proven writer: FinishCurrentMode sets it to 1. Still no reader, so the
    //          name stays unknown -- but the byte is now known to be a live flag, not padding.
    mbModeDataIsLoading = true;    // X360 +0x9501 = 1
    muUnkByte_0x950B    = 1;       // X360 +0x950B = 1

    // Cases 7 / 12 / 14 / 17 share the console's LABEL_39 tail (the stunt-run pass/fail test).
    bool lbEvaluateStuntRunOutcome = false;

    switch (meCurrentGameModeType)
    {
        // ---- jumptable cases 0,10,15: the race bucket ------------------------------------
        case GameStateModuleIO::E_MODE_OFFLINE_RACE:
        case GameStateModuleIO::E_MODE_ONLINE_RACE:
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY:
        {
            if (mbPlayerFinishedTimedOut)                                   // X360 `lbzx r11, r31, 0x94FD`
            {
                lbAttemptSucceeded = false;
                liFinishType       = KI_FINISH_TYPE_ABORTED;
                break;
            }

            s32 liFinishPosition;
            if (mbFinishCurrentModeNextUpdate && (miDebugFinishPosition > 0))
            {
                liFinishPosition = miDebugFinishPosition;
            }
            else
            {
                liFinishPosition = static_cast<s32>(mScoringSystem.GetCarRacePosition(lePlayerRaceCarIndex));
            }

            CGS_ASSERT(liFinishPosition != 0, "luFinishPosition > 0");                     // :1437
            liFinishType = liFinishPosition - 1;
            CGS_ASSERT(liFinishType >= KI_FINISH_TYPE_1ST, "leFinishType >= BrnGui::E_FINISH_TYPE_1ST"); // :1439
            CGS_ASSERT(liFinishType <= KI_FINISH_TYPE_8TH, "leFinishType <= BrnGui::E_FINISH_TYPE_8TH"); // :1440

            // Only a 1st place counts as a successful attempt (`cntlzw` + `extrwi 1,26` == "== 0").
            lbAttemptSucceeded = (liFinishType == KI_FINISH_TYPE_1ST);
            break;
        }

        // ---- jumptable cases 2,16: showtime ----------------------------------------------
        case GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME:
        case GameStateModuleIO::E_MODE_ONLINE_SHOWTIME:
        {
            // `(mbPlayerFinishedTimedOut == 0) + 8` -- aborted -> 8, otherwise 9. Note the attempt is left
            // SUCCESSFUL either way (the console never touches r29 in this arm).
            liFinishType = mbPlayerFinishedTimedOut ? KI_FINISH_TYPE_ABORTED : KI_FINISH_TYPE_PASSED;

            // One uninitialised byte, action 43 / size 1 (E_ACTION_IMPACT_TIME_END).
            u8 lacImpactTimeEnd[1] = { 0 };
            lpOutputBuffer->GetGameActionQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(lacImpactTimeEnd),
                GameStateModuleIO::E_ACTION_IMPACT_TIME_END, 1);

            if (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME)
            {
                // X360: `GetTimerRequest(outputBuffer)` then SetTimestepMultiplier(that + 8, 1.0).
                // TimerRequestInterface's SECOND embedded TimerRequests is mSimTimer @+8, so the
                // console restores the SIM timestep, not the game one.
                // [!] FLAG: OutputBuffer::GetTimerRequestInterface() still answers the 16-byte opaque
                // placeholder GameStateModuleIO::OutputBufferTimerRequestInterface. That placeholder
                // is EXACTLY sizeof(CgsSystem::TimerRequestInterface) (two 8-byte TimerRequests), and
                // the console's `+8` lands on mSimTimer either way. header_request filed to retype
                // the placeholder as a typedef of the real interface; the cast goes away when it lands.
                CgsSystem::TimerRequestInterface* lpTimerRequestInterface =
                    reinterpret_cast<CgsSystem::TimerRequestInterface*>(
                        lpOutputBuffer->GetTimerRequestInterface());
                lpTimerRequestInterface->GetSimTimerRequests()->SetTimestepMultiplier(
                    KF_NORMAL_TIMESTEP_MULTIPLIER);                     // flt_82001C98 == 1.0f
            }
            break;
        }

        // ---- jumptable case 3: road rage --------------------------------------------------
        case GameStateModuleIO::E_MODE_ROAD_RAGE:
        {
            // [X] PARKED LEG (one store), header_request filed. Console @0x8234BB90..0x8234BBB0:
            //     lwz r11, 0x58F0(r31)                       // ss+0x4B40, the FIRST of two reads
            //     r10 = mpProgressionManager(+0x6D5C) + 0x170 + 0x20000 - 0x32FC == Profile+118020
            //     lwz r9, 0(r10) / cmpw / ble / stw r11, 0(r10)
            // i.e. BrnProfile.h's `s32 miHighestNumberOfTakeDownsInRoadRage`, and
            //     if (liTakedowns > that) that = liTakedowns;
            // The member EXISTS and is at the proven offset, but it sits below BrnProfile.h:563's
            // `private:` and has no accessor pair. Requested:
            //     s32  GetHighestNumberOfTakeDownsInRoadRage() const;
            //     void SetHighestNumberOfTakeDownsInRoadRage(s32 liTakedowns);
            // Behavioural cost while parked: the profile's road-rage takedown high score never
            // rises. Nothing on the stunt-race path reads it.
            // Re-arm as:
            //     const s32 liTakedowns = mScoringSystem.GetPlayerModeTakedowns();  // ss+0x4B40
            //     if (liTakedowns > lpProfile->GetHighestNumberOfTakeDownsInRoadRage())
            //     {
            //         lpProfile->SetHighestNumberOfTakeDownsInRoadRage(liTakedowns);
            //     }
            // [!] FIX ROUND 2026-08-26: that GetPlayerModeTakedowns() call USED TO BE EMITTED here
            // and its result immediately discarded with `(void)liTakedowns;`. Deleted.
            // ScoringSystem::GetPlayerModeTakedowns is DECLARE-ONLY tree-wide (declared in
            // BrnScoringSystem.h, no definition anywhere in src/), so the live call bought an
            // LNK2019 for a value nothing
            // consumed. The console's SECOND read of ss+0x4B40, at 0x8234BBB4, is the one the
            // HasBeatenRoadRageTarget predicate below owns -- no console read is lost by moving this
            // one into the park with the store it belongs to.

            if (mScoringSystem.HasBeatenRoadRageTarget())    // console 0x8234BBB4-0x8234BBC4:
                                                             // ss+0x4B40 >= ss+0x4B4C, inlined
            {
                liFinishType = KI_FINISH_TYPE_PASSED;
            }
            else
            {
                liFinishType       = KI_FINISH_TYPE_FAILED;
                lbAttemptSucceeded = false;
            }
            break;
        }

        // ---- jumptable case 5: burning route ----------------------------------------------
        case GameStateModuleIO::E_MODE_BURNING_ROUTE:
            if (mbPlayerFinishedTimedOut)
            {
                lbAttemptSucceeded = false;
                liFinishType       = KI_FINISH_TYPE_ABORTED;
            }
            else
            {
                liFinishType = KI_FINISH_TYPE_1ST;    // console `mr r30, r24` (the zero register)
            }
            break;

        // ---- jumptable case 7: stunt attack -- falls into the shared LABEL_39 tail ---------
        case GameStateModuleIO::E_MODE_STUNT_ATTACK:
            lbEvaluateStuntRunOutcome = true;
            break;

        // ---- jumptable case 8: marked man --------------------------------------------------
        case GameStateModuleIO::E_MODE_MARKED_MAN:
            // Console: `lbzx 0x94FD` then `lbz r11, 0x5920(r31)` == ScoringSystem+0x4B70 ==
            // mbPlayerTotalled (the byte between mauiMedalScores[4] and mOnlineRaceModeScoring
            // @ss+0x4B74, anchored by the header's own miMaximumPlayerCrashedNumber @+0x4B58 /
            // miCurrentPlayerCrashedNumber @+0x4B5C pair).
            if (mbPlayerFinishedTimedOut || mScoringSystem.IsPlayerTotalled())
            {
                liFinishType       = KI_FINISH_TYPE_FAILED;
                lbAttemptSucceeded = false;
            }
            else
            {
                liFinishType = KI_FINISH_TYPE_PASSED;
            }
            break;

        // ---- jumptable case 11: online road rage (team) ------------------------------------
        case GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:
        {
            if (mbPlayerFinishedTimedOut)
            {
                // NOTE: the console does NOT clear r29 on this arm (unlike the offline race bucket).
                liFinishType = KI_FINISH_TYPE_ABORTED;
                break;
            }

            const GameStateModuleIO::EPlayerTeam lePlayerTeam =
                mScoringSystem.GetPlayerTeam(lePlayerRaceCarIndex);

            if (lePlayerTeam == GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
            {
                // Blue: eliminated -> failed, still standing -> passed.
                liFinishType = mScoringSystem.IsBlueTeamEliminated() ? KI_FINISH_TYPE_FAILED
                                                                     : KI_FINISH_TYPE_PASSED;
            }
            else if (mScoringSystem.GetPlayerTeam(lePlayerRaceCarIndex) ==
                     GameStateModuleIO::E_PLAYER_TEAM_RED_TEAM)
            {
                // Red: blue eliminated -> passed (red won), else failed. (The console really does
                // call GetPlayerTeam a SECOND time here rather than reusing r3.)
                liFinishType = mScoringSystem.IsBlueTeamEliminated() ? KI_FINISH_TYPE_PASSED
                                                                     : KI_FINISH_TYPE_FAILED;
            }
            else
            {
                // The console builds this message through a CgsDev::StrStream into
                // Assert::gpcMessageBuffer; the text (trailing newline included) is verbatim.
                CGS_ASSERT(false, "Finished online road rage when not on a team\n");   // :1564
                liFinishType = KI_FINISH_TYPE_FAILED;
            }
            break;
        }

        // ---- jumptable cases 12,14,17: the online stunt-run family -------------------------
        case GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:
        case GameStateModuleIO::E_MODE_ONLINE_MODE_END:
        {
            // Console: `addi r3, ss, 0x4D44` (the embedded OnlineStuntRunModeScoring),
            //          `mr r4, ss`, `lwz r5, 0x4EE8(ss)` @0x8234BD20..0x8234BD28.
            // [x] WITHDRAWN 2026-08-26 (fix round) -- the "THIRD ARGUMENT DROPPED" claim that used
            // to stand here was WRONG, and the header_request it filed would have corrupted a
            // correct signature. Re-derived from the CALLEE:
            // OnlineStuntRunModeScoring::GetWinnerTeam @0x8232F950 saves only r3->r29 (this) and
            // r4->r28 (0x8232F960/0x8232F96C), seeds r27/r30/r31 = 0, then loops
            // `mr r5, r28 / mr r4, r31 / mr r3, r29 / bl GetTeamScore` over teams 0..9 with the
            // assert "leEnumIndex <= E_PLAYER_TEAM_COUNT" (line 0x70) on the iterator. r5 ON ENTRY
            // IS NEVER READ -- the value it forwards to GetTeamScore is r28, i.e. its own r4. IDA's
            // own prototype for it is `(int a1, int a2)`. So `lwz r5, 0x4EE8(r30)` at the call site
            // is a DEAD LOAD, and the committed 2-arg
            // `s32 GetWinnerTeam(const ScoringSystem*) const` is correct and complete.
            const s32 liWinnerTeam =
                mScoringSystem.GetOnlineStuntRunScorer()->GetWinnerTeam(&mScoringSystem);

            lbAttemptSucceeded =
                (static_cast<s32>(mScoringSystem.GetPlayerTeam(lePlayerRaceCarIndex)) == liWinnerTeam);

            lbEvaluateStuntRunOutcome = true;   // falls into LABEL_39
            break;
        }

        // ---- jumptable case 13: online burning home run ------------------------------------
        case GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:
            if (mbPlayerFinishedTimedOut)
            {
                liFinishType = KI_FINISH_TYPE_ABORTED;
            }
            else
            {
                // `GetPlayerTeam(...) - 2` -> zero means BLUE; blue passes, anyone else fails.
                liFinishType = (mScoringSystem.GetPlayerTeam(lePlayerRaceCarIndex) ==
                                GameStateModuleIO::E_PLAYER_TEAM_BLUE_TEAM)
                                   ? KI_FINISH_TYPE_PASSED
                                   : KI_FINISH_TYPE_FAILED;
            }
            break;

        // ---- jumptable default, cases 1,4,6,9 -----------------------------------------------
        default:
            liFinishType = KI_FINISH_TYPE_UNKNOWN;                        // console sets 0xB FIRST
            CGS_ASSERT(false, "Unknown or unsupported game mode type");   // :1646
            break;
    }

    // ---- LABEL_39: the shared stunt-run pass/fail test (modes 7, 12, 14, 17) ----------------
    if (lbEvaluateStuntRunOutcome)
    {
        // Console: `lwz r11, 0x1110(r31)` vs `lwz r10, 0x1114(r31)` == the OFFLINE stunt scorer's
        // miCurrentScore (+0x10) vs miTargetScore (+0x14). It reads the OFFLINE scorer even on the
        // online arms -- that is the console's own behaviour, reproduced verbatim.
        const StuntModeScoring* lpStuntScorer = mScoringSystem.GetStuntScorer();
        const bool lbTargetMet = (lpStuntScorer->GetCurrentScore() >= lpStuntScorer->GetTargetScore());

        if (lbTargetMet || (mbFinishCurrentModeNextUpdate && (miDebugFinishPosition == 1)))
        {
            liFinishType = KI_FINISH_TYPE_PASSED;
        }
        else
        {
            lbAttemptSucceeded = false;
            liFinishType       = KI_FINISH_TYPE_FAILED;
        }
    }

    // ---- the consecutive-unsuccessful-attempts tally ----------------------------------------
    const GameStateModuleIO::EGameModeType leFinishedMode = meCurrentGameModeType;
    if (meLastAttemptedGameModeType != leFinishedMode)
    {
        miNumUnsucessfulGameModeAttempts = 0;      // a different mode -- start a fresh streak
    }
    if (lbAttemptSucceeded)
    {
        miNumUnsucessfulGameModeAttempts = 0;
    }
    else
    {
        ++miNumUnsucessfulGameModeAttempts;
    }
    meLastAttemptedGameModeType = leFinishedMode;

    // ---- tell the GUI ------------------------------------------------------------------------
    lpOutputBuffer->GetGameStateToGuiInterface()->AddFinishedRaceEvent(
        static_cast<BrnGui::EFinishType>(liFinishType), lePlayerRaceCarIndex);

    // ---- advance the mode's own state machine -------------------------------------------------
    // Console `(*(**(this+3480) + 48))(mode, 1)`. The wave's vtable micro-check pins vtbl+48 as
    // slot 12 == SendEvent(EGameModeEvent) and the argument as the ENUM E_GME_NEXT (== 1), NOT a
    // bool. The console does NOT null-check mpCurrentGameMode here (it does two instructions
    // later); reproduced as-is -- a null here is the console's own crash.
    mpCurrentGameMode->SendEvent(E_GME_NEXT);

    // ---- stop the mode timer -------------------------------------------------------------------
    // Console: `if (mpCurrentGameMode) v22 = *(mode + 0xAC); else v22 = 0;` == GameMode::IsOnline().
    const bool lbIsOnline = (mpCurrentGameMode != nullptr) ? mpCurrentGameMode->IsOnline() : false;
    mScoringSystem.StopModeTimer(lrTime, mpGameStateModule->GetPlayerActiveRaceCarIndex(), lbIsOnline);

    // ---- ship the results (both committed bodies -- hazards H2, CALLED not re-implemented) ------
    SendModeResults(lpOutputBuffer->GetGameActionQueue());        // BrnModeManager.cpp:347
    SendFinishedModeAction(lpOutputBuffer->GetGameActionQueue()); // this file

    CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");   // :1670

    // [X] [stuntrace] PARKED CALL -- conductor decision #6: GameStateModule::OnModeFinish belongs to
    // the detection/start-driver wave, not this one, and is ABSENT from BrnGameStateModule.h.
    // Console: `GameStateModule::OnModeFinish(mpGameStateModule, lpOutputBuffer)` @0x82390EE0 -- the
    // module-level end-of-mode hook (it is what unwinds the mode back to FreeBurn). Re-wire the
    // moment that body lands; this is the LAST statement of the console body, so nothing below it
    // is lost.
    (void)lpOutputBuffer;
}

// ============================================================================
// ModeManager::ShowModeResults -- X360 0x823436D0
// ============================================================================
// THE offline progression payoff. Builds the 232-byte E_ACTION_SHOW_MODE_RESULTS record, runs the
// profile/progression updates for the offline event modes {0,3,5,7,8}, and posts the record plus
// its two follow-on actions.
//
// [!] [stuntrace] ONLINE ARM DEFERRED -- none of the offline progression path is gated (hazards H7
// is explicit: OnEventFinishUpdateProfile is the campaign payoff and must NOT be gated). The one
// parked leg is the rank-up notification arm, and it is parked on a MISSING MEMBER, not on
// online-ness -- see its banner.
void ModeManager::ShowModeResults(
    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCarOutput,
    GameStateModuleIO::GameActionQueue*                                          lpGameActionQueue)
{
    // The global race-car interface argument is genuinely UNUSED by the console body (r4 is never
    // read after the prologue). Kept in the signature because the DWARF and the caller pass it.
    (void)lpGlobalRaceCarOutput;

    CGS_ASSERT(lpGameActionQueue != nullptr, "lpGameActionQueue != NULL");   // :3297

    const ::EActiveRaceCarIndex lePlayerRaceCarIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();
    CGS_ASSERT(lePlayerRaceCarIndex >= 0,
               "leLocalPlayerRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");            // :3302
    CGS_ASSERT(lePlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leLocalPlayerRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");         // :3303

    // Console: `r24 = this + 0x8380` (== &mCurrentGameModeParams) and `addic.` sets the flag the
    // assert tests -- i.e. GetCurrentGameModeParams() != NULL, which for an embedded member is
    // always true. Kept as the console's own diagnostic.
    GameModeParams* lpGameModeParams = &mCurrentGameModeParams;
    CGS_ASSERT(lpGameModeParams != nullptr, "GetCurrentGameModeParams()");           // :3304

    // ------------------------------------------------------------------------------------------
    // The 232-byte results record.
    //
    // [x] WITHDRAWN 2026-08-26 (fix round) -- the "FIELD-NAME SWAP IN BrnGameActions.h" banner and
    // header_request that used to stand here were STALE, and applying the requested swap would have
    // BROKEN three already-correct things.
    // The console facts are as this banner stated them: the MODE TYPE goes to record+0x00
    // (`stw r11, var_180` with r11 == *(this+0xD94), and the tail re-reads var_180 against the
    // showtime pair {2,16}) and the FINISH POSITION to record+0x04 (`stw r3, var_17C` straight out
    // of GetPlayersFinishPosition). What changed is the header: on-disk BrnGameActions.h:1155-1162
    // ALREADY declares `EGameModeType meGameModeType; // +0x00` then `s32 miFinishPosition; // +0x04`,
    // carrying its own note "Order verified 2026-08-26 verify wave". The by-name writes below are
    // therefore already byte-correct and there is nothing to work around.
    // Applying the withdrawn swap now would (a) put the mode type on +0x04 and the position on
    // +0x00, (b) break BrnProgressionManager_EventFinish.cpp:244's `lpAction->miFinishPosition < 1`
    // gate, which the asm pins to `lwz r11, 4(r19)`, and (c) break the committed consumer arm in
    // GameBridgeGameStateToX_EventFlowGuiEvents.cpp. DO NOT RE-FILE IT.
    //
    // (Two smaller stale comments in the same struct, offsets unaffected: it labels mfField0C as
    //  "the race arm stores a computed distance/score word here" and mfField18 as "the finish-time
    //  delta". The producer is the other way round -- +0x0C IS the finish-time field and +0x18 is
    //  the showtime distance / stunt zero. The OFFSETS are right, so nothing here works around it;
    //  the comments are reported for correction.)
    // ------------------------------------------------------------------------------------------
    GameStateModuleIO::ShowModeResultsAction lAction;
    std::memset(&lAction, 0, sizeof(lAction));   // console `memset(v42, 0, 0xE8)`

    // (The two `stw r11(-1), var_E8 / var_C0` stores at 0x823436E0..0x823436F4 -- record+0x98 and
    //  record+0xC0 -- precede that memset and are wiped by it. Dead on the console; not reproduced.)

    lAction.meGameModeType = meCurrentGameModeType;                       // record+0x00

    const bool lbIsOnline = (mpCurrentGameMode != nullptr) ? mpCurrentGameMode->IsOnline() : false;
    lAction.mbIsOnlinePostEvent = static_cast<u8>(lbIsOnline ? 1 : 0);    // record+0xE4  (mode+0xAC)
    lAction.mbHasBlock58        = 0;                                      // record+0xDF
    lAction.mu8FieldDB          = 0;                                      // record+0xDB
    lAction.mu8FieldE2          = static_cast<u8>(mbPlayerFinishedCarDestroyed ? 1 : 0);   // record+0xE2 (+0x94FE)
    lAction.mu8FieldE3          = static_cast<u8>(mbHasPlayerFinished ? 1 : 0);           // record+0xE3 (+0x94FF)
    lAction.mu8FieldE1          = static_cast<u8>(mbPlayerFinishedTimedOut ? 1 : 0);              // record+0xE1 (+0x94FD)

    if ((meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME) ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME))
    {
        // ---- the SHOWTIME score block ---------------------------------------------------------
        // Console offsets, all ScoringSystem-relative through the +0xDB0 embed:
        //   this+4268 == ss+0x2FC == CrashModeScoring+0x2DC == miBaseScore
        //   this+4276 == ss+0x304 == CrashModeScoring+0x2E4 == miScoreMultiplier
        //   this+4312 == ss+0x328 == CrashModeScoring+0x308 == mfDistanceTravelled   (`lfs`, a f32)
        // (CrashModeScoring sits at ss+0x20, after the four 8-byte CgsSystem::Time members; the
        //  f32 load at ss+0x328 landing exactly on mfDistanceTravelled is what pins that.)
        CrashModeScoring* lpCrashScorer = mScoringSystem.GetCrashScorer();

        const f32 lfDistanceTravelled = lpCrashScorer->GetDistanceTravelled();
        const s32 liBaseScore         = lpCrashScorer->miBaseScore;   // no GetRawScore body in the tree
        const s32 liScoreMultiplier   = lpCrashScorer->GetScoreMultiplier();

        lAction.mfField18 = lfDistanceTravelled;   // record+0x18
        lAction.miField10 = liBaseScore;           // record+0x10
        lAction.miField14 = liScoreMultiplier;     // record+0x14

        // record+0x08 == ((s32)(distance * 1.0936133) * 100 + baseScore) * multiplier
        // (`fmuls` by flt_820DB5A8, `fctiwz`, `mulli 0x64`, `add`, `mullw`) -- metres converted to
        // yards, a hundred points a yard, plus the damage score, all times the crash multiplier.
        const s32 liYards = static_cast<s32>(lfDistanceTravelled * KF_METRES_TO_YARDS);
        lAction.miField08 = (liYards * 100 + liBaseScore) * liScoreMultiplier;
    }
    else
    {
        // ---- the STUNT-RUN score block --------------------------------------------------------
        // Console picks ss+0x2620 (the online stunt scorer) when the mode is online, else ss+0x350.
        StuntModeScoring* lpStuntScorer = lbIsOnline ? mScoringSystem.GetOnlineStuntScorer()
                                                     : mScoringSystem.GetStuntScorer();
        const s32 liStuntScore = lpStuntScorer->GetCurrentScore();   // scorer+0x10

        lAction.mfField18 = KF_ZERO;         // record+0x18 (flt_82001CC0)
        lAction.miField10 = 0;               // record+0x10
        lAction.miField14 = 1;               // record+0x14
        lAction.miField08 = liStuntScore;    // record+0x08

        // [X] PARKED LEG (one store), header_request filed:
        // Console: `r10 = mpProgressionManager + 0x170` (== Profile) then
        //     if (liStuntScore > *(profile + 0x268)) *(profile + 0x268) = liStuntScore;
        // Profile+0x268 == 616 == BrnProfile.h's `s32 miBestStuntRunScore` -- the member EXISTS at
        // exactly that offset but is below BrnProfile.h:563's `private:` with no accessor pair.
        // Requested:
        //     s32  GetBestStuntRunScore() const;
        //     void SetBestStuntRunScore(s32 liScore);
        // Behavioural cost while parked: the profile's best-stunt-run high score never rises.
        (void)mpProgressionManager->GetProfile();
    }

    // ---- "is this the player's FIRST win of this event?" ---------------------------------------
    if ((meCurrentGameModeType >= GameStateModuleIO::E_MODE_ONLINE_RACE) ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME))
    {
        lAction.mu8FieldDA = 0;   // record+0xDA
    }
    else
    {
        // The event id: console `lwz r5, 0x44(r24)` / `lwzx r8, r31, 0x83C4` -- the same u32 at
        // mCurrentGameModeParams+0x44, which is the argument HasEventBeenWonPreviously(uint32_t)
        // and OnEventFinishUpdateProfile take. On the host that field is GameModeParams's PUBLIC
        // data member `u32 muEventJunctionID` (GameModeParams declares no GetEventJunctionId() --
        // that accessor belongs to StartGameModeParams). The host GameModeParams is 480 bytes short
        // of the console's 2160 (see the header's WIRE-FORMAT DIVERGENCE banner), so the field is
        // reached BY NAME and never by the console's +0x44.
        const u32 luEventId = lpGameModeParams->muEventJunctionID;

        const BrnProgression::ProgressionData* lpProgressionData =
            mpProgressionManager->GetProgressionData();
        CGS_ASSERT(lpProgressionData != nullptr, "lpProgressionData");   // :3341

        // The console walks the event-junction table inline: count at ProgressionData+0x1C, base at
        // +0x18, 16-byte stride, entry+0x00 == the id, entry+0x04 == the offline event pointer.
        // Reached here through the named accessors ProgressionData already declares for exactly
        // that table (GetEventJunctionCount / GetEventJunction).
        const BrnProgression::RaceEventData* lpEventData = nullptr;
        if (lpProgressionData != nullptr)
        {
            const u32 luJunctionCount = lpProgressionData->GetEventJunctionCount();
            for (u32 luJunction = 0; luJunction < luJunctionCount; ++luJunction)
            {
                const BrnProgression::EventJunction* lpJunction =
                    lpProgressionData->GetEventJunction(luJunction);
                if (lpJunction->GetID() == luEventId)
                {
                    lpEventData = lpJunction->GetOfflineEvent();
                    break;
                }
            }
        }
        CGS_ASSERT(lpEventData != nullptr, "lpEventData");   // :3343

        // record+0xDA == !HasEventBeenWonPreviously(eventId) -- i.e. "this is a first win".
        lAction.mu8FieldDA =
            static_cast<u8>(mpProgressionManager->HasEventBeenWonPreviously(luEventId) ? 0 : 1);
    }

    // record+0xE5 == 1. [!] FLAG: the frozen ShowModeResultsAction declares +0xE5..+0xE7 as
    // `maPadE5[3]`, but +0xE5 is a REAL field -- this body writes 1 into it and re-reads it at the
    // tail to decide whether to post the 48-byte follow-on action. Written through the pad byte so
    // the wire image is exact; header_request filed to name it.
    lAction.maPadE5[0] = 1;

    lAction.miFinishPosition = GetPlayersFinishPosition();                          // record+0x04
    lAction.mu8FieldE0       = static_cast<u8>(HasPlayerWon() ? 1 : 0);             // record+0xE0
    lAction.mu64FieldC8      = 0;                                                   // record+0xC8

    // ---- the finish-time field (record+0x0C) ----------------------------------------------------
    if (mbPlayerFinishedTimedOut)
    {
        lAction.mfField0C = KF_ABORTED_FINISH_TIME;   // flt_820282B4 == 1.0e8
    }
    else if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE)
    {
        lAction.mfField0C = KF_ZERO;                  // flt_82001CC0 == 0.0f (road rage has no time)
    }
    else
    {
        // Console: GetFinishTime returns a CgsSystem::Time by hidden pointer; the body then does
        // `(f32)(s64)time.miSeconds + time.mfFraction` (extsw/std/lfd/fcfid/frsp/fadds) -- exactly
        // Time::GetFloatVal(). The `fsel f0, f12, f0, f13` over `f12 = finish - mfModeTimeLimit`
        // answers mfModeTimeLimit when the difference is >= 0, else the finish time itself.
        const f32 lfFinishTime = mScoringSystem.GetFinishTime(lePlayerRaceCarIndex).GetFloatVal();
        lAction.mfField0C = ((lfFinishTime - mfModeTimeLimit) >= 0.0f) ? mfModeTimeLimit : lfFinishTime;
    }

    CGS_ASSERT(lpGameModeParams != nullptr, "lpGameModeParams");   // :3388

    // ---- THE PROGRESSION PAYOFF -----------------------------------------------------------------
    // Console gate: `if (!record[+0xE1] || meCurrentGameModeType == 3)` -- i.e. run the progression
    // update unless the run was aborted, with road rage exempt from the abort veto.
    if ((lAction.mu8FieldE1 == 0) || (meCurrentGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE))
    {
        switch (meCurrentGameModeType)
        {
            case GameStateModuleIO::E_MODE_OFFLINE_RACE:      // 0
            case GameStateModuleIO::E_MODE_ROAD_RAGE:         // 3
            case GameStateModuleIO::E_MODE_BURNING_ROUTE:     // 5
            case GameStateModuleIO::E_MODE_STUNT_ATTACK:      // 7
            case GameStateModuleIO::E_MODE_MARKED_MAN:        // 8
            {
                const u32 luEventId = lpGameModeParams->muEventJunctionID;

                // Console `bl OnEventFinishUpdateProfile` with r3=mpProgressionManager, r4=queue,
                // r5=eventId, r6=&record and r7 STILL LIVE from `lwz r7, 0xD94(r31)` two branches
                // earlier -- i.e. the mode type. That is the DWARF's own 4-argument shape.
                // (Body: agent 10, BrnProgressionManager.cpp.)
                mpProgressionManager->OnEventFinishUpdateProfile(lpGameActionQueue, luEventId,
                                                                &lAction, meCurrentGameModeType);

                if (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_RACE)
                {
                    // Console: `ldx r4, gsm, 0x456D8` / `ldx r5, gsm, 0x456E0` (the two CgsIDs the
                    // header names GetActivePlayerCarId / GetActivePlayerWheelId), r6=queue, r7=1.
                    mpGameStateModule->OnPlayerCarChange(mpGameStateModule->GetActivePlayerCarId(),
                                                         mpGameStateModule->GetActivePlayerWheelId(),
                                                         lpGameActionQueue, true);
                }

                if ((meCurrentGameModeType == GameStateModuleIO::E_MODE_BURNING_ROUTE) ||
                    (meCurrentGameModeType == GameStateModuleIO::E_MODE_STUNT_ATTACK))
                {
                    const CgsID lEventId = static_cast<CgsID>(luEventId);   // console `std`s the
                                                                            // zero-extended u32
                    const GameStateModuleIO::EGameModeType leGameModeType = meCurrentGameModeType;

                    // Burning route uploads the finish time in ms; stunt attack uploads the score.
                    const s32 liScoreToUpload =
                        (leGameModeType == GameStateModuleIO::E_MODE_BURNING_ROUTE)
                            ? static_cast<s32>(lAction.mfField0C * KF_SECONDS_TO_MILLISECONDS)
                            : lAction.miField08;

                    CGS_ASSERT(lpGameActionQueue != nullptr, "lpGameActionQueue");   // :3439

                    // The 16-byte payload the console builds on the stack before the AddEvent.
                    // FLAG: no homed record type for action 18 (see E_ACTION_EVENT_SCORE_TO_UPLOAD);
                    // built as raw bytes at the asm-proven offsets, the established house pattern
                    // (BrnCarSelectManager.cpp:1133-1153).
                    u8 lacEventScoreToUpload[16];
                    std::memset(lacEventScoreToUpload, 0, sizeof(lacEventScoreToUpload));
                    std::memcpy(lacEventScoreToUpload + 0x00, &lEventId,        sizeof(lEventId));
                    std::memcpy(lacEventScoreToUpload + 0x08, &leGameModeType,  sizeof(s32));
                    std::memcpy(lacEventScoreToUpload + 0x0C, &liScoreToUpload, sizeof(s32));
                    lpGameActionQueue->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(lacEventScoreToUpload),
                        GameStateModuleIO::E_ACTION_EVENT_SCORE_TO_UPLOAD, 16);

                    CGS_ASSERT(mpProgressionManager != nullptr, "mpProgressionManager");   // :3443
                    mpProgressionManager->GetProfile()->SetEventScoreToUpload(lEventId, liScoreToUpload,
                                                                             leGameModeType);
                }

                CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");            // :3449
                CGS_ASSERT(mpGameStateModule->GetDeveloperChallengeManager() != nullptr,
                           "mpGameStateModule->GetDeveloperChallengeManager()");          // :3450
                mpGameStateModule->GetDeveloperChallengeManager()->OnEventEnd(
                    static_cast<s32>(meCurrentGameModeType),
                    (lAction.miFinishPosition == 1));
                break;
            }

            default:
                break;
        }
    }

    // ---- the loss tally --------------------------------------------------------------------------
    if (meCurrentGameModeType < GameStateModuleIO::E_MODE_ONLINE_RACE)   // console `cmpwi r4, 0xA; bge`
    {
        BrnProgression::Profile* lpProfile = mpProgressionManager->GetProfile();

        // Burning route counts a loss when the ABORT latch is set; every other offline mode counts
        // a loss when the player did not win.
        const bool lbAddLoss = (meCurrentGameModeType == GameStateModuleIO::E_MODE_BURNING_ROUTE)
                                   ? (lAction.mu8FieldE1 != 0)
                                   : (lAction.mu8FieldE0 == 0);
        if (lbAddLoss)
        {
            lpProfile->AddLossForGameMode(meCurrentGameModeType);
        }

        // record+0xD4 == Profile::GetNumLossesForGameMode(mode).
        // [!] FLAG: +0xD4 falls inside ShowModeResultsAction's `maPadD0[0x0A]`; written through the
        // pad at its asm-proven offset. header_request filed to carve it as
        // `s32 miNumLossesForGameMode; // +0xD4`.
        const s32 liNumLosses = lpProfile->GetNumLossesForGameMode(meCurrentGameModeType);
        std::memcpy(&lAction.maPadD0[0x04], &liNumLosses, sizeof(liNumLosses));
    }

    // ---- the progression-rank block ----------------------------------------------------------------
    // [X] [stuntrace] DIVERGENCE -- THE RANK-UP NOTIFICATION ARM IS PARKED ON TWO MISSING MEMBERS.
    // Console:
    //     if (*(mpProgressionManager + 133488) && meCurrentGameModeType < 10) { ...celebration... }
    //     else                                                               { ...plain rank... }
    // where progMgr+133488 (0x20970) is a one-BYTE "a rank-up is pending" flag and progMgr+133472
    // (0x20960) is the QWORD it carries (copied into record+0x38, its non-zero-ness into record+0xDD);
    // the arm then CLEARS both. Neither exists in BrnProgressionManager.h -- the nearest declared
    // members are mi8ProgressionRank @+133484 and mbUpdateRivalsRequested @+133489, so +133488 and
    // +133472 are genuinely unnamed. header_request filed naming AGENT 10.
    // Behavioural cost while parked: the "you ranked up / you unlocked X" fields of the results
    // record stay zero; the ordinary rank fields below are filled exactly as the console fills them.
    //
    // [!!] THIS PARK AND PROGRESSION PARK P8 ARE **ONE BLOCKING ITEM**. NEITHER LANDS ALONE.
    // (Cross-seam audit S1c, 2026-08-26; the twin of this paragraph is on P8's own banner at
    // BrnProgressionManager_EventFinish.cpp:558.)
    //   * P8 is the next-rank CAR UNLOCK. Its whole output is `lpAction->mu64Field40 = lCarId;
    //     lpAction->mbHasField40 = true` -- record+0x40 and record+0xDE
    //     (asm 0x823A0584 `std r4, 0x40(r19)` / 0x823A058C `stb r27, 0xDE(r19)`, r19 == lpAction).
    //   * ShowModeResults runs AFTER OnEventFinishUpdateProfile returns and FORKS on the byte this
    //     park hardcodes false. The celebration arm (`0x82343D24 ld r11, var_140`) READS record+0x40
    //     and carries it through. The PLAIN arm -- the one `lbRankUpNotificationPending = false`
    //     forces -- ZEROES it: `0x82343DDC std r25, var_140` / `0x82343DE0 stb r25, var_A2`, which
    //     is exactly the `mu64Field40 = 0` / `mbHasField40 = 0` pair at the tail of this block.
    //   => Landing P8 while THIS park stands computes the unlock and then throws it away, silently,
    //     with no diagnostic and no assert -- the placeholder-zero failure mode the shadow-system
    //     campaign lost five days to. Landing THIS one while P8 stands is harmless but pointless
    //     (the celebration arm would carry a zero).
    // UN-PARK ORDER: P8's ProgressionRankData header AND the progMgr +0x20970 / +0x20960 accessors
    // in the SAME change, then delete both banners together. (For the record: the +0x20970 flag's
    // WRITER is neither of these two functions -- OnEventFinishUpdateProfile's whole export was
    // grepped and never touches it -- so a third, still-unfound function has to land with them.)
    {
        const bool lbRankUpNotificationPending = false;   // console: *(mpProgressionManager + 133488) != 0
        (void)lbRankUpNotificationPending;

        if (mpProgressionManager->PlayerHasFinishedLastRank())
        {
            lAction.mu8FieldDB = 1;    // record+0xDB
            lAction.miField48  = 0;    // record+0x48
            lAction.mu8FieldDA = 0;    // record+0xDA  (overwrites the first-win byte above)
        }
        else
        {
            // Console `extsb r11, r3` -- the rank is sign-extended from a byte before the store.
            lAction.miField48 = static_cast<s32>(
                static_cast<s8>(mpProgressionManager->GetProgressionRank()));   // record+0x48
        }

        lAction.mu8FieldDC   = 0;    // record+0xDC
        // record+0x38 = 0 -- inside ShowModeResultsAction's maPad1C[0x24]; already zero from the
        // memset, so the console's `std r25, var_148` is a no-op here (noted, not faked).
        lAction.maPadDD[0]   = 0;    // record+0xDD
        lAction.mu64Field40  = 0;    // record+0x40
        lAction.mbHasField40 = 0;    // record+0xDE
        lAction.miField4C    = -1;   // record+0x4C
    }

    // ---- post the record -----------------------------------------------------------------------------
    lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                GameStateModuleIO::E_ACTION_SHOW_MODE_RESULTS,
                                static_cast<s32>(sizeof(GameStateModuleIO::ShowModeResultsAction)));

    // ---- the two follow-on actions -------------------------------------------------------------------
    if (lAction.maPadE5[0] != 0)   // record+0xE5, raised above
    {
        // Console re-reads the MODE TYPE out of the record (`lwz r11, var_180`) and skips this post
        // for the showtime pair.
        const bool lbShowtime = (lAction.meGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME) ||
                                (lAction.meGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);
        if (!lbShowtime)
        {
            // 48 bytes, all zero except an s32 2 at +0x00 and an explicit zero byte at +0x2C. The
            // tree posts the same id/size from the junkyard exit with 1 at +0x00
            // (BrnCarSelectManager.cpp:1153) -- see E_ACTION_SET_PLAYER_CAR_DRIVER.
            u8 lacSetPlayerCarDriver[48];
            std::memset(lacSetPlayerCarDriver, 0, sizeof(lacSetPlayerCarDriver));
            const s32 liDriverSelector = 2;
            std::memcpy(lacSetPlayerCarDriver + 0x00, &liDriverSelector, sizeof(liDriverSelector));
            lacSetPlayerCarDriver[0x2C] = 0;
            lpGameActionQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(lacSetPlayerCarDriver),
                GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER, 48);
        }
    }

    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_RACE)   // console `cmpwi r11, 0xA`
    {
        s32 liTrainingType = KI_TRAINING_TYPE_AFTER_ONLINE_RACE;   // `li r11, 0x3E` == 62
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liTrainingType),
                                    GameStateModuleIO::E_ACTION_REQUEST_GAME_TRAINING, 4);
    }
}

} // namespace BrnGameState
