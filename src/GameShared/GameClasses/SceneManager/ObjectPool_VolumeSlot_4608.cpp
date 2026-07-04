// ===========================================================================
// Explicit-instantiation ledger for CgsContainers::ObjectPool<CgsSceneManager::
// VolumeSlot, 4608, int> -- the volume pool embedded in VolumeStore<4608>
// (DWARF CgsVolumeStore.h:126, mVolumePool). Three X360 ledger symbols land here,
// all thin instantiations of the generic ObjectPool bodies (inline in
// CgsObjectPool.h):
//
//   Clear             @ 0x828B9218  (VolumeManager::Prepare)
//   FreeObject        @ 0x828B9030  (VolumeStore<4608>::RemoveVolume)
//   IsObjectAllocated @ 0x828B9540  (VolumeStore<4608>::RemoveVolume / VolumeManager::GetRwVolume)
//
// Element type = CgsSceneManager::VolumeSlot (DWARF CgsVolumeStore.h:51): an
// opaque 128-byte slot (KI_VOLUME_SLOT_SIZE), so the pool stride is 128 and
// sizeof(maObjectPool) == 4608 * 128 == 589824 -- matching the X360 offsets
// (maiObjectFreeQueue @+589824, miNumObjectsFree @+608256,
// mObjectsAllocated(BitArray<4608>) @+608264, 72 u64 fields = 576 bytes).
// Store-for-store identical to the committed generic bodies.
// ===========================================================================
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"

template void CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::Clear();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::FreeObject(int);
template bool CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::IsObjectAllocated(int) const;
