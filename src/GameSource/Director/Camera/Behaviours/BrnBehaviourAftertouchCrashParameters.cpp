// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrashParameters.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourAftertouchCrash::Parameters serialiser
// slice: the ONE field-walk visitor body and its three explicit instantiations.
//
// It sits in its own sibling TU, and stays OUT of the exe link, for the same reason every other
// camera's *Parameters.cpp does: the visitor reaches the three camera-tunings serialisers and the
// shared unrecovered field label, none of which is on the runtime director path. The behaviour's
// four virtuals -- which the pooled takedown / crash-mode allocations DO need a vtable for -- are
// in BrnBehaviourAftertouchCrash.cpp, which is mounted.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.h"

// The aftertouch-crash Parameters::Serialise<S> field-walk drives the camera-tunings serialiser S
// by name; pull in the three serialisers this block is menu'd / saved / loaded through.
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // TextFileWriteSerialiser
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // TextFileReadSerialiser
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"  // DebugMenuSerialiser

namespace BrnDirector
{
namespace Camera
{

// FLAG (unrecovered read-only string): the field label the aftertouch-crash walk passes for the
// +0x40 tunable (mfFOV). All three instances reference only the address (lis/addi of one shared
// read-only slot -- DebugMenu, write, and read all reuse the same "%s : %f\n" label), so the
// literal string bytes are not in the export; the label is declared extern and NOT fabricated.
// This is the SAME symbol the committed bumper-cam walk flags for its mfFOV field. Define it with
// the literal bytes when that string is recovered.
extern const char* const KPC_LABEL_820051C0;   // the shared unrecovered field label -- mfFOV here

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::Parameters::Serialise<S> -- the ONE aftertouch-crash field-walk visitor
// body. Recurses into the "Shake Params" sub-block first, then hands the eleven f32 tunables to the
// serialiser S by name (in the console's walk order -- +0x40 before +0x3C). S supplies the per-field
// direction, inlined into each instance:
//   - Serialise<DebugMenuSerialiser>: AddToPath("Shake Params") + recurse, then
//       Process<float>(name, &field) + CgsDev::DebugComponent::SetStep(0.01) per field (the scalar
//       DebugMenuSerialiser::Serialise(const char*, f32&) inlines to exactly that Process+SetStep).
//   - Serialise<TextFileWriteSerialiser>: the "Shake Params" section header + FormatName
//       + fprintf "%s : %f\n" per field, each guarded by mpFile being open.
//   - Serialise<TextFileReadSerialiser>: consume the "Shake Params" header line +
//       recurse, then fscanf "%s : %f\n" per field while the wrapped FILE* is open (the read
//       short-circuits the moment the handle is null).
// The field sequence + labels are identical across all three instances; only S's inlined
// scalar helper differs -- so the source is this single uniform body (mirrors the committed
// CameraImpactEffect::Parameters::Serialise / BehaviourGameplayBumper::Parameters::Serialise design).
//
// Field/label map (from the DebugMenu/write/read instances, in walk order):
//   "Shake Params" -> mShakeParams (+0x08, nested)
//   +0x2C "Slow Distance"                +0x44 "Blend Factor Blend Factor"
//   +0x30 "Slow Height"                  +0x48 "Minimum Blend Factor"
//   +0x34 "Fast Distance"                +0x4C "Maximum Blend Factor"
//   +0x38 "Fast Height"                  +0x54 "Height Distance Blend Factor"
//   +0x40 <label unrecovered -- FLAG>            +0x58 "Height Distance Velocity Range"
//   +0x3C "Pitch"
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourAftertouchCrash::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake Params", mShakeParams);
    lrSerialiser.Serialise("Slow Distance", mfSlowDistance);
    lrSerialiser.Serialise("Slow Height", mfSlowHeight);
    lrSerialiser.Serialise("Fast Distance", mfFastDistance);
    lrSerialiser.Serialise("Fast Height", mfFastHeight);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfFOV);   // +0x40 -- label unrecovered (extern)
    lrSerialiser.Serialise("Pitch", mfPitch);
    lrSerialiser.Serialise("Blend Factor Blend Factor", mfBlendFactorBlendFactor);
    lrSerialiser.Serialise("Minimum Blend Factor", mfMinimumBlendFactor);
    lrSerialiser.Serialise("Maximum Blend Factor", mfMaximumBlendFactor);
    lrSerialiser.Serialise("Height Distance Blend Factor", mfHeightDistanceBlendFactor);
    lrSerialiser.Serialise("Height Distance Velocity Range", mfHeightDistanceVelocityRange);
}

// Explicit instantiations -- one per serialiser this block is menu'd / saved / loaded through.
template void BehaviourAftertouchCrash::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourAftertouchCrash::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourAftertouchCrash::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
