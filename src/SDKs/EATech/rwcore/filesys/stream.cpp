// =====================================================================================
// rw::core::filesys::Stream -- the RenderWare core filesystem streaming reader.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative. No
// reference source and no DecFIGS DWARF hints exist for this TU. See stream.h for the
// object model. The X360 EA::Thread::Mutex is a host Win32 CRITICAL_SECTION, exactly as
// device.cpp models the sibling scheduler's mutex (Lock/Unlock -> Enter/LeaveCriticalSection;
// the X360 Unlock's r3 result is the released lock count == 0 on the host).
// =====================================================================================

#include "SDKs/EATech/rwcore/filesys/stream.h"

#include <windows.h>  // Enter/LeaveCriticalSection
#include <cstring>    // strncpy, memcpy

namespace rw
{
    namespace core
    {
        namespace filesys
        {
            // X360 dword_8327F074 -- a process-wide request-generation counter. Each freed
            // request slot handed out gets a fresh generation in its id's high bits so a
            // stale id (same slot, older generation) fails the id-match validity checks.
            static u32 guRequestGeneration = 0;

            // -----------------------------------------------------------------------------
            // Stream::getfreerequest @0x82BBD708 -- pop a free request slot and stamp it with
            // the next generation (slot index kept in the low byte).
            // -----------------------------------------------------------------------------
            Request* Stream::getfreerequest(StreamState* lpState)
            {
                ::EnterCriticalSection(&lpState->mLock);

                Request* lpReq = lpState->mpFreeRequest;
                if (lpReq)
                {
                    lpState->mpFreeRequest = lpReq->mpNext;

                    guRequestGeneration += 0x100u;
                    if (guRequestGeneration == 0)
                        guRequestGeneration = 0x100u;

                    lpReq->miId = static_cast<s32>((static_cast<u32>(lpReq->miId) & 0xFFu) | guRequestGeneration);
                }

                ::LeaveCriticalSection(&lpState->mLock);
                return lpReq;
            }

            // -----------------------------------------------------------------------------
            // Stream::freerequest @0x82BBD7F8 -- unlink lpReq from the active list and push it
            // onto the free list. Returns the engine (the X360 r3 is unchanged).
            // -----------------------------------------------------------------------------
            StreamState* Stream::freerequest(StreamState* lpState, Request* lpReq)
            {
                if (lpReq == lpState->mpReqHead)
                    lpState->mpReqHead = lpReq->mpNext;
                else
                    lpReq->mpPrev->mpNext = lpReq->mpNext;

                if (lpReq == lpState->mpReqTail)
                    lpState->mpReqTail = lpReq->mpPrev;
                else
                    lpReq->mpNext->mpPrev = lpReq->mpPrev;

                if (lpReq == lpState->mpReqCurrent)
                {
                    Request* lpNext = lpReq->mpNext;
                    if (!lpNext)
                        lpNext = lpReq->mpPrev;
                    lpState->mpReqCurrent = lpNext;
                }

                lpReq->miState = 0;
                lpReq->mpNext = lpState->mpFreeRequest;
                lpState->mpFreeRequest = lpReq;
                return lpState;
            }

            // -----------------------------------------------------------------------------
            // Stream::queuerequest @0x82BBD780 -- append lpReq to the tail of the active list.
            // -----------------------------------------------------------------------------
            s32 Stream::queuerequest(StreamState* lpState, Request* lpReq)
            {
                lpReq->miState = 1;
                lpReq->mpNext  = nullptr;

                ::EnterCriticalSection(&lpState->mLock);

                Request* lpTail = lpState->mpReqTail;
                if (lpTail)
                {
                    lpReq->mpPrev = lpTail;
                    lpState->mpReqTail->mpNext = lpReq;
                }
                else
                {
                    lpReq->mpPrev = nullptr;
                    lpState->mpReqHead    = lpReq;
                    lpState->mpReqCurrent = lpReq;
                }
                lpState->mpReqTail = lpReq;

                ::LeaveCriticalSection(&lpState->mLock);
                return 0;
            }

