// ============================================================================
// GameSource/Effects/Particles/BrnParticleResourceAccessors.cpp
//
// The "BrnParticle facade" TU: the out-of-line template instantiations the X360
// ARTIST build emitted for the particle subsystem's resource-handle accessors and
// its one IO-helper constructor. None of these have hand-written source — they are
// pure compiler-generated template members the linker pulled out of line because
// they were used from more than one particle TU.
//
// X360 -> instantiation map (from the per-address IDA export names + the DecFIGS
// dwarfdump of CgsResourceHandle.h / CgsModuleIOHelper.h):
//
//   0x82286548  CgsResource::SafeResourceHandle<BrnParticle::BrnVFXMeshCollection>
//               conversion-to-pointer accessor  (baked CgsResourceHandle.h:505,
//               "Can not get resource pointer ...");
//               caller BrnParticle::Native::BrnDebrisRenderer::RenderDebrisArray.
//   0x822864A0  SafeResourceHandle<BrnVFXMeshCollection>::operator->() const
//               (baked :419, "Can not instance resource pointer ...");
//               caller BrnParticle::ParticleModule::LoadFXBundle.
//   0x822867E0  SafeResourceHandle<ParticleDescriptionCollection>::operator->()
//               (baked :403, "Can not instance resource pointer ...");
//               callers ParticleModule::StartLionEffect / ResumePlayingEffects.
//   0x822865F0  SafeResourceHandle<TextureNameMap>::operator->() const
//               (baked :419, "Can not instance resource pointer ...");
//               caller BrnParticle::LionParticleRender::FindTexture.
//   0x82286698  SafeResourceHandle<TextureNameMap>::operator->()
//               (baked :403, "Can not instance resource pointer ...");
//               caller BrnParticle::ParticleModule::LoadFXBundle.
//   0x82295FD0  CgsModule::IOHelper<BrnParticle::ParticleIO::PrepareOutputBuffer>
//               ::IOHelper(IOBufferStack*, const char*)  (baked
//               CgsModuleIOHelper.h:52, "mpStack->CreateIOBuffer( &mpBuffer,
//               lpcName )"); caller BrnEffects::EffectsModule::Prepare ("Particles").
//
// All accessor bodies live in CgsResourceHandle.h (the DWARF home, grown here for the
// templated SafeResourceHandle<T>). The IOHelper ctor body lives in CgsModuleIOHelper.h.
// This TU only forces the instantiations; explicit `template struct ...;` lines emit
// every member of each SafeResourceHandle<T> (which covers the operator-> and
// conversion symbols above for each T). The IOHelper ctor is forced on its own so the
// matching DestroyIOBuffer pop (emitted out-of-line at each EffectsModule use site)
// is not duplicated here.
// ============================================================================

#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"   // SafeResourceHandle<T>
#include "GameShared/GameClasses/Module/CgsModuleIOHelper.h"            // CgsModule::IOHelper<T>
#include "SharedClasses/Graphics/TextureNameMapResourceType.h"          // BrnParticle::TextureNameMap (complete)
#include "GameSource/Effects/Particles/ParticleModuleIO.h"             // BrnParticle::ParticleIO::PrepareOutputBuffer (complete)

namespace BrnParticle
{
    // HONEST PLACEHOLDERS: the two collection resource types are not committed in the
    // tree yet (their full layouts are deferred to BrnVFXMeshCollectionResourceType.* /
    // ParticleDescriptionResourceType.*). The SafeResourceHandle<T> accessors only
    // reinterpret_cast pointers-to-pointers and never touch a T member, so an incomplete
    // (forward-declared) T is sufficient to instantiate them. These are real DWARF type
    // names (BrnVFXMeshCollectionResourceType.h / the SafeResourceHandle<...> template
    // arg in CgsResourceHandle.h) — not fabricated. GROW into a real home when those TUs
    // land; do NOT define a layout here.
    struct BrnVFXMeshCollection;
    struct ParticleDescriptionCollection;
}

// ---- SafeResourceHandle<T> accessor instantiations (X360 0x82286548 / 0x822864A0 /
//      0x822867E0 / 0x822865F0 / 0x82286698) -----------------------------------------
template struct CgsResource::SafeResourceHandle<BrnParticle::BrnVFXMeshCollection>;
template struct CgsResource::SafeResourceHandle<BrnParticle::ParticleDescriptionCollection>;
template struct CgsResource::SafeResourceHandle<BrnParticle::TextureNameMap>;

// ---- IOHelper<PrepareOutputBuffer> constructor (X360 0x82295FD0) ------------------
// Force just the ctor (the bodied function for this TU); the dtor's DestroyIOBuffer pop
// is emitted out-of-line by EffectsModule::Prepare and is not this TU's symbol.
template
CgsModule::IOHelper<BrnParticle::ParticleIO::PrepareOutputBuffer>::IOHelper(
    CgsModule::IOBufferStack* lpStack, const char* lpcName);
