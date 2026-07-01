#include "GameSource/Sound/Module/LogicModule/BrnEmitterState.h"

// =============================================================================
// BrnSound::Logic::World::EmitterState -- out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnEmitterState.h for the
// inheritance rationale. Mirrors the committed sibling
// BrnSound::Logic::GlobalState::~GlobalState (@ 0x826D2250,
// GameSource/Sound/Global/BrnGlobalState.cpp) 1:1.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace World
{

// ---------------------------------------------------------------------------
// ~EmitterState  @ 0x826D1F70  (scalar deleting destructor)
//
//   stw  off_820AE1F4, 0(this)               ; install EmitterState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects   ; (this in r3) tear down effects
//   stw  off_820AA820, 0(this)               ; re-install the MemBase base vtable
//   if (flags & 1)                           ; deleting flavour
//       <sound allocator>.Free(this)         ; via off_82FFB954, vtable slot +0x14
//
// The single observable source-level side effect is the DestroyEffects() call on
// the State base (reused BY NAME, un-homed body). The two vtable installs and the
// conditional allocator-routed free are the compiler-synthesised parts of MSVC's
// deleting-destructor thunk, re-emitted here from this virtual destructor + the
// class's operator delete; off_82FFB954 (the sound allocator) is not homed in
// this group, so the host toolchain's `delete` stands in for the custom-allocator
// dispatch.
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body is
// a separate un-homed sound-logic recon slice). It is called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
EmitterState::~EmitterState()
{
    DestroyEffects();
}

} // namespace World
} // namespace Logic
} // namespace BrnSound
