#include "SDKs/EATech/eajobs/job_types.h" // EA::Jobs::JobInstanceHandle, SchedulerBackend
#include "SDKs/EATech/eajobs/detail.h"     // EA::Jobs::Detail::WaitOnYieldHelper

#include <windows.h>  // QueryPerformanceCounter, LARGE_INTEGER
#include <stdint.h>   // uintptr_t

// ============================================================================
// SDKs/EATech/eajobs/job_instance_handle.cpp
//
// EA::Jobs::JobInstanceHandle methods, reconstructed store-for-store from the
// X360 .XEX (BURNOUT_X360_ARTIST.XEX):
//   JobInstanceHandle::AddBarrier @ 0x82BC9A50
//   JobInstanceHandle::WaitOn     @ 0x82BCA470
//
// Both methods are thin forwarders into the owning scheduler backend's virtual
// dispatch. The X360 asm reads three values out of the 16-byte handle and hands
// them to the backend:
//   * mSubmissionId       -- the 64-bit submission id (ld 0(handle))
//   * mSchedulerBackend   -- the backend object the virtual is dispatched on (the
//                            low word of the +0x8 qword; lwz 8(handle))
//   * the packed +0x8 qword -- backend-ptr/index/padding read as one 64-bit key
//                            (ld 8(handle)); reconstructed below from the named
//                            members so no reinterpret-cast over the layout is used.
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
    // @ 0x82BC9A08 -- Detail::SchedulerBackend base scalar-deleting destructor. The
    // X360 thunk stores the SchedulerBackend vtable (off_82182408) into *this
    // (stw r11, 0(r31)), then for the deleting variant conditionally
    // operator-delete's (clrlwi. r10, r4, 31; beq skip; bl operator_delete) and
    // returns this. SchedulerBackend is an abstract base with no members and no base
    // subobject, so the body is empty: defining it out-of-line emits exactly the
    // vtable store + conditional operator-delete tail. (Adjacent in the .text to
    // JobInstanceHandle::AddBarrier @ 0x82BC9A50, which dispatches into this backend.)
    namespace Detail
    {
        SchedulerBackend::~SchedulerBackend()
        {
        }
    }

    // Reassemble the 64-bit value the X360 reads with `ld 8(handle)`: on the
    // big-endian X360 the qword at +0x8 is { mSchedulerBackend@+0x8 (4B, high),
    // mIndex@+0xC (2B), mPadding@+0xE (2B, low) }. Built from named members.
    static u64 PackHandleQword(const JobInstanceHandle& rHandle)
    {
        const u64 luBackend = static_cast<u64>(
            reinterpret_cast<uintptr_t>(rHandle.mSchedulerBackend));
        return (luBackend << 32)
             | (static_cast<u64>(rHandle.mIndex)   << 16)
             |  static_cast<u64>(rHandle.mPadding);
    }

    // @ 0x82BC9A50
    JobInstanceHandle& JobInstanceHandle::AddBarrier(const JobInstanceHandle& lrDependency)
    {
        // The X360 reads the backend, submission id and packed +0x8 qword from the
        // DEPENDENCY handle (a2 = lrDependency) -- NOT from this -- then dispatches that
        // backend's vtable slot +0x38, passing THIS handle as the dependent. (The binary
        // hands the dependent handle in r3 and the backend in r4; modelled here as the
        // backend virtual taking the dependent handle as its first explicit argument --
        // the same four operands reach the same slot-0x38 dispatch.)
        Detail::SchedulerBackend* lpBackend = lrDependency.mSchedulerBackend;
        lpBackend->AddBarrier(this, lrDependency.mSubmissionId, PackHandleQword(lrDependency));
        return *this;
    }

    // @ 0x82BCA470
    int JobInstanceHandle::WaitOn(Detail::WaitOnYieldCallbackArg pYieldCallback,
                                  int                            iYieldContext,
                                  s32                            lYieldSleepMs)
    {
        u8 lbDone = 0; // v10[0] -- the done flag handed to WaitOnYieldHelper

        LARGE_INTEGER lStart;
        QueryPerformanceCounter(&lStart);

        int liResult = 0;
        do
        {
            // Loop predicate (asm): keep going only while a backend is attached AND
            // the handle still names a live instance (index or submission id set).
            const bool lbPending =
                (mSchedulerBackend != 0) && (mIndex != 0 || mSubmissionId != 0);
            if (!lbPending)
                break;

            // Ask the backend whether the instance has finished. Nonzero == done.
            liResult = mSchedulerBackend->IsJobComplete(mSubmissionId, PackHandleQword(*this));
            if (liResult)
                break;

            // One yield pass: optional user predicate, optional sleep, elapsed-time
            // budget. Returns nonzero to keep waiting, 0 to give up.
            liResult = Detail::WaitOnYieldHelper(pYieldCallback, iYieldContext,
                                                 lYieldSleepMs, lStart.LowPart, &lbDone);
        }
        while (liResult);

        return liResult;
    }
}
}
