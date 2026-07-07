#include "SDKs/Realmc/RealmcMessageQueue.h"

// ===========================================================================
// RealmcCore::MessageQueue -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Only the three functions that touch the list-head sentinels + the EA::Thread
// primitives are homed here; the five list-NODE functions (GetMessage,
// PostResponse, SendMessage, _ExtractResponse, _WaitForResponse) are BLOCKED
// pending the un-homed RealmcCore::allocator_ node allocate/erase TU -- see the
// header banner for the full rationale.
// ===========================================================================

namespace RealmcCore
{

// ---------------------------------------------------------------------------
// MessageQueue::MessageQueue @ 0x82C47168
//
//   stw 0,0(r31) ; stw 0,4(r31) ; stw r31,0(r31) ; stw r31,4(r31)
//                                              -> mMessageList: next = prev = self
//   stw 0,0(r11) ; stw 0,4(r11) ; stw r11,0(r11) ; stw r11,4(r11)  (r11 = this+0xC)
//                                              -> mResponseList: next = prev = self
//   stb 0,0x18(r31) ; stb 0,0x19(r31)          -> mbInitialized = mbShuttingDown = 0
//   EA::Thread::Mutex::Mutex(this+0x20, 0, 0)  -> mMutex(nullptr, /*default*/false)
//   EA::Thread::Condition::Condition(this+0x50, 0, 0)
//                                              -> mCondition(nullptr, /*init*/false)
//   li r11, 0x1770 ; stw r11, 0xB0(r31)        -> miWaitTimeoutMs = 6000
//
// The two self-referential list stores are RealmcListHead's default ctor (the
// eastl::list empty-init); the compiler's zero-then-self double store collapses
// to the single self-init here. The mutex/condition are built WITHOUT auto-init
// (the (params=null, flag=false) ctor form), matching the two-phase pattern where
// Initialize() Init's them.
// ---------------------------------------------------------------------------
MessageQueue::MessageQueue()
    : mbInitialized(false)
    , mbShuttingDown(false)
    , mMutex(nullptr, false)
    , mCondition(nullptr, false)
    , miWaitTimeoutMs(6000)
{
    // mMessageList / mResponseList self-initialise to the empty (self-referential)
    // state via RealmcListHead's default ctor.
}

// ---------------------------------------------------------------------------
// MessageQueue::Initialize @ 0x82C44C50
//
//   if (!*(this+0x18)) {                        -> if not already initialised
//     if (a2 >= 0) *(this+0xB0) = a2            -> latch caller timeout when valid
//     EA::Thread::ConditionParameters(v4, 1, 0) -> intraProcess=true, name=null
//     EA::Thread::Condition::Init(this+0x50, v4)
//     EA::Thread::MutexParameters(v5, 1, 0)     -> intraProcess=true, name=null
//     EA::Thread::Mutex::Init(this+0x20, v5)
//     *(this+0x18) = 1                          -> mbInitialized = true
//   }
//   return 0;
// ---------------------------------------------------------------------------
int MessageQueue::Initialize(int iTimeoutMs)
{
    if (!mbInitialized)
    {
        if (iTimeoutMs >= 0)
        {
            miWaitTimeoutMs = iTimeoutMs;
        }

        EA::Thread::ConditionParameters lConditionParams(true, nullptr);
        mCondition.Init(&lConditionParams);

        EA::Thread::MutexParameters lMutexParams(true, nullptr);
        mMutex.Init(&lMutexParams);

        mbInitialized = true;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// MessageQueue::ShutDown @ 0x82C45000
//
//   if (*(this+0x18)) {                         -> only if initialised
//     Mutex::Lock(this+0x20, &kTimeoutNone)
//     *(this+0x19) = 1                          -> mbShuttingDown = true
//     Mutex::Unlock(this+0x20)
//     Condition::Signal(this+0x50, 1)           -> broadcast: wake the worker
//     v2 = 10
//     while (*(this+0x18)) {                     -> while still 'initialised'
//       if (v2-- <= 0) break                     -> give up after 10 iterations
//       Mutex::Lock(this+0x20, &kTimeoutNone)
//       *(this+0x18) = (cntlzw(sentinel - responseList.next) & 0x20) == 0
//                                                -> mbInitialized = !responseList.empty
//       Mutex::Unlock(this+0x20)
//       v5 = 10 ; EA::Thread::ThreadSleep(&v5)   -> 10ms yield
//     }
//   }
//   return 0;
//
// The branchless X360 flag recompute is a "is the response list non-empty" test:
// ShutDown keeps mbInitialized set (and keeps sleeping) until the worker has
// drained every queued response, then falls out and returns. The Mutex::Lock
// second argument (&unk_821BA1E4) is the kTimeoutNone ThreadTime reference the
// committed Mutex::Lock default supplies -- reproduced by the no-argument Lock().
// ---------------------------------------------------------------------------
int MessageQueue::ShutDown()
{
    if (mbInitialized)
    {
        mMutex.Lock();                 // Lock(kTimeoutNone)
        mbShuttingDown = true;
        mMutex.Unlock();

        mCondition.Signal(true);       // broadcast

        int iRetries = 10;
        while (mbInitialized)
        {
            if (iRetries-- <= 0)
            {
                break;
            }

            mMutex.Lock();
            mbInitialized = !mResponseList.IsEmpty();
            mMutex.Unlock();

            u32 luSleepMs = 10;
            EA::Thread::ThreadSleep(&luSleepMs);
        }
    }
    return 0;
}

} // namespace RealmcCore
