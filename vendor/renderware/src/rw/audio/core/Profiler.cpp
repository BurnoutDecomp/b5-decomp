// =====================================================================================
// rw::audio::core::Profiler bodies -- the audio-core CPU-time profiler.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every offset, store width and branch. No Feb-2007 source, no DecFIGS
// DWARF, and no ProStreet08 rwaudio PDB entry exist for this type. See Profiler.h for the
// byte layout and the per-function X360 addresses.
//   End                        @0x82B6A9F8 -- fold a bracket + publish the interval average
//   Release                    @0x82B6A9B0 -- tail-dispatch vtable[1] (the deleting dtor)
//   `vector deleting destructor'@0x82B6DC20 -- reinstall the vtable, conditional free
// =====================================================================================

#include "rw/audio/core/Profiler.h"

#include <new> // ::operator delete

namespace rw
{
namespace audio
{
namespace core
{

// Free-running CPU cycle counter (rw::audio::core::GetCpuCycle @todo). Its body lives in a
// platform timing TU that is not yet reconstructed; only the declaration is needed for the
// per-TU compile gate. Returns a monotonically increasing cycle count (r3). Same free
// function TimerManager.cpp declares/uses.
u32 GetCpuCycle();

// off_8214B260 -- the Profiler vtable the deleting destructor reinstalls (and
// System::GetProfiler installs at singleton construction; declared in Profiler.h). Slot
// contents recovered wave E: [0]=Profiler::Release, [1]=this deleting destructor. The
// host value stays an honest placeholder (the Limiter1 / CMpegBase precedent) -- no host
// code dispatches through it.
void *const KPF_ProfilerVTable = nullptr; // off_8214B260

// The wall-clock object the profiler samples (mpTimeSource, self+0x30). Only its +0x08
// double -- the current time in seconds -- is read here, so it is treated as a documented
// opaque foreign aggregate viewed at that fixed offset (the RawPuller2 OutputChannelNode
// precedent). Its real type is not homed in this TU.
namespace
{
    struct ProfilerTimeSource
    {
        char   mReserved0[8]; // +0x00
        double mfTimeSeconds; // +0x08 -- monotonically increasing seconds
    };
}

// Report interval constants (single numeric rodata immediates from End's asm).
static const double KD_REPORT_RUNAWAY   = 10.0;         // dbl_8202D7F8 -- clamp threshold
static const double KD_REPORT_STEP       = 1.0;          // dbl_82001CA0 -- next-report step
static const f32    KF_INTERVAL_SCALE    = 0.0099999998f; // flt_82002138 -- 1% of the interval
static const f32    KF_ZERO              = 0.0f;          // flt_82001CC0

// -------------------------------------------------------------------------------------
// End @0x82B6A9F8 -- close the current profiling bracket.
//
// Sample the CPU cycle counter and the wall clock, detecting a 32-bit cycle wrap since the
// previous End(). Fold the bracket's elapsed cost (elapsed >> 6) into the interval sum and
// peak. Then, once the wall clock reaches the scheduled report time (with a runaway clamp
// so a stalled clock can't push the schedule >10s into the future), compute the smoothed
// average utilisation over the interval, latch the next report base/time, and reset the
// per-interval accumulators. Returns the sampled cycle count.
// -------------------------------------------------------------------------------------
u32 Profiler::End(Profiler *self)
{
    u32 luCycle = GetCpuCycle();

    // Sample current wall-clock seconds from the time source (*(*(self+0x30)+8)).
    double lfNow =
        static_cast<const ProfilerTimeSource *>(self->mpTimeSource)->mfTimeSeconds;

    // A cycle counter that went backwards means the 32-bit counter wrapped.
    if (luCycle < self->muLastEndCycle)
        ++self->muWrapCount;

    u32 luAccum = self->muAccumTicks;                 // v4
    u32 luElapsed = luCycle - self->muBeginCycle;     // v5
    u32 luMax = self->muMaxTicks;                     // v6
    self->muLastEndCycle = luCycle;                   // *(self+0x20) = result

    u32 luTicks = luElapsed >> 6;                     // v7
    self->muAccumTicks = luAccum + luTicks;           // v8 -> *(self+0x14)
    if (luTicks > luMax)
        self->muMaxTicks = luTicks;                   // *(self+0x24) = v7

    // Runaway clamp: never let the report time sit more than 10s ahead of real time.
    if (self->mfNextReportTime - lfNow > KD_REPORT_RUNAWAY)
        self->mfNextReportTime = lfNow;               // *(self+0x08) = v3

    double lfReportTime = self->mfNextReportTime;     // v9
    if (lfNow >= lfReportTime)
    {
        // A reporting interval elapsed. The interval's cycle span is measured from the
        // latched base to the current (>>6) cycle, adding the wrap count in the high bits
        // so a mid-interval 32-bit wrap is accounted for. Utilisation = accumulated cost /
        // (1% of the interval span).
        u32 luAccumSum = self->muAccumTicks;                            // v8
        u32 luReportBase = (self->muWrapCount << 26) + (luCycle >> 6);  // current base (low)
        u32 luPrevBase = self->muReportBaseCycle;                       // previous base

        f32 lfIntervalCost = static_cast<f32>(static_cast<double>(luReportBase))
                           - static_cast<f32>(static_cast<double>(luPrevBase));
        f32 lfAverage = static_cast<f32>(static_cast<double>(luAccumSum))
                      / (lfIntervalCost * KF_INTERVAL_SCALE);

        // Publish only a non-negative figure, and only once a baseline exists (the first
        // interval seeds the base without publishing).
        if (!(lfAverage < KF_ZERO) && self->muHasBaseline)
            self->mfAverage = lfAverage;              // *(self+0x10) = average

        self->muReportBaseCycle = luCycle >> 6;       // *(self+0x18) = result >> 6
        self->muHasBaseline = 1;                      // *(self+0x28) = 1
        self->mfNextReportTime = lfReportTime + KD_REPORT_STEP; // *(self+0x08) = v9 + 1.0
        self->muAccumTicks = 0;                       // *(self+0x14) = 0
        self->muWrapCount = 0;                        // *(self+0x2C) = 0
    }

    return luCycle;
}

// -------------------------------------------------------------------------------------
// Release @0x82B6A9B0 -- destroy the profiler.
//   b (*(*this + 4))(this, 0)
// The X360 tail-dispatches vtable slot 1 with the "don't free" flag (0); that slot is this
// class's own vector deleting destructor. Resolved to the concrete callee here (Profiler is
// a leaf -- no subclass in the ledger overrides the slot).
// -------------------------------------------------------------------------------------
void *Profiler::Release(Profiler *self)
{
    return VectorDeletingDestructor(self, 0);
}

// -------------------------------------------------------------------------------------
// `vector deleting destructor' @0x82B6DC20 -- reinstall the Profiler vtable, then free when
// bit 0 of `flags` is set. (~Profiler is trivial and folded away -- the asm performs no
// member teardown, only the vtable reinstall.)
//   *self = off_8214B260;
//   if (flags & 1) operator delete(self);
//   return self;
// -------------------------------------------------------------------------------------
void *Profiler::VectorDeletingDestructor(Profiler *self, char flags)
{
    self->mpVTable = KPF_ProfilerVTable; // off_8214B260
    if ((flags & 1) != 0)
        ::operator delete(self);
    return self;
}

} // namespace core
} // namespace audio
} // namespace rw
