// ============================================================================
// GameSource/Director/Camera/AbstractPool_AllocateVoid_GameplayExternal.cpp
//
// BrnDirector::AbstractPool<250,8,rw::math::vpu::Vector4>::AllocateVoid<
//     BrnDirector::Camera::BehaviourGameplayExternal>  @0x82253820.
//
// Explicit-instantiation ledger TU for the LARGE-behaviour pool's typed allocate.
// The X360 ledger tracks this compiler-baked specialization as its own symbol: it is the
// AllocateVoid<T> the shared BehaviourManager::AllocateBehaviour<BehaviourGameplayExternal>
// body (BrnBehaviourManager.h:543) folds down to for the large pool
// (BrnDirector::AbstractPool<250u,8u,rw::math::vpu::Vector4> mLargeBehaviourPool,
// BrnBehaviourManager.h:457 -- GameplayExternal is 2840 bytes, so it takes a large slot).
//
// The generic AllocateVoid<T> body is committed out-of-line in BrnAbstractPool.h
// (assert-object-fits -> ObjectPool::AllocateObject (asserting space, "AbstractPool : out of
// space" at BrnAbstractPool.h:170) -> placement-new a fresh T in the popped slot ->
// AbstractPoolVoidHandle::Prepare(this, object, index, sizeof(T))), which reproduces the X360
// asm @0x82253820 store-for-store: AllocateObject on the +16-sized 250/8 pool, the out-of-space
// assert branch, the sub_82202110 slot fetch + the two vptr stores and ICE::ICETake at +0x320
// (placement-new of the behaviour), and the tail Prepare(a1, a2, object, index, 2928). Nothing
// to body here beyond emitting the concrete symbol over that committed generic body.
//
// GameplayExternal is a flat-slice behaviour header (it does NOT derive the real shared
// Camera::Behaviour base, so no C2011/C2371 header-fork collision -- it coexists with the 16
// flat behaviours in BrnBehaviourManager.cpp) and must be a COMPLETE type here so the
// placement-new inside AllocateVoid<T> sees sizeof + a ctor.
// ============================================================================

#include "GameSource/Director/Utils/BrnAbstractPool.h"                          // AbstractPool<>, AbstractPoolVoidHandle
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayExternal.h" // complete BehaviourGameplayExternal
#include "rw/math/vpu/types.h"                                                  // rw::math::vpu::Vector4 (pool bucket unit_type)

namespace BrnDirector
{

template AbstractPoolVoidHandle
AbstractPool<250u, 8u, rw::math::vpu::Vector4>::AllocateVoid<Camera::BehaviourGameplayExternal>();

} // namespace BrnDirector
