#ifndef EA_JOBS_JOB_H
#define EA_JOBS_JOB_H

#include "types.hpp"
#include "SDKs/EATech/eajobs/job_types.h" // JobInstanceHandle, JobEnvironment, Param, ...
#include "SDKs/EATech/eajobs/event.h"      // EA::Jobs::Event + Detail::BucketListNode
#include "SDKs/EATech/eajobs/entry_point.h"// EA::Jobs::EntryPoint (Job::mEntryPoint)

// SDKs/EATech/eajobs/job.h
//
// EA::Jobs::Job + its nested Dependency record. Reconstructed from the EATech DWARF
// (job.h) and the X360 .XEX. The nested Job::Dependency element type (the payload of
// BucketListNode<Job::Dependency,10>) is modelled fully; the enclosing Job is now
// modelled with its X360 layout so callers can embed a Job BY VALUE and drive it by
// name (Clear / SetCode / SetName / SetData), as CgsResource::DecompressionJobInterface
// does (its mJob sits at +0x180, the next member mpEntries at +0x4D0, so sizeof(Job)
// == 0x350 == 848 -- the exact AddJobs stride @ 0x82BCB498).
//
// X360 LAYOUT (job.h DWARF + Job::Clear @ 0x82BCA110 / Job::SetData @ 0x82BC9978 asm):
//   +0x00 mSeen              (bool; mEntryPoint follows at +4)
//   +0x04 mEntryPoint        (EntryPoint, 44 bytes -- Clear memcpy's a 44-byte default)
//   +0x30 mParams[4]         (Param[4]; SetData stores data@+0x34, size@+0x38)
//   +0x40 mJobInstanceHandle (JobInstanceHandle, 16 bytes)
//   +0x50 mHasJobDependency  (bool; Clear writes 0 at +0x50)
//   +0x54 .. the dependency/dependents/event bucket lists + start event, to 0x350.
// The bucket-list/event tail (mDependencies / mDependents / mEvents / mStartEvent) is
// the heavy BucketListNode template machinery this TU never touches by name; it is
// preserved as explicit named padding to hold sizeof(Job) == 848 without committing
// the template instantiations here (they are emitted in bucket_list_node.cpp).
//
// Vendor EA code reconstructed in its canonical home.

namespace EA
{
namespace Jobs
{
    class JobScheduler;

    // job.h:35 -- the Job descriptor (see the layout note above). The nested
    // Dependency element type follows.
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

        // --- Job API (X360-attested subset the engine drives by name) ---------------
        // job.h:51  -- reset the job to its default (empty) state. X360 0x82BCA110.
        void Clear();
        // job.h:81  -- point the job's entry at code (forwards to mEntryPoint.SetCode).
        void SetCode(JobEnvironment leEnvironment, const void* lpvCode, int liSize);
        // job.h:83  -- name the job (forwards to mEntryPoint.SetName).
        void SetName(const char* lpcName);
        // job.h:99  -- attach the job's data block (X360 0x82BC9978 stores it in mParams).
        void SetData(void* lpvData, size_t luSize);

        // Accessors (declaration-only; bodies live in the vendor job TU).
        const EntryPoint& GetEntryPoint() const { return mEntryPoint; }
        EntryPoint&       GetEntryPoint()       { return mEntryPoint; }

        // X360 object layout (see the header note). Members the reconstructed engine
        // code accesses are named; the bucket-list/event tail is held as named padding.
        bool              mSeen;              // +0x00 job.h:118
        EntryPoint        mEntryPoint;        // +0x04 job.h:127 (44 bytes on X360)
        Param             mParams[4];         // +0x30 job.h:128
        JobInstanceHandle mJobInstanceHandle; // +0x40 job.h:130
        bool              mHasJobDependency;  // +0x50 job.h:132
        u8                maPad0[3];          // +0x51 align the bucket-list tail to +0x54
        // +0x54 .. +0x34F : mDependencies (BucketListNode<Dependency,10>),
        // mDependents (BucketListNode<Job*,6>), mEvents (BucketListNode<Event,10>[2])
        // and mStartEvent (Event) -- the template-heavy bucket-list machinery this
        // build's reconstructed callers never reference by name. Held as explicit
        // named padding so sizeof(Job) == 0x350 (848, the AddJobs stride) without
        // pulling the BucketListNode<...> instantiations into every job.h includer.
        u8                maDependencyAndEventLists[0x350 - 0x54];
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
