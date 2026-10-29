// ============================================================================
// b5-decomp/src/GameSource/GameState/GameStateModule_gTD_00.cpp
//
// [takedown wave 2026-09-02, conductor] THE TAKEDOWN MANAGER'S PLUMBING INTO GameStateModule.
//
//
// Console positions reproduced here (GameStateModule::PreWorldUpdate @0x823A5328, !IsSimPaused arm,
// ): SetFromVehicleOutputInterface(stack, cachedVehicleOutput == r24 ==
// gsm+250816) ->
// TakedownManager::Update(this+0x238, activeIf, dt, crashQ, &crashingIf, preIn, out, trafficTypeQ)
// -> `*(gsm+249944) = 0` (the module copy's miLength) + TakedownEvent_::Append(gsm+249936, out's
// takedown queue) -> MugshotManager::Update(gsm +0x500, ...) -> PaybackManager::Update(gsm +0x570,
// ...) ->
// ProcessTakedownEvents(actionQ, gsm+249936, out).
// ============================================================================

#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/BrnGameStateModuleIO.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameSource/GameState/TakedownManager/BrnTakedownManager.h"
#include "GameSource/GameState/MugshotManager/BrnMugshotManager.h"     // MugshotManager (Construct / Update)
#include "GameSource/GameState/PaybackManager/BrnPaybackManager.h"     // PaybackManager (Construct / Update)
#include "GameSource/GameState/Offences/BrnDriveThruManager.h"    // DriveThroughsCanNowOpenAgain (OnModeFinish / OnModeEnd)
#include "GameSource/GameState/BrnGameActions.h"                   // SetTakedownCameraAction (OnModeFinish)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"   // GameMode::GetCurrentState / GetTimeInMode (harness hook)
#include <cstdlib>   // getenv / atof (harness hook)

