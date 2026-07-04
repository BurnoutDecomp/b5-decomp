#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestFine.h" // InEventVolumeTestFine (224-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestFine>::Append
//   @ X360 0x823C2410 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventVolumeTestFine>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is the
// thin explicit instantiation. The X360 body matches the generic store-for-store, and is identical
// to the sibling InEventVolumeTestDeepest::Append @ 0x823C2330:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) summed vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating, the copy always runs);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips);
//   * XMemCpy(this->mpEvents + this->miLength, lSource.mpEvents, sizeof(T) * lSource.miLength)
//     at a 224-byte element stride -- `mulli r5,r29,0xE0` (count == lSource.miLength * 224) and
//     `mulli r11,r11,0xE0` (dest offset == this->miLength * 224), `add r3,r11,r10` == mpEvents
//     + miLength*224; modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 224-byte (0xE0) stride matches sizeof(InEventVolumeTestFine) == 0xE0, the X360-attested
// stride read directly off this Append (`mulli ...,0xE0`) and recorded in the element home
// CgsSceneManagerIO_EventVolumeTestFine.h. Called from
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestFine>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventVolumeTestFine>&);
