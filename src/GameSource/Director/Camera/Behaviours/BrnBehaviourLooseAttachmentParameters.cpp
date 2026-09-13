// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachmentParameters.cpp
//
// Compilation home for BrnDirector::Camera::BehaviourLooseAttachment::Parameters::Serialise<S>
// -- the loose-attachment parameter-block field-walk visitor. This is a separate TU from
// BrnBehaviourLooseAttachment.cpp (which owns the behaviour's virtuals and reference binders),
// following the sibling BrnBehaviourAftertouchCrashParameters.cpp / BrnOrientationLagSerialise.cpp
// split: the visitor is only reachable through the camera-tunings serialiser system, whose
// serialiser types are not part of the shipping link, so the behaviour TU mounts without it.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"

// The three Parameters::Serialise<S> instances drive one serialiser type each by name; pull in
// every serialiser this TU instantiates the visitor over. (BrnCameraImpactEffect.h, for the
// embedded "Impact" sub-block's own nested Serialise<S>, arrives via the behaviour header.)
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // TextFileReadSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"  // DebugMenuSerialiser

namespace BrnDirector
{
namespace Camera
{

// FLAG (unrecovered rodata): the field label the loose-attachment Serialise passes for the +0x54
// tunable (mfField54, sibling to "Distance" at +0x50 and "Dutch" at +0x58). All three visitor
// instances reference only the address of that string -- the debug-menu Process<float>, the write
// FormatName and the read scanf -- so the literal bytes are not in the export and the label is
// declared extern and NOT fabricated (the same shared rodata slot the bumper-cam / camera-rig
// visitors reference for their own unrecovered field labels). Define it with the literal bytes
// when that rodata is recovered.
extern const char* const KPC_LABEL_820051C0;   // mfField54 field label

// ----------------------------------------------------------------------------
// BehaviourLooseAttachment::Parameters::Serialise<S> -- the ONE loose-attachment field-walk
// visitor body. Recurses into the embedded impact block as the nested "Impact" section, then
// walks the loose-attachment tunables, handing each to the serialiser S by name. S supplies the
// per-field/-section direction; the field sequence + labels are identical across all three
// instances (mirrors the committed sibling CameraImpactEffect::Parameters::Serialise):
//   - Serialise<DebugMenuSerialiser>:     AddToPath("Impact") + recurse into the impact block,
//       then each float -> Process<float>(name, &field) + SetStep(0.01); the bool via
//       Process<bool>("Look from target", &field).
//   - Serialise<TextFileWriteSerialiser>: the "Impact" section header + recurse, then each float
//       -> FormatName + fprintf "%s : %f\n"; the bool -> "%s : %d\n" when the file is open.
//   - Serialise<TextFileReadSerialiser>:  consume the section-header line + recurse, then each
//       float -> fscanf "%s : %f\n"; the bool -> fscanf "%s : %d\n" while the file is open.
//
// Field/label map (from the write/read/menu assembly, in visitor walk order):
//   +0x2C mImpact            "Impact"              (nested CameraImpactEffect::Parameters block)
//   +0x48 mfPitch            "Pitch"
//   +0x4C mfHeight           "Height"
//   +0x50 mfDistance         "Distance"
//   +0x54 mfField54          <label rodata unrecovered -- extern, FLAG>
//   +0x58 mfDutch            "Dutch"
//   +0x60 mbLookFromTarget   "Look from target"    (walked BEFORE the +0x5C float)
//   +0x5C mfDetachLerpAmount "Detach Lerp Amount"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourLooseAttachment::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Impact", mImpact);
    lrSerialiser.Serialise("Pitch", mfPitch);
    lrSerialiser.Serialise("Height", mfHeight);
    lrSerialiser.Serialise("Distance", mfDistance);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfField54);   // +0x54 -- label unrecovered
    lrSerialiser.Serialise("Dutch", mfDutch);
    lrSerialiser.Serialise("Look from target", mbLookFromTarget);   // +0x60 bool, walked before +0x5C
    lrSerialiser.Serialise("Detach Lerp Amount", mfDetachLerpAmount);
}

// Explicit instantiations -- one per serialiser this block is menu'd / saved / loaded through.
// All three serialiser types are reconstructed and homed (the DebugMenu instance's nested-block
// section overload lands via the additive declaration in BrnDebugMenuSerialiser.h), so every
// present instance is emitted.
template void BehaviourLooseAttachment::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourLooseAttachment::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourLooseAttachment::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
