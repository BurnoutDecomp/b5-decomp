// b5-decomp/src/GameSource/GameState/GameStateModule_gSR_00.cpp
//
// Partfile of the BrnGameState::GameStateModule TU (owning header BrnGameStateModule.h; the
// module's other committed bodies live in BrnGameStateModule.cpp and GameStateModule_gUI_00.cpp).
//
// ============================================================================================
// STUNT-RACES WAVE D, AGENT D3 -- THE FOUR OFFLINE EVENT-START FUNCTIONS.
//
// The event CORE was already mounted and linking before this file existed (ModeManager spine,
// fifteen modes, the mode states, the scoring stack, the StuntAttackMode trio, the traffic
// junction leaves). What was missing was everything that DETECTS the junction and DRIVES a
// start. These four are that, in the console's own call order -- all four are reached from
// GameStateModule::PreWorldUpdate @0x823A5328, which is itself not reconstructed:
//
//   1. CheckIfPlayerIsAtJunctionWithAnEvent  @0x82390418
//        TriggerQueryManager::GetPlayerCurrentTrafficLightId
//          -> TrafficData::GetJunctionLogicBoxForTrafficLight
//          -> JunctionLogicBox::GetEventJunctionID
//          -> ProgressionData's EventJunction table -> EventJunction::GetOfflineEvent
//          -> POSTS GAME ACTION 201 (E_ACTION_EVENT_AT_JUNCTION_AVAILABLE, 40 bytes)
//      * ACTION 201 IS THE VISIBLE ORACLE FOR THE WHOLE WAVE. The consumer half is already
//      mounted and live: GameBridgeGameStateToX_EventFlowGuiEvents.cpp's case-201 arm repacks it
//      into BrnGui::GuiEventJunctionInfo (id 311), FBurnMainHudState dispatches 311, and
//      JunctionInfoComponent::HandleJunctionChange draws the "hold accelerator + brake" banner.
//      Every field written below is matched FIELD FOR FIELD against that arm.
//
//   2. DetectModeStarts                      @0x8239A428   -- the arm; calls 3 then 4.
//   3. ShouldStartSnapRaceMode                @0x82363700  -- the 0.35 s accel+brake hold gate.
//   4. StartModeAtLights                      @0x82396CF8  -- builds the StartGameModeParams and
//                                                             calls ModeManager::StartGameMode.
//
// THE PSEUDOCODE OF ALL FOUR IS UNSAFE AND IS NOT THE SOURCE FOR ANYTHING HERE. Measured this
// pass, each defect called out again at its site:
//   (a) 0x82390418 opens with "local variable allocation has failed"; its whole 40-byte action
//       frame renders as thirteen unrelated locals (v91..v103) whose ORDER in the record is only
//       recoverable from the stack offsets in the asm.
//   (b) 0x82363700's prototype carries a phantom `int a4` -- the PPC float argument (the game
//       timestep, f1) consumes the r5 GPR slot, so the out-parameter is r6.
//   (c) 0x8239A428 renders `ShouldStartSnapRaceMode(this, byte, dt)` with the out-parameter
//       dropped, and `StartModeAtLights(this, in, out)` with the START MECHANISM dropped -- the
//       one argument StartModeAtLights early-returns on.
//   (d) 0x82396CF8 renders `GetOriginalCarId(v8[71094], v8[71095])` for a single `ldx r4, r25,
//       0x456D8` (one CgsID split into two dwords), and `*(v21 + 20)` for the `ld r11, 0x10`
//       that loads RaceEventData::mSpecialEventCarId whole.
//
// TWO CLASSES OF NULL GUARD ARE ADDED, AND THEY ARE THE ONLY BEHAVIOURAL ADDITIONS IN THIS FILE.
// The console asserts `lpRaceEvent` / `lpEventData` on a failed EventJunction lookup and then
// FALLS THROUGH INTO THE DEREFERENCE ANYWAY (0x82390418's LABEL_24 -> `*(v9 + 16)` with v9 == 0,
// and 0x82396CF8's LABEL_25 -> LABEL_26 with v21 == 0); the same shape repeats on every
// "lp<thing>" assert in both bodies. In a retail build, where FireAssert is inert, that is a
// crash. The asserts are reproduced verbatim -- they are the diagnostic -- and an early return
// is added behind each, marked [GUARD] at the site.
// ============================================================================================
#include "GameSource/GameState/BrnGameStateModule.h"

#include <stdlib.h>                                                     // getenv (the diag rungs)

#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"              // gpDebugPrint / gxMessageFilterFlags
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"        // VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/System/Timer/CgsTimerRequestInterface.h" // CgsSystem::TimerRequests (the showtime arm's third argument)
#include "rw/math/vpu/vector3_operation.h"                              // Dot (the showtime arm's two vmsum3fp128)

#include "GameSource/GameState/BrnGameStateModuleIO.h"                  // PreWorldInputBuffer / OutputBuffer / ControllerInput
#include "GameSource/GameState/BrnGameActions.h"                        // JunctionInfoAction / WrongCarForChallengeAction
#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"
#include "GameSource/GameState/TrainingManager/BrnTrainingManager.h"    // GetCurrentTrainingType
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/Progression/BrnProfile.h"
#include "GameSource/GameState/ModeManager/BrnModeManager.h"
#include "GameSource/GameState/ModeManager/GameModes/BrnGameMode.h"     // GameMode::IsOnline
#include "GameSource/GameState/ModeManager/GameModes/BrnGameModeParams.h" // StartGameModeParams

#include "SharedClasses/Progression/BrnProgressionData.h"               // ProgressionData / the junction table
#include "SharedClasses/Progression/BrnRaceEventData.h"                 // RaceEventData / EventJunction
#include "SharedClasses/Progression/BrnProgressionRankData.h"           // ProgressionRankData (SetProgressionRankData)
#include "SharedClasses/Progression/BrnTrainingTypes.h"                 // E_TRAINING_TYPE_* (the seen-tip marks)
#include "SharedClasses/Traffic/BrnTrafficDataResourceType.h"           // TrafficData::GetJunctionLogicBoxForTrafficLight
#include "SharedClasses/Traffic/Junctions/BrnJunctionLogicBox.h"        // JunctionLogicBox::GetID / GetEventJunctionID
#include "SharedClasses/Traffic/BrnTrafficLightTrigger.h"               // LightTriggerStartData::GetStartDirection
#include "SharedClasses/DataLists/VehicleList.h"                        // VehicleList::GetVehicleData(CgsID)
#include "SharedClasses/DataLists/VehicleListEntry.h"                   // BrnResource::VehicleListEntry

#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h"
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h" // RaceCarState::mfSpeedMPH (@972)

