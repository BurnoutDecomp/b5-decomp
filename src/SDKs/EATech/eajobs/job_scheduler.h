#ifndef EA_JOBS_JOB_SCHEDULER_H
#define EA_JOBS_JOB_SCHEDULER_H

#include "types.hpp"
#include "eathread/eathread_futex.h"                    // EA::Thread::Futex (CRITICAL_SECTION-backed lock)
#include "SDKs/EATech/eajobs/job_types.h"               // JobEnvironment, JobAffinity, Detail::SchedulerBackend, JobInstanceHandle, Job
#include "SDKs/EATech/eajobs/job_thread_parameters.h"   // JobThreadParameters
#include "SDKs/EATech/eajobs/job_thread_handle.h"       // JobThreadHandle

// SDKs/EATech/eajobs/job_scheduler.h
//
// EA::Jobs::JobScheduler -- the top-level job-manager facade. Reconstructed from
// the EATech DWARF
// (references/DecFIGS/dwarfdump/SDKs/EATech/include/job_manager/job_scheduler.h).
// The X360 .XEX drives this from CgsSystem::HardwareInit::InitializeHardware
// (SetProfiling / Initialize / AddThread call sites @ 0x828E06D4..0x828E075C).
//
// Vendor EA code reconstructed in its canonical home; members follow the EA SDK
// convention (mSchedulers / mLock / mEnableProfiling / mInitialized), NOT the
// Brn/Cgs project convention. Member function bodies live in the (out-of-group)
// job_scheduler.cpp vendor TU -- this header is the declaration surface the rest
// of the SDK and the engine call BY NAME.

namespace EA
{
namespace Jobs
{
    // Forward declarations for the submission/profiling surface (defined in the
    // out-of-group job_manager headers, not reconstructed here).
    struct EntryPoint;
    struct Param;
    struct JobMetrics;
    struct WaitOnCallback;

    // job_scheduler.h:35 -- DWARF member layout:
    //   mSchedulers[2] (Detail::SchedulerBackend*)
    //   mLock          (EA::Thread::Futex)
    //   mEnableProfiling (bool)
    //   mInitialized     (bool)
    class JobScheduler
    {
    public:
        // profiling.h:32 -- the user profiler callback signature.
        typedef void (*ProfilerCallback)(const JobMetrics* pMetrics, int iThread, void* pContext);

        JobScheduler();
        ~JobScheduler();

        // job_scheduler.h:45 -- enable/disable per-job profiling instrumentation.
        void SetProfiling(bool lbEnable);

        // job_scheduler.h:46 -- bring up the scheduler with the given worker/job
        // capacities (HardwareInit passes 128, 128).
        void Initialize(int iMaxThreads, int iMaxJobs);

        // job_scheduler.h:47 -- tear the scheduler down.
        void Destroy();

        // job_scheduler.h:59 -- spawn a worker thread described by lrParameters.
        JobThreadHandle AddThread(const JobThreadParameters& lrParameters);

        // job_scheduler.h:60/61 -- worker-thread introspection.
        JobThreadHandle GetThreadHandle(JobEnvironment leEnvironment, int iIndex) const;
        int             GetNumThreads(JobEnvironment leEnvironment) const;

        // job_scheduler.h:67/68/69 -- profiling controls.
        void SetProfilingCallback(ProfilerCallback lpCallback);
        void SetProfilingContext(void* lpContext);
        void FlushProfile();

        // job_scheduler.h:70/72 -- maintenance.
        void RunGarbageCollector();
        void WakeThreads(JobEnvironment leEnvironment);

        // job_scheduler.h:74/75 -- per-environment sleep timeout.
        size_t GetJobThreadSleepTimeoutMS(JobEnvironment leEnvironment) const;
        void   SetJobThreadSleepTimeoutMS(JobEnvironment leEnvironment, size_t luTimeoutMS);

    private:
        // Non-copyable (DWARF declares the copy ctor / operator= private).
        JobScheduler(const JobScheduler&);
        JobScheduler& operator=(const JobScheduler&);

        Detail::SchedulerBackend* mSchedulers[2]; // job_scheduler.h:83
        EA::Thread::Futex         mLock;           // job_scheduler.h:84
        bool                      mEnableProfiling; // job_scheduler.h:85
        bool                      mInitialized;     // job_scheduler.h:86
    };
}
}

#endif // EA_JOBS_JOB_SCHEDULER_H
