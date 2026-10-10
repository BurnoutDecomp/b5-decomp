#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRenderStreamReader.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT (buffer-allocation guard)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.h"         // DebugManager::ThreadSafeAquire/Release
#include "GameShared/GameClasses/Development/DebugSystem/Interface/CgsDebugInterface.h"  // DebugInterface::GetRender
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRender.h"        // DebugRender queues
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebugRenderCommon.h"  // Internal::DebugStreamInput
#include "GameShared/GameClasses/Development/Log/CgsLog.h"                               // gpDebugPrint (bad-id warning)

// CgsDev::DebugRenderStreamReader. Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   Construct(IResourceAllocator*, liMaxCommands, liDataBufferSize) @ 0x82820CE8
//   Begin                                                           @ 0x82817718
//   End

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
        // FLAG: ARTIST 0x82820D40 passes r6=0 (NULL name) to DoAllocate, not a label string.
        // The earlier "DebugRenderStreamResults" literal was fabricated; the X360 passes NULL.
        rw::Resource lResultRes = lpAllocator->DoAllocate(lResultDesc, NULL);
        void* lpResultBuffer = lResultRes.m_baseResources[0];

        // Variable-size data buffer: liDataBufferSize bytes, 128-aligned.
        rw::ResourceDescriptor lDataDesc;
        lDataDesc.m_baseResourceDescriptors[0].m_size      = static_cast<u32>(liDataBufferSize);
        lDataDesc.m_baseResourceDescriptors[0].m_alignment = 128;
        // FLAG: ARTIST 0x82820D94 passes r6=0 (NULL name) to DoAllocate; "DebugRenderStreamData"
        // was a fabricated label. The X360 passes NULL.
        rw::Resource lDataRes = lpAllocator->DoAllocate(lDataDesc, NULL);
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

    // Take the debug manager, close the read pass, and drain every result: each entry whose id
    // is a debug-draw event (0..24) is re-queued, with its own size, onto the buffered renderer's
    // 2D queue (mbIs2D) or world queue; any other id is reported under message filter bit 0.
    // Releasing the manager last.
    void DebugRenderStreamReader::End()
    {
        const s32 KI_NUM_DEBUG_EVENT_IDS = 25;

        DebugManager* lpDebugManager = DebugManager::ThreadSafeAquire();
        mInput.End();

        Internal::DebugStreamInput lInput;
        while (mInput.ReadResult(&lInput) == CgsMemory::DataStreamResultReader::E_READ_SUCCESS)
        {
            for (s32 liEntry = 0; liEntry < lInput.miNumEntries; ++liEntry)
            {
                const Internal::DebugStreamInputEntry& lrEntry = lInput.mEntries[liEntry];
                if (lrEntry.miEventId >= KI_NUM_DEBUG_EVENT_IDS || lrEntry.miEventId < 0)
                {
                    if (CgsDev::Message::gxMessageFilterFlags & 1)
                    {
                        *Log::gpDebugPrint << "Warning, invalid event id at index " << liEntry
                                           << ", id: " << lrEntry.miEventId << "\n";
                    }
                    continue;
                }

                DebugRender& lrRender = DebugInterface(lpDebugManager).GetRender();
                CgsModule::VariableEventQueue<16384, 16>& lrQueue =
                    lrEntry.mbIs2D ? lrRender.m2DQueue : lrRender.m3DQueue;
                lrQueue.AddEventSafe(static_cast<const CgsModule::Event*>(lrEntry.mpEventData),
                                     lrEntry.miEventId, lrEntry.miEventSize);
            }
        }

        DebugManager::ThreadSafeRelease(lpDebugManager);
    }
}
