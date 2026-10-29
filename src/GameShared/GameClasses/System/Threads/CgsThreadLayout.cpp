// Native update/dispatch handshake from ARTIST828D7988/828E15C8/828E17D0.
#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <cstdio>

namespace CgsSystem
{
    ThreadLayout::ThreadLayout()
        : mUpdateStartBarrier(0, false), mUpdateEndBarrier(0, false)
        , mAssertFromSecondaryThread(0)
    {
    }

    ThreadLayout::~ThreadLayout()
    {
    }

    // ARTIST828E1658: the worker blocks on the start barrier until the first frame.
    void ThreadLayout::InitThreads()
    {
        const EA::Thread::BarrierParameters lBarrierParameters(2, true);
        mUpdateStartBarrier.Init(&lBarrierParameters);
        mUpdateEndBarrier.Init(&lBarrierParameters);
        EA::Thread::Thread::SetGlobalRunnableFunctionUserWrapper(GlobalThreadBeginWrapper);
        mUpdateThreadId = EA::Thread::GetThreadId();
        EA::Thread::ThreadParameters lParameters;
        lParameters.mbSuspended = false;
        EA::Thread::SetThreadPriority(0);
        lParameters.mnProcessor = 2;
        lParameters.mnPriority = 0;
        lParameters.mpName = "DispatchThread";
        mDispatchThread.Begin(DispatchThread, this, &lParameters,
            EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper());
        if (CgsDev::Message::gxMessageFilterFlags & 1u)
        {
            CgsDev::StrStreamBase& lStream = *CgsDev::Log::gpDebugPrint;
            lStream << "Main thread priority: ";
            lStream << EA::Thread::GetThreadPriority() << "\n";
        }
        if (CgsDev::Message::gxMessageFilterFlags & 1u)
        {
            CgsDev::StrStreamBase& lStream = *CgsDev::Log::gpDebugPrint;
            lStream << "Dispatch priority: ";
            lStream << mDispatchThread.GetPriority() << "\n";
        }
    }

    intptr_t ThreadLayout::DispatchThread(void* lpContext)
    {
        ThreadLayout& lrLayout = *static_cast<ThreadLayout*>(lpContext);
        for (;;)
        {
            lrLayout.mUpdateStartBarrier.Wait();
            lrLayout.mpThreadClass->DispatchThread();
            lrLayout.mUpdateEndBarrier.Wait();
        }
    }

    bool ThreadLayout::Update()
    {
        if (const void* lpAssert = mAssertFromSecondaryThread.GetValue())
        {
            InteruptThreadForAssert(static_cast<const AssertData*>(lpAssert));
            return true;
        }
        mbUpdatingResourceSystem = false;
        mUpdateStartBarrier.Wait();
        mpThreadClass->OnStartOfUpdateFrame();
        const u32 luStartTime = GetSystemTimeMS();
        const bool lbResult = mpThreadClass->UpdateThread();
        const u32 luTimeTaken = GetSystemTimeMS() - luStartTime;
        static volatile bool _lbAllowStallTest = false;
        static volatile bool _lbContinue = false;
        if (_lbAllowStallTest && luTimeTaken > 250u)
        {
            std::printf("Crazy frame stall of %ums\n", luTimeTaken);
            // ARTIST samples the debugger byte once before polling.
            const bool lbContinue = _lbContinue;
            while (!lbContinue) {}
            _lbContinue = false;
        }
        if (miPerfMonAllThreadSyncs >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonAllThreadSyncs);
        if (miPerfMonUpdateWaitForDispatch >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonUpdateWaitForDispatch);
        mUpdateEndBarrier.Wait();
        mbUpdatingResourceSystem = true;
        if (miPerfMonUpdateWaitForDispatch >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonUpdateWaitForDispatch);
        if (miPerfMonAllThreadSyncs >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonAllThreadSyncs);
        mpThreadClass->OnCompletionOfVsyncWait();
        if (miPerfMonResource >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonResource);
        mpThreadClass->ResourceUpdateThread(0);
        if (miPerfMonResource >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonResource);
        mpThreadClass->OnEndOfUpdateFrame();
        return lbResult;
    }

    void ThreadLayout::Begin(IThreadClass* lpThreadInterface, s32 liPerfMonResource,
        s32 liPerfMonAllThreadSyncs, s32 liPerfMonUpdateWaitForDispatch)
    {
        InitThreads();
        mpThreadClass = lpThreadInterface;
        miPerfMonResource = liPerfMonResource;
        miPerfMonUpdateWaitForDispatch = liPerfMonUpdateWaitForDispatch;
        miPerfMonAllThreadSyncs = liPerfMonAllThreadSyncs;
        meFrameRate = E_FRAMERATE_60HZ;
        miResourcePercentageOfFrame = 4;
        mbUpdatingResourceSystem = false;
    }

    intptr_t ThreadLayout::GlobalThreadBeginWrapper(EA::Thread::RunnableFunction pRunnable, void* pContext)
    {
        return pRunnable(pContext);
    }

    bool ThreadLayout::InteruptThreadForAssert(const AssertData* lpAssert)
    {
        const EA::Thread::ThreadId lDispatchThreadId = mDispatchThread.GetId();
        if (EA::Thread::GetThreadId() == lDispatchThreadId)
            return false;
        if (EA::Thread::GetThreadId() != mUpdateThreadId)
        {
            mAssertFromSecondaryThread.SetValue(const_cast<AssertData*>(lpAssert));
            for (;;)
            {
                const EA::Thread::ThreadTime lSleepMs = 1000;
                EA::Thread::ThreadSleep(&lSleepMs);
            }
        }
        if (!mbUpdatingResourceSystem)
            mUpdateEndBarrier.Wait();
        mpThreadClass->RenderAssert(lpAssert);
        return true;
    }
}