namespace BrnGameState
{

// ==============================================================================================
// GameStateModule::CacheTakedownManagerPostWorldInputData  (, post-world `bl` #18)
//
// The console body, statement for statement (r29 == gsm, r26 == lpInput, r31 == gsm+250816,
// r27 == gsm+250800, r30 == lpInput's VehicleOutputInterface):
//   CGS_ASSERT(lpInput, "lpInput")
//   *(gsm+250280) = 0                     the crash queue's miLength                 (Clear)
//   *(gsm+250800) = 0                     the contact-spy word
//   *(+250816 +0x2628) = 0                the cached traffic-state queue's miLength   (Clear)
//   *(+250816 +0x2318) = 0                the cached impact queue's miLength          (Clear)
//   VariableEventQueue<1536,16>::Clear(+250816 +0x65F0)   the cached game-event queue (Clear)
//   std 0, +250816                        the used-cars head, zeroed
//   stb 0, +250816 +0x6C00 .. +0x6C04     the five AggressiveDrivingFlags bytes
//  *(gsm+250800) = *<contact-spy accessor>(lpInput)  [NAME NOT RECOVERED: that
//                                        symbol is unnamed in the image and its body is only the
//                                        "Not locked for reading" assert around a member fetch]
//   RaceCarCrashEvent_::Append(gsm+250272, GetRaceCarCrashEventQueue(lpInput))
//   r30 = GetVehicleOutputInterface(lpInput)
//   PhysicalTrafficState_::Append(+250816 +0x2620, r30 +0x2620)
//   ImpactEvent_::Append          (+250816 +0x2310, r30 +0x2310)
//   VariableEventQueue::Append    (+250816 +0x65F0, r30 +0x65F0)
//   std *(r30 +0), +250816                the used-cars head
//   memcpy(+250816 +0x10, r30 +0x10, 0x2300)   the eight RaceCarStates (CONSOLE span)
// That Clear-then-Append/copy of every field of the cached interface IS
// VehicleOutputInterface::operator= (, BrnVehicleOutputInterface.cpp), so the copy runs
// through that committed symbol -- by sizeof on the host, never at the console's 8960.
//   ONE RECORDED DIFFERENCE: the console leaves the cached AggressiveDrivingFlags ZEROED (the five
//   byte stores above are never followed by a copy) while operator= copies them. Nothing reads that
//   field out of the cache -- SetFromVehicleOutputInterface touches only mUsedRaceCars and the
//   RaceCarStates -- so the copy is inert; recorded rather than special-cased.
//
// The contact-spy handle (gsm+250800, mContactSpyInterface) is cleared and then copied out of the
// buffer here as well; its reader is RumbleManager::Update's UpdateImpacts on the next pre-world.
// ==============================================================================================
void GameStateModule::CacheTakedownManagerPostWorldInputData(
        const GameStateModuleIO::PostWorldInputBuffer* lpInput)
{
    CGS_ASSERT(lpInput, "lpInput");

    mRaceCarCrashEventQueue.Clear();
    mContactSpyInterface.Construct();
    mContactSpyInterface = *lpInput->GetContactSpyInterface();
    mRaceCarCrashEventQueue.Append(*lpInput->GetRaceCarCrashEventQueue());
    mVehicleOutputInterface = *lpInput->GetVehicleOutputInterface();
}

// ProcessGameEvents @0x823A0A18 case 27 (POST_EVENT_LEAVE) ends with `TakedownManager::
// ClearRaceCarData(gsm+568)` -- wired. The console's other callers are ProcessGameEvents case 32
// (no such arm on this build yet) and GameStateModule::OnModeEnd @0x823767E0 (below, LIVE
// 2026-09-10). ClearAllTakedowns' host callers: OnModeFinish @0x82390EE0 (below) and the online
// case 18 (no such arm yet).
// ⓘ [FX-TAILS-A 2026-09-24] Neither wrapper tests the manager pointer: the console has no pointer to
// test -- the manager is embedded at gsm+0x238 and every caller calls it straight
// (UpdateCurrentMode @0x823513DC `addi r3, r11, 0x238 ; bl IsInTakedownCamera`; OnModeEnd @0x82376804
// `addi r3, r31, 0x238 ; bl ClearRaceCarData`). On PC the pointer is set by Construct
// (ConstructTakedownBringUp) before any caller can run, so the `!= 0` tests these carried were
// invented arms that could only ever pass.
bool GameStateModule::IsInTakedownCamera() const
{
    return mTakedownManager.IsInTakedownCamera();
}

void GameStateModule::ClearTakedownRaceCarData()
{
    mTakedownManager.ClearRaceCarData();
}

// ==============================================================================================
// GameStateModule::OnModeFinish  (X360 0x82390EE0) -- FinishCurrentMode @0x8234B978's last call.
//
//   0x82390EFC  var_30 = -1 ; var_2C = 0 ; var_2B = 0        (an 8-byte SetTakedownCameraAction:
//                                                             focus -1, active 0, signature 0)
//   0x82390F1C  AddEvent(lpOutputBuffer->GetGameActionQueue(), &var_30, 6, 8)
//   0x82390F30  TakedownManager::ClearAllTakedowns(this + 568, lpOutputBuffer->GetGameActionQueue())
//   0x82390F38  stb 0, this+46620 ; std 0, this+46448       == DriveThruManager (this+44240)
//               +0x94C mbDriveThroughsCloseWhenUsed / +0x8A0 maDriveThroughClosed:
//               DriveThroughsCanNowOpenAgain().
// ==============================================================================================
void GameStateModule::OnModeFinish(GameStateModuleIO::OutputBuffer* lpOutputBuffer)
{
    GameStateModuleIO::GameActionQueue* lpActionQueue = lpOutputBuffer->GetGameActionQueue();

    GameStateModuleIO::SetTakedownCameraAction lCameraOff;
    lCameraOff.meFocusOnRaceCarIndex = ::E_ACTIVE_RACE_CAR_INDEX_INVALID; // li r11, -1 -> var_30 (the GLOBAL enum -- two of this name are visible here)
    lCameraOff.mbActive              = false;                             // stb 0, var_2C
    lCameraOff.mbIsSignature         = false;                             // stb 0, var_2B
    lCameraOff.mbIsRevengeTakedown   = false;                             // +6 / +7: stack residue on
    lCameraOff.muPad07               = 0;                                 //  the console; zeroed here
    lpActionQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lCameraOff),
                            GameStateModuleIO::E_ACTION_SET_TAKEDOWN_CAMERA_STATE,
                            static_cast<s32>(sizeof(lCameraOff)));

    // Embedded by value on the console (`addi r3, r30, 0x238 ; bl ClearAllTakedowns` @0x82390F2C,
    // r4 = the action queue re-fetched @0x82390F24) -- no test. A pointer on this build (see
    // ConstructTakedownBringUp), always set by Construct; the `!= 0` guard it used to carry is gone
    // [FX-TAILS-A 2026-09-24, REVIEW-E on a5619c29].
    mTakedownManager.ClearAllTakedowns(lpActionQueue);

    mDriveThruManager.DriveThroughsCanNowOpenAgain();
}

