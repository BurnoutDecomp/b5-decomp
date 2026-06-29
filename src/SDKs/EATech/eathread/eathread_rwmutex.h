#ifndef EA_THREAD_EARWMUTEX_H
#define EA_THREAD_EARWMUTEX_H

#include "types.hpp"

#include "SDKs/EATech/eathread/eathread_condition.h"  // EA::Thread::Condition (the two homed conditions)
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"      // EA::Thread::RWMutexParameters / ThreadId / GetThreadId

#include <windows.h>  // CRITICAL_SECTION (the embedded mMutex)

// ===========================================================================
// SDKs/EATech/eathread/eathread_rwmutex.h
//
// EA::Thread::RWMutex -- the X360 reader/writer mutex linked into
// BURNOUT_X360_ARTIST.XEX. Reconstructed from the X360 .XEX (per-addr exports
// under .ida-exports/BURNOUT_X360_ARTIST.XEX/). This header homes:
//   EA::Thread::RWMutex::Init   @ 0x82B43708  (build mutex + two conditions)
//   EA::Thread::RWMutex::Lock   @ 0x82B43778  (acquire read or write lock)
//   EA::Thread::RWMutex::Unlock @ 0x82B438A8  (release, waking waiters)
//
// =====================================================================
// FLAG -- COMMITTED-TYPE / VENDOR-SNAPSHOT DIVERGENCE (do NOT silently swap):
// The X360 binary links the generic two-condition Win32 RWMutex variant, whose
// EARWMutexData is exactly the vendor's generic EARWMutexData
// {int mnReadWaiters; int mnWriteWaiters; int mnReaders; ThreadId mThreadIdWriter;
//  Mutex mMutex; Condition mReadCondition; Condition mWriteCondition;}
// (b5-decomp/vendor/EAThread/include/eathread/eathread_rwmutex.h). The X360 asm is
// authoritative here, so this header reconstructs the X360 shape (with the homed
// X360 Condition and a host CRITICAL_SECTION standing in for the X360 Mutex's
// critical section) rather than #including the divergent vendor header. The
// conductor should decide whether to gate this X360 variant behind a platform
// branch; this file does NOT modify any committed vendor copy.
//
// X360 byte layout proven from the disassembly (RWMutex::Init @0x82B43708):
//   +0x00 mnReadWaiters    (int)       -- waiters wanting a read lock
//   +0x04 mnWriteWaiters   (int)       -- waiters wanting the write lock
//   +0x08 mnReaders        (int)       -- active read-lock count
//   +0x0C mThreadIdWriter  (ThreadId)  -- writer's thread id, 0 when no writer
//   +0x10 mMutex           (CRITICAL_SECTION + lock count) -- guards the above
//                          (RtlInitializeCriticalSection(this+0x10); the X360
//                          `EA::Thread::Mutex::Lock`/`RtlLeaveCriticalSection` calls
//                          are its enter/leave -- host Win32 Enter/LeaveCriticalSection)
//   +0x40 mReadCondition   (Condition) -- Condition::Init(this+0x40)
//   +0xA0 mWriteCondition  (Condition) -- Condition::Init(this+0xA0)
// =====================================================================

namespace EA
{
namespace Thread
{
    class RWMutex
    {
    public:
        // EAThread RWMutex::Result / LockType (vendor eathread_rwmutex.h:78/84,
        // matching the X360 asm: Lock keys on lockType==1 (read) / ==2 (write); the
        // -2 cancellation return is kResultTimeout).
        enum Result
        {
            kResultError   = -1,
            kResultTimeout = -2
        };

        enum LockType
        {
            kLockTypeNone  = 0,
            kLockTypeRead  = 1,
            kLockTypeWrite = 2
        };

        // @ 0x82B43708 -- if pRWMutexParameters is null return false; else zero the
        // counters, RtlInitializeCriticalSection the mutex, and Condition::Init both
        // conditions from a ConditionParameters{intra=params->mbIntraProcess, name=0}.
        // Returns true on success.
        bool Init(const RWMutexParameters* pRWMutexParameters);

        // @ 0x82B43778 -- acquire a read (lockType==kLockTypeRead) or write
        // (kLockTypeWrite) lock, blocking on the matching condition with the absolute
        // deadline pTimeoutAbsolute. Returns the post-acquire reader count (read),
        // 1 (write), or kResultTimeout(-2) if a wait was cancelled.
        int Lock(LockType lockType, const u32* pTimeoutAbsolute);

        // @ 0x82B438A8 -- release the held lock. A writer clears mThreadIdWriter; a
        // reader decrements mnReaders. When the lock is fully released, wake a writer
        // (Signal(false) on mWriteCondition) if any wait, else broadcast the readers
        // (Signal(true) on mReadCondition). Returns the remaining reader count (or 0).
        int Unlock();

    private:
        s32              mnReadWaiters;     // +0x00
        s32              mnWriteWaiters;    // +0x04
        s32              mnReaders;         // +0x08
        ThreadId         mThreadIdWriter;   // +0x0C
        CRITICAL_SECTION mMutex;            // +0x10 (X360: critsec + lockcount)
        Condition        mReadCondition;    // +0x40
        Condition        mWriteCondition;   // +0xA0

        // Non-copyable (embedded Conditions/Semaphores are non-copyable).
        RWMutex(const RWMutex& rhs);
        RWMutex& operator=(const RWMutex& rhs);

    public:
        // Default-constructs the embedded Conditions with no OS objects (Init builds
        // them). The recovered RWMutex TU homes only Init/Lock/Unlock; the ctor body
        // is not in this dossier, so this is the trivial member-wise construction
        // that lets the object be declared. mMutex is left uninitialised until Init.
        RWMutex();
    };
}
}

#endif // EA_THREAD_EARWMUTEX_H
