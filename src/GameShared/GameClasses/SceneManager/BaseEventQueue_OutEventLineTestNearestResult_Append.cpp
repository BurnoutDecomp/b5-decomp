#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"          // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h" // OutEventLineTestNearestResult (64-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>::Append
//   @ X360 0x823C2D98 (dossier id "class:CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>",
//   function "A").
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
//     at a 64-byte element stride -- `slwi r5,r29,6` (count == lSource.miLength * 64) and
//     `slwi r11,r11,6` (dest offset == this->miLength * 64), `add r3,r11,r10` == mpEvents
//     + miLength*64; modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 64-byte stride matches sizeof(OutEventLineTestNearestResult) == 0x40 (alignas(16): two
// Vector3 lanes mPosition/mNormal + VolumeInstanceId/SceneQueryId/EntityId/f32/u16/u16/bool),
// the existing committed element home in CgsSceneManagerModuleIO.h, already used by the
// sibling EventQueue<OutEventLineTestNearestResult,2000>::Construct instantiation
// (EventQueue_OutEventLineTestNearestResult_2000.cpp). Called from
// BrnPhysics::Vehicle::VehicleInputInterface::Append / ::operator=.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>&);