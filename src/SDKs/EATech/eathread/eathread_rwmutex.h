#ifndef EA_THREAD_EARWMUTEX_H
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
