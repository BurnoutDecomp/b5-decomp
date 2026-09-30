#include <Windows.h>
#undef DrawText
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/AssertSystem/CgsAssertManager.h"
#include "rw/core/debug/DebugCriticalSection.h"   // the canonical wrapper (this TU shares its home)
#include <atomic>
#include <cstring>
#include <exception>
#include "eathread/eathread.h"
#include "eathread/eathread_semaphore.h"

// The assert mutex is a rw::core::debug::detail::DebugCriticalSection (its Enter/Leave wrap a
// Win32 critical section, gated by the "initialised" flag - an un-Created section is a no-op,
// matching the X360 "if (*result)" guard at 0x82BBC4D8/0x82BBC4F0). gAssertMutex stays
// un-Created on the single-threaded boot (Begin/EndAssert become no-ops); the threading core
// Creates it.

namespace CgsDev
{
namespace Assert
{
    static rw::core::debug::detail::DebugCriticalSection gAssertMutex = { 0 };
    namespace
    {
        // FLAG PC-platform leaf: a foreign worker keeps this request on its
        // own stack until the joined frame owner has displayed/logged it.
        struct PendingAssertPC
        {
            PendingAssertPC* mpNext = nullptr;
            AssertData mData;
            SRWLOCK mCompletionLock = SRWLOCK_INIT;
            CONDITION_VARIABLE mCompletion = CONDITION_VARIABLE_INIT;
            bool mbDone = false;
            std::exception_ptr mFailure;
        };
        SRWLOCK gPendingLockPC = SRWLOCK_INIT;
        PendingAssertPC* gpPendingHeadPC = nullptr;
        PendingAssertPC* gpPendingTailPC = nullptr;
        std::atomic<DWORD> guFrameOwnerThreadPC{0};
        bool gbFrameOwnerActivePC = false; // protected by gPendingLockPC
        void* gpFrameOwnerPC = nullptr;   // owner thread only
        FrameFencePC gpFrameFencePC = nullptr;
        thread_local bool gbDispatchThreadPC = false;
        thread_local bool gbDeferredAssertPC = false;
        thread_local unsigned guAssertDepthPC = 0;

        void HandleCapturedPC(PendingAssertPC& lrPending, bool lbAllowDisplay)
        {
            gAssertMutex.Enter();
            try
            {
                gAssertManager.HandleAssertCapturedPC(lrPending.mData.macAssertMessage,
                    lrPending.mData.mpcFile, lrPending.mData.miLine,
                    lrPending.mData.mStack, lbAllowDisplay);
            }
            catch (...)
            {
                lrPending.mFailure = std::current_exception();
            }
            gAssertMutex.Leave();
        }

        void QueueAssertPC(const char* lpcExpression, const char* lpcFile, int liLine)
        {
            PendingAssertPC lPending;
            std::strncpy(lPending.mData.macAssertMessage,
                lpcExpression ? lpcExpression : "<no expression>", KI_MESSAGEBUFFERSIZE - 1);
            lPending.mData.macAssertMessage[KI_MESSAGEBUFFERSIZE - 1] = '\0';
            lPending.mData.mpcFile = lpcFile;
            lPending.mData.miLine = liLine;
            lPending.mData.mpMapReader = nullptr;
            lPending.mData.mStack.Prepare();
            AcquireSRWLockExclusive(&gPendingLockPC);
            const bool lbQueued = gbFrameOwnerActivePC;
            if (lbQueued)
            {
                if (gpPendingTailPC) gpPendingTailPC->mpNext = &lPending;
                else gpPendingHeadPC = &lPending;
                gpPendingTailPC = &lPending;
            }
            ReleaseSRWLockExclusive(&gPendingLockPC);
            if (lbQueued)
            {
                PostThreadMessageW(guFrameOwnerThreadPC.load(std::memory_order_acquire), WM_NULL, 0, 0);
                AcquireSRWLockExclusive(&lPending.mCompletionLock);
                while (!lPending.mbDone)
                    SleepConditionVariableSRW(&lPending.mCompletion,
                        &lPending.mCompletionLock, INFINITE, 0);
                ReleaseSRWLockExclusive(&lPending.mCompletionLock);
            }
            else
            {
                // No frame consumer exists before startup or after shutdown.
                // Preserve the report and failing stack without drawing from
                // a resource/audio worker into a destroyed or unowned device.
                HandleCapturedPC(lPending, false);
            }
            if (lPending.mFailure)
                std::rethrow_exception(lPending.mFailure);
        }
    }

    void AttachFrameOwnerPC(void* lpOwner, FrameFencePC lpFence)
    {
        guFrameOwnerThreadPC.store(GetCurrentThreadId(), std::memory_order_release);
        gpFrameOwnerPC = lpOwner;
        gpFrameFencePC = lpFence;
        AcquireSRWLockExclusive(&gPendingLockPC);
        gbFrameOwnerActivePC = true;
        ReleaseSRWLockExclusive(&gPendingLockPC);
    }

    void DispatchPendingAssertsPC(bool lbAllowDisplay)
    {
        for (;;)
        {
            AcquireSRWLockExclusive(&gPendingLockPC);
            PendingAssertPC* const lpPending = gpPendingHeadPC;
            const bool lbDisplay = lbAllowDisplay && gbFrameOwnerActivePC;
            if (lpPending)
            {
                gpPendingHeadPC = lpPending->mpNext;
                if (!gpPendingHeadPC) gpPendingTailPC = nullptr;
            }
            ReleaseSRWLockExclusive(&gPendingLockPC);
            if (!lpPending) return;
            HandleCapturedPC(*lpPending, lbDisplay);
            AcquireSRWLockExclusive(&lpPending->mCompletionLock);
            lpPending->mbDone = true;
            WakeConditionVariable(&lpPending->mCompletion);
            ReleaseSRWLockExclusive(&lpPending->mCompletionLock);
            // The requester may now have destroyed its stack record.
        }
    }

