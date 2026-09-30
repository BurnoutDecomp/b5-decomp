#include "SDKs/EATech/eajobs/local_backend.h"

#include "eathread/eathread_semaphore.h"
#include "SDKs/EATech/eajobs/jobs.h"               // EA::Jobs::Allocator / GetAllocator (off_8327F280 Free)

#include <intrin.h>  // _InterlockedCompareExchange / _InterlockedExchange (atomic CAS)
#include <windows.h> // QueryPerformanceCounter, LARGE_INTEGER
#include <cstring>   // std::memcpy (the parameters blob copy)
#include <cstddef>   // offsetof (the uncalled _AssertLayout layout pins)
#include <new>       // placement new (conditional semaphore construction)

// ============================================================================
// SDKs/EATech/eajobs/local_backend.cpp
//
// EA::Jobs::LocalBackend::JobInstance::AutoTryLockEventList ctor/dtor, reconstructed
// store-for-store from the X360 .XEX:
//   AutoTryLockEventList::AutoTryLockEventList @ 0x82BCA658
//   AutoTryLockEventList::~AutoTryLockEventList @ 0x82BC9C68
//
// ctor (@ 0x82BCA658):
//   *this        = pJobInstance        ; stw r4,0(r3)
//   this->locked = false               ; stb r10(=0),4(r3)
//   r11 = &pJobInstance->lock          ; addi r11,r4,0x284
//   if (*r11 != 0) return              ; lwz r9,0(r11) / bnelr  (already held -> bail)
//   // try-CAS the lock 0 -> 1:
//   reservation: old = lock; if (old == 0) lock = 1; observe `old`
//   if (old == 0) this->locked = true  ; the lock was free and we took it
//
// dtor (@ 0x82BC9C68):
//   if (!this->locked) return          ; lbz r11,4(r3) / beqlr
//   // CAS the lock 1 -> 0 (release what we took):
//   reservation: if (lock == 1) lock = 0
//
// The masked-interrupt lwarx/stwcx. reservation is modeled on the MSVC host with an
// interlocked compare-and-swap -- the same conditional atomic the PowerPC primitive
// provides.
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
namespace LocalBackend
{
    // @ 0x82BCA658
    JobInstance::AutoTryLockEventList::AutoTryLockEventList(JobInstance* pJobInstance)
        : mpJobInstance(pJobInstance)
        , mbLocked(false)
    {
        volatile long* lpLock =
            reinterpret_cast<volatile long*>(&pJobInstance->mEventListLock);

        // Fast bail-out: if the lock already reads non-zero, don't even attempt the
        // reservation (the asm's `lwz r9,0(r11) / bnelr`).
        if (*lpLock != 0)
            return;

        // try-CAS 0 -> 1; mbLocked records whether we actually won it.
        long lObserved = _InterlockedCompareExchange(lpLock, 1, 0);
        if (lObserved == 0)
            mbLocked = true;
    }

    // @ 0x82BC9C68
    JobInstance::AutoTryLockEventList::~AutoTryLockEventList()
    {
        if (!mbLocked)
            return;

        volatile long* lpLock =
            reinterpret_cast<volatile long*>(&mpJobInstance->mEventListLock);

        // Release: CAS 1 -> 0 (only ever called when we hold the lock).
        _InterlockedCompareExchange(lpLock, 0, 1);
    }

    // =======================================================================
    // EA::Jobs::LocalBackend::JobInstance
    // =======================================================================

    // @ 0x82BCBB50 -- construct an empty slot.
    JobInstance::JobInstance()
        : mStatus(0)              // stw 0,0(this)
        // The submission timestamp is written by Initialize; arguments are assigned
        // there before publication, then Run overlays them with profiling data.
        // mEventLists[0..1] default-construct (empty BucketListNode<Event,16>):
        // the X360 `vector constructor iterator` over 16 Events + nulled mNext/mSize.
        , mGarbageCollectorLock(0) // stw 0,0x270(this)
        , mEventListLock(0)        // reservation loop stores 0 at +0x284 (lock free)
        , mbBeginPlayed(false)     // stb 0,0x288(this)
        , mbEndPlayed(false)       // stb 0,0x289(this)
    {
        // Job-parameter defaults (the ctor's individual stores into the +0x04 blob).
        mParameters = EntryPoint();
        // mParameters.mAllowSleepOn stays 0 -- no semaphore until Initialize asks.

        // The semaphore is NOT constructed here (the X360 ctor never touches +0x274).
    }

    // @ 0x82BCB6C8 -- destroy the slot: free both event lists' overflow chains and
    // null their mNext/mSize. (The X360 walks list[1] then list[0]; C++ member
    // destruction is reverse-declaration order, which is the same.) The semaphore, if
    // it was built, is released by Clear before the slot is collected -- the X360
    // destructor itself does not touch +0x274.
    JobInstance::~JobInstance()
    {
        // Explicit to mirror the asm's per-node free; the member dtors that follow
        // see the now-empty nodes and become no-ops.
        for (int liList = KI_NUM_EVENT_LISTS - 1; liList >= 0; --liList)
            mEventLists[liList].Clear();
    }

