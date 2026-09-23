#include "GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/GameState/ModeManager/Scoring/BrnStuntModeScoring.h" // StuntModeScoring::IsComboInProgress
#include "GameSource/GameState/ModeManager/Scoring/BrnRoadRageModeScoring.h" // RoadRageModeScoring::DoesDamageCriticalMessageNeedToBeSent / ResetDamageCriticalMessageFlag
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface::IsPlayerCarCrashing (inline)
#include "GameSource/GameState/ModeManager/Scoring/BrnScoringSystemEventQueues.h" // InputBuffer::TakedownEventQueue, VehicleManagerOutputInterface::RaceCarCrashEventQueue (complete)
#include "GameSource/GameState/BrnGameActions.h"               // HUDMessageXCrashesAction (250), HUDMessagePlayerReachesCheckpointAction (249)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"     // gpDebugPrint ([hud-xcrash] witness)
#include <cstdlib>                                             // getenv (BRN_MODEMGR_DIAG)

// ============================================================================
// b5-decomp/src/GameSource/GameState/ModeManager/Hud/BrnHUDMessageLogic.cpp
// ============================================================================
// BrnGameState::HUDMessageLogic -- the object's lifecycle + per-frame entry points
// (Construct 0x8236F530 / Prepare 0x82366478 / PreWorldUpdate 0x82389248 /
// PostWorldUpdate 0x8239D998), the stunt-scorer notification pump
// (GenerateStuntMessage 0x82394DF8), the per-frame online stunt-run HUD message generators
// (X360 0x82394838 / 0x82394920 / 0x82394A78 / 0x82394B88 / 0x82394CC0) plus the
// per-car team-change recorder (X360 0x8231E498). Reconstructed store-for-store from
// the X360 pseudocode + assembly; the scoring queries go through the committed
// ScoringSystem / CarData / CgsSystem::Time / BitArray APIs by name.
//
// ⭐ GenerateStuntMessage is the image's ONLY consumer of StuntModeScoring's one-shot
// mbRecentStunt / mbRecentCombo latches. Its absence is what made
// StuntModeScoring::UpdateBufferedScore's opening `CGS_ASSERT(!mbRecentStunt)` fire mid-run
// in an offline stunt race -- the latch was armed every banked stunt and never drained.
// See the body for the full console attestation.
//
// The X360 binary reads the ScoringSystem's mode-timer fields raw (mStartTime.miSeconds /
// mEndTime.miSeconds); those reads are the inlined IsTimeLimitActive() predicate, restored
// here as the call (BrnScoringSystem_Timer.cpp documents the same lowering). The
// `lpCarData != NULL` asserts fire at BrnHUDMessageLogic.cpp:1026 / :1120 / :1120 in the
// X360 build; preserved via CGS_ASSERT.
namespace BrnGameState
{
namespace
{
    // The online stunt-run mode duration (X360 read-only float at 0x82CDB7B4, also stored
    // into GameModeParams::mfModeTimeLimit by OnlineStuntRunMode::Start @ 0x82339E70). Used
    // by the "leading" generator to gate on at-least-ten-seconds-elapsed. Modelled as a
    // single named constant so the elapsed-time math is self-consistent (the comparison is
    // exact for any value of the limit). FLAG: the literal float byte value is not in the
    // per-function IDA exports; 120.0f is the inferred online stunt-run mode length.
    const f32 KF_ONLINE_STUNT_RUN_MODE_TIME_LIMIT = 120.0f;

    // The generator gates (X360 read-only floats). Recognisable round constants from the
    // comparison context: elapsed > 10s before reporting a new leader; combo warning while
    // remaining in (0, lfComboWarningTime]; the 30-second time warning fires while remaining
    // is below 31s and the warning has not already been announced at 30s.
    const f32 KF_LEADING_MIN_ELAPSED_SECONDS = 10.0f;   // 0x8202AC38
    const f32 KF_ZERO_SECONDS                = 0.0f;    // 0x82001CC0
    const f32 KF_TIME_WARNING_WINDOW_SECONDS = 31.0f;   // 0x820323F4
    const f32 KF_TIME_WARNING_SECONDS        = 30.0f;   // 0x82029F30

    // DetectOnlineCrashes' buffer time: `lfs f0, -0x60E8(r25)` @0x8239468C == flt_82029F18, which
    // the image holds as 0x3FC00000 == 1.5f. The same word seeds mTimeSinceNewLeader in Prepare
    // (`lfs f1, -4(r29)` @0x82366508).
    const f32 KF_CRASH_MESSAGE_BUFFER_SECONDS = 1.5f;   // 0x82029F18

