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
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacContent.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoiceConfig.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacPlayerVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacSubmixVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacMasterVoice.h"
#include "GameShared/GameClasses/Sound/Playback/CgsDataStructures.h"
#include "GameShared/GameClasses/Sound/Playback/CgsVoice.h"
#include "GameShared/GameClasses/System/PC/CgsDacOutputPC.h"

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
#include "rw/audio/core/SubMix.h"
#include "rw/audio/core/plugins/GainFader.h"
#include "rw/audio/core/plugins/HighPassButterworth.h"
#include "rw/audio/core/plugins/Limiter1.h"
#include "rw/audio/core/plugins/LowPassButterworth.h"
#include "rw/audio/core/plugins/Pan2D.h"
#include "rw/audio/core/plugins/Pan2D1.h"
#include "rw/audio/core/plugins/Pause.h"
#include "rw/audio/core/plugins/ReverbModel1.h"
#include "rw/audio/core/DecoderRegistry.h"
#include "rw/audio/core/Pcm16BigDec.h"
#include "rw/audio/core/plugins/Dac.h"               // the output plug-in (phase D)
#include "rw/audio/core/plugins/SndPlayer1.h"
#include "rw/audio/core/Voice.h"
#include "rw/audio/core/SubMix.h"
#include "GameShared/GameClasses/Sound/Playback/Plugins/GainArray/CgsGainArrayPlugin.h" // the game-side 'JGA0'
#include "GameShared/GameClasses/Sound/Playback/Plugins/Ginsu/GinsuPlayer.h"            // the game-side 'Gns0'
#include "GameShared/GameClasses/Sound/Playback/Plugins/Streaming/internal/sndplayer1shared.h"

#include <cstdint> // intptr_t (the RwacPlugInEvent r5 ride-through)
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
    // The 3-arg engine event entry (bodied with the phase-D Dac slice 2026-08-28;
    // the BrnBaselineLinkStubs placeholder is retired): the console callers
    // (Environment::StartDac @0x82680F50 / StopDac @0x82680FE8) `bl
    // rw::audio::core::PlugIn::Event` -- the pass-through tail-vcall into the
    // plug-in's vt[1] (the Dac's EventEvent). The int arg rides as the console r5
    // (a param-block pointer for events 0..2; 0 for start/stop).
    void RwacPlugInEvent(PlugIn* apPlugIn, int aiEvent, int aiArg)
    {
        PlugIn::Event(apPlugIn, aiEvent,
                      reinterpret_cast<void*>(static_cast<intptr_t>(aiArg)));
    }
} // namespace core
} // namespace audio
} // namespace rw

