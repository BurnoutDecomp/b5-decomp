#include "SDKs/EATech/eajobs/local_backend.h"

#include "SDKs/EATech/eathread/BrnEAThreadX360.h" // EA::Thread::SemaphoreParameters
#include "SDKs/EATech/eajobs/jobs.h"               // EA::Jobs::Allocator / GetAllocator (off_8327F280 Free)

#include <intrin.h>  // _InterlockedCompareExchange / _InterlockedExchange (atomic CAS)
#include <windows.h> // QueryPerformanceCounter, LARGE_INTEGER
#include <cstring>   // std::memcpy (the parameters blob copy)
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
        // mStartTimeStamp(+0x30) and mHandle(+0x38) are NOT written by the ctor asm -- they stay
        // indeterminate until Initialize() stamps them (don't add zeroing the binary lacks).
        // mEventLists[0..1] default-construct (empty BucketListNode<Event,16>):
        // the X360 `vector constructor iterator` over 16 Events + nulled mNext/mSize.
        , mGarbageCollectorLock(0) // stw 0,0x270(this)
        , mEventListLock(0)        // reservation loop stores 0 at +0x284 (lock free)
        , mbBeginPlayed(false)     // stb 0,0x288(this)
        , mbEndPlayed(false)       // stb 0,0x289(this)
    {
        // Job-parameter defaults (the ctor's individual stores into the +0x04 blob).
        std::memset(&mParameters, 0, sizeof(mParameters));
        mParameters.mPriority = 128; // JOB_PRIORITY_DEFAULT / MEDIUM (li r11,0x80)
        mParameters.mAffinity = 63;  // JOB_AFFINITY_ANY              (li r10,0x3F)
        mParameters.mFlags24  = 1;   // (li r28,1; stw r28,0x24)
        // mParameters.mbHasSemaphore stays 0 -- no semaphore until Initialize asks.

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
    void JobInstance::Initialize(const void* pParameters, const JobInstanceHandle& rHandle)
    {
        // memcpy(this+4, src, 0x2C) -- copy the 44-byte job-parameters blob.
        std::memcpy(&mParameters, pParameters, sizeof(mParameters));

        // Stamp the start time with the FULL 64-bit performance counter (asm `ld r11,var_30 /
        // std r11,0x30`). The mbHasSemaphore byte (+0x29) only GATES the semaphore-build branch
        // below; it is NOT merged into the timestamp (the Hex-Rays HIDWORD merge was an artifact).
        LARGE_INTEGER lCounter;
        QueryPerformanceCounter(&lCounter);
        mStartTimeStamp = static_cast<u64>(lCounter.QuadPart);

        // Record the handle (a3[0..3]) and mark the slot live.
        mHandle  = rHandle;
        mStatus  = 0;                 // stw 0,0(this)
        mGarbageCollectorLock = 1;    // stw 1,0x270(this) -- slot is in use

        // Build the completion semaphore iff the parameters flag it. The X360 inlines
        // EA::Thread::Semaphore::Semaphore(SemaphoreParameters{0, intra=1, ""}, true)
        // into the slot's +0x274 storage (sub_82B439D0).
        if (mParameters.mbHasSemaphore)
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

    // @ 0x82BCA968 -- garbage-collect the slot.
    void JobInstance::Clear()
    {
        // Spin to take the GC reservation word: the X360 loops on lwarx/stwcx.
        // releasing the word (1 -> 0) and breaks out either way; modelled as a
        // CAS 1 -> 0 retried until the word is observed as anything but 1.
        volatile long* lpGcLock =
            reinterpret_cast<volatile long*>(&mGarbageCollectorLock);
        while (_InterlockedCompareExchange(lpGcLock, 0, 1) == 1)
        {
            // reservation lost / word toggled -- retry, matching the asm's spin.
        }

        // Destruct the semaphore iff one was built (lbz 0x29 gate).
        if (mParameters.mbHasSemaphore)
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
        if (!mParameters.mbHasSemaphore)
            return;

        EA::Thread::Semaphore* lpSemaphore = Semaphore();
        lpSemaphore->Wait(&KU_INFINITE_TIMEOUT);
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
}
}
}
