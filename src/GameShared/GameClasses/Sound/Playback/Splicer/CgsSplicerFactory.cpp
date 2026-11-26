#include "types.hpp"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerFactory.h"  // the REAL SplicerFactory : public Factory
#include "GameShared/GameClasses/Sound/Playback/Splicer/SpliceManager.h"      // the trailing-arena SpliceManager (slice 3)
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerContent.h"
#include "GameShared/GameClasses/Sound/Playback/Splicer/CgsSplicerPlayerVoice.h"
#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacFactory.h"
#include "rw/rwcore_structs.h"   // rw::IResourceAllocator / BaseResourceDescriptors (the carve)

#include <new>   // placement new (the in-carve constructs)

// This file's path as ARTIST's asserts spell it (string 0x820ADA88).
#define KAC_SPLICER_FACTORY_FILE "..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Splicer/CgsSplicerFactory.cpp"

namespace CgsSound
{
namespace Playback
{

// ARTIST 0x8268ABA0. Stream into the shared assertion buffer before firing.
void SplicerFactory::SplicerAssertFunc(char* lpExpression)
{
    CgsDev::Assert::BeginAssert();
    // DWARF BrnSoundUnity.cpp: the assert at CgsSplicerFactory.cpp:140 streams the message into
    // lStrStream; the null case is StrStreamBase::operator<<(const char*)'s own "<NULLSTRING>".
    CgsDev::StrStream lStrStream(CgsDev::Assert::gpcMessageBuffer,
                                 CgsDev::Assert::giMessageBufferSize);
    lStrStream << lpExpression;
    CgsDev::Assert::FireAssert(
        CgsDev::Assert::gpcMessageBuffer,
        CGS_ASSERT_LOCATION("..\\..\\..\\GameShared\\GameClasses\\Sound/Playback/Splicer/CgsSplicerFactory.cpp",
        140));
    CgsDev::Assert::EndAssert();
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

    void* lpMemory = arEnvironment.GetAllocator()->AllocateMemoryResource(luBytes,
                                            4, "SplicerFactory");
    if (lpMemory == 0)
    {
        return Handle<SplicerFactory>();
    }

    SplicerFactory* lpFactory = ::new (lpMemory) SplicerFactory(arEnvironment, akrSpec);

    return Handle<SplicerFactory>(lpFactory);   // Handle(T*) takes the returned reference
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
    mpManager->SetAssertCallbackFunction(&SplicerFactory::SplicerAssertFunc);
}

GenericRwacFactory& SplicerFactory::GetRwacFactory() const
{
    return *static_cast<GenericRwacFactory*>(mpRwacFactory);
}

// @ 0x826E9C88. A SplicerFactory owns the single SplicerContent class. The
// console checks the authored ContentType, then uses FactoryNew to allocate and
// construct this exact object and publishes one owned handle reference.
// DWARF (BrnSoundUnity) parameters lContentSpec / lhContentOut / luiIdent: assert the
// type (CgsSplicerFactory.cpp:121, 0x79 at 0x826E9CC8; the text spells the comparison),
// then the FactoryNew<SplicerContent> handle assigned through the CgsHandle.h:239 view and
// Handle<Content>::operator= (inlined here, 0x826E9CF8..0x826E9D2C), `return lhContentOut;`.
bool SplicerFactory::DoCreateContent(const ContentSpec& lContentSpec,
                                     Handle<Content>& lhContentOut,
                                     u32 luiIdent)
{
    CGS_ASSERT_AT(SplicerContent::SK_CONTENT_TYPE == lContentSpec.GetContentType().GetName(),
                  "SplicerContent::SK_CONTENT_TYPE == lContentSpec.GetContentType().GetName()",
                  KAC_SPLICER_FACTORY_FILE, 121);
    lhContentOut = FactoryNew<SplicerContent>(lContentSpec, luiIdent);
    return lhContentOut;
}

// @ 0x826FA578. Splicer voices are player voices: assert the type
// (CgsSplicerFactory.cpp:104, 0x68 at 0x826FA5A8), then the FactoryNew<SplicerPlayerVoice>
// handle (inlined: operator new, the constructor, the acquire merged with operator='s into
// `+2` at 0x826FA5F8) assigned through the view, `return lhVoiceOut;`.
bool SplicerFactory::DoCreateVoice(const VoiceSpec& lVoiceSpec,
                                   Handle<Voice>& lhVoiceOut,
                                   u32 luiIdent)
{
    CGS_ASSERT_AT(E_PLAYER_VOICE == lVoiceSpec.mu8VoiceType,
                  "E_PLAYER_VOICE == lVoiceSpec.GetVoiceType()",
                  KAC_SPLICER_FACTORY_FILE, 104);
    lhVoiceOut = FactoryNew<SplicerPlayerVoice>(lVoiceSpec, luiIdent);
    return lhVoiceOut;
}

// @ 0x8268AB60. The retail body brackets no work between the Splicer monitor
// calls; the environment owns the monitor front, so there is no factory state to
// advance here.
void SplicerFactory::DoUpdate(f32 /*af32DeltaTime*/)
{
}

} // namespace Playback
} // namespace CgsSound

#undef KAC_SPLICER_FACTORY_FILE
