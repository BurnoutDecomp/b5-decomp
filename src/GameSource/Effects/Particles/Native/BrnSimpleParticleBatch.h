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
#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"  // the canonical EffectsVertexBufferBatch
#include "GameShared/GameClasses/Containers/CgsArray.h"         // Array<T,N> (SimpleParticleBatchArray's base)

namespace BrnParticle
{
namespace Native
{
    // ⭐ ODR FORK RETIRED 2026-09-06. This header used to DEFINE its own eight-byte
    // EffectsVertexBufferBatch, with its own note saying "EffectsVertexBuffer.h (not yet
    // committed)". That home IS committed now, in the GLOBAL namespace, and it is the type
    // EffectsVertexBufferLocked::BeginBatch / EndBatch actually take -- so the two
    // definitions were a silent fork: any batch declared through this header could not be
    // handed to the functions that fill it. SparkVertexBufferBuilder::BuildDispatchData is
    // the first body that had to do exactly that, and it is where the fork surfaced (as
    // "cannot convert argument 2 from BrnParticle::Native::SparkBatch to
    // EffectsVertexBufferBatch &"). The console has ONE type; so does the tree now.
    using ::EffectsVertexBufferBatch;

    // BrnSimpleParticleRenderer.h:125 (DWARF). sizeof == 16 (== Array element stride,
    // X360-attested by Array<SimpleParticleBatch,13>::Append slwi-by-4 and operator[]).
    struct SimpleParticleBatch : public EffectsVertexBufferBatch
    {
        // AttribSys::Enums::NativeParticleType::NativeParticleType (BrnSimpleParticleRenderer.h:130)
        u32 meParticleType;   // enum word @ +0x08
        // CB4ParticleArrayStandardParams::EParticleBlend (BrnSimpleParticleRenderer.h:131)
        u32 meBlendMode;      // enum word @ +0x0C
    };

    // BrnSimpleParticleRenderer.h:136 (DWARF) -- the frame's simple-particle batch list plus the
    // one word that splits it around the Lion pass.
    //   struct SimpleParticleBatchArray : public Array<SimpleParticleBatch,13u> {
    //       uint32_t muPreLionCount;          // :160
    //       void Clear();                     // :140
    //       void SetPreLionCount();           // :147
    //       uint32_t GetPreLionCount() const; // :153
    //   }
    // All three members are inlined on the console, and each is recovered from its one site:
    //   Clear            ParticleRenderJob::RenderSimpleParticles 0x8291DECC/0x8291DED4 -- `stw 0`
    //                    into the count word (+0xD0) AND into +0xD4, before the first build.
    //   SetPreLionCount  the same body, 0x8291DEF4..0x8291DEFC -- `bl GetLength ; stw r3, 0xD4(r30)`
    //                    between the ten skid-smoke types and the two impact types.
    //   GetPreLionCount  ParticleModule::RenderQuarterResParticles 0x82294AC4 -- `lwzx` of +0x25CD4
    //                    (the array at +0x25C00, +0xD4), the split both Dispatch calls take.
    // +0xD4 == 13 * 16 + 4: the word directly behind Array's count, as the DWARF orders it.
    struct SimpleParticleBatchArray : public Array<SimpleParticleBatch, 13>
    {
        void Clear()
        {
            Array<SimpleParticleBatch, 13>::Clear();
            muPreLionCount = 0;
        }
        void SetPreLionCount() { muPreLionCount = GetLength(); }
        u32  GetPreLionCount() const { return muPreLionCount; }

    private:
        u32 muPreLionCount;   // :160 (+0xD4)
    };
}
}
