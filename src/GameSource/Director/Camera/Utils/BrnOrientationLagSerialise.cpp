// ============================================================================
// GameSource/Director/Camera/Utils/BrnOrientationLagSerialise.cpp
//
// OrientationLag::Parameters::Serialise<TSerialiser> -- the orientation-lag tunings walk, one
// generic body instantiated over the three camera serialisers (text-file read / write, debug
// menu).
//
// The walk stamps the code version into the tag, hands the tag to the serialiser, then walks
// the slerp-spring pair only when the tag still matches the code version (a mismatch walks
// nothing and does not assert). The pitch/yaw/roll springs (+0x04..+0x0C) are not walked.
// ============================================================================

#include "GameSource/Director/Camera/Utils/BrnOrientationLag.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{
namespace Utils
{

template void OrientationLag::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void OrientationLag::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void OrientationLag::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Utils
} // namespace Camera
} // namespace BrnDirector
