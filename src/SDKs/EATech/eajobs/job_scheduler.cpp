#include "SDKs/EATech/eajobs/job_scheduler.h"
#include "SDKs/EATech/eajobs/job.h"           // EA::Jobs::Job (INTERNAL_AddNotReady / SubmitEventsAndDeps / dependency walk)
#include "SDKs/EATech/eajobs/event.h"         // EA::Jobs::Event::Run (mStartEvent kick)
#include "SDKs/EATech/eajobs/local_backend.h" // EA::Jobs::LocalBackend::LocalBackend (Initialize / Destroy / AddThread / WakeThreads)
#include "SDKs/EATech/eajobs/jobs.h"          // EA::Jobs::GetAllocator (off_8327F280 -- backend Allocate)

#include <new>                                // placement new (construct the backend into the allocator block)

// ============================================================================
// SDKs/EATech/eajobs/job_scheduler.cpp
//
// EA::Jobs::JobScheduler member bodies, reconstructed store-for-store from the
// X360 .XEX (BURNOUT_X360_ARTIST.XEX):
//   JobScheduler::JobScheduler   @ 0x82BC9AA0
//   JobScheduler::~JobScheduler  @ 0x82BCA5E0
//   JobScheduler::SetProfiling   @ 0x82BC9AE8
//   JobScheduler::Initialize     @ 0x82BCC390
//   JobScheduler::Destroy        @ 0x82BC9AF0
//   JobScheduler::AddThread      @ 0x82BCA5F8
//   JobScheduler::AddJobs        @ 0x82BCB498
//   JobScheduler::AddTree        @ 0x82BCB540
//
// The X360 object packs (4-byte pointers):
//   +0x00 mSchedulers[2]   (the backends array; *this == &mSchedulers[0])
//   +0x08 mLock            (a CRITICAL_SECTION-backed Futex; ctor Rtl-inits this+8)
//   +0x28 mEnableProfiling (byte 40; ctor stores 1)
//   +0x29 mInitialized     (byte 41)
// The reconstruction reaches every one of those BY NAME.
//
// The six GetThreadHandle / GetNumThreads / SetProfilingCallback /
// SetProfilingContext / FlushProfile / RunGarbageCollector / WakeThreads /
// (Get|Set)JobThreadSleepTimeoutMS facade methods are DWARF declaration surface
// with no standalone X360 body in this build -- they are intentionally left as
// declarations in job_scheduler.h (no fabricated bodies here).
//
// Vendor EA code reconstructed in its canonical home; members/names follow the EA
// SDK convention, NOT the Brn/Cgs project convention.
// ============================================================================

namespace EA
{
namespace Jobs
{
    // @ 0x82BC9AA0 -- construct an idle, uninitialised scheduler. Store order from
    // the asm:
    //   RtlInitializeCriticalSection(this+8)  ; the mLock Futex ctor
    //   mInitialized     = 0   ; stb 0,0x29(this)
    //   mEnableProfiling = 1   ; stb 1,0x28(this)
    //   mSchedulers[0]   = 0   ; stw 0,0(this)
    JobScheduler::JobScheduler()
        : mLock()
        , mEnableProfiling(true)
        , mInitialized(false)
    {
        mSchedulers[0] = 0;
    }

    // @ 0x82BCA5E0 -- if the scheduler was initialised, tear it down. The X360 is a
    // tail branch: `if (mInitialized) return Destroy();` -- no other work.
    JobScheduler::~JobScheduler()
    {
        if (mInitialized)
        {
            Destroy();
        }
    }

    // @ 0x82BC9AE8 -- store the profiling-enable byte (no other side effects).
    void JobScheduler::SetProfiling(bool lbEnable)
    {
        mEnableProfiling = lbEnable;
    }

