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
#include "rw/audio/core/PlugIn.h"          // rw::audio::core::System
#include "rw/audio/core/SubMixConnector.h" // rw::audio::core::SubMixConnector, SubMix

namespace rw
{
namespace audio
{
namespace core
{

// A queued connect command pushed into the System command ring by EventEvent (24 bytes /
// 6 words): handler / route / then the 4-word connect event payload. ConnectByPointerHandler
// reads the target SubMix from word 0 (cmd+8) and three connect gains as floats from words
// 1..3 (cmd+0xC / +0x10 / +0x14).
struct RouteConnectCommand
{
    int (*mpHandler)(int); // +0x00 -- &Route::ConnectByPointerHandler
    int   mTarget;         // +0x04 -- the Route instance
    u32   mSubMix;         // +0x08 -- target SubMix* (event word 0)
    u32   mGain0;          // +0x0C -- connect gain 0 (float bits)
    u32   mGain1;          // +0x10 -- connect gain 1 (float bits)
    u32   mGain2;          // +0x14 -- connect gain 2 (float bits)
};

class Route
{
public:
    // ---- bodied in Route.cpp (X360 offsets above are authoritative) ----

    // Placement-construct the Route part: install the derived vtable, clear the embedded
    // connector's owning-SubMix/cursor/flag fields, and zero the foldback gain array.
    // X360 @0x82BA3D40. Returns 1.
    static int CreateInstance(int a1);

    // sizeof(Route) == 84. X360 @0x82B982F8.
    static int GetSize();

    // &off_82F8FBC8 -- the static "Route" plug-in run-time descriptor string. X360
    // @0x82B9B258.
    static char** GetPlugInDescRunTime();

    // Queue a connect command (ConnectByPointerHandler) into the owning System's
    // deferred-command ring. X360 @0x82BA3DC8. a3 is the 4-word connect event payload.
    static int EventEvent(int result, int a2, u32* a3);

    // Release: disconnect the embedded connector (folding its gains back into the
    // SubMix) and clear the foldback gain array. X360 @0x82BA3D88.
    static int ReleaseEvent(int a1);

    // Deferred connect handler replayed off the ring: disconnect the embedded connector,
    // clear the gain array, then -- if the command carries a target SubMix -- link the
    // connector into that SubMix's connector list and store the three byte gains. Returns
    // 24 (the command size). X360 @0x82B9FB38.
    static int ConnectByPointerHandler(int a1);

    // Scalar-deleting destructor: reset the vtable to the PlugIn base (off_820AA810) and,
    // for the deleting variant, operator delete. X360 @0x82BA1D10.
    static void* ScalarDeletingDestructor(void* a1, char a2);

    // NOTE: the embedded SubMixConnector holds host-width pointers, so the absolute
    // X360 offsets (e.g. mConnector @+0x24, maChannelGain @+0x38) do NOT hold on the
    // 64-bit host. Members are pinned BY NAME/ORDER only; the offset comments document
    // the X360 layout the asm walks. No absolute-offset gap arithmetic spans the
    // pointer-bearing subobject.
    void*           mpVTable;         // +0x00 (X360)
    System*         mpSystem;         // +0x04 (X360)
    char            mGap08[0x24 - 0x08]; // +0x08 .. +0x23 (X360) -- opaque PlugIn base body
    SubMixConnector mConnector;       // +0x24 (X360) -- embedded inbound connector subobject
    f32             maChannelGain[6]; // +0x38 (X360) -- foldback gain array
    u8              mau8Gain[3];      // +0x50 (X360) -- per-channel byte gains
};

} // namespace core
} // namespace audio
} // namespace rw
