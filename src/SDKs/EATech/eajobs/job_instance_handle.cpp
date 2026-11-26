// XDK size optimization matches ARTIST at 0x82BC9A08.
#if defined(_XBOX) && defined(_MSC_VER)
#pragma optimize("s", on)
#endif

#include "SDKs/EATech/eajobs/job_types.h" // EA::Jobs::JobInstanceHandle, SchedulerBackend
#include "SDKs/EATech/eajobs/event.h"
#include "SDKs/EATech/eajobs/detail.h"     // EA::Jobs::Detail::WaitOnYieldHelper

#include <xtl.h>  // QueryPerformanceCounter, LARGE_INTEGER

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

        // ARTIST base slots1C/44/48/4C resolve to the genuine bare blr82AD5078.
        void SchedulerBackend::WakeThreads() {}
        void SchedulerBackend::SetProfilingCallback(ProfilerCallback*) {}
        void SchedulerBackend::SetProfilingContext(void*) {}
        void SchedulerBackend::FlushProfile() {}

        // ARTIST base slot50 resolves to li r3,0;blr82B0F1B8.
        bool SchedulerBackend::RunSingleJob(int) { return false; }

    }


    // Inlined everywhere in ARTIST (no out-of-line copy): backend, then index, then
    // submission id (JobInstanceHandle::WaitOn 0x82BCA49C..0x82BCA4CC).
    bool JobInstanceHandle::Valid() const
    {
        return mSchedulerBackend != 0 && (mIndex != 0 || mSubmissionId != 0);
    }

    // @ 0x82BC9A50
    Event JobInstanceHandle::AddBarrier()
    {
        // ARTIST82BC9A50: r3 is the Event return buffer, r4 is this.
        // DWARF confirms Event AddBarrier(), with no explicit argument.
        return mSchedulerBackend->AddBarrier(*this);
    }

    // @ 0x82BCA470
    void JobInstanceHandle::WaitOn(WaitOnCallback* pYieldCallback,
                                   void* pYieldContext, s32 lYieldSleepMs) const
    {
        u8 lbDone = 0; // v10[0] -- the done flag handed to WaitOnYieldHelper

        LARGE_INTEGER lStart;
        QueryPerformanceCounter(&lStart);

        bool lbContinue = false;
        do
        {
            // Loop predicate (asm): keep going only while the handle is valid.
            if (!Valid())
                break;

            // Ask the backend whether the instance has finished.
            if (mSchedulerBackend->IsDone(*this))
                break;

            // One yield pass: optional user predicate, optional sleep, elapsed-time
            // budget. Returns true to keep waiting, false to give up.
            lbContinue = Detail::WaitOnYieldHelper(pYieldCallback, pYieldContext,
                lYieldSleepMs, static_cast<u64>(lStart.QuadPart), &lbDone);
        }
        while (lbContinue);

    }
}
}
