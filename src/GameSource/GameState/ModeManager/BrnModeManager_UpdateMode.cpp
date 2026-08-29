// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_UpdateMode.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// Wave-B keystone, AGENT 6 -- THE MODE DRIVER. Bodies the four per-frame driver functions:
//
//   ModeManager::UpdateCurrentMode                X360 0x82350EC8   (the biggest spine body)
//   ModeManager::CheckCountdownDisplay            X360 0x82342898
//   ModeManager::UpdateCheckpointDistanceRequests X360 0x823279B8
//   ModeManager::PlayerFinishedMode               X360 0x823280D8
//
// Reconstructed from those four exports' ASSEMBLY. The Hex-Rays pseudocode for
// UpdateCurrentMode is "local variable allocation has failed" garbage (a 30-int prototype,
// `if (a1 == -28088)` for what is really `&mTimerStatusInterface.mSimTimerStatus == NULL`,
// and fake 64-bit merges of adjacent 32-bit stores) -- every offset, every action id and
// every action size below is quoted from the disassembly, and every rodata float is
// image-cited (scratch/postfx_step9_final/envfix/work/image.bin, offset == VA - 0x82000000,
// big-endian).
//
// ARGUMENT MAP (the frozen header's 9-argument shape against the PPC register args; the
// IDA numbering is meaningless because of the failed allocation):
//   r3  this                       r4  a2  lpOutputBuffer          r5  a3  lpPreWorldInputBuffer
//   r6  a4  lpPlayerStatusInterface (NEVER READ by the console body)
//   r7  a5  lePlayerActiveRaceCarIndex                             r8  a6  lbIsOnline (NEVER READ)
//   r9  a7  lpGameActionQueue      r10 a8  lpGlobalRaceCarOutput
//   sp  a28 lpActiveRaceCarOutput  sp  a30 lbPaused
// a4/a6 are genuinely dead in the console body: the "is this mode online" question is asked
// of the MODE (mode+0xAC == GameMode::IsOnline()), never of the caller's flag.
//
// [X] hazards H2: none of the 16 committed BrnModeManager.cpp bodies is re-implemented here --
//     SendModeResults is CALLED (in the online arm's banner), never re-written.
// [X] hazards H6: all three fire-once latches are consume-and-clear IN THE SAME ARM.
//     (1) mbModeIntroStarted gates StartModeIntro; (2) the GameMode latch bytes +174..+177 are
//     cleared by the arm that consumes them; (3) miFramesUntilModeSwitchSend decrements to
//     EXACTLY 0 to fire once.
//
// [X][X] LINK FRONTIER THIS FILE INTRODUCES (added 2026-08-26, fix round -- gate-green is not
//        closeable). Two ScoringSystem members are CALLED here and have no definition anywhere in
//        src/; both are LNK2019 the moment this partfile mounts:
//   1. ScoringSystem::StartModeTimer(const CgsSystem::Time&)  -- declared in BrnScoringSystem.h,
//      and BrnScoringSystem_Timer.cpp:54-56 explicitly lists it as DEFERRED ("no export, no inlined
//      fragment recovered"). Call site: the timer-start latch below -- i.e. THE store that starts
//      every mode's clock. Console is two stores into the ScoringSystem's first member.
//   2. ScoringSystem::HasBeatenRoadRageTarget()  -- declared in BrnScoringSystem.h, no definition
//      and no link stub (BrnRoadRageModeScoringLinkStubs.cpp:175 defines the DIFFERENT
//      RoadRageModeScoring::HasBeatenRoadRageTarget). Two call sites in the road-rage / marked-man
//      end-condition arms below. Also called from BrnModeManager_Finish.cpp.
//   (Header line numbers are deliberately omitted: BrnScoringSystem.h is being edited concurrently
//   this wave, so a line cite would go stale faster than the fact does.)
// [!] A THIRD item the wave's batch-4 verdict listed under this heading is REFUTED, so do not
//     chase it: ScoringSystem::GetCheckpointDistanceToFinish (BrnModeManager_TransmitCrash.cpp:204)
//     IS defined -- BrnScoringSystem_Finish.cpp:75, complete with its two console asserts
//     ("luCheckpointIndex < (uint32_t)KI_MAX_LANDMARKS_IN_MODE" and "Distance to finish not ready").

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

#include "GameSource/GameState/BrnGameStateModule.h"              // GetPlayerActiveRaceCarIndex, meControllerState
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<>::AddEvent
#include "SharedClasses/Trigger/BrnRegion.h"                      // BrnTrigger::BoxRegion::GetPosition
#include "SharedClasses/Progression/BrnTrainingTypes.h"           // BrnProgression::ETrainingType (action-149 payload)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"        // [diagnostic] the [showtime-switch] arming witness