    // @ 0x82BCAA18 -- (re)initialise the slot from a submitted job.
    void JobInstance::Initialize(const EntryPoint* pParameters, const Param* pArguments)
    {
        // memcpy(this+4, src, 0x2C) -- copy the 44-byte job-parameters blob.
        mParameters = *pParameters;

        // Stamp the start time with the FULL 64-bit performance counter (asm `ld r11,var_30 /
        // std r11,0x30`). The mbHasSemaphore byte (+0x29) only GATES the semaphore-build branch
        // below; it is NOT merged into the timestamp (the Hex-Rays HIDWORD merge was an artifact).
        LARGE_INTEGER lCounter;
        QueryPerformanceCounter(&lCounter);
        mStartTimeStamp = static_cast<u64>(lCounter.QuadPart);

        // Record the four arguments (r5 points to the source block) and mark live.
        new (&mExecution) ExecutionData;
        for (int i=0;i<4;++i) mExecution.mArguments[i]=pArguments[i];
        mStatus  = 0;                 // stw 0,0(this)
        mGarbageCollectorLock = 1;    // stw 1,0x270(this) -- slot is in use

        // Build the completion semaphore iff the parameters flag it. The X360 inlines
        // EA::Thread::Semaphore::Semaphore(SemaphoreParameters{0, intra=1, ""}, true)
        // into the slot's +0x274 storage (sub_82B439D0).
        if (mParameters.mAllowSleepOn)
        {
            EA::Thread::SemaphoreParameters lParams(0, true, 0);
            new (mSemaphoreStorage) EA::Thread::Semaphore(&lParams, true);
        }

        // Reset the played flags and both event lists.
        mbBeginPlayed = false;        // stb 0,0x288(this)
        mbEndPlayed   = false;        // stb 0,0x289(this)
        for (int liList = 0; liList < KI_NUM_EVENT_LISTS; ++liList)
            mEventLists[liList].Clear();
    }

    // ARTIST82BCC2F0..82BCC388, absent from the exporter. r3-r6 receive
    // the four saved job arguments; profiling reuses that storage after return.
    void JobInstance::Run()
    {
        PlayEventList(Event::EVENT_WHEN_JOB_BEGIN);
        if (mParameters.mBreakOnEntry) DebugBreak();
        LARGE_INTEGER begin,end;
        QueryPerformanceCounter(&begin);
        // ARTIST82BCC324..32C permits event-only jobs with no code entry.
        if (mParameters.mpfnLocalJob)
            mParameters.mpfnLocalJob(mExecution.mArguments[0],mExecution.mArguments[1],
                                    mExecution.mArguments[2],mExecution.mArguments[3]);
        const u64 threadId=static_cast<u64>(EA::Thread::GetSysThreadId());
        QueryPerformanceCounter(&end);
        mExecution.mProfile={threadId,static_cast<u64>(begin.QuadPart),static_cast<u64>(end.QuadPart)};
        PlayEventList(Event::EVENT_WHEN_JOB_END);
    }

    // @ 0x82BCA968 -- garbage-collect the slot.
    void JobInstance::Clear()
    {
        // One strong CAS matches the console reservation loop: reservation loss
        // retries internally; either comparison result then proceeds to cleanup.
        volatile long* lpGcLock =
            reinterpret_cast<volatile long*>(&mGarbageCollectorLock);
        _InterlockedCompareExchange(lpGcLock, 0, 1);

        // Destruct the semaphore iff one was built (lbz 0x29 gate).
        if (mParameters.mAllowSleepOn)
            Semaphore()->~Semaphore();

        // Clear the played flags and both event lists.
        mbBeginPlayed = false;        // stb 0,0x288(this)
        mbEndPlayed   = false;        // stb 0,0x289(this)
        for (int liList = 0; liList < KI_NUM_EVENT_LISTS; ++liList)
            mEventLists[liList].Clear();
    }

    // The "infinite" absolute-timeout sentinel SleepOn hands to Semaphore::Wait. The
    // X360 passes &unk_821823E8; its role is the documented wait-forever deadline
    // (Semaphore::Wait treats -1 / INFINITE as a pass-through), so the sentinel holds
    // the INFINITE value. (The exact rodata bits at 0x821823E8 are not in the export
    // set; the value is grounded by Wait's documented INFINITE pass-through, not
    // invented as an arbitrary timeout.)
    static const u32 KU_INFINITE_TIMEOUT = 0xFFFFFFFFu;

    // @ 0x82BCAAE8 -- wait on then re-post the completion semaphore (if owned).
    void JobInstance::SleepOn()
    {
        if (!mParameters.mAllowSleepOn)
            return;

        EA::Thread::Semaphore* lpSemaphore = Semaphore();
        lpSemaphore->Wait(EA::Thread::kTimeoutNone);
        lpSemaphore->Post(1);
    }

    // @ 0x82BCBC00 -- run every event in one phase's event-list chain.
    void JobInstance::PlayEventList(Event::When eWhen)
    {
        // Spin until we actually win the event-list try-lock (the X360 loops:
        // construct guard; if it didn't take the lock, destruct + retry).
        for (;;)
        {
            AutoTryLockEventList lGuard(this);
            if (lGuard.Locked())
            {
                // Mark this phase played while holding the lock.
                if (eWhen == Event::EVENT_WHEN_JOB_BEGIN)
                    mbBeginPlayed = true;   // stb 1,0x288(this+when)
                else
                    mbEndPlayed = true;     // stb 1,0x289(this+when)
                break; // guard releases the lock as the scope exits
            }
            // guard's dtor releases nothing (it never took the lock); retry.
        }

        // Walk the phase's bucket chain, running each event in order.
        const Detail::BucketListNode<Event, 16>* lpNode = &mEventLists[eWhen];
        while (lpNode)
        {
            for (u32 luIndex = 0; luIndex < lpNode->mSize; ++luIndex)
                lpNode->mBucket[luIndex].Run();
            lpNode = lpNode->mNext;
        }
    }

    // @ 0x82BCBE70 -- MSVC `vector deleting destructor' thunk for JobInstance.
    //
    // Reconstructed as a free function: the per-object vector-deleting destructor is
    // a compiler-internal thunk with no portable C++ spelling, so the X360 binary
    // materialises it as this standalone routine (same treatment as
    // JobThread_ScalarDeletingDestructor in job_thread.cpp).
    //
    // cFlags bit1 (& 2) -> array variant (an array cookie holding the element count
    // sits 16 bytes before the first element); bit0 (& 1) -> also free the storage
    // via the process-wide Jobs allocator's Free virtual (off_8327F280, slot +0xC).
    JobInstance* JobInstance_VectorDeletingDestructor(JobInstance* pThis, char cFlags)
    {
        if ((cFlags & 2) != 0)
        {
            // Array form: the element count lives in the cookie 16 bytes ahead of the
            // array; destruct each element back-to-front (the X360's 0x290 stride).
            void* lpCookie = reinterpret_cast<u8*>(pThis) - 16;
            const u32 luCount = *reinterpret_cast<const u32*>(lpCookie);

            JobInstance* lpElement = pThis + luCount;
            for (s32 liIndex = static_cast<s32>(luCount) - 1; liIndex >= 0; --liIndex)
            {
                --lpElement;
                lpElement->~JobInstance();
            }

            if ((cFlags & 1) != 0 && lpCookie != 0)
            {
                EA::Jobs::Allocator* lpAllocator = EA::Jobs::GetAllocator();
                lpAllocator->Free(lpCookie, 0);
            }
            return reinterpret_cast<JobInstance*>(lpCookie);
        }

        // Scalar form: destruct the single object, optionally free it.
        pThis->~JobInstance();
        if ((cFlags & 1) != 0 && pThis != 0)
        {
            EA::Jobs::Allocator* lpAllocator = EA::Jobs::GetAllocator();
            lpAllocator->Free(pThis, 0);
        }
        return pThis;
    }

