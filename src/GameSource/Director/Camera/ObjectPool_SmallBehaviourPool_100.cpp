// ===========================================================================
// Explicit-instantiation ledger for CgsContainers::ObjectPool<
// rw::math::vpu::Vector4[100], 20, s32> -- the storage pool inside
// BrnDirector::Camera::BehaviourManager::mSmallBehaviourPool, declared as
// BrnDirector::AbstractPool<100u, 20u, rw::math::vpu::Vector4> at
// BrnBehaviourManager.h:445. AbstractPool<units,buckets,unit_type> wraps
// ObjectPool<unit_type[units], buckets, s32> (BrnAbstractPool.h:126,
// Bucket = unit_type[units_in_bucket]), so the element is a bucket of 100
// Vector4s. Two X360 ledger symbols land here, both thin instantiations of the
// generic ObjectPool bodies (inline in CgsObjectPool.h):
//
//   AllocateObject  @ 0x82202270
//   FreeObject      @ 0x827DDBD8
//
// Pool layout (relative to pool base r23/r26), read straight from the asm and
// matching the committed generic ObjectPool bodies store-for-store:
//   maObjectPool      @ +0x00000  stride 1600 = sizeof(Vector4[100]) (Vector4=16B)
//   maiObjectFreeQueue@ +0x07D00  (4*8000; TIndex=s32, element stride 4)
//   miNumObjectsFree  @ +0x07D50  (lwz 0x7D50)
//   mObjectsAllocated @ +0x07D58  (BitArray<20>, addi r,r,0x7D58; u64-pair scan)
//
// Stride derivation: free queue base = 4*8000 = 0x7D00 = 32000 bytes => 32000
// bytes hold N=20 buckets => per-bucket stride 1600 = 100*sizeof(Vector4)=100*16.
// N=20 confirmed by the 0x14 assert limits and the callers' mangled name
// ?$AbstractPool@$0GE@$0BE@VVector4@vpu@math@rw@@ ($0GE@=100 units, $0BE@=20
// buckets). The rw::math::vpu::Vector4 used is the vendor types.h struct (16B),
// which is the SAME type the mSmallBehaviourPool member declaration resolves
// (Camera.h -> rw/math/vpu/types.h), so ODR is consistent.
//
// Instantiate AllocateObject/FreeObject per-member (NOT `template class`) so the
// element type (a raw Vector4[100] bucket, no operator==) does not force
// FindObject.
// ===========================================================================
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Vector4 (pool bucket unit_type)

template s32  CgsContainers::ObjectPool<rw::math::vpu::Vector4[100], 20, s32>::AllocateObject();
template void CgsContainers::ObjectPool<rw::math::vpu::Vector4[100], 20, s32>::FreeObject(s32);
