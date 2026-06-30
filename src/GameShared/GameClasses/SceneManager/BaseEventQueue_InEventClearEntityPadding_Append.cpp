#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventClearEntityPadding.h" // InEventClearEntityPadding (4-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearEntityPadding>::Append
//   @ X360 0x827A6800 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventClearEntityPadding>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is the
// thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * asserts mpEvents != NULL (CgsBaseEventQueue.h:413 "mpEvents != NULL" tripwire,
//     `lwz r11,0(r31)`; bne skips the assert; li r5,0x19D == 413);
//   * asserts no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow" tripwire,
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) `add` vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips the assert -- NON-gating; li r5,0x19E == 414);
//   * reads lSource's backing buffer via GetQueueStartPointer (CgsBaseEventQueue.h:486
//     "mpEvents != NULL" tripwire on lSource, `lwz r11,0(r30)`; bne skips; li r5,0x1E6 == 486),
//     capturing liSourceLength via `lwz r29,8(r30)`;
//   * XMemCpy(this->mpEvents + this->miLength, lSource.mpEvents, sizeof(T) * lSource.miLength)
//     at a 4-byte element stride -- `slwi r5,r29,2` (count == lSource.miLength * 4) and
//     `slwi r11,r11,2` (dest offset == this->miLength * 4), `add r3,r11,r10` == mpEvents
//     + miLength*4; modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`lwz r11,8(r30)`+`lwz r10,8(r31)`+`add`+
//     `stw r11,8(r31)`) and returns 1 (`li r3,1`). Saves r27 (`__savegprlr_27`).
// The 4-byte stride matches sizeof(InEventClearEntityPadding) == 4 (a single u32 EntityId,
// the X360-attested stride read directly off this very Append; element layout already
// committed in CgsSceneManagerIO_EventClearEntityPadding.h). This is the Append member of the
// EventQueue<InEventClearEntityPadding,16> family whose Construct instantiation lives in
// EventQueue_InEventClearEntityPadding_16.cpp; called from
// InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearEntityPadding>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventClearEntityPadding>&);
