// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCamSerialise.cpp
//
// Compilation home for BrnDirector::Camera::BehaviourBystanderCam::Parameters::Serialise<S> --
// the bystander-cam parameter-block field-walk visitor. This is a separate TU from
// BehaviourBystanderCam.cpp (which owns Parameters::Construct and the behaviour virtuals); it only
// carries the three template instantiations the camera-tunings serialiser system drives:
//   - Serialise<DebugMenuSerialiser>     @0x8224BF40
//   - Serialise<TextFileReadSerialiser>  @0x82231B90
//   - Serialise<TextFileWriteSerialiser> @0x8224DC28
//
// ONE uniform templated body drives the serialiser S by name; each S supplies the per-field
// direction. The three X360 instances share the same nested-block + scalar field/label sequence in
// the same order -- only S's inlined leaf helper differs:
//   - DebugMenuSerialiser  @0x8224BF40: nested blocks pushed onto the menu path (AddToPath) then
//       recursed + popped; each scalar -> Process<T>(name,&field) then SetStep(0.01) on the debug
//       component (mpDebugComponent @+0x48) -- the scalar DebugMenuSerialiser::Serialise(f32&/bool&)
//       overload inlines to exactly that Process+SetStep pair (SetStep only for the floats; the two
//       bools take no step). De-inlined here to the uniform Serialise(name, field) call.
//   - TextFileWriteSerialiser @0x8224DC28: nested blocks -> the nested-block Serialise<T> template
//       (section header line + recurse); each scalar -> FormatName + fprintf "%s : %f\n" / "%s : %d\n"
//       while mpFile (+0x04) is open.
//   - TextFileReadSerialiser  @0x82231B90: nested blocks -> consume the section header line + recurse;
//       each scalar -> fscanf "%s : %f\n" / "%s : %d\n" while the wrapped FILE* is open (each read
//       short-circuits the moment the handle is null).
// Reconstructed as the shared source that produces all three (mirrors the committed
// BehaviourRig::Parameters::Serialise and Looker::Parameters::Serialise designs).
//
// Field/label map (ascending offset; identical across all three instances' asm):
//   +0x08 mShakeParams                          "Shake parameters"   (nested CameraShake::Parameters)
//   +0x18 mLookerParams                         "Looker parameters"  (nested Looker::Parameters)
//   +0x7C mfVelocityInfluenceOnPosition         "Target velocity influence on pos"
//   +0x80 mfMaxInitialDistanceKM                "Max initial distance KM"
//   +0x84 mfDistanceForFailKM                   "Distance for fail KM"
//   +0x88 mfTargetSpaceX                        "Target Space X"
//   +0x8C mfTargetSpaceY                        "Target Space Y"
//   +0x90 mfTargetSpaceZ                        "Target Space Z"
//   +0x94 mfHeight                              "Height"
//   +0x98 mbUseTargetSpaceInsteadOfPositionFinder "Use target space"
//   +0x99 mbUseRangeTesting                     "Use range testing"
// All labels are fully recovered rodata (no unresolved unk_82xxxxxx address in any of the three
// instances) -- no extern label flag needed. Unlike the leading version-tagged blocks (Looker/rig),
// the bystander block carries NO leading VersionNumber field -- the walk starts straight at the two
// nested sub-blocks, matching the asm (the first act is AddToPath/Serialise on a1+8, no `*a1 = 1`).
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourBystanderCamSerialise.h"

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"

namespace BrnDirector
{
namespace Camera
{

// ----------------------------------------------------------------------------
// BehaviourBystanderCam::Parameters::Serialise<S> -- the ONE bystander-cam field-walk visitor body.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourBystanderCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Shake parameters", mShakeParams);
    lrSerialiser.Serialise("Looker parameters", mLookerParams);

    lrSerialiser.Serialise("Target velocity influence on pos", mfVelocityInfluenceOnPosition);
    lrSerialiser.Serialise("Max initial distance KM", mfMaxInitialDistanceKM);
    lrSerialiser.Serialise("Distance for fail KM", mfDistanceForFailKM);
    lrSerialiser.Serialise("Target Space X", mfTargetSpaceX);
    lrSerialiser.Serialise("Target Space Y", mfTargetSpaceY);
    lrSerialiser.Serialise("Target Space Z", mfTargetSpaceZ);
    lrSerialiser.Serialise("Height", mfHeight);
    lrSerialiser.Serialise("Use target space", mbUseTargetSpaceInsteadOfPositionFinder);
    lrSerialiser.Serialise("Use range testing", mbUseRangeTesting);
}

// Explicit instantiations -- one per serialiser this block is menu-mirrored / saved / loaded through.
// All three serialiser types are reconstructed and homed, so (unlike the bumper-cam TU that predated
// the DebugMenuSerialiser home) the debug-menu instance is emitted here too.
template void BehaviourBystanderCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);
template void BehaviourBystanderCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourBystanderCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);

} // namespace Camera
} // namespace BrnDirector
