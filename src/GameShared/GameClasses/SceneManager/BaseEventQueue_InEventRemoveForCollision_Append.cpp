#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveForCollision.h" // InEventRemoveForCollision element (8-byte u64)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>::Append
//   @ X360 0x827A6008 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 \"mpEvents != NULL\" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * asserts no overflow (CgsBaseEventQueue.h:414 \"Base event queue overflow\" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) summed vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy always runs);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     \"mpEvents != NULL\" tripwire on lSource, `lwz r11,0(r30)`; bne skips);
//   * XMemCpy(this->mpEvents + this->miLength, lSource.mpEvents, sizeof(T) * lSource.miLength)
//     at an 8-BYTE element stride -- `slwi r5,r29,3` (count*8) and `slwi r11,r11,3` (dest
//     offset miLength*8) both shift by 3, confirming sizeof(InEventRemoveForCollision) == 8,
//     matching the element's committed header (single u64 muCollisionId) and the sibling
//     EventQueue<InEventRemoveForCollision,1536>::Construct instantiation (KI_LENGTH==1536);
//     modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// This is the Append member of the EventQueue<InEventRemoveForCollision,1536> family whose
// Construct instantiation lives in EventQueue_InEventRemoveForCollision_1536.cpp; called from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Append. The sibling AddEvent
// (BaseEventQueue<T>::AddEvent, X360 0x822AB2C8) is a separate dossier function not covered here.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>&);
