#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventRemoveEntity.h" // InEventRemoveEntity element (8-byte, two u32 words)

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveEntity>::Append
//   @ X360 0x827A5F28 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventRemoveEntity>").
//
// The generic BaseEventQueue<T>::Append body is already inline in CgsBaseEventQueue.h; this is
// the thin explicit instantiation. The X360 body matches the generic store-for-store:
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips the assert; line 0x19D);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow",
//     `lwz r11,8(r30)` (lSource.miLength) + `lwz r10,8(r31)` (this->miLength) vs
//     `lwz r9,4(r31)` (this->miMaxLength); ble skips; line 0x19E -- NON-gating, copy always runs);
//   * lSource.mpEvents != NULL via GetQueueStartPointer (CgsBaseEventQueue.h:486,
//     `lwz r11,0(r30)`; bne skips; line 0x1E6);
//   * XMemCpy(this->mpEvents + this->miLength, lSource.mpEvents, sizeof(T)*lSource.miLength)
//     at an 8-byte element stride -- `slwi r5,r29,3` (count == lSource.miLength*8),
//     `slwi r11,r11,3` (dest offset == this->miLength*8), `add r3,r11,r10` == mpEvents + miLength*8;
//     modelled as std::memcpy;
//   * bumps this->miLength by lSource.miLength (`add r11; stw r11,8(r31)`) and returns 1.
// The 8-byte stride matches sizeof(InEventRemoveEntity) == 8 (two u32 words), independently
// attested by the sibling AddEvent @0x822AB180 (same TU: `v12 = 8*a1[2] + *a1; *v12 = *a2;
// v12[1] = a2[1];`, two-word copy at slwi-x3 stride) and the Construct instantiation
// (EventQueue_InEventRemoveEntity_10000.cpp). Member offsets confirmed from asm: mpEvents@+0,
// miMaxLength@+4, miLength@+8 -- matches the committed generic BaseEventQueue<T> layout.
// Called from InSceneUpdateInterface::Append.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveEntity>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventRemoveEntity>&);