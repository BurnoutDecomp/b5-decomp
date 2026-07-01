#include "GameSource/Sound/Vehicles/Engines/BrnBoostEffect.h"

// =============================================================================
// BrnSound::Vehicles::Engines::BoostEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. This TU's recon'd function set is
// exactly ONE entry: the `vector deleting destructor' @ 0x826E43A0.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Engines
{

// ---------------------------------------------------------------------------
// ~BoostEffect  @ 0x826E43A0  (anchor for the X360 `vector deleting destructor')
//
// The inner ~BoostEffect is the inherited BrnEffectObject dual-base settle (both vptr
// stores + the attach/detach/resources-ready member clears), the same shape as the
// committed BrnEffectObject dtor @ 0x826AF4C8. In reconstructed C++ the dual-base
// settle and the deleting-destructor thunk are compiler-synthesised from the virtual
// dtor declared in the header, so the hand-written body is empty.
// FLAG: the (a2 & 1) tail frees the object through the global sound MemBase allocator
// (off_82FFB954); that allocator's vtable call is not homed here, so the `delete' half
// is left to the host toolchain.
// ---------------------------------------------------------------------------
BoostEffect::~BoostEffect()
{
}

} // namespace Engines
} // namespace Vehicles
} // namespace BrnSound
