#include "GameShared/GameClasses/Memory/CgsMemoryModuleIO.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"  // CGS_ASSERT

#include <cstring>  // std::strncpy (models the X360 strncpy default-name fill)

// CgsMemory::MemoryIO::InputBuffer accessors. Recovered from the X360 bodies
// (const @ 0x82869248, non-const @ 0x828E1040): each tests one IOBuffer status bit via
// the inherited query, asserts on failure, then returns the embedded queue (this+4).
namespace CgsMemory
{
namespace MemoryIO
{
    // Lifecycle (DWARF :746/:750 + :897/:901): construct the IOBuffer base (sets the constructed
    // status bit so the lock asserts pass) and the embedded request/response event queue. Destruct
    // clears the queue. (The X360 inlines the base Construct + the queue Construct here.)
    void InputBuffer::Construct()  { CgsModule::IOBuffer::Construct(); mMemoryRequestQueue.Construct(); }
    void InputBuffer::Destruct()   { mMemoryRequestQueue.Clear(); }
    void OutputBuffer::Construct() { CgsModule::IOBuffer::Construct(); mMemoryResponseQueue.Construct(); }
    void OutputBuffer::Destruct()  { mMemoryResponseQueue.Clear(); }

    // X360 0x82869248 (const): assert read-locked (status bit 4), then hand back the queue.
    const InputBuffer::MemoryRequestQueue* InputBuffer::GetMemoryRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMemoryRequestQueue;
    }

    // X360 0x828E1040 (non-const): assert write-locked (status bit 3), then hand back the queue.
    InputBuffer::MemoryRequestQueue* InputBuffer::GetMemoryRequestQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mMemoryRequestQueue;
    }

    // OutputBuffer accessors -- mirror of InputBuffer (the response queue lives at this+4).
    const OutputBuffer::MemoryResponseQueue* OutputBuffer::GetMemoryResponseQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return &mMemoryResponseQueue;
    }

    OutputBuffer::MemoryResponseQueue* OutputBuffer::GetMemoryResponseQueue()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return &mMemoryResponseQueue;
    }

    // X360 0x82663A68. Builds a create-bank request with default Params, ready for
    // the client's Set* helpers to fill in. Store order mirrors the X360:
    //   mpUser(=a2)@0, mnEventId(=a3)@4, meEventType(=0)@8  [the base request Construct],
    //   strncpy(macName, "Unnamed", 31) + null backstop @12..43,
    //   mnParentBankId(=-1)@44, mnBankId(=-1)@48,
    //   mauBankSize[0..4]=0 @52..68, mauBankBlocks[0..4]=0 @72..88,
    //   mbIsLeaf(=false)@92, mbAllowFragmentation(=true)@93.
    // The per-type loop runs KI_NUM_TYPES (5) iterations -- the X360's resource-type
    // count (the `liResourceType < rw::BASERESOURCE_NUMBER` bound the original asserts).
    void CreateBankRequest::Construct(CgsModule::BaseEventReceiverQueue* lpUser, s32 liEventId)
    {
        MemoryRequest::Construct(lpUser, liEventId, E_EVENT_TYPE_CREATE_BANK);

        std::strncpy(mParams.macName, "Unnamed", CgsMemory::MemoryBank::KI_NAME_SIZE - 1);
        mParams.macName[CgsMemory::MemoryBank::KI_NAME_SIZE - 1] = 0;

        mParams.mnParentBankId = -1;
        mParams.mnBankId       = -1;

        for (s32 liType = 0; liType < CgsMemory::MemoryBank::KI_NUM_TYPES; ++liType)
        {
            mParams.mauBankSize[liType]   = 0;
            mParams.mauBankBlocks[liType] = 0;
        }

        mParams.mbIsLeaf             = false;
        mParams.mbAllowFragmentation = true;
    }
}
}
