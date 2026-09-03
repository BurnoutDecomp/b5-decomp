// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.cpp
//
// cLionFX -- the LION runtime's front door. This TU carries:
//   * cLionFX::Init     @0x82914A98   (EXPORT-SET HOLE -- recovered from the image)
//   * cLionFX::Update   @0x82915758
//   * cLionFX::Render   @0x82914C50
//   * cLionFX::Dispatch @0x82912BA8   (EXPORT-SET HOLE -- recovered from the image)
//   * cLionFX::BinLoad  @0x82914388
//   * cLionFX::BinSave  @0x82914438   (trap stub; see its note)
// plus the three per-emitter sub-object pools the registries carve from.
//
// ⭐⭐ HOW Init AND Dispatch WERE FOUND. Neither has a `0x<addr>.json` in
// .ida-exports/BURNOUT_X360_ARTIST.XEX/ and neither has a row in progress/identity.json --
// they were invisible to `work show`, to the dossier, and to every ledger query. What names
// them is the CALLEE side: cParticleSystem::AppInit's `xrefs_to` reads
// `{0x82914A98, "cLionFX::Init"}` and cParticleRender::Dispatch's reads
// `{0x82912BA8, "cLionFX::Dispatch"}`. IDA had the names all along; the exporter dropped the
// bodies. Both were then disassembled straight out of the packed .i64 (tools/re/x360rd.py
// feeding a capstone PPC-BE pass) and read against the DecFIGS DWARF, which supplies both
// signatures with their original parameter names (LionFX.cpp:54 and :150).
// ⚠ Four more Lion functions have the same hole and are still unrecovered:
// cParticleEmitterManager::UnRegister @0x82913760, cParticleEmitter::SubEmitterInit
// @0x829112F0, cParticleEmitter::BucketRemove @0x82909790,
// cParticleBucketManager::MatrixBucketAlloc @0x8290CD60.
//
// WHY BinLoad IS HERE. Every BrnParticle::ParticleDescription resource in PARTICLES.BUNDLE
// carries a saved LION effect at +16, and ParticleDescriptionResourceType::DeSerialise
// @0x82675868 runs BinLoad over it in place; without it a description's effect pointer is
// never rebased and ParticleModule::StartLionEffect cannot resolve an effect.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffectManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffect.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffectManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionSmallAlloc.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleDescriptor.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleMaterial.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleSystem.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitterManager.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/ParticleRender.h"
#include "GameSource/Effects/Particles/LionParticleRender.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdint>
#include <cstdio>

// ----------------------------------------------------------------------------
// The three per-emitter sub-object pools (see LionFX.h). cParticleSystem::AppInit sizes
// them; cLionFX's Locator/Scaler/TriggerRegister carve from them.
// ----------------------------------------------------------------------------
cLionBlockAlloc gLionLocatorAllocator;   // X360 unk_83123914 -- 176-byte cParticleLocator
cLionBlockAlloc gLionScalerAllocator;    // X360 unk_83122DD0 --   4-byte cParticleScaler
cLionBlockAlloc gLionTriggerAllocator;   // X360 unk_83121D64 --  16-byte cParticleTrigger

// ----------------------------------------------------------------------------
// The bucket size cLionFX::Init divides by. It is cParticleBucketManager::
// KU_PARTICLES_PER_BUCKET, restated here with its owner named.
// ⭐ THE REASON FOR THE RESTATEMENT HAS GONE (2026-09-03) AND IS CORRECTED RATHER THAN LEFT TO
// MISLEAD. It used to read: "this TU CANNOT include that class's header: ParticleBucketManager.h
// pulls ParticleBucket.h, whose HONEST-PLACEHOLDER `struct cMatrix` collides with
// ParticleRender.h's `typedef rw::math::vpu::Matrix44 cMatrix` ... the same fork that parks
// cParticleRender::EmitterRender". There is no collision any more -- cMatrix has ONE home
// (eauk_common/Maths/Matrix.h) and both headers use it -- so including the manager here is now
// merely unnecessary, not impossible. Folding this constant back onto its owner is a safe
// follow-up, left out so this wave's diff stays about bodies.
// ----------------------------------------------------------------------------
const u32 KU_PARTICLES_PER_BUCKET = 16;   // == cParticleBucketManager::KU_PARTICLES_PER_BUCKET

