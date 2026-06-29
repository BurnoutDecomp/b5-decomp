#ifndef EA_JOBS_LOCAL_BACKEND_H
#define EA_JOBS_LOCAL_BACKEND_H

#include "types.hpp"

#include "SDKs/EATech/eajobs/event.h"        // EA::Jobs::Event + Detail::BucketListNode<Event,16>
#include "SDKs/EATech/eajobs/job_types.h"    // EA::Jobs::JobInstanceHandle / JobPriority / JobAffinity / Detail::SchedulerBackend
#include "SDKs/EATech/eajobs/entry_point.h"  // EA::Jobs::EntryPoint (CreateNotReadyInstance entry argument)
#include "SDKs/EATech/eajobs/reference_count.h" // EA::Jobs::Detail::ReferenceCount (the +0x270 word)
#include "SDKs/EATech/eajobs/job_thread.h"   // EA::Jobs::LocalBackend::JobThread (32 embedded worker threads)
#include "SDKs/EATech/eajobs/job_thread_handle.h" // EA::Jobs::JobThreadHandle (AddThread / GetThreadHandle out-struct)
#include "SDKs/EATech/eathread/eathread_semaphore.h" // EA::Thread::Semaphore (conditional member)

// SDKs/EATech/eajobs/local_backend.h
//
// EA::Jobs::LocalBackend::JobInstance -- one slot in the LocalBackend scheduler's
// job-instance table: the per-submission state (job parameters + handle + start
// timestamp), the per-phase begin/end event lists, an optional completion
// semaphore, and the two spinlocks (a garbage-collector reservation word and the
// event-list try-lock word). Plus the nested AutoTryLockEventList RAII helper.
//
// Reconstructed store-for-store from the X360 .XEX:
//   JobInstance::JobInstance              @ 0x82BCBB50
//   JobInstance::~JobInstance             @ 0x82BCB6C8
//   JobInstance::`vector deleting dtor'   @ 0x82BCBE70
//   JobInstance::Initialize               @ 0x82BCAA18
//   JobInstance::Clear                    @ 0x82BCA968
//   JobInstance::SleepOn                  @ 0x82BCAAE8
//   JobInstance::PlayEventList            @ 0x82BCBC00
//   AutoTryLockEventList()                @ 0x82BCA658
//   ~AutoTryLockEventList()               @ 0x82BC9C68
//
// X360 LAYOUT (sizeof 0x290 == 656, the 0x290 element stride in the vector deleting
// destructor's `mulli r10,r11,0x290`):
//   +0x000 mStatus            (u32)                       -- stw 0,0(this)
//   +0x004 mParameters        (44-byte job params blob)   -- memcpy(this+4, src, 0x2C)
//          within mParameters: +0x10 priority(=128 default), +0x14 affinity(=63 default),
//          +0x25 the "has semaphore" flag (lbz 0x29(this) gates the semaphore).
//   +0x030 mStartTimeStamp    (u64; QueryPerformanceCounter low word, hi = flag byte)
//   +0x038 mHandle            (JobInstanceHandle, 16B)    -- 4 dwords from a3[0..3]
//   +0x048 mPad48             (8B gap, unwritten)
//   +0x050 mEventLists[2]     (Detail::BucketListNode<Event,16>, 272B each: begin/end)
//   +0x270 mGarbageCollectorLock (u32 reservation word)
//   +0x274 mSemaphore         (EA::Thread::Semaphore, 16B; constructed only if flag set)
//   +0x284 mEventListLock     (u32 try-lock word; AutoTryLockEventList CASes it)
//   +0x288 mbBeginPlayed      (bool)
//   +0x289 mbEndPlayed        (bool)
//
// Vendor EA code reconstructed in its canonical home.

