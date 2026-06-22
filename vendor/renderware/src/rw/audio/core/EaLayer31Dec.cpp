// =====================================================================================
// rw::audio::core::EaLayer31Dec -- EALayer3 v1 decoder plug-in variant.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   EaLayer31Dec::CreateInstanceEvent @0x82B95EA0
// The PowerPC asm is authoritative; no prior source and no DWARF exist.
// The plug-in "create instance" event forwards to the shared base allocator with the
// v1 variant flag (0). EaLayer3DecBase::CreateInstance is defined in its own TU (it is
// a non-leaf initializer over a still-un-homed layout). See EaLayer3Dec.h.
// =====================================================================================

#include "rw/audio/core/EaLayer3Dec.h"

namespace rw
{
namespace audio
{
namespace core
{

// ---------------------------------------------------------------------------
// EaLayer31Dec::CreateInstanceEvent @ 0x82B95EA0
// ---------------------------------------------------------------------------
s32 EaLayer31Dec::CreateInstanceEvent(s32 self)
{
    return EaLayer3DecBase::CreateInstance(self, 0);
}

} // namespace core
} // namespace audio
} // namespace rw
