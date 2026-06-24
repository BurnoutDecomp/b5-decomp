// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGyroCam slices this TU owns:
//   SetParameters                        @0x821F4068 (inline in the header)
//   AttachToRaceCar                      @0x821F40D8 (inline in the header)
//   GetCollisionPolicy                   @0x821FA1C0 (inline in the header)
//   SetWorldSpaceNormalizedVectorFromCar @0x822067D0 (inline in the header)
// This .cpp is the translation-unit anchor that pulls the header into the compile gate and
// forces an out-of-line emission of each. The rest of the behaviour (Construct/Prepare/Update
// and the full rig) lands with its own TU.
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

BehaviourGyroCam::CollisionPolicy* BehaviourGyroCam_GetCollisionPolicyAnchor(
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

} // namespace Camera
} // namespace BrnDirector
