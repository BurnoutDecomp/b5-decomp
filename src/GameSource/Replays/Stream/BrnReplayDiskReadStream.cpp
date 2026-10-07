#include "GameSource/Replays/Stream/BrnReplayDiskReadStream.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h" // DeviceManager, GetDeviceManager
#include <cstddef>
#include <windows.h>

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Control flow, the ring/loop arithmetic,
// the status flags and every assert message are taken from the X360 asm; the DecFIGS
// BrnReplayDiskReadStream.h DWARF supplies the member/enum names. See the header for the
// field-order/offset map. Raw-image holes are cited at their recovered bodies.

// --- Win32/X360 critical-section primitives (the lock embedded at object offset 0).
//     Declared here exactly as the sibling GPUDiskWriteStream.cpp does; the real bodies
//     come from the platform. RtlEnter/LeaveCriticalSection(this) is the inlined
//     mMutex Lock/Unlock. ---
extern "C" void RtlEnterCriticalSection(void* lpCriticalSection);
extern "C" void RtlLeaveCriticalSection(void* lpCriticalSection);
extern "C" long RtlInitializeCriticalSection(void* lpCriticalSection);

// Xbox block copy the ARTIST asm calls for the delivered read data (semantically memcpy).
extern "C" void* XMemCpy(void* lpDest, const void* lpSrc, size_t luSize);

namespace BrnReplays
{
    // Inlined by ReplayModule's C++ constructor, 827E0438..043C.
    DiskReadStream::DiskReadStream()
    {
        static_assert(sizeof(mMutex) >= sizeof(CRITICAL_SECTION), "native disk reader lock storage");
        RtlInitializeCriticalSection(mMutex);
    }

    // Inlined in ReplayModule::Construct 82656E20. Retain fields that this
    // expansion never writes; stream-open/reset owns their later initialization.
    void DiskReadStream::Construct()
    {
        miField13C = -1;
        meStatus = E_STATUS_CLOSED;
        for (s32 liBlock = 0; liBlock < KI_MAX_STREAM_BLOCKS; ++liBlock)
            maBlocks[liBlock].mpOwner = this;
        miLoopStart = 0;
        miLoopEnd = 0;
    }

    // ARTIST 82659E20..82659EB0 (raw image). Close is idempotent at the
    // request boundary: the original emits a channel-one warning for repeats.
    void DiskReadStream::Close()
    {
        RtlEnterCriticalSection(mMutex);
        if (meStatus != E_STATUS_CLOSED && !mbWaitingToClose)
        {
            mbWaitingToClose = true;
            Service();
        }
        else if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
        {
            *CgsDev::Log::gpDebugPrint
                << "WARNING: Attempt to close a stream that is already closed or closing - ignoring request\n";
        }
        RtlLeaveCriticalSection(mMutex);
    }

    // ARTIST 82653968..826539B4 (raw image). Closing uses the urgent file
    // priority, not the current read priority; submission does not clear state.
    bool DiskReadStream::SubmitCloseRequest()
    {
        CgsFileSystem::GetDeviceManager()->Close(mHandle, &DiskReadStream::CloseCallback,
                                                this, miUrgentPriority);
        ++miPendingOperationCount;
        return true;
    }