    // The buffered-crash pool's capacity; DetectOnlineCrashes' second loop and
    // RemoveCrashingMessagesForTakendownPlayers both walk every slot (`cmpwi r31, 8`).
    const s32 KI_NUM_BUFFERED_CRASHING_CARS = 8;
}

// ============================================================================
// Lifecycle + per-frame entry points.
// ============================================================================

// X360 0x8236F530. Binds the action queue's buffer, seeds the latched mode type and runs
// Prepare(). Called by ModeManager::Construct (console 0x82340008 `bl HUDMessageLogic::Construct`).
//
// X360 (0x8236F530), store for store:
//   bl VariableEventQueue<256,16>::Construct   ; the queue IS this object's first member (offset 0)
//   bl VariableEventQueue<256,16>::Prepare
//   std r10(0), 0x240(r31)  (twice)            ; the team-changed bit set
//   std r10(0), 0x1B8(r31)                     ; the buffered-crash pool's occupancy bits
//   stw {7,6,5,4,3,2,1,0}, 0x190..0x1AC(r31) ; stw 8, 0x1B0(r31) ; std 0, 0x1B8(r31)
//                                              ; the pool's Clear image, written TWICE
//   stw r28(-1), 0x1C0(r31)                    ; meCurrentGameModeType = E_MODE_NONE
//   bl HUDMessageLogic::Prepare
//
// [FX-GS 2026-09-23, crash-parity G11-D2] The "nine-entry table" this banner used to park is
// mBufferedCrashingCars (DWARF BrnHUDMessageLogic.h:265, base r11 = this+0x110): the free queue
// {7..0} at +0x80, the free count 8 at +0xA0 and the occupancy word at +0xA8 are
// ObjectPool::Clear's image. One Clear() reproduces the doubled store run's end state.
void HUDMessageLogic::Construct()
{
    mActionQueue.Construct();
    mActionQueue.Prepare();

    mTeamChangedBits.Prepare();                                 // std 0, 0x240
    mBufferedCrashingCars.Clear();                              // 0x8236F574..0x8236F5D4
    meCurrentGameModeType = GameStateModuleIO::E_MODE_NONE;     // stw -1, 0x1C0

    Prepare();
}

// X360 0x82366478. Re-seeds the message edge-trackers so each notification fires once per mode.
// Called by Construct and by PostWorldUpdate on every latched-mode change.
//
// The console writes twenty-five fields; the six below are every one of them that this file's
// semantic-parity layout models, and each is X360-proven:
//   *(a1+452) = 0.0   -> mfTimeInMode                (+0x1C4)
//   *(a1+464) = -1    -> miScoreMessageRaceCarIndex  (+0x1D0)
//   *(a1+476) = -1    -> meEliminationRaceCarIndex   (+0x1DC)
//   *(a1+480) = 0     -> miLastLeadingTeam           (+0x1E0)
//   *(a1+484) = 0     -> miLastVictoryTeam           (+0x1E4)
//   *(a1+488) = -1.0  -> mfLastTimeWarningAnnounced  (+0x1E8)
//
// Checkpoint and finisher latches below are also initialized by ARTIST Prepare, and so is the
// buffered-crash pool: 0x82366528..0x82366578 write its Clear image (free queue 7..0 at
// +0x190..+0x1AC, count 8 at +0x1B0, occupancy 0 at +0x1B8) as Prepare's last stores
// [FX-GS 2026-09-23, crash-parity G11-D2].
// Still unmounted: +516/+520 (5.0), +524/+528 (0), +532/+540 (times 1.5/7.5),
// +584 (-1), +568/+588 (0), +592 (-1), +596 (0).
// Prepare deliberately leaves the two score-sample fields untouched, as in ARTIST.
void HUDMessageLogic::Prepare()
{
    meFinishingRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    miFinishPosition = 0;
    mCurrentPlayerCheckpointID = 0;
    mNextPlayerCheckpointID = 0;
    mbIsLastCheckpoint = false;
    mbPlayerHasJustTriggeredCheckpoint = false;
    miNextRivalCheckpoint = 1;
    meCheckpointTriggeringRaceCarIndex = E_ACTIVE_RACE_CAR_INDEX_INVALID;
    mRivalCheckpointID = 0;
    mfTimeInMode               = 0.0f;                                  // +0x1C4
    miScoreMessageRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);  // +0x1D0
    meEliminationRaceCarIndex  = static_cast<EActiveRaceCarIndex>(-1);  // +0x1DC
    miLastLeadingTeam          = 0;                                     // +0x1E0
    miLastVictoryTeam          = 0;                                     // +0x1E4
    mfLastTimeWarningAnnounced = -1.0f;                                 // +0x1E8
    mBufferedCrashingCars.Clear();                                      // +0x190..+0x1B8
}

// X360 0x82389248. The drain: bulk-append this frame's notifications into the module's outgoing
// game-action queue, then empty the local one. Its whole body is
//   CGS_ASSERT(lpOutputGameActionQueue, "lpOutputGameActionQueue != NULL")  (BrnHUDMessageLogic.cpp:116)
//   VariableEventQueue<13312,16>::Append<256,16>(a2, a1);
//   VariableEventQueue<256,16>::Clear(a1);
// `a1` is the object itself because mActionQueue is its first member; here it is named.
void HUDMessageLogic::PreWorldUpdate(GameStateModuleIO::GameActionQueue* lpOutputGameActionQueue)
{
    CGS_ASSERT(lpOutputGameActionQueue != NULL, "lpOutputGameActionQueue != NULL");

    lpOutputGameActionQueue->Append(mActionQueue);
    mActionQueue.Clear();
}

