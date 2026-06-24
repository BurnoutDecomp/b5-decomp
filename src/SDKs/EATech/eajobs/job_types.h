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

    namespace Detail
    {
        // The user yield-predicate JobInstanceHandle::WaitOn threads through to
        // WaitOnYieldHelper (declared in detail.h; redeclared here so the handle
        // surface does not pull detail.h). Nonzero == keep waiting.
        typedef int (*WaitOnYieldCallbackArg)(int iContext);

        // SchedulerBackend -- the polymorphic owner of submitted job-instance slots.
        // Only the two virtual entry points JobInstanceHandle dispatches into are
        // committed here (the X360 asm proves the call sites: vtable slot +0x30 for
        // the completion query, slot +0x38 for adding a barrier). The full backend
        // (LocalBackend etc.) is reconstructed elsewhere; this is the minimal
        // abstract surface the handle calls BY NAME. The polymorphic interface keeps
        // the named virtual dispatch the binary performs without committing the rest
        // of the backend's (out-of-group) vtable/layout.
        class SchedulerBackend
        {
        public:
            // Polymorphic base destructor. Defined OUT-OF-LINE in
            // job_instance_handle.cpp so the interface owns a real .cpp definition
            // home -- the X360 base scalar-deleting destructor @ 0x82BC9A08 (stores
            // the SchedulerBackend vtable off_82182408 into *this, then for the
            // deleting variant conditionally operator-delete's). The body is empty
            // (no members, no base); the vtable store + delete-tail is what the
            // compiler emits.
            virtual ~SchedulerBackend();

            // vtable slot +0x30: is the instance identified by (uSubmissionId,
            // uHandleQword==packed backend+index) finished? Nonzero == complete.
            // (X360 passes the submission id and the packed +0x8 qword as a 64-bit
            // handle key.)
            virtual int IsJobComplete(u64 uSubmissionId, u64 uHandleQword) = 0;

            // vtable slot +0x38: establish a barrier so the dependent job-instance handle
            // waits on the instance keyed by (uSubmissionId, uHandleQword). The X360
            // dispatches this through the *dependency's* backend vtable, handing it the
            // dependent handle, the backend, and the dependency's submission key.
            virtual void AddBarrier(JobInstanceHandle* pDependentHandle, u64 uSubmissionId, u64 uHandleQword) = 0;
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

        // @ 0x82BC9A50 -- make THIS handle's instance wait on the dependency instance
        // (the argument). The X360 reads the backend + submission key from the DEPENDENCY
        // handle (a2), then dispatches that backend's vtable slot +0x38 with this handle
        // as the dependent. Returns *this.
        JobInstanceHandle& AddBarrier(const JobInstanceHandle& lrDependency);

        // @ 0x82BCA470 -- block until this submitted job instance has completed (or a
        // watchdog budget gives up). Each spin: ask the backend whether the instance
        // is complete (vtable slot +0x30); if not, run one WaitOnYieldHelper pass
        // (optional user predicate / sleep / elapsed-time budget). Returns nonzero
        // while it should keep waiting; 0 once the wait is satisfied/abandoned.
        //   pYieldCallback : optional predicate run each spin (nonzero == keep waiting)
        //   iYieldContext  : argument forwarded to pYieldCallback
        //   lYieldSleepMs  : if >= 0, ms to sleep each spin
        int WaitOn(Detail::WaitOnYieldCallbackArg pYieldCallback,
                   int                            iYieldContext,
                   s32                            lYieldSleepMs);

        u64                       mSubmissionId;     // +0x0
        Detail::SchedulerBackend* mSchedulerBackend; // +0x8
        u16                       mIndex;            // +0xC
        u16                       mPadding;          // +0xE
    };

    inline JobInstanceHandle::JobInstanceHandle()
        : mSubmissionId(0)
        , mSchedulerBackend(0)
        , mIndex(0)
        , mPadding(0)
    {
    }

    inline JobInstanceHandle::JobInstanceHandle(Detail::SchedulerBackend* pBackend,
                                                u16 uIndex, u64 uSubmissionId)
        : mSubmissionId(uSubmissionId)
        , mSchedulerBackend(pBackend)
        , mIndex(uIndex)
        , mPadding(0)
    {
    }
}
}

#endif // EA_JOBS_JOB_TYPES_H