// ----------------------------------------------------------------------------
// gpLionParticleRender -- X360 dword_83121D60, the Lion particle-render singleton.
// cLionFX::Init (below) is its writer, and cParticleMaterial::Build @0x8290E500 is the
// reader that matters: `if (gpLionParticleRender) gpLionParticleRender->TextureRegister(
// this, name)`. Until 2026-09-03 Init was unreconstructed and this stayed NULL for the
// whole run, so no material ever registered its texture.
//
// NAMING: the datum is unnamed in the X360 export and no DWARF global pins it; the name is
// this project's, its address and role are asm facts.
// ----------------------------------------------------------------------------
BrnParticle::LionParticleRender* gpLionParticleRender = 0;

// ================================================================================================
// cLionFX::Init  @ 0x82914A98      (DWARF LionFX.h:51 / LionFX.cpp:54)
//
// Disassembled out of the image (this address has no export JSON). The body in order:
//
//   82914AA4  lBucketCount    = (auParticleCount        + 15) >> 4
//   82914AC0  lMatBucketCount = (auDynamicParticleCount + 15) >> 4
//   82914AC4  LionSmallAlloc::Init(apAllocator, 0x10000, 0x1000)          [inlined]
//   82914B08  cLionEffectManager::GetMe()->AppInit(apAllocator, auEmitterCount)   [inlined]
//   82914B5C  cParticleSystem::GetMe()->AppInit(apAllocator, auEmitterCount,
//                                               lBucketCount, lMatBucketCount)
//   82914B64  cParticleRender::Instance().AppInit(apAllocator, apRenderer)  [inlined,
//                                               including the eight lod distances]
//   82914BCC  gpLionParticleRender = apRenderer
//   82914C0C  cParticleRender::Instance()  -- a SECOND magic-static guard, see below
//   82914C24  1024-word clear at unk_83121DD0                              [see below]
//
// ⭐ THE `>> 4` IS A DIVISION BY THE BUCKET SIZE, not a magic shift. 16 ==
// cParticleBucketManager::KU_PARTICLES_PER_BUCKET, so `(n + 15) / 16` is "how many buckets
// hold n particles, rounded up". Written that way here per the de-optimisation rule; the two
// locals keep the DWARF's own names (lBucketCount / lMatBucketCount, LionFX.cpp:59/60).
//
// ⚠ apPhysics (r5) IS UNREAD BY THIS BUILD. The DWARF names the parameter (LionFX.cpp:54) and
// the asm never touches r5 -- it is overwritten with the block-alloc item size at 0x82914B14
// before any use. Its caller (ParticleModule::Prepare) passes 0. Kept in the signature because
// the DWARF attests it -- neither removed without saying so, nor invented into a use.
//
// ⚠ lpWrappedAllocator (DWARF LionFX.cpp:65) IS THE IDENTITY HERE. The DWARF shows the source
// wrapping apAllocator into a local before handing it on; on ARTIST r30 == r3 unchanged all
// the way through, so whatever the wrapper was, it compiled to nothing in this build. The
// local is kept for shape and is exactly apAllocator -- inventing a wrapper would be worse.
//
// ⚠ THE SECOND cParticleRender::Instance() IS REAL AND ITS CALLEE IS NOT RECOVERABLE. The asm
// tests the magic-static guard TWICE (0x82914B64 and 0x82914C0C), and the DWARF likewise lists
// cParticleRender::Instance() twice inside Init -- but the second reference feeds no call and
// no store. It is a call to a method that compiled to nothing on this build, exactly as
// cParticleRender::Update did in cLionFX::Update (GameInit() is the obvious candidate and that
// is a GUESS, so it is not written). Reproduced as the single Instance() use below: a
// magic-static's observable behaviour -- construct once, register the destructor once -- is
// identical either way.
//
// ⚠ THE 1024-WORD CLEAR AT unk_83121DD0 IS *NOT* REPRODUCED, and that is a stated gap, not an
// oversight. It is 0x1000 bytes of u32 ending exactly where the scaler pool begins
// (0x83122DD0). A literal scan of ALL 30,084 ARTIST exports finds NO reader and no other
// writer, and no DWARF global lands on that address, so there is no type, no name and no
// owner to write it into -- a zeroed array invented here would be a fabricated member of a
// fabricated class. Recorded so the next wave can pick it up from the address.
// ================================================================================================
void cLionFX::Init(EA::Allocator::ITaggedAllocator* apAllocator,
                   iParticleRender* apRenderer,
                   void* apPhysics,
                   u32 auEmitterCount,
                   u32 auParticleCount,
                   u32 auDynamicParticleCount)
{
    (void)apPhysics;   // unread on this build -- see the note above

    // The X360 literals: a 64 KB main page grown 4 KB at a time.
    const u32 KU_SMALLALLOC_MAIN_SIZE   = 0x10000;   // `lis r3, 1`   @0x82914ABC
    const u32 KU_SMALLALLOC_GROWTH_SIZE = 0x1000;    // `li  r11, 0x1000` @0x82914AE8

    const u32 lBucketCount =
        (auParticleCount + KU_PARTICLES_PER_BUCKET - 1) / KU_PARTICLES_PER_BUCKET;
    const u32 lMatBucketCount =
        (auDynamicParticleCount + KU_PARTICLES_PER_BUCKET - 1) / KU_PARTICLES_PER_BUCKET;

    EA::Allocator::ITaggedAllocator* lpWrappedAllocator = apAllocator;

    LionSmallAlloc::Init(lpWrappedAllocator, KU_SMALLALLOC_MAIN_SIZE, KU_SMALLALLOC_GROWTH_SIZE);

    cLionEffectManager::GetMe()->AppInit(lpWrappedAllocator, auEmitterCount);

    cParticleSystem::GetMe()->AppInit(lpWrappedAllocator, auEmitterCount,
                                      lBucketCount, lMatBucketCount);

    cParticleRender::Instance().AppInit(lpWrappedAllocator, apRenderer);

    gpLionParticleRender = static_cast<BrnParticle::LionParticleRender*>(apRenderer);

    // ---- [lionfx] witness. NOT console behaviour: ours, one line, once. --------------------
    // ⭐ WHY. The whole point of this function is that four pools and one pointer come into
    // existence; every one of those is invisible from outside. In particular
    // gpLionParticleRender going non-null is what lets cParticleMaterial::Build register a
    // texture at all -- and a build where Init never ran and a build where it ran and every
    // pool came back empty produce the same silence. This line prints the numbers that would
    // be WRONG in either case. DELETE-WHEN-STABLE.
    {
        char lacMsg[320];
        std::snprintf(lacMsg, sizeof(lacMsg),
            "[lionfx] Init emitters=%u particles=%u dynamic=%u -> buckets=%u matBuckets=%u "
            "renderer=%p smallAllocPages=%p mainSize=%u\n",
            auEmitterCount, auParticleCount, auDynamicParticleCount,
            lBucketCount, lMatBucketCount, static_cast<void*>(gpLionParticleRender),
            static_cast<void*>(LionSmallAlloc::mpPages), LionSmallAlloc::mMainSize);
        CgsDev::Log::WriteToLog(lacMsg);
    }
}

