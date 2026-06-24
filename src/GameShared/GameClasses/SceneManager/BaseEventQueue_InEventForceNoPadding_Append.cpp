#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventForceNoPadding.h" // InEventForceNoPadding (8-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventForceNoPadding>::Append
//   @ X360 0x827A5E48 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventForceNoPadding>").
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
//     at an 8-byte element stride -- `slwi r5,r29,3` (count == lSource.miLength * 8) and
//     `slwi r11,r11,3` (dest offset == this->miLength * 8), `add r3,r11,r10` == mpEvents
//     + miLength*8; modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 8-byte stride matches sizeof(InEventForceNoPadding) == 8 (a single u64 VolumeInstanceId,
// the X360-attested stride read directly off this very Append, see
// CgsSceneManagerIO_EventForceNoPadding.h). This is the Append member of the
// EventQueue<InEventForceNoPadding,64> family whose Construct instantiation lives in
// EventQueue_InEventForceNoPadding_64.cpp; called from
// InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventForceNoPadding>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventForceNoPadding>&);
