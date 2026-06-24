#include "GameShared/GameClasses/Module/CgsEventQueue.h"                                // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveForCollision.h" // InEventRemoveForCollision element (8-byte u64)

// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision, 1536>::Construct
//   @ X360 0x822E2020 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveForCollision,1536>").
//
// Fixed-capacity (1536) remove-for-collision input-queue instantiation: the derived
// EventQueue<T,1536>::Construct points the BaseEventQueue<T> base at its inline maEvents
// buffer and sets the capacity/length. The generic EventQueue<T,N>::Construct body is
// already inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the InEventRemoveForCollision/1536 specialisation.
//
// X360 store-for-store (asm at 0x822E2020):
//   addi r30, this, 0x10            ; lpEventBuffer = &maEvents (this + 16; the element is
//                                   ;   a 64-bit value (alignof 8), so the 12-byte header
//                                   ;   rounds 12->16)
//   cmplwi r30, 0; bne .store       ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                   ;   vacuous; the Hex-Rays `result == -16` is a misread
//   li r11, 0x600; stw r11, 4(this)  ; miMaxLength = 1536 (0x600)
//   stw r30, 0(this)                ; mpEvents    = &maEvents
//   li r10, 0;     stw r10, 8(this)  ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 1536.
//
// Just Construct is in this TU's ledger (n_funcs == 1); the queue's Append/AddEvent live
// in the separate InEventRemoveForCollision> base-template TU. Reached from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct.
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision, 1536>::Construct();