// ================================================================================================
// cLionFX::Update  @ 0x82915758      (DWARF LionFX.h:56 / LionFX.cpp:111)
//
// Nine instructions: the cParticleRender magic-static guard, its atexit, and a tail call to
// cParticleEmitterManager::Update. The DWARF lists cParticleRender::Update() between them; it
// compiled to nothing (see cParticleRender::Update in ParticleRender.cpp) and is written here
// because dropping it would lose the only record that the console called it.
// ================================================================================================
void cLionFX::Update(const cTime& arTime)
{
    cParticleRender::Instance().Update(arTime);
    cParticleEmitterManager::Instance().Update(arTime);
}

// ================================================================================================
// cLionFX::Render  @ 0x82914C50      (DWARF LionFX.h:65 / LionFX.cpp:130)
//
//   result = cParticleRender::Render(&dword_82FACC20, a1, a2, &dword_831238E8, a3);
//
// i.e. the render singleton, this frame's locked vertex buffer and batch array, the emitter
// MANAGER singleton (DWARF LionFX.cpp:132 names the local `lEmitterManager`), and the time.
// See LionFX.h for why this takes three parameters where the DWARF declares five.
// ================================================================================================
void cLionFX::Render(EffectsVertexBufferLocked& arVertexBuffer,
                     LionBatchArray& arBatchArray,
                     const cTime& arTime)
{
    cParticleRender::Instance().Render(arVertexBuffer, arBatchArray,
                                       cParticleEmitterManager::Instance(), arTime);
}