namespace EA
{
namespace Jobs
{
namespace LocalBackend
{
    // The 44-byte (0x2C) job-parameters blob copied wholesale into the instance by
    // Initialize (`memcpy(this+4, src, 0x2C)`). Only the fields the X360 ctor stores
    // a known default into are named (the priority/affinity defaults map exactly to
    // the EA::Jobs JobPriority/JobAffinity enumerators); the rest stay as honest
    // padding so each named field lands at its proven offset WITHOUT raw casts.
    struct JobInstanceParameters
    {
        u8  mPad00[0x10];   // +0x04..+0x14 (ctor stores byte 0 at rel +0; rest filled by memcpy)
        u32 mPriority;      // +0x14 -- ctor default 128 (JOB_PRIORITY_DEFAULT / MEDIUM)
        u32 mAffinity;      // +0x18 -- ctor default 63  (JOB_AFFINITY_ANY)
        u32 mPad18;         // +0x1C -- ctor default 0
        u32 mPad1C;         // +0x20 -- ctor default 0
        u32 mFlags24;       // +0x24 -- ctor default 1
        u8  mPad28;         // +0x28 -- ctor default 0
        u8  mbHasSemaphore; // +0x29 -- ctor default 0; nonzero == build mSemaphore
        u8  mPad2A[2];      // +0x2A..+0x2C
        u32 mPad2C;         // +0x2C -- ctor default 0  (end of the 0x2C blob)
    };

    // EA::Jobs::LocalBackend::JobInstance -- one scheduler slot (see header banner).
    struct JobInstance
    {
        // event.h:When -- 2 phases (begin/end), so 2 event-list nodes.
        enum { KI_NUM_EVENT_LISTS = Event::EVENT_WHEN_NUM };

        // @ 0x82BCBB50 -- zero the slot, default the parameters (priority 128, affinity
        // 63, flags24 1), construct the two empty begin/end event lists, release the
        // garbage-collector reservation word and the event-list lock, and clear the
        // begin/end "played" flags. Does NOT construct the semaphore.
        JobInstance();

        // @ 0x82BCB6C8 -- tear down both event lists (free their overflow chains and
        // null mNext/mSize). Does NOT touch the (conditionally-built) semaphore.
        ~JobInstance();

        // @ 0x82BCAA18 -- (re)initialise this slot from a submitted job: copy the
        // 44-byte parameters, stamp the start time (QueryPerformanceCounter low word),
        // record the handle, mark live (mStatus 0, mGarbageCollectorLock 1), build the
        // semaphore iff the parameters flag it, then reset the event lists + played
        // flags. pParameters -> the 44-byte job-params source; rHandle -> the slot's
        // JobInstanceHandle (4 dwords).
        void Initialize(const void* pParameters, const JobInstanceHandle& rHandle);

        // @ 0x82BCA968 -- garbage-collect this slot: spin to take the GC reservation
        // word (CAS 1 -> 0), destruct the semaphore iff present, clear the played
        // flags, and reset both event lists.
        void Clear();

        // Run the submitted job: play the begin events, invoke the job code, play the
        // end events. (LocalBackend::ExecuteReadyJobs @ 0x82BCC490 calls it once a slot
        // is claimed; the body lives in the out-of-group JobInstance object TU --
        // declared here so ExecuteReadyJobs links.)
        void Run();

        // @ 0x82BCAAE8 -- if this slot owns a semaphore, wait one permit on it then
        // immediately post one back (a wait-for-then-release of the completion gate).
        void SleepOn();

        // @ 0x82BCBC00 -- run every event in one phase's event-list chain. Spins to
        // take the event-list try-lock, marks the phase's "played" flag, releases the
        // lock, then walks the bucket chain calling Event::Run on each entry.
        //   eWhen : which phase (begin/end) of THIS instance to play.
        void PlayEventList(Event::When eWhen);

