// Per-instantiation .cpp for
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition>::GetEvent.
// The generic BaseEventQueue<T>::GetEvent(int) const body is fully inline in CgsBaseEventQueue.h; this
// TU forces only the explicit member instantiation (one out-of-line copy per using-TU), mirroring the
// committed sibling BaseEventQueue_LoadBundleRequest_GetEvent.cpp.
//
// CgsModule::BaseEventQueue<...::InEventUpdateCachedPosition>::GetEvent(s32) const  @ X360 0x828ACCD8
// Called by CgsSceneManager::TriangleCacheManager::ProcessUpdateCachedPositionEvents to walk
// EventQueue<InEventUpdateCachedPosition, 298>.
//
// X360 store-for-store (asm at 0x828ACCD8): the checked const accessor asserts
//   mpEvents != NULL          (lwz r11,0(r30); bne, line 272),
//   liIndex  < GetLength()    (lwz r11,8(r30) == miLength; cmpw; blt, line 274),
//   liIndex  >= 0             (cmpwi r29,0; bge, line 275),
// then returns &mpEvents[liIndex] via slwi r11,r29,5 (liIndex*32) + mpEvents@0.
// The 32-byte stride == sizeof(InEventUpdateCachedPosition) (s32 miCacheSlot @+0 +
// Vector3Plus mNewPositionAndRadius @+0x10, alignas(16) -> 0x20), the committed element.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"  // InEventUpdateCachedPosition (32B element)

template const CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition&
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventUpdateCachedPosition>::GetEvent(s32) const;
