#pragma once

// BrnReplays::DiskReadStream -- the playback-side replay disk READ streamer that sits
// behind a BrnReplays::ReadStream. It owns a ring of fixed-size ("stream block")
// buffers and keeps them filled by issuing async disk Reads through the
// CgsFileSystem::DeviceManager, wrapping the read window between a loop start/end so a
// replay can be scrubbed and looped. Each ring slot is a ReadStreamBlock carrying the
// 64-bit file position it holds, the buffer offset it reads into, a readable-byte
// window, a status flag word and a back-pointer to the owning stream (so the free-
// function completion callback can recover it).
//
// HOME: GameSource/Replays/Stream/BrnReplayDiskReadStream.{h,cpp} -- the canonical path
// printed by every assert in this TU ("..\\..\\..\\GameSource\\Replays/Stream/
// BrnReplayDiskReadStream.cpp").
//
// SLICE NOTE: this reconstruction lands the seven ledger functions of the X360 ARTIST
// TU (ReadBlock @0x8265CE08, ReadCallback @0x8265A670, CloseCallback @0x82650F98,
// ResetStreamBlocks @0x8264D8C0, Service @0x82659D40, SetRange @0x82659EB8,
// SubmitReadRequest @0x8264DA80). The remaining DiskReadStream methods (Construct, Open,
// Close, OnOpen/OnRead/OnClose, OpenCallback, GetStatus, GetCurrentFilePriority, the
// Start/StopAsyncReadInternal pair) are declared here and reconstructed in their own
// ledger work; GROW this home additively -- do NOT fork it.
//
// LAYOUT POLICY: member ORDER follows the X360 ARTIST asm offsets (and the DecFIGS
// BrnReplayDiskReadStream.h DWARF where the two agree); widths are natural PC/x64 (a
// host pointer/handle is wider than the 32-bit X360 field), so the documented X360
// offsets are NOT byte-exact -- they record the source of the ordering only. Every
// member is reached BY NAME, never by raw offset (same convention as the sibling
// GPUDiskWriteStream). Members between the ones this slice touches (set only by
// Construct/Open in their own TUs) are omitted; no offset is asserted across them.

#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"                          // CGS_ASSERT
#include "GameShared/GameClasses/System/FileSystem/CgsDeviceManager.h"      // DeviceManager, Handle

namespace BrnReplays
{
    class DiskReadStream
    {
    public:
        // ---- sizing (DWARF BrnReplayDiskReadStream.h:81-82) ----
        static const s32 KI_MAX_STREAM_BLOCKS = 32;    // ring slot count
        static const s32 KI_BLOCK_SIZE        = 65536; // default stream block size (bytes)

        // ---- ReadStreamBlock status flags (DWARF BrnReplayDiskReadStream.h:40-42) ----
        static const u32 KU_RSBFLAG_EMPTY   = 0; // slot free
        static const u32 KU_RSBFLAG_READING = 1; // async read in flight
        static const u32 KU_RSBFLAG_FULL    = 2; // slot holds readable data

        // Stream lifecycle status (DWARF BrnReplayDiskReadStream.h:73).
        enum EStreamStatus
        {
            E_STATUS_CLOSED  = 0,
            E_STATUS_OPENING = 1,
            E_STATUS_OPEN    = 2,
            E_STATUS_ERROR   = 3,
        };

        // ---- one ring slot ----
        // X360 stride is 32 bytes (8 words); the ARTIST asm reaches the 64-bit file
        // position via an 8-byte ld/std/cmpld at slot+0x00, the buffer offset at +0x08,
        // the readable-byte window at +0x0C/+0x10, the flag word at +0x14 and the owner
        // back-pointer at +0x18 (ReadCallback loads context+0x18). Reached BY NAME.
        struct ReadStreamBlock
        {
            s64             miFilePos;    // +0x00 64-bit file position this slot holds
            s32             miStreamPos;  // +0x08 buffer offset it reads into (index*blockSize)
            s32             miDataEnd;    // +0x0C readable-window end   (avail = end - start)
            s32             miDataStart;  // +0x10 readable-window start (consumed cursor)
            u32             muFlags;      // +0x14 KU_RSBFLAG_*
            DiskReadStream* mpOwner;      // +0x18 owning stream (set in Construct)
        };

        // ---- lifecycle (reconstructed in their own TUs) ----
        void         Construct();
        void         Open(const char* lpcFileName, void* lpBuffer, s32 liBufferSize);
        void         Close();
        EStreamStatus GetStatus() const;

        // @0x8265CE08. Try to satisfy a read of liDataSize bytes starting at file
        // position liFilePosition (both multiples of the block size) out of the buffered
        // ring, copying into lpData when supplied. Returns true if the data was ready and
        // delivered, false (after kicking Service) if it still has to be streamed in.
        bool ReadBlock(s32 liFilePosition, void* lpData, s32 liDataSize);

