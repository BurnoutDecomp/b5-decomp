#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h
//
// cLionFX -- the LION (eauk_lion) particle runtime's front door. DecFIGS DWARF
// LionFX.h:45 declares the class and its API; the X360 bodies are a thin facade over
// the module-scope singletons (cLionEffectManager @dword_83121D94,
// cParticleEmitterManager @dword_831238E8, cParticleRender @dword_82FACC20 ...).
//
// ⚠ THE MEMBERS ARE STATIC, and that is settled by the ASM, not by the DWARF dump.
// Every cLionFX entry point takes its first real argument in r3 with no `this`:
// BinLoad @0x82914388 is `BinLoad(blob)`, EffectCreate @0x82914CB8 forwards to
// cLionEffectManager::EffectCreate(&dword_83121D94, a1..a5) supplying the manager
// from a global rather than from `this`. (The dwarfdump cannot show the difference --
// it prints no implicit parameter either way.) Declaring them non-static would mangle
// every call site to a symbol with an extra `this` and silently fork the ODR.
//
// RECONSTRUCTED HERE: BinLoad, and -- added 2026-09-03 by the boost-exhaust wave --
// Init, Update, Render and Dispatch. ⭐ Init @0x82914A98 and Dispatch @0x82912BA8 ARE
// EXPORT-SET HOLES: IDA names both (in cParticleSystem::AppInit's and
// cParticleRender::Dispatch's `xrefs_to` respectively) but emits no 0x<addr>.json for
// either, so neither had a ledger row, a dossier or a pseudocode listing. They were
// disassembled straight out of the packed .i64 via tools/re/x360rd.py and cross-checked
// against the DecFIGS DWARF, which declares both with full parameter names.
//
// ⭐ ADDED 2026-09-03: the LOCATOR / SCALER / TRIGGER REGISTRY SURFACE -- LocatorRegister,
// LocatorUpdate, TriggerRegister, TriggerUpdate and ScalerUpdate. Each is a three-to-
// seventeen instruction facade over a pool allocation and one call; they were parked only
// because their callees were, and cParticleLocator::Update @0x829098D0 landing removes the
// last of that.
//
// STILL NOT DECLARED, deliberately -- a declaration with no definition is how a caller
// gets to fail at link instead of against a quiet body: DeInit, Flush, the three
// UnRegisters, ScalerRegister (an export-set hole), EffectCreate/EffectDestroy/
// EffectSetWorldIndex (all three need cLionEffectInstance, which is not homed), the
// Text/Bin (un)load pair beyond BinLoad/BinSave, and the fog/lod setters.
// ============================================================================

#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionBlockAlloc.h"
#include "SDKs/Packages/Lion/Final/Allocator/include/CoreAllocator/ITaggedAllocator.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/LionBatch.h"  // LionBatchArray (a typedef, so it cannot be forward-declared)

#include "SDKs/Packages/Lion/Final/eauk_common/Maths/Matrix.h"   // cMatrix (LocatorUpdate)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/ext-include/GameStructs/cTime.h"

struct cLionEffectDefinition;   // LionEffect.h (sibling home)
class  iParticleRender;         // ParticleRender/ParticleRender.h
struct cParticleLocator;        // ParticleLocator.h (sibling home)
struct cParticleScaler;         // ParticleScaler.h  (sibling home)
struct cParticleTrigger;        // ParticleTrigger.h (sibling home)
struct EffectsVertexBufferLocked;   // EffectsVertexBuffer.h
namespace renderengine { class VertexBuffer; class TextureState; }

// ----------------------------------------------------------------------------
// The three per-emitter sub-object pools. They are Lion MODULE globals, not members of
// anything: cParticleSystem::AppInit @0x82913810 sizes them (176 / 4 / 16 bytes, two
// entries per emitter) and the only users are cLionFX's own registries --
//   gLionLocatorAllocator  unk_83123914   cLionFX::LocatorRegister @0x8290AC20
//   gLionScalerAllocator   unk_83122DD0   cLionFX::ScalerRegister
//   gLionTriggerAllocator  unk_83121D64   cLionFX::TriggerRegister @0x8290ACA8
// -- which is why they are homed in this TU. The NAMES are this project's (the binary
// names none of the three); the sizes, counts and users are asm facts.
// ----------------------------------------------------------------------------
extern cLionBlockAlloc gLionLocatorAllocator;
extern cLionBlockAlloc gLionScalerAllocator;
extern cLionBlockAlloc gLionTriggerAllocator;

