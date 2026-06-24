// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourPassengerCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourPassengerCam slice this TU owns.
// SetParameters @0x821F3A30 is defined inline in the header; this .cpp is the translation-unit
// anchor that pulls the header into the compile gate and forces an out-of-line emission of
// SetParameters. The "passenger sees action" scripted moment and the arbitrator testbed adopt
// a passenger-cam parameter block through it. The rest of the behaviour (Construct/Prepare/
// Update and the full rig) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourPassengerCam.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourPassengerCam_SetParametersAnchor(
    BehaviourPassengerCam& lrBehaviour,
    const BehaviourPassengerCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

} // namespace Camera
} // namespace BrnDirector
