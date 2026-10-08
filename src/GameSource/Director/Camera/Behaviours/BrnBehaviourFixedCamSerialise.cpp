// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCamSerialise.cpp
//
// Partfile of BrnBehaviourFixedCam.cpp: BehaviourFixedCam::Parameters::Serialise<TSerialiser>,
// the fixed-cam tunings walk (two f32 fields, no version tag), one generic body instantiated over
// the three camera serialisers (text-file read / write, debug menu). The FOV is a plain f32
// here (stepped, not range-clamped, in the debug menu).
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFixedCam.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

template<class TSerialiser>
void BehaviourFixedCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("FOV", mfFOV);
    lrSerialiser.Serialise("Max Dutch", mfMaxDutch);
}

template void BehaviourFixedCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourFixedCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourFixedCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
