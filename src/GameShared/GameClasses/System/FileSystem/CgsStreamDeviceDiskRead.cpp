// CgsStreamDeviceDiskRead.cpp — the async read-stream engine (CgsFileSystem::StreamDeviceDiskRead).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (the assembly is ground truth; Hex-Rays dword
// offsets were NOT trusted — every field access below was mapped from the addis/ld/std/stb
// displacements to the DWARF-ordered member it lands on) + the DecFIGS DWARF for shape.
//   Construct              0x828DCDA8   Open                    0x82903F48
//   Close                  0x82900238   Seek                    0x829002D0
//   Read                   0x829041B8   GetAmountOfDataInBuffer  0x828DCDE0
//   ResetStreamBlocks      0x828E8D90   Service                 0x82900100
//   SubmitReadRequest      0x828F8CD0   SubmitCloseRequest      0x828F8F00
//   StartAsyncReadInternal 0x828E8F50   StopAsyncReadInternal   0x82900400
//   OnOpen                 0x82900740   OnRead                  0x829009C0
//   OnClose                0x828E9168
//   OpenCallback           0x82900CF8   ReadCallback  0x82900DA8   CloseCallback 0x828E93C8
//
// The stream owns a ring of KI_MAX_STREAM_BLOCKS blocks carved out of a single caller buffer.
// The INPUT cursor (miInputBlock/muInputPosition) submits block-sized device reads that fill
// blocks (KU_RSBFLAG_LOADING -> LOADED via OnRead); the OUTPUT cursor (miOutputBlock) is drained
// by StartAsyncReadInternal/StopAsyncReadInternal as the consumer reads. Service() is the pump:
// after every state change it (re)issues reads / a close as the pending-op count allows. All of
// this runs under mFutex, taken by the public entry points (Close/Seek/Open/Read/OnRead/
// GetAmountOfDataInBuffer); the internal helpers assume the lock is already held.

#include "GameShared/GameClasses/System/FileSystem/CgsStreamDeviceDiskRead.h"

#include "GameShared/GameClasses/System/FileSystem/CgsFile.h"           // FileFutexHelper
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"  // DeviceManager / GetDeviceManager / Handle
#include "GameShared/GameClasses/Core/CgsAssert.h"                      // CGS_ASSERT / Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Development/CgsStrStream.h"            // CgsDev::StrStream (dynamic assert messages)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "rw/core/stdc/stdc.h"                                          // rw::core::stdc::StringnCopy

#include <cstring>   // memcpy — models the X360 XMemCpy block copy

namespace CgsFileSystem
{
    // Build a formatted assert message via StrStream (as the X360 did with the shared assert
    // buffer) and fire it. Used for the asserts whose text embeds a runtime value; the plain-text
    // asserts use CGS_ASSERT directly.
    namespace
    {
        void FireFormattedAssert(const char* lpcMessage)
        {
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert(lpcMessage, __FILE__, __LINE__);
            CgsDev::Assert::EndAssert();
        }
    }

    // Construct @ 0x828DCDA8 — record the owner FileSystem/FileLog, mark the stream closed with no
    // log slot, and back-point every block at its owning stream (used by ReadCallback to recover
    // the stream from the block context). Note the loop runs the full KI_MAX_STREAM_BLOCKS, not
    // miNumBlocks (which Open sets later).
    void StreamDeviceDiskRead::Construct(FileSystem* lpFileSystem, FileLog* lpLog)
    {
        mpFileSystem = lpFileSystem;
        mpLog        = lpLog;
        miLogIndex   = -1;
        meStatus     = E_STATUS_CLOSED;

        for (s32 liIndex = 0; liIndex < KI_MAX_STREAM_BLOCKS; ++liIndex)
            maBlocks[liIndex].mpOwner = this;
    }