            // -----------------------------------------------------------------------------
            // Stream::decbufferusage @0x82BBFDC0 -- drop liBytes from the buffered-byte total.
            // If that pushes usage below the low-water mark while a read op is in flight, flag
            // "need more" and bump the outstanding read op to the urgent priority (redirecting
            // to the default device when the op's device runs on an external thread).
            // -----------------------------------------------------------------------------
            s32 Stream::decbufferusage(StreamState* lpState, s32 liBytes)
            {
                ::EnterCriticalSection(&lpState->mLock);
                s32 liOldUsage = lpState->miBufferUsage;
                lpState->miBufferUsage = liOldUsage - liBytes;
                ::LeaveCriticalSection(&lpState->mLock);

                s32 liResult = 0;
                s32 liThreshold = lpState->miThreshold;
                if (liOldUsage >= liThreshold && (liOldUsage - liBytes) < liThreshold)
                {
                    s32 liEngineState = lpState->miState;
                    lpState->mbNeedMore = 1;
                    if (liEngineState == 1 && lpState->mAsyncOp.miPriority != lpState->miUrgentPriority)
                    {
                        Device* lpDevice = lpState->mAsyncOp.mpDevice;
                        if (lpDevice->IsExternalThread())
                            lpDevice = gpFileSysManager->mpDefaultDevice;
                        liResult = lpDevice->ChangeOpPriority(&lpState->mAsyncOp, lpState->miUrgentPriority);
                    }
                }
                return liResult;
            }

            // -----------------------------------------------------------------------------
            // Stream::parsechunks @0x82BBF198 -- carve the freshly buffered bytes into chunks
            // by offering them to the current request's parse callback (once per registered
            // handler). Each recognised chunk is recorded, charged to its region, and the
            // parse cursor advanced. Returns 0 (more to parse), 1 (buffer drained, no chunk),
            // or 2 (end-of-stream flush).
            // -----------------------------------------------------------------------------
            s32 Stream::parsechunks(StreamState* lpState)
            {
                s32 liRegion = 1;
                s32 liResult = (lpState->mpParsePtr >= lpState->mpDataEndPtr) ? 1 : 0;
                Request* lpReq = lpState->mpReqCurrent;

                if (lpState->mpParsePtr < lpState->mpDataEndPtr)
                {
                    while (lpReq->miState != 4)
                    {
                        u32 luConsumed = 0;
                        s32 liParse = 0;
                        for (ChunkHandler* lpH = lpState->mpChunkHandlerHead; lpH; lpH = lpH->mpNext)
                        {
                            liParse = lpReq->mpfnParse(
                                lpState->mpParsePtr,
                                static_cast<u32>(lpState->mpDataEndPtr - lpState->mpParsePtr),
                                static_cast<u32>(lpReq->miId),
                                lpReq->mpParseContext,
                                lpH->muField04,
                                lpH->muField08,
                                &luConsumed);
                            if (liParse)
                            {
                                liRegion = lpH->miRegionIndex;
                                break;
                            }
                        }

                        if (!liParse)
                        {
                            // No handler matched: flush the remaining bytes as one final chunk
                            // only when the stream has reached its end.
                            if (!lpState->mpReqCurrent->mbAtEnd)
                            {
                                liResult = 1;
                                break;
                            }
                            liRegion = 1;
                            liParse  = 2;
                            luConsumed = static_cast<u32>(lpState->mpDataEndPtr - lpState->mpParsePtr);
                            if (!luConsumed)
                                return 2;
                        }

                        ::EnterCriticalSection(&lpState->mLock);
                        if (lpReq->miState == 4)
                        {
                            ::LeaveCriticalSection(&lpState->mLock);
                            liResult = 1;
                            break;
                        }

                        // X360 allocated a fixed 24-byte node; on the PC target the node is
                        // sizeof(ChunkNode) (pointer-widened).
                        ChunkNode* lpNode = static_cast<ChunkNode*>(Manager::Allocate(sizeof(ChunkNode)));
                        if (lpNode)
                        {
                            lpNode->mpNext          = nullptr;
                            lpNode->mChunk.miState  = 0;
                            lpNode->mChunk.muKey    = static_cast<u32>(liRegion);
                            lpNode->mChunk.mpData   = lpState->mpParsePtr;
                            lpNode->mChunk.muRequestId = static_cast<u32>(lpReq->miId);
                            lpNode->mChunk.muSize   = luConsumed;
                        }

                        // Push the node onto the chunk-list tail. (The X360 dereferences the
                        // node unconditionally here -- an OOM node would fault -- so keep the
                        // same shape rather than inventing a graceful path.)
                        ChunkNode* lpTail = lpState->mpChunkTail;
                        lpNode->mpNext = nullptr;
                        lpState->mpChunkTail = lpNode;
                        lpState->muChunkCount++;
                        if (lpTail)
                            lpTail->mpNext = lpNode;
                        else
                            lpState->mpChunkHead = lpNode;

                        // Charge the chunk to its region and the owning request.
                        BufferRegion* lpRegion = lpState->mpRegionHead;
                        for (s32 li = liRegion - 1; li; --li)
                            lpRegion = lpRegion->mpNext;
                        lpRegion->miUsage += static_cast<s32>(luConsumed);
                        lpReq->muBufferedBytes += luConsumed;
                        if (lpRegion->miUsage == static_cast<s32>(luConsumed))
                            lpRegion->mpCurrentChunk = lpNode;

                        // Advance the parse cursor and buffered-byte total; clear "need more"
                        // once usage climbs back above the low-water mark.
                        s32 liOldUsage = lpState->miBufferUsage;
                        lpState->mpParsePtr += luConsumed;
                        s32 liThreshold = lpState->miThreshold;
                        s32 liNewUsage = liOldUsage + static_cast<s32>(luConsumed);
                        lpState->miBufferUsage = liNewUsage;
                        if (liOldUsage < liThreshold && liNewUsage >= liThreshold)
                            lpState->mbNeedMore = 0;

                        lpNode->mChunk.miState = 0;
                        ::LeaveCriticalSection(&lpState->mLock);

                        if (liParse == 2)
                            liResult = 2;
                        if (liResult)
                            break;
                    }
                }
                return liResult;
            }

