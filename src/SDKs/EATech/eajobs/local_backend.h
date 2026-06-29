#ifndef EA_JOBS_LOCAL_BACKEND_H
#define EA_JOBS_LOCAL_BACKEND_H

#include "types.hpp"

#include "SDKs/EATech/eajobs/event.h"        // EA::Jobs::Event + Detail::BucketListNode<Event,16>
#include "SDKs/EATech/eajobs/job_types.h"    // EA::Jobs::JobInstanceHandle / JobPriority / JobAffinity
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
}
}
}

#endif // EA_JOBS_LOCAL_BACKEND_H
