#ifndef EA_JOBS_JOB_THREAD_PARAMETERS_H
#define EA_JOBS_JOB_THREAD_PARAMETERS_H

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h" // JobEnvironment, JobAffinity

// SDKs/EATech/eajobs/job_thread_parameters.h
//
// EA::Jobs::JobThreadParameters -- the parameter block describing a job-manager
// worker thread. Reconstructed from the X360 .XEX (ctor @ 0x828D6090) and the
// EATech DWARF (job_thread_parameters.h).
//
// Vendor EA code reconstructed in its canonical home; members follow the EA SDK
// convention (the DWARF field names environment/stackSize/priority/processor/
// affinity/ptrToName), NOT the Brn/Cgs project convention.

namespace EA
{
namespace Jobs
{
    // job_thread_parameters.h:19 -- X360 layout (ctor store order @ 0x828D6090):
    //   +0x0  environment (JobEnvironment) -- set to JOB_ENVIRONMENT_LOCAL (0)
    //   +0x4  stackSize   (int)            -- from EA::Thread::ThreadParameters
    //   +0x8  priority    (int)            -- from EA::Thread::ThreadParameters
    //   +0xC  processor   (int)            -- from EA::Thread::ThreadParameters
    //   +0x10 affinity    (JobAffinity)    -- JOB_AFFINITY_ANY (0x3F)
    //   +0x14 ptrToName   (const char*)    -- "Job Manager - Job Thread"
    struct JobThreadParameters
    {
        // @ 0x828D6090 -- default-constructs an EA::Thread::ThreadParameters,
        // adopts its stack-size/priority/processor defaults, then sets the
        // job-thread defaults (local environment, any-affinity, the fixed name).
        JobThreadParameters();

        JobEnvironment environment; // +0x0
        int            stackSize;   // +0x4
        int            priority;    // +0x8
        int            processor;   // +0xC
        JobAffinity    affinity;    // +0x10
        const char*    ptrToName;   // +0x14
    };
}
}

#endif // EA_JOBS_JOB_THREAD_PARAMETERS_H
