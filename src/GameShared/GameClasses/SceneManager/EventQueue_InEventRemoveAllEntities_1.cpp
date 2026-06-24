#include "GameShared/GameClasses/Module/CgsEventQueue.h"                              // CgsModule::EventQueue<T, N>::Construct (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveAllEntities.h" // InEventRemoveAllEntities element (1-byte marker)

// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveAllEntities, 1>::Construct
//   @ X360 0x822E2720 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveAllEntities,1>").
//
// Fixed-capacity (1) remove-all-entities input-queue instantiation: the derived
// EventQueue<T,1>::Construct points the BaseEventQueue<T> base at its inline maEvents
// buffer and sets the capacity/length. The generic EventQueue<T,N>::Construct body is
// already inline in CgsEventQueue.h; this TU is the thin explicit instantiation the X360
// emitted out-of-line for the InEventRemoveAllEntities/1 specialisation.
//
// X360 store-for-store (asm at 0x822E2720):
//   addi r30, this, 0xC             ; lpEventBuffer = &maEvents (this + 12; the element is
//                                   ;   byte-aligned, so the 12-byte header needs no padding)
//   cmplwi r30, 0; bne .store       ; assert lpEventBuffer != NULL (CgsBaseEventQueue.h:160) --
//                                   ;   vacuous (&maEvents never null); the Hex-Rays `result == -12`
//                                   ;   is a misread of this addi+cmplwi
//   li r11, 1;  stw r11, 4(this)    ; miMaxLength = 1
//   stw r30, 0(this)                ; mpEvents    = &maEvents
//   li r10, 0;  stw r10, 8(this)    ; miLength    = 0
//   return this
// == BaseEventQueue<T>::Construct(maEvents, KI_LENGTH) with KI_LENGTH == 1.
//
// Just Construct is in this TU's ledger (n_funcs == 1); the queue's Append lives in the
// separate InEventRemoveAllEntities> base-template TU
// (BaseEventQueue_InEventRemoveAllEntities_Append.cpp). Reached from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Construct.
template void
CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveAllEntities, 1>::Construct();
