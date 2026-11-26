// XDK size optimization: ARTIST compiled the RenderWare core filesys library for size
// (record-form cr0 null tests, e.g. AsyncOp::DoClose 0x82BBE028 `cmplwi r3,0`;
// Stream::readcallback 0x82BC0598 and Handle::~Handle 0x82BBD6A0 match only so).
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
// =====================================================================================
// rw::core::filesys::Stream -- the RenderWare core filesystem streaming reader.
//
// ARTIST establishes the native storage and dispatch. The engine (DecFIGS stream.h:50)
// uses the original EA::Thread::Mutex and RenderWare intrusive lists; the public API runs
// on a tap handle (stream.h).
// =====================================================================================

#include "SDKs/EATech/rwcore/filesys/stream.h"

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
            Request* Stream::getfreerequest()
            {
                mLock.Lock();

                Request* lpReq = mpFreeRequest;
                if (lpReq)
                {
                    mpFreeRequest = lpReq->mpNext;

                    guRequestGeneration += 0x100u;
                    if (guRequestGeneration == 0)
                        guRequestGeneration = 0x100u;

                    lpReq->miId = static_cast<s32>((static_cast<u32>(lpReq->miId) & 0xFFu) | guRequestGeneration);
                }

                mLock.Unlock();
                return lpReq;
            }

            // -----------------------------------------------------------------------------
            // Stream::freerequest @0x82BBD7F8 -- unlink lpReq from the active list and push it
            // onto the free list. DecFIGS stream.h:229 declares the void helper.
            // -----------------------------------------------------------------------------
            void Stream::freerequest(Request* lpReq)
            {
                if (lpReq == mpReqHead)
                    mpReqHead = lpReq->mpNext;
                else
                    lpReq->mpPrev->mpNext = lpReq->mpNext;

                if (lpReq == mpReqTail)
                    mpReqTail = lpReq->mpPrev;
                else
                    lpReq->mpNext->mpPrev = lpReq->mpPrev;

                if (lpReq == mpReqCurrent)
                {
                    Request* lpNext = lpReq->mpNext;
                    if (!lpNext)
                        lpNext = lpReq->mpPrev;
                    mpReqCurrent = lpNext;
                }

                lpReq->miState = 0;
                lpReq->mpNext = mpFreeRequest;
                mpFreeRequest = lpReq;
            }

            // -----------------------------------------------------------------------------
            // Stream::queuerequest @0x82BBD780 -- append lpReq to the tail of the active list.
            // -----------------------------------------------------------------------------
            void Stream::queuerequest(Request* lpReq)
            {
                lpReq->miState = 1;
                lpReq->mpNext  = 0;

                mLock.Lock();

                // Each arm stores the tail itself (PS3 DecFIGS 0xE4691C); ARTIST lays the
                // empty-list arm first (0x82BBD7C4).
                if (mpReqTail == 0)
                {
                    lpReq->mpPrev = 0;
                    mpReqTail    = lpReq;
                    mpReqHead    = lpReq;
                    mpReqCurrent = lpReq;
                }
                else
                {
                    lpReq->mpPrev = mpReqTail;
                    mpReqTail->mpNext = lpReq;
                    mpReqTail = lpReq;
                }

                mLock.Unlock();
            }

            // -----------------------------------------------------------------------------
            // Stream::decbufferusage @0x82BBFDC0 -- drop liBytes from the buffered-byte total.
            // If that pushes usage below the low-water mark while a read op is in flight, flag
            // "need more" and bump the outstanding read op to the urgent priority (redirecting
            // to the default device when the op's device runs on an external thread).
            // -----------------------------------------------------------------------------
            void Stream::decbufferusage(s32 liBytes)
            {
                mLock.Lock();
                s32 liOldUsage = miBufferUsage;
                miBufferUsage = liOldUsage - liBytes;
                mLock.Unlock();

                s32 liThreshold = miThreshold;
                if (liOldUsage >= liThreshold && (liOldUsage - liBytes) < liThreshold)
                {
                    mbNeedMore = true;
                    if (miState == 1)
                    {
                        AsyncOp* lpOp = &mAsyncOp;
                        const s32 liPriority = miUrgentPriority;
                        if (lpOp->miPriority != liPriority)
                        {
                            Device* lpDevice = lpOp->mpDevice;
                            if (lpDevice->IsExternalThread())
                                lpDevice = gpFileSysManager->mpDefaultDevice;
                            lpDevice->ChangeOpPriority(lpOp, liPriority);
                        }
                    }
                }
            }

            // -----------------------------------------------------------------------------
            // Stream::parsechunks @0x82BBF198 -- carve the freshly buffered bytes into chunks
            // by offering them to the current request's parse callback (once per registered
            // handler). Each recognised chunk is recorded, charged to its region, and the
            // parse cursor advanced. Returns 0 (more to parse), 1 (buffer drained, no chunk),
            // or 2 (end-of-stream flush).
            // -----------------------------------------------------------------------------
            s32 Stream::parsechunks()
            {
                s32 liRegion = 1;
                s32 liResult = (mpParsePtr >= mpDataEndPtr) ? 1 : 0;
                Request* lpReq = mpReqCurrent;

                if (mpParsePtr < mpDataEndPtr)
                {
                    while (lpReq->miState != 4)
                    {
                        u32 luConsumed = 0;
                        s32 liParse = 0;
                        for (ChunkHandler* lpH = mFilters.mpHead; lpH; lpH = lpH->mpNext)
                        {
                            liParse = lpReq->mpfnParse(
                                mpParsePtr,
                                static_cast<u32>(mpDataEndPtr - mpParsePtr),
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
                            if (!mpReqCurrent->mbAtEnd)
                            {
                                liResult = 1;
                                break;
                            }
                            liRegion = 1;
                            liParse  = 2;
                            luConsumed = static_cast<u32>(mpDataEndPtr - mpParsePtr);
                            if (!luConsumed)
                                return 2;
                        }

                        mLock.Lock();
                        if (lpReq->miState == 4)
                        {
                            mLock.Unlock();
                            liResult = 1;
                            break;
                        }

                        // ARTIST 0x82BBF2A4 allocates the native 24-byte chunk node.
                        ChunkNode* lpNode = static_cast<ChunkNode*>(Manager::Allocate(sizeof(ChunkNode)));
                        if (lpNode)
                        {
                            lpNode->mpNext          = 0;
                            lpNode->mChunk.miState  = 0;
                            lpNode->mChunk.miKey    = liRegion;
                            lpNode->mChunk.mpData   = mpParsePtr;
                            lpNode->mChunk.miRequestId = lpReq->miId;
                            lpNode->mChunk.muSize   = luConsumed;
                        }

                        // ARTIST 0x82BBF2F4 appends through the chunk list's links and count.
                        mChunks.Append(lpNode);

                        // Charge the chunk to its region and the owning request.
                        TapStruct* lpRegion = mTaps.mpHead;
                        for (s32 li = liRegion - 1; li; --li)
                            lpRegion = lpRegion->mpNext;
                        lpRegion->miUsage += static_cast<s32>(luConsumed);
                        lpReq->muBufferedBytes += luConsumed;
                        if (lpRegion->miUsage == static_cast<s32>(luConsumed))
                            lpRegion->mpCurrentChunk = lpNode;

                        // Advance the parse cursor and buffered-byte total; clear "need more"
                        // once usage climbs back above the low-water mark.
                        s32 liOldUsage = miBufferUsage;
                        mpParsePtr += luConsumed;
                        s32 liThreshold = miThreshold;
                        s32 liNewUsage = liOldUsage + static_cast<s32>(luConsumed);
                        miBufferUsage = liNewUsage;
                        if (liOldUsage < liThreshold && liNewUsage >= liThreshold)
                            mbNeedMore = 0;

                        lpNode->mChunk.miState = 0;
                        mLock.Unlock();

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
            void Stream::readcallback()
            {
                Request* lpReq = mpReqCurrent;

                s64 li64Transferred;
                s64 li64AlignAdjust = 0;
                if (lpReq->miRequestType == 1)
                {
                    li64Transferred = mi64ChunkBytes;
                    lpReq->mbAtEnd = mi64ReadPos + li64Transferred >= lpReq->mi64Size;
                }
                else
                {
                    li64Transferred = static_cast<s32>(mAsyncOp.GetResultSize());
                    lpReq->mbAtEnd = li64Transferred < mi64ChunkBytes;
                    li64AlignAdjust = mi64ReadPos & 3;
                }

                mpDataEndPtr += static_cast<s32>(li64Transferred);
                mi64ReadPos = (mi64ReadPos - li64AlignAdjust) + li64Transferred;
                mpParsePtr += static_cast<s32>(li64AlignAdjust);

                s32 liResult = parsechunks();

                const s32 liState = lpReq->miState;
                if (liState != 4)
                {
                    if (liResult != 2 && !lpReq->mbAtEnd)
                    {
                        if (liResult == 1)
                            restartstream(miUrgentPriority - 1);
                        return;
                    }

                    mLock.Lock();
                    if (lpReq->miState != 4)
                        lpReq->miState = 3;
                    mLock.Unlock();
                }
                startnextrequest(miUrgentPriority);
            }

            // -----------------------------------------------------------------------------
            // Stream::startnextrequest @0x82BC02C0 -- select the next request, align the
            // parse buffer, and either reuse a handle or queue its close/open operation.
            // ARTIST 0x82BC037C restarts memory-backed requests before file-path dispatch.
            // -----------------------------------------------------------------------------
            void Stream::startnextrequest(s32 liPriority)
            {
                mLock.Lock();

                Request* lpReq = mpReqCurrent;
                bool lbIdle = false;
                if (!lpReq || (lpReq->miState != 1 && !lpReq->mpNext))
                {
                    lbIdle = true;
                    miState = 0;
                }
                else
                {
                    if (lpReq->miState != 1)
                        mpReqCurrent = lpReq->mpNext;
                    lpReq = mpReqCurrent;
                    const uintptr_t luAligned =
                        (reinterpret_cast<uintptr_t>(mpParsePtr) + 15)
                        & ~static_cast<uintptr_t>(15);
                    mpParsePtr = reinterpret_cast<u8*>(luAligned);
                    lpReq->mpBufferPos = mpParsePtr;
                    lpReq->miState = 2;
                    lpReq->mbAtEnd = false;
                }
                mLock.Unlock();

                if (lbIdle)
                    return;

                mpDataEndPtr = mpParsePtr;
                if (lpReq->miRequestType == 1)
                {
                    mi64ReadPos = 0;
                    mu64FileSize = lpReq->mi64Size;
                    restartstream(liPriority);
                    return;
                }

                mi64ReadPos = lpReq->mi64Size;
                if (lpReq->mpPreOpenHandle)
                {
                    macOpenPath[0] = 0;
                    if (!mpHandle || (muFlags & 1u))
                    {
                        muFlags |= 1u;
                        mpHandle = lpReq->mpPreOpenHandle;
                        mu64FileSize = mpHandle->mu64Size;
                        restartstream(liPriority);
                        return;
                    }
                }
                else
                {
                    if (::strcmp(lpReq->macPath, macOpenPath) == 0)
                    {
                        restartstream(liPriority);
                        return;
                    }

                    ::strcpy(macOpenPath, lpReq->macPath);
                    if (!mpHandle || (muFlags & 1u))
                    {
                        mAsyncOp.Open(macOpenPath, 0, &Stream::opencallback, this, liPriority);
                        return;
                    }
                }

                mAsyncOp.Close(mpHandle, &Stream::closecallback, this, liPriority);
            }

            // -----------------------------------------------------------------------------
            // Stream::restartstream @0x82BC06D0 -- reclaim released chunks, evict requests
            // whose data has already streamed past, compact the ring buffer if that frees
            // enough room, then issue the next read (or copy the next slice from a mapped
            // request). If there is not a granule's worth of room, park the engine (state 2).
            // -----------------------------------------------------------------------------
            void Stream::restartstream(s32 liPriority)
            {
                mLock.Lock();

                // Phase 1: free chunks that have been released (state >= 2), advancing the
                // fill pointer as the buffer front empties.
                ChunkNode* lpChunk = mChunks.mpHead;
                if (lpChunk)
                {
                    while (lpChunk->mChunk.miState >= 2)
                    {
                        ChunkNode* lpHead = mChunks.mpHead;
                        if (lpHead)
                        {
                            if (lpHead == mChunks.mpTail)
                            {
                                mChunks.mpHead = 0;
                                mChunks.mpTail = 0;
                            }
                            else
                            {
                                mChunks.mpHead = lpHead->mpNext;
                            }
                            mChunks.muCount--;
                            lpHead->mpNext = 0;
                        }
                        if (!mChunks.mpHead)
                        {
                            uintptr_t luAligned =
                                reinterpret_cast<uintptr_t>(lpChunk->mChunk.mpData + lpChunk->mChunk.muSize)
                                & ~static_cast<uintptr_t>(0xF);
                            mpFillPtr = reinterpret_cast<u8*>(luAligned);
                        }
                        lpChunk->mpNext = 0;
                        Manager::Free(lpChunk);
                        lpChunk = mChunks.mpHead;
                        if (!lpChunk)
                            break;
                    }
                    if (lpChunk)
                        mpFillPtr = lpChunk->mChunk.mpData;
                }

                // Phase 2: evict head requests whose target buffer position no longer lies in
                // the live [fill, end) window (handling ring wrap).
                while (true)
                {
                    Request* lpNextReq = mpReqHead->mpNext;
                    if (!lpNextReq || lpNextReq->miState == 1)
                        break;

                    uintptr_t luFill = reinterpret_cast<uintptr_t>(mpFillPtr);
                    uintptr_t luEnd  = reinterpret_cast<uintptr_t>(mpDataEndPtr);
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
                    freerequest(mpReqHead);
                }

                mLock.Unlock();

                // Phase 3: work out how much contiguous room is free, compacting the buffer if
                // that recovers a granule.
                u8* lpFill = mpFillPtr;
                u8* lpEnd  = mpDataEndPtr;
                s32 liSpace;
                if (lpFill > lpEnd)
                {
                    liSpace = static_cast<s32>(lpFill - lpEnd) - 17;
                }
                else
                {
                    liSpace = static_cast<s32>(mpBufferTop - lpEnd) - 16;
                    if (liSpace < miReadGranularity)
                    {
                        s32 liKeep = static_cast<s32>(lpEnd - mpParsePtr);
                        if ((static_cast<s32>(lpFill - mpWritePtr) - 17) >= liKeep)
                        {
                            u8* lpDst;
                            s32 liRem = liKeep % 16;
                            if (liRem == 0 || mpReqCurrent->miRequestType == 1)
                                lpDst = mpBufferBase;
                            else
                                lpDst = mpBufferBase - liRem + 16;

                            mpWritePtr = lpDst;
                            ::memcpy(lpDst, mpParsePtr, static_cast<size_t>(liKeep));
                            u8* lpNewEnd = lpDst + liKeep;
                            mpParsePtr   = lpDst;
                            mpDataEndPtr = lpNewEnd;
                            liSpace = static_cast<s32>(lpFill - lpNewEnd) - 17;
                        }
                    }
                }

                if (liSpace >= miReadGranularity)
                {
                    Request* lpReq = mpReqCurrent;
                    if (lpReq->miRequestType == 1)
                    {
                        // Mapped request: copy the next slice straight from memory.
                        const s64 li64ReadPos = mi64ReadPos;
                        if (li64ReadPos + liSpace > lpReq->mi64Size)
                            mi64ChunkBytes = lpReq->mi64Size - li64ReadPos;
                        else
                            mi64ChunkBytes = liSpace;

                        ::memcpy(mpDataEndPtr, lpReq->mpMappedData,
                                 static_cast<size_t>(mi64ChunkBytes));
                        lpReq->mpMappedData += static_cast<s32>(mi64ChunkBytes);
                        readcallback();
                    }
                    else
                    {
                        // Async request: issue a granule-sized read at the aligned position.
                        mi64ChunkBytes = miReadGranularity;
                        mAsyncOp.Read(
                            mpHandle,
                            mpDataEndPtr,
                            mi64ReadPos & ~static_cast<s64>(3),
                            mi64ChunkBytes,
                            &Stream::filereadcallback,
                            this,
                            liPriority);
                    }
                }
                else
                {
                    miState = 2;
                }

            }

            // -----------------------------------------------------------------------------
            // Stream::opencallback @0x82BBE490 -- fired when the async Open lands. On success
            // latch the handle + file size and restart streaming; on failure free the request
            // and move on to the next queued one.
            // -----------------------------------------------------------------------------
            void Stream::opencallback(AsyncOp* lpOp)
            {
                Stream* lpState = static_cast<Stream*>(lpOp->GetContext());
                Handle* lpHandle = lpOp->GetResultHandle();
                lpState->mpHandle = lpHandle;
                if (!lpHandle)
                {
                    lpState->mLock.Lock();
                    lpState->macOpenPath[0] = 0;
                    if (lpState->miState == 1)
                        lpState->miState = 0;
                    lpState->freerequest(lpState->mpReqCurrent);
                    lpState->mLock.Unlock();

                    // Two calls, not one call with a ternary argument: ARTIST sets r3 to the engine before
                    // the greedy-state test and both arms share one tail call (cross-jumped), e.g.
                    // ReleaseChunk 0x82BC0A2C, QueueFile 0x82BC056C, opencallback 0x82BBE4FC.
                    if (lpState->mbNeedMore)
                        lpState->startnextrequest(lpState->miUrgentPriority);
                    else
                        lpState->startnextrequest(lpState->miNormalPriority);
                }
                else
                {
                    const s32 liPriority = lpState->miUrgentPriority;
                    lpState->muFlags &= ~1u;
                    lpState->mu64FileSize = lpHandle->mu64Size;
                    lpState->restartstream(liPriority);
                }
            }

            // -----------------------------------------------------------------------------
            // Stream::closecallback @0x82BC0268 -- fired when the previous file's Close lands.
            // If the next request pre-opened its handle, adopt it and restart; otherwise kick
            // off the Open for the next file.
            // -----------------------------------------------------------------------------
            void Stream::closecallback(AsyncOp* lpOp)
            {
                Stream* lpState = static_cast<Stream*>(lpOp->GetContext());
                Request* lpReq = lpState->mpReqCurrent;

                if (!lpReq->mpPreOpenHandle)
                {
                    lpState->mAsyncOp.Open(
                        lpState->macOpenPath,
                        0,
                        &Stream::opencallback,
                        lpState,
                        lpState->miUrgentPriority);
                    return;
                }

                s32 liPriority = lpState->miUrgentPriority;
                lpState->muFlags |= 1u;
                lpState->mpHandle = lpReq->mpPreOpenHandle;
                lpState->mu64FileSize = lpState->mpHandle->mu64Size;
                lpState->restartstream(liPriority);
            }

            // -----------------------------------------------------------------------------
            // Stream::filereadcallback @0x82BC06C8 -- the read op's completion trampoline:
            // recover the engine from the op and forward to readcallback.
            // -----------------------------------------------------------------------------
            void Stream::filereadcallback(AsyncOp* lpOp)
            {
                static_cast<Stream*>(lpOp->GetContext())->readcallback();
            }

            // -----------------------------------------------------------------------------
            // Stream::QueueFile @0x82BC04A0 -- grab a free request slot, fill it from the
            // arguments (defaulting the parse callback/context from the engine), queue it, and
            // kick the pipeline if the engine was idle. Returns the new request id (0 if the
            // request pool is exhausted).
            // -----------------------------------------------------------------------------
            s32 Stream::QueueFile(const char* lpcPath, Handle* lpPreOpenHandle, s64 li64Size,
                                  ChunkParseCallback lpfnParse, void* lpParseContext)
            {
                Stream* lpState = GetStreamHeaderFromHandle(this);

                Request* lpReq = lpState->getfreerequest();
                if (!lpReq)
                    return 0;

                lpReq->miRequestType = 0;
                ::strncpy(lpReq->macPath, lpcPath, 255);
                lpReq->mbField113      = 0;
                lpReq->mpPreOpenHandle = lpPreOpenHandle;
                lpReq->mi64Size        = li64Size;

                if (!lpfnParse)
                    lpfnParse = lpState->mpfnDefaultParse;
                lpReq->mpfnParse = lpfnParse;

                void* lpContext = lpParseContext ? lpParseContext : lpState->mpDefaultParseContext;
                lpReq->mpParseContext = lpContext;

                lpState->queuerequest(lpReq);

                lpState->mLock.Lock();
                s32 liEngineState = lpState->miState;
                if (!liEngineState)
                    lpState->miState = 1;
                lpState->mLock.Unlock();

                if (!liEngineState)
                {
                    // Two calls, not one call with a ternary argument: ARTIST sets r3 to the engine before
                    // the greedy-state test and both arms share one tail call (cross-jumped), e.g.
                    // ReleaseChunk 0x82BC0A2C, QueueFile 0x82BC056C, opencallback 0x82BBE4FC.
                    if (lpState->mbNeedMore)
                        lpState->startnextrequest(lpState->miUrgentPriority);
                    else
                        lpState->startnextrequest(lpState->miNormalPriority);
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
                Stream* lpState = GetStreamHeaderFromHandle(this);
                TapStruct* lpTap = reinterpret_cast<TapStruct*>(this);
                lpState->mLock.Lock();

                if (lpTap->miUsage == 0)
                {
                    lpState->mLock.Unlock();
                    return 0;
                }

                ChunkNode* lpNode = lpTap->mpCurrentChunk;
                Chunk* lpResult = &lpNode->mChunk;
                lpNode->mChunk.miState = 1;
                const u32 luSize = lpNode->mChunk.muSize;
                // The branch below tests the stored difference (ARTIST 0x82BBD8DC subf. feeds
                // the ble at 0x82BBD904 across the request-table store).
                const s32 liRemaining = lpTap->miUsage - luSize;
                lpTap->miUsage = liRemaining;
                lpState->mpRequests[lpNode->mChunk.miRequestId & 0xFF].muBufferedBytes -= luSize;
                if (liRemaining > 0)
                {
                    do
                        lpNode = lpNode->mpNext;
                    while (lpNode->mChunk.miKey != lpTap->miKey || lpNode->mChunk.miState);
                }
                else
                {
                    lpNode = 0;
                }
                lpTap->mpCurrentChunk = lpNode;

                lpState->mLock.Unlock();
                return lpResult;
            }

            // -----------------------------------------------------------------------------
            // Stream::locaterequest (DWARF stream.h:228) -- the live request with id
            // liRequestId, or null when the slot is out of range, reused or free. No
            // out-of-line copy survives in ARTIST: it is inlined into its callers.
            // -----------------------------------------------------------------------------
            Request* Stream::locaterequest(s32 liRequestId)
            {
                const s32 liSlot = liRequestId & 0xFF;
                if (liSlot < miRequestCount)
                {
                    Request* lpReq = &mpRequests[liSlot];
                    if (liRequestId == lpReq->miId && lpReq->miState != 0)
                        return lpReq;
                }
                return 0;
            }

            // -----------------------------------------------------------------------------
            // Stream::Gettable @0x82BBD940 / @0x82BBD948 (DWARF stream.h:218/221; unnamed in
            // ARTIST, whose 0x82BBD940 body is ICF-named AptNativeHash::GetPrototype):
            // the bytes buffered for this tap, and for one live request. SndPlayer1::EventEvent
            // calls both (0x82BA5E24 / 0x82BA5E3C).
            // -----------------------------------------------------------------------------
            s32 Stream::Gettable() const
            {
                return reinterpret_cast<const TapStruct*>(this)->miUsage;
            }

            s32 Stream::Gettable(s32 liRequestId) const
            {
                Stream* lpState = GetStreamHeaderFromHandle(this);
                const s32 liSlot = liRequestId & 0xFF;
                if (liSlot < lpState->miRequestCount)
                {
                    Request* lpReq = &lpState->mpRequests[liSlot];
                    if (liRequestId == lpReq->miId && lpReq->miState != 0)
                        return static_cast<s32>(lpReq->muBufferedBytes);
                }
                return 0;
            }

            // -----------------------------------------------------------------------------
            // Stream::ReleaseChunk @0x82BC09C8 -- return a chunk handed out by GetChunk: mark
            // it released, debit its bytes, and, if the engine had parked waiting for room,
            // restart streaming.
            // -----------------------------------------------------------------------------
            void Stream::ReleaseChunk(Chunk* lpChunk)
            {
                Stream* lpState = GetStreamHeaderFromHandle(this);
                lpState->mLock.Lock();

                s32 liSize = static_cast<s32>(lpChunk->muSize);
                lpChunk->miState = 2;
                lpState->decbufferusage(liSize);

                s32 liEngineState = lpState->miState;
                if (liEngineState == 2)
                    lpState->miState = 1;
                lpState->mLock.Unlock();

                if (liEngineState == 2)
                {
                    // Two calls, not one call with a ternary argument: ARTIST sets r3 to the engine before
                    // the greedy-state test and both arms share one tail call (cross-jumped), e.g.
                    // ReleaseChunk 0x82BC0A2C, QueueFile 0x82BC056C, opencallback 0x82BBE4FC.
                    if (lpState->mbNeedMore)
                        lpState->restartstream(lpState->miUrgentPriority);
                    else
                        lpState->restartstream(lpState->miNormalPriority);
                }
            }

            // -----------------------------------------------------------------------------
            // Stream::CancelRequest @0x82BBFE60 -- cancel request luRequestId: if still merely
            // queued, free it; if active, mark it cancelled, release every buffered chunk that
            // belongs to it, and re-home each region's current chunk.
            // -----------------------------------------------------------------------------
            void Stream::CancelRequest(s32 liRequestId)
            {
                Stream* lpState = GetStreamHeaderFromHandle(this);
                lpState->mLock.Lock();

                if ((liRequestId & 0xFF) < lpState->miRequestCount)
                {
                    Request* lpReq = &lpState->mpRequests[liRequestId & 0xFF];
                    if (liRequestId == lpReq->miId)
                    {
                        s32 liState = lpReq->miState;
                        if (liState != 0 && liState != 4)
                        {
                    if (liState == 1)
                    {
                        lpState->freerequest(lpReq);
                    }
                    else
                    {
                        bool lbTouched = false;
                        lpReq->miState = 4;
                        for (ChunkNode* lpNode = lpState->mChunks.mpHead; lpNode; lpNode = lpNode->mpNext)
                        {
                            if (lpNode->mChunk.miRequestId == lpReq->miId && !lpNode->mChunk.miState)
                            {
                                s32 liTap = lpNode->mChunk.miKey;
                                lbTouched = true;
                                TapStruct* lpTap = lpState->mTaps.mpHead;
                                for (; --liTap; lpTap = lpTap->mpNext)
                                    ;
                                lpTap->miUsage -= static_cast<s32>(lpNode->mChunk.muSize);
                                lpState->decbufferusage(static_cast<s32>(lpNode->mChunk.muSize));
                                lpNode->mChunk.miState = 2;
                            }
                        }
                        if (lbTouched)
                        {
                            for (TapStruct* lpTap = lpState->mTaps.mpHead; lpTap; lpTap = lpTap->mpNext)
                            {
                                if (lpTap->miUsage > 0)
                                {
                                    if (lpTap->mpCurrentChunk->mChunk.miState)
                                    {
                                        ChunkNode* lpMatch = lpState->mChunks.mpHead;
                                        while (lpMatch->mChunk.miKey != lpTap->miKey || lpMatch->mChunk.miState)
                                            lpMatch = lpMatch->mpNext;
                                        lpTap->mpCurrentChunk = lpMatch;
                                    }
                                }
                                else
                                {
                                    lpTap->mpCurrentChunk = 0;
                                }
                            }
                        }
                    }
                        }
                    }
                }

                lpState->mLock.Unlock();
            }

            // -----------------------------------------------------------------------------
            // Stream::GetRequestState @0x82BBD9D0 -- return the state of request luRequestId,
            // or 0 if the id is out of range / stale / free.
            // -----------------------------------------------------------------------------
            s32 Stream::GetRequestState(s32 liRequestId) const
            {
                Stream* lpState = GetStreamHeaderFromHandle(this);
                const s32 liSlot = liRequestId & 0xFF;
                if (liSlot < lpState->miRequestCount)
                {
                    Request* lpReq = &lpState->mpRequests[liSlot];
                    if (liRequestId == lpReq->miId && lpReq->miState != 0)
                        return lpReq->miState;
                }
                return 0;
            }

            // -----------------------------------------------------------------------------
            // Stream::GetState @0x82BBD990 -- the engine's streaming state word.
            // -----------------------------------------------------------------------------
            s32 Stream::GetState() const
            {
                return GetStreamHeaderFromHandle(this)->miState;
            }

            // -----------------------------------------------------------------------------
            // Stream::IsEndOfStream @0x82BBD9A0 -- true once the engine is idle (state 0) and
            // this cursor has nothing left to consume.
            // -----------------------------------------------------------------------------
            bool Stream::IsEndOfStream() const
            {
                return GetStreamHeaderFromHandle(this)->miState == 0
                    && reinterpret_cast<const TapStruct*>(this)->miUsage == 0;
            }

            // -----------------------------------------------------------------------------
            // Stream::GetFileSize @0x82BBDA10 -- the open file's byte size.
            // -----------------------------------------------------------------------------
            u64 Stream::GetFileSize() const
            {
                return GetStreamHeaderFromHandle(this)->mu64FileSize;
            }

            // -----------------------------------------------------------------------------
            // Stream::Kill @0x82BBFFD8 -- tear the stream down: cancel every in-flight request,
            // drain the active list back to the current request (marking it done), zero every
            // region's usage, drop the buffered-byte total, release all chunks, and idle the
            // engine.
            // -----------------------------------------------------------------------------
            void Stream::Kill()
            {
                Stream* lpState = GetStreamHeaderFromHandle(this);

                Request* lpReq = lpState->mpReqTail;
                if (lpReq)
                {
                    while (true)
                    {
                        s32 liState = lpReq->miState;
                        if (liState != 1 && liState != 2)
                            break;
                        CancelRequest(lpReq->miId);
                        lpReq = lpState->mpReqTail;
                    }

                    while (lpState->mpReqHead != lpState->mpReqCurrent)
                        lpState->freerequest(lpState->mpReqHead);
                    lpState->mpReqCurrent->miState = 4;

                    for (TapStruct* lpRegion = lpState->mTaps.mpHead; lpRegion; lpRegion = lpRegion->mpNext)
                        lpRegion->miUsage = 0;

                    lpState->decbufferusage(lpState->miBufferUsage);

                    lpState->mLock.Lock();
                    for (ChunkNode* lpNode = lpState->mChunks.mpHead; lpNode; lpNode = lpNode->mpNext)
                        lpNode->mChunk.miState = 2;
                    if (lpState->miState == 2)
                        lpState->miState = 0;
                    lpState->mLock.Unlock();
                }
            }
        }
    }
}
