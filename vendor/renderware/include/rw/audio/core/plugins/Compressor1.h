#pragma once

// =====================================================================================
// rw::audio::core::Compressor1 -- the "Compressor1" dynamics audio plug-in.
//
// A PlugIn processing-graph node (sibling of Gain / Limiter1 / Pan2D1 / the Iir2* filter
// shapes) that drives the shared CompressorLimiter1 envelope/gain engine (embedded by value
// at X360 +0x50) from five graph attributes. Unlike Limiter1 (which embeds the engine at
// +0x40), Compressor1 carries two extra attribute words up front, so on the console the
// engine sits at +0x50 and the "last-configured" snapshot mirror at +0xA0.. The plug-in
// re-Configures the engine only when a live attribute (or the format sample-rate) differs
// from its snapshot, and gates processing on an "active" flag so the engine buffer is
// cleared exactly once on the active->bypass edge.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and store. There is NO Feb-2007 leak source, NO DecFIGS
// DWARF, and NO ProStreet08 rwaudio PDB entry for this TU, so each offset below is grounded
// directly in the disassembly of the bodied member functions:
//   GetSize                      @0x82B96B20 -> the instance footprint (console li r3, 0xC0)
//   Configure                    @0x82B96B28 -> store-for-store
//   CreateInstance               @0x82BA2408 -> store-for-store
//   Process                      @0x82B9D988 -> store-for-store
//   `scalar deleting destructor' @0x82BA1680
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
//
// The PlugIn base sub-object is embedded (composition, not inheritance) as a PlugInBaseView
// at console +0x00..+0x23 -- the same idiom Gain / Limiter1 / Pan2D1 / the Iir2* filter shapes
// use so the data offsets are not shifted by a compiler-inserted vptr. CreateInstance bases the
// attribute table at console self+0x28 (mBase.mpAttributes -> &mfThreshold).
//
// HOST-LAYOUT RULE (this is the recurring 32-bit-literal defect and it is easy to reseed):
// PlugInBaseView is pointer-bearing, so it is WIDER on this x64 target than on the console
// (X360 0x24 == 36 bytes; host sizeof(PlugInBaseView) == 56). EVERY +0xNN annotation in this
// file is therefore a CONSOLE (32-bit) offset and is DOCUMENTARY ONLY -- none of them is the
// host offset, including +0x50 for the embedded engine and +0xA0 for the snapshot block. What
// IS load-bearing and IS preserved on the host is (a) the member ORDER, (b) the 8-byte spacing
// between the five graph attributes (the attribute-table stride mBase.mpAttributes indexes),
// and (c) the engine/snapshot adjacency -- all three pinned by _AssertLayout() below. Every
// access is by NAME; never by a literal byte offset. GetSize returns host sizeof(Compressor1),
// not the console constant 192, because it sizes the real allocation on this target (the same
// doctrine EaXmaDec_wG_02.cpp states for its own carve).
//
// Configure @0x82B96B28 is bodied (see Compressor1.cpp). It was previously left BLOCKED on the
// claim that sub_82C09970 had an unrecoverable ABI and that flt_82004FDC was un-recovered; both
// prongs were wrong. sub_82C09970 is the CRT pow(double,double) (its xrefs_from are
// _decomp/_get_exp/_set_exp/log/_powhlp/_copysign and the symbol `pow` @0x82674CD0 calls it --
// BrnMathUtils.cpp already identifies it), and the call site loads only f1 = dbl_8202D7F8 (10.0)
// and f2 = mfThreshold * flt_820047C8 (0.05), i.e. the dB->linear conversion. flt_82004FDC
// dumps as 0.95f (headless IDA on IDA Files/BURNOUT_X360_ARTIST.XEX.i64; GPTriangle.cpp had
// already recovered the same symbol as 0.94999999f).
// =====================================================================================

#include <cstddef> // offsetof

#include "types.hpp" // f32, u32, u8
#include "rw/audio/core/Iir2Filters.h"       // PlugInBaseView, AudioProcessContext
#include "rw/audio/core/CompressorLimiter1.h" // CompressorLimiter1 (embedded by value)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Compressor1 -- layout grounded in CreateInstance @0x82BA2408, Configure @0x82B96B28, and
// Process @0x82B9D988. The +0xNN below are the CONSOLE offsets (X360 sizeof 0xC0 == 192);
// see the HOST-LAYOUT RULE above -- they do NOT hold on x64.
//   +0x00  mBase (PlugInBaseView)   -- installed v-table (+0x00) + attribute-table base (+0x0C);
//                                       mBase.mbChannelCount (+0x21) is passed to the engine.
//   +0x28  mfThreshold (f32)        -- attribute a1[10] (init 120.0; Configure clamps >= -500.0).
//   +0x30  mfRatio (f32)            -- attribute a1[12] (init 1.0;  Configure derives 1/ratio-1).
//   +0x38  mfAttack (f32)           -- attribute a1[14] (init 0.1;  Configure clamps to [0,10]).
//   +0x40  mfRelease (f32)          -- attribute a1[16] (init 0.5;  Configure clamps to [0,30]).
//   +0x48  mfChannelLink (f32)      -- attribute a1[18] (init 1.0;  Configure tests == 1.0).
//   +0x50  mCompressorLimiter       -- the embedded envelope/gain engine (sizeof 0x50; it has
//                                       no pointer members, so 0x50 survives on the host too).
//   +0xA0  mfLastThreshold          -- the last-Configured snapshot of +0x28 (init 120.0).
//   +0xA4  mfLastRatio              -- snapshot of +0x30 (init 1.0).
//   +0xA8  mfLastAttack             -- snapshot of +0x38 (init 0.1).
//   +0xAC  mfLastRelease            -- snapshot of +0x40 (init 0.5).
//   +0xB0  mfLastChannelLink        -- snapshot of +0x48 (init 1.0).
//   +0xB4  mfLastSampleRate         -- snapshot of the format sample-rate (init 0.0).
//   +0xB8  muActive (u32)           -- 1 while the engine is actively compressing, else 0
//                                       (the active->bypass edge clears the engine buffer once).
// The mPad24/2C/34/3C/44/4C spans are NOT there to reproduce absolute console offsets (they
// cannot -- mBase is wider here). They exist to preserve the 8-byte spacing of the five graph
// attributes and the attribute-block-to-engine adjacency; _AssertLayout() enforces exactly that.
// -------------------------------------------------------------------------------------
class Compressor1
{
public:
    static int    GetSize();                                         // @0x82B96B20
    static int    CreateInstance(Compressor1 *self);                 // @0x82BA2408
    static int    Process(Compressor1 *self, AudioProcessContext *ctx); // @0x82B9D988
    static void  *ScalarDeletingDestructor(Compressor1 *self,
                                           char flags);              // @0x82BA1680

