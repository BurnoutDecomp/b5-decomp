#pragma once

// BrnReplays::GPUDiskWriteStream -- the GPU-frunk -> disk write streamer behind a
// BrnReplays::WriteStream. It owns three rings of fixed 64 KiB ("frunk"/global) blocks
// and shuffles them through a small relocator-driven copy pipeline before issuing async
// disk Writes via the CgsFileSystem::DeviceManager:
//
//   * Primary ring   -- blocks just handed in by AddBlock (CPU-side data + a 0x10000 byte
//                        payload copied into mpPrimaryData). Status word starts at 3.
//   * Secondary ring -- blocks the per-frame Relocator has copied from primary->secondary
//                        memory (status 7). Dispatch builds the relocator op that does this.
//   * Tertiary ring  -- blocks the Relocator has copied secondary->tertiary (status 13);
//                        these are the blocks SubmitWriteRequest actually streams to disk.
//
// Each ring slot is a LocalBlock {miGlobalBlockIndex, muOffset, mpStream}. A LocalBlock
// that holds data references a GlobalBlock (the 32-byte record with the on-disk offset,
// the status bits and a copy of the 4-byte "frk" frunk magic). A free-list of global-block
// indices recycles the GlobalBlocks.
//
// HOME: GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.{h,cpp} -- the canonical path
// printed by every assert in this TU ("..\\..\\..\\GameSource\\Replays/Stream/
// BrnReplayGPUDiskWriteStream.cpp" / ".h").
//
// LAYOUT POLICY: there is NO DWARF and NO Feb-2007 source for this class, so the member
// ORDER below is reconstructed directly from the X360 ARTIST asm offsets (Construct
// @0x8264D510 establishes the four arrays; Alloc/FreeGlobalBlock @0x8264B2F8/0x8264B378 and
// Service @0x82659748 / SubmitCloseRequest @0x82653890 pin the tail scalars). Per the project
// convention (see CgsFileSystem.h) members keep faithful DWARF/asm field ORDER with natural
// PC widths -- NOT exact X360 byte offsets (the X360 is 32-bit; a LocalBlock with a host
// pointer is wider than the 12 X360 bytes). The asm offsets are the authority for BEHAVIOUR;
// members are reached BY NAME, never by raw offset. The X360 idx in each comment is the asm
// source, not a PC offset.
//
//   X360 layout the offsets were derived from (idx = _DWORD index):
//     Primary  ring  idx 0   (32  LocalBlocks)   Secondary ring idx 96  (128 LocalBlocks)
//     Tertiary ring  idx 480 (32  LocalBlocks)   GlobalBlocks   idx 576 (192 * 8 words)
//     data ptrs idx 2112+    Relocator idx 2144  ops idx 2400   lock idx 3168
//     filename idx 3176      free-list idx 3240  tail scalars idx 3432+ (handle idx 3448)

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnReplays
{
    class GPUDiskWriteStream
    {
    public:
        // ---- block-record sizing (from Construct's loop trip-counts) ----
        static const s32 KI_NUM_GLOBAL_BLOCKS  = 192; // global block pool / free-list size
        static const s32 KI_PRIMARY_BLOCKS     = 32;  // primary ring slots
        static const s32 KI_SECONDARY_BLOCKS   = 128; // secondary ring slots
        static const s32 KI_TERTIARY_BLOCKS    = 32;  // tertiary ring slots
        static const s32 KI_BLOCK_SIZE         = 0x10000; // 64 KiB frunk payload

        // ---- block status bit flags (grounded in the asm masks/stores) ----
        // Primary populated     = 3   (AddBlock stores 3; SubmitWrite needs bit0 set, asserts !=2)
        // Primary -> secondary  = 7   (Dispatch sets 7; bit-mask test (s&7)==7 marks "in secondary")
        // Secondary cleared     = 5   (Dispatch sets 5 after freeing the primary payload)
        // Secondary -> tertiary = 13  (Dispatch sets 13; mask (s&0xD)==0xD marks "in tertiary")
        // Tertiary cleared      = 9   (Dispatch sets 9 after the secondary copy completes)
        // Writing               = 25  (SubmitWrite sets 25 while the disk op is in flight)
        static const s32 KI_STATUS_PRIMARY     = 3;
        static const s32 KI_STATUS_SECONDARY   = 7;
        static const s32 KI_STATUS_SEC_DONE    = 5;
        static const s32 KI_STATUS_TERTIARY    = 13;
        static const s32 KI_STATUS_TER_DONE    = 9;
        static const s32 KI_STATUS_WRITING     = 25;
        static const s32 KI_STATUS_EMPTY       = 0;

        // mState values (asm: Construct=0; SubmitWrite asserts ==2 for "open"; OnClose/OnWrite
        // set 4 on failure, 2 on cancel, 0 on clean close-complete).
        static const s32 KI_STATE_OPEN         = 2;
        static const s32 KI_STATE_FAILED       = 4;
        static const s32 KI_STATE_CANCELLED    = 2;

        // DeviceManager async op priority used for every Write/Close (asm: li r7, 0x19).
        static const s32 KI_OP_PRIORITY        = 25;

        // The invalid / unused sentinel stored in miGlobalBlockIndex etc. (asm stores -1).
        static const s32 KI_INVALID_BLOCK      = -1;

        // ---- one slot of a ring: which global block it points at, the on-disk offset, and
        //      a back-pointer to the owning stream (Construct seeds the back-pointer). ----
        struct LocalBlock
        {
            s32                 miGlobalBlockIndex; // [0] index into maGlobalBlocks, or -1
            s32                 miOffset;           // [1] on-disk byte offset
            GPUDiskWriteStream* mpStream;           // [2] owner (set in Construct)
        };

        // ---- one global block: 32 bytes; the on-disk offset, the status word, a cached copy
        //      of the 4-byte "frk" frunk magic and the owner back-pointer (set in Construct). ----
        struct GlobalBlock
        {
            s32                 miPrimaryLocalIndex;   // [0] owning primary local-block index
            s32                 miSecondaryLocalIndex; // [1] owning secondary local-block index
            s32                 miTertiaryLocalIndex;  // [2] owning tertiary local-block index
            s32                 miOffset;              // [3] on-disk byte offset (a2 in AddBlock)
            s32                 miStatus;              // [4] KI_STATUS_*
            u8                  maFrunk[4];            // [5] cached "frk\0" frunk magic
            s32                 miSecondPass;          // [6] flag: 1 sec->ter copy, 0 prim->sec
            GPUDiskWriteStream* mpStream;              // [7] owner (set in Construct)
        };

        // @0x8264D510 (boot trace). Seed all four arrays' owner back-pointers, zero the
        // handle/state, and construct the embedded relocator.
        void Construct();

        // @0x8265C378. Append a frunk's worth of 64 KiB blocks (a4 bytes, multiple of 64 KiB)
        // from buffer la3 at on-disk offset la2. Returns true on success, false if the pool is
        // full. Called by BrnReplays::WriteStream::AddFrunk.
        bool AddBlock(s32 la2Offset, const u8* lpData, s32 la4Size);

        // @0x8264B2F8. Pop a free global-block index off the free-list (asserts free>0).
        s32 AllocateGlobalBlock();

        // @0x8264B378. Push a global-block index back onto the free-list (asserts used>0).
        void FreeGlobalBlock(s32 liGlobalBlock);

        // @0x8265C2B8. Request a clean close once all queued blocks have drained. Called by
        // BrnReplays::ReplayModule::CloseReplayFiles.
        void Close();

        // @0x8265C648. Per-frame pump: run the relocator (primary->secondary->tertiary copies),
        // reclaim drained blocks, and kick the disk write/close pipeline.
        void Dispatch();

        // @0x82659748. Decide whether to submit the next disk write or the close request.
        s32 Service();

        // ---- async device-op completion callbacks (free statics; the context is the stream) ----
        // @0x82659C18 write-complete; @0x82650B00 close-complete. They validate and forward to
        // the OnWrite/OnClose members.
        static void WriteCallback(s32 liResult, s32 lHandle, u64 luSize, void* lpContext);
        static void CloseCallback(s32 liResult, s32 lHandle, u64 luSize, void* lpContext);

    private:
        // @0x826597E0. Write-op completion handler: free the just-written tertiary block.
        s32 OnWrite(s32 liResult, s32 lHandle, u64 luSize, void* lpBlock);
        // @0x82650848. Close-op completion handler: settle the close state.
        s32 OnClose(s32 liResult, void* lpStream);

        // @0x8264D5C8. If a tertiary block is ready, issue its async disk Write.
        s32 SubmitWriteRequest();
        // @0x82653890. Issue the async disk Close on mHandle.
        s32 SubmitCloseRequest();

        // ---- instance layout (X360 byte-exact; see header comment) ----
        // Three rings first (primary @0x000, secondary @0x180, tertiary @0x780).
        LocalBlock  maPrimaryBlocks[KI_PRIMARY_BLOCKS];     // +0x000  idx 0
        LocalBlock  maSecondaryBlocks[KI_SECONDARY_BLOCKS]; // +0x180  idx 96
        LocalBlock  maTertiaryBlocks[KI_TERTIARY_BLOCKS];   // +0x780  idx 480
        GlobalBlock maGlobalBlocks[KI_NUM_GLOBAL_BLOCKS];   // +0x900  idx 576

        s32         miNumGlobalBlocks;     // +0x2100 idx 2112  pool size (free+used budget)
        s32         miSecondaryRingSize;   // +0x2104 idx 2113  modulus for secondary indices
        s32         miTertiaryRingSize;    // +0x2108 idx 2114  modulus for tertiary indices
        u8*         mpPrimaryData;         // idx 2115  primary 64 KiB block backing store
        u8*         mpSecondaryData;       // idx 2116  secondary backing store
        u8*         mpTertiaryData;        // idx 2117  tertiary backing store

        // Relocator op cursor + count (Dispatch resets miRelocatorOpCount=0 and points the
        // cursor at maRelocatorOps each frame, then appends 16-byte ops as it builds copies).
        void*       mpRelocatorOpCursor;   // idx 2124  -> &maRelocatorOps[0]
        s32         miRelocatorOpCount;    // idx 2125

        // The embedded CgsMemory::Relocator state + its op buffer. Construct calls
        // CgsMemory::Relocator::Construct(this+idx2144). The op records (16 bytes each) live at
        // idx 2400. Modelled as opaque blobs reached only by name (maRelocator / maRelocatorOps);
        // the Relocator type is .cpp-local (no shared header) so the engine calls are declared as
        // file-local shims in the .cpp. Sizes kept at the X360 byte spans (storage only).
        u8          maRelocator[0x2580 - 0x2180];        // idx 2144 (relocator state, 1024 B)
        u8          maRelocatorOps[0x3180 - 0x2580];     // idx 2400 (op buffer, 16B ea, 3072 B)

        // Subsystem-wide lock (RTL_CRITICAL_SECTION, 28 bytes -> 32). The asm does
        // RtlEnterCriticalSection(this+idx3168); reached by name via maLock.
        u8          maLock[32];            // idx 3168

        char        macFileName[256];      // idx 3176  the stream file name (for asserts)
        s32         maGlobalBlockFreeList[KI_NUM_GLOBAL_BLOCKS]; // idx 3240

        s32         miBlocksInPrimary;     // idx 3432
        s32         miBlocksInSecondary;   // idx 3433
        s32         miBlocksInTertiary;    // idx 3434
        s32         miGlobalBlocksUsed;    // idx 3435
        s32         miGlobalBlocksFree;    // idx 3436
        s32         miPrimaryWriteIndex;   // idx 3437
        s32         miSecondaryReadIndex;  // idx 3438
        s32         miSecondaryWriteIndex; // idx 3439
        s32         miTertiaryReadIndex;   // idx 3440
        s32         miTertiaryWriteIndex;  // idx 3441
        s32         miWriteSubmitIndex;    // idx 3442
        bool        mbClosePending;        // idx 3443 (asm: lbz/stb -> 1 byte)
        s32         miState;               // idx 3444  KI_STATE_*
        s32         miPrimaryInFlight;     // idx 3445
        s32         miSecondaryInFlight;   // idx 3446
        s32         miPendingOps;          // idx 3447
        u64         muHandle;              // idx 3448/3449  device file handle
    };
}
