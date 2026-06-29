#include "GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.h"

#include <cstring> // memcpy / memset

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h" // DeviceManager, GetDeviceManager

// Reconstructed from BURNOUT_X360_ARTIST.XEX. The class layout, control flow, status
// constants and ring arithmetic are taken from the X360 asm (no DWARF / no Feb-2007 source
// exists for this TU). See the header for the offset map.

// --- Win32 critical-section primitives (the lock embedded at this+0x3180). Declared here as
//     the sibling CgsDeviceOperationPool.cpp does; the real bodies come from the platform. ---
extern "C" void RtlEnterCriticalSection(void* lpCriticalSection);
extern "C" void RtlLeaveCriticalSection(void* lpCriticalSection);

namespace CgsMemory
{
    // The embedded per-frame relocator copy engine (CgsMemory::Relocator). Its type is
    // .cpp-local in GameShared/Jobs/Relocator/CgsRelocator.cpp (no shared header), so the
    // op-driving entry points this TU calls are declared here as opaque pointer shims reached
    // by name on the relocator subobject (this+0x2180). Update(...) returns "ran this frame".
    //   Construct @0x828687C8, Update/Execute (Dispatch's relocator pump).
    void  Relocator_Construct(void* lpRelocator);
    bool  Relocator_Update(void* lpRelocator, char la2, char la3);
    void  Relocator_Execute(void* lpRelocator, void* lpOpList);

    // One relocator copy op (X360: 4 * u32 {src,dst,size,type}; here native pointers so the
    // op records survive a 64-bit host). Built into the stream's maRelocatorOps buffer.
    struct RelocatorOp
    {
        void* mpSrc;
        void* mpDst;
        u32   muSize;
        u32   muType; // 1 = secondary->tertiary, 2 = primary->secondary
    };
}

namespace BrnReplays
{
    // ---- file-scope debug toggles / state (asm: byte_82F2A598/599/59A, dword_82F2A634). ----
    // The three byte toggles gate the normal write/transfer pipeline; the boot trace shows the
    // stream operating, so they default ENABLED. NOTE: their init values are NOT asm-grounded
    // (read-only debug switches with no recovered initializer); defaulting to true reproduces
    // the observed behaviour. dword_82F2A634 is grounded: Construct stores 3.
    static bool gbEnablePrimaryTransfer   = true; // byte_82F2A598
    static bool gbEnableSecondaryTransfer = true; // byte_82F2A599
    static bool gbEnableWriteRequests     = true; // byte_82F2A59A
    static s32  giStreamSystemState       = 0;    // dword_82F2A634 (Construct sets 3)

    // =====================================================================================
    // Construct @ 0x8264D510
    // =====================================================================================
    void GPUDiskWriteStream::Construct()
    {
        muHandle  = 0;   // a1[3448] / a1[3449]
        miState   = 0;   // a1[3444]

        // Seed every ring slot's / global block's owner back-pointer to `this`.
        for (s32 li = 0; li < KI_NUM_GLOBAL_BLOCKS; ++li)
            maGlobalBlocks[li].mpStream = this;
        for (s32 li = 0; li < KI_PRIMARY_BLOCKS; ++li)
            maPrimaryBlocks[li].mpStream = this;
        for (s32 li = 0; li < KI_SECONDARY_BLOCKS; ++li)
            maSecondaryBlocks[li].mpStream = this;
        for (s32 li = 0; li < KI_TERTIARY_BLOCKS; ++li)
            maTertiaryBlocks[li].mpStream = this;

        CgsMemory::Relocator_Construct(maRelocator);
        giStreamSystemState = 3;
    }

    // =====================================================================================
    // AllocateGlobalBlock @ 0x8264B2F8 -- pop a free global-block index off the free-list.
    // =====================================================================================
    s32 GPUDiskWriteStream::AllocateGlobalBlock()
    {
        CGS_ASSERT(miGlobalBlocksFree > 0, "miGlobalBlocksFree > 0");
        --miGlobalBlocksFree;
        ++miGlobalBlocksUsed;
        return maGlobalBlockFreeList[miGlobalBlocksFree];
    }

