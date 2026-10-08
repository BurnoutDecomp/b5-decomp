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

// The block derives Behaviour::Parameters, whose debug-name pointer is 8 bytes on the host: the
// derived members start at the end of that head and keep the console spacing among themselves
// (console offset - 0x08 from mShakeParams).
typedef BehaviourGyroCam::Parameters GyroCamParameters;
static_assert(offsetof(GyroCamParameters, mShakeParams) == sizeof(Behaviour::Parameters), "mShakeParams follows the head");
static_assert(offsetof(GyroCamParameters, mLagParams)                    - offsetof(GyroCamParameters, mShakeParams) == 0x18 - 0x08, "mLagParams @ console +0x18");
static_assert(offsetof(GyroCamParameters, mLookerParams)                 - offsetof(GyroCamParameters, mShakeParams) == 0x2C - 0x08, "mLookerParams @ console +0x2C");
static_assert(offsetof(GyroCamParameters, mAttachmentTruckParams)        - offsetof(GyroCamParameters, mShakeParams) == 0x90 - 0x08, "mAttachmentTruckParams @ console +0x90");
static_assert(offsetof(GyroCamParameters, mfSlowDistance)                - offsetof(GyroCamParameters, mShakeParams) == 0x98 - 0x08, "mfSlowDistance @ console +0x98");
static_assert(offsetof(GyroCamParameters, mfField_B0)                    - offsetof(GyroCamParameters, mShakeParams) == 0xB0 - 0x08, "mfField_B0 @ console +0xB0");
static_assert(offsetof(GyroCamParameters, mfHeightDistanceVelocityRange) - offsetof(GyroCamParameters, mShakeParams) == 0xC4 - 0x08, "mfHeightDistanceVelocityRange @ console +0xC4");
static_assert(offsetof(GyroCamParameters, mbUseTruck)                    - offsetof(GyroCamParameters, mShakeParams) == 0xC8 - 0x08, "mbUseTruck @ console +0xC8");
static_assert(offsetof(GyroCamParameters, mbStickToGround)               - offsetof(GyroCamParameters, mShakeParams) == 0xCB - 0x08, "mbStickToGround @ console +0xCB");
static_assert(sizeof(GyroCamParameters) == 216, "host size (console 204: 8-byte head pointer)");

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