            // -----------------------------------------------------------------------------
            // Stream::readcallback @0x82BC0598 -- fired when a read (or in-memory fill) lands.
            // Advance the file position by the transferred bytes, flag end-of-stream, parse
            // out any new chunks, then either move to the next request (cancelled / drained /
            // at end) or refill the buffer.
            // -----------------------------------------------------------------------------
            s32 Stream::readcallback(StreamState* lpState)
            {
                Request* lpReq = lpState->mpReqCurrent;

                u64 luReadPos = lpState->mu64ReadPos;
                s64 liTransferred;
                u64 luAlignAdjust;
                if (lpReq->muField10 == 1)
                {
                    liTransferred = static_cast<s64>(lpState->mu64ChunkBytes);
                    lpReq->mbAtEnd = (luReadPos + lpState->mu64ChunkBytes >= lpReq->mu64Size) ? 1 : 0;
                    luAlignAdjust = 0;
                }
                else
                {
                    s64 liResultSize = static_cast<s64>(lpState->mAsyncOp.GetResultSize());
                    lpReq->mbAtEnd = (liResultSize < static_cast<s64>(lpState->mu64ChunkBytes)) ? 1 : 0;
                    liTransferred = liResultSize;
                    luAlignAdjust = luReadPos & 3u;
                }

                u8* lpDataEnd = lpState->mpDataEndPtr;
                u8* lpParse   = lpState->mpParsePtr;
                lpState->mpDataEndPtr = lpDataEnd + liTransferred;
                lpState->mu64ReadPos  = (luReadPos - luAlignAdjust) + static_cast<u64>(liTransferred);
                lpState->mpParsePtr   = lpParse + luAlignAdjust;

                s32 liResult = parsechunks(lpState);

                if (lpReq->miState == 4)
                    return startnextrequest(lpState, lpState->miUrgentPriority);

                if (liResult == 2 || lpReq->mbAtEnd)
                {
                    ::EnterCriticalSection(&lpState->mLock);
                    if (lpReq->miState != 4)
                        lpReq->miState = 3;
                    ::LeaveCriticalSection(&lpState->mLock);
                    return startnextrequest(lpState, lpState->miUrgentPriority);
                }

                if (liResult == 1)
                    return restartstream(lpState, lpState->miUrgentPriority - 1);

                return liResult;
            }

