#ifndef EA_JOBS_REFERENCE_COUNT_H
#define EA_JOBS_REFERENCE_COUNT_H

#include "types.hpp"

// SDKs/EATech/eajobs/reference_count.h
//
// EA::Jobs::Detail::ReferenceCount -- a single atomic 32-bit reference counter with
// "alive-only" increment/decrement semantics, reconstructed from the X360 .XEX:
//   Increment @ 0x82BCA520
//   Decrement @ 0x82BCA580
// In the X360 asm `this` (r3) points directly at the counter word -- the object is
// exactly one int. The methods retry a masked-interrupt lwarx/stwcx. reservation
// pair until the compare-and-swap succeeds.
//
// Vendor EA code reconstructed in its canonical home.

#include "SDKs/EATech/eathread/eathread_atomic.h"

namespace EA
{
namespace Jobs
{
namespace Detail
{
    struct ReferenceCount
    {
        // @ 0x82BCA520 -- if the count is already 0 (dead object) refuse and return
        // false; otherwise atomically bump it by 1 and return true.
        bool Increment();

        // @ 0x82BCA580 -- if the count is <= 1 (would drop to 0 / already at the
        // floor) refuse and return false; otherwise atomically drop it by 1 and
        // return true.
        bool Decrement();

        // +0x0 -- `this` points straight at this word in the asm. An EAThread unsigned
        // atomic: the guards compare unsigned (Decrement `cmplwi r11,1; bgt` 0x82BCA5C4) and
        // the CAS result is checked with cmplw (0x82BCA558), as AtomicInt::SetValueConditional.
        EA::Thread::AtomicUint32 mCount;
    };
}
}
}

#endif // EA_JOBS_REFERENCE_COUNT_H
