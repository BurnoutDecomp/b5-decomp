// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.cpp
//
// BrnDirector::Camera::BehaviourAftertouchCam -- the aftertouch camera's Behaviour overrides (see
// the FLAG in the header banner for the slots not reconstructed yet).
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"

#include "GameSource/Director/Camera/Utils/BrnCameraTweaker.h"   // Utils::Tweaker::Construct

namespace BrnDirector
{
namespace Camera
{

void BehaviourAftertouchCam::SetupTweaker(Utils::Tweaker& lrTweaker)
{
    lrTweaker.Construct();
}

const char* BehaviourAftertouchCam::GetName() const
{
    return "BehaviourAftertouchCam";
}

} // namespace Camera
} // namespace BrnDirector
