#include "GameSource/GameState/SharedIO/BrnGameStateToGuiIOInterfaces.h"

// =============================================================================
// BrnGameState::GameStateModuleIO::GameStateToGuiInterface -- the interface's own .cpp home.
//
// Created 2026-08-26 (stunt-races wave B, MOUNT-CLOSURE round). The owning header has always
// named this TU as where its publishers belong ("bodied by this interface's own TU
// (BrnGameStateToGuiIOInterfaces.cpp, not yet reconstructed)"), and the DecFIGS DWARF carries the
// file itself (references/DecFIGS/dwarfdump/GameSource/GameState/SharedIO/
// BrnGameStateToGuiIOInterfaces.cpp), so this is the file's real home rather than a convenience
// seat. Only the ONE member the mounted event core actually calls is reconstructed here; the
// other eight publishers, the two lifecycle methods and the ten queue/index accessors stay
// declared-only in the header until they have live callers to check them against.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   AddFinishedRaceEvent  @ 0x8236EA60
// (the interface's other two out-of-line X360 symbols, Construct @0x82379908 and
// AppendRaceCarCrashes @0x82379980, are NOT reconstructed here -- neither is an unresolved
// external today, and Construct additionally reaches the opaque trailing crash queue.)
// =============================================================================

namespace BrnGameState
{
namespace GameStateModuleIO
{

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