// DecFIGS DWARF LionFX.h:45.
struct cLionFX
{
    // cLionFX::Init @0x82914A98 (DWARF LionFX.h:51, parameter names from LionFX.cpp:54).
    // Build the whole Lion runtime: the small-block allocator and its main page, the
    // effect-instance manager, every cParticleSystem pool, and the particle-render
    // singleton -- and publish apRenderer as gpLionParticleRender, which is the pointer
    // cParticleMaterial::Build tests before registering a material's texture.
    static void Init(EA::Allocator::ITaggedAllocator* apAllocator,
                     iParticleRender* apRenderer,
                     void* apPhysics,
                     u32 auEmitterCount,
                     u32 auParticleCount,
                     u32 auDynamicParticleCount);

    // cLionFX::Update @0x82915758 (DWARF LionFX.h:56). Advance every live emitter.
    static void Update(const cTime& arTime);

    // cLionFX::Render @0x82914C50 (DWARF LionFX.h:65). Build this frame's particle
    // vertex buffers + batch list from the live emitters.
    //
    // ⚠ THE DWARF GIVES THIS FIVE PARAMETERS AND THE X360 BUILD TAKES THREE. DecFIGS
    // declares `Render(EffectsVertexBufferLocked&, LionBatchArray&, const cTime&, U32
    // aRenderGroupStart, U32 aRenderGroupEnd)`; ARTIST's body reads only r3/r4/r5 and
    // calls cParticleRender::Render(instance, r3, r4, &emitterManager, r5). The X360
    // ledger arbitrates what exists (AGENTS.md source-of-truth ladder), so the two
    // render-group parameters are a merge-window delta and are left out.
    static void Render(EffectsVertexBufferLocked& arVertexBuffer,
                       LionBatchArray& arBatchArray,
                       const cTime& arTime);

    // cLionFX::Dispatch @0x82912BA8 (DWARF LionFX.h:79). Replay the frame's batch list
    // to the device.
    static void Dispatch(renderengine::VertexBuffer* apVertexBuffer,
                         const LionBatchArray& arBatchArray,
                         f32 afWhiteLevel,
                         bool abEnableZFade,
                         f32 afNearPlane,
                         f32 afFarPlane,
                         f32 afDepthFadeDistance,
                         f32 afDepthSamplerOffsetU,
                         f32 afDepthSamplerOffsetV,
                         renderengine::TextureState* apDepthTextureState);

    // ---- the binding registries (DWARF LionFX.h:23 / :29 / :38 / :41 / :50) ----------
    // ⚠ The two Register entry points take a `const char*` NAME that the console IGNORES:
    // both overwrite r3 with their pool's address as their first instruction. The DWARF is
    // the authority for declaration shape, so the parameter stays; that it is dead is
    // recorded rather than dropped.
    static cParticleLocator* LocatorRegister(const char* apcName);
    static void LocatorUpdate(cParticleLocator* apLocator, const cMatrix& arMat,
                              const cTime& arTime);
    static cParticleTrigger* TriggerRegister(const char* apcName);
    static void TriggerUpdate(cParticleTrigger* apTrigger, u32 auFlags, const cTime& arTime);
    static void ScalerUpdate(cParticleScaler* apScaler, f32 afScale, const cTime& arTime);

    // cLionFX::BinLoad @0x82914388 (DWARF LionFX.h:99). Take a saved LION effect blob,
    // check its version word, re-base the effect graph inside it and build it, then link
    // the effect into the runtime's global effect chain. Returns the blob as a
    // cLionEffectDefinition*, or NULL when the blob is null or is not a LION effect.
    static cLionEffectDefinition* BinLoad(void* apData);

    // cLionFX::BinSave @0x82914438 (DWARF LionFX.h:100) -- the inverse: serialise a
    // definition's effect graph into a cLionSerialiser buffer and hand the bytes to the
    // supplied stream. TRAP STUB, deliberately: its chain needs cLionParticleEffect::
    // Remap and cLionEffectDefinition::Delocate (neither reconstructed) and
    // cLionSerialiser::StringStore (whose body the export set has a hole for). Declared
    // and defined only because ParticleDescriptionResourceType::Serialise @0x8267C220
    // calls it; nothing on the PC calls THAT (the game reads .lef data, it never writes
    // it), so the trap is unreachable rather than merely unlikely.
    static int BinSave(void* apData, int aiEndianTwiddleFlag, void* apStream);
};
