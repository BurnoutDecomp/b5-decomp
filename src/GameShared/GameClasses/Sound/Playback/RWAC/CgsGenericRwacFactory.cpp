// ============================================================================
// CgsGenericRwacFactory.cpp
//
// CgsSound::Playback::RwacLock::RwacLock(System*) -- the RWAC scoped lock guard's
// constructor. Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826810F8.
//
// RwacLock is a stack RAII guard holding the RenderWare audio-core System locked for
// the lifetime of the guard. The constructor:
//   mpSystem = apSystem;                     // stw r4,0(r31)
//   if (!apSystem)                           // fall back to the default System
//   {
//       mpSystem = GetDefaultRwacSystem();   // lwz off_83271928
//       CGS_ASSERT(mpSystem != 0, "mpSystem"); // CgsGenericRwacFactory.h:59
//   }
//   rw::audio::core::System::Lock(mpSystem); // engine entry RwacSystemLock
//
// Called by GenericRwacFactory::GenericRwacFactory and ::AddRegistry.
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "rw/audio/core/PlugIn.h"   // the complete System (Lock/Unlock members; phase B5)

// The registration pass (AEMS-cascade wave): the vendor plug-in descriptor getters
// + the decoder registry, each header the authoritative vendor home.
#include "rw/rwcore_structs.h"                       // rw::IResourceAllocator / BaseResourceDescriptors (the carve)
#include "rw/audio/core/AiffWriter.h"
#include "rw/audio/core/Iir2Filters.h"               // BandPass/HighPass/HighShelf/LowPass/LowShelf/Peaking Iir2
#include "rw/audio/core/Gain.h"
#include "rw/audio/core/Rechannel.h"
#include "rw/audio/core/Resample.h"
#include "rw/audio/core/Send.h"
#include "rw/audio/core/plugins/HighPassButterworth.h"
#include "rw/audio/core/plugins/Limiter1.h"
#include "rw/audio/core/plugins/Pan2D.h"
#include "rw/audio/core/plugins/Pan2D1.h"
#include "rw/audio/core/plugins/Pause.h"
#include "rw/audio/core/plugins/ReverbModel1.h"
#include "rw/audio/core/DecoderRegistry.h"
#include "rw/audio/core/Pcm16BigDec.h"

#include <new>   // placement new (the in-carve construct)

// The process-wide System singleton the vendor System.cpp publishes from
// CreateInstance (extern "C" System *off_83271928).
extern "C" rw::audio::core::System* off_83271928;

namespace rw
{
namespace audio
{
namespace core
{
    // The two host-side engine-entry shims the sound Playback callers dispatch
    // through BY NAME (bodied 2026-08-25, faithful-audio-engine phase B5): each
    // is the corresponding System method, exactly what the console `bl
    // rw::audio::core::System::Lock/Unlock` sites do.
    void RwacSystemLock(System* apSystem)
    {
        System::Lock(apSystem);
    }
    void RwacSystemUnlock(System* apSystem)
    {
        System::Unlock(apSystem);
    }
} // namespace core
} // namespace audio
} // namespace rw

namespace CgsSound
{
namespace Playback
{
    // The process-wide default RWAC System accessor (bodied phase B5): the console
    // inlines the off_83271928 read + the "mpSystem" assert (CgsGenericRwacFactory.h:59)
    // at every consumer.
    rw::audio::core::System* GetDefaultRwacSystem()
    {
        CGS_ASSERT(off_83271928 != 0, "mpSystem");
        return off_83271928;
    }
} // namespace Playback
} // namespace CgsSound

namespace CgsSound
{
namespace Playback
{

RwacLock::RwacLock(rw::audio::core::System* apSystem)
    : mpSystem(apSystem)
{
    if (mpSystem == 0)
    {
        // Fall back to the process-wide default RWAC System (X360 off_83271928).
        mpSystem = GetDefaultRwacSystem();
        CGS_ASSERT(mpSystem != 0, "mpSystem");
    }

    // Hold the audio-core System locked for the guard's lifetime.
    rw::audio::core::RwacSystemLock(mpSystem);
}

// The guard's release -- the console @0x826C17A0 ctor tail reloads the saved
// System pointer and calls System::Unlock @0x82B6BCF0 (the optimized body inlines
// the dtor; the RAII form here is the same unlock on scope exit).
RwacLock::~RwacLock()
{
    rw::audio::core::RwacSystemUnlock(mpSystem);
}

// ===========================================================================
// GenericRwacFactory  (AEMS-cascade wave 2026-08-28; register-level decode:
// progress/scratch_dossiers/aems_factory_cascade_codex.md, RWAC section)
// ===========================================================================

namespace
{
    // The interned factory name (console dword_83008650, written at static-init by
    // sub_82C654A8 = Name::MakeHash("~GenericRwacFactory::SK_NAME~")). Interned once
    // here at static-init, exactly as the console does; GenericRwacFactorySkName()
    // (BrnBaselineLinkStubs.cpp) interns the same literal -- one hash, one value.
    const Name skRwacFactoryName("~GenericRwacFactory::SK_NAME~");

