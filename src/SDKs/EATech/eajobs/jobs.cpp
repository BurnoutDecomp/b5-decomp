#include "SDKs/EATech/eajobs/jobs.h"

#include <intrin.h> // _InterlockedExchange (MSVC atomic intrinsic)

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

    // dbl_8327F288 -- the lazily-cached seconds-per-tick value (0.0 until the first
    // TicksToSeconds call computes it).
    static f64 sdSecondsPerTick = 0.0;

    // The X360 hardware timebase frequency (asm: lis 0x2F9 / ori 0x838 = 0x02F90838).
    static const u64 KU_TIMEBASE_FREQUENCY = 49875000ull;

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

    // @ 0x82BC9988 -- ticks -> seconds, caching 1.0/freq on first use.
    f32 TicksToSeconds(u64 uTicks)
    {
        f64 ldSecondsPerTick = sdSecondsPerTick;
        if (sdSecondsPerTick == 0.0)
        {
            ldSecondsPerTick = 1.0 / static_cast<f64>(KU_TIMEBASE_FREQUENCY);
            sdSecondsPerTick = ldSecondsPerTick;
        }
        return static_cast<f32>(static_cast<f64>(uTicks) * ldSecondsPerTick);
    }

    // @ 0x82915920 -- identity pass-through (thread id -> job-thread id).
    u32 ToJobThreadId(u32 uThreadId)
    {
        return uThreadId;
    }
}
}
