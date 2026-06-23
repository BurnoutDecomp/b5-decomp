#include "GameShared/GameClasses/Module/CgsEventQueue.h"            // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"          // BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest element

// =============================================================================
// CgsModule::EventQueue<BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest, 12>::Construct
//   @ 0x8278A028   (ledger id: class:BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest,12>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 12) ExtrapolatedRouteRequest queue: the 12 64-byte
// ExtrapolatedRouteRequest records live inline in the derived EventQueue's
// maEvents[12] buffer, and Construct() points the base queue at that inline
// storage, sets the capacity and clears the live count. The generic
// EventQueue<T,N>::Construct body is inline in CgsEventQueue.h; this TU is the
// explicit instantiation the X360 emitted out-of-line for the
// ExtrapolatedRouteRequest/12 specialisation.
//
// X360 store-for-store (asm at 0x8278A028, offsets are the BaseEventQueue header):
//   addi  r30, this, 0x10       ; lpEventBuffer = &maEvents (this + 16)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents is
//    never null; the Hex-Rays `result == -16` is a misread of this addi+cmplwi)
//   stw   r30, 0(this)          ; mpEvents     = &maEvents
//   li    r11, 0xC ; stw r11, 4(this)  ; miMaxLength = 12
//   li    r10, 0   ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 12. The
// mpEvents store lands at this+0x10 (NOT 0xC): ExtrapolatedRouteRequest is 8-byte
// aligned (stride 64, copied as 8 `std` qwords in AddEventSafe @0x8277AFD8), so the
// 12-byte BaseEventQueue header pads up to a 16-byte boundary before maEvents.
//
// Caller (X360): the ExtrapolatedRouteRequest queue is constructed by the route-map
// module's CreateIOBuffer<RouteMapModuleIO::InputBuffer> (0x82791878) input buffer.
// =============================================================================
template void
CgsModule::EventQueue<BrnAI::RouteMapModuleIO::ExtrapolatedRouteRequest, 12>::Construct();