        u32                              mStatus;                // +0x000
        JobInstanceParameters            mParameters;            // +0x004 (0x2C)
        u64                              mStartTimeStamp;        // +0x030
        JobInstanceHandle                mHandle;                // +0x038 (0x10)
        u8                               mPad48[8];              // +0x048 (gap)
        Detail::BucketListNode<Event, 16> mEventLists[KI_NUM_EVENT_LISTS]; // +0x050 (2 * 0x110)
        u32                              mGarbageCollectorLock;  // +0x270
        // The semaphore lives at +0x274 but is built only when mbHasSemaphore is set
        // (the X360 never default-constructs it). Raw, suitably aligned storage that
        // the methods placement-construct / -destruct under the flag -- matching the
        // binary, which adds no construction the parameters don't ask for.
        union
        {
            u8                           mSemaphoreStorage[sizeof(EA::Thread::Semaphore)]; // +0x274 (0x10)
            u64                          mSemaphoreAlign;
        };
        u32                              mEventListLock;         // +0x284
        bool                             mbBeginPlayed;          // +0x288
        bool                             mbEndPlayed;            // +0x289

        // Reach the conditionally-built semaphore by name (no raw cast off `this`).
        EA::Thread::Semaphore* Semaphore()
        {
            return reinterpret_cast<EA::Thread::Semaphore*>(mSemaphoreStorage);
        }

        // The +0x270 word doubles as a Detail::ReferenceCount: SleepOn / RunSingleJob /
        // the garbage collector take a live-reference on the slot (Increment/Decrement)
        // around running it, exactly as the X360 dispatches ReferenceCount on &this+0x270.
        // Reached by name (same storage as mGarbageCollectorLock; no raw cast off `this`).
        Detail::ReferenceCount* RefCount()
        {
            return reinterpret_cast<Detail::ReferenceCount*>(&mGarbageCollectorLock);
        }

        // EA::Jobs::LocalBackend::JobInstance::AutoTryLockEventList -- RAII try-lock
        // guard over the enclosing JobInstance's mEventListLock. X360 layout
        // (ctor store offsets @ 0x82BCA658):
        //   +0x0 mpJobInstance (JobInstance*) -- stw r4,0(r3)
        //   +0x4 mbLocked      (bool)         -- stb r10,4(r3)
        class AutoTryLockEventList
        {
        public:
            // @ 0x82BCA658 -- try to take pJobInstance's event-list lock. If the
            // lock word is already non-zero, give up immediately (mbLocked stays
            // false); otherwise CAS it 0 -> 1 and record success in mbLocked.
            AutoTryLockEventList(JobInstance* pJobInstance);

            // @ 0x82BC9C68 -- release the lock (CAS 1 -> 0) only if we took it.
            ~AutoTryLockEventList();

            // PlayEventList breaks its spin once the lock was actually won.
            bool Locked() const { return mbLocked; }

        private:
            JobInstance* mpJobInstance; // +0x0
            bool         mbLocked;      // +0x4

            AutoTryLockEventList(const AutoTryLockEventList&);
            AutoTryLockEventList& operator=(const AutoTryLockEventList&);
        };

    private:
        JobInstance(const JobInstance&);
        JobInstance& operator=(const JobInstance&);
    };

    // @ 0x82BCBE70 -- MSVC `vector deleting destructor' thunk for JobInstance,
    // materialised as a free function (the thunk has no portable C++ spelling).
    // cFlags bit1 == array variant (cookie at this-16 holds the element count);
    // cFlags bit0 == also free the storage via the Jobs allocator's Free virtual.
    JobInstance* JobInstance_VectorDeletingDestructor(JobInstance* pThis, char cFlags);

