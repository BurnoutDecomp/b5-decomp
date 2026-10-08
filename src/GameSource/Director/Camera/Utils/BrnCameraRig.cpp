// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraRig.cpp
//
// CameraRig::Params::Serialise<TSerialiser> -- the rig placement walk (two vec3 offsets, FOV,
// roll/pitch/yaw, the widescreen flag; no version tag), one generic body instantiated over the
// three camera serialisers (text-file read / write, debug menu). The FOV here is a plain f32
// (stepped, not range-clamped, in the debug menu). CameraRig::Construct lives in
// BrnCameraRigConstruct.cpp, the presets in BrnCameraRigParams.cpp.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"   // Utils::CameraRig::Params

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"  // the three serialisers

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

template<class TSerialiser>
void CameraRig::Params::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Offset from car", mOffsetFromTarget);
    lrSerialiser.Serialise("Offset from rotation centre", mOffsetFromRotationCentre);
    lrSerialiser.Serialise("FOV", mfFOV);
    lrSerialiser.Serialise("Roll", mfRoll);
    lrSerialiser.Serialise("Pitch", mfPitch);
    lrSerialiser.Serialise("Yaw", mfYaw);
    lrSerialiser.Serialise("Widescreen Only", mbWidescreenOnly);
}

template void CameraRig::Params::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void CameraRig::Params::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void CameraRig::Params::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
