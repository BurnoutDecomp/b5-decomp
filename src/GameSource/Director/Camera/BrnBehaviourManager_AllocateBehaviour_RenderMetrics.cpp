// ============================================================================
// GameSource/Director/Camera/BrnBehaviourManager_AllocateBehaviour_RenderMetrics.cpp
//
// BehaviourManager::AllocateBehaviour<BrnDirector::Camera::BehaviourRenderMetrics> @0x8224B770.
// Explicit-instantiation TU for the render-metrics probe behaviour.
//
// The shared AllocateBehaviour<TBehaviour> body is out-of-line in BrnBehaviourManager.h.
// The X360 asm at 0x8224B770 reads the SMALL pool ("small behaviour"), so
// ConsoleBehaviourPool<BehaviourRenderMetrics> is SMALL -> mSmallBehaviourPool (console size 224;
// AllocateBehaviour<> static_asserts the host fit).
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
