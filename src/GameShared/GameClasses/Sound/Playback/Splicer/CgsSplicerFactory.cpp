#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.h"  // the REAL SplicerFactory : public Factory
#include "GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h"      // the trailing-arena SpliceManager (slice 3)
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerContent.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerPlayerVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator / BaseResourceDescriptors (the carve)

#include <new>   // placement new (the in-carve constructs)

// ============================================================================
// GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.cpp
//
// CgsSound::Playback::SplicerFactory::SplicerAssertFunc -- the splice factory's
// assertion sink. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//
//   CgsSound::Playback::SplicerFactory::SplicerAssertFunc @ 0x8268ABA0
//
// This is the failure path the SplicerFactory routes its assertions through. On
// X360 it builds a message-stream object on the stack (a StrStream over the global
// CgsDev::Assert::gpcMessageBuffer), streams the supplied expression text into it,
// then fires the assert at this file's line 140 (0x8C) via the standard
// Begin/Fire/End sequence:
//
//   CgsDev::Assert::BeginAssert();
//   StrStream s(gpcMessageBuffer);   // off_82000D00 vtable, Clear, seed buffer+config
//   *gpcMessageBuffer = 0;
//   s << (a1 ? a1 : "<NULLSTRING>"); // virtual operator<<(const char*) at vtbl+4
//   CgsDev::Assert::FireAssert(gpcMessageBuffer,
//       "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Splicer/CgsSplicerFactory.cpp",
//       140);
//   return CgsDev::Assert::EndAssert();
//
// The StrStream construct + virtual append is the binary's CgsDev message-streaming
// machinery; the house CGS_ASSERT front-end (CgsAssert.h) forwards a plain string
// to FireAssert instead of streaming it (the streamed form is semantically vacuous
// under the house substitution -- the message that reaches the handler is the same
// expression text). The reconstruction therefore reproduces the observable
// behaviour store-for-store at the assert-front-end boundary: enter the assert,
// fire it with the supplied message (falling back to "<NULLSTRING>" exactly as the
// X360 does when the argument is null), and leave the assert returning EndAssert's
// result. The line number (140) and the null-string fallback are reproduced exactly.
//
// SplicerAssertFunc is itself the assertion -- it always fires (it is the sink the
// factory jumps to once a check has already failed), so there is no condition to
// gate on. The X360 leaves EndAssert's pointer result in r3; that is returned.
// ============================================================================

namespace CgsSound
{
namespace Playback
{

// SplicerAssertFunc is a member of the REAL SplicerFactory (: public Factory,
// Splicer/CgsSplicerFactory.h) -- the TU-local rival struct an earlier revision
// defined here (base-less, member-less) was an ODR violation against that header
// and was retired 2026-08-25 (audio-faithfulness wave 1).
void* SplicerFactory::SplicerAssertFunc(const char* lpcExpression)
{
    // Null-argument fallback, reproduced exactly from the X360 (`<NULLSTRING>`).
    const char* lpcMessage = (lpcExpression != 0) ? lpcExpression : "<NULLSTRING>";

    // Enter the assert, fire it with the (streamed-then-forwarded) message at this
    // file's line 140, and leave -- returning the front-end's leave result. This is
    // the observable behaviour of the X360 Begin/StrStream-append/Fire/End sequence
    // once the StrStream machinery is folded into the house CGS_ASSERT front-end.
    CgsDev::Assert::BeginAssert();
    CgsDev::Assert::FireAssert(
        lpcMessage,
        "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Splicer/CgsSplicerFactory.cpp",
        140);
    return CgsDev::Assert::EndAssert();
}

// ===========================================================================
// SplicerFactory Create + ctor (AEMS-cascade slice 3; register-level decode:
// progress/scratch_dossiers/aems_factory_cascade_codex.md, Splicer section)
// ===========================================================================

namespace
{
    // The interned factory name (console dword_83008404, written at static-init by
    // sub_82C65938 = Name::MakeHash("~SplicerFactory::SK_NAME~")).
    const Name skSplicerFactoryName("~SplicerFactory::SK_NAME~");

