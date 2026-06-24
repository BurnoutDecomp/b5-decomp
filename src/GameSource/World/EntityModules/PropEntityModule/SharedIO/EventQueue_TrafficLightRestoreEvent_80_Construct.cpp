#include "GameShared/GameClasses/Module/CgsEventQueue.h"                              // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"     // BrnWorld::PropEntityIO::TrafficLightRestoreEvent (4-byte element)

// CgsModule::EventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent, 80>::Construct @ 0x822E4D50
//   (ledger id: class:BrnWorld::PropEntityIO::TrafficLightRestoreEvent,80>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match). The
// fixed-capacity (N = 80) TrafficLightRestoreEvent queue: the 80 events live inline in the
// derived EventQueue's maEvents[80] buffer, and Construct() points the base queue at that
// inline storage, sets the capacity and clears the live count. The generic
// EventQueue<T,N>::Construct body is already inline in CgsEventQueue.h; this TU is the thin
// explicit instantiation the X360 emitted out-of-line for the TrafficLightRestoreEvent/80
// specialisation.
//
// X360 store-for-store (asm at 0x822E4D50):
//   addi  r30, this, 0xC        ; lpEventBuffer = &maEvents (this + 12)
//   (assert lpEventBuffer != NULL @ CgsBaseEventQueue.h:160 -- vacuous: &maEvents is never
//    null; the Hex-Rays `result == -12` is a misread of this addi+cmplwi)
//   stw   r30,  0(this)         ; mpEvents     = &maEvents
//   li    r11, 0x50 ; stw r11, 4(this)  ; miMaxLength = 80
//   li    r10, 0    ; stw r10, 8(this)  ; miLength    = 0
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 80. The mpEvents
// store landing at this+0xC (not +0x10) confirms NO alignment pad before maEvents: the
// 4-byte-aligned TrafficLightRestoreEvent sits inline at offset 12, directly past the
// {T* @0, s32 @4, s32 @8} header.
//
// Callers (X360): BrnWorld::PropEntityIO::OutputBuffer_PrePhysics::Construct and
// BrnTraffic::BrnTrafficIO::InputBuffer_PrePhysics::Construct.
template void
CgsModule::EventQueue<BrnWorld::PropEntityIO::TrafficLightRestoreEvent, 80>::Construct();
