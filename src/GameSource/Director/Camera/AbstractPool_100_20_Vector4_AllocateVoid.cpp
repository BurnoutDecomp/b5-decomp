// ===========================================================================
// AbstractPool_100_20_Vector4_AllocateVoid.cpp
// Explicit-instantiation ledger for
//   BrnDirector::AbstractPool<100u, 20u, rw::math::vpu::Vector4>::AllocateVoid<T>()
// -- the SMALL camera-behaviour pool (mSmallBehaviourPool) embedded in
// BrnDirector::Camera::BehaviourManager (BrnBehaviourManager.h:458).
//
// The generic AllocateVoid<T> body is committed inline in
//   GameSource/Director/Utils/BrnAbstractPool.h
// (AllocateObject a slot, assert space, placement-new a T in the bucket, and
// Prepare the four-word type-erased AbstractPoolVoidHandle). This TU only emits the
// two concrete per-behaviour-type symbols the X360 ledger tracks under the pool's
// class key; each is a thin instantiation of that shared body, store-for-store
// identical to the asm (AllocateObject -> null-guard assert "AbstractPool : out of
// space" -> construct T (the two vptr stores in the pseudocode) -> Prepare).
//
// Two X360 ledger symbols land here, both AllocateVoid<TBehaviour> over the SMALL
// (100 Vector4 == 1600-byte bucket) pool -- both behaviours fit the small bucket, so
// BehaviourManager::AllocateBehaviour<T> routes them to mSmallBehaviourPool, whose
// AllocateVoid<T> is exactly this pool's member template:
//
//   AllocateVoid<BehaviourRotateAboutVehicle>  @ 0x82253F08
//       (called by BehaviourManager::AllocateBehaviour<BehaviourRotateAboutVehicle>)
//   AllocateVoid<BehaviourSpirallingDeathcam>  @ 0x82254000
//       (called by BehaviourManager::AllocateBehaviour<BehaviourSpirallingDeathcam>)
//
// TBehaviour must be a COMPLETE type here (sizeof for the bucket-fit assert + a
// placement-new default construct); the two flat behaviour-slice headers below supply
// that. Both are the flat-slice behaviours the committed BrnBehaviourManager.cpp
// already co-includes and instantiates AllocateBehaviour<> for, so they coexist with
// no header-fork collision (the IceAnim/Bystander shared-slice redefinition hazard is
// avoided by not pulling those headers in).
// ===========================================================================
#include "GameSource/Director/Camera/BrnBehaviourManager.h"                       // AbstractPool<100,20,Vector4>, rw::math::vpu::Vector4
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRotateAboutVehicle.h" // complete TBehaviour
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h" // complete TBehaviour

// --- AllocateVoid<BehaviourRotateAboutVehicle> @0x82253F08 --------------------------------
template BrnDirector::AbstractPoolVoidHandle
BrnDirector::AbstractPool<100u, 20u, rw::math::vpu::Vector4>::AllocateVoid<BrnDirector::Camera::BehaviourRotateAboutVehicle>();

// --- AllocateVoid<BehaviourSpirallingDeathcam> @0x82254000 -------------------------------
template BrnDirector::AbstractPoolVoidHandle
BrnDirector::AbstractPool<100u, 20u, rw::math::vpu::Vector4>::AllocateVoid<BrnDirector::Camera::BehaviourSpirallingDeathcam>();
