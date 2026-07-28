#pragma once

// =====================================================================================
// rw::audio::core::Send -- a processing-graph "send" (aux-bus) plug-in: a node that taps
// its input and mixes it into a target SubMix (through an embedded SubMixConnector), with
// a per-send scalar gain that de-clicks (ramps) toward its target across the mixer frame.
// Structurally it is the accumulate-into-a-SubMix sibling of Route (which replaces rather
// than adds), so it shares Route's deferred connect/release path through the owning
// System's command ring, plus a Gain-style de-click ramp on the send level.
//
// EARenderWare "rwaudio" middleware. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (PowerPC) -- the X360 asm is authoritative for member layout. There is NO matching
// translation unit available for this type, no DecFIGS DWARF, and no ProStreet08 rwaudio
// PDB entry present, so every offset below is grounded directly in the disassembly of the
// bodied members (see Send.cpp).
//
// LAYOUT AUTHORITY (sizeof = 104 == GetSize() @0x82B98300 returns 0x68):
//   +0x00  mBase          PlugInBaseView -- installed vtable (+0x00), owning System
//                          (+0x04), the graph attribute pointer (+0x0C -> &mfGain), and
//                          the input channel count (mbFlag20 @+0x20).
//   +0x28  mfGain         f32 (init 1.0) -- the live "gain"/send-level graph attribute
//                          (mBase.mpAttributes points here). The ramp target.
//   +0x30  mSubMixConnector  embedded SubMixConnector subobject (Disconnect is called on
//                          it as this+0x30; CreateInstance zeroes its mpSubMixBuffer(+0x38)/
//                          mpSubMix(+0x3C)/mNumSubMixChannels(+0x40)).
//   +0x44  mDeClickValue  f32[6] foldback gain array passed to SubMixConnector::Disconnect
//                          (this+0x44); zeroed by CreateInstance / DisconnectImmediate.
//   +0x5C  mfCurrentGain  f32 (init 1.0) -- the gain the current frame started at; the
//                          ramp start value, latched to mfGain after each Process frame.
//   +0x60  mbNeedReset    u8 (init 0) -- set when the send produced no output this frame so
//                          the next active frame restarts the ramp at the target.
//   +0x61 .. +0x67        tail pad (sizeof == 0x68 == 104).
//
// The embedded SubMixConnector holds host-width pointers, so the absolute X360 offsets at
// and beyond +0x30 do NOT hold on the 64-bit host. Members are pinned BY NAME/ORDER and
// each per-member store WIDTH (stw / stfs / stb) is reproduced. GROW additively.
// =====================================================================================

#include "types.hpp" // f32, u32, u8
#include "rw/audio/core/Iir2Filters.h"     // PlugInBaseView / AudioProcessContext / AudioChannelBuffer
#include "rw/audio/core/SubMixConnector.h" // rw::audio::core::SubMixConnector, SubMix

namespace rw
{
namespace audio
{
namespace core
{

class Send; // fwd -- the ring records carry the Send instance

// A queued pointer-connect command pushed into the System command ring by EventEvent when
// the connect event carries a SubMix pointer (12 bytes / 3 words on the X360): handler /
// send / target SubMix. ConnectByPointerHandler reads the Send from X360 cmd+4 and the
// target SubMix from X360 cmd+8. Grounded in EventEvent @0x82BA3F00 (pointer path) and
// ConnectByPointerHandler @0x82B9FEF8.
// RECORD-STRIDE RULE (X360-literal trap): the producer's cursor advance and the handler's
// return are BOTH this record's size, and on the host that is
// sizeof(SendConnectByPointerCommand) (24 on x64 -- three pointers widened), never the
// console literal 12. The ring consumer (System::ExecuteCommands) calls the leading field
// through int (*)(void *), so the handler slot carries exactly that type.
struct SendConnectByPointerCommand
{
    int (*mpHandler)(void *); // +0x00 (X360) -- &Send::ConnectByPointerHandler
    Send   *mpTarget;         // +0x04 (X360) -- the Send instance
    SubMix *mpSubMix;         // +0x08 (X360) -- target SubMix (connect event word 0)
};

// The variable-length name-connect command EventEvent queues when the connect event
// carries a SubMix NAME instead of a pointer: a fixed header (handler / send / the
// record's own byte size) then the NUL-terminated name copied inline. The X360 record is
// (strlen + 16) & ~3 bytes (12-byte header + name + NUL, rounded up to its 4-byte pointer
// align); the host analogue is offsetof(SendConnectByNameCommand, maName) + strlen + 1
// rounded up to 8 so the next record's leading handler pointer stays aligned, and
// muRecordSize stores THAT host size (ConnectByNameHandler returns it as the ring
// advance). Grounded in EventEvent @0x82BA3F00 (name path, `clrrwi r8,r8,2` + the
// `v10[2] = size` store) and ConnectByNameHandler @0x82B9FF80 (returns *(cmd+8)).
struct SendConnectByNameCommand
{
    int (*mpHandler)(void *); // +0x00 (X360) -- &Send::ConnectByNameHandler
    Send *mpTarget;           // +0x04 (X360) -- the Send instance
    u32   muRecordSize;       // +0x08 (X360) -- this record's byte size (the ring advance)
    char  maName[1];          // +0x0C (X360) -- the NUL-terminated target SubMix name
};

class Send
{
public:
    // ---- bodied in Send.cpp (X360 offsets above are authoritative) ----

