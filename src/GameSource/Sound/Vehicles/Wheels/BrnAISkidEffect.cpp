#include "GameSource/Sound/Vehicles/Wheels/BrnAISkidEffect.h"

// =============================================================================
// BrnSound::Vehicles::Wheels::AISkidEffect -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. DWARF home: BrnAISkidEffect.h/.cpp.
// This slice bodies the single dossier function -- the `vector deleting destructor'
// @ 0x826E5E88. Mirrors the committed sibling CollisionEffect vector deleting
// destructor @ 0x826C90D8 store-for-store.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{
namespace Wheels
{

// ---------------------------------------------------------------------------
// ~AISkidEffect  @ 0x826E5E88  (the X360 `vector deleting destructor')
//
// The leaf teardown adds nothing of its own: the inner ~AISkidEffect is the inherited
// BrnEffectObject dual-base settle (both vptr stores + the attach/detach/resources-
// ready member clears). In reconstructed C++ the dual-base settle AND the deleting-
// destructor thunk are compiler-synthesised from the virtual dtor declared in the
// header, so the hand-written body is empty.
// FLAG: the (a2 & 1) tail frees the object through the global sound MemBase allocator
// (off_82FFB954, vtable slot +0x14 == Free); that allocator's vtable dispatch is not
// homed here, so the `delete' half is left to the host toolchain.
// ---------------------------------------------------------------------------
AISkidEffect::~AISkidEffect()
{
}

} // namespace Wheels
} // namespace Vehicles
} // namespace BrnSound
