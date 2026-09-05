// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleSystem.cpp
//
// cParticleSystem::AppInit @0x82913810 / ::AppDeInit @0x82911DF0 -- the Lion runtime's
// app-lifetime create/destroy, reconstructed store-for-store from the X360 ARTIST asm and
// pseudocode against the DecFIGS DWARF declarations.
//
// ⭐ WHAT THIS TU IS FOR. Nothing in the Lion runtime has a pool until AppInit runs: no
// bucket array, no emitter array, no locator/scaler/trigger blocks, no waveform tables, no
// effect-manager allocator. cParticleEmitterManager::Register returns null on an empty free
// list, so with this unbuilt no emitter can exist, so no particle can exist, so no boost
// exhaust can draw. Its only caller is cLionFX::Init @0x82914A98 (LionFX.cpp).
//
// ⚠ THE `this` IS NEVER DEREFERENCED. Both console bodies operate exclusively on OTHER
// singletons and module pools; not one store lands on cParticleSystem's own three members.
// That is reproduced as-is -- initialising them here would be an invented arm.
//
// ⚠ FOUR STORES IN AppInit TARGET GLOBALS WITH NO READER ANYWHERE IN THE IMAGE. A literal
// scan of all 30,084 ARTIST function exports finds cParticleSystem::AppInit as the ONLY
// mention of dword_830ED930, flt_83121C58 and flt_83121C5C. They are reproduced (dropping a
// store because it looks dead is how behaviour goes missing quietly), named descriptively,
// and flagged: the ADDRESSES, VALUES and adjacency are measured, the NAMES are this
// project's. ⚠ A literal scan also cannot see a reader that forms the address from a
// different base -- see the "literal scans miss real stores" note in this project's memory --
// so "no reader" here means "none found by that method", not "none exists".
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleSystem.h"

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleBucketManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleWaveForm.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionChunkManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffectManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSerialiser.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleLocator.h"  // sizeof(cParticleLocator) -- the pool block size
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleScaler.h"   // sizeof(cParticleScaler)
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleTrigger.h"  // sizeof(cParticleTrigger)

namespace
{
    // ---- AppInit's literal operands, all from the asm ------------------------------------
    // The three standalone block pools are sized per SUB-OBJECT, and the count is twice the
    // emitter count (`a3 *= 2` at 0x82913838).
    const u32 KU_SUB_POOL_MULTIPLIER = 2;

    // `li r5, 0xB0` -- 176 bytes. cLionFX::LocatorRegister @0x8290AC20 carves a
    // cParticleLocator out of this pool and calls cParticleLocator::Init on it.
    const u32 KU_POOL_BLOCK_LOCATOR = static_cast<u32>(sizeof(cParticleLocator));

    // `li r5, 4` -- 4 bytes. The scaler pool (cLionFX::ScalerRegister); a cParticleScaler is
    // one float on the console.
    const u32 KU_POOL_BLOCK_SCALER  = static_cast<u32>(sizeof(cParticleScaler));

    // `li r5, 0x10` -- 16 bytes. cLionFX::TriggerRegister @0x8290ACA8 carves from this pool
    // and clears four words, which is the whole record.
    const u32 KU_POOL_BLOCK_TRIGGER = static_cast<u32>(sizeof(cParticleTrigger));

    // ⭐⭐ THE DAY THIS NOTE WARNED ABOUT IS TODAY (2026-09-05). It used to read "none of the
    // three Register functions is reconstructed, so nothing carves out of these pools on this
    // build. Re-derive each from sizeof() the day one does -- a host record wider than its
    // console literal overruns every slot in the pool." Something carves from all three now:
    // ParticleModule::DispatchThreadUpdate calls LocatorRegister / ScalerRegister /
    // TriggerRegister once per created effect.
    //
    // So the block size is sizeof() of the HOST record, with the console's literal kept as a
    // floor -- a pool block smaller than the console's would mean a member had been lost, and
    // a size drift now fails the gate here instead of corrupting the heap at run time. (All
    // three still measure the console's own numbers: cParticleLocator's one trailing pointer
    // widens into padding its 16-byte alignment already had, and the other two carry no
    // pointers at all.)
    static_assert(sizeof(cParticleLocator) >= 0xB0,
                  "the console pool block is 0xB0 -- cParticleSystem::AppInit `li r5, 0xB0`");
    static_assert(sizeof(cParticleScaler) >= 4,
                  "the console pool block is 4 -- `li r5, 4`");
    static_assert(sizeof(cParticleTrigger) >= 16,
                  "the console pool block is 16 -- `li r5, 0x10`");

