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

// The block derives Behaviour::Parameters, whose debug-name pointer is 8 bytes on the host: the
// derived members start at the end of that head and keep the console spacing among themselves
// (console offset - 0x08 from mShakeParams).
typedef BehaviourFailsafe::Parameters FailsafeParameters;
static_assert(offsetof(FailsafeParameters, mShakeParams) == sizeof(Behaviour::Parameters), "mShakeParams follows the head");
static_assert(offsetof(FailsafeParameters, mLagParams)      - offsetof(FailsafeParameters, mShakeParams) == 0x18 - 0x08, "mLagParams @ console +0x18");
static_assert(offsetof(FailsafeParameters, mLookerParams)   - offsetof(FailsafeParameters, mShakeParams) == 0x2C - 0x08, "mLookerParams @ console +0x2C");
static_assert(offsetof(FailsafeParameters, mfSlowDistance)  - offsetof(FailsafeParameters, mShakeParams) == 0x90 - 0x08, "mfSlowDistance @ console +0x90");
static_assert(offsetof(FailsafeParameters, mfFOV)           - offsetof(FailsafeParameters, mShakeParams) == 0xA8 - 0x08, "mfFOV @ console +0xA8");
static_assert(offsetof(FailsafeParameters, mbStickToGround) - offsetof(FailsafeParameters, mShakeParams) == 0xC0 - 0x08, "mbStickToGround @ console +0xC0");
static_assert(sizeof(FailsafeParameters) == 208, "host size (console 196: 8-byte head pointer)");

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
