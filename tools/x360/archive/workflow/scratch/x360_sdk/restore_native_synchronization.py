"""One-time recovery of the native Mutex/Condition/RWMutex objects."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2] / 'b5-decomp'
BASE = 'src/SDKs/EATech/eathread/'


def write(name, text):
    (ROOT / name).write_bytes(text.replace('\r\n', '\n').replace('\n', '\r\n').encode('utf-8'))


write(BASE+'eathread_mutex.h', '''#ifndef EA_THREAD_EAMUTEX_H
#define EA_THREAD_EAMUTEX_H

#include "types.hpp"
#include "SDKs/EATech/eathread/eathread.h"

// ARTIST Mutex::Mutex82B43690 stores four 64-bit platform words, then the
// recursion count at20, intra-process flag24 and debug owner28. The native
// critical section occupies the first28 bytes of the32-byte platform storage.
struct EAMutexData
{
    union
    {
        u64 mData[4];
        CRITICAL_SECTION mMutex;
    };
    int mnLockCount;
    bool mbIntraProcess;
    EA::Thread::ThreadId mThreadId;

    EAMutexData();
};

CGS_STATIC_ASSERT((sizeof(CRITICAL_SECTION) == 0x1C), "XDK critical section");
CGS_STATIC_ASSERT((sizeof(EAMutexData) == 0x30), "ARTIST mutex size");
CGS_STATIC_ASSERT((offsetof(EAMutexData, mnLockCount) == 0x20), "ARTIST mutex count");
CGS_STATIC_ASSERT((offsetof(EAMutexData, mbIntraProcess) == 0x24), "ARTIST mutex flag");
CGS_STATIC_ASSERT((offsetof(EAMutexData, mThreadId) == 0x28), "ARTIST mutex owner");

namespace EA
{
namespace Thread
{
    class Mutex
    {
    public:
        enum Result { kResultError = -1, kResultTimeout = -2 };

        Mutex(const MutexParameters* pMutexParameters = 0, bool bDefaultParameters = true);
        ~Mutex();
        bool Init(const MutexParameters* pMutexParameters);
        int Lock(const ThreadTime& timeoutAbsolute = kTimeoutNone);
        int Unlock();
        int GetLockCount() const;
        bool HasLock() const;
        void* GetPlatformData() { return &mMutexData; }

    protected:
        EAMutexData mMutexData;

    private:
        Mutex(const Mutex&);
        Mutex& operator=(const Mutex&);
    };

    CGS_STATIC_ASSERT((sizeof(Mutex) == 0x30), "ARTIST Mutex wrapper");

    class AutoMutex
    {
    public:
        AutoMutex(Mutex& mutex) : mMutex(mutex) { const_cast<Mutex&>(mMutex).Lock(); }
        ~AutoMutex() { const_cast<Mutex&>(mMutex).Unlock(); }

    protected:
        const Mutex& mMutex;

    private:
        AutoMutex(const AutoMutex&);
        const AutoMutex& operator=(const AutoMutex&);
    };
}
}

#endif
''')

write(BASE+'eathread_mutex.cpp', '''#include "SDKs/EATech/eathread/eathread_mutex.h"

// Data initialization inlined in ARTIST82B43690.
EAMutexData::EAMutexData()
    : mnLockCount(0), mbIntraProcess(true), mThreadId(0)
{
    mData[0] = mData[1] = mData[2] = mData[3] = 0;
}

namespace EA
{
namespace Thread
{
    // ARTIST82B43690: Init ignores the parameter contents on this platform.
    Mutex::Mutex(const MutexParameters* pMutexParameters, bool bDefaultParameters)
    {
        if (pMutexParameters || bDefaultParameters)
        {
            mMutexData.mnLockCount = 0;
            ::InitializeCriticalSection(&mMutexData.mMutex);
        }
    }

    Mutex::~Mutex()
    {
        // RtlDeleteCriticalSection is a no-op in the native XDK.
        ::DeleteCriticalSection(&mMutexData.mMutex);
    }

    // ARTIST82B42770.
    bool Mutex::Init(const MutexParameters* pMutexParameters)
    {
        if (!pMutexParameters)
            return false;
        mMutexData.mnLockCount = 0;
        ::InitializeCriticalSection(&mMutexData.mMutex);
        return true;
    }

    // ARTIST82B427B0: unsigned deadline comparison after a failed try-enter.
    int Mutex::Lock(const ThreadTime& timeoutAbsolute)
    {
        if (timeoutAbsolute == kTimeoutNone)
            ::EnterCriticalSection(&mMutexData.mMutex);
        else
        {
            while (!::TryEnterCriticalSection(&mMutexData.mMutex))
            {
                if (::GetTickCount() + 0x2710u >= timeoutAbsolute)
                    return kResultTimeout;
                ::Sleep(1);
            }
        }
        return ++mMutexData.mnLockCount;
    }

    // ARTIST82B42838: save the decremented count before leaving the section.
    int Mutex::Unlock()
    {
        const int nCount = --mMutexData.mnLockCount;
        ::LeaveCriticalSection(&mMutexData.mMutex);
        return nCount;
    }
}
}
''')

write(BASE+'eathread_condition.h', '''#ifndef EA_THREAD_EACONDITION_H
#define EA_THREAD_EACONDITION_H

#include "types.hpp"
#include "SDKs/EATech/eathread/eathread_semaphore.h"
#include "SDKs/EATech/eathread/eathread_mutex.h"

namespace EA
{
namespace Thread
{
    // ARTIST82B43B38/82B43BB8: two semaphores and a complete recursive Mutex.
    class Condition
    {
    public:
        enum Result { kResultOK = 0, kResultError = -1, kResultTimeout = -2 };
        Condition(const ConditionParameters* pConditionParameters = 0, bool bInitialize = true);
        ~Condition();
        bool Init(const ConditionParameters* pConditionParameters);
        int Wait(Mutex* pMutex, const u32* pTimeoutAbsolute);
        int Signal(bool bBroadcast = false);

    private:
        s32 mnWaitersBlocked;
        s32 mnWaitersToUnblock;
        s32 mnWaitersGone;
        Semaphore mSemaphoreBlockQueue;
        Semaphore mSemaphoreBlockLock;
        Mutex mUnblockLock;

        void CheckLayout()
        {
            CGS_STATIC_ASSERT((offsetof(Condition, mSemaphoreBlockQueue) == 0xC), "ARTIST queue semaphore");
            CGS_STATIC_ASSERT((offsetof(Condition, mSemaphoreBlockLock) == 0x1C), "ARTIST block semaphore");
            CGS_STATIC_ASSERT((offsetof(Condition, mUnblockLock) == 0x30), "ARTIST condition mutex");
        }

        Condition(const Condition&);
        Condition& operator=(const Condition&);
    };

    CGS_STATIC_ASSERT((sizeof(Condition) == 0x60), "ARTIST Condition size");
}
}

#endif
''')

write(BASE+'eathread_condition.cpp', '''#include "SDKs/EATech/eathread/eathread_condition.h"

namespace EA
{
namespace Thread
{
    // ARTIST82B43BB8, with EAConditionData initialization inlined.
    Condition::Condition(const ConditionParameters* pConditionParameters, bool bInitialize)
        : mnWaitersBlocked(0), mnWaitersToUnblock(0), mnWaitersGone(0)
        , mSemaphoreBlockQueue(0, false), mSemaphoreBlockLock(0, false)
        , mUnblockLock(0, false)
    {
        ConditionParameters lDefault;
        if (!pConditionParameters && bInitialize)
            pConditionParameters = &lDefault;
        Init(pConditionParameters);
    }

    // ARTIST82B43288: member destruction closes the block-lock semaphore first.
    Condition::~Condition()
    {
    }

    // ARTIST82B432D0.
    bool Condition::Init(const ConditionParameters* pConditionParameters)
    {
        if (!pConditionParameters)
            return false;
        const char* pName = pConditionParameters->mName[0] ? pConditionParameters->mName : 0;
        const bool bIntraProcess = pConditionParameters->mbIntraProcess;
        SemaphoreParameters lQueueParams(0, bIntraProcess, pName);
        SemaphoreParameters lLockParams(1, bIntraProcess, pName);
        MutexParameters lMutexParams(bIntraProcess, pName);
        if (!mSemaphoreBlockQueue.Init(&lQueueParams))
            return false;
        if (!mSemaphoreBlockLock.Init(&lLockParams))
            return false;
        return mUnblockLock.Init(&lMutexParams);
    }

    // ARTIST82B43388. Unlock returns its saved recursion count; a negative
    // count exits immediately after registering the waiter, as in the binary.
    int Condition::Wait(Mutex* pMutex, const u32* pTimeoutAbsolute)
    {
        _InterlockedIncrement(reinterpret_cast<volatile long*>(&mnWaitersBlocked));
        const int nUnlock = pMutex->Unlock();
        if (nUnlock < 0)
            return nUnlock;

        const int nWaitResult = mSemaphoreBlockQueue.Wait(pTimeoutAbsolute);
        mUnblockLock.Lock();
        const s32 nToUnblock = mnWaitersToUnblock;
        if (nToUnblock)
            mnWaitersToUnblock = nToUnblock - 1;
        else if (++mnWaitersGone == 0x3FFFFFFF)
        {
            mSemaphoreBlockLock.Wait(&kTimeoutNone);
            _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&mnWaitersBlocked), -mnWaitersGone);
            mSemaphoreBlockLock.Post(1);
            mnWaitersGone = 0;
        }
        mUnblockLock.Unlock();
        if (nToUnblock == 1)
            mSemaphoreBlockLock.Post(1);
        if (pMutex->Lock() == Mutex::kResultError)
            return kResultError;
        return nWaitResult;
    }

    // ARTIST82B434D0: the semaphore gate's -1 result alone aborts a signal.
    int Condition::Signal(bool bBroadcast)
    {
        if (mUnblockLock.Lock() < 0)
            return 0;
        int nPostCount;
        if (mnWaitersToUnblock)
        {
            if (!mnWaitersBlocked)
            {
                mUnblockLock.Unlock();
                return 1;
            }
            if (bBroadcast)
            {
                nPostCount = _InterlockedExchange(reinterpret_cast<volatile long*>(&mnWaitersBlocked), 0);
                mnWaitersToUnblock += nPostCount;
            }
            else
            {
                nPostCount = 1;
                ++mnWaitersToUnblock;
                _InterlockedDecrement(reinterpret_cast<volatile long*>(&mnWaitersBlocked));
            }
        }
        else
        {
            if (mnWaitersBlocked <= mnWaitersGone)
            {
                mUnblockLock.Unlock();
                return 1;
            }
            if (mSemaphoreBlockLock.Wait(&kTimeoutNone) == -1)
            {
                mUnblockLock.Unlock();
                return 0;
            }
            if (mnWaitersGone)
            {
                _InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&mnWaitersBlocked), -mnWaitersGone);
                mnWaitersGone = 0;
            }
            if (bBroadcast)
            {
                nPostCount = _InterlockedExchange(reinterpret_cast<volatile long*>(&mnWaitersBlocked), 0);
                mnWaitersToUnblock = nPostCount;
            }
            else
            {
                nPostCount = mnWaitersToUnblock = 1;
                _InterlockedDecrement(reinterpret_cast<volatile long*>(&mnWaitersBlocked));
            }
        }
        mUnblockLock.Unlock();
        mSemaphoreBlockQueue.Post(nPostCount);
        return 1;
    }
}
}
''')

write(BASE+'eathread_rwmutex_data.h', '''#ifndef EA_THREAD_EARWMUTEXDATA_H
#define EA_THREAD_EARWMUTEXDATA_H

#include "SDKs/EATech/eathread/eathread_condition.h"

// ARTIST EARWMutexData::EARWMutexData82B43C18.
struct EARWMutexData
{
    s32 mnReadWaiters;
    s32 mnWriteWaiters;
    s32 mnReaders;
    EA::Thread::ThreadId mThreadIdWriter;
    EA::Thread::Mutex mMutex;
    EA::Thread::Condition mReadCondition;
    EA::Thread::Condition mWriteCondition;

    EARWMutexData();

private:
    EARWMutexData(const EARWMutexData&);
    EARWMutexData& operator=(const EARWMutexData&);
};

CGS_STATIC_ASSERT((offsetof(EARWMutexData, mMutex) == 0x10), "ARTIST RW mutex");
CGS_STATIC_ASSERT((offsetof(EARWMutexData, mReadCondition) == 0x40), "ARTIST reader condition");
CGS_STATIC_ASSERT((offsetof(EARWMutexData, mWriteCondition) == 0xA0), "ARTIST writer condition");
CGS_STATIC_ASSERT((sizeof(EARWMutexData) == 0x100), "ARTIST RW data size");

#endif
''')

write(BASE+'eathread_rwmutex_data.cpp', '''#include "SDKs/EATech/eathread/eathread_rwmutex_data.h"

// ARTIST82B43C18: these objects initialize even when the enclosing RWMutex
// defers its second initialization pass.
EARWMutexData::EARWMutexData()
    : mnReadWaiters(0), mnWriteWaiters(0), mnReaders(0), mThreadIdWriter(0)
    , mMutex(0, true), mReadCondition(), mWriteCondition()
{
}
''')

write(BASE+'eathread_rwmutex.h', '''#ifndef EA_THREAD_EARWMUTEX_H
#define EA_THREAD_EARWMUTEX_H

#include "SDKs/EATech/eathread/eathread_rwmutex_data.h"

namespace EA
{
namespace Thread
{
    class RWMutex
    {
    public:
        enum Result { kResultError = -1, kResultTimeout = -2 };
        enum LockType { kLockTypeNone = 0, kLockTypeRead = 1, kLockTypeWrite = 2 };

        RWMutex(const RWMutexParameters* pRWMutexParameters = 0, bool bDefaultParameters = true);
        bool Init(const RWMutexParameters* pRWMutexParameters);
        int Lock(LockType lockType, const u32* pTimeoutAbsolute = &kTimeoutNone);
        int Unlock();
        int GetLockCount(LockType lockType) const
        {
            if (lockType == kLockTypeRead) return mRWMutexData.mnReaders;
            if (lockType == kLockTypeWrite) return mRWMutexData.mThreadIdWriter != 0 ? 1 : 0;
            return 0;
        }

    private:
        EARWMutexData mRWMutexData;
        RWMutex(const RWMutex&);
        RWMutex& operator=(const RWMutex&);
    };

    CGS_STATIC_ASSERT((sizeof(RWMutex) == 0x100), "ARTIST RWMutex size");
}
}

#endif
''')

write(BASE+'eathread_rwmutex.cpp', '''#include "SDKs/EATech/eathread/eathread_rwmutex.h"

namespace EA
{
namespace Thread
{
    // ARTIST82B43CA0: data construction precedes the optional Init call.
    RWMutex::RWMutex(const RWMutexParameters* pRWMutexParameters, bool bDefaultParameters)
    {
        RWMutexParameters lDefault;
        if (!pRWMutexParameters && bDefaultParameters)
            pRWMutexParameters = &lDefault;
        Init(pRWMutexParameters);
    }

    // ARTIST82B43708: initialization preserves the four bookkeeping fields.
    bool RWMutex::Init(const RWMutexParameters* pRWMutexParameters)
    {
        if (!pRWMutexParameters)
            return false;
        MutexParameters lMutexParams(pRWMutexParameters->mbIntraProcess, 0);
        mRWMutexData.mMutex.Init(&lMutexParams);
        ConditionParameters lConditionParams(pRWMutexParameters->mbIntraProcess, 0);
        mRWMutexData.mReadCondition.Init(&lConditionParams);
        mRWMutexData.mWriteCondition.Init(&lConditionParams);
        return true;
    }

    // ARTIST82B43778.
    int RWMutex::Lock(LockType lockType, const u32* pTimeoutAbsolute)
    {
        EARWMutexData& d = mRWMutexData;
        d.mMutex.Lock();
        int nResult = 0;
        if (lockType == kLockTypeRead)
        {
            while (d.mThreadIdWriter)
            {
                ++d.mnReadWaiters;
                const int nWait = d.mReadCondition.Wait(&d.mMutex, pTimeoutAbsolute);
                --d.mnReadWaiters;
                if (nWait == kResultTimeout)
                {
                    d.mMutex.Unlock();
                    return kResultTimeout;
                }
            }
            nResult = ++d.mnReaders;
        }
        else if (lockType == kLockTypeWrite)
        {
            while (d.mnReaders > 0 || d.mThreadIdWriter)
            {
                ++d.mnWriteWaiters;
                const int nWait = d.mWriteCondition.Wait(&d.mMutex, pTimeoutAbsolute);
                --d.mnWriteWaiters;
                if (nWait == kResultTimeout)
                {
                    d.mMutex.Unlock();
                    return kResultTimeout;
                }
            }
            nResult = 1;
            d.mThreadIdWriter = GetThreadId();
        }
        d.mMutex.Unlock();
        return nResult;
    }

    // ARTIST82B438A8: writers have priority when the last reader releases.
    int RWMutex::Unlock()
    {
        EARWMutexData& d = mRWMutexData;
        d.mMutex.Lock();
        if (d.mThreadIdWriter)
            d.mThreadIdWriter = 0;
        else
        {
            const s32 nRemaining = --d.mnReaders;
            if (nRemaining > 0)
            {
                d.mMutex.Unlock();
                return nRemaining;
            }
        }
        if (d.mnWriteWaiters > 0)
            d.mWriteCondition.Signal(false);
        else if (d.mnReadWaiters > 0)
            d.mReadCondition.Signal(true);
        d.mMutex.Unlock();
        return 0;
    }
}
}
''')

q = ROOT/'src/SDKs/Realmc/RealmcMessageQueue.h'
s = q.read_text(encoding='utf-8')
s = s.replace('//                            set when a response wait times out)', '//                            set when a response wait returns an error)')
a = s.index('// The mutex and condition are the linked EAThread ones')
b = s.index('// ===========================================================================', a)
s = s[:a]+'// Native Mutex/Condition layouts are checked by their headers.\n'+s[b:]
s = s.replace('or a wait times\n    // out;', 'or a wait returns\n    // an error;')
s = s.replace('(a timed-out wait marks the queue shutting down).', '(a failed wait marks the queue shutting down).')
needle = '    // Non-copyable (embedded Mutex/Condition are non-copyable).'
s = s.replace(needle, '''    void CheckLayout()
    {
        CGS_STATIC_ASSERT((offsetof(MessageQueue, mMutex) == 0x20), "ARTIST queue mutex");
        CGS_STATIC_ASSERT((offsetof(MessageQueue, mCondition) == 0x50), "ARTIST queue condition");
        CGS_STATIC_ASSERT((offsetof(MessageQueue, miWaitTimeoutMs) == 0xB0), "ARTIST queue timeout");
    }

'''+needle)
write(str(q.relative_to(ROOT)), s)

p = ROOT/'src/SDKs/EATech/rwcore/filesys/device.cpp'
s = p.read_text(encoding='utf-8').replace('mWork.Wait(&lpThis->mLock, EA::Thread::kTimeoutNone)',
                                        'mWork.Wait(&lpThis->mLock, &EA::Thread::kTimeoutNone)')
write(str(p.relative_to(ROOT)), s)