        // @0x82659EB8. Set the looping read window [liStartFilePos, liEndFilePos) and
        // request a rebuild of the ring on the next service. Called by
        // BrnReplays::ReadStream::StartNewStream.
        void SetRange(s32 liStartFilePos, s32 liEndFilePos);

        // ---- async device-op completion callbacks (free statics; the context recovers
        //      the stream). @0x8265A670 read-complete forwards to OnRead via the block's
        //      owner; @0x82650F98 close-complete forwards to OnClose on the stream. ----
        static void ReadCallback(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpContext);
        static void CloseCallback(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpContext);

    private:
        // @0x8264D8C0. Clear every ring slot and reset the ring cursors/counters (only
        // legal while unlocked with no requests/ops outstanding).
        void ResetStreamBlocks();

        // @0x8264DCD0. Drop already-consumed (fully-read, data-empty) FULL slots from the
        // front of the ring, advancing the output cursor.
        void ChopEOFBlocks();

        // @0x82650CB8. Total readable bytes currently buffered across the contiguous run of
        // FULL slots from the output cursor (locks the subsystem mutex).
        s32  GetAmountOfDataInBuffer();

        // @0x82659D40. Per-service pump: refresh the file priority, then -- once no ops
        // are outstanding -- close, rebuild the range, and/or submit the next read.
        void Service();

        // @0x8264DA80. Issue the next async disk Read into the current input slot,
        // advancing the ring and the wrapped next-input position. Returns false when the
        // ring is already full.
        bool SubmitReadRequest();

        // ---- reconstructed elsewhere; declared for the callers in this TU ----
        bool SubmitCloseRequest();
        s32  GetCurrentFilePriority();
        void OnRead(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpContext);
        void OnClose(s32 liResult, CgsFileSystem::Handle lHandle, u64 luSize, void* lpContext);
        bool StartAsyncReadInternal(void** lppData, s32* lpiSize);
        void StopAsyncReadInternal(s32 liBlockSize);

        // ---- instance layout (faithful ARTIST field ORDER; x64 widths, NOT byte-exact) ----
        // Subsystem lock. DWARF: `Mutex mMutex` at object offset 0; the X360 inlines its
        // lock/unlock to RtlEnter/LeaveCriticalSection(this) (the critical section is the
        // mutex's first member). Modelled as an opaque blob reached by name -- same
        // pattern as the sibling GPUDiskWriteStream's lock; the real threading type lands
        // with the mutex layer.
        u8              mMutex[40];                          // X360 +0x000
        char            macFileName[256];                    // X360 +0x024 stream file name
        char*           mpBuffer;                            // X360 +0x124 ring backing store
        s32             miBufferSize;                        // X360 +0x128
        s32             miNumBlocks;                         // X360 +0x12C ring slot count in use
        s32             miBlockSize;                         // X360 +0x130 bytes per stream block
        // Two configured device-read priorities picked by GetCurrentFilePriority: the
        // relaxed one is used while the buffer is healthy (>=25% full) or already serviced,
        // the urgent one while the buffer is starved. Set at Open (its own TU); their
        // existence + read sites are grounded in the GetCurrentFilePriority asm
        // (lwz 0x134 / lwz 0x138), the names are inferred from that use.
        s32             miNormalPriority;                    // X360 +0x134 priority when buffered
        s32             miUrgentPriority;                    // X360 +0x138 priority when starved
        // DWARF: rw::core::filesys::Handle* volatile mpHandle. Read/written as 8 bytes and
        // truncated to CgsFileSystem::Handle at the Read call (same as the sibling's u64).
        u64             muHandle;                            // X360 +0x140 open device file handle
        ReadStreamBlock maBlocks[KI_MAX_STREAM_BLOCKS];      // X360 +0x148 the ring
        s32             miOutputBlock;                       // X360 +0x558 next slot to consume
        bool            mbLockedForRead;                     // X360 +0x55C locked by a reader
        s64             miReadPosition;                      // X360 +0x560 playback read cursor
        s32             miInputBlock;                        // X360 +0x568 next slot to fill
        s32             miInputRequestCount;                 // X360 +0x56C async reads outstanding
        s64             miNextInputPosition;                 // X360 +0x570 file pos of next read (wraps)
        bool            mbServiced;                          // X360 +0x578 service settled flag
        bool            mbWaitingToClose;                    // X360 +0x579 close requested
        s32             miBlocksUsed;                        // X360 +0x57C buffered slot count
        s32             miCurrentPriority;                   // X360 +0x580 current file priority
        bool            mbAdjustingRange;                    // X360 +0x584 range change pending
        EStreamStatus   meStatus;                            // X360 +0x588 lifecycle status
        s32             miPendingOperationCount;             // X360 +0x58C device ops outstanding
        s32             miLoopStart;                         // X360 +0x590 loop window start
        s32             miLoopEnd;                           // X360 +0x594 loop window end
    };
}
