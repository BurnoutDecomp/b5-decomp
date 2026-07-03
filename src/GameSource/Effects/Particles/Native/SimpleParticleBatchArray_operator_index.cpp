// ============================================================================
// GameSource/Effects/Particles/Native/SimpleParticleBatchArray_operator_index.cpp
//
// Array<BrnParticle::Native::SimpleParticleBatch, 13>::operator[] (non-const) @ 0x8227C9D0
//   (BrnParticle::Native::BrnSimpleParticleRenderer::Dispatch)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::operator[] body is
// already committed inline in CgsArray.h; this TU is the thin explicit per-member
// instantiation only (do NOT re-define the generic). The X360 body matches the generic
// store-for-store:
//
//   operator[]  asserts the array was Construct/Clear'd (count word @ +0xD0 != the -1
//     sentinel, "Array used before Construct/Clear was called", CgsArray.h:538), then asserts
//     index < count ("Array index out of bounds. Index: <i>, length: <n>", CgsArray.h:539),
//     then returns 16*index + base (`slwi r,index,4` == &maElements[index]).
//
// count word @ +0xD0 == 208 == 13 * sizeof(SimpleParticleBatch) confirms sizeof==16; the
// accessor's 16*index arithmetic confirms it again. Capacity 13 + element type DWARF-attested:
//   SimpleParticleBatchArray : public Array<BrnParticle::Native::SimpleParticleBatch,13u>.
//
// The element type (SimpleParticleBatch : EffectsVertexBufferBatch) is the single canonical
// definition in BrnSimpleParticleBatch.h. Per-member (not `template class`) because the
// element defines no operator== (see CgsArraySimpleParticleBatch13.cpp).
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::operator[] (inline generic)
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleBatch.h"

template BrnParticle::Native::SimpleParticleBatch&
Array<BrnParticle::Native::SimpleParticleBatch, 13>::operator[](u32);
