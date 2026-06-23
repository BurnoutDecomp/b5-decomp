#ifndef EA_JOBS_JOB_H
#define EA_JOBS_JOB_H

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h" // JobInstanceHandle, JobEnvironment, ...
#include "SDKs/EATech/eajobs/event.h"      // EA::Jobs::Event (Event::When)

// SDKs/EATech/eajobs/job.h
//
// EA::Jobs::Job + its nested Dependency record. Reconstructed from the EATech DWARF
// (job.h) and the X360 .XEX. Only the nested Job::Dependency element type (the
// payload of BucketListNode<Job::Dependency,10>, this group's TU) is fully modelled
// here; the full Job class body is out of this group's scope and is left forward-
// declared in job_types.h. A minimal Job shell is provided so Job::Dependency has a
// valid enclosing scope without committing the rest of the (large) Job layout.
//
// Vendor EA code reconstructed in its canonical home.

namespace EA
{
namespace Jobs
{
    // job.h:35 -- the full Job class is reconstructed elsewhere; here we only
    // commit the nested Dependency type that this group's BucketListNode needs.
    struct Job
    {
        // job.h:135 -- one entry in a Job's dependency bucket list. The Add path
        // (BucketListNode<Job::Dependency,10>::Add @ 0x82BCAC20) copies 32 bytes
        // per element (4x ld/std) and a freshly allocated node's slots are zeroed
        // at offsets 0 / 8 / 0x10 / 0x14 -- i.e. mJob, mJobInstanceHandle's
        // submission id / backend / index. sizeof == 32 (0x20, the slwi r11,r11,5
        // element stride).
        //
        // Layout (8-byte aligned for the embedded u64):
        //   +0x0  mJob               (Job*)              [4B, then 4B pad]
        //   +0x8  mJobInstanceHandle (JobInstanceHandle) [16B]
        //   +0x18 mTrigger           (Event::When)       [4B, then 4B tail pad]
        struct Dependency
        {
            Dependency();
            Dependency(Job& rJob, Event::When eTrigger);
            Dependency(JobInstanceHandle hJobInstance, Event::When eTrigger);

            Job*              mJob;               // +0x0
            JobInstanceHandle mJobInstanceHandle; // +0x8
            Event::When       mTrigger;           // +0x18
        };
    };

    inline Job::Dependency::Dependency()
        : mJob(0)
        , mJobInstanceHandle()
        , mTrigger(Event::EVENT_WHEN_JOB_BEGIN)
    {
    }

    inline Job::Dependency::Dependency(Job& rJob, Event::When eTrigger)
        : mJob(&rJob)
        , mJobInstanceHandle()
        , mTrigger(eTrigger)
    {
    }

    inline Job::Dependency::Dependency(JobInstanceHandle hJobInstance, Event::When eTrigger)
        : mJob(0)
        , mJobInstanceHandle(hJobInstance)
        , mTrigger(eTrigger)
    {
    }
}
}

#endif // EA_JOBS_JOB_H