    // ---- the three "no reader found" globals (see the banner) -----------------------------
    // dword_830ED930 <- apAllocator. It sits immediately after cParticleWaveFormTable::
    // mSingleton (0x830EA930 + 6*512*4 == 0x830ED930), which is what places it in the waveform
    // TU; nothing in the image reads it.
    EA::Allocator::ITaggedAllocator* gpLionWaveFormAllocator = nullptr;

    // flt_83121C58 / flt_83121C5C <- 1.0f, 1.0f. The two words immediately after
    // gpLionSerialiserAllocator (0x83121C54). Nothing in the image reads either.
    f32 gfLionScaleDefaultA = 0.0f;
    f32 gfLionScaleDefaultB = 0.0f;
}  // namespace

// ----------------------------------------------------------------------------
// cParticleSystem::GetMe (DWARF ParticleSystem.h:43)
//
// NO STANDALONE X360 BODY -- inlined at both call sites, which is why cLionFX::Init and
// cLionFX::DeInit @0x82912B18 pass the literal `&unk_83121B44` as `this`. A file-scope
// object, not a function-local static: the console has no guard word beside it.
// ----------------------------------------------------------------------------
namespace
{
    cParticleSystem gParticleSystemSingleton;   // X360 mSingleton @0x83121B44
}

cParticleSystem* cParticleSystem::GetMe()
{
    return &gParticleSystemSingleton;
}

