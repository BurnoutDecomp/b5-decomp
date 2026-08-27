// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/BrnModeManager_WorldTick.cpp
// ============================================================================
// Partfile of the BrnGameState::ModeManager TU (owning header BrnModeManager.h).
// Wave-B keystone, AGENT 7a (conductor decision #8 split). Bodies the TWO per-frame
// world ticks and nothing else:
//
//   ModeManager::PreWorldUpdate    X360 0x823537B8
//   ModeManager::PostWorldUpdate   X360 0x8234A9E0
//
// Agent 7b owns the five small callees these two drive (TransmitCheckPointDistancesToFinishLine
// 0x82341FF8, TransmitAndIncrementCheckPointsReached 0x82342098, TransmitAndIncrementFinishReached
// 0x823424D0, ProcessPlayerCrashes 0x8231E638, CheckForOutOfRangeCarsReachingFinish 0x82340800) in
// its OWN partfile. They are CALLED here, never defined here.
//
// [X] hazards H2: the 16 committed BrnModeManager.cpp bodies are CALLED, never re-implemented.
//     Nothing in this file redefines one.
//
// -- WHY THE PSEUDOCODE WAS NOT TRUSTED ------------------------------------------------------
// PreWorldUpdate's IDA prototype is 30 `int` arguments ("local variable allocation has failed",
// hazards H9). The real shape is the DWARF ten-argument one (BrnModeManager.h:276), and the
// mapping was recovered from the ASM parameter-save-area offsets, not guessed:
//     r3..r10        = this, lpOutputBuffer, lpPreWorldInputBuffer, lrTimerStatusInterface,
//                      lePlayerActiveRaceCarIndex, lePlayerGlobalRaceCarIndex, lbIsOnline,
//                      lpGameActionQueue
//     caller_sp+0x54 = arg 9  (word)  -> lpGlobalRaceCarOutput   (IDA's bogus "a28")
//     caller_sp+0x5C = arg 10 (word)  -> lpActiveRaceCarOutput   (IDA's "lpActiveCarInterface")
//     caller_sp+0x67 = arg 11 (byte)  -> lbPaused                (IDA's "arg_67")
// Proof of the slot pitch: UpdateCurrentMode @0x82350EC8 READS its own two stack arguments at
// `arg_54` (word) / `arg_5F` (byte), and PreWorldUpdate WRITES its outgoing pair to `var_BC`
// (== sp+0x54) / `var_B1` (== sp+0x5F) at 0x823538A4 / 0x823538AC -- 8-byte right-justified
// slots (the X360 param save area), exactly as the house note says.
//
// -- ARMING STATE (hazards H1 supersession) ---------------------------------------------------
// [!!] BOTH BODIES BELOW ARE LANDED BUT UNCALLED ON THIS BUILD, deliberately, exactly like
// ModeManager::Construct (agent 1's banner). GameStateModule still drives the bring-up seam:
//   BrnGameStateModule.cpp:1268   mModeManager.PreWorldUpdateClocksBringUp(lfGameTimestep);
// and there is no ModeManager::PostWorldUpdate call site at all (grepped: BrnGameStateModule.cpp
// names mModeManager eleven times and none of them is a world tick).
// PreWorldUpdate below CONTAINS the whole of PreWorldUpdateClocksBringUp -- the mode/online/
// freeburn clock if-else at 0x82353A98..0x82353B94 is the same code, reached through the same
// named accessors (GetCarSelectManager()->GetJunkyardId(), GetTrainingManager()->IsInPictureParadise()).
// DELETE-WHEN: the moment BrnGameStateModule.cpp:1268 is re-pointed at this PreWorldUpdate --
// which needs the ten arguments (both race-car output interfaces, the pre-world input buffer, the
// output buffer, the action queue and the live TimerStatusInterface) to be reachable at that call
// position -- PreWorldUpdateClocksBringUp must be deleted from ModeManager_gUI_00.cpp and from the
// header IN THE SAME CHANGE. NEVER LEAVE BOTH ARMED: the clocks would double-accumulate, and
// mfTimeInFreeBurn feeds TrainingManager's WAIT_FOR_MESSAGE gate (> 3.0 s).
//
// -- WHAT IS GATED AND WHY (hazards H7) -------------------------------------------------------
// Every omission below carries its own banner naming the exact console call. Summary:
//   ONLINE ARM DEFERRED  : the online-stunt network-player-results sweep, UpdatePaybackTakedowns,
//                          UpdateOnlineStuntModeScorePreWorld, StuntModeScoringOnline::Update,
//                          the mode-15 BurnoutSkillzManager tick, the online-stunt SetPlayerStuntScore.
//   DIVERGENCE           : ChallengeManager::PreWorldUpdate / ::PostWorldUpdate (not embedded).
//   PARKED (conductor #4): HUDMessageLogic::PreWorldUpdate / ::PostWorldUpdate.
//   PARKED (header)      : every site whose declaration this frozen tree does not carry; each one
//                          is filed as a numbered header_request in agent 7a's report.
// THE OFFLINE STUNT-RACE PATH IS WHOLE: UpdateCurrentMode, the three Transmit* calls, the
// FinishCurrentMode / ShouldFinish / ShouldExit / ExitCurrentMode ladder, the three clocks, and in
// PostWorldUpdate the mode-7 StuntModeScoring::Update driver plus the whole ScoringSystem update
// sweep that feeds it.

#include "GameSource/GameState/ModeManager/BrnModeManager.h"

// The spine buffers. BrnModeManager.h deliberately BANS this include (its EActiveRaceCarIndex
// dual-scope banner at :53-61); each partfile includes it locally, where the scope question is
// local -- which is why every enum-typed parameter below is spelled with the leading `::`.
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateModule.h"                        // GetCarSelectManager / GetTrainingManager (the freeburn clock gate)
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"        // TrainingManager::IsInPictureParadise (gsm+0xB644)
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"             // CarSelectManager::GetJunkyardId      (gsm+0x2CDC0)
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h"   // StuntModeScoring::PreWorldUpdate / ::Update
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"            // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"   // PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                  // gpDebugPrint ([queue-hwm] diag)
#include <stdlib.h>                                                        // getenv ([queue-hwm] diag)

