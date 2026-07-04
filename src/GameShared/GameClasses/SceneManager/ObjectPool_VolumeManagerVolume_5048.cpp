// ===========================================================================
// Explicit-instantiation ledger for CgsContainers::ObjectPool<CgsSceneManager::
// VolumeManagerVolume, 5048, int> -- the collision-volume record pool owned by
// VolumeManager. Four X360 ledger symbols land here, all thin instantiations of
// the generic ObjectPool bodies (inline in CgsObjectPool.h):
//
//   AllocateObject     @ 0x828AC5E0
//   Clear              @ 0x828AEB70  (VolumeManager::Prepare)
//   FreeObject         @ 0x828B75D0
//   IsObjectAllocated  @ 0x828B7A70
//
// Element type = CgsSceneManager::VolumeManagerVolume (DWARF CgsVolumeManager.h:60):
// X360-attested 32-byte stride (maObjectPool[5048] spans [0,161536);
// maiObjectFreeQueue @+161536, miNumObjectsFree @+181728,
// mObjectsAllocated(BitArray<5048>) @+181736, 79 u64 fields = 632 bytes).
// Store-for-store identical to the committed generic bodies.
// ===========================================================================
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"

template int  CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::AllocateObject();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::Clear();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::FreeObject(int);
template bool CgsContainers::ObjectPool<CgsSceneManager::VolumeManagerVolume, 5048, int>::IsObjectAllocated(int) const;
