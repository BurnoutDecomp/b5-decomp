// Per-instantiation .cpp for
// CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>::GetEvent.
// The generic BaseEventQueue<T>::GetEvent(int) const body is fully inline in CgsBaseEventQueue.h; this
// TU forces only the explicit member instantiation, mirroring the committed sibling GetEvent TUs
// (e.g. BaseEventQueue_LoadBundleRequest_GetEvent.cpp).
//
// CgsModule::BaseEventQueue<...::OutEventLineTestNearestResult>::GetEvent(s32) const  @ X360 0x825BB748
// Called by BrnPhysics::Vehicle::VehicleManager::ProcessAboveGroundLineTestsResults to walk the vehicle
// manager's line-test result queue == EventQueue<OutEventLineTestNearestResult, 2000>.
//
// X360 store-for-store (asm at 0x825BB748): asserts mpEvents!=NULL (272), liIndex<GetLength() (274),
// liIndex>=0 (275), then returns &mpEvents[liIndex] via slwi r11,r29,6 (liIndex*64) + mpEvents@0.
// The 64-byte stride is sizeof(OutEventLineTestNearestResult) == 0x40, pinned by the committed home's
// OutEventLineTestNearestResult_AssertLayout() in CgsSceneManagerModuleIO.h.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerModuleIO.h"  // OutEventLineTestNearestResult (64B element)

template const CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult&
CgsModule::BaseEventQueue<CgsSceneManager::SceneManagerIO::OutEventLineTestNearestResult>::GetEvent(s32) const;
