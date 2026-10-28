// CgsSystem::ThreadLayout - secondary-thread / barrier handshake owner. See CgsThreadLayout.h.
//
// ARTIST InitThreads 0x828E1658, Update 0x828E17D0 and dispatch worker
// 0x828D7988 define the two-barrier frame handshake. Native window messages
// and worker shutdown are adapted explicitly below.

#include <xtl.h>
#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include <cstdio>
#include <system_error>

#include "eathread/eathread.h"   // EA::Thread::GetThreadId / ThreadSleep / ThreadId

namespace CgsSystem
{
    // @ 0x828E15C8 - default-construct the owned thread, the two handshake barriers (deferred
    // init: BarrierParameters=NULL, bDefaultParameters=false) and the secondary-thread assert
    // slot (NULL). The asm runs the dispatch-thread default ctor (Hex-Rays mislabels its
    // address as rw::movie::VideoRenderable::~VideoRenderable) then two interrupt-guarded
    // atomic stores of 0 into the AtomicPointer at +0x7C - i.e. AtomicPointer(NULL).

    ThreadLayout::~ThreadLayout()
    {
    }

    void ThreadLayout::InitThreads()
    {
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
        lParameters.mpName = "DispatchThread";
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
        for (;;)
        {
            lrLayout.mUpdateStartBarrier.Wait();
            try
            {
                lrLayout.mpThreadClass->DispatchThread();
            }
            catch (...)
            {
            }
            // Signal before joining: after this point the worker only touches
            // the barrier, so the window owner can join without a SendMessage deadlock.
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
            throw;
        }
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
        if (miPerfMonAllThreadSyncs >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonAllThreadSyncs);
        if (miPerfMonUpdateWaitForDispatch >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonUpdateWaitForDispatch);
        mbUpdatingResourceSystem = true;
        if (miPerfMonUpdateWaitForDispatch >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonUpdateWaitForDispatch);
        if (miPerfMonAllThreadSyncs >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonAllThreadSyncs);
        mpThreadClass->OnCompletionOfVsyncWait();
        if (miPerfMonResource >= 0)
            CgsDev::PerfMonCpu::StartMonitor(miPerfMonResource);
        mpThreadClass->ResourceUpdateThread(nullptr);
        if (miPerfMonResource >= 0)
            CgsDev::PerfMonCpu::StopMonitor(miPerfMonResource);
        mpThreadClass->OnEndOfUpdateFrame();
        return lbResult;
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

        mpThreadClass->RenderAssert(lpAssert);
        return true;
    }
}