// ==============================================================================================
// GameStateModule::OnModeEnd  (X360 0x823767E0) -- SendModeStopMessages @0x8234BEC0's tail.
//
//   0x823767F4  MugshotManager::OnRoundEnd(this + 0x500, r4)
//   0x823767FC  PaybackManager::OnRoundEnd(this + 0x570, r4)
//     ✅ [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] WIRED -- the "argument unrecovered" FLAG that
//     stood here is resolved. The console sets only r3 at these two calls because r4 is ALREADY the
//     argument: SendModeStopMessages loads `mr r4, r29` (== !lbOnlineLobbyHandover) @0x8234C6DC
//     before `bl OnModeEnd`, OnModeEnd never writes r4, and MugshotManager::OnRoundEnd @0x82357AF8
//     only reads it (`clrlwi r11, r4, 24`, no r4 store), so the same bool reaches both callees.
//     DWARF: OnModeEnd(bool) :609, both OnRoundEnd(bool lbResetState).
//   0x82376804  TakedownManager::ClearRaceCarData(this + 568)
//   0x82376808  lwz this+7604 == meCurrentGameModeType ; == 2 || == 16 (the two SHOWTIME modes) ->
//   0x82376838      stw 0, +284504   == mShowtimePendingTrafficIndexStack's count (Clear)
//   0x8237683C      stfs 2.0, +284444 == mfTimeSinceLastCrashMode (the post-mode lockout, re-armed)
//   0x82376840      sth -1, +284508  == muShowtimeRequestedTrafficIndex (K_INVALID_VEHICLE_INDEX)
//   0x82376848  stb 0, this+46620 ; std 0, this+46448   == DriveThroughsCanNowOpenAgain()
// ==============================================================================================
void GameStateModule::OnModeEnd(bool lbResetState)
{
    // Embedded by value on the console (+0x500 / +0x570: no test before either `bl`, 0x823767F4 /
    // 0x823767FC); held by pointer on this build and never null here, so no test here either
    // [FX-FLOW 2026-09-24, review D]: ConstructTakedownBringUp -- called from GameStateModule::
    // Construct -- assigns both from `new` and dereferences them on the spot (->Construct(this)),
    // nothing else ever writes (&mMugshotManager), and (&mPaybackManager) is cleared only by
    // GameStateModule::Destruct, which has no ModeManager leg left to end a mode. OnModeEnd's one
    // caller is ModeManager::SendModeStopMessages, i.e. a running game.
    mMugshotManager.OnRoundEnd(lbResetState);
    mPaybackManager.OnRoundEnd(lbResetState);

    // [diag] BRN_MODEMGR_DIAG -- NOT IN THE X360 BINARY: the round-end hand-off.

    ClearTakedownRaceCarData();

    const GameStateModuleIO::EGameModeType leGameModeType = GetCurrentGameModeType();
    if (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_SHOWTIME ||
        leGameModeType == GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
    {
        mShowtimePendingTrafficIndexStack.Clear();
        mfTimeSinceLastCrashMode        = 2.0f;
        muShowtimeRequestedTrafficIndex = 0xFFFFu;
    }

    mDriveThruManager.DriveThroughsCanNowOpenAgain();
}

// ==============================================================================================
// GameStateModule::CopyInputDataToPaybackManager  (X360 0x8239AA78; DWARF BrnGameStateModule.h:835)
// [FX-FLOW 2026-09-24, NEW-PAYBACK-WIRING] Called by PreWorldUpdate @0x823A5328 at 0x823A572C, once
// per pre-world tick, unconditionally, right after DriveThruManager::Update. Both callees are
// inlined on the console:
//   0x8239AA98  PreWorldInputBuffer::GetTimerStatusInterface (0x8231CE28)
//   0x8239AAA0..0x8239AB0C  the 48-byte copy into PaybackManager +0x00  == SetTimerInterface
//   0x8239AB10  PreWorldInputBuffer::GetControllerInput (0x823632F8)
//   0x8239AB14..0x8239AB58  `lbz 0xC` (mbDirtyTrickPressed) -> +0x264/+0x265 and the press-edge
//                           ChangeState(5)                                 == SetDirtyTrickButtonState
// Without it the payback manager's timer copy never left Construct's Clear(): its aggressor timer
// never advanced and the OnRoundStart / OnRoundEnd reseed read frame count 0; and the dirty-trick
// button never reached the aggressor FSM.
// ==============================================================================================
void GameStateModule::CopyInputDataToPaybackManager(
        const GameStateModuleIO::PreWorldInputBuffer* lpPreWorldInputBuffer)
{
    // Embedded by value on the console (this + 0x570, used with no test); a pointer on this build,
    // never null here (see OnModeEnd above: set from `new` in GameStateModule::Construct's
    // ConstructTakedownBringUp, cleared only by Destruct) -- the one caller is the pre-world tick.

    mPaybackManager.SetTimerInterface(lpPreWorldInputBuffer->GetTimerStatusInterface());
    mPaybackManager.SetDirtyTrickButtonState(
        lpPreWorldInputBuffer->GetControllerInput()->mbDirtyTrickPressed);

    // [diag] BRN_MODEMGR_DIAG -- NOT IN THE X360 BINARY. One line, the first time a running game
    // timer arrives: the witness that the copy is dispatched and carries a live frame count.
    static bool       sbReported    = false;
    const CgsSystem::TimerStatus* const lpGameTimer =
        lpPreWorldInputBuffer->GetTimerStatusInterface()->GetGameTimerStatus();
}

}
