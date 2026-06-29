// X360-faithful EA::Thread::RWMutex (the generic two-condition Win32 reader/writer
// mutex linked into BURNOUT_X360_ARTIST.XEX). Source of truth: per-addr exports
// under .ida-exports/BURNOUT_X360_ARTIST.XEX/ (X360 asm overrides the divergent
// newer vendor snapshot). See eathread_rwmutex.h for the committed-type /
// vendor-snapshot divergence FLAG.
//
// Vendor EA code reconstructed in its canonical home.

#include "SDKs/EATech/eathread/eathread_rwmutex.h"

#include <windows.h>  // CRITICAL_SECTION, Initialize/Enter/LeaveCriticalSection

namespace EA
{
namespace Thread
{

namespace
{
    // The X360 passes &unk_821473D0 -- the EAThread kTimeoutNone constant -- to the
    // internal Mutex::Lock calls (block forever). On the X360/Win32 the block-forever
    // sentinel is 0xFFFFFFFF (INFINITE). Modelled as a file-scope const, address
    // handed to the lock helper exactly as the asm hands &constant.
    const u32 KU_TIMEOUT_NONE = 0xFFFFFFFFu;

    // Host stand-in for the X360 `EA::Thread::Mutex::Lock(critsec, &timeout)`: enter
    // the guard section. The X360 enter returns >= 0 on success; an intra-process
    // EnterCriticalSection always succeeds, so this returns 0.
    inline int LockGuard(CRITICAL_SECTION* pCS, const u32* /*pTimeoutAbsolute*/)
    {
        ::EnterCriticalSection(pCS);
        return 0;
    }
}

// ---------------------------------------------------------------------------
// RWMutex (default construction)
//
// The recovered RWMutex TU homes only Init/Lock/Unlock. The X360 ctor (RWMutex::
// RWMutex) is not in this dossier; the object is brought to life member-wise here
// with the counters zeroed and the two Conditions default-built without OS objects
// (Init creates them). mMutex's CRITICAL_SECTION is initialised by Init.
// ---------------------------------------------------------------------------
RWMutex::RWMutex()
    : mnReadWaiters(0)
    , mnWriteWaiters(0)
    , mnReaders(0)
    , mThreadIdWriter(0)
    , mReadCondition(0, false)
    , mWriteCondition(0, false)
{
}

// ---------------------------------------------------------------------------
// RWMutex::Init @ 0x82B43708
//
//   if (a2 == 0) return 0;
//   intra = a2->mbIntraProcess;                       ; lbz 0(a2)
//   *(this+0x30) = 0;                                  ; lock-count inside the section
//   RtlInitializeCriticalSection(this+0x10);           ; mMutex
//   ConditionParameters cp{mbIntraProcess=intra, name=0};   ; stb intra; stb 0
//   Condition::Init(this+0x40, &cp);                   ; mReadCondition
//   Condition::Init(this+0xA0, &cp);                   ; mWriteCondition
//   return 1;
//
// The two conditions share one ConditionParameters built from the RWMutex's
// intra-process flag with an empty name (the asm stores 0 into the name byte).
// ---------------------------------------------------------------------------
bool RWMutex::Init(const RWMutexParameters* pRWMutexParameters)
{
    if (pRWMutexParameters == 0)
        return false;

    const bool bIntraProcess = pRWMutexParameters->mbIntraProcess;  // lbz 0(a2)

    ::InitializeCriticalSection(&mMutex);  // RtlInitializeCriticalSection(this+0x10)

    ConditionParameters lConditionParams(bIntraProcess, 0);  // {intra, name=""}
    mReadCondition.Init(&lConditionParams);    // this+0x40
    mWriteCondition.Init(&lConditionParams);   // this+0xA0
    return true;
}

// ---------------------------------------------------------------------------
// RWMutex::Lock @ 0x82B43778
//
//   Mutex::Lock(&mMutex, kTimeoutNone);                 ; guard
//   result = 0;
//   if (lockType == kLockTypeRead) {
//       // A reader may proceed only when there is no active writer. If a writer
//       // holds the lock, wait on mReadCondition (counting as a read waiter).
//       while (mThreadIdWriter != 0) {
//           ++mnReadWaiters;
//           int r = mReadCondition.Wait(&mMutex, timeout);
//           --mnReadWaiters;
//           if (r == kResultTimeout) -> cancelled, return -2;
//       }
//       result = ++mnReaders;
//   } else if (lockType == kLockTypeWrite) {
//       // A writer needs zero readers and no other writer.
//       while (mnReaders > 0 || mThreadIdWriter != 0) {
//           ++mnWriteWaiters;
//           int r = mWriteCondition.Wait(&mMutex, timeout);
//           --mnWriteWaiters;
//           if (r == kResultTimeout) -> cancelled, return -2;
//       }
//       result = 1;
//       mThreadIdWriter = GetThreadId();
//   }
//   leave mMutex;
//   return result;
// ---------------------------------------------------------------------------
int RWMutex::Lock(LockType lockType, const u32* pTimeoutAbsolute)
{
    LockGuard(&mMutex, &KU_TIMEOUT_NONE);  // EA::Thread::Mutex::Lock(this+0x10, kTimeoutNone)

    int iResult = 0;

    if (lockType == kLockTypeRead)
    {
        // Reader: block while a writer holds the lock.
        while (mThreadIdWriter != 0)
        {
            ++mnReadWaiters;
            const int iWait = mReadCondition.Wait(&mMutex, pTimeoutAbsolute);
            --mnReadWaiters;
            if (iWait == kResultTimeout)  // -2: cancelled
            {
                LeaveCriticalSection(&mMutex);
                return kResultTimeout;
            }
        }
        iResult = ++mnReaders;
    }
    else if (lockType == kLockTypeWrite)
    {
        // Writer: block while any reader is active or another writer holds it.
        while (mnReaders > 0 || mThreadIdWriter != 0)
        {
            ++mnWriteWaiters;
            const int iWait = mWriteCondition.Wait(&mMutex, pTimeoutAbsolute);
            --mnWriteWaiters;
            if (iWait == kResultTimeout)  // -2: cancelled
            {
                LeaveCriticalSection(&mMutex);
                return kResultTimeout;
            }
        }
        iResult = 1;
        mThreadIdWriter = GetThreadId();  // EA::Thread::GetThreadId()
    }

    LeaveCriticalSection(&mMutex);
    return iResult;
}

// ---------------------------------------------------------------------------
// RWMutex::Unlock @ 0x82B438A8
//
//   Mutex::Lock(&mMutex, kTimeoutNone);                 ; guard
//   if (mThreadIdWriter != 0) {                          ; we hold the write lock
//       mThreadIdWriter = 0;
//       // fall through to the wake logic with the lock fully released
//   } else {
//       result = --mnReaders;                            ; drop one reader
//       if (result > 0) { leave; return result; }        ; readers still hold it
//   }
//   // lock fully released -> wake one writer (priority) or all readers
//   if (mnWriteWaiters > 0)      mWriteCondition.Signal(false);
//   else if (mnReadWaiters > 0)  mReadCondition.Signal(true);
//   leave;
//   return 0;
// ---------------------------------------------------------------------------
int RWMutex::Unlock()
{
    LockGuard(&mMutex, &KU_TIMEOUT_NONE);  // EA::Thread::Mutex::Lock(this+0x10, kTimeoutNone)

    if (mThreadIdWriter != 0)
    {
        // Releasing the write lock.
        mThreadIdWriter = 0;
    }
    else
    {
        // Releasing one read lock; if readers remain, no one needs waking.
        const s32 iRemaining = --mnReaders;
        if (iRemaining > 0)
        {
            LeaveCriticalSection(&mMutex);
            return iRemaining;
        }
    }

    // Lock fully released: prefer waking a pending writer, otherwise broadcast to
    // all pending readers.
    if (mnWriteWaiters > 0)
    {
        mWriteCondition.Signal(false);  // Signal(v4=this+0xA0, v3=0)
    }
    else if (mnReadWaiters > 0)
    {
        mReadCondition.Signal(true);    // Signal(v4=this+0x40, v3=1)
    }

    LeaveCriticalSection(&mMutex);
    return 0;
}

} // namespace Thread
} // namespace EA
