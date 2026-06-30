#include "GameShared/GameClasses/Module/CgsEventQueue.h"                        // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSphereTest.h" // InEventSphereTestFast element (48B)

// =============================================================================
// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSphereTestFast, 10>::Construct
//   @ 0x8222DB58   (ledger id: class:CgsSceneManager::SceneManagerIO::InEventSphereTestFast,10>)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// The fixed-capacity (N = 10) fast sphere-test input queue: the 10 InEventSphereTestFast
// records live inline in the derived EventQueue's maEvents[10] buffer, and Construct()
// points the base queue at that inline storage, sets the capacity and clears the live
// count. The generic EventQueue<T, N>::Construct body is already inline in CgsEventQueue.h;
// this TU is the thin explicit instantiation the X360 emitted out-of-line for the
// InEventSphereTestFast/10 specialisation. Reached from
// BrnDirector::DirectorIO::SceneQueryOutputBuffer::Construct.
//
// X360 store-for-store (asm at 0x8222DB58), offsets are the BaseEventQueue header:
//   addi r30, this, 0x10            ; lpEventBuffer = &maEvents (this + 16; alignas(16)
//                                   ;   element -> 12-byte header rounds 12->16)
//   cmplwi r30, 0; bne .store       ; assert lpEventBuffer != NULL (vacuous)
//   stw r30, 0(this)                ; mpEvents    = &maEvents
//   li  r11, 0xA;  stw r11, 4(this) ; miMaxLength = 10 (0xA)
//   li  r10, 0;    stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 10. The element
// stride (48, X360-attested off the sibling Append @ 0x823C2240) lives in the element home
// CgsSceneManagerIO_EventSphereTest.h; Construct never reads the element interior.
// =============================================================================
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventSphereTestFast, 10>::Construct();
