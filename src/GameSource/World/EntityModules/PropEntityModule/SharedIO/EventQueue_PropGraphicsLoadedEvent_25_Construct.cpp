#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                          // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropGraphicsAndZoneEvents.h" // BrnWorld::PropEntityIO::PropGraphicsLoadedEvent (2-byte element)

// CgsModule::EventQueue<BrnWorld::PropEntityIO::PropGraphicsLoadedEvent, 25>::Construct @ 0x822E4E30
//   (ledger id: class:BrnWorld::PropEntityIO::PropGraphicsLoadedEvent,25>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The
// fixed-capacity (N = 25) PropGraphicsLoadedEvent queue: the 25 events live inline in the
// derived EventQueue's maEvents[25] buffer, and Construct() points the base queue at that
// inline storage, sets the capacity and clears the live count. The generic
// EventQueue<T,N>::Construct body is already inline in CgsEventQueue.h; this TU is the thin
// explicit instantiation the X360 emitted out-of-line for the PropGraphicsLoadedEvent/25
// specialisation.
//
// X360 store-for-store (asm at 0x822E4E30):
//   addi  r30, this, 0xC        ; lpEventBuffer = &maEvents (this + 12)
//   (assert lpEventBuffer != NULL @ CgsBaseEventQueue.h:160 -- vacuous: &maEvents is never
//    null; the Hex-Rays `result == -12` is a misread of this addi+cmplwi)
//   stw   r30,  0(this)         ; mpEvents     = &maEvents
//   li    r11, 0x19 ; stw r11, 4(this)  ; miMaxLength = 25
//   li    r10, 0    ; stw r10, 8(this)  ; miLength    = 0
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 25. The mpEvents
// store landing at this+0xC (no pad) confirms the 2-byte PropGraphicsLoadedEvent sits inline
// at offset 12, immediately past the {T* @0, s32 @4, s32 @8} header. Element stride 2 is
// pinned by PropGraphicsLoadedEvent>::AddEvent @ 0x822C9120 (`lhz`/`sthx`, index << 1).
//
// Callers (X360): BrnWorld::WorldEntityIO::OutputBuffer_PreScene::Construct and
// BrnWorld::PropEntityIO::InputBuffer_PreScene::Construct.
template void
CgsModule::EventQueue<BrnWorld::PropEntityIO::PropGraphicsLoadedEvent, 25>::Construct();
