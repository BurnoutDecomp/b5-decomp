#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnSparkRenderer.h
//
// BrnParticle::Native::BrnSpark -- one spark particle, and the FXBucket<BrnSpark,4>
// specialisation used by the spark array's bucket banks.
//
// DWARF AUTHORITY (DecFIGS BrnSparkRenderer.h dump):
//   struct BrnSpark { Vector4 mPosition; Vector4 mVelocity; float mfSize; };
//     -- 2*16 + 4 = 36 bytes, 16-byte aligned -> sizeof == 48 (0x30). This matches
//        the 48-byte particle stride and +0x280 particle-array base attested by the
//        FXBucket<BrnSpark,4>::GetNewParticle asm (@ 0x82285460): maParticleData starts
//        at +0x10 + 4*156 = 0x280 = 640, stride 48.
//   typedef FXBucket<BrnParticle::Native::BrnSpark,4> SparkArray::SparkBank::SparkBucket.
//
// Only the BrnSpark element and the SparkBucket typedef are modelled here (the surface
// the reconstructed Clear/GetNewParticle instantiation needs); the renderer/array
// machinery grows additively.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h" // rw::math::vpu::Vector4 / Matrix44 / Matrix44Affine
#include "GameShared/GameClasses/Core/CgsAssert.h" // CGS_ASSERT (GetFrame)
#include "GameSource/Effects/Particles/Native/FXBuckets.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleBatch.h" // EffectsVertexBufferBatch base

namespace BrnParticle
{
namespace Native
{
    // BrnSparkRenderer.h:57 (DWARF) -- motion-blur frame ring size.
    static const u32 knSparksMaxBlurFrames = 8;

    // BrnSparkRenderer.h:63 (DWARF) -- the four spark-array types (+ the _Max count).
    enum ESparkArrayID
    {
        eSparkArray_GrindingWorld    = 0,
        eSparkArray_GrindingRaceCars = 1,
        eSparkArray_Crashing         = 2,
        eSparkArray_BodyPart_Contact = 3,
        eSparkArray_Max              = 4,
    };

    // BrnSparkRenderer.h:140 (DWARF). 8 (EffectsVertexBufferBatch base) + 4 (enum) = 12
    // bytes, no tail padding (max align 4) -> Array<SparkBatch,4> count word lands at
    // +0x30 (== 4*12), matching Append @0x8291FAD0 and operator[] @0x8227CAD8.
    struct SparkBatch : public EffectsVertexBufferBatch
    {
        ESparkArrayID meArrayId;   // BrnSparkRenderer.h:143 -- @ +0x08
    };

    // BrnSparkRenderer.h:75 (DWARF). 200 payload bytes, 16-aligned -> sizeof 0xD0 == 208
    // (stride attested by SparkFrameDataSet::GetFrame @0x8291FA40 mulli r29,0xD0).
    struct alignas(16) SparkFrameData
    {
        rw::math::vpu::Matrix44Affine mViewMatrix;                       // +0x00 (64)
        rw::math::vpu::Matrix44       mProjectionMatrix;                 // +0x40 (64)
        f32                           mfTimeStamp;                       // +0x80
        f32                           mfTimeStampCopyForDebugBuildsOnly; // +0x84
        rw::math::vpu::Matrix44Affine mViewMatrixCopyForDebugBuildsOnly; // +0x88 (64)
    };

    // BrnSparkRenderer.h:105 (DWARF). The ring of knSparksMaxBlurFrames (== 8) motion-blur
    // frame snapshots. GetFrame indexes maFrames[] by frame id.
    struct SparkFrameDataSet
    {
        SparkFrameData maFrames[knSparksMaxBlurFrames]; // +0x00, stride 0xD0

        // X360 @ 0x8291FA40 (called from SparkArray::RenderBank). Bounds-asserts the frame
        // id then returns &maFrames[luFrameId] (this + 208*luFrameId).
        const SparkFrameData& GetFrame(u32 luFrameId) const;
    };

    // BrnSparkRenderer.h:149 (DWARF). 16-byte aligned so sizeof rounds 36 up to 48,
    // giving the attested per-particle stride.
    struct alignas(16) BrnSpark
    {
        rw::math::vpu::Vector4 mPosition; // BrnSparkRenderer.h:152 -- @ +0x00
        rw::math::vpu::Vector4 mVelocity; // BrnSparkRenderer.h:153 -- @ +0x10
        f32                    mfSize;    // BrnSparkRenderer.h:154 -- @ +0x20
    };

    // BrnSparkRenderer.h:158 (DWARF): the spark bank's bucket type. With N=4 the derived
    // KuMaxNumParticles == 156 (KuMaxNumParticlesUnAligned 157 rounded down to even),
    // matching the 0x9C loop bound in Clear/GetNewParticle.
    typedef BrnParticle::FXBucket<BrnSpark, 4> SparkBucket;
}
}
