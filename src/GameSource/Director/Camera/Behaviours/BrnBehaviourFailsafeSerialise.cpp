// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafeSerialise.cpp
//
// BehaviourFailsafe::Parameters::Serialise<TSerialiser> -- the failsafe-camera tunings walk (two
// nested sub-blocks, twelve f32 tunables, one flag), one generic body instantiated over the three
// camera serialisers. The block has no version tag; mLagParams is not walked.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourFailsafe.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// The block is pointer-free, so the host offsets are the console's.
static_assert(offsetof(BehaviourFailsafe::Parameters, mShakeParams)    == 0x08, "mShakeParams @ +0x08");
static_assert(offsetof(BehaviourFailsafe::Parameters, mLagParams)      == 0x18, "mLagParams @ +0x18");
static_assert(offsetof(BehaviourFailsafe::Parameters, mLookerParams)   == 0x2C, "mLookerParams @ +0x2C");
static_assert(offsetof(BehaviourFailsafe::Parameters, mfSlowDistance)  == 0x90, "mfSlowDistance @ +0x90");
static_assert(offsetof(BehaviourFailsafe::Parameters, mfFOV)           == 0xA8, "mfFOV @ +0xA8");
static_assert(offsetof(BehaviourFailsafe::Parameters, mbStickToGround) == 0xC0, "mbStickToGround @ +0xC0");

template<class TSerialiser>
void BehaviourFailsafe::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake Params", mShakeParams);
    lrSerialiser.Serialise("Looker Params", mLookerParams);

    lrSerialiser.Serialise("Slow Distance", mfSlowDistance);
    lrSerialiser.Serialise("Slow Height", mfSlowHeight);
    lrSerialiser.Serialise("Slow Pitch", mfSlowPitch);
    lrSerialiser.Serialise("Fast Distance", mfFastDistance);
    lrSerialiser.Serialise("Fast Height", mfFastHeight);
    lrSerialiser.Serialise("Fast Pitch", mfFastPitch);
    lrSerialiser.Serialise("FOV", mfFOV);
    lrSerialiser.Serialise("Blend Factor Blend Factor", mfBlendFactorBlendFactor);
    lrSerialiser.Serialise("Minimum Blend Factor", mfMinimumBlendFactor);
    lrSerialiser.Serialise("Maximum Blend Factor", mfMaximumBlendFactor);
    lrSerialiser.Serialise("Height Distance Blend Factor", mfHeightDistanceBlendFactor);
    lrSerialiser.Serialise("Height Distance Velocity Range", mfHeightDistanceVelocityRange);
    lrSerialiser.Serialise("Stick to ground", mbStickToGround);
}

template void BehaviourFailsafe::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourFailsafe::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourFailsafe::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
