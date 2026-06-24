#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h" // InEventLineTestNearest element (64-byte, alignas(16))

// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestNearest, 40>::Construct
//   @ X360 0x8222DA78 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventLineTestNearest,40>").
//
// Fixed-capacity (40) nearest-line-test input-queue instantiation: the derived
// EventQueue<T,40>::Construct points the BaseEventQueue<T> base at its inline maEvents
// buffer and sets the capacity/length. The generic EventQueue<T,N>::Construct body is
// already inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the InEventLineTestNearest/40 specialisation.
//
// X360 store-for-store (asm at 0x8222DA78):
//   addi r30, this, 0x10            ; lpEventBuffer = &maEvents (this + 16; the element is
//                                   ;   alignas(16), so the 12-byte header rounds 12->16)
//   cmplwi r30, 0; bne .store       ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                   ;   vacuous (&maEvents never null); the Hex-Rays `result == -16`
//                                   ;   is a misread of this addi+cmplwi
//   li r11, 0x28; stw r11, 4(this)  ; miMaxLength = 40 (0x28)
//   stw r30, 0(this)                ; mpEvents    = &maEvents
//   li r10, 0;    stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 40.
//
// Just Construct is in this TU's ledger (n_funcs == 1); the queue's other members stay
// un-instantiated to match the X360 ledger (Append/AddEvent live in the separate
// InEventLineTestNearest> base-template TU). Reached from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct.
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestNearest, 40>::Construct();
