#include "SDKs/EATech/eajobs/job.h" // EA::Jobs::Job (+ Dependency), BucketListNode, Event

#include <cstring> // std::memcpy (Clear's 44-byte EntryPoint default block)
#include <stdint.h> // uintptr_t

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
    // NOTE on layout: the +0x.. offsets quoted throughout this TU and in job.h are the
    // X360 object offsets (4-byte pointers). On the 64-bit MSVC host the embedded
    // pointers/handles widen, so the byte offsets are NOT reproduced verbatim -- the
    // reconstruction is by-name and shape-faithful (as the committed siblings already
    // are; e.g. job_instance_handle.cpp packs the handle key from named members rather
    // than reading a raw +0x48 qword). No offsetof pin is asserted for this reason.

    // Reassemble the 64-bit "handle key" the X360 reads with `ld 0x48(handle)`: the
    // qword at JobInstanceHandle+0x8 is { mSchedulerBackend (4B, high), mIndex (2B),
    // mPadding (2B, low) } on the big-endian target. Built from named members so no
    // reinterpret over the struct layout is needed (mirrors job_instance_handle.cpp's
    // PackHandleQword, kept file-local to this TU).
    static u64 PackHandleQword(const JobInstanceHandle& rHandle)
    {
        const u64 luBackend = static_cast<u64>(
            reinterpret_cast<uintptr_t>(rHandle.mSchedulerBackend));
        return (luBackend << 32)
             | (static_cast<u64>(rHandle.mIndex)   << 16)
             |  static_cast<u64>(rHandle.mPadding);
    }

    // EntryPoint default-settings block. Clear() @ 0x82BCA110 memcpy's a 44-byte
    // default over mEntryPoint (Job+4): priority 128 (JOB_PRIORITY_DEFAULT, the
    // li 0x80 at +0x10), affinity 63 (JOB_AFFINITY_ANY, li 0x3F at +0x14),
    // code-recycle 1 (the li 1 at +0x20), everything else zero. The block is built
    // here field-by-field so no struct-internal layout is hardcoded.
    static EntryPoint MakeDefaultEntryPoint()
    {
        EntryPoint lEntryPoint;
        std::memset(&lEntryPoint, 0, sizeof(lEntryPoint));
        // The X360 default block stores: name[0]=0 (memset covers it), priority@+0x10
        // = 0x80, affinity@+0x14 = 0x3F, code-recycle@+0x20 = 1. EntryPoint's setters
        // are the only public mutators; the priority/code-recycle fields have no
        // setter, so the default block is materialised through the raw 44-byte image.
        u8* lpBytes = reinterpret_cast<u8*>(&lEntryPoint);
        *reinterpret_cast<u32*>(lpBytes + 0x10) = 128; // mPriority    = JOB_PRIORITY_DEFAULT
        *reinterpret_cast<u32*>(lpBytes + 0x14) = 63;  // mAffinity    = JOB_AFFINITY_ANY
        *reinterpret_cast<u32*>(lpBytes + 0x20) = 1;   // mCodeRecycle = CODE_RECYCLE_DEFAULT
        return lEntryPoint;
    }

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

    // @ 0x82BCA078 -- chain walker behind GetDependency: advance to the bucket node
    // that owns iIndex. Each full overflow node holds the fixed capacity (10), so the
    // X360 subtracts 10 per hop (NOT mSize) while iIndex is still >= the current
    // node's mSize, then returns &node.mBucket[iIndex].
    const Job::Dependency* Job::IndexDependencyChain(
        const Detail::BucketListNode<Dependency, 10>* pNode, int iIndex)
    {
        while (iIndex >= static_cast<int>(pNode->mSize))
        {
            pNode  = pNode->mNext;
            iIndex -= 10;
        }
        return &pNode->mBucket[iIndex];
    }

    // @ 0x82BCA258 -- the i-th dependency's owning Job.
    Job* Job::GetDependency(int iIndex) const
    {
        const Dependency* lpDependency;
        if (iIndex >= static_cast<int>(mDependencies.mSize))
            lpDependency = IndexDependencyChain(mDependencies.mNext, iIndex - 10);
        else
            lpDependency = &mDependencies.mBucket[iIndex];
        return lpDependency->mJob; // lwz r3,0(r3) -- the Dependency's leading Job*
    }

    // @ 0x82BCA2A0 -- the i-th dependent (back-pointer) Job.
    Job* Job::GetDependent(int iIndex) const
    {
        if (iIndex >= static_cast<int>(mDependents.mSize))
            return mDependents.mNext->operator[](iIndex - 6);
        return mDependents.mBucket[iIndex];
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
        if (mJobInstanceHandle.mSchedulerBackend != 0
            && (mJobInstanceHandle.mIndex != 0 || mJobInstanceHandle.mSubmissionId != 0))
        {
            return mJobInstanceHandle.mSchedulerBackend->IsJobComplete(
                       mJobInstanceHandle.mSubmissionId,
                       PackHandleQword(mJobInstanceHandle)) != 0;
        }
        return true;
    }

    // @ 0x82BCA1F8 -- block on the backend's "sleep on instance" entry (vtable +0x3C).
    void Job::SleepOn()
    {
        // Same liveness guard as IsDone; on a live instance, tail-call the backend's
        // BLOCKING sleep-on-instance slot (+0x3C) -- NOT IsJobComplete (+0x30).
        if (mJobInstanceHandle.mSchedulerBackend != 0
            && (mJobInstanceHandle.mIndex != 0 || mJobInstanceHandle.mSubmissionId != 0))
        {
            mJobInstanceHandle.mSchedulerBackend->SleepOn(
                mJobInstanceHandle.mSubmissionId,
                PackHandleQword(mJobInstanceHandle));
        }
    }

    // @ 0x82BCB238 -- spin-wait on this job's instance handle until done.
    void Job::WaitOn()
    {
        // Liveness guard read off mJobInstanceHandle (the +0x40 handle), then forward
        // to JobInstanceHandle::WaitOn (the spin/yield loop) with no extra knobs.
        if (mJobInstanceHandle.mSchedulerBackend != 0
            && (mJobInstanceHandle.mIndex != 0 || mJobInstanceHandle.mSubmissionId != 0))
        {
            mJobInstanceHandle.WaitOn(); // no-knobs form (see job_types.h WaitOn note)
        }
    }

    // @ 0x82BCA3D8 -- (scheduler-internal) record THIS job's instance handle from the
    // backend slot just selected for it, and seed mStartEvent's barrier.
    JobInstanceHandle* Job::INTERNAL_AddNotReady(Detail::SchedulerBackend** pBackends)
    {
        // The backend is chosen by the job's environment word: the X360 indexes
        // (4 * mEntryPoint.mEnvironment) into the backend array, then dispatches that
        // backend's vtable slot +0x28 (CreateNotReadyInstance) with an out-handle stack
        // temp, the entry point and the param block.
        JobEnvironment leEnvironment = mEntryPoint.GetEnvironment();
        Detail::SchedulerBackend* lpBackend = pBackends[leEnvironment];

        JobInstanceHandle lNewHandle;
        lpBackend->CreateNotReadyInstance(&lNewHandle, &mEntryPoint, mParams);

        // Store the 16-byte handle into mJobInstanceHandle (the two `std` stores into
        // this+0x40 / this+0x48).
        mJobInstanceHandle = lNewHandle;

        // Build a barrier handle against this instance and copy it (four words) into
        // mStartEvent (+0x340). The X360 AddBarrier(&temp, &mJobInstanceHandle) makes a
        // fresh handle wait on this job's instance; its returned 16 bytes seed mStartEvent.
        JobInstanceHandle lStartHandle;
        lStartHandle.AddBarrier(mJobInstanceHandle);
        std::memcpy(&mStartEvent, &lStartHandle, sizeof(mStartEvent));
        return &mJobInstanceHandle;
    }

    // @ 0x82BCB2F0 -- (scheduler-internal) push every dependency-edge barrier and every
    // begin/end event into the backend so the submitted instance fires them.
    void Job::INTERNAL_SubmitEventsAndDeps()
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
                    JobInstanceHandle lBarrier;
                    lBarrier.AddBarrier(mJobInstanceHandle);

                    const Job* lpDepJob = lrDependency.mJob;
                    Detail::SchedulerBackend* lpBackend =
                        lpDepJob->mJobInstanceHandle.mSchedulerBackend;
                    lpBackend->SubmitEvent(
                        lpDepJob->mJobInstanceHandle.mSubmissionId,
                        PackHandleQword(lpDepJob->mJobInstanceHandle),
                        &lBarrier,
                        lrDependency.mTrigger);
                }

                // The dependency may ALSO carry a direct job-instance handle (mJob == 0
                // form): submit it when the handle names a live instance.
                if (lrDependency.mJobInstanceHandle.mSchedulerBackend != 0
                    && (lrDependency.mJobInstanceHandle.mIndex != 0
                        || lrDependency.mJobInstanceHandle.mSubmissionId != 0))
                {
                    JobInstanceHandle lBarrier;
                    lBarrier.AddBarrier(mJobInstanceHandle);

                    Detail::SchedulerBackend* lpBackend =
                        lrDependency.mJobInstanceHandle.mSchedulerBackend;
                    lpBackend->SubmitEvent(
                        lrDependency.mJobInstanceHandle.mSubmissionId,
                        PackHandleQword(lrDependency.mJobInstanceHandle),
                        &lBarrier,
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
                lpBackend->SubmitEvent(
                    mJobInstanceHandle.mSubmissionId,
                    PackHandleQword(mJobInstanceHandle),
                    &lpNode->mBucket[liIndex],
                    liPhase);
            }
        }
    }

    // ------------------------------------------------------------------------
    // ⭐ ADDED 2026-08-10 (fill-worker wave 2) -- the three EntryPoint forwarders.
    // job.h has declared all three since the class landed ("forwards to
    // mEntryPoint.*"); none was ever bodied, because nothing in this port had ever
    // wired a job's entry point. RunFillTriangleCacheStream is the first thing that
    // does, and the LINK is what found them -- no per-TU gate could.
    //
    // None carries a standalone X360 symbol: the console inlines all three at both
    // dispatchers (0x82810E04 SetName / 0x82810E1C SetCode / 0x82810E38 SetCodeRecycle
    // call the EntryPoint bodies directly with `entrypoint = job + 4`). The forwarder
    // shape is the DWARF's (job.h:81 / :83 / :173), and mEntryPoint at Job+0x04 is what
    // makes `job + 4` the same address.
    // ------------------------------------------------------------------------
    void Job::SetCode(JobEnvironment leEnvironment, const void* lpvCode, int liSize)
    {
        mEntryPoint.SetCode(leEnvironment, lpvCode, liSize);
    }

    void Job::SetName(const char* lpcName)
    {
        mEntryPoint.SetName(lpcName);
    }

    void Job::SetCodeRecycle(EntryPoint::CodeRecycle leRecycle)
    {
        mEntryPoint.SetCodeRecycle(leRecycle);
    }
}
}
