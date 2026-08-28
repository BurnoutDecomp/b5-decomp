#pragma once

// =====================================================================================
// rw::audio::core::SinePlayer -- a sine-tone generator source plug-in. A PlugIn
// processing-graph node that synthesises a pure sine wave at a settable frequency into
// every channel of its output buffer, with a deferred play/stop command path through the
// owning System's command ring and a per-sample "start time" gate (samples before the
// scheduled start are written as silence).
//
// EARenderWare "rwaudio" middleware. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC); the asm is authoritative for every member offset and every store. There is
// NO matching Feb-2007 leak source, NO DecFIGS DWARF, and no ProStreet08 rwaudio PDB
// entry for this type, so each offset below is grounded directly in the disassembly of:
//   CreateInstance                @0x82BA3FB8  (install vtable, clear phase/frequency)
//   GetSize                       @0x82B98308  (X360 constant 72 == 0x48; host sizeof)
//   PlayHandler                   @0x82B9B900  -> X360 li r3,0x10; host sizeof(SinePlayerPlayCommand)
//                                                (deferred play cmd: latch start time)
//   StopHandler                   @0x82B9B920  -> X360 li r3,8;    host sizeof(SinePlayerStopCommand)
//                                                (deferred stop cmd: clear playing)
//   PreProcess                    @0x82B9B938  -> 0  (stow requested sample count)
//   Process                       @0x82B9B948  -> 0/1 (generate the frame; ping-pong)
//   `vector deleting destructor'  @0x82BA1DA0  (reinstall base vtable, conditional free)
//
// Sibling to the committed rw::audio::core source plug-ins (Gain, RawPuller2, the Iir2*
// filter shapes, Pan2D1). Like them the PlugIn base sub-object is EMBEDDED by value
// (composition, not inheritance) as a PlugInBaseView at +0x00 so the compiler inserts no
// second vptr and the data offsets are not shifted. The live "frequency" graph attribute
// lives at mfFrequency (+0x30) -- CreateInstance points the base attribute pointer (+0x0C)
// straight at it -- and the active channel count (the per-channel Process-loop bound)
// lives at mBase.mbChannelCount (+0x21).
//
// X360 pointers are 32-bit; on the 64-bit host the base-view pointers widen so the
// absolute offsets do NOT hold. Members are pinned BY NAME/ORDER and every per-member
// store WIDTH (stw / stfs / stfd / stb) is reproduced. GetSize returns the X360 constant
// 72, not sizeof(SinePlayer). Lowercase rw::audio:: namespaces match the third-party
// middleware API (per CXX_NAMING_CONVENTIONS: lowercase namespaces match a 3rd-party API).
// =====================================================================================

#include "types.hpp" // f32, f64, s32, u32, u8
#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioProcessContext / AudioChannelBuffer / AudioFormat

namespace rw
{
namespace audio
{
namespace core
{

class SinePlayer;

// A queued Play command pushed into the System command ring (16 bytes / 4 words on the
// X360). Grounded in PlayHandler @0x82B9B900: handler(+0x00) / target(+0x04) /
// mdStartTime(+0x08, a double) -- i.e. 4 + 4 + 8 with 32-bit console pointers.
// RECORD-STRIDE RULE (X360-literal trap): any producer's cursor advance and the handler's
// return are BOTH this record's size, and on the host that is
// sizeof(SinePlayerPlayCommand) (24 on x64 -- the two leading pointers widened), never the
// console literal 16. The ring consumer (System::ExecuteCommands) calls the leading field
// through int (*)(void *), so the handler slot carries exactly that type.
struct SinePlayerPlayCommand
{
    int (*mpHandler)(void *); // +0x00 (X360) -- &SinePlayer::PlayHandler
    SinePlayer *mpTarget;      // +0x04 (X360) -- the SinePlayer instance
    f64         mdStartTime;   // +0x08 (X360) -- scheduled start time (double), latched into mdStartTime
};

// The matching queued Stop command (8 bytes / 2 words on the X360). Grounded in StopHandler
// @0x82B9B920: handler(+0x00) / target(+0x04).
// Host size = sizeof(SinePlayerStopCommand) (16 on x64), same stride rule as above.
struct SinePlayerStopCommand
{
    int (*mpHandler)(void *); // +0x00 (X360) -- &SinePlayer::StopHandler
    SinePlayer *mpTarget;      // +0x04 (X360) -- the SinePlayer instance
};

// -------------------------------------------------------------------------------------
// SinePlayer -- layout grounded in CreateInstance @0x82BA3FB8 and Process @0x82B9B948
// (X360 sizeof 0x48 == 72; offsets documentary, order load-bearing):
//   +0x00  mBase (PlugInBaseView) -- installed v-table (+0x00), base attribute pointer
//                (+0x0C, pointed at &mfFrequency), active channel count mbChannelCount (+0x21).
//   +0x28  mdStartTime  (f64) -- scheduled start time; samples earlier than it are silence.
//   +0x30  mfFrequency  (f32, init 0.0) -- the live "frequency" graph attribute.
//   +0x38  mfPhase      (f32, init 0.0) -- the running phase accumulator (radians, wraps 2pi).
//   +0x3C  mbPlaying    (u8) -- 1 while playing (PlayHandler sets, StopHandler clears).
//   +0x40  mSamplesRequested (s32) -- the frame's requested sample count (PreProcess sets it).
// The +0x24../+0x34../+0x3D../+0x44.. gaps are alignment padding preserved so the named
// members land at the observed X360 offsets.
// -------------------------------------------------------------------------------------
class SinePlayer
{
public:
    // ---- bodied in SinePlayer.cpp (X360 offsets above are authoritative) ----

