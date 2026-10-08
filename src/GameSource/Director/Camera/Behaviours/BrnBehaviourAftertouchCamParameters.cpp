// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCamParameters.cpp
//
// BehaviourAftertouchCam::Parameters::Serialise<TSerialiser> -- the aftertouch-camera tunings
// walk (the shake sub-block, then eleven f32 tunables), one generic body instantiated over the
// three camera serialisers. The block has no version tag. The walk visits "FOV" (+0x40) before
// "Pitch" (+0x3C); the +0x18..+0x2B and +0x58..+0x6B words are not walked.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// The block is pointer-free, so the host offsets are the console's.
static_assert(offsetof(BehaviourAftertouchCam::Parameters, mShakeParams)                  == 0x08, "mShakeParams @ +0x08");
static_assert(offsetof(BehaviourAftertouchCam::Parameters, mfSlowDistance)                == 0x2C, "mfSlowDistance @ +0x2C");
static_assert(offsetof(BehaviourAftertouchCam::Parameters, mfPitch)                       == 0x3C, "mfPitch @ +0x3C");
static_assert(offsetof(BehaviourAftertouchCam::Parameters, mfFOV)                         == 0x40, "mfFOV @ +0x40");
static_assert(offsetof(BehaviourAftertouchCam::Parameters, mfHeightDistanceVelocityRange) == 0x54, "mfHeightDistanceVelocityRange @ +0x54");

template<class TSerialiser>
void BehaviourAftertouchCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake Params", mShakeParams);
    lrSerialiser.Serialise("Slow Distance", mfSlowDistance);
    lrSerialiser.Serialise("Slow Height", mfSlowHeight);
    lrSerialiser.Serialise("Fast Distance", mfFastDistance);
    lrSerialiser.Serialise("Fast Height", mfFastHeight);
    lrSerialiser.Serialise("FOV", mfFOV);
    lrSerialiser.Serialise("Pitch", mfPitch);
    lrSerialiser.Serialise("Blend Factor Blend Factor", mfBlendFactorBlendFactor);
    lrSerialiser.Serialise("Minimum Blend Factor", mfMinimumBlendFactor);
    lrSerialiser.Serialise("Maximum Blend Factor", mfMaximumBlendFactor);
    lrSerialiser.Serialise("Height Distance Blend Factor", mfHeightDistanceBlendFactor);
    lrSerialiser.Serialise("Height Distance Velocity Range", mfHeightDistanceVelocityRange);
}

template void BehaviourAftertouchCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourAftertouchCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourAftertouchCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
