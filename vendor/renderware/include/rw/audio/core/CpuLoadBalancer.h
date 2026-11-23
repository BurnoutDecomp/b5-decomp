#pragma once

// =====================================================================================
// rw::audio::core::CpuLoadBalancer -- the audio-core CPU-load governor. It sits on the
// owning System (mppVoiceListNodes + the per-System CPU-load fields) and, once per audio
// frame, folds the elapsed CPU-cycle span into a short moving average of the frame cost,
// projects the total voice cost against the System's available CPU budget, and -- when the
// projection overruns -- culls the lowest-priority voices (CullVoices, which selects its
// victim with FindLowestPriorityVoice) until the frame fits again. Dac drives it:
// Dac::CreateInstance calls Init, Dac::StartHandler calls Reset, Dac::Mix / Dac::StopHandler
// call Balance.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for member offsets and stores. The Xbox 360 audio PDB and MAP identify
// the counter, averager and their member methods.
//   CpuLoadBalancer::Init                    @0x82B6C6F0
//   CpuLoadBalancer::Reset                   @0x82B67638
//   CpuLoadBalancer::Balance                 @0x82B6EF00
//   CpuLoadBalancer::FindLowestPriorityVoice @0x82B67658
//   CpuLoadBalancer::CullVoices              @0x82B6E410
//
// X360 offsets are 4-byte-word; the PC layout widens the pointer member to x64 (only the
// member ORDER is load-bearing -- every access in the .cpp is by name, and the +0xNN
// comments are documentary X360 offsets).
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
// =====================================================================================

#include "types.hpp" // f32, u32

namespace rw
{
namespace audio
{
namespace core
{

class System;
class Voice;

struct CpuCycleCounter
{
    void Reset() { mCpuCycles = 0; }

    u32 mCpuCycles;
    u32 mCpuCycleStart;
    bool mStarted;
};

struct CpuCycleAverager
{
    void OverRide(f32 value)
    {
        mAverage = value;
        mHistory[0] = value;
        mHistory[1] = value;
    }

    void Average(f32 value)
    {
        mAverage = (mHistory[1] + mHistory[0] + value) * 0.33333334f; // ARTIST 0x820065E0
        mHistory[mSlotSwitcher] = value;
        mSlotSwitcher = (mSlotSwitcher == 0) ? 1 : 0;
    }

    f32 mAverage;
    f32 mHistory[2];
    s32 mSlotSwitcher;
};

class CpuLoadBalancer
{
public:
    // Seed the balancer against its owning `system`: clear the frame accumulator + the
    // moving-average state and publish the CPU clock rate (3.2 GHz) into the System's
    // load-tracking field. @0x82B6C6F0.
    void Init(System *system);

    // Clear the frame accumulator + the moving-average state (leaves mpSystem intact).
    // @0x82B67638.
    void Reset();

    // Per-frame governor step: fold the elapsed CPU-cycle span into the frame accumulator,
    // update the three-sample moving average, then -- while the System reports headroom --
    // project the active-voice cost against the CPU budget and cull if it overruns. Records
    // its own overhead back into the accumulator and publishes the balance duration into the
    // System. @0x82B6EF00.
    void Balance();

    System *mpSystem;                     // +0x00
    CpuCycleCounter mOverheadCounter;     // +0x04
    CpuCycleAverager mOverheadAverager;    // +0x10

private:

    // Scan the System's active-voice array for the culling victim: the still-active voice
    // with the lowest priority (freshly-created voices deprioritised, ties broken by the
    // lower priority-class index). Returns 0 when no voice qualifies or the lowest priority
    // is at/above the 100.0 floor. @0x82B67658.
    Voice *FindLowestPriorityVoice();

    // Cull the lowest-priority voices until the frame cost fits the CPU budget.
    void CullVoices(f32 afExcessCycles);
};

} // namespace core
} // namespace audio
} // namespace rw
