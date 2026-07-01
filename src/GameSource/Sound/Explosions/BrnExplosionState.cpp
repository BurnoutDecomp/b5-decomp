#include "GameSource/Sound/Explosions/BrnExplosionState.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

// =============================================================================
// BrnSound::Logic::Explosion::ExplosionState::Attach  @ 0x826891B8
//
// The X360 body is a single argument null-check:
//   cmplwi cr6, r4, 0          ; if (lpvAttachment == 0)
//   bne    cr6, <return>
//   bl     CgsDev::Assert::BeginAssert
//   ... FireAssert("lpvAttachment", "...BrnExplosionState.cpp", 69)
//   bl     CgsDev::Assert::EndAssert
//   <return>                   ; void
//
// No member is read or written; the function only validates its attachment
// pointer (the per-frame attachment the state binds to). CGS_ASSERT folds the
// Begin/Fire/End sequence; the baked d:\p4 file/line is replaced by __FILE__/
// __LINE__.
// =============================================================================

namespace BrnSound
{
namespace Logic
{
namespace Explosion
{

void ExplosionState::Attach(void* lpvAttachment)
{
    CGS_ASSERT(lpvAttachment != 0, "lpvAttachment");
}

// ---------------------------------------------------------------------------
// ~ExplosionState  @ 0x826D5740  (vector deleting destructor)
//
//   stw  off_820AE1F4, 0(this)                     ; install ExplosionState's own vtable
//   bl   CgsSound::Logic::State::DestroyEffects     ; (this in r3) tear down attached effects
//   stw  off_820AA820, 0(this)                      ; re-install the MemBase base vtable
//   if (a2 & 1)                                     ; deleting flavour
//       <sound allocator>.Free(this)                ; via off_82FFB954, vtable slot +0x14
//
// Store-for-store identical to the committed sibling GlobalState::~GlobalState
// (@ 0x826D2250, BrnGlobalState.cpp) modulo the leaf vtable constant: the same
// off_820AE1F4 (leaf vtable) / DestroyEffects / off_820AA820 (MemBase base
// vtable) sequence. The single observable source-level side effect is the
// DestroyEffects() call on the State base (declared on BrnState, reused BY NAME).
// The two vtable installs and the conditional allocator-routed free are the
// compiler-synthesised parts of MSVC's vector-deleting-destructor thunk;
// off_82FFB954 (the sound allocator) is not homed in this group, so the host
// toolchain's `delete` stands in for the custom-allocator dispatch.
//
// FLAG: State::DestroyEffects() is declaration-only in BrnState.h (its own body
// is a separate un-homed sound-logic recon slice). Called BY NAME here to match
// the X360 `bl` exactly; no body is fabricated for it.
// ---------------------------------------------------------------------------
ExplosionState::~ExplosionState()
{
    DestroyEffects();
}

} // namespace Explosion
} // namespace Logic
} // namespace BrnSound
