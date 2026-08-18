// ===========================================================================
// Explicit-instantiation ledger for CgsContainers::ObjectPool<CgsSceneManager::
// VolumeManagerVolume, 5048, int> -- the collision-volume record pool owned by
// VolumeManager (CgsVolumeManager.h:143, X360 +608848). Six X360 ledger symbols land
// here, all thin instantiations of the generic ObjectPool bodies (inline in
// CgsObjectPool.h):
//
//   AllocateObject     @ 0x828AC5E0 (99)   VolumeManager::AddDynamicVolume
//   Clear              @ 0x828AEB70 (27)   VolumeManager::Prepare
//   FreeObject         @ 0x828B75D0 (120)  VolumeManager::RemoveVolume
//   IsObjectAllocated  @ 0x828B7A70 (73)   VolumeManager::GetRwVolume
//   operator[] const   @ 0x828B77B0        (bounds assert CgsObjectPool.h:228 == 0xE4,
//                                           allocated assert :229 == 0xE5)
//   operator[]         @ 0x828B7910        (asserts :218 == 0xDA / :219 == 0xDB)
//
// Element type = CgsSceneManager::VolumeManagerVolume (DWARF CgsVolumeManager.h:60).
//
// ⚠️ STRIDE: 32 bytes ON THE CONSOLE, 40 ON THE HOST -- and that is correct, not a bug.
// The X360 stride is pinned twice (`slwi r11,r29,5` in both operator[] bodies; and
// maObjectPool[5048] spanning [0,161536) with maiObjectFreeQueue @+161536,
// miNumObjectsFree @+181728, mObjectsAllocated BitArray<5048> @+181736 = 79 u64 fields =
// 632 bytes, ending at 182368 -- which is exactly VolumeManager's
// mVolumePool(+608848) -> maVolumeHashElement(+791216) span). On x64 the record's
// embedded CgsResource::ResourceHandle (two pointers) widens 8 -> 16 bytes, so the record
// is 40 and the pool is indexed by sizeof(T); NOTHING in the tree depends on the console
// stride, which is why there is no sizeof static_assert here (contrast the pointer-free
// VolumeSlot pool, which IS asserted in CgsVolumeStore.h).
// ===========================================================================
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"

template int  CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::AllocateObject();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::Clear();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::FreeObject(int);
template bool CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::IsObjectAllocated(int) const;
template CgsSceneManager::VolumeManagerVolume&
    CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::operator[](int);
template const CgsSceneManager::VolumeManagerVolume&
    CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::operator[](int) const;
