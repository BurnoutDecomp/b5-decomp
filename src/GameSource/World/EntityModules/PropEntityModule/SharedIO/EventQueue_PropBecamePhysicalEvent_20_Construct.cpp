#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                       // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropBecamePhysicalEvent.h" // BrnWorld::PropEntityIO::PropBecamePhysicalEvent (16-byte element)

// CgsModule::EventQueue<BrnWorld::PropEntityIO::PropBecamePhysicalEvent, 20>::Construct @ 0x822E4F10
//   (ledger id: class:BrnWorld::PropEntityIO::PropBecamePhysicalEvent,20>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The
// fixed-capacity (N = 20) PropBecamePhysicalEvent queue: the 20 events live inline in the
// derived EventQueue's maEvents[20] buffer, and Construct() points the base queue at that
// inline storage, sets the capacity and clears the live count. The generic
// EventQueue<T,N>::Construct body is already inline in CgsEventQueue.h; this TU is the thin
// explicit instantiation the X360 emitted out-of-line for the PropBecamePhysicalEvent/20
// specialisation.
//
// X360 store-for-store (asm at 0x822E4F10):
//   addi  r30, this, 0x10       ; lpEventBuffer = &maEvents (this + 16)
//   (assert lpEventBuffer != NULL @ CgsBaseEventQueue.h:160 -- vacuous: &maEvents is never
//    null; the Hex-Rays `result == -16` is a misread of this addi+cmplwi)
//   stw   r30,  0(this)         ; mpEvents     = &maEvents
//   li    r11, 0x14 ; stw r11, 4(this)  ; miMaxLength = 20
//   li    r10, 0    ; stw r10, 8(this)  ; miLength    = 0
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 20. The mpEvents
// store landing at this+0x10 (not +0xC) confirms a 4-byte alignment pad before maEvents:
// the 16-byte-aligned PropBecamePhysicalEvent (single Vector3 mPosition) sits inline at
// offset 16, past the {T* @0, s32 @4, s32 @8} header + 4 pad.
//
// Callers (X360): the PropBecamePhysicalEvent queue is the embedded buffer constructed by
// BrnWorld::PropEntityIO::OutputBuffer_PostPhysics::Construct,
// BrnSound::Module::Io::RootInputBuffer::Construct, and BrnWorldIO::UpdateOutputBuffer::Construct.
template void
CgsModule::EventQueue<BrnWorld::PropEntityIO::PropBecamePhysicalEvent, 20>::Construct();
