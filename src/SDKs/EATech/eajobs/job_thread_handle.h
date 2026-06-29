#ifndef EA_JOBS_JOB_THREAD_HANDLE_H
#define EA_JOBS_JOB_THREAD_HANDLE_H

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h" // EA::Jobs::Detail::SchedulerBackend
#include "SDKs/EATech/eajobs/job_thread_parameters.h" // EA::Jobs::JobThreadParameters

// SDKs/EATech/eajobs/job_thread_handle.h
//
// EA::Jobs::JobThreadHandle -- the lightweight handle to a job-manager worker
// thread, returned by JobScheduler::AddThread. Reconstructed from the EATech DWARF
// (references/DecFIGS/dwarfdump/SDKs/EATech/include/job_manager/job_thread_handle.h).
//
// Vendor EA code reconstructed in its canonical home; members/names follow the EA
// SDK convention (mSchedulerBackend / mHandle), NOT the Brn/Cgs project convention.

namespace EA
{
namespace Jobs
{
    // job_thread_handle.h:23 -- DWARF layout:
    //   +0x0 mSchedulerBackend (Detail::SchedulerBackend*)
    //   +0x4 mHandle           (uint32_t)
    struct JobThreadHandle
    {
        // job_thread_id.h:26 -- a job-thread id is a 64-bit value.
        typedef u64 JobThreadId;

        JobThreadHandle();
        JobThreadHandle(Detail::SchedulerBackend* pBackend, u32 uHandle);

        void                RequestEnd();
        void                WaitForEnd();
        JobThreadId         GetId() const;
        JobThreadParameters GetParameters() const;
        bool                Valid() const;
        u32                 Handle() const;

    private:
        Detail::SchedulerBackend* mSchedulerBackend; // +0x0
        u32                       mHandle;           // +0x4
    };
}
}

#endif // EA_JOBS_JOB_THREAD_HANDLE_H
