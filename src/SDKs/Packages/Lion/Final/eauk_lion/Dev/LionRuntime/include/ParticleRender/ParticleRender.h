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
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Vector.h"   // cVector (mCamPos/mCamDir)
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

// ⛔⛔ cMatrix WAS A TYPEDEF TO rw::math::vpu::Matrix44 HERE, AND THAT WAS THE THIRD FORK
// OF THE TYPE -- the one that parked cParticleRender::EmitterRender / EmitterCubeRender for
// two waves ("a committed-header type conflict"). The DecFIGS DWARF gives cMatrix its own
// home and its own member names (eauk_common/Maths/Matrix.h:51, `cVector xa,ya,za,wa`), so
// the typedef was not the engine's fact, it was this header's guess -- and a guess that was
// a hard redefinition of two sibling Lion headers' `struct cMatrix`.
// ⭐ THE SWAP CHANGES NO BYTE: Matrix44 is four 16-byte, 16-aligned Vector4 rows and cMatrix
// is four 16-byte, 16-aligned cVector rows, both 64 bytes -- which is why the renderer's
// mCameraTransform slot and every `const cMatrix*` parameter below keep their layout.
#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"   // cMatrix -- the one home

class cParticleMaterial;            // ParticleMaterial.h
class cParticleEmitter;             // owning emitter (opaque at this boundary)
struct cParticleEmitterManager;     // ParticleEmitterManager.h -- Render takes it by reference
class cLionFog;                     // fog descriptor (opaque)
// ⛔⛔ `struct`, NOT `class` -- AND THAT IS A LINK FACT, NOT A STYLE ONE. MSVC mangles the two
// class-keys differently (`U` vs `V` in the decorated name), so a `class cTime;` forward
// declaration here against the real `struct cTime` in ParticleBucket.h produced
// `?Update@cParticleRender@@QEAAXAEBVcTime@@@Z` in the definition and
// `...AEBUcTime@@@Z` at every call site: two DIFFERENT symbols, one defined and one
// unresolved, from source that compiles clean (C4099 is level 2 and this build runs at /W1).
// Measured 2026-09-03: it cost three LNK2019s on cParticleRender::AppInit / Update / Render
// with the definitions sitting in the linked object the whole time. Same rule for
// ITaggedAllocator below.
struct cTime;                       // engine time stamp -- ext-include/GameStructs/cTime.h

// RenderedParticle is NO LONGER OPAQUE (2026-09-04). Its real home is included below rather
// than forward-declared: it is the OUTPUT of cParticleEmitter::ParticleBuild @0x82910118 and
// the INPUT of the three LionBlendRenderer draw halves, so neither side could be written by
// name while it was a bare `struct RenderedParticle;`. Seven 16-byte members, sizeof 0x70 ==
// the stride RenderSprites @0x82282608 walks the run with. The header pulls only the Lion
// vector home, so it adds no cycle here.
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/RenderedParticle.h"

namespace renderengine { class TextureState; class VertexBuffer; }
namespace EA { namespace Allocator { struct ITaggedAllocator; } }   // struct: see the note above

// Locked particle vertex buffer (EffectsVertexBuffer.h) -- EmitterRender/EmitterCubeRender
// stream vertices through it. Opaque at this boundary (used by reference only).
struct EffectsVertexBufferLocked;

