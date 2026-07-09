// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager_AllocateBehaviour_RenderMetrics.cpp
//
// BehaviourManager::AllocateBehaviour<BrnDirector::Camera::BehaviourRenderMetrics> @0x8224B770.
// Isolated explicit-instantiation TU: BrnBehaviourRenderMetrics.h derives (and re-declares) the
// minimal-slice Camera::Behaviour base, which collides with the sibling base-deriving behaviour
// headers (IceAnim / Rig) -- so this instantiation lives on its own.
//
// The shared AllocateBehaviour<TBehaviour> body is out-of-line in BrnBehaviourManager.h.
// sizeof(BehaviourRenderMetrics) == 240 (<= the 1600-byte small bucket) -> mSmallBehaviourPool
// ("small behaviour"), matching the X360 asm at 0x8224B770.
// ============================================================================

#include "GameSource/Director/Camera/BrnBehaviourManager.h"
#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRenderMetrics.h"

namespace BrnDirector
{
namespace Camera
{
    template AbstractPoolVoidHandle BehaviourManager::AllocateBehaviour<BehaviourRenderMetrics>();
}
} // namespace BrnDirector