// ----------------------------------------------------------------------------
// cParticleSystem::AppInit  @ 0x82913810
//
// The console body, in its own order:
//     cParticleBucketManager::AppInit (&unk_831238C0, alloc, aBucketCount, aMatrixBucketCount)
//     cParticleEmitterManager::AppInit(&dword_831238E8, alloc, aEmitterCount)
//     aEmitterCount *= 2
//     cLionBlockAlloc::Init(&unk_83123914, alloc, 176, n)     the locator pool
//     cLionBlockAlloc::Init(&unk_83122DD0, alloc,   4, n)     the scaler pool
//     cLionBlockAlloc::Init(&unk_83121D64, alloc,  16, n)     the trigger pool
//     cParticleWaveFormTable::Init(&flt_830EA930)
//     off_83123798   = alloc      cLionParticleEffectManager::mpAllocator
//     off_83121DC8   = alloc      cLionChunkManager::mpAllocator
//     dword_83121DCC = 0          cLionChunkManager::mpChunks
//     dword_830ED930 = alloc      (no reader found -- see the banner)
//     dword_831237BC = 0          cLionParticleEffectManager::mpEffects
//     cLionBlockAlloc::Init(&unk_8312379C, alloc, 12, n)      ...::mAllocator
//     64-word clear at unk_831237C0                          ...::mpEffectGroups[64]
//     off_83121C54 = alloc        gpLionSerialiserAllocator
//     flt_83121C58 = 1.0 ; flt_83121C5C = 1.0                 (no reader found)
//
// FOUR of those groups are inlined member functions of other classes, and each is
// re-outlined to the function the DWARF names (AGENTS.md: reverse the compiler's inlining):
//   cLionChunkManager::AppInit(alloc)                          (DWARF LionChunkManager.h:15)
//   cLionParticleEffectManager::AppInit(alloc, n)              (DWARF ...Manager.h:49)
// The three standalone block pools are NOT members of anything -- they are Lion module
// globals whose only users are cLionFX's Locator/Scaler/TriggerRegister, so they are homed
// in LionFX.cpp beside those users.
//
// ⚠ ORDER IS PRESERVED EXACTLY, including the odd interleave where the chunk manager and the
// waveform allocator are bound in the MIDDLE of the effect manager's own init. That is what
// the asm does; a tidier grouping would be a different program.
// ----------------------------------------------------------------------------
void cParticleSystem::AppInit(EA::Allocator::ITaggedAllocator* apAllocator,
                              u32 auEmitterCount,
                              u32 auBucketCount,
                              u32 auMatrixBucketCount)
{
    cParticleBucketManager::Instance().AppInit(apAllocator, auBucketCount, auMatrixBucketCount);
    cParticleEmitterManager::Instance().AppInit(apAllocator, auEmitterCount);

    // Every per-emitter sub-object pool holds two entries per emitter.
    const u32 luSubPoolCount = auEmitterCount * KU_SUB_POOL_MULTIPLIER;

    gLionLocatorAllocator.Init(apAllocator, KU_POOL_BLOCK_LOCATOR, luSubPoolCount);
    gLionScalerAllocator.Init (apAllocator, KU_POOL_BLOCK_SCALER,  luSubPoolCount);
    gLionTriggerAllocator.Init(apAllocator, KU_POOL_BLOCK_TRIGGER, luSubPoolCount);

    cParticleWaveFormTable::GetMe()->Init();

    // The effect manager's allocator is bound before the chunk manager on the console, then
    // the rest of the effect manager's init follows the waveform allocator store. Re-outlined
    // as the two AppInit calls the DWARF names, in the position of their first store.
    cLionParticleEffectManager::Instance().AppInit(apAllocator, luSubPoolCount);
    cLionChunkManager::GetMe()->AppInit(apAllocator);

    gpLionWaveFormAllocator = apAllocator;      // dword_830ED930 -- no reader found

    gpLionSerialiserAllocator = apAllocator;    // off_83121C54
    gfLionScaleDefaultA = 1.0f;                 // flt_83121C58  -- no reader found
    gfLionScaleDefaultB = 1.0f;                 // flt_83121C5C  -- no reader found
}

// ----------------------------------------------------------------------------
// cParticleSystem::AppDeInit  @ 0x82911DF0
//
// The console body, in order:
//     cParticleBucketManager::AppDeInit(&unk_831238C0)
//     emitterManager.mpAllocator->Free(emitterManager.mpEmitters, 0)   [inlined]
//     emitterManager.mpEmitters = mpUsed = mpFree = mEmitterCount = 0  [inlined]
//     cLionBlockAlloc::DeInit(&unk_83123914)    locator pool
//     cLionBlockAlloc::DeInit(&unk_83122DD0)    scaler pool
//     cLionBlockAlloc::DeInit(&unk_83121D64)    trigger pool
//     cLionChunkManager::AppDeInit(&off_83121DC8)
//     cLionBlockAlloc::DeInit(&unk_8312379C)    the effect manager's own pool
//
// ⛔ THE EMITTER-MANAGER TEARDOWN IS NOT REPRODUCED HERE, and it is announced rather than
// faked: the console inlines cParticleEmitterManager::AppDeInit (four stores plus one Free),
// but that method has no declaration in this tree and writing the stores from OUTSIDE the
// class would poke another object's privates -- exactly the offset-hack the project forbids.
// It lands with the emitter manager's own teardown TU. Nothing on this build calls AppDeInit
// at all (cLionFX::DeInit is not reconstructed), so nothing regresses in the meantime.
// ----------------------------------------------------------------------------
void cParticleSystem::AppDeInit()
{
    cParticleBucketManager::Instance().AppDeInit();

    gLionLocatorAllocator.DeInit();
    gLionScalerAllocator.DeInit();
    gLionTriggerAllocator.DeInit();

    cLionChunkManager::GetMe()->AppDeInit();
    cLionParticleEffectManager::Instance().AppDeInit();
}
