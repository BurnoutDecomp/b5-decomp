// ===========================================================================
// Explicit-instantiation ledger for CgsContainers::ObjectPool<CgsSceneManager::
// VolumeSlot, 4608, int> -- the volume pool embedded in VolumeStore<4608>
// (DWARF CgsVolumeStore.h:126, mVolumePool). Six X360 ledger symbols land here, all thin
// instantiations of the generic ObjectPool bodies (inline in CgsObjectPool.h):
//
//   AllocateObject    @ 0x828B8EA0  VolumeStore<4608>::AddVolume (inlined into
//                                   VolumeManager::AddDynamicVolume @0x828D1CBC)
//                                   -- absent from progress/identity.json (export hole);
//                                      named by the call site's own symbol text
//   Clear             @ 0x828B9218  VolumeManager::Prepare (called TWICE: Prepare + Clear)
//   FreeObject        @ 0x828B9030  VolumeStore<4608>::RemoveVolume
//   IsObjectAllocated @ 0x828B9540  VolumeStore<4608>::RemoveVolume / ::GetVolume
//   operator[] const  @ 0x828B9280  VolumeStore<4608>::GetVolume  (asserts
//                                   CgsObjectPool.h:228 == 0xE4 / :229 == 0xE5)
//   operator[]        @ 0x828B93E0  VolumeStore<4608>::ReplaceVolume (asserts :218 == 0xDA
//                                   / :219 == 0xDB)
//
// Element type = CgsSceneManager::VolumeSlot (DWARF CgsVolumeStore.h:51): an opaque
// 128-byte slot (KI_VOLUME_SLOT_SIZE), 16-byte aligned. Both operator[] bodies pin the
// stride with `slwi r11,r28,7` (128), the occupancy BitArray<4608> is reached as
// `(index/64 + 0x12901)*8` == +608264, maiObjectFreeQueue is @+589824 and
// miNumObjectsFree @+608256.
//
// This instantiation is entirely POINTER-FREE, so its console size survives the x64
// compile; sizeof == 608848 (with the 16-byte tail pad) is static_asserted in
// CgsVolumeStore.h rather than merely commented.
// ===========================================================================
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"

template int  CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::AllocateObject();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::Clear();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::FreeObject(int);
template bool CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::IsObjectAllocated(int) const;
template CgsSceneManager::VolumeSlot&
    CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::operator[](int);
template const CgsSceneManager::VolumeSlot&
    CgsContainers::ObjectPool<CgsSceneManager::VolumeSlot, 4608, int>::operator[](int) const;
