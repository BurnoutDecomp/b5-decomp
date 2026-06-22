#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRenderStreamReader.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (buffer-allocation guard)

// CgsDev::DebugRenderStreamReader. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct(IResourceAllocator*, liMaxCommands, liDataBufferSize) @ 0x82820CE8
//   Begin                                                           @ 0x82817718

namespace CgsDev
{
    // 0x82820CE8 - size and allocate the command + data buffers through the allocator, then build the
    // underlying result reader. The command count rounds up to whole 256-byte pages (15 commands per
    // page): liPageCount = (liMaxCommands + 14) / 15, result buffer = liPageCount * 256 bytes. Both
    // buffers are 128-byte aligned (the alignment DataStreamResultReader requires). On allocation
    // failure the X360 asserts ("Failed to allocate buffers"); we keep that guard, then forward the
    // (possibly null) pointers to mInput.Construct, matching the X360's unconditional final call.
    void DebugRenderStreamReader::Construct(rw::IResourceAllocator* lpAllocator,
                                            s32 liMaxCommands, s32 liDataBufferSize)
    {
        const s32 liPageCount        = (liMaxCommands + (KI_COMMANDS_PER_PAGE - 1)) / KI_COMMANDS_PER_PAGE;
        const s32 liResultBufferSize = liPageCount * KI_PAGE_SIZE;

        // Result (command page) buffer: liResultBufferSize bytes, 128-aligned.
        rw::ResourceDescriptor lResultDesc;
        lResultDesc.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(liResultBufferSize);
        lResultDesc.m_baseResourceDescriptors[0].m_alignment = 128;
        rw::Resource lResultRes = lpAllocator->DoAllocate(lResultDesc, "DebugRenderStreamResults");
        void* lpResultBuffer = lResultRes.m_baseResources[0];

        // Variable-size data buffer: liDataBufferSize bytes, 128-aligned.
        rw::ResourceDescriptor lDataDesc;
        lDataDesc.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(liDataBufferSize);
        lDataDesc.m_baseResourceDescriptors[0].m_alignment = 128;
        rw::Resource lDataRes = lpAllocator->DoAllocate(lDataDesc, "DebugRenderStreamData");
        void* lpDataBuffer = lDataRes.m_baseResources[0];

        CGS_ASSERT(lpResultBuffer != NULL && lpDataBuffer != NULL, "Failed to allocate buffers\n");

        mInput.Construct(lpResultBuffer, liResultBufferSize, KI_PAGE_SIZE,
                         lpDataBuffer, liDataBufferSize);
    }

    // 0x82817718 - open a read pass over the underlying result reader.
    void DebugRenderStreamReader::Begin()
    {
        mInput.Begin();
    }
}
