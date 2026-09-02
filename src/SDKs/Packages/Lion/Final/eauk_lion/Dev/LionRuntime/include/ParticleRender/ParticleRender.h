#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h
//
// iParticleRender -- the abstract Lion (eauk_lion) particle-renderer interface. The
// runtime owns ONE concrete renderer (here BrnParticle::LionParticleRender) behind this
// interface; cParticleRender::Dispatch / EmitterRender drive it through these virtuals.
//
// LAYOUT AUTHORITY: declaration shape (the virtual method set + signatures) is from the
// DecFIGS DWARF (ParticleRender.h:46). It carries NO data members -- it is a pure
// interface (vtable pointer only). The concrete LionParticleRender lays its members out
// directly after the vtable pointer; the X360 ARTIST asm for the LionParticleRender
// bodies confirms `this` is dereferenced as `vtable@0` then members from +0x04.
//
// Only the surface this project's LionParticleRender override path needs is declared as
// pure-virtual here; the wider Lion interface (mesh / light register, the cVector Render
// overload, IsVisible(emitter), GetPackedFrustumLrtb) is left out until a TU attests it
// in the X360 ledger (gate DWARF members on X360 attestation -- AGENTS.md).
//
// Forward-declared opaque render-path types: these flow through the interface by
// pointer/reference only (the renderer passes them straight to its blend renderer), so a
// full layout is not needed at this boundary.
// ============================================================================

#include "types.hpp"
#include "rw/math/vpu/types.h"   // rw::math::vpu::Matrix44 (the engine 4x4 matrix)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/LionBatch.h" // LionBatchArray

// EA / Lion scalar spellings used by the Lion runtime declarations.
typedef float    float32_t;
// bool8_t: EABase (vendor/EABase/.../eabase.h) defines the same name under the cooperative
// BOOL8_T_DEFINED guard, and every TU that embeds the effects module reaches both headers.
// Honour the guard both ways (whichever comes first defines it; both are one byte).
#ifndef BOOL8_T_DEFINED
#define BOOL8_T_DEFINED
typedef u8       bool8_t;
#endif
typedef u32      U32;
typedef float    FP32;

// cMatrix -- the engine's 4x4 matrix (the RenderWare vpu Matrix44). Single typedef shared by
// the renderer interface and its concrete implementations.
typedef rw::math::vpu::Matrix44 cMatrix;

class cParticleMaterial;            // ParticleMaterial.h
class cParticleEmitter;             // owning emitter (opaque at this boundary)
class cLionFog;                     // fog descriptor (opaque)
class cTime;                        // engine time stamp (opaque)
struct RenderedParticle;            // per-particle render record (opaque)

namespace renderengine { class TextureState; class VertexBuffer; }
namespace EA { namespace Allocator { class ITaggedAllocator; } }

// Locked particle vertex buffer (EffectsVertexBuffer.h) -- EmitterRender/EmitterCubeRender
// stream vertices through it. Opaque at this boundary (used by reference only).
struct EffectsVertexBufferLocked;

// EffectsVertexBufferIterator -- the locked vertex-buffer write cursor the renderers fill.
// Opaque here (only passed by reference into the blend renderer).
class EffectsVertexBufferIterator;

// ----------------------------------------------------------------------------
// iParticleRender -- pure-virtual renderer interface (ParticleRender.h:46).
// ----------------------------------------------------------------------------
class iParticleRender
{
public:
    virtual ~iParticleRender() {}

    virtual void Render(EffectsVertexBufferIterator& arIterator,
                        RenderedParticle* apParticle,
                        const cMatrix* apMatrix,
                        U32 auCount,
                        U32 auFlags,
                        const cParticleEmitter* apEmitter,
                        const cLionFog* apFog,
                        const cTime& arTime) = 0;

    virtual void TextureRegister(cParticleMaterial* apMaterial, char* apcName) = 0;
    virtual void TextureUnRegister(cParticleMaterial* apMaterial) = 0;

