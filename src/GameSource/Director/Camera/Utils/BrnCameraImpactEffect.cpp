#include "GameSource/Director/Camera/Utils/BrnCameraImpactEffect.h"

// The three CameraImpactEffect::Parameters::Serialise<S> instances drive one serialiser type
// each by name; pull in every serialiser this TU instantiates the visitor over. (BehaviourRig.h,
// for the embedded CameraShake::Parameters sub-block, arrives transitively via the header.)
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // TextFileReadSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"  // DebugMenuSerialiser

#include <cstddef>   // offsetof (layout pins)

// BrnDirector::Camera::Utils::CameraImpactEffect -- reconstructed from
// BURNOUT_X360_ARTIST.XEX.
//
// Bodied here:
//   class:BrnDirector::Camera::Utils::CameraImpactEffect
//     CameraImpactEffect::RegisterImpact @0x821F3648
//   class:BrnDirector::Camera::Utils::CameraImpactEffect::Parameters
//     Parameters::Serialise<DebugMenuSerialiser>     @0x822327B0
//     Parameters::Serialise<TextFileReadSerialiser>  @0x82215CF0
//     Parameters::Serialise<TextFileWriteSerialiser> @0x82232D98

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

// Layout pins (host-pointer-width invariant -- the block holds no pointers): the embedded shake
// sub-block sits at +0x00 and the three impact tunables at the a1+0x10/+0x14/+0x18 offsets the
// write/read/menu asm addresses.
static_assert(offsetof(CameraImpactEffect::Parameters, mShakeParams) == 0x00,
              "CameraImpactEffect::Parameters::mShakeParams @ +0x00");
static_assert(offsetof(CameraImpactEffect::Parameters, mfShakeDecayFactor) == 0x10,
              "CameraImpactEffect::Parameters::mfShakeDecayFactor @ +0x10");
static_assert(offsetof(CameraImpactEffect::Parameters, mfShakeMagnitude) == 0x14,
              "CameraImpactEffect::Parameters::mfShakeMagnitude @ +0x14");
static_assert(offsetof(CameraImpactEffect::Parameters, mfShakeFrequencyScale) == 0x18,
              "CameraImpactEffect::Parameters::mfShakeFrequencyScale @ +0x18");

// ⭐ RegisterImpact @0x821F3648 MOVED OUT (2026-08-29, crash-camera wave), to the partfile
//     Camera/Utils/BrnCameraImpactEffectRegisterImpact.cpp
// Same split, same reason as CameraShake::Update -> BrnCameraShakeUpdate.cpp: this TU's three
// explicit Parameters::Serialise<S> instantiations drag DebugMenuSerialiser /
// TextFileWriteSerialiser / TextFileReadSerialiser, none of which is on the exe source list, so
// mounting this file to reach ONE tiny impact accumulator opens serialiser externals to close an
// impact one. The partfile has no dependency but this header, so it mounts for free.
// DELETE-WHEN: the three camera-tunings serialisers are mounted -- then fold it back in.

// ----------------------------------------------------------------------------
// CameraImpactEffect::Parameters::Serialise<S> -- the ONE impact-shake field-walk visitor body.
//
// Recurses into the embedded shake block as the nested "Shake parameters" section, then walks the
// three impact tunables in ascending-offset order, handing each to the serialiser S by name. S
// supplies the per-field/-section direction; the field sequence + labels are identical across all
// three instances' asm, so the source is this single uniform body (mirrors the committed sibling
// CameraShake::Parameters::Serialise in BrnCameraShake.cpp):
//   - Serialise<DebugMenuSerialiser>    @0x822327B0: AddToPath("Shake parameters") + recurse +
//       pop path, then each float -> Process<float>(name, &field) + SetStep(0.01) (the scalar
//       DebugMenuSerialiser::Serialise(const char*, f32&) inlines to that Process+SetStep pair).
//   - Serialise<TextFileWriteSerialiser> @0x82232D98: the "Shake parameters" section header +
//       recurse, then each float -> FormatName + fprintf "%s : %f\n" when mpFile is open.
//   - Serialise<TextFileReadSerialiser>  @0x82215CF0: consume the section-header line + recurse,
//       then each float -> fscanf "%s : %f\n" while the wrapped FILE* is open.
//
// Field/label map (from the write/read/menu asm, ascending offset):
//   +0x00 mShakeParams           "Shake parameters"       (nested CameraShake::Parameters block)
//   +0x10 mfShakeDecayFactor     "Shake decay factor"
//   +0x14 mfShakeMagnitude       "Shake magnitude"
//   +0x18 mfShakeFrequencyScale  "Shake frequency scale"
// All four labels are fully recovered rodata (no unresolved unk_82xxxxxx address in any of the
// three instances) -- no extern label flag needed.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void CameraImpactEffect::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake parameters", mShakeParams);
    lrSerialiser.Serialise("Shake decay factor", mfShakeDecayFactor);
    lrSerialiser.Serialise("Shake magnitude", mfShakeMagnitude);
    lrSerialiser.Serialise("Shake frequency scale", mfShakeFrequencyScale);
}

// Explicit instantiations -- one per serialiser this block is menu'd / saved / loaded through.
// All three serialiser types are reconstructed and homed (the DebugMenu instance's nested-block
// section overload lands via the additive declaration in BrnDebugMenuSerialiser.h), so every
// present instance is emitted (unlike the older bumper-cam TU that predated the DebugMenu home).
template void CameraImpactEffect::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void CameraImpactEffect::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void CameraImpactEffect::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

}
}
}
