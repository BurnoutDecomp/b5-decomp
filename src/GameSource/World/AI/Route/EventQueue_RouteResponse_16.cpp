#include "GameShared/GameClasses/Module/CgsEventQueue.h"            // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"          // BrnAI::RouteMapModuleIO::RouteResponse element

// =============================================================================
// CgsModule::EventQueue<BrnAI::RouteMapModuleIO::RouteResponse, 16>::Construct
//   @ 0x8278A098   (ledger id: class:BrnAI::RouteMapModuleIO::RouteResponse,16>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 16) RouteResponse queue: the 16 5136-byte RouteResponse
// records live inline in the derived EventQueue's maEvents[16] buffer, and
// Construct() points the base queue at that inline storage, sets the capacity and
// clears the live count. The generic EventQueue<T,N>::Construct body is inline in
// CgsEventQueue.h; this TU is the explicit instantiation the X360 emitted out-of-line
// for the RouteResponse/16 specialisation.
//
// X360 store-for-store (asm at 0x8278A098, offsets are the BaseEventQueue header):
//   addi  r30, this, 0xC        ; lpEventBuffer = &maEvents (this + 12)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents is
//    never null; the Hex-Rays `result == -12` is a misread of this addi+cmplwi)
//   stw   r30, 0(this)          ; mpEvents     = &maEvents
//   li    r11, 0x10 ; stw r11, 4(this)  ; miMaxLength = 16
//   li    r10, 0    ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 16. The
// mpEvents store lands at this+0xC (the natural 12-byte base, no pad): RouteResponse
// is only 4-byte aligned (stride 5136 = 1284*4, block-copied in AddEvent @0x8277B090
// / Append @0x8277B668), so maEvents starts immediately past the {T*@0, s32@4, s32@8}
// header -- reproduced automatically by EventQueue<T,N>.
//
// Caller (X360): the RouteResponse queue is constructed by the AI module output
// buffer (BrnAI::AIModuleIO::OutputBuffer::Construct) and the route-map module's
// CreateIOBuffer<RouteMapModuleIO::OutputBuffer> (0x82791960).
// =============================================================================
template void
CgsModule::EventQueue<BrnAI::RouteMapModuleIO::RouteResponse, 16>::Construct();