    // Placement-construct the Send part: install the derived vtable, point the base
    // attribute slot at mfGain, seed the send level / current gain to 1.0, clear the
    // embedded connector's owning-SubMix/buffer/channel fields, and zero the foldback
    // gain array. X360 @0x82BA3E98. Returns 1.
    static int CreateInstance(Send* self);

    // sizeof(Send) == 104. X360 @0x82B98300.
    static int GetSize();

    // &off_82F8FF60 -- the static "Send" plug-in run-time descriptor. X360 @0x82B9B798.
    static char** GetPlugInDescRunTime();

    // Per-frame processing: build the src/dst channel pointer arrays, accumulate the input
    // into the target SubMix's buffer (ramped when the send level changed this frame),
    // latch the current gain, and refresh the per-input-channel foldback gains. When the
    // send is not connected it flags a ramp restart for the next active frame. X360
    // @0x82B9B7A8. `resetRamp` restarts the de-click ramp at the target. Returns 1.
    static int Process(Send* self, AudioProcessContext* ctx, char resetRamp);

    // Disconnect the embedded connector: when it is live, remap the send's per-channel
    // gains into a SubMix-channel foldback array (ReChannelGainWrite) and fold them back on
    // Disconnect; then clear the foldback gain array. X360 @0x82B9FE38. (The X360 passes
    // two dead trailing GPR args at the call sites; they are unused here.)
    static int DisconnectImmediate(Send* self);

    // Deferred pointer-connect handler replayed off the ring (the consumer calls it
    // through int (*)(void *) with the record's own address): disconnect the embedded
    // connector, clear the gain array, then -- if the command carries a target SubMix --
    // link the connector into that SubMix's connector list. Returns the record's byte size
    // -- host sizeof(SendConnectByPointerCommand) (X360: 12). @0x82B9FEF8.
    static int ConnectByPointerHandler(void *cmd);

    // BLOCKED -- deferred name-connect handler @0x82B9FF80. Declared (the X360 ledger
    // attests it and EventEvent stores its address) but its BODY is intentionally not
    // reconstructed: it walks a global by-name SubMix registry (list head off_8327EE68 +
    // iterator cursor dword_8327EE00 -- un-homed foreign statics) via an X360-offset-
    // specific intrusive-list idiom (node = link - 0x2C, empty-list sentinel == address
    // 0x2C, name compared at node+0x4C). Faithful reconstruction on the host needs the
    // registry's home type and its SubMix name/list-link members, none of which are
    // groundable from this TU's asm alone. When bodied it returns the queued record's
    // muRecordSize (the ring advance). See Send.cpp.
    static int ConnectByNameHandler(void *cmd);

    // Queue a connect command into the owning System's deferred-command ring. When
    // `useName` is zero the command is the fixed pointer form (ConnectByPointerHandler,
    // payload[0] = SubMix*); otherwise it is the variable-length name form
    // (ConnectByNameHandler, payload[0] = the NUL-terminated name string). The connect
    // event's word 0 is a pointer either way, so the payload is a host-width pointer
    // array. X360 @0x82BA3F00. Returns `self` (the X360 r3 passthrough).
    static Send *EventEvent(Send *self, int useName, void *const *payload);

    // Release: tail-call to DisconnectImmediate (fold the send's gains back into the
    // SubMix and unlink). X360 @0x82BA3EF8 (b DisconnectImmediate).
    static int ReleaseEvent(Send* self);

    // Scalar-deleting destructor: reset the vtable to the PlugIn base (off_820AA810) and,
    // for the deleting variant, operator delete. X360 @0x82BA1D58.
    static void* ScalarDeletingDestructor(void* self, char flags);

    // NOTE: the embedded SubMixConnector holds host-width pointers, so the absolute X360
    // offsets (mSubMixConnector @+0x30, mDeClickValue @+0x44, ...) do NOT hold on the
    // 64-bit host. Members are pinned BY NAME/ORDER only; the +0xNN comments document the
    // X360 layout the asm walks. No absolute-offset gap arithmetic spans the
    // pointer-bearing subobject.
    PlugInBaseView  mBase;               // +0x00 .. +0x23 (X360) -- embedded PlugIn base view
    char            mPad24[0x28 - 0x24]; // +0x24 .. +0x27 (X360)
    f32             mfGain;              // +0x28 (X360) -- live send-level graph attribute
    char            mPad2C[0x30 - 0x2C]; // +0x2C .. +0x2F (X360)
    SubMixConnector mSubMixConnector;    // +0x30 (X360) -- embedded inbound connector subobject
    f32             mDeClickValue[6];    // +0x44 (X360) -- foldback gain array
    f32             mfCurrentGain;       // +0x5C (X360) -- ramp start / latched current gain
    u8              mbNeedReset;         // +0x60 (X360) -- restart-ramp flag
    char            mPad61[0x68 - 0x61]; // +0x61 .. +0x67 (X360) -- tail pad (sizeof 0x68)
};

} // namespace core
} // namespace audio
} // namespace rw
