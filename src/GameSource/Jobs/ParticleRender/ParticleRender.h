#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsCamera.h"
#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"
#include "GameSource/Effects/Particles/Native/BrnSimpleParticleArray.h"
#include "GameSource/Effects/Particles/Native/BrnSparkRenderer.h"
#include <cstddef>

// DecFIGS GameSource/Jobs/ParticleRender/ParticleRender.h:33-49 names every field.
// ARTIST BeginParticleRenderJob 8228A7C0 stages these same offsets, and Execute
// 8291DF38 consumes the two array pointers and dispatch flags at28C..2A2.
// Native module records start24D80 and25A80; their980-byte stride reaches the
// next Job at25700 and26400. The128-byte DMA alignment supplies both gaps.
struct CGS_ALIGN(128) ParticleRenderJobData
{
    CgsGraphics::Camera mCamera;                                      //000
    EffectsVertexBufferLocked mDestinationVertexBuffer;               //170
    BrnParticle::Native::SimpleParticleBatchArray mSimpleParticleBatchArray; //180
    BrnParticle::Native::SparkBatchArray mSparkBatchArray;              //258
    const BrnParticle::Native::BrnSimpleParticleArray* mpaSimpleParticleArrays; //28C
    const BrnParticle::Native::SparkArray* mpaSparkArrays;               //290
    f32 mfCurrentTime;                                                //294
    f32 mfTimeMultiplier;                                             //298
    f32 mfWhiteLevel;                                                 //29C
    bool mbIsCrash;                                                   //2A0
    bool mbRenderParticles;                                           //2A1
    bool mbRenderSparks;                                              //2A2
    BrnParticle::Native::SparkFrameDataSet mSparkFrameDataSet;          //2B0
};

CGS_STATIC_ASSERT((sizeof(ParticleRenderJobData) == 0x980), "ARTIST particle render job data extent");
CGS_STATIC_ASSERT((__alignof(ParticleRenderJobData) == 128), "ARTIST particle DMA alignment");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mDestinationVertexBuffer) == 0x170), "ARTIST locked vertex span");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mSimpleParticleBatchArray) == 0x180), "ARTIST simple batches");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mSparkBatchArray) == 0x258), "ARTIST spark batches");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mpaSimpleParticleArrays) == 0x28C), "ARTIST simple array pointer");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mpaSparkArrays) == 0x290), "ARTIST spark array pointer");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mfCurrentTime) == 0x294), "ARTIST render time");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mfTimeMultiplier) == 0x298), "ARTIST time multiplier");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mfWhiteLevel) == 0x29C), "ARTIST white level");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mbIsCrash) == 0x2A0), "ARTIST crash flag");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mbRenderParticles) == 0x2A1), "ARTIST simple render flag");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mbRenderSparks) == 0x2A2), "ARTIST spark render flag");
CGS_STATIC_ASSERT((offsetof(ParticleRenderJobData,mSparkFrameDataSet) == 0x2B0), "ARTIST motion blur frames");