    // Open @ 0x82903F48 — configure the stream for luNumBlocks blocks over the caller buffer, reset
    // the ring, and kick off the async device open (completion -> OpenCallback -> OnOpen).
    void StreamDeviceDiskRead::Open(const char* lpcName, void* lpBuffer, u32 luBufferSize,
                                    u32 luNumBlocks, s32 liNormalPriority, s32 liHighPriority,
                                    bool lbPrintToLog, s32 liLogIndex)
    {
        miLogIndex = liLogIndex;
        rw::core::stdc::StringnCopy(macFileName, lpcName, 255);

        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT((luBufferSize % luNumBlocks) == 0,
                   "Buffer size must be divisible by number of blocks\n");

        mpBuffer         = static_cast<char*>(lpBuffer);
        miBufferSize     = static_cast<s32>(luBufferSize);
        miNumBlocks      = static_cast<s32>(luNumBlocks);
        miBlockSize      = static_cast<s32>(luBufferSize / luNumBlocks);
        miNormalPriority = liNormalPriority;
        miHighPriority   = liHighPriority;
        mbPrintToLog     = lbPrintToLog;

        if (static_cast<s32>(luNumBlocks) > KI_MAX_STREAM_BLOCKS)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Maximum blocks per stream is " << KI_MAX_STREAM_BLOCKS
                     << " but you chose " << miNumBlocks << "\n";
            FireFormattedAssert(lacMessage);
        }

        muOutputPosition        = 0;
        muInputPosition         = 0;
        mbIsOutputing           = false;
        miInputRequestsPending  = 0;
        muLastOperationSize     = 0;
        miPendingOperationCount = 0;
        miNextPriority          = miNormalPriority;
        muCacheId               = 0xFFFFFFFFu;   // -1: no disk-cache entry
        mpDEBUGCacheEntry       = nullptr;

        ResetStreamBlocks();

        if (lbPrintToLog)
            CGS_ASSERT(mpFileSystem != nullptr, "mpFileSystem\n");

        GetDeviceManager()->Open(macFileName, 1, &StreamDeviceDiskRead::OpenCallback, this, miHighPriority);

