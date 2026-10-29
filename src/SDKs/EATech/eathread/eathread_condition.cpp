#include "SDKs/EATech/eathread/eathread_condition.h"

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
