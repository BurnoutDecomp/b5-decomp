// ============================================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionRuntimeLinkStubs.cpp
//
// [FLAG link stubs] The Lion runtime symbols the INSTALL path's link closure needs and that
// this build cannot reach. Every one is a real, unreconstructed X360 ledger body; each is a
// LOUD trap, never a quiet return.
//
// ⭐ WHY THESE EXIST AT ALL, AND WHY THEY ARE SAFE. ParticleModule::Prepare now calls
// cLionFX::Init, so cParticleSystem::AppInit and with it cParticleEmitterManager::AppInit,
// cParticleBucketManager::AppInit and cLionParticleEffectManager::AppInit are on the link. Those
// three TUs also carry their register/unregister/update surface, which references the emitter
// bodies below. NONE of that surface RUNS on this build:
//
//   * Nothing registers an emitter. cLionFX::EffectCreate @0x82914CB8 is not reconstructed, so
//     no cLionEffectInstance is ever created, so cLionParticleEffectManager::BindingsAttach is
//     never called, so cParticleEmitterManager::Register is never called.
//   * Nothing steps the runtime. cLionFX::Update / cLionFX::Render are called only from
//     ParticleModule::BuildLionVertexBuffers @0x8228AC20, whose LION half is parked; and
//     cLionFX::Dispatch only from RenderFullResParticles @0x8229AFD0, whose Lion branch is
//     parked too. Both say so in their own log line every run.
//
// So the free list built by AppInit is populated and never drawn from. If one of these traps
// ever fires, that is real news: it means an arm was unparked without its callee.
//
// ⛔ THIS FILE IS A HOLDING PEN, NOT A HOME. Each body below belongs in the TU named beside it.
// Delete each entry as its real body lands -- a stand-in that outlives its reason is a fork
// waiting to happen (this project has already paid for that twice in this subsystem).
// ============================================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"
#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// ---- cParticleEmitter (home: ParticleEmitter.cpp) --------------------------------------------

// ---- cParticleBehaviour (home: ParticleBehaviour.cpp) ---------------------------------------

// cParticleBehaviour::Lerp @0x8290B1F8 -- 1,530 instructions, a wave of its own: it interpolates
// EVERY channel of two behaviour layers (position/velocity/acceleration/rotation/size/colour
// base-and-variance pairs, the colour steps, the compiled base-variance pack and the AABB) into
// a third.
//
// ⛔ IT IS A LOG-ONCE STUB, NOT AN ASSERT, AND THAT IS DELIBERATE. cParticleEmitter::Blend
// @0x8290F730 reaches this once per frame for any effect whose descriptor has more than one
// behaviour layer AND whose scaler sits strictly between two of them. An assert there is the
// 840,000-line storm that has starved a harness on this project before, so this announces itself
// once and returns.
//
// WHAT THE MISS COSTS, precisely, so nobody has to guess: mpTempBehaviour keeps whatever
// cParticleEmitter::Init left in it (a real Init()+Build() behaviour, not zeros), so a
// mid-blend effect plays that layer instead of an interpolation of its two neighbours. Layers
// selected outright -- a scaler at or within 1% of an integer position, which includes every
// single-layer effect and every effect whose scaler is never driven -- are UNAFFECTED, because
// Blend snaps to a real layer on those paths and never calls this.
void cParticleBehaviour::Lerp(const cParticleBehaviour* /*apLo*/,
                              const cParticleBehaviour* /*apHi*/,
                              f32 /*afWeight*/)
{
    static bool sbLogged = false;
    if (!sbLogged)
    {
        sbLogged = true;
        CgsDev::Log::WriteToLog(
            "[effects] NOT RECONSTRUCTED: cParticleBehaviour::Lerp @0x8290B1F8 (1,530 "
            "instructions). cParticleEmitter::Blend reached a FRACTIONAL blend position; the "
            "temp behaviour keeps its Init()+Build() state, so this effect plays one layer "
            "instead of an interpolation of two. Integral layer positions are unaffected.\n");
    }
}


// cParticleEmitter::Update @0x829153D8 -- 190 pseudocode lines, and the head of the whole Lion
// SIMULATION core: IsGenerating -> Generate -> Emit -> ParticleBuild (1,417 lines) ->
// InitialiseParticle (366) -> Blend (131) -> cParticleBehaviour::Lerp (817). That closure is
// what stands between this build and a boost particle on screen; it is measured at 37 functions
// / ~6,400 pseudocode lines from the four cLionFX entry points.
u32 cParticleEmitter::Update(const cTime& /*arTime*/)
{
    CGS_ASSERT(false, "cParticleEmitter::Update @0x829153D8 -- NOT RECONSTRUCTED (the Lion simulation core)");
    return 0;
}

