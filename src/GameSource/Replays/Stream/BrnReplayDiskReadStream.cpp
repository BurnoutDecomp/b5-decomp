#include "GameSource/Replays/Stream/BrnReplayDiskReadStream.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h" // DeviceManager, GetDeviceManager

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Control flow, the ring/loop arithmetic,
// the status flags and every assert message are taken from the X360 asm; the DecFIGS
// BrnReplayDiskReadStream.h DWARF supplies the member/enum names. See the header for the
// field-order/offset map. Only the seven ledger functions of this TU are defined here.

// --- Win32/X360 critical-section primitives (the lock embedded at object offset 0).
//     Declared here exactly as the sibling GPUDiskWriteStream.cpp does; the real bodies
//     come from the platform. RtlEnter/LeaveCriticalSection(this) is the inlined
//     mMutex Lock/Unlock. ---
extern "C" void RtlEnterCriticalSection(void* lpCriticalSection);
extern "C" void RtlLeaveCriticalSection(void* lpCriticalSection);

// Xbox block copy the ARTIST asm calls for the delivered read data (semantically memcpy).
extern "C" void* XMemCpy(void* lpDest, const void* lpSrc, size_t luSize);

namespace BrnReplays
{
    // =====================================================================================
    // ResetStreamBlocks @ 0x8264D8C0 -- clear the ring and reset its cursors/counters.
    // =====================================================================================
    void DiskReadStream::ResetStreamBlocks()
    {
        CGS_ASSERT(!mbLockedForRead, "Can not reset stream while locked for read\n");
        CGS_ASSERT(miInputRequestCount == 0, "Can not reset while input requests are occuring\n");
        CGS_ASSERT(miPendingOperationCount == 0, "Can not reset while pending operations are occuring\n");

        for (s32 li = 0; li < miNumBlocks; ++li)
        {
            ReadStreamBlock& lBlock = maBlocks[li];
            lBlock.miFilePos   = 0;
            lBlock.muFlags     = KU_RSBFLAG_EMPTY;
            lBlock.miDataStart = 0;
            lBlock.miDataEnd   = 0;
            lBlock.miStreamPos = li * miBlockSize;
        }

        miBlocksUsed     = 0;
        mbServiced       = false;
        miOutputBlock    = 0;
        miInputBlock     = 0;
        mbAdjustingRange = false;
    }

    // =====================================================================================
    // Service @ 0x82659D40 -- refresh the priority, then (once idle) close / rebuild the
    // range / submit the next read as the current state demands.
    // =====================================================================================
    void DiskReadStream::Service()
    {
        if (!mbAdjustingRange && !mbWaitingToClose && mbServiced)
            return;

        miCurrentPriority = GetCurrentFilePriority();

        if (miPendingOperationCount > 0)
            return;

        if (mbWaitingToClose)
        {
            if (SubmitCloseRequest())
                mbWaitingToClose = false;
            return;
        }

        if (mbAdjustingRange)
        {
            ResetStreamBlocks();
            mbAdjustingRange = false;
        }

        if (meStatus != E_STATUS_CLOSED && meStatus != E_STATUS_ERROR)
            SubmitReadRequest();
    }

    // =====================================================================================
    // SetRange @ 0x82659EB8 -- set the looping read window and flag a ring rebuild.
    // =====================================================================================
    void DiskReadStream::SetRange(s32 liStartFilePos, s32 liEndFilePos)
    {
        RtlEnterCriticalSection(mMutex);

        CGS_ASSERT(meStatus == E_STATUS_OPEN, "Invalid state to do set range\n");
        CGS_ASSERT(!mbLockedForRead, "Can not set range while locked for read\n");

        miLoopStart         = liStartFilePos;
        miLoopEnd           = liEndFilePos;
        mbAdjustingRange    = true;
        miNextInputPosition = miLoopStart;
        miReadPosition      = miLoopStart;

        Service();
        RtlLeaveCriticalSection(mMutex);
    }

    // =====================================================================================
    // SubmitReadRequest @ 0x8264DA80 -- issue the next async disk Read into the current
    // input slot; advance the ring and the wrapped next-input position.
    // =====================================================================================
    bool DiskReadStream::SubmitReadRequest()
    {
        if (miBlocksUsed >= miNumBlocks)
            return false;

        CGS_ASSERT(meStatus == E_STATUS_OPEN, "Attempting to service a none-open stream\n");

        ReadStreamBlock& lBlock = maBlocks[miInputBlock];
        CGS_ASSERT(lBlock.muFlags == KU_RSBFLAG_EMPTY,
                   "Should never be loading into a none-empty block\n");

        lBlock.miFilePos   = miNextInputPosition;
        lBlock.miDataEnd   = 0;
        lBlock.miDataStart = 0;
        lBlock.muFlags     = KU_RSBFLAG_READING;

        CgsFileSystem::DeviceManager* lpDeviceManager = CgsFileSystem::GetDeviceManager();
        lpDeviceManager->Read(static_cast<CgsFileSystem::Handle>(muHandle),
                              static_cast<u64>(miNextInputPosition),
                              mpBuffer + lBlock.miStreamPos,
                              static_cast<u32>(miBlockSize),
                              &DiskReadStream::ReadCallback,
                              &lBlock,
                              miCurrentPriority);

        miInputBlock = (miInputBlock + 1) % miNumBlocks;
        ++miBlocksUsed;
        ++miPendingOperationCount;
        ++miInputRequestCount;

        miNextInputPosition += miBlockSize;
        if (miNextInputPosition == miLoopEnd)
            miNextInputPosition = miLoopStart;

        return true;
    }

