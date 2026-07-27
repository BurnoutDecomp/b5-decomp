#pragma once

// =====================================================================================
// rw::audio::core::Profiler -- the audio-core CPU-time profiler: it samples the
// free-running CPU cycle counter across a Begin()/End() bracket, accumulates the
// per-bracket cycle cost, and once per (~1s) reporting interval computes a smoothed
// average utilisation figure. Driven from rw::audio::core::Dac::XenonThread (the only
// caller of End).
//
// EARenderWare "rwaudio" middleware. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC) -- the X360 asm is authoritative for member layout and every store width.
// There is NO Feb-2007 source, NO DecFIGS DWARF, and NO ProStreet08 rwaudio PDB entry
// for this type, so every offset below is grounded directly in the disassembly of the
// bodied members (End @0x82B6A9F8 in particular).
//
// LAYOUT AUTHORITY (X360 byte offsets from End's asm; members pinned BY NAME):
//   +0x00  mpVTable          vtable pointer (off_8214B260, reinstalled by the dtor)
//   +0x08  mfNextReportTime  double: next scheduled report timestamp (seconds)
//   +0x10  mfAverage         float:  last computed average utilisation (published output)
//   +0x14  muAccumTicks      u32:    running sum of (elapsed cycles >> 6) this interval
//   +0x18  muReportBaseCycle u32:    (End cycle >> 6) latched at the previous report
//   +0x1C  muBeginCycle      u32:    cycle stamp captured by Begin() (read by End)
//   +0x20  muLastEndCycle    u32:    previous End() cycle stamp (32-bit wrap detect)
//   +0x24  muMaxTicks        u32:    peak (elapsed >> 6) seen this interval
//   +0x28  muHasBaseline     u32:    flag: a first report base has been latched
//   +0x2C  muWrapCount       u32:    32-bit cycle-counter wraps seen this interval
//   +0x30  mpTimeSource      opaque* wall-clock object; its +0x08 is a double (seconds)
//
// X360 pointers are 32-bit; on the 64-bit host mpVTable widens from 4 to 8 bytes, which
// exactly absorbs the X360 +0x04 alignment pad ahead of the +0x08 double, so the named
// layout is preserved. Members are pinned BY NAME (not by absolute offset). GROW additively.
// =====================================================================================

#include "types.hpp" // f32, u32

namespace rw
{
namespace audio
{
namespace core
{

// off_8214B260 -- the Profiler vtable value (installed by System::GetProfiler at
// singleton construction and reinstalled by the deleting destructor). The X360 table's
// two slots are RECOVERED: [0] = Profiler::Release @0x82B6A9B0, [1] = the vector deleting
// destructor @0x82B6DC20 (scratchpad/waveE/AudioSystem.rodata.txt). On the host the slot
// VALUE is modelled as an honest opaque placeholder (the Limiter1 / CMpegBase precedent)
// because no host code dispatches through it -- the one X360 dispatch site
// (System::Release's vt[0] call) resolves statically to Profiler::Release, a leaf class.
// Defined in Profiler.cpp; shared here so System.cpp's GetProfiler installs the SAME
// symbol the destructor reinstalls.
extern void *const KPF_ProfilerVTable;

class Profiler
{
public:
    // ---- bodied in Profiler.cpp (X360 addresses noted per body) ----

    // Close the current Begin()/End() bracket: fold the elapsed cycle cost into the
    // interval accumulators and, once the report interval elapses, publish the average.
    // Returns the sampled cycle count. X360 @0x82B6A9F8.
    static u32 End(Profiler *self);

    // Destroy the profiler: the X360 tail-dispatches the vtable[1] deleting destructor
    // with the "don't free" flag. X360 @0x82B6A9B0.
    static void *Release(Profiler *self);

    // Compiler `vector deleting destructor': reinstall the vtable, then conditionally
    // free when bit 0 of `flags` is set. X360 @0x82B6DC20.
    static void *VectorDeletingDestructor(Profiler *self, char flags);

    // ---- layout (see LAYOUT AUTHORITY above; pinned by name) ----
    void  *mpVTable;          // +0x00
    double mfNextReportTime;  // +0x08
    f32    mfAverage;         // +0x10
    u32    muAccumTicks;      // +0x14
    u32    muReportBaseCycle; // +0x18
    u32    muBeginCycle;      // +0x1C
    u32    muLastEndCycle;    // +0x20
    u32    muMaxTicks;        // +0x24
    u32    muHasBaseline;     // +0x28
    u32    muWrapCount;       // +0x2C
    void  *mpTimeSource;      // +0x30
};

} // namespace core
} // namespace audio
} // namespace rw
