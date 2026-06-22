// =====================================================================================
// rw::audio::core::EaLayer32PcmDec -- EALayer3 v2 / PCM decoder plug-in variant.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   EaLayer32PcmDec::CreateInstanceEvent @0x82B95EA8
// The PowerPC asm is authoritative; no prior source and no DWARF exist.
// The plug-in "create instance" event forwards to the shared base allocator with the
// v2/PCM variant flag (1). EaLayer3DecBase::CreateInstance is defined in its own TU (it
// is a non-leaf initializer over a still-un-homed layout). See EaLayer3Dec.h.
// =====================================================================================

#include "rw/audio/core/EaLayer3Dec.h"

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// EaLayer32PcmDec::CreateInstanceEvent @ 0x82B95EA8
// ---------------------------------------------------------------------------
s32 EaLayer32PcmDec::CreateInstanceEvent(s32 self)
{
    return EaLayer3DecBase::CreateInstance(self, 1);
}

} // namespace core
} // namespace audio
} // namespace rw