    // FLAG PC-platform leaf: the frame owner can be waiting for a device/job
    // worker that has just asserted. Release that dependency through the log
    // path; an incomplete frame must not be rendered from this wait site.
    void ServiceWorkerAssertsWhileWaitingPC()
    {
        if (guFrameOwnerThreadPC.load(std::memory_order_acquire) != GetCurrentThreadId())
            return;
        // Dispatch may hold this mutex while its modal draw sends synchronous
        // window messages. Never block the window owner before its message pump.
        if (gAssertMutex.miInitialised && !TryEnterCriticalSection(&gAssertMutex.mCriticalSection))
            return;
        DispatchPendingAssertsPC(false);
        gAssertMutex.Leave();
    }

    int WaitForWorkerPC(EA::Thread::Semaphore& lrCompletion)
    {
        if (guFrameOwnerThreadPC.load(std::memory_order_acquire) != GetCurrentThreadId())
            return lrCompletion.Wait();
        bool lbQuit = false;
        int liQuitCode = 0;
        int liResult;
        while ((liResult = lrCompletion.Wait(EA::Thread::GetThreadTime() + 10))
               == EA::Thread::Semaphore::kResultTimeout)
        {
            ServiceWorkerAssertsWhileWaitingPC();
            // The modal dispatch path may itself need the window owner while
            // this file operation is incomplete. Keep that dependency moving.
            MSG lMessage;
            while (PeekMessageW(&lMessage, nullptr, 0, 0, PM_REMOVE))
            {
                if (lMessage.message == WM_QUIT)
                {
                    lbQuit = true;
                    liQuitCode = static_cast<int>(lMessage.wParam);
                }
                else
                {
                    TranslateMessage(&lMessage);
                    DispatchMessageW(&lMessage);
                }
            }
        }
        if (lbQuit) PostQuitMessage(liQuitCode);
        return liResult;
    }

    void DetachFrameOwnerPC(void* lpOwner)
    {
        if (gpFrameOwnerPC != lpOwner) return;
        AcquireSRWLockExclusive(&gPendingLockPC);
        gbFrameOwnerActivePC = false;
        ReleaseSRWLockExclusive(&gPendingLockPC);
        gpFrameFencePC = nullptr;
        gpFrameOwnerPC = nullptr;
        DispatchPendingAssertsPC();
    }

    void SetDispatchThreadPC(bool lbDispatch) { gbDispatchThreadPC = lbDispatch; }

    // @ 0x82820758 - the once-guarded assert-system bring-up (byte_83019200 latch), called first
    // by DebugManager::Construct: Create gAssertMutex, then initialise the assert-screen state.
    // In this tree the screen-state/vector-font members the X360 body seeds ({-1,0,0} + the
    // {14,14}*0.125 font-size vector at Manager+480) are initialised by Manager's ctor and
    // SetRenderer; Manager::RenderThreadAsserts (the per-thread assert render kick) is the
    // threading follow-on.
    void Construct()
    {
        static bool sbConstructed = false;   // X360 byte_83019200
        if (sbConstructed)
            return;
        sbConstructed = true;

        gAssertMutex.Create();
        guFrameOwnerThreadPC.store(GetCurrentThreadId(), std::memory_order_release);
    }

    // @ 0x82817548 - enter the assert mutex.
    int BeginAssert()
    {
        if (guAssertDepthPC++ == 0)
        {
            const DWORD luOwner = guFrameOwnerThreadPC.load(std::memory_order_acquire);
            gbDeferredAssertPC = luOwner != 0 && luOwner != GetCurrentThreadId()
                && !gbDispatchThreadPC;
            // Join BEFORE taking gAssertMutex: dispatch may itself be asserting.
            // Dispatch owns D3D already and uses an independent modal buffer.
            if (!gbDeferredAssertPC && luOwner == GetCurrentThreadId() && gpFrameFencePC)
                gpFrameFencePC(gpFrameOwnerPC);
        }
        if (!gbDeferredAssertPC) gAssertMutex.Enter();
        return 0;
    }

    // @ 0x82817558 - leave the assert mutex.
    void* EndAssert()
    {
        void* lpResult = gbDeferredAssertPC ? nullptr : gAssertMutex.Leave();
        if (guAssertDepthPC) --guAssertDepthPC;
        // A nonblocking wait-service attempt may have left a worker queued.
        // Wake the owner again now that a modal assertion released its mutex.
        AcquireSRWLockShared(&gPendingLockPC);
        const bool lbPending = gpPendingHeadPC != nullptr;
        ReleaseSRWLockShared(&gPendingLockPC);
        if (lbPending)
            PostThreadMessageW(guFrameOwnerThreadPC.load(std::memory_order_acquire), WM_NULL, 0, 0);
        return lpResult;
    }

    // @ 0x82820810 - report a fired assertion. Native workers capture their
    // own stack before handoff; the owner logs it and can display a modal frame.
    int FireAssert(const char* lpcExpression, const char* lpcFile, int liLine)
    {
        if (gbDeferredAssertPC)
            QueueAssertPC(lpcExpression, lpcFile, liLine);
        else
            gAssertManager.HandleAssert(lpcExpression, lpcFile, liLine);
        // __debugbreak();   // commented out per request - asserts log to file and continue
        return 0;
    }
}
}
