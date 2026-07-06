// ============================================================================
// GameSource/Effects/Particles/Native/StackTrailEmitter96_Push.cpp
//
// CgsContainers::Stack<BrnParticle::Native::TrailEmitter*, 96>::Push(TrailEmitter* const&)
//   @ 0x82286280  (BrnParticle::Native::TrailSystem::Prepare / ::EndOfFrame)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Stack<Type,N>::Push() body is
// already committed inline in CgsStack.h; this TU is the thin explicit per-member
// instantiation only (do NOT re-define the generic). The X360 body matches the generic:
//
//   Push()  asserts the stack was Construct/Clear'd (miLength @ +0x180 != the 0x7FFFFFFF
//     sentinel, "Stack used before Construct/Clear was called"). The X360 emits this
//     constructed-check TWICE (CgsStack.h:98 direct, then :169 from the inlined IsFull),
//     then asserts !IsFull() (miLength != 96 == N, "!IsFull()", CgsStack.h:99). The committed
//     generic collapses the doubled constructed-check into one CGS_ASSERT -- semantically
//     identical. It then stores the element at the top slot (maData[miLength] = *lpEntry;
//     the asm computes `slwi miLength,2; stwx` == &maData[miLength] for the 4-byte pointer
//     element) and grows the count (++miLength).
//
// Element type TrailEmitter* (4-byte pointer on X360), so maData[96] occupies +0x000..+0x17F
// and miLength lands at +0x180 == 96*4, exactly the `lwz/stw 0x180(this)` the asm reads.
// Capacity 96 + element type DWARF-attested (BrnTrailSystem.h:164):
//   Stack<BrnParticle::Native::TrailEmitter*,96> mFreeEmitters.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsStack.h"  // Stack<T,N>::Push() (inline generic)

namespace BrnParticle { namespace Native {
    class TrailEmitter;   // BrnTrailSystem.h -- incomplete; only the pointer element is needed
} } // namespace BrnParticle::Native

template void
CgsContainers::Stack<BrnParticle::Native::TrailEmitter*, 96>::Push(
    BrnParticle::Native::TrailEmitter* const& lrEntry);
