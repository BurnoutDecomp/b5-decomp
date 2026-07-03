// ============================================================================
// GameShared/GameClasses/Containers/CgsArraySimpleParticleBatch13.cpp
//
// Per-instantiation .cpp for Array<BrnParticle::Native::SimpleParticleBatch,13>. The
// generic Array<T,N> body (Append / GetLength + siblings) is fully inline in CgsArray.h,
// so this TU is just the explicit per-member instantiation (the X360 emits one out-of-
// line copy per using-TU):
//   Array<SimpleParticleBatch,13>::Append    @ 0x8291DFA8
//                    (BrnParticle::Native::SimpleParticleVertexBufferBuilder::BuildDispatchData)
//   Array<SimpleParticleBatch,13>::GetLength @ 0x8227C190
//                    (BrnParticle::Native::ParticleRenderJob::RenderSimpleParticles)
//
// Layout: maElements[13] (13 * 16 = 208B) + miCount @ +0xD0(==208), matching the X360
// lwz/stw at 0xD0(r30) count word and the slwi-by-4 (stride 16) element store at
// maElements[miCount]. Element stride 16 == sizeof(SimpleParticleBatch): its base
// EffectsVertexBufferBatch is 8 bytes (muStartVertex + muVertexCount) and the derived
// SimpleParticleBatch adds two enum words (meParticleType + meBlendMode). DWARF authority:
// BrnSimpleParticleRenderer.h:125 + EffectsVertexBuffer.h:42.
//
// Instantiate ONLY the used members (NOT `template class`): SimpleParticleBatch defines no
// operator==, so forcing the whole class would instantiate Contains/FindFirstInstanceOf/
// CountInstancesOf/EraseInstancesOf and fail to compile (same per-member technique as
// CgsArrayVpuVector3_8.cpp). The operator[] instantiation is homed in its own TU
// (SimpleParticleBatchArray_operator_index.cpp @ 0x8227C9D0). The dynamic out-of-space
// message collapses to the shared static CGS_ASSERT string in the generic Append body.
//
// Spelled unqualified (Array<T,N>) to match the committed container convention (CgsArray.h /
// CgsArrayHullToActivateInfo7.cpp); the DWARF spells the type CgsContainers::Array<...,13u>.
// ============================================================================
#include "GameShared/GameClasses/Containers/CgsArray.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleBatch.h"

template void Array<BrnParticle::Native::SimpleParticleBatch, 13>::Append(
    const BrnParticle::Native::SimpleParticleBatch&);
template u32 Array<BrnParticle::Native::SimpleParticleBatch, 13>::GetLength() const;