    // =====================================================================================
    // FreeGlobalBlock @ 0x8264B378 -- push a global-block index back onto the free-list.
    // =====================================================================================
    void GPUDiskWriteStream::FreeGlobalBlock(s32 liGlobalBlock)
    {
        CGS_ASSERT(miGlobalBlocksUsed > 0, "miGlobalBlocksUsed > 0");
        maGlobalBlockFreeList[miGlobalBlocksFree] = liGlobalBlock;
        ++miGlobalBlocksFree;
        --miGlobalBlocksUsed;
    }

    // =====================================================================================
    // AddBlock @ 0x8265C378 -- queue la4Size bytes (a multiple of 64 KiB) of frunk data from
    // lpData at on-disk offset la2Offset into the primary ring. Returns false if the pool is
    // full. Called by BrnReplays::WriteStream::AddFrunk.
    // =====================================================================================
    bool GPUDiskWriteStream::AddBlock(s32 la2Offset, const u8* lpData, s32 la4Size)
    {
        RtlEnterCriticalSection(maLock);

        CGS_ASSERT((la4Size % KI_BLOCK_SIZE) == 0, "Data size must be a multiple of 0x10000");

        // The frunk must start with the "frk" magic.
        const char* lpcMagic = "frk";
        bool lbValidFrunk = (lpData[0] == static_cast<u8>(lpcMagic[0]))
                         && (lpData[1] == static_cast<u8>(lpcMagic[1]))
                         && (lpData[2] == static_cast<u8>(lpcMagic[2]));
        CGS_ASSERT(lbValidFrunk, "Attempting to add invalid frunk");

        s32 liNumBlocks = la4Size / KI_BLOCK_SIZE;

        // Reject if it would overflow the pool or the free budget.
        if (liNumBlocks > (miNumGlobalBlocks - miBlocksInPrimary) || liNumBlocks > miGlobalBlocksFree)
        {
            RtlLeaveCriticalSection(maLock);
            return false;
        }

        s32 liOffset = la2Offset;
        const u8* lpSrc = lpData;
        for (s32 li = 0; li < liNumBlocks; ++li)
        {
            s32          liGlobalBlock = AllocateGlobalBlock();
            GlobalBlock& lGlobal       = maGlobalBlocks[liGlobalBlock];
            LocalBlock&  lLocalBlock   = maPrimaryBlocks[miPrimaryWriteIndex];

            CGS_ASSERT(lLocalBlock.miGlobalBlockIndex == KI_INVALID_BLOCK,
                       "lLocalBlock.miGlobalBlockIndex == -1");

            lGlobal.miOffset            = liOffset;
            lGlobal.miStatus            = KI_STATUS_PRIMARY;
            lGlobal.miPrimaryLocalIndex = miPrimaryWriteIndex;
            lLocalBlock.miGlobalBlockIndex = liGlobalBlock;

            std::memcpy(mpPrimaryData + lLocalBlock.miOffset, lpSrc, KI_BLOCK_SIZE);

            lGlobal.maFrunk[0] = lpSrc[0];
            lGlobal.maFrunk[1] = lpSrc[1];
            lGlobal.maFrunk[2] = lpSrc[2];
            lGlobal.maFrunk[3] = lpSrc[3];

            liOffset += KI_BLOCK_SIZE;
            lpSrc    += KI_BLOCK_SIZE;

            miPrimaryWriteIndex = (miPrimaryWriteIndex + 1) % miNumGlobalBlocks;
            ++miBlocksInPrimary;
        }

        Service();
        RtlLeaveCriticalSection(maLock);
        return true;
    }

    // =====================================================================================
    // Service @ 0x82659748 -- drive the next disk op: a close once everything has drained,
    // otherwise the next write while the stream is open.
    // =====================================================================================
    s32 GPUDiskWriteStream::Service()
    {
        if (mbClosePending && miGlobalBlocksUsed == 0)
        {
            s32 liResult = SubmitCloseRequest();
            mbClosePending = false;
            return liResult;
        }

        if (miPendingOps < miBlocksInTertiary && miState == KI_STATE_OPEN)
        {
            if (gbEnableWriteRequests)
                return SubmitWriteRequest();
        }
        return 0; // X360 returned `this` here; the result is discarded by callers
    }

