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
// authoritative for every member offset and store:
//   GetPlugInDescRunTime         @0x82B97AA0 -> the registered "Limiter1" descriptor
//   Configure                    @0x82B97AB0 -> UNBLOCKED (phase E, see below)
//   GetSize                      @0x82B97DA8 -> 168 (0xA8)
//   `scalar deleting destructor' @0x82BA18C0
//   CreateInstance               @0x82BA2F28
//   Process                      @0x82B9E3A0 -> bodied with the phase-E callback wave
//
// ⭐ VENDOR HEADER (phase E 2026-08-28): references/Feb-2007/BrnEntityModuleUnity/SDKs/
// Packages/rwaudiocore/2.11.00/include/rw/audio/core/plugins/limiter1.h IS the authoritative
// naming/shape source for this type and it MATCHES the ARTIST layout member-for-member. The
// earlier "no leak source" note is retired, and the placeholder names it forced
// (mfAttribute0/1/2, mfDefaultAttribute0/1/2, mfField9C, muFieldA0) are replaced by the
// vendor's own: mAttribute[ATTRIBUTE_MAX], mLastThreshold, mLastReleaseTime,
// mLastChannelMode, mLastSampleRate, mState. The three attributes are now NAMED by the
// vendor enum (threshold dB / release seconds / channel mode), which is exactly the
// semantic the Configure decode inferred from behaviour.
//
// Lowercase rw::audio:: namespaces match the third-party middleware API (per
// CXX_NAMING_CONVENTIONS: lowercase namespaces are acceptable to match a third-party API).
//
// The PlugIn base sub-object is embedded (composition, not inheritance) as a PlugInBaseView
// at +0x00..+0x23 -- the same idiom Gain / Pan2D / ReverbModel1 / the Iir2* filter shapes use
// so the data offsets are not shifted by a compiler-inserted vptr. (The vendor source spells
// this `class Limiter1 : public PlugIn`; the composition view is the PC-side equivalent that
// keeps the leaf data offsets stable -- see Iir2Filters.h.) CreateInstance bases the
// 8-byte-stride attribute table at self+0x28 (mBase.mpAttributes -> &mAttribute[0]).
//
// The embedded CompressorLimiter1 carries no pointer members, so it keeps its X360 sizeof
// (0x50) on the x64 PC target and +0x40 + 0x50 == +0x90 lands exactly; the PlugInBaseView
// pointer members widen on x64, so the +0xNN annotations below are the X360 (32-bit) offsets
// and are documentary only -- only the member ORDER is load-bearing, and every access is by
// name (the same rule Gain / ReverbModel1 state).
//
// Configure @0x82B97AB0 was BLOCKED on two unknowns; BOTH ARE RESOLVED (phase E, decode
// report progress/scratch_dossiers/limiter1_configure_decode_codex.md):
//   * the anonymous helper sub_82C09970 is the X360 CRT's double-precision pow(x,y) core
//     (f1=x, f2=y, double result in f1) -- proven by its log2(e)/ln(2) polynomial constants,
//     its negative-base integral-exponent classification, its _decomp/_get_exp/_set_exp call
//     tree, and the named `pow` wrapper at 0x82674CD0 that frsp's its result; the same
//     identification unblocks Butterworth.cpp's flagged use of it;
//   * the two rodata constants are flt_82004FDC == 0.95f (0x3F733333, the threshold-off
//     hysteresis multiplier) and flt_8200D5A4 == -0.9f (0xBF666666, the fixed 10:1
//     compressor exponent, == 1/ratio - 1).
// =====================================================================================

#include "types.hpp" // f32, s32, u32, u8
#include "rw/audio/core/Iir2Filters.h"       // PlugInBaseView / AudioProcessContext
#include "rw/audio/core/PlugIn.h"            // PlugIn::Attribute_t (the 8-byte attribute slot)
#include "rw/audio/core/CompressorLimiter1.h" // CompressorLimiter1 (embedded by value @+0x40)

