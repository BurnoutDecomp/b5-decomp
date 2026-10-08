// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrashParameters.cpp
//
// BehaviourAftertouchCrash::Parameters::Serialise<TSerialiser> -- the aftertouch-crash camera
// tunings walk (the shake sub-block, then eleven f32 tunables), one generic body instantiated over
// the three camera serialisers. The block has no version tag. The walk visits "FOV" (+0x40)
// before "Pitch" (+0x3C) and skips mLagParams, mfManualBlendFactor and the rival-selection tail.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

template<class TSerialiser>
void BehaviourAftertouchCrash::Parameters::Serialise(TSerialiser& lrSerialiser)
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

template void BehaviourAftertouchCrash::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourAftertouchCrash::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourAftertouchCrash::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