namespace BrnGameState
{

// ============================================================================================
// [x] header_request #8 CLOSED 2026-08-26 (stuntrace waveB CLOSURE round).
// ============================================================================================
// GameStateModuleIO::PlayerFinishedModeEvent used to be DEFINED HERE -- at real
// BrnGameState::GameStateModuleIO scope, in a .cpp, i.e. with external linkage and no
// GameEvent<> base. It now lives in its owning header, BrnGameEvents.h, with the base, the
// same asm-pinned three bytes and offsetof pins on all three; and the X360 discriminant it
// could not prove is proven there (E_EVENT_PLAYER_FINISHED_MODE == 32, ProcessGameEvents
// jumptable case 32 @0x823A27F4 -> ModeManager::PlayerFinishedMode).
// DO NOT re-add a local copy: two external-linkage definitions of one class is an ODR
// violation no compile-only gate can see. BrnModeManager.h already includes BrnGameEvents.h,
// so the type is visible here by name.

// ============================================================================================
// TU-LOCAL CONSTANTS -- every one asm- or image-cited. No invented values.
// ============================================================================================

// ---- rodata floats, dumped big-endian from the image THIS SESSION ---------------------------
//   0x820213B8  3E4CCCCD  0.2f    the second-phase PrepareForMode repost delay
//   0x82001CC0  00000000  0.0f    the zero register the whole body compares against
//   0x82020F70  3DCCCCCD  0.1f    the player-totalled grace period
//   0x82020F90  40400000  3.0f    the time-up outro duration
//   0x82020E90  45E10000  7200.0f the online-race hard time limit (2 hours)
//   0x82004A28  42F00000  120.0f  the online-road-rage seconds-per-kilometre allowance
//   0x820049E0  42C80000  100.0f  online-stunt result payload field (online arm, deferred)
//   0x82020EF0  43700000  240.0f  the lobby "you have been here 4 minutes" tip threshold
//   0x82020F08  43960000  300.0f  the online "you have been online 5 minutes" tip threshold
static const f32 KF_PFM_SECOND_PHASE_DELAY_SECONDS = 0.2f;
static const f32 KF_PLAYER_TOTALLED_GRACE_SECONDS  = 0.1f;
static const f32 KF_TIME_UP_OUTRO_SECONDS          = 3.0f;
static const f32 KF_ONLINE_RACE_TIME_LIMIT_SECONDS = 7200.0f;
static const f32 KF_ONLINE_ROAD_RAGE_SECONDS_PER_KM = 120.0f;

// ---- action discriminants ---------------------------------------------------------------
// [x] ALL FOUR NOW LIVE IN GameStateModuleIO::EGameActionType (landed 2026-08-26, stuntrace waveB
// CLOSURE round) and are used BY NAME below. The TU-local mirrors are gone; the evidence for each
// value moved into BrnGameActions.h with it, so do not re-mint them here.
//   33  -> E_ACTION_STOP_MODE_COUNTDOWN   (`li r5,0x21` @0x82351134, size 1)
//   143 -> E_ACTION_SHOWTIME_MODE_SWITCH  (`li r5,0x8F` @0x82350F70, size 16)
//   149 -> E_ACTION_REQUEST_GAME_TRAINING (already enumerated). The three payload values this
//          body posts decode cleanly as BrnProgression::ETrainingType: 48 == TOTALLED,
//          59 == FREEBURNING_ONLINE, 70 == ONLINE_WIN_CAR.
//   262 -> E_ACTION_MODE_TIME_UP          (`li r5,0x106` @0x823515D4, size 1)
// [x] THE 262 COLLISION THIS FILE RAISED IS SETTLED, AND IN THIS PRODUCER'S FAVOUR:
// E_ACTION_ROAD_RULES_BATCH_QUERY has moved OFF 262 to its own producer-pinned 275
// (GameStateModule::ProcessGameEvents case 98 posts `li r5,0x113` + `li r6,0x308` == 776 ==
// sizeof(RoadRulesBatchQueryAction) straight after FillInRoadRulesQuery). 262 is this one-byte
// bool, and its enumerator name is FLAGGED in the header for the same reason this banner gave --
// no DWARF entry fits a one-byte payload at a monotone shift.

// The wire size of the cached PrepareForModeAction repost. See the divergence note at the post.
// Console: `li r6, 0x8E0` == 2272.
static const s32 KI_CONSOLE_PFM_ACTION_WIRE_SIZE = 2272;

// The "there is no next mode" sentinel UpdateCurrentMode hands ExitCurrentMode (`li r6, 0x12`).
// It is one PAST the last dispatchable slot, i.e. exactly ModeManager::KI_GAME_MODE_SLOTS (18) --
// which is why hazards H3 insists the slot count is 18 and not the enum's 17.
static const s32 KI_NO_NEXT_GAME_MODE = ModeManager::KI_GAME_MODE_SLOTS;

// ============================================================================================
// [!!] TEMPORARY ACCESS BRIDGE -- DELETE WHEN header_requests #1..#6 LAND ON BrnGameMode.h.
// ============================================================================================
// UpdateCurrentMode is, at its core, the consumer of SIX GameMode fields (the console reads them as
// mode+0xA4 / +0xAD / +0xAE / +0xAF / +0xB0 / +0xB1). The committed BrnGameMode.h exposes SETTERS
// for four of the latches (SetFinished / SetTimerStartRequest / SetShowResultsRequest /
// SetIntroJustFinished) but NO READERS for any of them, no accessor at all for
// mbCountdownJustFinished, and no setter for mfTimeStepSeconds -- and this partfile may not edit
// that header. Without the readers the function is literally unwritable, so the six accessors are
// bridged here through the standard protected-member idiom (a derived type may name a protected
// base member through a pointer to the derived type). Nothing is ever constructed; the bridge has
// no state and no console counterpart.
//
// EVERY SITE BELOW IS A ONE-LINE MECHANICAL EDIT once the header grows: ModeLatch::IsFinished(m)
// becomes m->IsFinished(), and so on. The exact declaration text is in this agent's report.
namespace
{
struct ModeLatch : public GameMode
{
    // console mode+0xAD -- GameMode::Initialise clears it; the state machine sets it.
    static bool IsFinished(const GameMode* lpMode)
        { return static_cast<const ModeLatch*>(lpMode)->mbFinished; }
    // console mode+0xAE
    static bool IsTimerStartRequested(const GameMode* lpMode)
        { return static_cast<const ModeLatch*>(lpMode)->mbTimerStartRequested; }
    // console mode+0xAF
    static bool IsShowResultsRequested(const GameMode* lpMode)
        { return static_cast<const ModeLatch*>(lpMode)->mbShowResultsRequested; }
    // console mode+0xB0
    static bool HasIntroJustFinished(const GameMode* lpMode)
        { return static_cast<const ModeLatch*>(lpMode)->mbIntroJustFinished; }
    // console mode+0xB1 -- the ONLY latch with neither a getter nor a setter on the committed header.
    static bool HasCountdownJustFinished(const GameMode* lpMode)
        { return static_cast<const ModeLatch*>(lpMode)->mbCountdownJustFinished; }
    static void SetCountdownJustFinished(GameMode* lpMode, bool lbValue)
        { static_cast<ModeLatch*>(lpMode)->mbCountdownJustFinished = lbValue; }
    // console `stfs f31, 0xA4(r11)` -- the per-frame update step every GameModeState ticks by.
    static void SetUpdateTimeStep(GameMode* lpMode, f32 lfTimeStepSeconds)
        { static_cast<ModeLatch*>(lpMode)->mfTimeStepSeconds = lfTimeStepSeconds; }
};

// The three-byte PlayerFinishedModeEvent the console builds on the stack at four call sites inside
// UpdateCurrentMode. Factored so each site reads as the console's three byte stores.
inline GameStateModuleIO::PlayerFinishedModeEvent
MakePlayerFinishedModeEvent(bool lbTimedOut, bool lbCarDestroyed, bool lbCrossedFinishLine)
{
    GameStateModuleIO::PlayerFinishedModeEvent lEvent;
    lEvent.mbTimedOut          = lbTimedOut;
    lEvent.mbCarDestroyed      = lbCarDestroyed;
    lEvent.mbCrossedFinishLine = lbCrossedFinishLine;
    return lEvent;
}

// ---- the action-45-family route-request record UpdateCheckpointDistanceRequests builds --------
// [header_request #10] Its home is BrnGameActions.h (DWARF E_ACTION_REQUEST_ROUTE_INFO == 45); it is
// built here because GameStateModule::SendRouteRequestAction owns the id + the AddEvent, and neither
// the callee nor the record exists in the tree yet. EVERY offset is a store in the producer's asm
// (0x82327AC8..0x82327B84) and every field the consumer reads is at the offset
// GameStateModule::SendRouteRequestAction @0x82381DC8 loads it from (`v26 = a2 + 32` node types,
// `v25 = a2 + 48` node ids, loop count 2 == the 4th argument):
struct alignas(16) RouteRequestEventPayload
{
    Vector3 mCurrentPosition;              // +0x00  the current checkpoint landmark's box-region position
    Vector3 mDestinationPosition;          // +0x10  the NEXT checkpoint landmark's position
    s32     miCurrentNodeType;             // +0x20  = 0 (a plain world-position node; the callee
                                           //        asserts "Unknown route node type" above 2)
    s32     miDestinationNodeType;         // +0x24  = 0
    u8      maPad28[8];                    // +0x28  never written by this producer
    CgsID   mCurrentLandmarkId;            // +0x30  maLandmarkCgsIDs[i]
    CgsID   mDestinationLandmarkId;        // +0x38  maLandmarkCgsIDs[i + 1]
    u16     mu16CurrentSectionIndex;       // +0x40  mauLandmarkSectionIndices[i]
    u16     mu16DestinationSectionIndex;   // +0x42  mauLandmarkSectionIndices[i + 1]
    u16     mu16CheckpointIndex;           // +0x44  muNextDistanceRequestCheckpoint
    u8      maPad46[10];                   // +0x46  tail padding to the 16-byte-aligned 0x50
};

// The two route nodes SendRouteRequestAction walks (its 4th argument, `li r6, 2`).
const s32 KI_ROUTE_REQUEST_NODE_COUNT = 2;
} // anonymous namespace

// ============================================================================================
// ModeManager::UpdateCurrentMode -- X360 0x82350EC8
// ============================================================================================
// The per-frame driver: it ticks the current GameMode, drains the mode's five latch bytes, applies
// the per-mode time limit at the moment the timer starts, polls every mode-specific END condition,
// and finally exits the mode when the state machine says it is finished.
void ModeManager::UpdateCurrentMode(GameStateModuleIO::OutputBuffer*              lpOutputBuffer,
                                    const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer,
                                    const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* lpPlayerStatusInterface,
                                    EActiveRaceCarIndex                           lePlayerActiveRaceCarIndex,
                                    bool                                          lbIsOnline,
                                    GameStateModuleIO::GameActionQueue*           lpGameActionQueue,
                                    const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCarOutput,
                                    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput,
                                    bool                                          lbPaused)
{
    // The console body never touches r6 (the player-status interface) or r8 (the caller's online
    // flag) -- it asks the MODE whether it is online. Kept in the signature because the frozen
    // header (and PreWorldUpdate's call) spell them.
    (void)lpPlayerStatusInterface;
    (void)lbIsOnline;

    CGS_ASSERT(mpCurrentGameMode != nullptr, "mpCurrentGameMode != NULL");   // BrnModeManager.cpp:1814

    // ---- (1) the delayed showtime mode-switch broadcast (hazards H6 latch 3) -------------------
    // PrepareForMode sets this to 30 for modes 2 / 16 only; it must reach EXACTLY 0 to fire once.
    if (miFramesUntilModeSwitchSend > 0)
    {
        --miFramesUntilModeSwitchSend;
        if (miFramesUntilModeSwitchSend == 0)
        {
            // ⭐⭐⭐ UN-PARKED 2026-08-29 (showtime session-length wave). THE PARK'S OWN BLOCKER HAD
            // ALREADY BEEN DISCHARGED AND NOBODY RE-READ THE NOTE. It said "the POST is parked (a
            // wrong network id on the wire is worse than no post) ... re-arm the moment
            // GameStateModule grows GetLocalPlayerNetworkID() -- header_request #9". That header
            // request LANDED THE SAME DAY: BrnGameStateModule.h:974 declares
            // `GetLocalPlayerNetworkID()` over the member at :1337, pinned to X360 gsm+0x38B68 by
            // OnlineFlybyManager::GetLocalPlayerNetworkID @0x82358720. The gate outlived its
            // blocker by three days. [[gates-are-stale-not-dead]] -- ask WHEN this last ran.
            //
            // ⛔⛔ AND THE PARK COST THE WHOLE OFFLINE SHOWTIME SESSION, because it reasoned about
            // the ONE field its offline consumer never reads. MEASURED 2026-08-29, end to end on a
            // -Drive -Showtime run:
            //     [crash-exit]  OPENED crash record for active race car 0 seconds=4.000000
            //     [crash-exit]  CRASH COMPLETE posted for active race car 0 remove=0
            //     [crash-latch] mbPlayerIsCrashing 1 -> 0 at tMode=4.049997  playerIdx=0
            //                   playerActive=1 rawState.mbCrashing=0
            //     [crash-end]   ENDED via !mbPlayerIsCrashing
            // i.e. the ORDINARY FREE-BURN CRASH-RECOVERY TIMER ended the showtime session. The
            // console's protection against exactly that is CrashModule::TickCrashes @0x827C6690:
            // `if (IsCarInShowtime(owner)) { retract the ending message; continue; }` -- a showtime
            // wreck is never ticked at all. That guard is reconstructed and mounted, and it was
            // INERT, because IsCarInShowtime reads E_RACE_CAR_OUTPUT_FLAG_IN_SHOWTIME, which
            // RaceCarEntityModule::UpdateOutputInterfaces raises from ActiveRaceCar::mbIsInShowtime
            // (+0x788) -- and a scan of the ASSEMBLY of every exported ARTIST function finds
            // exactly two stores to +0x788, ActiveRaceCar::Prepare and ::ResetAfterCrash, BOTH
            // WRITING ZERO. The only writer of a ONE is the action this post carries.
            // [[silent-drop-stubs]] -- check a value has a writer before you trust it.
            //
            // ⚠️ CORRECTION TO 265b059d's OWN COMMIT MESSAGE, made here so it is not
            // inherited: that message cited "10 prop-carrying frames to 252" as the
            // starvation relief. Those are [contact-entry] counters, and that witness
            // deliberately accumulates BEFORE every gate -- so its change measures where
            // each run's car ended up, not the gate. The past-the-gate counter is
            // [contact-pass] gatedFrames, and it is the one that moved:
            //     before  [contact-pass] gatedFrames=241   (its last line of the run)
            //     after   [contact-pass] gatedFrames=7921
            // 33x more frames reach the console's own mode-and-state gate. Same claim,
            // right instrument. [[diagnostics-that-lie]] -- a probe placed before a gate
            // cannot measure that gate, however much its number moves.
            // Evidence page (before/after pixels + the log ladder):
            //   https://claude.ai/code/artifact/ebe1c741-6e40-4a0b-95ba-9c4fe61d42ca
            // Console 0x82350F50..0x82350F8C, store for store; see ShowtimeModeSwitchAction's
            // banner in BrnGameActions.h for the field-name provenance (the DWARF-named GUI twin).
            // The score word is the console's LITERAL ZERO on this producer -- it is
            // SendModeStopMessages @0x8234BFDC, the LEAVING post, that carries the real
            // end-of-mode score, and that one is still parked on three CrashModeScoring accessors.
            GameStateModuleIO::ShowtimeModeSwitchAction lSwitch;
            lSwitch.mNetworkPlayerID     = mpGameStateModule->GetLocalPlayerNetworkID();  // gsm+0x38B68
            lSwitch.meActiveRaceCarIndex = mePlayerActiveRaceCarIndex;                    // this+0x8038
            lSwitch.miFinalShowtimeScore = 0;                                             // `stw r22(0)`
            lSwitch.mbEnteringShowtime   = true;                                          // `stb r21(1)`
            lSwitch.maPad[0] = 0;
            lSwitch.maPad[1] = 0;
            lSwitch.maPad[2] = 0;

            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lSwitch),
                                        GameStateModuleIO::E_ACTION_SHOWTIME_MODE_SWITCH,
                                        static_cast<s32>(sizeof(GameStateModuleIO::ShowtimeModeSwitchAction)));

            // [DIAG] NOT IN THE X360 BINARY. One-shot, unconditional (no env gate) because this
            // post is the arming edge of the whole showtime session and its absence is exactly the
            // defect above -- a run that does not print this line has the 5-second session.
            if (CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint
                    << "[showtime-switch] action 143 posted: car "
                    << static_cast<s32>(lSwitch.meActiveRaceCarIndex)
                    << " entering=1 (arms ActiveRaceCar::mbIsInShowtime -> IsCarInShowtime ->"
                       " CrashModule::TickCrashes skips this wreck)\n";
            }
        }
    }

    // ---- (2) the checkpoint distance-request pump ---------------------------------------------
    if (mbIsCalculatingCheckpointDistances)   // console lbzx this, 0x8C0D
    {
        UpdateCheckpointDistanceRequests(lpGameActionQueue);
    }

    // ---- (3) the SIM timer snapshot -----------------------------------------------------------
    // The console takes `addi r27, r31, 0x6DB8` == &mTimerStatusInterface + 24 -- i.e. the SIM
    // sub-status, NOT the game one (the header's spec-correction D2 pins mTimerStatusInterface at
    // +28064, which is what makes +0x6DB8 land on TimerStatus #2). It then reads
    //   f31 = *(r27+8) * *(r27+4)   == GetTimeStepMultiplier() * GetBaseTimeStep()
    //                               == TimerStatus::GetCurrentTimeStep()
    //   r24 = *(r27+16), f29 = *(r27+20)  == TimerStatus::GetTime()
    // IDA renders the null check as `if (a1 == -28088)`; it is the inlined assert of the accessor.
    const CgsSystem::TimerStatus* lpSimTimerStatus = mTimerStatusInterface.GetSimTimerStatus();
    CGS_ASSERT(lpSimTimerStatus != nullptr, "lpSimTimerStatus");   // BrnModeManager.cpp:1839

    const f32 lfTimeStepSeconds = lpSimTimerStatus->GetCurrentTimeStep();

    // ---- (4) the cached second-phase PrepareForMode repost (hazards H5) ------------------------
    if (mbIsWaitingForSecondPFM)
    {
        mfPFMSecondPhaseTimer += lfTimeStepSeconds;
        if (mfPFMSecondPhaseTimer > KF_PFM_SECOND_PHASE_DELAY_SECONDS)
        {
            // [X][X] WIRE-FORMAT DIVERGENCE, deliberate and narrow. The console posts
            // `AddEvent(queue, this+35856, 23, 0x8E0)` -- 2272 bytes, the console
            // sizeof(PrepareForModeAction). BrnModeManager.h's WIRE-FORMAT DIVERGENCE note records
            // that the HOST record measures 1792 (a 480-byte hole in BrnGameModeParams.h), so
            // posting the console literal would over-read mPFMActionCache by 480 bytes into
            // mfPFMSecondPhaseTimer and everything after it. The size is therefore taken from the
            // object. RESTORE THE 2272 LITERAL (KI_CONSOLE_PFM_ACTION_WIRE_SIZE) the moment
            // BrnGameModeParams.h closes the hole -- and note that AGENT 3's PrepareForMode must
            // make the SAME choice for the FIRST post or producer and consumer disagree.
            lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&mPFMActionCache),
                                        GameStateModuleIO::E_ACTION_PREPARE_FOR_MODE,
                                        static_cast<s32>(sizeof(mPFMActionCache)));
            mfPFMSecondPhaseTimer   = 0.0f;
            mbIsWaitingForSecondPFM = false;
        }
    }

    // ---- (5) THE INTRO FIRE-ONCE LATCH (hazards H6 latch 1) ------------------------------------
    // Losing the latch replays the intro every frame; losing the distance gate means the intro
    // never fires at all. A stunt run has ZERO checkpoints, and PrepareForMode is what must still
    // set mbDistanceToFinishLineTransmitted true on that path.
    if (!mbModeIntroStarted && mScoringSystem.AreAllRaceCarsSetup() && mbDistanceToFinishLineTransmitted)
    {
        mbModeIntroStarted = true;
        StartModeIntro(lpGameActionQueue);
    }

    // ---- (6) tick the mode ---------------------------------------------------------------------
    ModeLatch::SetUpdateTimeStep(mpCurrentGameMode, lfTimeStepSeconds);   // console stfs f31, 0xA4(mode)

    // [x] SIGNATURE RESTORED 2026-08-26 (fix round) -- the 26-slot BrnGameMode.h has landed, so the
    // "call the committed 0-argument shape, widen when the header takes the console shape" note that
    // used to stand here is DISCHARGED. All six console arguments are now passed.
    // Re-derived from the dispatch at 0x82351094..0x823510CC:
    //     lwz r3,  0xD98(r31)          ; mpCurrentGameMode
    //     addi r25, r31, 0xDB0         ; &mScoringSystem                     -> r9  (6th arg)
    //     lwz r30, arg_54(r1)          ; the stacked ACTIVE race-car output  -> r7  (4th arg)
    //     mr  r4,  r18                 ; lpOutputBuffer                      -> r4  (1st arg)
    //     mr  r5,  r28                 ; lpPreWorldInputBuffer               -> r5  (2nd arg)
    //     mr  r6,  r26                 ; lpGlobalRaceCarOutput               -> r6  (3rd arg)
    //     lbz r8,  arg_5F(r1)          ; the stacked bool (lbPaused)         -> r8  (5th arg)
    //     lwz r11, 0(r3) / lwz r11, 8(r11) / mtctr / bctrl     ; vtbl+8 == slot 2
    // Six argument registers r4..r9, matching BrnGameMode.h's slot-2 declaration exactly. The
    // ScoringSystem is the LAST argument, not the first -- r9, materialised at 0x82351098.
    // WHY IT MATTERS beyond shape: those arguments are what GameMode::PreWorldUpdate @0x8232FB20
    // feeds its rival-visibility scan (it walks the ACTIVE race-car interface and recomputes
    // mbVisibleCars at mode+179), and mbVisibleCars is exactly what GameMode::ShouldExit reads to
    // pick its 3.0f-vs-10.0f timeout. With the 0-argument call that scan could not run.
    mpCurrentGameMode->PreWorldUpdate(lpOutputBuffer,
                                      lpPreWorldInputBuffer,
                                      lpGlobalRaceCarOutput,
                                      lpActiveRaceCarOutput,
                                      lbPaused,
                                      &mScoringSystem);

    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        // [!] ONLINE ARM DEFERRED (hazards H7). Console:
        //   BurnoutSkillzManager::PreWorldUpdate(this+3136, lpPreWorldInputBuffer,
        //                                        lpActiveRaceCarOutput, lpOutputBuffer, true)
        // this+3136 is the BurnoutSkillzManager region embedded INSIDE mOnlineFreeBurnLobby
        // (BrnModeManager.h's member run pins the lobby at +2952 and the skillz region at +3136);
        // the host header exposes no accessor for it, and the Skillz TU carries the known
        // GameActionQueue typedef clash the wave brief puts out of scope.
    }

    // ---- (7) the intro-just-finished latch -----------------------------------------------------
    if (ModeLatch::HasIntroJustFinished(mpCurrentGameMode))
    {
        StopModeIntro(lpGameActionQueue);
        mpCurrentGameMode->SetIntroJustFinished(false);
        mScoringSystem.ClearHighestPositions();
    }

    // ---- (8) the countdown-just-finished latch --------------------------------------------------
    if (ModeLatch::HasCountdownJustFinished(mpCurrentGameMode))
    {
        // [!] The console passes an UNINITIALISED stack byte (the slot is not written until the
        // action-262 post far below) and the consumer reads no payload. Zero-initialised here so
        // the host never puts indeterminate bytes on the shared queue.
        u8 luStopModeCountdownPayload = 0;
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&luStopModeCountdownPayload),
                                    GameStateModuleIO::E_ACTION_STOP_MODE_COUNTDOWN, 1);
        ModeLatch::SetCountdownJustFinished(mpCurrentGameMode, false);

        // [x] UN-PARKED 2026-08-26 (stuntrace fsm round) -- header_request #9b IS CLOSED. Console
        // `stwx 3, gsm, 0x38B64`, i.e.
        //   mpGameStateModule->meControllerState = E_CONTROLLERSTATE_ACTIVE_GAME_MODE_STATE;
        // THE store that hands the pad back to the player when the lights go out, emitted at its
        // console position -- the LAST instruction of the countdown-just-finished arm
        // (0x82351144..0x8235115C: `lwz r11,0xD98(r31)` / `stb r22,0xB1(r11)` /
        //  `lwz r11,0x6D58(r31)` / `stwx r9(3), r11, 0x38B64`).
        // The blocker this used to be parked on is gone: BrnGameStateModule.h:749 now carries the
        // public `SetActiveGameModeState()` setter over the private member, so the store is made BY
        // NAME and this partfile still does not touch that header.
        // ⓘ It is NOT what was starving the mode tick (see the file banner / the [mode-fsm] rung in
        // BrnModeManager_WorldTick.cpp): the console does not gate UpdateCurrentMode on the
        // controller state at all -- PreWorldUpdate @0x823537B8 calls it under
        // `if (mpCurrentGameMode)` and nothing else. NOTE (verify 2026-08-26): IsControllerActive()
        // returns true for BOTH 0 and 3, so the [mode-fsm] rung's controllerActive bit does NOT
        // show this store; the store's value is the state-machine correctness itself (the
        // NOT_IN_GAME reset in SendModeStopMessages + the INACTIVE store in PlayerFinishedMode
        // are its re-armed siblings -- the trio moves together).
        mpGameStateModule->SetActiveGameModeState();
        // [!] FIX ROUND 2026-08-26: an invented `CGS_ASSERT(mpGameStateModule != nullptr,
        // "mpGameStateModule")` used to stand here. REMOVED -- re-derived the console arm at
        // 0x82351144..0x8235115C: `lwz r11,0xD98(r31)` / `stb r22,0xB1(r11)` / `lwz r11,0x6D58(r31)`
        // / `stwx r9(3), r11, 0x38B64`. Four instructions, no BeginAssert/FireAssert anywhere in the
        // arm. The string was not an X360 string at this site, and a fabricated assert also poisons
        // the H10 assert-storm oracle.
    }

    CheckCountdownDisplay(lpGameActionQueue);

    // ---- (9) the two mode-family predicates the tail arms reuse ---------------------------------
    const bool lbShowtimeMode =
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME) ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);
    const bool lbRoadRageOrMarkedMan =
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE) ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_MARKED_MAN);

    // The sim clock, read here because that is where the console reads it (after
    // CheckCountdownDisplay, `lwz r24, 0x10(r27)` / `lfs f29, 0x14(r27)`).
    const CgsSystem::Time lCurrentTime = lpSimTimerStatus->GetTime();

    // ---- (10) THE TIMER-START LATCH: the mode actually begins here ------------------------------
    if (ModeLatch::IsTimerStartRequested(mpCurrentGameMode))
    {
        // console: two stores, `stfs f29, 4(scoring)` + `stw r24, 0(scoring)` -- the inlined
        // ScoringSystem::StartModeTimer(lTime) (mStartTime = lTime; ScoringSystem's first member).
        mScoringSystem.StartModeTimer(lCurrentTime);

        mpCurrentGameMode->SetTimerStartRequest(false);
        StartPlayingMode(lpGameActionQueue);

        if (lbShowtimeMode)
        {
            // console `li r11, -1; stw r11, 0(scoring)` -- mStartTime.miSeconds = -1, which is
            // EXACTLY the committed ScoringSystem::ClearModeTimer() body
            // (BrnScoringSystem_Timer.cpp:163) and the sentinel IsTimerActive() tests. Showtime has
            // no mode timer -- its countdown-table entry is an authored 0.0 too.
            mScoringSystem.ClearModeTimer();
        }

        // The per-mode time limit, applied once, at the instant the timer starts. The console
        // switches on `meCurrentGameModeType - 3` over a 15-entry jump table (modes 3..17); every
        // mode not listed below falls through the default and gets no limit.
        switch (meCurrentGameModeType)
        {
        case GameStateModuleIO::E_MODE_ROAD_RAGE:            // 3   jumptable case 0
        case GameStateModuleIO::E_MODE_STUNT_ATTACK:         // 7   jumptable case 4  <- THE stunt race
        case GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:      // 12  jumptable case 9
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:     // 14  jumptable case 11
        case GameStateModuleIO::E_MODE_ONLINE_MODE_END:      // 17  jumptable case 14
            mScoringSystem.SetTimeLimitSeconds(mfModeTimeLimit);   // console lfsx this, 0x8024
            break;

        case GameStateModuleIO::E_MODE_BURNING_ROUTE:        // 5   jumptable case 2
            // console assert "lpGameModeParams" (BrnModeManager.cpp:1977) -- a null test on
            // &mCurrentGameModeParams, i.e. vacuously true for an embedded member on the host; kept
            // as this note rather than as a tautological CGS_ASSERT.
            // console lfs f3, 0x60(params) / f2, 0x64 / f1, 0x68 -> SetMedalModeTimer(gold, silver,
            // bronze). The three consecutive f32s at params+0x60/0x64/0x68 are exactly
            // mfNeedForBronze / mfNeedForSilver / mfNeedForGold in BrnGameModeParams.h's member run.
            mScoringSystem.SetMedalModeTimer(mCurrentGameModeParams.mfNeedForGold,
                                             mCurrentGameModeParams.mfNeedForSilver,
                                             mCurrentGameModeParams.mfNeedForBronze);
            break;

        case GameStateModuleIO::E_MODE_ONLINE_RACE:          // 10  jumptable case 7
            mScoringSystem.SetTimeLimitSeconds(KF_ONLINE_RACE_TIME_LIMIT_SECONDS);
            break;

        case GameStateModuleIO::E_MODE_ONLINE_ROAD_RAGE:     // 11  jumptable case 8
            // console `lfs f0, 0x6A94(this)` == mScoringSystem's mfTotalRaceDistance, reached here
            // through its named accessor (hazards H9: never a raw offset off `this`).
            if (mScoringSystem.GetTotalRaceDistance() != 0.0f)
            {
                mScoringSystem.SetTimeLimitPerKm(KF_ONLINE_ROAD_RAGE_SECONDS_PER_KM);
            }
            break;

        case GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN:   // 13  jumptable case 10
            // [x] UN-PARKED 2026-08-26 (stuntrace waveB CLOSURE round). header_request #11 LANDED:
            // BrnGameModeParams.h now carries `f32 GetOnlineTimeLimit() const` (inline) over
            // mfOnlineModeTimeLimit, and the member-run offset dispute this banner used to raise is
            // SETTLED IN FAVOUR OF THIS PRODUCER -- the header's CORRECTED RUN reads
            // mfOnlineModeTimeLimit(+0x850), meAStarDistanceFunction(+0x854),
            // miPlayerWreckCount(+0x85C).
            // Console, re-derived this pass (r30 == r31 + 0x10000 - 0x7C80 == &mCurrentGameModeParams,
            // r25 == &mScoringSystem):
            //   0x823512D8  cmplwi r30, 0            -> assert "lpGameModeParams"  (:2009, li r5,0x7D9)
            //   0x823512FC  lfs f0, 0x850(r30)
            //   0x82351300  fcmpu f0, f30(=0.0f) / bgt
            //                                        -> assert "lpGameModeParams->GetOnlineTimeLimit()
            //                                           > 0.0f"                   (:2010, li r5,0x7DA)
            //   0x82351324  lfs f1, 0x850(r30)
            //   0x82351334  bl ScoringSystem::SetTimeLimitSeconds
            // The first assert is a null test on an EMBEDDED member, i.e. vacuously true on the
            // host -- recorded here rather than written as a tautological CGS_ASSERT, exactly as the
            // BURNING_ROUTE arm above does with its own :1977 twin.
            CGS_ASSERT(mCurrentGameModeParams.GetOnlineTimeLimit() > 0.0f,
                       "lpGameModeParams->GetOnlineTimeLimit() > 0.0f");             // :2010
            mScoringSystem.SetTimeLimitSeconds(mCurrentGameModeParams.GetOnlineTimeLimit());
            break;

        default:
            break;
        }
    }

    // ---- (11) the show-results latch -------------------------------------------------------------
    if (ModeLatch::IsShowResultsRequested(mpCurrentGameMode))
    {
        ShowModeResults(lpGlobalRaceCarOutput, lpGameActionQueue);
        mpCurrentGameMode->SetShowResultsRequest(false);
    }

    // ---- (12) showtime: the crash chain has run out -----------------------------------------------
    if (lbShowtimeMode)
    {
        if ((mpCurrentGameMode != nullptr) &&
            (mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS))
        {
            // console `addi r3, r31, 0xDD0` == &mScoringSystem + 32 == the embedded CrashModeScoring,
            // and `fmr f1, f30` == 0.0f.
            if (mScoringSystem.GetCrashScorer()->HasCrashModeEnded(0.0f))
            {
                const GameStateModuleIO::PlayerFinishedModeEvent lEvent =
                    MakePlayerFinishedModeEvent(false, false, false);
                PlayerFinishedMode(&lEvent);
            }
        }
    }

    // ---- (13) road rage / marked man: the player has been totalled --------------------------------
    if (lbRoadRageOrMarkedMan)
    {
        // [!] ROAD-RAGE ARM GATED (conductor decision #10). Console:
        //   if (!TakedownManager::IsInTakedownCamera(mpGameStateModule + 0x238) &&
        //       mbPlayerCrashedLastFrame)
        //       mScoringSystem.OnRoadRagePlayerCrashed(lpOutputBuffer,
        //                                              GameStateModuleIO::E_ROADRAGE_CRASHTYPE_WRECKED);
        // BrnGameState::TakedownManager::IsInTakedownCamera @0x82359620 has no host body AND
        // GameStateModule exposes no accessor for the TakedownManager it embeds at gsm+0x238. The
        // whole `if` is gated rather than half of it: without the camera test the scorer would fire
        // during every takedown camera, which is worse than not firing. mbPlayerCrashedLastFrame
        // (+0x950A) keeps its pinning comment in the header from THIS reader.

        // console `lbz r11, 0x5920(this)` == mScoringSystem + 0x4B70 == mbPlayerTotalled, reached
        // through its named accessor.
        if (mScoringSystem.IsPlayerTotalled())
        {
            mfPlayerTotalledTime += lfTimeStepSeconds;
            if (mfPlayerTotalledTime > KF_PLAYER_TOTALLED_GRACE_SECONDS)
            {
                // console stores {0, 1, 0}: the car was DESTROYED, not timed out.
                const GameStateModuleIO::PlayerFinishedModeEvent lEvent =
                    MakePlayerFinishedModeEvent(false, true, false);
                PlayerFinishedMode(&lEvent);
            }

            // console `lwz 0x58F0(this)` vs `lwz 0x58FC(this)` == mScoringSystem + 0x4B40 vs
            // +0x4B4C, i.e. the player's road-rage takedowns against the event's takedown target
            // (+0x4B40 is the offset BrnScoringSystem.h's GetPlayerModeTakedowns() note already
            // cites). The named predicate for `takedowns >= target` is HasBeatenRoadRageTarget();
            // this arm is its negation, so the "you were totalled" tip is suppressed once the
            // road-rage target is already in the bag. FLAG: the accessor mapping is an inference
            // from those two offsets -- verifier, confirm against HasBeatenRoadRageTarget's asm.
            if ((meCurrentGameModeType != GameStateModuleIO::E_MODE_ROAD_RAGE) ||
                !mScoringSystem.HasBeatenRoadRageTarget())
            {
                s32 liTrainingType = BrnProgression::E_TRAINING_TYPE_TOTALLED;   // console li r11, 0x30 == 48
                lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&liTrainingType),
                                            GameStateModuleIO::E_ACTION_REQUEST_GAME_TRAINING, 4);
            }
        }
    }

    // ---- (14) in-progress: the mode clock has run out (road rage + burning route) -----------------
    if ((mpCurrentGameMode != nullptr) &&
        (mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS))
    {
        if ((meCurrentGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE) ||
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_BURNING_ROUTE))
        {
            if (mbIsInTimeUpOutro)
            {
                mfTimeUpStateTimer -= lfTimeStepSeconds;
                if (mfTimeUpStateTimer < 0.0f)
                {
                    // console stores {1, 0, 0}: TIMED OUT.
                    const GameStateModuleIO::PlayerFinishedModeEvent lEvent =
                        MakePlayerFinishedModeEvent(true, false, false);
                    PlayerFinishedMode(&lEvent);
                    mbIsInTimeUpOutro = false;
                }
            }
            else if (mScoringSystem.HasModeTimeExpired(lCurrentTime))
            {
                CGS_ASSERT(!mbIsInTimeUpOutro, "!mbIsInTimeUpOutro");   // BrnModeManager.cpp:2135

                mbIsInTimeUpOutro  = true;
                mfTimeUpStateTimer = KF_TIME_UP_OUTRO_SECONDS;

                // The one-byte payload: TRUE unless this was a failure. A burning route that runs
                // out of time has failed outright; a road rage that runs out of time has failed
                // only if the takedown target was never met.
                u8 lbSucceeded = static_cast<u8>(
                    (meCurrentGameModeType != GameStateModuleIO::E_MODE_BURNING_ROUTE) &&
                    ((meCurrentGameModeType != GameStateModuleIO::E_MODE_ROAD_RAGE) ||
                     mScoringSystem.HasBeatenRoadRageTarget()));
                lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lbSucceeded),
                                            GameStateModuleIO::E_ACTION_MODE_TIME_UP, 1);
            }
        }
    }

    // ---- (15) THE STUNT-RACE END CONDITION (mode 7) -----------------------------------------------
    // This is the poll the whole stunt-races campaign hangs on: once the stunt scorer says the run
    // is over, the player has finished the mode. (hazards H8: BrnBaselineLinkStubs.cpp's
    // `StuntModeScoring::HasStuntModeEnded { return true; }` makes this fire on frame 1 until the
    // per-symbol retire lands -- that is the stub, not this body.)
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_STUNT_ATTACK)
    {
        const bool lbModeIsOnline = (mpCurrentGameMode != nullptr) ? mpCurrentGameMode->IsOnline() : false;
        if (mScoringSystem.HasStuntAttackModeEnded(lCurrentTime, lePlayerActiveRaceCarIndex, lbModeIsOnline))
        {
            const GameStateModuleIO::PlayerFinishedModeEvent lEvent =
                MakePlayerFinishedModeEvent(false, false, false);
            PlayerFinishedMode(&lEvent);
        }
    }

    // ---- (16) the online tail ----------------------------------------------------------------------
    // Console: `if (mpCurrentGameMode && mpCurrentGameMode->IsOnline())`, then a fork between the
    // three online-stunt modes (12 / 14 / 17 -- the trio whose mapGameModes slots ALL alias the one
    // OnlineStuntRunMode object) and every other online mode.
    const bool lbCurrentModeIsOnline = (mpCurrentGameMode != nullptr) ? mpCurrentGameMode->IsOnline() : false;
    if (lbCurrentModeIsOnline)
    {
        const bool lbOnlineStuntFamily =
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN) ||
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE) ||
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END);

        if (lbOnlineStuntFamily)
        {
            // console `(*(*mode + 56))(mode, &mScoringSystem)` -- vtable slot 14, ShouldFinish.
            if (mpCurrentGameMode->ShouldFinish(&mScoringSystem))
            {
                const GameStateModuleIO::PlayerFinishedModeEvent lEvent =
                    MakePlayerFinishedModeEvent(false, false, false);
                PlayerFinishedMode(&lEvent);
                // [!] ONLINE ARM DEFERRED: the console also latches `this+2500` -- a byte INSIDE the
                // embedded OnlineStuntRunMode (+2256, so mode+244), the "results already sent"
                // one-shot. OnlineStuntRunMode exposes no accessor for it.
            }
            else
            {
                // [!] ONLINE ARM DEFERRED (hazards H7 lists this post by name). Console:
                //   if (mScoringSystem.HasStuntAttackModeEnded(lCurrentTime,
                //                                              lePlayerActiveRaceCarIndex,
                //                                              mpCurrentGameMode->IsOnline())
                //       && !mOnlineStuntRun's results-sent latch)
                //   {
                //       SendModeResults(lpOutputBuffer->GetGameActionQueue());   // the COMMITTED body
                //       mOnlineStuntRun's results-sent latch = true;
                //       *(this + 27868) = lePlayerActiveRaceCarIndex;   // inside the HUDMessageLogic span
                //       AddEvent(lpOutputBuffer->GetGameActionQueue(), &payload, 170, 20)
                //         payload = { lePlayerActiveRaceCarIndex, 7, 100.0f, 5, (u8)1 }
                //   }
                // Deferred for three reasons at once: the results-sent latch has no accessor, the
                // +27868 store lands inside mHUDMessageLogic (whose lifecycle conductor decision #4
                // parks), and reaching lpOutputBuffer->GetGameActionQueue() needs
                // BrnGameStateModuleIO.h -- which BrnModeManager.h bans for the EActiveRaceCarIndex
                // dual-scope hazard.
            }
        }
        else
        {
            if (mScoringSystem.HasModeTimeExpired(lCurrentTime))
            {
                // console calls HasModeTimeExpired a SECOND time and stores its result as byte 0 --
                // i.e. the event's mbTimedOut. Inside this arm it is true by construction; the
                // second call is preserved as the literal reconstruction of the console's shape.
                const GameStateModuleIO::PlayerFinishedModeEvent lEvent =
                    MakePlayerFinishedModeEvent(mScoringSystem.HasModeTimeExpired(lCurrentTime), false, false);
                PlayerFinishedMode(&lEvent);
            }
            else if (mScoringSystem.IsTimeLimitActive() &&
                     (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_BURNING_HOME_RUN))
            {
                // [!] ONLINE ARM DEFERRED (hazards H7 lists this post by name). Console:
                //   CgsSystem::Time lRemaining = mScoringSystem.GetModeTimeRemaining(lCurrentTime);
                //   AddEvent(lpGameActionQueue, &lRemaining, 152, 8)
                // 152 == DWARF E_ACTION_MODE_TIME_TIMEOUT (144) + the proven +8 shift for that band;
                // filed with the other three action ids as header_request #12.
                // The `IsTimeLimitActive()` guard is the console's own
                // `mStartTime.GetSeconds() >= 0 && mEndTime.GetSeconds() >= 0` pair, de-inlined to
                // the committed predicate (BrnScoringSystem_Timer.cpp:80).
            }
        }
    }

    // ---- (17) THE EXIT GATE ------------------------------------------------------------------------
    // console `lbz r11, 0xAD(mode)` -- the mode's own mbFinished. 18 is the "no next mode" sentinel.
    if (ModeLatch::IsFinished(mpCurrentGameMode))
    {
        ExitCurrentMode(lpOutputBuffer,
                        mbHasTimedOut,   // console lbzx this, 0x94FB
                        static_cast<GameStateModuleIO::EGameModeType>(KI_NO_NEXT_GAME_MODE));
    }

    // ---- (18) the two idle-time training tips --------------------------------------------------------
    // [!] ONLINE ARM DEFERRED (both legs). Console:
    //   if (meCurrentGameModeType == E_MODE_ONLINE_FREE_BURN_LOBBY && mfTimeInMode > 240.0f &&
    //       (*(u64*)(mpProgressionManager + 0x1CE30) & (1ull << 59)) == 0)
    //       AddEvent(queue, &E_TRAINING_TYPE_FREEBURNING_ONLINE /*59*/, 149, 4);
    //   if (mpCurrentGameMode->IsOnline() && mfTimeInOnline > 300.0f &&
    //       (*(u64*)(mpProgressionManager + 0x1CE38) & 0x40) == 0)
    //       AddEvent(queue, &E_TRAINING_TYPE_ONLINE_WIN_CAR /*70*/, 149, 4);
    // Both gates are raw bit tests into the ProgressionManager's saved profile flag words
    // (+0x1CE30 / +0x1CE38) -- "has this tip already been shown". BrnProgressionManager.h exposes no
    // named accessor for either word and this partfile does not fabricate the offsets (hazards H9).
    // Both legs are lobby/online-only; the two clocks they read (mfTimeInMode / mfTimeInOnline) are
    // accumulated by PreWorldUpdate (agent 7a), not here.
}

