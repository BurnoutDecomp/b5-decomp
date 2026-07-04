// Per-instantiation .cpp for
// CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache>::GetEvent.
// The generic BaseEventQueue<T>::GetEvent(int) const body is fully inline in CgsBaseEventQueue.h; this
// TU forces only the explicit member instantiation, mirroring the committed sibling
// BaseEventQueue_LoadBundleRequest_GetEvent.cpp.
//
// CgsModule::BaseEventQueue<...::InEventRemoveFromCache>::GetEvent(s32) const  @ X360 0x828ACD80
// Called by CgsSceneManager::TriangleCacheManager::ProcessRemoveFromCacheEvents to walk
// EventQueue<InEventRemoveFromCache, 298>.
//
// X360 store-for-store (asm at 0x828ACD80): asserts mpEvents!=NULL (272), liIndex<GetLength() (274),
// liIndex>=0 (275), then returns &mpEvents[liIndex] via slwi r11,r29,2 (liIndex*4) + mpEvents@0.
// The 4-byte stride is sizeof(InEventRemoveFromCache) (single s32 miCacheSlot), the committed element.
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CacheManager/CgsTriangleCacheManagerIO.h"  // InEventRemoveFromCache (4B element)

template const CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache&
CgsModule::BaseEventQueue<CgsSceneManager::TriangleCacheManagerIO::InEventRemoveFromCache>::GetEvent(s32) const;
