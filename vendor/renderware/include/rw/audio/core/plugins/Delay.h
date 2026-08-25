#pragma once

// =====================================================================================
// rw::audio::core::Delay -- the "Delay" feedback delay-line audio plug-in.
//
// A PlugIn processing-graph node (sibling of Gain / Pan2D / the Iir2* filter shapes and the
// ReverbModel1 reverb) that runs a single feedback DelayLine coloured by a DelayFilter. Each
// frame Process installs the filter's apply/reset dispatch funcs, allocates a scratch local
// buffer from the System object table, and -- driven by a three-state crossfade machine
// (start / running / re-tune) -- primes or re-delays the line then filters one frame through
// it. A per-frame profiling TimerHandle (registered on the System's TimerManager, callback
// RwacTimerClient) re-sizes the ring whenever the delay-time attribute or the sample rate
// grows the required buffer. UpdateLatencyAndDecay reports the delay latency the mixer must
// pre-roll (Schroeder RT60 form d - d*5/log10(g)).
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and store. There is NO Feb-2007 leak source, NO
// DecFIGS DWARF, and NO ProStreet08 rwaudio PDB entry for this TU, so each offset below is
// grounded directly in the disassembly of the bodied member functions:
//   GetSize                @0x82B96A38 -> 184 (0xB8)
//   Delay (ctor)           @0x82B9DD90
//   GetPpuTicksEvent       @0x82B9DDE0
//   `vector deleting destructor' @0x82B9DDE8
//   UpdateLatencyAndDecay  @0x82B9DE50
//   ReleaseEvent           @0x82B97278
//   RwacTimerClient        @0x82B972C8
//   Process                @0x82BA2A08
//   CreateInstance         @0x82BA2918
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
//
// The PlugIn base sub-object is embedded (composition, not inheritance) as a PlugInBaseView
// at +0x00..+0x23 -- the same idiom Gain / the Iir2* shapes / ReverbModel1 use so the data
// offsets are not shifted by a compiler-inserted vptr. +0x00 is the installed v-table word;
// mBase.mpAttributes (+0x0C) is pointed at the first input attribute (mfDelayTime @+0x28) so
// the base SetAttribute writes land there; the input-channel count lives in mBase.mbFlag20
// (+0x20). mBase.mLatencyInSamples/mDecaySamples (+0x14/+0x18) cache the latency + decay tail (see
// UpdateLatencyAndDecay).
//
// The embedded DelayFilter / DelayLine / TimerHandle widen on the x64 PC target (they carry
// pointer members), so the +0xNN annotations below are the X360 (32-bit) offsets and are
// documentary only -- only the member ORDER is load-bearing, and every access is by name.
// GetSize returns the X360 constant 184, not sizeof(Delay).
// =====================================================================================

#include "types.hpp" // f32, s32, u8, u32

#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioProcessContext
#include "rw/audio/core/DelayFilter.h" // DelayFilter
#include "rw/audio/core/DelayLine.h"   // DelayLine
#include "rw/audio/core/TimerHandle.h" // TimerHandle

namespace rw
{
namespace audio
{
namespace core
{

class System; // owning rwaudio sub-system (PlugIn.h) -- used only through the .cpp

// -------------------------------------------------------------------------------------
// Delay -- byte-exact X360 layout (X360 sizeof 0xB8 == 184; GetSize is hard-coded).
//
// One feedback DelayLine (mDelayLine) coloured by one DelayFilter (mFilter), reconfigured by
// a profiling TimerHandle (mTimer). mState is the crossfade machine: 0 = idle/priming,
// 1 = running, 2 = re-delaying (waiting for enough valid samples), >=3 = inert.
// -------------------------------------------------------------------------------------
class Delay
{
public:
    // ---- factory / v-table surface ----
    static int    GetSize();                                       // @0x82B96A38 -> 184
    static Delay *Delay_ctor(Delay *self);                         // @0x82B9DD90
    static u32    GetPpuTicksEvent(Delay *self);                   // @0x82B9DDE0
    static int    CreateInstance(Delay *self, f32 *pMaxDelayTime); // @0x82BA2918

    // `vector deleting destructor' @0x82B9DDE8 -- destruct the embedded TimerHandle (trivial /
    // folded to the image's shared empty thunk 0x82AD5078) and DelayLine, reinstall the base
    // PlugIn v-table, then free when (flags & 1). See the .cpp for the STUB resolution.
    static void  *VectorDeletingDestructor(Delay *self, char flags);

    // ---- per-frame processing / configuration ----
    static int    Process(Delay *self, AudioProcessContext *ctx);  // @0x82BA2A08
    static void   UpdateLatencyAndDecay(Delay *self);              // @0x82B9DE50
    static int    RwacTimerClient(Delay *self);                   // @0x82B972C8
    static int    ReleaseEvent(Delay *self);                      // @0x82B97278

    // ---- layout (X360 offsets documentary; access by name) ----
    PlugInBaseView mBase;                 // +0x00 .. +0x23 (mbFlag20 @+0x20 = input channels)
    char       mPad24[0x28 - 0x24];       // +0x24 .. +0x27
    // ---- input attributes (8-byte-stride attribute table; base = &mfDelayTime) ----
    f32        mfDelayTime;               // +0x28 -- live delay time, seconds (attribute 0)
    char       mPad2C[0x30 - 0x2C];       // +0x2C
    f32        mfFeedback;                // +0x30 -- feedback gain (attribute 1)
    char       mPad34[0x38 - 0x34];       // +0x34
    // ---- crossfade / run state ----
    s32        mState;                    // +0x38 -- crossfade machine (0/1/2, >=3 inert)
    f32        mfSampleRate;              // +0x3C -- internal rate, 48000 (RwacTimerClient latch)
    f32        mfMaxDelayTime;            // +0x40 -- max delay time, seconds (ctor param / sizing)
    // ---- embedded processing objects ----
    DelayFilter mFilter;                  // +0x44 -- feedback filter (mfFeedback @+0x54)
    DelayLine   mDelayLine;               // +0x5C -- delay ring (miReadPosition @+0x84)
    TimerHandle mTimer;                   // +0x98 -- profiling / reconfigure timer (mCpuTicks @+0xA8)
    u8         mbActive;                  // +0xB0 -- the reconfigure timer is registered
    char       mPadB1[0xB8 - 0xB1];       // +0xB1 (X360 sizeof 0xB8 == 184; GetSize is hard-coded)
};

} // namespace core
} // namespace audio
} // namespace rw