    // ========================================================================
    // EA::Jobs::LocalBackend::LocalBackend
    //
    // All bodies reconstructed store-for-store from the X360 .XEX (addresses on each
    // method). The packed-slot-word atomics (lwarx/ldarx + stdcx./stwcx. under a brief
    // interrupt mask) are modelled on the MSVC host with _InterlockedCompareExchange /
    // _InterlockedCompareExchange64 -- the same conditional 32/64-bit atomic the PowerPC
    // reservation primitive provides.
    // ========================================================================

    // dword_8327F284 -- the process-wide "should the job manager give up waiting?"
    // watchdog predicate. CreateNotReadyInstance consults it only after the ~12s GC
    // budget elapses (the X360 indirect-calls it through this file static, treating a
    // null pointer as "no watchdog installed"). Installed by the host; null here.
    typedef int (*GlobalAbortPredicate)();
    static GlobalAbortPredicate spGlobalAbortPredicate = 0;

    // Decode helpers for the packed 64-bit priority-queue slot word. The X360 builds
    // and tests these with rotate/clear-bit immediates; expressed here as named ops on
    // the recovered field widths (all masks are asm immediates).
    static inline u32 SlotIndexFromHandle(u64 uHandleQword)
    {
        // HIWORD(handle) -- the slot index sits in the high 16 bits of the low word.
        return static_cast<u32>(static_cast<u16>(uHandleQword >> 16));
    }

    // Pin the LocalBackend layout. The X360 byte offsets (ctor @ 0x82BCBCB8 / the
    // accessors) are 32-bit-pointer offsets:
    //   +0x004 mpJobInstances   +0x008 mpPriorityQueue  +0x00C mNextSlot
    //   +0x010 mNumSlots        +0x014 mThreads[32]     +0x598 mSubmissionCounter
    //   +0x5A0 mpProfilingCallback +0x5A4 mpProfilingContext +0x5A8 mGarbageCollectorCursor
    //   +0x5AC mpEnableProfiling +0x5B0 mJobThreadSleepTimeoutMS
    // The compile gate is an LLP64 (8-byte-pointer) host, so the absolute byte offsets
    // cannot be reproduced here -- only the pointer-width-INVARIANT structural facts the
    // CRITICAL fix turns on. The load-bearing one: there is EXACTLY ONE vptr at +0x0
    // (a second polymorphic base would push the first data member past one pointer and
    // shift every subsequent member), so the first data member sits immediately after
    // the vptr and the member ORDER below is the X360 order. Uncalled.
    static void LocalBackend_AssertLayout()
    {
        // Single vptr: first data member directly follows the one vtable pointer.
        static_assert(offsetof(LocalBackend, mpJobInstances) == sizeof(void*),
                      "single vptr: mpJobInstances is the first data member (one vtable ptr, no 2nd base)");
        // Member ORDER matches the X360 store sequence (offsets monotonic, ascending).
        static_assert(offsetof(LocalBackend, mpJobInstances)  < offsetof(LocalBackend, mpPriorityQueue),  "mpJobInstances < mpPriorityQueue");
        static_assert(offsetof(LocalBackend, mpPriorityQueue) < offsetof(LocalBackend, mNextSlot),        "mpPriorityQueue < mNextSlot");
        static_assert(offsetof(LocalBackend, mNextSlot)       < offsetof(LocalBackend, mNumSlots),        "mNextSlot < mNumSlots");
        static_assert(offsetof(LocalBackend, mNumSlots)       < offsetof(LocalBackend, mThreads),         "mNumSlots < mThreads");
        static_assert(offsetof(LocalBackend, mThreads)        < offsetof(LocalBackend, mSubmissionCounter), "mThreads < mSubmissionCounter");
        static_assert(offsetof(LocalBackend, mSubmissionCounter)     < offsetof(LocalBackend, mpProfilingCallback),     "mSubmissionCounter < mpProfilingCallback");
        static_assert(offsetof(LocalBackend, mpProfilingCallback)    < offsetof(LocalBackend, mpProfilingContext),      "mpProfilingCallback < mpProfilingContext");
        static_assert(offsetof(LocalBackend, mpProfilingContext)     < offsetof(LocalBackend, mGarbageCollectorCursor), "mpProfilingContext < mGarbageCollectorCursor");
        static_assert(offsetof(LocalBackend, mGarbageCollectorCursor) < offsetof(LocalBackend, mpEnableProfiling),  "mGarbageCollectorCursor < mpEnableProfiling");
        static_assert(offsetof(LocalBackend, mpEnableProfiling)  < offsetof(LocalBackend, mJobThreadSleepTimeoutMS),"mpEnableProfiling < mJobThreadSleepTimeoutMS");
        // The 32 inline workers occupy a real span between mThreads and the next member.
        static_assert(offsetof(LocalBackend, mSubmissionCounter) - offsetof(LocalBackend, mThreads)
                          == LocalBackend::KI_NUM_THREADS * sizeof(JobThread),
                      "mThreads is exactly 32 contiguous JobThreads");
    }

