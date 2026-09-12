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
// LAYOUT POLICY: no original header exists for this class, so the member ORDER below is
// recovered from the offsets the console's own code reaches: Construct establishes the four
// arrays; AllocateGlobalBlock / FreeGlobalBlock and Service / SubmitCloseRequest pin the tail
// scalars. Per the project convention (see CgsFileSystem.h) members keep faithful field ORDER
// with natural PC widths -- NOT the console's exact byte offsets (the console is 32-bit, so a
// LocalBlock carrying a host pointer is wider than its 12 bytes there). The console offsets are
// the authority for BEHAVIOUR; members are reached BY NAME, never by raw offset. The +0xNN in
// each comment is the console offset, not a PC offset.
//
//   The console offsets the order was derived from:
//     Primary  ring  +0x0    (32  LocalBlocks)   Secondary ring +0x180  (128 LocalBlocks)
//     Tertiary ring  +0x780  (32  LocalBlocks)   GlobalBlocks   +0x900  (192 * 32 bytes)
//     data ptrs +0x2100+     Relocator +0x2180   ops +0x2580    lock +0x3180
//     filename +0x31A0       free-list +0x32A0   tail scalars +0x35A0+ (handle +0x35E0)

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceOperation.h"  // CgsFileSystem::Handle
#include "GameShared/Jobs/Relocator/CgsRelocator.h"  // CgsMemory::Relocator / RelocationParams / RelocateOp

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

        // ---- block status bit flags (grounded in the masks and stores the console makes) ----
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

        // mState values (Construct sets 0; SubmitWrite asserts ==2 for "open"; OnClose/OnWrite
        // set 4 on failure, 2 on cancel, 0 on clean close-complete).
        static const s32 KI_STATE_OPEN         = 2;
        static const s32 KI_STATE_FAILED       = 4;
        static const s32 KI_STATE_CANCELLED    = 2;

        // DeviceManager async op priority used for every Write/Close (the console passes 25).
        static const s32 KI_OP_PRIORITY        = 25;

        // The invalid / unused sentinel stored in miGlobalBlockIndex etc. (the console stores -1).
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

        // Seed all four arrays' owner back-pointers, zero the
        // handle/state, and construct the embedded relocator.
        void Construct();

        // Append a frunk's worth of 64 KiB blocks (a4 bytes, multiple of 64 KiB)
        // from buffer la3 at on-disk offset la2. Returns true on success, false if the pool is
        // full. Called by BrnReplays::WriteStream::AddFrunk.
        bool AddBlock(s32 la2Offset, const u8* lpData, s32 la4Size);

        // Pop a free global-block index off the free-list (asserts free>0).
        s32 AllocateGlobalBlock();

        // Push a global-block index back onto the free-list (asserts used>0).
        void FreeGlobalBlock(s32 liGlobalBlock);

        // Request a clean close once all queued blocks have drained. Called by
        // BrnReplays::ReplayModule::CloseReplayFiles.
        void Close();

        // Per-frame pump: run the relocator (primary->secondary->tertiary copies),
        // reclaim drained blocks, and kick the disk write/close pipeline.
        void Dispatch();

        // Decide whether to submit the next disk write or the close request.
        s32 Service();

        // ---- async device-op completion callbacks (free statics; the context is the stream) ----
        // One write-complete, one close-complete. They validate and forward to
        // the OnWrite/OnClose members.
        static void WriteCallback(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpContext);
        static void CloseCallback(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpContext);

    private:
        // Write-op completion handler: free the just-written tertiary block.
        s32 OnWrite(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpBlock);
        // Close-op completion handler: settle the close state.
        s32 OnClose(s32 liResult, void* lpStream);

        // If a tertiary block is ready, issue its async disk Write.
        s32 SubmitWriteRequest();
        // Issue the async disk Close on mHandle.
        s32 SubmitCloseRequest();

        // ---- instance layout (console field order; see the header comment) ----
        // Three rings first (primary +0x0, secondary +0x180, tertiary +0x780).
        LocalBlock  maPrimaryBlocks[KI_PRIMARY_BLOCKS];     // +0x0
        LocalBlock  maSecondaryBlocks[KI_SECONDARY_BLOCKS]; // +0x180
        LocalBlock  maTertiaryBlocks[KI_TERTIARY_BLOCKS];   // +0x780
        GlobalBlock maGlobalBlocks[KI_NUM_GLOBAL_BLOCKS];   // +0x900

        s32         miNumGlobalBlocks;     // +0x2100  pool size (free+used budget)
        s32         miSecondaryRingSize;   // +0x2104  modulus for secondary indices
        s32         miTertiaryRingSize;    // +0x2108  modulus for tertiary indices
        u8*         mpPrimaryData;         // +0x210C  primary 64 KiB block backing store
        u8*         mpSecondaryData;       // +0x2110  secondary backing store
        u8*         mpTertiaryData;        // +0x2114  tertiary backing store

        // The relocator parameter block Dispatch hands to Relocator::Execute (the console
        // passes the address of this sub-object). Dispatch resets miNumOps to 0 and points
        // mpOps at maRelocatorOps each frame, then appends ops as it builds the copies.
        // FLAG: the bounce-buffer pair is installed by the stream's open/init path, which is
        // not reconstructed; it stays as the zero-initialised storage gives it. The pipeline's
        // three backing stores never overlap, so the copy job never takes the bounce path.
        CgsMemory::RelocationParams mRelocatorParams;   // +0x2130..+0x213C

        // The embedded copy engine + its op buffer. Construct runs the relocator's own
        // Construct; the op buffer holds one record per global block (the console's 3072-byte
        // span at +0x2580 is 192 * 16 bytes).
        CgsMemory::Relocator   mRelocator;                            // +0x2180
        CgsMemory::RelocateOp  maRelocatorOps[KI_NUM_GLOBAL_BLOCKS];  // +0x2580

        // Subsystem-wide lock (RTL_CRITICAL_SECTION, 28 bytes -> 32). The console enters and
        // leaves this critical section directly; reached by name via maLock.
        u8          maLock[32];            // +0x3180

        char        macFileName[256];      // +0x31A0  the stream file name (for asserts)
        s32         maGlobalBlockFreeList[KI_NUM_GLOBAL_BLOCKS]; // +0x32A0

        s32         miBlocksInPrimary;     // +0x35A0
        s32         miBlocksInSecondary;   // +0x35A4
        s32         miBlocksInTertiary;    // +0x35A8
        s32         miGlobalBlocksUsed;    // +0x35AC
        s32         miGlobalBlocksFree;    // +0x35B0
        s32         miPrimaryWriteIndex;   // +0x35B4
        s32         miSecondaryReadIndex;  // +0x35B8
        s32         miSecondaryWriteIndex; // +0x35BC
        s32         miTertiaryReadIndex;   // +0x35C0
        s32         miTertiaryWriteIndex;  // +0x35C4
        s32         miWriteSubmitIndex;    // +0x35C8
        bool        mbClosePending;        // +0x35CC (1 byte on the console too)
        s32         miState;               // +0x35D0  KI_STATE_*
        s32         miPrimaryInFlight;     // +0x35D4
        s32         miSecondaryInFlight;   // +0x35D8
        s32         miPendingOps;          // +0x35DC
        // The compound device file handle {device, device-private handle}. The console packed
        // the pair into the two words at +0x35E0; here it is the typed value itself.
        CgsFileSystem::Handle mHandle;     // +0x35E0  device file handle
    };
}
