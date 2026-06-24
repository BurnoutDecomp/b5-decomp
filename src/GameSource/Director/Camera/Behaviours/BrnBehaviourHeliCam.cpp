// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourHeliCam slice this TU owns.
// SetParameters @0x821F3AA0 is defined inline in the header; this .cpp is the translation-unit
// anchor that pulls the header into the compile gate and forces an out-of-line emission of
// SetParameters. The arbitrator testbed adopts a heli-cam parameter block through it. The rest
// of the behaviour (Construct/Prepare/Update and the full rig) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourHeliCam_SetParametersAnchor(
    BehaviourHeliCam& lrBehaviour,
    const BehaviourHeliCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

} // namespace Camera
} // namespace BrnDirector
