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

// EA / Lion scalar spellings used by the Lion runtime declarations.
typedef float    float32_t;
typedef u8       bool8_t;
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

namespace renderengine { class TextureState; }

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
