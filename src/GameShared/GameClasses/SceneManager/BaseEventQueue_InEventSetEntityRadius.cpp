#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                            // BaseEventQueue<T>::AddEvent / Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventSetEntityRadius.h" // CgsSceneManager::SceneManagerIO::InEventSetEntityRadius (8-byte element)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetEntityRadius>::{AddEvent,Append}
//   AddEvent @ X360 0x822AB928, Append @ X360 0x827A6560
//   (dossier id "class:CgsSceneManager::SceneManagerIO::InEventSetEntityRadius>", funcs: 2).
//
// The generic AddEvent / Append bodies are already inline in CgsBaseEventQueue.h; these are the
// thin explicit instantiations. Both X360 bodies match the generic store-for-store. Header offsets:
// mpEvents @ +0, miMaxLength @ +4, miLength @ +8.
//
// AddEvent (@0x822AB928) -- appends UNCONDITIONALLY (asserts are non-gating tripwires):
//   * mpEvents != NULL  (CgsBaseEventQueue.h:312, `lwz r11,0(r29)`; bne skips);
//   * miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length", `lwz r11,8(r29)` vs
//     `lwz r10,4(r29)`, blt skips) -- tripwire only; the copy below always runs;
//   * the element copy is two 4-byte word stores at an 8-byte stride: `slwi r11,r11,3` (miLength*8),
//     `add r11,r11,r10` (== mpEvents + miLength*8), `stw r9,0(r11)` / `stw r10,4(r11)` (whole-struct
//     copy of the 2-word InEventSetEntityRadius from a2); bumps miLength (`stw r11,8(r29)`);
//     returns 1.
//
// Append (@0x827A6560) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", lSource.miLength +
//     this->miLength vs this->miMaxLength, ble skips);
//   * reads lSource via GetQueueStartPointer (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + miLength, lSource.mpEvents, sizeof(T)*lSource.miLength) at the same
//     8-byte stride (`slwi r5,r29,3` == count*8, `slwi r11,r11,3` == dest offset miLength*8);
//     bumps miLength (`add`/`stw r11,8(r31)`); returns 1.
//
// The 8-byte stride matches sizeof(InEventSetEntityRadius) == 8 (EntityId + f32, see
// CgsSceneManagerIO_EventSetEntityRadius.h). This is the AddEvent/Append pair of the
// EventQueue<InEventSetEntityRadius,512> family whose Construct instantiation lives in
// EventQueue_InEventSetEntityRadius_512.cpp. AddEvent is called from
// InSceneUpdateInterface::SetEntityRadius; Append from InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetEntityRadius>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventSetEntityRadius&);

template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetEntityRadius>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventSetEntityRadius>&);
