// CgsSystem::ThreadLayout - secondary-thread / barrier handshake owner. See CgsThreadLayout.h.
//
// Only the four functions the X360 boot-trace packet carries asm for are reconstructed here
// (the constructor, Begin, GlobalThreadBeginWrapper and InteruptThreadForAssert). The other
// DWARF-declared members of the TU (Update, InitThreads, DispatchThread, SetFrameRate, ...)
// did not run in the goal trace and have no asm in this packet, so they are declared in the
// header but intentionally left undefined here rather than fabricated.

#include "GameShared/GameClasses/System/Threads/CgsThreadLayout.h"

#include "eathread/eathread.h"   // EA::Thread::GetThreadId / ThreadSleep / ThreadId

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
    {
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

        if (!mbUpdatingResourceSystem)
        {
            mUpdateEndBarrier.Wait();
        }
        mpThreadClass->RenderAssert(lpAssert);
        return true;
    }
}
