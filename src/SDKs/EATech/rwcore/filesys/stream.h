#pragma once

#include "types.hpp"

#include "SDKs/EATech/rwcore/filesys/device.h"   // AsyncOp / Handle / Device / Manager / gpFileSys*

#include <windows.h>  // CRITICAL_SECTION -- the host mapping of the X360 EA::Thread::Mutex

// =====================================================================================
// rw::core::filesys::Stream -- the RenderWare core filesystem streaming reader.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU. Members are modelled by
// NAME at the X360 offsets proven by the asm (never accessed via raw `this+N` casts); the
// host layout follows the PC ABI (pointers widen to 8 bytes), exactly as the sibling
// device.h/asyncop.h do. The X360 EA::Thread::Mutex is a host Win32 CRITICAL_SECTION
// (Lock/Unlock -> Enter/LeaveCriticalSection), as device.cpp already models it.
//
// The public API is a lightweight cursor object (`Stream`) that references a shared
// streaming engine (`StreamState`, the object `Stream::mpState` points at). Both are
// distinct byte layouts (a `Stream` handle's +0x08 is a cursor key, a `StreamState`'s
// +0x08 is the engine mutex), so they are two structs; the private engine helpers are
// static members of `Stream` that take the engine explicitly (matching the asm, which
// passes the engine in r3 with no this-adjust).
//
//   rw::core::filesys::Stream::QueueFile        @0x82BC04A0
//   rw::core::filesys::Stream::GetChunk         @0x82BBD878
//   rw::core::filesys::Stream::ReleaseChunk     @0x82BC09C8
//   rw::core::filesys::Stream::CancelRequest    @0x82BBFE60
//   rw::core::filesys::Stream::GetRequestState  @0x82BBD9D0
//   rw::core::filesys::Stream::GetState         @0x82BBD990
//   rw::core::filesys::Stream::IsEndOfStream    @0x82BBD9A0
//   rw::core::filesys::Stream::GetFileSize      @0x82BBDA10
//   rw::core::filesys::Stream::Kill             @0x82BBFFD8
//   rw::core::filesys::Stream::freerequest      @0x82BBD7F8
//   rw::core::filesys::Stream::getfreerequest   @0x82BBD708
//   rw::core::filesys::Stream::queuerequest     @0x82BBD780
//   rw::core::filesys::Stream::decbufferusage   @0x82BBFDC0
//   rw::core::filesys::Stream::parsechunks      @0x82BBF198
//   rw::core::filesys::Stream::readcallback     @0x82BC0598
//   rw::core::filesys::Stream::restartstream    @0x82BC06D0
//   rw::core::filesys::Stream::opencallback     @0x82BBE490
//   rw::core::filesys::Stream::closecallback    @0x82BC0268
//   rw::core::filesys::Stream::filereadcallback @0x82BC06C8
// =====================================================================================

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            struct StreamState;
            struct ChunkNode;

            // The chunk-parse callback a queued request carries. It inspects the freshly
            // buffered bytes and reports how many belong to the next chunk (via lpuConsumed),
            // returning non-zero when it recognised one. Args mirror the X360 call verbatim:
            //   (bufStart, bufLen, requestId, parseContext, handlerA, handlerB, &consumed)
            typedef s32 (*ChunkParseCallback)(u8* lpBuf, u32 luLen, u32 luRequestId,
                                              void* lpContext, u32 luHandlerA, u32 luHandlerB,
                                              u32* lpuConsumed);

            // The public chunk descriptor GetChunk returns (the X360 hands back the node's
            // +0x04, i.e. &ChunkNode::mChunk) and ReleaseChunk takes back.
            //   +0x00 muRequestId : owning request's id (low byte == its slot index)
            //   +0x04 muSize      : chunk byte size
            //   +0x08 mpData      : chunk data pointer inside the ring buffer
            //   +0x0C miState     : 0 pending, 1 handed out, >=2 released/free-able
            //   +0x10 muKey       : region ordinal this chunk belongs to
            struct Chunk
            {
                u32  muRequestId;   // node +0x04
                u32  muSize;        // node +0x08
                u8*  mpData;        // node +0x0C
                s32  miState;       // node +0x10
                u32  muKey;         // node +0x14
            };

            // A buffered chunk record. The engine keeps a singly-linked list of these
            // (mpChunkHead/Tail); GetChunk/CancelRequest walk them and ReleaseChunk/
            // restartstream free them. Allocated through Manager::Allocate (X360: 24 bytes).
            struct ChunkNode
            {
                ChunkNode* mpNext;   // +0x00
                Chunk      mChunk;   // +0x04 ..
            };

            // A registered chunk-type handler. parsechunks walks the engine's handler list
            // and offers the buffered bytes to each request's parse callback with the
            // handler's two opaque words; a match latches the handler's region ordinal.
            struct ChunkHandler
            {
                ChunkHandler* mpNext;         // +0x00
                u32           muField04;      // +0x04  parse-callback arg 5
                u32           muField08;      // +0x08  parse-callback arg 6
                s32           miRegionIndex;  // +0x0C  1-based region ordinal on a match
            };

            // A ring-buffer region. The engine tracks buffered-byte usage per region and the
            // chunk currently occupying it; region ordinals are 1-based positions in the list.
            struct BufferRegion
            {
                BufferRegion* mpNext;          // +0x00
                u32           muField04;       // +0x04
                u32           muKey;           // +0x08  region ordinal (== Chunk::muKey)
                s32           miUsage;         // +0x0C  buffered bytes charged to this region
                ChunkNode*    mpCurrentChunk;  // +0x10  chunk currently occupying the region
            };

            // A queued file request. Requests live in a fixed array (StreamState::mpRequests,
            // X360 stride 0x140) plus an intrusive free list / active doubly-linked list.
            //   +0x00 miId          : request id (generation<<8 | slot index)
            //   +0x04 miState       : 0 free, 1 queued, 2/3 active, 4 cancelled/done
            //   +0x08 mpPrev/+0x0C mpNext : active-list links (also free-list via mpNext)
            //   +0x10 muField10     : 1 == data mapped in memory (mpMappedData), else async
            //   +0x14 macPath[255]  : file path
            //   +0x114 mpPreOpenHandle : optional already-open Handle (0 -> Open via path)
            //   +0x118 mpMappedData : source pointer for the in-memory (mapped) read path
            //   +0x120 mu64Size     : file/stream byte size
            //   +0x128 mpBufferPos  : the request's target ring-buffer position
            //   +0x12C mbAtEnd      : end-of-stream reached
            //   +0x130 mpfnParse    : chunk-parse callback
            //   +0x134 mpParseContext : callback user context
            //   +0x138 muBufferedBytes : bytes currently buffered for this request
            struct Request
            {
                u32                miId;             // +0x00
                s32                miState;          // +0x04
                Request*           mpPrev;           // +0x08
                Request*           mpNext;           // +0x0C
                u32                muField10;        // +0x10
                char               macPath[255];     // +0x14
                u8                 mbField113;       // +0x113
                Handle*            mpPreOpenHandle;  // +0x114
                u8*                mpMappedData;     // +0x118
                u64                mu64Size;         // +0x120
                u8*                mpBufferPos;      // +0x128
                u8                 mbAtEnd;          // +0x12C
                ChunkParseCallback mpfnParse;        // +0x130
                void*              mpParseContext;   // +0x134
                u32                muBufferedBytes;  // +0x138
            };

            // The shared streaming engine (the object a Stream cursor's mpState points at).
            // The X360 byte offsets (shown for provenance) are NOT reproduced literally --
            // the host CRITICAL_SECTION / widened pointers change them -- but the field ORDER
            // mirrors the X360 object and every access is by NAME.
            struct StreamState
            {
                u32                muReserved00;         // X360 +0x00
                u32                muFlags;              // X360 +0x04  (bit0 = open-in-progress)
                CRITICAL_SECTION   mLock;                // X360 +0x08  (EA::Thread::Mutex)
                Request*           mpRequests;           // X360 +0x38  (fixed array; stride 0x140)
                u32                muRequestCount;       // X360 +0x3C
                ChunkHandler*      mpChunkHandlerHead;   // X360 +0x40
                BufferRegion*      mpRegionHead;         // X360 +0x4C
                ChunkNode*         mpChunkHead;          // X360 +0x58
                ChunkNode*         mpChunkTail;          // X360 +0x5C
                u32                muChunkCount;         // X360 +0x60
                u8*                mpBufferBase;         // X360 +0x64
                u8*                mpWritePtr;           // X360 +0x68
                u8*                mpBufferTop;          // X360 +0x6C
                s32                miState;              // X360 +0x70  (0 idle,1 running,2 waiting,3/4)
                s32                miNormalPriority;     // X360 +0x74
                s32                miUrgentPriority;     // X360 +0x78
                s32                miThreshold;          // X360 +0x7C  (low-water mark)
                u8                 mbNeedMore;           // X360 +0x80
                s32                miBufferUsage;        // X360 +0x84
                u8*                mpFillPtr;            // X360 +0x88
                u8*                mpParsePtr;           // X360 +0x8C
                u8*                mpDataEndPtr;         // X360 +0x90
                Request*           mpReqHead;            // X360 +0x94
                Request*           mpReqCurrent;         // X360 +0x98
                Request*           mpReqTail;            // X360 +0x9C
                Request*           mpFreeRequest;        // X360 +0xA0
                ChunkParseCallback mpfnDefaultParse;     // X360 +0xA4
                void*              mpDefaultParseContext;// X360 +0xA8
                char               macOpenPath[256];     // X360 +0xAC .. +0x1AB
                Handle*            mpHandle;             // X360 +0x1AC
                u64                mu64ReadPos;          // X360 +0x1B0
                AsyncOp            mAsyncOp;             // X360 +0x1B8 (0x48 bytes)
                u64                mu64ChunkBytes;       // X360 +0x200
                s32                miReadGranularity;    // X360 +0x208
                u64                mu64FileSize;         // X360 +0x210
            };

            // The public streaming cursor. A thin handle over a shared StreamState engine.
            //   +0x00 muReserved00
            //   +0x04 mpState        : the shared engine
            //   +0x08 muKey          : this cursor's chunk key (matched to Chunk::muKey)
            //   +0x0C miRemaining    : bytes left for this cursor to consume
            //   +0x10 mpCurrentChunk : the chunk node GetChunk will hand out next
            struct Stream
            {
                u32          muReserved00;    // +0x00
                StreamState* mpState;         // +0x04
                u32          muKey;           // +0x08
                s32          miRemaining;     // +0x0C
                ChunkNode*   mpCurrentChunk;  // +0x10

                // ---- public API (this == the cursor handle) --------------------------------
                s32    QueueFile(const char* lpcPath, Handle* lpPreOpenHandle, u64 lu64Size,
                                 ChunkParseCallback lpfnParse, void* lpParseContext);
                Chunk* GetChunk();
                s32    ReleaseChunk(Chunk* lpChunk);
                s32    CancelRequest(u32 luRequestId);
                s32    GetRequestState(u32 luRequestId) const;
                s32    GetState() const;
                bool   IsEndOfStream() const;
                u64    GetFileSize() const;
                s32    Kill();

                // ---- private engine helpers (the asm passes the engine in r3) ---------------
                static StreamState* freerequest(StreamState* lpState, Request* lpReq);
                static Request*     getfreerequest(StreamState* lpState);
                static s32          queuerequest(StreamState* lpState, Request* lpReq);
                static s32          decbufferusage(StreamState* lpState, s32 liBytes);
                static s32          parsechunks(StreamState* lpState);
                static s32          readcallback(StreamState* lpState);
                static s32          restartstream(StreamState* lpState, s32 liPriority);

                // Owned by its own (not-yet-homed) TU; declared so this one compiles.
                static s32          startnextrequest(StreamState* lpState, s32 liPriority);

                // ---- async-op completion callbacks (op->mUserData carries the engine) -------
                static s32          opencallback(AsyncOp* lpOp);
                static s32          closecallback(AsyncOp* lpOp);
                static s32          filereadcallback(AsyncOp* lpOp);
            };
        }
    }
}
