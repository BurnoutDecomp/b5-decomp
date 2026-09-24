// =================================================================================================
// GameStateModule_Showtime.cpp
//
// The SHOWTIME (crash-mode) start leg. Started 2026-08-27 (showtime S7b-a wave) with StartCrashMode
// alone plus a harness stand-in; CLOSED the same day (S7b-b) by landing the two console gates that
// stand above it, and the harness leg was deleted with them.
//
// ⭐⭐⭐ WHY S7b-b EXISTS: A PLAYER REPORTED SHOWTIME DOES NOT WORK ON THE PUBLISHED BUILD.
// "i don't think showtime works ... unless you have to activate it in a crash, but just driving
// around the buttons to activate it do nothing." They were right, and the reason was structural:
// what shipped in place of the two missing upper links was HarnessInjectShowtimeBringUp, gated on
// getenv("BRN_START_SHOWTIME"), which the published build does not set. Holding both bumpers
// reached a one-shot that was switched off. The fix is not a wider hook -- it is the console's own
// gate stack, which is what this file now carries.
//
// THE CONSOLE CHAIN, COMPLETE:
//     DetectModeStarts @0x8239A428 (the `else` arm, 0x8239A568..0x8239A8EC)  <-- GameStateModule_gSR_00.cpp
//       -> ShouldStartShowtimeMode @0x82356B18 (166 insns)                   <-- HERE
//         -> StartCrashMode @0x8236B580 (80 insns)                           <-- HERE
//           -> StartGameModeParams::Construct @0x8231C1F8   (mounted)
//           -> ModeManager::StartGameMode      @0x8234FCE8   (mounted)
// plus the two showtime-intro accessors the arm's latch pair backs:
//     IsInShowtimeIntro        @0x82356A60 (11 insns)
//     GetShowtimeIntroSteering @0x82356A90 (33 insns)
//
// ⭐⭐ THE GESTURE WAS ALWAYS REAL AND IS UNCHANGED. `ControllerInput::mbCrashModePressed` (+0x42)
// is the console's own both-bumpers-held byte, written every frame by BrnGameStateModuleIO.cpp:92
// from action rows 54 and 55, which are bound to LSHOULDER/RSHOULDER (and to the keyboard) in
// CgsInputPadsPC.cpp's KA_BINDINGS. Nothing in this wave touched the input path; what was missing
// was everything ABOVE it.
// =================================================================================================

#include <cstdlib>                                                          // getenv (the [FLAG PC bring-up] progression-gate switch)

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                      // EGameModeType, EGameModeState, IsOnlineFreeBurnLobby
#include "GameSource/GameState/BrnGameStateModuleIO.h"                      // PreWorldInputBuffer / OutputBuffer / ControllerInput
#include "GameSource/GameState/BrnGameStateTypes.h"                         // EShowtimeBehaviour
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"         // GameMode::IsOnline / GetCurrentState
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"   // StartGameModeParams
#include "GameSource/GameState/Progression/BrnProgressionManager.h"         // AreRoadRulesAvailable
#include "GameSource/GameState/Progression/BrnProfile.h"                    // GetMedalCountFromTheStart (the refusal diagnostic)
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"             // GetJunkyardId
#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h"   // CgsSystem::TimerRequests
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
// [FX-SHOWTIME2 2026-09-24] UpdateShowtimeMode's action records, its scorer and its answer queue.
#include "GameSource/GameState/BrnGameActions.h"                            // Toggle/TrafficTypeRequest/VehicleHit actions
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystem.h"      // ScoringSystem::GetCrashScorer
#include "GameSource/GameState/ModeManager/Scoring/BrnCrashModeScoringRecentCrash.h" // CrashModeScoring
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h" // TrafficTypeResponse
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                // CgsModule::BaseEventQueue

namespace BrnGameState
{
namespace
{
    // [FX-SHOWTIME2 2026-09-24] The console's two file-scope constants for the showtime traffic
    // hand-off, DWARF BrnGameStateModule.cpp:154/:155 (the console's own TU; this file is a split
    // of it). Both are immediates in UpdateShowtimeMode: `ori r27, r10, 0xFFFF` @0x82380F98 /
    // `cmplwi cr6, r11, 0xFFFF` @0x82380F88, and `li r11, 2` @0x82381124.
    const u16 K_INVALID_VEHICLE_INDEX             = 65535;
    const s32 KI_SHOWTIME_TRAFFIC_RESPONSE_FRAMES = 2;

