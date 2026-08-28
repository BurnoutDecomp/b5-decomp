#pragma once

// =====================================================================================
// rw::audio::core::AiffWriter -- an audio-core debug "record to disk" plug-in that writes
// the live mix out as a standard AIFF (Audio Interchange File Format) file.
//
// It is driven by two deferred commands posted into the System command ring (EventEvent):
// a "start" record names the output file, a "stop" record finalises it. On the audio
// thread the ring dispatches those records to StartHandler / StopHandler: StartHandler
// opens the file, reserves an 82-byte header placeholder, and registers a profiling timer
// (RwacTimerClient) that pumps the recording; Process interleaves each block's channels to
// signed-16-bit PCM into mpBuf; StopHandler seeks back and rewrites the header (FORM / COMM
// / INST / SSND chunks) with the final sizes, then closes the file and drops the timer. A
// rw::audio::core::TimerHandle is embedded (+0x24) for that timer registration.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and every store. There is NO matching Feb-2007 leak
// source, NO DecFIGS DWARF, and NO ProStreet08 PDB entry for this type, so every offset
// below is grounded directly in the disassembly of the bodied methods:
//   CreateInstance                @0x82B9D488
//   EventEvent                    @0x82BA5448
//   GetPlugInDescRunTime          @0x82B968B0
//   GetSize                       @0x82B96838
//   IntToExtended                 @0x82B96848
//   Process                       @0x82BA2168
//   ReleaseEvent                  @0x82BA53F8
//   StartHandler                  @0x82B9D510
//   StopHandler                   @0x82BA1EC0
//   `scalar deleting destructor'  @0x82B9D428
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable for matching a
// third-party/legacy API). The +0x00 vtable word + a leading base header (the same base
// whose v-table off_820AA810 the deleting destructor reinstalls -- shared with the PlugIn /
// Snd9Service family) are embedded directly (composition, not inheritance) so the data
// offsets are not shifted by a compiler-inserted vptr; the same idiom Snd9Service / the Iir2
// filter shapes use. The +0xNN annotations are the X360 (32-bit-pointer) offsets from the
// asm and are documentary only -- members carry x64 widths (the PC target), so only the
// member ORDER is load-bearing and every access is by name.
//   +0x00  mpVTable            (installed v-table word)
//   +0x04  mpSystem            (owning rw::audio::core::System)
//   +0x08  (base header, un-homed here -- opaque)
//   +0x20  muChannelCount      (u8; channels in the recorded/interleaved block)
//   +0x21  muBufferChannels    (u8; channel count the mpBuf allocation is sized for)
//   +0x24  mTimerHandle        (embedded TimerHandle; timer registration)
//   +0x3C  mpFile              (open output FILE*, 0 when not recording)
//   +0x40  mpBuf               (interleaved s16 PCM staging allocation)
//   +0x44  muSampleFrameCount  (u32; sample frames written so far -- the AIFF frame count)
//   +0x48  muSampleRate        (u32; output sample rate, latched from the format on Process)
//   +0x4C  mbDataWritten       (u8; set once Process has flushed a block)
//   +0x4D  mbTimerCreated      (u8; 1 once the pump timer is registered)
// =====================================================================================

#include "types.hpp" // u8, u32

#include <cstdio> // std::FILE (mpFile)

#include "rw/audio/core/TimerHandle.h" // rw::audio::core::TimerHandle -- embedded by value (+0x24)

namespace rw
{
namespace audio
{
namespace core
{

class System;                // owning sub-system (pointer member; full def in PlugIn.h)
class Mixer;                             // the stage process context (unified; Mixer.h)
typedef Mixer AudioProcessContext;       // pointer param only

// -------------------------------------------------------------------------------------
// AiffWriter -- see the file header for the byte-exact layout.
// -------------------------------------------------------------------------------------
class AiffWriter
{
public:
    // Placement-init over `self`: install the v-table, default-construct the embedded
    // TimerHandle, clear the file/flag fields, and allocate the s16 PCM staging buffer
    // (muBufferChannels * 512 bytes) through the System allocator. Returns 1 if the buffer
    // allocated, 0 otherwise. @0x82B9D488.
    static int CreateInstance(AiffWriter *self);

