#include "GameShared/GameClasses/Module/CgsEventQueue.h"                            // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestFine.h" // InEventVolumeTestFine element (224B)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestFine, 1>::Construct
//   @ 0x8222DC38   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventVolumeTestFine,1>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 1) fine-volume-test input queue: the single InEventVolumeTestFine
// record lives inline in the derived EventQueue's maEvents[1] buffer, and Construct() points
// the base queue at that inline storage, sets the capacity and clears the live count. The
// generic EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h; this TU is the
// thin explicit instantiation the X360 emitted out-of-line for the InEventVolumeTestFine/1
// specialisation. Reached from BrnDirector::DirectorIO::SceneQueryOutputBuffer::Construct.
//
// X360 store-for-store (asm at 0x8222DC38), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10           ; lpEventBuffer = &maEvents (this + 16; alignas(16)
//                                  ;   element -> 12-byte header rounds 12->16)
//   cmplwi r30, 0; bne .store      ; assert lpEventBuffer != NULL (vacuous)
//   stw r30, 0(this)               ; mpEvents    = &maEvents
//   li  r11, 1;  stw r11, 4(this)  ; miMaxLength = 1
//   li  r10, 0;  stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 1. The element
// stride (224, X360-attested off the sibling Append @ 0x823C2410) lives in the element home
// CgsSceneManagerIO_EventVolumeTestFine.h; Construct never reads the element interior.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestFine, 1>::Construct();