// ============================================================================================
// ModeManager::CheckCountdownDisplay -- X360 0x82342898
// ============================================================================================
// Publish the countdown number to the GUI, but only on the frames it actually changes.
void ModeManager::CheckCountdownDisplay(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    // The console inlines IsInGameMode() as `if (!mpCurrentGameMode)`; the assert string names the
    // predicate, so it is de-inlined back into the real call.
    CGS_ASSERT(IsInGameMode(), "IsInGameMode()");   // BrnModeManager.cpp:2449

    s32 liNewCountdownDisplay = 0;
    if (mpCurrentGameMode->HasCountdownDisplayChanged(&liNewCountdownDisplay))
    {
        // The console copies the out-param into a SECOND stack slot and posts that; the action
        // record is the committed GameStateModuleIO::SetCountdownAction (whose own comment already
        // names this function as its producer and @0x823EAD50 as its consumer).
        GameStateModuleIO::SetCountdownAction lAction;
        lAction.miCountdownDisplay = liNewCountdownDisplay;
        lpGameActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lAction),
                                    GameStateModuleIO::E_ACTION_SET_COUNTDOWN,
                                    static_cast<s32>(sizeof(GameStateModuleIO::SetCountdownAction)));
    }
}

// ============================================================================================
// ModeManager::UpdateCheckpointDistanceRequests -- X360 0x823279B8
// ============================================================================================
// Ask the world for the driving route between checkpoint N and checkpoint N+1, one pair per
// request. UpdateCurrentMode pumps this while mbIsCalculatingCheckpointDistances is set; the
// per-request arming flag is mbNeedToSendNextRequest.
void ModeManager::UpdateCheckpointDistanceRequests(GameStateModuleIO::GameActionQueue* lpGameActionQueue)
{
    if (!mbNeedToSendNextRequest)   // console lbzx this, 0x8C0C
    {
        return;
    }

    CGS_ASSERT(muNextDistanceRequestCheckpoint < (muNumLandmarks - 1),
               "muNextDistanceRequestCheckpoint < muNumLandmarks - 1");   // BrnModeManager.cpp:2288

    const u32 luCheckpoint = muNextDistanceRequestCheckpoint;

    // Both landmarks are resolved through the SAME TriggerData the checkpoint tracker uses -- the
    // console fetches it twice, once per landmark, from mpTriggerQueryManager's ResourcePtr at
    // TQM+0x620 (the accessor grow BrnModeManager.h documents as GetCheckpointTriggerData()).
    // maLandmarkIndices holds TriggerData REGION indexes, sign-extended from the stored u16
    // (console `lhzx` then `extsh`), which is what makes the 0xFFFF clear value read as -1.
    const s32 liCurrentRegionIndex     = static_cast<s16>(maLandmarkIndices[luCheckpoint]);
    const BrnTrigger::Landmark* lpCurrentLandmark =
        GetCheckpointTriggerData()->GetLandmarkFromRegionIndex(liCurrentRegionIndex);

    const s32 liDestinationRegionIndex = static_cast<s16>(maLandmarkIndices[luCheckpoint + 1]);
    const BrnTrigger::Landmark* lpDestinationLandmark =
        GetCheckpointTriggerData()->GetLandmarkFromRegionIndex(liDestinationRegionIndex);

    CGS_ASSERT(lpCurrentLandmark != nullptr, "lpCurrentLandmark");           // BrnModeManager.cpp:2297
    CGS_ASSERT(lpDestinationLandmark != nullptr, "lpDestinationLandmark");   // BrnModeManager.cpp:2298

    RouteRequestEventPayload lRouteRequest;
    std::memset(&lRouteRequest, 0, sizeof(lRouteRequest));

    // console: three `lfs` from landmark+0/4/8 plus an explicit 0.0 in the fourth lane, then one
    // 16-byte vector move into the record. Landmark's first member is its TriggerRegion BoxRegion,
    // whose first member is the position -- reached by name, exactly as the committed
    // ModeManager::GetCheckpointPosition does. FLAG: the console zeroes the w lane; a host Vector3
    // copy carries whatever w the region's position holds.
    lRouteRequest.mCurrentPosition     = lpCurrentLandmark->GetBoxRegion()->GetPosition();
    lRouteRequest.mDestinationPosition = lpDestinationLandmark->GetBoxRegion()->GetPosition();

    lRouteRequest.miCurrentNodeType     = 0;   // console stw of the zero register
    lRouteRequest.miDestinationNodeType = 0;

    lRouteRequest.mCurrentLandmarkId     = maLandmarkCgsIDs[luCheckpoint];
    lRouteRequest.mDestinationLandmarkId = maLandmarkCgsIDs[luCheckpoint + 1];

    lRouteRequest.mu16CurrentSectionIndex     = mauLandmarkSectionIndices[luCheckpoint];
    lRouteRequest.mu16DestinationSectionIndex = mauLandmarkSectionIndices[luCheckpoint + 1];
    lRouteRequest.mu16CheckpointIndex         = static_cast<u16>(luCheckpoint);

    // [X][X] FRONTIER -- header_request #10. Console:
    //   GameStateModule::SendRouteRequestAction(mpGameStateModule, &lRouteRequest,
    //                                           lpGameActionQueue, 2)   // @0x82381DC8
    // That method does not exist on the host GameStateModule (it owns the action id, the AddEvent
    // and the per-node AI-section resolution; its own asserts are "lpRouteRequestEvent" and
    // "lpOutputActionQueue" at BrnGameStateModule.cpp:5899/5900). The record above is built
    // byte-for-byte so the call is a one-line re-arm the moment the callee lands. Note the request
    // is NOT self-clearing: mbNeedToSendNextRequest and muNextDistanceRequestCheckpoint are advanced
    // by the ROUTE RESPONSE path, not by this producer -- do not "fix" that here.
    // [!] FIX ROUND 2026-08-26: an invented `CGS_ASSERT(mpGameStateModule != nullptr,
    // "mpGameStateModule")` used to stand here. REMOVED. This export fires exactly THREE asserts
    // (BeginAssert at 0x82327A0C, 0x82327A88, 0x82327AAC -- the checkpoint bound at line 0x8F0 and
    // lpCurrentLandmark / lpDestinationLandmark at 0x8F9 / 0x8FA), and this body already carries all
    // three, correctly. The producer tail 0x82327B00..0x82327B88 just does `lwz r3, 0x6D58(r31)` and
    // calls -- no assert at the site.
    (void)lpGameActionQueue;
    (void)KI_ROUTE_REQUEST_NODE_COUNT;
}

