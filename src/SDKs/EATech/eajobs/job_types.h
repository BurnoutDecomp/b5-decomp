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

    // Forward declarations for the scheduler backend (an opaque .cpp-local type in
    // the SDK) and the Job class (defined in job.h, not reconstructed in this group).
    namespace Detail { class SchedulerBackend; }
    struct Job;

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
