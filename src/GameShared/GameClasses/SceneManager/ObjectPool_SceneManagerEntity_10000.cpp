// ===========================================================================
// Explicit-instantiation ledger for CgsContainers::ObjectPool<CgsSceneManager::
// SceneManagerEntity, 10000, s32> -- the entity pool embedded in EntityManager
// (mEntityPool). Six X360 ledger symbols land here, all thin instantiations of
// the generic ObjectPool bodies (inline in CgsObjectPool.h):
//
//   AllocateObject     @ 0x828B68A0
//   Clear              @ 0x828B8E30  (EntityManager::Prepare)
//   FreeObject         @ 0x828B6A28
//   GetFirstObjectIndex@ 0x828C3538
//   GetNextObjectIndex @ 0x828C35A0
//   IsObjectAllocated  @ 0x828B6ED8
//
// this = &mEntityManager.mEntityPool: miNumObjectsFree @ +0x27100,
// maiObjectFreeQueue @ +0x1D4C0 (base 120000), BitArray<10000> mObjectsAllocated
// @ +0x27108. All six map store-for-store to the committed generic bodies.
// ===========================================================================
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/SceneManager/CgsEntityManager.h"

template s32  CgsContainers::ObjectPool<CgsSceneManager::SceneManagerEntity, 10000, s32>::AllocateObject();
template void CgsContainers::ObjectPool<CgsSceneManager::SceneManagerEntity, 10000, s32>::Clear();
template void CgsContainers::ObjectPool<CgsSceneManager::SceneManagerEntity, 10000, s32>::FreeObject(s32);
template s32  CgsContainers::ObjectPool<CgsSceneManager::SceneManagerEntity, 10000, s32>::GetFirstObjectIndex() const;
template s32  CgsContainers::ObjectPool<CgsSceneManager::SceneManagerEntity, 10000, s32>::GetNextObjectIndex(s32) const;
template bool CgsContainers::ObjectPool<CgsSceneManager::SceneManagerEntity, 10000, s32>::IsObjectAllocated(s32) const;
