#include "GameShared/GameClasses/Module/CgsEventQueue.h"                // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerContact.h"  // CgsSceneManager::Contact element (alignas(16))

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::Contact, 16384>::Construct
//   @ 0x828C4F00   (ledger id: class:CgsSceneManager::Contact,16384>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 16384) Contact queue: the 16384 Contact records live
// inline in the derived EventQueue's maEvents[16384] buffer (right after the
// BaseEventQueue header), and Construct() points the base queue at that inline
// storage, sets the capacity and clears the live count. The generic
// EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is
// the thin explicit instantiation the X360 emitted out-of-line for the
// Contact/16384 specialisation.
//
// X360 store-for-store (asm at 0x828C4F00, offsets are the BaseEventQueue header):
//   addi  r30, this, 0x10       ; lpEventBuffer = &maEvents (this + 16)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents is
//    never null; the Hex-Rays `result == -16` is a misread of this addi+cmplwi)
//   stw   r30, 0(this)          ; mpEvents     = &maEvents
//   li    r11, 0x4000 ; stw r11, 4(this)  ; miMaxLength = 16384
//   li    r10, 0      ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 16384, which
// stores mpEvents = maEvents, miMaxLength = 16384, miLength = 0 -- matching the three
// header stores exactly. Here the mpEvents store lands at this+0x10 (NOT 0xC): Contact
// is alignas(16) (it carries vec4 lanes), so the compiler pads the 12-byte
// BaseEventQueue header up to a 16-byte boundary before maEvents -- the generic
// EventQueue<T,N> { BaseEventQueue<T>; T maEvents[N]; } reproduces this offset
// automatically from Contact's 16-byte alignment.
//
// Caller (X360): the Contact queue is constructed via
// IOBufferStack<CgsModule>::CreateIOBuffer<OverlapCullingIO::OutputBuffer> while
// building the OverlapCulling output buffer.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::Contact, 16384>::Construct();
