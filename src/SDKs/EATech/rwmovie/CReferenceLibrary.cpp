// ===========================================================================
// SDKs/EATech/rwmovie/CReferenceLibrary.cpp
//
// CReferenceLibrary -- reference-frame pool for the RTCMV/WMV video-object encoder.
// Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is authoritative.
//
//   CReferenceLibrary::AddFrame              0x82A03F28
//   CReferenceLibrary::RemoveFrame           0x82A03ED8
//   CReferenceLibrary::CloseReferenceLibrary 0x82A04068
//   CReferenceLibrary::SetupReferenceLibrary 0x82A04198
//   CReferenceLibrary::~CReferenceLibrary    0x82A04190
//
// The frames live in a CQueue; each queue element is a raw-allocated PictureCYUV420. The X360
// inlines CQueue::AddElement(frame, 0) into AddFrame and CQueue::RemoveElement(&frame, 0) into
// the CloseReferenceLibrary drain loop; both are de-inlined here to the real CQueue methods
// (store-for-store equivalent). XMemAlloc/XMemFree (declared in CQueue.h) are the raw XDK heap
// API, called with no `this` argument and attributes 0x248C8000 -- the same convention as every
// sibling rwmovie TU.
// ===========================================================================

#include "SDKs/EATech/rwmovie/CReferenceLibrary.h"

#include <new>   // placement new for the CQueue and PictureCYUV420 objects

#include "SDKs/EATech/rwmovie/CQueue.h"
#include "SDKs/EATech/rwmovie/PictureCYUV420.h"

namespace
{
    // XMemAlloc/XMemFree attribute word for this codec's queue/frame objects
    // (lis 0x248C / ori 0x8000).
    const u32 KU_REFERENCELIBRARY_XMEM_ATTRIBUTES = 0x248C8000u;
}

// ---------------------------------------------------------------------------
// CReferenceLibrary::AddFrame  @ 0x82A03F28
//
// De-inlined CQueue::AddElement(lpFrame, 0): the X360 open-codes the head insert whose guard
// (frame != null && queue count >= 0) is exactly AddElement's success condition, then bumps the
// frame cursor. Returns 0 on success, -100 when the insert could not be performed.
// ---------------------------------------------------------------------------
s32 CReferenceLibrary::AddFrame(PictureCYUV420* lpFrame)
{
    if (mpQueue->AddElement(lpFrame, 0) == 0)
        return -100;

    ++miCurrentIndex;
    return 0;
}

// ---------------------------------------------------------------------------
// CReferenceLibrary::RemoveFrame  @ 0x82A03ED8
//
// Removes the frame at liIndex (-1 = tail) via CQueue::RemoveElement, adjusting the cursor. The
// out-of-range guard (count != 0 && liIndex <= count-1) is stricter than RemoveElement's own so
// that a -1 (tail) request on an empty queue is rejected here. On that rejection the X360 leaves
// the (unused) queue pointer in the return register; the load-bearing result is the nulled
// out-param, so this path returns 0.
// ---------------------------------------------------------------------------
s32 CReferenceLibrary::RemoveFrame(PictureCYUV420** lppFrame, s32 liIndex)
{
    const s32 liCount = mpQueue->GetCount();
    if (liCount != 0 && liIndex <= liCount - 1)
    {
        if (liIndex >= 0 || miCurrentIndex == liCount)
            --miCurrentIndex;

        // The X360 tail-calls RemoveElement with the caller's out pointer directly; the queue
        // stores elements as void*, so the frame-pointer slot is passed through untouched.
        return mpQueue->RemoveElement(reinterpret_cast<void**>(lppFrame), liIndex);
    }

    *lppFrame = nullptr;
    return 0;
}

