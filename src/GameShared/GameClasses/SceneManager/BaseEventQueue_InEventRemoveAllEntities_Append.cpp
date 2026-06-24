#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveAllEntities.h" // InEventRemoveAllEntities (1-byte marker element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveAllEntities>::Append
//   @ X360 0x827A6E20 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveAllEntities>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy always runs);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips);
//   * XMemCpy(this->mpEvents + this->miLength, lSource.mpEvents, sizeof(T) * lSource.miLength)
//     at a 1-BYTE element stride -- the count (`r5 = r29 = lSource.miLength`) and the dest
//     offset (`r3 = mpEvents + miLength`, `add r3,r11,r10` with NO `slwi` shift on miLength)
//     are UNSCALED, i.e. sizeof(InEventRemoveAllEntities) == 1; modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 1-byte stride confirms the element is a payload-less marker (sizeof 1), the
// X360-attested stride read directly off this very Append (see
// CgsSceneManagerIO_EventRemoveAllEntities.h). This is the Append member of the
// EventQueue<InEventRemoveAllEntities,1> family whose Construct instantiation lives in
// EventQueue_InEventRemoveAllEntities_1.cpp; called from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveAllEntities>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveAllEntities>&);