    // @ 0x82BCBCB8
    LocalBackend::LocalBackend(u32 uMaxJobs, bool* pEnableProfiling)
    {
        // (vtable +0x0 is installed by the C++ ABI prologue == off_82182500.)
        mpJobInstances = 0;           // stw 0,4(this)
        mNextSlot      = 0;           // stw 0,0xC(this)
        mNumSlots      = static_cast<s32>(uMaxJobs); // stw r28,0x10(this)

        // Construct the 32 idle worker threads (the X360 `vector constructor iterator`
        // over JobThread at this+0x14, stride 0x2C).
        // mThreads is a typed C++ array: its members are already constructed.
        // Repeating the console vector-constructor loop would leak wake events.

        // Release the submission counter (reservation loop storing 0 at +0x598).
        mSubmissionCounter       = 0;
        mpProfilingCallback      = 0;          // stw 0,0x5A0
        mpProfilingContext       = 0;          // stw 0,0x5A4
        mGarbageCollectorCursor  = 0;          // stw 0,0x5A8
        mpEnableProfiling    = pEnableProfiling;   // stw r27,0x5AC (ctor arg a3)
        mJobThreadSleepTimeoutMS = 1;          // li r11,1; stw r11,0x5B0

        EA::Jobs::Allocator* lpAllocator = EA::Jobs::GetAllocator();

        // Allocate + construct the JobInstance table (uMaxJobs slots, 0x290 stride). The
        // X360 saturates the byte count to 0xFFFFFFFF on overflow (the > 0x63E706 and
        // > 0xFFFFFFEF guards) and asks the allocator for size+16, align 16.
        const size_t luInstanceRequest = sizeof(JobInstance)*static_cast<size_t>(uMaxJobs)+16;

        void* lpInstanceBlock = lpAllocator->Alloc(
            luInstanceRequest, "EA::Jobs::LocalBackend::JobInstance", 1, 16, 0);
        if (lpInstanceBlock)
        {
            // The array cookie (element count) sits in the first 16 bytes; the elements
            // start 16 bytes in.
            *static_cast<u32*>(lpInstanceBlock) = uMaxJobs;
            JobInstance* lpInstances =
                reinterpret_cast<JobInstance*>(static_cast<u8*>(lpInstanceBlock) + 16);
            for (u32 luSlot = 0; luSlot < uMaxJobs; ++luSlot)
                new (&lpInstances[luSlot]) JobInstance();
            mpJobInstances = lpInstances;
        }
        else
        {
            mpJobInstances = 0;
        }

        // Allocate the parallel priority queue (uMaxJobs u64 words, align 128) and set
        // every slot to -1 ("never used / reclaimed").
        u32 luQueueBytes = 8u * uMaxJobs;
        if (uMaxJobs > 0x1FFFFFFFu)
            luQueueBytes = 0xFFFFFFFFu;
        SlotWord* lpQueue = static_cast<SlotWord*>(lpAllocator->Alloc(
            luQueueBytes, "EA::Jobs::PriorityQueueEntry", 0, 128, 0));
        if (lpQueue)
        {
            for (u32 luSlot = 0; luSlot < uMaxJobs; ++luSlot)
                lpQueue[luSlot] = static_cast<u64>(-1);
            mpPriorityQueue = lpQueue;
        }
        else
        {
            mpPriorityQueue = 0;
        }
    }

    // @ 0x82BCBF38
    LocalBackend::~LocalBackend()
    {
        // (vtable +0x0 was restamped to off_82182500 by the C++ ABI dtor prologue.)
        // Tell every worker to quit and wake it.
        for (int liThread = 0; liThread < KI_NUM_THREADS; ++liThread)
        {
            mThreads[liThread].mbQuit = true;     // stb 1,-0x1F(thread+0x3C) == thread+0x09
            SetEvent(mThreads[liThread].mhEvent);
        }

        // Join them all.
        for (int liThread = 0; liThread < KI_NUM_THREADS; ++liThread)
            mThreads[liThread].WaitForEnd();

        // Final forced garbage-collection pass.
        TryToRunJobInstanceGarbageCollector(1);

        EA::Jobs::Allocator* lpAllocator = EA::Jobs::GetAllocator();

        // Free the instance table (array vector-deleting dtor, flags 3 == array + free).
        if (mpJobInstances)
            JobInstance_VectorDeletingDestructor(mpJobInstances, 3);

        // Free the priority queue.
        if (mpPriorityQueue)
            lpAllocator->Free(mpPriorityQueue, 0);

        // Destruct the 32 worker threads (reverse order, matching the X360 walk).
        // The typed worker array destructs once through normal C++ lifetime.

        // (The C++ ABI dtor epilogue stamps the Detail::SchedulerBackend base vtable
        // off_82182408 into vtable +0x0 -- the X360's final `stw off_82182408,0(this)`.)
    }

