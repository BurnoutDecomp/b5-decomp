#include "SDKs/EATech/eajobs/job_thread.h"
#include "SDKs/EATech/eajobs/local_backend.h" // EA::Jobs::LocalBackend::LocalBackend (mpBackend dispatch target)
#include "SDKs/EATech/eajobs/jobs.h" // EA::Jobs::GetAllocator (off_8327F280 Free)

#include <windows.h> // CreateEventA, CloseHandle, WaitForSingleObject

// ============================================================================
// SDKs/EATech/eajobs/job_thread.cpp
//
// EA::Jobs::LocalBackend::JobThread, reconstructed store-for-store from the X360
// .XEX (BURNOUT_X360_ARTIST.XEX):
//   JobThread::JobThread                  @ 0x82BCAB38
//   JobThread::Run                        @ 0x82BC9FB0
//   JobThread::WaitForEnd                 @ 0x82BC9F60
//   JobThread::~JobThread                 @ 0x82BC9E78
//   JobThread::`scalar deleting destructor' @ 0x82BCABB0
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
namespace LocalBackend
{
    // @ 0x82BCAB38 -- construct an idle worker. Store order from the asm:
    //   *this        = JobThread vtable (off_821824D0)   ; stw r10,0(r31)
    //   mpBackend    = 0                                 ; stw 0,4(r31)
    //   mbStarted    = 0                                 ; stb 0,8(r31)
    //   mbQuit       = 0                                 ; stb 0,9(r31)
    //   mThread      EA::Thread::Thread()  (single store)  ; at +0xC
    //   mParameters  JobThreadParameters() (defaults)      ; at +0x10
    //   mhEvent      = CreateEventA(0,0,0,0)               ; +0x28 (auto-reset, unsignaled)
    JobThread::JobThread()
        : mpBackend(0)
        , mbStarted(false)
        , mbQuit(false)
        , mThread()
        , mParameters()
    {
        // CreateEventA(lpEventAttributes=0, bManualReset=0, bInitialState=0, lpName=0):
        // an unnamed, auto-reset, initially-unsignaled wake event.
        mhEvent = CreateEventA(0, FALSE, FALSE, 0);
    }

    // @ 0x82BC9E78 -- close the wake event, restore the base vtable, then run the
    // embedded Thread subobject's destructor (release its dynamic-data reference).
    // Store order: zero the wake handle's slot vtable to JobThread's, CloseHandle,
    // ~Thread(+0xC), then stamp the IRunnable base vtable (off_821823FC).
    JobThread::~JobThread()
    {
        CloseHandle(mhEvent);
        // (~Thread runs implicitly via the member subobject after this body.)
    }

    // @ 0x82BC9FB0 -- worker loop. Spin until told to quit: each pass try to run any
    // ready jobs for our affinity; if none ran, sleep on the wake event for the
    // backend-supplied timeout. Returns 0.
    intptr_t JobThread::Run(void* /*pContext*/)
    {
        while (!mbQuit)
        {
            // ExecuteReadyJobs returns 0 when there was nothing to run -> sleep.
            if (!mpBackend->ExecuteReadyJobs(static_cast<int>(mParameters.affinity)))
            {
                const u32 luTimeoutMs = mpBackend->GetSleepTimeoutMs();
                WaitForSingleObject(mhEvent, luTimeoutMs);
            }
        }
        return 0;
    }

    // @ 0x82BC9F60 -- if started, join the OS thread; then clear the started flag.
    void JobThread::WaitForEnd()
    {
        if (mbStarted)
        {
            // The X360 passes (&unk_821823E8, 0): wait forever, no return-value
            // out-param. unk_821823E8 is the "infinite" absolute-timeout sentinel.
            mThread.WaitForEnd(0, 0);
        }
        mbStarted = false;
    }

    // @ 0x82BCABB0 -- the MSVC scalar deleting destructor thunk: run ~JobThread,
    // then (if the delete bit is set and the pointer is non-null) hand the storage
    // back to the process-wide Jobs allocator via its Free virtual (vtable slot
    // +0x0C), called as Free(this, ptr, 0).
    //
    // Reconstructed as a free function because the per-object scalar-deleting
    // destructor is a compiler-internal thunk with no portable C++ spelling; the
    // X360 binary materialises it as this standalone routine.
    JobThread* JobThread_ScalarDeletingDestructor(JobThread* pThis, char cFlags)
    {
        pThis->~JobThread();
        if ((cFlags & 1) != 0 && pThis != 0)
        {
            EA::Jobs::Allocator* lpAllocator = EA::Jobs::GetAllocator();
            lpAllocator->Free(pThis, 0);
        }
        return pThis;
    }
}
}
}
