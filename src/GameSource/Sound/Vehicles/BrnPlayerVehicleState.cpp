#include "GameSource/Sound/Vehicles/BrnPlayerVehicleState.h"

// =============================================================================
// BrnSound::Vehicles::PlayerVehicleState -- out-of-line deleting-destructor body.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnPlayerVehicleState.h for the
// inheritance rationale. Mirrors the committed sibling scalar deleting destructors
// StreamingState (CgsState.cpp @ 0x826C9B28) and GlobalState (BrnGlobalState.cpp
// @ 0x826D2250).
// =============================================================================

namespace BrnSound
{
namespace Vehicles
{

// ---------------------------------------------------------------------------
// ~PlayerVehicleState  @ 0x826CA250  (the X360 `scalar deleting destructor')
//
// The two vtable installs and the conditional allocator-routed free (off_82FFB954,
// vtable slot +0x14) are MSVC's compiler-synthesised deleting-destructor thunk,
// re-emitted from this virtual destructor + operator delete -- NOT hand-written.
// The single observable source-level side effect is the DestroyEffects() call on
// the State grandparent base, reused BY NAME (its body is un-homed -- a separate
// sound-logic recon slice; declared in BrnState.h, no body fabricated here).
//
// FLAG: the (a2 & 1) tail invokes the global sound allocator (off_82FFB954) to free
// the object; that allocator is not homed here, so operator-delete dispatch is left
// to the host toolchain (the `delete` half of the X360 deleting destructor).
// ---------------------------------------------------------------------------
PlayerVehicleState::~PlayerVehicleState()
{
    DestroyEffects();
}

} // namespace Vehicles
} // namespace BrnSound
