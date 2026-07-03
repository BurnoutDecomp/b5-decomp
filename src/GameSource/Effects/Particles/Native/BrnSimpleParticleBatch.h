#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnSimpleParticleBatch.h
//
// The effects vertex-buffer batch base (EffectsVertexBufferBatch) plus the
// 16-byte simple-particle batch element (SimpleParticleBatch), the element type
// of Array<BrnParticle::Native::SimpleParticleBatch,13>
// (CgsArraySimpleParticleBatch13.cpp / SimpleParticleBatchArray_operator_index.cpp).
//
// DWARF authority (BrnSimpleParticleRenderer.h:125 + EffectsVertexBuffer.h:42):
//
//   struct EffectsVertexBufferBatch { u32 muStartVertex; u32 muVertexCount; };   // 8B
//   struct SimpleParticleBatch : EffectsVertexBufferBatch {
//       ENativeParticleType meParticleType;   // enum word @ +0x08
//       EParticleBlend      meBlendMode;       // enum word @ +0x0C
//   };                                          // sizeof == 16
//
// The 16-byte stride is X360-attested by Array<...,13>::Append (slwi r11,r11,4 element
// store; count word @ +0xD0 == 13 * 16) and by operator[] (0x8227C9D0, slwi index,4).
// Only the SIZE/stride is load-bearing for the Array instantiation; the two enum
// members are modelled as plain u32-sized enum words (their exact enumerators live in
// AttribSys NativeParticleType.h / CB4ParticleArrayStandardParams and are not required
// to size the container).
//
// EffectsVertexBufferBatch's true home is EffectsVertexBuffer.h (not yet committed); the
// minimal 8-byte base is defined here so the batch elements are sizeable. This header is
// the single canonical definition of EffectsVertexBufferBatch/SimpleParticleBatch in the
// tree (BrnSparkRenderer.h includes it to reuse the same base for SparkBatch). Migrate to
// a committed EffectsVertexBuffer.h when that TU lands (GROW additively).
// ============================================================================

#include "types.hpp"

namespace BrnParticle
{
namespace Native
{
    // EffectsVertexBuffer.h:42 (DWARF) -- base of every effects vertex-buffer batch.
    // Minimal 8-byte base sized to the attested SimpleParticleBatch/SparkBatch strides;
    // replace with the shared EffectsVertexBuffer.h definition once that TU is reconstructed.
    struct EffectsVertexBufferBatch
    {
        u32 muStartVertex;   // EffectsVertexBuffer.h:59 -- @ +0x00
        u32 muVertexCount;   // EffectsVertexBuffer.h:60 -- @ +0x04

        u32 GetStartVertex() const { return muStartVertex; }
        u32 GetVertexCount() const { return muVertexCount; }
    };

    // BrnSimpleParticleRenderer.h:125 (DWARF). sizeof == 16 (== Array element stride,
    // X360-attested by Array<SimpleParticleBatch,13>::Append slwi-by-4 and operator[]).
    struct SimpleParticleBatch : public EffectsVertexBufferBatch
    {
        // AttribSys::Enums::NativeParticleType::NativeParticleType (BrnSimpleParticleRenderer.h:130)
        u32 meParticleType;   // enum word @ +0x08
        // CB4ParticleArrayStandardParams::EParticleBlend (BrnSimpleParticleRenderer.h:131)
        u32 meBlendMode;      // enum word @ +0x0C
    };
}
}