    // ========================================================================
    // EA::Jobs::LocalBackend::LocalBackend -- the local (single-process) scheduler
    // backend. It owns a fixed pool of 32 worker JobThreads, a heap-allocated table
    // of JobInstance slots and a parallel priority-queue of 64-bit slot words, and
    // it implements the abstract Detail::SchedulerBackend the JobScheduler / handles
    // dispatch into (create-not-ready / is-complete / submit-event / barrier /
    // sleep-on) plus the JobThread worker entry points (execute-ready / sleep-timeout).
    //
    // X360 LAYOUT (ctor store offsets @ 0x82BCBCB8; dtor restores SchedulerBackend
    // vtable off_82182408 at +0x0 on the way out):
    //   +0x000 vtable                       (off_82182500 -- combined LocalBackend vtable)
    //   +0x004 mpJobInstances               (JobInstance*, mNumSlots elements, 0x290 stride)
    //   +0x008 mpPriorityQueue              (u64*, mNumSlots packed slot words)
    //   +0x00C mNextSlot                    (s32 round-robin allocation cursor)
    //   +0x010 mNumSlots                    (s32 capacity; ctor arg a2)
    //   +0x014 mThreads[32]                 (JobThread, 0x2C stride; +0x14..+0x594)
    //   +0x598 mSubmissionCounter           (u64 monotonically-rising submission-id source)
    //   +0x5A0 mpProfilingCallback          (profiling metrics sink; 0 == disabled)
    //   +0x5A4 mpProfilingContext           (user context handed to the callback)
    //   +0x5A8 mGarbageCollectorCursor      (s32 round-robin GC scan cursor)
    //   +0x5AC mpDefaultSleepContext        (ctor arg a3; the host's wait context)
    //   +0x5B0 mJobThreadSleepTimeoutMS     (u32; ctor default 1)
    //
    // The packed priority-queue word (u64 per slot):
    //   low  47 bits : the submission id            (& 0x7FFFFFFFFFFF)
    //   bits 47..    : state tag in the top 3 bits of the HIGH word (0x60000000 == free/done)
    //   plus the priority/affinity/index bitfields AddNotReady builds in.
    // -1 (all ones) marks a never-used / reclaimed slot.
    //
    // VTABLE: the X360 stamps a SINGLE combined vtable (off_82182500) at +0x0 -- the
    // Detail::SchedulerBackend vtable (slots +0x28 CreateNotReadyInstance / +0x30
    // IsJobComplete / +0x34 SubmitEvent / +0x38 AddBarrier / +0x3C SleepOn) EXTENDED
    // with the worker entry points (+0x50 ExecuteReadyJobs / +0x54 GetSleepTimeoutMs)
    // and the wake hook (+0x1C WakeThreads). The dtor restores the base vtable
    // off_82182408 on the way out, proving LocalBackend IS a concrete SchedulerBackend
    // -- so it overrides every pure virtual (no second base, ONE vptr at +0x0). The
    // worker entry points are declared as further virtuals ON THIS class (the same
    // combined vtable the workers dispatch BY NAME), NOT inherited from a second
    // polymorphic base (which would insert a 2nd vptr and shift every member offset).
    //
    // Vendor EA code reconstructed in its canonical home.
    class LocalBackend : public Detail::SchedulerBackend
    {
    public:
        enum { KI_NUM_THREADS = 32 };

        // The "free / completed" state tag (top 3 bits of the slot word's HIGH word).
        // X360: `lis r8,0x6000` compared against `clrrwi r9,r9,29` (top-3-bits mask).
        enum { KU_SLOT_STATE_MASK = 0xE0000000u, KU_SLOT_STATE_FREE = 0x60000000u };

        // @ 0x82BCBCB8 -- bring the backend up: stamp the vtable, default the cursors,
        // construct the 32 idle worker threads, release the submission counter, default
        // the profiling/sleep fields (uContext kept), then allocate + construct the
        // JobInstance table (uMaxJobs slots) and the parallel priority-queue (all -1).
        LocalBackend(u32 uMaxJobs, void* pContext);

        // @ 0x82BCBF38 -- tear the backend down: tell every worker to quit + wake it,
        // join them all, run a final garbage-collection pass, free the instance table
        // and the priority queue, destruct the worker threads, restore the base vtable.
        virtual ~LocalBackend();

        // -- Detail::SchedulerBackend overrides (vtable +0x28/+0x30/+0x34/+0x38/+0x3C) --

