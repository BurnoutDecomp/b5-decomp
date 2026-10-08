// ============================================================================
// GameSource/Director/Camera/Utils/BrnLookerSerialise.cpp
//
// Looker::Parameters::Serialise<TSerialiser> -- the look-at/zoom tunings walk, one generic body
// instantiated over the three camera serialisers (text-file read / write, debug menu).
//
// The walk stamps the code version into the tag, hands the tag to the serialiser (the read can
// change it; the debug-menu leaf ignores it), then walks the fields only when the tag still
// matches the code version; any other version asserts and walks nothing. The subject X/Y
// size / screen-offset fields (+0x10..+0x1C) and meZoomType (+0x60) are not walked.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnLooker.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT (version mismatch)

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

namespace
{
    // The current code version of the Looker::Parameters block.
    const u32 KU_LOOKER_PARAMETERS_VERSION = 1u;
} // namespace

template<class TSerialiser>
void Looker::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    mVersion.muVersion = KU_LOOKER_PARAMETERS_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", mVersion);

    switch (mVersion.muVersion)
    {
    case KU_LOOKER_PARAMETERS_VERSION:
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
        break;

    default:
        // The console streams the code version and the data version after this text.
        CGS_ASSERT(false, "Looker::Parameters : code/data version mismatch, code is version: ");
        break;
    }
}

template void Looker::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void Looker::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void Looker::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
