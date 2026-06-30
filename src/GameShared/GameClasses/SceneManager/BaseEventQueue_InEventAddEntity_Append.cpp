#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"               // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddEntity.h" // InEventAddEntity (32-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddEntity>::Append
//   @ X360 0x827A5AA8 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventAddEntity>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is
// the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy
//     always runs);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips);
//   * XMemCpy(mpEvents + miLength, lSource.mpEvents, sizeof(T) * lSource.miLength) at a
//     32-byte element stride -- `slwi r5,r29,5` (count == lSource.miLength * 32) and
//     `slwi r11,r11,5` (dest offset == this->miLength * 32), `add r3,r11,r10` == mpEvents
//     + miLength*32; modelled as std::memcpy via the generic;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 32-byte stride matches sizeof(InEventAddEntity) == 32 (alignas(16) Vector3 lane +
// EntityId + s32 + f32), the X360-attested stride read directly off this Append and the
// sibling AddEvent (`slwi r11, r11, 5`). This is the Append member of the
// EventQueue<InEventAddEntity,5120> family whose Construct instantiation lives in
// EventQueue_InEventAddEntity_5120.cpp; called from InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddEntity>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddEntity>&);
