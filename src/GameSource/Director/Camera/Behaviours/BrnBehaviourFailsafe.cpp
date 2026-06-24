// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourFailsafe slice this TU owns:
//   SetParameters      @0x821F4308 (inline in the header)
//   GetCollisionPolicy @0x821FABE0 (inline in the header)
// This .cpp is the translation-unit anchor that pulls the header into the compile gate and
// forces an out-of-line emission of both. The rest of the behaviour (Construct/Prepare/Update
// and the full rig) lands with its own TU.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"

namespace BrnDirector
{
namespace Camera
{

// Out-of-line anchors: force the two slice functions to be emitted in this TU. The arbitrator
// testbed adopts a failsafe parameter block through SetParameters and reads the behaviour's
// collision policy through GetCollisionPolicy.
void BehaviourFailsafe_SetParametersAnchor(
    BehaviourFailsafe& lrBehaviour,
    const BehaviourFailsafe::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

CollisionPolicyAttachedToVehicleStub* BehaviourFailsafe_GetCollisionPolicyAnchor(
    BehaviourFailsafe& lrBehaviour)
{
    return lrBehaviour.GetCollisionPolicy();
}

} // namespace Camera
} // namespace BrnDirector
