#include "GameShared/GameClasses/Module/CgsEventQueue.h"                            // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestQuery.h" // InEventLineTest element (64-byte, alignas(16))

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTest, 256>::Construct
//   @ 0x828C4618   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventLineTest,256>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 256) line-test input queue: the 256 InEventLineTest records
// live inline in the derived EventQueue's maEvents[256] buffer, and Construct() points the
// base queue at that inline storage, sets the capacity and clears the live count. The
// generic EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is
// the thin explicit instantiation the X360 emitted out-of-line for the InEventLineTest/256
// specialisation. Reached from CgsSceneManager::SceneManagerIO::InputBuffer_Query::Construct.
//
// X360 store-for-store (asm at 0x828C4618), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10           ; lpEventBuffer = &maEvents (this + 16; the element is
//                                  ;   alignas(16), so the 12-byte header rounds 12 -> 16)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                  ;   vacuous (&maEvents never null); the Hex-Rays `result == -16`
//                                  ;   is a misread of this addi+cmplwi
//   stw r30, 0(this)               ; mpEvents    = &maEvents        (stw = 32-bit pointer slot)
//   li  r11, 0x100; stw r11, 4(this) ; miMaxLength = 256 (0x100)    (stw = 32-bit)
//   li  r10, 0;     stw r10, 8(this) ; miLength    = 0              (stw = 32-bit)
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 256.
//
// Just Construct is in this TU's ledger (n_funcs == 1); the queue's other members stay
// un-instantiated to match the X360 ledger.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTest, 256>::Construct();
