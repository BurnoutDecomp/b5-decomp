// X360-faithful EA::Thread::Condition (the generic "algorithm 8a" Win32 condition
// variant linked into BURNOUT_X360_ARTIST.XEX). Source of truth: per-addr exports
// under .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides the divergent
// newer vendor snapshot). See eathread_condition.h for the committed-type /
// vendor-snapshot divergence FLAG.
//
// Vendor EA code reconstructed in its canonical home.

#include "SDKs/EATech/eathread/eathread_condition.h"

#include <windows.h>  // CRITICAL_SECTION, Initialize/Enter/LeaveCriticalSection
#include <intrin.h>   // _InterlockedIncrement / _InterlockedExchange / _InterlockedExchangeAdd

namespace EA
{
namespace Thread
{

// The X360 condition guards its bookkeeping with an internal "unblock-lock"
// CRITICAL_SECTION (Init does RtlInitializeCriticalSection on it; Signal/Wait do the
// X360 `EA::Thread::Mutex::Lock` enter + `RtlLeaveCriticalSection` leave). On the
// host those map directly to the Win32 critical-section API. The asm enter returns
// 0 (>= 0) on success; a negative return means the lock could not be taken (only
// possible for the cancellable/inter-process variant, which the X360 build does not
// hit here -- EnterCriticalSection always succeeds for an intra-process section).
namespace
{
    // The X360 passes &unk_821473D0 -- the EAThread kTimeoutNone constant -- to the
    // internal Semaphore::Wait / Mutex::Lock calls (block forever, no deadline). On
    // the X360/Win32 the homed Semaphore::Wait treats 0xFFFFFFFF as INFINITE, so the
    // block-forever sentinel is 0xFFFFFFFF. Modelled as a file-scope const whose
    // address is handed to Wait, exactly as the asm hands &constant.
    const u32 KU_TIMEOUT_NONE = 0xFFFFFFFFu;

