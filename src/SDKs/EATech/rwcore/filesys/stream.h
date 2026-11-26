#pragma once

#include "types.hpp"

#include "SDKs/EATech/rwcore/filesys/device.h"   // AsyncOp / Handle / Device / Manager / gpFileSys*

#include "SDKs/EATech/eathread/eathread_mutex.h"

// =====================================================================================
// rw::core::filesys::Stream -- the RenderWare core filesystem streaming reader.
//
// ARTIST establishes the native offsets. DecFIGS stream.h:243 and :246-248
// declare the EA mutex and the Filter, Tap and Chunk lists of the shared stream.
//
// DecFIGS stream.h:50 declares ONE struct Stream: the shared streaming engine (fields
// stream.h:241..276, the offsets ARTIST reads). Its private helpers (stream.h:224-239:
// decbufferusage, getfreerequest, queuerequest, freerequest, parsechunks, readcallback,
// startnextrequest, restartstream) are non-static members; PS3 DecFIGS mangles them with an
// implicit `this` (e.g. _ZN2rw4core7filesys6Stream14getfreerequestEv), and ARTIST sets r3 to
// the engine before evaluating the arguments of each call (Stream::opencallback 0x82BBE4E4,
// ReleaseChunk 0x82BC0A2C). The public API runs on a tap handle (CreateTap returns a
// Stream*): the tap's +0x04 is its engine, which GetStreamHeaderFromHandle (stream.h:224,
// PS3 0xE46818 `lwz r3,4(r4)`) returns.
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
            struct Stream;
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
            //   +0x00 miRequestId : owning request's id (low byte == its slot index)
            //   +0x04 muSize      : chunk byte size
            //   +0x08 mpData      : chunk data pointer inside the ring buffer
            //   +0x0C miState     : 0 pending, 1 handed out, >=2 released/free-able
            //   +0x10 miKey       : region ordinal this chunk belongs to
            struct Chunk
            {
                s32  miRequestId;   // node +0x04  (DWARF ChunkInfo::reqId int)
                u32  muSize;        // node +0x08
                u8*  mpData;        // node +0x0C
                s32  miState;       // node +0x10
                s32  miKey;         // node +0x14  (tap id; ARTIST cmpw, CancelRequest 0x82BBFF94)
            };

            // A buffered chunk record. The engine keeps a singly-linked list of these
            // (mpChunkHead/Tail); GetChunk/CancelRequest walk them and ReleaseChunk/
            // restartstream free them. Allocated through Manager::Allocate (X360: 24 bytes).
            //
            // parsechunks creates one with a new-expression: ARTIST tests the allocation for
            // null before the stores (0x82BBF2BC) and loads every field value before the first
            // store (0x82BBF2C4..0x82BBF2D0), i.e. constructor arguments, while the PS3 body
            // (0xE491C4) stores into Manager::Allocate's block untested (a class operator new).
            struct ChunkNode
            {
                ChunkNode* mpNext;   // +0x00
                Chunk      mChunk;   // +0x04 ..

                ChunkNode(s32 liRequestId, u32 luSize, u8* lpData, s32 liKey)
                {
                    mpNext = 0;
                    mChunk.miState = 0;
                    mChunk.miKey = liKey;
                    mChunk.mpData = lpData;
                    mChunk.miRequestId = liRequestId;
                    mChunk.muSize = luSize;
                }

                static void* operator new(size_t luSize);
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

            // A tap (DWARF stream.h:247 `ListSingle<TapStruct> tap`): one consumer of the
            // shared stream. The public `Stream*` handle a client holds is a tap: GetChunk walks
            // the chunks carrying its id, parsechunks charges each chunk's bytes to its tap,
            // and GetStreamHeaderFromHandle reads the engine from +0x04. Tap ids are 1-based
            // positions in the list.
            struct TapStruct
            {
                TapStruct*    mpNext;          // +0x00
                Stream*       mpStream;        // +0x04  the shared engine
                s32           miKey;           // +0x08  tap id (== Chunk::miKey)
                s32           miUsage;         // +0x0C  buffered bytes left for this tap
                ChunkNode*    mpCurrentChunk;  // +0x10  next chunk GetChunk hands out
            };

            // A queued file request. Requests live in a fixed array (Stream::mpRequests,
            // X360 stride 0x140) plus an intrusive free list / active doubly-linked list.
            //   +0x00 miId          : request id (generation<<8 | slot index)
            //   +0x04 miState       : 0 free, 1 queued, 2/3 active, 4 cancelled/done
            //   +0x08 mpPrev/+0x0C mpNext : active-list links (also free-list via mpNext)
            //   +0x10 miRequestType : 1 for a memory-backed request
            //   +0x14 macPath[255]  : file path
            //   +0x114 mpPreOpenHandle : optional already-open Handle (0 -> Open via path)
            //   +0x118 mpMappedData : source pointer for the in-memory (mapped) read path
            //   +0x120 mi64Size     : signed size or initial file offset, depending on request type
            //   +0x128 mpBufferPos  : the request's target ring-buffer position
            //   +0x12C mbAtEnd      : end-of-stream reached
            //   +0x130 mpfnParse    : chunk-parse callback
            //   +0x134 mpParseContext : callback user context
            //   +0x138 muBufferedBytes : bytes currently buffered for this request
            struct Request
            {
                s32                miId;             // +0x00  (int: ARTIST cmpw, Gettable 0x82BBD96C)
                s32                miState;          // +0x04
                Request*           mpPrev;           // +0x08
                Request*           mpNext;           // +0x0C
                s32                miRequestType;        // +0x10
                char               macPath[255];     // +0x14
                u8                 mbField113;       // +0x113
                Handle*            mpPreOpenHandle;  // +0x114
                u8*                mpMappedData;     // +0x118
                s64                mi64Size;         // +0x120
                u8*                mpBufferPos;      // +0x128
                u8                 mbAtEnd;          // +0x12C
                ChunkParseCallback mpfnParse;        // +0x130
                void*              mpParseContext;   // +0x134
                u32                muBufferedBytes;  // +0x138
            };

            // The shared streaming engine (DecFIGS stream.h:50).
            struct Stream
            {
                u32                muReserved00;         // X360 +0x00
                u32                muFlags;              // X360 +0x04  (bit 0 = borrowed handle)
                EA::Thread::Mutex  mLock;                // X360 +0x08
                Request*           mpRequests;           // X360 +0x38  (fixed array; stride 0x140)
                s32                miRequestCount;       // X360 +0x3C  (DWARF stream.h:245 int requests)
                detail::ListSingle<ChunkHandler> mFilters; // X360 +0x40
                detail::ListSingle<TapStruct> mTaps;        // X360 +0x4C
                detail::ListSingle<ChunkNode> mChunks;     // X360 +0x58
                u8*                mpBufferBase;         // X360 +0x64
                u8*                mpWritePtr;           // X360 +0x68
                u8*                mpBufferTop;          // X360 +0x6C
                volatile s32       miState;              // X360 +0x70  (0 idle,1 running,2 waiting,3/4)
                s32                miNormalPriority;     // X360 +0x74
                s32                miUrgentPriority;     // X360 +0x78
                s32                miThreshold;          // X360 +0x7C  (low-water mark)
                bool               mbNeedMore;           // X360 +0x80
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
                s64                mi64ReadPos;          // X360 +0x1B0
                AsyncOp            mAsyncOp;             // X360 +0x1B8 (0x48 bytes)
                s64                mi64ChunkBytes;       // X360 +0x200
                s32                miReadGranularity;    // X360 +0x208
                u64                mu64FileSize;         // X360 +0x210

                // ---- public API (this == a tap handle) ------------------------------------
                s32    QueueFile(const char* lpcPath, Handle* lpPreOpenHandle, s64 li64Size,
                                 ChunkParseCallback lpfnParse, void* lpParseContext);
                Chunk* GetChunk();
                void   ReleaseChunk(Chunk* lpChunk);
                void   CancelRequest(s32 liRequestId);
                s32    Gettable() const;
                s32    Gettable(s32 liRequestId) const;
                s32    GetRequestState(s32 liRequestId) const;
                s32    GetState() const;
                bool   IsEndOfStream() const;
                u64    GetFileSize() const;
                void   Kill();

                // stream.h:224 -- the engine behind a tap handle (inlined at every ARTIST use,
                // e.g. QueueFile 0x82BC04AC `lwz r30,4(r3)`).
                Stream* GetStreamHeaderFromHandle(const Stream* lpHandle) const
                {
                    return reinterpret_cast<const TapStruct*>(lpHandle)->mpStream;
                }

                // ---- private engine helpers (stream.h:225-237, this == the engine) ----------
                void         decbufferusage(s32 liBytes);
                Request*     getfreerequest();
                Request*     locaterequest(s32 liRequestId);
                void         queuerequest(Request* lpReq);
                void         freerequest(Request* lpReq);
                s32          parsechunks();
                void         readcallback();
                void         startnextrequest(s32 liPriority);
                void         restartstream(s32 liPriority);

                // ---- async-op completion callbacks (the op's context is the engine) --------
                static void  opencallback(AsyncOp* lpOp);
                static void  closecallback(AsyncOp* lpOp);
                static void  filereadcallback(AsyncOp* lpOp);
            };

            CGS_STATIC_ASSERT(offsetof(Stream, mpRequests) == 0x38, "ARTIST request table");
            CGS_STATIC_ASSERT(offsetof(Stream, mFilters) == 0x40, "ARTIST chunk filter list");
            CGS_STATIC_ASSERT(offsetof(Stream, mTaps) == 0x4C, "ARTIST buffer tap list");
            CGS_STATIC_ASSERT(offsetof(Stream, mChunks) == 0x58, "ARTIST chunk list");
            CGS_STATIC_ASSERT(offsetof(Stream, miState) == 0x70, "ARTIST stream state");
            CGS_STATIC_ASSERT(offsetof(Stream, mAsyncOp) == 0x1B8, "ARTIST embedded operation");
            CGS_STATIC_ASSERT(offsetof(Stream, mu64FileSize) == 0x210, "ARTIST stream file size");
            CGS_STATIC_ASSERT(sizeof(Request) == 0x140, "ARTIST stream request stride");

        }
    }
}
