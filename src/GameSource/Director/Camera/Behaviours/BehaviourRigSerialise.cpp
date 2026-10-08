// ============================================================================
// GameSource/Director/Camera/Behaviours/BehaviourRigSerialise.cpp
//
// BehaviourRig::Parameters::Serialise<TSerialiser> -- the rig-cam tunings walk, one generic body
// instantiated over the three camera serialisers (text-file read / write, debug menu).
//
// The walk stamps the code version into the tag, hands the tag to the serialiser, then walks the
// block only when the tag still matches the code version (a mismatch walks nothing and does not
// assert). The shake / looker sub-blocks, the accel-spring fields and the accel-spring / shake
// flags are not walked.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BehaviourRig.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The current code version of the BehaviourRig::Parameters block.
    const u32 KU_BEHAVIOUR_RIG_PARAMETERS_VERSION = 1u;
} // namespace

template<class TSerialiser>
void BehaviourRig::Parameters::Serialise(TSerialiser& lrSerialiser)
{
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

template void BehaviourRig::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourRig::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourRig::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
