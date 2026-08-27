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
// other eight publishers, Clear() and the ten queue/index accessors stay declared-only in the
// header until they have live callers to check them against.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   AddFinishedRaceEvent  @ 0x8236EA60
//   Construct             @ 0x82379908   (added 2026-08-27, stunt-races frontier round 2 -- see D2)
// (the interface's remaining out-of-line X360 symbol, AppendRaceCarCrashes @0x82379980, is NOT
// reconstructed here -- it is not an unresolved external today and it reaches the opaque
// trailing crash queue.)
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
// ⚠️ [FLAG PC bring-up] THE LAST LEG IS NOT MADE: mRaceCarCrashEventQueue is still modelled as
// the documented opaque tail maRaceCarCrashEventQueueStorage[524] (its element type,
// BrnPhysics::Vehicle::RaceCarCrashEvent, is owned by the VehicleManager/RaceCarEntityModule
// TUs), so its Construct cannot be spelled from here. That queue has NO producer and NO consumer
// in the tree today -- AppendRaceCarCrashes @0x82379980 is not reconstructed either -- and the
// storage is zero-filled by OutputBuffer's `new T()` value-initialisation, so it is inert rather
// than dangerous. DELETE-WHEN RaceCarCrashEvent is typed here: add the ninth Construct leg in
// this body FIRST, before wiring any producer, or this exact null-buffer crash returns one queue
// along.
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

    // [FLAG PC bring-up] the console's ninth leg,
    //     BrnPhysics::Vehicle::RaceCarCrashEvent_8_::Construct(this + 0x1E0)
    // cannot be made while mRaceCarCrashEventQueue is the opaque tail -- see the banner's ⚠️.
    // DELETE-WHEN RaceCarCrashEvent is a committed type in this header.
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
// KI_FINISH_TYPE_* constants -- BrnGui::EFinishType is still the minimal stub in
// BrnGameStateToGuiEvents.h, so the enum is carried through here untouched and unvalidated,
// exactly as the console carries it (no assert of any kind in the X360 body).
// -----------------------------------------------------------------------------
void GameStateToGuiInterface::AddFinishedRaceEvent(BrnGui::EFinishType leFinishType,
                                                   ::EActiveRaceCarIndex leActiveRaceCarIndex)
{
    GameStateToGuiFinishedRaceEvent lEvent;
    lEvent.meFinishType         = leFinishType;           // record +0x00  (`stw r11, var_10`)
    lEvent.meActiveRaceCarIndex = leActiveRaceCarIndex;   // record +0x04  (`stw r5,  var_C`)

    mFinishedRaceEventQueue.AddEvent(lEvent);             // this + 244    (`addi r3, r3, 0xF4`)
}

}
}