    // ARTIST 8265A340. The handle is unused. Completion size stays 64-bit
    // in the stream and is narrowed only when stored in the block's end cursor.
    void DiskReadStream::OnRead(s32 liResult, CgsFileSystem::Handle /*lHandle*/,
                                u64 luSize, void* lpContext)
    {
        RtlEnterCriticalSection(mMutex);
        CGS_ASSERT(meStatus != E_STATUS_CLOSED, "Incorrect stream state\n");
        CGS_ASSERT(miPendingOperationCount > 0,
                   "Received file event with 0 pending operations\n");
        ReadStreamBlock* lpBlock = static_cast<ReadStreamBlock*>(lpContext);
        const ptrdiff_t liBlockIndex = lpBlock - maBlocks;
        if (liBlockIndex < 0 || liBlockIndex >= KI_MAX_STREAM_BLOCKS)
        {
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Stream block " << static_cast<s32>(liBlockIndex) << " is out of range\n";
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
        switch (liResult)
        {
        case -2:
            meStatus = E_STATUS_ERROR;
            if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
                *CgsDev::Log::gpDebugPrint << "Warning: read failed on: " << macFileName << "\n";
            break;
        case -1:
            meStatus = E_STATUS_OPEN;
            if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
                *CgsDev::Log::gpDebugPrint << "Warning: read cancelled on: " << macFileName << "\n";
            break;
        case 0:
            muLastReadSize = luSize;
            lpBlock->miDataEnd = static_cast<s32>(muLastReadSize);
            lpBlock->muFlags = KU_RSBFLAG_FULL;
            if (muLastReadSize != static_cast<u64>(static_cast<s64>(miBlockSize)))
            {
                CGS_ASSERT(muLastReadSize == 0, "Expected file to be multiple of block size\n");
                lpBlock->muFlags |= KU_RSBFLAG_EOF;
                miNextInputPosition = 0;
            }
            break;
        default:
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liResult << "\n";
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            break;
        }
        --miPendingOperationCount;
        --miInputRequestCount;
        Service();
        RtlLeaveCriticalSection(mMutex);
    }

    // ARTIST 82650D70. Failure still closes the stream; cancellation returns
    // it to OPEN. An unhandled result asserts and retains the previous state.
    void DiskReadStream::OnClose(s32 liResult, CgsFileSystem::Handle /*lHandle*/,
                                 u64 /*luSize*/, void* /*lpContext*/)
    {
        RtlEnterCriticalSection(mMutex);
        CGS_ASSERT(meStatus != E_STATUS_CLOSED, "Incorrect stream state\n");
        CGS_ASSERT(miPendingOperationCount > 0,
                   "Received stream event with 0 pending operations\n");
        switch (liResult)
        {
        case -2:
            if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
                *CgsDev::Log::gpDebugPrint << "WARNING: Close failed on: " << macFileName << "\n";
            meStatus = E_STATUS_CLOSED;
            break;
        case -1:
            if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0)
                *CgsDev::Log::gpDebugPrint << "WARNING: Close cancelled on: " << macFileName << "\n";
            meStatus = E_STATUS_OPEN;
            break;
        case 0:
            meStatus = E_STATUS_CLOSED;
            break;
        default:
            CgsDev::Assert::BeginAssert();
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liResult << "\n";
            CgsDev::Assert::FireAssert(lacMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
            break;
        }
        --miPendingOperationCount;
        RtlLeaveCriticalSection(mMutex);
    }

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
        lpDeviceManager->Read(mHandle,
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

    // =====================================================================================
    // ChopEOFBlocks @ 0x8264DCD0 -- retire fully-consumed FULL slots at the ring front.
    // =====================================================================================
    void DiskReadStream::ChopEOFBlocks()
    {
        while (miBlocksUsed > 0)
        {
            ReadStreamBlock& lBlock = maBlocks[miOutputBlock];
            if ((lBlock.muFlags & KU_RSBFLAG_FULL) == 0)
                break;
            if (lBlock.miDataEnd != lBlock.miDataStart)
                break;

            lBlock.muFlags = KU_RSBFLAG_EMPTY;
            --miBlocksUsed;
            miOutputBlock = (miOutputBlock + 1) % miNumBlocks;
        }
    }

    // =====================================================================================
    // GetAmountOfDataInBuffer @ 0x82650CB8 -- readable bytes held across the contiguous run
    // of FULL slots from the output cursor.
    // =====================================================================================
    s32 DiskReadStream::GetAmountOfDataInBuffer()
    {
        RtlEnterCriticalSection(mMutex);

        s32 liAmount = 0;
        if (miBlocksUsed > 0)
        {
            s32 li = 0;
            do
            {
                ReadStreamBlock& lBlock = maBlocks[(miOutputBlock + li) % miNumBlocks];
                if ((lBlock.muFlags & KU_RSBFLAG_FULL) == 0)
                    break;
                ++li;
                liAmount += lBlock.miDataEnd - lBlock.miDataStart;
            }
            while (li < miBlocksUsed);
        }

        RtlLeaveCriticalSection(mMutex);
        return liAmount;
    }

    // =====================================================================================
    // GetCurrentFilePriority @ 0x826526E0 -- pick the relaxed priority while the buffer is
    // serviced or at least a quarter full, otherwise the urgent priority.
    // =====================================================================================
    s32 DiskReadStream::GetCurrentFilePriority()
    {
        if (mbServiced)
            return miNormalPriority;

        double lfFraction = static_cast<double>(static_cast<u32>(GetAmountOfDataInBuffer()))
                          / static_cast<double>(miBufferSize);
        if (lfFraction >= 0.25)
            return miNormalPriority;

        return miUrgentPriority;
    }

    // =====================================================================================
    // StartAsyncReadInternal @ 0x8264DD80 -- lock the contiguous run of loaded blocks at the
    // output cursor for direct reading, handing back a pointer/size, or report "not ready".
    // =====================================================================================
    bool DiskReadStream::StartAsyncReadInternal(void** lppData, s32* lpiSize)
    {
        CGS_ASSERT(meStatus == E_STATUS_OPEN, "Can only start a read if open\n");
        CGS_ASSERT(!mbLockedForRead, "Already reading data\n");

        *lppData = nullptr;
        *lpiSize = 0;

        if (miBlocksUsed == 0 || mbAdjustingRange)
            return false;

        ChopEOFBlocks();

        // Scan forward (no wrap) over the run of FULL slots starting at the output cursor.
        s32 liEnd = miOutputBlock;
        while (liEnd < miNumBlocks && (maBlocks[liEnd].muFlags & KU_RSBFLAG_FULL) != 0)
            ++liEnd;

        if (liEnd == miOutputBlock)
            return false;

        s32 liAvailable = 0;
        for (s32 li = miOutputBlock; li < liEnd; ++li)
            liAvailable += maBlocks[li].miDataEnd - maBlocks[li].miDataStart;

        if (liAvailable == 0)
            return false;

        ReadStreamBlock& lBlock = maBlocks[miOutputBlock];
        *lppData = mpBuffer + lBlock.miStreamPos + lBlock.miDataStart;
        *lpiSize = liAvailable;
        mbLockedForRead = true;
        return true;
    }

    // =====================================================================================
    // StopAsyncReadInternal @ 0x8265A000 -- release liBlockSize bytes previously locked by
    // StartAsyncReadInternal, retiring whole slots and partially advancing the last, then
    // advance the read position and re-service.
    // =====================================================================================
    void DiskReadStream::StopAsyncReadInternal(s32 liBlockSize)
    {
        CGS_ASSERT(meStatus == E_STATUS_OPEN, "Can only start a read if open\n");
        CGS_ASSERT(mbLockedForRead, "Already reading data\n");
        CGS_ASSERT(miBlocksUsed > 0, "No blocks used but somehow started a read operation?\n");

        s32 liRemaining = liBlockSize;
        if (liBlockSize > 0)
        {
            bool lbPartial = true;
            while (true)
            {
                CGS_ASSERT(miOutputBlock < miNumBlocks, "Gone past end of buffer\n");
                ReadStreamBlock& lBlock = maBlocks[miOutputBlock];
                CGS_ASSERT((lBlock.muFlags & KU_RSBFLAG_FULL) != 0, "Block not loaded\n");

                s32 liBlockRemaining = lBlock.miDataEnd - lBlock.miDataStart;
                if (liBlockRemaining > liRemaining)
                    break;

                liRemaining -= liBlockRemaining;
                lBlock.miDataStart = lBlock.miDataEnd;
                lBlock.muFlags     = KU_RSBFLAG_EMPTY;
                ++miOutputBlock;
                --miBlocksUsed;
                if (liRemaining <= 0)
                {
                    lbPartial = false;
                    break;
                }
            }

            if (lbPartial)
                maBlocks[miOutputBlock].miDataStart += liRemaining;
        }

        miOutputBlock   = miOutputBlock % miNumBlocks;
        mbLockedForRead = false;
        miReadPosition += static_cast<u32>(liBlockSize);

        Service();
    }
}
