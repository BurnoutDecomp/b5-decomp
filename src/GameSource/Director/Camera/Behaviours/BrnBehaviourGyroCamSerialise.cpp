// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCamSerialise.cpp
//
// BehaviourGyroCam::Parameters::Serialise<TSerialiser> -- the gyro-cam tunings walk (three nested
// sub-blocks, twelve f32 tunables, four flags), one generic body instantiated over the three
// camera serialisers. The block has no version tag.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGyroCam.h"

#include "GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.h"   // AttachmentTruck::Parameters (nested block)
#include "GameSource/Director/Camera/Behaviours/Serialisation.h"       // the three serialisers

#include <cstddef>   // offsetof

namespace BrnDirector
{
namespace Camera
{

// The block is pointer-free, so the host offsets are the console's.
static_assert(offsetof(BehaviourGyroCam::Parameters, mShakeParams)                  == 0x08, "mShakeParams @ +0x08");
static_assert(offsetof(BehaviourGyroCam::Parameters, mLookerParams)                 == 0x2C, "mLookerParams @ +0x2C");
static_assert(offsetof(BehaviourGyroCam::Parameters, mAttachmentTruckParams)        == 0x90, "mAttachmentTruckParams @ +0x90");
static_assert(offsetof(BehaviourGyroCam::Parameters, mfSlowDistance)                == 0x98, "mfSlowDistance @ +0x98");
static_assert(offsetof(BehaviourGyroCam::Parameters, mfField_B0)                    == 0xB0, "mfField_B0 @ +0xB0");
static_assert(offsetof(BehaviourGyroCam::Parameters, mfHeightDistanceVelocityRange) == 0xC4, "mfHeightDistanceVelocityRange @ +0xC4");
static_assert(offsetof(BehaviourGyroCam::Parameters, mbUseTruck)                    == 0xC8, "mbUseTruck @ +0xC8");
static_assert(offsetof(BehaviourGyroCam::Parameters, mbStickToGround)               == 0xCB, "mbStickToGround @ +0xCB");

template<class TSerialiser>
void BehaviourGyroCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake Params", mShakeParams);
    lrSerialiser.Serialise("Looker Params", mLookerParams);
    lrSerialiser.Serialise("Attachment truck", mAttachmentTruckParams);

    lrSerialiser.Serialise("Slow Distance", mfSlowDistance);
    lrSerialiser.Serialise("Slow Height", mfSlowHeight);
    lrSerialiser.Serialise("Slow Pitch", mfSlowPitch);
    lrSerialiser.Serialise("Fast Distance", mfFastDistance);
    lrSerialiser.Serialise("Fast Height", mfFastHeight);
    lrSerialiser.Serialise("Fast Pitch", mfFastPitch);
    lrSerialiser.Serialise("FOV", mfField_B0);
    lrSerialiser.Serialise("Blend Factor Blend Factor", mfBlendFactorBlendFactor);
    lrSerialiser.Serialise("Minimum Blend Factor", mfMinimumBlendFactor);
    lrSerialiser.Serialise("Maximum Blend Factor", mfMaximumBlendFactor);
    lrSerialiser.Serialise("Height Distance Blend Factor", mfHeightDistanceBlendFactor);
    lrSerialiser.Serialise("Height Distance Velocity Range", mfHeightDistanceVelocityRange);

    lrSerialiser.Serialise("Use Truck", mbUseTruck);
    lrSerialiser.Serialise("Use Side Vector", mbUseSideVector);
    lrSerialiser.Serialise("Invert Vector", mbInvertVector);
    lrSerialiser.Serialise("Stick to ground", mbStickToGround);
}

template void BehaviourGyroCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourGyroCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourGyroCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
