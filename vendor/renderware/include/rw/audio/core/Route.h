#pragma once

// =====================================================================================
// rw::audio::core::Route -- a processing-graph "route" plug-in: a node that connects a
// source into a SubMix (through an embedded SubMixConnector) with a deferred connect /
// release command path through the owning System's command ring.
//
// EARenderWare "rwaudio" middleware. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC) -- the X360 asm is authoritative for member layout. There is NO matching
// translation unit available for this type and no DecFIGS DWARF, so every offset below
// is grounded directly in the disassembly of the bodied members (see Route.cpp).
//
// LAYOUT AUTHORITY (sizeof = 84 == GetSize() @0x82B982F8 returns 0x54):
//   +0x00  mpVTable      vtable pointer (off_8217F524 installed by CreateInstance;
//                         reset to off_820AA810 -- the PlugIn base vtable -- by the
//                         scalar-deleting destructor @0x82BA1D10)
//   +0x04  mpSystem      owning System* (set by the PlugIn base CreateInstance; its
//                         +0x20 ring base / +0x10B8 ring cursor are used by EventEvent)
//   +0x08 .. +0x23       opaque PlugIn base body
//   +0x24  mConnector    embedded SubMixConnector subobject (Disconnect is called on it
//                         as `this+0x24`; CreateInstance zeroes its mField08/+0x2C,
//                         mpSubMix/+0x30, mbField10/+0x34)
//   +0x38  maChannelGain f32[6] foldback gain array passed to SubMixConnector::Disconnect
//                         (this+0x38); zeroed by CreateInstance / ConnectByPointerHandler /
//                         ReleaseEvent
//   +0x50  mau8Gain      u8[3] per-channel byte gains ConnectByPointerHandler derives from
//                         the connect command's three float operands (fctidz -> low byte)
//   +0x53                 tail pad (sizeof == 0x54)
//
// X360 pointers are 32-bit; on the 64-bit host they widen so the absolute offsets above
// do NOT hold. Members are pinned BY NAME/ORDER and the per-member store WIDTHS (stw /
// stfs / stb) are reproduced. GROW additively.
// =====================================================================================

#include "types.hpp" // f32, u32, s32, u8
#include "rw/audio/core/Iir2Filters.h"     // PlugInBaseView / AudioProcessContext
#include "rw/audio/core/SubMixConnector.h" // rw::audio::core::SubMixConnector, SubMix

namespace rw
{
namespace audio
{
namespace core
{

class Route; // fwd -- RouteConnectCommand carries the Route instance

// The 4-word connect event payload EventEvent receives and copies into the ring record
// (event words 0..3 -> command words 2..5 on the X360). Word 0 is the target SubMix
// pointer; words 1..3 are floats ConnectByPointerHandler truncates (fctidz) to the three
// per-channel byte gains. On the host the pointer word widens, so the payload is typed.
struct RouteConnectEvent
{
    SubMix *mpSubMix; // event word 0 -- target SubMix (null == disconnect only)
    f32     mfGain0;  // event word 1 -- source start channel (float-carried)
    f32     mfGain1;  // event word 2 -- target start channel (float-carried)
    f32     mfGain2;  // event word 3 -- channel count (float-carried)
};

// A queued connect command pushed into the System command ring by EventEvent (24 bytes /
// 6 words on the X360): handler / route / then the 4-word connect event payload.
// ConnectByPointerHandler reads the target SubMix from X360 cmd+8 and three connect gains
// as floats from X360 cmd+0xC / +0x10 / +0x14.
// RECORD-STRIDE RULE (X360-literal trap): the producer's cursor advance and the handler's
// return are BOTH this record's size, and on the host that is sizeof(RouteConnectCommand)
// (40 on x64 -- the handler/Route/SubMix pointers widened), never the console literal 24.
// The ring consumer (System::ExecuteCommands) calls the leading field through
// int (*)(void *), so the handler slot carries exactly that type.
struct RouteConnectCommand
{
    int (*mpHandler)(void *); // +0x00 (X360) -- &Route::ConnectByPointerHandler
    Route  *mpTarget;         // +0x04 (X360) -- the Route instance
    SubMix *mpSubMix;         // +0x08 (X360) -- target SubMix (connect event word 0)
    f32     mfGain0;          // +0x0C (X360) -- connect gain 0
    f32     mfGain1;          // +0x10 (X360) -- connect gain 1
    f32     mfGain2;          // +0x14 (X360) -- connect gain 2
};

class Route
{
public:
    // ---- bodied in Route.cpp (X360 offsets above are authoritative) ----

