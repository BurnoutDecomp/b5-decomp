// =====================================================================================
// rw::audio::core::StreamPool -- bodies.
//
// See StreamPool.h for the sources and, more importantly, for WHY this pool's registry
// is empty on the real console and why that makes this home faithful rather than a stub.
// Decode: progress/scratch_dossiers/streampool_decode_codex.md (the dead-registry proof)
//     and progress/scratch_dossiers/streampool_acquire_release_codex.md (these bodies).
// =====================================================================================

#include "rw/audio/core/StreamPool.h"
#include "rw/audio/core/PlugIn.h"   // System (mfSystemTime)
#include "SDKs/EATech/rwcore/filesys/stream.h" // rw::core::filesys::Stream::Kill

#include <cstddef> // offsetof

namespace rw
{
namespace audio
{
namespace core
{

namespace
{
    // The registry list head. ARTIST reads it at `addis r11,r0,0x8327` /
    // `lwz r11,0x1C7C(r11)` (@0x82B6BA68..0x82B6BA6C).
    //
    // ⚠️ It is .bss on the console and NO INSTRUCTION IN THE IMAGE EVER WRITES IT, so it
    // is zero for the life of the process. Reproduced here as a genuinely empty list --
    // a zero-initialised head with no writer -- rather than as `GetInstance { return 0; }`,
    // so that the lookup fails for the console's own reason and stays correct if a future
    // slice ever does find the creation path. See the header banner.
    ListDNode *gpStreamPoolListHead = 0;

    // 100.0f @0x8214AED0 -- the eviction ceiling. A candidate whose priority is not
    // ordered-below BOTH the incoming priority and this constant is never replaced.
    const f32 KF_MAX_REPLACEABLE_PRIORITY = 100.0f;