        // @ 0x82BCC010 (vtable +0x28) CreateNotReadyInstance: claim a free queue slot for
        // a freshly-submitted job, Initialize its JobInstance, and write the new
        // JobInstanceHandle (submission id / backend / index) into pOutHandle. If no slot
        // frees within the ~12s watchdog budget and the global abort predicate fires,
        // writes a null handle instead. (Backend-internal name: AddNotReady.)
        virtual void CreateNotReadyInstance(JobInstanceHandle* pOutHandle,
                                            const EntryPoint*  pEntryPoint,
                                            const Param*       pParams);

        // @ 0x82BC9D88 (vtable +0x30) IsJobComplete: nonzero if the slot keyed by
        // (uSubmissionId, uHandleQword) is free/done, reclaimed (-1), or no longer holds
        // this submission. (Backend-internal name: IsDone.)
        virtual int IsJobComplete(u64 uSubmissionId, u64 uHandleQword);

        // @ 0x82BCC1D8 (vtable +0x34) SubmitEvent: register pPayload (an Event) on phase
        // iWhen of the slot keyed by (uSubmissionId, uHandleQword). If the slot is still
        // live and the phase has NOT played, append the event to that phase's list (under
        // the slot's event-list try-lock); otherwise run the event immediately.
        // (Backend-internal name: AddEvent.)
        virtual int SubmitEvent(u64 uSubmissionId, u64 uHandleQword,
                                const void* pPayload, int iWhen);

        // @ 0x82BCA878 (vtable +0x38) AddBarrier -- the SchedulerBackend +0x38 slot. The
        // X360 body (the GetAllowSleepOn logic) consults whether the slot keyed by
        // (uSubmissionId, uHandleQword) permits a blocking sleep-on: it dispatches its OWN
        // vtable +0x30 (IsJobComplete) and, if the job is not already complete and the slot
        // still holds this submission, reads the instance's mAllowSleepOn byte (returning 0
        // otherwise). This IS the override that makes LocalBackend concrete; the dependent
        // handle is the X360's r4 argument (the body keys off the submission id / handle
        // qword the dependency carries). (Backend-internal name: GetAllowSleepOn.)
        virtual void AddBarrier(JobInstanceHandle* pDependentHandle, u64 uSubmissionId,
                                u64 uHandleQword);

        // The +0x38 body's recovered result (0 == not sleepable). AddBarrier evaluates it
        // for its side-effect dispatch; surfaced as a named helper so the value the X360
        // computes is reachable without a void return swallowing it.
        int GetAllowSleepOn(u64 uSubmissionId, u64 uHandleQword);

        // @ 0x82BCBA80 (vtable +0x3C) SleepOn: BLOCKING wait on the slot keyed by
        // (uSubmissionId, uHandleQword). Takes a live-reference on the slot, re-checks
        // completion, and if still pending waits on (then re-posts) its semaphore.
        virtual int SleepOn(u64 uSubmissionId, u64 uHandleQword);

        // -- JobThread worker entry points (LocalBackend vtable +0x50 / +0x54) --

        // @ 0x82BCC490 (vtable +0x50) ExecuteReadyJobs: pick the highest-priority ready
        // slot whose affinity matches iAffinity, atomically claim it, run its
        // JobInstance, then publish completion (post its semaphore / decrement its ref)
        // and wake the workers. Returns 1 if a job ran, 0 if there was nothing to do.
        // (Backend-internal name: RunSingleJob; X360 takes the affinity as an u8 mask.)
        virtual int ExecuteReadyJobs(int iAffinity);

        // @ 0x82BCBE60 (vtable +0x54) GetSleepTimeoutMs: the ms a worker waits on its
        // wake event before the next execute pass.
        virtual u32 GetSleepTimeoutMs();

        // @ 0x82BCA910 (vtable +0x1C) WakeThreads: SetEvent every started worker's wake
        // event so they pick up newly-ready / completed work.
        virtual void WakeThreads();

        // -- Non-virtual scheduler facade helpers (called directly, not through vtable) --

