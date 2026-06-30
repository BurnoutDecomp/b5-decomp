#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearCullingTable.h" // InEventClearCullingTable (1-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::Append
//   @ X360 0x827A6488 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventClearCullingTable>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is the
// thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy always runs);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips);
//   * XMemCpy(this->mpEvents + this->miLength, lSource.mpEvents, sizeof(T) * lSource.miLength)
//     at a 1-byte element stride -- the asm computes the count directly as `mr r5,r29`
//     (count == lSource.miLength, loaded at 0x827A6508 `lwz r29,8(r30)`, used RAW with NO slwi
//     shift => stride 1) and the dest directly as `add r3,r11,r10` (== mpEvents + miLength, again
//     no shift), confirming sizeof(InEventClearCullingTable) == 1 (single bool mbCullAll member,
//     CgsSceneManagerIO_EventClearCullingTable.h); modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// This is the Append member of the EventQueue<InEventClearCullingTable,64> family whose
// Construct instantiation lives in EventQueue_InEventClearCullingTable_64.cpp; called from
// InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearCullingTable>&);
