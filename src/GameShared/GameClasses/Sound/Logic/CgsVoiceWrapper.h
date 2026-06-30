#ifndef CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H
#define CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H

#include "types.hpp"

// =============================================================================
// CgsSound::Logic::VoiceWrapper -- minimal home (additive).
//   GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.h  (DWARF home inferred) +
//   GameShared/GameClasses/Sound/Logic/CgsVoiceWrapper.cpp
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. VoiceWrapper is the small per-effect
// voice-wrapper sub-object that sound-logic effect leaves embed (e.g.
// BrnSound::Logic::Explosion::ExplosionEffect embeds one at its +0x38). It is the
// callee of the inlined-effect-ctor tail `bl CgsSound::Logic::VoiceWrapper::
// VoiceWrapper` seen at ExplosionEffect::ExplosionEffect @ 0x826D5480.
//
// FLAG (MINIMAL un-homed home): only the constructor SHAPE is materialised here so
// embedding leaves can default-construct the member BY NAME. The X360 has no
// standalone dump of VoiceWrapper::VoiceWrapper in this TU's postmortem (the
// embedding ctor just `bl`s it), so its member layout, its non-trivial body and the
// rest of its surface (Connect/Play/Stop/gain forwarding, if any) are NOT
// reconstructed and are DEFERRED to the VoiceWrapper recon slice. Nothing about its
// internals is fabricated: the wrapper carries no reconstructed members here.
// =============================================================================

namespace CgsSound
{
namespace Logic
{

// Minimal declaration: a default-constructible empty wrapper so embedding effect
// leaves (ExplosionEffect, ...) can hold one BY NAME. The real member set + bodies
// are DEFERRED.
struct VoiceWrapper
{
    VoiceWrapper() {}
};

} // namespace Logic
} // namespace CgsSound

#endif // CGS_SOUND_LOGIC_CGSVOICEWRAPPER_H