// X360 0x8239D998. Latch the game mode, tick the in-mode clock, run the per-mode generators.
//
// ⚠ THE ARGUMENTS ARE THE DEVIATION, NOT THE BODY. The console signature is ten arguments --
//   PostWorldUpdate(this, lpActiveRaceCarInterface, leGameModeType, lpModeManager, lpScoringSystem,
//                   lpRaceCarCrashEventQueue, lpVehicleOutputInterface, lpTakedownQueue, lfDelta,
//                   [sp+0x5C] mePlayerActiveRaceCarIndex, [sp+0x67] IsGameModeInProgress(mode))
// (r4 iface, r5 mode, r6 ModeManager, r7 scoring, r8 crash queue, r9 vehicle interface,
// r10 takedown queue, f1 delta; 0x8239D9A8..0x8239D9CC, `lwz r29, 0x10C(r1)` @0x8239DA40).
// The seven carried here are the ones the reproduced arms read, so both of the console's head
// asserts stay verbatim rather than being dropped with the arguments they guard. Still dropped:
// the ModeManager (only case 13 reads it), the VehicleOutputInterface (only the online-team tail)
// and the in-progress byte (only case 11).
//
// REPRODUCED, statement for statement:
//   CGS_ASSERT(a2, "lpActiveRaceCarInterface != NULL")  (BrnHUDMessageLogic.cpp:144)
//   CGS_ASSERT(a5, "lpScoringSystem != NULL")           (BrnHUDMessageLogic.cpp:145)
//   if (*(a1+448) != a3) { Prepare(a1); *(a1+448) = a3; }
//   *(a1+452) += a9;
//   switch (*(a1+448)) { case 0/10: GenerateRaceModeMessages(a1, a2, a5, a6, a8, [sp+0x5C], a9);
//                        case 3: GenerateCriticalDamageMessage(a1, a2, a5); break;
//                        case 7: GenerateStuntMessage(a1, a5); break;
//                        case 12/14/17: GenerateStuntMessage(a1, a5); ... break;
//                        case 15: DetectOnlineCrashes(a1, a2, a6, a9);
//                                 RemoveCrashingMessagesForTakendownPlayers(a1, a8); break; }
// Note the switch tests the LATCHED member, not the incoming argument -- they differ only on the
// frame the mode changes, and the console reads the member. Faithfully kept.
// (case 3 added by the road-rage wave 2026-09-02: it reads only a2 and a5, both carried here.)
// [FX-GS 2026-09-23, crash-parity G11-D1/D2/D3] cases 0/10 (@0x8239DAB0, `bl` @0x8239DACC) and
// 15 (@0x8239DB40, `bl` @0x8239DB50 / @0x8239DB5C) are reproduced; jump table 0x8239DA68.
//
// [X] NOT REPRODUCED, named rather than faked -- the other switch arms and the tail:
//   case 11    GenerateOnlineBlueTeamEscapingMessage + ...AreBehindYouMessage + ...LeaderMilestone;
//   case 12/14 (not 17) GenerateOnlineStuntRunVictoryMessages + ...LeadingMessages;
//   case 12/14/17 GenerateOnlineStuntRunEliminationMessages + ...TimeMessages + ...ScoreMessages;
//   case 13    GenerateBurningHomeRunMessages;
//   tail       GenerateOnlineTeamChangeMessages(a1, a7, a31).
// The five online stunt-run generators ARE bodied in this file. Leading and Time also take a
// CgsSystem::Time read off the ModeManager (`lwz 0x6DC8(r27) / lfs 0x6DCC(r27)` @0x8239DB9C /
// @0x8239DBDC) that this reduced set does not carry; Victory, Elimination and Score need only the
// player index carried since 2026-09-23 and are still unwired (online-only; not part of the
// crash-message fix). Behaviour cost on an OFFLINE stunt race -- the mode that leg exists for --
// is zero: case 7 is the whole of its arm.
// DELETE-WHEN the console's full argument set is reachable (a real PostWorldInputBuffer exists and
// ModeManager::PostWorldUpdate becomes the live caller again).
void HUDMessageLogic::PostWorldUpdate(
    const StuntModeScoring::ActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    GameStateModuleIO::EGameModeType leGameModeType,
    ScoringSystem* lpScoringSystem,
    const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpRaceCarCrashQueue,
    const InputBuffer::TakedownEventQueue* lpTakedownQueue,
    f32 lfDelta,
    EActiveRaceCarIndex lePlayerActiveRaceCarIndex)
{
    CGS_ASSERT(lpActiveRaceCarInterface != NULL, "lpActiveRaceCarInterface != NULL");
    CGS_ASSERT(lpScoringSystem != NULL, "lpScoringSystem != NULL");

    if (meCurrentGameModeType != leGameModeType)
    {
        Prepare();
        meCurrentGameModeType = leGameModeType;
    }

    mfTimeInMode += lfDelta;

    switch (meCurrentGameModeType)
    {
        case GameStateModuleIO::E_MODE_OFFLINE_RACE:        // case 0
        case GameStateModuleIO::E_MODE_ONLINE_RACE:         // case 10
            GenerateRaceModeMessages(lpActiveRaceCarInterface, lpScoringSystem, lpRaceCarCrashQueue,
                                     lpTakedownQueue, lePlayerActiveRaceCarIndex, lfDelta);
            break;

        case GameStateModuleIO::E_MODE_ROAD_RAGE:           // case 3
            GenerateCriticalDamageMessage(lpActiveRaceCarInterface, lpScoringSystem);
            break;

        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN_LOBBY:  // case 15
            DetectOnlineCrashes(lpActiveRaceCarInterface, lpRaceCarCrashQueue, lfDelta);
            RemoveCrashingMessagesForTakendownPlayers(lpTakedownQueue);
            break;

        case GameStateModuleIO::E_MODE_STUNT_ATTACK:        // case 7
            GenerateStuntMessage(lpScoringSystem);
            break;

        case GameStateModuleIO::E_MODE_ONLINE_FUGITIVE:     // case 12
        case GameStateModuleIO::E_MODE_ONLINE_FREE_BURN:    // case 14
        case GameStateModuleIO::E_MODE_ONLINE_MODE_END:     // case 17
            GenerateStuntMessage(lpScoringSystem);
            break;

        default:
            break;
    }
}

// ============================================================================
// [FX-GS 2026-09-23, crash-parity G11-D1/D2/D3] The race arm and the crash messages.
// ============================================================================

// X360 0x82399C78 (DWARF BrnHUDMessageLogic.h:128 / .cpp:242). Registers on entry: r4 iface -> r30,
// r5 scoring -> r29, r6 crash queue -> r28, r7 takedown queue -> r27, r8 player index -> r26,
// f1 time step -> f31. The console order, call for call:
//   0x82399CA4  bl GenerateLeaderMessages(iface, scoring)                    [X] no body in the tree
//   0x82399CB0  bl GenerateFinisherMessage(iface)                            [X] no body in the tree
//   0x82399CB4  lbz 0x201 ; beq -> the inlined GeneratePlayerCheckpointMessage (AddEvent 249, 24)
//   0x82399CF8  bl GenerateRivalCheckpointMessage(iface, scoring)            [X] no body in the tree
//   0x82399CFC  lwz 0x1C0 ; cmpwi 0 ; bne -> mode 0: bl DetectCrashes(iface, crash queue) @0x82399D14
//                                           else:   bl DetectOnlineCrashes(iface, crash queue, f1)
//                                                   @0x82399D20, then
//                                                   bl RemoveCrashingMessagesForTakendownPlayers(
//                                                   takedown queue) @0x82399D2C
//   0x82399D44  bl GenerateFirstOrLastMessage(scoring, f1, player, iface)    [X] no body in the tree
//   0x82399D54  bl GenerateDistanceToFinishMessage(scoring, player)          [X] no body in the tree
//   0x82399D5C  stb 0, 0x201                  mbPlayerHasJustTriggeredCheckpoint = false
// [X] NOT REPRODUCED, named rather than faked: the five sibling generators marked above (0x82394110,
// 0x82394258, 0x82394338, 0x82395760, 0x82395A88) post actions 242..248 and read members this
// layout does not carry yet (+0x204..+0x21C, +0x250/+0x254). Their GUI consumers
// (TranslateGameActionsToGuiEvents cases 242..248) are not mounted either, so nothing downstream
// changes. DELETE-WHEN those bodies land: they slot in at the positions above.
void HUDMessageLogic::GenerateRaceModeMessages(
    const StuntModeScoring::ActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    ScoringSystem* lpScoringSystem,
    const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpRaceCarCrashQueue,
    const InputBuffer::TakedownEventQueue* lpTakedownQueue,
    EActiveRaceCarIndex lePlayerRaceCarIndex,
    f32 lfTimeStep)
{
    // (GenerateLeaderMessages / GenerateFinisherMessage -- see the banner.)

    GeneratePlayerCheckpointMessage();

    // (GenerateRivalCheckpointMessage -- see the banner.)

    if (meCurrentGameModeType == GameStateModuleIO::E_MODE_OFFLINE_RACE)   // lwz 0x1C0 ; cmpwi 0
    {
        DetectCrashes(lpActiveRaceCarInterface, lpRaceCarCrashQueue);
    }
    else
    {
        DetectOnlineCrashes(lpActiveRaceCarInterface, lpRaceCarCrashQueue, lfTimeStep);
        RemoveCrashingMessagesForTakendownPlayers(lpTakedownQueue);
    }

    // (GenerateFirstOrLastMessage / GenerateDistanceToFinishMessage -- see the banner.)

    mbPlayerHasJustTriggeredCheckpoint = false;                          // stb 0, 0x201
}

