#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                 // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveVolume.h" // InEventRemoveVolume (8-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveVolume>::Append
//   @ X360 0x827A60E8 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveVolume>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h;
// this is the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (original CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert; `li r5,0x19D` == 413);
//   * asserts no overflow (original CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy always
//     runs; `li r5,0x19E` == 414);
//   * reads lSource's backing buffer via GetQueueStartPointer (original CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips; `li r5,0x1E6` == 486);
//   * XMemCpy(this->mpEvents + 8*this->miLength, lSource.mpEvents, 8*lSource.miLength)
//     at an 8-BYTE element stride -- both the dest offset (`lwz r11,8(r31); slwi r11,r11,3`
//     == miLength*8) and the count (`slwi r5,r29,3` == lSource.miLength*8) are scaled by
//     3 (x8), matching sizeof(InEventRemoveVolume) == 8 (the packed u64 VolumeId; see
//     CgsSceneManagerIO_EventRemoveVolume.h);
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 8-byte stride is X360-attested directly off this very Append and cross-confirmed by
// the sibling AddEvent @0x822AB400 (same TU, `slwi r11,r11,3` + a single 64-bit `stdx`).
// This is the Append member of the EventQueue<InEventRemoveVolume,1344> family whose
// Construct instantiation lives in EventQueue_InEventRemoveVolume_1344.cpp; called from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveVolume>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveVolume>&);
