// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumperParameters.cpp
//
// BehaviourGameplayBumper::Parameters::Serialise<TSerialiser> -- the bumper-camera tunings walk
// (eleven f32 tunables in offset order), one generic body instantiated over the three camera
// serialisers. The block has no version tag; mbIsValid and the shake block are not walked.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

template<class TSerialiser>
void BehaviourGameplayBumper::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Y Offset", mfYOffset);
    lrSerialiser.Serialise("Z Offset", mfZOffset);
    lrSerialiser.Serialise("Accel. Dampening", mfAccelerationDampening);
    lrSerialiser.Serialise("Accel. Response", mfAccelerationResponse);
    lrSerialiser.Serialise("Pitch Spring", mfPitchSpring);
    lrSerialiser.Serialise("Yaw Spring", mfYawSpring);
    lrSerialiser.Serialise("Roll Spring", mfRollSpring);
    lrSerialiser.Serialise("FOV", mfFOV);
    lrSerialiser.Serialise("Body Roll Scale", mfBodyRollScale);
    lrSerialiser.Serialise("Body Pitch Scale", mfBodyPitchScale);
    lrSerialiser.Serialise("FOV during boost", mfBoostFOV);
}

template void BehaviourGameplayBumper::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourGameplayBumper::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourGameplayBumper::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