// DWARF BrnHUDMessageLogic.h:150; inlined into GenerateRaceModeMessages on the X360:
//   0x82399CB4  lbz r11, 0x201(r31) ; beq -> skip          mbPlayerHasJustTriggeredCheckpoint
//   0x82399CC0  ld 0x1F0 -> std var+0x00                    mCurrentPlayerCheckpointID
//   0x82399CD8  ld 0x1F8 -> std var+0x08                    mNextPlayerCheckpointID
//   0x82399CE0  lbz 0x200 -> stb var+0x10                   mbIsLastCheckpoint
//   0x82399CE8  AddEvent(var, 0xF9, 0x18)
// The latch is NOT dropped here: GenerateRaceModeMessages drops it after its last generator.
void HUDMessageLogic::GeneratePlayerCheckpointMessage()
{
    if (mbPlayerHasJustTriggeredCheckpoint)
    {
        GameStateModuleIO::HUDMessagePlayerReachesCheckpointAction lCheckpointAction;
        lCheckpointAction.mThisLandmarkID          = mCurrentPlayerCheckpointID;
        lCheckpointAction.mNextLandmarkID          = mNextPlayerCheckpointID;
        lCheckpointAction.mbIsPenultimatedLandmark = mbIsLastCheckpoint;

        // The typed overload: liSize == sizeof(HUDMessagePlayerReachesCheckpointAction) == 24 == `li r6, 0x18`.
        mActionQueue.AddEvent(&lCheckpointAction, GameStateModuleIO::E_ACTION_HUD_MESSAGE_PLAYER_REACHES_CHECKPOINT);
    }
}

// X360 0x82394418 (DWARF BrnHUDMessageLogic.h:160 / .cpp:707).
//   0x82394434  lwz r23, 8(r27)            the queue length, read ONCE before the loop
//   0x82394468  bl GetEvent(i)             (BaseEventQueue<RaceCarCrashEvent>, 0x822ACAE0)
//   0x82394474  ld / rldicl 32 / extrwi 14,8   the event's entity index == the crashed active slot
//   0x8239446C  lwz 0x2858 ; cmpwi -1      the inlined GetPlayerActiveRaceCarIndex ("Player car
//                                          index hasn't been set", :980); `cmpw ; beq` skips the player
//   0x823944A8  the inlined GetRivalId(idx) (:824/:825) -> `ldx` maRivalIds[idx] @+0x2630
//   0x8239450C  AddEvent(var, 0xFA, 0x10)  {+0 rival id, +8 index}
// No primary-crash test (+0x38 is never read), no active or Showtime gate: every non-player crash
// event of the frame becomes one record.
void HUDMessageLogic::DetectCrashes(
    const StuntModeScoring::ActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpRaceCarCrashQueue)
{
    const s32 liRaceCarCrashQueueLength = lpRaceCarCrashQueue->GetLength();

    for (s32 liCrashedRaceCarQueueIndex = 0; liCrashedRaceCarQueueIndex < liRaceCarCrashQueueLength;
         ++liCrashedRaceCarQueueIndex)
    {
        const BrnPhysics::Vehicle::RaceCarCrashEvent& lRaceCarCrashEvent =
            lpRaceCarCrashQueue->GetEvent(liCrashedRaceCarQueueIndex);
        const EActiveRaceCarIndex leCrashedCarIndex = static_cast<EActiveRaceCarIndex>(
            lRaceCarCrashEvent.mRaceCarVolumeInstanceID.GetEntityIDEntityIndex());

        if (leCrashedCarIndex != lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex())
        {
            GameStateModuleIO::HUDMessageXCrashesAction lCrashingEvent;
            lCrashingEvent.mRivalID            = lpActiveRaceCarInterface->GetRivalId(leCrashedCarIndex);
            lCrashingEvent.meRivalRaceCarIndex = leCrashedCarIndex;

            // The typed overload: liSize == sizeof(HUDMessageXCrashesAction) == 16 == `li r6, 0x10`.
            mActionQueue.AddEvent(&lCrashingEvent, GameStateModuleIO::E_ACTION_HUD_MESSAGE_X_CRASHES);

            // [DIAG] NOT IN THE X360 BINARY -- the `[hud-xcrash]` witness (BRN_MODEMGR_DIAG, first 20
            // lines): proves the offline race arm dispatched this body and what it posted.
            static const bool sbHudCrashDiag = (getenv("BRN_MODEMGR_DIAG") != 0);
            if (sbHudCrashDiag && CgsDev::Log::gpDebugPrint != 0)
            {
                static s32 siHudCrashLines = 0;
                if (siHudCrashLines < 20)
                {
                    ++siHudCrashLines;
                    *CgsDev::Log::gpDebugPrint
                        << "[hud-xcrash] action 250 posted: crashed slot " << static_cast<s32>(leCrashedCarIndex)
                        << " (event " << liCrashedRaceCarQueueIndex << " of " << liRaceCarCrashQueueLength
                        << ", mode " << static_cast<s32>(meCurrentGameModeType) << ", hud queue length "
                        << mActionQueue.GetLength() << ")\n";
                }
            }
        }
    }
}

