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
        // DWARF locals lBarrierParams (cpp:91) and lThreadParams (cpp:103) are default-constructed and
        // then filled: ARTIST calls BarrierParameters(0, true, NULL) @0x828E1678 and then stores
        // mbIntraProcess = 1 / mnHeight = 2 (@0x828E168C / 0x828E1690).
        EA::Thread::BarrierParameters lBarrierParams;
        lBarrierParams.mnHeight       = 2;
        lBarrierParams.mbIntraProcess = true;
        mUpdateStartBarrier.Init(&lBarrierParams);
        mUpdateEndBarrier.Init(&lBarrierParams);
        EA::Thread::Thread::SetGlobalRunnableFunctionUserWrapper(GlobalThreadBeginWrapper);
        mUpdateThreadId = EA::Thread::GetThreadId();
        EA::Thread::ThreadParameters lThreadParams;
        lThreadParams.mbSuspended = false;
        EA::Thread::SetThreadPriority(0);
        lThreadParams.mnProcessor = 2;
        lThreadParams.mnPriority = 0;
        lThreadParams.mpName = "DispatchThread";
        mDispatchThread.Begin(DispatchThread, this, &lThreadParams,
            EA::Thread::Thread::GetGlobalRunnableFunctionUserWrapper());
        if (CgsDev::Message::gxMessageFilterFlags & 1u)
            *CgsDev::Log::gpDebugPrint << "Main thread priority: " << EA::Thread::GetThreadPriority() << "\n";
        if (CgsDev::Message::gxMessageFilterFlags & 1u)
            *CgsDev::Log::gpDebugPrint << "Dispatch priority: " << mDispatchThread.GetPriority() << "\n";
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
        // DWARF: the test goes through AtomicInt<int32_t>::operator const int32_t (cmpwi @0x828E17E4)
        // and the call re-reads the pointer (second lwz @0x828E17EC).
        if (mAssertFromSecondaryThread != 0)
        {
            InteruptThreadForAssert(static_cast<const AssertData*>(mAssertFromSecondaryThread.GetValue()));
            return true;
        }
        mbUpdatingResourceSystem = false;
        mUpdateStartBarrier.Wait();
        mpThreadClass->OnStartOfUpdateFrame();
        const u32 luStartTime = GetSystemTimeMS();
        const bool lbResult = mpThreadClass->UpdateThread();
        const u32 luTimeTaken = GetSystemTimeMS() - luStartTime;
        // DWARF static locals _lbAllowStallTest (cpp:220) and _lbContinue (cpp:226, inner scope): plain
        // bools a debugger flips. ARTIST reads _lbContinue once and spins on the register
        // (lbz @0x828E1884, beq-to-self @0x828E188C), the code a non-volatile flag compiles to.
        static bool _lbAllowStallTest = false;
        if (_lbAllowStallTest && luTimeTaken > 250u)
        {
            std::printf("Crazy frame stall of %ums\n", luTimeTaken);
            {
                static bool _lbContinue = false;
                while (!_lbContinue) {}
                _lbContinue = false;
            }
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

    // ARTIST BrnGameModule::Prepare publishes these words at 0x9A1198 and 0x9A119C.
    void ThreadLayout::SetFrameRate(EFrameRate leFrameRate)
    {
        meFrameRate = leFrameRate;
    }

    void ThreadLayout::SetResourcePercentageOfFrame(s32 liPercentage)
    {
        miResourcePercentageOfFrame = liPercentage;
    }


    intptr_t ThreadLayout::GlobalThreadBeginWrapper(EA::Thread::RunnableFunction pRunnable, void* pContext)
    {
        return pRunnable(pContext);
    }

    bool ThreadLayout::InteruptThreadForAssert(const AssertData* lpAssert)
    {
        // DWARF: GetDispatchThreadId (inline) and AtomicPointer::SetValue; ARTIST compares
        // GetThreadId() against each id (cmplw r3,r30 @0x828D79FC / 0x828D7A18) and places the
        // secondary-thread branch after the update-thread one.
        if (EA::Thread::GetThreadId() == GetDispatchThreadId())
            return false;
        if (EA::Thread::GetThreadId() == GetUpdateThreadId())
        {
            if (!mbUpdatingResourceSystem)
                mUpdateEndBarrier.Wait();
            mpThreadClass->RenderAssert(lpAssert);
            return true;
        }
        mAssertFromSecondaryThread.SetValue(const_cast<AssertData*>(lpAssert));
        for (;;)
        {
            const EA::Thread::ThreadTime lSleepMs = 1000;
            EA::Thread::ThreadSleep(&lSleepMs);
        }
    }
}
