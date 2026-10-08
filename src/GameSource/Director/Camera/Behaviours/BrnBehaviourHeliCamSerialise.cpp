// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCamSerialise.cpp
//
// BehaviourHeliCam::Parameters::Serialise<TSerialiser> -- the heli-cam tunings walk, one generic
// body instantiated over the three camera serialisers (text-file read / write, debug menu).
//
// The walk stamps the code version (2) into the tag and hands the tag to the serialiser (only the
// read can change it). Version 1 blocks carry the shake sub-block only, version 2 adds the looker
// sub-block; both then walk the scalar fields, the FOV last. Any other version asserts and walks
// nothing more.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourHeliCam.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT (version mismatch)

namespace BrnDirector
{
namespace Camera
{

namespace
{
    // The block versions the walk understands; the code writes the current one.
    const u32 KU_HELI_CAM_PARAMETERS_VERSION_SHAKE_ONLY = 1u;
    const u32 KU_HELI_CAM_PARAMETERS_VERSION            = 2u;
} // namespace

template<class TSerialiser>
void BehaviourHeliCam::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    muVersion.muVersion = KU_HELI_CAM_PARAMETERS_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);

    switch (muVersion.muVersion)
    {
    case KU_HELI_CAM_PARAMETERS_VERSION_SHAKE_ONLY:
        lrSerialiser.Serialise("Shake Parameters", mShakeParams);
        break;

    case KU_HELI_CAM_PARAMETERS_VERSION:
        lrSerialiser.Serialise("Shake Parameters", mShakeParams);
        lrSerialiser.Serialise("Looker Parameters", mLookerParams);
        break;

    default:
        // The console streams the code version and the data version after this text.
        CGS_ASSERT(false, "BehaviourHeliCam::Parameters : code/data version mismatch, code is version: ");
        return;
    }

    lrSerialiser.Serialise("Height (KM)", mfHeight);
    lrSerialiser.Serialise("Initial Distance X (KM)", mfInitialDistanceX);
    lrSerialiser.Serialise("Initial Distance Z (KM)", mfInitialDistanceZ);
    lrSerialiser.Serialise("Velocity MPS", mfVelocityMPS);
    lrSerialiser.Serialise("FOV", mFOV);
}

template void BehaviourHeliCam::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourHeliCam::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourHeliCam::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