namespace BrnGameState
{

// The two PerfMon handles. On console these are the BrnModeManager.cpp FILE STATICS
// dword_82CDB6F4 (pre) / dword_82CDB6F8 (post) -- the DecFIGS dump spells them
// `extern int32_t miPreWorldUpdatePM / miPostWorldUpdatePM` precisely because they are NOT
// members. The host TU is split across partfiles, so agent 1 defines them at namespace scope in
// BrnModeManager_Lifecycle.cpp:64-65 and names this exact extern pair as the way to reach them.
extern s32 miPreWorldUpdatePM;
extern s32 miPostWorldUpdatePM;

// The idiom both bodies below re-evaluate at every gate: `lwz r11, 0x28(mode); addi r11, r11, -2;
// cntlzw; extrwi r11, r11, 1, 26` -- i.e. (mode != NULL && mode->GetCurrentState() == E_GMS_IN_PROGRESS).
// PreWorldUpdate spells it out three times and PostWorldUpdate SIX (0x8234AC1C, 0x8234AC8C,
// 0x8234ADB4, 0x8234AE30, 0x8234AFA4, 0x8234B08C), each one a FRESH load of mpCurrentGameMode and a
// fresh read of +0x28. Kept as a re-evaluated call rather than one hoisted local on purpose: the
// mode's own PostWorldUpdate (vtable slot 3) runs between the first two sites and can move the
// state, so caching it once would be a behaviour change the console does not make.
static bool
IsGameModeInProgress(const GameMode* lpGameMode)
{
    return (lpGameMode != NULL) &&
           (lpGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS);
}

// ==============================================================================================
// ModeManager::PreWorldUpdate -- X360 0x823537B8
// ==============================================================================================
// Console call order, verbatim from the asm:
//   PerfMonCpu::StartMonitor(miPreWorldUpdatePM)
//   mpGameActionQueue = lpGameActionQueue                                     (+0x6D68)
//   mTimerStatusInterface = lrTimerStatusInterface                            (+0x6DA0, 2 x 24 B)
//   mePlayerActiveRaceCarIndex / mePlayerGlobalRaceCarIndex                   (+0x8038 / +0x803C)
//   if (mpCurrentGameMode) { UpdateCurrentMode; <online results sweep>;
//                            UpdateNetworkPlayerResults; <UpdatePaybackTakedowns>;
//                            mfTimeInMode += dt; mfTimeInOnline += dt or 0 }
//   else                   { mfTimeInOnline = 0; mfTimeInFreeBurn += dt or 0 }
//   <the per-mode stunt-scorer pre-world fork>
//   ChallengeManager::PreWorldUpdate
//   TransmitCheckPointDistancesToFinishLine / ...CheckPointsReached / ...FinishReached
//   HUDMessageLogic::PreWorldUpdate
//   muUnkByte_0x950B = 0; the two consume-and-clear latches
//   the ShouldFinish / ShouldExit ladder
//   publish the game-mode output block; roll the previous type/state; mpGameActionQueue = 0
//   ScoringSystemDebugComponent::DebugRenderChainableStunts
//   PerfMonCpu::StopMonitor(miPreWorldUpdatePM)
// ==============================================================================================
void
ModeManager::PreWorldUpdate(GameStateModuleIO::OutputBuffer*              lpOutputBuffer,
                            const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer,
                            const CgsSystem::TimerStatusInterface&        lrTimerStatusInterface,
                            ::EActiveRaceCarIndex                         lePlayerActiveRaceCarIndex,
                            ::EGlobalRaceCarIndex                         lePlayerGlobalRaceCarIndex,
                            bool                                          lbIsOnline,
                            GameStateModuleIO::GameActionQueue*           lpGameActionQueue,
                            const BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface* lpGlobalRaceCarOutput,
                            const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput,
                            bool                                          lbPaused)
{
    CgsDev::PerfMonCpu::StartMonitor(miPreWorldUpdatePM);

    // `stw r18, 0x6D68(r31)` -- the queue is published on the member for the whole tick and
    // cleared again at the very end (see the tail). Nothing may leave it set across frames.
    mpGameActionQueue = lpGameActionQueue;

    // 0x823537F8..0x82353870: twelve field-for-field copies, source +0x00..+0x14 then +0x18..+0x2C,
    // destination this+0x6DA0 (== +28064) -- i.e. the WHOLE 48-byte TimerStatusInterface, both
    // 24-byte sub-statuses. That store run is what pins mTimerStatusInterface at +28064 and NOT at
    // header_grow_spec's +28060: agent 1's deviation D2, confirmed here independently.
    mTimerStatusInterface = lrTimerStatusInterface;

    // `stwx r29, r31, 0x8038` / `stwx r28, r31, 0x803C`.
    mePlayerActiveRaceCarIndex = lePlayerActiveRaceCarIndex;
    mePlayerGlobalRaceCarIndex = lePlayerGlobalRaceCarIndex;

    // The console reads the timestep off the INCOMING interface (`r26 = a4 + 0x18`, then
    // `lfs 8(r26) * lfs 4(r26)`), i.e. the SIM sub-status' mfTimeStepMultiplier * mfBaseTimeStep.
    // Same number as the member copy above by construction; kept on the parameter to stay literal.
    const f32 lfSimTimeStep =
        lrTimerStatusInterface.GetSimTimerStatus()->GetCurrentTimeStep();

    if (mpCurrentGameMode != NULL)
    {
        UpdateCurrentMode(lpOutputBuffer,
                          lpPreWorldInputBuffer,
                          lpPreWorldInputBuffer->GetPlayerStatusInterface(),
                          lePlayerActiveRaceCarIndex,
                          lbIsOnline,
                          lpGameActionQueue,
                          lpGlobalRaceCarOutput,
                          lpActiveRaceCarOutput,
                          lbPaused);

        // [!] [stuntrace] ONLINE ARM DEFERRED -- the online-stunt disconnect sweep.
        // Console 0x823538F4..0x823539FC, entered only for meCurrentGameModeType 12 / 14 / 17 (the
        // one aliased OnlineStuntRunMode): it walks the player-results array on
        // lpActiveRaceCarOutput (`*(iface+512)` entries of 28 bytes), fires the three verbatim
        // asserts
        //   "Array used before Construct/Clear was called" (CgsArray.h:336),
        //   "liIndex >= 0"            (BrnNetworkModulePlayerResultsInterface.h:141),
        //   "lpPlayerResultsData"     (BrnModeManager.cpp:599)
        // and, for each still-present record whose scoring CarData is not marked at +217, stores
        // the record's active-car index into this+27868 (X360 +0x6CDC).
        // NOT REPRODUCED: +0x6CDC has no member on this tree. It lands INSIDE the HUDMessageLogic
        // span (27392..27992) -- the same SUSPECT region the header's SetNetworkStuntScore stand-in
        // trio (+0x6CD0/D4/D8) is already flagged against -- so naming a fourth byte there blind
        // would guess a layout the header explicitly refuses to guess. An offline stunt race
        // (mode 7) never enters this arm.

        // ScoringSystem::UpdateNetworkPlayerResults(&mScoringSystem, results, lbFinal).
        // lbFinal (r29) = mode->IsOnline() && !(type == 15 || type == 16) -- 0x82353A00..0x82353A50.
        // KEPT WHOLE: both the interface accessor and the scorer entry point exist by name with the
        // console's arity, and on an offline event the results interface carries no players.
        const bool lbFinalNetworkResults =
            mpCurrentGameMode->IsOnline() &&
            !(meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
              meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);
        mScoringSystem.UpdateNetworkPlayerResults(
            lpPreWorldInputBuffer->GetNetworkPlayerResultsInterface(), lbFinalNetworkResults);

        // [!] [stuntrace] ONLINE ARM DEFERRED -- payback (dirty-trick) takedowns.
        // Console 0x82353A70..0x82353A94:
        //   ScoringSystem::UpdatePaybackTakedowns(&mScoringSystem,
        //       lpPreWorldInputBuffer->GetNetworkToGameStateInterface()->GetDirtyTrickQueue(),
        //       lpOutputBuffer->GetGameStateToNetworkInterface());
        // (first argument is netToGameState+0x2268 == the documented DirtyTrickQueue seat; second is
        //  OutputBuffer+0x4190 == GetGameStateToNetworkInterface, X360 0x8231D800, write-lock line 290.)
        // NOT REPRODUCED, TWO REASONS: (a) the committed ScoringSystem declaration takes TWO
        // DirtyTrickQueue pointers, not queue + interface (header_request #6); (b)
        // NetworkToGameStateInterface::GetDirtyTrickQueue() is declare-only on this tree
        // (BrnGameStateModuleIO.h:317), so calling it would be an unresolved external at link.
        // Payback takedowns are an online-only mechanic.

        // Clocks, mode branch. `addi r11, r11, -0x6AE0` == this+0x9520 == mfTimeInMode.
        mfTimeInMode += lfSimTimeStep;
        if (mpCurrentGameMode->IsOnline())
        {
            mfTimeInOnline += lfSimTimeStep;   // this+0x9524
        }
        else
        {
            // `lfs f0, flt_82001CC0` -- IMAGE-CITED: image.bin offset 0x1CC0 reads 00 00 00 00,
            // i.e. 0.0f. Not a placeholder zero: it is the authored reset.
            mfTimeInOnline = 0.0f;
        }

        // ==========================================================================================
        // [DIAG] NOT IN THE X360 BINARY -- the `[mode-fsm]` rung (added 2026-08-26, stuntrace fsm
        // round). ONE rung, and it is the instrument the previous round did not have.
        // ==========================================================================================
        // WHAT IT ANSWERS, and why the ladder could not: run 20260826_194302 concluded "the mode
        // state machine never ticks" from the `e-count (never)` mark, but that mark reads the
        // `[evt-flow]` bridge diag, which has a SHARED 24-LINE BUDGET
        // (GameBridgeGameStateToX_EventFlowGuiEvents.cpp:396) that the per-frame action-201 junction
        // post exhausts within half a second of the start. The ladder went blind, not quiet. This
        // rung has no budget and reports the three words the question is actually about:
        //   * the mode type,
        //   * GameMode::GetCurrentState() (the SAME word SetCurrentState writes -- host +0x50 at
        //     both ends, verified in the built objects, so there is no reader/writer split here),
        //   * the meControllerState-equivalent IsControllerActive() (the member itself is private;
        //     this is its only public reader, and UpdateCurrentMode's countdown arm now writes 3),
        // plus TWO starvation meters that are the point of the rung:
        //   * `tick`, the number of times THIS arm has run, and
        //   * mfTimeInMode, the game time the mode has actually been given.
        // A mode that is alive but starved prints a slowly-rising tick with a still state; a mode
        // whose arm is skipped prints nothing at all. The two were indistinguishable before.
        // KEY includes floor(mfTimeInMode), so a healthy run prints one line per second of mode
        // time (a heartbeat) and an extra line the instant the state moves -- ~8 lines for a whole
        // stunt-run start, not a per-frame stream.
        // GATE: BRN_PROP_DIAG, the same env the rest of the `[UI-gate]`/`[evt-flow]` ladder stands
        // behind and the one flow_run.ps1 already records in marks.txt as DIAGENV -- so the rung
        // prints on the next stunt run with no harness change. Delete with the bring-up.
        {
            static const bool sbFsmDiag = (getenv("BRN_PROP_DIAG") != 0);
            if (sbFsmDiag && CgsDev::Log::gpDebugPrint != 0)
            {
                static s32 siTicks        = 0;
                static s32 siLastMode     = -2;
                static s32 siLastState    = -2;
                static s32 siLastCtrl     = -2;
                static s32 siLastSecond   = -2;

                ++siTicks;

                const s32 liMode   = static_cast<s32>(meCurrentGameModeType);
                const s32 liState  = mpCurrentGameMode->GetCurrentState();
                const s32 liCtrl   = (mpGameStateModule != NULL &&
                                      mpGameStateModule->IsControllerActive()) ? 1 : 0;
                const s32 liSecond = static_cast<s32>(mfTimeInMode);

                if (liMode != siLastMode || liState != siLastState ||
                    liCtrl != siLastCtrl || liSecond != siLastSecond)
                {
                    siLastMode   = liMode;
                    siLastState  = liState;
                    siLastCtrl   = liCtrl;
                    siLastSecond = liSecond;

                    *CgsDev::Log::gpDebugPrint
                        << "[mode-fsm] tick " << siTicks
                        << " mode " << liMode
                        << " state " << liState
                        << " (0=countdown 1=intro 2=inprogress 3=outro 4=results 5=quit)"
                        << " controllerActive " << liCtrl
                        << " timeInMode " << mfTimeInMode << "\n";
                }
            }
        }
    }
    else
    {
        mfTimeInOnline = 0.0f;   // flt_82001CC0 again, stored BEFORE the junkyard test

        // `lwz r9, 0x6D58(r31)` -- mpGameStateModule, read WITHOUT an assert at this console site.
        // (The bring-up leg's CGS_ASSERT is deliberately NOT reproduced: no such assert exists at
        //  0x82353B10..0x82353B5C, and this file carries only verbatim X360 asserts.)
        // `ldx r11, r9, 0x2CDC0` is a 64-bit CgsID load == the junkyard id;
        // `lbzx r11, r9, 0xB644` is the picture-paradise bool. Both reached by name, exactly the way
        // the superseded PreWorldUpdateClocksBringUp reaches them.
        const bool lbInJunkyard =
            (mpGameStateModule->GetCarSelectManager()->GetJunkyardId() != 0);
        const bool lbInPictureParadise =
            (mpGameStateModule->GetTrainingManager() != NULL) &&
            mpGameStateModule->GetTrainingManager()->IsInPictureParadise();

        if (lbInJunkyard || lbInPictureParadise)
        {
            mfTimeInFreeBurn = 0.0f;
        }
        else
        {
            mfTimeInFreeBurn += lfSimTimeStep;   // this+0x951C
        }
    }

    // ------------------------------------------------------------------------------------------
    // The per-mode stunt-scorer pre-world fork (0x82353B94..0x82353C80).
    // ------------------------------------------------------------------------------------------
    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_STUNT_ATTACK &&
        IsGameModeInProgress(mpCurrentGameMode))
    {
        // THE offline stunt-race producer: StuntModeScoring::PreWorldUpdate(this+0x1100,
        // lpOutputBuffer->GetGameActionQueue()). this+0x1100 == 4352 == &mScoringSystem + 0x350 ==
        // the OFFLINE stunt scorer, reached BY NAME through GetStuntScorer() (hazards H9: never a
        // raw offset off `this`).
        mScoringSystem.GetStuntScorer()->PreWorldUpdate(lpOutputBuffer->GetGameActionQueue());
    }
    else
    {
        const bool lbOnlineStuntFamily =
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE)  ||
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN) ||
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END);
        if ((lbOnlineStuntFamily || mbStuntChallengeActive) &&
            IsGameModeInProgress(mpCurrentGameMode))
        {
            // [!] [stuntrace] ONLINE ARM DEFERRED -- the online stunt scorer's pre-world step.
            // Console 0x82353C50..0x82353C7C:
            //   ScoringSystem::UpdateOnlineStuntModeScorePreWorld(&mScoringSystem,
            //       *(lpPreWorldInputBuffer->GetNetworkToGameStateInterface() + 0x2434),
            //       lpOutputBuffer->GetGameActionQueue(),
            //       mbStuntChallengeActive);
            // NOT REPRODUCED: the committed declaration is the TWO-argument
            // (s32 liCurrentTimeMs, VariableEventQueue<13312,16>*) shape (header_request #7), and
            // the s32 it wants comes from an UNNAMED word at NetworkToGameStateInterface+0x2434
            // that this tree has no accessor for (header_request #8). Fabricating either would be
            // an invented offset. Offline stunt races take the mode-7 arm above instead.
        }
    }

    // [X][X] [stuntrace] DIVERGENCE: ChallengeManager NOT embedded/mounted (29 TUs, ~35 unresolved
    // externals; freeburn challenges are off the offline-event path -- header_grow_spec section 5).
    // Console 0x82353C80..0x82353CC4:
    //   ChallengeManager::PreWorldUpdate(this + 28160,
    //       &mTimerStatusInterface,
    //       *(lpPreWorldInputBuffer->GetNetworkToGameStateInterface() + 0x2434),
    //       lpPreWorldInputBuffer->GetNetworkToGameStateInterface() + 0x1B20,
    //       lpActiveRaceCarOutput,
    //       lpPreWorldInputBuffer->GetPlayerStatusInterface()-><byte @ +0x9EC>,
    //       lpOutputBuffer);
    // Behaviour lost: freeburn challenges do not tick. Re-wire when the ChallengeManager mount lands.

    // ------------------------------------------------------------------------------------------
    // The three checkpoint/finish transmitters (agent 7b's bodies -- CALLED here).
    // ------------------------------------------------------------------------------------------
    TransmitCheckPointDistancesToFinishLine(lpOutputBuffer, mScoringSystem);
    TransmitAndIncrementCheckPointsReached(lpGameActionQueue);
    TransmitAndIncrementFinishReached(lpGameActionQueue);

    // [x] UN-PARKED 2026-08-27 (stunt-scorer latch-drain fix). Console 0x82353CF4:
    //   HUDMessageLogic::PreWorldUpdate(&mHUDMessageLogic, lpGameActionQueue);
    // THE DRAIN half of the HUD-message pump: it bulk-Appends the frame's notifications into the
    // outgoing game-action queue and Clears the local <256,16> one. Without it the post-world
    // producer (GenerateStuntMessage, now live) would fill a 256-byte queue that nothing empties
    // and trip CgsVariableEventQueue.h's overflow assert after a handful of stunts. The GUI bridge
    // ignores the four notification types it has no arm for (TranslateGameActionsToGuiEvents'
    // `default:` returns false), so appending them is inert until those arms land.
    mHUDMessageLogic.PreWorldUpdate(lpGameActionQueue);

    // `stbx r27(=0), r31, 0x950B`. THIS IS THE ONLY WRITER OF muUnkByte_0x950B ANYWHERE: the byte
    // is cleared unconditionally every pre-world tick, before the two latch arms below. The header
    // records "no reader identified anywhere" -- still true, but the byte now has a proven writer,
    // which is why it cannot be pad. Reported to the verifier's bool-block collision pass (H4).
    muUnkByte_0x950B = 0;

    if (mpCurrentGameMode != NULL)
    {
        // Latch 1: FinishCurrentModeNextUpdate(WithFinishPosition) sets +0x94F7; it is consumed and
        // cleared HERE, and the finish is suppressed (but the latch still cleared) while mode data
        // is loading. The Time handed to FinishCurrentMode is read from the MEMBER interface at
        // +0x6DC8 / +0x6DCC == the SIM sub-status' mTime pair (seconds + fraction).
        if (mbFinishCurrentModeNextUpdate)
        {
            if (!mbModeDataIsLoading)
            {
                const CgsSystem::Time lTime = mTimerStatusInterface.GetSimTimerStatus()->GetTime();
                FinishCurrentMode(lpOutputBuffer, lTime);
            }
            mbFinishCurrentModeNextUpdate = false;
        }

        // Latch 2: the TellGuiToShowOnlineFinalStandings latch (+0x94F8) posts action 38 --
        // E_ACTION_FINISHED_MODE_RESULTS, size 1 -- and clears itself. The console passes an
        // UNINITIALISED one-byte stack local (`addi r4, r1, var_B0`, no preceding store): the action
        // carries no payload. Modelled as an explicit zero byte, the same shape
        // BrnGameStateModule.cpp:1133 uses for KI_ACTION_UNPAUSE.
        if (mbOnlineFinalStandingsShown)
        {
            u8 lacFinishedModeResults[1] = { 0 };
            lpGameActionQueue->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(lacFinishedModeResults),
                GameStateModuleIO::E_ACTION_FINISHED_MODE_RESULTS, 1);
            mbOnlineFinalStandingsShown = false;
        }
    }

    // ------------------------------------------------------------------------------------------
    // The finish / exit ladder (0x82353D90..0x82353EF4). Slot numbers are the vtable micro-check's
    // console map: vtbl+0x38 == slot 14 == ShouldFinish(ScoringSystem*),
    //              vtbl+0x34 == slot 13 == ShouldExit(const ScoringSystem*) const.
    // Both console call sites pass `r4 = this + 0xDB0` == &mScoringSystem.
    // ------------------------------------------------------------------------------------------
    if ((mpCurrentGameMode != NULL) && mpCurrentGameMode->IsOnline())
    {
        const bool lbInstantIntroMode =
            (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
             meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);

        if (!lbInstantIntroMode && !mbModeDataIsLoading &&
            mpCurrentGameMode->ShouldFinish(&mScoringSystem))
        {
            const CgsSystem::Time lTime = mTimerStatusInterface.GetSimTimerStatus()->GetTime();
            FinishCurrentMode(lpOutputBuffer, lTime);
        }
    }
    else if ((mpCurrentGameMode != NULL) &&
             (mpCurrentGameMode->GetCurrentState() == GameStateModuleIO::E_GMS_IN_PROGRESS))
    {
        if (!mbModeDataIsLoading && mpCurrentGameMode->ShouldFinish(&mScoringSystem))
        {
            const CgsSystem::Time lTime = mTimerStatusInterface.GetSimTimerStatus()->GetTime();
            FinishCurrentMode(lpOutputBuffer, lTime);
        }
        // [x] ARGUMENT RESTORED 2026-08-26 (fix round) -- the 26-slot BrnGameMode.h has landed, so
        // the "written in the compiling shape, re-point when the block lands" note that used to
        // stand here is DISCHARGED. Console 0x82353E9C..0x82353EAC:
        //     lwz r3, 0xD98(r31)        ; mpCurrentGameMode
        //     mr  r4, r29               ; r29 == this + 0xDB0 == &mScoringSystem
        //                               ;   (`addi r29, r31, 0xDB0` @0x82353CC8)
        //     lwz r11, 0(r3) / lwz r11, 0x34(r11) / mtctr / bctrl
        // vtbl+0x34 == slot 13 == `bool ShouldExit(const ScoringSystem*) const`, and the base body
        // (GameMode::ShouldExit @0x82315B80) READS that pointer -- it polls the shared "mode has
        // been quiet long enough" float pair with the 4.0 / 3.0 thresholds (10.0 for the second
        // while mbVisibleCars is set). The sibling slot-14 ShouldFinish call above takes the SAME
        // r29 (`mr r4, r29` @0x82353E5C), which is the cross-check that r29 is the ScoringSystem.
        else if (mpCurrentGameMode->ShouldExit(&mScoringSystem))
        {
            // The unsuccessful-attempt counter: reset when the attempted type changes, then
            // increment, then exit with lbTimedOut = true and the "no next mode" sentinel 18
            // (the console's `li r6, 0x12` == KI_GAME_MODE_SLOTS, hazards H3 -- NOT E_MODE_COUNT,
            // which is 17 on this build and is the last enumerator, not a count).
            if (meLastAttemptedGameModeType != meCurrentGameModeType)
            {
                miNumUnsucessfulGameModeAttempts = 0;
            }
            meLastAttemptedGameModeType = meCurrentGameModeType;
            ++miNumUnsucessfulGameModeAttempts;

            ExitCurrentMode(lpOutputBuffer, true,
                            static_cast<GameStateModuleIO::EGameModeType>(KI_GAME_MODE_SLOTS));
        }
    }

    // ------------------------------------------------------------------------------------------
    // Publish the game-mode output block, then roll the previous type/state.
    // `addis r11, r16, 3; addi r11, r11, -0x4F28` == lpOutputBuffer + 176344, the
    // GameModeOutputInterface seat (BrnGameStateModuleIO.h:259, console span 16):
    //     +0  = mePreviousGameModeType       +4  = mePreviousGameModeState
    //     +8  = meCurrentGameModeType        +12 = current mode state (or -1 with no mode)
    // ORDER IS LOAD-BEARING: the PREVIOUS pair is published BEFORE it is overwritten.
    //
    // [!] [stuntrace] PARKED (header) -- header_request #3. GameModeOutputInterface is a
    // forward-declared, field-less struct backed by opaque storage, and OutputBuffer offers only
    // the CONST getter, so there is no way to write those four words without inventing a layout.
    // Behaviour lost: BridgeGameStateToSound's 16-byte copy (GameBridgeGameStateToX.cpp:280) reads
    // a stale block, so mode-change sound cues do not re-trigger. Nothing on the stunt-race start
    // path depends on it.
    //
    // The member ROLL itself is NOT parked -- it is ModeManager's own state and UpdateCurrentMode /
    // ExitCurrentMode read it.
    // ------------------------------------------------------------------------------------------
    mePreviousGameModeType  = meCurrentGameModeType;
    mePreviousGameModeState = (mpCurrentGameMode != NULL)
                                  ? static_cast<GameStateModuleIO::EGameModeState>(
                                        mpCurrentGameMode->GetCurrentState())
                                  : GameStateModuleIO::E_GMS_INVALID;

    // ==============================================================================================
    // [DIAG] NOT IN THE X360 BINARY -- the GameAction-queue PEAK WATERMARK rung.
    // ==============================================================================================
    // Seam-audit action 12 (S4). WHY IT EARNS ITS PLACE, and why it is not optional before the
    // first boot test: CgsVariableEventQueue.h:443-467's AddEvent fires the overflow assert and
    // then **falls through to the memcpy anyway** -- there is no early return. Past 13312 bytes
    // that memcpy walks straight over miBufferWritePos / miLength / miFirstEventOffset, i.e. an
    // overflow here is a MEMORY-CORRUPTING WRITE, not a dropped event. The semantics are
    // console-faithful and are deliberately NOT changed; what changes is that the approach to the
    // cliff becomes visible instead of silent.
    //
    // The budget it watches (S4, measured, console record sizes): wave B's own worst plausible
    // frame -- a mode ending while the next one starts -- is ~7.1 KB of 13312 (53%), of which the
    // two reachable action-23 posts alone are 4,576 B (34%). Today the host posts action 23 at
    // 1792 B, not 2272 (the BrnGameModeParams.h hole), so the live figure reads ~6.1 KB and HIDES
    // the real headroom; when that hole closes the 4,576 comes back. The remaining ~6.2 KB is
    // shared with every other GameState producer in the same frame (StuntManager, ChallengeManager,
    // CarSelectManager, RoadRules, GuiEvents) and cannot be bounded statically. Hence: measure.
    //
    // WHY HERE AND NOT IN StartGameMode (the H10 rung's home -- deviation, stated openly):
    // StartGameMode has no GameActionQueue. Its only route to one is the member mpGameActionQueue,
    // which is published at the head of THIS function and cleared four lines below -- and
    // StartGameMode is driven from ProcessGameEvents, outside the pre-world leg, so that member is
    // NULL there and the rung would print nothing. This spot is also the strictly better
    // measurement point: it is the last instruction of the frame's GameState posting, so what it
    // samples IS the frame's peak rather than a mid-frame sample. It prints meCurrentGameModeType
    // so its lines pair one-for-one with the StartGameMode rung H10 asks for.
    //
    // GetSizeInBytes() is the PUBLIC accessor for exactly the quantity wanted:
    // `return miBufferWritePos - miFirstEventOffset` (CgsVariableEventQueue.h:653-665), and
    // miFirstEventOffset is a fixed sub-ALIGN constant set once by Construct. No protected member
    // is touched. Reads only; cannot change queue state.
    //
    // Guarded by BRN_MODEMGR_DIAG. Prints only on a NEW high-water mark, so a steady frame is
    // silent and the log is one line per genuine escalation; the 50% and 75% lines are the two
    // thresholds worth stopping at (75% of 13312 == 9984, i.e. one action-23 pair from the edge).
    {
        static const bool sbQueueHwm = (getenv("BRN_MODEMGR_DIAG") != 0);
        if (sbQueueHwm && lpGameActionQueue != NULL && CgsDev::Log::gpDebugPrint != 0)
        {
            static s32 siPeakBytes = 0;
            const s32  liUsedBytes = lpGameActionQueue->GetSizeInBytes();
            if (liUsedBytes > siPeakBytes)
            {
                siPeakBytes = liUsedBytes;
                *CgsDev::Log::gpDebugPrint
                    << "[queue-hwm] GameActionQueue peak=" << siPeakBytes
                    << " of 13312 (" << ((siPeakBytes * 100) / 13312) << "%) mode="
                    << static_cast<s32>(meCurrentGameModeType)
                    << " events=" << lpGameActionQueue->GetLength() << "\n";
            }
        }
    }

    // `stw r27(=0), 0x6D68(r31)` -- the queue pointer never outlives the tick.
    mpGameActionQueue = NULL;

    // `lfs 0x6DC0 * lfs 0x6DBC` compared against flt_82005574 -- IMAGE-CITED: image.bin offset
    // 0x5574 reads 3C A3 D7 0A == 0.019999999552965164f, i.e. a 0.02 s (50 Hz) sim step. That is
    // exactly CgsSystem::TimerStatusInterface::IsSimTimerFrequency50Hz (X360 0x8230E990), de-inlined.
    // [!] NAME DIVERGENCE, reported not fixed: the committed
    // ScoringSystemDebugComponent::DebugRenderChainableStunts spells this fourth argument
    // `bool lbThirtyFps`; the console value is "the SIM timer is running at 50 Hz".
    const bool lbSimTimerAt50Hz = mTimerStatusInterface.IsSimTimerFrequency50Hz();
    (void)lbSimTimerAt50Hz;

    // [!] [stuntrace] PARKED (header) -- header_request #8. Console 0x82353F58..0x82353FFC:
    //     CGS_ASSERT(lpActiveRaceCarOutput,   "lpActiveCarInterface");     // BrnScoringSystemDebugComponent.cpp:135
    //     CGS_ASSERT(lpPlayerStatusInterface, "lpPlayerStatusInterface");  // ...:136
    //     mScoringSystemDebugComponent.DebugRenderChainableStunts(
    //         lpActiveRaceCarOutput,
    //         lpPreWorldInputBuffer->GetPlayerStatusInterface(),
    //         *(lpPreWorldInputBuffer->GetNetworkToGameStateInterface() + 0x2434),
    //         lbSimTimerAt50Hz);
    // NOT REPRODUCED: the third argument is the same unnamed NetworkToGameStateInterface+0x2434
    // word the online stunt arm needs, and this tree has no accessor for it. Developer HUD overlay
    // only (the grouping sheet already classes this call verify-or-park).

    CgsDev::PerfMonCpu::StopMonitor(miPreWorldUpdatePM);
}