    // [DIAG] NOT IN THE X360 BINARY. The UpdateShowtimeMode witness, under the same opt-in the
    // ProcessContacts half uses (BRN_SHOWTIME_WATCH). One line per pop / answer / miss / toggle,
    // capped, so a run can COUNT the hand-off's hops: ProcessContacts' `[showtime-crash]` pushes
    // -> `pop` (action 116) -> `answer` (DealWithScoreForVehicleClass + action 140) or `MISS`.
    bool ShowtimeScoreWitness()
    {
        static const bool sbWatch = (getenv("BRN_SHOWTIME_WATCH") != 0);
        static s32        siLinesLeft = 160;
        if (!sbWatch || CgsDev::Log::gpDebugPrint == 0 || siLinesLeft <= 0)
        {
            return false;
        }
        --siLinesLeft;
        return true;
    }
    // [DIAG] NOT IN THE X360 BINARY. Names the gate that refused a both-bumpers press. It exists
    // because the defect this file closes was reported by a player as "the buttons do nothing",
    // and a gate stack with ten terms has ten ways to look exactly like a dead button.
    // Deliberately NOT env-gated (see the call sites); bounded to one line PER DISTINCT REASON.
    void LogShowtimeRefusal(const char* lpcReason)
    {
        // ⚠️ ONCE PER **REASON**, NOT ONCE PER PROCESS -- and the difference is the whole point.
        // A plain one-shot reports the gate that happened to be down on the FIRST frame the
        // gesture was seen, and then goes quiet for ever. Measured this session: that first frame
        // is the junkyard-exit frame, where a term that clears seconds later still reads as down,
        // so the run's only line named a transient. Every distinct reason now gets exactly one
        // line, so the log shows the gate stack PEELING rather than a single snapshot -- and the
        // volume is still bounded, because there are only ten reasons and they are literals.
        // [[diagnostics-that-lie]] -- ask what the probe cannot see, then make it see that.
        static const char* spcLastReason = 0;
        if (spcLastReason != lpcReason && CgsDev::Log::gpDebugPrint != 0)
        {
            spcLastReason = lpcReason;
            *CgsDev::Log::gpDebugPrint
                << "[showtime] BOTH BUMPERS held, but ShouldStartShowtimeMode @0x82356B18 refused: "
                << lpcReason << "\n";
        }
    }
}

    // =============================================================================================
    // GameStateModule::StartCrashMode  @0x8236B580  (80 insns)
    //   source BrnGameStateModule.cpp:5579/5580/5582 (the three baked assert line numbers)
    //
    // Build a StartGameModeParams at the player's current position for the SHOWTIME mode -- offline
    // (2) or online (16) -- and hand it to ModeManager::StartGameMode. That is the whole function.
    //
    // ASM SPINE (0x8236B580..0x8236B6BC), in order:
    //   0x8236B594  the `li r11,0xF / stw -1 / addi r10,0x2C` do-while over var_338 -- SIXTEEN
    //               44-byte CheckpointData slots seeded to the CgsArray -1 sentinel, plus one more
    //               -1 at var_A0. That is the INLINED default construction of the local
    //               StartGameModeParams' embedded Array<CheckpointData,16>; on the host the
    //               declaration below runs the same constructors for free. Not transcribed.
    //   0x8236B5BC  assert lpInput  != NULL   (:5579)
    //   0x8236B5E4  assert lpOutput != NULL   (:5580)
    //   0x8236B60C  assert !IsOnlineGameMode() || mModeManager.IsOnlineFreeBurnLobby()   (:5582)
    //               -- the second half is the `*(this+7604) == 15 || == 16` pair, i.e.
    //               GameStateModuleIO::IsOnlineFreeBurnLobby(mModeManager.GetCurrentGameModeType()),
    //               which is already a committed free predicate over exactly those two enumerators.
    //   0x8236B664  IsOnlineGameMode() again (the console calls it twice; not CSE'd)
    //   0x8236B66C  `addis r4,r30,4 ; addi r4,r4,-0x6820` == this + 235488 ==
    //               mLastActiveRaceCarInterface, then sub_823102F0 == GetPlayerPosition
    //   0x8236B684  li r4, 0x10  (online)   /   0x8236B690  li r4, 2  (offline)
    //   0x8236B698  li r5, 0                 == E_GAMEMODESTARTMECHANISM_DEFAULT
    //   0x8236B6A0  lvx128 v1, r0, r11       == the returned position, into Construct's Vector3 arg
    //   0x8236B6A4  StartGameModeParams::Construct(&params, mode, mechanism, position)
    //   0x8236B6B4  ModeManager::StartGameMode(this + 0x1020 /* 4128 */, lpOutput, &params)
    //
    // ⚠️ lpInput IS ASSERTED AND THEN NEVER USED. After the null test at 0x8236B5BC, r4 is
    // immediately reloaded (0x8236B66C) with the race-car interface address and the incoming
    // pointer is gone. The parameter is kept because the console's signature keeps it and because
    // the assert is a real behaviour; it is (void)-cast rather than deleted.
    // [[invented-arms-and-the-c4715-ratchet]] -- do not "clean this up" by dropping the argument.
    //
    // ⚠️ THE THREE ASSERTS ARE THE CONSOLE'S, INCLUDING THE ONLINE ONE, and none of them returns.
    // The console fires the assert and carries straight on into the start. Reproduced with
    // CGS_ASSERT, which has the same non-returning shape here.
    // =============================================================================================
    void GameStateModule::StartCrashMode(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                         GameStateModuleIO::OutputBuffer*              lpOutput)
    {
        // The local. Its embedded Array<CheckpointData,16> is default-constructed by this
        // declaration -- that IS the asm's -1-sentinel do-while at 0x8236B594.
        StartGameModeParams lStartGameModeParams;

        CGS_ASSERT(lpInput  != 0, "lpInput != NULL");    // :5579
        CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");   // :5580
        CGS_ASSERT(!IsOnlineGameMode() ||
                       GameStateModuleIO::IsOnlineFreeBurnLobby(
                           mModeManager.GetCurrentGameModeType()),
                   "!IsOnlineGameMode() || mModeManager.IsOnlineFreeBurnLobby()");   // :5582

        (void)lpInput;   // asserted above and then never read -- see the banner

        // [GUARD] Not X360. The console dereferences lpOutput unconditionally after its assert;
        // on a build whose asserts only log, that is an immediate null deref. The console's own
        // precondition is the assert, so honouring it is not a behaviour change on any input the
        // assert accepts.
        if (lpOutput == 0)
        {
            return;
        }

        const GameStateModuleIO::EGameModeType leShowtimeMode =
            IsOnlineGameMode() ? GameStateModuleIO::E_MODE_ONLINE_SHOWTIME     // li r4, 0x10
                               : GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME;   // li r4, 2

        const Vector3 lPlayerPosition = mLastActiveRaceCarInterface.GetPlayerPosition();

        lStartGameModeParams.Construct(leShowtimeMode,
                                       lPlayerPosition,
                                       E_GAMEMODESTARTMECHANISM_DEFAULT);      // li r5, 0

        mModeManager.StartGameMode(lpOutput, &lStartGameModeParams);
    }