    // @ 0x82BCC010 (vtable +0x28) -- CreateNotReadyInstance / AddNotReady.
    void LocalBackend::CreateNotReadyInstance(JobInstanceHandle* pOutHandle,
                                              const EntryPoint*  pEntryPoint,
                                              const Param*       pParams)
    {
        // Atomically take the next submission id (X360 increments the +0x598 counter
        // under the masked reservation and keeps the post-increment value).
        u64 luSubmissionId = static_cast<u64>(_InterlockedIncrement64(
            reinterpret_cast<volatile __int64*>(&mSubmissionCounter)));

        LARGE_INTEGER lStartTick;
        QueryPerformanceCounter(&lStartTick);

        // The ~12-second give-up budget the watchdog branch uses (flt_82013FB0).
        const f32 KF_GC_TIMEOUT_SECONDS = 12.0f;

        s32 liSlot = mNextSlot + 1;
        for (;;)
        {
            // No free slot in this pass -> run a GC sweep and re-check the time budget.
            if (liSlot >= mNumSlots)
            {
                liSlot = 0;
                TryToRunJobInstanceGarbageCollector(0);

                LARGE_INTEGER lNow;
                QueryPerformanceCounter(&lNow);
                // The asm @ 0x82BCC158 loads BOTH counters as full doublewords (`ld`)
                // and subtracts qwords (`subf r3,r10,r11`) before TicksToSeconds -- a
                // full 64-bit delta, NOT a 32-bit LowPart difference.
                f32 lfElapsed = EA::Jobs::TicksToSeconds(
                    static_cast<u64>(lNow.QuadPart) - static_cast<u64>(lStartTick.QuadPart));
                if (lfElapsed >= KF_GC_TIMEOUT_SECONDS && spGlobalAbortPredicate
                    && !spGlobalAbortPredicate())
                {
                    // Give up: write a null handle (submission id 0, backend 0, index 0).
                    pOutHandle->mSubmissionId     = 0;
                    pOutHandle->mSchedulerBackend = 0;
                    pOutHandle->mIndex            = 0;
                    return;
                }
                continue;
            }

            // Try to claim slot liSlot iff its queue word is still -1.
            volatile __int64* lpEntry =
                reinterpret_cast<volatile __int64*>(&mpPriorityQueue[liSlot]);
            if (*lpEntry == static_cast<__int64>(-1))
            {
                // Build the packed word: low 47 bits = submission id; the priority /
                // affinity bitfield + the not-ready state from the entry point.
                u64 luLowId = luSubmissionId & 0x7FFFFFFFFFFFull;
                u32 luPriority = static_cast<u32>(pEntryPoint->GetPriority()) & 0xFFu;
                u32 luAffinity = static_cast<u32>(pEntryPoint->GetAffinity()) & 0xFFu;
                u32 luField = ((((~luAffinity) << 8) & 0x3F00u) | 0x8000u | luPriority);
                u64 luPacked = (static_cast<u64>(luField << 15) << 32) | luLowId;

                if (_InterlockedCompareExchange64(lpEntry, static_cast<__int64>(luPacked),
                                                  static_cast<__int64>(-1))
                    == static_cast<__int64>(-1))
                {
                    // Won the slot: initialise the JobInstance and publish the handle.
                    JobInstanceHandle lNewHandle(this, static_cast<u16>(liSlot),
                                                 luSubmissionId);
                    mpJobInstances[liSlot].Initialize(pEntryPoint, pParams);
                    mNextSlot = liSlot;
                    pOutHandle->mSubmissionId     = luSubmissionId;
                    pOutHandle->mSchedulerBackend = this;
                    pOutHandle->mIndex            = static_cast<u16>(liSlot);
                    return;
                }
            }
            ++liSlot;
        }
    }

    // @ 0x82BC9D88 (vtable +0x30) -- IsJobComplete / IsDone.
    int LocalBackend::IsJobComplete(u64 uSubmissionId, u64 uHandleQword)
    {
        u32 luIndex = SlotIndexFromHandle(uHandleQword);
        u64 luEntry = mpPriorityQueue[luIndex];

        // The slot must still hold this submission (low 47 bits) or it is done.
        if (uSubmissionId != (luEntry & 0x7FFFFFFFFFFFull))
            return 1;
        if (!mpJobInstances)
            return 1;

        // Top-3-bits state tag == FREE -> done.
        if ((luEntry & KU_SLOT_STATE_MASK) == KU_SLOT_STATE_FREE)
            return 1;
        // Fully reclaimed word -> done.
        if (luEntry == static_cast<u64>(-1))
            return 1;
        // Submission id mismatch on the full word -> done; else still pending.
        if (uSubmissionId != (luEntry & 0x7FFFFFFFFFFFull))
            return 1;
        return 0;
    }

    // @ 0x82BCC1D8 (vtable +0x34) -- SubmitEvent / AddEvent.
    int LocalBackend::SubmitEvent(u64 uSubmissionId, u64 uHandleQword,
                                  const void* pPayload, int iWhen)
    {
        Event* lpEvent = const_cast<Event*>(static_cast<const Event*>(pPayload));
        u32 luIndex    = SlotIndexFromHandle(uHandleQword);
        u64 luEntry    = mpPriorityQueue[luIndex];

        JobInstance* lpInstance =
            (uSubmissionId == (luEntry & 0x7FFFFFFFFFFFull))
                ? &mpJobInstances[luIndex]
                : 0;

        // The phase's "played" flag: begin (+0x288) / end (+0x289), reached by name.
        const bool* lpPlayedFlag = lpInstance
            ? (iWhen == Event::EVENT_WHEN_JOB_BEGIN
                   ? &lpInstance->mbBeginPlayed
                   : &lpInstance->mbEndPlayed)
            : 0;

        if (lpInstance)
        {
            // Spin while the slot is live, still holds our submission, and this phase
            // has NOT yet been played.
            while ((static_cast<u64>(mpPriorityQueue[luIndex]) & KU_SLOT_STATE_MASK)
                       != KU_SLOT_STATE_FREE
                   && (mpPriorityQueue[luIndex] & 0x7FFFFFFFFFFFull) == uSubmissionId
                   && !*lpPlayedFlag)
            {
                JobInstance::AutoTryLockEventList lGuard(lpInstance);
                if (lGuard.Locked())
                {
                    // Re-check under the lock: if it went done/replayed, run the event
                    // immediately; otherwise append it to the phase's event list.
                    if ((static_cast<u64>(mpPriorityQueue[luIndex]) & KU_SLOT_STATE_MASK)
                            == KU_SLOT_STATE_FREE
                        || (mpPriorityQueue[luIndex] & 0x7FFFFFFFFFFFull) != uSubmissionId
                        || *lpPlayedFlag)
                    {
                        lpEvent->Run();
                    }
                    else
                    {
                        lpInstance->mEventLists[iWhen].Add(*lpEvent);
                    }
                    return 0; // guard releases the lock as it leaves scope
                }
                // Didn't win the lock -> retry.
            }
        }

        // Slot already gone / phase already played -> just run the event.
        lpEvent->Run();
        return 0;
    }

