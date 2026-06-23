// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourFixedCam slice this TU owns.
// SetParameters @0x821F4518 is defined inline in the header; this .cpp is the
// translation-unit anchor that pulls the header into the compile gate and forces an
// out-of-line emission of SetParameters. The rest of the behaviour (Construct/Prepare/
// Update and the full rig) lands with its own TU.
//
// SetParameters is adopted by the scripted-moment / arbitrator code (the static-cam impact
// moment and the arbitrator testbed) when they install a fixed-cam parameter block.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourFixedCam_SetParametersAnchor(
    BehaviourFixedCam& lrBehaviour,
    const BehaviourFixedCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

} // namespace Camera
} // namespace BrnDirector
