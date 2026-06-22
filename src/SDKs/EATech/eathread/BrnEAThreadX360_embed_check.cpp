// Tiny embed check: proves BrnEAThreadX360.h is self-contained and the X360
// EA::Thread facade + parameter PODs are usable by name with the proven
// layouts/signatures. Compile-only; no linkage.

#include "SDKs/EATech/eathread/BrnEAThreadX360.h"

namespace
{
    void BrnEAThreadX360EmbedCheck()
    {
        using namespace EA::Thread;

        // Parameter PODs construct with their X360 ctor signatures.
        BarrierParameters   lBarrier(4, true, "barrier");
        SemaphoreParameters lSem(1, true, "sem");
        MutexParameters     lMutex(true, "mutex");
        ConditionParameters lCond(true, "cond");

        // Touch the named X360-layout members (no mMaxCount on the semaphore).
        (void)lBarrier.mnHeight;
        (void)lBarrier.mbIntraProcess;
        (void)lSem.mInitialCount;
        (void)lMutex.mName[0];
        (void)lCond.mbIntraProcess;

        // Facade entry points resolve with their X360 signatures.
        ThreadId lId = GetThreadId();
        (void)lId;
        (void)GetThreadPriority();
        (void)SetThreadPriority(0);

        const u32 luZero = 0;
        (void)ThreadSleep(&luZero);

        // Dynamic-data pool API.
        EAThreadDynamicData* lpData = AllocateThreadDynamicData();
        FreeThreadDynamicData(lpData);

        SetCurrentThreadHandle(0, false);
    }
}
