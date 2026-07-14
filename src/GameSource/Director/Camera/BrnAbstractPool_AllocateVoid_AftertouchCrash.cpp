// ============================================================================
// GameSource/Director/Camera/BrnAbstractPool_AllocateVoid_AftertouchCrash.cpp
//
// BrnDirector::AbstractPool<100,20,rw::math::vpu::Vector4>
//     ::AllocateVoid<BrnDirector::Camera::BehaviourAftertouchCrash>  @0x82253920.
//
// One of the two per-behaviour explicit instantiations of the director small-behaviour
// pool's AllocateVoid<T> member template (the other is BehaviourLooseAttachment). This is
// the pool allocation the shipped BehaviourManager::AllocateBehaviour<BehaviourAftertouch-
// Crash> forwards to (sizeof(BehaviourAftertouchCrash) <= the 1600-byte small bucket ->
// mSmallBehaviourPool == AbstractPool<100,20,Vector4>): pop a free slot, placement-new the
// behaviour into it (the asm's off_8200A580 primary vtable + off_8200A150 secondary base
// vtable stores at slot +0 / +0x60 are that T() constructor), and Prepare the four-word
// AbstractPoolVoidHandle with (this, object, index, sizeof==0x3E0). The shared AllocateVoid
// body is out-of-line in BrnAbstractPool.h; this TU only pins the concrete instantiation.
//
// Isolated per-behaviour TU (mirrors the committed BrnBehaviourManager_AllocateBehaviour_*
// siblings): BrnBehaviourAftertouchCrash.h pulls BehaviourRig.h's shared camera-support
// layer, which collides with the sibling loose-attachment header's includes -- so the two
// instantiations of this one pool cannot share a single TU.
// ============================================================================

#include "GameSource/Director/Camera/BrnBehaviourManager.h"   // BrnDirector::AbstractPool<>, AbstractPoolVoidHandle, rw::math::vpu::Vector4
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"

namespace BrnDirector
{
    template AbstractPoolVoidHandle
    AbstractPool<100u, 20u, rw::math::vpu::Vector4>::AllocateVoid<Camera::BehaviourAftertouchCrash>();
}
