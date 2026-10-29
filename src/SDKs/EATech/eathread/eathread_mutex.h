#ifndef EA_THREAD_EAMUTEX_H
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
