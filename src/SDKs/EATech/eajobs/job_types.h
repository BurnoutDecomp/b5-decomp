#ifndef EA_JOBS_JOB_TYPES_H
#define EA_JOBS_JOB_TYPES_H

#include "types.hpp"

// SDKs/EATech/eajobs/job_types.h
//
// Shared EA Tech "job_manager" SDK value types: the JobEnvironment / JobAffinity /
// JobPriority enumerations, the JobInstanceHandle handle struct, and the
// EA::Jobs::Job forward declaration + its nested Dependency record. Reconstructed
// from the X360 .XEX (BURNOUT_X360_ARTIST.XEX) and the EATech DWARF surface in
// references/DecFIGS/dwarfdump/SDKs/EATech/include/job_manager/ (job_affinity.h,
// job_environment.h, job_priority.h, job_instance_handle.h, job.h).
//
// Vendor EA code reconstructed in its canonical home; members/enumerators follow
// the EA SDK convention (mFoo / JOB_*), NOT the Brn/Cgs project convention.

namespace EA
{
namespace Jobs
{
    // job_environment.h:15 -- which execution environment a job/thread runs in.
    enum JobEnvironment
    {
        JOB_ENVIRONMENT_LOCAL       = 0,
        JOB_ENVIRONMENT_SPU_THREADS = 1,
        JOB_ENVIRONMENT_NUM         = 2,
        JOB_ENVIRONMENT_DEFAULT     = 1
    };

    // job_affinity.h:15 -- per-processor affinity bitmask (one bit per hardware
    // thread). JOB_AFFINITY_ANY (63) is the 6-bit all-ones default used by the
    // job-thread parameter block (@ 0x828D6090 stores 0x3F).
    enum JobAffinity
    {
        JOB_AFFINITY_NONE     = 0,
        JOB_AFFINITY_0        = 1,
        JOB_AFFINITY_1        = 2,
        JOB_AFFINITY_2        = 4,
        JOB_AFFINITY_3        = 8,
        JOB_AFFINITY_4        = 16,
        JOB_AFFINITY_5        = 32,
        JOB_AFFINITY_ANY      = 63,
        JOB_AFFINITY_COUNT    = 6,
        JOB_AFFINITY_NUM_BITS = 6
    };

    // job_priority.h:15 -- scheduling priority (lower numeric == higher priority).
    enum JobPriority
    {
        JOB_PRIORITY_LOW      = 255,
        JOB_PRIORITY_MEDIUM   = 128,
        JOB_PRIORITY_HIGH     = 0,
        JOB_PRIORITY_DEFAULT  = 128,
        JOB_PRIORITY_NUM_BITS = 8
    };

    // Forward declarations for the scheduler backend and the Job class (defined in
    // job.h, not reconstructed in this group).
    struct Job;
    struct JobInstanceHandle;
    struct JobThreadHandle;
    struct JobThreadParameters;
    struct JobMetrics;
    typedef void ProfilerCallback(const JobMetrics* pMetrics, int iCount, void* pContext);
    struct EntryPoint;   // entry_point.h -- CreateNotReadyInstance's entry argument
    struct Event;

    // job_types.h -- a single 32-bit job argument word. Jobs receive four of them
    // (Job::mParams[4] / the local-job entry signature); SetData packs the data
    // pointer + size into two of the slots. The four-byte union carries a pointer,
    // an int, or a float as a raw word. Tag is `struct` to match job_scheduler.h.
    struct Param
    {
        union
        {
            void* mpValue;
            s32   miValue;
            u32   muValue;
            f32   mfValue;
        };

        Param() : mpValue(0) {}
        Param(void* lpValue) : mpValue(lpValue) {}
        Param(s32 liValue) : miValue(liValue) {}
        Param(u32 luValue) : muValue(luValue) {}
        Param(f32 lfValue) : mfValue(lfValue) {}
    };

    // wait_on.h:19/27 (DecFIGS), callback dispatch ARTIST82BC9B88.
    enum WaitOnControl
    {
        WAIT_ON_CANCEL = 0,
        WAIT_ON_CONTINUE = 1,
        WAIT_ON_YIELD_THEN_CONTINUE = 2
    };
    typedef WaitOnControl WaitOnCallback(void* pContext);