    // =============================================================================================
    // GameStateModule::ShouldStartShowtimeMode  @0x82356B18  (166 insns)
    //
    // ⭐⭐⭐ THE GATE STACK. Called twice per frame by the DetectModeStarts else arm and returns
    // true on the ONE frame the crash-start hold expires with every condition satisfied. Every
    // refusal below the timer arms re-arms that hold, so a gesture that is interrupted starts over.
    //
    // ARGUMENTS -- and the phantom-parameter trap, resolved two ways.
    //   Hex-Rays renders this `(int a1, double a2, int a3, char a4, unsigned int* a5)`: FIVE
    //   arguments. It is THREE. The f32 rides f1 and consumes the r4 GPR slot, so r4 is never read
    //   in the body; the bool is r5 (`clrlwi r11, r5, 24` @0x82356C18) and the pointer is r6
    //   (`lwz r11, 0(r6)` @0x82356BF0). DecFIGS agrees independently -- BrnGameStateModule.h:781
    //   spells `bool ShouldStartShowtimeMode(float32_t, bool, TimerRequests*)`. Rung 1 and rung 2
    //   give the same three arguments in the same order, which is why this is stated rather than
    //   flagged. [[invented-arms-and-the-c4715-ratchet]] -- do not "restore" a fourth argument.
    //
    // ASM SPINE (0x82356B18..0x82356DAC), in the console's own order:
    //   0x82356B38  mModeManager.mpCurrentGameMode ? GameMode::mbIsOnline (+0xAC) : 0
    //   0x82356B5C  when NOT online: the inlined ProgressionManager::AreRoadRulesAvailable()
    //               (`>= 4u` on Profile+42512 || pm+133456 || pm+133460) -- refuse if false
    //   0x82356BB4  mfTimeSinceLastCrashMode > 0 -> decrement, re-arm the hold, refuse
    //   0x82356BF0  any of the SIM TimerRequests bits 0/1/2 set -> refuse (WITHOUT re-arming)
    //   0x82356C18  the and-chain, every failure branching to the shared re-arm tail loc_82356D7C
    //   0x82356D50  hold -= dt ; return hold <= 0.0f
    //
    // ⚠️ THE SIM-TIMER REFUSAL IS THE ONE ARM THAT DOES **NOT** RE-ARM THE HOLD (@0x82356BFC /
    // 0x82356C08 / 0x82356C14 all jump to loc_82356D90 == `li r3,0 ; blr`, not to loc_82356D7C).
    // That asymmetry is the console's and it is deliberate: a frame in which something else has
    // already asked the sim timer to start/stop/retime is skipped, not treated as a broken hold.
    // =============================================================================================
    bool GameStateModule::ShouldStartShowtimeMode(f32                       lfGameTimestep,
                                                  bool                      lbCrashModePressed,
                                                  CgsSystem::TimerRequests* lpSimTimerRequests)
    {
        // The console's own re-arm literal: flt_82029F24, image-read 0x3C23D70A == 0.0099999998f.
        const f32 KF_CRASH_START_HOLD_SECONDS = 0.0099999998f;

        const GameMode* const lpCurrentGameMode = mModeManager.GetCurrentGameMode();
        const bool lbOnlineModeRunning =
            (lpCurrentGameMode != 0) && lpCurrentGameMode->IsOnline();

        // @0x82356B5C..0x82356BB0. Offline, the whole function is gated on road rules being
        // available. The console inlines ProgressionManager::AreRoadRulesAvailable @0x82311520
        // here -- the identical three-term test, term for term and register for register
        // (`>= 4u` on the Profile medal count, then the two road-rule tallies) -- and that body is
        // already committed and mounted, so it is reached BY NAME rather than re-open-coded.
        // ⚠️ THIS IS A REAL PROGRESSION GATE, not a bring-up artefact: on a profile that has never
        // won four events and has never ruled a road, showtime does not start offline. That is the
        // console's behaviour and it is reproduced, not softened.
        //
        // [FLAG PC bring-up gate 2026-08-29, NOT in the X360 binary] BRN_SHOWTIME_IGNORE_PROGRESSION.
        // ⛔ WHY IT EXISTS, and why it is a *gate* rather than a poke at the progression state.
        // The only two things that open AreRoadRulesAvailable are four medals
        // (ProgressionManager::AwardMedal -> Profile::SetMedalCountFromTheStart,
        // BrnProgressionManager_EventFinish.cpp:496) and a ruled road
        // (miNumberOfParCrash/ParTimeRoadRulesRuledByPlayer, which have NO writer on this build --
        // ProgressionManager::Construct zeroes them and nothing else touches them). Neither is
        // reachable today: finishing four offline events end-to-end is not something this build
        // can do yet. So a wave that wants to see the showtime SCORE cannot get into showtime at
        // all, and the score half of the feature would stay unverifiable indefinitely.
        // ⭐ WHAT IT DELIBERATELY DOES NOT DO. It does NOT write the medal count, does NOT write
        // either road-rule tally, and does NOT touch the save. Those words feed
        // ProgressionManager::GetPercentageComplete (BrnProgressionManager_Completion.cpp:261/:267)
        // and the unlock predicates (BrnProgressionManager_Unlocks.cpp:321/:325); forging them
        // would make a completion percentage and an unlock condition read differently, i.e. a
        // value that renders and is WRONG. This flag is scoped to THIS ONE DECISION -- every other
        // reader of progression still sees the true, empty profile -- and the nine gates below it
        // still run unchanged, so a run under it is still a real gate-stack pass.
        // ⚠️ OFF BY DEFAULT and CLEARED by tools/diagnostics/flow_run.ps1 on every run, on the same
        // CAPABILITY discipline as BRN_EVENT_FSM / BRN_START_EVENT: it changes what the game DOES.
        // A run taken under it is NOT comparable with a default run and must say so.
        // DELETE-WHEN: the offline event flow can award a medal (then four wins open the gate the
        // console's way and this flag is a lie).
        static const bool sbIgnoreProgressionGate = (getenv("BRN_SHOWTIME_IGNORE_PROGRESSION") != 0);

        if (!lbOnlineModeRunning && !mProgressionManager.AreRoadRulesAvailable() &&
            sbIgnoreProgressionGate && lbCrashModePressed)
        {
            // Say so, once, and say what the true state was -- a run under this flag must be
            // readable as such from its own log, not from the harness's banner alone.
            static bool sbSaidSo = false;
            if (!sbSaidSo && CgsDev::Log::gpDebugPrint != 0)
            {
                sbSaidSo = true;
                const BrnProgression::Profile* const lpcProfile = mProgressionManager.GetProfile();
                *CgsDev::Log::gpDebugPrint
                    << "[showtime] ⚠ BRN_SHOWTIME_IGNORE_PROGRESSION is set: the console's road-rules"
                       " gate REFUSED and is being ignored for this decision only. True state:"
                       " medalsFromTheStart="
                    << ((lpcProfile != 0)
                            ? static_cast<s32>(lpcProfile->GetMedalCountFromTheStart()) : -1)
                    << " parCrashRoadsRuled="
                    << static_cast<s32>(
                           mProgressionManager.GetNumberOfParCrashRoadRulesRuledByPlayer())
                    << " parTimeRoadsRuled="
                    << static_cast<s32>(
                           mProgressionManager.GetNumberOfParTimeRoadRulesRuledByPlayer())
                    << ". This run is NOT comparable with a default run. [FLAG PC bring-up]\n";
            }
        }

        if (!lbOnlineModeRunning && !mProgressionManager.AreRoadRulesAvailable() &&
            !sbIgnoreProgressionGate)
        {
            if (lbCrashModePressed)
            {
                // [DIAG] the three NUMBERS, not just the name of the gate: a bare "road rules are
                // not available" cannot be told apart from "the profile never loaded", and those
                // want opposite fixes. Printed once, through the same one-shot as every other
                // refusal reason. (The accessors are the public console ones.)
                // Re-armed on a CHANGE of the junkyard term, for the reason spelled out on
                // LogShowtimeRefusal above: the first press lands on the junkyard-exit frame, and
                // a snapshot taken there reports a term that may clear seconds later.
                static s32 siLastJunkyardTerm = -1;
                const s32  liJunkyardTerm = (mCarSelectManager.GetJunkyardId() != 0) ? 1 : 0;
                if (siLastJunkyardTerm != liJunkyardTerm && CgsDev::Log::gpDebugPrint != 0)
                {
                    siLastJunkyardTerm = liJunkyardTerm;
                    const BrnProgression::Profile* const lpProfile = mProgressionManager.GetProfile();
                    *CgsDev::Log::gpDebugPrint
                        << "[showtime] AreRoadRulesAvailable @0x82311520 terms: medalsFromTheStart="
                        << ((lpProfile != 0)
                                ? static_cast<s32>(lpProfile->GetMedalCountFromTheStart()) : -1)
                        << " (needs >= 4) parCrashRoadsRuled="
                        << static_cast<s32>(
                               mProgressionManager.GetNumberOfParCrashRoadRulesRuledByPlayer())
                        << " parTimeRoadsRuled="
                        << static_cast<s32>(
                               mProgressionManager.GetNumberOfParTimeRoadRulesRuledByPlayer())
                        << " (either > 0 also opens the gate)\n";

                    // ⭐ AND THE NINE TERMS BELOW IT, so the line answers "is the progression
                    // gate the ONLY thing in the way" rather than just "it is the FIRST thing".
                    // The console short-circuits and never evaluates these when road rules are
                    // unavailable; every one is a pure read, so evaluating them HERE (in a
                    // one-shot diagnostic, off the decision path) changes nothing.
                    *CgsDev::Log::gpDebugPrint
                        << "[showtime] the nine gates BELOW it, evaluated for the record: "
                        << "playerCarActive=" << (mLastActiveRaceCarInterface.IsPlayerCarActive() ? 1 : 0)
                        << " simPaused="      << (IsSimPaused(false, false) ? 1 : 0)
                        << " junkyardId="     << ((mCarSelectManager.GetJunkyardId() != 0) ? 1 : 0)
                        << " modeState="      << ((lpCurrentGameMode != 0)
                                                     ? lpCurrentGameMode->GetCurrentState() : -1)
                        << " showtimeBehaviour=" << static_cast<s32>(meShowtimeBehaviour)
                        << " modeType="       << static_cast<s32>(mModeManager.GetCurrentGameModeType())
                        << " postModeLockout=" << mfTimeSinceLastCrashMode
                        << " crashStartHold=" << mfTimeSpentDoingCrashStartAction
                        << "  (want: active=1 paused=0 junkyard=0 modeState=-1or2 behaviour!=0"
                           " modeType!=2and!=16)\n";
                }
                LogShowtimeRefusal("road rules are not available yet -- the profile needs "
                                       "4 medals from the start, or one ruled road "
                                       "(ProgressionManager::AreRoadRulesAvailable @0x82311520)");
            }
            return false;
        }

        // @0x82356BB4..0x82356BEC. The two-second post-mode lockout.
        if (mfTimeSinceLastCrashMode > 0.0f)
        {
            if (lbCrashModePressed)
            {
                LogShowtimeRefusal("the 2 s post-mode lockout (mfTimeSinceLastCrashMode) "
                                       "has not expired");
            }
            mfTimeSinceLastCrashMode        -= lfGameTimestep;
            mfTimeSpentDoingCrashStartAction = KF_CRASH_START_HOLD_SECONDS;
            return false;
        }

        // @0x82356BF0..0x82356C14. See the warning in the banner -- no re-arm on this path.
        if (lpSimTimerRequests == 0)
        {
            // [GUARD] Not X360. The console dereferences r6 unconditionally; both of its call
            // sites hand it OutputBuffer::GetTimerRequest() + 8, which cannot be null. Refusing
            // here is the same shape of guard StartCrashMode carries for lpOutput.
            return false;
        }
        if (lpSimTimerRequests->IsStartRequested()  ||
            lpSimTimerRequests->IsStopRequested()   ||
            lpSimTimerRequests->IsMultiplierRequested())
        {
            return false;
        }

        // -----------------------------------------------------------------------------------------
        // @0x82356C18..0x82356D4C. Nine tests, in the console's order, each of which branches to the
        // SAME tail (loc_82356D7C: re-arm the hold, return false). Written as one short-circuiting
        // chain because that is exactly what the branch structure is; the address on each line is
        // the console instruction that tests it.
        // -----------------------------------------------------------------------------------------

        // [X][X] DIVERGENCE at the fourth test (@0x82356C50..0x82356C60): the console reads
        // gsm+35880, which is mModeManager.mChallengeManager.meChallengeManagerStatus -- the
        // ChallengeManager sits at ModeManager+28160 and the status word at ChallengeManager+3592
        // (0xE08), pinned by twenty-one of its own methods, of which Construct @0x82332DB0 stores 0,
        // BeginChallenge stores 1, TriggerFreeburnChallenge stores 2 and EndChallenge stores 3.
        // ModeManager does NOT embed the ChallengeManager on this build (the divergence is recorded
        // at its console seat in BrnModeManager.h: 29 TUs, ~35 unresolved externals), so no freeburn
        // challenge can be begun and the status can only ever hold its Construct value.
        // => The term is written as the constant the object's absence forces, NOT as a convenient
        // one: it is false because nothing on this build can make it true, and the moment the
        // ChallengeManager mount lands this line must become the real read.
        // DELETE-WHEN ModeManager embeds mChallengeManager.
        const bool lbFreeburnChallengeRunning = false;

        const GameStateModuleIO::EGameModeType leCurrentGameModeType =
            mModeManager.GetCurrentGameModeType();

        const bool lbConditionsMet =
               lbCrashModePressed                                                   // @0x82356C18
            && mLastActiveRaceCarInterface.IsPlayerCarActive()                      // @0x82356C2C
            && !IsSimPaused(false, false)                                           // @0x82356C44 (raw miSimPauseFlags == 0)
            && !lbFreeburnChallengeRunning                                          // @0x82356C58 [X][X] see above
            && (mCarSelectManager.GetJunkyardId() == 0)                             // @0x82356C6C (an 8-byte `ldx`: no junkyard flow)
            && (lpCurrentGameMode == 0 ||
                lpCurrentGameMode->GetCurrentState() ==
                    GameStateModuleIO::E_GMS_IN_PROGRESS)                           // @0x82356C8C
            && (meShowtimeBehaviour != E_SHOWTIME_MODE_OFF)                         // @0x82356CC8
            && (!lbOnlineModeRunning ||
                leCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY ||
                leCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) // @0x82356CDC
            && (leCurrentGameModeType != GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME &&
                leCurrentGameModeType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME);// @0x82356D28

        if (!lbConditionsMet)
        {
            // [DIAG] one line, once, naming the FIRST gate that said no. Not behind an env guard,
            // for the same reason StartModeAtLights' wrong-car line is not (GameStateModule_gSR_00
            // .cpp): from the outside a refusal and a dead input channel print the same thing --
            // nothing -- and that is exactly the ambiguity the player report started from. The
            // terms are re-evaluated here, not captured above, so the decision path stays the
            // console's short-circuit chain; every one of them is a pure read.
            // [[diagnostics-that-lie]] -- a probe must say what it could not see.
            if (lbCrashModePressed)
            {
                const char* lpcReason =
                    (!mLastActiveRaceCarInterface.IsPlayerCarActive())      ? "no active player car"
                  : (IsSimPaused(false, false))                             ? "the sim is paused (miSimPauseFlags != 0)"
                  : (lbFreeburnChallengeRunning)                            ? "a freeburn challenge is running"
                  : (mCarSelectManager.GetJunkyardId() != 0)                ? "a junkyard flow is active"
                  : (lpCurrentGameMode != 0 &&
                     lpCurrentGameMode->GetCurrentState() !=
                         GameStateModuleIO::E_GMS_IN_PROGRESS)              ? "a game mode is running and is not IN_PROGRESS"
                  : (meShowtimeBehaviour == E_SHOWTIME_MODE_OFF)            ? "meShowtimeBehaviour is E_SHOWTIME_MODE_OFF"
                  : (leCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
                     leCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME) ? "showtime is already running"
                  :                                                           "the online mode type is not a lobby/showtime";
                LogShowtimeRefusal(lpcReason);
            }
            mfTimeSpentDoingCrashStartAction = KF_CRASH_START_HOLD_SECONDS;   // loc_82356D7C
            return false;
        }

        // @0x82356D50..0x82356D74. `f0 = hold - dt ; store ; return f0 <= 0.0f`.
        mfTimeSpentDoingCrashStartAction -= lfGameTimestep;
        return mfTimeSpentDoingCrashStartAction <= 0.0f;
    }

