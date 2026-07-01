#include "GameSource/Sound/Traffic/BrnTrafficState.h"

// =============================================================================
// BrnSound::Logic::Traffic::TrafficState -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnTrafficState.h for the
// inheritance rationale.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Traffic
{

// ---------------------------------------------------------------------------
// ~TrafficState  @ 0x826CB1D0  (vector deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install TrafficState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects   ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//
// Identical shape (same off_820AE1F4 own-vtable install / off_820AA820 MemBase
// base re-install pair) to the sibling GlobalState::~GlobalState @ 0x826D2250
// (BrnGlobalState.cpp) -- both are BrnState-derived leaves whose only observable
// source-level side effect is the DestroyEffects() call on the State base (reused
// BY NAME, declaration-only in BrnState.h). The two vtable installs and the
// conditional allocator-routed free are the compiler-synthesised parts of MSVC's
// vector-deleting-destructor thunk, re-emitted here from this virtual destructor +
// the class's operator delete; off_82FFB954 (the sound allocator) is not homed in
// this group, so the host toolchain's `delete` stands in for the custom-allocator
// dispatch.
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body is
// a separate un-homed sound-logic recon slice). It is called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
TrafficState::~TrafficState()
{
    DestroyEffects();
}

} // namespace Traffic
} // namespace Logic
} // namespace BrnSound
