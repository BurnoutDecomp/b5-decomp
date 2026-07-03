// ============================================================================
// GameSource/Effects/Particles/Native/StackTrailEmitter96_Peek.cpp
//
// CgsContainers::Stack<BrnParticle::Native::TrailEmitter*, 96>::Peek() const @ 0x822863F0
//   (BrnParticle::Native::TrailSystem::AttachTrailEmitter)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Stack<Type,N>::Peek() body is
// already committed inline in CgsStack.h; this TU is the thin explicit per-member
// instantiation only (do NOT re-define the generic). The X360 body matches the generic:
//
//   Peek()  asserts the stack was Construct/Clear'd (miLength @ +0x180 != the 0x7FFFFFFF
//     sentinel, "Stack used before Construct/Clear was called"). The X360 emits this
//     constructed-check TWICE (CgsStack.h:149 direct, then :177 from the inlined IsEmpty),
//     then asserts !IsEmpty() (miLength != 0, "!IsEmpty()", CgsStack.h:150). The committed
//     generic collapses the doubled constructed-check into one CGS_ASSERT -- semantically
//     identical. It then returns `4*miLength + this - 4` == &maData[miLength - 1].
//
// Element type TrailEmitter* (4-byte pointer on X360), so maData[96] occupies +0x000..+0x17F
// and miLength lands at +0x180 == 96*4, exactly the `lwz/stw 0x180(this)` the asm reads.
// Capacity 96 + element type DWARF-attested (BrnTrailSystem.h:164):
//   Stack<BrnParticle::Native::TrailEmitter*,96> mFreeEmitters.
// (This is the TrailSystem free-list Stack, distinct from the sibling EmitterArray in
//  BrnTrailSystem.h which uses a plain size field with no 0x7FFFFFFF sentinel.)
//
// TrailEmitter is used only as a pointer element here, so a forward declaration suffices
// (matching the committed BrnTrailSystem.h forward decl; its payload lives in its own home).
// Instantiate ONLY Peek (NOT `template struct`): the pointer element is fine for Contains but
// the per-member form matches the single X360-emitted member for this instance.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsStack.h"  // Stack<T,N>::Peek() (inline generic)

namespace BrnParticle { namespace Native {
    class TrailEmitter;   // BrnTrailSystem.h -- incomplete; only the pointer element is needed
} } // namespace BrnParticle::Native

template BrnParticle::Native::TrailEmitter* const&
CgsContainers::Stack<BrnParticle::Native::TrailEmitter*, 96>::Peek() const;
