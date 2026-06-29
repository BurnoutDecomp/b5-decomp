#ifndef EA_JOBS_LOCAL_BACKEND_JOB_THREAD_H
#define EA_JOBS_LOCAL_BACKEND_JOB_THREAD_H

#include "types.hpp"
#include "SDKs/EATech/eathread/BrnEAThreadX360.h"     // EA::Thread::Thread, IRunnable, HANDLE
#include "SDKs/EATech/eajobs/job_thread_parameters.h" // EA::Jobs::JobThreadParameters

// SDKs/EATech/eajobs/job_thread.h
//
// EA::Jobs::LocalBackend::JobThread -- one worker thread of the local (in-process)
// job-manager backend. It is an EA::Thread::IRunnable whose Run loop repeatedly asks
// the owning backend to execute ready jobs, sleeping on an auto-reset event between
// passes until the backend signals more work (or the thread is told to quit).
//
// Reconstructed from the X360 .XEX (BURNOUT_X360_ARTIST.XEX):
//   JobThread::JobThread                  @ 0x82BCAB38   (ctor)
//   JobThread::Run                        @ 0x82BC9FB0   (IRunnable::Run override)
//   JobThread::WaitForEnd                 @ 0x82BC9F60
//   JobThread::~JobThread                 @ 0x82BC9E78
//   JobThread::`scalar deleting destructor' @ 0x82BCABB0
//
// X360 layout (ctor store offsets @ 0x82BCAB38; 44-byte stride proven by
// LocalBackend::AddThread's `44 * index` element addressing):
//   +0x00 (IRunnable vtable; off_821824D0 == JobThread vtable, base off_821823FC)
//   +0x04 mpBackend     (LocalBackend*)   -- the LocalBackend this worker drives
//   +0x08 mbStarted     (bool)            -- thread was Begin()'d (WaitForEnd guard)
//   +0x09 mbQuit        (bool)            -- Run loop exit flag
//   +0x0C mThread       (EA::Thread::Thread) [4B]
//   +0x10 mParameters   (JobThreadParameters) [24B, +0x10..+0x27]
//   +0x28 mhEvent       (HANDLE)          -- auto-reset wake event (CreateEventA)
// sizeof == 0x2C (44).
//
// Vendor EA code reconstructed in its canonical home.

namespace EA
{
namespace Jobs
{
namespace LocalBackend
{
    // The backend JobThread::Run drives. The X360 stores the owning LocalBackend* in
    // the worker's +0x04 slot and dispatches its combined-vtable worker entry points
    // BY NAME -- slot +0x50 (ExecuteReadyJobs, run any ready jobs for an affinity) and
    // slot +0x54 (GetSleepTimeoutMs, the wait before the next wake). LocalBackend has
    // ONE vptr (off_82182500, the Detail::SchedulerBackend vtable extended with the
    // worker slots), so the worker calls those virtuals directly on the concrete
    // backend -- there is NO separate worker-interface base/vtable. Forward-declared
    // here; the definition (and the call dispatch in JobThread::Run) lives in
    // job_thread.cpp, which includes local_backend.h.
    class LocalBackend;

    class JobThread : public EA::Thread::IRunnable
    {
    public:
        // @ 0x82BCAB38 -- construct an idle (not-yet-started) worker: default
        // parameters, a fresh auto-reset wake event, flags clear.
        JobThread();

        // @ 0x82BC9E78
        virtual ~JobThread();

        // @ 0x82BC9FB0 -- the IRunnable body: loop until mbQuit; each pass run the
        // backend's ready jobs for our affinity, and if none ran, sleep on the wake
        // event for the backend's sleep timeout. Returns 0.
        virtual intptr_t Run(void* pContext = 0);

        // Start this idle worker running: adopt the supplied parameters + backend, mark
        // it started and Begin the underlying OS thread. (LocalBackend::AddThread @
        // 0x82BCA6C0 calls it as Start(&parameters, backend); the body lives in the
        // out-of-group JobThread object TU -- declared here so AddThread links.)
        void Start(const EA::Jobs::JobThreadParameters* pParameters, LocalBackend* pBackend);

        // @ 0x82BC9F60 -- if the thread was started, join it (EA::Thread::Thread::
        // WaitForEnd), then clear the started flag.
        void WaitForEnd();

        LocalBackend*             mpBackend;    // +0x04
        bool                      mbStarted;    // +0x08
        bool                      mbQuit;       // +0x09
        EA::Thread::Thread        mThread;      // +0x0C
        EA::Jobs::JobThreadParameters mParameters; // +0x10
        HANDLE                    mhEvent;      // +0x28
    };
}
}
}

#endif // EA_JOBS_LOCAL_BACKEND_JOB_THREAD_H
