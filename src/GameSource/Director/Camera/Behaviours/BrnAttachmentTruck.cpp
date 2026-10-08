// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.cpp
//
// AttachmentTruck::Parameters::Serialise<TSerialiser> -- the attachment-truck tunings walk (two
// f32 tunables, "Convergence Time Secs" first), one generic body instantiated over the three
// camera serialisers. The block has no version tag.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnAttachmentTruck.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

template<class TSerialiser>
void AttachmentTruck::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Convergence Time Secs", mfConvergenceTimeSecs);
    lrSerialiser.Serialise("Initial offset dist.", mfInitialOffsetDist);
}

template void AttachmentTruck::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void AttachmentTruck::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void AttachmentTruck::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
