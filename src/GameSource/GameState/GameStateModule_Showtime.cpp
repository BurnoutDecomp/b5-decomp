// =================================================================================================
// GameStateModule_Showtime.cpp
//
// The SHOWTIME (crash-mode) start leg. Landed 2026-08-27 (showtime S7b-a wave), immediately after
// PhysicsModule::HandleGameActions @0x825A72F0 landed (b5-decomp e7f9f116) and made
// VehicleManager::SetPlayerCarToShowtimeMode reachable at all.
//
// WHAT IS HERE, AND WHAT IS DELIBERATELY NOT
// ------------------------------------------
// ONE console function, reconstructed:
//     GameStateModule::StartCrashMode @0x8236B580 (80 insns)
// ONE harness leg, NOT in the X360 binary and clearly marked:
//     GameStateModule::HarnessInjectShowtimeBringUp
//
// The console's showtime chain is
//     DetectModeStarts @0x8239A428 (the `else` arm, 0x8239A568..0x8239A8EC, ~225 insns)
//       -> ShouldStartShowtimeMode @0x82356B18 (166 insns)
//         -> StartCrashMode @0x8236B580          <-- LANDED HERE
//           -> StartGameModeParams::Construct @0x8231C1F8   (mounted)
//           -> ModeManager::StartGameMode      @0x8234FCE8   (mounted)
// The two upper links are NOT reconstructed and this file does not pretend otherwise. The `else`
// arm is a named PARK in GameStateModule_gSR_00.cpp (a squared-speed compare against 10.0f, a
// 0.5 s window latch at gsm+284448, a cached direction vector at gsm+284464, a facing dot-product
// re-test, an alignment bit at gsm+284510, a sign latch at gsm+284452, and two posts of game event
// 146); ShouldStartShowtimeMode is a 166-instruction gate stack over eleven GameStateModule
// members that this slice does not home. What the harness leg stands in for is EXACTLY those two,
// and nothing below them.
//
// ⭐ THAT IS THE SAME TRADE, IN THE SAME PLACE, AS HarnessInjectEventStartBringUp. That hook
// (GameStateModule_gUI_00.cpp, 2026-08-26) substitutes for ShouldStartSnapRaceMode's 0.35 s
// analogue hold and calls StartModeAtLights directly. This one substitutes for
// ShouldStartShowtimeMode's hold/speed/facing stack and calls StartCrashMode directly. In both
// cases everything downstream of the call is the console's own, and a failure downstream fails
// exactly the way the real gesture would -- which is the point of doing it this way rather than
// poking a mode type into ModeManager.
//
// ⭐⭐ AND THE GESTURE ITSELF IS REAL. This hook does NOT invent its trigger: it reads
// `ControllerInput::mbCrashModePressed` (+0x42) off the pre-world input buffer -- the console's
// own both-bumpers-held byte, written every frame by BrnGameStateModuleIO.cpp:92 from action rows
// 54 and 55, which are bound to LSHOULDER/RSHOULDER (and to the keyboard) in
// CgsInputPadsPC.cpp's KA_BINDINGS. A player holding both bumpers on a real pad drives this, and
// so does the harness, through the ordinary input chain. The env gate exists so a DEFAULT run
// cannot leave free burn, not because the gesture is fake.
// =================================================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"                      // EGameModeType, IsOnlineFreeBurnLobby
#include "GameSource/GameState/BrnGameStateModuleIO.h"                      // PreWorldInputBuffer / OutputBuffer / ControllerInput
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h"   // StartGameModeParams
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdlib>   // getenv (the harness gate, same shape as every other diag gate in this module)