    // @ 0x82BCC390 -- bring up the LOCAL backend. The asm allocates 1464 (0x5B8)
    // bytes from the process-wide Jobs allocator (off_8327F280, vtable +0x04:
    // Allocate(size=0x5B8, name, flags=1, align=0x10, alignOffset=0)) and, if the
    // block is non-null, placement-constructs a LocalBackend(maxJobs, &mEnableProfiling)
    // into it; otherwise the backend pointer is null. The result is stored into
    // mSchedulers[0] and mInitialized is set.
    //
    // Only the second argument participates in the X360 body (r4 -> the backend's
    // uMaxJobs); iMaxThreads is the DWARF-surface first parameter and is unused here
    // (the worker count is driven later by AddThread).
    void JobScheduler::Initialize(int /*iMaxThreads*/, int iMaxJobs)
    {
        Allocator* lpAllocator = GetAllocator();
        void* lpvBlock = lpAllocator->Allocate(
            sizeof(LocalBackend::LocalBackend), "EA::Jobs::LocalBackend::LocalBackend", 1, 16, 0);

        LocalBackend::LocalBackend* lpBackend = 0;
        if (lpvBlock)
        {
            lpBackend = new (lpvBlock) LocalBackend::LocalBackend(
                static_cast<u32>(iMaxJobs), &mEnableProfiling);
        }

        mSchedulers[0] = lpBackend;
        mInitialized   = true;
    }

    // @ 0x82BC9AF0 -- destroy the LOCAL backend. The asm reads mSchedulers[0] and, if
    // non-null, dispatches its vtable slot +0x00 with the delete flag (1) -- the MSVC
    // vector-deleting-destructor thunk, which runs ~LocalBackend and frees the storage
    // through the Jobs allocator. Then clears mSchedulers[0] and mInitialized.
    void JobScheduler::Destroy()
    {
        LocalBackend::LocalBackend* lpBackend = static_cast<LocalBackend::LocalBackend*>(mSchedulers[0]);
        if (lpBackend)
        {
            LocalBackend::LocalBackend_VectorDeletingDestructor(lpBackend, 1);
        }
        mSchedulers[0] = 0;
        mInitialized   = false;
    }

    // @ 0x82BCA5F8 -- spawn a worker thread. The X360 selects the backend by the
    // parameter block's environment field (`mSchedulers[lrParameters.environment]`),
    // takes the scheduler lock around the dispatch, and calls that backend's AddThread
    // entry (the only concrete backend is LocalBackend), handing it the out-handle
    // buffer and the parameter block. Returns the populated handle by value.
    //
    // Dispatch grounding (asm 0x82BCA61C..0x82BCA63C):
    //   lwz  r11, 0(r29)        ; r29 = &lrParameters; r11 = lrParameters.environment
    //   slwi r11, r11, 2        ; * sizeof(Detail::SchedulerBackend*)
    //   lwzx r4,  r11, r30      ; r4  = mSchedulers[environment]   (the backend)
    //   lwz  r11, 0(r4)         ; r11 = backend vtable
    //   lwz  r11, 0x10(r11)     ; r11 = vtable[+0x10]  == the AddThread slot
    //   mtctr r11 ; bctrl       ; call(r3 = &lHandle (sret), r5 = &lrParameters)
    // The combined LocalBackend vtable's worker slots are WakeThreads +0x1C /
    // ExecuteReadyJobs +0x50 / GetSleepTimeoutMs +0x54 (see local_backend.h), so +0x10
    // is a distinct earlier slot: the SchedulerBackend AddThread entry. The only
    // concrete backend is LocalBackend, whose non-virtual AddThread(JobThreadHandle*,
    // const JobThreadParameters&) @ 0x82BCA6C0 IS the body that slot resolves to --
    // so the concrete call below is the exact target of the +0x10 dispatch.
    JobThreadHandle JobScheduler::AddThread(const JobThreadParameters& lrParameters)
    {
        JobThreadHandle lHandle;

        mLock.Lock();
        LocalBackend::LocalBackend* lpBackend =
            static_cast<LocalBackend::LocalBackend*>(mSchedulers[lrParameters.environment]);
        lpBackend->AddThread(&lHandle, lrParameters);
        mLock.Unlock();

        return lHandle;
    }

