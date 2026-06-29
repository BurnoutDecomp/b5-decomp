#ifndef EA_THREAD_EACONDITION_H
#define EA_THREAD_EACONDITION_H

#include "types.hpp"

#include "SDKs/EATech/eathread/eathread_semaphore.h"   // EA::Thread::Semaphore (the homed Win32 variant)
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"       // EA::Thread::ConditionParameters / SemaphoreParameters

// On the MSVC/Windows host the X360 kernel critical-section primitive maps to a
// Win32 CRITICAL_SECTION; pull it in so the embedded mUnblockLock has its real host
// layout (the X360 lays a 28-byte CRITICAL_SECTION + a lock-count int into +0x30).
#include <windows.h>  // CRITICAL_SECTION

// ===========================================================================
// SDKs/EATech/eathread/eathread_condition.h
//
// EA::Thread::Condition -- the X360 condition-variable wrapper linked into
// BURNOUT_X360_ARTIST.XEX. Reconstructed from the X360 .XEX (per-addr exports
// under .ida-exports/BURNOUT_X360_ARTIST.XEX/). This header homes:
//   EA::Thread::Condition::Condition  @ 0x82B43BB8  (ctor)
//   EA::Thread::Condition::~Condition @ 0x82B43288  (dtor: CloseHandle both sems)
//   EA::Thread::Condition::Init       @ 0x82B432D0  (build the two sems + lock)
//   EA::Thread::Condition::Wait       @ 0x82B43388  (wait, releasing/re-taking mutex)
//   EA::Thread::Condition::Signal     @ 0x82B434D0  (wake one or all waiters)
//
// =====================================================================
// FLAG -- COMMITTED-TYPE / VENDOR-SNAPSHOT DIVERGENCE (do NOT silently swap):
// b5-decomp/vendor/EAThread is a NEWER upstream EAThread snapshot. The X360 binary
// links the generic "algorithm 8a" Win32 condition variant (two semaphores + an
// unblock-lock critical section), whose EAConditionData is exactly the vendor's
// generic non-Posix EAConditionData {AtomicInt32 mnWaitersBlocked; int
// mnWaitersToUnblock; int mnWaitersDone; ...} (vendor eathread_condition.h, the
// "#else" branch). The X360 asm is authoritative here, so this header reconstructs
// the X360 shape rather than #including the divergent vendor header. The conductor
// should decide whether to gate this X360 variant behind a platform branch in the
// vendor tree; this file does NOT modify any committed vendor copy.
//
// Concrete X360 facts proven from the disassembly:
//   * +0x00 mnWaitersBlocked   : AtomicInt32 -- masked-interrupt lwarx/stwcx. inc on
//                                 Wait entry, atomic dec/adjust on Signal.
//   * +0x04 mnWaitersToUnblock : int -- pending wake count.
//   * +0x08 mnWaitersGone      : int -- capped at 0x3FFFFFFF (asm @0x82B4342C builds
//                                 the lis 0x3FFF / ori 0xFFFF constant and compares).
//   * +0x0C mSemaphoreBlockQueue : EA::Thread::Semaphore -- the block-queue sem
//                                   (SemaphoreParameters(0, intra, name); Init r3=this+0xC).
//   * +0x1C mSemaphoreBlockLock  : EA::Thread::Semaphore -- the block-lock sem
//                                   (SemaphoreParameters(1, intra, name); Init r3=this+0x1C).
//   * +0x30 mUnblockLock          : CRITICAL_SECTION + lock count -- RtlInitialize-
//                                    CriticalSection(this+0x30); the X360 `EA::Thread::
//                                    Mutex::Lock`/`RtlLeaveCriticalSection` calls are the
//                                    enter/leave of this critical section (host: Win32
//                                    Enter/LeaveCriticalSection) and the X360 keeps an
//                                    explicit lock-count int at +0x20 within the block.
// =====================================================================
//
// The Wait/Signal int returns are the EAThread Condition::Result sentinels (vendor
// eathread_condition.h:130): kResultOK(0)/kResultError(-1)/kResultTimeout(-2). They
// are reproduced verbatim from the asm (Wait returns -1 on re-lock failure, the
// inner Semaphore::Wait result otherwise; Signal returns 1/0).

namespace EA
{
namespace Thread
{
    class Condition
    {
    public:
        // EAThread Condition::Result (vendor eathread_condition.h). The X360 Wait
        // leaves these in r3 alongside the inner Semaphore::Wait result.
        enum Result
        {
            kResultOK      =  0,
            kResultError   = -1,
            kResultTimeout = -2
        };

        // @ 0x82B43BB8 -- construct the EAConditionData half, then Init. When
        // pConditionParameters is null but bInitialize is true, Init is driven from
        // a default-constructed ConditionParameters{intraProcess=true, name=""} built
        // on the stack (the asm sets v8[0]=1, v8[1]=0).
        Condition(const ConditionParameters* pConditionParameters = 0, bool bInitialize = true);

        // @ 0x82B43288 -- CloseHandle mSemaphoreBlockLock's OS handle (+0x1C) then
        // mSemaphoreBlockQueue's (+0x0C), each only if non-null. (Modelled via the
        // embedded Semaphore destructors, whose homed dtor performs exactly that
        // CloseHandle.)
        ~Condition();

        // @ 0x82B432D0 -- build SemaphoreParameters(0, intra, name) and (1, intra,
        // name), Init the two embedded semaphores, then RtlInitializeCriticalSection
        // the unblock-lock. Returns true on success, false if pConditionParameters is
        // null or either Semaphore::Init fails.
        bool Init(const ConditionParameters* pConditionParameters);

        // @ 0x82B43388 -- atomically register as a blocked waiter, release the
        // caller-supplied mutex critical section, block on the block-queue semaphore
        // with the absolute deadline pTimeoutAbsolute, then unwind the unblock
        // bookkeeping and re-acquire the mutex. pMutex points at the external mutex's
        // CRITICAL_SECTION (the RWMutex's mMutex). Returns kResultOK / kResultError /
        // kResultTimeout (or the inner Semaphore::Wait count).
        int Wait(void* pMutex, const u32* pTimeoutAbsolute);

        // @ 0x82B434D0 -- wake one waiter (bBroadcast false) or all (bBroadcast true).
        // Returns 1 on success, 0 if the unblock-lock could not be taken.
        int Signal(bool bBroadcast = false);

    private:
        s32              mnWaitersBlocked;       // +0x00 (AtomicInt32)
        s32              mnWaitersToUnblock;      // +0x04
        s32              mnWaitersGone;           // +0x08
        Semaphore        mSemaphoreBlockQueue;    // +0x0C
        Semaphore        mSemaphoreBlockLock;     // +0x1C
        CRITICAL_SECTION mUnblockLock;            // +0x30 (X360: 28B critsec + lockcount)

        // Non-copyable (embedded Semaphores are non-copyable) -- declared, not defined.
        Condition(const Condition& rhs);
        Condition& operator=(const Condition& rhs);
    };
}
}

#endif // EA_THREAD_EACONDITION_H