    // Placement-construct the Route part: install the derived vtable, clear the embedded
    // connector's owning-SubMix/cursor/flag fields, and zero the foldback gain array.
    // X360 @0x82BA3D40. Returns 1.
    static int CreateInstance(Route *self);

    // sizeof(Route) == 84. X360 @0x82B982F8.
    static int GetSize();

    // &off_82F8FBC8 -- the static "Route" plug-in run-time descriptor string. X360
    // @0x82B9B258.
    static char** GetPlugInDescRunTime();

    // Accumulate the selected source channels into the connected SubMix buffer, then
    // retain each channel's final sample for disconnect de-clicking. X360
    // @0x82B9B268. Returns 1.
    static int Process(Route *self, AudioProcessContext *ctx, char resetRamp);

    // Queue a connect command (ConnectByPointerHandler) into the owning System's
    // deferred-command ring. X360 @0x82BA3DC8. pEvent is the 4-word connect event payload
    // (typed host-side -- see RouteConnectEvent). Returns self (the X360 r3 passthrough).
    static Route *EventEvent(Route *self, int a2, const RouteConnectEvent *pEvent);

    // Release: disconnect the embedded connector (folding its gains back into the
    // SubMix) and clear the foldback gain array. X360 @0x82BA3D88.
    static SubMixConnector *ReleaseEvent(Route *self);

    // Deferred connect handler replayed off the ring (the consumer calls it through
    // int (*)(void *) with the record's own address): disconnect the embedded connector,
    // clear the gain array, then -- if the command carries a target SubMix -- link the
    // connector into that SubMix's connector list and store the three byte gains. Returns
    // the record's byte size -- host sizeof(RouteConnectCommand) (X360: 24). @0x82B9FB38.
    static int ConnectByPointerHandler(void *cmd);

    // Scalar-deleting destructor: reset the vtable to the PlugIn base (off_820AA810) and,
    // for the deleting variant, operator delete. X360 @0x82BA1D10.
    static void* ScalarDeletingDestructor(void* a1, char a2);

    // NOTE: the embedded SubMixConnector holds host-width pointers, so the absolute
    // X360 offsets (e.g. mConnector @+0x24, maChannelGain @+0x38) do NOT hold on the
    // 64-bit host. Members are pinned BY NAME/ORDER only; the offset comments document
    // the X360 layout the asm walks. No absolute-offset gap arithmetic spans the
    // pointer-bearing subobject.
    // FLAG (rwaudio PDB reconcile): rw::audio::core::Route [sizeof=84] confirmed by the
    // ProStreet08 X360 PDB; field order + offsets MATCH this ARTIST layout exactly. Own
    // members renamed to the PDB names; the +0x50 byte triple was a single guessed array
    // (mau8Gain[3]) -- the PDB resolves it as three named per-channel bytes. The PlugIn
    // base members belong to PlugIn and are left untouched.
    PlugInBaseView  mBase;            // +0x00 .. +0x23 (X360)
    SubMixConnector mSubMixConnector; // +0x24 (X360) -- embedded inbound connector subobject
    f32             mDeClickValue[6]; // +0x38 (X360) -- foldback gain array
    u8              mSourceStartChannel; // +0x50 (X360) -- per-channel byte gain (source)
    u8              mTargetStartChannel; // +0x51 (X360) -- per-channel byte gain (target)
    u8              mNumChannels;     // +0x52 (X360) -- per-channel byte gain (count)
};

} // namespace core
} // namespace audio
} // namespace rw