    // =====================================================================================
    // SubmitCloseRequest @ 0x82653890 -- issue the async disk Close on the file handle.
    // =====================================================================================
    s32 GPUDiskWriteStream::SubmitCloseRequest()
    {
        CGS_ASSERT(muHandle != 0, "No handle");

        CgsFileSystem::DeviceManager* lpDeviceManager = CgsFileSystem::GetDeviceManager();
        lpDeviceManager->Close(static_cast<CgsFileSystem::Handle>(muHandle),
                               &GPUDiskWriteStream::CloseCallback, this, KI_OP_PRIORITY);
        ++miPendingOps;
        return 1;
    }

    // =====================================================================================
    // SubmitWriteRequest @ 0x8264D5C8 -- if a tertiary block is ready, stream it to disk.
    // =====================================================================================
    s32 GPUDiskWriteStream::SubmitWriteRequest()
    {
        if (miBlocksInTertiary == 0)
            return 0;

        LocalBlock& lLocalBlock = maTertiaryBlocks[miWriteSubmitIndex];
        CGS_ASSERT(lLocalBlock.miGlobalBlockIndex >= 0, "lLocalBlock.miGlobalBlockIndex >= 0");

        GlobalBlock& lGlobal = maGlobalBlocks[lLocalBlock.miGlobalBlockIndex];
        CGS_ASSERT(miState == KI_STATE_OPEN, "Attempting to service a none-open stream");
        CGS_ASSERT((lGlobal.miStatus & 1) != 0, "Should never be writing from an empty block");

        if (lGlobal.miStatus != KI_STATUS_TER_DONE)
            return 0;

        // Validate the frunk magic still present in the tertiary copy.
        const u8* lpFrunk = mpTertiaryData + lLocalBlock.miOffset;
        bool lbValidFrunk = (lpFrunk[0] == lGlobal.maFrunk[0]) && (lpFrunk[1] == lGlobal.maFrunk[1])
                         && (lpFrunk[2] == lGlobal.maFrunk[2]) && (lpFrunk[3] == lGlobal.maFrunk[3]);
        CGS_ASSERT(lbValidFrunk, "Invalid frunk has been written");

        lGlobal.miStatus = KI_STATUS_WRITING;
        CGS_ASSERT(muHandle != 0, "No handle");

        CgsFileSystem::DeviceManager* lpDeviceManager = CgsFileSystem::GetDeviceManager();
        lpDeviceManager->Write(static_cast<CgsFileSystem::Handle>(muHandle),
                               static_cast<u64>(lGlobal.miOffset),
                               mpTertiaryData + lLocalBlock.miOffset,
                               KI_BLOCK_SIZE,
                               &GPUDiskWriteStream::WriteCallback, &lLocalBlock, KI_OP_PRIORITY);

        miWriteSubmitIndex = (miWriteSubmitIndex + 1) % miTertiaryRingSize;
        ++miPendingOps;
        return 1;
    }

