// ============================================================================
// GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachmentParameters.cpp
//
// BehaviourLooseAttachment::Parameters::Serialise<TSerialiser> -- the loose-attachment tunings
// walk (the nested "Impact" block, then the placement fields; no version tag), one generic body
// instantiated over the three camera serialisers (text-file read / write, debug menu). The
// position-lag and shake sub-blocks at +0x08 / +0x1C are not walked; the +0x60 bool is walked
// before the +0x5C float.
// ============================================================================

#include "GameSource/Director/Camera/Behaviours/BrnBehaviourLooseAttachment.h"

#include "GameSource/Director/Camera/Behaviours/Serialisation.h"   // the three serialisers

namespace BrnDirector
{
namespace Camera
{

template<class TSerialiser>
void BehaviourLooseAttachment::Parameters::Serialise(TSerialiser& lrSerialiser)
{
    lrSerialiser.Serialise("Impact", mImpact);
    lrSerialiser.Serialise("Pitch", mfPitch);
    lrSerialiser.Serialise("Height", mfHeight);
    lrSerialiser.Serialise("Distance", mfDistance);
    lrSerialiser.Serialise("FOV", mfField54);
    lrSerialiser.Serialise("Dutch", mfDutch);
    lrSerialiser.Serialise("Look from target", mbLookFromTarget);
    lrSerialiser.Serialise("Detach Lerp Amount", mfDetachLerpAmount);
}

template void BehaviourLooseAttachment::Parameters::Serialise<TextFileReadSerialiser>(TextFileReadSerialiser&);
template void BehaviourLooseAttachment::Parameters::Serialise<TextFileWriteSerialiser>(TextFileWriteSerialiser&);
template void BehaviourLooseAttachment::Parameters::Serialise<DebugMenuSerialiser>(DebugMenuSerialiser&);

} // namespace Camera
} // namespace BrnDirector
