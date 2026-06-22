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

        // EA::Thread::Thread object API (this TU): construct/destruct and touch
        // each recovered method by its X360 signature.
        Thread::SetGlobalRunnableFunctionUserWrapper(0);
    }

    // intptr_t (*)(void*) runnable used to exercise Begin's signature.
    intptr_t BrnEAThreadX360EmbedRunnable(void* /*pContext*/) { return 0; }

    void BrnEAThreadX360ThreadObjectEmbedCheck()
    {
        using namespace EA::Thread;

        Thread lThread; // ctor is implicit/trivial; dtor releases the record.

        ThreadParameters lParams;
        lParams.mnStackSize = 0;
        lParams.mnPriority  = 0;
        lParams.mnProcessor = -1;
        lParams.mpName      = "worker";

        ThreadId lTid = lThread.Begin(&BrnEAThreadX360EmbedRunnable, 0, &lParams, 0);
        (void)lTid;

        intptr_t lReturn = 0;
        Thread::Status lStatus = lThread.GetStatus(&lReturn);
        (void)lStatus;
        (void)lReturn;

        (void)lThread.GetId();
        (void)lThread.GetPriority();
        lThread.SetName("worker");
    }
}
