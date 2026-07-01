#include "GameSource/Sound/Vehicles/Engines/BrnWhineEffect.h"

// =============================================================================
// BrnSound::Vehicles::Engines::WhineEffect -- out-of-line deleting-destructor body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This TU's recon'd function set is the
// `scalar deleting destructor' @ 0x826E49A8.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ~WhineEffect  @ 0x826E49A8  (anchor for the X360 `scalar deleting destructor').
// The scalar dtor runs the inherited BrnEffectObject dual-base teardown + the embedded
// mWhineVoice VoiceWrapper destruction; those are compiler-synthesised, so this leaf
// body is empty.
// FLAG: the (a2 & 1) tail frees the object via the global sound allocator
// (off_82FFB954); that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain.
WhineEffect::~WhineEffect()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