// ---------------------------------------------------------------------------
// CReferenceLibrary::CloseReferenceLibrary  @ 0x82A04068
//
// Drains the queue -- for every active frame: pop it from the front (inlined
// CQueue::RemoveElement(&frame, 0)), Clean() it, and free it -- then destroys and frees the
// queue itself and nulls the pointer. Reproduces the two-phase teardown of the asm exactly.
// ---------------------------------------------------------------------------
s32 CReferenceLibrary::CloseReferenceLibrary()
{
    if (mpQueue != nullptr)
    {
        if (mpQueue->GetCount() > 0)
        {
            void* lpElement = nullptr;
            mpQueue->RemoveElement(&lpElement, 0);
            while (lpElement != nullptr)
            {
                PictureCYUV420* lpFrame = static_cast<PictureCYUV420*>(lpElement);
                PictureCYUV420_Clean(lpFrame);
                XMemFree(lpFrame, KU_REFERENCELIBRARY_XMEM_ATTRIBUTES);

                if (mpQueue->GetCount() <= 0)
                    break;

                mpQueue->RemoveElement(&lpElement, 0);
            }
        }

        if (mpQueue != nullptr)
        {
            mpQueue->DestroyQueue();
            XMemFree(mpQueue, KU_REFERENCELIBRARY_XMEM_ATTRIBUTES);
            mpQueue = nullptr;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// CReferenceLibrary::SetupReferenceLibrary  @ 0x82A04198
//
// Builds the frame pool: allocate + construct the CQueue (liNumFrames capacity), then allocate,
// default-construct and PictureCYUV420_init liNumFrames frames, enqueuing each at the front. All
// error codes flow through *lpiResult; any failure tears the library back down through
// CloseReferenceLibrary. The parameters after liNumFrames are forwarded verbatim to the frame
// init (liNumFrames itself is only the queue capacity / loop count and is not passed on).
// ---------------------------------------------------------------------------
s32 CReferenceLibrary::SetupReferenceLibrary(s32* lpiResult, s32 liParam2, s32 liParam3,
                                             s32 liNumFrames, s32 liParam5, s32 liParam6,
                                             s32 liParam7)
{
    miCurrentIndex = 0;
    miReserved08   = 0;

    // Allocate + construct the frame queue. The queue ctor writes its own status through the
    // shared out-param (lpiResult).
    void* lpQueueMem = XMemAlloc(sizeof(CQueue), KU_REFERENCELIBRARY_XMEM_ATTRIBUTES);
    mpQueue = lpQueueMem ? new (lpQueueMem) CQueue(lpiResult, liNumFrames) : nullptr;
    if (mpQueue == nullptr)
    {
        *lpiResult = -3;
        return 0;
    }

    // The queue ctor flagged a failure (e.g. node-pool allocation): tear down and bail.
    if (*lpiResult != 0)
        return CloseReferenceLibrary();

    for (s32 liFrame = 0; liFrame < liNumFrames; ++liFrame)
    {
        void* lpFrameMem = XMemAlloc(sizeof(PictureCYUV420), KU_REFERENCELIBRARY_XMEM_ATTRIBUTES);
        if (lpFrameMem == nullptr)
        {
            *lpiResult = -3;
            return CloseReferenceLibrary();
        }

        PictureCYUV420* lpFrame = new (lpFrameMem) PictureCYUV420();
        PictureCYUV420_init(lpFrame, lpiResult, liParam2, liParam3, liParam5, liParam6, liParam7);
        if (*lpiResult != 0)
        {
            // This frame's init failed: release the orphan, then tear down the rest.
            PictureCYUV420_Clean(lpFrame);
            XMemFree(lpFrame, KU_REFERENCELIBRARY_XMEM_ATTRIBUTES);
            return CloseReferenceLibrary();
        }

        mpQueue->AddElement(lpFrame, 0);
    }

    *lpiResult = 0;
    return 1;
}

// ---------------------------------------------------------------------------
// CReferenceLibrary::~CReferenceLibrary  @ 0x82A04190 -- non-virtual; tail-calls Close.
// ---------------------------------------------------------------------------
CReferenceLibrary::~CReferenceLibrary()
{
    CloseReferenceLibrary();
}