        ++miPendingOperationCount;
        meStatus         = E_STATUS_OPENING;
        mbWaitingToClose = false;
        mbWaitingToSeek  = false;
    }

    // Close @ 0x82900238 — request a close (serviced asynchronously); warn (gated by the message
    // filter) if the stream is already closed or a close is already pending.
    void StreamDeviceDiskRead::Close()
    {
        FileFutexHelper lGuard(mFutex);

        if (meStatus == E_STATUS_CLOSED || mbWaitingToClose)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint
                    << "WARNING: Attempt to close a stream that is already closed or closing - ignoring request\n";
        }
        else
        {
            mbWaitingToClose = true;
            Service();
        }
    }

    // Seek @ 0x829002D0 — reposition the input/output cursors and flag a pending reset; the actual
    // ring reset happens in Service (only valid on an OPEN stream not currently locked for read).
    void StreamDeviceDiskRead::Seek(u64 luPosition)
    {
        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meStatus == E_STATUS_OPEN, "Invalid state to do seek\n");
        CGS_ASSERT(!mbIsOutputing, "Can not seek while locked for read\n");

        mbWaitingToSeek  = true;
        muInputPosition  = luPosition;
        muOutputPosition = luPosition;
        Service();
    }

    // OnOpen @0x82900740 — consume the device-open completion, retaining the compound
    // logical-device/device-handle pair on success. Every arm matches ARTIST's state update;
    // the handler always retires the pending open and re-enters Service.
    void StreamDeviceDiskRead::OnOpen(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        (void)luSize;
        (void)lpContext;

        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meStatus == E_STATUS_OPENING, "Incorrect stream state\n");
        CGS_ASSERT(miPendingOperationCount > 0,
                   "Received stream event with 0 pending operations\n");

        if (liResult == -2)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "WARNING: Open failed on: "
                                           << macFileName << "\n";
            meStatus = E_STATUS_CLOSED;
            mbWaitingToClose = false;
            miPendingOperationCount = 0;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "WARNING: Failed to open file '"
                                           << macFileName << "'";
        }
        else if (liResult == -1)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "WARNING: Open cancelled on: "
                                           << macFileName << "\n";
            meStatus = E_STATUS_CLOSED;
        }
        else if (liResult != 0)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liResult << "\n";
            FireFormattedAssert(lacMessage);
        }
        else
        {
            mHandle = lHandle;
            meStatus = E_STATUS_OPEN;
        }

        --miPendingOperationCount;
        Service();
    }

    // Read @ 0x829041B8 — synchronous read of luAmount bytes out of the streamed ring into lpDest
    // (lpDest may be null to just consume). Loops draining loaded blocks until the request is
    // satisfied or the stream runs dry; returns the number of bytes delivered (luAmount on full
    // success, the partial count if the stream ended first).
    u32 StreamDeviceDiskRead::Read(u32 luAmount, void* lpDest)
    {
        FileFutexHelper lGuard(mFutex);

        u32   luBytesRead = 0;
        u32   luRemaining = luAmount;
        void* lpData      = nullptr;
        u32   luAvailable = 0;

        if (StartAsyncReadInternal(&lpData, &luAvailable))
        {
            bool lbComplete = true;
            while (luAvailable < luRemaining)
            {
                if (lpDest)
                    memcpy(static_cast<char*>(lpDest) + luBytesRead, lpData, luAvailable);
                StopAsyncReadInternal(luAvailable);
                luBytesRead += luAvailable;
                luRemaining -= luAvailable;
                if (!StartAsyncReadInternal(&lpData, &luAvailable))
                {
                    lbComplete = false;
                    break;
                }
            }

            if (lbComplete)
            {
                if (lpDest)
                    memcpy(static_cast<char*>(lpDest) + luBytesRead, lpData, luRemaining);
                StopAsyncReadInternal(luRemaining);
                luBytesRead = luAmount;
            }
        }

        return luBytesRead;
    }

    // GetAmountOfDataInBuffer @ 0x828DCDE0 — total contiguously-loaded, not-yet-consumed bytes,
    // walking loaded blocks forward from the output cursor.
    u32 StreamDeviceDiskRead::GetAmountOfDataInBuffer() const
    {
        FileFutexHelper lGuard(const_cast<EA::Thread::Futex&>(mFutex));

        s32 liTotalAvailable = 0;
        s32 liBlockCount     = 0;
        if (miBlocksUsed > 0)
        {
            do
            {
                s32 liIndex = (miOutputBlock + liBlockCount) % miNumBlocks;
                const ReadStreamBlock& lrBlock = maBlocks[liIndex];
                if ((lrBlock.muFlags & KU_RSBFLAG_LOADED) == 0)
                    break;
                ++liBlockCount;
                liTotalAvailable += lrBlock.miWritten - lrBlock.miRead;
            }
            while (liBlockCount < miBlocksUsed);
        }

        return static_cast<u32>(liTotalAvailable);
    }

    // GetCurrentFilePriority @ 0x828E0F70 — pick the streaming priority for the next device read: at
    // end-of-file (nothing more to fetch) run at normal priority; otherwise run at normal priority
    // while the buffer is at least a quarter full, and escalate to the high priority once it drains
    // below 25% so a starving consumer catches up. (ARTIST: mbIsEndOfFile byte-test @0x588, then the
    // signed miBufferSize @0x130 as the denominator and the zero-extended GetAmountOfDataInBuffer
    // result as the numerator, compared against 0.25.)
    s32 StreamDeviceDiskRead::GetCurrentFilePriority()
    {
        if (mbIsEndOfFile)
            return miNormalPriority;

        f64 lfCapacity = static_cast<f64>(miBufferSize);
        f64 lfInBuffer = static_cast<f64>(GetAmountOfDataInBuffer());
        if (lfInBuffer / lfCapacity >= 0.25)
            return miNormalPriority;

        return miHighPriority;
    }

    // ResetStreamBlocks @ 0x828E8D90 — clear the ring back to empty and re-seat each block's fixed
    // buffer slot (miStreamPos = index * blockSize). Only valid when nothing is in flight.
    void StreamDeviceDiskRead::ResetStreamBlocks()
    {
        CGS_ASSERT(!mbIsOutputing, "Can not reset stream while locked for read\n");
        CGS_ASSERT(miInputRequestsPending == 0, "Can not reset while input requests are occuring\n");
        CGS_ASSERT(miPendingOperationCount == 0, "Can not reset while pending operations are occuring\n");

        for (s32 liIndex = 0; liIndex < miNumBlocks; ++liIndex)
        {
            ReadStreamBlock& lrBlock = maBlocks[liIndex];
            lrBlock.muFilePos   = 0;
            lrBlock.miWritten   = 0;
            lrBlock.miRead      = 0;
            lrBlock.muFlags     = KU_RSBFLAG_EMPTY;
            lrBlock.miStreamPos = liIndex * miBlockSize;
        }

        miBlocksUsed    = 0;
        mbIsEndOfFile   = false;
        miOutputBlock   = 0;
        miInputBlock    = 0;
        mbWaitingToSeek = false;
    }

    // Service @ 0x82900100 — the ring pump. Runs when a seek/close is pending or the stream has not
    // hit EOF. Refreshes the current priority, then (as long as no more than one op is in flight)
    // either finishes a pending close, applies a pending seek-reset, or issues the next read(s).
    void StreamDeviceDiskRead::Service()
    {
        if (mbWaitingToSeek || mbWaitingToClose || !mbIsEndOfFile)
        {
            miNextPriority = GetCurrentFilePriority();

            if (miPendingOperationCount <= 1)
            {
                if (miPendingOperationCount == 1)
                {
                    if (!mbWaitingToClose && !mbWaitingToSeek &&
                        meStatus != E_STATUS_CLOSED && meStatus != E_STATUS_ERROR)
                    {
                        SubmitReadRequest();
                    }
                }
                else if (mbWaitingToClose)
                {
                    if (SubmitCloseRequest())
                        mbWaitingToClose = false;
                }
                else
                {
                    if (mbWaitingToSeek)
                    {
                        ResetStreamBlocks();
                        mbWaitingToSeek = false;
                    }

                    if (meStatus != E_STATUS_CLOSED && meStatus != E_STATUS_ERROR)
                    {
                        ++miNextPriority;
                        SubmitReadRequest();
                        --miNextPriority;
                        SubmitReadRequest();
                    }
                }
            }
        }
    }

    // SubmitReadRequest @ 0x828F8CD0 — if a ring block is free, queue a block-sized device read to
    // fill the input block (completion -> ReadCallback -> OnRead) and advance the input cursor.
    // Returns whether a read was queued.
    bool StreamDeviceDiskRead::SubmitReadRequest()
    {
        if (miBlocksUsed >= miNumBlocks)
            return false;

        CGS_ASSERT(meStatus == E_STATUS_OPEN, "Attempting to service a none-open stream\n");
        CGS_ASSERT(maBlocks[miInputBlock].muFlags == KU_RSBFLAG_EMPTY,
                   "Should never be loading into a none-empty block\n");

        ReadStreamBlock& lrBlock = maBlocks[miInputBlock];
        lrBlock.muFilePos = muInputPosition;
        lrBlock.miWritten = 0;
        lrBlock.miRead    = 0;
        lrBlock.muFlags   = KU_RSBFLAG_LOADING;

        void* lpBlockBuffer = mpBuffer + lrBlock.miStreamPos;
        GetDeviceManager()->Read(mHandle, muInputPosition, lpBlockBuffer,
                                 static_cast<u32>(miBlockSize),
                                 &StreamDeviceDiskRead::ReadCallback, &lrBlock, miNextPriority);

        miInputBlock = (miInputBlock + 1) % miNumBlocks;
        ++miBlocksUsed;
        ++miPendingOperationCount;
        ++miInputRequestsPending;
        muInputPosition += static_cast<u64>(miBlockSize);
        return true;
    }

    // SubmitCloseRequest @ 0x828F8F00 — issue the async device close (completion -> CloseCallback
    // -> OnClose) and count the op. When a disk-cache entry is bound and EOF has not been reached,
    // instead just reset the ring and re-pump reads (no close needed). Returns whether a close was
    // actually queued.
    bool StreamDeviceDiskRead::SubmitCloseRequest()
    {
        if (muCacheId != 0xFFFFFFFFu && !mbIsEndOfFile)
        {
            ResetStreamBlocks();
            SubmitReadRequest();
            return false;
        }

        GetDeviceManager()->Close(mHandle, &StreamDeviceDiskRead::CloseCallback, this, miHighPriority);
        ++miPendingOperationCount;
        return true;
    }

    // StartAsyncReadInternal @ 0x828E8F50 — expose the current run of loaded, unconsumed data as a
    // (pointer, length) pair and lock the stream for reading. Returns false (and zeroes the outputs)
    // when nothing is available. The lock is released by StopAsyncReadInternal.
    bool StreamDeviceDiskRead::StartAsyncReadInternal(void** lppData, u32* lpuAmount)
    {
        CGS_ASSERT(meStatus == E_STATUS_OPEN, "Can only start a read if open\n");
        CGS_ASSERT(!mbIsOutputing, "Already reading data\n");

        *lppData   = nullptr;
        *lpuAmount = 0;

        if (miBlocksUsed == 0 || mbWaitingToSeek)
            return false;

        // Scan forward from the output cursor over the contiguous run of loaded blocks.
        s32 liBlock = miOutputBlock;
        while (liBlock < miNumBlocks)
        {
            if ((maBlocks[liBlock].muFlags & KU_RSBFLAG_LOADED) == 0)
                break;
            ++liBlock;
        }
        if (liBlock == miOutputBlock)
            return false;

        s32 liLastBlock = liBlock - 1;
        if (miOutputBlock > liLastBlock)
            return false;

        s32 liTotalToRead = 0;
        for (s32 liIndex = miOutputBlock; liIndex <= liLastBlock; ++liIndex)
            liTotalToRead += maBlocks[liIndex].miWritten - maBlocks[liIndex].miRead;

        if (liTotalToRead == 0)
            return false;

        ReadStreamBlock& lrBlock = maBlocks[miOutputBlock];
        *lppData   = mpBuffer + lrBlock.miStreamPos + lrBlock.miRead;
        *lpuAmount = static_cast<u32>(liTotalToRead);
        mbIsOutputing = true;
        return true;
    }

    // StopAsyncReadInternal @ 0x82900400 — consume luAmount bytes previously exposed by
    // StartAsyncReadInternal: advance each fully-drained block's read cursor, empty it, and step
    // the output cursor; a partial final block just advances its read cursor. Unlocks reading,
    // advances the output position, and re-pumps via Service.
    void StreamDeviceDiskRead::StopAsyncReadInternal(u32 luAmount)
    {
        CGS_ASSERT(meStatus == E_STATUS_OPEN, "Can only start a read if open\n");
        CGS_ASSERT(mbIsOutputing, "Already reading data\n");
        CGS_ASSERT(miBlocksUsed > 0, "No blocks used but somehow started a read operation?\n");

        s32 liRemaining = static_cast<s32>(luAmount);
        if (liRemaining > 0)
        {
            for (;;)
            {
                CGS_ASSERT(miOutputBlock < miNumBlocks, "Gone past end of buffer\n");
                CGS_ASSERT((maBlocks[miOutputBlock].muFlags & KU_RSBFLAG_LOADED) != 0,
                           "Block not loaded\n");

                ReadStreamBlock& lrBlock = maBlocks[miOutputBlock];
                s32 liBlockRemaining = lrBlock.miWritten - lrBlock.miRead;
                if (liBlockRemaining > liRemaining)
                {
                    lrBlock.miRead += liRemaining;
                    break;
                }

                liRemaining -= liBlockRemaining;
                lrBlock.miRead  = lrBlock.miWritten;
                lrBlock.muFlags = KU_RSBFLAG_EMPTY;
                ++miOutputBlock;
                --miBlocksUsed;
                if (liRemaining <= 0)
                    break;
            }
        }

        miOutputBlock    = miOutputBlock % miNumBlocks;
        mbIsOutputing    = false;
        muOutputPosition += static_cast<u64>(luAmount);
        Service();
    }

    // OnRead @ 0x829009C0 — device read-completion handler (from ReadCallback). Validates state and
    // the block, then: on failure (-2) flags the stream ERROR, on cancel (-1) returns it to OPEN,
    // and on success records the bytes read into the block, marks it LOADED (and EOF when short),
    // then decrements the outstanding counts and re-pumps via Service.
    void StreamDeviceDiskRead::OnRead(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        (void)lHandle;   // the device handle is not used by the read completion

        FileFutexHelper lGuard(mFutex);

        ReadStreamBlock* lpBlock = static_cast<ReadStreamBlock*>(lpContext);

        CGS_ASSERT(meStatus != E_STATUS_CLOSED, "Incorrect stream state\n");
        CGS_ASSERT(miPendingOperationCount > 0, "Received file event with 0 pending operations\n");

        s32 liBlockIndex = static_cast<s32>(lpBlock - maBlocks);
        if (liBlockIndex < 0 || liBlockIndex >= KI_MAX_STREAM_BLOCKS)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Stream block " << liBlockIndex << " is out of range\n";
            FireFormattedAssert(lacMessage);
        }

        if (liResult == -2)
        {
            meStatus = E_STATUS_ERROR;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "Warning: read failed on: " << macFileName << "\n";
        }
        else if (liResult == -1)
        {
            meStatus = E_STATUS_OPEN;
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "Warning: read cancelled on: " << macFileName << "\n";
        }
        else if (liResult != 0)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liResult << "\n";
            FireFormattedAssert(lacMessage);
        }
        else
        {
            muLastOperationSize = luSize;
            lpBlock->miWritten  = static_cast<s32>(muLastOperationSize);
            lpBlock->muFlags    = KU_RSBFLAG_LOADED;

            // A short read (less than a full block) is end-of-file.
            if (muLastOperationSize != static_cast<u64>(miBlockSize))
            {
                mbIsEndOfFile   = true;
                lpBlock->muFlags |= KU_RSBFLAG_EOF;
            }

            if (muCacheId != 0xFFFFFFFFu)
            {
                CGS_ASSERT(mpFileSystem != nullptr, "mpFileSystem\n");
                // (X360 debug disk-cache-entry bookkeeping here compiled to dead loads with no
                // observable store in ARTIST — nothing to reproduce.)
            }
        }

        --miPendingOperationCount;
        --miInputRequestsPending;
        Service();
    }

    // OnClose @0x828E9168 — retire the queued close and return the stream to CLOSED on
    // success/failure, or OPEN when the operation was cancelled. The cache-end side effect in
    // ARTIST is conditional on a live cache id; the PC path never assigns one.
    void StreamDeviceDiskRead::OnClose(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        (void)lHandle;
        (void)luSize;
        (void)lpContext;

        FileFutexHelper lGuard(mFutex);

        CGS_ASSERT(meStatus != E_STATUS_CLOSED, "Incorrect stream state\n");
        CGS_ASSERT(miPendingOperationCount > 0,
                   "Received stream event with 0 pending operations\n");

        if (liResult == -2)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "WARNING: Close failed on: "
                                           << macFileName << "\n";
            meStatus = E_STATUS_CLOSED;
        }
        else if (liResult == -1)
        {
            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0)
                *CgsDev::Log::gpDebugPrint << "WARNING: Close cancelled on: "
                                           << macFileName << "\n";
            meStatus = E_STATUS_OPEN;
        }
        else if (liResult != 0)
        {
            char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lMessage(lacMessage, sizeof(lacMessage));
            lMessage << "Unhandled response: " << liResult << "\n";
            FireFormattedAssert(lacMessage);
        }
        else
        {
            meStatus = E_STATUS_CLOSED;
            if (muCacheId != 0xFFFFFFFFu)
                CGS_ASSERT(mpFileSystem != nullptr, "mpFileSystem\n");
        }

        --miPendingOperationCount;
    }

    // OpenCallback @ 0x82900CF8 — device open-completion trampoline: forward to OnOpen on the owning
    // stream (the context), then assert the context was valid (matching ARTIST's call order).
    void StreamDeviceDiskRead::OpenCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        StreamDeviceDiskRead* lpStream = static_cast<StreamDeviceDiskRead*>(lpContext);
        lpStream->OnOpen(liResult, lHandle, luSize, lpContext);
        CGS_ASSERT(lpContext != nullptr, "Invalid stream\n");
    }

    // ReadCallback @ 0x82900DA8 — device read-completion trampoline: the context is the block; its
    // mpOwner is the stream OnRead is dispatched on.
    void StreamDeviceDiskRead::ReadCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        ReadStreamBlock* lpBlock = static_cast<ReadStreamBlock*>(lpContext);

        CGS_ASSERT(lpContext != nullptr, "Invalid stream block\n");
        CGS_ASSERT(lpBlock->mpOwner != nullptr, "Invalid stream\n");

        lpBlock->mpOwner->OnRead(liResult, lHandle, luSize, lpContext);
    }

    // CloseCallback @ 0x828E93C8 — device close-completion trampoline: assert the context is valid,
    // then forward to OnClose on the owning stream.
    void StreamDeviceDiskRead::CloseCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        StreamDeviceDiskRead* lpStream = static_cast<StreamDeviceDiskRead*>(lpContext);

        CGS_ASSERT(lpContext != nullptr, "Invalid stream\n");
        lpStream->OnClose(liResult, lHandle, luSize, lpContext);
    }

} // namespace CgsFileSystem
