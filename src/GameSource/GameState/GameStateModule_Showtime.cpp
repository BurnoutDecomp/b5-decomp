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

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                      // EGameModeType, EGameModeState, IsOnlineFreeBurnLobby
#include "GameSource/GameState/BrnGameStateModuleIO.h"                      // PreWorldInputBuffer / OutputBuffer / ControllerInput
#include "GameSource/GameState/BrnGameStateTypes.h"                         // EShowtimeBehaviour
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"         // GameMode::IsOnline / GetCurrentState
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"   // StartGameModeParams
#include "GameSource/GameState/Progression/BrnProgressionManager.h"         // AreRoadRulesAvailable
#include "GameSource/GameState/CarSelect/BrnCarSelectManager.h"             // GetJunkyardId
#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h"   // CgsSystem::TimerRequests
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

namespace BrnGameState
{
namespace
{
    // [DIAG] NOT IN THE X360 BINARY. ONE line per process, naming the gate that refused a
    // both-bumpers press. It exists because the defect this file closes was reported by a player
    // as "the buttons do nothing", and a gate stack with nine terms has nine ways to look exactly
    // like a dead button. Deliberately NOT env-gated (see the call sites); at most one line ever.
    void LogShowtimeRefusalOnce(const char* lpcReason)
    {
        static bool sbLogged = false;
        if (!sbLogged && CgsDev::Log::gpDebugPrint != 0)
        {
            sbLogged = true;
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
        if (!lbOnlineModeRunning && !mProgressionManager.AreRoadRulesAvailable())
        {
            if (lbCrashModePressed)
            {
                LogShowtimeRefusalOnce("road rules are not available yet -- the profile needs "
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
                LogShowtimeRefusalOnce("the 2 s post-mode lockout (mfTimeSinceLastCrashMode) "
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
                LogShowtimeRefusalOnce(lpcReason);
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
}
