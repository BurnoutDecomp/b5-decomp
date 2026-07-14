// ============================================================================
// GameSource/Director/Camera/BrnAbstractPool_AllocateVoid_LooseAttachment.cpp
//
// BrnDirector::AbstractPool<100,20,rw::math::vpu::Vector4>
//     ::AllocateVoid<BrnDirector::Camera::BehaviourLooseAttachment>  @0x82253D18.
//
// The second per-behaviour explicit instantiation of the director small-behaviour pool's
// AllocateVoid<T> member template (sibling of the aftertouch-crash one). This is the pool
// allocation the shipped BehaviourManager::AllocateBehaviour<BehaviourLooseAttachment>
// forwards to (sizeof(BehaviourLooseAttachment) <= the 1600-byte small bucket ->
// mSmallBehaviourPool == AbstractPool<100,20,Vector4>): pop a free slot, placement-new the
// behaviour into it (the asm's off_8200A600 primary vtable + off_8200A150 secondary base
// vtable stores at slot +0 / +0x20 are that T() constructor), and Prepare the four-word
// AbstractPoolVoidHandle with (this, object, index, sizeof==0x330). The shared AllocateVoid
// body is out-of-line in BrnAbstractPool.h; this TU only pins the concrete instantiation.
//
// Isolated per-behaviour TU (mirrors the committed BrnBehaviourManager_AllocateBehaviour_*
// siblings): the loose-attachment / aftertouch-crash behaviour headers pull mutually-
// colliding shared camera-support layers, so the two instantiations of this one pool live
// in separate TUs.
// ============================================================================

#include "GameSource/Director/Camera/BrnBehaviourManager.h"   // BrnDirector::AbstractPool<>, AbstractPoolVoidHandle, rw::math::vpu::Vector4
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"

namespace BrnDirector
{
    template AbstractPoolVoidHandle
    AbstractPool<100u, 20u, rw::math::vpu::Vector4>::AllocateVoid<Camera::BehaviourLooseAttachment>();
}
