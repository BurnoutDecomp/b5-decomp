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

// ⭐⭐ THIS SECTION IS EMPTY (2026-09-06). cParticleBehaviour::Lerp @0x8290B1F8 -- the last
// entry here, and the largest body left in the Lion runtime at 1,530 instructions -- is bodied
// in ParticleBehaviour.cpp. Nothing on cParticleBehaviour is stubbed any more.
//
// ⚠ THE STANDING CLAIM THAT IT WAS OFF THE PARTICLE PATH IS REFUTED, and the refutation is
// worth keeping because it was written here twice. The line said "every effect sits at scaler
// 1.0, an integral position, which snaps". Both halves were false: cLionFX::ScalerUpdate
// @0x82908878 has exactly one xref-to on the console (ParticleModule::DispatchThreadUpdate
// @0x8229C5F0), that loop IS bodied in this tree and calls it every iteration with the
// record's own mfStateBlend, and the stub's own log line fired in EVERY run of 2026-09-05 --
// in scratch/FLAME/BIG at line 21145, right after the first [lionemit] roster and ~66k lines
// before the first boost instance, i.e. on the EXHAUST SMOKE, from the first ticks of the
// drive. The interpolator was live and missing on every run of the game.


// ---- cParticleEmitter (home: ParticleEmitter.cpp) --------------------------------------------

// ⭐⭐ THE WHOLE cParticleEmitter CHAIN IS BODIED (2026-09-05) and this section is EMPTY.
// Update @0x829153D8, ParticleBuild @0x82910118, Generate @0x82915158, Emit @0x82914D38 and
// ParentMatrixCurrentBuild @0x829113E8 all live in ParticleEmitter.cpp now, with the seven
// behaviour processors underneath them. Nothing on the emitter is stubbed here any more.
//
// ⚠ THAT DOES NOT MEAN THE SIMULATION RUNS. It still cannot be reached, for the same single
// reason as before: nothing calls cLionFX::Update, because ParticleModule::BuildLionVertexBuffers'
// LION half is parked and announces itself every run. What stands between this file and a
// particle on screen is now the three cParticleEmitter::SimulateParticlesInBucketGeneral<>
// kernels (546 instructions) plus the render driver -- cParticleRender::Render @0x829147F8,
// EmitterRender @0x82913928 and EmitterCubeRender @0x82913C80 -- and the two parked
// ParticleModule arms. None of those belongs in this holding pen.

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

// D3DDevice_DrawVertices -- RETIRED FROM THIS HOLDING PEN (2026-09-05).
//
// It used to be a CGS_ASSERT(false) trap here, on the stated grounds that "writing a real
// DrawPrimitive here would be inventing a draw path for a pass that cannot reach it". That
// premise expired the moment cParticleRender::Dispatch got a body: the Lion pass reaches it on
// every batch, so the trap would fire on every particle draw.
//
// The real body is in pc/gcm/renderengine/XenonD3D9Shims.cpp beside D3DDevice_DrawIndexedVertices
// -- which is where it belongs, because the geometry it draws is the fast-set vertex stash that
// TU owns (D3DDevice_SetStreamSource -> WorldDraw_SetVertexSourceRaw). It expands the Xenos
// QUADLIST (13) this pass submits into a D3D9 TRIANGLELIST using QuadDraw's own 0/1/3/2 corner
// loop; the note above WorldDraw_NonIndexedUP carries the derivation.
