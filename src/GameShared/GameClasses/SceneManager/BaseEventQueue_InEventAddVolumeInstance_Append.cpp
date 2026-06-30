#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddVolumeInstance.h" // InEventAddVolumeInstance (80-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>::Append
//   @ X360 0x827A5D58 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>").
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
//   * XMemCpy(this->mpEvents + this->miLength*80, lSource.mpEvents, 80 * lSource.miLength)
//     at an 80-byte element stride -- `slwi r9,r29,2` + `add r9,r29,r9` (=r29*5) then
//     `slwi r5,r9,4` (count == lSource.miLength*5*16 == lSource.miLength*80), and the
//     mirrored dest-offset math `slwi r8,r11,2; add r11,r11,r8; slwi r11,r11,4`
//     (== this->miLength*80); modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 80-byte stride matches sizeof(InEventAddVolumeInstance) == 80 (VolumeInstanceId +
// VolumeId + Matrix44Affine, 16-byte aligned), the X360-attested stride read directly off
// this Append (and shared with the sibling AddEve (AddEvent) member of the same class, which
// uses the identical `80 * miLength` addressing). This is the Append member of the
// EventQueue<InEventAddVolumeInstance,1280> family whose Construct instantiation lives in
// EventQueue_InEventAddVolumeInstance_1280.cpp; called from InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddVolumeInstance>&);
