// Partfile of BehaviourPassengerCam.cpp: BehaviourPassengerCam::Parameters::Serialise<TSerialiser>,
// the passenger-cam tunings walk (the nested "Impact Params" block only), over the three camera serialisers.

#include "GameSource/Director/Camera/Behaviours/BehaviourPassengerCam.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

template<class TSerialiser>
void BehaviourPassengerCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Impact Params", mImpactParams);
}

template void BehaviourPassengerCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourPassengerCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourPassengerCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
