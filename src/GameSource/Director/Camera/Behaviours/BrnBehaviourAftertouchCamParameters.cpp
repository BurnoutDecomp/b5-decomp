// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCamParameters.cpp
//
// BehaviourAftertouchCam::Parameters::Serialise<TSerialiser> -- the aftertouch-camera tunings
// walk (the shake sub-block, then eleven f32 tunables), one generic body instantiated over the
// three camera serialisers. The block has no version tag. The walk visits "FOV" (console +0x40)
// before "Pitch" (+0x3C); the lag block and the rival-selection tail are not walked.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// The block derives Behaviour::Parameters, whose debug-name pointer is 8 bytes on the host: the
// derived members start at the end of that head and keep the console spacing among themselves
// (console offset - 0x08 from mShakeParams).
typedef BehaviourAftertouchCam::Parameters AftertouchCamParameters;
static_assert(offsetof(AftertouchCamParameters, mShakeParams) == sizeof(Behaviour::Parameters), "mShakeParams follows the head");
static_assert(offsetof(AftertouchCamParameters, mLagParams)                    - offsetof(AftertouchCamParameters, mShakeParams) == 0x18 - 0x08, "mLagParams @ console +0x18");
static_assert(offsetof(AftertouchCamParameters, mfMinDistance)                 - offsetof(AftertouchCamParameters, mShakeParams) == 0x2C - 0x08, "mfMinDistance @ console +0x2C");
static_assert(offsetof(AftertouchCamParameters, mfPitch)                       - offsetof(AftertouchCamParameters, mShakeParams) == 0x3C - 0x08, "mfPitch @ console +0x3C");
static_assert(offsetof(AftertouchCamParameters, mfFOV)                         - offsetof(AftertouchCamParameters, mShakeParams) == 0x40 - 0x08, "mfFOV @ console +0x40");
static_assert(offsetof(AftertouchCamParameters, mfHeightDistanceVelocityRange) - offsetof(AftertouchCamParameters, mShakeParams) == 0x54 - 0x08, "mfHeightDistanceVelocityRange @ console +0x54");
static_assert(offsetof(AftertouchCamParameters, mfTimeBetweenDecisions)        - offsetof(AftertouchCamParameters, mShakeParams) == 0x68 - 0x08, "mfTimeBetweenDecisions @ console +0x68");
static_assert(sizeof(AftertouchCamParameters) == 120, "host size (console 108: 8-byte head pointer, 8-byte alignment)");

template<class TSerialiser>
void BehaviourAftertouchCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake Params", mShakeParams);
    lrSerialiser.Serialise("Slow Distance", mfMinDistance);
    lrSerialiser.Serialise("Slow Height", mfMinHeight);
    lrSerialiser.Serialise("Fast Distance", mfMaxDistance);
    lrSerialiser.Serialise("Fast Height", mfMaxHeight);
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
