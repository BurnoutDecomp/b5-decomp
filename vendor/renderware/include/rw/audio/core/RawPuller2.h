#pragma once

// =====================================================================================
// rw::audio::core::RawPuller2 -- a "raw pull" source plug-in: a processing-graph node
// that pulls PCM frames from a user-supplied callback into the audio output ring, with
// a deferred play/stop command path through the owning System's command ring.
//
// EARenderWare "rwaudio" middleware. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC) -- the X360 asm is authoritative for member layout. There is NO matching
// translation unit available for this type and no DecFIGS DWARF, so every
// offset below is grounded directly in the disassembly of the bodied members.
//
// LAYOUT AUTHORITY (sizeof = 56 == GetSize() @0x82B97348 returns 0x38):
//   +0x00  mpVTable     vtable pointer (off_8217F4E4)            -- CreateInstance
//   +0x04  mpSystem     owning System* (its +0x20 ring base / +0x10B8 ring cursor are
//                        used by EventEvent to queue Play/Stop commands)
//   +0x24  mpCallback   PCM-fill callback `bool(*)(void* ctx, void* dst, float)`, run by
//                        Process via bctrl; 0 == idle (CreateInstance seeds 0)
//   +0x28  mpContext    callback context/userdata pointer (PlayHandler installs it)
//   +0x2C  mfRate       float pull rate/pitch (PlayHandler stores it; PreProcess/Process
//                        push it into the output node)
//   +0x30  muFrameCount frames-per-pull count (PreProcess sets it from its a4; Process
//                        uses it as the per-channel frame stride)
//   +0x34  mbRestart    byte: 1 == the next Process must re-prime the output (set 1 at
//                        construct + on a Play whose params changed; Process clears it)
//   +0x35  mbNumChannels byte channel count (PlayHandler derives it from the play
//                        command's float arg; Process copies that many channels)
//
// X360 pointers are 32-bit; on the 64-bit host they widen so the absolute offsets above
// do NOT hold. Members are pinned BY NAME/ORDER and the per-member store WIDTHS (stw / stfs
// / stb) are reproduced. GROW additively.
// =====================================================================================

#include "types.hpp" // f32, u32, s32
#include "rw/audio/core/PlugIn.h" // rw::audio::core::System

namespace rw
{
namespace audio
{
namespace core
{

// The fill callback the puller drives each Process. Register setup at the X360 bctrl
// (@0x82B9A6B4): r3 = muFrameCount, r4 = scratch dst, r6 = mpContext, f1 = the peek float;
// returns true (low byte) when it produced the requested frames into the scratch buffer.
typedef int (*RawPuller2FillFn)(u32 luFrameCount, void* lpDst, void* lpContext, f32 lfArg);

// A queued Play command pushed into the System command ring by EventEvent (24 bytes /
// 6 words): handler / puller / then the 4-word play event payload (callback, context,
// rate, frame-count). The matching Stop command is 8 bytes (handler / puller).
struct RawPuller2PlayCommand
{
    int (*mpHandler)(int);  // +0x00 -- &RawPuller2::PlayHandler
    int   mTarget;          // +0x04 -- the RawPuller2 instance
    u32   mPayload0;        // +0x08 -- play event word 0
    u32   mPayload1;        // +0x0C -- play event word 1
    u32   mPayload2;        // +0x10 -- play event word 2
    u32   mPayload3;        // +0x14 -- play event word 3
};

class RawPuller2
{
public:
    // ---- bodied in RawPuller2.cpp (X360 offsets above are authoritative) ----

    // Placement-construct: install the vtable, clear the callback + frame count, and
    // mark the puller for a restart. X360 @0x82B3798. Returns 1.
    static int CreateInstance(int self);

    // sizeof(RawPuller2) == 56. X360 @0x82B97348.
    static int GetSize();

    // Queue a Play (a2 != 0 is actually Stop in the asm's branch sense -- see body) or
    // Stop command into the owning System's deferred-command ring. X360 @0x82B9F358.
    static int EventEvent(int self, int bStop, u32* lpEvent);

    // Deferred command handlers replayed off the ring. X360 @0x82B9A540 / @0x82B9A5B0.
    static int PlayHandler(int lpCommand);   // returns 24 (the Play command size)
    static int StopHandler(int lpCommand);   // returns 8  (the Stop command size)

    // Per-frame setup + render. PreProcess stows the requested frame count; Process pulls
    // the callback and copies the produced channels into the output node. X360
    // @0x82B9A5C8 / @0x82B9A5D8.
    static int PreProcess(int self, int a2, int a3, int a4);
    static int Process(int self, int a2);

    void*            mpVTable;      // +0x00
    System*          mpSystem;      // +0x04
    char             mGap08[0x24 - 0x08]; // +0x08 .. +0x23 -- opaque PlugIn base body
    RawPuller2FillFn mpCallback;    // +0x24
    void*            mpContext;     // +0x28
    f32              mfRate;        // +0x2C
    u32              muFrameCount;  // +0x30
    char             mbRestart;     // +0x34
    char             mbNumChannels; // +0x35
    char             mPad36[0x38 - 0x36]; // +0x36 .. +0x37 -- tail pad (sizeof == 0x38)
};

} // namespace core
} // namespace audio
} // namespace rw