namespace BrnGameState
{
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
    // GameStateModule::HarnessInjectShowtimeBringUp -- HARNESS-ONLY, NOT IN THE X360 BINARY.
    //
    // WHAT IT SUBSTITUTES: `ShouldStartShowtimeMode @0x82356B18` (166 insns) and the
    // `DetectModeStarts` else-arm that calls it. Between them those two require: a squared-speed
    // compare against 10.0f, a 0.5 s window latch, a cached facing direction re-tested by dot
    // product, an alignment bit, a sign latch, a hold timer at gsm+284440 counting down from
    // 0.0099999998, and eleven GameStateModule members none of which is homed on this slice.
    //
    // WHAT IT DOES NOT SUBSTITUTE: anything below StartCrashMode. The mode type, the player
    // position, the params construction, ModeManager::StartGameMode, PrepareForMode's action-23
    // post with KU_FLAG_USE_SHOWTIME_VEHICLE_BEHAVIOUR, and PhysicsModule::HandleGameActions'
    // case-23 arm calling SetPlayerCarToShowtimeMode are ALL the console's own. If showtime does
    // not start after this fires, the failure is downstream and real.
    //
    // ⭐ THE TRIGGER IS THE REAL GESTURE. `mbCrashModePressed` is ControllerInput +0x42, and its
    // producer is the console's own -- BrnGameStateModuleIO.cpp:92, `(row 54 held) && (row 55
    // held)`, i.e. BOTH BUMPERS. Rows 54/55 are bound to LSHOULDER/RSHOULDER in KA_BINDINGS, so a
    // pad player holding both bumpers reaches this, and so does the scripted harness through the
    // two shoulder channels ConsumeHarnessAction serves. This hook does NOT fabricate an input.
    //
    // GATES, all four required, and it fires AT MOST ONCE per process:
    //   1. BRN_START_SHOWTIME=1 in the environment (read once, like every other diag gate here).
    //      ⛔ IT IS A CAPABILITY, NOT AN INSTRUMENT: a run carrying it can leave free burn, so it
    //      must be in flow_run.ps1's CLEARED list and no golden may be banked or gated through it.
    //   2. mbCrashModePressed -- the gesture above.
    //   3. no game mode already running (the same !mpCurrentGameMode gate every console start arm
    //      carries, and the same one HarnessInjectEventStartBringUp uses).
    //   4. the player car is active -- because GetPlayerPosition() inside StartCrashMode asserts
    //      IsPlayerCarActive() and would otherwise read maRaceCars[-1].
    //      ⚠️ THAT GATE IS THE CONSOLE'S TOO, one level up: ShouldStartShowtimeMode's own chain
    //      calls RCEntityActiveRaceCarOutputInterface::IsPlayerCarActive @0x82277B90 before it can
    //      ever reach StartCrashMode. Checking it here reproduces that ordering rather than adding
    //      a guard the console lacks.
    //
    // DELETE-WHEN: ShouldStartShowtimeMode and the DetectModeStarts else-arm land. This function
    // and its one call site go together.
    // =============================================================================================
    void GameStateModule::HarnessInjectShowtimeBringUp(
        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
        GameStateModuleIO::OutputBuffer*              lpOutputBuffer)
    {
        static const bool sbHarnessShowtime = (getenv("BRN_START_SHOWTIME") != 0);
        static bool       sbFired           = false;

        if (!sbHarnessShowtime || sbFired || lpInput == 0 || lpOutputBuffer == 0)
        {
            return;
        }

        const GameStateModuleIO::ControllerInput* const lpControllerInput =
            lpInput->GetControllerInput();
        if (lpControllerInput == 0 || !lpControllerInput->mbCrashModePressed)
        {
            return;
        }

        // [DIAG] ONE line the first time the gesture is SEEN, before the two remaining gates, so a
        // run that presses the bumpers and gets nothing says WHICH gate refused rather than looking
        // like a dead input channel. Without this, "no showtime" and "no button press" print the
        // same thing -- nothing. [[diagnostics-that-lie]] -- ask what the probe cannot see.
        {
            static bool sbGestureSeen = false;
            if (!sbGestureSeen && CgsDev::Log::gpDebugPrint != 0)
            {
                sbGestureSeen = true;
                *CgsDev::Log::gpDebugPrint
                    << "[showtime] gesture SEEN: ControllerInput::mbCrashModePressed (both bumpers)"
                    << " is true; modeRunning="
                    << ((mModeManager.GetCurrentGameMode() != 0) ? 1 : 0)
                    << " playerCarActive="
                    << (mLastActiveRaceCarInterface.IsPlayerCarActive() ? 1 : 0) << "\n";
            }
        }

        if (mModeManager.GetCurrentGameMode() != 0)
        {
            return;
        }

        if (!mLastActiveRaceCarInterface.IsPlayerCarActive())
        {
            return;
        }

        sbFired = true;

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "[showtime] ***** HARNESS-ONLY SHOWTIME INJECTION (BRN_START_SHOWTIME=1) *****"
                << " the gesture is REAL (mbCrashModePressed, both bumpers, the console's own"
                << " +0x42 byte); what is bypassed is ShouldStartShowtimeMode @0x82356B18's"
                << " hold/speed/facing gate stack. Calling StartCrashMode @0x8236B580 --"
                << " everything below it is the console's own. One-shot.\n";
        }

        StartCrashMode(lpInput, lpOutputBuffer);

        if (CgsDev::Log::gpDebugPrint != 0)
        {
            // The post-condition, by name. ModeManager::StartGameMode is synchronous, so by this
            // line the mode object either exists or the start refused -- and this says which.
            const GameMode* const lpMode = mModeManager.GetCurrentGameMode();
            *CgsDev::Log::gpDebugPrint
                << "[showtime] StartCrashMode returned: mpCurrentGameMode "
                << ((lpMode != 0) ? "LIVE" : "still null")
                << " modeType " << static_cast<s32>(mModeManager.GetCurrentGameModeType())
                << " (2 == E_MODE_OFFLINE_SHOWTIME, 16 == E_MODE_ONLINE_SHOWTIME)\n";
        }
    }
}
