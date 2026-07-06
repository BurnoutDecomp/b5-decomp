// ============================================================================
// GameSource/Effects/Particles/Native/StackTrailEmitter96_Pop.cpp
//
// CgsContainers::Stack<BrnParticle::Native::TrailEmitter*, 96>::Pop() @ 0x82286340
//   (BrnParticle::Native::TrailSystem::AttachTrailEmitter)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Stack<Type,N>::Pop() body is
// already committed inline in CgsStack.h; this TU is the thin explicit per-member
// instantiation only (do NOT re-define the generic). The X360 body matches the generic:
//
//   Pop()  asserts the stack was Construct/Clear'd (miLength @ +0x180 != the 0x7FFFFFFF
//     sentinel, "Stack used before Construct/Clear was called"). The X360 emits this
//     constructed-check TWICE (CgsStack.h:121 direct, then :177 from the inlined IsEmpty),
//     then asserts !IsEmpty() (miLength != 0, "!IsEmpty()", CgsStack.h:122). The committed
//     generic collapses the doubled constructed-check into one CGS_ASSERT -- semantically
//     identical. It then drops the top element (--miLength; the slot is left as-is).
//
// Element type TrailEmitter* (4-byte pointer on X360), so maData[96] occupies +0x000..+0x17F
// and miLength lands at +0x180 == 96*4, exactly the `lwz/stw 0x180(this)` the asm reads
// (0x82286358 / 0x822863E0). Capacity 96 + element type DWARF-attested (BrnTrailSystem.h:164):
//   Stack<BrnParticle::Native::TrailEmitter*,96> mFreeEmitters.
// (This is the TrailSystem free-list Stack -- the sibling of the committed Peek/Push members.)
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsStack.h"  // Stack<T,N>::Pop() (inline generic)

namespace BrnParticle { namespace Native {
    class TrailEmitter;   // BrnTrailSystem.h -- incomplete; only the pointer element is needed
} } // namespace BrnParticle::Native

template void
CgsContainers::Stack<BrnParticle::Native::TrailEmitter*, 96>::Pop();