    virtual void RenderGroupBeginLite(const cParticleMaterial& arMaterial) = 0;
    virtual void RenderGroupEndLite() = 0;

    virtual void BeginRendering(float32_t afA, bool8_t abB, float32_t afC, float32_t afD,
                                float32_t afE, float32_t afF, float32_t afG,
                                renderengine::TextureState* apTextureState) = 0;
    virtual void EndRendering() = 0;

    virtual void RenderGroupBegin(const cParticleMaterial& arMaterial) = 0;
    virtual void RenderGroupEnd() = 0;

    virtual U32 GetVertexStride(const cParticleMaterial& arMaterial) = 0;
};

// ----------------------------------------------------------------------------
// cParticleRender -- the Lion particle-render pipeline driver (ParticleRender.cpp). It owns
// ONE concrete iParticleRender (mpRenderer) and, per frame, walks the live emitters, culls
// them, drives the per-emitter simulate-and-emit path (EmitterRender / EmitterCubeRender),
// and finally replays the accumulated LionBatchArray to the device (Dispatch).
//
// LAYOUT AUTHORITY (X360 ARTIST asm for the bodies in this TU):
//   mpRenderer          @ guest +0x04  (Dispatch/EmitterRender: renderer = *(this+4))
//   muParticlesRendered @ guest +0x08  (EmitterRender accumulates the emitted vertex count)
//   mpAllocator         @ guest +0x00  (AppInit stores the ITaggedAllocator; unread by the
//                                        render bodies -- named for shape)
// The camera-space transform / view-direction the cull path in Render/EmitterCubeRender reads
// live at guest +0x20 / +0x30; those bodies are VMX128 and are not reconstructed in this pass,
// so those members are intentionally omitted here (grown when Render is homed). X360 pointers
// are 32-bit; on the host they widen, so absolute offsets are NOT host layout facts -- members
// are accessed BY NAME.
// ----------------------------------------------------------------------------
class cParticleRender
{
public:
    // Replay the frame's accumulated batch list to the device: for each LionBatch bind its
    // material's render group + vertex stride and issue the DrawVertices (X360 0x82911E98).
    void Dispatch(renderengine::VertexBuffer* apVertexBuffer,
                  const LionBatchArray& arBatchArray,
                  float32_t afWhiteLevel,
                  bool8_t abEnableZFade,
                  float32_t afNearPlane,
                  float32_t afFarPlane,
                  float32_t afDepthFadeDistance,
                  float32_t afDepthSamplerOffsetU,
                  float32_t afDepthSamplerOffsetV,
                  renderengine::TextureState* apDepthTextureState);

    // Simulate + emit one emitter's buckets into the vertex buffer, appending a LionBatch per
    // material run (X360 0x82913928 / 0x82913C80). Declared for the class surface; the bodies
    // are reconstructed in their own pass (EmitterCubeRender is VMX128; EmitterRender needs the
    // cParticleBucket layout, whose committed placeholder math-type home conflicts with this
    // header's cMatrix -- see ParticleRender.cpp).
    void EmitterRender(const EffectsVertexBufferLocked& arVertexBuffer,
                       const LionBatchArray& arBatchArray,
                       cParticleEmitter* apEmitter,
                       const cTime& arTime);
    void EmitterCubeRender(const EffectsVertexBufferLocked& arVertexBuffer,
                           const LionBatchArray& arBatchArray,
                           cParticleEmitter* apEmitter,
                           const cTime& arTime);

    // Per-frame entry: cull the live emitters and drive the per-emitter path (X360 0x829147F8;
    // VMX128 frustum cull -- reconstructed in its own pass).
    void Render(const EffectsVertexBufferLocked& arVertexBuffer,
                const LionBatchArray& arBatchArray,
                cParticleEmitter* apEmitterList,
                const cTime& arTime);

private:
    EA::Allocator::ITaggedAllocator* mpAllocator;        // guest +0x00
    iParticleRender*                 mpRenderer;          // guest +0x04
    u32                              muParticlesRendered; // guest +0x08
};
