// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourInterpolate slice this TU owns.
// The no-arg Setup @0x821F3DC8 is defined inline in the header; this .cpp is the
// translation-unit anchor that pulls the header into the compile gate and forces an
// out-of-line emission of Setup. The rest of the behaviour (Construct/Prepare/Update/
// Release and the interpolation math) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourInterpolate.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces the no-arg Setup to be emitted in this TU. The ICE movie player
// latches an already-populated interpolate behaviour through it.
void BehaviourInterpolate_SetupAnchor(BehaviourInterpolate& lrBehaviour)
{
    lrBehaviour.Setup();
}

} // namespace Camera
} // namespace BrnDirector
