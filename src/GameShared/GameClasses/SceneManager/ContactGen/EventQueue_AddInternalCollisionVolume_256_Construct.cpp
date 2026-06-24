#include "GameShared/GameClasses/Module/CgsEventQueue.h"                              // CgsModule::EventQueue<T,N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapCullingModuleIO.h" // OverlapCullingIO::AddInternalCollisionVolume

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::OverlapCullingIO::AddInternalCollisionVolume, 256>::Construct
//   @ 0x828C4E20   (ledger id: class:CgsSceneManager::OverlapCullingIO::AddInternalCollisionVolume,256>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, asm widths matched).
// The fixed-capacity (N = 256) internal-volume-add request queue: the 256 events live inline
// in the derived EventQueue's maEvents[256] buffer (right after the 12-byte BaseEventQueue
// header), and Construct() points the base queue at that inline storage, sets the capacity and
// clears the live count. The generic EventQueue<T,N>::Construct body is already inline in
// CgsEventQueue.h; this TU is the thin explicit instantiation the X360 emitted out-of-line.
//
// X360 store-for-store (asm at 0x828C4E20, offsets are the BaseEventQueue header):
//   addi  r30, this, 0xC        ; lpEventBuffer = &maEvents (this + 12)
//   (assert lpEventBuffer != NULL, CgsBaseEventQueue.h:160 -- vacuous: &maEvents is never null;
//    the Hex-Rays `result == -12` is a misread of the addi+cmplwi against &maEvents)
//   stw   r30, 0(this)          ; mpEvents     = &maEvents
//   li    r11, 0x100 ; stw r11, 4(this)  ; miMaxLength = 256
//   li    r10, 0     ; stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 256. The buffer
// pointer lands at this+0xC (NOT 0x10): AddInternalCollisionVolume is only 4-aligned (3 u32),
// so the compiler does not pad the 12-byte BaseEventQueue header up to 16 bytes before maEvents.
//
// Caller (X360): OverlapCullingIO::InputBuffer::Construct constructs this queue (one of the
// InputBuffer's three EventQueues).
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::OverlapCullingIO::AddInternalCollisionVolume, 256>::Construct();
