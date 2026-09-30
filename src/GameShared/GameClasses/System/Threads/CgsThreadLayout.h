#pragma once

// CgsSystem::IThreadClass - the engine's thread-callback interface. A class that drives the
// update/dispatch/resource threads implements it; ThreadLayout calls these back on the
// appropriate threads. BrnGameModule is the concrete implementor. Interface recovered from
// the DecFIGS DWARF (System/Threads/CgsThreadLayout.h); only the interface is declared here
// (ThreadLayout itself is its own TU).
//
// CgsSystem::ThreadLayout - owns the secondary (dispatch) thread and the two barriers that
// fence the update/dispatch handshake each frame. It drives an IThreadClass implementation:
// the update thread runs IThreadClass::UpdateThread, the dispatch thread runs
// DispatchThread/IThreadClass::DispatchThread, and asserts raised on a secondary thread are
// funnelled back through InteruptThreadForAssert -> IThreadClass::RenderAssert. Layout/method
// shape from the DecFIGS DWARF; bodies grounded against the X360 asm.

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"   // CgsSystem::EFrameRate
#include "eathread/eathread.h"                                  // EA::Thread::ThreadId, RunnableFunction
#include "eathread/eathread_thread.h"                           // EA::Thread::Thread
#include "eathread/eathread_barrier.h"                          // EA::Thread::Barrier
#include "eathread/eathread_atomic.h"                           // EA::Thread::AtomicPointer
#include <exception>

class Mutex;
namespace CgsDev { namespace Assert { struct AssertData; } }
using AssertData = CgsDev::Assert::AssertData;

namespace CgsSystem
{
    struct IThreadClass
    {
        virtual bool UpdateThread() = 0;
        virtual void DispatchThread() = 0;
        virtual void ResourceUpdateThread(Mutex* lpMutex) = 0;
        virtual void OnStartOfUpdateFrame() = 0;
        virtual void OnEndOfUpdateFrame() = 0;
        virtual void OnCompletionOfVsyncWait() = 0;
        virtual void RenderAssert(const AssertData* lpAssertData) = 0;
    };

    struct ThreadLayout
    {
        ThreadLayout();
        ~ThreadLayout();

        void                Begin(IThreadClass* lpThreadInterface,
                                  s32 liPerfMonResource,
                                  s32 liPerfMonAllThreadSyncs,
                                  s32 liPerfMonUpdateWaitForDispatch);
        bool                Update();
        void                SetFrameRate(EFrameRate leFrameRate);
        void                SetResourcePercentageOfFrame(s32 liPercentage);
        EA::Thread::ThreadId GetUpdateThreadId() const;
        EA::Thread::ThreadId GetDispatchThreadId() const;
        bool                InteruptThreadForAssert(const AssertData* lpAssert);
        // FLAG PC-platform leaf: join the dispatch worker before destroying the
        // native window/device. The console kept this worker until process exit.
        void                EndPC(bool lbCloseFrameOwner = true);
        void                SetParallelEnabledPC(bool lbEnabled) { mbParallelEnabledPC = lbEnabled; }
        void                WaitForUpdateCompletionPC();
        void                SynchronizeDispatchPC();

    private:
        void     InitThreads();
        static intptr_t GlobalThreadBeginWrapper(EA::Thread::RunnableFunction defaultRunnableFunction, void* lpContext);
        static intptr_t DispatchThread(void* lpContext);
        void     WaitForDispatchCompletionPC();

        // Layout from the DecFIGS DWARF (member order); the X360 asm pins the offsets the
        // bodies touch (mpThreadClass @0, the perf-mon ids @4/8/12, meFrameRate/percentage/
        // mbUpdatingResourceSystem seeded by Begin, mAssertFromSecondaryThread published by
        // InteruptThreadForAssert).
        IThreadClass*                 mpThreadClass;
        s32                           miPerfMonResource;
        s32                           miPerfMonUpdateWaitForDispatch;
        s32                           miPerfMonAllThreadSyncs;
        EA::Thread::ThreadId          mUpdateThreadId;
        EA::Thread::Thread            mDispatchThread;
        EA::Thread::Barrier           mUpdateStartBarrier;
        EA::Thread::Barrier           mUpdateEndBarrier;
        volatile EFrameRate           meFrameRate;
        volatile s32                  miResourcePercentageOfFrame;
        bool                          mbUpdatingResourceSystem;
        EA::Thread::AtomicPointer     mAssertFromSecondaryThread;
        // FLAG PC-platform leaf: the window-owning update thread must service
        // synchronous Win32 messages while the D3D dispatch thread finishes.
        void*                         mpDispatchReadyEventPC;
        void*                         mpUpdateReadyEventPC;
        EA::Thread::AtomicInt32        mStopDispatchPC;
        bool                          mbDispatchStartedPC;
        bool                          mbFrameInFlightPC;
        bool                          mbParallelEnabledPC;
        // FLAG PC-platform leaf: propagate worker exceptions after joining it.
        std::exception_ptr            mDispatchExceptionPC;
    };
}