// X360 0x82394528 (DWARF BrnHUDMessageLogic.h:166 / .cpp:748).
// First loop (queue length read ONCE, `lwz r24, 8(r26)` @0x82394558), for each non-player event:
//   0x823945E0  bl ObjectPool::AllocateObject (this+0x110)
//   0x823945E8  cmpwi 0 ; bge -> store, else assert "liAllocatedIndex >= 0" (:804) and skip
//   0x8239465C  ldx maRivalIds[idx] (the inlined GetRivalId, :824/:825)
//   0x82394670  std id, 0(obj) ; 0x82394684 stw idx, 0xC(obj) ; 0x82394690 stfs 1.5 (flt_82029F18), 8(obj)
// Second loop, every slot j < 8 that IsObjectAllocated:
//   0x82394728  lhzx 2*(idx+0x13C0) ; clrlwi 31    the inlined IsRaceCarActive (:854/:855)
//               clear -> FreeObject(j) @0x82394818
//   0x8239474C  fsubs (timer -= step) ; 0x8239475C fcmpu vs flt_82001CC0 (0.0) ; bge -> keep
//   0x823947BC  lbzx at the SAME halfword address = its high byte, bit 0x0100 -- the inlined
//               IsCarInShowtime (:938/:939); set -> skip the post
//   0x8239480C  AddEvent({id, idx}, 0xFA, 0x10) ; then FreeObject(j)
void HUDMessageLogic::DetectOnlineCrashes(
    const StuntModeScoring::ActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    const VehicleManagerOutputInterface::RaceCarCrashEventQueue* lpRaceCarCrashQueue,
    f32 lfSimTimeStep)
{
    const s32 liRaceCarCrashQueueLength = lpRaceCarCrashQueue->GetLength();

    for (s32 liCrashedRaceCarQueueIndex = 0; liCrashedRaceCarQueueIndex < liRaceCarCrashQueueLength;
         ++liCrashedRaceCarQueueIndex)
    {
        const BrnPhysics::Vehicle::RaceCarCrashEvent& lRaceCarCrashEvent =
            lpRaceCarCrashQueue->GetEvent(liCrashedRaceCarQueueIndex);
        const EActiveRaceCarIndex leCrashedCarIndex = static_cast<EActiveRaceCarIndex>(
            lRaceCarCrashEvent.mRaceCarVolumeInstanceID.GetEntityIDEntityIndex());

        if (leCrashedCarIndex != lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex())
        {
            const s32 liAllocatedIndex = mBufferedCrashingCars.AllocateObject();
            CGS_ASSERT(liAllocatedIndex >= 0, "liAllocatedIndex >= 0");   // BrnHUDMessageLogic.cpp:804

            // `bge` @0x823945EC: the failure arm branches past the stores to the loop tail
            // (`b 0x82394694` @0x82394608), so a full pool drops the crash.
            if (liAllocatedIndex >= 0)
            {
                mBufferedCrashingCars[liAllocatedIndex].mRivalID =
                    lpActiveRaceCarInterface->GetRivalId(leCrashedCarIndex);
                mBufferedCrashingCars[liAllocatedIndex].meActiveRaceCarIndex  = leCrashedCarIndex;
                mBufferedCrashingCars[liAllocatedIndex].mfTimeUntilUnbuffered = KF_CRASH_MESSAGE_BUFFER_SECONDS;
            }
        }
    }

    for (s32 liIndex = 0; liIndex < KI_NUM_BUFFERED_CRASHING_CARS; ++liIndex)
    {
        if (!mBufferedCrashingCars.IsObjectAllocated(liIndex))
        {
            continue;
        }

        if (!lpActiveRaceCarInterface->IsRaceCarActive(mBufferedCrashingCars[liIndex].meActiveRaceCarIndex))
        {
            mBufferedCrashingCars.FreeObject(liIndex);
            continue;
        }

        mBufferedCrashingCars[liIndex].mfTimeUntilUnbuffered -= lfSimTimeStep;
        if (mBufferedCrashingCars[liIndex].mfTimeUntilUnbuffered < 0.0f)   // flt_82001CC0; fcmpu/bge
        {
            if (!lpActiveRaceCarInterface->IsCarInShowtime(mBufferedCrashingCars[liIndex].meActiveRaceCarIndex))
            {
                GameStateModuleIO::HUDMessageXCrashesAction lCrashingEvent;
                lCrashingEvent.mRivalID            = mBufferedCrashingCars[liIndex].mRivalID;
                lCrashingEvent.meRivalRaceCarIndex = mBufferedCrashingCars[liIndex].meActiveRaceCarIndex;
                mActionQueue.AddEvent(&lCrashingEvent, GameStateModuleIO::E_ACTION_HUD_MESSAGE_X_CRASHES);
            }
            mBufferedCrashingCars.FreeObject(liIndex);
        }
    }
}

// X360 0x82366590 (DWARF BrnHUDMessageLogic.h:233 / .cpp:826).
//   0x823665A4  lwz r11, 8(r27) -- and again @0x82366610 on every outer iteration: the length is
//               re-read, not cached
//   0x823665BC  bl GetEvent(i) (BaseEventQueue<TakedownEvent>, 0x822AC660) ; lwz r29, 4(r3)
//               == TakedownEvent::meVictimIndex
//   0x823665D0  for j < 8: IsObjectAllocated(j) ; lwz 0xC(obj) ; cmpw victim ; bne ->
//               FreeObject(j) @0x82366600
void HUDMessageLogic::RemoveCrashingMessagesForTakendownPlayers(const InputBuffer::TakedownEventQueue* lpTakedownQueue)
{
    for (s32 liEventIndex = 0; liEventIndex < lpTakedownQueue->GetLength(); ++liEventIndex)
    {
        const TakedownEvent* lpTakedownEvent = &lpTakedownQueue->GetEvent(liEventIndex);
        const EActiveRaceCarIndex leVictimRaceCarIndex = lpTakedownEvent->meVictimIndex;

        for (s32 liIndex = 0; liIndex < KI_NUM_BUFFERED_CRASHING_CARS; ++liIndex)
        {
            if (mBufferedCrashingCars.IsObjectAllocated(liIndex) &&
                mBufferedCrashingCars[liIndex].meActiveRaceCarIndex == leVictimRaceCarIndex)
            {
                mBufferedCrashingCars.FreeObject(liIndex);
            }
        }
    }
}