            // -----------------------------------------------------------------------------
            // Stream::restartstream @0x82BC06D0 -- reclaim released chunks, evict requests
            // whose data has already streamed past, compact the ring buffer if that frees
            // enough room, then issue the next read (or copy the next slice from a mapped
            // request). If there is not a granule's worth of room, park the engine (state 2).
            // -----------------------------------------------------------------------------
            s32 Stream::restartstream(StreamState* lpState, s32 liPriority)
            {
                ::EnterCriticalSection(&lpState->mLock);

                // Phase 1: free chunks that have been released (state >= 2), advancing the
                // fill pointer as the buffer front empties.
                ChunkNode* lpChunk = lpState->mpChunkHead;
                if (lpChunk)
                {
                    while (lpChunk->mChunk.miState >= 2)
                    {
                        ChunkNode* lpHead = lpState->mpChunkHead;
                        if (lpHead)
                        {
                            if (lpHead == lpState->mpChunkTail)
                            {
                                lpState->mpChunkHead = nullptr;
                                lpState->mpChunkTail = nullptr;
                            }
                            else
                            {
                                lpState->mpChunkHead = lpHead->mpNext;
                            }
                            lpState->muChunkCount--;
                            lpHead->mpNext = nullptr;
                        }
                        if (!lpState->mpChunkHead)
                        {
                            uintptr_t luAligned =
                                reinterpret_cast<uintptr_t>(lpChunk->mChunk.mpData + lpChunk->mChunk.muSize)
                                & ~static_cast<uintptr_t>(0xF);
                            lpState->mpFillPtr = reinterpret_cast<u8*>(luAligned);
                        }
                        lpChunk->mpNext = nullptr;
                        Manager::Free(lpChunk);
                        lpChunk = lpState->mpChunkHead;
                        if (!lpChunk)
                            break;
                    }
                    if (lpChunk)
                        lpState->mpFillPtr = lpChunk->mChunk.mpData;
                }

                // Phase 2: evict head requests whose target buffer position no longer lies in
                // the live [fill, end) window (handling ring wrap).
                while (true)
                {
                    Request* lpNextReq = lpState->mpReqHead->mpNext;
                    if (!lpNextReq || lpNextReq->miState == 1)
                        break;

                    uintptr_t luFill = reinterpret_cast<uintptr_t>(lpState->mpFillPtr);
                    uintptr_t luEnd  = reinterpret_cast<uintptr_t>(lpState->mpDataEndPtr);
                    uintptr_t luPos  = reinterpret_cast<uintptr_t>(lpNextReq->mpBufferPos) - 1;

                    bool lbKeep;
                    if (luFill > luEnd)
                    {
                        if (luPos >= luFill)        lbKeep = true;
                        else if (luPos >= luEnd)     lbKeep = false;
                        else                         lbKeep = true;
                    }
                    else
                    {
                        if (luPos < luFill)          lbKeep = false;
                        else if (luPos < luEnd)      lbKeep = true;
                        else                         lbKeep = false;
                    }

                    if (lbKeep)
                        break;
                    freerequest(lpState, lpState->mpReqHead);
                }

                ::LeaveCriticalSection(&lpState->mLock);
                s32 liResult = 0;

                // Phase 3: work out how much contiguous room is free, compacting the buffer if
                // that recovers a granule.
                u8* lpFill = lpState->mpFillPtr;
                u8* lpEnd  = lpState->mpDataEndPtr;
                s32 liSpace;
                if (lpFill > lpEnd)
                {
                    liSpace = static_cast<s32>(lpFill - lpEnd) - 17;
                }
                else
                {
                    liSpace = static_cast<s32>(lpState->mpBufferTop - lpEnd) - 16;
                    if (liSpace < lpState->miReadGranularity)
                    {
                        s32 liKeep = static_cast<s32>(lpEnd - lpState->mpParsePtr);
                        if ((static_cast<s32>(lpFill - lpState->mpWritePtr) - 17) >= liKeep)
                        {
                            u8* lpDst;
                            s32 liRem = liKeep % 16;
                            if (liRem == 0 || lpState->mpReqCurrent->muField10 == 1)
                                lpDst = lpState->mpBufferBase;
                            else
                                lpDst = lpState->mpBufferBase - liRem + 16;

                            lpState->mpWritePtr = lpDst;
                            ::memcpy(lpDst, lpState->mpParsePtr, static_cast<size_t>(liKeep));
                            u8* lpNewEnd = lpDst + liKeep;
                            lpState->mpParsePtr   = lpDst;
                            lpState->mpDataEndPtr = lpNewEnd;
                            liSpace = static_cast<s32>(lpFill - lpNewEnd) - 17;
                        }
                    }
                }

                if (liSpace >= lpState->miReadGranularity)
                {
                    Request* lpReq = lpState->mpReqCurrent;
                    if (lpReq->muField10 == 1)
                    {
                        // Mapped request: copy the next slice straight from memory.
                        u64 luReadPos = lpState->mu64ReadPos;
                        if (luReadPos + static_cast<u64>(static_cast<s64>(liSpace)) <= lpReq->mu64Size)
                            lpState->mu64ChunkBytes = static_cast<u64>(static_cast<s64>(liSpace));
                        else
                            lpState->mu64ChunkBytes = lpReq->mu64Size - luReadPos;

                        ::memcpy(lpState->mpDataEndPtr, lpReq->mpMappedData,
                                 static_cast<size_t>(lpState->mu64ChunkBytes));
                        lpReq->mpMappedData += lpState->mu64ChunkBytes;
                        liResult = readcallback(lpState);
                    }
                    else
                    {
                        // Async request: issue a granule-sized read at the aligned position.
                        lpState->mu64ChunkBytes = static_cast<u64>(static_cast<s64>(lpState->miReadGranularity));
                        liResult = lpState->mAsyncOp.Read(
                            lpState->mpHandle,
                            lpState->mpDataEndPtr,
                            lpState->mu64ReadPos & ~static_cast<u64>(3),
                            static_cast<u64>(static_cast<s64>(lpState->miReadGranularity)),
                            reinterpret_cast<CompletionCallback>(&Stream::filereadcallback),
                            reinterpret_cast<uintptr_t>(lpState),
                            liPriority);
                    }
                }
                else
                {
                    lpState->miState = 2;
                }

                return liResult;
            }

