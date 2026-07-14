// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourRigSerialise.cpp
//
// Compilation home for BrnDirector::Camera::BehaviourRig::Parameters::Serialise<S> --
// the rig-camera parameter-block field-walk visitor. This is a separate TU from
// BehaviourRig.cpp (which owns Parameters::Construct and the behaviour virtuals); it
// only carries the three template instantiations the camera-tunings serialiser system
// drives:
//   - Serialise<DebugMenuSerialiser>     @0x822540F8
//   - Serialise<TextFileReadSerialiser>  @0x822549F0
//   - Serialise<TextFileWriteSerialiser> @0x8224DAE0
//
// ONE uniform templated body drives the serialiser S by name; each S supplies the per-field
// direction (write a "<label> : <value>\n" line / read it back / register a debug-menu
// variable with a 0.01 adjust step). The three X360 instances share the same field/label
// sequence in the same order -- only S's inlined leaf helper differs (Hex-Rays renders the
// write as FormatName+fprintf, the read as fscanf, the debug as Process<T>+SetStep). This
// is the shared source that produces all three.
//
// VERSION GATE (not an assert here): each instance stamps mVersion.muVersion = 1, serialises
// the tag, then walks the block only when the tag still reads 1. On the debug-menu instance
// the VersionNumber leaf is a no-op, so the compiler proves the tag is 1 and folds the gate
// away (matching the gate-less @0x822540F8 walk); on the read/write instances the tag passes
// through the file overload by reference, so the compiler keeps the `if (version == 1)` gate
// (matching the `if (*(a1+8) == 1)` in @0x822549F0 / @0x8224DAE0).
//
// Field/label map (order attested identical across all three asm bodies):
//   +0x08  muVersion             "Version Number (dont change)"   (write-only label; read/debug tag)
//   [gate: version == 1]
//   nested mOrientationLagParams "Orient. Lag Params"
//   nested mPositionLagParams    "Pos. Lag Params"
//   nested mRigParams            "Rig Params"
//   bool   mbReverse             "Reverse Angle"
//   bool   mbUseOrientationLag   "Use Orient. Lag"
//   bool   mbUsePositionLag      "Use Pos. Lag"
//   f32    mfDOFNear             "DOF Near"
//   f32    mfDOFFar              "DOF Far"
//   f32    mfDOFBlurDepth        "DOF Depth"
//   f32    mfDOFIntensity        "DOF Intensity"
// (mShakeParams / mLookerParams / the accel-spring tunables / mbUseAccelSpring / mbUseShake are
//  NOT walked by this visitor -- consistent across all three instances' asm.)
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The current code version of the BehaviourRig::Parameters block. Pinned from the asm:
    // every instance stores 1 into muVersion (`*(a1+8) = 1`) before serialising, and the
    // read/write instances gate the field-walk on the (possibly read-back) tag equalling it.
    const u32 KU_BEHAVIOUR_RIG_PARAMETERS_VERSION = 1u;
} // namespace

// ----------------------------------------------------------------------------
// BehaviourRig::Parameters::Serialise<S> -- the ONE rig-camera field-walk visitor body.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void BehaviourRig::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    // Stamp the code version, serialise the tag, then only walk the block when the tag
    // matches. On the write/read instances the tag round-trips through the file and the gate
    // is kept; on the debug-menu instance the VersionNumber leaf is a no-op (the version is
    // not exposed as a menu variable) so the gate folds away.
    muVersion.muVersion = KU_BEHAVIOUR_RIG_PARAMETERS_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);

    if (muVersion.muVersion == KU_BEHAVIOUR_RIG_PARAMETERS_VERSION)
    {
        lrSerialiser.Serialise("Orient. Lag Params", mOrientationLagParams);
        lrSerialiser.Serialise("Pos. Lag Params", mPositionLagParams);
        lrSerialiser.Serialise("Rig Params", mRigParams);

        lrSerialiser.Serialise("Reverse Angle", mbReverse);
        lrSerialiser.Serialise("Use Orient. Lag", mbUseOrientationLag);
        lrSerialiser.Serialise("Use Pos. Lag", mbUsePositionLag);

        lrSerialiser.Serialise("DOF Near", mfDOFNear);
        lrSerialiser.Serialise("DOF Far", mfDOFFar);
        lrSerialiser.Serialise("DOF Depth", mfDOFBlurDepth);
        lrSerialiser.Serialise("DOF Intensity", mfDOFIntensity);
    }
}

// Explicit instantiations -- one per serialiser this block is saved/loaded/menu-mirrored through.
template void BehaviourRig::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourRig::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourRig::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
