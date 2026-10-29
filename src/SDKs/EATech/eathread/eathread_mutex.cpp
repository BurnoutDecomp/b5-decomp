#include "SDKs/EATech/eathread/eathread_mutex.h"

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
        DeleteCriticalSection(&mMutexData.mMutex);
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
