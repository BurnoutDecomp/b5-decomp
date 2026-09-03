// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.cpp
//
// cLionFX::BinLoad @0x82914388 -- the LION effect binary loader, plus the two
// module-scope globals the runtime keeps here.
//
// WHY THIS TU EXISTS. Every BrnParticle::ParticleDescription resource in
// PARTICLES.BUNDLE carries a saved LION effect at +16, and
// ParticleDescriptionResourceType::DeSerialise @0x82675868 runs BinLoad over it in
// place. Until now that call went to a local `__debugbreak()` stub inside
// SharedClasses/Graphics/ParticleDescriptionResourceType.cpp, so the description's
// effect pointer was never rebased and BrnParticle::ParticleModule::StartLionEffect
// could not have resolved an effect even with a correct name.
// ============================================================================

#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionFX.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionEffect.h"
#include "SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/LionParticleEffect.h"
#include "GameSource/Effects/Particles/LionParticleRender.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"

#include <cstdint>
#include <cstdio>

// ----------------------------------------------------------------------------
// gpLionParticleEffectChain -- X360 dword_831237BC, the head of the singly-linked
// chain of every cLionParticleEffect the runtime has loaded. BinLoad pushes each
// newly built effect onto it through that effect's mpNext (`*(v5 + 8) =
// dword_831237BC; dword_831237BC = a1[18]`), which is why
// cLionParticleEffect::Delocate deliberately does NOT twiddle mpNext: it is runtime
// state, overwritten on every load.
//
// NAMING: the datum is unnamed in the X360 export and no DWARF global pins it, so
// the name is this project's, chosen from its one observed use. Its offset and role
// are asm facts; the spelling is not.
// ----------------------------------------------------------------------------
cLionParticleEffect* gpLionParticleEffectChain = 0;

// ----------------------------------------------------------------------------
// gpLionParticleRender -- X360 dword_83121D60, the Lion particle-render singleton.
// cLionFX::Init(allocator, iParticleRender*, ...) (DWARF LionFX.h:51) is its writer
// and cParticleMaterial::Build @0x8290E500 is the reader that matters here
// (`if (gpLionParticleRender) gpLionParticleRender->TextureRegister(this, name)`).
//
// ⛔ NOT RECONSTRUCTED: cLionFX::Init. This pointer therefore stays NULL for the whole
// run, so no material registers its texture with the Lion renderer. The console guards
// that call with the same null test, so the SHAPE here is faithful -- what is absent is
// the writer, and it is absent because the Lion render core it installs is absent.
// DELETE-WHEN cLionFX::Init lands.
// ----------------------------------------------------------------------------
BrnParticle::LionParticleRender* gpLionParticleRender = 0;

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

    cLionParticleEffect* lpEffect = lpDefinition->mpParticles.Get();
    if (lpEffect != 0)
    {
        lpEffect->mpNext.Set(gpLionParticleEffectChain);
        gpLionParticleEffectChain = lpEffect;
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
