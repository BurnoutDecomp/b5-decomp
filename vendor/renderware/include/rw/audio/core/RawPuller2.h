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
// PPC ABI: a float argument travels in an FPR and its GPR slot is SKIPPED. The bctrl
// setup @0x82B9A6A4..0x82B9A6B4 loads r3 (frames), r4 (dst) and r6 (context) but never
// r5 -- r5 still holds leftover data -- and passes the gain in f1. An argument in r6
// with r5 unset can only mean an intervening float, so the third parameter is the gain
// and the fourth is the context. The ProStreet08 rwaudio PDB agrees:
// bool(*)(int, float*, float, void*). (An earlier note here read register NUMBERS as
// parameter POSITIONS and had the last two swapped.)
typedef int (*RawPuller2FillFn)(u32 luFrameCount, void* lpDst, f32 lfArg, void* lpContext);

class RawPuller2; // fwd -- the ring records carry the RawPuller2 instance
class Mixer;
typedef Mixer AudioProcessContext;

// The 4-word play event payload EventEvent receives and copies into the ring record
// (event words 0..3 -> command words 2..5 on the X360). Words 0/1 are floats (PlayHandler
// reads them with lfs; word 0 is fctidz-truncated to the byte channel count); words 2/3
// are the callback context and the fill callback. On the host the pointer words widen,
// so the payload is typed.
struct RawPuller2PlayEvent
{
    f32              mfNumChannels; // event word 0 -- channel count (float-carried)
    f32              mfSampleRate;  // event word 1 -- pull rate
    void            *mpContext;     // event word 2 -- callback context/userdata
    RawPuller2FillFn mpCallback;    // event word 3 -- the PCM fill callback
};

// A queued Play command pushed into the System command ring by EventEvent (24 bytes /
// 6 words on the X360): handler / puller / then the 4-word play event payload.
// RECORD-STRIDE RULE (X360-literal trap): the producer's cursor advance and the handler's
// return are BOTH this record's size, and on the host that is
// sizeof(RawPuller2PlayCommand) (40 on x64 -- four pointers widened), never the console
// literal 24. The ring consumer (System::ExecuteCommands) calls the leading field through
// int (*)(void *), so the handler slot carries exactly that type.
struct RawPuller2PlayCommand
{
    int (*mpHandler)(void *);       // +0x00 (X360) -- &RawPuller2::PlayHandler
    RawPuller2      *mpTarget;      // +0x04 (X360) -- the RawPuller2 instance
    f32              mfNumChannels; // +0x08 (X360) -- play event word 0 (float channels)
    f32              mfSampleRate;  // +0x0C (X360) -- play event word 1 (pull rate)
    void            *mpContext;     // +0x10 (X360) -- play event word 2 (context)
    RawPuller2FillFn mpCallback;    // +0x14 (X360) -- play event word 3 (fill callback)
};

// The matching queued Stop command (8 bytes / 2 words on the X360): handler / puller.
// Host size = sizeof(RawPuller2StopCommand) (16 on x64), same stride rule as above.
struct RawPuller2StopCommand
{
    int (*mpHandler)(void *); // +0x00 (X360) -- &RawPuller2::StopHandler
    RawPuller2 *mpTarget;     // +0x04 (X360) -- the RawPuller2 instance
};

class RawPuller2
{
public:
    // ---- bodied in RawPuller2.cpp (X360 offsets above are authoritative) ----

    // Placement-construct: install the vtable, clear the callback + frame count, and
    // mark the puller for a restart. X360 @0x82B3798. Returns 1.
    static int CreateInstance(RawPuller2* self);

    // sizeof(RawPuller2) == 56. X360 @0x82B97348.
    static int GetSize();

    // Queue a Play (bStop == 0) or Stop (bStop != 0) command into the owning System's
    // deferred-command ring. pEvent is the 4-word play event payload (typed host-side --
    // see RawPuller2PlayEvent; unused on the Stop path). Returns self (the X360 r3
    // passthrough). X360 @0x82B9F358.
    static RawPuller2 *EventEvent(RawPuller2 *self, int bStop, const RawPuller2PlayEvent *pEvent);

    // Deferred command handlers replayed off the ring (the consumer calls each through
    // int (*)(void *) with the record's own address). Each returns its record's byte size
    // -- host sizeof(RawPuller2PlayCommand) / sizeof(RawPuller2StopCommand) (X360: 24 / 8).
    // X360 @0x82B9A540 / @0x82B9A5B0.
    static int PlayHandler(void *cmd);
    static int StopHandler(void *cmd);

    // Per-frame setup + render. PreProcess stows the requested frame count; Process pulls
    // the callback and copies the produced channels into the output node. X360
    // @0x82B9A5C8 / @0x82B9A5D8.
    static int PreProcess(RawPuller2* self, int a2, int a3, int a4);
    static int Process(RawPuller2* self, AudioProcessContext* context);

    // FLAG (rwaudio PDB reconcile): NFS ProStreet 08 X360 PDB confirms this exact layout
    // (sizeof=56, field order matches). Member names below are the authoritative PDB names;
    // the X360 ARTIST "+0xNN" offset comments are retained. The PlugIn base body (+0x08..0x23)
    // is left opaque here -- PlugIn is reconciled in its own header. Note: the PDB declares the
    // callback as `bool(*)(int, float*, float, void*)`, which the typedef below now matches
    // (frames, dst, context, arg) is kept ASM-correct per the bctrl reg setup, so the .cpp call
    // order is unchanged.
    void*            mpVTable;          // +0x00
    System*          mpSystem;          // +0x04
    char             mGap08[0x24 - 0x08]; // +0x08 .. +0x23 -- opaque PlugIn base body
    RawPuller2FillFn mpPullCallback;    // +0x24 (PDB: mpPullCallback)
    void*            mpContext;         // +0x28 (PDB: mpContext)
    f32              mPlaySampleRate;   // +0x2C (PDB: mPlaySampleRate)
    int              mSamplesRequested; // +0x30 (PDB: mSamplesRequested, int)
    char             mFormatChanged;    // +0x34 (PDB: mFormatChanged)
    char             mPlayNumChannels;  // +0x35 (PDB: mPlayNumChannels)
    char             mPad36[0x38 - 0x36]; // +0x36 .. +0x37 -- tail pad (sizeof == 0x38)
};

} // namespace core
} // namespace audio
} // namespace rw
