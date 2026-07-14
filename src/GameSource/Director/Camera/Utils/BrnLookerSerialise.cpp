// ============================================================================
// GameSource/Director/Camera/Utils/BrnLookerSerialise.cpp
//
// Compilation home for BrnDirector::Camera::Utils::Looker::Parameters::Serialise<S> --
// the look-at/zoom parameter-block field-walk visitor. This is a separate TU from
// BrnLooker.cpp (which owns Construct/Track/Zoom/Update); it only carries the three
// template instantiations the camera-tunings serialiser system drives:
//   - Serialise<DebugMenuSerialiser>    @0x822156E0
//   - Serialise<TextFileReadSerialiser> @0x822030E8
//   - Serialise<TextFileWriteSerialiser>@0x82216198
//
// ONE uniform templated body drives the serialiser S by name; each S supplies the per-field
// direction (write a "<label> : <value>\n" line / read it back / register a debug-menu
// variable). The three X360 instances are byte-for-byte the same field/label sequence in the
// same order -- only S's inlined leaf helper differs (Hex-Rays renders the write as FormatName+
// fprintf, the read as fscanf, the debug as Process<T>+SetStep). Reconstructed as the shared
// source that produces all three.
//
// The leading version handling (mVersion tag + the code/data version-mismatch assert) is part
// of the uniform body: it is emitted by the write/read instances (which serialise the tag and
// keep the mismatch assert) and elided by the debug-menu instance (whose VersionNumber leaf is a
// no-op -- a version tag is not an editable menu variable -- so the tag store is the only trace,
// matching the `*a1 = 1` with no Process/assert in @0x822156E0).
//
// Field/label map (ascending offset; from the write @0x82216198 + read @0x822030E8 asm):
//   +0x00 mVersion                       "Version Number (dont change)"
//   +0x04 mfInitialXLookOffsetRange      "Initial X LookOffset range"
//   +0x08 mfInitialYLookOffsetRange      "Initial Y LookOffset range"
//   +0x0C mfTargetSubjectSize            "Target subject size"
//   +0x20 mfTrackingTolerance            "Tracking tolerance"
//   +0x24 mfTrackingSpeed                "Tracking speed"
//   +0x28 mfTrackingAcceleration         "Tracking acceleration"
//   +0x2C mfMinFOVVelocity               "Min FOV speed"
//   +0x30 mfMaxFOVVelocity               "Max FOV speed"
//   +0x34 mfDesiredPerceivedDistance     "Desired perceived distance"
//   +0x38 mfDistanceToVelocityFactor     "Distance to Velocity factor"
//   +0x3C mfToleranceForDistanceFromIdeal  "Tolerance for distance from ideal"
//   +0x40 mfToleranceForDistanceFromTarget "Tolerance for distance from target"
//   +0x44 mfOvershootFactor              "Overshoot factor"
//   +0x48 mfMinFOV                       "Min FOV"
//   +0x4C mfMaxFOV                       "Max FOV"
//   +0x50 mfIdealFOVVelocityLerpAmount   "Ideal FOV velocity blending amount"
//   +0x54 mfStaticDOF                    "Static DOF"
//   +0x58 mfStaticFocalLength            "Static Focal Length"
//   +0x5C mbUseStaticDOF                 "Use Static DOF"
//   +0x5D mbInitialiseToLookingAtTarget  "Initialise to looking at target?"
//   +0x5E mbInitialiseToZoomedToTarget   "Initialise to zoomed in to target?"
//   +0x5F mbUseZoom                      "Use zoom?"
// (The +0x10..+0x1C subject X/Y size/screen-offset fields and the +0x60 meZoomType are NOT
// walked by this visitor -- consistent across all three instances' asm.)
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnLooker.h"

#include "GameSource/Director/Camera/Utils/BrnTextFileWriteSerialiser.h"
#include "GameSource/Director/Camera/Utils/BrnTextFileReadSerialiser.h"
#include "GameSource/Director/Camera/Behaviours/BrnDebugMenuSerialiser.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (version-mismatch guard)

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

namespace
{
    // The current code version of the Looker::Parameters block. Pinned from the asm: every
    // instance stores 1 into mVersion (`*a1 = 1`) before serialising, and the write/read
    // instances assert the (possibly read-back) tag still equals it (BrnLooker.h:222).
    const u32 KU_LOOKER_PARAMETERS_VERSION = 1u;
} // namespace

// ----------------------------------------------------------------------------
// Looker::Parameters::Serialise<S> -- the ONE look-at/zoom field-walk visitor body.
// ----------------------------------------------------------------------------
template<class TSerialiser>
void Looker::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    // Stamp the code version, serialise the tag, then assert the data matches the code. On the
    // write/read instances the tag round-trips through the file; on the debug-menu instance the
    // VersionNumber leaf is a no-op (the version is not exposed as a menu variable).
    mVersion.muVersion = KU_LOOKER_PARAMETERS_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", mVersion);
    CGS_ASSERT(mVersion.muVersion == KU_LOOKER_PARAMETERS_VERSION,
               "Looker::Parameters : code/data version mismatch");

    lrSerialiser.Serialise("Initial X LookOffset range", mfInitialXLookOffsetRange);
    lrSerialiser.Serialise("Initial Y LookOffset range", mfInitialYLookOffsetRange);
    lrSerialiser.Serialise("Target subject size", mfTargetSubjectSize);
    lrSerialiser.Serialise("Tracking tolerance", mfTrackingTolerance);
    lrSerialiser.Serialise("Tracking speed", mfTrackingSpeed);
    lrSerialiser.Serialise("Tracking acceleration", mfTrackingAcceleration);
    lrSerialiser.Serialise("Min FOV speed", mfMinFOVVelocity);
    lrSerialiser.Serialise("Max FOV speed", mfMaxFOVVelocity);
    lrSerialiser.Serialise("Desired perceived distance", mfDesiredPerceivedDistance);
    lrSerialiser.Serialise("Distance to Velocity factor", mfDistanceToVelocityFactor);
    lrSerialiser.Serialise("Tolerance for distance from ideal", mfToleranceForDistanceFromIdeal);
    lrSerialiser.Serialise("Tolerance for distance from target", mfToleranceForDistanceFromTarget);
    lrSerialiser.Serialise("Overshoot factor", mfOvershootFactor);
    lrSerialiser.Serialise("Min FOV", mfMinFOV);
    lrSerialiser.Serialise("Max FOV", mfMaxFOV);
    lrSerialiser.Serialise("Ideal FOV velocity blending amount", mfIdealFOVVelocityLerpAmount);
    lrSerialiser.Serialise("Static DOF", mfStaticDOF);
    lrSerialiser.Serialise("Static Focal Length", mfStaticFocalLength);
    lrSerialiser.Serialise("Use Static DOF", mbUseStaticDOF);
    lrSerialiser.Serialise("Initialise to looking at target?", mbInitialiseToLookingAtTarget);
    lrSerialiser.Serialise("Initialise to zoomed in to target?", mbInitialiseToZoomedToTarget);
    lrSerialiser.Serialise("Use zoom?", mbUseZoom);
}

// Explicit instantiations -- one per serialiser this block is saved/loaded/menu-mirrored through.
template void Looker::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void Looker::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void Looker::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
