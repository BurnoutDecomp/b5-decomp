// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_Prepare.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// Wave-B keystone, AGENT 3 -- the setup/prepare leg of the mode spine:
//
//   ModeManager::SetupGameMode              X360 0x8234B158
//   ModeManager::PrepareForMode             X360 0x82342930   (the 2272-byte action-23 post)
//   ModeManager::HandleLoadingScreenLoaded  X360 0x8234B8A8
//   ModeManager::ResetNextLandmarks         X360 0x82328460
//
// Console call order this file sits in (grouping sheet):
//   StartGameMode -> (SetUpCheckPoints/Pathfinding/OpponentData/Districts) -> SetupGameMode ->
//   PrepareForMode -> [HandleLoadingScreenLoaded -> PrepareForMode again] -> PreWorldUpdate ...
//
// EVERY store below is reconstructed from the export's ASSEMBLY. The Hex-Rays pseudocode for both
// big bodies is register-pair garbage in the places that matter (hazards H9): it renders the
// `std r20(=0), this+0x8C00` zero store as `*(a1 + 35840) = v5` with v5 carrying a leftover string
// pointer in its high word, it renders the inlined GetLandmarkFromRegionIndex 9-dword copy as
// `__PAIR64__` shuffling, and it renders the GameModeParams member reads as `*(v6 + 82)` /
// `v6[148]` on a char* that is really a dword base.
//
// [X] DO NOT re-implement anything from hazards H2's list of 16 committed bodies -- call them.
//     (This file calls MarkCarHittingCheckpoint, one of them.)

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

// The header deliberately BANS BrnGameStateModuleIO.h (the dual-scope EActiveRaceCarIndex hazard,
// BrnModeManager.h:53-61) and tells each agent to include it in its OWN partfile instead. None of
// the four signatures bodied here names EActiveRaceCarIndex, so the scope question stays local.
#include "GameSource/GameState/BrnGameStateModuleIO.h"                        // OutputBuffer::GetGameActionQueue
#include "GameSource/GameState/BrnGameStateModule.h"                          // GameStateModule (mpGameStateModule reads)
#include "GameSource/GameState/NetworkRoundManager/BrnNetworkRoundManager.h"  // the round index + the StartNetworkGameEvent
#include "GameSource/GameState/Progression/BrnProgressionManager.h"           // ProgressionManager::GetProgressionData
#include "SharedClasses/Progression/BrnProgressionData.h"                     // BrnProgression::ProgressionData
#include "SharedClasses/Trigger/BrnTriggerBase.h"                             // TriggerRegion::GetBoxRegion / GetId
#include "SharedClasses/Trigger/BrnRegion.h"                                  // BrnTrigger::BoxRegion (36 B on console)

#include <cstring>   // memcpy -- the console's own three memcpy call sites
#include <cstddef>   // offsetof -- the action-24 wire-format pin below