    // =====================================================================================
    // Dispatch @ 0x8265C648 -- per-frame pump. Run the relocator, reclaim drained blocks, and
    // build the relocator ops that copy primary->secondary then secondary->tertiary.
    // =====================================================================================
    void GPUDiskWriteStream::Dispatch()
    {
        RtlEnterCriticalSection(maLock);

        if (CgsMemory::Relocator_Update(maRelocator, 1, 1))
        {
            // --- reclaim blocks whose secondary copy has completed (status bit (s&7)==7). ---
            if (miPrimaryInFlight > 0)   // asm 0x8265C68C reads 0x35D4 (miPrimaryInFlight)
            {
                for (s32 li = 0; li < KI_NUM_GLOBAL_BLOCKS; ++li)
                {
                    GlobalBlock& lGlobal = maGlobalBlocks[li];
                    if ((lGlobal.miStatus & 7) == 7)
                    {
                        if (lGlobal.miSecondPass <= 0)
                        {
                            const u8* lpData = mpPrimaryData
                                + maPrimaryBlocks[lGlobal.miPrimaryLocalIndex].miOffset;
                            bool lbValidFrunk = (lpData[0] == lGlobal.maFrunk[0])
                                && (lpData[1] == lGlobal.maFrunk[1])
                                && (lpData[2] == lGlobal.maFrunk[2])
                                && (lpData[3] == lGlobal.maFrunk[3]);
                            CGS_ASSERT(lbValidFrunk, "Invalid frunk has been transferred");

                            std::memset(mpPrimaryData
                                + maPrimaryBlocks[lGlobal.miPrimaryLocalIndex].miOffset,
                                0xCD, KI_BLOCK_SIZE);
                            maPrimaryBlocks[lGlobal.miPrimaryLocalIndex].miGlobalBlockIndex
                                = KI_INVALID_BLOCK;
                            --miBlocksInPrimary;
                            lGlobal.miPrimaryLocalIndex = KI_INVALID_BLOCK;
                            lGlobal.miStatus            = KI_STATUS_SEC_DONE;
                            --miPrimaryInFlight;   // asm 0x8265C810 --0x35D4 (keep --miBlocksInPrimary above)
                        }
                        else
                        {
                            --lGlobal.miSecondPass;
                        }
                    }
                }
            }

            // --- reclaim blocks whose tertiary copy has completed (status (s&0xD)==0xD). ---
            if (miSecondaryInFlight > 0)   // asm 0x8265C82C reads 0x35D8 (miSecondaryInFlight)
            {
                for (s32 li = 0; li < KI_NUM_GLOBAL_BLOCKS; ++li)
                {
                    GlobalBlock& lGlobal = maGlobalBlocks[li];
                    if ((lGlobal.miStatus & 0xD) == 0xD)
                    {
                        if (lGlobal.miSecondPass <= 0)
                        {
                            maSecondaryBlocks[lGlobal.miSecondaryLocalIndex].miGlobalBlockIndex
                                = KI_INVALID_BLOCK;
                            --miBlocksInSecondary;
                            lGlobal.miSecondaryLocalIndex = KI_INVALID_BLOCK;
                            lGlobal.miStatus              = KI_STATUS_TER_DONE;
                            --miSecondaryInFlight;   // asm 0x8265C8A0 --0x35D8 (keep --miBlocksInSecondary above)
                        }
                        else
                        {
                            --lGlobal.miSecondPass;
                        }
                    }
                }
            }

            Service();

            // Reset the relocator op list for this frame.
            miRelocatorOpCount   = 0;
            mpRelocatorOpCursor  = maRelocatorOps;
            CgsMemory::RelocatorOp* lpOps
                = reinterpret_cast<CgsMemory::RelocatorOp*>(maRelocatorOps);

            // --- build secondary->tertiary copy ops (status 5 -> 13). ---
            if (gbEnableSecondaryTransfer && miBlocksInSecondary > miSecondaryInFlight)
            {
                while (miBlocksInSecondary > miSecondaryInFlight)
                {
                    if (maTertiaryBlocks[miTertiaryWriteIndex].miGlobalBlockIndex != KI_INVALID_BLOCK)
                        break;

                    LocalBlock& lSecondary = maSecondaryBlocks[miTertiaryReadIndex];
                    CGS_ASSERT(lSecondary.miGlobalBlockIndex >= 0,
                               "lSecondary.miGlobalBlockIndex >= 0");

                    GlobalBlock& lGlobal = maGlobalBlocks[lSecondary.miGlobalBlockIndex];
                    if (lGlobal.miStatus != KI_STATUS_SEC_DONE)
                        break;

                    LocalBlock& lTertiary = maTertiaryBlocks[miTertiaryWriteIndex];
                    CGS_ASSERT(lTertiary.miGlobalBlockIndex == KI_INVALID_BLOCK,
                               "lTertiary.miGlobalBlockIndex == -1");

                    CgsMemory::RelocatorOp& lOp = lpOps[miRelocatorOpCount];
                    lOp.mpSrc  = mpSecondaryData + lSecondary.miOffset;
                    lOp.mpDst  = mpTertiaryData + lTertiary.miOffset;
                    lOp.muSize = KI_BLOCK_SIZE;
                    lOp.muType = 1;
                    ++miRelocatorOpCount;

                    lTertiary.miGlobalBlockIndex = lSecondary.miGlobalBlockIndex;
                    lGlobal.miTertiaryLocalIndex = miTertiaryWriteIndex;
                    lGlobal.miStatus             = KI_STATUS_TERTIARY;
                    lGlobal.miSecondPass         = 1;

                    miTertiaryReadIndex  = (miTertiaryReadIndex + 1) % miSecondaryRingSize;
                    miTertiaryWriteIndex = (miTertiaryWriteIndex + 1) % miTertiaryRingSize;
                    ++miBlocksInTertiary;  // a1[3434]
                    ++miSecondaryInFlight; // a1[3446]
                }
            }

            // --- build primary->secondary copy ops (status 3 -> 7). ---
            if (gbEnablePrimaryTransfer && miBlocksInPrimary > miPrimaryInFlight)
            {
                while (miBlocksInPrimary > miPrimaryInFlight)
                {
                    if (maSecondaryBlocks[miSecondaryWriteIndex].miGlobalBlockIndex != KI_INVALID_BLOCK)
                        break;

                    LocalBlock& lPrimary = maPrimaryBlocks[miSecondaryReadIndex];
                    CGS_ASSERT(lPrimary.miGlobalBlockIndex >= 0,
                               "lPrimary.miGlobalBlockIndex >= 0");

                    GlobalBlock& lGlobal = maGlobalBlocks[lPrimary.miGlobalBlockIndex];
                    if (lGlobal.miStatus != KI_STATUS_PRIMARY)
                        break;

                    const u8* lpData = mpPrimaryData + lPrimary.miOffset;
                    bool lbValidFrunk = (lpData[0] == lGlobal.maFrunk[0])
                        && (lpData[1] == lGlobal.maFrunk[1])
                        && (lpData[2] == lGlobal.maFrunk[2])
                        && (lpData[3] == lGlobal.maFrunk[3]);
                    CGS_ASSERT(lbValidFrunk, "Invalid frunk is beginning transfer");

                    LocalBlock& lSecondary = maSecondaryBlocks[miSecondaryWriteIndex];
                    CGS_ASSERT(lSecondary.miGlobalBlockIndex == KI_INVALID_BLOCK,
                               "lSecondary.miGlobalBlockIndex == -1");

                    CgsMemory::RelocatorOp& lOp = lpOps[miRelocatorOpCount];
                    lOp.mpSrc  = mpPrimaryData + lPrimary.miOffset;
                    lOp.mpDst  = mpSecondaryData + lSecondary.miOffset;
                    lOp.muSize = KI_BLOCK_SIZE;
                    lOp.muType = 2;
                    ++miRelocatorOpCount;

                    lSecondary.miGlobalBlockIndex = lPrimary.miGlobalBlockIndex;
                    lGlobal.miSecondaryLocalIndex = miSecondaryWriteIndex;
                    lGlobal.miStatus              = KI_STATUS_SECONDARY;
                    lGlobal.miSecondPass          = 0;

                    miSecondaryReadIndex  = (miSecondaryReadIndex + 1) % miNumGlobalBlocks;
                    miSecondaryWriteIndex = (miSecondaryWriteIndex + 1) % miSecondaryRingSize;
                    ++miBlocksInSecondary;
                    ++miPrimaryInFlight;
                }
            }

            CgsMemory::Relocator_Execute(maRelocator, &mpRelocatorOpCursor);
        }

        RtlLeaveCriticalSection(maLock);
    }

