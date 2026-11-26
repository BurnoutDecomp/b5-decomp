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

#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"  // DeviceManager / GetDeviceManager / Handle
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                      // CGS_ASSERT / Assert::{Begin,Fire,End}Assert
#include "GameShared/GameClasses/Development/CgsStrStream.h"            // CgsDev::StrStream (dynamic assert messages)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // Log::gpDebugPrint / Message::gxMessageFilterFlags
#include "rw/core/stdc/stdc.h"                                          // rw::core::stdc::StringnCopy

#include <cstring>   // memcpy — models the X360 XMemCpy block copy

#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\system\\filesystem\\CgsStreamDeviceDiskRead.cpp"

namespace CgsFileSystem
{
    namespace
    {
        // Asserts that embed a runtime value use the shared formatted assertion path.
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
        miLogIndex   = -1;            // first: ARTIST materialises the -1 before any store (@0x828DCDA8)
        mpFileSystem = lpFileSystem;
        mpLog        = lpLog;
        meStatus     = E_STATUS_CLOSED;

        for (s32 liIndex = 0; liIndex < KI_MAX_STREAM_BLOCKS; ++liIndex)
            maBlocks[liIndex].mpOwner = this;
    }










































    // Open @ 0x82903F48 — configure the stream for luNumBlocks blocks over the caller buffer, reset
    // the ring, and kick off the async device open (completion -> OpenCallback -> OnOpen).
    void StreamDeviceDiskRead::Open(const char* lpcName, void* lpBuffer, u32 luBufferSize,
                                    u32 luNumBlocks, s32 liNormalPriority, s32 liHighPriority,
                                    bool lbUseDiskCache, s32 liLogIndex)
    {
        miLogIndex = liLogIndex;
        rw::core::stdc::StringnCopy(macFileName, lpcName, 255);

        StreamLockGuard lGuard(mFutex.mCriticalSection);


















        CGS_ASSERT_STREAM_AT((luBufferSize % luNumBlocks) == 0, "Buffer size must be divisible by number of blocks\n", CGS_ARTIST_FILE, 148);

        mpBuffer         = static_cast<char*>(lpBuffer);
        miBufferSize     = static_cast<s32>(luBufferSize);
        miNumBlocks      = static_cast<s32>(luNumBlocks);
        miBlockSize      = miBufferSize / miNumBlocks;   // signed: ARTIST divw @0x82904028
        miNormalPriority = liNormalPriority;
        miHighPriority   = liHighPriority;


        CGS_ASSERT_STREAM_AT(static_cast<s32>(luNumBlocks) <= KI_MAX_STREAM_BLOCKS, "Maximum blocks per stream is " << KI_MAX_STREAM_BLOCKS << " but you chose " << miNumBlocks << "\n", CGS_ARTIST_FILE, 158);

        muOutputPosition        = 0;
        muInputPosition         = 0;
        mbIsOutputing           = false;
        miInputRequestsPending  = 0;
        muLastOperationSize     = 0;
        miPendingOperationCount = 0;
        miNextPriority          = miNormalPriority;
        muCacheId               = 0xFFFFFFFFu;   // -1: no disk-cache entry
        mpDEBUGCacheEntry       = 0;

        ResetStreamBlocks();

        if (lbUseDiskCache)










            CGS_ASSERT_AT(mpFileSystem != 0, "mpFileSystem\n", CGS_ARTIST_FILE, 183);

        GetDeviceManager()->Open(macFileName, 1, &StreamDeviceDiskRead::OpenCallback, this, miHighPriority);

        ++miPendingOperationCount;
        meStatus         = E_STATUS_OPENING;
        mbWaitingToClose = false;
        mbWaitingToSeek  = false;
    }











































