// ============================================================================
// GameShared/GameClasses/Containers/CgsArraySparkBatch4.cpp
//
// Per-instantiation .cpp for Array<BrnParticle::Native::SparkBatch, 4>. The generic
// Array<T,N>::Append body is fully inline in CgsArray.h; the X360 emits one out-of-line
// copy per using-TU. This TU is the explicit per-member instantiation only (do NOT
// re-define the generic):
//   Array<BrnParticle::Native::SparkBatch,4>::Append @ 0x8291FAD0
//     (caller: BrnParticle::Native::SparkVertexBufferBuilder::BuildDispatchData)
//
// Layout/asm parity (DWARF: BrnSparkRenderer.h:140 SparkBatch : EffectsVertexBufferBatch
// {u32 muStartVertex; u32 muVertexCount;} + ESparkArrayID meArrayId => sizeof == 12).
// The typedef SparkBatchArray = Array<SparkBatch,4>. The count word therefore sits at byte
// offset 0x30 (== 4 * 12), matching the asm's lwz/stw 0x30(r30). Append @0x8291FAD0:
// asserts Construct/Clear'd (count @+0x30 != -1 sentinel, CgsArray.h:169), asserts room
// (unsigned count >= 4 streams the dynamic "Array container out of space, Length: <n>,
// Capacity: 4" message, CgsArray.h:170), then copies the three element dwords
// (slwi r10,r11,1; add; slwi r11,r11,2 => stride 12) from a2 into maElements[count] and
// ++count -- exactly the committed generic Append for a 12-byte trivially-copyable element.
// The dynamic out-of-space message collapses to the shared static CGS_ASSERT string in the
// generic body.
//
// Instantiate ONLY Append (NOT `template class`): SparkBatch defines no operator==, so
// forcing the whole class would instantiate Contains/FindFirstInstanceOf/CountInstancesOf/
// EraseInstancesOf and fail to compile (same per-member technique as CgsArrayVpuVector3_8.cpp).
//
// Spelled unqualified (Array<T,N>) to match the committed container convention (CgsArray.h
// header note; the DWARF spells it CgsContainers::Array<SparkBatch,4u> -- mirror
// CgsArrayVpuVector3_8.cpp). The element type (SparkBatch + EffectsVertexBufferBatch base +
// ESparkArrayID) is defined in BrnSparkRenderer.h.
// ============================================================================
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"

template void Array<BrnParticle::Native::SparkBatch, 4>::Append(
    const BrnParticle::Native::SparkBatch&);
