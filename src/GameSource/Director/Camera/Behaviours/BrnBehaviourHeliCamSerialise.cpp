// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCamSerialise.cpp
//
// Compilation home for BrnDirector::Camera::BehaviourHeliCam::Parameters::Serialise<S> --
// the helicopter-cam parameter-block field-walk visitor. This is a separate TU from
// BrnBehaviourHeliCam.cpp (which anchors the SetParameters slice); it only carries the
// three template instantiations the camera-tunings serialiser system drives:
//   - Serialise<DebugMenuSerialiser>     @0x8224C110
//   - Serialise<TextFileReadSerialiser>  @0x82231D30
//   - Serialise<TextFileWriteSerialiser> @0x8224DE90
//
// ONE uniform templated body drives the serialiser S by name; each S supplies the per-field
// direction (register a debug-menu variable / read a "<label> : <value>\n" line back / write
// one). The three X360 instances share the same field/label sequence -- only S's inlined leaf
// helper differs (Hex-Rays renders the write as FormatName+fprintf, the read as fscanf, the
// debug as Process<T> plus a SetStep/SetRange tuning call). This is the shared source that
// produces all three, mirroring BehaviourRigSerialise.cpp / BrnLookerSerialise.cpp.
//
// VERSION BRANCH (attested in the read @0x82231D30 + write @0x8224DE90 asm): each instance
// stamps muVersion = 2, serialises the tag, then:
//   - version 1 (legacy blocks): walk mShakeParams only, then the scalar tunables;
//   - version 2 (current):       walk mShakeParams + mLookerParams, then the scalar tunables;
//   - anything else:             fire a "code/data version mismatch" assert.
// On the write/debug instances muVersion always reads back 2 (the code stamp), so the version-1
// and mismatch arms are dead there; only the read instance -- whose VersionNumber leaf pulls the
// tag out of the file by reference -- can take the version-1 arm (matching the `if (*v4 == 1)` /
// `if (*v4 != 2)` control flow in @0x82231D30). On the debug-menu instance the VersionNumber leaf
// is a no-op (a version tag is not an editable menu variable), so the compiler proves the tag is 2
// and folds the branch to the version-2 walk (matching the branch-less @0x8224C110).
//
// Field/label map (ascending offset; from all three asm bodies):
//   +0x08 mShakeParams        "Shake Parameters"          (nested; v1 + v2)
//   +0x18 mLookerParams       "Looker Parameters"         (nested; v2 only)
//   +0x7C muVersion           "Version Number (dont change)"
//   +0x84 mfHeight            "Height (KM)"               (debug SetStep 0.01)
//   +0x88 mfInitialDistanceX  "Initial Distance X (KM)"   (debug SetStep 0.01)
//   +0x8C mfInitialDistanceZ  "Initial Distance Z (KM)"   (debug SetStep 0.01)
//   +0x90 mfVelocityMPS       "Velocity MPS"              (debug SetStep 0.01)
//   +0x80 mfFOV               <unk_820051C0>              (debug SetRange 1..150; walked LAST)
// The debug-menu SetStep(0.01)/SetRange(1..150) adjust-tuning calls are per-field properties of
// the DebugMenuSerialiser leaf (Process<float> + a component SetStep/SetRange) and are not part
// of the block's field-walk; the uniform body reduces every leaf to lrSerialiser.Serialise(label,
// member), exactly as the committed BehaviourRig / Looker serialise TUs do.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (version-mismatch guard)

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The current code version of the BehaviourHeliCam::Parameters block. Pinned from the asm:
    // every instance stores 2 into muVersion (`*(a1+124) = 2`) before serialising, and the
    // read/write instances branch the field-walk on the (possibly read-back) tag.
    const u32 KU_BEHAVIOUR_HELI_CAM_PARAMETERS_VERSION = 2u;

    // The prior (shake-only) block version the read/write instances still accept.
    const u32 KU_BEHAVIOUR_HELI_CAM_PARAMETERS_VERSION_LEGACY = 1u;
} // namespace

// FLAG (unrecovered rodata @0x820051C0): the field label the heli-cam Serialise passes for mfFOV
// (the +0x80 field walked last, with the debug-menu range 1..150). The X360 references only the
// address (lis/addi unk_820051C0) in the debug @0x8224C110 and write @0x8224DE90 visitors; the
// literal string bytes are not in the export, so the label is declared extern and NOT fabricated
// (the same @0x820051C0 label the bumper-cam Serialise passes for its FOV field). When that rodata
// is recovered, define it with the literal bytes at @0x820051C0.
extern const char* const KPC_LABEL_820051C0;   // @0x820051C0 -- mfFOV field label

// ----------------------------------------------------------------------------
// BehaviourHeliCam::Parameters::Serialise<S> -- the ONE heli-cam field-walk visitor body.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourHeliCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    // Stamp the code version, serialise the tag, then branch on the (possibly read-back) value.
    muVersion.muVersion = KU_BEHAVIOUR_HELI_CAM_PARAMETERS_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);

    if (muVersion.muVersion == KU_BEHAVIOUR_HELI_CAM_PARAMETERS_VERSION_LEGACY)
    {
        // Version 1 blocks carried only the shake sub-block.
        lrSerialiser.Serialise("Shake Parameters", mShakeParams);
    }
    else if (muVersion.muVersion == KU_BEHAVIOUR_HELI_CAM_PARAMETERS_VERSION)
    {
        // Version 2 (current) added the looker sub-block after the shake sub-block.
        lrSerialiser.Serialise("Shake Parameters", mShakeParams);
        lrSerialiser.Serialise("Looker Parameters", mLookerParams);
    }
    else
    {
        CGS_ASSERT(false, "BehaviourHeliCam::Parameters : code/data version mismatch");
    }

    // The scalar tunables, walked in every version. mfFOV is walked last (its store follows the
    // four KM/MPS fields in all three instances).
    lrSerialiser.Serialise("Height (KM)", mfHeight);
    lrSerialiser.Serialise("Initial Distance X (KM)", mfInitialDistanceX);
    lrSerialiser.Serialise("Initial Distance Z (KM)", mfInitialDistanceZ);
    lrSerialiser.Serialise("Velocity MPS", mfVelocityMPS);
    lrSerialiser.Serialise(KPC_LABEL_820051C0, mfFOV);      // +0x80 -- label unrecovered (extern)
}

// Explicit instantiations -- one per serialiser this block is saved/loaded/menu-mirrored through.
template void BehaviourHeliCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourHeliCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourHeliCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
