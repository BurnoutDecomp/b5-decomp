#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"

// =============================================================================
// BrnGameState::GameStateModuleIO::GameStateToGuiInterface -- the interface's own .cpp home.
//
// Created 2026-08-26 (stunt-races wave B, MOUNT-CLOSURE round). The owning header has always
// named this TU as where its publishers belong ("bodied by this interface's own TU
// (BrnGameStateToGuiIOInterfaces.cpp, not yet reconstructed)"), and the DecFIGS DWARF carries the
// file itself (references/DecFIGS/dwarfdump/GameSource/GameState/SharedIO/
// BrnGameStateToGuiIOInterfaces.cpp), so this is the file's real home rather than a convenience
// seat. Only the members the mounted event core actually calls are reconstructed here; the
// remaining publishers (AddTookLeadEvent / AddTookLastEvent -- no console caller: the image has no
// AddEvent for either queue and no store to interface +0x120 / +0x140), Clear() and
// GetPlayerRaceCarIndex stay declared-only in the header until they have live callers to check
// them against. The eight const queue accessors are bodied (FX-GS2, G10-D11 part 2) for their one
// reader, BrnGameModule's TranslateGuiInterfaceToGuiEvents.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   AddFinishedRaceEvent  @ 0x8236EA60
//   Construct             @ 0x82379908   (added 2026-08-27, stunt-races frontier round 2 -- see D2)
//   AddDirtyTrickEnding   (no own body; inlined into PaybackManager::ProcessDirtyTrickEventQueue)
//   AddDirtyTrickTriggered (no own body; inlined into PaybackManager::HandleTriggeringPayback)
//   AddNewDirtyTrick      (no own body; inlined into PaybackManager::HandleAwardingPayback)
//   AddOnTailEvent        (no own body; inlined into GameStateModule::CheckForTailingRivals)
//   AddOvertakeEvent      (no own body; inlined into GameStateModule::EmmPreWorldUpdate @0x8238F324)
//   the eight const Get*Queue accessors (no own bodies; inlined into
//                          BrnGameModule::TranslateGuiInterfaceToGuiEvents @0x823E1D90)
//   AppendRaceCarCrashes  @ 0x82379980   (FX-GS2 2026-09-23, G11-D5, with Construct's ninth leg)
//   SetPlayerRaceCarIndex (no own body; inlined into GameStateModule::PreWorldUpdate @0x823A5594)
// =============================================================================

