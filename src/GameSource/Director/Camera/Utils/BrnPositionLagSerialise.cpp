// ============================================================================
// GameSource/Director/Camera/Utils/BrnPositionLagSerialise.cpp
//
// PositionLag::Parameters::Serialise<TSerialiser> -- the position-lag tunings walk, one generic
// body instantiated over the three camera serialisers (text-file read / write, debug menu).
//
// The walk stamps the code version into the tag, hands the tag to the serialiser (the read can
// change it; the debug-menu leaf ignores it), then walks the four fields only when the tag still
// matches the code version. A mismatch walks nothing and does not assert.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnPositionLag.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

namespace
{
    // The current code version of the PositionLag::Parameters block.
    const u32 KU_POSITION_LAG_PARAMETERS_VERSION = 1u;
} // namespace

template<class TSerialiser>
void PositionLag::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    muVersion.muVersion = KU_POSITION_LAG_PARAMETERS_VERSION;
    lrSerialiser.Serialise("Version Number (dont change)", muVersion);

    if (muVersion.muVersion == KU_POSITION_LAG_PARAMETERS_VERSION)
    {
        lrSerialiser.Serialise("X Response", mfXResponse);
        lrSerialiser.Serialise("Y Response", mfYResponse);
        lrSerialiser.Serialise("Z Response", mfZResponse);
        lrSerialiser.Serialise("Smoothing", mfSmoothing);
    }
}

template void PositionLag::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void PositionLag::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void PositionLag::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
