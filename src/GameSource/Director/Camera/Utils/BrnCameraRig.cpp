// ============================================================================
// GameSource/Director/Camera/Utils/BrnCameraRig.cpp
//
// Compilation home for the BrnDirector::Camera::Utils::CameraRig::Params field-walk
// serialiser slice this TU owns:
//   - CameraRig::Params::Serialise<DebugMenuSerialiser>     @0x822326A8
//   - CameraRig::Params::Serialise<TextFileReadSerialiser>  @0x82232CA8
//   - CameraRig::Params::Serialise<TextFileWriteSerialiser> @0x82216020
//
// CameraRig::Params is a small author-visible camera-rig parameter block (two Vector3
// offsets + FOV + Roll/Pitch/Yaw + a widescreen flag); its FULL layout is already homed
// from the DWARF in BehaviourRig.h (Utils::CameraRig::Params). This TU provides ONE
// templated field-walk visitor body -- the de-inlined `void Serialise<S>(S&)` that hands
// each field to the serialiser S by name -- plus one explicit instantiation per serialiser
// the rig block is saved/loaded/debug-menu'd through. The per-field direction (debug-menu
// register + step / text write "<label> : <value>\n" / text read fscanf) lives in each S's
// own leaf overloads (separate TUs); this body is uniform across all three, exactly as the
// three X360 instances' asm show (identical field sequence + labels, only S's inlined leaf
// helper differs).
//
// Field/label map (from the write @0x82216020, read @0x82232CA8 and debug-menu @0x822326A8
// asm, ascending offset):
//   +0x00 mOffsetFromTarget         "Offset from car"              (Vector3)
//   +0x10 mOffsetFromRotationCentre "Offset from rotation centre"  (Vector3)
//   +0x20 mfFOV                     <unk_820051C0>  -- label unrecovered (extern, FLAG)
//   +0x24 mfRoll                    "Roll"
//   +0x28 mfPitch                   "Pitch"
//   +0x2C mfYaw                     "Yaw"
//   +0x30 mbWidescreenOnly          "Widescreen Only"              (bool)
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"           // Utils::CameraRig::Params (DWARF home) + Vector3
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"  // DebugMenuSerialiser leaf overloads
#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"   // TextFileWriteSerialiser leaf overloads
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"    // TextFileReadSerialiser leaf overloads

namespace BrnDirector
{
namespace Camera
{

// FLAG (unrecovered rodata @0x820051C0): the field label the rig serialiser passes for mfFOV (the
// +0x20 field). Every instance references only the address (lis/addi unk_820051C0) in the write
// @0x82216070, read @0x82232CE8-forwarded and debug-menu @0x822326E8 visitors; the literal string
// bytes are not in the export, so the label is declared extern and NOT fabricated. This is the SAME
// rodata the bumper-cam FOV field labels (BrnBehaviourGameplayBumper.cpp declares the identical
// extern), i.e. the shared "FOV" tunable label. When that rodata is recovered, define it once with
// the literal bytes at @0x820051C0.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- mfFOV field label

namespace Utils
{

// ----------------------------------------------------------------------------
// CameraRig::Params::Serialise<S> -- the ONE rig-block field-walk visitor body.
//
// Hands each field to S by name in ascending-offset order. S supplies the per-field direction:
//   - DebugMenuSerialiser  (@0x822326A8): vec3 overload registers x/y/z (Process<float>+SetStep
//     0.01); the scalar/bool overloads register each as a debug-menu variable (inlined here to
//     Process<float>/Process<bool> + SetStep in the asm).
//   - TextFileWriteSerialiser (@0x82216020): vec3 overload writes the block (sub_822089C0); the
//     scalar/bool overloads write "<label> : <value>\n" (FormatName + fprintf, inlined).
//   - TextFileReadSerialiser  (@0x82232CA8): vec3 overload reads the block (dispatcher @0x82219AE0);
//     the scalar/bool overloads read the value line back (fscanf "%s : %f\n" / "%s : %d\n", inlined).
// The three instances' asm carry an identical field sequence + labels; only S's inlined leaf helper
// differs -- so this body is uniform.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void CameraRig::Params::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Offset from car", mOffsetFromTarget);             // +0x00
    lrSerialiser.Serialise("Offset from rotation centre", mOffsetFromRotationCentre); // +0x10
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfFOV);                        // +0x20 -- label unrecovered (extern)
    lrSerialiser.Serialise("Roll", mfRoll);                                   // +0x24
    lrSerialiser.Serialise("Pitch", mfPitch);                                 // +0x28
    lrSerialiser.Serialise("Yaw", mfYaw);                                     // +0x2C
    lrSerialiser.Serialise("Widescreen Only", mbWidescreenOnly);             // +0x30
}

// Explicit instantiations -- one per serialiser the rig block is walked through (attested by the
// three CameraRig::Params::Serialise<S> functions in the export).
template void CameraRig::Params::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void CameraRig::Params::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void CameraRig::Params::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
