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
// The X360 asm at 0x82263428 reads the LARGE pool ("large behaviour"), so
// ConsoleBehaviourPool<BehaviourIceAnim> is LARGE -> mLargeBehaviourPool. The host
// sizeof(BehaviourIceAnim) (3984 when measured 2026-09-27; console 3632, `li r7` @0x822597D0)
// must fit the 4000-byte large bucket -- AllocateBehaviour<> static_asserts it.
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