// ============================================================================================
// ModeManager::PlayerFinishedMode -- X360 0x823280D8
// ============================================================================================
// THE FINISH FUNNEL. Every end-of-mode arm in UpdateCurrentMode lands here, as does the
// E_EVENT_PLAYER_FINISHED_MODE game event. It records WHY the player finished, optionally registers
// the finish with the scorer, hands the pad back to the inactive-game-mode state, and arms
// FinishCurrentMode for the next update.
void ModeManager::PlayerFinishedMode(const GameStateModuleIO::PlayerFinishedModeEvent* lpEvent)
{
    CGS_ASSERT(lpEvent != nullptr, "lpPlayerFinishedModeEvent");   // BrnModeManager.cpp:2875

    // [x] H4 SETTLED (closure round 2026-08-26) -- THIS BODY IS THE WRITER THAT DECIDED IT.
    // The console sets 0x94FD from the event's mbTimedOut and 0x94FE from its mbCarDestroyed, and
    // the PS3 DWARF names the two inlined setters SetTimedOut() / SetCrashedOut(); the header's
    // bytes are now named for exactly that -- mbPlayerFinishedTimedOut (+38141) and
    // mbPlayerFinishedCarDestroyed (+38142). This is NOT a two-byte shift onto the header's own
    // mbHasTimedOut / mbHasCrashedOut at +38139 / +38140: those are the MODE's flags, and
    // UpdateCurrentMode independently reads +38139 as ExitCurrentMode's lbTimedOut argument. Both
    // readers are real and both bytes keep exactly one name. Evidence: BrnModeManager.h's H4 block.
    if (lpEvent->mbTimedOut)
    {
        mbPlayerFinishedTimedOut = true;              // console stbx 1, this, 0x94FD  (DWARF: SetTimedOut)
    }
    if (lpEvent->mbCarDestroyed)
    {
        mbPlayerFinishedCarDestroyed = true;   // console stbx 1, this, 0x94FE  (DWARF: SetCrashedOut)
    }

    if (lpEvent->mbCrossedFinishLine)
    {
        // console reads the SIM sub-status time pair (this+0x6DC8 / +0x6DCC == mTimerStatusInterface
        // + 24 + 16 / +20) straight into the RegisterFinishForCar argument.
        const CgsSystem::Time lFinishTime = mTimerStatusInterface.GetSimTimerStatus()->GetTime();

        CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule");   // BrnModeManager.cpp:5700
        const EActiveRaceCarIndex lePlayerActiveRaceCarIndex = mpGameStateModule->GetPlayerActiveRaceCarIndex();

        mScoringSystem.RegisterFinishForCar(true, lePlayerActiveRaceCarIndex, lFinishTime);
    }

    // The tail, in the console's order. THREE stores, no assert -- re-derived instruction for
    // instruction from 0x823281AC..0x823281D8 (fix round 2026-08-26):
    //     0x823281D0  stwx  r9(-1), r31, 0x9518   -> miDebugFinishPosition = -1
    //     0x823281D4  stbx  r28(1), r31, 0x94F7   -> mbFinishCurrentModeNextUpdate = true
    //     0x823281D8  stwx  r6(2),  r10, 0x38B64  -> meControllerState = INACTIVE_GAME_MODE (parked)
    // [!] FIX ROUND: the -1 store was previously lost. This body called
    // FinishCurrentModeNextUpdate() alone and its comment claimed that call was "the inlined pair" --
    // it is not: ModeManager::FinishCurrentModeNextUpdate (BrnModeManager_Accessors.cpp:210-213)
    // sets ONLY the bool. Without the -1, any earlier
    // FinishCurrentModeNextUpdateWithFinishPosition(n) override survives, and the committed
    // SendModeResults gate (BrnModeManager.cpp:373, `mbFinishCurrentModeNextUpdate &&
    // miDebugFinishPosition > 0`) then forces a stale finish position into every later result.
    // The store is emitted here, in console order, ahead of the bool.
    // [!] FIX ROUND: the invented `CGS_ASSERT(mpGameStateModule != nullptr, "mpGameStateModule")`
    // that used to sit here is DELETED -- 0x823281AC..0x823281D8 fires no assert. This body's one
    // real mpGameStateModule assert is BrnModeManager.cpp:5700 (`li r5,0x1644` @0x8232817C) inside
    // the mbCrossedFinishLine arm, and it is already reproduced above.
    // RE-ARMED 2026-08-26 (mode-tick verify): header_request #9b's setter landed
    // (BrnGameStateModule.h:750), so the console's inlined store runs BY NAME. Console
    // `stwx 2, gsm, 0x38B64` == GameStateModule::SetInActiveGameModeState().
    mpGameStateModule->SetInActiveGameModeState();
    miDebugFinishPosition = -1;      // console: stwx r9(-1), r31, 0x9518 @0x823281D0
    FinishCurrentModeNextUpdate();   // console: stbx r28(1), r31, 0x94F7 @0x823281D4
}

} // namespace BrnGameState