namespace CgsSound
{
namespace Playback
{

const Name GenericRwacFeatureImplementation::SK_TYPE_NAME(
    "~GenericRwacFeatureImplementation~");

namespace
{
    const EntityFixer<GenericRwacFeatureImplementation>
        sGenericRwacFeatureImplementationFixer;
}

void GenericRwacFeatureImplementation::ResolvePluginInfoHandle(
    u32 au32Index, rw::audio::core::PlugInRegistry* apRegistry) const
{
    PluginInfo* lpInfo = GetPluginInfoAddress(au32Index);
    lpInfo->mHandle = rw::audio::core::PlugInRegistry::GetPlugInHandle(
        apRegistry, static_cast<int>(lpInfo->mGuid));
}
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
    const Name skWaveContentType(
        "~GenericRwacWaveContent::SK_WAVE_DATA_CONTENT_TYPE~");
    const Name skReverbIrContentType(
        "~GenericRwacReverbIRContent::SK_REVERB_IR_DATA_CONTENT_TYPE~");
    const Name skInternalSubmixFeature("~InternalSubmixFeature~");
    const Name skInternalSendFeature("~InternalSendFeature~");
    const Name skInternalDacFeature("~InternalDacFeature~");

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
    : Factory(skRwacFactoryName, arEnvironment),
      mpSystem(akrSpec.mpSystem),
      mCommandQueue(),
      mpRegistry(0)
{
    mCommandQueue.mu32Write = 0;
    mCommandQueue.mu32Read  = 0;
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

        // The 25 RegisterPlugInRunTime calls in exact console order. Every descriptor
        // and every callback it publishes is now live on the host.
        #define CGS_RWAC_REGISTER(GETTER) \
            PlugInRegistry::RegisterPlugInRunTime(lpPlugInRegistry, \
                reinterpret_cast<PlugInDescRunTime*>(GETTER))
        CGS_RWAC_REGISTER(rw::audio::core::AiffWriter::GetPlugInDescRunTime());          // 1  @0x82B968B0
        CGS_RWAC_REGISTER(rw::audio::core::BandPassIir2::GetPlugInDescRunTime());        // 2  @0x82B96A40
        CGS_RWAC_REGISTER(rw::audio::core::Dac::GetPlugInDescRunTime());                // 3  @0x82B96DB8 (phase D)
        CGS_RWAC_REGISTER(rw::audio::core::Gain::GetPlugInDescRunTime());                // 4  @0x82B97350
        CGS_RWAC_REGISTER(rw::audio::core::GainFader::GetPlugInDescRunTime());           // 5  @0x82B97368 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::HighPassIir2::GetPlugInDescRunTime());        // 6  @0x82B978B0 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::HighPassButterworth::GetPlugInDescRunTime()); // 7  @0x82B976D0
        CGS_RWAC_REGISTER(rw::audio::core::HighShelfIir2::GetPlugInDescRunTime());       // 8  @0x82B97978
        CGS_RWAC_REGISTER(rw::audio::core::Limiter1::GetPlugInDescRunTime());            // 9  @0x82B97AA0 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::LowPassIir2::GetPlugInDescRunTime());         // 10 @0x82B97DB0
        CGS_RWAC_REGISTER(rw::audio::core::LowPassButterworth::GetPlugInDescRunTime());  // 11 @0x82B97BF0 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::LowShelfIir2::GetPlugInDescRunTime());        // 12 @0x82B97E70
        CGS_RWAC_REGISTER(rw::audio::core::Pan2D::GetPlugInDescRunTime());               // 13 @0x82B984E8
        CGS_RWAC_REGISTER(rw::audio::core::Pan2D1::GetPlugInDescRunTime());              // 14 @0x82B98748 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::Pause::GetPlugInDescRunTime());               // 15 @0x82B9A130 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::PeakingIir2::GetPlugInDescRunTime());         // 16 @0x82B9A460
        CGS_RWAC_REGISTER(rw::audio::core::Rechannel::GetPlugInDescRunTime());           // 17 @0x82B9A718
        CGS_RWAC_REGISTER(rw::audio::core::Resample::GetPlugInDescRunTime());            // 18 @0x82B9A850 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::ReverbModel1::GetPlugInDescRunTime());        // 19 @0x82B9AD98
        CGS_RWAC_REGISTER(rw::audio::core::Send::GetPlugInDescRunTime());                // 20 @0x82B9B798
        CGS_RWAC_REGISTER(rw::audio::core::SndPlayer1::GetPlugInDescRunTime());          // 21 @0x82B9BE60
        CGS_RWAC_REGISTER(rw::audio::core::SubMix::GetPlugInDescRunTime());              // 22 @0x82B9C370 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::GinsuPlayer::GetPlugInDescRunTime());         // 23 off_82F2D094 (phase E)
        CGS_RWAC_REGISTER(rw::audio::core::SndPlayer1_CgsStreamMod::GetPlugInDescRunTime()); // 24 off_82F2E124
        CGS_RWAC_REGISTER(CgsSound::Playback::Plugins::GainArray::GetPlugInDescRunTime()); // 25 off_82F2E664 (phase E)
        #undef CGS_RWAC_REGISTER

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

void GenericRwacFactory::AddRegistry(Registry& arRegistry)
{
    RwacLock lLock(mpSystem);
    rw::audio::core::PlugInRegistry* lpPlugInRegistry =
        rw::audio::core::System::GetPlugInRegistry(mpSystem);

    for (u32 luI = 0; luI < arRegistry.GetEntityCapacity(); ++luI)
    {
        const Entity* lpEntity = arRegistry.GetEntityAtSlot(luI);
        if (lpEntity == 0 ||
            lpEntity->mTypeName != GenericRwacFeatureImplementation::SK_TYPE_NAME)
            continue;

        const GenericRwacFeatureImplementation* lpFeature =
            static_cast<const GenericRwacFeatureImplementation*>(lpEntity);
        for (u32 luPlugin = 0; luPlugin < lpFeature->GetPluginInfoCount(); ++luPlugin)
            lpFeature->ResolvePluginInfoHandle(luPlugin, lpPlugInRegistry);
    }

    *mpRegistry += arRegistry;
}

const GenericRwacFeatureImplementation&
GenericRwacFactory::GetFeatureImplementation(Name aName) const
{
    const GenericRwacFeatureImplementation* lpImplementation =
        mpRegistry->GetEntity<GenericRwacFeatureImplementation>(aName);
    CGS_ASSERT(lpImplementation != 0, "lpFeatureImplementation");
    return *lpImplementation;
}

GenericRwacVoiceConfig* GenericRwacFactory::SetupConfig(
    const VoiceSpec& akrSpec, Voice& arBaseVoice, GenericRwacVoice& arVoiceOut)
{
    GenericRwacVoiceConfig* lpConfig =
        new (mEnvironment) GenericRwacVoiceConfig(mEnvironment);
    if (lpConfig == 0)
        return 0;

    const EVoiceType leVoiceType = static_cast<EVoiceType>(akrSpec.mu8VoiceType);
    lpConfig->SetVoiceType(leVoiceType);
    switch (leVoiceType)
    {
    case E_PLAYER_VOICE:
        lpConfig->SetProcessingStage(0);
        break;
    case E_SUBMIX_VOICE:
        lpConfig->SetProcessingStage(akrSpec.mu8ProcessingStage);
        break;
    case E_MASTER_VOICE:
        lpConfig->SetProcessingStage(255);
        break;
    default:
        CGS_ASSERT(false, "Invalid Voice Type");
        break;
    }

    u32 luCurrentPlugin = 0;
    u32 luFeaturePluginBase = 0;
    u8 luChannels = akrSpec.mu8ChannelCount;

    if (leVoiceType != E_PLAYER_VOICE)
    {
        void* lpContext = 0;
        if (leVoiceType == E_MASTER_VOICE)
        {
            rw::audio::core::SubMix::ConstructorParams* lpParams =
                static_cast<rw::audio::core::SubMix::ConstructorParams*>(
                    lpConfig->GetScratchpad().Allocate(
                        sizeof(rw::audio::core::SubMix::ConstructorParams)));
            CGS_ASSERT(lpParams != 0, "lpParams");
            lpParams->pName = "Master";
            lpContext = lpParams;
        }

        const GenericRwacFeatureImplementation& lrSubmix =
            GetFeatureImplementation(skInternalSubmixFeature);
        for (u32 luPlugin = 0; luPlugin < lrSubmix.GetPluginInfoCount(); ++luPlugin)
        {
            lpConfig->SetConfig(luCurrentPlugin++,
                lrSubmix.GetPluginInfoHandle(luPlugin), luChannels, lpContext);
        }
        luFeaturePluginBase = 1;
    }

    const VoiceSchema& lrVoiceSchema = akrSpec.GetVoiceSchema();
    for (u32 luFeature = 0; luFeature < lrVoiceSchema.GetFeatureSchemaCount();
         ++luFeature)
    {
        const FeatureSchema& lrSchema = lrVoiceSchema.GetFeatureSchema(luFeature);
        const GenericRwacFeatureImplementation& lrImplementation =
            GetFeatureImplementation(lrSchema.mName);
        const u32 luImplementationBase = luCurrentPlugin;

        for (u32 luPlugin = 0;
             luPlugin < lrImplementation.GetPluginInfoCount(); ++luPlugin)
        {
            u8 luOutputChannels = static_cast<u8>(
                lrImplementation.GetPluginInfoOutputChannels(luPlugin));
            if (luOutputChannels == 0)
            {
                CGS_ASSERT(luChannels > 0, "lu8Channels > 0");
                luOutputChannels = luChannels;
            }
            lpConfig->SetConfig(luCurrentPlugin++,
                lrImplementation.GetPluginInfoHandle(luPlugin),
                luOutputChannels, 0);
            luChannels = luOutputChannels;
        }

        for (u32 luSlotMap = 0;
             luSlotMap < lrImplementation.GetSlotMapCount(); ++luSlotMap)
        {
            const u32 luSlotIndex = arBaseVoice.GetIndexOfSlot(
                lrImplementation.GetSlotMapName(luSlotMap));
            if (luSlotIndex != static_cast<u32>(-1))
            {
                Slot& lrSlot = arBaseVoice.GetSlot(luSlotIndex);
                lrSlot.SetPluginOffset(luFeaturePluginBase +
                    lrImplementation.GetSlotMapOffset(luSlotMap));

                const Name lRuntimeClass =
                    lrImplementation.GetSlotMapRuntimeClass(luSlotMap);
                const ISlotFactory* lpSlotFactory =
                    ISlotFactory::GetFactory(lRuntimeClass);
                CGS_ASSERT(lpSlotFactory != 0,
                           "Could not find Slot Factory with requested name");
                if (lpSlotFactory)
                    lrSlot.SetImplementation(
                        lpSlotFactory->DoCreateSlot(arBaseVoice));
            }
        }

        for (u32 luParameterMap = 0;
             luParameterMap < lrImplementation.GetParameterMapCount();
             ++luParameterMap)
        {
            const Name lName =
                lrImplementation.GetParameterMapName(luParameterMap);
            const u32 luInput = arBaseVoice.GetIndexOfInputParameter(lName);
            if (luInput != static_cast<u32>(-1))
            {
                arVoiceOut.AddParameterMap(
                    static_cast<u8>(luInput),
                    static_cast<u8>(luImplementationBase +
                        lrImplementation.GetParameterMapOffset(luParameterMap)),
                    static_cast<u8>(
                        lrImplementation.GetParameterMapAttribute(luParameterMap)),
                    E_PARAMETER_INPUT);
                continue;
            }

            const u32 luOutput = arBaseVoice.GetIndexOfOutputParameter(lName);
            if (luOutput != static_cast<u32>(-1))
            {
                arVoiceOut.AddParameterMap(
                    static_cast<u8>(luOutput),
                    static_cast<u8>(luImplementationBase +
                        lrImplementation.GetParameterMapOffset(luParameterMap)),
                    static_cast<u8>(
                        lrImplementation.GetParameterMapAttribute(luParameterMap)),
                    E_PARAMETER_OUTPUT);
            }
        }

        luFeaturePluginBase += lrImplementation.GetPluginInfoCount();
    }

    if (leVoiceType == E_MASTER_VOICE)
    {
        lpConfig->SetFirstSendPlugin(0);
        const GenericRwacFeatureImplementation& lrDac =
            GetFeatureImplementation(skInternalDacFeature);
        for (u32 luPlugin = 0; luPlugin < lrDac.GetPluginInfoCount(); ++luPlugin)
        {
            lpConfig->SetConfig(luCurrentPlugin++,
                lrDac.GetPluginInfoHandle(luPlugin), 0, 0);
        }
    }
    else
    {
        lpConfig->SetFirstSendPlugin(luCurrentPlugin);
        const GenericRwacFeatureImplementation& lrSend =
            GetFeatureImplementation(skInternalSendFeature);
        for (u32 luSend = 0; luSend < akrSpec.GetSendCount(); ++luSend)
        {
            for (u32 luPlugin = 0; luPlugin < lrSend.GetPluginInfoCount(); ++luPlugin)
            {
                lpConfig->SetConfig(luCurrentPlugin++,
                    lrSend.GetPluginInfoHandle(luPlugin), luChannels, 0);
            }
        }
    }

    lpConfig->SetPluginCount(luCurrentPlugin);
    return lpConfig;
}

bool GenericRwacFactory::DoCreateVoice(const VoiceSpec& akrSpec,
                                       Handle<Voice>& arHandleOut,
                                       u32 au32Ident)
{
    arHandleOut.SetObject(0);
    Voice* lpVoice = 0;
    GenericRwacVoice* lpRwacVoice = 0;
    rw::audio::core::PlugIn** lppSubmix = 0;

    switch (static_cast<EVoiceType>(akrSpec.mu8VoiceType))
    {
    case E_PLAYER_VOICE:
    {
        GenericRwacPlayerVoice* lpPlayer = new (*this, akrSpec)
            GenericRwacPlayerVoice(*this, akrSpec, au32Ident);
        lpVoice = lpPlayer;
        lpRwacVoice = lpPlayer;
        break;
    }
    case E_SUBMIX_VOICE:
    {
        GenericRwacSubmixVoice* lpSubmix = new (*this, akrSpec)
            GenericRwacSubmixVoice(*this, akrSpec, au32Ident);
        lpVoice = lpSubmix;
        lpRwacVoice = lpSubmix;
        lppSubmix = lpSubmix->GetSubmixAddress();
        break;
    }
    case E_MASTER_VOICE:
    {
        GenericRwacMasterVoice* lpMaster = new (*this, akrSpec)
            GenericRwacMasterVoice(*this, akrSpec, au32Ident);
        lpVoice = lpMaster;
        lpRwacVoice = lpMaster;
        lppSubmix = lpMaster->GetSubmixAddress();
        break;
    }
    default:
        CGS_ASSERT(false, "Invalid Voice Type");
        return false;
    }

    if (lpVoice == 0 || lpRwacVoice == 0 ||
        !lpRwacVoice->CreateVoiceInstance(akrSpec, *lpVoice, *this, lppSubmix))
        return false;

    lpVoice->Acquire();
    arHandleOut.SetObject(lpVoice);
    return true;
}

// ARTIST @0x826D8748. A plug-in event command optionally consumes the next
// queued parameter command and translates the compact cross-thread payload into
// the concrete RWAC event record. Player-play is the only multi-field record;
// the send/reverb/ginsu parameter records each contain one pointer.
void GenericRwacFactory::HandlePluginEvent(
    u32 au32CommandCount, const uintptr_t* apuCommandWords)
{
    CGS_ASSERT(au32CommandCount == 4u,
               "Plugin-event command word count");
    const RwacCommandPluginEvent& lrEvent =
        *reinterpret_cast<const RwacCommandPluginEvent*>(apuCommandWords);
    rw::audio::core::PlugIn* lpPlugin =
        reinterpret_cast<rw::audio::core::PlugIn*>(lrEvent.mpPlugin);
    const int liEvent = static_cast<int>(lrEvent.maOperand1);
    if (lrEvent.maOperand2 == 0u)
    {
        rw::audio::core::PlugIn::Event(lpPlugin, liEvent, 0);
        return;
    }

    CGS_ASSERT(!mCommandQueue.IsEmpty(),
               "Parameterized plug-in event has no parameter command");
    uintptr_t luParamCount = 0;
    mCommandQueue.GetCommand(&luParamCount);
    CGS_ASSERT(luParamCount > 0u && luParamCount <= 16u,
               "luCommandCount > 0 && luCommandCount <= 16");

    uintptr_t lauParamWords[16] = {};
    for (u32 luWord = 0; luWord < static_cast<u32>(luParamCount); ++luWord)
        mCommandQueue.GetCommand(&lauParamWords[luWord]);

    switch (static_cast<ERwacCommandType>(lauParamWords[0]))
    {
    case E_RWAC_COMMAND_PLAYER_PLAY_PARAMETERS:
    {
        RwacCommandPlayerPlayParameters lParameters(
            static_cast<u32>(luParamCount),
            *reinterpret_cast<const RwacCommandPlayerPlayParameters*>(
                lauParamWords));
        CGS_ASSERT(lParameters.mpRequestHandle != 0,
                   "lParameters.GetRequestHandleAddress()");
        CGS_ASSERT(lParameters.mpWaveContent != 0, "mpObject");

        char lacStreamPath[128] = {};
        const char* lpcStreamPath = 0;
        if (lParameters.mpWaveContent->GetStreamPath(
                lacStreamPath, sizeof(lacStreamPath)))
            lpcStreamPath = lacStreamPath;

        rw::audio::core::SndPlayer1::PlayParams lPlayParams = {};
        lPlayParams.startTime = 0.0;
        lPlayParams.streamFileOffset = 0.0;
        lPlayParams.pStreamFilePath = lpcStreamPath;
        lPlayParams.pRamData = lParameters.mpWaveContent->GetData(
            E_CONTENT_STATE_LOADED);
        lPlayParams.streamPoolGuid = 0u;
        lPlayParams.expelMode = 0.0f;
        lPlayParams.requestHandle = 0.0f;

        rw::audio::core::PlugIn::Event(lpPlugin, liEvent, &lPlayParams);
        *lParameters.mpRequestHandle = lPlayParams.requestHandle;
        break;
    }
    case E_RWAC_COMMAND_PLAYER_IS_REQUEST_DONE_PARAMETERS:
    {
        RwacCommandPlayerIsRequestDoneParameters lParameters(
            static_cast<u32>(luParamCount),
            *reinterpret_cast<const RwacCommandPlayerIsRequestDoneParameters*>(
                lauParamWords));
        rw::audio::core::PlugIn::Event(lpPlugin, liEvent,
                                       lParameters.mpParams);
        break;
    }
    case E_RWAC_COMMAND_SEND_CONNECT_PARAMETERS:
    case E_RWAC_COMMAND_REVERBIR_APPLY_IR_DATA:
    case E_RWAC_COMMAND_GINSU_ATTACH_DATA_PARAMETERS:
    {
        // Each source record is a one-pointer parameter struct. The compact
        // command stores that pointer; the engine event receives its address.
        CGS_ASSERT(luParamCount == 2u,
                   "Single-pointer parameter command word count");
        void* lpParameter = reinterpret_cast<void*>(lauParamWords[1]);
        rw::audio::core::PlugIn::Event(lpPlugin, liEvent, &lpParameter);
        break;
    }
    default:
        CGS_ASSERT(false, "Invalid Command");
        break;
    }
}

void GenericRwacFactory::DoUpdate(f32 /*af32DeltaTime*/)
{
    while (!mCommandQueue.IsEmpty())
    {
        uintptr_t luWordCount = 0;
        mCommandQueue.GetCommand(&luWordCount);
        CGS_ASSERT(luWordCount > 0 && luWordCount <= 16,
                   "luCommandCount > 0 && luCommandCount <= 16");
        uintptr_t lauWords[16] = {};
        for (u32 luWord = 0; luWord < static_cast<u32>(luWordCount); ++luWord)
            mCommandQueue.GetCommand(&lauWords[luWord]);

        switch (static_cast<ERwacCommandType>(lauWords[0]))
        {
        case E_RWAC_COMMAND_VOICE_CREATE_INSTANCE:
        {
            CGS_ASSERT(luWordCount == 6, "Voice-create command word count");
            RwacCommandVoiceCreateInstance* lpCommand =
                reinterpret_cast<RwacCommandVoiceCreateInstance*>(lauWords);
            Voice* lpPlaybackVoice =
                reinterpret_cast<Voice*>(lpCommand->mpPlaybackVoice);
            rw::audio::core::Voice** lppRwacVoice =
                reinterpret_cast<rw::audio::core::Voice**>(lpCommand->mppVoice);
            rw::audio::core::PlugIn*** lpppPlugin =
                reinterpret_cast<rw::audio::core::PlugIn***>(lpCommand->mpppPlugin);
            GenericRwacVoiceConfig* lpConfig =
                reinterpret_cast<GenericRwacVoiceConfig*>(lpCommand->mpConfig);

            *lppRwacVoice = rw::audio::core::Voice::CreateInstance(
                static_cast<u8>(lpConfig->GetProcessingStage()),
                static_cast<int>(lpConfig->GetPluginCount()),
                &lpConfig->GetConfig(0), lpppPlugin, mpSystem);
            lpPlaybackVoice->SetPlaybackState(E_PLAYBACK_STATE_STOPPED);
            lpPlaybackVoice->AcknowledgePlaybackStateChange();

            rw::audio::core::PlugIn** lppSubmix =
                reinterpret_cast<rw::audio::core::PlugIn**>(lpCommand->maOperand);
            if (lppSubmix != 0 && *lpppPlugin != 0)
                *lppSubmix = (*lpppPlugin)[0];

            if (lpConfig->GetVoiceType() == E_MASTER_VOICE &&
                *lppRwacVoice != 0 && lpConfig->GetPluginCount() != 0)
            {
                rw::audio::core::PlugIn* lpDac =
                    (*lpppPlugin)[lpConfig->GetPluginCount() - 1u];
                mEnvironment.SetDacPlugin(lpDac);
                DacOutputPC::Attach(static_cast<rw::audio::core::Dac*>(lpDac));
                rw::audio::core::PlugIn::Event(lpDac, 3, 0);
            }
            lpConfig->Release();
            break;
        }
        case E_RWAC_COMMAND_VOICE_RELEASE:
            CGS_ASSERT(luWordCount == 2, "Voice-release command word count");
            rw::audio::core::Voice::Release(
                reinterpret_cast<rw::audio::core::Voice*>(lauWords[1]));
            break;
        case E_RWAC_COMMAND_PLUGIN_EVENT:
            HandlePluginEvent(static_cast<u32>(luWordCount), lauWords);
            break;
        case E_RWAC_COMMAND_PLUGIN_GET_ATTRIBUTE:
            CGS_ASSERT(luWordCount == 4, "Get-attribute command word count");
            rw::audio::core::PlugIn::GetAttribute(
                reinterpret_cast<rw::audio::core::PlugIn*>(lauWords[1]),
                static_cast<int>(lauWords[2]),
                reinterpret_cast<f32*>(lauWords[3]));
            break;
        default:
            CGS_ASSERT(false, "Invalid Command");
            break;
        }
    }
}

// @0x826E9990. Select the concrete content class from the authored ContentType,
// construct it in a factory-owned carve, and return one owned handle reference.
bool GenericRwacFactory::DoCreateContent(const ContentSpec& akrSpec,
                                         Handle<Content>& arHandleOut,
                                         u32 au32Ident)
{
    const Name& lkrContentTypeName = akrSpec.GetContentType().GetName();
    Content* lpContent = 0;

    if (lkrContentTypeName == skWaveContentType)
    {
        lpContent = new (*this, akrSpec)
            GenericRwacWaveContent(*this, akrSpec, au32Ident);
    }
    else if (lkrContentTypeName == skReverbIrContentType)
    {
        lpContent = new (*this, akrSpec)
            GenericRwacReverbIRContent(*this, akrSpec, au32Ident);
    }
    else
    {
        CGS_ASSERT(false, "Unknown content type");
    }

    if (lpContent)
        lpContent->Acquire();
    arHandleOut.SetObject(lpContent);
    return lpContent != 0;
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