namespace BrnGameState
{
namespace
{

// --------------------------------------------------------------------------------------------
// The EventJunction lookup both CheckIfPlayerIsAtJunctionWithAnEvent and StartModeAtLights
// open-code, THREE times between them. The console form, verbatim (0x82390A20..0x82390A48 is
// the clearest of the three):
//     count = *(progressionData + 28);          // muEventJunctionCount
//     table = *(progressionData + 24);          // mpaEventJunctions, 16-byte stride
//     for (i = 0; i < count; ++i)
//         if (table[i].muID == luEventJunctionID) return &table[i];
//     return NULL;                              // then the caller's own assert fires
// Reproduced through ProgressionData::GetEventJunctionCount / GetEventJunction so no body here
// touches the +0x18 / +0x1C words directly.
// --------------------------------------------------------------------------------------------
const BrnProgression::EventJunction* FindEventJunctionById(
        const BrnProgression::ProgressionData* lpProgressionData, u32 luEventJunctionID)
{
    if (lpProgressionData == 0)
    {
        return 0;
    }
    const u32 luCount = lpProgressionData->GetEventJunctionCount();
    for (u32 lu = 0; lu < luCount; ++lu)
    {
        const BrnProgression::EventJunction* lpJunction = lpProgressionData->GetEventJunction(lu);
        if (lpJunction != 0 && lpJunction->GetID() == luEventJunctionID)
        {
            return lpJunction;
        }
    }
    return 0;
}

// --------------------------------------------------------------------------------------------
// "A BLOCKING training tip is on screen right now."
// The console open-codes this at THREE sites in this wave's chain, always identically
// (@0x8236382C-5C / @0x82390900-24 / @0x82390D68-8C, r11 = gsm+46640, the EMBEDDED
// TrainingManager):
//     lwz r10, 0(r11); cmpwi 0;    beq -> false     <- meTrainingState (+0x00) != INACTIVE
//     lwz r11, 4(r11); cmpwi 0x4D; blt -> true      <- meCurrentTrainingType (+0x04) < 77
// BOTH loads are TrainingManager MEMBERS -- `*(gsm+46640)` is meTrainingState, NOT a pointer
// null test (the 2026-08-26 verify caught that misread: the type idles at -1, signed, so the
// state term is load-bearing -- without it this returned TRUE on every idle frame and killed
// the gesture hold + forced canEnter=0 at every junction).
// 77 is BrnProgression::E_TRAINING_TYPE_NOT_TIMED_COUNT (below = MODAL tip; at/above = ambient
// timed). Sites:
//     ShouldStartSnapRaceMode              @0x82363700 (the pre-gate)
//     CheckIfPlayerIsAtJunctionWithAnEvent @0x82390418 twice (mbCanEnterEventAtJunction, and the
//                                                     action's own mbCanEnterEvent at LABEL_118)
// De-inlined over the named accessors: IsTipPending() is bodied as exactly
// `meTrainingState != E_TRAINING_STATE_INACTIVE` (BrnTrainingManager.cpp:808). The manager is a
// POINTER on this build (documented include-cycle deviation) -- the null guard is a HOST guard,
// not the console's first term.
// --------------------------------------------------------------------------------------------
bool IsBlockingTrainingTipActive(const TrainingManager* lpTrainingManager)
{
    if (lpTrainingManager == 0)
    {
        return false;
    }
    return lpTrainingManager->IsTipPending() &&
           lpTrainingManager->GetCurrentTrainingType() <
               BrnProgression::E_TRAINING_TYPE_NOT_TIMED_COUNT;
}

// --------------------------------------------------------------------------------------------
// LightTriggerId::IsValid(), as CheckIfPlayerIsAtJunctionWithAnEvent's DEPARTURE arm inlines it
// on the CACHED handle (@0x82390DEC..0x82390E10):
//     rlwinm r9, r11, 0,8,23   ; id & 0x00FFFF00   -- the hull field, bits 8..23
//     cmplw  r9, 0xFFFF00      ; hull == 0xFFFF ?  -> invalid
//     clrlwi r11, r11, 24      ; id & 0xFF         -- the light-trigger index, bits 0..7
//     cmplwi r11, 0xFF         ; index == 0xFF ?   -> invalid
// Identical to the predicate TrafficData::GetJunctionLogicBoxForTrafficLight asserts and to
// BrnModeManager_IntroPlay.cpp's file-local IsLightTriggerIdValid. Note the owner TAG in bits
// 24..31 (0x39 == traffic-light trigger) is NOT masked off by either test -- the handle stays
// packed exactly as TrafficEntityModule::ManageTriggers built it.
// --------------------------------------------------------------------------------------------
bool IsLightTriggerIdValid(u32 luLightTriggerId)
{
    return ((luLightTriggerId & 0x00FFFF00u) != 0x00FFFF00u) &&
           ((luLightTriggerId & 0x000000FFu) != 0x000000FFu);
}

// [DIAG] the `[snap]` rung -- NOT IN THE X360 BINARY. Same logger and same env guard
// (BRN_PROP_DIAG) as the `[UI-gate]` / `[evt-flow]` ladders elsewhere in this chain.
bool SnapDiagEnabled()
{
    static const bool sbDiag = (getenv("BRN_PROP_DIAG") != 0);
    return sbDiag && CgsDev::Log::gpDebugPrint != 0;
}

// [DIAG] the `[showtime]` rung -- NOT IN THE X360 BINARY, same guard and same logger.
bool ShowtimeDiagEnabled()
{
    static const bool sbDiag = (getenv("BRN_PROP_DIAG") != 0);
    return sbDiag && CgsDev::Log::gpDebugPrint != 0;
}

// ---- the showtime arm's three console literals, all image-read out of the ARTIST .rdata -----
// flt_82CDB8CC == 0x41200000 == 10.0f. The arm squares it (`fmuls f30, f0, f0` @0x8239A60C) and
// compares the squared speed against the square, so this is the minimum speed in the linear
// velocity's own units (m/s -- RaceCarState::mLinearVelocity is raw, see GetPlayerLinearVelocity's
// banner), i.e. about 22 mph.
const f32 KF_SHOWTIME_MIN_SPEED = 10.0f;
// flt_82CDB8D0 == 0x3F000000 == 0.5f. The showtime-intro window.
const f32 KF_SHOWTIME_INTRO_SECONDS = 0.5f;
// flt_82004884 == 0x3727C5AC == 9.99999975e-06f. Not a "nearly zero" placeholder: it is a
// deliberate hair above zero, so IsInShowtimeIntro() stays true and the next frame re-enters the
// decision arm. That is how the console waits for a car that is still airborne to land.
const f32 KF_SHOWTIME_INTRO_RETRY_SECONDS = 9.99999975e-06f;

// --------------------------------------------------------------------------------------------
// [FLAG PC bring-up, NOT in the X360 binary] BRN_SKIP_TRAINING_TIP -- the harness tip bypass.
//
// The boot-time tutorial tip ("Find an Auto-Repair shop...") is a MODAL type (< 77), so
// IsBlockingTrainingTipActive above is TRUE for it, and it holds the junction canEnter gate down
// for minutes of real time. It does clear on its own -- this is not a defect and this flag is not
// a fix for one -- but a harness run is 275 s long and spends most of that boot-time window
// standing at a junction with the start-hint glyphs suppressed, so the hint chain cannot be
// exercised at all under the harness.
//
// OPT IN ONLY: OFF unless the environment carries BRN_SKIP_TRAINING_TIP, cached once in the same
// `static const bool` shape as SnapDiagEnabled's BRN_PROP_DIAG read, so a retail-shaped run
// (no env) is bit-identical to the console gate.
//
// ⛔ SCOPE. This wrapper is called from the TWO canEnter computations in
// CheckIfPlayerIsAtJunctionWithAnEvent ONLY (mbCanEnterEventAtJunction and the action record's
// own mbCanEnterEvent). IsBlockingTrainingTipActive itself is NOT modified, because its third
// caller -- ShouldStartSnapRaceMode's pre-gate @0x82363700 -- is the gesture-hold arm, and that
// one must keep the console's behaviour: a bypass there would let a harness run actually START a
// mode out of a tip-blocked state, which is a capability -StartEvent already owns explicitly.
//
// DELETE-WHEN the training flow's tip lifecycle is fully reconstructed and fast-forwardable
// (i.e. TrainingManager's RequestTip / GetTimeSinceLastTip / tip-expiry path is bodied and the
// harness can retire or complete a pending tip directly, so nothing needs to ignore one).
// --------------------------------------------------------------------------------------------
bool IsBlockingTrainingTipActiveForCanEnterGate(const TrainingManager* lpTrainingManager)
{
    const bool lbBlocking = IsBlockingTrainingTipActive(lpTrainingManager);
    if (!lbBlocking)
    {
        return false;
    }

    static const bool sbSkipTip = (getenv("BRN_SKIP_TRAINING_TIP") != 0);
    if (!sbSkipTip)
    {
        return true;
    }

    // One shot, and only when the flag CHANGED the answer -- so a log proves the bypass did
    // something rather than merely that the variable was set.
    if (SnapDiagEnabled())
    {
        static bool sbLoggedSuppression = false;
        if (!sbLoggedSuppression)
        {
            sbLoggedSuppression = true;
            *CgsDev::Log::gpDebugPrint
                << "[snap] BRN_SKIP_TRAINING_TIP -- blocking training tip (type "
                << static_cast<s32>(lpTrainingManager->GetCurrentTrainingType())
                << ") IGNORED for the junction canEnter gate."
                << " [FLAG PC bring-up, NOT in the X360 binary]\n";
        }
    }
    return false;
}

}   // anonymous namespace

// ============================================================================================
// GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent -- X360 0x82390418
//   (BrnGameStateModule.cpp:6925..7135; every assert line quoted below is that file's)
// ============================================================================================
// THE ACTION-201 RECORD, and how its twelve fields were recovered. Hex-Rays gives the stores as
// thirteen unrelated locals; the frame is `var_C0` at sp+0x70 and AddEvent is called with
// `addi r4, r1, 0x130+var_C0` (@0x82390DD4 and @0x82390EB8), `li r5, 0xC9` (201) and
// `li r6, 0x28` (40). Subtracting the base from each store offset gives the layout:
//     var_C0  +0x00 muJunctionLogicBoxId        var_A4  +0x1C mi8Difficulty
//     var_BC  +0x04 muEventJunctionID           var_A3  +0x1D mi8MedalAchieved
//     var_B8  +0x08 muLightTriggerId            var_A2  +0x1E mbOnEntry
//     var_B0  +0x10 mSpecialEventCarId (std)    var_A1  +0x1F mbCanEnterEvent
//     var_A8  +0x18 meGameModeType              var_A0  +0x20 mbEventUnlocked
//                                               var_9F  +0x21 mbSpecificCarEventValid
//                                               var_9E  +0x22 mbIsNewlyDiscovered
//                                               var_9D  +0x23 mbIsAutoUnlockedChallenge
// which is exactly the committed GameStateModuleIO::JunctionInfoAction (BrnGameActions.h:1287)
// and exactly the eleven loads the mounted consumer arm emits at 0x823EA810.
//
// A REAL CONSOLE DEFECT, RECORDED RATHER THAN REPRODUCED: mi8Difficulty (+0x1C) IS NEVER STORED
// ON THE EVENT POST. A sweep of the whole export for `var_A4` finds exactly two stores, both
// `-1`, and both on paths that carry NO event (@0x82390DBC, the "this junction has no event"
// post, and @0x82390E94, the departure post). The event arm at 0x82390AD4..0x82390B18 stores
// meGameModeType, mi8MedalAchieved, mbEventUnlocked and mbSpecificCarEventValid and leaves
// +0x1C alone -- so the console ships whatever was on its stack there, and the GUI's
// GuiEventJunctionInfo::mi8Difficulty reads uninitialised memory on every junction arrival. The
// record here is VALUE-INITIALISED, so this build sends a stable 0 instead of stack garbage.
// That is a divergence and it is stated rather than hidden; it cannot be "fixed" into the right
// value because the console never computed one.
//
// TWO ARMS ARE PARKED, both marked [PARKED] at their sites with the console's own body:
//   * the DISCOVERY arm (0x823905C8..0x823907E8) -- ProfileEvent flag bit 0, action 55,
//     Profile::AddGameModeTypeToDiscovered, CheckForAllEventsOfATypeFound /
//     CheckForAllEventsBeingFound. Parked by the wave brief.
//   * the TRAINING-TIP arm (0x82390B18..0x82390D5C) -- the three-way DISCOVERS_EVENT /
//     CORRECT_CAR / WRONG_CAR tip choice.
// Neither parked arm posts an action the mounted GUI consumes; the action-201 post, which does,
// is complete.
// ============================================================================================
void GameStateModule::CheckIfPlayerIsAtJunctionWithAnEvent(
        const GameStateModuleIO::PreWorldInputBuffer* lpInput,
        GameStateModuleIO::OutputBuffer*             lpOutput)
{
    if (lpInput == 0 || lpOutput == 0)
    {
        return;   // [GUARD] -- the console takes both non-null from PreWorldUpdate.
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrInterface =
        mLastActiveRaceCarInterface;

    // @0x82390440-ish: the interface's own bounds assert, fired on ITS index word (+10328) with
    // the interface header's file/line -- BrnRaceCarEntityModuleOutputInterface.h:967.
    CGS_ASSERT(lrInterface.GetPlayerActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    // `v12 = (idx != -1) ? mbIsPlayerCarActive : 0; if (!v12) return;` -- the whole body is
    // behind a live player car.
    const bool lbPlayerCarActive =
        (lrInterface.GetPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
        lrInterface.IsPlayerCarActive();
    if (!lbPlayerCarActive)
    {
        return;
    }

    // `GetMemor(this + 43920, ...)` @0x8239055C -- 43920 == mTriggerQueryManager (this+42320)
    // + 0x640, i.e. the inlined TriggerQueryManager::GetTrafficData().
    const BrnTraffic::TrafficData* lpTrafficData = mTriggerQueryManager.GetTrafficData();
    CGS_ASSERT(lpTrafficData != 0, "lpTrafficData");                              // :6925
    if (lpTrafficData == 0)
    {
        return;   // [GUARD] -- the console dereferences it below; see the file banner.
    }

    CGS_ASSERT(lrInterface.GetPlayerActiveRaceCarIndex() != E_ACTIVE_RACE_CAR_INDEX_INVALID,
               "Player car index hasn't been set");    // interface header :980
    const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState =
        lrInterface.GetRaceCarState(lrInterface.GetPlayerActiveRaceCarIndex());
    CGS_ASSERT(lpPlayerRaceCarState != 0, "lpPlayerRaceCarState");                // :6929
    if (lpPlayerRaceCarState == 0)
    {
        return;   // [GUARD]
    }

    // ---- 1) the "which event is this" pre-pass ---------------------------------------------
    // mbSpecificCarEventValid: the junction's event demands a specific car AND the player is in
    // it (`*&v22 = *(lpRaceEvent + 16)` == mSpecialEventCarId, compared against
    // GetOriginalCarId(mActivePlayerCarId) -- @0x82390618-ish).
    bool lbSpecificCarEventValid = false;

    if (mTriggerQueryManager.IsPlayerInTrafficLightRegion())
    {
        const LightTriggerId lLightTriggerId = mTriggerQueryManager.GetPlayerCurrentTrafficLightId();
        const BrnTraffic::JunctionLogicBox* lpJunctionLogicBox =
            lpTrafficData->GetJunctionLogicBoxForTrafficLight(static_cast<u32>(lLightTriggerId));
        CGS_ASSERT(lpJunctionLogicBox != 0, "lpJunctionLogicBox");                // :6941

        if (lpJunctionLogicBox != 0)   // [GUARD]
        {
            const u32 luEventJunctionID = lpJunctionLogicBox->GetEventJunctionID();
            const BrnProgression::ProgressionData* lpProgressionData =
                mProgressionManager.GetProgressionData();
            const BrnProgression::EventJunction* lpEventJunction =
                FindEventJunctionById(lpProgressionData, luEventJunctionID);
            const BrnProgression::RaceEventData* lpRaceEvent =
                (lpEventJunction != 0) ? lpEventJunction->GetOfflineEvent() : 0;
            CGS_ASSERT(lpRaceEvent != 0, "lpRaceEvent");                          // :6944

            if (lpRaceEvent != 0)   // [GUARD]
            {
                const CgsID lSpecialEventCarId = lpRaceEvent->GetSpecialEventCarId();
                lbSpecificCarEventValid =
                    (lSpecialEventCarId != 0) &&
                    (GetOriginalCarId(mActivePlayerCarId) == lSpecialEventCarId);
            }

            // ⭐ THE DISCOVERY ARM (0x823905C8..0x823907E8) -- LANDED 2026-08-27 (D1
            // profile-event-list wave). It was parked on two things, and both are now gone: the
            // ProfileEvent flag-bit mutator this tree did not model (ProfileEvent::EnableFlags,
            // DWARF BrnProfile.h:328, now bodied in BrnProfile.cpp) and the fact that the profile
            // held NO event records at all, so the ":6957 lpProfileEvent" assert would have fired
            // on every single junction arrival. ProgressionManager::UnlockToProgressionRank
            // @0x8239DDE8 now populates the table at boot, so the lookup below finds its record.
            //
            // WHY THIS ARM IS LOAD-BEARING AND NOT COSMETIC: E_FLAG_DISCOVERED (bit 0) is what
            // ProgressionManager::OnEventFinishUpdateProfile @0x823A0040 asserts on before it
            // writes the win flags ("lpEvent->IsFlagSet( ProfileEvent::E_FLAG_DISCOVERED )",
            // BrnProgressionManager.cpp:1672). Standing at the junction is what sets it, and
            // standing at the junction is exactly how the player starts the event they then win.
            //
            // The gate is the console's: `!mbJunctionNewlyDiscovered && !mpCurrentGameMode &&
            // !IsOnlineGameMode()` (@0x82390684..0x823906AC).
            // (`lpRaceEvent != 0` is this file's standing [GUARD] class, not a new one: the
            // console asserts it at :6944 above and then dereferences it here regardless --
            // see the file banner's "TWO CLASSES OF NULL GUARD" note.)
            if (lpRaceEvent != 0 &&
                !mbJunctionNewlyDiscovered &&
                mModeManager.GetCurrentGameMode() == 0 &&
                !IsOnlineGameMode())
            {
                BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();

                // @0x823906B0..0x8239071C: the open-coded scan of maEvents for the junction id,
                // then the ":6957 lpProfileEvent" assert on a miss. Routed through the named
                // finder (Profile::FindEvent, DWARF BrnProfile.h:713).
                BrnProgression::ProfileEvent* lpProfileEvent =
                    (lpProfile != 0) ? lpProfile->FindEvent(luEventJunctionID) : 0;
                CGS_ASSERT(lpProfileEvent != 0, "lpProfileEvent");                 // :6957

                // `lhz r11, 4(r30) / clrlwi 31 / bne` -- already discovered, nothing to do.
                if (lpProfileEvent != 0 && !lpProfileEvent->IsFlagSet(
                        BrnProgression::ProfileEvent::E_FLAG_DISCOVERED))
                {
                    CGS_ASSERT(lpProfile != 0, "lpProfile");                       // :6962

                    // @0x82390750..0x82390760: `lhz / ori 1 / sth`.
                    lpProfileEvent->EnableFlags(
                        static_cast<u16>(BrnProgression::ProfileEvent::E_FLAG_DISCOVERED));

                    // @0x82390764..0x82390780: the SAME data-mode -> runtime-mode hop the
                    // action-201 arm below makes, including the +180972 override word whose
                    // retail value is the 6 sentinel (see that arm's FLAG for the full cite).
                    lpProfile->AddGameModeTypeToDiscovered(
                        static_cast<GameStateModuleIO::EGameModeType>(
                            mProgressionManager.GetEvent(static_cast<s32>(lpRaceEvent->GetMode()))));

                    // @0x82390798: the latch the action-201 record reads as mbIsNewlyDiscovered
                    // (and which the post below clears again).
                    mbJunctionNewlyDiscovered = true;

                    // @0x8239079C..0x823907AC: `li r11,0 / stb / AddEvent(queue, &byte, 0x37, 1)`.
                    // NOTE the payload byte is ZERO here -- OnEventFinishUpdateProfile's copy of
                    // the same action posts 1. Reproduced as written.
                    const u8 lu8AutoSavePayload = 0;
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lu8AutoSavePayload),
                        GameStateModuleIO::E_ACTION_REQUEST_AUTOSAVE,
                        static_cast<s32>(sizeof(lu8AutoSavePayload)));

                    // ⛔ [PARKED] the arm's last two calls, @0x823907B0..0x823907E8:
                    //     CheckForAllEventsOfATypeFound(profile, queue, lpRaceEvent->GetMode());
                    //     CheckForAllEventsBeingFound(profile, queue);
                    // GameStateModule::CheckForAllEventsBeingFound @0x82382460 is DECLARE-ONLY in
                    // this tree (BrnGameStateModule.h:1027, grown for the DriveThruManager TU and
                    // never bodied) and CheckForAllEventsOfATypeFound @0x823822C8 has no
                    // declaration at all. Both are "have you now found every event / every event
                    // of this type" trophy checks; neither feeds the action-201 record. Landing
                    // the calls would add two unresolved externals to a MOUNTED TU.
                }
            }
        }
    }
    else
    {
        // @0x823907EC: leaving every traffic-light region drops the "arrival posted" latch.
        mbAtJunctionWithEvent = false;
    }

    // ---- 2) the post gate -------------------------------------------------------------------
    // @0x823907F0..0x8239086C, verbatim:
    //     show = mbJunctionNewlyDiscovered
    //         || !lpInput->GetControllerInput()->mbAcceleratePressed   (lbz 0x12(ctrl))
    //         || playerState->mfSpeedMPH < 40.0f                       (flt_82004D0C, image-read)
    //         || lbSpecificCarEventValid;
    //     if (IsPlayerInTrafficLightRegion() && (show || mbAtJunctionWithEvent)) -> arrival post
    //     else                                                                   -> departure post
    // i.e. the banner is SUPPRESSED only while the player blasts through a junction with the
    // throttle down in a car the event does not care about -- and never once an arrival has
    // already been posted for the junction they are still standing in.
    const GameStateModuleIO::ControllerInput* lpControllerInput = lpInput->GetControllerInput();
    const bool lbShowJunctionInfo =
        mbJunctionNewlyDiscovered ||
        (lpControllerInput == 0) || !lpControllerInput->mbAcceleratePressed ||
        (lpPlayerRaceCarState->mfSpeedMPH < 40.0f) ||
        lbSpecificCarEventValid;

    if (mTriggerQueryManager.IsPlayerInTrafficLightRegion() &&
        (lbShowJunctionInfo || mbAtJunctionWithEvent))
    {
        // ---- THE ARRIVAL POST (0x82390870..0x82390DDC) --------------------------------------
        const LightTriggerId lLightTriggerId = mTriggerQueryManager.GetPlayerCurrentTrafficLightId();
        const BrnTraffic::JunctionLogicBox* lpJunctionLogicBox =
            lpTrafficData->GetJunctionLogicBoxForTrafficLight(static_cast<u32>(lLightTriggerId));
        CGS_ASSERT(lpJunctionLogicBox != 0, "lpJunctionLogicBox");                // :7002
        if (lpJunctionLogicBox == 0)
        {
            return;   // [GUARD]
        }

        const BrnPhysics::Vehicle::RaceCarState* lpState =
            lrInterface.GetRaceCarState(lrInterface.GetPlayerActiveRaceCarIndex());
        CGS_ASSERT(lpState != 0, "lpPlayerRaceCarState");                         // :7005
        if (lpState == 0)
        {
            return;   // [GUARD]
        }

        // mbCanEnterEventAtJunction (gsm+284369), @0x82390904..0x82390984: no game mode running,
        // no blocking training tip, and at or below 30 mph (flt_82029F30, image-read).
        // [FLAG PC bring-up, NOT in the X360 binary] the tip term goes through the
        // BRN_SKIP_TRAINING_TIP wrapper (banner at its definition above). With the env var unset
        // -- every retail-shaped run -- it is IsBlockingTrainingTipActive verbatim.
        bool lbCanEnterEvent = false;
        if (mModeManager.GetCurrentGameMode() == 0)
        {
            lbCanEnterEvent = !IsBlockingTrainingTipActiveForCanEnterGate(mpTrainingManager) &&
                              (lpState->mfSpeedMPH <= 30.0f);
        }

        GameStateModuleIO::JunctionInfoAction lAction = GameStateModuleIO::JunctionInfoAction();

        // The console's store ORDER matters in exactly two places: mbIsNewlyDiscovered is read
        // (`lbz r11, 0(r17)` @0x82390964) BEFORE the post clears it, and the cached box id is
        // read back out of the member AFTER being written (`stw` then `lwz`, @0x82390998/9C).
        lAction.mbIsNewlyDiscovered = mbJunctionNewlyDiscovered;
        mbAtJunctionWithEvent          = true;                                    // @0x8239096C
        muCachedJunctionLightTriggerId = static_cast<u32>(lLightTriggerId);        // @0x82390974
        mbCanEnterEventAtJunction      = lbCanEnterEvent;                          // @0x82390984
        lAction.mbOnEntry                 = true;                                  // @0x8239098C
        lAction.mbIsAutoUnlockedChallenge = false;                                 // @0x82390990

        const u32 luPreviousJunctionLogicBoxId = muCachedJunctionLogicBoxId;       // @0x82390994
        muCachedJunctionLogicBoxId            = lpJunctionLogicBox->GetID();       // @0x82390998
        lAction.muJunctionLogicBoxId          = muCachedJunctionLogicBoxId;        // @0x823909B0
        lAction.muLightTriggerId              = static_cast<u32>(lLightTriggerId);  // @0x82390954
        const u32 luEventJunctionID           = lpJunctionLogicBox->GetEventJunctionID();
        lAction.muEventJunctionID             = luEventJunctionID;                 // @0x823909B8

        // "The junction under the car CHANGED this frame" (@0x823909A0..0x823909BC). Consumed
        // only by the PARKED training-tip arm below; computed here because it is the console's,
        // and so that landing that arm needs no re-derivation.
        const bool lbJunctionChanged =
            (muCachedJunctionLogicBoxId != luPreviousJunctionLogicBoxId);
        (void)lbJunctionChanged;

        if (static_cast<s32>(luEventJunctionID) <= 0)                              // @0x823909C0
        {
            // "This traffic light is not an event junction" (@0x82390DA8..0x82390DC0).
            lAction.mbEventUnlocked  = false;
            lAction.mbCanEnterEvent  = false;
            lAction.meGameModeType   = GameStateModuleIO::E_MODE_OFFLINE_COUNT;    // li r11, 0xA
            lAction.mi8Difficulty    = -1;
            lAction.mi8MedalAchieved = -1;
        }
        else
        {
            const BrnProgression::ProgressionData* lpProgressionData =
                mProgressionManager.GetProgressionData();
            CGS_ASSERT(lpProgressionData != 0, "lpProgressionData");               // :7048

            const BrnProgression::EventJunction* lpEventJunction =
                FindEventJunctionById(lpProgressionData, luEventJunctionID);
            const BrnProgression::RaceEventData* lpRaceEvent =
                (lpEventJunction != 0) ? lpEventJunction->GetOfflineEvent() : 0;
            CGS_ASSERT(lpRaceEvent != 0, "lpRaceEvent");                           // :7051

            BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
            CGS_ASSERT(lpProfile != 0, "lpProfile");                               // :7054

            if (lpRaceEvent == 0 || lpProfile == 0)
            {
                return;   // [GUARD] -- the console falls straight into the deref here.
            }

            // THE DATA MODE -> RUNTIME MODE HOP. The console reads a game-type word first and
            // only falls back to the event's own mode when it holds 6:
            //     v59 = *(gsm + 180972); if (v59 == 6) v59 = lpRaceEvent->mu8Mode;
            //     meGameModeType = ProgressionManager::GetEvent(v59);
            // (@0x82390AB8..0x82390AC8, and StartModeAtLights repeats it at 0x82396F10.)
            //
            // FLAG -- THE OVERRIDE WORD HAS NO MEMBER AND NO WRITER. gsm+180972 is
            // mProgressionManager + 133052, which lands inside that manager's mDebugComponent
            // slot (the ProgressionDebugComponent reserved at the X360 +133000 region); that
            // sub-layout is not modelled in this tree, and a sweep of the export set finds NO
            // function that writes the word at all -- CheckIfPlayerIsAtJunctionWithAnEvent and
            // StartModeAtLights are its only two appearances anywhere. Its retail value is the 6
            // sentinel ("no override"): RaceEventData::EModeType tops out at 5, so 6 is outside
            // the data enum, and 6 is the ONLY value for which the console consults the event.
            // Reproduced as the sentinel path taken unconditionally, with the console expression
            // written out above so the override can be restored the moment
            // ProgressionDebugComponent is reconstructed. DELETE-WHEN that happens.
            const s32 liDataGameType = static_cast<s32>(lpRaceEvent->GetMode());
            lAction.meGameModeType   = static_cast<GameStateModuleIO::EGameModeType>(
                                           mProgressionManager.GetEvent(liDataGameType));

            const CgsID lSpecialEventCarId  = lpRaceEvent->GetSpecialEventCarId();
            lAction.mSpecialEventCarId      = lSpecialEventCarId;                  // std @0x82390AE4
            lAction.mbSpecificCarEventValid = false;                               // @0x82390ADC
            lAction.mbEventUnlocked         = true;                                // @0x82390AE8
            lAction.mi8MedalAchieved        = static_cast<s8>(
                lpProfile->GetMedalAchievedForEventWithID(static_cast<s32>(luEventJunctionID)));

            // r28 is the console's "unlocked" flag; LABEL_118 gates on it as well as on the
            // member, which is why it is a separate local from the record field.
            bool lbEventUnlocked = true;
            if (lSpecialEventCarId != 0)                                           // @0x82390AF0
            {
                if (lbSpecificCarEventValid)
                {
                    lAction.mbSpecificCarEventValid = true;                        // @0x82390B08
                }
                else
                {
                    lbEventUnlocked         = false;
                    lAction.mbEventUnlocked = false;                               // @0x82390B14
                }
            }

            // [PARKED] THE TRAINING-TIP ARM (0x82390B18..0x82390D5C), entered only when
            // `lbJunctionChanged || TrainingManager::mbVoiceoverFinishedLastFrame` (gsm+46663).
            // Console body, written out for whoever lands it -- it is a clean three-way choice
            // and every id below is image-cited:
            //   * no special car          -> E_TRAINING_TYPE_DISCOVERS_EVENT (8)
            //   * special car, I am in it -> E_TRAINING_TYPE_CORRECT_CAR_FOR_CHALLENGE (24),
            //                                and mark tip 25 already-seen on the Profile
            //   * special car, I am not   -> E_TRAINING_TYPE_WRONG_CAR_FOR_CHALLENGE (25)
            //   each behind the same gauntlet: !TrainingManager::IsTipPending() &&
            //   !IsInPictureParadise() && ModeManager::GetTimeInFreeBurn() >= 30.0f (the first
            //   arm only) && IsTipAllowedInGameMode(type) &&
            //   !Profile::HasPlayerSeenTrainingType(type) && GetTimeSinceLastTip() >= 5.0f,
            //   then RequestTip(type).
            // Parked because it drives TrainingManager state through accessors that are
            // declare-only on this build (IsTipPending / GetProfile / GetTimeSinceLastTip /
            // RequestTip) and posts NO action -- nothing the mounted GUI reads depends on it.

            // LABEL_118 (@0x82390D5C..0x82390DA0): the record's OWN mbCanEnterEvent is the member
            // AND the unlocked flag AND "no blocking tip", recomputed here because the console
            // recomputes it (the member was latched before the event lookup ran).
            // [FLAG PC bring-up, NOT in the X360 binary] same BRN_SKIP_TRAINING_TIP wrapper as the
            // member gate above -- both halves of canEnter must agree or the record would say
            // "cannot enter" while the member said it could.
            bool lbActionCanEnterEvent = false;
            if (mbCanEnterEventAtJunction && lbEventUnlocked)
            {
                lbActionCanEnterEvent = !IsBlockingTrainingTipActiveForCanEnterGate(mpTrainingManager);
            }
            lAction.mbCanEnterEvent = lbActionCanEnterEvent;
        }

        lpOutput->GetGameActionQueue()->AddEvent(
            reinterpret_cast<const CgsModule::Event*>(&lAction),
            GameStateModuleIO::E_ACTION_EVENT_AT_JUNCTION_AVAILABLE,
            static_cast<s32>(sizeof(GameStateModuleIO::JunctionInfoAction)));      // li r5,0xC9 / li r6,0x28

        mbJunctionNewlyDiscovered = false;                                         // @0x82390DDC

        if (SnapDiagEnabled())
        {
            static s32 siArrivalLines = 0;
            if (siArrivalLines < 12)
            {
                ++siArrivalLines;
                *CgsDev::Log::gpDebugPrint
                    << "[snap] junction ENTER box=" << lAction.muJunctionLogicBoxId
                    << " event=" << lAction.muEventJunctionID
                    << " mode=" << static_cast<s32>(lAction.meGameModeType)
                    << " canEnter=" << (lAction.mbCanEnterEvent ? 1 : 0)
                    << " unlocked=" << (lAction.mbEventUnlocked ? 1 : 0)
                    << " speed=" << lpState->mfSpeedMPH << "\n";
            }
        }
        return;
    }

    // ---- THE DEPARTURE POST (0x82390DE8..0x82390ED0) ----------------------------------------
    // Only when the CACHED handle still names a junction -- i.e. exactly once per exit.
    if (!IsLightTriggerIdValid(muCachedJunctionLightTriggerId))
    {
        return;
    }

    const BrnTraffic::JunctionLogicBox* lpCachedBox =
        lpTrafficData->GetJunctionLogicBoxForTrafficLight(muCachedJunctionLightTriggerId);
    CGS_ASSERT(lpCachedBox != 0, "lpJunctionLogicBox");                            // :7127
    if (lpCachedBox == 0)
    {
        return;   // [GUARD]
    }

    GameStateModuleIO::JunctionInfoAction lAction = GameStateModuleIO::JunctionInfoAction();
    lAction.mbOnEntry                 = false;                                     // @0x82390E6C
    lAction.mbIsNewlyDiscovered       = false;                                     // @0x82390E74
    lAction.mbIsAutoUnlockedChallenge = false;                                      // @0x82390E78
    lAction.muEventJunctionID         = lpCachedBox->GetEventJunctionID();          // @0x82390E7C
    lAction.muLightTriggerId          = muCachedJunctionLightTriggerId;             // @0x82390E84
    lAction.meGameModeType            = GameStateModuleIO::E_MODE_OFFLINE_COUNT;    // @0x82390E8C
    lAction.mi8Difficulty             = -1;                                         // @0x82390E94
    lAction.mi8MedalAchieved          = -1;                                         // @0x82390E98
    lAction.muJunctionLogicBoxId      = muCachedJunctionLogicBoxId;                 // @0x82390EA0
    lAction.mbCanEnterEvent           = mbCanEnterEventAtJunction;                  // @0x82390EA8
    // (mbEventUnlocked / mbSpecificCarEventValid are not stored on this path either -- the same
    //  uninitialised-stack family as mi8Difficulty; value-initialised false here.)

    lpOutput->GetGameActionQueue()->AddEvent(
        reinterpret_cast<const CgsModule::Event*>(&lAction),
        GameStateModuleIO::E_ACTION_EVENT_AT_JUNCTION_AVAILABLE,
        static_cast<s32>(sizeof(GameStateModuleIO::JunctionInfoAction)));

    muCachedJunctionLightTriggerId = 0xFFFFFFFFu;                                  // @0x82390EC4
    muCachedJunctionLogicBoxId     = 0xFFFFFFFFu;                                  // @0x82390EC8
    mbCanEnterEventAtJunction      = false;                                        // @0x82390ECC
    mbJunctionNewlyDiscovered      = false;                                        // @0x82390ED0

    if (SnapDiagEnabled())
    {
        static s32 siExitLines = 0;
        if (siExitLines < 12)
        {
            ++siExitLines;
            *CgsDev::Log::gpDebugPrint
                << "[snap] junction LEAVE box=" << lAction.muJunctionLogicBoxId
                << " event=" << lAction.muEventJunctionID << "\n";
        }
    }
}

// ============================================================================================
// GameStateModule::ShouldStartSnapRaceMode -- X360 0x82363700
//   (BrnGameStateModule.cpp:5161..5210)
// ============================================================================================
// THE GESTURE IS ANALOGUE AND THE TREE USED TO MISNAME IT. lbRaceModePressed is
// PreWorldInputBuffer +0x45 == ControllerInput::mbRaceModePressed, which SetButtonPressed
// @0x823BA240 computes as `padAction[+0x00].value > 0.25f && padAction[+0x08].value > 0.25f` --
// the ACCELERATOR and BRAKE analogue axes, not the sticks and not the shoulder buttons.
//
// The gate, in the console's own order. Every bail arm RE-ARMS the timer to 0.35; the two arms
// at the bottom do NOT -- that asymmetry is the whole hold mechanic:
//     interface bounds assert
//     player car live?                     no  -> re-arm, false
//     crashing / no index / engine not RUNNING (mePlayerEngineState != 2)
//                                          yes -> re-arm, false
//     a game mode is running AND it is not E_MODE_ONLINE_FREE_BURN_LOBBY(15) /
//         E_MODE_ONLINE_SHOWTIME(16)           -> re-arm, false
//     not in a traffic-light region            -> re-arm, false
//     a blocking training tip is up            -> re-arm, false
//     gesture not held OR speed > 30 mph       -> re-arm, false
//     timer -= dt; still > 0, or the sim is paused -> false   (NO re-arm: the hold runs)
//     else                                     -> re-arm, write the mechanism, TRUE
//
// The final `*out = <in a light region> ? 2 : 1` is the console's, kept verbatim even though the
// region test five lines above already forces the 2 -- StartModeAtLights early-returns on
// anything but 2, so the branch is load-bearing documentation of the discriminant. The Hex-Rays
// form of that store is `*a5 = ((_cntlzw(v19) & 0x20) == 0) + 1`, i.e. (v19 != 0) + 1.
// ============================================================================================
bool GameStateModule::ShouldStartSnapRaceMode(bool                     lbRaceModePressed,
                                              f32                      lfGameTimestep,
                                              EGameModeStartMechanism* lpOutStartMechanism)
{
    CGS_ASSERT(lpOutStartMechanism != 0, "lpOutStartMechanism");                   // :5161
    if (lpOutStartMechanism == 0)
    {
        return false;   // [GUARD]
    }

    const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface& lrInterface =
        mLastActiveRaceCarInterface;

    CGS_ASSERT(lrInterface.GetPlayerActiveRaceCarIndex() < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");      // interface :967

    const ::EActiveRaceCarIndex lePlayerIndex = lrInterface.GetPlayerActiveRaceCarIndex();

    const bool lbPlayerCarActive = (lePlayerIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                                   lrInterface.IsPlayerCarActive();
    if (!lbPlayerCarActive)
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    if (lrInterface.IsPlayerCarCrashing() ||
        lePlayerIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID ||
        lrInterface.GetPlayerEngineState() !=
            BrnWorld::RaceCarEntityModuleIO::E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING)
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    if (mModeManager.GetCurrentGameMode() != 0)
    {
        const GameStateModuleIO::EGameModeType leType = mModeManager.GetCurrentGameModeType();
        if (leType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
            leType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            mfSnapRaceStartHoldSeconds = 0.35f;
            return false;
        }
    }

    if (!mTriggerQueryManager.IsPlayerInTrafficLightRegion())
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    if (IsBlockingTrainingTipActive(mpTrainingManager))
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState =
        lrInterface.GetRaceCarState(lrInterface.GetPlayerActiveRaceCarIndex());
    CGS_ASSERT(lpPlayerRaceCarState != 0, "lpPlayerRaceCarState");                 // :5196
    if (lpPlayerRaceCarState == 0)
    {
        return false;   // [GUARD]
    }

    if (!lbRaceModePressed || lpPlayerRaceCarState->mfSpeedMPH > 30.0f)
    {
        mfSnapRaceStartHoldSeconds = 0.35f;
        return false;
    }

    // The hold. `v18 = timer - dt; timer = v18; if (v18 > 0.0 || IsSimPaused(1,0)) return 0;`
    mfSnapRaceStartHoldSeconds -= lfGameTimestep;
    if (mfSnapRaceStartHoldSeconds > 0.0f || IsSimPaused(true, false))
    {
        return false;
    }

    const bool lbAtLights = mTriggerQueryManager.IsPlayerInTrafficLightRegion();
    mfSnapRaceStartHoldSeconds = 0.35f;
    *lpOutStartMechanism = lbAtLights ? E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS
                                      : E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_ANYWHERE;

    if (SnapDiagEnabled())
    {
        *CgsDev::Log::gpDebugPrint
            << "[snap] HOLD COMPLETE -- start mechanism "
            << static_cast<s32>(*lpOutStartMechanism)
            << " (speed " << lpPlayerRaceCarState->mfSpeedMPH << ")\n";
    }
    return true;
}

// ============================================================================================
// GameStateModule::DetectModeStarts -- X360 0x8239A428
//   (BrnGameStateModule.cpp:5240..5241 for the two asserts)
// ============================================================================================
// The arm. Two gates and a fork:
//   * `lwz r11, 0x1DB8(this)` == mModeManager.mpCurrentGameMode; if non-null,
//     `lbz r11, 0xAC(mode)` == GameMode::mbIsOnline (+172 -- the byte
//     OfflineGameMode::Construct clears and OnlineGameMode::Construct sets). Detection runs when
//     NO online mode is running -- or when the running mode is E_MODE_ONLINE_FREE_BURN_LOBBY
//     (15) or E_MODE_ONLINE_SHOWTIME (16), the two online modes you can still gesture in.
//   * on a successful hold, the SAME 15/16 test again -- but INVERTED: StartModeAtLights is
//     called only when the mode type is NOT one of those two (@0x8239A52C..0x8239A54C). That
//     asymmetry is the console's: the online lobby modes let the gesture be DETECTED (the online
//     path consumes it elsewhere) but never let it start an OFFLINE event.
//
// ✅ THE SHOWTIME (CRASH-MODE) ARM IS NO LONGER PARKED -- it is landed below, in full
// (0x8239A568..0x8239A8EC). See its own banner at the `else` for what it does, for the three
// errors the old park note carried, and for the two timesteps it needs. [showtime S7b-b]
// ============================================================================================
void GameStateModule::DetectModeStarts(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                       GameStateModuleIO::OutputBuffer*             lpOutput,
                                       f32                                          lfGameTimestep)
{
    CGS_ASSERT(lpInput != 0,  "lpInput != NULL");                                  // :5240
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");                                 // :5241
    if (lpInput == 0 || lpOutput == 0)
    {
        return;   // [GUARD]
    }

    // (X360 also brackets the whole body in PerfMonCpu::Start/StopMonitor(*(this + 292320)) --
    //  the module's CPU-monitor table is not modelled on this slice; not fabricated.)

    const GameMode* lpCurrentGameMode   = mModeManager.GetCurrentGameMode();
    const bool      lbOnlineModeRunning = (lpCurrentGameMode != 0) && lpCurrentGameMode->IsOnline();
    if (lbOnlineModeRunning)
    {
        const GameStateModuleIO::EGameModeType leType = mModeManager.GetCurrentGameModeType();
        if (leType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
            leType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            return;
        }
    }

    // `lbz r4, 0x45(r23)` @0x8239A508 -- the buffer's ControllerInput::mbRaceModePressed.
    const GameStateModuleIO::ControllerInput* lpControllerInput = lpInput->GetControllerInput();
    const bool lbRaceModePressed = (lpControllerInput != 0) && lpControllerInput->mbRaceModePressed;

    EGameModeStartMechanism leStartMechanism = E_GAMEMODESTARTMECHANISM_DEFAULT;
    if (ShouldStartSnapRaceMode(lbRaceModePressed, lfGameTimestep, &leStartMechanism))
    {
        const GameStateModuleIO::EGameModeType leType = mModeManager.GetCurrentGameModeType();
        if (leType != GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY &&
            leType != GameStateModuleIO::E_MODE_ONLINE_SHOWTIME)
        {
            StartModeAtLights(lpInput, lpOutput, leStartMechanism);
        }
    }
    // ---------------------------------------------------------------------------------------
    // ⭐⭐⭐ [showtime S7b-b, 2026-08-27] THE SHOWTIME (CRASH-MODE) ARM -- the whole `else`
    // branch, 0x8239A568..0x8239A8EC. It was PARKED here since wave D; this is it landed.
    //
    // ⚠️⚠️ THE PARK NOTE THAT SAT HERE WAS WRONG IN THREE PLACES, and each error would have cost
    // a re-derivation, so they are corrected on the record rather than quietly fixed:
    //   1. "two posts of game EVENT 146" -- they are posts of game ACTION 146. The queue is
    //      `GameStateModuleIO::Ou(lpOutput)` @0x8231D4B8, which returns lpOutput+4 with the
    //      "Not locked for writing" assert from BrnGameStateModuleIO.h:266 -- i.e. it IS
    //      OutputBuffer::GetGameActionQueue(). Action 146 == E_ACTION_SHOWTIME_INTRO_START.
    //   2. "a half-wired showtime latch would post into a queue with no consumer" -- THE CONSUMER
    //      IS MOUNTED AND LIVE. PhysicsModule::HandleGameActions' case-146 arm
    //      (BrnPhysicsModuleGameActions.cpp) reads the record's first 16 bytes and hands them to
    //      RaceCarPhysics::SetShowtimeAimDirection. It landed after the park note was written.
    //   3. "an alignment bit at gsm+284510" -- it is not an alignment bit. It is
    //      mbShowtimeIntroHasTouchedGround (DWARF BrnGameStateModule.h:857), OR-ed each frame with
    //      `!IsPlayerInAir()`, and it is the LAST gate before StartCrashMode.
    // [[gates-are-stale-not-dead]] -- ask when a park note last ran, not whether it exists.
    //
    // WHAT THE ARM IS. A two-phase latch around a 0.5 s "showtime intro" window:
    //
    //   PHASE 1 (mfShowtimeIntroTimeLeft <= 0), @0x8239A568..0x8239A738
    //     ShouldStartShowtimeMode(dt, mbCrashModePressed, simTimerRequests) must pass, AND the
    //     player must either already be crashing or be doing at least 10 m/s
    //     (`dot3(v,v) >= 10.0f*10.0f`, flt_82CDB8CC image-read as 41200000 == 10.0f). Then:
    //       * post action 146 with mbStart = 1 and the car's current heading;
    //       * open the window (mfShowtimeIntroTimeLeft = 0.5f, flt_82CDB8D0 image-read);
    //       * cache that heading in mShowtimeIntroOriginalDirection;
    //       * clear mbShowtimeIntroHasTouchedGround;
    //       * latch mfShowtimeIntroSteering to -1.0f when the car's angular velocity Y is >= 0,
    //         else +1.0f (flt_820037C8 / flt_82001C98, both image-read).
    //
    //   PHASE 2 (the window is open), @0x8239A73C..0x8239A8EC
    //     Decrement the window; OR the touched-ground bit with `!IsPlayerInAir()`; dot the current
    //     heading against the cached one. DECIDE as soon as ANY of these is true --
    //       the window expired | the car is airborne | the car is crashing |
    //       the un-writable byte at gsm+245952 is set | the heading dot has gone <= 0.0f
    //     -- otherwise keep waiting. On DECIDE: clear the window, re-ask
    //     ShouldStartShowtimeMode with the button treated as held (`li r5,1` @0x8239A884), and
    //       * if it passes AND mbShowtimeIntroHasTouchedGround -> StartCrashMode;
    //       * if it passes but the car has never touched the ground -> re-arm the window to
    //         9.99999975e-06f (flt_82004884) and try again next frame -- i.e. WAIT FOR A LANDING;
    //       * if it refuses -> post action 146 again with mbStart = 0 (the cancel).
    //
    // ⚠️ VMX DECODE NOTES, since this arm is the VMX-heavy one:
    //   * `vmsum3fp128 v0, v0, v0` @0x8239A62C is dot3(v,v) over the first three lanes, splatted;
    //     the following `vcmpgefp.` + `mfocrf r11, 2` + `extrwi r11,r11,1,24` reads CR6's
    //     "all lanes true" bit, which after a splat is just the scalar compare.
    //   * `vspltw v0, v0, 1` @0x8239A6F4 selects WORD 1 of the angular-velocity vector -- lane
    //     order is [X,Y,Z,W] from the MSB, so word 1 is Y.
    //   * ⛔ NO +32 SOURCE-REGISTER CORRECTION IS NEEDED ANYWHERE IN THIS ARM. Every VMX source
    //     here (v0, v13, v127) has a defining instruction a few lines above it in the same
    //     listing, so the [[ida-vmx-plus32-and-rdata-unlock]] rule does not fire -- stated
    //     because a reader who applies it blindly gets v95/v96 and a dead end.
    //
    // ⚠️⚠️ TWO TIMESTEPS, AND THEY ARE NOT THE SAME ONE. The console feeds ShouldStartShowtimeMode
    // `*(f32*)(Get()+8) * *(f32*)(Get()+4)` where Get() is PreWorldInputBuffer's TimerStatusInterface
    // accessor @0x8231CE28 -- i.e. CgsSystem::TimerStatus::GetCurrentTimeStep() on the GAME timer
    // (mfBaseTimeStep * mfTimeStepMultiplier) -- while the window decrement at 0x8239A74C uses the
    // module's own cached mfSimTimeStep at gsm+292284. NEITHER has a producer on this build (see
    // the [FLAG] at the site), and a zero dt here is NOT inert: it would mean the 10 ms crash-start
    // hold never counts down and the gate never fires -- a silent no-op indistinguishable from a
    // dead input. Both are therefore taken from this function's PC-only lfGameTimestep parameter,
    // which is the same substitution ShouldStartSnapRaceMode above already runs on.
    // ---------------------------------------------------------------------------------------
    else
    {
        // @0x8239A56C..0x8239A588. The console's own expression, computed first so that the
        // moment PreWorldInputBuffer::SetTimerStatusInterface gets a producer this reverts to the
        // real value with no edit. maEntries[0] is the GAME timer status; mfValue04/mfValue08 are
        // CgsSystem::TimerStatus::mfBaseTimeStep / mfTimeStepMultiplier (the local
        // GameStateModuleIO::TimerStatusInterface is a padding-fork of CgsSystem's -- same 24-byte
        // TimerStatus pair, different declaration; not unified here, it is not this wave's file).
        f32 lfShowtimeTimestep = 0.0f;
        const GameStateModuleIO::TimerStatusInterface* const lpTimerStatus =
            lpInput->GetTimerStatusInterface();
        if (lpTimerStatus != 0)
        {
            lfShowtimeTimestep =
                lpTimerStatus->maEntries[0].mfValue04 * lpTimerStatus->maEntries[0].mfValue08;
        }
        // [FLAG PC bring-up] nothing on this build calls PreWorldInputBuffer::SetTimerStatusInterface
        // (grep: the definition exists, the call does not), and GameStateModule::Construct value-
        // initialises the buffer -- so the product above is exactly 0.0f today. Falling back to the
        // caller's timestep keeps the gate live; the test is `== 0.0f` so the substitution retires
        // itself. DELETE-WHEN that setter has a caller.
        if (lfShowtimeTimestep == 0.0f)
        {
            lfShowtimeTimestep = lfGameTimestep;
        }

        // @0x8239A5A4 / @0x8239A87C: `GetTimerRequest(out)` then `addi r6, r11, 8` -- the SIM half
        // of the output buffer's TimerRequestInterface (mGameTimer @+0, mSimTimer @+8).
        CgsSystem::TimerRequestInterface* const lpTimerRequests =
            lpOutput->GetTimerRequestInterface();
        CgsSystem::TimerRequests* const lpSimTimerRequests =
            (lpTimerRequests != 0) ? lpTimerRequests->GetSimTimerRequests() : 0;

        // `lbz r5, 0x42(r23)` @0x8239A5B0 -- ControllerInput::mbCrashModePressed, i.e. BOTH
        // BUMPERS HELD (BrnGameStateModuleIO.cpp:92 ANDs action rows 54 and 55, bound to
        // LSHOULDER/RSHOULDER in CgsInputPadsPC.cpp's KA_BINDINGS).
        const bool lbCrashModePressed =
            (lpControllerInput != 0) && lpControllerInput->mbCrashModePressed;

        // The console's 32-byte stack record at var_A0/var_90, shared by BOTH posts.
        // ⓘ On the cancel post the console's aim vector is whatever the frame left in that slot
        // (it is only written on the arm-the-intro path), and the consumer stores it regardless of
        // mbStart. Here the host's Vector3 default constructor decides instead; either way the
        // value is meaningless when mbStart is false, and no reconstructed code reads it.
        GameStateModuleIO::ShowtimeIntroAction lShowtimeIntro;

        if (mfShowtimeIntroTimeLeft <= 0.0f)   // `lfs f12, 0(r26) ; fcmpu ; bgt` @0x8239A590
        {
            // ---- PHASE 1: the gesture, and opening the window ---------------------------------
            if (ShouldStartShowtimeMode(lfShowtimeTimestep, lbCrashModePressed, lpSimTimerRequests))
            {
                // @0x8239A5D0..0x8239A654. Already crashing? then the speed test is skipped
                // entirely (`bne cr6, loc_8239A658`). Otherwise the car must be doing 10 m/s.
                bool lbMayStartIntro = mLastActiveRaceCarInterface.IsPlayerCarCrashing();
                if (!lbMayStartIntro)
                {
                    // `sub_82310240(iface)` == GetPlayerRaceCarState(); `lvx128 v0, r3, 0x330`
                    // == element +816 == RaceCarState::mLinearVelocity, raw m/s.
                    const BrnPhysics::Vehicle::RaceCarState* const lpPlayerRaceCarState =
                        mLastActiveRaceCarInterface.GetPlayerRaceCarState();
                    const f32 lfSpeedSquared = static_cast<f32>(
                        rw::math::vpu::Dot(lpPlayerRaceCarState->mLinearVelocity,
                                           lpPlayerRaceCarState->mLinearVelocity));
                    lbMayStartIntro = (lfSpeedSquared >= KF_SHOWTIME_MIN_SPEED *
                                                          KF_SHOWTIME_MIN_SPEED);
                }

                if (lbMayStartIntro)
                {
                    // loc_8239A658..0x8239A738. `sub_82310398` == GetPlayerDirection() ==
                    // mTransform.At(), the car's forward row. The console calls it TWICE
                    // (@0x8239A668 and @0x8239A6A4) with the same argument; it is pure, so it is
                    // called once here and the value used twice.
                    const Vector3 lPlayerDirection =
                        mLastActiveRaceCarInterface.GetPlayerDirection();

                    lShowtimeIntro.mbStart       = true;              // `stb r11(1), var_90` @0x8239A664
                    lShowtimeIntro.mAimDirection = lPlayerDirection;  // `stvx128 v0, var_A0`  @0x8239A67C
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lShowtimeIntro),
                        GameStateModuleIO::E_ACTION_SHOWTIME_INTRO_START,
                        static_cast<s32>(sizeof(GameStateModuleIO::ShowtimeIntroAction)));

                    mfShowtimeIntroTimeLeft         = KF_SHOWTIME_INTRO_SECONDS;  // @0x8239A6A0
                    mShowtimeIntroOriginalDirection = lPlayerDirection;           // @0x8239A6C4
                    mbShowtimeIntroHasTouchedGround = false;                      // @0x8239A6C8

                    // @0x8239A6CC..0x8239A738. `sub_82310240` again, `lvx128 v0, r0, r3+0x340`
                    // == element +832 == mAngularVelocity, then `vspltw v0, v0, 1` == lane Y.
                    // NOTE THE SIGN: Y >= 0 latches MINUS one (the `bne` arm), Y < 0 latches PLUS.
                    const BrnPhysics::Vehicle::RaceCarState* const lpPlayerRaceCarState =
                        mLastActiveRaceCarInterface.GetPlayerRaceCarState();
                    const f32 lfAngularVelocityY = lpPlayerRaceCarState->mAngularVelocity.y;
                    mfShowtimeIntroSteering = (lfAngularVelocityY >= 0.0f) ? -1.0f : 1.0f;

                    if (ShowtimeDiagEnabled())
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[showtime] INTRO OPEN -- both bumpers held, gate passed."
                            << " crashing=" << (mLastActiveRaceCarInterface.IsPlayerCarCrashing() ? 1 : 0)
                            << " steering=" << mfShowtimeIntroSteering
                            << " window=" << mfShowtimeIntroTimeLeft << "s\n";
                    }
                }
            }
            // Either refusal falls into loc_8239A73C, which re-reads the window; it is still
            // <= 0 in this phase, so the console does nothing more this frame.
        }
        else
        {
            // ---- PHASE 2: the window is open --------------------------------------------------
            // @0x8239A748..0x8239A754. See the two-timesteps warning: the console decrements by
            // mfSimTimeStep (gsm+292284), which has no producer here.
            mfShowtimeIntroTimeLeft -= lfGameTimestep;

            // @0x8239A758..0x8239A7BC. `mbShowtimeIntroHasTouchedGround |= !IsPlayerInAir()`.
            // The console spells the negation as `cntlzw` + bit 5 (32 when the input was 0), then
            // ORs it into the byte and stores it straight back.
            const bool lbPlayerInAir = mLastActiveRaceCarInterface.IsPlayerInAir();
            if (!lbPlayerInAir)
            {
                mbShowtimeIntroHasTouchedGround = true;
            }

            // @0x8239A7A8..0x8239A7D8. dot3(current heading, the heading cached when the window
            // opened). `vmsum3fp128 v0, v0, v127` with v127 == the +284464 vector.
            const f32 lfHeadingDot = static_cast<f32>(
                rw::math::vpu::Dot(mLastActiveRaceCarInterface.GetPlayerDirection(),
                                   mShowtimeIntroOriginalDirection));

            // @0x8239A7DC..0x8239A870. Any one of these ends the wait.
            const bool lbDecideNow =
                   (mfShowtimeIntroTimeLeft <= 0.0f)                              // @0x8239A7DC
                || mLastActiveRaceCarInterface.IsPlayerInAir()                    // @0x8239A7E0
                || mLastActiveRaceCarInterface.IsPlayerCarCrashing()              // @0x8239A820
                || false /* gsm+245952 -- see the FLAG below */                   // @0x8239A84C
                || (lfHeadingDot <= 0.0f);                                        // @0x8239A860
            // [FLAG] gsm+245952 is a byte with NO WRITER anywhere in the 30,084-function export
            // set -- measured on the ASM, not just the pseudocode: the only three functions whose
            // listings form 0x3C0C0 are this one, ShouldAllowTimedTutorialTips @0x82356DB0 and
            // TrainingManager::RequestTraining @0x82365B20, and all three only READ it. A member
            // the console never sets reads as its zero-init, which is what the term above says.
            // ShouldAllowTimedTutorialTips already carries the identical FLAG for the identical
            // byte; named as a gap, not modelled.

            if (lbDecideNow)
            {
                mfShowtimeIntroTimeLeft = 0.0f;   // `stfs f31, 0(r26)` @0x8239A878

                // @0x8239A884: `li r5, 1` -- during the window the button is treated as HELD.
                if (ShouldStartShowtimeMode(lfShowtimeTimestep, true, lpSimTimerRequests))
                {
                    if (mbShowtimeIntroHasTouchedGround)
                    {
                        if (ShowtimeDiagEnabled())
                        {
                            *CgsDev::Log::gpDebugPrint
                                << "[showtime] WINDOW CLOSED, ground touched -> StartCrashMode"
                                << " (dot " << lfHeadingDot << ")\n";
                        }
                        StartCrashMode(lpInput, lpOutput);              // @0x8239A8BC
                    }
                    else
                    {
                        // @0x8239A8C4: re-arm a hair above zero so the next frame lands back in
                        // PHASE 2 and re-decides -- the console's "wait until the car lands".
                        mfShowtimeIntroTimeLeft = KF_SHOWTIME_INTRO_RETRY_SECONDS;
                    }
                }
                else
                {
                    // @0x8239A8D4..0x8239A8EC. The cancel post: the same 32-byte record with
                    // mbStart cleared, onto the same queue.
                    lShowtimeIntro.mbStart = false;                     // `stb r28(0), var_90`
                    lpOutput->GetGameActionQueue()->AddEvent(
                        reinterpret_cast<const CgsModule::Event*>(&lShowtimeIntro),
                        GameStateModuleIO::E_ACTION_SHOWTIME_INTRO_START,
                        static_cast<s32>(sizeof(GameStateModuleIO::ShowtimeIntroAction)));

                    if (ShowtimeDiagEnabled())
                    {
                        *CgsDev::Log::gpDebugPrint
                            << "[showtime] WINDOW CLOSED but the gate refused -- posted the"
                               " cancel (action 146, mbStart 0)\n";
                    }
                }
            }
        }
    }
}

// ============================================================================================
// GameStateModule::StartModeAtLights -- X360 0x82396CF8
//   (BrnGameStateModule.cpp:5365..5512; every assert line quoted below is that file's)
// ============================================================================================
// THE START. Resolve the junction, refuse if the event demands a car the player is not in, build
// a StartGameModeParams and hand it to ModeManager::StartGameMode.
//
// FOUR TRAPS, ALL OF THEM LIVE ON THE TEST TARGET (junction 480897 / event 558269):
//  1. THE MODE ENUM CROSSING. RaceEventData::mu8Mode is the DATA enum (STUNT_ATTACK == 2);
//     ModeManager indexes the RUNTIME enum (E_MODE_STUNT_ATTACK == 7).
//     ProgressionManager::GetEvent @0x82359850 is the ONLY bridge, and it is already bodied in
//     the mounted BrnProgressionManager.cpp -- verified against the export this wave: cases
//     0->0, 1->3, 2->7, 3->8, 4->5, 5->4, default asserts ("I dont know what this game type
//     is! ...", BrnProgressionManager.cpp:906) and returns -1. Using the data value as a mode
//     index would silently select E_MODE_OFFLINE_SHOWTIME.
//  2. THE START MECHANISM IS A DISCRIMINANT, NOT A FLAG. `cmpwi r31, 2 / bne` @0x82396D64.
//  3. THE SPECIAL-EVENT CAR GATE ABORTS THE START. On a mismatch the console posts action 272
//     (8 bytes, the demanded CgsID) and RETURNS -- it does not start anything. On a boot-drive
//     in an arbitrary car this WILL fire on Burning Route junctions; the one-shot log below is
//     what makes that visible instead of looking like a dead chain.
//  4. THE RANK FORK. Modes {0, 3, 7, 8} (stunt IS 7) take the PER-MODE rank pair; everything
//     else takes the global one, with a further PlayerHasFinishedLastRank branch. Getting it
//     wrong changes the target score and the time limit rather than crashing.
//
// THREE TAIL LEGS ARE PARKED, each with its console stores written out at the site.
// ============================================================================================
void GameStateModule::StartModeAtLights(const GameStateModuleIO::PreWorldInputBuffer* lpInput,
                                        GameStateModuleIO::OutputBuffer*             lpOutput,
                                        EGameModeStartMechanism                      leStartMechanism)
{
    CGS_ASSERT(lpInput != 0,  "lpInput != NULL");                                  // :5365
    CGS_ASSERT(lpOutput != 0, "lpOutput != NULL");                                 // :5366
    if (lpOutput == 0)
    {
        return;   // [GUARD]
    }
    (void)lpInput;   // the console asserts it and never dereferences it in this body.

    // @0x82396D64. Trap 2.
    if (leStartMechanism != E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS)
    {
        return;
    }

    // @0x82396D6C: `addis r3, r25, 1 / addi r3, r3, -0x5470` == this + 43920 == the inlined
    // TriggerQueryManager::GetTrafficData().
    const BrnTraffic::TrafficData* lpTrafficData = mTriggerQueryManager.GetTrafficData();
    CGS_ASSERT(lpTrafficData != 0, "lpTrafficData");                               // :5384
    if (lpTrafficData == 0)
    {
        return;   // [GUARD]
    }

    const BrnProgression::ProgressionData* lpProgressionData =
        mProgressionManager.GetProgressionData();
    CGS_ASSERT(lpProgressionData != 0, "lpProgressionData");                       // :5387
    if (lpProgressionData == 0)
    {
        return;   // [GUARD]
    }

    const LightTriggerId lLightTriggerId = mTriggerQueryManager.GetPlayerCurrentTrafficLightId();
    const BrnTraffic::JunctionLogicBox* lpJunctionLogicBox =
        lpTrafficData->GetJunctionLogicBoxForTrafficLight(static_cast<u32>(lLightTriggerId));
    CGS_ASSERT(lpJunctionLogicBox != 0, "lpJunctionLogicBox");                     // :5391
    if (lpJunctionLogicBox == 0)
    {
        return;   // [GUARD]
    }

    BrnProgression::Profile* lpProfile = mProgressionManager.GetProfile();
    CGS_ASSERT(lpProfile != 0, "lpProfile");                                       // :5394
    if (lpProfile == 0)
    {
        return;   // [GUARD]
    }

    // The console's `v15 = 15; do { *v16 = -1; v16 += 44; } while (v15-- >= 0); v58 = -1;`
    // (@0x82396E30-ish) is Array<CheckpointData,16>'s own default construction, inlined -- the
    // 16 per-element -1 heads plus the -1 live-count word at +704. On the host that IS the
    // default constructor of this local, so it is not restated.
    StartGameModeParams lStartGameModeParams;

    // `lwz r23, 0x38(r27); cmpwi r23, -1; beq -> return` @0x82396E9C.
    const u32 luEventJunctionID = lpJunctionLogicBox->GetEventJunctionID();
    if (luEventJunctionID == 0xFFFFFFFFu)
    {
        return;
    }

    const BrnProgression::EventJunction* lpEventJunctionForEvent =
        FindEventJunctionById(lpProgressionData, luEventJunctionID);
    const BrnProgression::RaceEventData* lpEventData =
        (lpEventJunctionForEvent != 0) ? lpEventJunctionForEvent->GetOfflineEvent() : 0;
    CGS_ASSERT(lpEventData != 0, "lpEventData");                                   // :5405
    if (lpEventData == 0)
    {
        return;   // [GUARD] -- the console falls into the deref here; see the file banner.
    }

    // Trap 1. The same +180972 override the junction check reads -- see the FLAG on it in
    // CheckIfPlayerIsAtJunctionWithAnEvent above; the sentinel path is taken here for the same
    // reason and carries the same DELETE-WHEN. (@0x82396F10..0x82396F20.)
    const s32 liDataGameType = static_cast<s32>(lpEventData->GetMode());
    const GameStateModuleIO::EGameModeType leGameModeType =
        static_cast<GameStateModuleIO::EGameModeType>(mProgressionManager.GetEvent(liDataGameType));

    // ---- Trap 3: the special-event car gate (@0x82396F28..0x82396FAC) -----------------------
    const CgsID lSpecialEventCarId = lpEventData->GetSpecialEventCarId();
    if (lSpecialEventCarId != 0)
    {
        const CgsID lOriginalCarId = GetOriginalCarId(mActivePlayerCarId);
        if (lOriginalCarId != lSpecialEventCarId)
        {
            GameStateModuleIO::WrongCarForChallengeAction lWrongCar;
            lWrongCar.mSpecialEventCarId = lSpecialEventCarId;
            lpOutput->GetGameActionQueue()->AddEvent(
                reinterpret_cast<const CgsModule::Event*>(&lWrongCar),
                GameStateModuleIO::E_ACTION_WRONG_CAR_FOR_CHALLENGE,
                static_cast<s32>(sizeof(GameStateModuleIO::WrongCarForChallengeAction)));

            // [DIAG] NOT IN THE X360 BINARY. One-shot, and deliberately NOT behind the
            // BRN_PROP_DIAG env guard the other rungs use: a wrong-car abort looks exactly like
            // a broken chain from the outside (the gesture completes, nothing happens), and this
            // is the one line that tells the two apart. Emitted at most once per process.
            static bool sbWrongCarLogged = false;
            if (!sbWrongCarLogged && CgsDev::Log::gpDebugPrint != 0)
            {
                sbWrongCarLogged = true;
                *CgsDev::Log::gpDebugPrint
                    << "[snap] START ABORTED -- this event needs a specific car (event junction "
                    << luEventJunctionID << "); posted action 272.\n";
            }
            return;
        }

        // The two 8-byte OR-stores into Profile+117952 (@0x82396F98..0x82396FAC): that word is
        // the head field of Profile::maHasPlayerSeenTraining, a CgsContainers::BitArray<256>
        // whose fields are u64 with `1 << (index % 64)` -- so `oris 0x100` sets bit 24 and
        // `oris 0x200` sets bit 25, i.e. training types 24 and 25 exactly. Their names confirm
        // the reading: E_TRAINING_TYPE_CORRECT_CAR_FOR_CHALLENGE / _WRONG_CAR_FOR_CHALLENGE --
        // once you have started a special-event race in the right car, neither car tip is ever
        // shown again. Written through Profile::SetTrainingAlreadySeen, not by offset.
        lpProfile->SetTrainingAlreadySeen(BrnProgression::E_TRAINING_TYPE_CORRECT_CAR_FOR_CHALLENGE);
        lpProfile->SetTrainingAlreadySeen(BrnProgression::E_TRAINING_TYPE_WRONG_CAR_FOR_CHALLENGE);
    }

    // The unconditional `ori 0x100` on the same word (@0x82396FB0..0x82396FC0) -- bit 8 ==
    // E_TRAINING_TYPE_DISCOVERS_EVENT.
    lpProfile->SetTrainingAlreadySeen(BrnProgression::E_TRAINING_TYPE_DISCOVERS_EVENT);

    // [PARKED] ProgressionManager::FixGameModeRanks @0x82395CD8, called here (@0x82396FC4).
    // It re-derives cached per-mode rank thresholds on the manager -- console reads/writes at
    // ProgressionManager +480 / +876 / +888 / +904, walking ProgressionRankData +96/+97/+98 with
    // its own "liProgressionRank >= 0 && liProgressionRank < liNumRanks" assert at
    // BrnProgressionManager.cpp:3802. None of those four manager members is modelled on this
    // tree's ProgressionManager slice, so the call is NAMED rather than faked. Effect of the
    // park: those cached thresholds keep whatever the last writer left; the rank VALUES the fork
    // below reads (GetProgressionRankForGameMode / GetProgressionRank) are computed
    // independently of them, so the start still gets a rank -- it just may not have been
    // re-clamped this frame.

    // ---- build the params (@0x82396FC8..0x8239729C) -----------------------------------------
    // `sub_823102F0(&frame, this + 235488)` is the inlined
    // RCEntityActiveRaceCarOutputInterface::GetPlayerPosition(); it rides v1 and consumes no GPR
    // slot, which is why Hex-Rays renders Construct with only two arguments.
    lStartGameModeParams.Construct(leGameModeType,
                                   mLastActiveRaceCarInterface.GetPlayerPosition(),
                                   E_GAMEMODESTARTMECHANISM_SPIN_WHEELS_AT_LIGHTS);

    lStartGameModeParams.SetTrafficDensity(lpEventData->GetTrafficDensity());      // lfs 8(event)
    lStartGameModeParams.SetBoostEarning(lpEventData->GetBoostEarning());          // lfs 0xC(event)
    lStartGameModeParams.SetTrafficLightTriggerId(
        mTriggerQueryManager.GetPlayerCurrentTrafficLightId());                    // @0x82397018
    lStartGameModeParams.SetEventJunctionId(luEventJunctionID);                    // @0x82397024
    lStartGameModeParams.SetEventData(lpEventData);                                // @0x82397020
    lStartGameModeParams.SetJunctionID(lpJunctionLogicBox->GetID());               // @0x82397038

    // `ldx r29, r25, 0x456D8` == mActivePlayerCarId (@0x82397034).
    const CgsID lPlayerCarId = mActivePlayerCarId;
    CGS_ASSERT(lPlayerCarId != 0, "That Player Car CGSID is just wrong\n");         // :5471

    // `lwzx r3, r25, 0x456E8` == mpVehicleList, then sub_82233A28 == VehicleList::GetVehicleData
    // (CgsID) -- the committed body at SharedClasses/DataLists/VehicleList.cpp:309.
    const BrnResource::VehicleListEntry* lpPlayerVehicleEntry =
        (mpVehicleList != 0) ? mpVehicleList->GetVehicleData(lPlayerCarId) : 0;
    CGS_ASSERT(lpPlayerVehicleEntry != 0, "Couldnt get the Player Car Vehicle List Entry\n");  // :5475
    lStartGameModeParams.SetPlayerVehicleGamePlayData(lpPlayerVehicleEntry);

    // ---- Trap 4: the rank fork (@0x823970E0..0x82397174) ------------------------------------
    f32 lfProgressionRankAsRatio;
    s8  li8ProgressionRank;
    if (leGameModeType == GameStateModuleIO::E_MODE_OFFLINE_RACE ||
        leGameModeType == GameStateModuleIO::E_MODE_ROAD_RAGE    ||
        leGameModeType == GameStateModuleIO::E_MODE_STUNT_ATTACK ||
        leGameModeType == GameStateModuleIO::E_MODE_MARKED_MAN)
    {
        lfProgressionRankAsRatio =
            mProgressionManager.GetProgressionRankForGameModeNormalised(leGameModeType);
        li8ProgressionRank = mProgressionManager.GetProgressionRankForGameMode(leGameModeType);
    }
    else
    {
        lfProgressionRankAsRatio = mProgressionManager.GetProgressionRankNormalisedForCurrentRank();
        if (mProgressionManager.PlayerHasFinishedLastRank())
        {
            // `sub_82369020(this + 181268)` == GetProgressionData(); `lwz r11, 0x14(r3)` ==
            // muProgressionRankCount; `addi -1 / extsb` (@0x82397150..0x82397164).
            li8ProgressionRank = static_cast<s8>(
                static_cast<s32>(lpProgressionData->GetProgressionRankCount()) - 1);
        }
        else
        {
            li8ProgressionRank = static_cast<s8>(mProgressionManager.GetProgressionRank());
        }
    }
    lStartGameModeParams.SetProgressionRankAsRatio(lfProgressionRankAsRatio);

    if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
    {
        *CgsDev::Log::gpDebugPrint << "liProgressionRank: "
                                   << static_cast<s32>(li8ProgressionRank) << "\n";
    }

    const BrnProgression::ProgressionRankData* lpProgressionRankData =
        lpProgressionData->GetProgressionRankData(
            static_cast<u32>(static_cast<s32>(li8ProgressionRank)));
    CGS_ASSERT(lpProgressionRankData != 0, "lpProgressionRankData != NULL");       // :5506
    lStartGameModeParams.SetProgressionRankData(lpProgressionRankData);

    // The SECOND EventJunction scan (@0x82397208..0x82397258) -- same key, different assert
    // string, and a different field taken off the record.
    const BrnProgression::EventJunction* lpEventJunction =
        FindEventJunctionById(lpProgressionData, luEventJunctionID);
    CGS_ASSERT(lpEventJunction != 0, "lpEventJunction != NULL");                   // :5512
    lStartGameModeParams.SetShotGroup(
        (lpEventJunction != 0) ? lpEventJunction->GetShotGroup() : 0);             // junction +0xC

    // `addi r31, r25, 0x1020` == &mModeManager (this + 4128) -- the offset this campaign had to
    // arbitrate: it is 0x1020, NOT the old note's +46640 (which is the training manager).
    const BrnTraffic::LightTriggerStartData* lpStartData =
        mModeManager.GetStartDataForTrafficLight(static_cast<u32>(lLightTriggerId));
    if (lpStartData != 0)   // [GUARD] -- the console tail-calls straight into GetStartDirection
    {
        lStartGameModeParams.SetStartDirection(lpStartData->GetStartDirection(0));
    }

    lStartGameModeParams.SetPlayerBaseDeformation(
        lpProfile->GetPlayerBaseDeformAmount(lPlayerCarId));                       // @0x82397298

    if (SnapDiagEnabled())
    {
        *CgsDev::Log::gpDebugPrint
            << "[snap] START GAME MODE runtime-mode " << static_cast<s32>(leGameModeType)
            << " (data mode " << liDataGameType << ") eventJunction " << luEventJunctionID
            << " junction " << lpJunctionLogicBox->GetID()
            << " rank " << static_cast<s32>(li8ProgressionRank)
            << " ratio " << lfProgressionRankAsRatio << "\n";
    }

    // THE HOP. ModeManager::StartGameMode @0x8234FCE8 is COMMITTED (BrnModeManager_Start.cpp)
    // and mounted; it is what runs SetupGameMode -> PrepareForMode (the 2272-byte action 23) and
    // seats the concrete GameMode.
    mModeManager.StartGameMode(lpOutput, &lStartGameModeParams);

    // [PARKED] THE POST-START TAIL, both halves (@0x823972B0..0x823972FC):
    //   (a) five stores into the embedded StreetManager (base this + 284520):
    //          +0x1D3C, +0x1D80, +0x1D84  <- dword_820A766C, rodata holding -1 (image-read at
    //                                        0x820A766C: FF FF FF FF). Note it is a LOAD from
    //                                        rodata, not an immediate -- the same shape
    //                                        BrnModeManager_Start.cpp already records for
    //                                        SendModeStopMessages.
    //          +0x1DC7, +0x1DC8           <- 0
    //       i.e. GameStateModule +292004 / +292072 / +292076 / +292143 / +292144. A road-rule
    //       "current run" reset; none of the five is a modelled StreetManager member.
    //   (b) for E_MODE_ROAD_RAGE (3) and E_MODE_MARKED_MAN (8) only:
    //          *(this + 46448) = 0   (std, 8 bytes -- r19 is 0 from the prologue @0x82396DA4,
    //                                 so this is a CLEAR, not the trigger id the pseudocode's
    //                                 `*(v8 + 5806) = v12` suggests)
    //          *(this + 46620) = 1
    //       both inside the embedded DriveThruManager (base this + 44240).
    // Neither leg affects a stunt run and neither has a modelled member to bind to. Named here
    // in full so landing them is a lookup, not a re-derivation.
}

// ==============================================================================================
// GameStateModule::CheckForAllEventsBeingFound  (X360 0x82382460)
//
// ⭐ THE [PARKED] NOTE IN OnEventFinishUpdateProfile ABOVE IS PAID -- and this is not a
// nice-to-have: with BrnDriveThruManager.cpp mounted it is ON THE LIVE DRIVING PATH. Measured
// 2026-08-27 with a trap stub in its place: a plain junkyard -> car-select -> free-burn run
// broke into the debugger 9.5 s after the first throttle input, at
//   CheckForAllEventsBeingFound  <-  DriveThruManager::HandleDriveThru
//                                <-  TriggerQueryManager::ProcessPlayerTriggers
//                                <-  ...PreWorldUpdatePlayerTriggersBringUp
// so it runs the moment the player is near any drive-thru region.
//
// "Has the player now discovered EVERY event in the profile?" -- walk the profile's event
// records, stop at the first one whose DISCOVERED bit is clear, and if the walk ran to the end,
// fire the find-all-events achievement (once) and post the 1-byte all-events-discovered action.
//
// ASM, @0x8238247C..0x82382540, store for store:
//     lwz    r11, 0x278(r29)                   ; Profile::miEventCount
//     cmplwi r11, 0 / beq -> tail              ; count == 0 skips the loop ENTIRELY, and the
//                                              ; tail's `0 >= 0` then reports ALL FOUND
//     addi   r30, r29, 0x7084                  ; &maEvents[0].muFlags   (0x7080 + 4)
//   loop:
//     cmplw  r31, r11 / blt ok                 ; UNSIGNED index-vs-count check ...
//     FireAssert("luIndex < static_cast<uint32_t>(miEventCount)", ..., 3004)
//   ok:                                        ; ... i.e. this loop body IS Profile::GetEvent
//     lhz    r11, 0(r30) / clrlwi r11, r11, 31 ; muFlags & 1 == E_FLAG_DISCOVERED
//     cmplwi r11, 0 / beq -> tail              ; first UNdiscovered record ends the walk
//     lwz    r11, 0x278(r29) / addi r31,r31,1 / addi r30,r30,8 / cmplw / blt loop
//   tail:
//     lwz    r11, 0x278(r29) / cmplw r31, r11 / blt -> return
//     addis  r31, r26, 3 / addi r31, r31, -0x3A50   ; this + 181680 == mAchievementManager
//     li     r4, 0x19 / slot 1 -> IsAchievementEarnt(25); if clear, slot 0 -> AchievementEarnt(25)
//     li     r6, 1 / li r5, 0xCA / AddEvent(queue, &stackbyte, 202, 1)
// The achievement pair is spelled through AchievementManagerBase::OnFindAllEvents(), which is
// exactly those four instructions and is where they have to live -- both virtuals are protected
// in the DWARF, so this class cannot name them. See that function's banner for the inference.
//
// ⚠️ The console's 1-byte payload for action 202 is an UNINITIALISED stack slot -- var_50 is
// handed to AddEvent with no preceding store (unlike the sibling id-55 post in
// UnlockCarChallengeForCar, which does `v21[0] = 0` first). A zero is written here instead of
// reproducing an indeterminate read: no consumer of 202 exists in this tree to read it, and
// posting stack garbage would be a real defect, not fidelity. FLAG, deliberate.
// ==============================================================================================
void GameStateModule::CheckForAllEventsBeingFound(BrnProgression::Profile* lpProfile,
                                                  GameStateModuleIO::GameActionQueue* lpQueue)
{
    u32 luIndex = 0;
    if (lpProfile->GetEventCount() != 0)
    {
        // The console's do/while: the first record is read before any continue test, and the
        // walk stops at the first record whose DISCOVERED bit is clear.
        do
        {
            if (!lpProfile->GetEvent(luIndex)->IsFlagSet(
                    BrnProgression::ProfileEvent::E_FLAG_DISCOVERED))
            {
                break;
            }
            ++luIndex;
        }
        while (luIndex < lpProfile->GetEventCount());
    }

    if (luIndex >= lpProfile->GetEventCount())
    {
        mAchievementManager.OnFindAllEvents();

        const u8 lu8Payload = 0;   // FLAG: the console hands AddEvent an uninitialised stack byte
        lpQueue->AddEvent(reinterpret_cast<const CgsModule::Event*>(&lu8Payload),
                          GameStateModuleIO::E_ACTION_ALL_EVENTS_DISCOVERED,
                          static_cast<s32>(sizeof(lu8Payload)));
    }
}

}   // namespace BrnGameState