    // @ 0x82BCA7B8 (vtable +0x38): increment this instance's prerequisite count
    // and return the event which enables its packed queue word on the final signal.
    Event LocalBackend::AddBarrier(u64 uSubmissionId,u64 uHandleQword)
    {
        const u32 index=SlotIndexFromHandle(uHandleQword);
        JobInstance* instance=(uSubmissionId==(static_cast<u64>(mpPriorityQueue[index])&0x7FFFFFFFFFFFull))
            ? &mpJobInstances[index] : nullptr;
        u32* location=nullptr;u32 value=0;
        GetEnabler(uSubmissionId,uHandleQword,location,value);
        // FLAG PC-platform leaf: dependency completion may race a later
        // submission adding another barrier. Pair with Event::Run's decrement.
        _InterlockedIncrement(reinterpret_cast<volatile long*>(&instance->mStatus));
        return Event(Event::EVENT_TYPE_WRITE,value,location,&instance->mStatus);
    }

    void LocalBackend::GetEnabler(u64,u64 uHandleQword,u32*& pLocation,u32& uValue)
    {
        SlotWord& slot=mpPriorityQueue[SlotIndexFromHandle(uHandleQword)];
        pLocation=&slot.mHigh;
        uValue=static_cast<u32>(static_cast<u64>(slot)>>32)&0x1FFFFFFFu;
    }

    // @ 0x82BCA878 (vtable +0x40), 0 == not sleepable. The leading
    // `(*(*this + 48))(this)` in the asm is THIS object's vtable +0x30 == IsJobComplete.
    int LocalBackend::GetAllowSleepOn(u64 uSubmissionId, u64 uHandleQword)
    {
        u32 luIndex = SlotIndexFromHandle(uHandleQword);

        // Already complete -> not sleepable (asm: bctrl through vtable +0x30).
        if (IsJobComplete(uSubmissionId, uHandleQword))
            return 0;

        u64 luEntry = mpPriorityQueue[luIndex];
        JobInstance* lpInstance =
            (uSubmissionId == (luEntry & 0x7FFFFFFFFFFFull))
                ? &mpJobInstances[luIndex]
                : 0;
        if (!lpInstance)
            return 0;

        // The JobInstance mAllowSleepOn byte at +0x29 (mParameters+0x25). Read by name
        // through mParameters to honour the no-raw-offset rule.
        return lpInstance->mParameters.mAllowSleepOn;
    }

    // @ 0x82BCBA80 (vtable +0x3C) -- SleepOn.
    int LocalBackend::SleepOn(u64 uSubmissionId, u64 uHandleQword)
    {
        int liResult = IsJobComplete(uSubmissionId, uHandleQword);
        if (liResult)
            return liResult;

        u32 luIndex = SlotIndexFromHandle(uHandleQword);
        u64 luEntry = mpPriorityQueue[luIndex];
        JobInstance* lpInstance =
            (uSubmissionId == (luEntry & 0x7FFFFFFFFFFFull))
                ? &mpJobInstances[luIndex]
                : 0;
        if (!lpInstance)
            return liResult;

        // Take a live-reference on the slot while we wait on it.
        bool lbRef = lpInstance->RefCount()->Increment();
        if (lbRef)
        {
            // Re-check completion; only block if still pending.
            liResult = IsJobComplete(uSubmissionId, uHandleQword);
            if (!liResult)
            {
                lpInstance->SleepOn();
                liResult = 0;
            }
            lpInstance->RefCount()->Decrement();
        }
        return liResult;
    }

    // @ 0x82BCC490 (vtable +0x50) -- ExecuteReadyJobs / RunSingleJob.
    int LocalBackend::ExecuteReadyJobs(int iAffinity)
    {
        u8 uAffinityMask = static_cast<u8>(iAffinity);
        s32 liCount = mNumSlots;
        if (liCount <= 0)
            return 0;

        // Pick the slot with the smallest packed word among the ready entries (the X360
        // ORs in the affinity-don't-care mask before comparing).
        u64 luAffinityFill = (~static_cast<u64>(uAffinityMask) & 0x3Full) << 55;
        s32 liBest = -1;
        u64 luBestWord = static_cast<u64>(-1);
        SlotWord* lpQueue = mpPriorityQueue;
        for (s32 liSlot = 0; liSlot < liCount; ++liSlot)
        {
            u64 luWord = lpQueue[liSlot] | luAffinityFill;
            if (luWord < luBestWord)
            {
                luBestWord = luWord;
                liBest     = liSlot;
            }
        }
        if (liBest < 0)
            return 0;

        volatile __int64* lpEntry =
            reinterpret_cast<volatile __int64*>(&lpQueue[liBest]);
        u64 luOriginal = static_cast<u64>(*lpEntry);

        // Bits 55..60 store the complement of the job's six affinity bits.
        u32 luJobAffinity = static_cast<u32>(luOriginal >> 55) & 0x3Fu;
        if ((uAffinityMask & ~luJobAffinity) == 0)
            return 0;

        // Clear the three state bits and construct the RUNNING word.
        const u64 luReady = luOriginal & ~KU_SLOT_STATE_MASK;
        u64 luRunning = luReady | 0x2000000000000000ull;

        // Only a READY word can transition to RUNNING, even if the scan saw NOT_READY.
        if (_InterlockedCompareExchange64(lpEntry, static_cast<__int64>(luRunning),
                                          static_cast<__int64>(luReady))
        != static_cast<__int64>(luReady))
        {
            return 0; // lost the race -> nothing ran
        }

        // Run the job.
        JobInstance* lpInstance = &mpJobInstances[liBest];
        lpInstance->Run();

        // The "completed" word: set the 0x60000000 free/done tag.
        u64 luDone = luReady | KU_SLOT_STATE_FREE;

        if (lpInstance->mParameters.mAllowSleepOn)
        {
            // Sleep-on jobs: hold a reference, publish DONE, post the
            // completion permit, then drop the reference.
            lpInstance->RefCount()->Increment();
            _InterlockedCompareExchange64(lpEntry, static_cast<__int64>(luDone),
                                          static_cast<__int64>(luRunning));
            if (lpInstance->mParameters.mAllowSleepOn)
                lpInstance->Semaphore()->Post(1);
            lpInstance->RefCount()->Decrement();
        }
        else if (mpEnableProfiling && *mpEnableProfiling)
        {
            // Keep the completed record for GC while scheduler profiling is enabled.
            _InterlockedCompareExchange64(lpEntry, static_cast<__int64>(luDone),
                                          static_cast<__int64>(luRunning));
        }
        else
        {
            // Mark the slot free/done (-1 republish via the running word).
            _InterlockedCompareExchange64(lpEntry, -1, static_cast<__int64>(luRunning));
        }

        // Wake the workers so they pick up newly-ready / completed work.
        WakeThreads();
        return 1;
    }

