// ============================================================================
// GameSource/Effects/Particles/Native/SparkBatchArray_operator_index.cpp
//
// Array<BrnParticle::Native::SparkBatch, 4>::operator[] (non-const) @ 0x8227CAD8
//   (BrnParticle::Native::SparkRenderer::Dispatch)
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The generic Array<T,N>::operator[] body is
// already committed inline in CgsArray.h; this TU is the thin explicit per-member
// instantiation only (do NOT re-define the generic). The X360 body matches the generic
// store-for-store:
//
//   operator[]  asserts the array was Construct/Clear'd (count word @ +0x30 != the -1
//     sentinel, "Array used before Construct/Clear was called", CgsArray.h:538), then asserts
//     index < count ("Array index out of bounds. Index: <i>, length: <n>", CgsArray.h:539),
//     then returns 12*index + base. The X360 spells 12*index as (index + 2*index)<<2
//     (`slwi r11,index,1; add r11,index,r11; slwi r11,r11,2`) == &maElements[index].
//
// count word @ +0x30 == 48 == 4 * sizeof(SparkBatch) confirms sizeof==12; the accessor's
// 12*index arithmetic confirms it again. Capacity 4 + element type DWARF-attested:
//   typedef CgsContainers::Array<BrnParticle::Native::SparkBatch,4u> SparkBatchArray.
// (Dossier ledger key "SparkBatc" is the IDA-truncated "SparkBatch".)
//
// SparkBatch (: EffectsVertexBufferBatch + ESparkArrayID meArrayId) is the single canonical
// definition in BrnSparkRenderer.h. Per-member (not `template class`) because the element
// defines no operator== (see CgsArraySparkBatch4.cpp).
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"  // Array<T,N>::operator[] (inline generic)
#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"

template BrnParticle::Native::SparkBatch&
Array<BrnParticle::Native::SparkBatch, 4>::operator[](u32);
