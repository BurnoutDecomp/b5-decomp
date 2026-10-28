#include "SDKs/EATech/eajobs/jobs.h"

#include <xtl.h> // native atomic intrinsics

// ARTIST calls this real XDK conversion helper even for the cold frequency.
extern "C" f64 __u64tod(u64 value);

// ============================================================================
// SDKs/EATech/eajobs/jobs.cpp
//
// EA::Jobs namespace-level facade helpers, reconstructed store-for-store from the
// X360 .XEX (BURNOUT_X360_ARTIST.XEX):
//   EA::Jobs::AtomicStore     @ 0x82BCC6E8
//   EA::Jobs::SetAllocator    @ 0x82BC9830
//   EA::Jobs::TicksToSeconds  @ 0x82BC9988
//   EA::Jobs::ToJobThreadId   @ 0x82915920
//
// Vendor EA code reconstructed in its canonical home.
// ============================================================================

namespace EA
{
namespace Jobs
{
    // ---- file-scope statics (the X360 .data symbols the helpers touch) --------
    // off_8327F280 -- the process-wide Jobs allocator, installed by SetAllocator
    // and consumed by the rest of the job_manager SDK.
    static void* spAllocator = 0;

    // @ 0x82BCC6E8 -- atomic store of uValue into *puLocation; returns puLocation.
    u32* AtomicStore(u32* puLocation, u32 uValue)
    {
        _InterlockedExchange(reinterpret_cast<volatile long*>(puLocation),
                             static_cast<long>(uValue));
        return puLocation;
    }

    // @ 0x82BC9830 -- store the supplied allocator into the file static.
    void SetAllocator(void* pAllocator)
    {
        spAllocator = pAllocator;
    }

    // Typed view of the installed allocator (off_8327F280) for the SDK's
    // scalar-deleting destructors. The stored object is the polymorphic Jobs
    // allocator; the host installs it via SetAllocator.
    Allocator* GetAllocator()
    {
        return static_cast<Allocator*>(spAllocator);
    }

    // @ 0x82BC9988 -- native 49,875,000-Hz ticks; cache the reciprocal on first use.
    f32 TicksToSeconds(u64 uTicks)
    {
        static f64 ldSecondsPerTick = 0.0;
        if (ldSecondsPerTick == 0.0)
            ldSecondsPerTick = 1.0 / __u64tod(static_cast<u64>(0x02F90838u));
        return static_cast<f32>(static_cast<f64>(uTicks) * ldSecondsPerTick);
    }

    // @ 0x82915920 -- identity pass-through (thread id -> job-thread id).
    u32 ToJobThreadId(u32 uThreadId)
    {
        return uThreadId;
    }
}
}