    // =====================================================================================
    // Close @ 0x8265C2B8 -- flag a clean close; Service issues it once all blocks have drained.
    // Called by BrnReplays::ReplayModule::CloseReplayFiles.
    // =====================================================================================
    void GPUDiskWriteStream::Close()
    {
        RtlEnterCriticalSection(maLock);
        CGS_ASSERT(miState != 0, "Invalid state to perform operation");
        mbClosePending = true;
        Service();
        RtlLeaveCriticalSection(maLock);
    }

    // =====================================================================================
    // WriteCallback @ 0x82659C18 -- async write completion. The context is the tertiary
    // LocalBlock; its mpStream back-pointer is the stream.
    // =====================================================================================
    void GPUDiskWriteStream::WriteCallback(s32 liResult, s32 lHandle, u64 luSize, void* lpContext)
    {
        LocalBlock* lpBlock = static_cast<LocalBlock*>(lpContext);
        CGS_ASSERT(lpBlock != 0, "Invalid stream block");
        CGS_ASSERT(lpBlock->mpStream != 0, "Invalid stream");
        lpBlock->mpStream->OnWrite(liResult, lHandle, luSize, lpContext);
    }

    // =====================================================================================
    // CloseCallback @ 0x82650B00 -- async close completion. The context is the stream.
    // =====================================================================================
    void GPUDiskWriteStream::CloseCallback(s32 liResult, s32 lHandle, u64 luSize, void* lpContext)
    {
        (void)lHandle;
        (void)luSize;
        CGS_ASSERT(lpContext != 0, "Invalid stream");
        static_cast<GPUDiskWriteStream*>(lpContext)->OnClose(liResult, lpContext);
    }

