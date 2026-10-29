#ifndef EA_THREAD_EACONDITION_H
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