    // @ 0x82BCBE60 (vtable +0x54) -- GetSleepTimeoutMs.
    u32 LocalBackend::GetSleepTimeoutMs()
    {
        return mJobThreadSleepTimeoutMS;
    }

    // @ 0x82BCA910 (vtable +0x1C) -- WakeThreads.
    void LocalBackend::WakeThreads()
    {
        for (int liThread = 0; liThread < KI_NUM_THREADS; ++liThread)
        {
            if (mThreads[liThread].mbStarted)
                SetEvent(mThreads[liThread].mhEvent);
        }
    }

    // @ 0x82BCA6C0 -- AddThread.
    void LocalBackend::AddThread(JobThreadHandle* pOutHandle,
                                 const JobThreadParameters& rParameters)
    {
        int liSlot = 0;
        while (mThreads[liSlot].mbStarted)
        {
            ++liSlot;
            if (liSlot >= KI_NUM_THREADS)
            {
                // All workers in use: no-op handle.
                GetThreadHandle(pOutHandle, 0, 0);
                return;
            }
        }
        mThreads[liSlot].Start(&rParameters, this);
        GetThreadHandle(pOutHandle, this, static_cast<u32>(liSlot));
    }

    // @ 0x82BC9D58 -- GetNumThreads.
    int LocalBackend::GetNumThreads() const
    {
        int liCount = KI_NUM_THREADS;
        for (int liThread = 0; liThread < KI_NUM_THREADS; ++liThread)
        {
            if (!mThreads[liThread].mbStarted)
                --liCount;
        }
        return liCount;
    }

    // @ 0x82BC9CF8 -- GetNumJobsInFlight.
    int LocalBackend::GetNumJobsInFlight() const
    {
        int liResult = 0;
        s32 liCount  = mNumSlots;
        if (liCount > 0)
        {
            const SlotWord* lpQueue = mpPriorityQueue;
            for (s32 liSlot = 0; liSlot < liCount; ++liSlot)
            {
                u64 luWord = lpQueue[liSlot];
                if ((luWord & KU_SLOT_STATE_MASK) != KU_SLOT_STATE_FREE
                    && luWord != static_cast<u64>(-1))
                {
                    ++liResult;
                }
            }
        }
        return liResult;
    }

    // @ 0x82BCA738 -- WaitForEnd (join worker iIndex).
    void LocalBackend::WaitForEnd(int iIndex)
    {
        mThreads[iIndex].WaitForEnd();
    }

    // @ 0x82BC9E20 -- GetThreadHandle.
    void LocalBackend::GetThreadHandle(JobThreadHandle* pOutHandle,
                                       Detail::SchedulerBackend* pBackend,
                                       u32 uHandle) const
    {
        *pOutHandle = JobThreadHandle(pBackend, uHandle);
    }

    // @ 0x82BC9E30 -- GetThreadId. The X360 truncates the id to its low 32 bits
    // (`clrldi r3,r3,32`) before returning it as a job-thread id.
    u64 LocalBackend::GetThreadId(int iIndex) const
    {
        // FLAG PC-platform leaf: native EAThread::ThreadId is a HANDLE. Export
        // its system ID, the same identity recorded by JobInstance::Run.
        return static_cast<u64>(EA::Thread::GetSysThreadId(mThreads[iIndex].mThread.GetId()));
    }

    // @ 0x82BCA770 -- GetThreadParameters.
    void LocalBackend::GetThreadParameters(JobThreadParameters* pOut, int iIndex) const
    {
        std::memcpy(pOut, &mThreads[iIndex].mParameters, sizeof(JobThreadParameters));
    }

    // @ 0x82BC9CB8 -- GetEnabler.


    // @ 0x82BC9E68 -- SetProfilingCallback.
    void LocalBackend::SetProfilingCallback(ProfilerCallback* pCallback)
    {
        mpProfilingCallback = pCallback;
    }

    // @ 0x82BC9E70 -- SetProfilingContext.
    void LocalBackend::SetProfilingContext(void* pContext)
    {
        mpProfilingContext = pContext;
    }

    // @ 0x82BCBB48 -- FlushProfile.
    void LocalBackend::FlushProfile()
    {
        TryToRunJobInstanceGarbageCollector(1);
    }

    // @ 0x82BCBE68 -- SetJobThreadSleepTimeoutMS.
    void LocalBackend::SetJobThreadSleepTimeoutMS(u32 uTimeoutMS)
    {
        mJobThreadSleepTimeoutMS = uTimeoutMS;
    }

    // The per-slot profiling-metric record harvested by the garbage collector when a
    // profiling callback is installed (X360 init @ 0x82BCB750, harvest @ 0x82BCB938;
    // 0x50 == 80-byte stride from `mulli r10,r25,0x50`). The GC builds a 23-entry stack
    // array and hands the callback a pointer + count. Field NAMES follow each store's
    // role; OFFSETS/SIZES are all asm-grounded and pinned below.
    //
    //   harvest: +0x00 <= instance+0x30 ; +0x08 <= instance+0x40
    //            +0x10 <= instance+0x48 ; +0x18 <= instance+0x38
    //            memcpy(+0x20, instance+0x04, 0x2C)
    //   init   : +0x20=0(byte) +0x30=0x80 +0x34=0x3F +0x38=0 +0x3C=0
    //            +0x40=1 +0x44=0(byte) +0x45=0(byte) +0x48=0
    //
    // The trailing EntryPoint is default-constructed, then assigned on harvest.

