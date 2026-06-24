#include "GameShared/GameClasses/Module/CgsEventQueue.h"                          // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveEntity.h" // InEventRemoveEntity element (8-byte, two u32 words)

// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveEntity, 10000>::Construct
//   @ X360 0x822E1FB0 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveEntity,10000>").
//
// Fixed-capacity (10000) remove-entity input-queue instantiation: the derived
// EventQueue<T,10000>::Construct points the BaseEventQueue<T> base at its inline maEvents
// buffer and sets the capacity/length. The generic EventQueue<T,N>::Construct body is
// already inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the InEventRemoveEntity/10000 specialisation.
//
// X360 store-for-store (asm at 0x822E1FB0):
//   addi r30, this, 0xC             ; lpEventBuffer = &maEvents (this + 12; the element is
//                                   ;   4-byte aligned (two u32 words), so the 12-byte header
//                                   ;   needs no padding)
//   cmplwi r30, 0; bne .store       ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                   ;   vacuous; the Hex-Rays `result == -12` is a misread
//   li r11, 0x2710; stw r11, 4(this) ; miMaxLength = 10000 (0x2710)
//   stw r30, 0(this)                ; mpEvents    = &maEvents
//   li r10, 0;      stw r10, 8(this) ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 10000.
//
// Just Construct is in this TU's ledger (n_funcs == 1); the queue's Append/AddEvent live
// in the separate InEventRemoveEntity> base-template TU. Reached from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct.
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveEntity, 10000>::Construct();
