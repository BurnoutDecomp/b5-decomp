#include "GameShared/GameClasses/Module/CgsEventQueue.h"                       // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h" // InEventLineTestFastDoubleSided element

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestFastDoubleSided, 16>::Construct
//   @ 0x828C46F8   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventLineTestFastDoubleSided,16>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 16) fast-double-sided-line-test input queue: the 16
// InEventLineTestFastDoubleSided records live inline in the derived EventQueue's maEvents[16]
// buffer, and Construct() points the base queue at that inline storage, sets the capacity and
// clears the live count. The generic EventQueue<T, N>::Construct body is already inline in
// CgsEventQueue.h; this TU is the thin explicit instantiation the X360 emitted out-of-line for
// the InEventLineTestFastDoubleSided/16 specialisation. Reached from
// CgsSceneManager::SceneManagerIO::InputBuffer_Query::Construct.
//
// X360 store-for-store (asm at 0x828C46F8), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10           ; lpEventBuffer = &maEvents (this + 16; the element is
//                                  ;   alignas(16), so the 12-byte header rounds 12->16 before maEvents)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -16`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents
//   li  r11, 0x10; stw r11, 4(this) ; miMaxLength = 16 (0x10)
//   li  r10, 0;    stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 16.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestFastDoubleSided, 16>::Construct();
