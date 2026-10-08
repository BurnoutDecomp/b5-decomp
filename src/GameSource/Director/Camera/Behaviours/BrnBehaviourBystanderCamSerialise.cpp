// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCamSerialise.cpp
//
// BehaviourBystanderCam::Parameters::Serialise<TSerialiser> -- the bystander-camera tunings walk
// (two nested sub-blocks, seven f32 tunables, two flags), one generic body instantiated over the
// three camera serialisers. The block has no version tag.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourBystanderCam.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

template<class TSerialiser>
void BehaviourBystanderCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake parameters", mShakeParams);
    lrSerialiser.Serialise("Looker parameters", mLookerParams);

    lrSerialiser.Serialise("Target velocity influence on pos", mfVelocityInfluenceOnPosition);
    lrSerialiser.Serialise("Max initial distance KM", mfMaxInitialDistanceKM);
    lrSerialiser.Serialise("Distance for fail KM", mfDistanceForFailKM);
    lrSerialiser.Serialise("Target Space X", mfTargetSpaceX);
    lrSerialiser.Serialise("Target Space Y", mfTargetSpaceY);
    lrSerialiser.Serialise("Target Space Z", mfTargetSpaceZ);
    lrSerialiser.Serialise("Height", mfHeight);
    lrSerialiser.Serialise("Use target space", mbUseTargetSpaceInsteadOfPositionFinder);
    lrSerialiser.Serialise("Use range testing", mbUseRangeTesting);
}

template void BehaviourBystanderCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourBystanderCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourBystanderCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
