#include "GameSource/Sound/Vehicles/BrnAIVehicleState.h"

// =============================================================================
// BrnSound::Vehicles::AIVehicleState -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnAIVehicleState.h for the
// inheritance rationale. Same shape as the sibling BrnSound::Logic::Collision::
// CollisionState (BrnCollisionState.cpp @ 0x826D3380) and GlobalState @ 0x826D2250.
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// ---------------------------------------------------------------------------
// ~AIVehicleState  @ 0x826CA8F8  (the X360 `scalar deleting destructor')
//
// The single observable source-level side effect is the DestroyEffects() call on
// the State base (inherited via VehicleState -> BrnState -> CgsSound::Logic::State,
// called BY NAME). The two vtable installs and the conditional allocator-routed
// free are the compiler-synthesised parts of MSVC's deleting-destructor thunk,
// re-emitted from this virtual destructor + the class's operator delete;
// off_82FFB954 (the sound allocator) is not homed in this group, so the host
// toolchain's `delete` stands in for the custom-allocator dispatch. Byte-for-byte
// identical thunk to CollisionState's dtor @ 0x826D3380.
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body is
// a separate un-homed sound-logic recon slice). It is called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
AIVehicleState::~AIVehicleState()
{
    DestroyEffects();
}

} // namespace Vehicles
} // namespace BrnSound