    // =============================================================================================
    // GameStateModule::IsInShowtimeIntro  @0x82356A60  (11 insns)
    //
    // The whole body is `lfsx f13, this, 0x45720 ; fcmpu f13, flt_82001CC0 (0.0f) ; bgt -> 1`.
    // =============================================================================================
    bool GameStateModule::IsInShowtimeIntro() const
    {
        return mfShowtimeIntroTimeLeft > 0.0f;
    }

    // =============================================================================================
    // GameStateModule::GetShowtimeIntroSteering  @0x82356A90  (33 insns)
    //
    // The same `> 0.0f` test, this time as a NON-RETURNING assert (BrnGameStateModule.cpp:5050),
    // then `lfsx f1, this, 0x45724`. The console fires the assert and reads the word anyway; that
    // is reproduced -- an early return would invent a value the binary never has.
    // ⓘ ITS CONSUMER IS ALREADY WAITING: GameBridgeControllerToX.cpp carries a FLAG'd leg that
    // wants exactly this pair to force the player's steering during the intro window.
    // =============================================================================================
    f32 GameStateModule::GetShowtimeIntroSteering() const
    {
        CGS_ASSERT(IsInShowtimeIntro(), "IsInShowtimeIntro()");   // :5050
        return mfShowtimeIntroSteering;
    }

