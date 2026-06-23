#ifndef EA_JOBS_DETAIL_H
#define EA_JOBS_DETAIL_H

#include "types.hpp"

// SDKs/EATech/eajobs/detail.h
//
// EA::Jobs::Detail namespace-scope helpers. This header homes:
//   WaitOnYieldHelper @ 0x82BC9B60
// the shared "should I keep spinning?" decision used by JobInstanceHandle::WaitOn:
// optionally run a user callback, optionally sleep, then decide whether to yield
// again based on an elapsed-time budget and an optional global watchdog predicate.
//
// Vendor EA code reconstructed in its canonical home.

namespace EA
{
namespace Jobs
{
namespace Detail
{
    // A user wait callback: returns nonzero to keep waiting, 0 to stop. (X360 calls
    // it through ctr with the single arg in r3.)
    typedef int (*WaitOnYieldCallback)(int iContext);

    // @ 0x82BC9B60 -- one iteration of WaitOn's yield loop.
    //   pCallback   : optional predicate; if present and it returns 0, stop (false).
    //   iContext    : argument passed to pCallback.
    //   lSleepMs    : if >= 0, sleep this many ms (via EA::Thread::ThreadSleep).
    //   iStartTicks : the QPC tick stamp the wait started at (elapsed budget base).
    //   pbDone      : the done flag; if already set, keep waiting (true).
    // Returns nonzero to continue yielding, 0 to give up.
    int WaitOnYieldHelper(WaitOnYieldCallback pCallback,
                          int                 iContext,
                          s32                 lSleepMs,
                          int                 iStartTicks,
                          const u8*           pbDone);
}
}
}

#endif // EA_JOBS_DETAIL_H
