// ============================================================================
// CgsAemsFactory.cpp -- CgsSound::Playback::AemsFactory runtime bodies.
//
// Bodied from BURNOUT_X360_ARTIST.XEX:
//   AemsFactory::CsisPrint(const char*)        @ 0x8268A018
//   AemsFactory::FindPatchMonitor(const char*) @ 0x82689E98
//
// CsisPrint @ 0x8268A018:
//   if (CgsDev::Message::gxMessageFilterFlags & 1)
//       (*gpDebugPrint)[vslot 1](gpDebugPrint, str ? str : "<NULLSTRING>");
// The X360 calls the gpDebugPrint stream's second vtable slot, which is the
// committed DebugPrint::operator<<(const char*) (CgsLog.h). So the print routes
// through `*gpDebugPrint << text` exactly as the engine logging convention.
// DWARF (CgsAemsFactory.cpp:397) confirms this is `protected` and returns
// `void`; the tail-called vtable slot's own return value is not propagated.
//
// FindPatchMonitor @ 0x82689E98:
//   if (muPatchMonitorCount == 0) return 0;
//   for each maPatchMonitors[i] (i < count):
//       byte-compare maPatchMonitors[i].mpName against lpcName;  // strcmp
//       if equal -> return &maPatchMonitors[i];
//   return 0;
// The X360 inner do/while is a hand-inlined strcmp over the two C strings; the
// outer loop walks the table and bails out at the registered count. Called by
// AemsFactory::AddPatchMonitor (DEFERRED sibling).
// ============================================================================

#include "GameShared/GameClasses/Sound/Playback/AEMS/CgsAemsFactory.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include "rw/rwcore_structs.h"      // rw::IResourceAllocator / BaseResourceDescriptors (the carve)
#include "SDKs/Csis/CsisSystem.h"   // Csis::System::IsInited
#include "SDKs/EATech/include/snd/sndaems.h" // Snd9::Aems::SetSamplePlayerFactory

#include <cstring>
#include <new>      // placement new (the in-carve construct)

namespace CgsSound
{
namespace Playback
{

namespace
{
    // The interned factory name (console dword_83008664, written at static-init
    // by sub_82C65788 = Name::MakeHash("~AemsFactory::SK_NAME~")).
    const Name skAemsFactoryName("~AemsFactory::SK_NAME~");

