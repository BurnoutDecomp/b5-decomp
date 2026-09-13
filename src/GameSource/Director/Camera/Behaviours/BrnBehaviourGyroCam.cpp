// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGyroCam slices this TU owns:
//   Construct / Prepare / GetCollisionPolicy / GetName  (the declared vtable slots)
//   SetParameters / AttachToRaceCar / SetWorldSpaceNormalizedVectorFromCar
// Every one of them is header-inline (this class is re-based onto Camera::Behaviour and this
// .cpp is not on the build list, so an out-of-line body here would be a vtable slot no link
// could resolve). This .cpp is the translation-unit anchor that pulls the header into the
// compile gate and forces an emission of each. The rig's Update and SetupTweaker slots land
// with the gyro-cam rig TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchors: force the slice functions to be emitted in this TU. The moment/arbitrator
// code adopts a gyro-cam parameter block, attaches the rig to a race car, reads the active
// collision policy, and seeds the world-space from-car vector through these.
void BehaviourGyroCam_SetParametersAnchor(
    BehaviourGyroCam& lrBehaviour,
    const BehaviourGyroCam::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

void BehaviourGyroCam_AttachToRaceCarAnchor(
    BehaviourGyroCam& lrBehaviour,
    s32 meRaceCarIndex)
{
    lrBehaviour.AttachToRaceCar(meRaceCarIndex);
}

CollisionPolicy* BehaviourGyroCam_GetCollisionPolicyAnchor(
    BehaviourGyroCam& lrBehaviour)
{
    return lrBehaviour.GetCollisionPolicy();
}

void BehaviourGyroCam_SetWorldSpaceNormalizedVectorFromCarAnchor(
    BehaviourGyroCam& lrBehaviour,
    rw::math::vpu::Vector3 lVectorFromCar)
{
    lrBehaviour.SetWorldSpaceNormalizedVectorFromCar(lVectorFromCar);
}

void BehaviourGyroCam_SetUseVehicleAttachmentCollisionAnchor(
    BehaviourGyroCam& lrBehaviour,
    bool lbUseVehicleAttachmentCollision)
{
    lrBehaviour.SetUseVehicleAttachmentCollision(lbUseVehicleAttachmentCollision);
}

} // namespace Camera
} // namespace BrnDirector