    // @ 0x82BCB718 -- TryToRunJobInstanceGarbageCollector.
    void LocalBackend::TryToRunJobInstanceGarbageCollector(int bForce, int iClearArg2,
                                                           int iClearArg3)
    {
        // The rolling collection window: min(32, mNumSlots).
        s32 liWindow = (mNumSlots < KI_NUM_THREADS) ? mNumSlots : KI_NUM_THREADS;

        // Stage-buffer of up to 23 profiling records (defaults set per the X360 init
        // loop). Only consumed when a profiling callback is installed.
        JobMetrics laRecords[23]{};

        s32 liStart = mGarbageCollectorCursor;
        if (bForce == 1)
            liStart = 0;

        s32 liNumSlots   = mNumSlots;
        s32 liFirstFreed = -1;
        s32 liRecCount   = 0;

        // Advance the persisted cursor by the window (mod capacity) up front, exactly as
        // the X360 (`(cursor + window) % numSlots`).
        if (liNumSlots > 0)
            mGarbageCollectorCursor = (liStart + liWindow) % liNumSlots;

        if (liStart >= liNumSlots)
        {
            // Nothing to scan this pass; still flush any staged records below.
        }
        else
        {
            s32 liSlot = liStart;
            for (;;)
            {
                volatile __int64* lpEntry =
                    reinterpret_cast<volatile __int64*>(&mpPriorityQueue[liSlot]);
                u64 luCaptured = static_cast<u64>(*lpEntry);

                // ARTIST82BCB7EC..854: claim only a completed slot,
                // changing its state from DONE to GC. Never reclaim a live job.
                const u64 luDone=(luCaptured&~KU_SLOT_STATE_MASK)|KU_SLOT_STATE_FREE;
                const u64 luCollecting=(luCaptured&~KU_SLOT_STATE_MASK)|0x8000000000000000ull;
                bool lbStable=(_InterlockedCompareExchange64(lpEntry,
                    static_cast<__int64>(luCollecting),static_cast<__int64>(luDone))
                    ==static_cast<__int64>(luDone));

                if (lbStable)
                {
                    JobInstance* lpInstance = &mpJobInstances[liSlot];

                    // Sleep-on jobs: the asm @0x82BCB868 CAS's the refcount word (this+0x270):
                    // if the old count == 1 it stores 0 and PROCEEDS to reclaim (no outstanding
                    // sleeper); otherwise (count != 1) it republishes the captured word and SKIPS
                    // reclaim. mCount is the s32 ReferenceCount word; CAS 1->0 mirrors the binary.
                    bool lbReclaim = true;
                    if (lpInstance->mParameters.mAllowSleepOn)
                    {
                        volatile long* lpRefCount =
                            reinterpret_cast<volatile long*>(&lpInstance->RefCount()->mCount);
                        if (_InterlockedCompareExchange(lpRefCount, 0, 1) != 1)
                        {
                            // still referenced (outstanding sleeper) -> leave the slot alive,
                            // republish the captured queue word and advance without reclaiming.
                            _InterlockedCompareExchange64(
                                lpEntry, static_cast<__int64>(luDone),
                                static_cast<__int64>(luCollecting));
                            lbReclaim = false;
                        }
                    }

                    if (lbReclaim)
                    {
                        // Harvest a profiling record if a callback is installed. The asm
                        // @ 0x82BCB938 writes four qwords from the instance then memcpys
                        // the 44-byte params blob -- to the record offsets pinned above.
                        if (mpProfilingCallback)
                        {
                            JobMetrics& lrRec = laRecords[liRecCount];
                            lrRec.ticksAtSubmission=lpInstance->mStartTimeStamp;
                            lrRec.ticksAtBegin=lpInstance->mExecution.mProfile.mBegin;
                            lrRec.ticksAtEnd=lpInstance->mExecution.mProfile.mEnd;
                            lrRec.threadId=lpInstance->mExecution.mProfile.mThreadId;
                            lrRec.entryPoint=lpInstance->mParameters;
                            if (++liRecCount == 23)
                            {
                                mpProfilingCallback(laRecords, 23, mpProfilingContext);
                                liRecCount = 0;
                            }
                        }

                        // Reclaim the slot.
                        lpInstance->Clear();
                        if (liFirstFreed == -1)
                            liFirstFreed = liSlot;

                        // Republish the queue word as -1 (mark fully reclaimed).
                        _InterlockedCompareExchange64(
                            lpEntry, static_cast<__int64>(-1),
                            static_cast<__int64>(luCollecting));

                        if (bForce == 1)
                        {
                            ++liSlot;
                            if (liSlot < liNumSlots) continue; else break;
                        }
                        if (liSlot - liStart > liWindow && liFirstFreed != -1)
                            break;
                    }
                }

                // Window-wrap handling (the X360's LABEL_23 / wrap arithmetic).
                if (bForce == 1 || liSlot + 1 != mNumSlots
                    || (liSlot - liStart + 1) >= liWindow)
                {
                    ++liSlot;
                }
                else
                {
                    liWindow -= liStart;
                    liStart   = 0;
                    liSlot    = 0;
                }
                if (liSlot >= mNumSlots)
                    break;
            }

            if (liFirstFreed != -1)
                mNextSlot = liFirstFreed - 1;
        }

        // Flush any staged profiling records.
        if (liRecCount > 0 && mpProfilingCallback)
        {
            mpProfilingCallback(laRecords, liRecCount, mpProfilingContext);
        }

        (void)iClearArg2;
        (void)iClearArg3;
    }

    // @ 0x82BCC420 -- LocalBackend `vector deleting destructor' thunk.
    LocalBackend* LocalBackend_VectorDeletingDestructor(LocalBackend* pThis, char cFlags)
    {
        pThis->~LocalBackend();
        if ((cFlags & 1) != 0 && pThis != 0)
        {
            EA::Jobs::Allocator* lpAllocator = EA::Jobs::GetAllocator();
            lpAllocator->Free(pThis, 0);
        }
        return pThis;
    }
}
}
}
