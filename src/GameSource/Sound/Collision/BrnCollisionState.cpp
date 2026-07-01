#include "GameSource/Sound/Collision/BrnCollisionState.h"

// =============================================================================
// BrnSound::Logic::Collision::CollisionState -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnCollisionState.h for the
// inheritance rationale. Same shape as the sibling BrnSound::Logic::GlobalState
// (BrnGlobalState.cpp @ 0x826D2250).
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Collision
{

// ---------------------------------------------------------------------------
// ~CollisionState  @ 0x826D3380  (scalar deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install CollisionState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects   ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//
// The single observable source-level side effect is the DestroyEffects() call on
// the State base (inherited, called BY NAME). The two vtable installs and the
// conditional allocator-routed free are the compiler-synthesised parts of MSVC's
// deleting-destructor thunk, re-emitted from this virtual destructor + the class's
// operator delete; off_82FFB954 (the sound allocator) is not homed in this group,
// so the host toolchain's `delete` stands in for the custom-allocator dispatch.
// Byte-for-byte identical thunk to GlobalState's dtor @ 0x826D2250 (same
// off_820AE1F4 / off_820AA820 vtable literals, same DestroyEffects call, same
// off_82FFB954 vtable-slot-0x14 allocator-free tail).
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body is
// a separate un-homed sound-logic recon slice). It is called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
CollisionState::~CollisionState()
{
    DestroyEffects();
}

} // namespace Collision
} // namespace Logic
} // namespace BrnSound