    // =============================================================================================
    // GameStateModule::ToggleShowtimeBehaviour  (DWARF BrnGameStateModule.h:651)
    //
    // No out-of-line console symbol: its one caller, GameStateDebugComponent::ToggleShowtimeCallback
    // @0x823578F8, inlines it as `*(module + 284512) = 1`. UpdateShowtimeMode below consumes it.
    // =============================================================================================
    void GameStateModule::ToggleShowtimeBehaviour()
    {
        mbToggleShowtimeBehaviour = true;
    }

    // =============================================================================================
    // GameStateModule::UpdateShowtimeMode  @0x82380EF8  (163 insns; DWARF BrnGameStateModule.h:691,
    // source BrnGameStateModule.cpp:1484..1611)  [FX-SHOWTIME2 2026-09-24]
    //
    // THE PRE-WORLD HALF OF THE SHOWTIME "CARS CRASHED" CHAIN. ProcessContacts (post-world) pushes
    // each newly-crashed traffic car's index onto mShowtimePendingTrafficIndexStack; this function
    // turns them, one every KI_SHOWTIME_TRAFFIC_RESPONSE_FRAMES frames, into traffic-type REQUESTS
    // (action 116) and, on the next frame, the traffic module's ANSWER into a score
    // (CrashModeScoring::DealWithScoreForVehicleClass -- the only writer of maiNumCarsCrashed) and a
    // VehicleHitAction (140) for the director's close-up, crash play and the GUI. Until this body
    // landed nothing popped the stack: it filled to eight and the count never moved.
    //
    // ARGUMENTS (prologue @0x82380F04..0x82380F14): r3 = this (r30), r5 = lpOutput (r25),
    // r7 = lpResponseQueue (r26). r4 (lpInput) and r6 (lpContacts) are never read -- the DWARF
    // names them and the caller passes them; the X360 body has no load through either.
    //
    // THE FOUR LEGS, in the console's order:
    //   1. 0x82380F0C..0x82380F78  the debug toggle: mbToggleShowtimeBehaviour set ->
    //      meShowtimeBehaviour = (x + 1) % 3 (`mulhw 0x55555556` + sign fix: a signed % 3), stored to
    //      the record AND the module, action 138 (size 4), then the flag cleared.
    //   2. 0x82380F7C..0x82381084  the outstanding request: while muShowtimeRequestedTrafficIndex is
    //      not K_INVALID_VEHICLE_INDEX, walk the response queue (GetEvent per index, length re-read
    //      every iteration) for the FIRST element whose muVehicleIndex matches; score it
    //      (r4 = lhz +0, r5 = lwz +4 class, r6 = ld +8 CgsID, r7..r10 + the stack slot = the record's
    //      five out-fields) and post action 140 (size 0x24) with the four fields the caller fills
    //      itself (+0x00 class, +0x08 GetNumCarsCrashed() -- the four-word sum at scorer+0x2E8,
    //      +0x18 miScoreMultiplier, +0x20 the index), then invalidate the index. Then, match or not,
    //      assert that it IS invalid (BrnGameStateModule.cpp:1611 -- a missing answer is a console
    //      assert, not a retry) and invalidate it unconditionally.
    //   3. 0x82381088..0x82381128  the next request: the inlined IsEmpty (with its
    //      "Stack used before Construct/Clear was called" tripwire, CgsStack.h:177); non-empty ->
    //      `addic. -1 ; bgt`: decrement miShowtimePendingFrameDelay and, once it is no longer
    //      positive, Peek -> action 116 (size 2) -> muShowtimeRequestedTrafficIndex -> Pop -> re-seed
    //      the delay to KI_SHOWTIME_TRAFFIC_RESPONSE_FRAMES. So the first victim goes the frame after
    //      its push, the rest every second frame, and each answer is consumed the frame after its
    //      request -- which is why the answer MUST come back within one frame.
    //   4. 0x8238112C..0x82381178  AchievementManagerBase::OnShowTimeMultiplier(miScoreMultiplier),
    //      inlined (x10 -> console achievement 13). Runs every frame, showtime or not.
    // =============================================================================================
    void GameStateModule::UpdateShowtimeMode(
            const GameStateModuleIO::PreWorldInputBuffer*       lpInput,
            GameStateModuleIO::OutputBuffer*                    lpOutput,
            const BrnPhysics::ContactSpy::ContactSpyInterface*  lpContacts,
            const CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse>* lpResponseQueue)
    {
        (void)lpInput;      // r4 -- never read by the console body
        (void)lpContacts;   // r6 -- never read by the console body

        // ---- 1. the debug behaviour toggle (0x82380F0C..0x82380F78) ---------------------------
        if (mbToggleShowtimeBehaviour)
        {
            GameStateModuleIO::ToggleShowtimeBehaviourAction lToggleAction;
            meShowtimeBehaviour =
                static_cast<EShowtimeBehaviour>((meShowtimeBehaviour + 1) % E_SHOWTIME_MODE_COUNT);
            lToggleAction.meShowtimeBehaviour = meShowtimeBehaviour;
            lpOutput->GetGameActionQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lToggleAction),
                GameStateModuleIO::E_ACTION_TOGGLE_SHOWTIME_BEHAVIOUR,
                static_cast<s32>(sizeof(lToggleAction)));
            mbToggleShowtimeBehaviour = false;