namespace BrnGameState
{
namespace GameStateModuleIO
{

// -----------------------------------------------------------------------------
// Construct (X360 @ 0x82379908, DWARF BrnGameStateToGuiIOInterfaces.h:65) -- seed the player
// index and point every notification queue at its own inline storage.
//
// ⭐ ADDED 2026-08-27 (stunt-races frontier round 2, defect D2). Until now this member was
// declared-only, and NOTHING in the tree ran it: OutputBuffer::Construct's own checklist listed
// "GameStateToGuiInterface::Construct (this + 17488)" under "STILL NOT MADE" because the member
// was opaque storage there. The interface's queues were therefore permanently
// mpEvents == NULL / miMaxLength == 0, and the first publisher to fire -- ModeManager::
// FinishCurrentMode's AddFinishedRaceEvent, at the very end of the first stunt run -- wrote
// through a null buffer pointer. RUN EVIDENCE scratch/flow_run/20260827_134528/BrnGame.log:
//     [ASSERT 30517] mpEvents != NULL (CgsBaseEventQueue.h:35)
//     [ASSERT 30518] EventQueue::AddEvent - Reached Max length (CgsBaseEventQueue.h:36)
//     [EXCEPTION] EXCEPTION_ACCESS_VIOLATION ... access violation WRITING 0x0000000000000000
//         AddFinishedRaceEvent + 0x83 <- FinishCurrentMode + 0x38A <- ModeManager::PreWorldUpdate
// -- textbook assert-is-not-a-guard: BaseEventQueue<T>::AddEvent appends UNCONDITIONALLY (both
// asserts are non-gating tripwires, exactly as on the console), so the two fired asserts fell
// straight through into `mpEvents[0] = lEvent` on a null pointer. The fix is the console's own
// missing Construct, not a guard at the publisher.
//
// The console body is a flat construct list, quoted whole (0x82379908..0x8237996C):
//     li   r11, -1 ; stw r11, 0(r31)                    miPlayerRaceCarIndex = -1
//     addi r3, r31, 4     -> GameStateToGuiNewDirtyTrick_4_::Construct
//     addi r3, r31, 0x40  -> GameStateToGuiTriggeredDirtyTrick_4_::Construct
//     addi r3, r31, 0x7C  -> GameStateToGuiEndingDirtyTrick_4_::Construct
//     addi r3, r31, 0xC8  -> GameStateToGuiOvertakeEvent_4_::Construct
//     addi r3, r31, 0xF4  -> GameStateToGuiFinishedRaceEvent_4_::Construct
//     addi r3, r31, 0x120 -> GameStateToGuiTookLeadEvent_1_::Construct
//     addi r3, r31, 0x140 -> GameStateToGuiTookLastEvent_1_::Construct
//     addi r3, r31, 0x160 -> GameStateToGuiOnTailEvent_7_::Construct
//     addi r3, r31, 0x1E0 -> BrnPhysics::Vehicle::RaceCarCrashEvent_8_::Construct
//
// ⚠️ THE -1 IS NOT A ZERO-FILL. miPlayerRaceCarIndex is seeded to the INVALID active-race-car
// index (::E_ACTIVE_RACE_CAR_INDEX_INVALID == -1, BurnoutConstants.h:10), the same "no car yet"
// idle value OutputBuffer::Construct stamps on meActivePaybackAggressor. A value-initialised
// buffer would publish player index 0 -- a REAL car slot -- to the GUI until somebody called
// SetPlayerRaceCarIndex.
//
// ⭐ THE NINE OFFSETS ARE A WHOLE-STRUCT LAYOUT PROOF, and they land on the committed header's
// layout member for member. Walking the header's own element sizes against the console's
// `addi` constants:
//     +0     miPlayerRaceCarIndex                                             ->    4
//     +4     mNewDirtyTrickQueue        12 + 4 * 12                ==  60     ->   64 = 0x40  ✓
//     +64    mDirtyTrickTriggeredQueue  12 + 4 * 12                ==  60     ->  124 = 0x7C  ✓
//     +124   mDirtyTrickEndingQueue     12 + 4 * 16                ==  76     ->  200 = 0xC8  ✓
//     +200   mOvertakeEventQueue        12 + 4 *  8                ==  44     ->  244 = 0xF4  ✓
//     +244   mFinishedRaceEventQueue    12 + 4 *  8                ==  44     ->  288 = 0x120 ✓
//     +288   mTookLeadEventQueue        12 + 1 * 16 (+pad)         ==  32     ->  320 = 0x140 ✓
//     +320   mTookLastEventQueue        12 + 1 * 16 (+pad)         ==  32     ->  352 = 0x160 ✓
//     +352   mOnTailEventQueue          12 + 7 * 16 (+pad)         == 128     ->  480 = 0x1E0 ✓
//     +480   mRaceCarCrashEventQueue    12 + 8 * 64                == 524     -> 1004 -> 1008
// The header had only ever been pinned as far as +244 (BrnPaybackManager to +124,
// AddFinishedRaceEvent to +244). This body pins the whole thing, including the trailing crash
// queue, and 1008 is exactly the OutputBuffer span 0x4840-0x4450 the member occupies.
//
// ✅ THE NINTH LEG IS MADE (FX-GS2 2026-09-23, crash-parity G11-D5). mRaceCarCrashEventQueue used
// to be the documented opaque tail maRaceCarCrashEventQueueStorage[524], so this body could not
// construct it; the header now types it as the console's EventQueue<RaceCarCrashEvent,8>, and the
// leg is built here BEFORE its producer (AppendRaceCarCrashes, below) is wired -- the order the old
// banner asked for, so the null-buffer AddEvent crash of D2 cannot return one queue along.
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::Construct()
{
    miPlayerRaceCarIndex = static_cast<s32>(::E_ACTIVE_RACE_CAR_INDEX_INVALID);  // `li r11,-1; stw r11,0(r31)`

    mNewDirtyTrickQueue.Construct();        // this + 4
    mDirtyTrickTriggeredQueue.Construct();  // this + 0x40
    mDirtyTrickEndingQueue.Construct();     // this + 0x7C
    mOvertakeEventQueue.Construct();        // this + 0xC8
    mFinishedRaceEventQueue.Construct();    // this + 0xF4
    mTookLeadEventQueue.Construct();        // this + 0x120
    mTookLastEventQueue.Construct();        // this + 0x140
    mOnTailEventQueue.Construct();          // this + 0x160
    mRaceCarCrashEventQueue.Construct();    // this + 0x1E0 (0x82379964: RaceCarCrashEvent_8_::Construct)
}

// -----------------------------------------------------------------------------
// AddFinishedRaceEvent (X360 @ 0x8236EA60) -- publish "this car finished the race, like so" to
// the GUI. Two fields, one queue append.
//
// The console body is short enough to quote whole, and it leaves nothing to infer:
//     0x8236EA6C  mr   r11, r4                 ; a2 == leFinishType
//     0x8236EA70  stw  r5, var_C(r1)           ; a3 == leActiveRaceCarIndex -> stack record +4
//     0x8236EA74  addi r4, r1, var_10          ; &stack record
//     0x8236EA78  addi r3, r3, 0xF4            ; this + 244  == the queue this appends to
//     0x8236EA7C  stw  r11, var_10(r1)         ; leFinishType             -> stack record +0
//     0x8236EA80  bl   BrnGameState__GameStateToGuiFinishedRaceEvent___AddEvent
// i.e. it builds a two-word GameStateToGuiFinishedRaceEvent on the stack -- meFinishType at +0,
// meActiveRaceCarIndex at +4, matching this header's declaration order -- and hands it to
// BaseEventQueue<GameStateToGuiFinishedRaceEvent>::AddEvent. The callee is the per-instantiation
// out-of-line AddEvent the X360 emits; on the host that body is the generic inline in
// CgsBaseEventQueue.h, so the call is spelled as the member call and the explicit-instantiation
// ledger TU (EventQueue_GameStateToGuiFinishedRaceEvent_4.cpp) remains the record of it. AddEvent
// appends unconditionally there, exactly as it does on the console; its overflow assert is a
// non-gating tripwire.
//
// THE +0xF4 IS AN INDEPENDENT CONFIRMATION OF THIS HEADER'S WHOLE FRONT HALF, not just of one
// member. 244 is where mFinishedRaceEventQueue falls out of the committed layout when you walk it
// from the top with the DWARF's own element sizes:
//     +0    miPlayerRaceCarIndex                                        s32          ->   4
//     +4    mNewDirtyTrickQueue        base 12 + 4 * 12 (3 enums)       == 60        ->  64
//     +64   mDirtyTrickTriggeredQueue  base 12 + 4 * 12                 == 60        -> 124
//     +124  mDirtyTrickEndingQueue     base 12 + 4 * 16 (+ bool)        == 76        -> 200
//     +200  mOvertakeEventQueue        base 12 + 4 *  8 (u8 + enum)     == 44        -> 244
//     +244  mFinishedRaceEventQueue    <- the console's `addi r3, r3, 0xF4`
// The header had previously only pinned as far as +124 (from the BrnPaybackManager bodies, which
// inline the two dirty-trick publishers). This body extends the same chain two members further
// with a third, unrelated call site, and it lands on the nose -- so the OvertakeEventQueue sizing
// in between is corroborated rather than assumed.
//
// The mounted caller is BrnModeManager_Finish.cpp:669, which passes the GLOBAL ::EActiveRaceCarIndex
// (that file's own :56 banner records the choice) and a finish type cast from its file-local
// KI_FINISH_TYPE_* constants (bound to BrnGui::EFinishType's DWARF enumerators, BrnGameStateToGuiEvents.h).
// The enum is carried through here untouched and unvalidated, exactly as the console carries it
// (no assert of any kind in the X360 body).
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::AddFinishedRaceEvent(BrnGui::EFinishType leFinishType,
                                                   ::EActiveRaceCarIndex leActiveRaceCarIndex)
{
    GameStateToGuiFinishedRaceEvent lEvent;
    lEvent.meFinishType         = leFinishType;           // record +0x00  (`stw r11, var_10`)
    lEvent.meActiveRaceCarIndex = leActiveRaceCarIndex;   // record +0x04  (`stw r5,  var_C`)

    mFinishedRaceEventQueue.AddEvent(lEvent);             // this + 244    (`addi r3, r3, 0xF4`)
}

// -----------------------------------------------------------------------------
// AddDirtyTrickEnding (declared in BrnGameStateToGuiIOInterfaces.h) -- publish "this dirty trick
// ended" to the GUI. No out-of-line console body: PaybackManager::ProcessDirtyTrickEventQueue
// inlines it at both of its call sites (dirty-trick status 3 and 4). Each site builds the
// 16-byte record on the stack -- aggressor at +0x0, victim at +0x4, trick type at +0x8, the
// survived byte at +0xC (1 for status 3, 0 for status 4) -- and appends it to the queue at
// interface +0x7C, i.e. mDirtyTrickEndingQueue.
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::AddDirtyTrickEnding(::EActiveRaceCarIndex leAggressor,
                                                  ::EActiveRaceCarIndex leVictim,
                                                  BrnNetwork::EPaybackType leTrickType,
                                                  bool lbSurvived)
{
    GameStateToGuiEndingDirtyTrick lEvent;
    lEvent.meAggressorActiveRaceCarIndex = leAggressor;   // record +0x0
    lEvent.meVictimActiveRaceCarIndex    = leVictim;      // record +0x4
    lEvent.meTrickType                   = leTrickType;   // record +0x8
    lEvent.mbSurvived                    = lbSurvived;    // record +0xC

    mDirtyTrickEndingQueue.AddEvent(lEvent);              // this + 0x7C
}

// -----------------------------------------------------------------------------
// AddDirtyTrickTriggered (DWARF BrnGameStateToGuiIOInterfaces.h:83) -- publish "a dirty trick was
// triggered" to the GUI. [FX-GS 2026-09-23, crash-parity G12-D2] No out-of-line console body:
// PaybackManager::HandleTriggeringPayback @0x82397C08 inlines it at 0x82397C58..0x82397C74:
//     bl   0x8231D8A8            ; OutputBuffer::GetGameStateToGuiInterface (write lock)
//     addi r3, r3, 0x40          ; mDirtyTrickTriggeredQueue
//     stw  r27, var_48           ; aggressor  -> record +0x0
//     stw  r28, var_44           ; victim     -> record +0x4
//     stw  r29, var_40           ; trick type -> record +0x8
//     bl   GameStateToGuiTriggeredDirtyTrick AddEvent 0x82368940   (a 12-byte copy, length++)
// The PS3 twin (DecFIGS 0x2592B8, PaybackManager::DirtyTrickTriggered) calls this member by name.
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::AddDirtyTrickTriggered(::EActiveRaceCarIndex leAggressor,
                                                     ::EActiveRaceCarIndex leVictim,
                                                     BrnNetwork::EPaybackType leTrickType)
{
    GameStateToGuiTriggeredDirtyTrick lEvent;
    lEvent.meAggressorActiveRaceCarIndex = leAggressor;   // record +0x0
    lEvent.meVictimActiveRaceCarIndex    = leVictim;      // record +0x4
    lEvent.meTrickType                   = leTrickType;   // record +0x8

    mDirtyTrickTriggeredQueue.AddEvent(lEvent);           // this + 0x40
}

// -----------------------------------------------------------------------------
// AddNewDirtyTrick (DWARF BrnGameStateToGuiIOInterfaces.h:76) -- publish "a dirty trick was
// awarded" to the GUI. [FX-GS2 2026-09-23, crash-parity G12-D6] No out-of-line console body:
// PaybackManager::HandleAwardingPayback @0x82397970 inlines it (through DirtyTrickAwarded) at
// 0x82397A4C..0x82397A64:
//     bl   0x8231D8A8            ; OutputBuffer::GetGameStateToGuiInterface (write lock)
//     addi r3, r3, 4             ; mNewDirtyTrickQueue
//     stw  r28, var_50           ; aggressor  -> record +0x0
//     stw  r29, var_4C           ; victim     -> record +0x4
//     stw  r30, var_48           ; trick type -> record +0x8
//     bl   GameStateToGuiNewDirtyTrick AddEvent 0x823687E8   (a 12-byte copy, length++)
// The PS3 twin (DecFIGS 0x258614, PaybackManager::DirtyTrickAwarded) appends the same record to
// mNewDirtyTrickQueue.
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::AddNewDirtyTrick(::EActiveRaceCarIndex leAggressor,
                                               ::EActiveRaceCarIndex leVictim,
                                               BrnNetwork::EPaybackType leTrickType)
{
    GameStateToGuiNewDirtyTrick lEvent;
    lEvent.meAggressorActiveRaceCarIndex = leAggressor;   // record +0x0
    lEvent.meVictimActiveRaceCarIndex    = leVictim;      // record +0x4
    lEvent.meTrickType                   = leTrickType;   // record +0x8

    mNewDirtyTrickQueue.AddEvent(lEvent);                 // this + 4
}

// -----------------------------------------------------------------------------
// AddOnTailEvent (DWARF BrnGameStateToGuiIOInterfaces.h:121) -- publish "this rival is on the
// player's tail" to the GUI. [FX-GS2 2026-09-23, crash-parity G10-D11] No out-of-line console
// body: GameStateModule::CheckForTailingRivals @0x82375F90 inlines it at 0x82376350..0x82376378:
//     bl   RCEntityActiveRaceCarOutputInterface::GetRivalId  ; the rival's car id (u64)
//     bl   0x8231D8A8            ; OutputBuffer::GetGameStateToGuiInterface (write lock)
//     stw  r30, var_B8           ; the slot  -> record +0x8
//     addi r3, r3, 0x160         ; mOnTailEventQueue
//     std  r31, var_C0           ; the id    -> record +0x0
//     bl   GameStateToGuiOnTailEvent AddEvent 0x82368E80   (a 16-byte copy, length++)
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::AddOnTailEvent(CgsID lOfflineRivalCarID, ::EActiveRaceCarIndex leActiveRaceCarIndex)
{
    GameStateToGuiOnTailEvent lEvent;
    lEvent.mOfflineRivalCarID         = lOfflineRivalCarID;     // record +0x0
    lEvent.meOnTailActiveRaceCarIndex = leActiveRaceCarIndex;   // record +0x8

    mOnTailEventQueue.AddEvent(lEvent);                         // this + 0x160
}

// -----------------------------------------------------------------------------
// AddOvertakeEvent (DWARF BrnGameStateToGuiIOInterfaces.h:97) -- publish "the player gained a
// place" to the GUI. [FX-FLOW 2026-09-24, crash-parity NEW-EMMTAIL] No out-of-line console body:
// GameStateModule::EmmPreWorldUpdate @0x8238EF50 inlines it at 0x8238F324..0x8238F33C:
//     bl   0x8231D8A8            ; OutputBuffer::GetGameStateToGuiInterface (write lock)
//     addi r3, r3, 0xC8          ; mOvertakeEventQueue
//     stb  r31, var_88           ; the new race position (u8) -> record +0x0
//     stw  r30, var_84           ; the car slot               -> record +0x4
//     bl   GameStateToGuiOvertakeEvent AddEvent 0x82368BF0   (an 8-byte copy, length++)
// Its one reader is TranslateGuiInterfaceToGuiEvents (GUI 371).
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::AddOvertakeEvent(u8 lu8NewPosition, ::EActiveRaceCarIndex leActiveRaceCarIndex)
{
    GameStateToGuiOvertakeEvent lEvent;
    lEvent.mu8NewPosition       = lu8NewPosition;         // record +0x0
    lEvent.meActiveRaceCarIndex = leActiveRaceCarIndex;   // record +0x4

    mOvertakeEventQueue.AddEvent(lEvent);                 // this + 0xC8
}

// -----------------------------------------------------------------------------
// AppendRaceCarCrashes (X360 @ 0x82379980, DWARF BrnGameStateToGuiIOInterfaces.h:126) -- copy the
// frame's race-car crash events into the interface's own crash queue. [FX-GS2 2026-09-23,
// crash-parity G11-D5] The console body, whole:
//     0x8237999C  cmplwi r31, 0 ; bne          ; lpRaceCarCrashEventQueue == NULL ->
//     0x823799A4  BeginAssert / FireAssert("lpRaceCarCrashEventQueue", line 0xF7 = 247) / EndAssert
//     0x823799C4  mr r4, r31 ; addi r3, r30, 0x1E0
//     0x823799CC  bl RaceCarCrashEvent_::Append      ; mRaceCarCrashEventQueue.Append(*queue)
// The assert is a non-gating tripwire: the Append follows either way, exactly as here. Its one
// caller is GameStateModule's pre-world pump, in online modes only (console 0x823A5544..0x823A55A0).
// Nothing in the image reads the queue back.
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::AppendRaceCarCrashes(
    const CgsModule::EventQueue<BrnPhysics::Vehicle::RaceCarCrashEvent, 8>* lpRaceCarCrashEventQueue)
{
    CGS_ASSERT(lpRaceCarCrashEventQueue != 0, "lpRaceCarCrashEventQueue");   // line 247
    mRaceCarCrashEventQueue.Append(*lpRaceCarCrashEventQueue);                // this + 0x1E0
}

// -----------------------------------------------------------------------------
// SetPlayerRaceCarIndex (DWARF BrnGameStateToGuiIOInterfaces.h:137). No out-of-line console body:
// GameStateModule::PreWorldUpdate inlines it as `stw r28, 0(r11)` @0x823A5594, r11 being the
// write-locked GetGameStateToGuiInterface() and r28 GetPlayerActiveRaceCarIndex().
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::SetPlayerRaceCarIndex(s32 liPlayerRaceCarIndex)
{
    miPlayerRaceCarIndex = liPlayerRaceCarIndex;   // this + 0
}

// -----------------------------------------------------------------------------
// The eight const queue accessors (DWARF BrnGameStateToGuiIOInterfaces.h:128-135). [FX-GS2
// 2026-09-23, crash-parity G10-D11 part 2] No out-of-line console bodies: their one reader,
// BrnGameModule::TranslateGuiInterfaceToGuiEvents @0x823E1D90, inlines all eight in its prologue
// as plain address arithmetic on the interface pointer (r5), 0x823E1D9C..0x823E1DC4:
//     addi r30, r5, 4       ; GetNewDirtyTrickQueue
//     addi r28, r5, 0x40    ; GetDirtyTrickTriggeredQueue
//     addi r25, r5, 0x7C    ; GetDirtyTrickEndingQueue
//     addi r22, r5, 0xC8    ; GetOvertakeEventQueue
//     addi r20, r5, 0xF4    ; GetFinishedRaceEventQueue
//     addi r18, r5, 0x120   ; GetTookLeadEventQueue
//     addi r15, r5, 0x140   ; GetTookLastEventQueue
//     addi r11, r5, 0x160   ; GetOnTailEventQueue
// -- the same eight offsets Construct walks above, so each accessor is `return &member;`.
// -----------------------------------------------------------------------------
const GameStateToGuiInterface::NewDirtyTrickQueue* GameStateToGuiInterface::GetNewDirtyTrickQueue() const
{
    return &mNewDirtyTrickQueue;         // this + 4
}

const GameStateToGuiInterface::DirtyTrickTriggeredQueue* GameStateToGuiInterface::GetDirtyTrickTriggeredQueue() const
{
    return &mDirtyTrickTriggeredQueue;   // this + 0x40
}

const GameStateToGuiInterface::DirtyTrickEndingQueue* GameStateToGuiInterface::GetDirtyTrickEndingQueue() const
{
    return &mDirtyTrickEndingQueue;      // this + 0x7C
}

const GameStateToGuiInterface::OvertakeEventQueue* GameStateToGuiInterface::GetOvertakeEventQueue() const
{
    return &mOvertakeEventQueue;         // this + 0xC8
}

const GameStateToGuiInterface::FinishedRaceEventQueue* GameStateToGuiInterface::GetFinishedRaceEventQueue() const
{
    return &mFinishedRaceEventQueue;     // this + 0xF4
}

const GameStateToGuiInterface::TookLeadEventQueue* GameStateToGuiInterface::GetTookLeadEventQueue() const
{
    return &mTookLeadEventQueue;         // this + 0x120
}

const GameStateToGuiInterface::TookLastEventQueue* GameStateToGuiInterface::GetTookLastEventQueue() const
{
    return &mTookLastEventQueue;         // this + 0x140
}

const GameStateToGuiInterface::OnTailEventQueue* GameStateToGuiInterface::GetOnTailEventQueue() const
{
    return &mOnTailEventQueue;           // this + 0x160
}

}
}
