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
        // job.h:42 -- construct a named, empty job. X360 0x82BCB0C0 (inits the entry
        // defaults + both dependency/event bucket lists, then Clear()s and SetName()s).
        explicit Job(const char* lpcName);
        // job.h:43 -- tear the job down: destroy the event/dependency/dependent bucket
        // chains. X360 0x82BCB1A8.
        ~Job();

        // job.h:51  -- reset the job to its default (empty) state. X360 0x82BCA110.
        void Clear();
        // job.h:81  -- point the job's entry at code (forwards to mEntryPoint.SetCode).
        void SetCode(JobEnvironment leEnvironment, const void* lpvCode, int liSize);
        // job.h:83  -- name the job (forwards to mEntryPoint.SetName).
        void SetName(const char* lpcName);
        // job.h:99  -- attach the job's data block (X360 0x82BC9978 stores it in mParams).
        void SetData(void* lpvData, size_t luSize);

        // job.h:60 -- declare that THIS job depends on rOther firing eTrigger before it
        // may run; records the edge on both sides. X360 0x82BCB280.
        void DependsOn(Job& rOther, Event::When eTrigger);
        // job.h:62 -- depend on an already-submitted instance (DWARF surface overload;
        // no standalone X360 body in this build -- see job.cpp).
        void DependsOn(JobInstanceHandle hInstance, Event::When eTrigger);

        // job.h:70 -- the i-th dependency's owning Job. X360 0x82BCA258 (+ the chain
        // walker Job::Depende @ 0x82BCA078).
        Job* GetDependency(int iIndex) const;
        // job.h:72 -- the i-th dependent (back-pointer) Job. X360 0x82BCA2A0.
        Job* GetDependent(int iIndex) const;
        // job.h:74 -- how many dependents this job has across its bucket chain. X360 0x82BCA390.
        int  GetNumDependents() const;

        // job.h:90 -- has this job's submitted instance completed (or no instance)? X360 0x82BCA2E8.
        bool IsDone() const;
        // job.h:92 -- block on the backend's "sleep on instance" entry until done. X360 0x82BCA1F8.
        void SleepOn();
        // job.h:94 -- spin-wait on this job's instance handle until done. X360 0x82BCB238.
        void WaitOn();

        // job.h:110 -- (scheduler-internal) record THIS job's instance handle from the
        // backend's just-submitted slot and seed mStartEvent's barrier. X360 0x82BCA3D8.
        JobInstanceHandle* INTERNAL_AddNotReady(Detail::SchedulerBackend** pBackends);
        // job.h:112 -- (scheduler-internal) push every dependency-edge barrier and every
        // begin/end event into the backend so the instance fires them. X360 0x82BCB2F0.
        void INTERNAL_SubmitEventsAndDeps();

        // Accessors (declaration-only; bodies live in the vendor job TU).
        const EntryPoint& GetEntryPoint() const { return mEntryPoint; }
        EntryPoint&       GetEntryPoint()       { return mEntryPoint; }

    private:
        // job.h -- chain walker behind GetDependency: advance to the bucket node that
        // owns iIndex (fixed capacity-10 hops, matching the X360 helper @ 0x82BCA078),
        // returning that node's slot. Static so it can recurse over a raw node pointer.
        static const Dependency* IndexDependencyChain(
            const Detail::BucketListNode<Dependency, 10>* pNode, int iIndex);

    public:

        // Object layout. The +0x.. offsets below are the X360 object offsets proven by
        // the Clear/ctor/dtor/SubmitEventsAndDeps asm; the reconstruction models every
        // member BY NAME (no raw-offset reads). NOTE: the X360 had 4-byte pointers, so on
        // the 64-bit host the embedded JobInstanceHandle/Param/EntryPoint pointers widen
        // and the byte offsets are NOT reproduced verbatim -- the layout is therefore
        // shape-faithful, not host-ABI byte-exact (matching the committed siblings, which
        // pack the handle key from named members instead of reading raw +0x48). Member
        // declaration ORDER is preserved exactly so the (X360) layout and the C++ member
        // sequence agree; natural alignment supplies any inter-member padding.
        bool              mSeen;              // +0x00  job.h:118
        EntryPoint        mEntryPoint;        // +0x04  job.h:127 (44 bytes on X360)
        Param             mParams[4];         // +0x30  job.h:128
        JobInstanceHandle mJobInstanceHandle; // +0x40  job.h:130
        bool              mHasJobDependency;  // +0x50  job.h:132

        // The dependency/dependents/event bucket-list machinery, modelled by name.
        // X360 offsets (verified against the asm `addi rN,base,<off>`):
        //   mDependencies @ +0x60  (BucketListNode<Dependency,10>: mNext @ +0x1A0,
        //                           mSize @ +0x1A4; sizeof 0x150 -> ends +0x1B0)
        //   mDependents   @ +0x1B0 (BucketListNode<Job*,6>: mNext @ +0x1C8,
        //                           mSize @ +0x1CC; sizeof 0x28 -> ends +0x1D8)
        //   mEvents[2]    @ +0x1E0 (BucketListNode<Event,10>, 0xB0 stride -> +0x340)
        //   mStartEvent   @ +0x340 (Event, 16 bytes -> ends +0x350; sizeof(Job)==0x350,
        //                           the AddJobs stride @ 0x82BCB498)
        Detail::BucketListNode<Dependency, 10> mDependencies;   // +0x60
        Detail::BucketListNode<Job*, 6>        mDependents;     // +0x1B0
        Detail::BucketListNode<Event, 10>      mEvents[2];      // +0x1E0
        Event                                  mStartEvent;     // +0x340
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
