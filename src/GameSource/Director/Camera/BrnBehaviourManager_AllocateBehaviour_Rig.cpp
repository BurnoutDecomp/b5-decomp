// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager_AllocateBehaviour_Rig.cpp
//
// BehaviourManager::AllocateBehaviour<BrnDirector::Camera::BehaviourRig> @0x82258F90.
// Isolated explicit-instantiation TU: BehaviourRig.h derives the Camera::Behaviour base and
// locally re-declares the whole shared camera-support layer (Behaviour / CollisionPolicy /
// VisibilityCollisionPolicy / CameraShake / Looker / Tweaker / Timestep / BehaviourSharedInfo),
// which collides with the sibling base-deriving headers (IceAnim / RenderMetrics) and the real
// shared headers -- so this instantiation lives on its own.
//
// The shared AllocateBehaviour<TBehaviour> body is out-of-line in BrnBehaviourManager.h.
// sizeof(BehaviourRig) == 1152 (<= the 1600-byte small bucket) -> mSmallBehaviourPool
// ("small behaviour"), matching the X360 asm at 0x82258F90.
// ============================================================================

#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"

namespace BrnDirector
{
namespace Camera
{
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourRig>();
}
} // namespace BrnDirector
