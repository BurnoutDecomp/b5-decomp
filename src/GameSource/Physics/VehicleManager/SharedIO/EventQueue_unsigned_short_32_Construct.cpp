#include "GameShared/GameClasses/Module/CgsEventQueue.h"   // CgsModule::EventQueue<T, N>::Construct (inline generic)

// =============================================================================
// CgsModule::EventQueue<unsigned short, 32>::Construct   @ 0x822E32F0
//   (ledger id: class:short,32> -- a demangling collision with Array<u16,32>;
//    this method belongs to the u16 EventQueue generic, NOT the Array generic.)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The fixed-capacity (N = 32) u16
// event queue: the 32 u16 events live inline in the derived EventQueue's
// maEvents[32] buffer (right after the 12-byte BaseEventQueue header), and
// Construct() points the base queue at that inline storage, sets the capacity
// and clears the live count. The generic EventQueue<T,N>::Construct body is
// already inline in CgsEventQueue.h; this TU is the thin explicit instantiation
// the X360 emitted out-of-line for the u16/32 specialisation.
//
// X360 store-for-store (asm at 0x822E32F0):
//   addi  r30, this, 0xC        ; lpEventBuffer = &maEvents (this + 12)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents
//    is never null; the Hex-Rays `result == -12` is a misread of addi+cmplwi)
//   stw   r30,  0(this)         ; mpEvents     = &maEvents
//   li    r11, 0x20 ; stw r11, 4(this)  ; miMaxLength = 32
//   li    r10, 0    ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 32.
// The mpEvents store landing at this+0xC confirms the 2-byte u16 element sits
// inline at offset 12, immediately past the {T* @0, s32 @4, s32 @8} header.
//
// Caller (X360): BrnPhysics::Vehicle::VehicleManagerOutputInterface::Construct.
// u16 is a primitive element -- no element-home include needed.
// =============================================================================
template void CgsModule::EventQueue<unsigned short, 32>::Construct();