// ================================================================================================
// cLionFX::Dispatch  @ 0x82912BA8      (DWARF LionFX.h:79 / LionFX.cpp:150)
//
// Disassembled out of the image (this address has no export JSON). A pure forwarder with the
// cParticleRender magic-static guard inlined in front of it: it saves f1..f6 across the guard
// (fmr f31..f26 at 0x82912BC0..0x82912BE4), reloads the tenth argument off the caller's frame
// (`lwz r11, 0x11C(r1)` -> `stw r11, 0x64(r1)`), and tail-calls
// cParticleRender::Dispatch(&dword_82FACC20, ...). Nine arguments in, ten out -- the extra one
// being the singleton -- with every value passed straight through unchanged.
// ================================================================================================
void cLionFX::Dispatch(renderengine::VertexBuffer* apVertexBuffer,
                       const LionBatchArray& arBatchArray,
                       f32 afWhiteLevel,
                       bool abEnableZFade,
                       f32 afNearPlane,
                       f32 afFarPlane,
                       f32 afDepthFadeDistance,
                       f32 afDepthSamplerOffsetU,
                       f32 afDepthSamplerOffsetV,
                       renderengine::TextureState* apDepthTextureState)
{
    cParticleRender::Instance().Dispatch(apVertexBuffer, arBatchArray, afWhiteLevel,
                                         static_cast<bool8_t>(abEnableZFade),
                                         afNearPlane, afFarPlane, afDepthFadeDistance,
                                         afDepthSamplerOffsetU, afDepthSamplerOffsetV,
                                         apDepthTextureState);
}