    // The console's `rw::IResourceAllocator::AllocateMemoryResource` five-pair
    // descriptor inline -- (bytes, align) + four (0, 1) pairs through the
    // allocator's DoAllocate with the tag riding second (the identical idiom the
    // Module Prepare TU carries; TU-local copy, the established convention).
    void* AllocateMemoryResource(rw::IResourceAllocator* lpAllocator, size_t luSize,
                                 u32 luAlignment, const char* lpcName)
    {
        rw::BaseResourceDescriptors<5> lDescriptor;
        for (u32 luEntry = 0u; luEntry < 5u; ++luEntry)
        {
            lDescriptor.m_baseResourceDescriptors[luEntry].m_size      = 0u;
            lDescriptor.m_baseResourceDescriptors[luEntry].m_alignment = 1u;
        }
        lDescriptor.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(luSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = luAlignment;

        rw::Resource lResource = lpAllocator->DoAllocate(
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor), lpcName);
        return lResource.m_baseResources[0];
    }
}

// @ 0x826C7AD0. The console lowers the 16-byte spec BY VALUE in r5:r6 (unlike the
// AEMS/Splicer ref-spec creates); the hidden return-storage handle receives the
// object pointer + one explicit refcount increment (+0x04 -- the Factory-at-offset-
// zero Object count).
Handle<GenericRwacFactory> GenericRwacFactory::Create(Environment& arEnvironment,
                                                      GenericRwacFactorySpec aSpec)
{
    // Null-system resolution: off_83271928, asserted (console "lSpec.mpSystem",
    // CgsGenericRwacFactory.h:906).
    if (aSpec.mpSystem == 0)
    {
        aSpec.mpSystem = GetDefaultRwacSystem();
        CGS_ASSERT(aSpec.mpSystem != 0, "lSpec.mpSystem");
    }

    // Carve size: console `4 * (entityCount + 0x100F) + dataBytes + stringBytes`
    // (the 0x100F-word term == the 0x403C fixed head: class through registry
    // header). Host: the same regions at host widths -- the Environment
    // operator-new precedent.
    const size_t luBytes = sizeof(GenericRwacFactory)
                         + sizeof(Registry)
                         + sizeof(void*) * aSpec.mu32EntityCount
                         + aSpec.mu32DataSize
                         + aSpec.mu32StringTableSize;

    // The five-pair request through the ENVIRONMENT's allocator (console
    // Environment+0x30, vtable +0x10), tag "GenericRwacFactory".
    void* lpMemory = AllocateMemoryResource(arEnvironment.GetAllocator(), luBytes,
                                            4, "GenericRwacFactory");
    if (lpMemory == 0)
    {
        return Handle<GenericRwacFactory>();
    }

    GenericRwacFactory* lpFactory =
        ::new (lpMemory) GenericRwacFactory(arEnvironment, aSpec);

    // The returned handle's owned ref (console: `stw ptr, 0(r29)` + the +0x04
    // increment). Interim plain-store Handle model: the explicit Acquire beside
    // the store IS the handle's ref.
    lpFactory->Acquire();
    return Handle<GenericRwacFactory>(lpFactory);
}

// @ 0x826C17A0. Store order per the decode: base Factory (name = the intern
// above), the final vtable (host: implicit), mpSystem, the two command-queue
// control words, the registry pointer, the in-place Registry -- then, under the
// RwacLock guard, the complete plug-in/decoder registration pass.
GenericRwacFactory::GenericRwacFactory(Environment& arEnvironment,
                                       const GenericRwacFactorySpec& akrSpec)
    : Factory(skRwacFactoryName, arEnvironment)
{
    mpSystem = akrSpec.mpSystem;
    mu32CommandQueueWriteCursor = 0;   // console +0x4014
    mu32CommandQueueReadCursor  = 0;   // console +0x4018
    // (the +0x14..+0x4013 queue payload is NOT ctor-touched -- decode-attested)

    // The in-place Registry immediately after the fixed head (console +0x4020).
    RegistrySpec lRegistrySpec;
    lRegistrySpec.mu32EntityCount   = akrSpec.mu32EntityCount;
    lRegistrySpec.muDataSize        = akrSpec.mu32DataSize;
    lRegistrySpec.muStringTableSize = akrSpec.mu32StringTableSize;
    mpRegistry = ::new (reinterpret_cast<u8*>(this) + sizeof(GenericRwacFactory))
        Registry(lRegistrySpec);

    // ---- the registration pass, under the console's RwacLock stack guard --------
    {
        RwacLock lLock(mpSystem);

        typedef rw::audio::core::PlugInRegistry     PlugInRegistry;
        typedef rw::audio::core::PlugInDescRunTime  PlugInDescRunTime;
        typedef rw::audio::core::DecoderRegistry    DecoderRegistry;

        PlugInRegistry* lpPlugInRegistry =
            rw::audio::core::System::GetPlugInRegistry(mpSystem);
        (void)lpPlugInRegistry;

        // FLAG [the descriptor-record deferral -- the WHOLE plug-in pass]: the
        // console registers 25 PlugInDescRunTime records here, in this exact
        // order (each getter address below). It CANNOT run yet: every vendor
        // getter on this build returns a PLACEHOLDER (&g_XxxDesc -- a single
        // static pointer standing in for the un-recovered console record), and
        // RegisterPlugInRunTime writes the record's mpNext/muId link fields ~40
        // bytes in -- registering a placeholder SCRIBBLES the neighbouring
        // globals (measured: the first live pass corrupted the loading-stage
        // table + the menu-music stream headers; boot log "loading stage
        // <garbage>"). The pass lands when the PlugInDescRunTime field layout is
        // decoded (from PlugIn::CreateInstance / Voice::CreateInstance's reads)
        // and each plugin's record is BUILT with host callback pointers --
        // recovered rodata alone cannot carry the console's function pointers.
        // Console order, addresses:
        //   1  AiffWriter          @0x82B968B0    2  BandPassIir2  @0x82B96A40
        //   3  Dac                 @0x82B96DB8    4  Gain          @0x82B97350
        //   5  GainFader           @0x82B97368    6  HighPassIir2  @0x82B978B0
        //   7  HighPassButterworth @0x82B976D0    8  HighShelfIir2 @0x82B97978
        //   9  Limiter1            @0x82B97AA0   10  LowPassIir2   @0x82B97DB0
        //  11  LowPassButterworth  @0x82B97BF0   12  LowShelfIir2  @0x82B97E70
        //  13  Pan2D               @0x82B984E8   14  Pan2D1        @0x82B98748
        //  15  Pause               @0x82B9A130   16  PeakingIir2   @0x82B9A460
        //  17  Rechannel           @0x82B9A718   18  Resample      @0x82B9A850
        //  19  ReverbModel1        @0x82B9AD98   20  Send          @0x82B9B798
        //  21  SndPlayer1          @0x82B9BE60   22  SubMix        @0x82B9C370
        //  23  "GinsuPlayer" custom off_82F2D094  24  "SndPlayer1_CgsStreamMod"
        //  custom off_82F2E124                    25  "GainArray" custom off_82F2E664
        // An empty registry keeps GetPlugInHandle returning null, so every
        // voice-create path fails through its guarded callback -- the honest
        // degraded state.

        // The decoder pass: the standard runtime set (Xas1 -> Xas -> EaXma, inside
        // RegisterStandardRunTimeDecoders @0x82B6B538) + Pcm16Big registered
        // directly (@0x82B91E38), through the lazily-created decoder registry
        // (@0x82B6DD78).
        DecoderRegistry* lpDecoderRegistry =
            rw::audio::core::System::GetDecoderRegistry(mpSystem);
        DecoderRegistry::RegisterStandardRunTimeDecoders(lpDecoderRegistry);
        DecoderRegistry::RegisterDecoder(lpDecoderRegistry,
                                         rw::audio::core::Pcm16BigDec::GetDecoderDesc());
    }   // ~RwacLock == System::Unlock (the console tail)
}

// The per-factory registry accessor (CgsSoundPlaybackModule.h:99 -- the console
// reads GenericRwacFactory+0x401C). REAL now; the BrnBaselineLinkStubs null shim
// is retired with this body.
Registry* GetRwacFactoryRegistry(Factory* lpRwacFactory)
{
    return static_cast<GenericRwacFactory*>(lpRwacFactory)->GetRegistry();
}

} // namespace Playback
} // namespace CgsSound
