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

    // The cascade design entry. ⭐ ABI RECOVERED (phase E 2026-08-28, from the callee and
    // BOTH callers -- HighPassButterworth::Process @0x82B976E0 and
    // LowPassButterworth::Process @0x82B97C00): r3=self, f1=cutoff, f2=sampleRate,
    // f3=the third shaping attribute, r5=the fctidz'd filter order, r7=the type selector
    // (0 == low-pass, 1 == high-pass). r6 is never initialised by either caller and never
    // read by the callee -- the "un-decodable ABI" the old one-argument stub cited was
    // that phantom slot. The BODY is still a keystone (it needs the three rodata design
    // tables); its declaration is now the true shape so both callers can pass real
    // arguments instead of dropping them.
    enum FilterType { KFILTER_LOWPASS = 0, KFILTER_HIGHPASS = 1 };
    static int          CalculateFilterCoefficients(Butterworth *self, f32 afCutoff,
                                                    f32 afSampleRate, f32 afShape,
                                                    s32 aiOrder, s32 aiType); // @0x82B64698
    void                Filter(AudioProcessContext *ctx);                 // @0x82B649E8
    void                ClearBuffer();                                    // @0x82B64980
    static Butterworth *CreateInstance(int order, Butterworth *self);     // @0x82B6C420
    static u32          GetSize(int order);                               // @0x82B6C408

    // FLAG (rwaudio PDB reconcile): names/types from NFS ProStreet 08 X360 PDB
    // (class rw::audio::core::Butterworth [sizeof=48]); layout MATCHES the ARTIST
    // reconstruction exactly. The nine guessed mfB0..mfA4 floats + the "+0x14 pad"
    // are really two f32[5] coefficient arrays inside a nested Coefficients struct:
    //   b[0..4] @ +0x00..+0x10, a[0..4] @ +0x14..+0x24. (Filter uses b[0..4] and
    //   a[1..4]; a[0] @ +0x14 is the slot the ARTIST reconstruction guessed as pad.)

    // ---- coefficient header (written by CalculateFilterCoefficients) ----
    // Filter() reads, per output sample:
    //   feedforward = b[0]*x[n] + b[1]*x[n-1] + b[2]*x[n-2] + b[3]*x[n-3] + b[4]*x[n-4]
    //   feedback    = a[1]*y[n-1] + a[2]*y[n-2] + a[3]*y[n-3] + a[4]*y[n-4]
    //   y[n] = feedforward - feedback + 1e-18
    struct Coefficients
    {
        f32 b[5]; // +0x00 -- feedforward taps b[0..4]
        f32 a[5]; // +0x14 -- feedback taps a[0..4] (a[0] unused by Filter)
    };
    Coefficients mCoefficients;       // +0x00 (sizeof 40)
    u32 mChannels;                    // +0x28 -- number of channels / rows processed
    u16 mInputHistoryOffset;          // +0x2C -- short relative offset (from `this`) to work-buffer A
    u16 mOutputHistoryOffset;         // +0x2E -- short relative offset (from `this`) to work-buffer B
    // work buffers (A then B) follow at the 8-aligned offsets recorded above.
};

} // namespace core
} // namespace audio
} // namespace rw