    // =====================================================================================
    // OnWrite @ 0x826597E0 -- a write op finished: clear the tertiary block and free its
    // global block.
    // =====================================================================================
    s32 GPUDiskWriteStream::OnWrite(s32 liResult, s32 lHandle, u64 luSize, void* lpBlock)
    {
        (void)lHandle;
        (void)luSize;
        RtlEnterCriticalSection(maLock);

        CGS_ASSERT(miState != 0, "Incorrect stream state");
        CGS_ASSERT(miPendingOps > 0, "Received file event with 0 pending operations");

        LocalBlock* lpLocal = static_cast<LocalBlock*>(lpBlock);

        if (liResult == -2)
        {
            miState = KI_STATE_FAILED;
            CGS_ASSERT(false, "Read failed on stream");
        }
        else if (liResult == -1)
        {
            miState = KI_STATE_FAILED;
            CGS_ASSERT(false, "Read cancelled on stream");
        }
        else if (liResult != 0)
        {
            CGS_ASSERT(false, "Unhandled response");
        }
        else
        {
            GlobalBlock& lGlobal = maGlobalBlocks[lpLocal->miGlobalBlockIndex];
            CGS_ASSERT(lGlobal.miStatus == KI_STATUS_WRITING, "Invalid block status when writing");

            const u8* lpFrunk = mpTertiaryData + lpLocal->miOffset;
            bool lbValidFrunk = (lpFrunk[0] == lGlobal.maFrunk[0]) && (lpFrunk[1] == lGlobal.maFrunk[1])
                && (lpFrunk[2] == lGlobal.maFrunk[2]) && (lpFrunk[3] == lGlobal.maFrunk[3]);
            CGS_ASSERT(lbValidFrunk, "Invalid frunk has been written");

            std::memset(mpTertiaryData + lpLocal->miOffset, 0xCD, KI_BLOCK_SIZE);
            lGlobal.miStatus            = KI_STATUS_EMPTY;
            lGlobal.miOffset            = 0;
            lGlobal.miTertiaryLocalIndex = KI_INVALID_BLOCK;
            FreeGlobalBlock(lpLocal->miGlobalBlockIndex);
            lpLocal->miGlobalBlockIndex = KI_INVALID_BLOCK;
            --miBlocksInTertiary;
        }

        --miPendingOps;
        Service();
        RtlLeaveCriticalSection(maLock);
        return 0; // X360 returned `this`; the callback result is discarded
    }

    // =====================================================================================
    // OnClose @ 0x82650848 -- a close op finished: settle the close state.
    // =====================================================================================
    s32 GPUDiskWriteStream::OnClose(s32 liResult, void* lpStream)
    {
        (void)lpStream;
        RtlEnterCriticalSection(maLock);

        CGS_ASSERT(miState != 0, "Incorrect stream state");
        CGS_ASSERT(miPendingOps > 0, "Received stream event with 0 pending operations");

        if (liResult == -2)
        {
            CGS_ASSERT(false, "Close failed on stream");
            miState = KI_STATE_FAILED;
        }
        else if (liResult == -1)
        {
            CGS_ASSERT(false, "Close cancelled on stream");
            miState = KI_STATE_CANCELLED;
        }
        else if (liResult != 0)
        {
            CGS_ASSERT(false, "Unhandled response");
        }
        else
        {
            // asm zeroes the 64-bit handle (bytes 13792/13796) and the state (13776).
            muHandle = 0;
            miState  = 0;
        }

        --miPendingOps;
        RtlLeaveCriticalSection(maLock);
        return 0; // X360 returned `this`; the callback result is discarded
    }

}
