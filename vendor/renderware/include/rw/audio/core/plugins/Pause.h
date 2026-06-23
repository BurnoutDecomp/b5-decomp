#pragma once

// =====================================================================================
// rw::audio::core::Pause -- the "pause / fade-gate" audio plug-in. A PlugIn graph node
// that gates its block through depending on a 0..1 pause amount, ramping the audible
// length up or down as the amount crosses 0.0 / 1.0.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm
// is authoritative for every member offset. No Feb-2007 leak source and no DecFIGS DWARF
// exist, so each offset is grounded in CreateInstance @0x82BA36B8, PreProcess
// @0x82B9A140, GetSize @0x82B982D0, GetPlugInDescRunTime @0x82B9A130 and the scalar
// deleting destructor @0x82BA1B48.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API.
// =====================================================================================

#include "types.hpp" // f32, s16, u8

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Pause -- GetSize() == 0x40 (64 bytes). Layout grounded in CreateInstance @0x82BA36B8:
//   +0x00  mpVTable      (off_8217F4C4 install; the scalar-deleting dtor reinstalls the
//                         base PlugIn vtable off_820AA810 before any free)
//   +0x04..+0x27         opaque PlugIn base body (input/factory/state; not touched by
//                         the Pause-specific members modelled here)
//   +0x0C  (mpAttribute) the base's attribute-table pointer, pointed at &mfPauseAmount
//                         (self+0x28) so SetAttribute writes land on mfPauseAmount
//   +0x28  mfPauseAmount (f32, init 0.0 = flt_82001CC0; the 0..1 gate amount)
//   +0x30  mfOne         (f32, init 1.0 = flt_82001C98; the "fully through" reference)
//   +0x34  miLength      (s16; the requested block length, set each PreProcess)
//   +0x36  muOutLength   (u8;  the granted/audible length this block)
//   +0x37  mState        (u8,  init 2; ramp state: 0=closing,1=closed-ish,2=open,>=4=mute)
//   +0x38  mbActive      (u8,  init 0; set once PreProcess has been driven)
// The +0x04..+0x27 / +0x29..+0x2F / +0x39..+0x3F spans are the PlugIn base body and
// alignment padding; preserved as opaque storage so the named members land at the
// observed offsets and sizeof stays 0x40.
// -------------------------------------------------------------------------------------
class Pause
{
public:
    static int CreateInstance(Pause *self);                         // @0x82BA36B8
    static char **GetPlugInDescRunTime();                           // @0x82B9A130
    static int GetSize();                                           // @0x82B982D0
    static int PreProcess(Pause *self, int a2, char force, int length); // @0x82B9A140
    static void *ScalarDeletingDestructor(Pause *self, char flags); // @0x82BA1B48

    void *mpVTable;            // +0x00
    u8    mBase04[0x28 - 0x04]; // +0x04..+0x27 -- opaque PlugIn base body (mpAttribute @+0x0C lives here)
    f32   mfPauseAmount;       // +0x28
    u8    mPad2C[0x30 - 0x2C]; // +0x2C..+0x2F -- alignment pad
    f32   mfOne;               // +0x30
    s16   miLength;            // +0x34
    u8    muOutLength;         // +0x36
    u8    mState;              // +0x37
    u8    mbActive;            // +0x38
    u8    mPad39[0x40 - 0x39]; // +0x39..+0x3F -- tail pad to sizeof 0x40
};

} // namespace core
} // namespace audio
} // namespace rw
