// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourSpirallingDeathcam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourSpirallingDeathcam slices this TU set
// owns:
//   - BehaviourSpirallingDeathcam::Prepare @0x821FB3F8  (resets a state field, returns true)
//   - BehaviourSpirallingDeathcam::Start   @0x821F5620  (latches mbStarted)
//
// Prepare is run when the behaviour is installed; Start is run by the crashing/testbed
// arbitrator states (BrnDirector::ArbStateCrashing::Update / ArbStateTestbed::Update) when the
// spiralling deathcam first goes active.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourSpirallingDeathcam.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Prepare @0x821FB3F8
//   Behaviour-faithful: reset the per-activation state field and report prepared OK.
//     *(this + 8) = 0;   // muStateField = 0
//     return 1;
// ----------------------------------------------------------------------------
bool BehaviourSpirallingDeathcam::Prepare()
{
    muStateField = 0;            // stw 0, 8(this)
    return true;
}

// ----------------------------------------------------------------------------
// BrnDirector::Camera::BehaviourSpirallingDeathcam::Start @0x821F5620
//   lbz  r11, 0x2F4(this)      ; mbStarted
//   cmplwi r11, 0              ; assert it is clear (!mbStarted)
//   ... on failure: Begin/Fire/End assert ...
//   li   r11, 1
//   stb  r11, 0x2F4(this)      ; mbStarted = 1
// ----------------------------------------------------------------------------
void BehaviourSpirallingDeathcam::Start()
{
    CGS_ASSERT(!mbStarted, "!mbStarted");   // lbz 0x2F4; assert clear
    mbStarted = 1;                          // stb 1, 0x2F4(this)
}

} // namespace Camera
} // namespace BrnDirector
