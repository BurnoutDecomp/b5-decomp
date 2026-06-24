// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourBystanderCam slices this TU owns.
// GetCol @0x821F9B28, SetParameters @0x821F3F10 and SetTarget @0x821F3F80 are defined inline in
// the header; this .cpp is the translation-unit anchor that pulls the header into the compile
// gate and forces out-of-line emission of all three. The rest of the behaviour
// (Construct/Prepare/Update/the virtual GetCollisionPolicy and the full rig) lands with its own TU.
//
// SetParameters/SetTarget are driven by the "bystander sees action" moment and the arbitrator
// testbed state when they install + aim a bystander-cam; GetCol exposes the behaviour's embedded
// collision policy at +0xD0.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCam.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces GetCol to be emitted in this TU.
BehaviourBystanderCam::CollisionPolicy* BehaviourBystanderCam_GetColAnchor(
    BehaviourBystanderCam& lrBehaviour)
{
    return lrBehaviour.GetCol();
}

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourBystanderCam_SetParametersAnchor(
    BehaviourBystanderCam& lrBehaviour,
    const BehaviourBystanderCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

// Out-of-line anchor: forces SetTarget to be emitted in this TU.
void BehaviourBystanderCam_SetTargetAnchor(
    BehaviourBystanderCam& lrBehaviour,
    s32 meRaceCarIndex)
{
    lrBehaviour.SetTarget(meRaceCarIndex);
}

} // namespace Camera
} // namespace BrnDirector
