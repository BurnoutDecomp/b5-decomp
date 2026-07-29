// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumperParameters.cpp
//
// Compilation home for the BrnDirector::Camera::BehaviourGameplayBumper::Parameters serialiser
// slice (class:BrnDirector::Camera::BehaviourGameplayBumper::Parameters):
//   - BehaviourGameplayBumper::Parameters::Serialise<DebugMenuSerialiser>     @0x822308B8
//   - BehaviourGameplayBumper::Parameters::Serialise<TextFileWriteSerialiser> @0x82230B68
//   - BehaviourGameplayBumper::Parameters::Serialise<TextFileReadSerialiser>  @0x82214C70
//
// SPLIT OUT of BrnBehaviourGameplayBumper.cpp on 2026-07-29, when that TU was mounted in the
// game link with the behaviour's RE-BASE onto Camera::Behaviour. The visitor instantiates over
// the three camera-tunings serialisers, none of which is on the runtime director path; leaving
// it beside the behaviour bodies would have forced all three into the exe. This is the same
// split BehaviourGameplayExternal (BrnBehaviourGameplayExternalParameters.cpp) and the
// aftertouch cam (BrnBehaviourAftertouchCamParameters.cpp) already use.
//
// The block itself (BehaviourGameplayBumper::Parameters, deriving Behaviour::Parameters) is
// homed in BrnBehaviourGameplayBumper.h; its seeding step Parameters::Set is bodied beside the
// behaviour. This TU bodies only the ONE field-walk visitor declared there.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourGameplayBumper.h"

// The visitor drives the camera-tunings serialiser S by name; each S provides the scalar field
// helper. Pull in the three serialisers this TU instantiates it over: the two file serialisers
// plus the debug-menu serialiser.
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"

namespace BrnDirector
{
namespace Camera
{

// FLAG (unrecovered rodata @0x820051C0): the field label the bumper-cam Serialise passes for
// mfFOV (the +0x24 field, sibling to "FOV during boost" at +0x30). The X360 references only the
// address (lis/addi unk_820051C0) in both the write @0x822150DC and debug-menu @0x82214B40
// visitors; the literal string bytes are not in the export, so the label is declared extern and
// NOT fabricated (same treatment as the read serialiser's unrecovered component suffixes). When
// that rodata is recovered, define it with the literal bytes at @0x820051C0.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- mfFOV field label

// ----------------------------------------------------------------------------
// BehaviourGameplayBumper::Parameters::Serialise<S> -- the ONE bumper-cam field-walk visitor body.
//
// Walks the block's eleven tunable f32 fields in ascending-offset order, handing each to the
// serialiser S by name. S supplies the per-field direction: TextFileWriteSerialiser writes each as
// a "<label> : <value>\n" line (FormatName + fprintf, inlined -> @0x82214F10); TextFileReadSerialiser
// reads each back (fscanf "%s : %f\n", inlined -> @0x82202B50). The body is uniform across S -- the
// field sequence + labels are identical in the write and read instances' asm; only S's inlined
// scalar helper differs. The DebugMenuSerialiser instance (@0x82214A08, Process<float> + SetStep per
// field) shares this same source body but is NOT instantiated here (its serialiser type has no
// reconstructed home yet -- see the note below).
//
// Field/label map (from the write+read asm, ascending offset):
//   +0x08 mfYOffset "Y Offset"                 +0x1C mfYawSpring      "Yaw Spring"
//   +0x0C mfZOffset "Z Offset"                 +0x20 mfRollSpring     "Roll Spring"
//   +0x10 mfAccelerationDampening "Accel. Dampening"  +0x24 mfFOV     <unk_820051C0>
//   +0x14 mfAccelerationResponse  "Accel. Response"   +0x28 mfBodyRollScale  "Body Roll Scale"
//   +0x18 mfPitchSpring "Pitch Spring"         +0x2C mfBodyPitchScale "Body Pitch Scale"
//                                              +0x30 mfBoostFOV       "FOV during boost"
// ⭐ Every label matches the DWARF member name at that offset one-for-one -- which is what
// re-named the whole block away from the retired slice's mfField08..mfField2C.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourGameplayBumper::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Y Offset", mfYOffset);
    lrSerialiser.Serialise("Z Offset", mfZOffset);
    lrSerialiser.Serialise("Accel. Dampening", mfAccelerationDampening);
    lrSerialiser.Serialise("Accel. Response", mfAccelerationResponse);
    lrSerialiser.Serialise("Pitch Spring", mfPitchSpring);
    lrSerialiser.Serialise("Yaw Spring", mfYawSpring);
    lrSerialiser.Serialise("Roll Spring", mfRollSpring);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfFOV);      // +0x24 -- label unrecovered (extern)
    lrSerialiser.Serialise("Body Roll Scale", mfBodyRollScale);
    lrSerialiser.Serialise("Body Pitch Scale", mfBodyPitchScale);
    lrSerialiser.Serialise("FOV during boost", mfBoostFOV);
}

// Explicit instantiations -- one per serialiser this block is menu-mirrored / saved / loaded through.
// The DebugMenuSerialiser home (BrnDebugMenuSerialiser.h) has since landed -- its scalar
// Serialise(const char*, f32&) overload inlines to the Process<float> + CgsDev::DebugComponent::SetStep
// pair the X360 debug-menu instance @0x82214A08 emits per field -- so the debug-menu instance is now
// emitted here too (mirrors the committed BehaviourBystanderCam::Parameters::Serialise instantiations).
template void BehaviourGameplayBumper::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourGameplayBumper::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourGameplayBumper::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