        // @ 0x82BCA6C0 -- start the next idle worker thread with rParameters and write a
        // JobThreadHandle (backend, index) into pOutHandle. No-op handle (0,0) when all
        // 32 slots are in use.
        void AddThread(JobThreadHandle* pOutHandle, const JobThreadParameters& rParameters);

        // @ 0x82BC9D58 -- number of started worker threads.
        int GetNumThreads() const;

        // @ 0x82BC9CF8 -- number of slots currently holding a live (not free, not
        // reclaimed) job instance.
        int GetNumJobsInFlight() const;

        // @ 0x82BCA738 -- join worker thread iIndex.
        void WaitForEnd(int iIndex);

        // @ 0x82BC9E20 -- pack (pBackend, uHandle) into the JobThreadHandle out-struct.
        void GetThreadHandle(JobThreadHandle* pOutHandle, Detail::SchedulerBackend* pBackend,
                             u32 uHandle) const;

        // @ 0x82BC9E30 -- the OS thread id of worker iIndex.
        u64 GetThreadId(int iIndex) const;

        // @ 0x82BCA770 -- copy worker iIndex's 24-byte JobThreadParameters into pOut.
        void GetThreadParameters(JobThreadParameters* pOut, int iIndex) const;

        // @ 0x82BC9CB8 -- read the slot keyed by uHandleQword: write its packed
        // priority-queue entry pointer to *ppEntry and the masked (low-61-bit) state word
        // (>>32) to *puState. (X360 internal name: GetEnabler.)
        void GetEnabler(u64 uSubmissionId, u64 uHandleQword, u64** ppEntry, u32* puState);

        // @ 0x82BC9E68 / 0x82BC9E70 -- install the profiling sink + its user context.
        void SetProfilingCallback(void* pCallback);
        void SetProfilingContext(void* pContext);

        // @ 0x82BCBB48 -- force one full garbage-collection sweep (FlushProfile == GC
        // with the "force/flush" flag set).
        void FlushProfile();

        // @ 0x82BCBE68 -- set the worker wake-event timeout (ms).
        void SetJobThreadSleepTimeoutMS(u32 uTimeoutMS);

        // @ 0x82BCB718 -- garbage-collect completed slots: walk a budgeted window of the
        // instance table, reclaim every free/done slot (Clear + republish the queue word
        // as -1), harvest profiling metrics into the callback if installed, and advance
        // the GC cursor. bForce (== 1) collects unconditionally without the rolling
        // window. The other two ints are forwarded verbatim to JobInstance::Clear.
        void TryToRunJobInstanceGarbageCollector(int bForce, int iClearArg2 = 0,
                                                 int iClearArg3 = 0);

        // +0x000 inherited Detail::SchedulerBackend vtable
        JobInstance*              mpJobInstances;            // +0x004
        u64*                      mpPriorityQueue;           // +0x008
        s32                       mNextSlot;                 // +0x00C
        s32                       mNumSlots;                 // +0x010
        JobThread                 mThreads[KI_NUM_THREADS];  // +0x014 (32 * 0x2C)
        u64                       mSubmissionCounter;        // +0x598
        void*                     mpProfilingCallback;       // +0x5A0
        void*                     mpProfilingContext;        // +0x5A4
        s32                       mGarbageCollectorCursor;   // +0x5A8
        void*                     mpDefaultSleepContext;     // +0x5AC (ctor arg a3)
        u32                       mJobThreadSleepTimeoutMS;  // +0x5B0

    private:
        LocalBackend(const LocalBackend&);
        LocalBackend& operator=(const LocalBackend&);
    };

    // @ 0x82BCC420 -- MSVC `vector deleting destructor' thunk for LocalBackend (no
    // portable C++ spelling; materialised as a free function). Runs ~LocalBackend, then
    // (delete bit set + non-null) frees the storage via the Jobs allocator's Free virtual.
    LocalBackend* LocalBackend_VectorDeletingDestructor(LocalBackend* pThis, char cFlags);
}
}
}

#endif // EA_JOBS_LOCAL_BACKEND_H
