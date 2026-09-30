// CgsSystem::ThreadLayout - secondary-thread / barrier handshake owner. See CgsThreadLayout.h.
//
// ARTIST InitThreads 0x828E1658, Update 0x828E17D0 and dispatch worker
// 0x828D7988 define the two-barrier frame handshake. Native window messages
// and worker shutdown are adapted explicitly below.

#include <Windows.h>
#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <system_error>

#include "eathread/eathread.h"   // EA::Thread::GetThreadId / ThreadSleep / ThreadId

namespace
{
    // FLAG PC-platform leaf: preserve the caller's quit notification while
    // servicing messages required by another thread's synchronous window calls.
    void WaitForWindowThreadHandle(HANDLE lhHandle)
    {
        bool lbQuit = false;
        int liQuitCode = 0;
        for (;;)
        {
            const DWORD luWait = MsgWaitForMultipleObjectsEx(1, &lhHandle, INFINITE,
                QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            if (luWait == WAIT_OBJECT_0)
                break;
            if (luWait == WAIT_FAILED)
                throw std::system_error(static_cast<int>(GetLastError()),
                    std::system_category(), "frame completion wait");
            CgsDev::Assert::ServiceWorkerAssertsWhileWaitingPC();
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
        if (lbQuit)
            PostQuitMessage(liQuitCode);
    }
}

namespace CgsSystem
{
    // @ 0x828E15C8 - default-construct the owned thread, the two handshake barriers (deferred
    // init: BarrierParameters=NULL, bDefaultParameters=false) and the secondary-thread assert
    // slot (NULL). The asm runs the dispatch-thread default ctor (Hex-Rays mislabels its
    // address as rw::movie::VideoRenderable::~VideoRenderable) then two interrupt-guarded
    // atomic stores of 0 into the AtomicPointer at +0x7C - i.e. AtomicPointer(NULL).
    ThreadLayout::ThreadLayout()
        : mpThreadClass(NULL)
        , miPerfMonResource(0)
        , miPerfMonUpdateWaitForDispatch(0)
        , miPerfMonAllThreadSyncs(0)
        , mUpdateThreadId(EA::Thread::kThreadIdInvalid)
        , mDispatchThread()
        , mUpdateStartBarrier(NULL, false)
        , mUpdateEndBarrier(NULL, false)
        , meFrameRate(E_FRAMERATE_UNKNOWN)
        , miResourcePercentageOfFrame(0)
        , mbUpdatingResourceSystem(false)
        , mAssertFromSecondaryThread(NULL)
        , mpDispatchReadyEventPC(nullptr)
        , mpUpdateReadyEventPC(nullptr)
        , mStopDispatchPC(0)
        , mbDispatchStartedPC(false)
        , mbFrameInFlightPC(false)
        , mbParallelEnabledPC(true)
    {
    }

    ThreadLayout::~ThreadLayout()
    {
        EndPC();
    }

    void ThreadLayout::InitThreads()
    {
        mDispatchExceptionPC = nullptr;
        if (mUpdateThreadId == EA::Thread::kThreadIdInvalid)
        {
            const EA::Thread::BarrierParameters lBarrierParameters(2, true);
            mUpdateStartBarrier.Init(&lBarrierParameters);
            mUpdateEndBarrier.Init(&lBarrierParameters);
        }
        EA::Thread::Thread::SetGlobalRunnableFunctionUserWrapper(GlobalThreadBeginWrapper);
        mUpdateThreadId = EA::Thread::GetThreadId();
        EA::Thread::SetThreadPriority(0);
        EA::Thread::ThreadParameters lParameters;
        lParameters.mnPriority = 0;
        lParameters.mbDisablePriorityBoost = false;
        // FLAG PC-platform leaf: ARTIST pins console processor 2. The native
        // scheduler chooses an available host core, including on hybrid CPUs.
        lParameters.mnProcessor = EA::Thread::kProcessorAny;
        lParameters.mpName = "DispatchThread";
        mStopDispatchPC.SetValue(0);
        if (mbParallelEnabledPC)
        {
            mpDispatchReadyEventPC = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            mpUpdateReadyEventPC = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        }
        if (mpDispatchReadyEventPC != nullptr && mpUpdateReadyEventPC != nullptr)
        {
            mbDispatchStartedPC = mDispatchThread.Begin(DispatchThread, this, &lParameters)
                != EA::Thread::kThreadIdInvalid;
        }
        if (CgsDev::Log::gpDebugPrint)
            *CgsDev::Log::gpDebugPrint << "[frame-layout] requested="
                << static_cast<u32>(mbParallelEnabledPC) << " worker="
                << static_cast<u32>(mbDispatchStartedPC) << "\n";
        if ((CgsDev::Message::gxMessageFilterFlags & 1u) != 0 && CgsDev::Log::gpDebugPrint)
        {
            *CgsDev::Log::gpDebugPrint << "Main thread priority: "
                << EA::Thread::GetThreadPriority() << "\n";
            *CgsDev::Log::gpDebugPrint << "Dispatch priority: "
                << mDispatchThread.GetPriority() << "\n";
        }
    }

    intptr_t ThreadLayout::DispatchThread(void* lpContext)
    {
        ThreadLayout& lrLayout = *static_cast<ThreadLayout*>(lpContext);
        CgsDev::Assert::SetDispatchThreadPC(true);
        for (;;)
        {
            lrLayout.mUpdateStartBarrier.Wait();
            if (lrLayout.mStopDispatchPC.GetValue() != 0)
            {
                CgsDev::Assert::SetDispatchThreadPC(false);
                return 0;
            }
            try
            {
                lrLayout.mpThreadClass->DispatchThread();
            }
            catch (...)
            {
                lrLayout.mDispatchExceptionPC = std::current_exception();
            }
            // Signal before joining: after this point the worker only touches
            // the barrier, so the window owner can join without a SendMessage deadlock.
            SetEvent(lrLayout.mpDispatchReadyEventPC);
            lrLayout.mUpdateEndBarrier.Wait();
        }
    }

    // FLAG PC-platform leaf: a window-owning thread must process sent/queued
    // messages while waiting for D3D work on another thread (Microsoft's
    // Creating Windows in Threads / MsgWaitForMultipleObjectsEx contract).
    void ThreadLayout::WaitForDispatchCompletionPC()
    {
        if (!mbFrameInFlightPC)
            return;
        WaitForWindowThreadHandle(mpDispatchReadyEventPC);
        mUpdateEndBarrier.Wait();
        mbFrameInFlightPC = false;
    }

    bool ThreadLayout::Update()
    {
        if (const void* lpAssert = mAssertFromSecondaryThread.GetValue())
        {
            InteruptThreadForAssert(static_cast<const AssertData*>(lpAssert));
            return true;
        }
        mbUpdatingResourceSystem = false;
        CgsDev::Assert::DispatchPendingAssertsPC();
        // FLAG PC-platform leaf: native debug callbacks can change render
        // switches directly. Run that start-of-frame phase before releasing the
        // worker, so those writes cannot race this frame's dispatch.
        mpThreadClass->OnStartOfUpdateFrame();
        if (mbDispatchStartedPC)
        {
            mDispatchExceptionPC = nullptr;
            ResetEvent(mpUpdateReadyEventPC);
            mUpdateStartBarrier.Wait();
            mbFrameInFlightPC = true;
        }
        bool lbResult;
        u32 luTimeTaken;
        try
        {
            const u32 luStartTime = GetSystemTimeMS();
            lbResult = mpThreadClass->UpdateThread();
            luTimeTaken = GetSystemTimeMS() - luStartTime;
        }
        catch (...)
        {
            // FLAG PC-platform leaf: release the debug tail even if a native
            // update callback throws, then join before unwinding its resources.
            if (mbDispatchStartedPC)
                SetEvent(mpUpdateReadyEventPC);
            WaitForDispatchCompletionPC();
            throw;
        }
        if (mbDispatchStartedPC)
            SetEvent(mpUpdateReadyEventPC);
        // DecFIGS names these debugger-controlled locals. ARTIST tests the
        // corresponding BSS bytes 0x830EA8B5 / 0x830EA8B4; both start false.
        static volatile bool _lbAllowStallTest = false;
        static volatile bool _lbContinue = false;
        if (_lbAllowStallTest && luTimeTaken > 250u)
        {
            std::printf("Crazy frame stall of %ums\n", luTimeTaken);
            while (!_lbContinue) {}
            _lbContinue = false;
        }
        // FLAG PC-platform leaf: if native thread/event creation failed, consume
        // the same published frame serially and retain the callback ordering.
        if (!mbDispatchStartedPC)
            mpThreadClass->DispatchThread();
        if (miPerfMonAllThreadSyncs >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonAllThreadSyncs);
        if (miPerfMonUpdateWaitForDispatch >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonUpdateWaitForDispatch);
        WaitForDispatchCompletionPC();
        mbUpdatingResourceSystem = true;
        if (miPerfMonUpdateWaitForDispatch >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonUpdateWaitForDispatch);
        if (miPerfMonAllThreadSyncs >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonAllThreadSyncs);
        if (mDispatchExceptionPC)
            std::rethrow_exception(mDispatchExceptionPC);
        CgsDev::Assert::DispatchPendingAssertsPC();
        mpThreadClass->OnCompletionOfVsyncWait();
        if (miPerfMonResource >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonResource);
        mpThreadClass->ResourceUpdateThread(nullptr);
        if (miPerfMonResource >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonResource);
        mpThreadClass->OnEndOfUpdateFrame();
        return lbResult;
    }

    void ThreadLayout::EndPC(bool lbCloseFrameOwner)
    {
        if (mbDispatchStartedPC)
        {
            WaitForDispatchCompletionPC();
            mStopDispatchPC.SetValue(1);
            mUpdateStartBarrier.Wait();
            WaitForWindowThreadHandle(mDispatchThread.GetId());
            mDispatchThread.WaitForEnd();
            mbDispatchStartedPC = false;
        }
        if (lbCloseFrameOwner)
            CgsDev::Assert::DetachFrameOwnerPC(this);
        if (mpDispatchReadyEventPC != nullptr)
        {
            CloseHandle(mpDispatchReadyEventPC);
            mpDispatchReadyEventPC = nullptr;
        }
        if (mpUpdateReadyEventPC != nullptr)
        {
            CloseHandle(mpUpdateReadyEventPC);
            mpUpdateReadyEventPC = nullptr;
        }
    }

    void ThreadLayout::WaitForUpdateCompletionPC()
    {
        // FLAG PC-platform leaf: the debug renderer still reads the live debug
        // manager. Only that tail waits; world/particle dispatch overlaps update.
        if (mbDispatchStartedPC && EA::Thread::GetThreadId() == mDispatchThread.GetId())
            WaitForSingleObject(mpUpdateReadyEventPC, INFINITE);
    }

    void ThreadLayout::SynchronizeDispatchPC()
    {
        // FLAG PC-platform leaf: rare producer-side resource mutations must
        // finish the old frame before touching dispatch-owned effect instances.
        if (mbFrameInFlightPC)
        {
            SetEvent(mpUpdateReadyEventPC);
            WaitForDispatchCompletionPC();
        }
    }

    // @ 0x828EBAA0 - InitThreads() spins up the dispatch thread + barriers, then the perf-mon
    // resource ids and the default frame layout are stored: meFrameRate = 60 (E_FRAMERATE_60HZ),
    // miResourcePercentageOfFrame = 4, mbUpdatingResourceSystem = false. Note the asm parameter
    // order: a3->miPerfMonResource(+4), a4->miPerfMonAllThreadSyncs(+12),
    // a5->miPerfMonUpdateWaitForDispatch(+8).
    void ThreadLayout::Begin(IThreadClass* lpThreadInterface,
                             s32 liPerfMonResource,
                             s32 liPerfMonAllThreadSyncs,
                             s32 liPerfMonUpdateWaitForDispatch)
    {
        InitThreads();

        mpThreadClass                  = lpThreadInterface;
        miPerfMonResource              = liPerfMonResource;
        miPerfMonUpdateWaitForDispatch = liPerfMonUpdateWaitForDispatch;
        miPerfMonAllThreadSyncs        = liPerfMonAllThreadSyncs;

        meFrameRate                  = E_FRAMERATE_60HZ;
        miResourcePercentageOfFrame  = 4;
        mbUpdatingResourceSystem     = false;
        CgsDev::Assert::AttachFrameOwnerPC(this, [](void* lpOwner)
        {
            static_cast<ThreadLayout*>(lpOwner)->SynchronizeDispatchPC();
            CgsDev::Assert::DispatchPendingAssertsPC();
        });
    }

    // @ 0x828D7978 - the default RunnableFunctionUserWrapper EAThread installs for the
    // dispatch thread: just invoke the runnable with its context (tail call, no extra work).
    intptr_t ThreadLayout::GlobalThreadBeginWrapper(EA::Thread::RunnableFunction defaultRunnableFunction,
                                                    void* lpContext)
    {
        return defaultRunnableFunction(lpContext);
    }

    // @ 0x828D79D8 - called when an assert fires on a secondary thread. If we ARE the dispatch
    // thread there is nothing to interrupt (return false). Otherwise, if we are NOT the update
    // thread either, publish the assert pointer atomically and then sleep forever (this thread
    // is parked - the assert is handled elsewhere). When we are the update thread, drop through:
    // wait on the end barrier (+0x44) unless a resource-system update is already in flight, then hand
    // the assert to the thread class via RenderAssert (vtable slot 6, +0x18) and return true.
    bool ThreadLayout::InteruptThreadForAssert(const AssertData* lpAssert)
    {
        const EA::Thread::ThreadId lDispatchThreadId = mDispatchThread.GetId();
        if (EA::Thread::GetThreadId() == lDispatchThreadId)
        {
            return false;
        }

        if (EA::Thread::GetThreadId() != mUpdateThreadId)
        {
            // Not the update thread - hand the assert off and park this thread indefinitely.
            mAssertFromSecondaryThread.SetValue(const_cast<AssertData*>(lpAssert));
            for (;;)
            {
                EA::Thread::ThreadTime lSleepMs = 1000;
                EA::Thread::ThreadSleep(lSleepMs);
            }
        }

        if (!mbUpdatingResourceSystem && mbFrameInFlightPC)
        {
            SynchronizeDispatchPC();
        }
        mpThreadClass->RenderAssert(lpAssert);
        return true;
    }
}
