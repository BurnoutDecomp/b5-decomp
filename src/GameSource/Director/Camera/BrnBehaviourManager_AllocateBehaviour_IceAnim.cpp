// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager_AllocateBehaviour_IceAnim.cpp
//
// BehaviourManager::AllocateBehaviour<BrnDirector::Camera::BehaviourIceAnim> @0x82263428.
// Isolated explicit-instantiation TU: BrnBehaviourIceAnim.h derives the (minimal-slice)
// Camera::Behaviour base and pulls the REAL shared camera-support headers (BrnLooker /
// BrnCollisionPolicy / BrnCameraTweaker), which mutually collide with the sibling behaviour
// headers' local re-declarations of the same types -- so this instantiation cannot share the
// BrnBehaviourManager.cpp group TU and lives here on its own.
//
// The shared AllocateBehaviour<TBehaviour> body is out-of-line in BrnBehaviourManager.h.
// sizeof(BehaviourIceAnim) == 3904 (> the 1600-byte small bucket) -> mLargeBehaviourPool
// ("large behaviour"), matching the X360 asm at 0x82263428.
// ============================================================================

#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourIceAnim.h"

namespace BrnDirector
{
namespace Camera
{
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourIceAnim>();
}
} // namespace BrnDirector