namespace BrnGameState
{

// ----------------------------------------------------------------------------
// TU-local constants. Asm-cited, none invented.
// ----------------------------------------------------------------------------

// SetupGameMode's online-round assert compares `cmplwi r26, 0xA` (0x8234B254) and its message names
// GameStateModuleIO::KU_MAX_ONLINE_ROUNDS_IN_MODE. That constant has NO home in this tree yet
// (BrnGuiCache.h:1428 only mentions it in a comment), so it is spelled here from the asm and filed
// as a header_request against BrnGameStateSharedIO.h. Do not fork a second copy: delete this the
// moment the real constant lands.
static const u32 KU_MAX_ONLINE_ROUNDS_IN_MODE = 10;

// The two game-action ids this file posts. NEITHER has an enumerator in BrnGameActions.h's
// EGameActionType yet, and inventing a console NAME for them would be a fabrication -- so they are
// spelled by VALUE with the producing instruction cited, and the naming is filed as a
// header_request for whoever identifies the consumers.
//   id 22: `li r5, 0x16` + `li r6, 1`     @0x8234B4B8 / 0x8234B4B4 (SetupGameMode, 1-byte payload)
//   id 24: `li r5, 0x18` + `li r6, 0x30`  @0x82342F60 / 0x82342F5C (PrepareForMode, 48-byte payload)
// (E_ACTION_PREPARE_FOR_MODE == 23 IS enumerated and is used by name below.)
//
// [x] BOTH NAMED AND ENUMERATED 2026-08-26 (stuntrace waveB CLOSURE round), and the numerics are
// retired -- the call sites below now use GameStateModuleIO::E_ACTION_CHECK_FOR_LOADING_SCREEN
// (22) and GameStateModuleIO::E_ACTION_BROADCAST_MODE_FINISH_LINES (24). Both names are the DWARF
// enumerators (18 and 20) at the +4 shift this band carries, and the shift is now producer-pinned
// on BOTH sides of them: PREPARE_FOR_MODE 19 -> 23 below, and ModeManager::MarkedManLoaded posting
// id 31 == DWARF 27 E_ACTION_MARKED_MAN_LOADED above. Each name also matches its producer's role
// -- SetupGameMode is the console function that then waits on HandleLoadingScreenLoaded, and the
// 48-byte post is a per-landmark trigger box + CgsID broadcast. The names are FLAGGED in the
// header as BAND-derived, not producer-symbol-pinned; the VALUES are pinned.

// ----------------------------------------------------------------------------
// The two ad-hoc action payloads the console builds on the stack. Neither has a named record type
// in BrnGameActions.h; both are pinned by the AddEvent size argument, so they are declared here as
// TU-local records rather than fabricated as offsets into some other struct.
//
// [!] ANONYMOUS NAMESPACE, ADDED 2026-08-26 (stuntrace waveB CLOSURE round). These two were the
// only records in the ModeManager partfile set declared at plain `namespace BrnGameState` scope,
// i.e. with EXTERNAL linkage -- BrnModeManager_Start.cpp, BrnModeManager_TransmitCrash.cpp and
// BrnModeManager_UpdateMode.cpp all open `namespace {` for theirs. A future
// `BrnGameState::ModeLandmarkAction` in any header would then be a silent ODR mismatch rather
// than a compile error. Internal linkage removes that whole class of failure.
// ----------------------------------------------------------------------------
namespace
{

// SetupGameMode's loading-screen post: the console hands AddEvent a stack slot it NEVER WRITES and
// a size of ONE (`addi r4, r1, var_B0` with no preceding store; `li r6, 1`). It is a bare tag
// action -- the payload byte carries no information.
struct ModeLoadingScreenAction
{
    u8 muTag;
};

// PrepareForMode's landmark post: 36 bytes copied verbatim off the Landmark (its TriggerRegion base
// begins with BoxRegion, and the copy is exactly 9 dwords -- `li r9, 9` + lwz/stw loop
// @0x82342F3C..0x82342F54), then the landmark's CgsID sign-extended into the 8-byte slot at +40
// (`lwz r11, 0x24(r31); extsw r11, r11; std r11, var_988` -- var_988 == var_9B0 + 40). Total 48,
// which is the size AddEvent is given.
struct ModeLandmarkAction
{
    BrnTrigger::BoxRegion mBoxRegion;    // +0x00, 36 B (9 dwords)
    CgsID                 mLandmarkId;   // +0x28 (40)
};

}  // anonymous namespace

// ---- WIRE-FORMAT PINS (hazards H5; header_grow_spec section 3.4 requires them) ---------------
// [!] FIX ROUND 2026-08-26: both records are posted through the size-DEDUCING AddEvent<EventT>
// overload, so nothing was checking that the host layout still matched the console's `li r6, <n>`.
// An unpinned 48 is exactly the silent wire-format break H5 exists to prevent. Same treatment
// BrnModeManager_Start.cpp:196-221 already gives its nine records.
//
// Console evidence, re-derived from the exports this round:
//   PrepareForMode @0x82342930 -- `li r6, 0x30` (48) @0x82342F5C paired with `li r5, 0x18` (24)
//   @0x82342F60. The payload is built at stack base var_9B0: a 9-dword copy loop
//   (`li r9,9 / mtctr / lwz+stw` @0x82342F3C..0x82342F54) fills +0x00..+0x23, then
//   `lwz r11, 0x24(r31) / extsw r11, r11 / std r11, var_988` @0x82342F58/64/70 writes the id.
//   var_9B0 - var_988 == 0x28 == 40, which is the offsetof pinned below.
//   BoxRegion is 9 x f32 (BrnRegion.h:83-91) == 36; CgsID is a u64, so it aligns to 40 and the
//   record closes at 48.
//   SetupGameMode @0x8234B158 -- `li r6, 1` @0x8234B4B4 paired with `li r5, 0x16` (22)
//   @0x8234B4B8: a one-byte bare tag.
static_assert(sizeof(ModeLandmarkAction) == 48,
              "X360 PrepareForMode posts action 24 with size 48 (`li r6,0x30` @0x82342F5C)");
static_assert(offsetof(ModeLandmarkAction, mLandmarkId) == 40,
              "X360 PrepareForMode writes the landmark CgsID at record+40 (var_988 == var_9B0+0x28)");
static_assert(sizeof(ModeLoadingScreenAction) == 1,
              "X360 SetupGameMode posts action 22 with size 1 (`li r6,1` @0x8234B4B4)");

// ============================================================================
// ModeManager::ResetNextLandmarks -- X360 0x82328460
// ============================================================================
// Re-arm every global race car's checkpoint tracker for the current landmark set, then clear the
// two "has reached" bit sets.
//
// The console body inlines TWO things:
//   * CarCheckpointData::SetupCheckpoints(muNumLandmarks) per car (a real call in the asm), and
//   * ModeManager::MarkCarHittingCheckpoint(0, car) -- de-inlined back to the COMMITTED body here
//     (hazards H2). It is recognisable by its own three asserts, which the console emits verbatim
//     at BrnModeManager.cpp:5515/5517/5518: "static_cast<uint32_t>( liCheckpointIndex ) <
//     muNumLandmarks", "leGlobalRaceCarIndex >= E_GLOBAL_RACE_CAR_INDEX_0" and
//     "leGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT".
// The fourth assert in the export ("leEnumIndex <= E_GLOBAL_RACE_CAR_INDEX_COUNT",
// BurnoutConstants.h:84) belongs to the inlined ++EGlobalRaceCarIndex iterator, not to this body.
//
// ARGUMENT SENSE (asm 0x823284B8 onward -- `cmplwi r21,0` then skip): the extra
// MarkCarHittingCheckpoint(0) pass runs when the argument is FALSE. lbResetAll == true is the
// "start of event" reset that leaves checkpoint 0 outstanding; SetupGameMode calls it that way
// (`li r4, 1` at its ResetNextLandmarks call site).
void ModeManager::ResetNextLandmarks(bool lbResetAll)
{
    for (s32 liGlobalRaceCarIndex = 0;
         liGlobalRaceCarIndex < E_GLOBAL_RACE_CAR_INDEX_COUNT;
         ++liGlobalRaceCarIndex)
    {
        maCarCheckpointData[liGlobalRaceCarIndex].SetupCheckpoints(static_cast<s32>(muNumLandmarks));

        if (!lbResetAll)
        {
            MarkCarHittingCheckpoint(0u, static_cast<EGlobalRaceCarIndex>(liGlobalRaceCarIndex));
        }
    }

    // X360 `std r20(=0), 0(this+0x8BF0)` / `std r20, 0(this+0x8BF8)` @0x8232857C -- a whole-array
    // 64-bit zero of each single-field BitArray<35>, i.e. UnSetAll().
    mRaceCarReachedCheckpoint.UnSetAll();
    mRaceCarReachedFinish.UnSetAll();
}

// ============================================================================
// ModeManager::HandleLoadingScreenLoaded -- X360 0x8234B8A8
// ============================================================================
// The loading-screen half of the two-step start: when the current mode has a loading screen,
// SetupGameMode posts the action-22 tag INSTEAD of preparing, and this is where the prepare finally
// happens once the screen has loaded.
//
// GATE: `lwz r11, 0(r3); lwz r11, 0x60(r11)` == vtbl+96 == slot 24 == GameMode::HasLoadingScreen()
// (this wave's vtable micro-check). StuntAttackMode's vtable 0x820D0720 carries the folded
// `li r3,0; blr` leaf (0x827E2F38) at slot 24, so a stunt race NEVER reaches this function -- it
// takes SetupGameMode's else-arm instead.
void ModeManager::HandleLoadingScreenLoaded(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (mpCurrentGameMode == NULL)
    {
        return;
    }
    if (!mpCurrentGameMode->HasLoadingScreen())
    {
        return;
    }

    CGS_ASSERT(lpGameActionQueue != NULL, "lpActionQueue");
    CGS_ASSERT(mpNetworkRoundManager != NULL, "mpNetworkRoundManager");

    // X360 @0x8234B954..0x8234B964: `lwz r10, 0x12C(r11)` / `lwz r11, 0x128(r11)` / `subf r11,r11,r10`
    // / `addi r6, r11, -1` -- miTotalRounds(+300) minus miRoundsRemaining(+296) minus one == the
    // zero-based index of the round about to start.
    // [!] FIX ROUND 2026-08-26 -- INVERTED EXPRESSION CORRECTED. This site used to spell that as
    // `GetTotalRounds() - GetCurrentRound() - 1`, which under the tree's ratified convention
    // evaluates to miRoundsRemaining, NOT the round index. THE CONVENTION: GetCurrentRound() IS the
    // whole console expression NRM+300 - NRM+296 - 1. It is fixed by three landed sites, not by
    // this wave -- BrnGameStateRichPresenceManagerBase.cpp:173-185 (committed, its own comment spells
    // out "NRM+300 - NRM+296 - 1 ... those are the NetworkRoundManager public accessors
    // GetTotalRounds() and GetCurrentRound()"), BrnModeManager_Accessors.cpp:355-367, and
    // BrnModeManager_Start.cpp:603 (which stores GetCurrentRound() RAW into
    // StopModeAction::miField08, documented at BrnGameActions.h:1119 as "round index derived from
    // the network round manager"). Under that convention the console arithmetic IS GetCurrentRound().
    const s32 liCurrentRound = mpNetworkRoundManager->GetCurrentRound();

    PrepareForMode(lpGameActionQueue, &mCurrentGameModeParams, liCurrentRound, &mStartGameModeParams);
}

// ============================================================================
// ModeManager::SetupGameMode -- X360 0x8234B158
// ============================================================================
// Second half of the start sequence: reset the per-event latches, hand the mode + params to the
// ScoringSystem, publish the checkpoint/landmark tables built by SetUpCheckPointsForGameMode, and
// either prepare the world immediately or defer to the loading screen.
//
// TWO ARMS, and the split is on the CURRENT mode type, not on online-ness:
//   `lwz r4, 0xD94(r31); addi r11, r4, -3; cntlzw; extrwi r29, r11, 1,26`
//   == (meCurrentGameModeType == E_MODE_ROAD_RAGE).
// Road rage skips the whole checkpoint/landmark leg (it has none -- the arm ends on the assert
// "muNumLandmarks == 0") and instead seeds the road-rage scorer from the player's progression rank.
void ModeManager::SetupGameMode(GameStateModuleIO::OutputBuffer* lpOutputBuffer,
                                GameModeParams*                  lpGameModeParams,
                                StartGameModeParams*             lpStartGameModeParams)
{
    // X360 `lwz r11, 0x6D60(this); addi r3, r11, 0x620; bl TriggerData::GetMemory` -- the mode's
    // TriggerData through the TriggerQueryManager's ResourcePtr (accessor grow (5); never the raw
    // TQM+1568).
    const BrnTrigger::TriggerData* lpTriggerData = GetTriggerData();

    // X360 @0x8234B190..0x8234B19C: `lwz r10, 0x12C(r11)` / `lwz r11, 0x128(r11)` /
    // `subf r11,r11,r10` / `addi r26, r11, -1` (r11 = mpNetworkRoundManager, loaded from
    // this+0x6D64 at 0x8234B184) -- miTotalRounds - miRoundsRemaining - 1.
    // [!] FIX ROUND 2026-08-26 -- INVERTED EXPRESSION CORRECTED, same defect and same reasoning as
    // HandleLoadingScreenLoaded above: GetCurrentRound() IS that whole expression under the tree's
    // ratified convention (BrnGameStateRichPresenceManagerBase.cpp:173-185 /
    // BrnModeManager_Accessors.cpp:355-367 / BrnModeManager_Start.cpp:603). Writing
    // `GetTotalRounds() - GetCurrentRound() - 1` here produced miRoundsRemaining, which fed
    // PrepareForModeAction::miCurrentRound (consumed by
    // GameBridgeGameStateToX_EventFlowGuiEvents.cpp:528), the `liCurrentRound == 0` gate on
    // ClearCumulativeData, and the KU_MAX_ONLINE_ROUNDS_IN_MODE assert below.
    const s32 liCurrentRound = mpNetworkRoundManager->GetCurrentRound();

    // The console fires the SAME message twice, at BrnModeManager.cpp:1162 AND :1163 (asm
    // 0x8234B1B4 and 0x8234B1D4 -- two Begin/Fire/End sequences sharing r30). Reproduced verbatim.
    CGS_ASSERT(lpGameModeParams != NULL, "lpGameModeParams != NULL");
    CGS_ASSERT(lpGameModeParams != NULL, "lpGameModeParams != NULL");
    CGS_ASSERT(lpStartGameModeParams != NULL, "lpStartGameModeParams != NULL");
    CGS_ASSERT(lpTriggerData != NULL, "lpTriggerData != NULL");
    CGS_ASSERT(!mpCurrentGameMode->IsOnline()
                   || (liCurrentRound >= 0
                       && static_cast<u32>(liCurrentRound) < KU_MAX_ONLINE_ROUNDS_IN_MODE),
               "!mpCurrentGameMode->IsOnline() || (liCurrentRound >= 0 && uint32_t(liCurrentRound) < GameStateModuleIO::KU_MAX_ONLINE_ROUNDS_IN_MODE)");

    const bool lbRoadRage = (meCurrentGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE);

    if (mpCurrentGameMode->IsOnline())
    {
        mScoringSystem.StartOnlineGameModeScoring(meCurrentGameModeType);
    }

    // The per-event latch reset. Nine stores, in console order (asm 0x8234B2A0..0x8234B318): two
    // f32 clocks through flt_82001CC0 (DUMPED FROM THE IMAGE this session -- offset 0x1CC0 reads
    // 00 00 00 00, so the constant IS 0.0f, not a placeholder) and seven byte flags 0x94FB..0x9501.
    mfTimeInFreeBurn             = 0.0f;   // +0x951C
    mbHasTimedOut                = false;  // +0x94FB
    mbHasCrashedOut              = false;  // +0x94FC
    mfTimeInMode                 = 0.0f;   // +0x9520
    mbPlayerFinishedTimedOut             = false;  // +0x94FD
    mbPlayerFinishedCarDestroyed  = false;  // +0x94FE
    mbHasPlayerFinished          = false;  // +0x94FF
    mbModeStartFromRegionEnabled = false;  // +0x9500
    mbModeDataIsLoading          = false;  // +0x9501

    // First round of a (possibly multi-round) event: wipe the cumulative scoring.
    // DE-INLINED, EXACTLY: the console's sixteen stores (ss+0x5030 + k*0x158 -> {0, -1}, k = 0..7)
    // followed by OnlineGameResults::Clear(ss+0x4DD0) ARE ScoringSystem::ClearCumulativeData
    // @0x8231F140 -- that body is those same seventeen operations and nothing else, verified
    // store-for-store against its export this session. No raw ScoringSystem offsets needed.
    if (liCurrentRound == 0)
    {
        mScoringSystem.ClearCumulativeData();
    }

    mpCurrentGameMode->Initialise();      // vtbl+16 == slot 4

    // `lbRestart` is the NEGATION of the instant-intro composite: the console builds
    // (mode == 15 || mode == 16) && mbInstantIntroSplash into r11 and passes r6 = !that. That
    // composite is exactly IsOnlineModeWithInstantIntro().
    //
    // [!] SIGNATURE DIVERGENCE (header_request against BrnScoringSystem.h): the X360 passes a
    // FOURTH argument -- `lwz r11, 0x6D64(this); lwz r7, 0x12C(r11)` @0x8234B3CC/0x8234B3DC, i.e.
    // mpNetworkRoundManager->GetTotalRounds(). The committed declaration is the PS3 DWARF's 3-arg
    // form, so the round count is DROPPED here. Any OnModeStart leg that scales with the number of
    // rounds is inert until the 4th parameter lands. Console call:
    //     ScoringSystem::OnModeStart(&mScoringSystem, meCurrentGameModeType, lpGameModeParams,
    //                                !IsOnlineModeWithInstantIntro(),
    //                                mpNetworkRoundManager->GetTotalRounds());
    mScoringSystem.OnModeStart(meCurrentGameModeType, lpGameModeParams, !IsOnlineModeWithInstantIntro());

    // ------------------------------------------------------------------------------------------
    // THE STREAMING GATE. Console @0x8234B3E4..0x8234B420:
    //     lwz  r3, 0xD98(r31) / lwz r11, 0(r3) / lwz r11, 0x5C(r11) / mtctr / bctrl
    //         -- vtbl+0x5C == vtbl+92 == slot 23 == GameMode::RequiresStreaming()
    //     clrlwi r11, r3, 24 / cmplwi 0 / beq  -> skip
    //     stbx r15(1), r31, 0x9503                       ; mbIsModePrepared = true
    //     bl   OutputBuffer::GetGameActionQueue / lwz r3, 0x6D58(r31)
    //     bl   GameStateModule::WaitForStreaming         ; PARKED, see below
    // [x] BLOCKER (1) DISCHARGED 2026-08-26 (fix round): BrnGameMode.h now carries the console
    // 26-slot order and declares `virtual bool RequiresStreaming() const;` at slot 23, so the GATE
    // AND ITS STORE ARE NOW EMITTED. They used to be lost together with the parked call, which meant
    // mbIsModePrepared was never set on any streaming mode.
    // [X] BLOCKER (2) STILL OPEN -- GameStateModule::WaitForStreaming has no declaration OR body
    // anywhere in the tree (grep: only comments), and conductor decision #6 puts it in the
    // detection/start-driver wave, not this one. Only that ONE call stays parked; re-arm with
    //     mpGameStateModule->WaitForStreaming(lpOutputBuffer->GetGameActionQueue());
    // BEHAVIOURAL IMPACT ON THIS CAMPAIGN: NONE. StuntAttackMode's vtable (0x820D0720) carries the
    // folded `li r3,0; blr` leaf (0x827E2F38) at BOTH slot 23 and slot 24, so a stunt race enters
    // neither this arm nor the HandleLoadingScreenLoaded path. Base GameMode::RequiresStreaming()
    // returns TRUE, so this arm IS live for race / face-off / pursuit / survivor.
    // [!] +0x9503 NAMING NOTE for the verifier's bool-block collision pass (hazards H4): this is
    // the ONLY writer of 0x9503 in the wave's four bodies here, and its semantics are "the mode
    // needs streaming and we have just asked for it", NOT "the mode is prepared". The frozen
    // header's provisional `mbIsModePrepared` is CONTESTED by that; reported, not renamed here.
    // ------------------------------------------------------------------------------------------
    if (mpCurrentGameMode->RequiresStreaming())   // vtbl+92 == slot 23
    {
        mbIsModePrepared = true;                 // console stbx r15(1), r31, 0x9503
        // [X] PARKED CALL (blocker 2 above):
        //     mpGameStateModule->WaitForStreaming(lpOutputBuffer->GetGameActionQueue());
    }

    if (lbRoadRage)
    {
        // ---- shared tail (the console emits it twice; this is the road-rage copy) -------------
        // [!] WIRE-FORMAT DIVERGENCE, already bannered in BrnModeManager.h: the console memcpy
        // sizes are 832 (0x340) and 2160 (0x870); the host types measure 864 and 1680. sizeof is
        // used so the host copy stays whole -- the SIZE MISMATCH is a BrnGameModeParams.h
        // reconstruction hole, reported as a wave item, not papered over with the console numbers.
        memcpy(&mStartGameModeParams, lpStartGameModeParams, sizeof(mStartGameModeParams));
        memcpy(&mCurrentGameModeParams, lpGameModeParams, sizeof(mCurrentGameModeParams));

        if (mpCurrentGameMode->HasLoadingScreen())   // vtbl+96 == slot 24
        {
            CGS_ASSERT(lpOutputBuffer->GetGameActionQueue() != NULL, "lpOutput->GetGameActionQueue()");
            ModeLoadingScreenAction lLoadingScreenAction;
            lLoadingScreenAction.muTag = 0;   // console leaves the slot UNWRITTEN (see struct banner)
            lpOutputBuffer->GetGameActionQueue()->AddEvent(&lLoadingScreenAction, GameStateModuleIO::E_ACTION_CHECK_FOR_LOADING_SCREEN);
        }
        else
        {
            PrepareForMode(lpOutputBuffer->GetGameActionQueue(), &mCurrentGameModeParams,
                           liCurrentRound, &mStartGameModeParams);
        }
        // ---- end shared tail ------------------------------------------------------------------

        const s32 liTakedownTarget = static_cast<s32>(GetRoadRageTakedownTarget());

        const BrnProgression::ProgressionData* lpProgressionData = mpProgressionManager->GetProgressionData();
        CGS_ASSERT(lpProgressionData != NULL, "lpProgressionData != NULL");

        // --------------------------------------------------------------------------------------
        // [X] FRONTIER -- THE ROAD-RAGE PROGRESSION SEED IS PARKED. Console (asm 0x8234B544..
        // 0x8234B5D8), verbatim:
        //     const ProgressionRankData* lpProgressionRankDataRank = lpProgressionData->
        //         GetProgressionRankData(mpProgressionManager->GetProgressionRankForGameMode(
        //                                    GameStateModuleIO::E_MODE_ROAD_RAGE));
        //     CGS_ASSERT(lpProgressionRankDataRank, "lpProgressionRankDataRank");   // :1241
        //     // ss+0x4B40..0x4B57 -- an inlined RoadRageModeScoring::Prepare, field for field:
        //     //   miNumTakedownsAchieved                 = 0          (stw 0x4B40)
        //     //   miNumTakedownsAchievedForNextExtention = 0          (stw 0x4B44)
        //     //   muRoadRageTriggerExtension             = 1          (sth 0x4B48)
        //     //   muRoadRageExtensionTime                = *(rank+86) (sth 0x4B4A, `lhz 0x56`)
        //     //   miTargetNumTakedowns                   = target     (stw 0x4B4C)
        //     //   miNextTimeIncreaseIndex                = 0          (stw 0x4B50)
        //     //   mbDamageCriticalMessageNeedToBeSent    = 0          (stb 0x4B54)
        //     //   mbPlayerDamageCritical                 = 0          (stb 0x4B55)
        //     //   mbPlayerCarDestroyed                   = 0          (stb 0x4B56)
        //     //   mbGameModeActive                       = 1          (stb 0x4B57)
        //     mScoringSystem.GetRoadRageScoring()->Prepare(liTakedownTarget, <rank+86>);
        //     // ss+0x4B60/64/68 -- the road-rage medal SCORES:
        //     mauiMedalScores[0] = target; [1] = target - 1; [2] = target - 2;
        //     // ss+0x5D08 / 0x5D0C:
        //     meCurrentMedalTarget   = E_CURRENT_MEDAL_TARGET_TIME_GOLD;   // 0
        //     meCurrentMedalAchieved = E_CURRENT_MEDAL_TARGET_TIME_NONE;   // 3
        // THREE missing declarations, all filed as header_requests:
        //   (a) ProgressionManager::GetProgressionRankForGameMode(EGameModeType) -- X360
        //       0x8237B4E8, absent from the tree entirely (grouping-sheet frontier, shared with the
        //       StuntAttackMode wave). WITHOUT IT THE RANK CANNOT BE OBTAINED, so everything
        //       downstream of it parks with it.
        //   (b) ProgressionRankData::GetRoadRageExtensionTime() -- the u16 at rank+0x56.
        //   (c) ScoringSystem setters for mauiMedalScores / meCurrentMedalTarget /
        //       meCurrentMedalAchieved (no setter exists; the members are private).
        // LAYOUT FINDING for BrnScoringSystem.h (asm-pinned, DWARF order refuted): the three `stw`
        // at ss+0x4B60/64/68 land on mauiMedalScores, and mOnlineRaceModeScoring starts at
        // ss+0x4B74, so the run after RoadRageModeScoring (24 B @0x4B40) is
        //   miMaximumPlayerCrashedNumber(0x4B58) miCurrentPlayerCrashedNumber(0x4B5C)
        //   mauiMedalScores[4](0x4B60..0x4B70) mbPlayerTotalled(0x4B70)
        // -- i.e. mauiMedalScores sits ABOVE mbPlayerTotalled, not below it as the DWARF order in
        // the committed header has it. ss+0x5D08 / 0x5D0C are independently confirmed as the medal
        // target/achieved pair by ScoringSystem::SetMedalModeTimer @0x823108E0, which writes 0 to
        // both of them.
        // BEHAVIOURAL IMPACT: ROAD RAGE ONLY (this whole arm is gated on
        // meCurrentGameModeType == E_MODE_ROAD_RAGE). Zero impact on the stunt-race campaign.
        // --------------------------------------------------------------------------------------
        // ⭐ [road-rage wave 2026-09-02, conductor] SEED UN-PARKED. All three blockers above are gone:
        //   (a) ProgressionManager::GetProgressionRankForGameMode -- bodied (BrnProgressionManager.cpp:1299);
        //   (b) ProgressionRankData::GetRoadRageExtensionTime  -- BrnProgressionRankData.h:145 (rank+0x56);
        //   (c) the medal stores -- written by name under the friend grant in BrnScoringSystem.h.
        // Store for store against 0x8234B544..0x8234B5D8 (asm re-read this wave); the block is
        // what makes RoadRageModeScoring::IsActive() true, i.e. what lets WriteDataToOutput publish
        // the takedown target instead of 0/0 (run1 witness: 10,373 HUD "Invalid takedown target"
        // asserts from exactly this park).
        const BrnProgression::ProgressionRankData* lpProgressionRankDataRank =
            lpProgressionData->GetProgressionRankData(static_cast<u32>(
                mpProgressionManager->GetProgressionRankForGameMode(
                    GameStateModuleIO::E_MODE_ROAD_RAGE)));                           // 0x8234B544..0x8234B55C
        CGS_ASSERT(lpProgressionRankDataRank != NULL, "lpProgressionRankDataRank");  // :1241

        // ss+0x4B40..0x4B57 -- the inlined RoadRageModeScoring::Prepare (0x8234B588..0x8234B5C4).
        mScoringSystem.GetRoadRageScoring()->Prepare(
            liTakedownTarget, lpProgressionRankDataRank->GetRoadRageExtensionTime());

        // ss+0x5D08 / 0x5D0C (0x8234B5C8 / 0x8234B5CC): medal target GOLD, achieved NONE.
        mScoringSystem.meCurrentMedalTarget   = E_CURRENT_MEDAL_TARGET_TIME_GOLD;      // 0
        mScoringSystem.meCurrentMedalAchieved = E_CURRENT_MEDAL_TARGET_TIME_NONE;      // 3

        // ss+0x4B60/64/68 (0x8234B5D0..0x8234B5D8): the road-rage medal SCORES, target / -1 / -2.
        mScoringSystem.mauiMedalScores[0] = static_cast<u32>(liTakedownTarget);
        mScoringSystem.mauiMedalScores[1] = static_cast<u32>(liTakedownTarget - 1);
        mScoringSystem.mauiMedalScores[2] = static_cast<u32>(liTakedownTarget - 2);

        CGS_ASSERT(muNumLandmarks == 0, "muNumLandmarks == 0");
        return;
    }

    // ---- the checkpointed-mode arm ------------------------------------------------------------
    // ss+0x4ED8 (`stw r15(=1), 0x5C88(this)`; 0x5C88 - 0xDB0 == 0x4ED8 == muTotalLaps).
    mScoringSystem.SetTotalLaps(1u);

    if (mpCurrentGameMode->IsOnline())
    {
        // ------------------------------------------------------------------------------------
        // [X] FRONTIER (ONLINE-ONLY) -- the timestep reset is parked. Console:
        //     CgsSystem::TimerRequests::SetTimestepMultiplier(
        //         lpOutputBuffer->GetTimerRequest() + 8, 1.0f);          // flt_82001C98
        // `+ 8` is TimerRequestInterface::mSimTimer (CgsTimerRequestInterface.h:121), so the
        // console call is `...GetTimerRequestInterface()->GetSimTimerRequests()->
        // SetTimestepMultiplier(1.0f)`.
        // BLOCKED BECAUSE: BrnGameStateModuleIO.h:273 models OutputBufferTimerRequestInterface as
        // `{ u8 maOpaque[16] }` -- 16 bytes IS sizeof(CgsSystem::TimerRequestInterface), but the
        // opaque exposes no members, and reinterpret_cast'ing an opaque blob is exactly the kind of
        // fabricated reach this wave bans. Filed as a header_request (retype the opaque, or add a
        // named accessor). Offline events -- the whole stunt-race campaign -- never take this arm.
        // ------------------------------------------------------------------------------------
    }

    // Both GetCheckpointCount() reads below carry the console's inlined Array<T,N>::GetLength()
    // assert ("Array used before Construct/Clear was called", CgsArray.h:336) -- it is the callee's,
    // so it is not duplicated here.
    muNumLandmarks = static_cast<u32>(lpGameModeParams->GetCheckpointCount());

    // Publish the mode's checkpoint table into the three parallel landmark arrays. STUNT NOTE:
    // stunt events author ZERO checkpoints, so this loop runs zero times -- that is DATA, not a bug
    // (scout-proven), and PrepareForMode's mbDistanceToFinishLineTransmitted gate does not depend on
    // the count.
    for (u32 luCheckpoint = 0; luCheckpoint < muNumLandmarks; ++luCheckpoint)
    {
        // ------------------------------------------------------------------------------------
        // [X] FRONTIER -- the per-entry read is parked on ONE missing declaration. Console (asm
        // 0x8234B6AC..0x8234B73C), verbatim:
        //     const CheckpointData* lpCheckpointData =
        //         lpGameModeParams->GetCheckpointData(luCheckpoint);       // Array<..,16>::GetItem
        //     maLandmarkIndices[luCheckpoint] =
        //         static_cast<u16>(lpCheckpointData->GetLandmarkIndex());  // lhz 0(It)/sth -0xA0(r28)
        //     maLandmarkCgsIDs[luCheckpoint] = lpTriggerData->GetLandmarkFromRegionIndex(
        //         static_cast<s16>(maLandmarkIndices[luCheckpoint]))->GetId();
        //         // ^ the two inlined asserts in the export ("liRegionIndex < miRegionCount",
        //         //   BrnTriggerData.h:624, and "lpTriggerRegion->GetType() ==
        //         //   TriggerRegion::E_TYPE_LANDMARK", :615) ARE that callee's own body, and the
        //         //   `lwz 0x24; extsw; std` IS TriggerRegion::GetId() widening its s32 storage.
        //     mauLandmarkSectionIndices[luCheckpoint] = lpCheckpointData->GetAISectionIndex();
        //         // lhz 2(It) / sth 0(r28)
        // BLOCKED BECAUSE: GameModeParams declares GetCheckpointCount() but NOT
        // GetCheckpointData(s32) / GetCheckpoints(), and maCheckpointDataArray is private.
        // StartGameModeParams -- the sibling class in the SAME header -- declares BOTH, so this is
        // a one-line symmetry gap. Filed as a BLOCKING header_request; agent 4's
        // SetUpCheckPointsForGameMode fills the same array and needs the same accessor.
        // BEHAVIOURAL IMPACT: the landmark tables stay unpublished for CHECKPOINTED modes (race /
        // burning route / marked man). ZERO impact on stunt races, which author no checkpoints.
        // ------------------------------------------------------------------------------------
        break;   // the loop body cannot be written until the accessor lands; see the banner
    }

    // ss+0x4EE0 (`stw r11, 0x5C90(this)`) == miTotalCheckpoints. The console re-reads the array
    // length here (its second CgsArray.h:336 assert) rather than reusing muNumLandmarks, and it
    // evaluates the `> 1` test BEFORE that store (`cmplwi r10, 1` at 0x8234B788).
    const bool lbNeedsCheckpointDistances = (muNumLandmarks > 1u);
    mScoringSystem.SetTotalCheckpoints(lpGameModeParams->GetCheckpointCount());

    if (lbNeedsCheckpointDistances)
    {
        mbNeedToSendNextRequest            = true;   // +0x8C0C
        mbIsCalculatingCheckpointDistances = true;   // +0x8C0D
        muNextDistanceRequestCheckpoint    = 0u;     // +0x8C08
    }

    ResetNextLandmarks(true);

    // The console re-asserts the SAME lpTriggerData it captured at entry (BrnModeManager.cpp:1285 --
    // the string/file registers are reloaded off the stack for it).
    CGS_ASSERT(lpTriggerData != NULL, "lpTriggerData != NULL");

    // ---- shared tail (second copy; identical to the road-rage one above) -----------------------
    memcpy(&mStartGameModeParams, lpStartGameModeParams, sizeof(mStartGameModeParams));
    memcpy(&mCurrentGameModeParams, lpGameModeParams, sizeof(mCurrentGameModeParams));

    if (mpCurrentGameMode->HasLoadingScreen())   // vtbl+96 == slot 24
    {
        CGS_ASSERT(lpOutputBuffer->GetGameActionQueue() != NULL, "lpOutput->GetGameActionQueue()");
        ModeLoadingScreenAction lLoadingScreenAction;
        lLoadingScreenAction.muTag = 0;
        lpOutputBuffer->GetGameActionQueue()->AddEvent(&lLoadingScreenAction, GameStateModuleIO::E_ACTION_CHECK_FOR_LOADING_SCREEN);
    }
    else
    {
        PrepareForMode(lpOutputBuffer->GetGameActionQueue(), &mCurrentGameModeParams,
                       liCurrentRound, &mStartGameModeParams);
    }
}

// ============================================================================
// ModeManager::PrepareForMode -- X360 0x82342930
// ============================================================================
// Builds and posts THE 2272-byte action-23 record that arms every world entity module for the
// event, registers each player with the ScoringSystem, optionally arms the 0.2 s second-phase
// re-post, and posts the 48-byte action-24 finish-landmark record.
//
// [!] HAZARD H5 (queue budget): action 23 is 2272 bytes into VariableEventQueue<13312,16> and this
// body can ALSO leave a cached copy that UpdateCurrentMode re-posts 0.2 s later. The original post
// below and that re-post must stay mutually exclusive per frame -- which they are, because the
// cache is stamped E_PFM_STAGE_SECOND_OF_TWO and mbIsWaitingForSecondPFM gates the re-post.
void ModeManager::PrepareForMode(GameStateModuleIO::GameActionQueue* lpGameActionQueue,
                                 GameModeParams*                     lpGameModeParams,
                                 s32                                 liCurrentRound,
                                 StartGameModeParams*                lpStartGameModeParams)
{
    CGS_ASSERT(lpGameModeParams != NULL, "lpGameModeParams != NULL");
    CGS_ASSERT(lpGameActionQueue != NULL, "lpGameActionQueue != NULL");
    CGS_ASSERT(lpStartGameModeParams != NULL, "lpStartGameModeParams != NULL");

    mbAbortedDuringIntro        = false;   // +0x94FA  (Construct does NOT zero this one)
    mbPlayerFinishedCarDestroyed = false;   // +0x94FE
    mbPlayerFinishedTimedOut            = false;   // +0x94FD

    // The record is a plain automatic. The export's pre-Construct `-1` stores (var_700, then sixteen
    // at var_6C8 + k*0x2C, then var_430 and var_118) are the INLINED DEFAULT CONSTRUCTORS of its
    // embedded Array<>s and Array<CheckpointData,16> elements -- 0x2C == 44 == the CheckpointData
    // stride and -1 is CgsArray's "used before Construct/Clear" sentinel. On the host those same
    // constructors run here, for free.
    GameStateModuleIO::PrepareForModeAction lAction;

    // The composite the loop's line-2968 assert compares against (`stb r10, var_A00`): built as
    // (mode == 15 || mode == 16) && mbInstantIntroSplash == IsOnlineModeWithInstantIntro().
    const bool lbInstantIntro = IsOnlineModeWithInstantIntro();

    // [x] WITHDRAWN 2026-08-26 (fix round) -- THE "FIVE REGISTERS / lpStartGameModeParams IS
    // DROPPED" CLAIM THAT USED TO STAND HERE WAS FALSE, and so was the header_request it carried.
    // Re-derived twice, both ends:
    //   (a) CALLER. The argument setup at 0x82342A6C..0x82342A80 writes exactly FOUR registers --
    //       `mr r5, r30` / `lbzx r6, r21, r11(0x9508)` / `mr r4, r29` / `addi r3, r1, var_980` --
    //       then `bl PrepareForModeAction::Construct`. r7 is never written. The only r7 reference in
    //       the whole range 0x82342930..0x82342A80 is `mr r31, r7` at 0x8234295C, which is
    //       PrepareForMode saving its OWN incoming 4th argument (lpStartGameModeParams) into a
    //       non-volatile at entry -- classic IDA register residue, the same artefact already
    //       diagnosed for HUDMessageLogic::Prepare in BrnModeManager_Start.cpp.
    //   (b) CALLEE. PrepareForModeAction::Construct @0x8230FDF0 saves r4/r5/r6 into r29/r28/r27 at
    //       0x8230FDFC..0x8230FE08 and NEVER READS r7 anywhere in its body (its own IDA prototype is
    //       `(a1, a2, a3, char a4)`). It asserts "lpGameModeParams" (BrnGameActions.h line 0x49F),
    //       memcpy's 0x870 from r4 to this+0x30, stores r5 at +0x8A0 and r6 at +0x8D0.
    // The committed 3-arg declaration at BrnGameActions.h:473 is therefore CORRECT AND COMPLETE, the
    // emitted call below is right, and nothing is dropped. Do not grow a 4th parameter.
    // [!] NAMING EVIDENCE for the bool block (hazards H4): the 4th argument's parameter name on this
    // record is `lbComingFromOnlineLobbyMode` (it lands on action+0x8D0), which is direct support
    // for +0x9508's PINNED-semantics reading in the frozen header -- (mode 15/16) is the
    // online-freeburn-lobby / online-showtime pair. Reported, not renamed here.
    lAction.Construct(lpGameModeParams, liCurrentRound, mbInstantIntroSplash);

    // Four fields the console pokes straight after Construct (asm 0x82342A90..0x82342ABC). Each is
    // de-inlined to the record's own named setter; every source offset is pinned by the tree's own
    // offset-faithful StartGameModeParams member run (+796 mfBoostEarning, +800 miShotGroup) and by
    // BrnNetworkRoundManager.h (+304 mbStartingGameDueToPlayerJoin).
    lAction.SetPlayerBoostEarning(lpStartGameModeParams->GetBoostEarning());        // action +0x8C8
    lAction.SetShotGroup(lpStartGameModeParams->GetShotGroup());                    // action +0x8CC
    lAction.SetStartingFreeburnLobbyDueToPlayerJoin(
        mpNetworkRoundManager->GetStartingFreeburnLobbyDueToPlayerJoin());          // action +0x8D2
    // [!] BOOL-BLOCK PIN (hazards H4) -- this is the byte the frozen header carries as the SUSPECT
    // `muUnkByte_0x9507`. It is copied VERBATIM into PrepareForModeAction::mbFinishedOnlineEvent
    // (action+0x8D1: `lbz r11, 0(r27)` where r27 == this+0x9507, then `stb r11, var_AF`), whose
    // named setter is SetFinishedOnlineEvent. That is a writer whose semantics can be stated, so
    // +0x9507 IS mbFinishedOnlineEvent -- and it settles the header's open question, because the
    // DWARF-order candidate `mbFinishedOnlineEvent` was tried at +0x9505 and rejected THERE. It also
    // explains the tail gate below: "the finish-line distance counts as transmitted unless we have
    // just finished an online event and are dropping into the lobby/showtime pair."
    // [OK] APPLIED 2026-08-26 by the wave-B fix round: the member IS mbFinishedOnlineEvent now
    // (three independent pins agree -- this copy, StartModeIntro's action-29 copy, and
    // SendModeStopMessages' writer, whose condition is literally "an online non-lobby,
    // non-showtime mode ended cleanly").
    lAction.SetFinishedOnlineEvent(mbFinishedOnlineEvent);                          // action +0x8D1

    // Player roster. ONE loop, ONE bound: (online ? miNumNetworkPlayers : miNumRivals) + 1 (asm
    // 0x82342AC0..0x82342AD4 -- `lbz 1(r29)` vs `lbz 0(r29)`, extsb, +1). Per hazards H7 the
    // online/offline fork is INSIDE the loop, never around it.
    const s32 liPlayerCount = static_cast<s32>(mpCurrentGameMode->IsOnline()
                                                   ? lpGameModeParams->miNumNetworkPlayers
                                                   : lpGameModeParams->miNumRivals) + 1;

    for (s32 liGridPosition = 0; liGridPosition < liPlayerCount; ++liGridPosition)
    {
        GameStateModuleIO::EPlayerScoringIndex leScoringIndex;

        if (mpCurrentGameMode->IsOnline())
        {
            // Both array identities come from the console's OWN assert strings, not from offsets.
            const BrnNetwork::NetworkPlayerID lNetworkPlayerID =
                lpGameModeParams->maNetworkPlayerID[liGridPosition];
            // [!] The tree types GameModeParams::maePlayerTeam as the placeholder EPlayerTeam_Stub;
            // the console value IS GameStateModuleIO::EPlayerTeam (the assert strings spell
            // "GsmIO::E_PLAYER_TEAM_NONE" / "_COUNT"). Converted explicitly at the boundary.
            const GameStateModuleIO::EPlayerTeam lePlayerTeam =
                static_cast<GameStateModuleIO::EPlayerTeam>(
                    static_cast<s32>(lpGameModeParams->maePlayerTeam[liGridPosition]));

            CGS_ASSERT(lNetworkPlayerID != -1, "lNetworkPlayerID != CgsNetwork::K_INVALID_PLAYER_ID");
            CGS_ASSERT(static_cast<s32>(lePlayerTeam) >= 0,
                       "lpGameModeParams->maePlayerTeam[liGridPosition] >= GsmIO::E_PLAYER_TEAM_NONE");
            // [!] VALUE DIVERGENCE, ASM WINS: the console bound is `cmpwi 9`, i.e. its
            // E_PLAYER_TEAM_COUNT is 9 -- the tree's enum says 3. BrnOnlineStuntRunModeScoring.h:44
            // already records that exact drift. The literal is used so the assert does not fire
            // spuriously on a legitimate team id; filed as a note for the enum's owner.
            CGS_ASSERT(static_cast<s32>(lePlayerTeam) < 9,
                       "lpGameModeParams->maePlayerTeam[liGridPosition] < GsmIO::E_PLAYER_TEAM_COUNT");
            // The console builds this one's message with a StrStream ("Player " << id << " already
            // in (or not in) the scoring system\n"); CGS_ASSERT forwards a plain string, so the
            // literal tail is kept verbatim and the interpolated id is dropped (house convention).
            CGS_ASSERT(mScoringSystem.IsNetworkPlayerInScoringSystem(lNetworkPlayerID) == lbInstantIntro,
                       "Player already in (or not in) the scoring system\n");

            if (mScoringSystem.IsNetworkPlayerInScoringSystem(lNetworkPlayerID))
            {
                leScoringIndex = mScoringSystem.GetPlayerScoringIndex(lNetworkPlayerID);
            }
            else
            {
                // sub_8231E340 RESOLVED: it calls the 0-arg ScoringSystem::AddPlayer @0x8231E288,
                // asserts the id is not already registered on any car ("lNetworkPlayerID !=
                // maCarData[leCheckPlayerIndex].GetNetworkPlayerID()", BrnScoringSystem.h:3260) and
                // then writes team / team / id onto the new car record -- i.e. it IS
                // ScoringSystem::AddPlayer(NetworkPlayerID, EPlayerTeam). (The committed header pins
                // that overload at 0x8231E288; 0x8231E288 is actually the 0-ARG one it calls. Filed
                // as an address-comment fix, not a behaviour change.)
                leScoringIndex = mScoringSystem.AddPlayer(lNetworkPlayerID, lePlayerTeam);
            }

            // sub_8231DD88 RESOLVED: the maCarData[8] search on GetNetworkPlayerID (base ss+20224,
            // stride 344, id at car+328) == ScoringSystem::GetCarData(NetworkPlayerID).
            CarData* lpCarData = mScoringSystem.GetCarData(lNetworkPlayerID);
            CGS_ASSERT(lpCarData != NULL, "lpCarData");

            // `*(car + 341) = *(nrm + 216 + i)`. SOURCE: mStartNetworkGameEvent.mabPlayerHasFever[i]
            // (the event's mafPlayerData[8] ends at +216 and BrnGameEvents.h pins
            // mLocalNetworkPlayerID at +224, so 216..224 IS the 8-byte fever array). DESTINATION:
            // CarData+341 == mbHasFever -- miCurrentCheckPoint is independently pinned at
            // CarData+0x154 (340) by two committed Scoring bodies, and mbEliminated lives inside
            // CarScoreData (CarData+0xD9), so +341 is the only seat left for the fever flag.
            lpCarData->SetHasFever(
                mpNetworkRoundManager->GetNetworkGameEvent()->mabPlayerHasFever[liGridPosition]);

            // ----------------------------------------------------------------------------------
            // [X] FRONTIER (ONLINE-ONLY, ONE COMPARISON) -- the console guards the disconnect copy
            // with the LOCAL player's id:
            //     if (*(mpGameStateModule + 232296) != lNetworkPlayerID
            //         && mScoringSystem.GetPlayerDisconnected(lNetworkPlayerID))
            //         lAction.SetPlayerDisconnected(lNetworkPlayerID);
            // gsm+232296 (0x38BE8) has no named member or accessor on GameStateModule; filed as a
            // header_request (a GetLocalNetworkPlayerID()-shaped accessor). Without it the LOCAL
            // player would also be reported disconnected if the scorer ever flags them -- an
            // online-only divergence, listed for the online wave.
            // ----------------------------------------------------------------------------------
            if (mScoringSystem.GetPlayerDisconnected(lNetworkPlayerID))
            {
                lAction.SetPlayerDisconnected(lNetworkPlayerID);
            }
        }
        else
        {
            leScoringIndex = mScoringSystem.AddPlayer();
        }

        // The two BrnGameActions.h:1238/1239 asserts in the export ("(liIndex>=0) &&
        // (liIndex<E_ACTIVE_RACE_CAR_INDEX_COUNT)" / "(leScoringIndex>=E_PLAYER_SCORING_INDEX_0) &&
        // (leScoringIndex<E_PLAYER_SCORING_INDEX_COUNT)") are this setter's own body, inlined -- not
        // duplicated here.
        lAction.SetPlayerScoringIndex(liGridPosition, leScoringIndex);
    }

    // ------------------------------------------------------------------------------------------
    // THE SECOND-PHASE CACHE. Console gate (asm 0x82342E14..0x82342E30):
    //     lwz r11, 0x148(lpGameModeParams)  ; == GameModeParams::meGameModeType (pinned: Construct
    //                                       ;    @0x8231C374 stores its EGameModeType arg to +0x148)
    //     cmpwi r11, 2 ; beq -> skip        ; != E_MODE_OFFLINE_SHOWTIME
    //     lbz  r11, 0x94(lpGameModeParams)  ; a BYTE at GameModeParams+148
    //     cmplwi 0 ; bne -> skip            ; ... and that byte must be ZERO
    //
    // [!] SUSPECT / FRONTIER -- the SECOND half of the gate is NOT reproduced, deliberately.
    // GameModeParams+0x94 cannot be tied to a named member on this tree: the class's public block is
    // exactly 0..328 bytes on console (meGameModeType@0x148 above it, maStartLocations@336,
    // maCheckpointDataArray@608 with its count at +1312 -- all asm-pinned), and this body's own
    // player loop pins maNetworkPlayerID@+8 (stride 4) and maePlayerTeam@+280 (stride 4). Laying the
    // committed DWARF-order member run between those two anchors comes up TWENTY-FOUR BYTES short,
    // so +0x94 lands inside mfOvertakingDifficulty under every consistent reading -- and a `lbz` on
    // a float is not how a compiler tests one. The honest conclusion is that the X360 GameModeParams
    // carries members the PS3 DWARF list does not, and this byte is one of them. NAMING IT WOULD BE
    // A FABRICATION (hazards H4's rule, applied off-class), so it is filed as a header_request.
    // DIVERGENCE THIS CAUSES: the second phase arms slightly MORE often than the console (whenever
    // that byte is non-zero). GameModeParams::Construct @0x8231C370 resets the block, so for the
    // offline event path -- the whole stunt-race campaign -- the byte is zero and the two behaviours
    // coincide. The BROADER gate (mode != SHOWTIME) is the one the campaign turns on and it IS here.
    // ------------------------------------------------------------------------------------------
    if (lpGameModeParams->GetGameModeType() != GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME)
    {
        lAction.SetPrepareStage(GameStateModuleIO::PrepareForModeAction::E_PFM_STAGE_FIRST_OF_TWO);

        // [!] WIRE FORMAT (hazards H5). Console: `memcpy(this+0x8C10, &lAction, 0x8E0)` -- 2272
        // bytes, and mfPFMSecondPhaseTimer sits at exactly 0x8C10 + 2272. The host record measures
        // 1792 because BrnGameModeParams.h is 480 bytes short; sizeof is used so the host copy stays
        // self-consistent, and the shortfall is the conductor's wave item (BrnModeManager.h carries
        // the same banner at mPFMActionCache).
        memcpy(&mPFMActionCache, &lAction, sizeof(mPFMActionCache));

        // The console re-stores stage 1 into the LOCAL after the memcpy (`stw r28, var_980` at
        // 0x82342E5C) and stamps the CACHE with stage 2 (`stw r11(=2), 0(r31)`), so the record that
        // goes out now is the FIRST of two and the cached re-post is the SECOND.
        lAction.SetPrepareStage(GameStateModuleIO::PrepareForModeAction::E_PFM_STAGE_FIRST_OF_TWO);
        mPFMActionCache.SetPrepareStage(GameStateModuleIO::PrepareForModeAction::E_PFM_STAGE_SECOND_OF_TWO);

        mbIsWaitingForSecondPFM = true;    // +0x94F4
        mfPFMSecondPhaseTimer   = 0.0f;    // +0x94F0  (flt_82001CC0, image-dumped == 0.0f)
    }

    lpGameActionQueue->AddEvent(&lAction, GameStateModuleIO::E_ACTION_PREPARE_FOR_MODE);

    // `std r20(=0), this+0x8C00` -- a 64-bit ZERO. The Hex-Rays `*(a1 + 35840) = v5` is the H9
    // register-pair artefact (v5's high word still holds a string pointer from the loop above); the
    // asm stores r20, which is this function's zero register from 0x823429D8 onward. So the frozen
    // header's SUSPECT note resolves in the DWARF's favour: this is a CgsID cleared at mode prepare,
    // i.e. mPlayersPreSpecialEventCarID, NOT a player-loop count. Filed as a rename request.
    mPlayersPreSpecialEventCarID = static_cast<CgsID>(0);

    if (muNumLandmarks != 0u)
    {
        // The event's FINISH landmark == the last entry of the mode's landmark table. Console:
        // `addi r10, muNumLandmarks, 0x3F0F; slwi 1; lhzx r31, r10, this` == a half-word read at
        // this + 32286 + 2*muNumLandmarks == &maLandmarkIndices[muNumLandmarks - 1].
        const s32 liRegionIndex = static_cast<s16>(maLandmarkIndices[muNumLandmarks - 1u]);

        const BrnTrigger::TriggerData* lpTriggerData = GetTriggerData();
        const BrnTrigger::Landmark*    lpLandmark    =
            lpTriggerData->GetLandmarkFromRegionIndex(liRegionIndex);

        ModeLandmarkAction lLandmarkAction;
        lLandmarkAction.mBoxRegion  = *lpLandmark->GetBoxRegion();   // the 9-dword copy
        lLandmarkAction.mLandmarkId = lpLandmark->GetId();           // lwz 0x24 / extsw / std +40

        lpGameActionQueue->AddEvent(&lLandmarkAction, GameStateModuleIO::E_ACTION_BROADCAST_MODE_FINISH_LINES);
    }

    // Showtime (offline 2 / online 16) delays its mode-switch action by thirty frames; the countdown
    // itself lives in UpdateCurrentMode, which posts action 143 at EXACTLY zero (hazards H6, latch 3).
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME
        || meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        miFramesUntilModeSwitchSend = 30;   // +0x9510
    }

    mfPlayerTotalledTime = 0.0f;    // +0x8028
    mbModeIntroStarted   = false;   // +0x9505 -- re-arms the StartModeIntro fire-once latch (H6)

    // THE INTRO GATE (hazards H6): mbDistanceToFinishLineTransmitted is set TRUE unless we have just
    // finished an online event AND are entering the lobby/showtime pair. It does NOT depend on the
    // checkpoint count, which is exactly why a stunt run with ZERO checkpoints still lets
    // StartModeIntro fire. Console shape (asm 0x82342FB0..0x82343008):
    //     b9504 = !(b9507 && (mode == 15 || mode == 16))
    const bool lbEnteringOnlineLobbyPair =
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY
         || meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);
    mbDistanceToFinishLineTransmitted = !(mbFinishedOnlineEvent && lbEnteringOnlineLobbyPair);
}

} // namespace BrnGameState