// X360 0x82394DF8. THE stunt scorer's notification pump -- and the ONLY consumer in the whole
// image of the three one-shot latches StuntModeScoring arms: mbRecentCombo (+0x64's sibling
// +0x80), mbRecentStunt (+0x64) and the time-up edge. Neither WasStuntRecentlyPerformed
// (0x82313280) nor WasComboRecentlyPerformed (0x823132D0) has a single direct xref in the image;
// both are reached from here, the first through the scorer's vtable slot +0x18
// (`lwz r11,0(r31) / lwz r11,0x18(r11) / bctrl` @0x82394EBC..0x82394ED0).
//
// ⛔ THIS IS WHY StuntModeScoring::UpdateBufferedScore CAN OPEN WITH `CGS_ASSERT(!mbRecentStunt)`.
// The console arms the latch inside UpdateBufferedScore during ModeManager::PostWorldUpdate's
// per-mode scorer fork (0x8234AD2C) and drains it, later in that same PostWorldUpdate, through
// HUDMessageLogic::PostWorldUpdate (0x8234B0E8) -> here. Set and consumed inside one frame, so
// the next frame's UpdateBufferedScore always finds it false. With this function absent the latch
// is write-only and that assert fires on the SECOND stunt banked in any offline stunt race.
//
// X360, branch for branch:
//   * the scorer is picked off the LATCHED mode (`lwz r11, 0x1C0(r30)`): 12/14/17 -> the online
//     scorer at lpScoringSystem+0x2620, anything else -> the offline one at +0x350. Both are
//     reached BY NAME here (GetOnlineStuntScorer / GetStuntScorer), never by offset.
//   * CGS_ASSERT(lpStuntModeScoring, "lpStuntModeScoring")   (BrnHUDMessageLogic.cpp:1228)
//   * the three queries are an ELSE-IF CHAIN: at most one notification is emitted per frame, and
//     the stunt query is not even called on a frame the combo query fires. That ordering is
//     load-bearing and is reproduced exactly -- see the note below.
//   * combo arm : AddEvent(record, 133, 12) then AddEvent(&miCurrentScore, 20, 4).
//   * stunt arm : score = (s32)mfComboScore * miComboMultiplier + miCurrentScore
//                 (`lfs 0x20 / fctiwz / lwz 0x24 / lwz 0x10 / mullw / add`), then
//                 AddEvent(stuntInfo, 132, 24) then AddEvent(&score, 20, 4).
//                 GetComboScore() IS that fctiwz truncation (BrnStuntModeScoring_Queries.cpp:366).
//   * time arm  : AddEvent(<uninitialised byte>, 134, 1).
//
// ⓘ ON THE ELSE-IF: a frame where BOTH mbRecentCombo and mbRecentStunt were armed would leave the
// stunt latch set and trip UpdateBufferedScore next frame. The console cannot reach that state --
// mbRecentCombo is armed by EndCombo (0x823215D8), which clears mbComboInProgress, and the
// UpdateBufferedScore path that arms mbRecentStunt asserts mbComboInProgress at
// BrnStuntModeScoring_UpdatePass.cpp:437 immediately after arming it. The two are mutually
// exclusive by that invariant, which is why this stays an else-if and does NOT become three
// independent drains: turning it into three would silence a real tripwire.
void HUDMessageLogic::GenerateStuntMessage(ScoringSystem* lpScoringSystem)
{
    const bool lbOnlineStuntFamily =
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FREE_BURN) ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_FUGITIVE)  ||
        (meCurrentGameModeType == GameStateModuleIO::E_MODE_ONLINE_MODE_END);

    StuntModeScoring* lpStuntModeScoring = lbOnlineStuntFamily
                                             ? lpScoringSystem->GetOnlineStuntScorer()   // ss+0x2620
                                             : lpScoringSystem->GetStuntScorer();        // ss+0x350
    CGS_ASSERT(lpStuntModeScoring != NULL, "lpStuntModeScoring");

    ComboPerformedMessage lComboMessage;
    if (lpStuntModeScoring->WasComboRecentlyPerformed(&lComboMessage.miComboScore,
                                                      &lComboMessage.mbValidCombo,
                                                      &lComboMessage.mfComboTime))
    {
        ScoreUpdateMessage lScoreMessage;
        lScoreMessage.miScore = lpStuntModeScoring->GetCurrentScore();   // lwz 0x10

        mActionQueue.AddEvent(&lComboMessage, E_HUD_MESSAGE_COMBO_PERFORMED,
                              sizeof(ComboPerformedMessage));
        mActionQueue.AddEvent(&lScoreMessage, E_HUD_MESSAGE_SCORE_UPDATE,
                              sizeof(ScoreUpdateMessage));
        return;
    }

    StuntPerformedMessage lStuntMessage;
    if (lpStuntModeScoring->WasStuntRecentlyPerformed(&lStuntMessage.mStuntInfo))
    {
        ScoreUpdateMessage lScoreMessage;
        lScoreMessage.miScore = lpStuntModeScoring->GetComboScore()          // (s32)mfComboScore
                                  * lpStuntModeScoring->GetComboMultiplier() // miComboMultiplier
                              + lpStuntModeScoring->GetCurrentScore();       // miCurrentScore

        mActionQueue.AddEvent(&lStuntMessage, E_HUD_MESSAGE_STUNT_PERFORMED,
                              sizeof(StuntPerformedMessage));
        mActionQueue.AddEvent(&lScoreMessage, E_HUD_MESSAGE_SCORE_UPDATE,
                              sizeof(ScoreUpdateMessage));
        return;
    }

    if (lpStuntModeScoring->WasTimeRecentlyUp())
    {
        StuntTimeUpMessage lTimeUpMessage;
        mActionQueue.AddEvent(&lTimeUpMessage, E_HUD_MESSAGE_STUNT_TIME_UP,
                              sizeof(StuntTimeUpMessage));
    }
}