    // The five-pair allocator-descriptor inline (the RWAC/AEMS TU-local convention).
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

// @ 0x826DB130. The ref-spec create: console carve 4*(entityCount+0x1C0)+data+
// strings (the 0x1C0-word term == the fixed head + registry header + the
// SpliceManager arena); host: the same regions at host sizeofs. Allocated
// through the ENVIRONMENT's allocator tagged "SplicerFactory"; the returned
// handle carries one explicit Acquire (the interim plain-store Handle model).
Handle<SplicerFactory> SplicerFactory::Create(Environment& arEnvironment,
                                              const SplicerFactorySpec& akrSpec)
{
    const size_t luBytes = sizeof(SplicerFactory)
                         + sizeof(Registry)
                         + sizeof(void*) * akrSpec.mu32EntityCount
                         + akrSpec.mu32DataSize
                         + akrSpec.mu32StringTableSize
                         + sizeof(SpliceManager);

    void* lpMemory = AllocateMemoryResource(arEnvironment.GetAllocator(), luBytes,
                                            4, "SplicerFactory");
    if (lpMemory == 0)
    {
        return Handle<SplicerFactory>();
    }

    SplicerFactory* lpFactory = ::new (lpMemory) SplicerFactory(arEnvironment, akrSpec);

    lpFactory->Acquire();
    return Handle<SplicerFactory>(lpFactory);
}

// @ 0x826DB010. Store order per the decode: base Factory (name = the intern
// above), the final vtable (host: implicit), the retained RWAC handle (+0x14),
// the registry pointer (+0x10) + the in-place Registry (+0x1C), the trailing
// SpliceManager (its address derived from the registry's own table/data/string
// spans -- the console's lwz+4/+0x10/+8 arithmetic at host widths), and the
// manager's assert-sink install (manager+0x610 := &SplicerAssertFunc, the
// ABI-corrected static one-argument callback).
SplicerFactory::SplicerFactory(Environment& arEnvironment,
                               const SplicerFactorySpec& akrSpec)
    : Factory(skSplicerFactoryName, arEnvironment)
{
    // +0x14: the retained RWAC factory handle (raw pointer + explicit retain).
    mpRwacFactory = akrSpec.mpRwacFactory;
    if (mpRwacFactory != 0)
    {
        mpRwacFactory->Acquire();
    }

    // +0x10 -> the in-place Registry immediately after the fixed head (console +0x1C).
    RegistrySpec lRegistrySpec;
    lRegistrySpec.mu32EntityCount   = akrSpec.mu32EntityCount;
    lRegistrySpec.muDataSize        = akrSpec.mu32DataSize;
    lRegistrySpec.muStringTableSize = akrSpec.mu32StringTableSize;
    u8* lpRegistryBase = reinterpret_cast<u8*>(this) + sizeof(SplicerFactory);
    mpRegistry = ::new (lpRegistryBase) Registry(lRegistrySpec);

    // +0x18 -> the trailing SpliceManager arena: past the registry header, its
    // entity table, its data blob and its string table (the console's
    // registry-member arithmetic, host sizeofs).
    u8* lpManagerBase = lpRegistryBase
                      + sizeof(Registry)
                      + sizeof(void*) * akrSpec.mu32EntityCount
                      + akrSpec.mu32DataSize
                      + akrSpec.mu32StringTableSize;
    mpManager = ::new (lpManagerBase)
        SpliceManager(arEnvironment, 0x40, 0x18);   // mono 64 / stereo 24 (console literals)

    // manager+0x610 := the assert sink (the post-construction callback store).
    // SplicerAssertFunc returns EndAssert's pointer result; the manager's
    // one-argument sink type discards it -- the console callers do too.
    mpManager->SetAssertCallbackFunction(
        reinterpret_cast<SpliceManager::AssertCallbackFunc>(&SplicerFactory::SplicerAssertFunc));
}

GenericRwacFactory& SplicerFactory::GetRwacFactory() const
{
    return *static_cast<GenericRwacFactory*>(mpRwacFactory);
}

// @ 0x826E9C88. A SplicerFactory owns the single SplicerContent class. The
// console checks the authored ContentType, then uses FactoryNew to allocate and
// construct this exact object and publishes one owned handle reference.
bool SplicerFactory::DoCreateContent(const ContentSpec& akrSpec,
                                     Handle<Content>& arHandleOut,
                                     u32 au32Ident)
{
    SplicerContent* lpContent = new (*this, akrSpec)
        SplicerContent(*this, akrSpec, au32Ident);
    if (lpContent)
        lpContent->Acquire();
    arHandleOut.SetObject(lpContent);
    return lpContent != 0;
}

// @ 0x826FA578. Splicer voices are player voices. The concrete constructor
// builds the slot implementation and delegates its render graph to RWAC.
bool SplicerFactory::DoCreateVoice(const VoiceSpec& akrSpec,
                                   Handle<Voice>& arHandleOut,
                                   u32 au32Ident)
{
    arHandleOut.SetObject(0);
    CGS_ASSERT(akrSpec.mu8VoiceType == E_PLAYER_VOICE,
               "E_PLAYER_VOICE == lVoiceSpec.GetVoiceType()");
    if (akrSpec.mu8VoiceType != E_PLAYER_VOICE)
        return false;

    SplicerPlayerVoice* lpVoice = new (*this, akrSpec)
        SplicerPlayerVoice(*this, akrSpec, au32Ident);
    if (lpVoice)
        lpVoice->Acquire();
    arHandleOut.SetObject(lpVoice);
    return lpVoice != 0;
}

// @ 0x8268AB60. The retail body brackets no work between the Splicer monitor
// calls; the environment owns the monitor front, so there is no factory state to
// advance here.
void SplicerFactory::DoUpdate(f32 /*af32DeltaTime*/)
{
}

} // namespace Playback
} // namespace CgsSound
