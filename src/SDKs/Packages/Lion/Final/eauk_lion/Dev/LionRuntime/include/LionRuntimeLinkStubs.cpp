// ============================================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionRuntimeLinkStubs.cpp
//
// [FLAG link stubs] The Lion runtime symbols the INSTALL path's link closure needs and that
// this build cannot reach. Every one is a real, unreconstructed X360 ledger body; each is a
// LOUD trap, never a quiet return.
//
// ⭐⭐ THE ORIGINAL SAFETY ARGUMENT FOR THIS FILE IS NOW HALF FALSE, AND FOUR ENTRIES LEFT IT
// (2026-09-03). It used to read: "Nothing registers an emitter. cLionFX::EffectCreate
// @0x82914CB8 is not reconstructed, so no cLionEffectInstance is ever created, so
// cLionParticleEffectManager::BindingsAttach is never called, so cParticleEmitterManager::
// Register is never called." EffectCreate IS reconstructed now (LionEffectManager.cpp), so
// every one of those steps CAN run, and the four bodies that sat behind that argument had to
// land with it or become a live trap on the first effect the game starts:
//
//     cParticleEmitter::Bind                              -> ParticleEmitter.cpp
//     cParticleEmitter::BucketRemove        @0x82909790   -> ParticleEmitter.cpp
//     cParticleEmitterManager::UnRegister(emitter)        @0x82913760
//                                                         -> ParticleEmitterManager.cpp
//     cParticleEmitterManager::UnRegister(descriptor,...) @0x829146D0
//                                                         -> ParticleEmitterManager.cpp
//
// ⚠ ONE HALF OF THE ARGUMENT STILL HOLDS, and it is what keeps cParticleEmitter::Update's
// trap unreachable: NOTHING STEPS THE RUNTIME. cLionFX::Update / cLionFX::Render are called
// only from ParticleModule::BuildLionVertexBuffers @0x8228AC20, whose LION half is parked, and
// cLionFX::Dispatch only from RenderFullResParticles @0x8229AFD0, whose Lion branch is parked
// too. Both say so in their own log line every run. So an emitter registered by the create
// path above is initialised and linked onto the used list, and never advanced -- which is
// exactly the state the console would be in with those two arms parked.
//
// ⛔ THIS FILE IS A HOLDING PEN, NOT A HOME. Each body below belongs in the TU named beside it.
// Delete each entry as its real body lands -- a stand-in that outlives its reason is a fork
// waiting to happen (this project has already paid for that twice in this subsystem).
// ⛔ AND ITS OWN COMMENTS GO STALE. Two of the four entries removed today were parked on
// reasons that had already expired: UnRegister(emitter) said "its DeInit is not bodied either"
// when cParticleEmitter::DeInit @0x82913330 has been bodied in ParticleEmitter.cpp for some
// time. Re-derive the reason before trusting it.
// ============================================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"
#include "types.hpp"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBehaviour.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

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
//
// ⚠ THE "never driven" CASE IS NOW A REAL ONE, NOT A HYPOTHETICAL. cLionFX::ScalerRegister
// @0x8290AC68 (landed 2026-09-03) initialises a fresh scaler to 1.0f, and nothing on this build
// calls ScalerUpdate, so every effect the create path starts today sits at scale 1.0 -- an
// integral position, which snaps.
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


// ---- cParticleEmitter (home: ParticleEmitter.cpp) --------------------------------------------

// cParticleEmitter::Update @0x829153D8 -- 201 instructions, and the head of the whole Lion
// SIMULATION core: IsGenerating -> Generate -> Emit -> ParticleBuild (1,142) ->
// InitialiseParticle -> Blend -> cParticleBehaviour::Lerp (1,530), plus the three
// SimulateParticlesInBucketGeneral<> kernels (549). That closure is what stands between this
// build and a boost particle on screen.
//
// ⛔ IT IS STILL AN ASSERT AND IT IS STILL UNREACHABLE, for one reason only: nothing calls
// cLionFX::Update. cParticleEmitterManager::Update is its only caller and cLionFX::Update is
// that function's only caller, and the arm that would call THAT
// (ParticleModule::BuildLionVertexBuffers' Lion half) is parked and announces itself. If this
// trap ever fires, an arm was unparked without its callee -- which is real news, and is why
// this one is not softened to a log line.
u32 cParticleEmitter::Update(const cTime& /*arTime*/)
{
    CGS_ASSERT(false, "cParticleEmitter::Update @0x829153D8 -- NOT RECONSTRUCTED (the Lion simulation core)");
    return 0;
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
