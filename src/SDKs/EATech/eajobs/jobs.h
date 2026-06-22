#ifndef EA_JOBS_JOBS_H
#define EA_JOBS_JOBS_H

#include "types.hpp"

// SDKs/EATech/eajobs/jobs.h
//
// EA::Jobs namespace-level facade helpers for the EA Tech "job_manager" SDK,
// reconstructed from the X360 .XEX (BURNOUT_X360_ARTIST.XEX). Sibling to the
// committed EA::Jobs::Event (event.h) / EA::Jobs::EntryPoint (entrypoint.cpp)
// surfaces -- this header homes the loose namespace-scope functions:
//
//   AtomicStore(u32* puLocation, u32 uValue)  @ 0x82BCC6E8
//   SetAllocator(void* pAllocator)            @ 0x82BC9830
//   TicksToSeconds(u64 uTicks)                @ 0x82BC9988
//   ToJobThreadId(u32 uThreadId)              @ 0x82915920
//
// Vendor EA code reconstructed in its canonical home; members/names follow the EA
// SDK convention, not the Brn/Cgs project convention.

namespace EA
{
namespace Jobs
{
    // @ 0x82BCC6E8 -- atomically store uValue into *puLocation. On X360 this masks
    // interrupts (mfmsr/mtmsree) around a lwarx/stwcx. reservation pair, retrying
    // until the store-conditional succeeds. Returns puLocation (the X360 leaves the
    // target address in r3). The portable reconstruction expresses the same atomic
    // overwrite.
    u32* AtomicStore(u32* puLocation, u32 uValue);

    // @ 0x82BC9830 -- install the process-wide Jobs allocator (stored in the
    // off_8327F280 file static read by the rest of the job_manager SDK).
    void SetAllocator(void* pAllocator);

    // @ 0x82BC9988 -- convert a hardware tick count to seconds. The first call
    // lazily computes and caches the per-tick second value (1.0 / 49875000.0, the
    // X360 timebase frequency) in dbl_8327F288; subsequent calls reuse it. Returns a
    // single-precision-rounded result (the X360 frsp's f1 before returning).
    f32 TicksToSeconds(u64 uTicks);

    // @ 0x82915920 -- reinterpret a thread id as a job-thread id (identity pass
    // through; the X360 round-trips r3 through the stack and returns it unchanged).
    u32 ToJobThreadId(u32 uThreadId);
}
}

#endif // EA_JOBS_JOBS_H
