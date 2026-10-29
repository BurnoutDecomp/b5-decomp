#include "SDKs/EATech/eathread/eathread_rwmutex.h"

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
