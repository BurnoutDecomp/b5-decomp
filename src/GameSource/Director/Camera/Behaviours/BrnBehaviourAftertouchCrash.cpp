// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourAftertouchCrash.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourAftertouchCrash slice this TU owns.
// SetParameters @0x821F4378 and Get* @0x821FA8F8 are defined inline in the header; this .cpp
// is the translation-unit anchor that pulls the header into the compile gate and forces
// out-of-line emission of both. The rest of the behaviour (Construct/Prepare/Update and the
// full rig) lands with its own TU.
//
// SetParameters is adopted by the crash-mode / takedown arbitrator states (Prepare) and the
// arbitrator testbed when they install an aftertouch-crash parameter block; Get* exposes the
// behaviour's embedded sub-object at +0x60 (gated by the +0x3C2 flag).
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

// FLAG (unrecovered rodata @0x820051C0): the field label the aftertouch-crash walk passes for the
// +0x40 tunable (mfField40). All three instances reference only the address (lis/addi unk_820051C0
// -- DebugMenu @0x8224C83C, write @0x8224E854, read reuses the same "%s : %f\n" slot), so the
// literal string bytes are not in the export; the label is declared extern and NOT fabricated. This
// is the SAME symbol the committed bumper-cam walk flags for its mfFOV field. Define it with the
// literal bytes when the @0x820051C0 rodata is recovered.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- mfField40 field label

// Out-of-line anchor: forces SetParameters to be emitted in this TU.
void BehaviourAftertouchCrash_SetParametersAnchor(
    BehaviourAftertouchCrash& lrBehaviour,
    const BehaviourAftertouchCrash::Parameters* lpParameters)
{
    lrBehaviour.SetParameters(lpParameters);
}

// Out-of-line anchor: forces Get* to be emitted in this TU.
BehaviourAftertouchCrash::GettableSubObject* BehaviourAftertouchCrash_GetAnchor(
    BehaviourAftertouchCrash& lrBehaviour)
{
    return lrBehaviour.Get();
}

// ----------------------------------------------------------------------------
// BehaviourAftertouchCrash::Parameters::Serialise<S> -- the ONE aftertouch-crash field-walk visitor
// body. Recurses into the "Shake Params" sub-block first, then hands the eleven f32 tunables to the
// serialiser S by name (in the asm walk order -- +0x40 before +0x3C). S supplies the per-field
// direction, inlined into each instance's asm:
//   - Serialise<DebugMenuSerialiser>    @0x8224C748: AddToPath("Shake Params") + recurse, then
//       Process<float>(name, &field) + CgsDev::DebugComponent::SetStep(0.01) per field (the scalar
//       DebugMenuSerialiser::Serialise(const char*, f32&) inlines to exactly that Process+SetStep).
//   - Serialise<TextFileWriteSerialiser> @0x8224E728: the "Shake Params" section header + FormatName
//       + fprintf "%s : %f\n" per field, each guarded by mpFile being open.
//   - Serialise<TextFileReadSerialiser>  @0x82232330: consume the "Shake Params" header line +
//       recurse, then fscanf "%s : %f\n" per field while the wrapped FILE* is open (the read
//       short-circuits the moment the handle is null).
// The field sequence + labels are identical across all three instances' asm; only S's inlined
// scalar helper differs -- so the source is this single uniform body (mirrors the committed
// CameraImpactEffect::Parameters::Serialise / BehaviourGameplayBumper::Parameters::Serialise design).
//
// Field/label map (from the DebugMenu/write/read asm, in walk order):
//   "Shake Params" -> mShakeParams (+0x08, nested)
//   +0x2C "Slow Distance"                +0x44 "Blend Factor Blend Factor"
//   +0x30 "Slow Height"                  +0x48 "Minimum Blend Factor"
//   +0x34 "Fast Distance"                +0x4C "Maximum Blend Factor"
//   +0x38 "Fast Height"                  +0x54 "Height Distance Blend Factor"
//   +0x40 <label @0x820051C0 unrecovered -- FLAG>   +0x58 "Height Distance Velocity Range"
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
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfField40);   // +0x40 -- label unrecovered (extern)
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