            // -----------------------------------------------------------------------------
            // Stream::opencallback @0x82BBE490 -- fired when the async Open lands. On success
            // latch the handle + file size and restart streaming; on failure free the request
            // and move on to the next queued one.
            // -----------------------------------------------------------------------------
            s32 Stream::opencallback(AsyncOp* lpOp)
            {
                StreamState* lpState = reinterpret_cast<StreamState*>(lpOp->mUserData);
                Handle* lpHandle = static_cast<Handle*>(lpOp->GetResultHandle());
                lpState->mpHandle = lpHandle;

                if (lpHandle)
                {
                    s32 liPriority = lpState->miUrgentPriority;
                    lpState->muFlags &= ~1u;
                    lpState->mu64FileSize = lpHandle->mu64Size;
                    return restartstream(lpState, liPriority);
                }

                ::EnterCriticalSection(&lpState->mLock);
                s32 liEngineState = lpState->miState;
                lpState->macOpenPath[0] = 0;
                if (liEngineState == 1)
                    lpState->miState = 0;
                freerequest(lpState, lpState->mpReqCurrent);
                ::LeaveCriticalSection(&lpState->mLock);

                s32 liNext = lpState->mbNeedMore ? lpState->miUrgentPriority : lpState->miNormalPriority;
                return startnextrequest(lpState, liNext);
            }

