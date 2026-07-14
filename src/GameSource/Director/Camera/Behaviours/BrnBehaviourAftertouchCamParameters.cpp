// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCamParameters.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourAftertouchCam::Parameters serialiser
// slice this TU owns (class:BrnDirector::Camera::BehaviourAftertouchCam::Parameters):
//   - BehaviourAftertouchCam::Parameters::Serialise<DebugMenuSerialiser>    @0x8224C530
//   - BehaviourAftertouchCam::Parameters::Serialise<TextFileWriteSerialiser> @0x8224E458
//   - BehaviourAftertouchCam::Parameters::Serialise<TextFileReadSerialiser>  @0x822321B0
//
// BehaviourAftertouchCam::Parameters itself (the embedded shake block + the eleven aftertouch
// tunables) is homed, fully laid out, in BrnBehaviourAftertouchCam.h; this TU only bodies the
// ONE field-walk visitor declared there and instantiates it over the three camera-tunings
// serialisers. The block is the aftertouch (slow-motion crash-follow) camera's authored tunings,
// menu'd / saved / loaded through each serialiser's uniform Serialise<T> protocol.
//
// This mirrors the committed CameraShake::Parameters serialiser TU (BrnCameraShake.cpp) and the
// bumper-cam serialiser TU (BrnBehaviourGameplayBumper.cpp) exactly: one uniform Serialise body,
// one explicit instantiation per serialiser.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCam.h"   // Parameters (the block being walked) + embedded CameraShake::Parameters
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"      // DebugMenuSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"       // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"        // TextFileReadSerialiser

namespace BrnDirector
{
namespace Camera
{

// FLAG (unrecovered rodata @0x820051C0): the field label the aftertouch-cam Serialise passes for
// the +0x40 float (mfField40). The X360 references only the address (lis/addi unk_820051C0) in all
// three visitors -- the debug-menu @0x8224C530, write @0x8224E458 and (implicitly via the same
// literal) read walkers; the literal string bytes are not in the export, so the label is declared
// extern and NOT fabricated. This is the SAME rodata address the committed bumper-cam serialiser
// flags for its mfFOV field (BrnBehaviourGameplayBumper.cpp), so the two share one extern; when the
// bytes at @0x820051C0 are recovered, defining it there resolves both. Same treatment as the read
// serialiser's unrecovered component suffixes.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- mfField40 label (shared with bumper-cam mfFOV)

// ----------------------------------------------------------------------------
// BehaviourAftertouchCam::Parameters::Serialise<S> -- the ONE aftertouch-cam field-walk visitor body.
//
// Walks the block into the serialiser S: first the embedded shake block as a "Shake Params"
// sub-section (S's nested-block Serialise<CameraShake::Parameters> overload: AddToPath+recurse+pop
// for the debug menu; a section-header line + recurse for the text files), then the eleven f32
// tunables in the exact order all three visitors' asm emit them. S supplies the per-field direction:
//   - Serialise<DebugMenuSerialiser>    @0x8224C530: each field -> Process<float>(name,&field) then
//       SetStep(0.01) on the debug component (the scalar DebugMenu Serialise(const char*, f32&)
//       inlines to exactly that Process+SetStep pair -- de-inlined here to the uniform call).
//   - Serialise<TextFileWriteSerialiser> @0x8224E458: each field -> FormatName + fprintf "%s : %f\n"
//       when mpFile is open (FormatName + the write inlined into the walk).
//   - Serialise<TextFileReadSerialiser>  @0x822321B0: each field -> fscanf "%s : %f\n" while the
//       wrapped FILE* is open (the read short-circuits the moment the handle is null).
//
// The field/label sequence is identical across all three instances' asm (only S's inlined scalar/
// nested helper differs), so the source is this single uniform body. NOTE the walk order is NOT
// ascending offset: the +0x40 field (<unk_820051C0>) is emitted BEFORE the +0x3C "Pitch" field in
// all three visitors -- reproduced here store-for-store.
//
// Field/label map (walk order; offsets from the asm displacements):
//   +0x08 mShakeParams                    "Shake Params" (nested CameraShake::Parameters)
//   +0x2C mfSlowDistance                  "Slow Distance"
//   +0x30 mfSlowHeight                    "Slow Height"
//   +0x34 mfFastDistance                  "Fast Distance"
//   +0x38 mfFastHeight                    "Fast Height"
//   +0x40 mfField40                       <unk_820051C0>   (label unrecovered -- extern above)
//   +0x3C mfPitch                         "Pitch"
//   +0x44 mfBlendFactorBlendFactor        "Blend Factor Blend Factor"
//   +0x48 mfMinimumBlendFactor            "Minimum Blend Factor"
//   +0x4C mfMaximumBlendFactor            "Maximum Blend Factor"
//   +0x50 mfHeightDistanceBlendFactor     "Height Distance Blend Factor"
//   +0x54 mfHeightDistanceVelocityRange   "Height Distance Velocity Range"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourAftertouchCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake Params", mShakeParams);
    lrSerialiser.Serialise("Slow Distance", mfSlowDistance);
    lrSerialiser.Serialise("Slow Height", mfSlowHeight);
    lrSerialiser.Serialise("Fast Distance", mfFastDistance);
    lrSerialiser.Serialise("Fast Height", mfFastHeight);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfField40);   // +0x40 -- label unrecovered (extern)
    lrSerialiser.Serialise("Pitch", mfPitch);
    lrSerialiser.Serialise("Blend Factor Blend Factor", mfBlendFactorBlendFactor);
    lrSerialiser.Serialise("Minimum Blend Factor", mfMinimumBlendFactor);
    lrSerialiser.Serialise("Maximum Blend Factor", mfMaximumBlendFactor);
    lrSerialiser.Serialise("Height Distance Blend Factor", mfHeightDistanceBlendFactor);
    lrSerialiser.Serialise("Height Distance Velocity Range", mfHeightDistanceVelocityRange);
}

// Explicit instantiations -- one per serialiser this block is menu'd / saved / loaded through.
// All three serialiser types are reconstructed and homed, so the debug-menu instance is emitted
// here too (matching the committed CameraShake::Parameters serialiser TU).
template void BehaviourAftertouchCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourAftertouchCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourAftertouchCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
