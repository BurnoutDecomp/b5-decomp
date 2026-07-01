#include "GameSource/Sound/Vehicles/Environment/BrnAmbienceControl.h"

// =============================================================================
// BrnSound::Vehicles::Environment::AmbienceControl -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. Recon'd function set:
//   CreateObject(u32)              @ 0x826D0B70  (the RTTI factory hook)
//   `vector deleting destructor'   @ 0x826B9558  (-> ~AmbienceControl anchor)
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Environment
{

// ---------------------------------------------------------------------------
// AmbienceControl::CreateObject(u32)  @ 0x826D0B70   (the factory hook)
// Allocates a 144-byte (0x90) block via CgsSound::MemBase::operator new(size, tag,
// flavour) tagged "AmbienceControl" (the u32 arg only selects the new flavour 0/1) and
// inline-constructs an AmbienceControl, returning object+4 (the EffectControl base view).
// FLAG (allocator gate): CgsSound::MemBase::operator new is not homed here, so this uses
// the host `new`; observable result matches. Mirrors CollisionControl::CreateObject.
// ---------------------------------------------------------------------------
CgsSound::Logic::EffectControl* AmbienceControl::CreateObject( u32 /*luType*/ )
{
    return new AmbienceControl();
}

// ---------------------------------------------------------------------------
// ~AmbienceControl  @ 0x826B9558  (anchor for the X360 `vector deleting destructor').
// Every stored member (meDetachState/meAttachState/mbResourcesReady) is owned by the
// committed BrnEffectControl base, so the observable teardown is the inherited
// ~BrnEffectControl chain; this leaf body adds nothing. The (a2 & 1) allocator-free tail
// is left to the host toolchain (off_82FFB954 not homed here).
// ---------------------------------------------------------------------------
AmbienceControl::~AmbienceControl()
{
}

} // namespace Environment
} // namespace Vehicles
} // namespace BrnSound
