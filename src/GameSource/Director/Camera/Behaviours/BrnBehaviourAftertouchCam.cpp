// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourAftertouchCam slice this TU owns.
// SetParameters @0x821F3EA0 and GetCo* @0x821FB588 are defined inline in the header; this
// .cpp is the translation-unit anchor that pulls the header into the compile gate and forces
// out-of-line emission of both. The rest of the behaviour (Construct/Prepare/Update and the
// full rig) lands with its own TU.
//
// SetParameters is adopted by the behaviour-manager (NewBehaviour) and the arbitrator testbed
// when they install an aftertouch-cam parameter block; GetCo* exposes the behaviour's embedded
// sub-object at +0x20.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourAftertouchCam_SetParametersAnchor(
    BehaviourAftertouchCam& lrBehaviour,
    const BehaviourAftertouchCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

// Out-of-line anchor: forces GetCo* to be emitted in this TU.
BehaviourAftertouchCam::CoSubObject* BehaviourAftertouchCam_GetCoAnchor(
    BehaviourAftertouchCam& lrBehaviour)
{
    return lrBehaviour.GetCo();
}

} // namespace Camera
} // namespace BrnDirector
