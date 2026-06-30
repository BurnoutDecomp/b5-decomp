#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                       // BaseEventQueue<T>::Append (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h" // InEventLineTestNearest element (64-byte, alignas(16))

// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestNearest>::Append
//   @ X360 0x823C2080 (dossier id "class:CgsSceneManager::SceneManagerIO::InEventLineTestNearest>").
//
// The generic Append body is already inline in CgsBaseEventQueue.h; this is the thin explicit
// instantiation. The X360 body matches the generic store-for-store.
//
// Append (@0x823C2080) -- merges lSource onto the tail (asserts are non-gating tripwires):
//   * mpEvents != NULL (CgsBaseEventQueue.h:413, `lwz r11,0(r31)`; bne skips);
//   * no overflow (CgsBaseEventQueue.h:414 "Base event queue overflow", `lwz r11,8(r30);
//     lwz r10,8(r31); lwz r9,4(r31); add r11,r11,r10; cmpw r11,r9`, ble skips);
//   * lSource.mpEvents != NULL (CgsBaseEventQueue.h:486 tripwire, `lwz r11,0(r30)`);
//   * XMemCpy(mpEvents + 64*miLength, lSource.mpEvents, 64*lSource.miLength) at a 64-byte stride
//     (`lwz r11,8(r31); slwi r11,r11,6` == miLength*64 dest offset, `add r3,r11,r10` dest;
//     `slwi r5,r29,6` == lSource.miLength*64 == XMemCpy Size; `lwz r4,0(r30)` == src);
//   * bumps miLength by lSource.miLength (`stw r11,8(r31)`); returns 1.
//
// The 64-byte stride is X360-attested off this very function (slwi-by-6 == *64 on both the
// source count and the dest offset) and matches sizeof(InEventLineTestNearest) == 0x40 (64),
// already established by the sibling Construct instantiations (EventQueue_InEventLineTestNearest_40.cpp
// / _256.cpp) and CgsSceneManagerIO_EventLineTestNearest.h. The AddEvent counterpart for this type
// (@0x82210718) is the other function of this TU and is instantiated separately.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestNearest>::Append(
    const CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestNearest>&);