    // ResetStreamBlocks @ 0x828E8D90 — clear the ring back to empty and re-seat each block's fixed
    // buffer slot (miStreamPos = index * blockSize). Only valid when nothing is in flight.
    void StreamDeviceDiskRead::ResetStreamBlocks()
    {


        CGS_ASSERT_STREAM_AT(!mbIsOutputing, "Can not reset stream while locked for read\n", CGS_ARTIST_FILE, 241);
        CGS_ASSERT_STREAM_AT(miInputRequestsPending == 0, "Can not reset while input requests are occuring\n", CGS_ARTIST_FILE, 242);
        CGS_ASSERT_STREAM_AT(miPendingOperationCount == 0, "Can not reset while pending operations are occuring\n", CGS_ARTIST_FILE, 243);


        for (s32 liIndex = 0; liIndex < miNumBlocks; ++liIndex)
        {
            // ARTIST's volatile stores run muFilePos, muFlags, miRead, miWritten, miStreamPos
            // (@0x828E8F04..@0x828E8F20); DWARF has no block local, only liIndex (cpp:246).
            maBlocks[liIndex].muFilePos   = 0;
            maBlocks[liIndex].muFlags     = KU_RSBFLAG_EMPTY;
            maBlocks[liIndex].miRead      = 0;
            maBlocks[liIndex].miWritten   = 0;
            maBlocks[liIndex].miStreamPos = liIndex * miBlockSize;
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






        CGS_ASSERT_STREAM_AT(meStatus == E_STATUS_OPEN, "Attempting to service a none-open stream\n", CGS_ARTIST_FILE, 410);


        CGS_ASSERT_STREAM_AT(maBlocks[miInputBlock].muFlags == KU_RSBFLAG_EMPTY, "Should never be loading into a none-empty block\n", CGS_ARTIST_FILE, 413);

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




































    // Close @ 0x82900238 — request a close (serviced asynchronously); warn (gated by the message
    // filter) if the stream is already closed or a close is already pending.













    // Seek @ 0x829002D0 — reposition the input/output cursors and flag a pending reset; the actual
    // ring reset happens in Service (only valid on an OPEN stream not currently locked for read).
    void StreamDeviceDiskRead::Seek(u64 luPosition)
    {
        StreamLockGuard lGuard(mFutex.mCriticalSection);
        CGS_ASSERT_STREAM_AT(meStatus == E_STATUS_OPEN, "Invalid state to do seek\n", CGS_ARTIST_FILE, 558);
        CGS_ASSERT_STREAM_AT(!mbIsOutputing, "Can not seek while locked for read\n", CGS_ARTIST_FILE, 559);

        mbWaitingToSeek  = true;
        muInputPosition  = luPosition;
        muOutputPosition = luPosition;
        Service();
    }














    // GetAmountOfDataInBuffer @ 0x828DCDE0 — total contiguously-loaded, not-yet-consumed bytes,
    // walking loaded blocks forward from the output cursor.
    // Inlined by ARTIST StreamDataFunc at829046CC-D8: lock, read the
    // 64-bit output position at+0x570, unlock, then return the snapshot.
    u64 StreamDeviceDiskRead::Tell()
    {
        StreamLockGuard lGuard(mFutex.mCriticalSection);
        return muOutputPosition;
    }









    // Read @ 0x829041B8 — synchronous read of luAmount bytes out of the streamed ring into lpDest
    // (lpDest may be null to just consume). Loops draining loaded blocks until the request is
    // satisfied or the stream runs dry; returns the number of bytes delivered (luAmount on full
    // success, the partial count if the stream ended first).
    u32 StreamDeviceDiskRead::Read(u32 luAmount, void* lpDest)
    {
        u32   luBytesRead = 0;
        u32   luRemaining = luAmount;
        void* lpData;
        u32   luAvailable;

        StreamLockGuard lGuard(mFutex.mCriticalSection);

        // ARTIST @0x829041B8 copies through the XDK's XMemCpy (bl 0x82926BA0).
        while (StartAsyncReadInternal(&lpData, &luAvailable))
        {
            if (luAvailable >= luRemaining)
            {
                if (lpDest)
                    XMemCpy(static_cast<char*>(lpDest) + luBytesRead, lpData, luRemaining);
                StopAsyncReadInternal(luRemaining);
                luBytesRead = luAmount;
                break;
            }
            if (lpDest)
                XMemCpy(static_cast<char*>(lpDest) + luBytesRead, lpData, luAvailable);
            StopAsyncReadInternal(luAvailable);
            luBytesRead += luAvailable;
            luRemaining -= luAvailable;
        }
        return luBytesRead;
    }





































    // StartAsyncReadInternal @ 0x828E8F50 — expose the current run of loaded, unconsumed data as a
    // (pointer, length) pair and lock the stream for reading. Returns false (and zeroes the outputs)
    // when nothing is available. The lock is released by StopAsyncReadInternal.
    bool StreamDeviceDiskRead::StartAsyncReadInternal(void** lppBuffer, u32* lpuBufferSize)
    {





        s32 liIndex;    // DWARF: one function-scope index (cpp:677) walks both loops


        CGS_ASSERT_STREAM_AT(meStatus == E_STATUS_OPEN, "Can only start a read if open\n", CGS_ARTIST_FILE, 680);
        CGS_ASSERT_STREAM_AT(!mbIsOutputing, "Already reading data\n", CGS_ARTIST_FILE, 681);

        *lppBuffer     = 0;
        *lpuBufferSize = 0;

        if (miBlocksUsed == 0)
            return false;
        if (mbWaitingToSeek)
            return false;

        // Scan forward from the output cursor over the contiguous run of loaded blocks.
        for (liIndex = miOutputBlock; liIndex < miNumBlocks; ++liIndex)
        {
            if ((maBlocks[liIndex].muFlags & KU_RSBFLAG_LOADED) == 0)
                break;
        }
        if (liIndex == miOutputBlock)
            return false;





















        s32 liLastBlock   = liIndex - 1;
        s32 liTotalToRead = 0;
        for (liIndex = miOutputBlock; liIndex <= liLastBlock; ++liIndex)
            liTotalToRead += maBlocks[liIndex].miWritten - maBlocks[liIndex].miRead;

        if (liTotalToRead == 0)
            return false;

        // ARTIST reads the volatile miOutputBlock once per term (@0x828E911C, @0x828E9138).
        *lppBuffer     = mpBuffer + maBlocks[miOutputBlock].miStreamPos + maBlocks[miOutputBlock].miRead;
        *lpuBufferSize = static_cast<u32>(liTotalToRead);
        mbIsOutputing  = true;
        return true;
    }





















    // StopAsyncReadInternal @ 0x82900400 — consume luAmount bytes previously exposed by
    // StartAsyncReadInternal: advance each fully-drained block's read cursor, empty it, and step
    // the output cursor; a partial final block just advances its read cursor. Unlocks reading,
    // advances the output position, and re-pumps via Service.
    void StreamDeviceDiskRead::StopAsyncReadInternal(u32 luAmountRead)
    {



        CGS_ASSERT_STREAM_AT(meStatus == E_STATUS_OPEN, "Can only start a read if open\n", CGS_ARTIST_FILE, 764);
        CGS_ASSERT_STREAM_AT(mbIsOutputing, "Already reading data\n", CGS_ARTIST_FILE, 765);
        CGS_ASSERT_STREAM_AT(miBlocksUsed > 0, "No blocks used but somehow started a read operation?\n", CGS_ARTIST_FILE, 766);


        s32 liTotalRead = static_cast<s32>(luAmountRead);
        while (liTotalRead > 0)
        {



            CGS_ASSERT_STREAM_AT(miOutputBlock < miNumBlocks, "Gone past end of buffer\n", CGS_ARTIST_FILE, 775);
            CGS_ASSERT_STREAM_AT((maBlocks[miOutputBlock].muFlags & KU_RSBFLAG_LOADED) != 0, "Block not loaded\n", CGS_ARTIST_FILE, 776);

            // ARTIST re-indexes maBlocks[miOutputBlock] for every access (@0x82900654..@0x829006B8).
            s32 liRemainingInCurrentBlock = maBlocks[miOutputBlock].miWritten - maBlocks[miOutputBlock].miRead;
            if (liRemainingInCurrentBlock > liTotalRead)
            {
                maBlocks[miOutputBlock].miRead += liTotalRead;
                break;
            }

            liTotalRead -= liRemainingInCurrentBlock;
            maBlocks[miOutputBlock].miRead  = maBlocks[miOutputBlock].miWritten;
            maBlocks[miOutputBlock].muFlags = KU_RSBFLAG_EMPTY;
            ++miOutputBlock;
            --miBlocksUsed;
        }




        miOutputBlock    = miOutputBlock % miNumBlocks;
        mbIsOutputing    = false;
        muOutputPosition += static_cast<u64>(luAmountRead);
        Service();
    }




























    u32 StreamDeviceDiskRead::GetAmountOfDataInBuffer() const
    {
        StreamLockGuard lGuard(mFutex.mCriticalSection);

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












































    // OnOpen @0x82900740 — consume the device-open completion, retaining the compound
    // logical-device/device-handle pair on success. Every arm matches ARTIST's state update;
    // the handler always retires the pending open and re-enters Service.
    void StreamDeviceDiskRead::OnOpen(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        (void)luSize;
        (void)lpContext;

        StreamLockGuard lGuard(mFutex.mCriticalSection);




        CGS_ASSERT_STREAM_AT(meStatus == E_STATUS_OPENING, "Incorrect stream state\n", CGS_ARTIST_FILE, 908);
        CGS_ASSERT_STREAM_AT(miPendingOperationCount > 0, "Received stream event with 0 pending operations\n", CGS_ARTIST_FILE, 909);

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
















    // OnRead @ 0x829009C0 — device read-completion handler (from ReadCallback). Validates state and
    // the block, then: on failure (-2) flags the stream ERROR, on cancel (-1) returns it to OPEN,
    // and on success records the bytes read into the block, marks it LOADED (and EOF when short),
    // then decrements the outstanding counts and re-pumps via Service.
    void StreamDeviceDiskRead::OnRead(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        (void)lHandle;   // the device handle is not used by the read completion

        StreamLockGuard lGuard(mFutex.mCriticalSection);

        ReadStreamBlock* lpBlock = static_cast<ReadStreamBlock*>(lpContext);










        CGS_ASSERT_STREAM(meStatus != E_STATUS_CLOSED, "Incorrect stream state\n");
        CGS_ASSERT_STREAM(miPendingOperationCount > 0, "Received file event with 0 pending operations\n");

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

                CGS_ASSERT(mpFileSystem != 0, "mpFileSystem\n");
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

        StreamLockGuard lGuard(mFutex.mCriticalSection);




        CGS_ASSERT_STREAM(meStatus != E_STATUS_CLOSED, "Incorrect stream state\n");
        CGS_ASSERT_STREAM(miPendingOperationCount > 0, "Received stream event with 0 pending operations\n");

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
                CGS_ASSERT(mpFileSystem != 0, "mpFileSystem\n");
        }

        --miPendingOperationCount;
    }























    // OpenCallback @ 0x82900CF8 — device open-completion trampoline: forward to OnOpen on the owning
    // stream (the context), then assert the context was valid (matching ARTIST's call order).
    void StreamDeviceDiskRead::OpenCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        StreamDeviceDiskRead* lpStream = static_cast<StreamDeviceDiskRead*>(lpContext);
        lpStream->OnOpen(liResult, lHandle, luSize, lpContext);

        CGS_ASSERT_STREAM_AT(lpContext != 0, "Invalid stream\n", CGS_ARTIST_FILE, 1158);
    }







    // ReadCallback @ 0x82900DA8 — device read-completion trampoline: the context is the block; its
    // mpOwner is the stream OnRead is dispatched on.
    void StreamDeviceDiskRead::ReadCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        ReadStreamBlock* lpBlock = static_cast<ReadStreamBlock*>(lpContext);

        CGS_ASSERT_STREAM_AT(lpContext != 0, "Invalid stream block\n", CGS_ARTIST_FILE, 1173);
        CGS_ASSERT_STREAM_AT(lpBlock->mpOwner != 0, "Invalid stream\n", CGS_ARTIST_FILE, 1174);

        lpBlock->mpOwner->OnRead(liResult, lHandle, luSize, lpContext);
    }






    // CloseCallback @ 0x828E93C8 — device close-completion trampoline: assert the context is valid,
    // then forward to OnClose on the owning stream.
    void StreamDeviceDiskRead::CloseCallback(s32 liResult, Handle lHandle, u64 luSize, void* lpContext)
    {
        StreamDeviceDiskRead* lpStream = static_cast<StreamDeviceDiskRead*>(lpContext);

        CGS_ASSERT_STREAM_AT(lpContext != 0, "Invalid stream\n", CGS_ARTIST_FILE, 1190);
        lpStream->OnClose(liResult, lHandle, luSize, lpContext);
    }


    // ARTIST8290112C-48 /8290132C-44: each operation locks independently;
    // the ring's locked-block flags protect the exposed bytes between calls.
    bool StreamDeviceDiskRead::StartAsyncRead(void** lppData, u32* lpuAmount)
    {
        StreamLockGuard lGuard(mFutex.mCriticalSection);
        return StartAsyncReadInternal(lppData, lpuAmount);
    }


    void StreamDeviceDiskRead::StopAsyncRead(u32 luAmount)
    {
        StreamLockGuard lGuard(mFutex.mCriticalSection);
        StopAsyncReadInternal(luAmount);
    }


    // Inlined at ARTIST82900FCC, before deciding whether to prefetch.
    bool StreamDeviceDiskRead::IsEndOfFile() const
    {
        return mbIsEndOfFile;
    }


    // GetCurrentFilePriority @ 0x828E0F70 — pick the streaming priority for the next device read: at
    // end-of-file (nothing more to fetch) run at normal priority; otherwise run at normal priority
    // while the buffer is at least a quarter full, and escalate to the high priority once it drains
    // below 25% so a starving consumer catches up. (ARTIST: mbIsEndOfFile byte-test @0x588, then the
    // signed miBufferSize @0x130 as the denominator and the zero-extended GetAmountOfDataInBuffer
    // result as the numerator, compared against 0.25.)
    s32 StreamDeviceDiskRead::GetCurrentFilePriority()
    {
        // ARTIST @0x828E0F70 keeps one exit: the normal-priority load closes the function.
        if (!mbIsEndOfFile)
        {
            f64 lfCapacity = static_cast<f64>(miBufferSize);
            f64 lfInBuffer = static_cast<f64>(GetAmountOfDataInBuffer());
            if (lfInBuffer / lfCapacity < 0.25)
                return miHighPriority;
        }
        return miNormalPriority;
    }

} // namespace CgsFileSystem

#undef CGS_ARTIST_FILE