    // Placement-construct: install the vtable, clear phase + frequency, point the base
    // attribute pointer at mfFrequency, and clear the playing flag. @0x82BA3FB8. Returns 1.
    static int   CreateInstance(SinePlayer *self);

    // X360 instance footprint: li r3,0x48 == 72. NOT host sizeof(SinePlayer) (96 on x64).
    // @0x82B98308. The factory-side allocation contract is console-sized across the rest of
    // this family (see the committed RawPuller2::GetSize / Delay::GetSize), so the constant is
    // reproduced verbatim rather than swapped for sizeof. Compressor1::GetSize is the one
    // plug-in that has already moved to host sizeof; the family sweep is a filed follow-up.
    static int   GetSize();

    // Deferred command handlers replayed off the System command ring. @0x82B9B900 /
    // @0x82B9B920. Each returns its command-record size, which is the ring cursor advance
    // System::ExecuteCommands applies -- so each returns the HOST sizeof of its record
    // (24 / 16 on x64), never the console literal (16 / 8).
    static int   PlayHandler(void *lpCommand); // returns sizeof(SinePlayerPlayCommand)
    static int   StopHandler(void *lpCommand); // returns sizeof(SinePlayerStopCommand)

    // Per-frame setup: stow the requested sample count (a4). @0x82B9B938. Returns 0.
    static int   PreProcess(SinePlayer *self, int a2, int a3, int a4);

    // Per-frame render: publish the frame descriptor, and while playing, generate the sine
    // (gating pre-start samples to silence) and ping-pong the context buffer slots.
    // @0x82B9B948. Returns 0 when idle, 1 when it produced a frame.
    static int   Process(SinePlayer *self, AudioProcessContext *ctx);

    // `vector deleting destructor' @0x82BA1DA0 -- reinstall the base PlugIn v-table, then
    // conditionally free. (~SinePlayer is trivial and folded away.)
    static void *VectorDeletingDestructor(SinePlayer *self, char flags);

    // ---- layout (X360 sizeof 0x48 == 72; offsets documentary, order load-bearing) ----
    PlugInBaseView mBase;                 // +0x00 .. +0x23
    char       mPad24[0x28 - 0x24];       // +0x24 .. +0x27
    f64        mdStartTime;               // +0x28 -- scheduled start time (double)
    f32        mfFrequency;               // +0x30 -- the live "frequency" attribute
    char       mPad34[0x38 - 0x34];       // +0x34 .. +0x37
    f32        mfPhase;                   // +0x38 -- phase accumulator (radians)
    u8         mbPlaying;                 // +0x3C -- playing flag
    char       mPad3D[0x40 - 0x3D];       // +0x3D .. +0x3F
    s32        mSamplesRequested;         // +0x40 -- requested sample count (PreProcess)
    char       mPad44[0x48 - 0x44];       // +0x44 .. +0x47  (sizeof 0x48 == 72)
};

} // namespace core
} // namespace audio
} // namespace rw