            // -----------------------------------------------------------------------------
            // Stream::closecallback @0x82BC0268 -- fired when the previous file's Close lands.
            // If the next request pre-opened its handle, adopt it and restart; otherwise kick
            // off the Open for the next file.
            // -----------------------------------------------------------------------------
            s32 Stream::closecallback(AsyncOp* lpOp)
            {
                StreamState* lpState = reinterpret_cast<StreamState*>(lpOp->mUserData);
                Request* lpReq = lpState->mpReqCurrent;

                if (!lpReq->mpPreOpenHandle)
                {
                    return lpState->mAsyncOp.Open(
                        lpState->macOpenPath,
                        0,
                        reinterpret_cast<CompletionCallback>(&Stream::opencallback),
                        reinterpret_cast<uintptr_t>(lpState),
                        lpState->miUrgentPriority);
                }

                Handle* lpHandle = lpReq->mpPreOpenHandle;
                s32 liPriority = lpState->miUrgentPriority;
                lpState->muFlags |= 1u;
                lpState->mpHandle = lpHandle;
                lpState->mu64FileSize = lpHandle->mu64Size;
                return restartstream(lpState, liPriority);
            }

            // -----------------------------------------------------------------------------
            // Stream::filereadcallback @0x82BC06C8 -- the read op's completion trampoline:
            // recover the engine from the op and forward to readcallback.
            // -----------------------------------------------------------------------------
            s32 Stream::filereadcallback(AsyncOp* lpOp)
            {
                return readcallback(reinterpret_cast<StreamState*>(lpOp->mUserData));
            }

            // -----------------------------------------------------------------------------
            // Stream::QueueFile @0x82BC04A0 -- grab a free request slot, fill it from the
            // arguments (defaulting the parse callback/context from the engine), queue it, and
            // kick the pipeline if the engine was idle. Returns the new request id (0 if the
            // request pool is exhausted).
            // -----------------------------------------------------------------------------
            s32 Stream::QueueFile(const char* lpcPath, Handle* lpPreOpenHandle, u64 lu64Size,
                                  ChunkParseCallback lpfnParse, void* lpParseContext)
            {
                StreamState* lpState = mpState;

                Request* lpReq = getfreerequest(lpState);
                if (!lpReq)
                    return 0;

                lpReq->muField10 = 0;
                ::strncpy(lpReq->macPath, lpcPath, 255);
                lpReq->mbField113      = 0;
                lpReq->mpPreOpenHandle = lpPreOpenHandle;
                lpReq->mu64Size        = lu64Size;

                if (!lpfnParse)
                    lpfnParse = lpState->mpfnDefaultParse;
                lpReq->mpfnParse = lpfnParse;

                void* lpContext = lpParseContext ? lpParseContext : lpState->mpDefaultParseContext;
                lpReq->mpParseContext = lpContext;

                queuerequest(lpState, lpReq);

                ::EnterCriticalSection(&lpState->mLock);
                s32 liEngineState = lpState->miState;
                if (!liEngineState)
                    lpState->miState = 1;
                ::LeaveCriticalSection(&lpState->mLock);

                if (!liEngineState)
                {
                    s32 liPriority = lpState->mbNeedMore ? lpState->miUrgentPriority
                                                         : lpState->miNormalPriority;
                    startnextrequest(lpState, liPriority);
                }

                return lpReq->miId;
            }