    // @ 0x82BCB498 -- submit liCount contiguous jobs. The X360 walks the array three
    // times (the 848-byte == sizeof(Job) stride) when liCount > 0:
    //   pass 1: register each job as not-ready (Job::INTERNAL_AddNotReady), keyed by
    //           the scheduler's backends array (== `this`/&mSchedulers[0]).
    //   pass 2: submit each job's events + dependency edges (INTERNAL_SubmitEventsAndDeps).
    //   pass 3: run each job's start event (the mStartEvent at +0x340 within the Job).
    // Finally it kicks the local backend (mSchedulers[0]->vtable +0x1C == WakeThreads).
    void JobScheduler::AddJobs(Job* lpaJobs, int liCount)
    {
        if (liCount > 0)
        {
            for (int liIndex = 0; liIndex < liCount; ++liIndex)
            {
                lpaJobs[liIndex].INTERNAL_AddNotReady(mSchedulers);
            }

            for (int liIndex = 0; liIndex < liCount; ++liIndex)
            {
                lpaJobs[liIndex].INTERNAL_SubmitEventsAndDeps();
            }

            for (int liIndex = 0; liIndex < liCount; ++liIndex)
            {
                lpaJobs[liIndex].mStartEvent.Run();
            }
        }

        static_cast<LocalBackend::LocalBackend*>(mSchedulers[0])->WakeThreads();
    }

    // @ 0x82BCB540 -- submit a job and its transitively-reachable dependency/dependent
    // graph as one batch. A fixed 24-slot work list (on the stack) doubles as the
    // visited set: each newly seen job is marked (mSeen = 1), registered not-ready, and
    // enqueued; the breadth-first walk pulls each job's dependencies then its dependents,
    // enqueuing any not-yet-seen neighbour. Once the closure is gathered it submits every
    // job's events + dependency edges, runs every job's start event (clearing mSeen as it
    // goes), and finally kicks the local backend (WakeThreads).
    void JobScheduler::AddTree(Job* lpRoot)
    {
        Job* lapWorkList[24];

        int liWritten = 1;
        lapWorkList[0] = lpRoot;
        lpRoot->INTERNAL_AddNotReady(mSchedulers);
        lpRoot->mSeen = true;

        int liRead = 0;
        do
        {
            Job* lpJob = lapWorkList[liRead];

            const int liNumDependencies = lpJob->GetNumDependencies();
            for (int liDep = 0; liDep < liNumDependencies; ++liDep)
            {
                Job* lpDependency = lpJob->GetDependency(liDep);
                if (!lpDependency->mSeen)
                {
                    lpDependency->mSeen = true;
                    lpDependency->INTERNAL_AddNotReady(mSchedulers);
                    lapWorkList[liWritten] = lpDependency;
                    ++liWritten;
                }
            }

            const int liNumDependents = lpJob->GetNumDependents();
            for (int liDpt = 0; liDpt < liNumDependents; ++liDpt)
            {
                Job* lpDependent = lpJob->GetDependent(liDpt);
                if (!lpDependent->mSeen)
                {
                    lpDependent->mSeen = true;
                    lpDependent->INTERNAL_AddNotReady(mSchedulers);
                    lapWorkList[liWritten] = lpDependent;
                    ++liWritten;
                }
            }

            ++liRead;
        }
        while (liRead != liWritten);

        if (liRead > 0)
        {
            for (int liIndex = 0; liIndex < liRead; ++liIndex)
            {
                lapWorkList[liIndex]->INTERNAL_SubmitEventsAndDeps();
            }

            for (int liIndex = 0; liIndex < liRead; ++liIndex)
            {
                Job* lpJob = lapWorkList[liIndex];
                lpJob->mStartEvent.Run();
                lpJob->mSeen = false;
            }
        }

        static_cast<LocalBackend::LocalBackend*>(mSchedulers[0])->WakeThreads();
    }
}
}
