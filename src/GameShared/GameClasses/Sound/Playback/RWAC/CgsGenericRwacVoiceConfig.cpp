#include "GameShared/GameClasses/Sound/Playback/RWAC/CgsGenericRwacVoiceConfig.h"

#include "rw/rwcore_structs.h"                                  // rw::ResourceDescriptor, rw::Resource
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h" // CgsResource::ResourceDescriptor (= rw::BaseResourceDescriptors<5>)

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x826AEEE8
//   (CgsSound::Playback::GenericRwacVoiceConfig::operator new(size_t, Environment&))
//   called by CgsSound::Playback::GenericRwacFactory::SetupConfig.
//
// This is the placement operator new declared at CgsGenericRwacVoice.h:156
//   void* operator new(size_t, CgsSound::Playback::Environment&);
// It allocates the config block through the sound Environment's RenderWare
// IResourceAllocator and returns the allocated base pointer.
//
// X360 behaviour (asm, big-endian):
//   r3 = size (a1), r4 = Environment& (a2)
//   - build an 8-byte block {m_size = size, m_alignment = 4}        (stw size; li 4)
//   - lpAllocator = *(Environment + 0x30)  == Environment::mpAllocator (GetAllocator())
//   - build the X360 serialised descriptor CgsResource::ResourceDescriptor
//     (rw::BaseResourceDescriptors<5>, 40B -- the asm populates var_30..var_C = 5 entries):
//         desc[0] = { size, 4 }
//         desc[1] = desc[2] = desc[3] = desc[4] = { 0, 1 }
//   - rw::Resource lResult = lpAllocator->DoAllocate(desc, "GenericRwacVoiceConfig");
//         (virtual call, X360 vtable slot +0x10 == rw::IResourceAllocator::DoAllocate,
//          sret result buffer in r3, allocator in r4, &desc in r5, name in r6)
//   - return lResult.m_baseResources[0]    (lwz r3, 0(resultbuf))
//
// FLAG (Environment MINIMAL home): CgsSound::Playback::Environment has its own large
// DWARF home (CgsEnvironment.h) and is not yet committed. Modelled here as a minimal
// type exposing GetAllocator() BY NAME -- the only thing operator new touches. The
// X360 reads the allocator at Environment +0x30 (== mpAllocator, after the Object
// base + mCpuMonitors per CgsEnvironment.h:404-405); GetAllocator() (CgsEnvironment.h:675)
// is the named accessor. The exact +0x30 slot is DEFERRED to the Environment keystone
// TU and is intentionally NOT offset-asserted here (member access by name only).

namespace CgsSound
{
namespace Playback
{
    GenericRwacVoiceConfig::GenericRwacVoiceConfig(Environment& arEnvironment)
        : mScratchPad(), mu32PluginCount(0), mu32FirstSendPlugin(0),
          miProcessingStage(0), meVoiceType(E_PLAYER_VOICE),
          mEnvironment(arEnvironment)
    {
    }

    void* GenericRwacVoiceConfig::operator new(size_t auSize, Environment& arEnvironment)
    {
        rw::IResourceAllocator* lpAllocator = arEnvironment.GetAllocator();

        // X360 stack build: a FIVE-entry serialised descriptor (40B). Reuse the committed
        // CgsResource::ResourceDescriptor (= rw::BaseResourceDescriptors<5>) BY NAME.
        // desc[0] = { size, 4 }; desc[1..4] = { 0, 1 }.
        CgsResource::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = static_cast<u32>(auSize);
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 4;
        for (u32 luIndex = 1; luIndex < 5; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size = 0;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }
        static_assert(sizeof(CgsResource::ResourceDescriptor) == 40,
                      "X360 serialised descriptor is 5x{size,align}=40B");

        // The PC rwcore IResourceAllocator::DoAllocate ABI takes the narrower
        // rw::ResourceDescriptor (<4>); the X360 5th {0,1} entry is inert (size 0), so the
        // <4> prefix of the <5> build drives the identical allocation. Bridge the X360-faithful
        // <5> build to the PC <4> ABI view (layout-compatible leading 4 entries).
        const rw::ResourceDescriptor& lAbiDescriptor =
            reinterpret_cast<const rw::ResourceDescriptor&>(lDescriptor);
        rw::Resource lResource = lpAllocator->DoAllocate(lAbiDescriptor, "GenericRwacVoiceConfig");
        return lResource.m_baseResources[0];
    }

    void GenericRwacVoiceConfig::operator delete(void* /*apMemory*/,
                                                  Environment& /*arEnvironment*/)
    {
    }

    void GenericRwacVoiceConfig::Release()
    {
        rw::IResourceAllocator* lpAllocator = mEnvironment.GetAllocator();
        rw::Resource lResource;
        lResource.m_baseResources[0] = this;
        lResource.m_baseResources[1] = 0;
        lResource.m_baseResources[2] = 0;
        lResource.m_baseResources[3] = 0;
        this->~GenericRwacVoiceConfig();
        lpAllocator->DoFree(lResource);
    }
}
}