            // -----------------------------------------------------------------------------
            // Stream::GetChunk @0x82BBD878 -- hand out the cursor's current chunk (marking it
            // in use and debiting its bytes), then advance the cursor to the next chunk that
            // matches this cursor's key. Returns null once the cursor has consumed its span.
            // -----------------------------------------------------------------------------
            Chunk* Stream::GetChunk()
            {
                StreamState* lpState = mpState;
                ::EnterCriticalSection(&lpState->mLock);

                Chunk* lpResult;
                if (miRemaining)
                {
                    ChunkNode* lpNode = mpCurrentChunk;
                    lpResult = &lpNode->mChunk;
                    lpNode->mChunk.miState = 1;
                    u32 luSize = lpNode->mChunk.muSize;
                    bool lbMore = (miRemaining - static_cast<s32>(luSize)) > 0;
                    miRemaining -= static_cast<s32>(luSize);

                    lpState->mpRequests[lpNode->mChunk.muRequestId & 0xFFu].muBufferedBytes -= luSize;

                    if (lbMore)
                    {
                        do
                        {
                            do
                                lpNode = lpNode->mpNext;
                            while (lpNode->mChunk.muKey != muKey);
                        }
                        while (lpNode->mChunk.miState);
                    }
                    else
                    {
                        lpNode = nullptr;
                    }
                    mpCurrentChunk = lpNode;
                }
                else
                {
                    lpResult = nullptr;
                }

                ::LeaveCriticalSection(&lpState->mLock);
                return lpResult;
            }

            // -----------------------------------------------------------------------------
            // Stream::ReleaseChunk @0x82BC09C8 -- return a chunk handed out by GetChunk: mark
            // it released, debit its bytes, and, if the engine had parked waiting for room,
            // restart streaming.
            // -----------------------------------------------------------------------------
            s32 Stream::ReleaseChunk(Chunk* lpChunk)
            {
                StreamState* lpState = mpState;
                ::EnterCriticalSection(&lpState->mLock);

                s32 liSize = static_cast<s32>(lpChunk->muSize);
                lpChunk->miState = 2;
                decbufferusage(lpState, liSize);

                s32 liEngineState = lpState->miState;
                if (liEngineState == 2)
                    lpState->miState = 1;
                ::LeaveCriticalSection(&lpState->mLock);
                s32 liResult = 0;

                if (liEngineState == 2)
                {
                    s32 liPriority = lpState->mbNeedMore ? lpState->miUrgentPriority
                                                         : lpState->miNormalPriority;
                    liResult = restartstream(lpState, liPriority);
                }
                return liResult;
            }

            // -----------------------------------------------------------------------------
            // Stream::CancelRequest @0x82BBFE60 -- cancel request luRequestId: if still merely
            // queued, free it; if active, mark it cancelled, release every buffered chunk that
            // belongs to it, and re-home each region's current chunk.
            // -----------------------------------------------------------------------------
            s32 Stream::CancelRequest(u32 luRequestId)
            {
                StreamState* lpState = mpState;
                ::EnterCriticalSection(&lpState->mLock);

                if ((luRequestId & 0xFFu) < lpState->muRequestCount)
                {
                    Request* lpReq = &lpState->mpRequests[luRequestId & 0xFFu];
                    if (static_cast<u32>(lpReq->miId) == luRequestId)
                    {
                        s32 liState = lpReq->miState;
                        if (liState != 0 && liState != 4)
                        {
                            if (liState == 1)
                            {
                                freerequest(lpState, lpReq);
                            }
                            else
                            {
                                bool lbTouched = false;
                                lpReq->miState = 4;
                                for (ChunkNode* lpNode = lpState->mpChunkHead; lpNode; lpNode = lpNode->mpNext)
                                {
                                    if (lpNode->mChunk.muRequestId == static_cast<u32>(lpReq->miId)
                                        && !lpNode->mChunk.miState)
                                    {
                                        u32 luRegion = lpNode->mChunk.muKey;
                                        lbTouched = true;
                                        BufferRegion* lpRegion = lpState->mpRegionHead;
                                        for (; --luRegion; lpRegion = lpRegion->mpNext)
                                            ;
                                        lpRegion->miUsage -= static_cast<s32>(lpNode->mChunk.muSize);
                                        decbufferusage(lpState, static_cast<s32>(lpNode->mChunk.muSize));
                                        lpNode->mChunk.miState = 2;
                                    }
                                }
                                if (lbTouched)
                                {
                                    for (BufferRegion* lpRegion = lpState->mpRegionHead; lpRegion;
                                         lpRegion = lpRegion->mpNext)
                                    {
                                        ChunkNode* lpMatch;
                                        if (lpRegion->miUsage <= 0)
                                        {
                                            lpMatch = nullptr;
                                        }
                                        else
                                        {
                                            if (!lpRegion->mpCurrentChunk->mChunk.miState)
                                                continue;
                                            for (lpMatch = lpState->mpChunkHead;
                                                 lpMatch->mChunk.muKey != lpRegion->muKey || lpMatch->mChunk.miState;
                                                 lpMatch = lpMatch->mpNext)
                                                ;
                                        }
                                        lpRegion->mpCurrentChunk = lpMatch;
                                    }
                                }
                            }
                        }
                    }
                }

                ::LeaveCriticalSection(&lpState->mLock);
                return 0;
            }

