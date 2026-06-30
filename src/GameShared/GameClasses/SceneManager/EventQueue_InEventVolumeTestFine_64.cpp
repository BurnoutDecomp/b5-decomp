#include "GameShared/GameClasses/Module/CgsEventQueue.h"                            // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestFine.h" // InEventVolumeTestFine element (224B)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestFine, 64>::Construct
//   @ 0x828C4848   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventVolumeTestFine,64>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 64) fine-volume-test input queue: the 64 InEventVolumeTestFine
// records live inline in the derived EventQueue's maEvents[64] buffer, and Construct() points
// the base queue at that inline storage, sets the capacity and clears the live count. The
// generic EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is the
// thin explicit instantiation the X360 emitted out-of-line for the InEventVolumeTestFine/64
// specialisation. Reached from CgsSceneManager::SceneManagerIO::InputBuffer_Query::Construct.
//
// X360 store-for-store (asm at 0x828C4848), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10            ; lpEventBuffer = &maEvents (this + 16; alignas(16)
//                                   ;   element -> 12-byte header rounds 12->16)
//   cmplwi r30, 0; bne .store       ; assert lpEventBuffer != NULL (vacuous)
//   stw r30, 0(this)                ; mpEvents    = &maEvents
//   li  r11, 0x40; stw r11, 4(this) ; miMaxLength = 64 (0x40)
//   li  r10, 0;    stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 64. The element
// stride is sizeof(InEventVolumeTestFine) == 0xE0 (224), X360-attested off the sibling
// Append @ 0x823C2410 (mulli ...,0xE0); it lives in the element home
// CgsSceneManagerIO_EventVolumeTestFine.h. Construct never reads the element interior, so the
// aggregate sizeof == 16 + 64*224 == 14352 follows from that stride.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestFine, 64>::Construct();
