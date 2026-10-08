// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.cpp
//
// BrnDirector::Camera::BehaviourFailsafe -- the failsafe camera's Behaviour overrides (see the
// FLAG in the header banner for the slots not reconstructed yet).
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"

#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"   // Utils::Tweaker::Construct

namespace BrnDirector
{
namespace Camera
{

void BehaviourFailsafe::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

const char* BehaviourFailsafe::GetName() const
{
    return "BehaviourFailsafe";
}

} // namespace Camera
} // namespace BrnDirector