            // -----------------------------------------------------------------------------
            // Stream::GetRequestState @0x82BBD9D0 -- return the state of request luRequestId,
            // or 0 if the id is out of range / stale / free.
            // -----------------------------------------------------------------------------
            s32 Stream::GetRequestState(u32 luRequestId) const
            {
                StreamState* lpState = mpState;
                if ((luRequestId & 0xFFu) >= lpState->muRequestCount)
                    return 0;
                Request* lpReq = &lpState->mpRequests[luRequestId & 0xFFu];
                if (static_cast<u32>(lpReq->miId) != luRequestId)
                    return 0;
                s32 liState = lpReq->miState;
                if (!liState)
                    return 0;
                return liState;
            }

            // -----------------------------------------------------------------------------
            // Stream::GetState @0x82BBD990 -- the engine's streaming state word.
            // -----------------------------------------------------------------------------
            s32 Stream::GetState() const
            {
                return mpState->miState;
            }

            // -----------------------------------------------------------------------------
            // Stream::IsEndOfStream @0x82BBD9A0 -- true once the engine is idle (state 0) and
            // this cursor has nothing left to consume.
            // -----------------------------------------------------------------------------
            bool Stream::IsEndOfStream() const
            {
                if (mpState->miState)
                    return false;
                if (miRemaining)
                    return false;
                return true;
            }

            // -----------------------------------------------------------------------------
            // Stream::GetFileSize @0x82BBDA10 -- the open file's byte size.
            // -----------------------------------------------------------------------------
            u64 Stream::GetFileSize() const
            {
                return mpState->mu64FileSize;
            }

            // -----------------------------------------------------------------------------
            // Stream::Kill @0x82BBFFD8 -- tear the stream down: cancel every in-flight request,
            // drain the active list back to the current request (marking it done), zero every
            // region's usage, drop the buffered-byte total, release all chunks, and idle the
            // engine.
            // -----------------------------------------------------------------------------
            s32 Stream::Kill()
            {
                StreamState* lpState = mpState;
                s32 liResult = 0;

                Request* lpReq = lpState->mpReqTail;
                if (lpReq)
                {
                    while (true)
                    {
                        s32 liState = lpReq->miState;
                        if (liState != 1 && liState != 2)
                            break;
                        CancelRequest(static_cast<u32>(lpReq->miId));
                        lpReq = lpState->mpReqTail;
                    }

                    while (lpState->mpReqHead != lpState->mpReqCurrent)
                        freerequest(lpState, lpState->mpReqHead);
                    lpState->mpReqCurrent->miState = 4;

                    for (BufferRegion* lpRegion = lpState->mpRegionHead; lpRegion; lpRegion = lpRegion->mpNext)
                        lpRegion->miUsage = 0;

                    decbufferusage(lpState, lpState->miBufferUsage);

                    ::EnterCriticalSection(&lpState->mLock);
                    for (ChunkNode* lpNode = lpState->mpChunkHead; lpNode; lpNode = lpNode->mpNext)
                        lpNode->mChunk.miState = 2;
                    if (lpState->miState == 2)
                        lpState->miState = 0;
                    ::LeaveCriticalSection(&lpState->mLock);
                    liResult = 0;
                }

                return liResult;
            }
        }
    }
}
