#pragma once

// =====================================================================================
// rw::audio::core::Butterworth -- a cascaded 2nd-order Butterworth IIR section, the DSP
// helper embedded inside HighPassButterworth / LowPassButterworth (which are the PlugIn
// shapes; not modelled here). Unlike the single-biquad Iir2 shapes, this is a plain
// (non-PlugIn) struct: a 9-float coefficient header, a sample count, two short relative
// offsets to a pair of count-by-5-float work buffers, then the buffers themselves.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX; the PowerPC asm is
// authoritative for every offset and every store. No Feb-2007 leak source and no DecFIGS
// DWARF exist. Layout grounded in:
//   Filter        @0x82B649E8   (reads coeffs at +0x00..+0x24, buffers at +0x2C/+0x2E)
//   ClearBuffer   @0x82B64980   (zeros both 20*count buffers)
//   CreateInstance@0x82B6C420   (lays out header + two 8-aligned 20*count buffers)
//   GetSize       @0x82B6C408   (align8(20*count + 55) + 20*count)
//
// The coefficient header (+0x00..+0x24, nine f32) is produced by
// CalculateFilterCoefficients @0x82B64698, which is a KEYSTONE: its body is hand-written
// asm over an undecoded helper (sub_82C09970) and three undecoded rodata coefficient
// tables, so it is NOT reconstructed here (see Butterworth.cpp). Filter/ClearBuffer/
// CreateInstance/GetSize are reconstructed store-for-store.
// =====================================================================================

#include "types.hpp" // f32, s32, u16, u32
#include "rw/audio/core/Iir2Filters.h" // AudioProcessContext / AudioChannelBuffer

namespace rw
{
namespace audio
{
namespace core
{

class Butterworth
{
public:
    // Number of bytes per work-buffer row: 5 f32 (one 2nd-order section's I/O history).
    enum { KU_ROW_BYTES = 20 };

    static int          CalculateFilterCoefficients(Butterworth *self); // @0x82B64698 (KEYSTONE)
    void                Filter(AudioProcessContext *ctx);                 // @0x82B649E8
    void                ClearBuffer();                                    // @0x82B64980
    static Butterworth *CreateInstance(int order, Butterworth *self);     // @0x82B6C420
    static u32          GetSize(int order);                               // @0x82B6C408

    // ---- coefficient header (written by CalculateFilterCoefficients) ----
    // Filter() reads, per output sample:
    //   feedforward = b0*x[n] + b1*x[n-1] + b2*x[n-2] + b3*x[n-3] + b4*x[n-4]
    //   feedback    = a1*y[n-1] + a2*y[n-2] + a3*y[n-3] + a4*y[n-4]
    //   y[n] = feedforward - feedback + 1e-18
    f32 mfB0;     // +0x00
    f32 mfB1;     // +0x04
    f32 mfB2;     // +0x08
    f32 mfB3;     // +0x0C
    f32 mfB4;     // +0x10
    f32 mfPad14;  // +0x14 (unused by Filter)
    f32 mfA1;     // +0x18
    f32 mfA2;     // +0x1C
    f32 mfA3;     // +0x20
    f32 mfA4;     // +0x24
    s32 mCount;   // +0x28 -- number of channels / rows processed
    u16 muOffsetA;// +0x2C -- short relative offset (from `this`) to work-buffer A
    u16 muOffsetB;// +0x2E -- short relative offset (from `this`) to work-buffer B
    // work buffers (A then B) follow at the 8-aligned offsets recorded above.
};

} // namespace core
} // namespace audio
} // namespace rw
