// ===========================================================================
// ObjectPool_MomentBucket_20.cpp
// Explicit-instantiation ledger for CgsContainers::ObjectPool<Vector4[70], 20, int>
// -- the moment object pool embedded in BrnDirector::MomentController
// (BrnMomentController.h: `AbstractPool<70u,20u,rw::math::vpu::Vector4> mMomentPool`).
//
// The pool's element T is the AbstractPool bucket `unit_type[units_in_bucket]` =
// rw::math::vpu::Vector4[70] (stride 70*sizeof(Vector4)=70*16=1120 bytes), so the pool
// slot is sized to hold the largest director moment (AllocateVoid<MomentXxx>()). The
// element type is spelled via the pool's own public typedef
//   BrnDirector::AbstractPool<70u,20u,rw::math::vpu::Vector4>::Bucket
// (== Vector4[70]); a raw array cannot be written directly in the template-id position.
//
// Three X360 ledger symbols land here, all thin instantiations of the generic
// ObjectPool bodies (inline in CgsObjectPool.h):
//
//   AllocateObject     @ 0x822009B0  (AbstractPool<70,20,Vector4>::AllocateVoid<Moment*>)
//   FreeObject         @ 0x827DD838  (AbstractPool<70,20,Vector4>::FreeObject callback)
//   IsObjectAllocated  @ 0x82200C90  (MomentController::UpdateAllMoments)
//
// X360-attested layout of ObjectPool<Vector4[70],20,int> (pins the stride):
//   maObjectPool[20]       @ +0        (20 buckets * 1120 = 22400 bytes)
//   maiObjectFreeQueue[20] @ +22400    (int[20]; lwzx/stwx at 4*(idx+5600))
//   miNumObjectsFree       @ +22480    (0x57D0)
//   mObjectsAllocated      @ +22488    (0x57D8; BitArray<20> = one u64 field)
// -- exactly the generic ObjectPool member order, matching every committed
// ObjectPool_* instantiation TU. Store-for-store identical to the inline generic bodies.
// ===========================================================================
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"
#include "GameSource/Director/MomentController/BrnMomentController.h" // AbstractPool<70,20,Vector4>, mMomentPool

// Element T = the AbstractPool<70,20,Vector4> bucket == rw::math::vpu::Vector4[70] (1120 bytes).
// --- AllocateObject @0x822009B0 -----------------------------------------------------------
template int  CgsContainers::ObjectPool<BrnDirector::AbstractPool<70u, 20u, rw::math::vpu::Vector4>::Bucket, 20, int>::AllocateObject();

// --- FreeObject @0x827DD838 ---------------------------------------------------------------
template void CgsContainers::ObjectPool<BrnDirector::AbstractPool<70u, 20u, rw::math::vpu::Vector4>::Bucket, 20, int>::FreeObject(int);

// --- IsObjectAllocated @0x82200C90 --------------------------------------------------------
template bool CgsContainers::ObjectPool<BrnDirector::AbstractPool<70u, 20u, rw::math::vpu::Vector4>::Bucket, 20, int>::IsObjectAllocated(int) const;
