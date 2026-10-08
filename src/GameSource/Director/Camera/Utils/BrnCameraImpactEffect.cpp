// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraImpactEffect.cpp
//
// CameraImpactEffect::Parameters::Serialise<TSerialiser> -- the impact-shake tunings walk (the
// nested "Shake parameters" block, then three f32 fields), one generic body instantiated over the
// three camera serialisers (text-file read / write, debug menu). RegisterImpact lives in
// BrnCameraImpactEffectRegisterImpact.cpp.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

#include <cstddef>   // offsetof (layout pins)

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// Layout pins (the block holds no pointers, so host offsets are the console's).
static_assert(offsetof(CameraImpactEffect::Parameters, mShakeParams) == 0x00,
              "CameraImpactEffect::Parameters::mShakeParams @ +0x00");
static_assert(offsetof(CameraImpactEffect::Parameters, mfShakeDecayFactor) == 0x10,
              "CameraImpactEffect::Parameters::mfShakeDecayFactor @ +0x10");
static_assert(offsetof(CameraImpactEffect::Parameters, mfShakeMagnitude) == 0x14,
              "CameraImpactEffect::Parameters::mfShakeMagnitude @ +0x14");
static_assert(offsetof(CameraImpactEffect::Parameters, mfShakeFrequencyScale) == 0x18,
              "CameraImpactEffect::Parameters::mfShakeFrequencyScale @ +0x18");

template<class TSerialiser>
void CameraImpactEffect::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake parameters", mShakeParams);
    lrSerialiser.Serialise("Shake decay factor", mfShakeDecayFactor);
    lrSerialiser.Serialise("Shake magnitude", mfShakeMagnitude);
    lrSerialiser.Serialise("Shake frequency scale", mfShakeFrequencyScale);
}

template void CameraImpactEffect::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void CameraImpactEffect::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void CameraImpactEffect::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
