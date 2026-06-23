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
//   +0x04 mpBackend     (Backend*)        -- the LocalBackend this worker drives
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
    // The backend JobThread::Run drives. Only the two virtual entry points Run
    // dispatches into are committed here (the X360 asm proves the call sites:
    // backend vtable slot +0x50 to run any ready jobs for an affinity, slot +0x54 to
    // get the sleep timeout before the next wake). The full LocalBackend is
    // reconstructed elsewhere; this is the minimal abstract surface Run calls BY
    // NAME without committing the (out-of-group) backend layout.
    class Backend
    {
    public:
        virtual ~Backend() {}

        // vtable slot +0x50: try to execute ready jobs for the given affinity mask.
        // Returns nonzero if it did work (so Run loops again immediately without
        // sleeping); 0 if there was nothing to do.
        virtual int ExecuteReadyJobs(int iAffinity) = 0;

        // vtable slot +0x54: how long (ms) to wait on the wake event before the next
        // execute pass when there was no work.
        virtual u32 GetSleepTimeoutMs() = 0;
    };

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

        // @ 0x82BC9F60 -- if the thread was started, join it (EA::Thread::Thread::
        // WaitForEnd), then clear the started flag.
        void WaitForEnd();

        Backend*                  mpBackend;    // +0x04
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