// EffectsVertexBufferIterator -- the locked vertex-buffer write cursor the renderers fill.
// Opaque here (only passed by reference into the blend renderer).
struct EffectsVertexBufferIterator;  // struct, not class: both definitions say struct, and the mismatch mangles PEAV vs PEAU

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
// LAYOUT AUTHORITY: the DecFIGS DWARF (ParticleRender.h:137, members h:249..258) gives the
// COMPLETE member set, and the X360 ARTIST asm attests every one of its offsets:
//
//   mpAllocator             @ +0x000  h:249   cLionFX::Init  `stw r30, 0(r11)`     @0x82914BAC
//   mpRenderer              @ +0x004  h:250   cLionFX::Init  `stw r26, 4(r11)`     @0x82914BBC
//                                             (and Dispatch/EmitterRender read *(this+4))
//   mParticlesRenderedCount @ +0x008  h:251   EmitterRender accumulates the vertex count
//   mFogEnabledFlag         @ +0x00C  h:252
//   mFogNear                @ +0x010  h:253
//   mFogFar                 @ +0x014  h:254
//   mCamPos                 @ +0x020  h:255   cVector -- 16-byte aligned, so +0x18 pads to 0x20
//   mCamDir                 @ +0x030  h:256
//   mLodDistances[8]        @ +0x040  h:257   cLionFX::Init stores 100,90,80,70,60,50,40,30
//                                             into +0x40..+0x5C (0x82914B9C..0x82914C08)
//   mFogAlphas[256]         @ +0x060  h:258   -> the record ends at +0x460
//
// ⭐⭐ THE END OF THE RECORD IS INDEPENDENTLY CORROBORATED, and it is what pins the whole
// layout: the singleton lives at 0x82FACC20 and MSVC's magic-static guard word for it is
// dword_82FAD080 -- exactly 0x460 bytes later. So the DWARF member list and the asm's own
// data layout agree on the size with nothing left over. (Same reasoning as the "sizeof is
// short by N -- check ALIGNMENT first" rule: the two 16-byte-aligned cVectors are what make
// mLodDistances land on 0x40.)
//
// X360 pointers are 32-bit; on the host they widen, so absolute offsets are NOT host layout
// facts -- members are accessed BY NAME.
// ----------------------------------------------------------------------------
class cParticleRender
{
public:
    // The render singleton (DWARF h:165). NO STANDALONE X360 BODY: every caller inlines it,
    // which is why each shows the same magic-static guard word (dword_82FAD080) and the same
    // `atexit(cParticleRender::Instance::`2'::dynamic atexit destructor for 'm_instance')`
    // registration -- and that mangled symbol is what names the object `m_instance` and its
    // accessor `Instance`. Reproduced as a function-local static, which is the construct that
    // emits exactly that guard + atexit pair.
    static cParticleRender& Instance();

    // App lifetime (DWARF h:174). NO STANDALONE X360 BODY -- inlined into cLionFX::Init
    // @0x82914A98 (0x82914B9C..0x82914C08); re-outlined here as the source's own function.
    void AppInit(EA::Allocator::ITaggedAllocator* apAllocator, iParticleRender* apRenderer);

    // Per-frame (DWARF h:224), called by cLionFX::Update (DWARF LionFX.cpp:111 lists it).
    // ⚠ ATTESTED EMPTY ON THIS BUILD: cLionFX::Update @0x82915758 emits the Instance()
    // guard and then ONLY `cParticleEmitterManager::Update` -- there is no call and no store
    // between them, so cParticleRender::Update compiled to nothing. The empty body is the
    // faithful transcription, not a stub; see ParticleRender.cpp.
    void Update(const cTime& arTime);

    // DWARF h:217.
    iParticleRender* GetpRenderer() { return mpRenderer; }

    // DWARF h:207 / h:215.
    void SetLodDistance(u32 auLodGroup, FP32 afDistance) { mLodDistances[auLodGroup] = afDistance; }
    FP32 GetLodDistance(u32 auLodGroup) const           { return mLodDistances[auLodGroup]; }

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
    //
    // ⚠ SIGNATURE DEFECT FIXED 2026-09-03. This was declared with a third parameter of
    // `cParticleEmitter* apEmitterList`. It is the emitter MANAGER, not an emitter: the DWARF
    // (h:184) types it `cParticleEmitterManager &`, and its sole caller cLionFX::Render
    // @0x82914C50 passes `&dword_831238E8`, which is the manager singleton (its +0x10/+0x14/
    // +0x18/+0x28 are the very words cParticleSystem::AppDeInit @0x82911DF0 clears as
    // mpEmitters / mpFree / mpUsed / mpAllocator). The old declaration had no definition, so
    // nothing had yet been built against it.
    void Render(EffectsVertexBufferLocked& arVertexBuffer,
                LionBatchArray& arBatchArray,
                cParticleEmitterManager& arEmitterManager,
                const cTime& arTime);

private:
    // Compile-time layout pins (defined at the foot of ParticleRender.cpp; never called). A
    // private static member so it can see the members below.
    static void _AssertLayout();

    EA::Allocator::ITaggedAllocator* mpAllocator;              // +0x000  ParticleRender.h:249
    iParticleRender*                 mpRenderer;               // +0x004  ParticleRender.h:250
    U32                              mParticlesRenderedCount;  // +0x008  ParticleRender.h:251
    U32                              mFogEnabledFlag;          // +0x00C  ParticleRender.h:252
    FP32                             mFogNear;                 // +0x010  ParticleRender.h:253
    FP32                             mFogFar;                  // +0x014  ParticleRender.h:254
    cVector                          mCamPos;                  // +0x020  ParticleRender.h:255
    cVector                          mCamDir;                  // +0x030  ParticleRender.h:256
    FP32                             mLodDistances[8];         // +0x040  ParticleRender.h:257
    FP32                             mFogAlphas[256];          // +0x060  ParticleRender.h:258
};
