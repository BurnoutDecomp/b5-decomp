// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraShake.cpp
//
// CameraShake::Parameters::Serialise<TSerialiser> -- the shake tunings walk (four f32 fields,
// no version tag), one generic body instantiated over the three camera serialisers (text-file
// read / write, debug menu). CameraShake::Update lives in BrnCameraShakeUpdate.cpp.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

template<class TSerialiser>
void CameraShake::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Yaw/Pitch Angular shake in degrees", mfXYShakeMagnitudeDegs);
    lrSerialiser.Serialise("Roll Angular shake in degrees", mfZShakeMagnitudeDegs);
    lrSerialiser.Serialise("Wobble in degrees", mfXYWobbleMagnitudeDegs);
    lrSerialiser.Serialise("Wobble centering factor", mfWobbleCenteringFactor);
}

template void CameraShake::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void CameraShake::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void CameraShake::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
