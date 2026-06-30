#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddDynamicVolume.h" // InEventAddDynamicVolume (144-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume>::Append
//   @ X360 0x827A5B88 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is the
// thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) -> add vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy always runs);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips);
//   * XMemCpy(this->mpEvents + this->miLength*144, lSource.mpEvents, sizeof(T) * lSource.miLength)
//     at a 144-byte element stride -- `slwi r9,r29,3; add r9,r29,r9` == lSource.miLength*9, then
//     `slwi r5,r9,4` == (*9)*16 == *144 (count); same *9 then *16 math on `r11` (this->miLength)
//     for the destination byte offset (`add r3,r11,r10` with r10 == mpEvents); modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 144-byte stride matches sizeof(InEventAddDynamicVolume) == 144 (0x90: 8-byte muId +
// 1-byte mu8Flags + 7-byte pad + 128-byte opaque volume blob), the X360-attested stride per
// CgsSceneManagerIO_EventAddDynamicVolume.h and re-derived directly off this Append's slwi/add math.
// This is the Append member of the EventQueue<InEventAddDynamicVolume,1280> family whose Construct
// instantiation lives in EventQueue_InEventAddDynamicVolume_1280.cpp; called from
// InSceneUpdateInterface::Append.
//
// X360-attested element stride (`slwi *3; add` == *9, then `slwi *4` == *16, total *144).
static_assert(sizeof(CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume) == 144,
              "InEventAddDynamicVolume stride 144");

template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventAddDynamicVolume>&);