    namespace Detail
    {
        // The user yield-predicate JobInstanceHandle::WaitOn threads through to
        // WaitOnYieldHelper (declared in detail.h; redeclared here so the handle
        // surface does not pull detail.h). Nonzero == keep waiting.
        typedef WaitOnCallback* WaitOnYieldCallbackArg;

        // SchedulerBackend -- the polymorphic owner of submitted job-instance slots.
        // ARTIST base 82182408 and LocalBackend 82182500 have these 22 slots.
        class SchedulerBackend
        {
        public:
            virtual ~SchedulerBackend();                                      //00
            virtual int GetNumJobsInFlight() const = 0;                         //04
            virtual int GetNumThreads() const = 0;                              //08
            virtual JobThreadHandle GetThreadHandle(u32 uIndex) const = 0;      //0C
            virtual JobThreadHandle AddThread(const JobThreadParameters&) = 0;  //10
            virtual void RequestEnd(u64 uHandle) = 0;                           //14
            virtual void WaitForEnd(u64 uHandle) = 0;                           //18
            virtual void WakeThreads();                                        //1C
            virtual u64 GetThreadId(u64 uHandle) const = 0;                      //20
            virtual JobThreadParameters GetThreadParameters(u64 uHandle) const = 0; //24
            virtual JobInstanceHandle CreateNotReadyInstance(const EntryPoint*, const Param*) = 0; //28
            virtual void GetEnabler(u64,u64,u32*&,u32&) = 0;                     //2C
            virtual int IsJobComplete(u64,u64) = 0;                             //30
            virtual int SubmitEvent(u64,u64,const void*,int) = 0;               //34
            virtual Event AddBarrier(u64,u64) = 0;                              //38
            virtual int SleepOn(u64,u64) = 0;                                   //3C
            virtual int GetAllowSleepOn(u64,u64) = 0;                           //40
            virtual void SetProfilingCallback(ProfilerCallback*);              //44
            virtual void SetProfilingContext(void*);                            //48
            virtual void FlushProfile();                                       //4C
            virtual bool ExecuteReadyJobs(int);                                //50
            virtual u32 GetSleepTimeoutMs() = 0;                                //54
        };

    }

    // job_instance_handle.h:21 -- the lightweight handle to a submitted job
    // instance living in a scheduler backend's slot table.
    //
    // X360 layout (job_instance_handle.h DWARF, 8-byte aligned -> sizeof 16):
    //   +0x0 mSubmissionId     (uint64_t)
    //   +0x8 mSchedulerBackend (Detail::SchedulerBackend*)
    //   +0xC mIndex            (uint16_t)
    //   +0xE mPadding          (uint16_t)
    struct JobInstanceHandle
    {
        JobInstanceHandle();
        JobInstanceHandle(Detail::SchedulerBackend* pBackend, u16 uIndex, u64 uSubmissionId);

        // @ 0x82BC9A50 -- return a barrier event for THIS instance. Signaling
        // its final outstanding barrier makes the instance ready.
        Event AddBarrier();

        // @ 0x82BCA470 -- wait until complete, or until the callback/watchdog
        // cancels. DWARF pins void return and pointer context. Job::WaitOn's
        // tail branch preserves r4/r5/r6; game callers supply null/null/-1.
        void WaitOn(WaitOnCallback* pYieldCallback = 0,
                    void*          pYieldContext = 0,
                    s32            lYieldSleepMs = -1) const;

        u64                       mSubmissionId;     // +0x0
        Detail::SchedulerBackend* mSchedulerBackend; // +0x8
        u16                       mIndex;            // +0xC
        u16                       mPadding;          // +0xE
    };

    inline JobInstanceHandle::JobInstanceHandle()
        : mSubmissionId(0)
        , mSchedulerBackend(0)
        , mIndex(0)
    {
        // ARTIST82BCC190..198 leaves the final padding word untouched.
    }

    inline JobInstanceHandle::JobInstanceHandle(Detail::SchedulerBackend* pBackend,
                                                u16 uIndex, u64 uSubmissionId)
        : mSubmissionId(uSubmissionId)
        , mSchedulerBackend(pBackend)
        , mIndex(uIndex)
    {
        // ARTIST82BCC1C8..1D0 writes only submission id, backend and index.
    }
}
}

#endif // EA_JOBS_JOB_TYPES_H
