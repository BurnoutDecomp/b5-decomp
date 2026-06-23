#include "GameShared/GameClasses/Module/CgsEventQueue.h"            // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"          // BrnAI::RouteMapModuleIO::RaceRouteRequest element

// =============================================================================
// CgsModule::EventQueue<BrnAI::RouteMapModuleIO::RaceRouteRequest, 1>::Construct
//   @ 0x82789FB8   (ledger id: class:BrnAI::RouteMapModuleIO::RaceRouteRequest,1>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 1) RaceRouteRequest queue: the single 128-byte
// RaceRouteRequest record lives inline in the derived EventQueue's maEvents[1]
// buffer, and Construct() points the base queue at that inline storage, sets the
// capacity and clears the live count. The generic EventQueue<T,N>::Construct body
// is inline in CgsEventQueue.h; this TU is the explicit instantiation the X360
// emitted out-of-line for the RaceRouteRequest/1 specialisation.
//
// X360 store-for-store (asm at 0x82789FB8, offsets are the BaseEventQueue header):
//   addi  r30, this, 0x10       ; lpEventBuffer = &maEvents (this + 16)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents is
//    never null; the Hex-Rays `result == -16` is a misread of this addi+cmplwi)
//   stw   r30, 0(this)          ; mpEvents     = &maEvents
//   li    r11, 1 ; stw r11, 4(this)  ; miMaxLength = 1
//   li    r10, 0 ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 1. The
// mpEvents store lands at this+0x10 (NOT 0xC): RaceRouteRequest is 8-byte aligned
// (stride 128, copied as 16 `std` qwords in AddEventSafe @0x8277AF20), so the
// 12-byte BaseEventQueue header pads up to a 16-byte boundary before maEvents --
// reproduced automatically by EventQueue<T,N> { BaseEventQueue<T>; T maEvents[N]; }.
//
// Caller (X360): the RaceRouteRequest queue is constructed by the AI module input
// buffer (BrnAI::AIModuleIO::InputBuffer::Construct) and the route-map module's
// CreateIOBuffer<RouteMapModuleIO::InputBuffer>.
// =============================================================================
template void
CgsModule::EventQueue<BrnAI::RouteMapModuleIO::RaceRouteRequest, 1>::Construct();
