// ============================================================================
// GameSource/Director/Camera/AbstractPool_AllocateVoid_DebugOrbitPlayer.cpp
//
// BrnDirector::AbstractPool<100,20,rw::math::vpu::Vector4>::AllocateVoid<
//     BrnDirector::Camera::BehaviourDebugOrbitPlayer>  @0x82214668.
//
// Explicit-instantiation ledger TU for the small-behaviour pool's typed allocate.
// The X360 ledger tracks this compiler-baked specialization as its own symbol: it is
// the AllocateVoid<T> the shared BehaviourManager::AllocateBehaviour<BehaviourDebugOrbitPlayer>
// body (BrnBehaviourManager.h:533) folds down to for the small pool
// (BrnDirector::AbstractPool<100u,20u,rw::math::vpu::Vector4> mSmallBehaviourPool,
// BrnBehaviourManager.h:458).
//
// The generic AllocateVoid<T> body is committed out-of-line in BrnAbstractPool.h
// (assert-object-fits -> ObjectPool::AllocateObject (asserting space) -> placement-new a
// fresh T in the popped slot -> AbstractPoolVoidHandle::Prepare(this, object, index,
// sizeof(T))), which reproduces the X360 asm store-for-store: AllocateObject on the
// 100/20 pool, the "AbstractPool : out of space" assert, the sub_822023F0 slot fetch +
// vptr store (placement-new of the behaviour), and the tail Prepare(a1, a2). Nothing to
// body here beyond emitting the concrete symbol over that committed generic body.
//
// DebugOrbitPlayer is a flat-slice behaviour header (it does NOT derive the real shared
// Camera::Behaviour base, so no C2011/C2371 header-fork collision) and needs to be a
// COMPLETE type here so the placement-new inside AllocateVoid<T> sees sizeof + a ctor.
// ============================================================================

#include "GameSource/Director/Utils/BrnAbstractPool.h"                       // AbstractPool<>, AbstractPoolVoidHandle
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourDebugOrbitPlayer.h" // complete BehaviourDebugOrbitPlayer
#include "rw/math/vpu/types.h"                                               // rw::math::vpu::Vector4 (pool bucket unit_type)

namespace BrnDirector
{

template AbstractPoolVoidHandle
AbstractPool<100u, 20u, rw::math::vpu::Vector4>::AllocateVoid<Camera::BehaviourDebugOrbitPlayer>();

} // namespace BrnDirector
