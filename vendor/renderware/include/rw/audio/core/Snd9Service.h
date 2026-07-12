#pragma once

// =====================================================================================
// rw::audio::core::Snd9Service -- the Snd9 (Xbox/Win32) hardware audio-output service.
//
// The concrete audio-core "service" that drives the low-level Snd9 mixer/voice HAL: it
// owns a 6-channel software mix buffer, registers a per-tick profiling timer whose
// callback (RwacTimerClient) pumps the Snd9 mixer in <=256-sample chunks at 48 kHz, and
// interleaves the six mix channels into the driver's double-buffered output block on
// Process(). A rw::audio::core::TimerHandle is embedded (+0x24) for the timer
// registration.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset and every store. There is NO matching
// Feb-2007 leak source, NO DecFIGS DWARF, and NO ProStreet08 PDB entry for this type, so
// every offset below is grounded directly in the disassembly of the bodied methods:
//   CreateInstance                @0x82BA0130
//   GetPlugInDescRunTime          @0x82B9BB28
//   Process                       @0x82B9BD80
//   ReleaseEvent                  @0x82B9BB38
//   RwacTimerClient               @0x82B9BC38
//   UpdateSnd9SampleRate          @0x82B9BB98
//   `vector deleting destructor'  @0x82BA00D0
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable for matching a
// third-party/legacy API). The +0x00 vtable word + a leading base-Service header (the
// same base whose v-table off_820AA810 the deleting destructor reinstalls -- shared with
// the PlugIn family) are embedded directly (composition, not inheritance) so the data
// offsets are not shifted by a compiler-inserted vptr; the same idiom Gain / the Iir2
// filter shapes use. The +0xNN annotations are the X360 (32-bit-pointer) offsets from the
// asm and are documentary only -- members carry x64 widths (the PC target), so only the
// member ORDER is load-bearing and every access is by name.
//   +0x00  mpVTable            (installed v-table word)
//   +0x04  mpSystem            (owning rw::audio::core::System)
//   +0x08  (base-Service header, un-homed here -- opaque)
//   +0x24  mTimerHandle        (embedded TimerHandle; timer registration)
//   +0x3C  mpMixBufStart       (6*1024-byte software mix allocation base)
//   +0x40  mpMixChannels[6]    (the six 1024-byte channel sub-buffers)
//   +0x58  mfSampleRate        (f32, 48000.0)
//   +0x5C  miSampleCounter     (100 Hz tick accumulator)
//   +0x60  miSampleAccum       (produced-sample running total)
//   +0x64  miSamplesRemaining  (samples still owed this service pass)
//   +0x68  mbTimerCreated      (u8; 1 once AddTimer succeeded -- gates ReleaseEvent unregister)
// =====================================================================================

#include "types.hpp" // f32, f64, s32, u8

#include "rw/audio/core/TimerHandle.h" // rw::audio::core::TimerHandle -- embedded by value (+0x24)

namespace rw
{
namespace audio
{
namespace core
{

class System;                // owning sub-system (pointer member; full def in PlugIn.h)
struct AudioProcessContext;  // Process's graph context (Iir2Filters.h); pointer param only

// -------------------------------------------------------------------------------------
// Snd9Service -- see the file header for the byte-exact layout.
// -------------------------------------------------------------------------------------
class Snd9Service
{
public:
    // Placement-init over `self`: install the v-table, default-construct the embedded
    // TimerHandle, carve the 6-channel mix buffer out of a single System allocation, seed
    // the 48 kHz book-keeping, and register the pump timer. Returns 1 on success, 0 if the
    // mix allocation or the timer registration failed. @0x82BA0130.
    static int CreateInstance(Snd9Service *self);

    // Return the address of the registered run-time descriptor record (label "Snd9Service").
    // @0x82B9BB28.
    static char **GetPlugInDescRunTime();

    // Interleave the six mix channels into the context's destination output block (fixed
    // channel permutation), then ping-pong the src/dst block slots. Returns 1. @0x82B9BD80.
    static int Process(Snd9Service *self, AudioProcessContext *ctx);

    // Tear the service down: unregister the pump timer (if it was created), restore the Snd9
    // driver, and free the mix buffer. @0x82B9BB38.
    static int ReleaseEvent(Snd9Service *self);

    // The registered profiling-timer callback: force the Snd9 mixer to 48 kHz if it drifted,
    // then pump the mixer/packet-player in <=256-sample chunks for one 100 Hz service window,
    // advancing the per-channel read cursors. (The timer passes a time value the body
    // ignores -- the asm uses only r3 = the service context.) @0x82B9BC38.
    static int RwacTimerClient(Snd9Service *self);

    // Retune the Snd9 mixer to `sampleRate`: destroy the current mixer, then recreate it with
    // the rounded rate, the voice-free callback, and the driver's channel-config bytes. (The
    // asm passes the rate in f1 only; r3 is stale.) @0x82B9BB98.
    static int UpdateSnd9SampleRate(f64 sampleRate);

    // Compiler `vector deleting destructor': destruct the embedded TimerHandle sub-object,
    // reinstall the base-Service v-table, then conditionally free. @0x82BA00D0.
    static void *VectorDeletingDestructor(Snd9Service *self, char flags);

    void       *mpVTable;            // +0x00  installed v-table word
    System     *mpSystem;           // +0x04  owning sub-system
    u8          mPad08[0x24 - 0x08]; // +0x08 .. +0x23  base-Service header (un-homed; opaque)
    TimerHandle mTimerHandle;       // +0x24  embedded profiling-timer handle
    void       *mpMixBufStart;      // +0x3C  6*1024-byte mix allocation base
    void       *mpMixChannels[6];   // +0x40 .. +0x57  the six 1024-byte channel buffers
    f32         mfSampleRate;       // +0x58  current mixer rate (48000.0)
    s32         miSampleCounter;    // +0x5C  100 Hz tick accumulator
    s32         miSampleAccum;      // +0x60  produced-sample running total
    s32         miSamplesRemaining; // +0x64  samples still owed this service pass
    u8          mbTimerCreated;     // +0x68  1 once the pump timer is registered
};

} // namespace core
} // namespace audio
} // namespace rw