    // The five-pair allocator-descriptor inline (the RWAC/Module TU-local convention).
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

// ---------------------------------------------------------------------------
// AemsFactory::Create @ 0x826DAC28  (AEMS-cascade slice 2; full decode
// progress/scratch_dossiers/aems_factory_cascade_codex.md). Ref-spec ABI;
// console carve 4*(entityCount+0x162)+data+strings (0x162 words == the fixed
// head through the +0x56C registry header) at host widths; the returned
// handle's ref rides the Factory-subobject refcount (console overall +0x08 --
// the MI proof), which on the host is the same Object::Acquire through the
// Factory base.
// ---------------------------------------------------------------------------
Handle<AemsFactory> AemsFactory::Create(Environment& arEnvironment,
                                        const AemsFactorySpec& akrSpec)
{
    const size_t luBytes = sizeof(AemsFactory)
                         + sizeof(Registry)
                         + sizeof(void*) * akrSpec.mu32EntityCount
                         + akrSpec.mu32DataSize
                         + akrSpec.mu32StringTableSize;

    void* lpMemory = AllocateMemoryResource(arEnvironment.GetAllocator(), luBytes,
                                            4, "AemsFactory");
    if (lpMemory == 0)
    {
        return Handle<AemsFactory>();
    }

    AemsFactory* lpFactory = ::new (lpMemory) AemsFactory(arEnvironment, akrSpec);

    // The returned handle's owned ref (console `stw ptr` + the +0x08 increment;
    // interim plain-store Handle model -- the explicit Acquire beside the store).
    lpFactory->Acquire();
    return Handle<AemsFactory>(lpFactory);
}

// ---------------------------------------------------------------------------
// AemsFactory ctor @ 0x826DAAD0. Console store order after the AemsRW base:
// the final vptr pair (host: compiler-emitted), the retained RWAC handle
// (+0x64 + the pointee refcount increment), the two command-queue control
// words, the registry pointer + the in-place Registry (+0x56C), the CSIS
// checks, and the global sample-player-factory install.
// ---------------------------------------------------------------------------
AemsFactory::AemsFactory(Environment& arEnvironment, const AemsFactorySpec& akrSpec)
    : AemsRWSampleFactory(skAemsFactoryName, arEnvironment)
{
    // +0x64: the retained RWAC factory handle (raw pointer + explicit retain --
    // the console's `old count + 1` on the pointee when non-null).
    mpRwacFactory = akrSpec.mpRwacFactory;
    if (mpRwacFactory != 0)
    {
        mpRwacFactory->Acquire();
    }

    // The two command-queue control words (+0x464/+0x468); the payload span is
    // ctor-untouched (decode-attested).
    mu32CommandQueueControl0 = 0;
    mu32CommandQueueControl1 = 0;

    // +0x60 -> the in-place Registry immediately after the fixed head (console +0x56C).
    RegistrySpec lRegistrySpec;
    lRegistrySpec.mu32EntityCount   = akrSpec.mu32EntityCount;
    lRegistrySpec.muDataSize        = akrSpec.mu32DataSize;
    lRegistrySpec.muStringTableSize = akrSpec.mu32StringTableSize;
    mpRegistry = ::new (reinterpret_cast<u8*>(this) + sizeof(AemsFactory))
        Registry(lRegistrySpec);

    // The CSIS integration tail:
    //  1. Csis::System::IsInited @0x82B10040, asserted (console fires when the
    //     low byte is zero).
    CGS_ASSERT(Csis::System::IsInited() != 0, "Csis::System::IsInited()");
    //  2. The console materializes &CsisPrint (the static one-arg callback --
    //     the ABI-corrected shape) and calls 0x82B0F1B8 with it. That callee's
    //     own dossier is a two-instruction `li r3,0; blr` leaf: it neither
    //     reads the callback nor writes state, so there is NO registration to
    //     reproduce -- promoting it into an invented CSIS callback registry is
    //     exactly what the decode warns against. The address expression is kept
    //     here so the callback's shape stays load-bearing.
    (void)&AemsFactory::CsisPrint;
    //  3. Snd9::Aems::SetSamplePlayerFactory(this) @0x82B6FCD0 -- the process
    //     global (off_82F87DBC) now points at this factory's IAems interface.
    Snd9::Aems::SetSamplePlayerFactory(this);

    // +0x5C := 0 -- LAST, after the CSIS check (console order 9).
    muPatchMonitorCount = 0;
}

// The per-factory registry accessor (CgsSoundPlaybackModule.h:100 -- the console
// reads AemsFactory+0x60). REAL now; the BrnBaselineLinkStubs null shim is
// retired with this body. The generic Factory* arrives as the Factory SUBOBJECT
// pointer (the environment table stores that, per the MI decode), so the cast
// walks down through the real hierarchy.
Registry* GetAemsFactoryRegistry(Factory* lpAemsFactory)
{
    return static_cast<AemsFactory*>(lpAemsFactory)->GetRegistry();
}

// ---------------------------------------------------------------------------
// AemsFactory::CsisPrint(const char* lpcText)  @ 0x8268A018
// ---------------------------------------------------------------------------
void AemsFactory::CsisPrint(const char* lpcText)
{
    if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
    {
        *CgsDev::Log::gpDebugPrint << (lpcText ? lpcText : "<NULLSTRING>");
    }
}

// ---------------------------------------------------------------------------
// AemsFactory::FindPatchMonitor(const char* lpcName)  @ 0x82689E98
// ---------------------------------------------------------------------------
PatchMonitor* AemsFactory::FindPatchMonitor(const char* lpcName)
{
    if (muPatchMonitorCount == 0)
    {
        return nullptr; // cmplwi count,0 ; beq -> li r3,0
    }

    for (u32 luI = 0; luI < muPatchMonitorCount; ++luI)
    {
        // Hand-inlined strcmp over the C strings (the X360 do/while loop).
        if (std::strcmp(maPatchMonitors[luI].mpName, lpcName) == 0)
        {
            return &maPatchMonitors[luI];
        }
    }
    return nullptr;
}

} // namespace Playback
} // namespace CgsSound
