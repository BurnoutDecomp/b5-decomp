// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraShake.cpp
//
// Compilation home for the BrnDirector::Camera::Utils::CameraShake slice this TU owns:
//   - CameraShake::Update                                        @0x82221310
//   - CameraShake::Parameters::Serialise<DebugMenuSerialiser>    @0x82215540
//   - CameraShake::Parameters::Serialise<TextFileReadSerialiser> @0x82202EE0
//   - CameraShake::Parameters::Serialise<TextFileWriteSerialiser> @0x82215D90
//
// CameraShake::Parameters itself (the four f32 shake tunables) is homed in the canonical
// Utils/BrnCameraShake.h (the BehaviourRig.h copy that this banner used to name was retired
// in 2026-07-29); this TU bodies the per-frame Update and the ONE field-walk visitor, and
// instantiates the visitor over the three camera-tunings serialisers. The block is the shake
// post-process tunings shared by the gameplay/bystander/gyro/aftertouch/heli camera
// behaviours (each embeds a CameraShake and saves/loads/menus this block through its own
// Serialise<T>).
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnCameraShake.h"             // THE home: CameraShake + ::Parameters
#include "GameSource/Director/Camera/Utils/CameraUtils.h"                // Utils::RotateMatrix44AffineByEulerAnglesZXY
#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"          // (kept: the serialiser slice's original include)
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h" // DebugMenuSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"  // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"   // TextFileReadSerialiser

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{


// ----------------------------------------------------------------------------
// CameraShake::Parameters::Serialise<S> -- the ONE shake-tunings field-walk visitor body.
//
// Walks the block's four f32 tunables in ascending-offset order, handing each to the
// serialiser S by name. S supplies the per-field direction:
//   - Serialise<DebugMenuSerialiser>    @0x82215540: each field -> Process<float>(name, &field)
//       then SetStep(0.01) on the debug component (mpDebugComponent @+0x48; the scalar
//       DebugMenuSerialiser::Serialise(const char*, f32&) inlines to exactly that Process+SetStep
//       pair -- de-inlined here to the uniform Serialise(name, field) call).
//   - Serialise<TextFileWriteSerialiser> @0x82215D90: each field -> FormatName + fprintf
//       "%s : %f\n" when mpFile is open (FormatName + the write inlined into the walk).
//   - Serialise<TextFileReadSerialiser>  @0x82202EE0: each field -> fscanf "%s : %f\n" while
//       the wrapped FILE* is open (the read short-circuits the moment the handle is null).
// The field sequence + labels are identical across all three instances' asm; only S's inlined
// scalar helper differs -- so the source is this single uniform body (mirrors the committed
// BehaviourGameplayBumper::Parameters::Serialise design).
//
// Field/label map (from the DebugMenu/write/read asm, ascending offset):
//   +0x00 mfXYShakeMagnitudeDegs   "Yaw/Pitch Angular shake in degrees"
//   +0x04 mfZShakeMagnitudeDegs    "Roll Angular shake in degrees"
//   +0x08 mfXYWobbleMagnitudeDegs  "Wobble in degrees"
//   +0x0C mfWobbleCenteringFactor  "Wobble centering factor"
// All four labels are fully recovered rodata (no unresolved unk_82xxxxxx address in any of the
// three instances) -- no extern label flag needed.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void CameraShake::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Yaw/Pitch Angular shake in degrees", mfXYShakeMagnitudeDegs);
    lrSerialiser.Serialise("Roll Angular shake in degrees", mfZShakeMagnitudeDegs);
    lrSerialiser.Serialise("Wobble in degrees", mfXYWobbleMagnitudeDegs);
    lrSerialiser.Serialise("Wobble centering factor", mfWobbleCenteringFactor);
}

// Explicit instantiations -- one per serialiser this shake block is menu'd / saved / loaded
// through. All three serialiser types are reconstructed and homed, so (unlike the bumper-cam
// TU that predated the DebugMenuSerialiser home) the debug-menu instance is emitted here too.
template void CameraShake::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void CameraShake::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void CameraShake::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
