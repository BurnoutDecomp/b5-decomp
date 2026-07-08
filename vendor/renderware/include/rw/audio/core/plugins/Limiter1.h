#pragma once

// =====================================================================================
// rw::audio::core::Limiter1 -- the "Limiter1" dynamics audio plug-in.
//
// A PlugIn processing-graph node (sibling of Gain / Pan2D / ReverbModel1 / the Iir2* filter
// shapes) that drives the shared CompressorLimiter1 envelope/gain engine (embedded by value
// at +0x40) from three graph attributes, wiring its attack/release/threshold coefficients in
// Configure. The three attributes are mirrored into a "default" snapshot block at +0x90.. so
// the plug-in can restore them.
//
// EARenderWare "rwaudio". Reconstructed from BURNOUT_X360_ARTIST.XEX (PowerPC); the asm is
// authoritative for every member offset and store. There is NO Feb-2007 leak source, NO
// DecFIGS DWARF, and NO ProStreet08 rwaudio PDB entry for this TU, so each offset below is
// grounded directly in the disassembly of the bodied member functions:
//   GetPlugInDescRunTime         @0x82B97AA0 -> the registered "Limiter1" descriptor
//   Configure                    @0x82B97AB0 -> BLOCKED (see below)
//   GetSize                      @0x82B97DA8 -> 168 (0xA8)
//   `scalar deleting destructor' @0x82BA18C0
//   CreateInstance               @0x82BA2F28
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
//
// The PlugIn base sub-object is embedded (composition, not inheritance) as a PlugInBaseView
// at +0x00..+0x23 -- the same idiom Gain / Pan2D / ReverbModel1 / the Iir2* filter shapes use
// so the data offsets are not shifted by a compiler-inserted vptr. CreateInstance bases the
// 8-byte-stride attribute table at self+0x28 (mBase.mpAttributes -> &mfAttribute0).
//
// The embedded CompressorLimiter1 carries no pointer members, so it keeps its X360 sizeof
// (0x50) on the x64 PC target and +0x40 + 0x50 == +0x90 lands exactly; the PlugInBaseView
// pointer members widen on x64, so the +0xNN annotations below are the X360 (32-bit) offsets
// and are documentary only -- only the member ORDER is load-bearing, and every access is by
// name (the same rule Gain / ReverbModel1 state). GetSize returns the X360 constant 168, not
// sizeof(Limiter1).
//
// Configure @0x82B97AB0 is BLOCKED, DECLARED here but intentionally NOT bodied (see
// Limiter1.cpp): its body forwards its parameters into sub_82C09970 -- the same anonymous,
// fully un-typed helper whose register-level argument shape IDA cannot recover and which is
// already flagged un-decodable in Butterworth.cpp -- and then calls CompressorLimiter1::
// Configure with two un-recovered rodata DSP constants (flt_82004FDC, flt_8200D5A4). Both
// the helper's ABI and those coefficients are load-bearing, so the body is blocked rather
// than guessed. The bodied functions below do not reference Configure, so this TU compiles.
// =====================================================================================

#include "types.hpp" // f32, u32, u8
#include "rw/audio/core/Iir2Filters.h"       // PlugInBaseView
#include "rw/audio/core/CompressorLimiter1.h" // CompressorLimiter1 (embedded by value @+0x40)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Limiter1 -- layout grounded in CreateInstance @0x82BA2F28 and Configure @0x82B97AB0
// (X360 sizeof 0xA8 == 168; GetSize is hard-coded to that constant):
//   +0x00  mBase (PlugInBaseView)   -- installed v-table (+0x00) + attribute-table base (+0x0C).
//   +0x28  mfAttribute0 (f32)       -- graph attribute 0 (init 120.0).
//   +0x30  mfAttribute1 (f32)       -- graph attribute 1 (init 0.1; Configure clamps to [0,10]).
//   +0x38  mfAttribute2 (f32)       -- graph attribute 2 (init 1.0; Configure tests == 1.0).
//   +0x40  mCompressorLimiter       -- the embedded envelope/gain engine (CompressorLimiter1).
//   +0x90  mfDefaultAttribute0..2   -- the initial-defaults snapshot mirror of +0x28/+0x30/+0x38.
//   +0x9C  mfField9C (f32)          -- init 0.0 (a "last"-style scalar; role awaits Configure).
//   +0xA0  muFieldA0 (u32)          -- init 0 (a flag/counter word; role awaits Configure).
//
// Precise per-attribute semantics (which attribute is threshold vs. attack vs. sidechain
// frequency) are NOT pinned here because their sole consumer, Configure, is BLOCKED; the
// names above reflect only what CreateInstance's stores attest (the graph-attribute table
// and its default mirror). The +0x24../+0x2C../+0x34../+0x3C.. spans are alignment padding
// preserved so the named members land at the observed X360 offsets.
// -------------------------------------------------------------------------------------
class Limiter1
{
public:
    static char **GetPlugInDescRunTime();                            // @0x82B97AA0
    static int    GetSize();                                         // @0x82B97DA8 -> 168
    static void  *ScalarDeletingDestructor(Limiter1 *self,
                                           char flags);              // @0x82BA18C0
    static int    CreateInstance(Limiter1 *self);                    // @0x82BA2F28

    // BLOCKED -- declared for the class surface, intentionally NOT bodied (see header note
    // and Limiter1.cpp): forwards into the un-decodable helper sub_82C09970 and passes
    // un-recovered rodata coefficients to CompressorLimiter1::Configure. Signature is the
    // asm-grounded shape (r3=self, f1 -> f30 first FP arg, r4..r10 forwarded to the helper).
    static int    Configure(Limiter1 *self, f32 a2, int a3, int a4,
                            int a5, int a6, int a7, int a8);         // @0x82B97AB0

    PlugInBaseView     mBase;                 // +0x00 .. +0x23
    char               mPad24[0x28 - 0x24];   // +0x24 .. +0x27
    f32                mfAttribute0;          // +0x28
    char               mPad2C[0x30 - 0x2C];   // +0x2C .. +0x2F
    f32                mfAttribute1;          // +0x30
    char               mPad34[0x38 - 0x34];   // +0x34 .. +0x37
    f32                mfAttribute2;          // +0x38
    char               mPad3C[0x40 - 0x3C];   // +0x3C .. +0x3F
    CompressorLimiter1 mCompressorLimiter;    // +0x40 .. +0x8F (X360 sizeof 0x50)
    f32                mfDefaultAttribute0;   // +0x90
    f32                mfDefaultAttribute1;   // +0x94
    f32                mfDefaultAttribute2;   // +0x98
    f32                mfField9C;             // +0x9C
    u32                muFieldA0;             // +0xA0
    char               mPadA4[0xA8 - 0xA4];   // +0xA4 (X360 sizeof 0xA8 == 168; GetSize hard-coded)
};

} // namespace core
} // namespace audio
} // namespace rw