    // Post a start (a2 == 0, `pName` -> the output path) or stop (a2 != 0) record into the
    // System deferred-command ring; the ring later dispatches it to StartHandler /
    // StopHandler. Returns `self`. @0x82BA5448.
    static AiffWriter *EventEvent(AiffWriter *self, int a2, char **pName);

    // Return the address of the registered run-time descriptor record (label "AiffWriter").
    // @0x82B968B0.
    static char **GetPlugInDescRunTime();

    // The plug-in instance footprint (80 bytes). @0x82B96838.
    static int GetSize();

    // Convert an unsigned integer `uValue` into its 80-bit IEEE-754 extended-precision
    // big-endian form (10 bytes) in `pExtended`, as the AIFF COMM chunk stores the sample
    // rate. Returns `pExtended`. @0x82B96848.
    static u8 *IntToExtended(u8 *pExtended, u32 uValue);

    // Interleave one block's channels of the graph context's source buffer to signed-16-bit
    // PCM in mpBuf (via a System scratch reservation), when recording is live. Returns 1.
    // @0x82BA2168.
    static int Process(AiffWriter *self, AudioProcessContext *ctx);

    // Finalise a recording: run StopHandler to flush + close, then free the PCM buffer.
    // @0x82BA53F8.
    static int ReleaseEvent(AiffWriter *self);

    // Deferred "start recording" ring handler: open the file named in the record, write the
    // 82-byte header placeholder, and register the pump timer. Returns the record's byte
    // size (used by the ring to advance). @0x82B9D510.
    static int StartHandler(void *pCommandRecord);

    // Deferred "stop recording" ring handler: seek to 0 and rewrite the AIFF header (FORM /
    // COMM / INST / SSND) with the final sizes, close the file, and drop the pump timer.
    // Returns 8 (the record's byte size). @0x82BA1EC0.
    static int StopHandler(void *pCommandRecord);

    // The registered profiling-timer callback that pumps the recording. Defined in its own
    // ledger TU; declared here because StartHandler takes its address for AddTimer.
    static int RwacTimerClient(AiffWriter *self);

    // Compiler `scalar deleting destructor': destruct the embedded TimerHandle sub-object,
    // reinstall the base v-table, then conditionally free. @0x82B9D428.
    static void *ScalarDeletingDestructor(AiffWriter *self, char flags);

    void       *mpVTable;             // +0x00  installed v-table word
    System     *mpSystem;            // +0x04  owning sub-system
    u8          mPad08[0x20 - 0x08];  // +0x08 .. +0x1F  base header (un-homed; opaque)
    u8          muChannelCount;      // +0x20  channels in the recorded block
    u8          muBufferChannels;    // +0x21  channel count the mpBuf allocation is sized for
    u8          mPad22[0x24 - 0x22];  // +0x22 .. +0x23  alignment pad
    TimerHandle mTimerHandle;        // +0x24  embedded profiling-timer handle
    std::FILE  *mpFile;              // +0x3C  open output file (0 when idle)
    void       *mpBuf;               // +0x40  interleaved s16 PCM staging allocation
    u32         muSampleFrameCount;  // +0x44  sample frames written (AIFF frame count)
    u32         muSampleRate;        // +0x48  latched output sample rate
    u8          mbDataWritten;       // +0x4C  set once Process has flushed a block
    u8          mbTimerCreated;      // +0x4D  1 once the pump timer is registered
    u8          mPad4E[0x50 - 0x4E];  // +0x4E .. +0x4F  tail pad to sizeof 0x50
};

} // namespace core
} // namespace audio
} // namespace rw