    // =====================================================================================
    // ReadBlock @ 0x8265CE08 -- deliver liDataSize bytes from file position
    // liFilePosition out of the buffered ring, or kick Service and report "not yet".
    // =====================================================================================
    bool DiskReadStream::ReadBlock(s32 liFilePosition, void* lpData, s32 liDataSize)
    {
        RtlEnterCriticalSection(mMutex);

        CGS_ASSERT((liFilePosition % miBlockSize) == 0,
                   "Must specify file pos that is multiple of stream block size\n");
        CGS_ASSERT((liDataSize % miBlockSize) == 0,
                   "Must specify read blocks size that is multiple of stream block size\n");

        bool lbNeedsService = false;

        // Drop leading buffered slots that no longer hold the requested position.
        if (miBlocksUsed > 0)
        {
            while (true)
            {
                ReadStreamBlock& lBlock = maBlocks[miOutputBlock];
                if ((lBlock.muFlags & KU_RSBFLAG_FULL) == 0)
                {
                    lbNeedsService = true;
                    break;
                }
                if (lBlock.miFilePos == liFilePosition)
                    break;

                lBlock.muFlags = KU_RSBFLAG_EMPTY;
                --miBlocksUsed;
                miOutputBlock = (miOutputBlock + 1) % miNumBlocks;
                if (miBlocksUsed <= 0)
                    break;
            }
        }

        if (!lbNeedsService)
        {
            // Enough whole blocks buffered, and enough readable bytes within them?
            if (miBlocksUsed >= liDataSize / miBlockSize)
            {
                s32 liAvailable = 0;
                for (s32 li = 0; li < miBlocksUsed; ++li)
                {
                    ReadStreamBlock& lBlock = maBlocks[(miOutputBlock + li) % miNumBlocks];
                    if ((lBlock.muFlags & KU_RSBFLAG_FULL) == 0)
                        break;
                    liAvailable += lBlock.miDataEnd - lBlock.miDataStart;
                }

                if (liAvailable >= liDataSize)
                {
                    if (lpData != nullptr)
                    {
                        char* lpDest = static_cast<char*>(lpData);
                        for (s32 liOffset = 0; liOffset < liDataSize; liOffset += miBlockSize)
                        {
                            void* lpBlockData = nullptr;
                            s32   liBlockSize = 0;
                            bool  lbStarted   = StartAsyncReadInternal(&lpBlockData, &liBlockSize);
                            CGS_ASSERT(lbStarted, "Failed to read data\n");
                            CGS_ASSERT(liBlockSize >= miBlockSize, "Data too small\n");

                            XMemCpy(lpDest, lpBlockData, static_cast<size_t>(miBlockSize));
                            lpDest += miBlockSize;
                            StopAsyncReadInternal(miBlockSize);
                        }
                    }

                    RtlLeaveCriticalSection(mMutex);
                    return true;
                }
            }

            lbNeedsService = true;
        }

        // Not ready -- pump the service loop and report that the data must be streamed in.
        (void)lbNeedsService;
        Service();
        RtlLeaveCriticalSection(mMutex);
        return false;
    }

    // =====================================================================================
    // ReadCallback @ 0x8265A670 -- async read completion. The context is the ring slot;
    // its owner back-pointer is the stream that OnRead runs on.
    // =====================================================================================
    void DiskReadStream::ReadCallback(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpContext)
    {
        ReadStreamBlock* lpBlock = static_cast<ReadStreamBlock*>(lpContext);
        CGS_ASSERT(lpBlock != nullptr, "Invalid stream block\n");
        CGS_ASSERT(lpBlock->mpOwner != nullptr, "Invalid stream\n");
        lpBlock->mpOwner->OnRead(liResult, lHandle, luSize, lpContext);
    }

    // =====================================================================================
    // CloseCallback @ 0x82650F98 -- async close completion. The context is the stream.
    // =====================================================================================
    void DiskReadStream::CloseCallback(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpContext)
    {
        CGS_ASSERT(lpContext != nullptr, "Invalid stream\n");
        static_cast<DiskReadStream*>(lpContext)->OnClose(liResult, lHandle, luSize, lpContext);
    }
}
