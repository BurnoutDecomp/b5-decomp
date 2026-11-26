// XDK size optimization: ARTIST compiled the EA job manager for size (record-form cr0
// tests), as job_thread.cpp and job_instance_handle.cpp already note.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif
#include "SDKs/EATech/eajobs/job.h" // EA::Jobs::Job (+ Dependency), BucketListNode, Event
#include "SDKs/EATech/eajobs/job_scheduler.h" // JobScheduler::mSchedulers (INTERNAL_AddNotReady)

#include <cstring> // std::memcpy (Clear's 44-byte EntryPoint default block)

// ============================================================================
// SDKs/EATech/eajobs/job.cpp
//
// EA::Jobs::Job methods, reconstructed store-for-store from the X360 .XEX
// (BURNOUT_X360_ARTIST.XEX):
//   Job::Clear                      @ 0x82BCA110
//   Job::Job(const char*)           @ 0x82BCB0C0
//   Job::~Job                       @ 0x82BCB1A8
//   Job::SetData                    @ 0x82BC9978
//   Job::DependsOn(Job&,When)       @ 0x82BCB280
//   Job::GetDependency              @ 0x82BCA258  (+ chain walker @ 0x82BCA078)
//   Job::GetDependent               @ 0x82BCA2A0
//   Job::GetNumDependents           @ 0x82BCA390
//   Job::IsDone                     @ 0x82BCA2E8
//   Job::SleepOn                    @ 0x82BCA1F8
//   Job::WaitOn                     @ 0x82BCB238
//   Job::INTERNAL_AddNotReady       @ 0x82BCA3D8
//   Job::INTERNAL_SubmitEventsAndDeps @ 0x82BCB2F0
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
    // Offsets quoted here and in job.h use the native four-byte pointer layout.

    // EntryPoint default-settings block. Clear() @ 0x82BCA110 memcpy's a 44-byte
    // default over mEntryPoint (Job+4): priority 128 (JOB_PRIORITY_DEFAULT, the
    // li 0x80 at +0x10), affinity 63 (JOB_AFFINITY_ANY, li 0x3F at +0x14),
    // code-recycle 1 (the li 1 at +0x20), everything else zero. The block is built
    // here field-by-field so no struct-internal layout is hardcoded.
    static EntryPoint MakeDefaultEntryPoint() { return EntryPoint(); }

    // @ 0x82BCA110 -- reset the job to its default (empty) state.
    void Job::Clear()
    {
        // memcpy a 44-byte EntryPoint default block over mEntryPoint (Job+4).
        EntryPoint lDefault = MakeDefaultEntryPoint();
        std::memcpy(&mEntryPoint, &lDefault, sizeof(mEntryPoint));

        mHasJobDependency = false; // stb 0,0x50
        mSeen             = false; // stb 0,0(this)

        // mParams[4] -> 0 (stw 0 at +0x30/+0x34/+0x38/+0x3C).
        mParams[0] = Param();
        mParams[1] = Param();
        mParams[2] = Param();
        mParams[3] = Param();

        // Free + reset the dependency overflow chain (destruct mNext if any), then the
        // dependents chain, then both event lists. The X360 walks each list's overflow
        // node (the scalar-deleting destructors) and nulls mNext/mSize -- BucketListNode::Clear.
        mDependencies.Clear();
        mDependents.Clear();
        for (int liList = 0; liList < 2; ++liList) // do { ... } while(--v6) over mEvents[2]
            mEvents[liList].Clear();
    }

    // @ 0x82BCB0C0 -- construct a named, empty job.
    //
    // The X360 ctor pre-seeds the EntryPoint defaults inline, zero-inits the instance
    // handle and both event lists' bucket nodes, runs the 2-iteration event-list
    // `vector constructor iterator`, then calls Clear() (which memcpy's the canonical
    // EntryPoint default block back over the pre-seed) and EntryPoint::SetName. Modelled
    // here by default-constructing the sub-objects (the same zero/empty state), then
    // Clear() + SetName() -- matching the asm's call order and final state.
    Job::Job(const char* lpcName)
        : mSeen(false)
        , mEntryPoint()
        , mJobInstanceHandle()
        , mHasJobDependency(false)
        , mDependencies()
        , mDependents()
        , mEvents()
        , mStartEvent()
    {
        Clear();
        mEntryPoint.SetName(lpcName);
    }

    // @ 0x82BCB1A8 -- destroy the job: free the event/dependency/dependent overflow
    // chains. The X360 destructs mEvents[1..0] (the two 0xB0-stride nodes walked from
    // +0x340-0xB0), then the dependents chain (+0x1B0), then the dependencies chain
    // (+0x60), nulling each list's mNext/mSize. C++ reverse-declaration member
    // destruction visits the same nodes; the explicit Clear() calls mirror the asm's
    // per-list overflow free so the trailing member dtors are no-ops.
    Job::~Job()
    {
        for (int liList = 1; liList >= 0; --liList) // v3 walks mEvents[1] then mEvents[0]
            mEvents[liList].Clear();
        mDependents.Clear();
        mDependencies.Clear();
    }

    // @ 0x82BC9978 -- attach the job's data block (stored into two of mParams).
    void Job::SetData(void* lpvData, size_t luSize)
    {
        mParams[1].mpValue = lpvData;                       // stw r4,0x34
        mParams[2].muValue = static_cast<u32>(luSize);      // stw r5,0x38
    }

    // @ 0x82BCB280 -- declare that THIS job depends on rOther firing eTrigger.
    void Job::DependsOn(Job& rOther, Event::When eTrigger)
    {
        mHasJobDependency = true; // stb 1,0x50

        // Record the forward edge on THIS job: a Dependency naming rOther + trigger.
        Dependency lDependency(rOther, eTrigger);
        mDependencies.Add(lDependency);

        // Record the back-edge on rOther: a Job* dependent pointing at this.
        Job* lpSelf = this;
        rOther.mDependents.Add(lpSelf);
    }

    // DWARF-surface overload (job.h:62). The DependsOn(JobInstanceHandle, When) form
    // has NO standalone X360 address/asm in this TU's dossier, so its body is not
    // reconstructed here -- declared only to satisfy the header's DWARF surface.
    // (Reconstruct it if/when a ground-truth address surfaces; do not fabricate it.)

    // @ 0x82BCA258 -- the i-th dependency's owning Job.
    Job* Job::GetDependency(int iIndex) const
    {
        return mDependencies[iIndex].mJob;
    }

    // @ 0x82BCA2A0 -- the i-th dependent (back-pointer) Job.
    Job* Job::GetDependent(int iIndex) const
    {
        return mDependents[iIndex];
    }

    int Job::GetNumDependencies() const
    {
        return static_cast<int>(mDependencies.ListSize());
    }

    // @ 0x82BCA390 -- how many dependents across the bucket chain.
    int Job::GetNumDependents() const
    {
        u32 luOverflow = 0;
        if (mDependents.mNext)
            luOverflow = mDependents.mNext->ListSize();
        return static_cast<int>(mDependents.mSize + luOverflow);
    }

    // @ 0x82BCA2E8 -- has the submitted instance completed (or is there no instance)?
    bool Job::IsDone() const
    {
        // Guard: only ask the backend while a backend is attached AND the handle still
        // names a live instance (index or submission id set); otherwise treat as done.
        if (mJobInstanceHandle.Valid())
        {
            return mJobInstanceHandle.mSchedulerBackend->IsDone(mJobInstanceHandle);
        }
        return true;
    }

    // @ 0x82BCA1F8 -- block on the backend's "sleep on instance" entry (vtable +0x3C).
    void Job::SleepOn()
    {
        // Same liveness guard as IsDone; on a live instance, tail-call the backend's
        // BLOCKING sleep-on-instance slot (+0x3C) -- NOT IsDone (+0x30).
        if (mJobInstanceHandle.Valid())
        {
            mJobInstanceHandle.mSchedulerBackend->SleepOn(mJobInstanceHandle);
        }
    }

    // @ 0x82BCB238 -- spin-wait on this job's instance handle until done.
    void Job::WaitOn(WaitOnCallback* pCallback, void* pContext, s32 lSleepMs) const
    {
        // Liveness guard read off mJobInstanceHandle (the +0x40 handle), then forward
        // to JobInstanceHandle::WaitOn, preserving r4/r5/r6 through the tail call.
        if (mJobInstanceHandle.Valid())
        {
            mJobInstanceHandle.WaitOn(pCallback, pContext, lSleepMs);
        }
    }

    // @ 0x82BCA3D8 -- (scheduler-internal) record THIS job's instance handle from the
    // backend slot just selected for it, and seed mStartEvent's barrier.
    void Job::INTERNAL_AddNotReady(JobScheduler& rScheduler)
    {
        // The backend is chosen by the job's environment word: the X360 indexes
        // (4 * mEntryPoint.mEnvironment) into the backend array, then dispatches that
        // backend's vtable slot +0x28 (AddNotReady) with an out-handle stack
        // temp, the entry point and the param block.
        JobEnvironment leEnvironment = mEntryPoint.GetEnvironment();
        Detail::SchedulerBackend* lpBackend = rScheduler.mSchedulers[leEnvironment];

        JobInstanceHandle lNewHandle = lpBackend->AddNotReady(&mEntryPoint, mParams);

        // Store the 16-byte handle into mJobInstanceHandle (the two `std` stores into
        // this+0x40 / this+0x48).
        mJobInstanceHandle = lNewHandle;

        // Build a barrier handle against this instance and copy it (four words) into
        // mStartEvent (+0x340). The X360 AddBarrier(&temp, &mJobInstanceHandle) makes a
        // fresh handle wait on this job's instance; its returned 16 bytes seed mStartEvent.
        mStartEvent=mJobInstanceHandle.AddBarrier();
    }

    // @ 0x82BCB2F0 -- (scheduler-internal) push every dependency-edge barrier and every
    // begin/end event into the backend so the submitted instance fires them.
    void Job::INTERNAL_SubmitEventsAndDeps(JobScheduler& /*rScheduler*/)
    {
        // ---- Dependencies -------------------------------------------------------
        // total = mDependencies.mSize + (mDependencies.mNext ? mNext->ListSize() : 0);
        // walk the dependency chain only if total != 0 (do NOT double-add mSize).
        u32 luOverflow = 0;
        if (mDependencies.mNext)
            luOverflow = mDependencies.mNext->ListSize();
        u32 luTotalDeps = mDependencies.mSize + luOverflow;

        if (luTotalDeps != 0)
        {
            const Detail::BucketListNode<Dependency, 10>* lpNode = &mDependencies;
            int liIndex = -1;
            for (;;)
            {
                // Advance through the bucket chain one slot at a time; cross to the
                // overflow node once we run off the current node's mSize.
                ++liIndex;
                bool lbHaveSlot;
                if (liIndex >= static_cast<int>(lpNode->mSize))
                {
                    if (lpNode->mNext == 0)
                        lbHaveSlot = false;
                    else
                    {
                        lpNode    = lpNode->mNext;
                        liIndex   = 0;
                        lbHaveSlot = true;
                    }
                }
                else
                {
                    lbHaveSlot = true;
                }
                if (!lbHaveSlot)
                    break;

                const Dependency& lrDependency = lpNode->mBucket[liIndex];

                // A dependency edge fires UNCONDITIONALLY once dep.mJob is non-null:
                // barrier THIS job's handle against the dependency job's instance, then
                // submit the edge through the dependency job's backend (vtable +0x34).
                if (lrDependency.mJob != 0)
                {
                    Event lBarrier=mJobInstanceHandle.AddBarrier();

                    const Job* lpDepJob = lrDependency.mJob;
                    Detail::SchedulerBackend* lpBackend =
                        lpDepJob->mJobInstanceHandle.mSchedulerBackend;
                    lpBackend->AddEvent(
                        lpDepJob->mJobInstanceHandle,
                        lBarrier,
                        lrDependency.mTrigger);
                }

                // The dependency may ALSO carry a direct job-instance handle (mJob == 0
                // form): submit it when the handle names a live instance.
                if (lrDependency.mJobInstanceHandle.Valid())
                {
                    Event lBarrier=mJobInstanceHandle.AddBarrier();

                    Detail::SchedulerBackend* lpBackend =
                        lrDependency.mJobInstanceHandle.mSchedulerBackend;
                    lpBackend->AddEvent(
                        lrDependency.mJobInstanceHandle,
                        lBarrier,
                        lrDependency.mTrigger);
                }
            }
        }

        // ---- Events -------------------------------------------------------------
        // For each phase list (mEvents[0] begin, mEvents[1] end), submit EVERY event in
        // the chain through THIS job's backend (vtable +0x34) so the instance fires it
        // at the right phase. The event dispatch fires for EVERY event (no backend guard).
        for (int liPhase = 0; liPhase < 2; ++liPhase)
        {
            const Detail::BucketListNode<Event, 10>* lpNode = &mEvents[liPhase];
            int liIndex = -1;
            for (;;)
            {
                ++liIndex;
                bool lbHaveSlot;
                if (liIndex >= static_cast<int>(lpNode->mSize))
                {
                    if (lpNode->mNext == 0)
                        lbHaveSlot = false;
                    else
                    {
                        lpNode     = lpNode->mNext;
                        liIndex    = 0;
                        lbHaveSlot = true;
                    }
                }
                else
                {
                    lbHaveSlot = true;
                }
                if (!lbHaveSlot)
                    break;

                Detail::SchedulerBackend* lpBackend =
                    mJobInstanceHandle.mSchedulerBackend;
                lpBackend->AddEvent(
                    mJobInstanceHandle,
                    lpNode->mBucket[liIndex],
                    static_cast<Event::When>(liPhase));
            }
        }
    }

}
}
