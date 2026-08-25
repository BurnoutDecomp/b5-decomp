#pragma once

// =====================================================================================
// rw::audio::core::HighPassButterworth -- the "HighPassButterworth" high-pass filter
// audio plug-in.
//
// A PlugIn processing-graph node (sibling of Limiter1 / Gain / Pan2D / ReverbModel1 and
// the Iir2* biquad shapes) that wraps the shared cascaded-Butterworth DSP kernel
// (rw::audio::core::Butterworth, embedded by value at the tail). It carries three design
// parameters (cutoff / filter-order / a third shaping scalar) mirrored by a "last"-value
// snapshot so Process only recomputes coefficients when a parameter actually changes, and
// it registers its group-delay latency into the upstream input the same way the Iir2*
// shapes do.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and store. There is NO Feb-2007 leak source, NO
// DecFIGS DWARF, and NO ProStreet08 rwaudio PDB entry for this TU, so each offset below is
// grounded directly in the disassembly of the bodied member functions:
//   GetPlugInDescRunTime         @0x82B976D0 -> the registered "HighPassButterworth" record
//   Process                      @0x82B976E0 -> store-for-store (recompute-guard + filter)
//   `vector deleting destructor' @0x82BA17A0 -> reinstall base vtable, conditional free
//   GetSize                      @0x82B9DF88 -> Butterworth::GetSize(order) + 96
//   CreateInstance               @0x82BA2CB0 -> store-for-store
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
//
// The PlugIn base sub-object is embedded (composition, not inheritance) as a PlugInBaseView
// at +0x00..+0x23 -- the same idiom Limiter1 / Gain / the Iir2* filter shapes use so the
// data offsets are not shifted by a compiler-inserted vptr (PlugIn is polymorphic with an
// explicit +0x00 vtable word). CreateInstance points mBase.mpAttributes at self+0x28
// (mfCutoffFreq -- the live graph attribute).
//
// WIDTH CAVEAT (family-wide): PlugInBaseView's pointer members widen on the x64 PC target,
// so the +0xNN annotations below are the X360 (32-bit) offsets and are DOCUMENTARY only --
// only the member ORDER is load-bearing and every access is by name (same rule the Iir2*
// shapes and Limiter1 state). The embedded Butterworth is located at run time through the
// stored relative offset muButterworthOffset, so it stays self-consistent on x64. GetSize
// returns the X360-faithful `Butterworth::GetSize(order) + 96` (96 = the X360 header size),
// exactly as the sibling GetSize functions return their X360 size constants.
// =====================================================================================

#include "types.hpp" // f32, u8, u16
#include "rw/audio/core/Iir2Filters.h" // PlugInBaseView / AudioProcessContext
#include "rw/audio/core/Butterworth.h" // Butterworth (embedded by value at the tail)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// HighPassButterworth -- layout grounded in CreateInstance @0x82BA2CB0 and Process
// @0x82B976E0 (X360 header size 0x60 == 96; the embedded Butterworth and its runtime work
// buffers start at +0x60 and extend past sizeof into the GetSize()-byte over-allocation):
//   +0x00  mBase (PlugInBaseView)  -- installed vtable (+0x00), mpSystem (+0x04, passed to
//                                     Butterworth::CreateInstance but unused there),
//                                     mpVoice (+0x08, the owning Voice; decay accumulator at
//                                     voice+0x28), mpAttributes (+0x0C -> &mfCutoffFreq),
//                                     mDecaySamples (+0x18, this node's registered decay tail),
//                                     mbChannelCount (+0x21, the Butterworth channel/section
//                                     count -> buffer sizing).
//   +0x28  mfCutoffFreq            -- live cutoff graph attribute (init 0.0; mpAttributes
//                                     points here; Process clamps it to the guard band).
//   +0x30  mfFilterOrder           -- filter order (init 4.0; truncated to int for the
//                                     Butterworth coefficient design order).
//   +0x38  mfParam3                -- third design scalar (init 1.0).
//   +0x40  mfLastCutoffFreq        -- last-applied cutoff (init 15000.0; recompute guard).
//   +0x48  mfLastFilterOrder       -- last-applied order (recompute guard).
//   +0x50  mfLastParam3            -- last-applied param3 (recompute guard; note Process
//                                     never re-latches it -- reproduced faithfully).
//   +0x58  muButterworthOffset     -- relative offset (from `this`) to the embedded Butterworth.
//   +0x60  mButterworth            -- the embedded cascaded-Butterworth DSP kernel + buffers.
// -------------------------------------------------------------------------------------
class HighPassButterworth
{
public:
    static char **GetPlugInDescRunTime();                                    // @0x82B976D0
    int           Process(AudioProcessContext *ctx);                         // @0x82B976E0
    static void  *VectorDeletingDestructor(HighPassButterworth *self,
                                           char flags);                      // @0x82BA17A0
    static int    GetSize(HighPassButterworth *desc);                        // @0x82B9DF88
    static HighPassButterworth *CreateInstance(HighPassButterworth *self);   // @0x82BA2CB0

    PlugInBaseView mBase;                     // +0x00 .. +0x23
    char        mPad24[0x28 - 0x24];          // +0x24 .. +0x27
    f32         mfCutoffFreq;                  // +0x28
    char        mPad2C[0x30 - 0x2C];          // +0x2C .. +0x2F
    f32         mfFilterOrder;                 // +0x30
    char        mPad34[0x38 - 0x34];          // +0x34 .. +0x37
    f32         mfParam3;                      // +0x38
    char        mPad3C[0x40 - 0x3C];          // +0x3C .. +0x3F
    f32         mfLastCutoffFreq;              // +0x40
    char        mPad44[0x48 - 0x44];          // +0x44 .. +0x47
    f32         mfLastFilterOrder;             // +0x48
    char        mPad4C[0x50 - 0x4C];          // +0x4C .. +0x4F
    f32         mfLastParam3;                  // +0x50
    char        mPad54[0x58 - 0x54];          // +0x54 .. +0x57
    u16         muButterworthOffset;           // +0x58
    char        mPad5A[0x60 - 0x5A];          // +0x5A .. +0x5F
    Butterworth mButterworth;                  // +0x60 (LAST member; trailing runtime work buffers)
};

} // namespace core
} // namespace audio
} // namespace rw
