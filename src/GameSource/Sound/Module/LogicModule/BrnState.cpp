#include "GameSource/Sound/Module/LogicModule/BrnState.h"

// =============================================================================
// BrnSound::Logic::BrnState — out-of-line bodies.
// Reconstructed from BURNOUT_X360_ARTIST.XEX. See BrnState.h for the inheritance
// rationale.
// =============================================================================

namespace BrnSound
{
namespace Logic
{

// ---------------------------------------------------------------------------
// `scalar deleting destructor'  @ 0x826C84D8  (the ONLY function this TU's
// dossier attests for BrnState)
//
//   *a1 = off_820AE1F4              ; re-install BrnState's own vtable
//   CgsSound::Logic::State::DestroyEffects()
//   *a1 = &off_820AA820             ; re-install State's vtable before free
//   if (a2 & 1) operator delete(a1) ; the "deleting" half of the thunk
//
// The two vtable-pointer stores + the conditional operator-delete call are the
// standard scalar-deleting-destructor thunk shape (compiler-synthesized here
// from `virtual ~BrnState()`); the one real, named side effect is the call
// into the base's DestroyEffects(), reproduced below by name.
// ---------------------------------------------------------------------------
BrnState::~BrnState()
{
    DestroyEffects();
}

} // namespace Logic
} // namespace BrnSound