    // @0x82B96B28. The asm reads exactly two incoming argument registers -- r3 (mr r31, r3
    // @0x82B96B40) and f1 (fmr f30, f1 @0x82B96B44) -- and the sole call site
    // @0x82B9DA48..0x82B9DA50 sets exactly those two. The a2..a8 ints in the IDA prototype are
    // Hex-Rays artifacts of mis-typing sub_82C09970 (= pow(double,double)).
    // Returns whatever the tail CompressorLimiter1::Configure leaves in r3, i.e. the engine
    // pointer (that callee never writes r3), so the return type is the pointer, not an int --
    // typing it `int` would truncate a 64-bit pointer on this host.
    static CompressorLimiter1 *Configure(Compressor1 *self, f32 lfSampleRate);

    PlugInBaseView     mBase;                 // console +0x00 .. +0x23
    char               mPad24[0x28 - 0x24];   // console +0x24 .. +0x27
    f32                mfThreshold;           // console +0x28  (attribute a1[10])
    char               mPad2C[0x30 - 0x2C];   // console +0x2C .. +0x2F
    f32                mfRatio;               // console +0x30  (attribute a1[12])
    char               mPad34[0x38 - 0x34];   // console +0x34 .. +0x37
    f32                mfAttack;              // console +0x38  (attribute a1[14])
    char               mPad3C[0x40 - 0x3C];   // console +0x3C .. +0x3F
    f32                mfRelease;             // console +0x40  (attribute a1[16])
    char               mPad44[0x48 - 0x44];   // console +0x44 .. +0x47
    f32                mfChannelLink;         // console +0x48  (attribute a1[18])
    char               mPad4C[0x50 - 0x4C];   // console +0x4C .. +0x4F
    CompressorLimiter1 mCompressorLimiter;    // console +0x50 .. +0x9F (sizeof 0x50)
    f32                mfLastThreshold;       // console +0xA0
    f32                mfLastRatio;           // console +0xA4
    f32                mfLastAttack;          // console +0xA8
    f32                mfLastRelease;         // console +0xAC
    f32                mfLastChannelLink;     // console +0xB0
    f32                mfLastSampleRate;      // console +0xB4
    u32                muActive;              // console +0xB8
    char               mPadBC[0xC0 - 0xBC];   // console +0xBC (tail pad up to the console 0xC0)

private:
    // Never called and never defined-out-of-line: a member-function body is a complete-class
    // context (and has private access), which is what lets these compile in-class. Pins ONLY
    // the pointer-width-invariant facts -- absolute console offsets are deliberately NOT pinned
    // because mBase widens on x64 and pinning them would be false.
    static void _AssertLayout()
    {
        // The five graph attributes keep the console attribute-table stride of 8 bytes;
        // CreateInstance points mBase.mpAttributes at &mfThreshold and the console Configure
        // indexes that table as a1[10]/a1[12]/a1[14]/a1[16]/a1[18].
        static_assert(offsetof(Compressor1, mfRatio) - offsetof(Compressor1, mfThreshold) == 8,
                      "attribute stride");
        static_assert(offsetof(Compressor1, mfAttack) - offsetof(Compressor1, mfRatio) == 8,
                      "attribute stride");
        static_assert(offsetof(Compressor1, mfRelease) - offsetof(Compressor1, mfAttack) == 8,
                      "attribute stride");
        static_assert(offsetof(Compressor1, mfChannelLink) - offsetof(Compressor1, mfRelease) == 8,
                      "attribute stride");

        // The engine has no pointer members, so its console sizeof survives on the host.
        static_assert(sizeof(CompressorLimiter1) == 0x50, "engine sizeof");

        // The snapshot block starts immediately after the embedded engine, and its six words
        // are contiguous with muActive last (the console +0xA0..+0xB8 run).
        static_assert(offsetof(Compressor1, mfLastThreshold)
                          == offsetof(Compressor1, mCompressorLimiter) + sizeof(CompressorLimiter1),
                      "engine/snapshot adjacency");
        static_assert(offsetof(Compressor1, mfLastSampleRate)
                          - offsetof(Compressor1, mfLastThreshold) == 20,
                      "snapshot block contiguity");
        static_assert(offsetof(Compressor1, muActive)
                          - offsetof(Compressor1, mfLastSampleRate) == 4,
                      "active flag follows the snapshot block");
    }
};

} // namespace core
} // namespace audio
} // namespace rw
