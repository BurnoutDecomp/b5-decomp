// ===========================================================================
// Explicit-instantiation ledger for CgsContainers::ObjectPool<
//   rw::math::vpu::Vector4[250], 8, int> -- the LARGE behaviour pool embedded
// in BrnDirector::AbstractPool<250u,8u,Vector4>, held as
// BehaviourManager::mLargeBehaviourPool (BrnBehaviourManager.h:444).
//
// AbstractPool<250,8,Vector4>::Bucket == Vector4[250] (4000 bytes), so the pool
// element stride is 4000 and sizeof(maObjectPool) == 8 * 4000 == 32000 (0x7D00),
// matching the X360 offsets: maiObjectFreeQueue @+32000 (0x7D00),
// miNumObjectsFree @+32032 (0x7D20), mObjectsAllocated(BitArray<8>) @+32040
// (0x7D28, one u64). TIndex == int.
//
// Two X360 ledger symbols land here, both thin instantiations of the generic
// ObjectPool bodies (inline in CgsObjectPool.h), store-for-store identical:
//   AllocateObject @ 0x82201F90
//   FreeObject     @ 0x827DDA08
//
// Only AllocateObject/FreeObject are instantiated (per-member, NOT `template
// class`): the array element type Vector4[250] has no operator==, so FindObject
// must not be forced.
// ===========================================================================
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector4

template int  CgsContainers::ObjectPool<rw::math::vpu::Vector4[250], 8, int>::AllocateObject();
template void CgsContainers::ObjectPool<rw::math::vpu::Vector4[250], 8, int>::FreeObject(int);