    // Host stand-in for the X360 `EA::Thread::Mutex::Lock(critsec, &timeout)`: enter
    // the section and report success (0). The X360 keeps an explicit lock-count int
    // alongside the section; the caller decrements it on leave, so we expose the
    // count separately rather than burying it here.
    inline int EnterUnblockLock(CRITICAL_SECTION* pCS)
    {
        ::EnterCriticalSection(pCS);
        return 0;  // bge -> success
    }
}

// ---------------------------------------------------------------------------
// Condition::Condition @ 0x82B43BB8
//
//   bl EAConditionData::EAConditionData          ; (modelled by the members' init)
//   if (a2 == 0 && (a3 & 0xFF) != 0)             ; no params but bInitialize:
//     v8[0] = 1; v8[1] = 0;                       ; ConditionParameters{intra=1, name=""}
//   bl EA::Thread::Condition::Init(this, params)
//
// The EAConditionData half is the {mnWaitersBlocked, mnWaitersToUnblock,
// mnWaitersGone, two Semaphores, unblock-lock} aggregate; the two embedded
// Semaphores are default-built (no OS object yet) and Init creates them.
// ---------------------------------------------------------------------------
Condition::Condition(const ConditionParameters* pConditionParameters, bool bInitialize)
    : mnWaitersBlocked(0)
    , mnWaitersToUnblock(0)
    , mnWaitersGone(0)
    , mSemaphoreBlockQueue(0, false)  // +0x0C built without an OS object; Init creates it
    , mSemaphoreBlockLock(0, false)   // +0x1C built without an OS object; Init creates it
{
    ConditionParameters lDefault;  // {mbIntraProcess=true, mName=""}
    if (pConditionParameters == 0 && bInitialize)
    {
        lDefault.mbIntraProcess = true;  // v8[0] = 1
        lDefault.mName[0]       = '\0';  // v8[1] = 0
        pConditionParameters    = &lDefault;
    }

    Init(pConditionParameters);
}

// ---------------------------------------------------------------------------
// Condition::~Condition @ 0x82B43288
//
//   if (*(this+0x1C)) CloseHandle(*(this+0x1C))   ; mSemaphoreBlockLock handle
//   if (*(this+0x0C)) CloseHandle(*(this+0x0C))   ; mSemaphoreBlockQueue handle
//
// The two CloseHandle's are exactly the embedded Semaphore destructors (each homed
// dtor CloseHandles its non-null OS handle). The X360 also leaves the unblock-lock
// CRITICAL_SECTION untouched at teardown (no RtlDeleteCriticalSection in this TU).
// ---------------------------------------------------------------------------
Condition::~Condition()
{
    // Members are destroyed in reverse declaration order: mSemaphoreBlockLock
    // (+0x1C) first, then mSemaphoreBlockQueue (+0x0C) -- matching the asm order.
}

// ---------------------------------------------------------------------------
// Condition::Init @ 0x82B432D0
//
//   if (a2 == 0) return 0;
//   name = (a2->mName[0] != 0) ? &a2->mName : 0;   ; lbz 1(a2); addi r30,a2,1
//   intra = a2->mbIntraProcess;                     ; lbz 0(a2)
//   SemaphoreParameters sp0(0, intra, name);        ; block-queue sem
//   SemaphoreParameters sp1(1, intra, name);        ; block-lock sem
//   MutexParameters     mp(intra, name);            ; (built; unused by the section init)
//   if (!Semaphore::Init(this+0xC, &sp0)) return 0;
//   if (!Semaphore::Init(this+0x1C, &sp1)) return 0;
//   *(this+0x50) = 0;                               ; lock-count inside the section
//   RtlInitializeCriticalSection(this+0x30);
//   return 1;
//
// The X360 ConditionParameters layout is {mbIntraProcess@0, mName@1}. The name is
// passed through only when non-empty; the two semaphores seed their OS objects with
// initial counts 0 (queue) and 1 (lock).
// ---------------------------------------------------------------------------
bool Condition::Init(const ConditionParameters* pConditionParameters)
{
    if (pConditionParameters == 0)
        return false;

    const char* pName = (pConditionParameters->mName[0] != '\0')
                            ? pConditionParameters->mName
                            : 0;
    const bool bIntraProcess = pConditionParameters->mbIntraProcess;

    SemaphoreParameters lQueueParams(0, bIntraProcess, pName);  // initial count 0
    SemaphoreParameters lLockParams(1, bIntraProcess, pName);   // initial count 1

    // The X360 also materialises a MutexParameters(intra, name) here (bl
    // MutexParameters::MutexParameters), but the section is created with the
    // parameter-less RtlInitializeCriticalSection below, so the struct is unused.

    if (!mSemaphoreBlockQueue.Init(&lQueueParams))
        return false;
    if (!mSemaphoreBlockLock.Init(&lLockParams))
        return false;

    ::InitializeCriticalSection(&mUnblockLock);  // RtlInitializeCriticalSection(this+0x30)
    return true;
}

// ---------------------------------------------------------------------------
// Condition::Wait @ 0x82B43388
//
// Standard "algorithm 8a" wait. a1=this, a2=pMutex (the external CRITICAL_SECTION
// the caller holds), a3=absolute timeout.
//
//   atomically ++mnWaitersBlocked;                   ; lwarx/stwcx. on this+0
//   release the external mutex (RtlLeaveCriticalSection(pMutex), lock-count--);
//   if (lock-count-after >= 0) {                      ; the external mutex was held
//       r = Semaphore::Wait(mSemaphoreBlockQueue, a3);
//       EnterUnblockLock(this+0x30);
//       if (mnWaitersToUnblock != 0) {
//           --mnWaitersToUnblock;
//       } else if (++mnWaitersGone == 0x3FFFFFFF) {   ; gone counter overflow guard
//           Semaphore::Wait(mSemaphoreBlockLock, kTimeoutNone);
//           mnWaitersBlocked -= mnWaitersGone;          ; atomic adjust
//           Semaphore::Post(mSemaphoreBlockLock, 1);
//           mnWaitersGone = 0;
//       }
//       leave unblock-lock (lock-count--);
//       if (mnWaitersToUnblock-before == 1)            ; we were the last to unblock
//           Semaphore::Post(mSemaphoreBlockLock, 1);
//       re-acquire the external mutex; if that failed return -1, else return r.
//   } else {
//       return lock-count-after;                       ; mutex was not held -> error
//   }
// ---------------------------------------------------------------------------
int Condition::Wait(void* pMutex, const u32* pTimeoutAbsolute)
{
    CRITICAL_SECTION* const lpExternalMutex = static_cast<CRITICAL_SECTION*>(pMutex);

    // Register as a blocked waiter (atomic ++mnWaitersBlocked).
    _InterlockedIncrement(reinterpret_cast<volatile long*>(&mnWaitersBlocked));

    // Release the caller's mutex so other threads can change the guarded state.
    ::LeaveCriticalSection(lpExternalMutex);

    // Block on the queue semaphore until signalled (or the deadline elapses).
    const int iWaitResult =
        mSemaphoreBlockQueue.Wait(pTimeoutAbsolute);

    EnterUnblockLock(&mUnblockLock);

    const s32 iToUnblock = mnWaitersToUnblock;
    if (iToUnblock != 0)
    {
        mnWaitersToUnblock = iToUnblock - 1;
    }
    else
    {
        const s32 iGone = mnWaitersGone + 1;
        mnWaitersGone = iGone;
        if (iGone == 0x3FFFFFFF)  // lis 0x3FFF / ori 0xFFFF (asm @0x82B4342C)
        {
            // mnWaitersGone overflow guard: fold the gone count back into the
            // blocked count under the block-lock, then reset gone to 0.
            mSemaphoreBlockLock.Wait(&KU_TIMEOUT_NONE);
            _InterlockedExchangeAdd(
                reinterpret_cast<volatile long*>(&mnWaitersBlocked),
                -mnWaitersGone);
            mSemaphoreBlockLock.Post(1);
            mnWaitersGone = 0;
        }
    }

    ::LeaveCriticalSection(&mUnblockLock);

    if (iToUnblock == 1)
    {
        // We were the last of a signalled batch: release the block-lock gate.
        mSemaphoreBlockLock.Post(1);
    }

    // Re-acquire the caller's mutex before returning. A failed re-lock maps to
    // kResultError; otherwise return the inner wait result.
    EnterUnblockLock(lpExternalMutex);   // X360 Mutex::Lock(pMutex, kTimeoutNone)
    // EnterCriticalSection cannot fail for an intra-process section, so the X360's
    // "(re-lock == -1) -> return -1" branch is never taken here; return the wait
    // result verbatim.
    return iWaitResult;
}

// ---------------------------------------------------------------------------
// Condition::Signal @ 0x82B434D0
//
//   if (EnterUnblockLock(this+0x30) < 0) return 0;
//   if (mnWaitersToUnblock != 0) {                    ; a prior signal is still draining
//       if (mnWaitersBlocked != 0) {
//           if (bBroadcast) { n = atomic-exchange(mnWaitersBlocked, 0);
//                             mnWaitersToUnblock += n; }
//           else            { ++mnWaitersToUnblock; n = 1; atomic --mnWaitersBlocked; }
//           leave; Semaphore::Post(mSemaphoreBlockQueue, n);
//       } else { leave; }
//       return 1;
//   }
//   if (mnWaitersBlocked <= mnWaitersGone) { leave; return 1; }   ; nothing to wake
//   if (Semaphore::Wait(mSemaphoreBlockLock, kTimeoutNone) == -1) { leave; goto err; }
//   if (mnWaitersGone != 0) { mnWaitersBlocked -= mnWaitersGone; mnWaitersGone = 0; }
//   if (bBroadcast) { n = atomic-exchange(mnWaitersBlocked, 0); mnWaitersToUnblock = n; }
//   else            { mnWaitersToUnblock = 1; n = 1; atomic --mnWaitersBlocked; }
//   leave; Semaphore::Post(mSemaphoreBlockQueue, n);
//   return 1;
// err:
//   return 0;
// ---------------------------------------------------------------------------
int Condition::Signal(bool bBroadcast)
{
    if (EnterUnblockLock(&mUnblockLock) < 0)
        return 0;  // could not take the unblock-lock

    int iPostCount;

    if (mnWaitersToUnblock != 0)
    {
        // A previous signal is still unblocking waiters; just add to the batch.
        if (mnWaitersBlocked != 0)
        {
            if (bBroadcast)
            {
                const long lBlocked = _InterlockedExchange(
                    reinterpret_cast<volatile long*>(&mnWaitersBlocked), 0);
                iPostCount = static_cast<int>(lBlocked);
                mnWaitersToUnblock += static_cast<s32>(lBlocked);
            }
            else
            {
                iPostCount = 1;
                ++mnWaitersToUnblock;
                _InterlockedDecrement(reinterpret_cast<volatile long*>(&mnWaitersBlocked));
            }

            ::LeaveCriticalSection(&mUnblockLock);
            mSemaphoreBlockQueue.Post(iPostCount);
        }
        else
        {
            ::LeaveCriticalSection(&mUnblockLock);
        }
        return 1;
    }

    // No batch in flight. If no one is blocked beyond the already-gone waiters,
    // there is nothing to wake.
    if (mnWaitersBlocked <= mnWaitersGone)
    {
        ::LeaveCriticalSection(&mUnblockLock);
        return 1;
    }

    // Take the block-lock gate. The X360 passes &kTimeoutNone here.
    if (mSemaphoreBlockLock.Wait(&KU_TIMEOUT_NONE) == -1)
    {
        ::LeaveCriticalSection(&mUnblockLock);
        return 0;
    }

    if (mnWaitersGone != 0)
    {
        // Fold any gone waiters out of the blocked count.
        _InterlockedExchangeAdd(
            reinterpret_cast<volatile long*>(&mnWaitersBlocked),
            -mnWaitersGone);
        mnWaitersGone = 0;
    }

    if (bBroadcast)
    {
        const long lBlocked = _InterlockedExchange(
            reinterpret_cast<volatile long*>(&mnWaitersBlocked), 0);
        iPostCount = static_cast<int>(lBlocked);
        mnWaitersToUnblock = static_cast<s32>(lBlocked);
    }
    else
    {
        iPostCount = 1;
        mnWaitersToUnblock = 1;
        _InterlockedDecrement(reinterpret_cast<volatile long*>(&mnWaitersBlocked));
    }

    ::LeaveCriticalSection(&mUnblockLock);
    mSemaphoreBlockQueue.Post(iPostCount);
    return 1;
}

} // namespace Thread
} // namespace EA