// ==============================================================================================
// ModeManager::PostWorldUpdate -- X360 0x8234A9E0
// ==============================================================================================
// THE SCORER DRIVER. The console signature is THREE arguments:
//     PostWorldUpdate(const PostWorldInputBuffer* lpInput,
//                     const InputBuffer::TakedownEventQueue* lpTakedownQueue,
//                     f32 lfDelta)                          [DWARF BrnModeManager.h:279]
// (r3 = this, r4 = lpInput, r5 = lpTakedownQueue, f1 = lfDelta.)
// The frozen header carries only TWO -- its own banner says the takedown-queue typedef does not
// exist yet and files it as a header_request; that is header_request #4 below, and it is what
// parks the UpdateTakedowns / UpdateCrashes pair and the console's second assert.
//
// Interface accessors used below, each pinned by its return offset AND its "Not locked for
// reading" assert line in BrnGameStateModuleIO.h (the X360 line numbers match the DWARF ones
// exactly, which is how the four truncated IDA names were resolved):
//     0x8231D170 line 198 -> +0x0010  GetRaceCarCrashEventQueue()
//     0x8231D218 line 201 -> +0x0220  GetVehicleOutputInterface()
//     0x8231D2C0 line 210 -> +0x7250  GetActiveRaceCarOutputInterface()
//     0x8231D368 line 213 -> +0x9B40  GetGlobalRaceCarOutputInterface()
//     0x8231D410 line 216 -> +0xAAC0  GetAICarOutputInterface()
// ==============================================================================================
void
ModeManager::PostWorldUpdate(const GameStateModuleIO::PostWorldInputBuffer* lpPostWorldInputBuffer,
                             f32                                            lfDelta)
{
    CgsDev::PerfMonCpu::StartMonitor(miPostWorldUpdatePM);

    CGS_ASSERT(lpPostWorldInputBuffer != NULL, "lpInput != NULL");
    // The console's second assert (BrnModeManager.cpp:770) cannot be reproduced without the
    // parameter:  CGS_ASSERT(lpTakedownQueue != NULL, "lpTakedownQueue != NULL");
    // Restore it together with header_request #4.

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarOutput =
        lpPostWorldInputBuffer->GetActiveRaceCarOutputInterface();

    if (mpCurrentGameMode != NULL)
    {
        // The scratch stunt-score record the online stunt scorer fills and ChallengeManager reads.
        // Cleared unconditionally; on the offline path it stays cleared and is simply unused.
        GameStateModuleIO::StuntScoreInfo lStuntScoreInfo;
        lStuntScoreInfo.Clear();

        CheckForOutOfRangeCarsReachingFinish(lpPostWorldInputBuffer);

        // [x] UN-PARKED 2026-08-26 (fix round) -- the 26-slot BrnGameMode.h has landed, so
        // header_request #5's blocker for this hook is discharged. Console 0x8234AAB0..0x8234AAC4:
        //     lwz r3, 0xD98(r31)        ; mpCurrentGameMode
        //     mr  r4, r20               ; lpPostWorldInputBuffer (the same r20 handed to
        //                               ;   CheckForOutOfRangeCarsReachingFinish two lines above)
        //     lwz r11, 0(r3) / lwz r11, 0xC(r11) / mtctr / bctrl
        // vtbl+0x0C == slot 3 == `virtual void PostWorldUpdate(const PostWorldInputBuffer*)`.
        // Behaviour restored: every concrete mode's own post-world tick.
        mpCurrentGameMode->PostWorldUpdate(lpPostWorldInputBuffer);   // vtbl+12 == slot 3

        // The player-scoring-slot -> active-race-car binding sweep. The console reads the
        // interface's slot table inline at `iface + 0x2838` and fires the interface's own range
        // assert; reached here through the named accessor instead (hazards H9: never a raw offset
        // into a foreign type).
        for (s32 liSlot = 0; liSlot < GameStateModuleIO::E_PLAYER_SCORING_INDEX_COUNT; ++liSlot)
        {
            const GameStateModuleIO::EPlayerScoringIndex lePlayerScoringIndex =
                static_cast<GameStateModuleIO::EPlayerScoringIndex>(liSlot);

            const ::EActiveRaceCarIndex leActiveRaceCarIndex =
                lpActiveRaceCarOutput->GetActiveRaceCarIndex(lePlayerScoringIndex);

            // `cmpwi r28, 8` -- E_ACTIVE_RACE_CAR_INDEX_COUNT is the interface's "empty slot" value
            // here, NOT the _INVALID (-1) one. The two are distinct on this path.
            if (leActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_COUNT)
            {
                continue;
            }
            if (!mScoringSystem.IsPlayerInScoringSystem(lePlayerScoringIndex))
            {
                continue;
            }

            // `lwz r11, 0x144(r3)` on the CarData -- its stored active-race-car index. -1 means the
            // slot has never been bound, and that is the ONLY case that raises the spawn hook.
            CarData* lpCarData =
                mScoringSystem.GetCarDataFromPlayerScoringIndex(lePlayerScoringIndex);
            const bool lbFirstBind =
                (lpCarData->GetActiveRaceCarIndex() == E_ACTIVE_RACE_CAR_INDEX_INVALID);

            mScoringSystem.SetPlayerRaceCarIndex(lePlayerScoringIndex, leActiveRaceCarIndex);

            if (lbFirstBind)
            {
                // [x] UN-PARKED 2026-08-26 (fix round) -- the 26-slot BrnGameMode.h has landed.
                // Console 0x8234AB60..0x8234AB74:
                //     lwz r3, 0xD98(r31)        ; mpCurrentGameMode
                //     mr  r4, r28               ; r28 == `lwz r28, 0(r27)` @0x8234AB10, the slot
                //                               ;   table's ACTIVE race-car index -- the same r28
                //                               ;   SetPlayerRaceCarIndex just took as its r5
                //     lwz r11, 0(r3) / lwz r11, 0x4C(r11) / mtctr / bctrl
                // vtbl+0x4C == slot 19 == `virtual void PlayerHasSpawned(EActiveRaceCarIndex)`.
                // Note the console reaches it ONLY down the first-bind arm (`bne -> loc_8234AB7C`
                // at 0x8234AB58 takes the already-bound path to a bare SetPlayerRaceCarIndex and
                // skips the hook), which is exactly this `if (lbFirstBind)`.
                // Behaviour restored: a mode is told that one of its cars took the grid.
                mpCurrentGameMode->PlayerHasSpawned(leActiveRaceCarIndex);   // vtbl+76 == slot 19
            }
        }
        // NOT REPRODUCED, and deliberately not faked: the console's per-iteration loop guard is the
        // INLINED EPlayerScoringIndex operator++ range assert
        //     CGS_ASSERT(leEnumIndex <= E_PLAYER_SCORING_INDEX_COUNT,
        //                "leEnumIndex <= E_PLAYER_SCORING_INDEX_COUNT");   // BrnGameStateSharedEnums.h:155
        // This tree has no operator++ for EPlayerScoringIndex -- BurnoutConstants.h grows only the
        // EActiveRaceCarIndex / EGlobalRaceCarIndex ones -- so the loop uses a plain s32 counter.
        // header_request #9 adds the operator; writing the assert here over a plain counter would
        // make it a tautology, which is worse than absent.

        mScoringSystem.UpdateTeamStats(lfDelta);

        // `ldx` on mCurrentGameModeParams+0x860 (this+0x8BE0), then `rlwinm r11,r11,0,9,9` -- bit 9
        // of the low word == 1 << 22 == 0x400000 == KU_FLAG_DISABLE_ALL_TDS. (The console spells the
        // outer test as `!mpCurrentGameMode || !flag`, but this whole block already sits inside the
        // mpCurrentGameMode gate, so only the flag survives.)
        if (!mCurrentGameModeParams.GetFlag(GameModeParams::KU_FLAG_DISABLE_ALL_TDS) &&
            IsGameModeInProgress(mpCurrentGameMode))
        {
            // [!] [stuntrace] PARKED (header) -- header_requests #4 and #6. Console 0x8234AC40:
            //   ScoringSystem::UpdateTakedowns(&mScoringSystem, lpTakedownQueue,
            //                                  mePlayerActiveRaceCarIndex, mbStuntChallengeActive);
            //   ScoringSystem::UpdateCrashes(&mScoringSystem,
            //                                lpPostWorldInputBuffer->GetRaceCarCrashEventQueue());
            // The takedown queue is the missing third ModeManager parameter (#4). Independently, the
            // committed ScoringSystem::UpdateTakedowns takes ONE argument where the console passes
            // THREE -- the asm is unambiguous (`lwzx r5, r31, 0x8038` = mePlayerActiveRaceCarIndex,
            // `lbzx r6, r31, 0x950D` = mbStuntChallengeActive) -- so calling the one-argument form
            // would SILENTLY DROP two live arguments, which is worse than parking (#6). UpdateCrashes
            // is parked with it because the committed declaration wants
            // VehicleManagerOutputInterface::RaceCarCrashEventQueue while the buffer hands out
            // GameStateModuleIO::RaceCarCrashEventQueue (#6).
            // Behaviour lost: takedown + crash scoring. A stunt race scores neither.
        }

        mScoringSystem.UpdateDistanceToPlayer(lpActiveRaceCarOutput);
        mScoringSystem.StoreCarIds(lpActiveRaceCarOutput);

        // The THIRD argument is (mpCurrentGameMode && state == E_GMS_IN_PROGRESS), computed at
        // 0x8234AC80..0x8234ACA0 and passed in r6 (the PPC float-argument GPR skip puts the bool in
        // r6, not r5). The committed declaration names that parameter `bool lbOnline`, which the
        // call site REFUTES -- filed as a RENAME-ONLY header_request (#10), not a signature change.
        // IDA's pseudocode drops the argument entirely; the asm is unambiguous.
        mScoringSystem.UpdateGeneralStats(lpActiveRaceCarOutput, lfDelta,
                                         IsGameModeInProgress(mpCurrentGameMode));
        mScoringSystem.UpdateNumberOfCarsInMode(lpActiveRaceCarOutput);

        // `rlwinm r11,r11,0,18,18` -- bit 18 == 1 << 13 == 0x2000 == KU_FLAG_HAS_ROUTE.
        if (mCurrentGameModeParams.GetFlag(GameModeParams::KU_FLAG_HAS_ROUTE))
        {
            // [!] [stuntrace] PARKED (header) -- header_request #11. Console 0x8234AD18:
            //   ScoringSystem::UpdateRacePositions(&mScoringSystem,
            //       lpActiveRaceCarOutput,
            //       lpPostWorldInputBuffer->GetGlobalRaceCarOutputInterface(),
            //       lpPostWorldInputBuffer->GetAICarOutputInterface(),
            //       this);
            // PostWorldInputBuffer::GetGlobalRaceCarOutputInterface() (X360 0x8231D368, +0x9B40,
            // read-lock line 213) is NOT declared on this tree's PostWorldInputBuffer -- only the
            // ACTIVE and the AI ones are. The routed-event position sort is a race / burning-route
            // mechanic; a stunt run has KU_FLAG_HAS_ROUTE clear and never enters this arm.

            // The wrong-way detector needs only the active interface, so it is NOT parked.
            mScoringSystem.DetectPlayerDrivingWrongWay(lpActiveRaceCarOutput, lfDelta);
        }

        // -------- the per-mode scorer fork (0x8234AD2C..0x8234AEC8) --------
        if (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
            meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            // [!] [stuntrace] PARKED (header) -- header_request #12. Console 0x8234AD64:
            //   CrashModeScoring::Update(&mScoringSystem's mCrashModeScoring,   // this+0xDD0
            //       lpActiveRaceCarOutput,
            //       lpPostWorldInputBuffer->GetVehicleOutputInterface()->GetTrafficStateQueue(),
            //       lfDelta);
            // The +9760 the console adds to the vehicle interface IS
            // VehicleOutputInterface::mTrafficStateQueue @0x2620 (BrnVehicleOutputInterface.h:178)
            // -- but GameStateModuleIO's `class VehicleOutputInterface` (BrnGameStateModuleIO.h:223)
            // is a DIFFERENT, still-incomplete forward declaration over opaque storage, so the queue
            // cannot be reached by name. Crash mode only.
        }
        else if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE)
        {
            // The whole console arm is one assert -- BrnRoadRageModeScoring.cpp:91, verbatim.
            CGS_ASSERT(lpActiveRaceCarOutput != NULL, "lpActiveRaceCarInterface != NULL");
        }
        else if (meCurrentGameModeType == GameStateModuleIO::E_MODE_STUNT_ATTACK &&
                 IsGameModeInProgress(mpCurrentGameMode))
        {
            // >>> THE OFFLINE STUNT-RACE SCORER DRIVER <<<
            // this+0x1100 == &mScoringSystem + 0x350 == the offline StuntModeScoring.
            mScoringSystem.GetStuntScorer()->Update(lpActiveRaceCarOutput, lfDelta);
        }
        else
        {
            const bool lbOnlineStuntFamily =
                (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE)  ||
                (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN) ||
                (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END);
            if ((lbOnlineStuntFamily || mbStuntChallengeActive) &&
                IsGameModeInProgress(mpCurrentGameMode))
            {
                // [!] [stuntrace] ONLINE ARM DEFERRED. Console 0x8234AE54..0x8234AEC4:
                //   StuntModeScoringOnline::Update(&mScoringSystem + 0x2620, lpActiveRaceCarOutput,
                //                                  &lStuntScoreInfo, lfDelta);
                //   if (mbStuntChallengeActive) {
                //       CGS_ASSERT(lpStuntModeScoring, "lpStuntModeScoring");   // BrnModeManager.cpp:869
                //       if ((*(*onlineScorer + 24))(onlineScorer, <32-byte scratch>)) <local> = 1;
                //   }
                // NOT REPRODUCED: BrnStuntModeScoringOnline's Update override and the slot-6 query
                // behind it belong to the online stunt-run wave, and the local flag the console sets
                // is only ever read by the parked HUDMessageLogic call in the tail.
            }
        }

        // [X][X] [stuntrace] DIVERGENCE: ChallengeManager NOT embedded/mounted. Console 0x8234AEE8:
        //   ChallengeManager::PostWorldUpdate(this + 28160,
        //       lpPostWorldInputBuffer->GetRaceCarCrashEventQueue(),
        //       &lStuntScoreInfo,
        //       &mTimerStatusInterface);
        // Behaviour lost: freeburn challenges never see the frame's stunt score.

        if (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY)
        {
            // [!] [stuntrace] ONLINE ARM DEFERRED -- the lobby BurnoutSkillz tick. Console
            // 0x8234AEEC..0x8234AF94, with its two verbatim asserts
            //   "lpFreeburnLobbyMode" (BrnModeManager.cpp:887) and
            //   "mpGameStateModule"   (BrnModeManager.cpp:5700):
            //   BurnoutSkillzManager::SetNewSkillIfGreater(mode + 184, 10,
            //       <lookup(*(mode + 184 + 112), playerActiveIndex)>, playerActiveIndex,
            //       (f32)<a word of lStuntScoreInfo>);
            // The embedded BurnoutSkillzManager region and its per-player lookup belong to the
            // online free-burn lobby, and the Skillz TU carries the known GameActionQueue typedef
            // clash that hazards H7 puts out of scope.
        }

        (void)lStuntScoreInfo;
    }

    // -------- the tail, OUTSIDE the mpCurrentGameMode gate --------

    // 0x8234AF98: DetectPlayerStationary runs when the mode is in progress OR when there is no mode
    // at all (free roam), and never while the mode is prepared-but-not-started (+0x9503).
    {
        const bool lbInProgressOrNoMode =
            (mpCurrentGameMode == NULL) || IsGameModeInProgress(mpCurrentGameMode);
        if (lbInProgressOrNoMode && !mbIsModePrepared)
        {
            mScoringSystem.DetectPlayerStationary(lpActiveRaceCarOutput, lfDelta);
        }
    }

    if ((meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE)  ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN) ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END))
    {
        // [!] [stuntrace] ONLINE ARM DEFERRED. Console 0x8234B024..0x8234B07C:
        //   score = (s32)<onlineStuntScorer f32 @ +0x20> * <s32 @ +0x24> + <s32 @ +0x10>;
        //           (this+0x33F0 / +0x33F4 / +0x33E0 -- all inside the ss+0x2620 online scorer)
        //   CGS_ASSERT(lpActiveRaceCarOutput->GetPlayerActiveRaceCarIndex() != -1,
        //              "Player car index hasn't been set");  // BrnRaceCarEntityModuleOutputInterface.h:980
        //   ScoringSystem::SetPlayerStuntScore(&mScoringSystem,
        //       lpActiveRaceCarOutput->GetPlayerActiveRaceCarIndex(), score);
        // NOT REPRODUCED: those three words are StuntModeScoringOnline internals with no named
        // accessors, and reaching them by raw offset off `this` is exactly what hazards H9 forbids.
        // Offline stunt races (mode 7) never enter this arm -- their score goes through
        // StuntModeScoring::Update above.
    }

    // [!] [stuntrace] STILL PARKED HERE -- but the leg is LIVE, in the extracted post-world leg.
    // Console 0x8234B0E8 -- eight register arguments plus f1 plus TWO stack ones (IDA's pseudocode
    // shows only the register set):
    //   HUDMessageLogic::PostWorldUpdate(&mHUDMessageLogic, lpActiveRaceCarOutput,
    //       meCurrentGameModeType, this, &mScoringSystem,
    //       lpPostWorldInputBuffer->GetRaceCarCrashEventQueue(),
    //       lpPostWorldInputBuffer->GetVehicleOutputInterface(),
    //       lpTakedownQueue, lfDelta,
    //       [sp+0x5C] mePlayerActiveRaceCarIndex, [sp+0x67] IsGameModeInProgress(mpCurrentGameMode));
    //
    // ⓘ 2026-08-27 (stunt-scorer latch-drain fix): HUDMessageLogic::PostWorldUpdate is now bodied
    // (reduced argument set) and CALLED -- but from GameStateModule::PostWorldUpdateStuntBringUp's
    // LEG 4, not from here, because THIS function has no call site on this build (see the TU
    // banner: nothing creates a PostWorldInputBuffer). It is the ONLY consumer of the stunt
    // scorer's mbRecentStunt latch; leaving it unreached is what made
    // StuntModeScoring::UpdateBufferedScore's CGS_ASSERT(!mbRecentStunt) fire mid-run.
    // DELETE-WHEN this function becomes the live post-world caller again: the extracted leg then
    // folds back to `mHUDMessageLogic.PostWorldUpdate(...)` right here, with the full argument set.

    // `lbzx 0x94F5 -> stbx 0x94F6`: the mode-start-region edge detector's one-frame history. This
    // is the ONLY writer of mbLastInModeStartRegion, and it runs every post-world tick, mode or not.
    mbLastInModeStartRegion = mbInModeStartRegion;

    if (mpCurrentGameMode != NULL)
    {
        // [x] UN-PARKED 2026-08-26 (fix round) -- the 26-slot BrnGameMode.h has landed and declares
        // slot 10. Console 0x8234B110..0x8234B130:
        //     mr  r3, r20 / bl PostWorldInputBuffer::Get...   ; the AI-car output interface
        //     lbz r11, 0x14E4(r3) / cmplwi 0 / beq -> skip    ; mbPlayerIsInShortcut @ +5348, which
        //                                                     ;   is BrnAICarOutputInterface.h:95's
        //                                                     ;   IsPlayerInShortcut() / :107 member
        //     lwz r3, 0xD98(r31) / lwz r11, 0(r3) / lwz r11, 0x28(r11) / mtctr / bctrl
        // vtbl+0x28 == slot 10 == `virtual void OnPlayerInShortCut()`, no arguments.
        // The whole thing sits inside the `mpCurrentGameMode != NULL` test at 0x8234B100/0x8234B10C,
        // which is this enclosing `if`. Behaviour restored: a mode is told the player cut the course
        // (base body is the folded empty `blr`; RoadRage 0x823160A0 and Survivor 0x82316398 override).
        if (lpPostWorldInputBuffer->GetAICarOutputInterface()->IsPlayerInShortcut())
        {
            mpCurrentGameMode->OnPlayerInShortCut();   // vtbl+40 == slot 10
        }
    }

    ProcessPlayerCrashes(lpPostWorldInputBuffer);

    CgsDev::PerfMonCpu::StopMonitor(miPostWorldUpdatePM);
}

}
