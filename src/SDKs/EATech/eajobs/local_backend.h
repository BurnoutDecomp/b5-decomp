#ifndef EA_JOBS_LOCAL_BACKEND_H
#define EA_JOBS_LOCAL_BACKEND_H

#include "types.hpp"

// SDKs/EATech/eajobs/local_backend.h
//
// EA::Jobs::LocalBackend::JobInstance::AutoTryLockEventList -- the RAII helper that
// try-acquires a JobInstance's event-list spinlock for the duration of a scope,
// recording whether it actually won the lock so the destructor only releases what
// it took. Reconstructed from the X360 .XEX:
//   AutoTryLockEventList(JobInstance*)  @ 0x82BCA658
//   ~AutoTryLockEventList()             @ 0x82BC9C68
//
// The X360 asm proves the spinlock word lives at offset 0x284 (644) inside the
// (large, out-of-this-group) JobInstance. Only that one field is committed here; a
// byte-span placeholder carries JobInstance up to that offset so the named
// mEventListLock member lands exactly at +0x284 WITHOUT a raw-offset cast. The rest
// of JobInstance is reconstructed elsewhere -- this header does not commit its full
// layout (the trailing members past the lock are intentionally unmodelled).
//
// Vendor EA code reconstructed in its canonical home.

namespace EA
{
namespace Jobs
{
namespace LocalBackend
{
    // Minimal JobInstance shell: only the event-list spinlock at +0x284 is needed
    // by this group. The leading bytes are an HONEST opaque placeholder for the
    // (out-of-group) JobInstance state -- named so no reinterpret-cast is used to
    // reach the lock. The trailing JobInstance state past the lock is NOT modelled.
    struct JobInstance
    {
        // +0x000 .. +0x283 -- opaque JobInstance state (not reconstructed here).
        u8  mOpaqueState[0x284];
        // +0x284 -- the event-list spinlock word (0 == free, 1 == held), driven by
        // AutoTryLockEventList via a masked-interrupt lwarx/stwcx. reservation.
        s32 mEventListLock;

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

        private:
            JobInstance* mpJobInstance; // +0x0
            bool         mbLocked;      // +0x4

            AutoTryLockEventList(const AutoTryLockEventList&);
            AutoTryLockEventList& operator=(const AutoTryLockEventList&);
        };
    };
}
}
}

#endif // EA_JOBS_LOCAL_BACKEND_H