            if (ShowtimeScoreWitness())
            {
                *CgsDev::Log::gpDebugPrint
                    << "[showtime-score] toggle -> meShowtimeBehaviour " << static_cast<s32>(meShowtimeBehaviour)
                    << " -> action 138\n";
            }
        }

        // ---- 2. the answer to the outstanding request (0x82380F7C..0x82381084) ----------------
        if (muShowtimeRequestedTrafficIndex != K_INVALID_VEHICLE_INDEX)
        {
            for (s32 liResponseIndex = 0; liResponseIndex < lpResponseQueue->GetLength(); ++liResponseIndex)
            {
                const BrnTraffic::BrnTrafficIO::TrafficTypeResponse& lResponse =
                    lpResponseQueue->GetEvent(liResponseIndex);
                if (lResponse.muVehicleIndex != muShowtimeRequestedTrafficIndex)
                {
                    continue;
                }

                CrashModeScoring* const lpCrashScorer = mModeManager.GetScoringSystem()->GetCrashScorer();

                GameStateModuleIO::VehicleHitAction lHitAction = {};   // +0x22..+0x23 never written by the console
                lpCrashScorer->DealWithScoreForVehicleClass(lResponse.muVehicleIndex,
                                                            lResponse.meType,
                                                            lResponse.mTypeId,
                                                            &lHitAction.miVehicleTypeCrashed,
                                                            &lHitAction.miVehicleBaseScore,
                                                            &lHitAction.meVehicleScoreCategory,
                                                            &lHitAction.miScoreMultiplierEarned,
                                                            &lHitAction.miComboBonusEarned);
                lHitAction.muTrafficEntityIndex   = lResponse.muVehicleIndex;           // sth var_50
                lHitAction.meVehicleClass         = lResponse.meType;                   // stw var_70
                lHitAction.miTotalVehiclesCrashed = lpCrashScorer->GetNumCarsCrashed(); // stw var_68
                lHitAction.miTotalScoreMultiplier = lpCrashScorer->GetScoreMultiplier();// stw var_58
                lpOutput->GetGameActionQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lHitAction),
                    GameStateModuleIO::E_ACTION_VEHICLE_HIT,
                    static_cast<s32>(sizeof(lHitAction)));

                if (ShowtimeScoreWitness())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[showtime-score] answer traffic car " << static_cast<s32>(lResponse.muVehicleIndex)
                        << " class " << static_cast<s32>(lResponse.meType)
                        << " -> DealWithScoreForVehicleClass: carsCrashed " << lHitAction.miTotalVehiclesCrashed
                        << " (class tally " << lHitAction.miVehicleTypeCrashed << ")"
                        << " base " << lHitAction.miVehicleBaseScore
                        << " category " << static_cast<s32>(lHitAction.meVehicleScoreCategory)
                        << " mult +" << lHitAction.miScoreMultiplierEarned
                        << " (total " << lHitAction.miTotalScoreMultiplier << ")"
                        << " chain " << lHitAction.miComboBonusEarned
                        << " -> action 140\n";
                }

                muShowtimeRequestedTrafficIndex = K_INVALID_VEHICLE_INDEX;
                break;
            }

            if (muShowtimeRequestedTrafficIndex != K_INVALID_VEHICLE_INDEX && ShowtimeScoreWitness())
            {
                *CgsDev::Log::gpDebugPrint
                    << "[showtime-score] MISS: no answer for traffic car "
                    << static_cast<s32>(muShowtimeRequestedTrafficIndex)
                    << " among " << lpResponseQueue->GetLength() << " responses (assert :1611)\n";
            }
            CGS_ASSERT(muShowtimeRequestedTrafficIndex == K_INVALID_VEHICLE_INDEX,
                       "muShowtimeRequestedTrafficIndex == K_INVALID_VEHICLE_INDEX");   // :1611 (li r5, 0x64B)
            muShowtimeRequestedTrafficIndex = K_INVALID_VEHICLE_INDEX;
        }

        // ---- 3. the next pending victim (0x82381088..0x82381128) ------------------------------
        // The inlined IsEmpty's constructed-check (CgsStack.h:177, li r5, 0xB1) is the console's
        // own; this tree's Stack::IsEmpty() carries no assert, so it is spelled at the call site.
        CGS_ASSERT(mShowtimePendingTrafficIndexStack.GetLength() != CgsContainers::KI_STACK_UNCONSTRUCTED,
                   "Stack used before Construct/Clear was called");                   // CgsStack.h:177
        if (!mShowtimePendingTrafficIndexStack.IsEmpty())
        {
            --miShowtimePendingFrameDelay;
            if (miShowtimePendingFrameDelay <= 0)
            {
                GameStateModuleIO::TrafficTypeRequestAction lRequest;
                lRequest.muTrafficVehicleIndex = mShowtimePendingTrafficIndexStack.Peek();
                lpOutput->GetGameActionQueue()->AddEvent(
                    reinterpret_cast<const CgsModule::Event*>(&lRequest),
                    GameStateModuleIO::E_ACTION_TRAFFIC_TYPE_REQUEST,
                    static_cast<s32>(sizeof(lRequest)));
                muShowtimeRequestedTrafficIndex = lRequest.muTrafficVehicleIndex;
                mShowtimePendingTrafficIndexStack.Pop();
                miShowtimePendingFrameDelay = KI_SHOWTIME_TRAFFIC_RESPONSE_FRAMES;

                if (ShowtimeScoreWitness())
                {
                    *CgsDev::Log::gpDebugPrint
                        << "[showtime-score] pop traffic car " << static_cast<s32>(lRequest.muTrafficVehicleIndex)
                        << " -> action 116 (still pending " << mShowtimePendingTrafficIndexStack.GetLength()
                        << ")\n";
                }
            }
        }

        // ---- 4. the x10 multiplier achievement (0x8238112C..0x82381178, inlined) --------------
        mAchievementManager.OnShowTimeMultiplier(
            mModeManager.GetScoringSystem()->GetCrashScorer()->GetScoreMultiplier());
    }
}
