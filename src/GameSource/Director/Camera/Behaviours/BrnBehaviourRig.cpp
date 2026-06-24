// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourRig.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourRig slice this TU owns.
// SetParameters @0x821F3B10 is defined inline in the header; this .cpp is the translation-unit
// anchor that pulls the header into the compile gate and forces an out-of-line emission of
// SetParameters. The takedown-lookback moment, the arbitrator testbed and the camera-script
// helpers adopt a rig parameter block through it. The rest of the behaviour (Construct/Prepare/
// Update and the full rig) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourRig.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourRig_SetParametersAnchor(
    BehaviourRig& lrBehaviour,
    const BehaviourRig::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

} // namespace Camera
} // namespace BrnDirector