// X360 0x82395BA8 (DWARF BrnHUDMessageLogic.h:199 / .cpp:1450). The road-rage critical-damage
// notification. Reproduced statement for statement from the asm:
//   0x82395BB8  lwz   r11, 0x2858(r4)        mePlayerActiveRaceCarIndex
//   0x82395BC0  cmpwi r11, -1 / mulli 0x460 / lbz 0x77A(r11)   == the inlined
//                                            RCEntityActiveRaceCarOutputInterface::IsPlayerCarCrashing()
//                                            (invalid index -> false; else maRaceCarStates[i].mbCrashing)
//   0x82395BE8  lbz   r11, 0x4B54(r31)       ss.mRoadRageModeScoring.mbDamageCriticalMessageNeedToBeSent
//                                            (== GetRoadRageScoring()->DoesDamageCriticalMessageNeedToBeSent();
//                                             0x4B54 sits directly under miMaximumPlayerCrashedNumber @0x4B58)
//   0x82395BF4  li r11,1 / stb var_20 / li r6,1 / li r5,0x34 / bl AddEvent
//   0x82395C10  stb   0, 0x4B54(r31)         ResetDamageCriticalMessageFlag()
// Only the crash gate and the flag are read -- the scorer's crash counters
// (GetPlayerCrashesRemaining etc.) are NOT consulted here.
void HUDMessageLogic::GenerateCriticalDamageMessage(
    const StuntModeScoring::ActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
    ScoringSystem* lpScoringSystem)
{
    if (lpActiveRaceCarInterface->IsPlayerCarCrashing())
    {
        return;
    }

    RoadRageModeScoring* lpRoadRageScoring = lpScoringSystem->GetRoadRageScoring();
    if (lpRoadRageScoring->DoesDamageCriticalMessageNeedToBeSent())
    {
        GameStateModuleIO::DamageCriticalMessageAction lDamageAction;
        lDamageAction.mbPlayerCarIsDamageCritical = true;

        // The typed overload: liSize == sizeof(DamageCriticalMessageAction) == 1 == `li r6, 1`.
        mActionQueue.AddEvent(&lDamageAction, GameStateModuleIO::E_ACTION_DAMAGE_CRITICAL);

        lpRoadRageScoring->ResetDamageCriticalMessageFlag();
    }
}

// X360 0x82394A78. When a pending "rival eliminated" car is recorded, build the
// team-eliminated notification: if the eliminated rival's whole team is now out (and the
// team had more than one player) report the team, otherwise report the individual rival's
// network id. Clears the pending slot afterwards so the message fires once.
void HUDMessageLogic::GenerateOnlineStuntRunEliminationMessages(
    ScoringSystem* lpScoringSystem, EActiveRaceCarIndex leLocalPlayerIndex)
{
    // Suppress once a victory has been declared this round.
    if (miLastVictoryTeam != 0)
    {
        return;
    }

    const EActiveRaceCarIndex leEliminatedCar = meEliminationRaceCarIndex;
    if (leEliminatedCar == -1)
    {
        return;
    }

    const GameStateModuleIO::EPlayerTeam leEliminatedTeam =
        lpScoringSystem->GetPlayerTeam(leEliminatedCar);

    StuntRunTeamMessage lMessage;
    if (lpScoringSystem->IsTeamEliminated(static_cast<s32>(leEliminatedTeam))
        && lpScoringSystem->GetTeamPlayerCount(static_cast<s32>(leEliminatedTeam)) > 1)
    {
        // The whole team is out -- report the team.
        lMessage.miTeam = static_cast<s32>(leEliminatedTeam);
        lMessage.mNetworkPlayerID = -1;
    }
    else
    {
        // Report the individual eliminated rival.
        const CarData* lpCarData = lpScoringSystem->GetCarData(meEliminationRaceCarIndex);
        CGS_ASSERT(lpCarData != NULL, "lpCarData != NULL");

        lMessage.mNetworkPlayerID = lpCarData->GetNetworkPlayerID();
        lMessage.miTeam = 0;
    }

    lMessage.mbLocalPlayerIsSubject = (meEliminationRaceCarIndex == leLocalPlayerIndex);

    const GameStateModuleIO::EPlayerTeam leLocalTeam =
        lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex);
    meEliminationRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
    lMessage.mbLocalPlayerOnTeam = (static_cast<s32>(leLocalTeam) == static_cast<s32>(leEliminatedTeam));

    mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_ELIMINATION, sizeof(StuntRunTeamMessage));
}

// X360 0x82394838. Once every other team is eliminated, declare the leading stunt team the
// victor (single-player team -> report that player, otherwise report the team). Latches the
// victory team into miLastVictoryTeam so it fires once.
void HUDMessageLogic::GenerateOnlineStuntRunVictoryMessages(
    ScoringSystem* lpScoringSystem, EActiveRaceCarIndex leLocalPlayerIndex)
{
    if (miLastVictoryTeam != 0)
    {
        return;
    }

    const s32 liLeadingTeam = lpScoringSystem->GetLeadingStuntTeam(0);
    if (!lpScoringSystem->AreAllOtherTeamsEliminated(liLeadingTeam))
    {
        return;
    }

    StuntRunTeamMessage lMessage;
    if (lpScoringSystem->GetTeamPlayerCount(liLeadingTeam) == 1)
    {
        lMessage.miTeam = 0;
        lMessage.mbLocalPlayerOnTeam = false;
        lMessage.mbLocalPlayerIsSubject =
            (static_cast<s32>(lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex)) == liLeadingTeam);
        lMessage.mNetworkPlayerID = lpScoringSystem->GetFirstTeamPlayer(liLeadingTeam);
    }
    else
    {
        lMessage.miTeam = liLeadingTeam;
        lMessage.mNetworkPlayerID = -1;
        lMessage.mbLocalPlayerIsSubject = false;
        lMessage.mbLocalPlayerOnTeam =
            (static_cast<s32>(lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex)) == liLeadingTeam);
    }

    miLastVictoryTeam = liLeadingTeam;
    mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_VICTORY, sizeof(StuntRunTeamMessage));
}

