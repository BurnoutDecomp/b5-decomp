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
// authoritative for every member offset and store. No Feb-2007 leak source and no DecFIGS
// DWARF exist for this TU, and it is absent from the ProStreet08 rwaudio PDB, so each
// offset below is grounded directly in the disassembly of the bodied members:
//   CpuLoadBalancer::Init                    @0x82B6C6F0
//   CpuLoadBalancer::Reset                   @0x82B67638
//   CpuLoadBalancer::Balance                 @0x82B6EF00
//   CpuLoadBalancer::FindLowestPriorityVoice @0x82B67658
//   CpuLoadBalancer::CullVoices              (separate ledger TU -- declared here only)
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

class CpuLoadBalancer
{
public:
    // Seed the balancer against its owning `system`: clear the frame accumulator + the
    // moving-average state and publish the CPU clock rate (3.2 GHz) into the System's
    // load-tracking field. Returns `self`. @0x82B6C6F0.
    static CpuLoadBalancer *Init(CpuLoadBalancer *self, System *system);

    // Clear the frame accumulator + the moving-average state (leaves mpSystem intact).
    // Returns `self`. @0x82B67638.
    static CpuLoadBalancer *Reset(CpuLoadBalancer *self);

    // Per-frame governor step: fold the elapsed CPU-cycle span into the frame accumulator,
    // update the three-sample moving average, then -- while the System reports headroom --
    // project the active-voice cost against the CPU budget and cull if it overruns. Records
    // its own overhead back into the accumulator and publishes the balance duration into the
    // System. Returns the final CPU-cycle snapshot. @0x82B6EF00.
    static u32 Balance(CpuLoadBalancer *self);

    // Scan the System's active-voice array for the culling victim: the still-active voice
    // with the lowest priority (freshly-created voices deprioritised, ties broken by the
    // lower priority-class index). Returns 0 when no voice qualifies or the lowest priority
    // is at/above the 100.0 floor. @0x82B67658.
    static Voice *FindLowestPriorityVoice(CpuLoadBalancer *self);

    // Cull the lowest-priority voices until the frame cost fits the CPU budget. Its body is a
    // separate ledger TU; declared here because Balance calls it (r3 = self).
    static u32 CullVoices(CpuLoadBalancer *self);

    // ----------------------------------------------------------------------------------
    // Layout. X360 (4-byte-word) offsets in the +0xNN comments; access is by name.
    // ----------------------------------------------------------------------------------
    System *mpSystem;      // +0x00  the owning audio-core System
    u32 muFrameCycles;     // +0x04  CPU cycles accumulated for the current audio frame
    u32 muLastStamp;       // +0x08  last GetCpuCycle() snapshot (span anchor)
    u32 mUnk0C;            // +0x0C  (untouched by the reconstructed bodies)
    f32 mfAverageLoad;     // +0x10  three-sample moving average of muFrameCycles
    f32 mafHistory[2];     // +0x14  the two alternating history samples
    u32 muHistoryIndex;    // +0x1C  toggles 0<->1: which history slot Balance overwrites
};

} // namespace core
} // namespace audio
} // namespace rw