// ----------------------------------------------------------------------------
// cLionFX::BinLoad  @ 0x82914388
//
//   if (!apData)                        return NULL;
//   if (def->mVersion != 65539)         return NULL;    -- not a LION effect blob
//   if (def->mpParticles)  def->mpParticles = (u8*)def + (offset)def->mpParticles;
//   def->mpParticles->Relocate();       -- rebase the whole descriptor graph
//   def->mpParticles->Build();          -- finalise materials + behaviours
//   if (def->mpParticles) { effect->mpNext = gpLionParticleEffectChain;
//                           gpLionParticleEffectChain = effect; }
//   return def;
//
// ⚠ THE REBASE IS IN BYTES, and Hex-Rays says otherwise. Its `a1[18] = a1 + v4`
// renders a byte add on a `_DWORD*`, which reads as `a1 + 4*v4`. It is bytes: the asm
// is a plain register add, and the shipped data settles it independently -- in
// BoostYellow.lef the definition sits at payload +16 with word 18 == 96, and +112 is a
// cLionParticleEffect whose descriptor chain walks cleanly to six named descriptors
// (SMOKE, BURSTBOOST, LIGHT, NEWVOLUMEBOOST, BOOSTNORMAL, BOOSTNORMAL_YELLOW), while
// +400 is not a record at all. Same rule as every other self-relative Lion link.
//
// Relocate and Build are called UNCONDITIONALLY on the console -- both carry their own
// null-this guard -- and the chain push is the one part that is guarded. Reproduced in
// that order.
// ----------------------------------------------------------------------------
cLionEffectDefinition* cLionFX::BinLoad(void* apData)
{
    if (apData == 0)
        return 0;

    cLionEffectDefinition* lpDefinition = static_cast<cLionEffectDefinition*>(apData);
    if (lpDefinition->mVersion != cLionEffectDefinition::KU_VERSION)
        return 0;

    // ------------------------------------------------------------------------
    // The one assumption the whole .lef graph rests on, said out loud.
    //
    // Every link in this blob is a 32-bit slot (see LionSerialisedPtr.h) and every
    // Relocate below rebases it with `slot += (u32)&owner`. That is the console's own
    // arithmetic and it is correct on the host ONLY while the resource heap stays below
    // 4 GB -- the same convention ParticleDescriptionResourceType::FixUp @0x8267DF60
    // already relies on. Above 4 GB the truncation is SILENT: the graph would relocate
    // to plausible-looking addresses in the wrong place and fault somewhere unrelated.
    // So say it once, here, at the one point where the whole payload is in hand. The
    // console has no such test; this only writes a log line and changes nothing.
    // DELETE-WHEN-STABLE (or when the resource heap gains a hard below-4 GB guarantee
    // of its own).
    // ------------------------------------------------------------------------
    {
        static bool sbAnnouncedHighBlob = false;
        const uintptr_t lBlob = reinterpret_cast<uintptr_t>(lpDefinition);
        if (!sbAnnouncedHighBlob && lBlob > 0xFFFFFFFFull)
        {
            sbAnnouncedHighBlob = true;
            char lacMsg[256];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[lion] .lef payload at %p is ABOVE 4 GB -- every 32-bit link in it will "
                "relocate to a truncated address. The LION records model serialised links "
                "as u32 by design (they are 4 bytes in the file); what is broken is the "
                "heap placement, not the records.\n", apData);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    // Self-relative byte offset -> pointer.
    lpDefinition->mpParticles.Relocate(lpDefinition);

    lpDefinition->mpParticles->Relocate();
    lpDefinition->mpParticles->Build();

    // Push the newly built effect onto the manager's loaded-effect chain.
    // ⭐ THIS USED TO WRITE A FREE GLOBAL CALLED gpLionParticleEffectChain, whose note in this
    // file claimed "no DWARF global pins it". It does: dword_831237BC is mSingleton + 0x24 of
    // cLionParticleEffectManager, i.e. its mpEffects member (DWARF LionParticleEffectManager.h
    // :88) -- the neighbouring 64-word array cParticleSystem::AppInit clears at 0x831237C0 is
    // that class's mpEffectGroups[64], and sizeof(cLionBlockAlloc)==0x20 is what puts mpEffects
    // at +0x24. Keeping the free global would have meant AppInit clearing one variable while
    // BinLoad pushed onto another -- an ODR fork by duplication, resolved silently at link.
    cLionParticleEffect* lpEffect = lpDefinition->mpParticles.Get();
    if (lpEffect != 0)
    {
        cLionParticleEffectManager& lrManager = cLionParticleEffectManager::Instance();
        lpEffect->mpNext.Set(lrManager.GetpEffects());
        lrManager.SetpEffects(lpEffect);
    }

    // ---- [lionload] witness. NOT console behaviour: ours, bounded, log-only. ---------
    // ⭐ WHY A *SUCCESS* WITNESS. The sibling failure line (ParticleDescriptionResourceType::
    // DeSerialise's "[particles] BinLoad returned NULL") is one-shot and silent when nothing
    // goes wrong -- so its ABSENCE proves nothing: a build where the resource type was never
    // registered, and one where all 41 .lefs loaded perfectly, produce byte-identical logs.
    // This line is the discriminator, and it is deliberately made of the things that would be
    // WRONG if the record layout were wrong: the descriptor chain is walked to a count, and
    // the first descriptor's name and its material's texture name are printed -- both reached
    // through 32-bit slots at +0x38 and +0x4C/+0x10. A layout that had drifted would give a
    // garbage count or an unreadable name here rather than nothing at all.
    // DELETE-WHEN-STABLE.
    {
        static u32 suLoadWitness = 0;
        const u32 KU_LOAD_WITNESS_LIMIT = 6;
        if (suLoadWitness < KU_LOAD_WITNESS_LIMIT)
        {
            ++suLoadWitness;

            u32 luDescriptors = 0;
            const char* lpcFirstName = "<no descriptors>";
            const char* lpcFirstTex  = "<none>";
            if (lpEffect != 0)
            {
                for (cParticleDescriptor* lpDes = lpEffect->GetDescriptors(); lpDes != 0;
                     lpDes = lpDes->GetNextDescriptor())
                {
                    if (luDescriptors == 0)
                    {
                        if (lpDes->mpName)
                            lpcFirstName = lpDes->mpName.Get();
                        if (lpDes->mpMaterial && lpDes->mpMaterial->mpTextureName)
                            lpcFirstTex = lpDes->mpMaterial->mpTextureName.Get();
                    }
                    ++luDescriptors;
                }
            }

            // The .lef name is LionChar (UTF-16); narrow it for the log.
            char lacName[cLionEffectDefinition::KU_MAX_NAME_LENGTH + 1];
            u32 luChar = 0;
            for (; luChar < cLionEffectDefinition::KU_MAX_NAME_LENGTH; ++luChar)
            {
                const u16 lu16 = lpDefinition->m_name[luChar];
                if (lu16 == 0)
                    break;
                lacName[luChar] = (lu16 < 0x80u) ? static_cast<char>(lu16) : '?';
            }
            lacName[luChar] = 0;

            char lacMsg[512];
            std::snprintf(lacMsg, sizeof(lacMsg),
                "[lionload] #%u LOADED \"%s\" key=%08X def=%p effect=%p hash=%08X "
                "descriptors=%u first=\"%s\" firstTexture=\"%s\"\n",
                suLoadWitness, lacName, lpDefinition->Key(), lpDefinition, lpEffect,
                (lpEffect != 0) ? lpEffect->mHash : 0u,
                luDescriptors, lpcFirstName, lpcFirstTex);
            CgsDev::Log::WriteToLog(lacMsg);
        }
    }

    return lpDefinition;
}

// ----------------------------------------------------------------------------
// cLionFX::BinSave  @ 0x82914438 -- TRAP STUB (see the declaration's note).
// The console body allocates a cLionSerialiser sized by
// cLionParticleEffect::GetSerialiseSize, DataStores the 84-byte definition, serialises
// the effect graph into it, Remaps the copied pointers, Delocates the definition for the
// target endianness and hands the buffer to the stream callback. Three of those steps
// have no reconstructed body (cLionParticleEffect::Remap, cLionEffectDefinition::
// Delocate, cLionSerialiser::StringStore), so writing this one would mean inventing
// them. It is the SAVE path: the only caller is ParticleDescriptionResourceType::
// Serialise @0x8267C220, which the PC build never runs.
// ----------------------------------------------------------------------------
int cLionFX::BinSave(void* apData, int aiEndianTwiddleFlag, void* apStream)
{
    (void)apData;
    (void)aiEndianTwiddleFlag;
    (void)apStream;
    __debugbreak();
    return 0;
}