// X360 0x82394920. While the timer is active and at least ten seconds into the event,
// announce a change in the leading stunt team (single-player team -> report that player,
// otherwise report the team). Latches miLastLeadingTeam so it fires only on a change.
void HUDMessageLogic::GenerateOnlineStuntRunLeadingMessages(
    ScoringSystem* lpScoringSystem, EActiveRaceCarIndex leLocalPlayerIndex,
    const CgsSystem::Time& lCurrentTime)
{
    if (!lpScoringSystem->IsTimeLimitActive())
    {
        return;
    }

    const CgsSystem::Time lRemaining = lpScoringSystem->GetModeTimeRemaining(lCurrentTime);
    const f32 lfElapsed = KF_ONLINE_STUNT_RUN_MODE_TIME_LIMIT - lRemaining.GetFloatVal();

    if (lfElapsed <= KF_LEADING_MIN_ELAPSED_SECONDS || miLastVictoryTeam != 0)
    {
        return;
    }

    const s32 liLeadingTeam = lpScoringSystem->GetLeadingStuntTeam(0);
    if (liLeadingTeam == miLastLeadingTeam)
    {
        return;
    }

    StuntRunTeamMessage lMessage;
    if (lpScoringSystem->GetTeamPlayerCount(liLeadingTeam) == 1)
    {
        lMessage.miTeam = 0;
        lMessage.mbLocalPlayerOnTeam = false;
        lMessage.mbLocalPlayerIsSubject =
            (static_cast<s32>(lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex)) == liLeadingTeam);
        lMessage.mNetworkPlayerID = lpScoringSystem->GetFirstTeamPlayer(liLeadingTeam);
    }
    else
    {
        lMessage.miTeam = liLeadingTeam;
        lMessage.mbLocalPlayerOnTeam =
            (static_cast<s32>(lpScoringSystem->GetPlayerTeam(leLocalPlayerIndex)) == liLeadingTeam);
        lMessage.mbLocalPlayerIsSubject = false;
        lMessage.mNetworkPlayerID = -1;
    }

    miLastLeadingTeam = liLeadingTeam;
    mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_LEADING, sizeof(StuntRunTeamMessage));
}

// X360 0x82394B88. Two time-based notifications while the timer is active: (1) when the
// remaining time enters the combo-warning window and a combo is in progress, announce the
// combo is about to end; (2) when the remaining time drops below 31s, announce the 30-second
// warning once. Each announcement latches mfLastTimeWarningAnnounced.
void HUDMessageLogic::GenerateOnlineStuntRunTimeMessages(
    ScoringSystem* lpScoringSystem, const CgsSystem::Time& lCurrentTime, f32 lfComboWarningTime)
{
    if (!lpScoringSystem->IsTimeLimitActive())
    {
        return;
    }

    const CgsSystem::Time lRemaining = lpScoringSystem->GetModeTimeRemaining(lCurrentTime);
    const f32 lfRemainingSeconds = lRemaining.GetFloatVal();

    if (lfRemainingSeconds > KF_ZERO_SECONDS && lfRemainingSeconds <= lfComboWarningTime)
    {
        if (lpScoringSystem->GetOnlineStuntScorer()->IsComboInProgress())
        {
            StuntRunComboEndMessage lMessage;
            mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_COMBO_END, sizeof(StuntRunComboEndMessage));
            mfLastTimeWarningAnnounced = KF_ZERO_SECONDS;
        }
    }

    if (lfRemainingSeconds < KF_TIME_WARNING_WINDOW_SECONDS
        && mfLastTimeWarningAnnounced != KF_TIME_WARNING_SECONDS)
    {
        StuntRunTimeMessage lMessage;
        lMessage.mfWarningTimeSeconds = KF_TIME_WARNING_SECONDS;
        mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_TIME, sizeof(StuntRunTimeMessage));
        mfLastTimeWarningAnnounced = KF_TIME_WARNING_SECONDS;
    }
}

// X360 0x82394CC0. When the watched rival's stunt score crosses the 1,000,000-point
// milestone this frame, announce it with that rival's network id; then clear the watch slot.
void HUDMessageLogic::GenerateOnlineStuntRunScoreMessages(ScoringSystem* lpScoringSystem)
{
    const s32 KI_SCORE_MILESTONE = 1000000;

    const EActiveRaceCarIndex leWatchedCar = miScoreMessageRaceCarIndex;
    if (leWatchedCar == -1)
    {
        return;
    }

    if (miScoreSampleThisFrame >= KI_SCORE_MILESTONE && miScoreSampleLastFrame < KI_SCORE_MILESTONE)
    {
        const CarData* lpCarData = lpScoringSystem->GetCarData(leWatchedCar);
        CGS_ASSERT(lpCarData != NULL, "lpCarData != NULL");

        StuntRunScoreMessage lMessage;
        lMessage.mNetworkPlayerID = lpCarData->GetNetworkPlayerID();
        lMessage.miScore = KI_SCORE_MILESTONE;
        mActionQueue.AddEvent(&lMessage, E_HUD_MESSAGE_STUNT_RUN_SCORE, sizeof(StuntRunScoreMessage));
    }

    miScoreMessageRaceCarIndex = static_cast<EActiveRaceCarIndex>(-1);
}

// X360 0x8231E498. Flag that the given active-race-car has changed team this round. The
// X360 inlines BitArray<8>::SetBit; its bounds guard fires the dynamic CgsBitArray.h:222
// "Index: N, Number of bits: 8" StrStream assert, reduced here to the static expression
// per the committed BrnGameStateSharedIO.cpp convention. (The X360 returns `this`; that is
// a calling-convention artifact, dropped for this void method.)
void HUDMessageLogic::OnlineTeamChange(EActiveRaceCarIndex leActiveRaceCarIndex)
{
    CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
               "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
    CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");

    CGS_ASSERT(static_cast<u32>(leActiveRaceCarIndex) < E_ACTIVE_RACE_CAR_INDEX_COUNT,
               "luIndex < NUMBITS");

    mTeamChangedBits.SetBit(static_cast<u32>(leActiveRaceCarIndex));
}
}
