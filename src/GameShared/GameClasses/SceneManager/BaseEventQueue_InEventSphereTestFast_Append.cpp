#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSphereTest.h"    // InEventSphereTestFast (48-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSphereTestFast>::Append
//   @ X360 0x823C2240 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventSphereTestFast>").
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
//     at a 48-byte element stride -- `slwi r9,r29,1; add r9,r29,r9` (== lSource.miLength*3) then
//     `slwi r5,r9,4` (count == lSource.miLength*48), and the mirrored
//     `slwi r8,r11,1; add r11,r11,r8; slwi r11,r11,4` (dest offset == this->miLength*48),
//     `add r3,r11,r10` == mpEvents + miLength*48; modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 48-byte stride (0x30) is the X360-attested element stride read directly off this Append.
// No field-level DWARF exists for InEventSphereTestFast in this TU's scope, so the element is an
// opaque correctly-sized 48-byte payload (see CgsSceneManagerIO_EventSphereTest.h). This is the
// Append member of the EventQueue<InEventSphereTestFast,N> family; called from
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::Append (sibling
// BaseEventQueue_InEventLineTestFastDoubleSided_Append.cpp @ 0x823C2160 is identical bar stride 64).
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSphereTestFast>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSphereTestFast>&);