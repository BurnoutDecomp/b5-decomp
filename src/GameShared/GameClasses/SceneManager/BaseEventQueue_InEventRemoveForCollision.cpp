#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                          // BaseEventQueue<T>::AddEvent / Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveForCollision.h" // InEventRemoveForCollision element (8-byte u64)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>::{AddEvent,Append}
//   AddEvent @ X360 0x822AB2C8, Append @ X360 0x827A6008
//   (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>", funcs: 2).
//
// The generic AddEvent / Append bodies are already inline in CgsBaseEventQueue.h; these are the
// thin explicit instantiations. Both X360 bodies match the generic store-for-store.
//
// AddEvent (@0x822AB2C8) -- appends UNCONDITIONALLY (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:312, `lwz r11,0(r28)`; bne skips);
//   * miLength < miMaxLength (CgsBaseEventQueue.h:313 "Reached Max length", `lwz r11,8(r28)` vs
//     `lwz r10,4(r28)`, blt skips) -- tripwire only; the copy below always runs;
//   * single 64-bit store of *lEvent at mpEvents + miLength*8: `ld r10,0(r26)` (load the 8-byte
//     element), `lwz r11,8(r28)` (miLength), `slwi r11,r11,3` (*8 -- matches sizeof(T)==8),
//     `lwz r9,0(r28)` (mpEvents), `stdx r10,r11,r9` (mpEvents[miLength] = *lEvent); bumps miLength
//     (`stw r11,8(r28)` via addi r11,r11,1); returns 1 (`li r3,1`).
//
// Append (@0x827A6008) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", lSource.miLength@+8 +
//     this->miLength@+8 vs this->miMaxLength@+4, ble skips);
//   * reads lSource via GetQueueStartPointer (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + miLength*8, lSource.mpEvents, lSource.miLength*8) at the 8-byte
//     stride (`slwi r5,r29,3` == count*8, `slwi r11,r11,3` == miLength*8 dest offset,
//     `add r3,r11,r10` == &mpEvents[miLength]); bumps miLength (`add; stw r11,8(r31)`); returns 1.
//
// The 8-byte stride matches sizeof(InEventRemoveForCollision) == 8 (X360-attested single u64,
// see CgsSceneManagerIO_EventRemoveForCollision.h). This is the AddEvent/Append pair of the
// EventQueue<InEventRemoveForCollision, 1536> family whose Construct instantiation lives in
// EventQueue_InEventRemoveForCollision_1536.cpp. AddEvent/Append are called from
// CgsSceneManager::SceneManagerIO::InSceneUpdateInterface::RemoveForCollision/Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>::AddEvent(
    const CgsSceneManager::SceneManagerIO::InEventRemoveForCollision&);

template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveForCollision>&);