    // DBL_MAX @0x8214B270 (7F EF FF FF FF FF FF FF) -- the initial tie-break timestamp.
    const f64 KD_INITIAL_TIMESTAMP = 1.7976931348623157e+308;
}

// -------------------------------------------------------------------------------------
// StreamPool::GetInstance @0x82B6BA68
//
// Walk the registry comparing each pool's mGuid. The console recovers the owning pool
// from the list node by subtracting the literal 0x2C (@0x82B6BA78); on the host that
// literal is invalid the moment any member before mListNode widens, so the same
// container-of is expressed through offsetof.
//
// ⚠️ ALWAYS RETURNS NULL in this build -- the list is empty and has no writer. That is
// console behaviour, not a host limitation.
// -------------------------------------------------------------------------------------
StreamPool *StreamPool::GetInstance(u32 auGuid)
{
    ListDNode *lpNode = gpStreamPoolListHead;
    while (lpNode != 0)
    {
        StreamPool *lpPool = reinterpret_cast<StreamPool *>(
            reinterpret_cast<u8 *>(lpNode) - offsetof(StreamPool, mListNode));
        if (lpPool->mGuid == auGuid)
            return lpPool;
        lpNode = lpNode->pnext;   // `lwz r11, 0(r11)` @0x82B6BA90
    }
    return 0;                     // `li r3, 0` @0x82B6BA9C
}

// -------------------------------------------------------------------------------------
// StreamPool::AllocateStream -- the vendor's private inline helper. Its stores are
// visible in ARTIST at 0x82B6BB84..0x82B6BBAC (the free-slot path) and
// 0x82B6BC04..0x82B6BC30 (the eviction path); both write the same six fields.
// -------------------------------------------------------------------------------------
StreamPool::StreamDesc *StreamPool::AllocateStream(StreamDesc *apStreamDesc,
                                                   f32 afPriority,
                                                   StreamLostCallback apfnStreamLost,
                                                   void *apStreamLostContext)
{
    apStreamDesc->allocated = 1;
    ++apStreamDesc->refCount;
    apStreamDesc->priority = afPriority;
    apStreamDesc->pStreamLostCallback = apfnStreamLost;
    apStreamDesc->pStreamLostContext = apStreamLostContext;
    apStreamDesc->timeStamp = mpSystem->mfSystemTime;   // System::GetTime()
    return apStreamDesc;
}

// -------------------------------------------------------------------------------------
// StreamPool::AcquireStream @0x82B6BAB0
//
// Three scans, in order:
//   1. REUSE  -- the first allocated slot whose stored context is non-null and equals
//                the incoming context. Bumps the refcount and returns it. This is how a
//                single client that asks twice shares one stream.
//   2. FREE   -- the first slot with allocated == 0.
//   3. EVICT  -- the lowest-priority slot, ties broken by the lowest timeStamp.
//
// ⚠️ The `this` pointer is dereferenced at once (`lbz r11,0x28(r30)` @0x82B6BAD4) with no
// null guard, and no guard is added: SndPlayer1 calls this with a pool that is ALWAYS
// null in retail, so the console would fault here too. See the header banner.
// -------------------------------------------------------------------------------------
StreamPool::StreamHandle StreamPool::AcquireStream(f32 afPriority,
                                                   StreamLostCallback apfnStreamLost,
                                                   void *apStreamLostContext)
{
    // --- pass 1: reuse (0x82B6BAE4..0x82B6BB00, 0x82B6BB74..0x82B6BB80) ---
    for (int liIndex = 0; liIndex < mNumStreams; ++liIndex)
    {
        StreamDesc &lStream = mpStreamDesc[liIndex];
        if (lStream.allocated != 0 &&
            lStream.pStreamLostContext != 0 &&
            lStream.pStreamLostContext == apStreamLostContext)
        {
            ++lStream.refCount;
            return &lStream;
        }
    }

    // --- pass 2: first free slot (0x82B6BB20..0x82B6BB3C, 0x82B6BB84..0x82B6BBB0) ---
    for (int liIndex = 0; liIndex < mNumStreams; ++liIndex)
    {
        StreamDesc &lStream = mpStreamDesc[liIndex];
        if (lStream.allocated == 0)
            return AllocateStream(&lStream, afPriority, apfnStreamLost,
                                  apStreamLostContext);
    }

    // --- pass 3: pick an eviction victim (0x82B6BB40..0x82B6BBDC) ---
    // The running best starts AT the incoming priority, so a candidate must be strictly
    // below the caller's own priority to be considered at all.
    f32 lfSelectedPriority = afPriority;
    f64 ldSelectedTimeStamp = KD_INITIAL_TIMESTAMP;
    StreamDesc *lpSelected = 0;

    for (int liIndex = 0; liIndex < mNumStreams; ++liIndex)
    {
        StreamDesc &lStream = mpStreamDesc[liIndex];
        if (lStream.priority < lfSelectedPriority)
        {
            lfSelectedPriority = lStream.priority;
            ldSelectedTimeStamp = lStream.timeStamp;
            lpSelected = &lStream;
        }
        else if (lStream.priority == lfSelectedPriority &&
                 lStream.timeStamp < ldSelectedTimeStamp)
        {
            ldSelectedTimeStamp = lStream.timeStamp;
            lpSelected = &lStream;
        }
    }

    // --- eviction eligibility (0x82B6BBDC..0x82B6BBF0) ---
    // ⚠️ KEEP THESE AS NEGATED ORDERED COMPARISONS. The console's `fcmpu` + branch takes
    // the NOT-taken path when the comparison is unordered, so a NaN priority must FAIL to
    // qualify. Rewriting either as `>=` inverts the NaN case.
    if (!(lfSelectedPriority < afPriority) ||
        !(lfSelectedPriority < KF_MAX_REPLACEABLE_PRIORITY))
    {
        return 0;
    }

    // ⚠️ No null check on lpSelected, deliberately: the console calls straight through it
    // (@0x82B6BBF4..0x82B6BBF8). It is non-null whenever the ordered tests above passed,
    // because lfSelectedPriority can only have moved below afPriority by a store that also
    // set lpSelected.
    lpSelected->pStreamLostCallback(lpSelected->pStreamLostContext);
    return AllocateStream(lpSelected, afPriority, apfnStreamLost, apStreamLostContext);
}

// -------------------------------------------------------------------------------------
// StreamPool::ReleaseStream @0x82B6BC48
//
// ⚠️ The refcount is a SIGNED 16-bit value and the console sign-extends the decremented
// result before testing it (`extsh.` @0x82B6BC64). An over-release therefore goes
// NEGATIVE and the slot is never freed again -- it does not "clamp at zero". Reproduced.
// -------------------------------------------------------------------------------------
void StreamPool::ReleaseStream(StreamHandle apStreamHandle)
{
    StreamDesc *lpStreamDesc = static_cast<StreamDesc *>(apStreamHandle);
    if (--lpStreamDesc->refCount == 0)
    {
        // FLAG [ONE CALL OMITTED -- rw::core::filesys::Stream::Kill @0x82BBFFD8]
        //
        // The console calls it here (@0x82B6BC70..0x82B6BC74). It is fully DECODED AND
        // BODIED in this tree, at src/SDKs/EATech/rwcore/filesys/stream.cpp -- but that TU
        // is deliberately NOT MOUNTED. Two independent reasons, neither of which this
        // slice may paper over:
        //   1. rw::core::filesys is a documented PC simplification here. The PC build's
        //      real async I/O runs through the DeviceManager engine; the rw filesys bundle
        //      path was consciously left unmounted, so pulling it in is a subsystem
        //      decision, not a link fix.
        //   2. Mounting stream.cpp immediately exposes Stream::startnextrequest, which its
        //      own header records as "owned by its own (not-yet-homed) TU". That is a new
        //      decode front, and it belongs to the stream-content phase, not to this one.
        //
        // ⚠️ This omission has NO runtime effect, and the reason is exact rather than
        // hopeful: ReleaseStream is reachable only through a handle that AcquireStream
        // returned, AcquireStream is reachable only through a non-null pool, and
        // StreamPool::GetInstance CANNOT return non-null in this image (the registry list
        // head is a .bss global with no writer -- see the header banner). So this branch
        // is unreachable for the same reason the console's stream path is.
        //
        // DELETE-WHEN: Stream::startnextrequest is homed and stream.cpp is mounted. Then
        // restore the call -- it is one line: lpStreamDesc->pRwCoreStream->Kill();
        lpStreamDesc->allocated = 0;           // @0x82B6BC7C
    }
}

// -------------------------------------------------------------------------------------
// The vendor's two inline handle accessors. No ARTIST body of their own -- they are
// inlined wherever used -- but they are the vendor's public surface for a StreamHandle.
// -------------------------------------------------------------------------------------
void StreamPool::SetStreamPriority(StreamHandle apStreamHandle, f32 afPriority)
{
    static_cast<StreamDesc *>(apStreamHandle)->priority = afPriority;
}

rw::core::filesys::Stream *StreamPool::GetRwCoreStream(StreamHandle apStreamHandle)
{
    return static_cast<StreamDesc *>(apStreamHandle)->pRwCoreStream;
}

} // namespace core
} // namespace audio
} // namespace rw
