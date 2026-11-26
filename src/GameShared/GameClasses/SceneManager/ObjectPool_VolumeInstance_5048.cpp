// ===========================================================================
// Explicit-instantiation ledger for CgsContainers::ObjectPool<CgsSceneManager::
// VolumeInstance, 5048, s32> -- the volume-instance pool embedded in EntityManager
// (mVolumeInstancePool @ EntityManager+0x27600). Three X360 ledger symbols land
// here, all thin instantiations of the generic ObjectPool bodies (inline in
// CgsObjectPool.h):
//
//   AllocateObject     @ 0x828AC450
//   Clear              @ 0x828AEB00  (EntityManager::Prepare)
//   FreeObject         @ 0x828B7000
//
// Pool layout (relative to pool base): maObjectPool @+0 (stride 128 =
// sizeof(VolumeInstance)), maiObjectFreeQueue @+0x9DC00, miNumObjectsFree
// @+0xA2AE0, mObjectsAllocated(BitArray<5048>) @+0xA2AE8. Store-for-store
// identical to the committed generic bodies.
// ===========================================================================
// ARTIST compiled this unit through the nested CgsBitArray.h include (its BitArray asserts name
// containers\./CgsBitArray.h, e.g. ObjectPool<VolumeInstance,5048,int>::FreeObject 0x828B7000): see CgsBitArray.h.
#define CGS_ARTIST_CGSBITARRAY_NESTED_INCLUDE
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstance.h"

template s32  CgsContainers::ObjectPool<CgsSceneManager::VolumeInstance, 5048, s32>::AllocateObject();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeInstance, 5048, s32>::Clear();
template void CgsContainers::ObjectPool<CgsSceneManager::VolumeInstance, 5048, s32>::FreeObject(s32);
