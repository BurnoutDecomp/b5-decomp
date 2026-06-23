// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGyroCam slice this TU owns.
// SetParameters @0x821F4068 is defined inline in the header; this .cpp is the
// translation-unit anchor that pulls the header into the compile gate and forces an
// out-of-line emission of SetParameters. The rest of the behaviour (Construct/Prepare/
// Update and the full rig) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces SetParameters to be emitted in this TU. The moment/arbitrator
// code adopts a gyro-cam parameter block through it.
void BehaviourGyroCam_SetParametersAnchor(
    BehaviourGyroCam& lrBehaviour,
    const BehaviourGyroCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

} // namespace Camera
} // namespace BrnDirector