namespace rw
{
namespace audio
{
namespace core
{

// -------------------------------------------------------------------------------------
// Limiter1 -- layout grounded in CreateInstance @0x82BA2F28, Configure @0x82B97AB0 and
// Process @0x82B9E3A0, and MATCHED member-for-member by the vendor limiter1.h (X360
// sizeof 0xA8 == 168):
//   +0x00  mBase (PlugInBaseView)   -- installed v-table (+0x00) + attribute-table base (+0x0C).
//   +0x28  mAttribute[3]            -- the graph attributes (8-byte Attribute_t stride,
//                                      spanning +0x28..+0x3F exactly):
//                                       [0] SETTHRESHOLD   dB      (init 120.0 == INIT_THRESHOLD)
//                                       [1] SETRELEASETIME seconds (init 0.1; Configure clamps [0,10])
//                                       [2] SETCHANNELMODE ChannelMode (init 1.0 == GROUPED)
//   +0x40  mCompressorLimiter1      -- the embedded envelope/gain engine (CompressorLimiter1).
//   +0x90  mLastThreshold           -- Process's change-detect cache of mAttribute[0].
//   +0x94  mLastReleaseTime         -- ... of mAttribute[1].
//   +0x98  mLastChannelMode         -- ... of mAttribute[2].
//   +0x9C  mLastSampleRate          -- ... of the context format's output sample rate.
//   +0xA0  mState (State)           -- STATE_OFF / STATE_ON (the active-limiting latch).
// The +0x24 gap is alignment padding preserved so mAttribute lands at the observed offset.
// -------------------------------------------------------------------------------------
class Limiter1
{
public:
    // Vendor enums (limiter1.h). ATTRIBUTE_* index mAttribute[]; MAX_THRESHOLD is the dB
    // value at/above which the limiter switches itself OFF (Process's `< 20.0f` gate).
    enum ChannelMode
    {
        CHANNELMODE_INDEPENDENT = 0,
        CHANNELMODE_GROUPED = 1,
        CHANNELMODE_MAX = 2
    };

    enum Attribute
    {
        ATTRIBUTE_SETTHRESHOLD = 0,
        ATTRIBUTE_SETRELEASETIME = 1,
        ATTRIBUTE_SETCHANNELMODE = 2,
        ATTRIBUTE_MAX = 3
    };

    enum State
    {
        STATE_OFF = 0,
        STATE_ON = 1,
        NUM_STATES = 2
    };

    // Vendor constants (limiter1.h). LIMITER_RATIO/ATTACKTIME/... are declared there as
    // out-of-line `const static float`s; the ARTIST Configure inlines their VALUES, and the
    // recovered rodata identifies them (see Limiter1.cpp's constant block).
    enum { KU_INIT_THRESHOLD = 120, KU_MAX_THRESHOLD = 20, KU_FILTER_ORDER = 2 };

    static char **GetPlugInDescRunTime();                            // @0x82B97AA0
    static int    GetSize();                                         // @0x82B97DA8 -> 168
    static void  *ScalarDeletingDestructor(Limiter1 *self,
                                           char flags);              // @0x82BA18C0
    static int    CreateInstance(Limiter1 *self);                    // @0x82BA2F28

    // @0x82B9E3A0 -- the registered Process callback. r5 (the discontinuity flag) is never
    // read; every path returns BUFFERSTATUS_AVAILABLE (1).
    static int    Process(Limiter1 *self, AudioProcessContext *ctx, bool discontinuity);

    // @0x82B97AB0 -- UNBLOCKED (phase E). The ABI is r3=self, f1=sampleRate ONLY (the
    // former 8-arg spelling was a Hex-Rays artifact: the callee never reads incoming
    // r4..r10, and the vendor header spells it `void Configure(float sampleRate)`). The
    // machine return is CompressorLimiter1::Configure's untouched r3 (== &mCompressorLimiter1);
    // the sole caller discards it, and the vendor's `void` is the authoritative spelling.
    static void   Configure(Limiter1 *self, f32 afSampleRate);

    PlugInBaseView      mBase;                 // +0x00 .. +0x23
    char                mPad24[0x28 - 0x24];   // +0x24 .. +0x27
    PlugIn::Attribute_t mAttribute[ATTRIBUTE_MAX]; // +0x28 .. +0x3F (8-byte stride)
    CompressorLimiter1  mCompressorLimiter1;   // +0x40 .. +0x8F (X360 sizeof 0x50)
    f32                 mLastThreshold;        // +0x90
    f32                 mLastReleaseTime;      // +0x94
    f32                 mLastChannelMode;      // +0x98
    f32                 mLastSampleRate;       // +0x9C
    s32                 mState;                // +0xA0 (State; stored/compared as a word)
    char                mPadA4[0xA8 - 0xA4];   // +0xA4 (X360 sizeof 0xA8 == 168)
};

} // namespace core
} // namespace audio
} // namespace rw