// cLionBindings::SetEmitter. Inlined at its only call site (BindingsAttach @0x82914530), so it
// has no standalone X360 body to transcribe; it lands with the effect-instance path.
void cParticleEmitter::Bind(cLionBindings& /*arBindings*/)
{
    CGS_ASSERT(false, "cParticleEmitter::Bind -- NOT RECONSTRUCTED (the effect-instance binding path)");
}

// cParticleEmitter::BucketRemove @0x82909790 -- an EXPORT-SET HOLE (IDA names it in
// cParticleBucketManager::Free's xrefs; no JSON). Unlinks a bucket from the emitter's list.
void cParticleEmitter::BucketRemove(cParticleBucket* /*apBucket*/)
{
    CGS_ASSERT(false, "cParticleEmitter::BucketRemove @0x82909790 -- NOT RECONSTRUCTED (export-set hole)");
}

// ---- cParticleEmitterManager (home: ParticleEmitterManager.cpp) -------------------------------

// cParticleEmitterManager::UnRegister(cParticleEmitter*) @0x82913760 -- also an EXPORT-SET HOLE.
// Disassembled out of the image: it unlinks the emitter from mpUsed by walking the +0x204 next
// chain, calls cParticleEmitter::DeInit, then pushes it onto mpFree and decrements mUsedCount.
// NOT bodied here because its DeInit is not bodied either -- a faithful UnRegister that calls a
// trap is worse than the trap, because it does its list surgery FIRST and leaves the pool
// half-modified when the trap fires.
void cParticleEmitterManager::UnRegister(cParticleEmitter* /*apEmitter*/)
{
    CGS_ASSERT(false, "cParticleEmitterManager::UnRegister(emitter) @0x82913760 -- NOT RECONSTRUCTED (export-set hole)");
}

// cParticleEmitterManager::UnRegister(descriptor, bindings, bindBase) @0x829146D0 -- walks the
// used list unregistering (or re-binding) every emitter whose descriptor is, or descends from,
// the given one. Reached only from cLionParticleEffectManager::BindingsRemove.
void cParticleEmitterManager::UnRegister(const cParticleDescriptor& /*arDescriptor*/,
                                         cLionBindings& /*arBindings*/,
                                         cLionBindings* /*apBindBase*/)
{
    CGS_ASSERT(false, "cParticleEmitterManager::UnRegister(descriptor,...) @0x829146D0 -- NOT RECONSTRUCTED");
}

// ---- cParticleRender::Dispatch's platform surface (home: a renderengine PC leaf) --------------
//
// Both are referenced ONLY by cParticleRender::Dispatch, which cannot run on this build (the
// Lion branch of ParticleModule::RenderFullResParticles is parked and announces itself).
//
// gpLionParticleSamplerState is X360 dword_83010F60, the sampler-state object the Lion pass
// binds on sampler 0. Its builder is part of the particle-module render init that is not landed,
// so it is defined null here rather than left as an undefined external -- and Dispatch's
// shadow::Device::SetState(null, 0) is the same no-op the console would make with an unbuilt
// state. FLAG PC-platform leaf: the state object itself is a renderengine resource with no
// console-independent construction path yet.
void* gpLionParticleSamplerState = nullptr;

// D3DDevice_DrawVertices is the Xenon fast-path draw thunk. The sibling
// D3DDevice_SetStreamSource already has a PC home (pc/gcm/renderengine/MeshHelper.cpp); this one
// does not, and writing a real DrawPrimitive here would be inventing a draw path for a pass that
// cannot reach it. FLAG PC-platform leaf: a Xenon D3D9 entry point with no PC equivalent yet.
extern "C" void D3DDevice_DrawVertices(struct IDirect3DDevice9* /*lpDevice*/,
                                       u32 /*luPrimitiveType*/,
                                       u32 /*luStartVertex*/,
                                       u32 /*luVertexCount*/)
{
    CGS_ASSERT(false, "D3DDevice_DrawVertices -- NOT RECONSTRUCTED (Xenon fast-path draw; the "
                      "Lion dispatch pass that uses it is parked)");
}
